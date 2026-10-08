#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class ListExpression : public Expression
    {
      public:
        std::vector<std::unique_ptr<Expression>> elements;
    };
}
