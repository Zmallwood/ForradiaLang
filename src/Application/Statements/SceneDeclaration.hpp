#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    class SceneDeclaration : public Statement
    {
      public:
        std::string name;
        std::vector<std::unique_ptr<Statement>> update;
        std::vector<std::unique_ptr<Statement>> draw;
        std::vector<std::unique_ptr<Statement>> onMouseDown;
        std::string onMouseDownParameter;
        std::vector<std::unique_ptr<Statement>> onKeyDown;
        std::string onKeyDownParameter;
    };
}
