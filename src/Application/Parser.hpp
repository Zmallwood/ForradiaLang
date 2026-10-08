#pragma once

#include "Statements/Statement.hpp"
#include "Token.hpp"

namespace ForradiaLang
{
    class Parser
    {
      public:
        std::vector<std::unique_ptr<Statement>>
        Parse(const std::vector<Token> &tokens);
    };
}
