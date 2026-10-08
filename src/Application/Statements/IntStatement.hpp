#pragma once

#include "../Expressions/Expression.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class IntStatement : public Statement
    {
      public:
        std::string name;
        std::unique_ptr<Expression> value;
    };
}
