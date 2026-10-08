#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class PrintStatement : public Statement
    {
      public:
        PrintStatement() : Statement(StatementKind::Print)
        {
        }

        std::unique_ptr<Expression> expression;
    };
}
