#include "Coloring.hpp"

namespace ForradiaLang
{
    bool Coloring::IsModule(std::string_view name)
    {
        return name == "Std.Common.Matter.Coloring";
    }

    bool Coloring::IsColorType(std::string_view name)
    {
        return name == "Color";
    }
}
