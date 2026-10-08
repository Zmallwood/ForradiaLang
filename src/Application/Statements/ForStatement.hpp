#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class ForStatement : public Statement
    {
      public:
        std::string name;
        std::unique_ptr<Expression> start;
        std::unique_ptr<Expression> end;
        std::vector<std::unique_ptr<Statement>> body;
    };
}
