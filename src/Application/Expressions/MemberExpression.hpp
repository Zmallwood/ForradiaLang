#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class MemberExpression : public Expression
    {
      public:
        std::string objectName;
        std::string memberName;
    };
}
