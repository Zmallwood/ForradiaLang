#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class VariableExpression : public Expression
    {
      public:
        std::string name;
    };
}
