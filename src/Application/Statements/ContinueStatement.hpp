#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    class ContinueStatement : public Statement
    {
      public:
        ContinueStatement() : Statement(StatementKind::Continue)
        {
        }
    };
}
