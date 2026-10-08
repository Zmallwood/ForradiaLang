#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class UnaryExpression : public Expression
    {
      public:
        char operation;
        std::unique_ptr<Expression> operand;
    };
}
