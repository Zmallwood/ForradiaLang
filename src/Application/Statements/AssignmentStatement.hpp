#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class AssignmentStatement : public Statement
    {
      public:
        std::unique_ptr<Expression> target;
        std::unique_ptr<Expression> value;
    };
}
