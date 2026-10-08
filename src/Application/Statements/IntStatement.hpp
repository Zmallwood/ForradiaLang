#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class IntStatement : public Statement
    {
      public:
        IntStatement() : Statement(StatementKind::Int)
        {
        }

        std::string typeName;
        std::string name;
        std::unique_ptr<Expression> value;
        bool isConstant{false};
    };
}
