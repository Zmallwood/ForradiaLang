#pragma once

#include "Statements/Statement.hpp"

#include <filesystem>

namespace ForradiaLang
{
    class Interpreter
    {
      public:
        void Execute(
            const std::vector<std::unique_ptr<Statement>> &statements,
            const std::filesystem::path &sourceDirectory);
    };
}
