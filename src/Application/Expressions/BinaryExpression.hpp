#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class BinaryExpression : public Expression
    {
      public:
        BinaryExpression() : Expression(ExpressionKind::Binary)
        {
        }

        std::unique_ptr<Expression> left;
        char operation;
        std::unique_ptr<Expression> right;
    };
}
