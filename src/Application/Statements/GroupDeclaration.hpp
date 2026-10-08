#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    class GroupDeclaration : public Statement
    {
      public:
        GroupDeclaration() : Statement(StatementKind::Group)
        {
        }

        std::string name;
        std::vector<std::unique_ptr<Statement>> body;
    };
}
