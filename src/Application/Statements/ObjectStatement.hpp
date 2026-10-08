#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class ObjectStatement : public Statement
    {
      public:
        std::string typeName;
        std::string name;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
