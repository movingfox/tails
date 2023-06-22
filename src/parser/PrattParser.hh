//
// PrattParser.hh
//
// 
//

#pragma once
#include "stack_effect.hh"
#include "tokenizer.hh"
#include "value.hh"
#include <functional>
#include <unordered_map>

namespace tails {
    class Compiler;
    class Parser;

    enum class priority_t : int { None = INT_MIN };

    static inline priority_t operator""_pri (unsigned long long v) {return priority_t(int(v));}

    // A symbol in the grammar: defines how to parse it.
    class Symbol {
    public:
        explicit Symbol(Word const&);
        explicit Symbol(Value);
        explicit Symbol(std::string const& token);
        explicit Symbol(const char* token)              :Symbol(std::string(token)) { }
        virtual ~Symbol();

        std::string token;

        bool isLiteral() const      {return !_literal.isNull();}
        Value literalValue() const  {return _literal;}

        using ParsePrefixFn  = std::function<StackEffect(Parser&)>;
        using ParseInfixFn   = std::function<StackEffect(StackEffect const&, Parser&)>;
        using ParsePostfixFn = ParseInfixFn;

        Symbol&& makePrefix(priority_t) &&;
        Symbol&& makePrefix(priority_t, Word const&) &&;
        Symbol&& makePrefix(priority_t, ParsePrefixFn) &&;

        Symbol&& makeInfix(priority_t left, priority_t right) &&;
        Symbol&& makeInfix(priority_t left, priority_t right, Word const&) &&;
        Symbol&& makeInfix(priority_t left, priority_t right, ParseInfixFn) &&;

        Symbol&& makePostfix(priority_t) &&;
        Symbol&& makePostfix(priority_t, ParsePostfixFn) &&;

        bool isPrefix() const       {return prefixPriority != priority_t::None;}
        bool isInfix() const        {return leftPriority != priority_t::None;}
        bool isPostfix() const      {return postfixPriority != priority_t::None;}

        priority_t prefixPriority  = priority_t::None;
        priority_t leftPriority    = priority_t::None;
        priority_t rightPriority   = priority_t::None;
        priority_t postfixPriority = priority_t::None;

        virtual StackEffect parsePrefix(Parser&) const;
        virtual StackEffect parseInfix(StackEffect const&, Parser&) const;
        virtual StackEffect parsePostfix(StackEffect const&, Parser&) const;

    private:
        Word const*     _word = nullptr;
        Word const*     _prefixWord = nullptr;
        Value           _literal;
        ParsePrefixFn   _customParsePrefix;
        ParseInfixFn    _customParseInfix;
        ParsePostfixFn  _customParsePostfix;
    };


    class SymbolRegistry {
    public:
        SymbolRegistry() = default;

        void add(Symbol&&);

        Symbol const* get(std::string_view) const;

    private:
        std::unordered_map<std::string, Symbol> _registry;
    };


    class Parser {
    public:
        explicit Parser(SymbolRegistry const& reg)
        :_registry(reg)
        ,_tokens(_registry)
        { }

        CompiledWord parse(std::string const& sourceCode);

        StackEffect nextExpression(priority_t minPriority);

        Tokenizer& tokens()     {return _tokens;}

        // Consumes the next token if its literal value matches; else fails.
        void requireToken(std::string_view);

        // Consumes the next token and returns true if its literal value matches; else returns false.
        bool ifToken(std::string_view);

        Compiler& compiler()    {return *_compiler;}

    private:
        StackEffect literal(Value);
        
        SymbolRegistry const& _registry;
        Tokenizer _tokens;
        std::unique_ptr<Compiler> _compiler;
    };

}
