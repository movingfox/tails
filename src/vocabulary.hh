//
// vocabulary.hh
//
// Copyright (C) 2020 Jens Alfke. All Rights Reserved.
//

#pragma once
#include "instruction.hh"
#include <string_view>
#include <unordered_map>

class Word;

/// A lookup table to find Words by name. Used by the Forth parser.
class Vocabulary {
public:
    Vocabulary();

    explicit Vocabulary(const Word* const *wordList);

    void add(const Word &word);

    const Word* lookup(std::string_view name);

    const Word* lookup(Instruction);

    using map = std::unordered_map<std::string_view, const Word*>;
    using iterator = map::iterator;

    iterator begin()    {return _words.begin();}
    iterator end()      {return _words.end();}

    static Vocabulary global;

private:
    map _words;
};