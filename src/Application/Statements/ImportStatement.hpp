#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    class ImportStatement : public Statement
    {
      public:
        ImportStatement() : Statement(StatementKind::Import)
        {
        }

        std::string moduleName;
    };
}
