#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class MemberExpression : public Expression
    {
      public:
        std::unique_ptr<Expression> object;
        std::string memberName;
    };
}
