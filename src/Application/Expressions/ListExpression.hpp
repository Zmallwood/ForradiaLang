#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class ListExpression : public Expression
    {
      public:
        ListExpression() : Expression(ExpressionKind::List)
        {
        }

        std::vector<std::unique_ptr<Expression>> elements;
    };
}
