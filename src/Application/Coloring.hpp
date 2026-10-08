#pragma once

namespace ForradiaLang
{
    namespace Coloring
    {
        bool IsModule(std::string_view name);

        bool IsColorType(std::string_view name);

        struct Color
        {
            double red;
            double green;
            double blue;
            double alpha;
        };
    }
}
