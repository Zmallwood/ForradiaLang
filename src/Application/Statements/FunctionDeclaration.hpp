#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    class FunctionDeclaration : public Statement
    {
      public:
        std::string name;
        std::vector<std::unique_ptr<Statement>> body;
    };
}
