#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class CallExpression : public Expression
    {
      public:
        std::string name;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
