#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class ReturnStatement : public Statement
    {
      public:
        std::unique_ptr<Expression> value;
    };
}
