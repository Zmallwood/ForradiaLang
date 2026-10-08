#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class FunctionCall : public Statement
    {
      public:
        std::string name;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
