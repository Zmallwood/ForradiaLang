#pragma once

#include "../Expressions/Expression.hpp"
#include "FunctionDeclaration.hpp"
#include "Statement.hpp"

namespace ForradiaLang
{
    struct FieldDeclaration
    {
        std::string typeName;
        std::string name;
        std::unique_ptr<Expression> value;
    };

    class ClassDeclaration : public Statement
    {
      public:
        std::string name;
        std::vector<FieldDeclaration> fields;
        std::vector<std::unique_ptr<FunctionDeclaration>> methods;
    };
}
