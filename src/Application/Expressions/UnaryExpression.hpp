#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class UnaryExpression : public Expression
    {
      public:
        UnaryExpression() : Expression(ExpressionKind::Unary)
        {
        }

        char operation;
        std::unique_ptr<Expression> operand;
    };
}
