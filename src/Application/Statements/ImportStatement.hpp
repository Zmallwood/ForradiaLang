#pragma once

#include "Statement.hpp"

namespace ForradiaLang
{
    class ImportStatement : public Statement
    {
      public:
        std::string moduleName;
    };
}
