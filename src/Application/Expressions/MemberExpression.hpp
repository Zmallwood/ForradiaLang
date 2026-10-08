#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class MemberExpression : public Expression
    {
      public:
        MemberExpression() : Expression(ExpressionKind::Member)
        {
        }

        std::unique_ptr<Expression> object;
        std::string memberName;
        std::vector<std::unique_ptr<Expression>> arguments;
        bool isCall{false};
    };
}
