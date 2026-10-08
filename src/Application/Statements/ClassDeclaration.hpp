#pragma once

#include "FunctionDeclaration.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    class ClassDeclaration : public Statement
    {
      public:
        std::string name;
        std::vector<std::unique_ptr<FunctionDeclaration>> methods;
    };
}
