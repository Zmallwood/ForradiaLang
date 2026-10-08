#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    struct Parameter
    {
        std::string typeName;
        std::string name;
    };

    class FunctionDeclaration : public Statement
    {
      public:
        std::string name;
        std::string returnType;
        std::vector<Parameter> parameters;
        std::vector<std::unique_ptr<Statement>> body;
    };
}
