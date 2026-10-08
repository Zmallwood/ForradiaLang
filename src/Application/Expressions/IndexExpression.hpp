#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class IndexExpression : public Expression
    {
      public:
        IndexExpression() : Expression(ExpressionKind::Index)
        {
        }

        std::unique_ptr<Expression> object;
        std::unique_ptr<Expression> index;
    };
}
