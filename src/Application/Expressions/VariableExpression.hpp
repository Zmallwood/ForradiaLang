#pragma once

#include "Expression.hpp"

namespace ForradiaLang
{
    class VariableExpression : public Expression
    {
      public:
        VariableExpression() : Expression(ExpressionKind::Variable)
        {
        }

        std::string name;
    };
}
