#pragma once

#include "Token.hpp"

namespace ForradiaLang
{
    class Lexer
    {
      public:
        std::vector<Token> Tokenize(std::string_view source);
    };
}