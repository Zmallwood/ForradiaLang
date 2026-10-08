#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class StringExpression : public Expression
    {
      public:
        std::string value;
    };
}
