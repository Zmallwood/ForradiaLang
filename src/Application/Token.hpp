#pragma once

#include "TokenTypes.hpp"

namespace ForradiaLang
{
    class Token
    {
      public:
        TokenTypes type;
        std::string value;
    };
}