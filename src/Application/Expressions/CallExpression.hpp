#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class CallExpression : public Expression
    {
      public:
        CallExpression() : Expression(ExpressionKind::Call)
        {
        }

        std::string name;
        std::vector<std::unique_ptr<Expression>> arguments;
    };
}
