#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class IndexExpression : public Expression
    {
      public:
        std::unique_ptr<Expression> object;
        std::unique_ptr<Expression> index;
    };
}
