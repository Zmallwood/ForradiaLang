#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class BinaryExpression : public Expression
    {
      public:
        std::unique_ptr<Expression> left;
        char operation;
        std::unique_ptr<Expression> right;
    };
}