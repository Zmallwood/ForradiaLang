#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class NumberExpression : public Expression
    {
      public:
        double value{0.0};
    };
}