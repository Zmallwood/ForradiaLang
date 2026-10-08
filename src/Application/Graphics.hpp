#pragma once

#include <functional>
#include <vector>

namespace ForradiaLang
{
    namespace Graphics
    {
        bool IsModule(std::string_view name);

        double FullscreenFlag();

        double WindowedFlag();

        void Initialize(int x, int y, int width, int height,
                        unsigned int flags, std::string_view title);

        void SetClearColor(double red, double green, double blue,
                           double alpha);

        void LoadImages(std::string_view directory);

        void InitializeText(std::string_view fontFile);

        void AddFontSizes(const std::vector<int> &sizes);

        void DrawImage(std::string_view name, double x, double y,
                       double width, double height);

        void DrawString(std::string_view text, double x, double y,
                        int fontSize, bool centered);

        void RunUntilClosed(const std::function<void()> &update,
                            const std::function<void()> &draw);

        void Shutdown();
    }
}
