#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class FunctionCall : public Statement
    {
      public:
        FunctionCall() : Statement(StatementKind::FunctionCall)
        {
        }

        std::string name;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
