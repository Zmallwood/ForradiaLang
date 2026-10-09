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

        void AddCursorStyle(std::string_view styleName,
                            std::string_view imageName);

        void SetDefaultCursorStyle(std::string_view styleName);

        void EnableFPSCounter(double x, double y, int fontSize);

        void DrawImage(std::string_view name, double x, double y,
                       double width, double height);

        void GetImageSize(std::string_view name, int &width, int &height);

        void GetMousePosition(double &x, double &y);

        double ConvertWidthToHeight(double width);

        void DrawString(std::string_view text, double x, double y,
                        int fontSize, bool centered);

        void RunUntilClosed(const std::function<void()> &update,
                            const std::function<void()> &draw,
                            const std::function<void(int)> &onMouseDown,
                            const std::function<void(int)> &onKeyDown,
                            const std::function<void(int)> &onKeyUp);

        void Shutdown();
    }
}
