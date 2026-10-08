#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class IfStatement : public Statement
    {
      public:
        std::unique_ptr<Expression> condition;
        std::vector<std::unique_ptr<Statement>> thenBranch;
        std::vector<std::unique_ptr<Statement>> elseBranch;
    };
}
