#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class StringExpression : public Expression
    {
      public:
        StringExpression() : Expression(ExpressionKind::String)
        {
        }

        std::string value;
    };
}
