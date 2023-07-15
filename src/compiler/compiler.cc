//
// compiler.cc
//
// Copyright (C) 2021 Jens Alfke. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "compiler.hh"
#include "compiler+stackcheck.hh"
#include "assembler.hh"
#include "disassembler.hh"
#include "core_words.hh"
#include "native_word.hh"
#include "stack_effect_parser.hh"
#include "utils.hh"
#include "vocabulary.hh"
#include <optional>
#include <sstream>
#include <string>
#include <string_view>


namespace tails {
    using namespace std;
    using namespace tails::core_words;


#pragma mark - COMPILEDWORD:


    CompiledWord::CompiledWord(string &&name, StackEffect effect, vector<Opcode> &&instrs)
    :_nameStr(toupper(name))
    ,_instrs(move(instrs))
    {
        _effect = effect;
        _romEffect = &_effect;
        _instr = Instruction((Instruction*)&_instrs.front());
        if (!_nameStr.empty()) {
            _name = _nameStr.c_str();
            Compiler::activeVocabularies.current()->add(*this);
        }
    }


    CompiledWord::CompiledWord(Compiler &&compiler)
    :CompiledWord(move(compiler._name), {}, compiler.generateInstructions())
    {
        // Compiler's flags & effect are not valid until after generateInstructions(), above.
        assert((compiler._flags & ~(Word::Inline | Word::Recursive | Word::Magic)) == 0);
        _flags = compiler._flags;
        _effect = compiler._effect;
    }


    CompiledWord::CompiledWord(const CompiledWord &word, std::string &&name)
    :CompiledWord(move(name), word.stackEffect(), vector<Opcode>(word._instrs))
    {
        _flags = word._flags;
    }


#pragma mark - COMPILER:


    VocabularyStack Compiler::activeVocabularies;


    Compiler::Compiler() {
        assert(activeVocabularies.current() != nullptr);
        _words.push_back({NOP});
    }


    Compiler::~Compiler() = default;


    void Compiler::setInputStack(const Value *bottom, const Value *top) {
        _effect = StackEffect();
        if (bottom && top) {
            for (auto vp = bottom; vp <= top; ++vp)
                _effect.addInput(TypeSet(vp->type()));
        }
        _effectCanAddInputs = false;
        _effectCanAddOutputs = true;
    }


    CompiledWord Compiler::compile(std::initializer_list<WordRef> words) {
        Compiler compiler;
        for (auto &ref : words)
            compiler.add(ref);
        return move(compiler).finish();
    }


    Compiler::InstructionPos Compiler::add(const WordRef &ref, const char *source) {
        auto i = prev(_words.end());
        bool isDst = i->isBranchDestination;
        *i = SourceWord(ref, source);
        i->isBranchDestination = isDst;  // preserve this flag

        _words.push_back({NOP});
        return i;
    }


    Compiler::InstructionPos Compiler::addInline(const Word &word, const char *source) {
        if (word.isNative()) {
            return add({word});
        } else {
            auto i = prev(_words.end());
            Disassembler dis(word.instruction().param.word);
            while (true) {
                WordRef ref = dis.next();
                if (ref.word == &_RETURN)
                    break;
                add(ref, source);
            }
            return i;
        }
    }


    Compiler::InstructionPos Compiler::add(const Word* word, const char *sourcePos) {
        if (word->isMagic())
            throw compile_error("Special word " + string(word->name())
                                + " cannot be added by parser", sourcePos);
        assert(word->parameters() == 0);
        if (word->hasFlag(Word::Inline)) {
            return addInline(*word, sourcePos);
        } else {
            return add(*word, sourcePos);
        }
    }


    Compiler::InstructionPos Compiler::add(const Word* word,
                                           intptr_t param,
                                           const char *sourcePos)
    {
        assert(word->parameters() == 1);
        if (word->hasIntParams())
            return add({*word, param}, sourcePos);
        else
            return add({*word, Value(double(param))}, sourcePos);
    }


    Compiler::InstructionPos Compiler::addLiteral(Value v, const char *sourcePos) {
        if (v.isDouble()) {
            double n = v.asDouble();
            if (canCastToInt16(n))
                return add({_INT, int16_t(n)}, sourcePos);
        }
        return add({_LITERAL, v}, sourcePos);
    }


    Compiler::InstructionPos Compiler::addGetArg(int stackOffset, const char *sourcePos) {
        assert(stackOffset >= 1 - _effect.inputCount());
        assert(stackOffset <= int(_localsTypes.size()));
        _usesArgs = true;
        return add({_GETARG, stackOffset}, sourcePos);
    }

    Compiler::InstructionPos Compiler::addSetArg(int stackOffset, const char *sourcePos) {
        return add({_SETARG, stackOffset}, sourcePos);
    }


