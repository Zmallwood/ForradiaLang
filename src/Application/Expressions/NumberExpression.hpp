#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class NumberExpression : public Expression
    {
      public:
        NumberExpression() : Expression(ExpressionKind::Number)
        {
        }

        double value{0.0};
    };
}
