#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class MethodCall : public Statement
    {
      public:
        MethodCall() : Statement(StatementKind::MethodCall)
        {
        }

        std::unique_ptr<Expression> object;
        std::string methodName;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
