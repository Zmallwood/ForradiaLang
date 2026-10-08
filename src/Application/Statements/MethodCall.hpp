#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class MethodCall : public Statement
    {
      public:
        std::string objectName;
        std::string methodName;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