    int Compiler::reserveLocalVariable(TypeSet type) {
        // First find the _LOCALS instruction at the start, or add one:
        InstructionPos iLocals;
        if (!_words.empty() && _words.begin()->word == &_LOCALS)
            iLocals = _words.begin();
        else
            iLocals = _words.insert(_words.begin(), SourceWord({_LOCALS, 0}, nullptr));
        _localsTypes.push_back(type);
        int offset = int(_localsTypes.size());
        iLocals->param.param.offset = offset;
        return offset;
    }


    void Compiler::addRecurse() {
        add({_RECURSE, intptr_t(-1)})->branchesTo(_words.begin());
    }


    void Compiler::addBranchBackTo(InstructionPos pos) {
        add({_BRANCH, intptr_t(-1)})->branchesTo(pos);
    }


    void Compiler::fixBranch(InstructionPos src) {
        src->branchesTo(prev(_words.end()));
    }


    /// Adds a branch instruction (unless `branch` is NULL)
    /// and pushes its location onto the control-flow stack.
    void Compiler::pushBranch(char identifier, const Word *branch) {
        InstructionPos branchRef;
        if (branch)
            branchRef = add({*branch, intptr_t(-1)}, _curToken.data());
        else
            branchRef = prev(_words.end()); // Will point to next word to be added
        _controlStack.push_back({identifier, branchRef});
    }

    /// Pops the control flow stack, checks that the popped identifier matches,
    /// and returns the address of its branch instruction.
    Compiler::InstructionPos Compiler::popBranch(const char *matching) {
        if (!_controlStack.empty()) {
            auto ctrl = _controlStack.back();
            if (strchr(matching, ctrl.first)) {
                _controlStack.pop_back();
                return ctrl.second;
            }
        }
        throw compile_error("no matching IF or WHILE", _curToken.data());
    }


    // Returns true if this instruction a RETURN, or a BRANCH to a RETURN.
    bool Compiler::returnsImmediately(Compiler::InstructionPos pos) {
        if (pos->word == &_BRANCH)
            return returnsImmediately(*pos->branchTo);
        else
            return (pos->word == &_RETURN);
    }


    vector<Opcode> Compiler::generateInstructions() {
        if (!_controlStack.empty())
            throw compile_error("Unfinished IF-ELSE-THEN or BEGIN-WHILE-REPEAT)", nullptr);

        // If the word preserves its args or has locals, clean up the stack:
        if (_usesArgs || !_localsTypes.empty()) {
            AfterInstruction::DropCount drop {
                .locals = uint8_t(_effect.inputCount() + _localsTypes.size()),
                .results = uint8_t(_effect.outputCount()),
            };
            if (drop.locals > 0)
                add(WordRef{_DROPARGS, drop}, nullptr);
        }

        // Add a RETURN, replacing the "next word" placeholder:
        assert(_words.back().word == &NOP);
        _words.back() = {_RETURN};

        // Compute the stack effect and do type-checking:
        computeEffect();

        // Assign a PC offset to each instruction, and do some optimizations:
        {
            Assembler asmblr;
            bool afterBranch = false;
            for (auto i = _words.begin(); i != _words.end();) {
                if (afterBranch && !i->isBranchDestination) {
                    // Unreachable instruction after a branch
                    i = _words.erase(i);
                } else {
                    if (i->word == &_RECURSE) {
                        // Detect tail recursion: Change RECURSE to BRANCH if it's followed by RETURN:
                        if (returnsImmediately(next(i)))
                            i->word = &_BRANCH;
                        else
                            _flags = Word::Flags(_flags | Word::Recursive);
                    }
                    if (auto dst = i->branchTo) {
                        // Follow chains of branches:
                        while ((*dst)->word == &_BRANCH)
                            dst = (*dst)->branchTo;
                        i->branchTo = dst;
                        // OPT: We could optimize a BRANCH to RETURN into a RETURN; but currently we use
                        // RETURN as an end-of-word marker, so it can only appear at the end of a word.
                    }
                    // Add word to temporary assembly so we know its pc offset:
                    i->pc = asmblr.codeSize();
                    asmblr.add(*i->word, i->param);
                    afterBranch = (i->word == &_BRANCH);
                    ++i;
                }
            }
        }

        // Assemble `_words` into a series of instructions:
        Assembler asmblr;
        for (auto i = _words.begin(); i != _words.end(); ++i) {
            if (i->branchTo) {
                // Now that we know the destination's pc offset we can compute the relative jump:
                i->param.param.offset = (*i->branchTo)->pc - i->pc - 1;
            }
            asmblr.add(*i->word, i->param);
        }
        return std::move(asmblr).finish();
    }


    CompiledWord Compiler::finish() && {
        return CompiledWord(move(*this));
        // the CompiledWord constructor will call generateInstructions()
    }


#pragma mark WORDS:
    

    namespace core_words {

        NATIVE_WORD(DEFINE, "DEFINE", ROMStackEffect{ {Value::AQuote, Value::AString}, {} }) {
            auto name = sp[0].asString();
            auto quote = (const CompiledWord*)sp[-1].asQuote();
            sp -= 2;
            new CompiledWord(*quote, string(name));
            NEXT();
        }

    }
}
