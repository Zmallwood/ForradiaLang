#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class PrintStatement : public Statement
    {
      public:
        std::unique_ptr<Expression> expression;
    };
}
