#include "Graphics.hpp"

#include <filesystem>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
#endif

namespace ForradiaLang
{
    namespace
    {
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        Uint8 clearRed = 0;
        Uint8 clearGreen = 0;
        Uint8 clearBlue = 0;
        Uint8 clearAlpha = 255;
        bool imageSupportReady = false;
        bool textSupportReady = false;
        std::string imagesDirectory;
        std::string fontFile;
        struct CursorStyle
        {
            std::string imageName;
            int hotspotX{0};
            int hotspotY{0};
        };

        std::unordered_map<std::string, SDL_Texture *> images;
        std::unordered_map<std::string, SDL_Texture *> textImages;
        std::unordered_map<std::string, CursorStyle> cursorStyles;
        std::unordered_map<int, TTF_Font *> fonts;
        std::string defaultCursorStyle;
        int confinedCursorWidth = 0;
        int confinedCursorHeight = 0;
        SDL_Cursor *blankSdlCursor = nullptr;
#ifdef _WIN32
        HCURSOR blankWinCursor = nullptr;
        HWND cursorHookWindow = nullptr;
        WNDPROC originalCursorProc = nullptr;

        LRESULT CALLBACK SuppressSystemCursorProc(HWND hwnd, UINT message,
                                                  WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_SETCURSOR && blankWinCursor != nullptr)
            {
                SetCursor(blankWinCursor);
                return TRUE;
            }

            if (originalCursorProc == nullptr)
            {
                return DefWindowProc(hwnd, message, wParam, lParam);
            }

            return CallWindowProc(originalCursorProc, hwnd, message, wParam,
                                  lParam);
        }
#endif

        Uint8 ToChannel(double value)
        {
            if (value < 0.0)
            {
                value = 0.0;
            }

            if (value > 1.0)
            {
                value = 1.0;
            }

            int channel = static_cast<int>(value * 255.0 + 0.5);

            if (channel > 255)
            {
                channel = 255;
            }

            return static_cast<Uint8>(channel);
        }

        void ClearBackground()
        {
            if (renderer == nullptr)
            {
                return;
            }

            SDL_SetRenderDrawColor(renderer, clearRed, clearGreen, clearBlue,
                                   clearAlpha);
            SDL_RenderClear(renderer);
        }

        void Present()
        {
            if (renderer == nullptr)
            {
                return;
            }

            SDL_RenderPresent(renderer);
        }

        void DestroyImages()
        {
            for (const auto &entry : images)
            {
                SDL_DestroyTexture(entry.second);
            }

            images.clear();
        }

        void EnsureImageSupport()
        {
            if (imageSupportReady)
            {
                return;
            }

            if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
            {
                throw std::runtime_error("Could not initialize graphics.");
            }

            imageSupportReady = true;
        }

        void DestroyText()
        {
            for (const auto &entry : textImages)
            {
                SDL_DestroyTexture(entry.second);
            }

            textImages.clear();
        }

        void DestroyFonts()
        {
            DestroyText();

            for (const auto &entry : fonts)
            {
                TTF_CloseFont(entry.second);
            }

            fonts.clear();
        }

        void EnsureTextSupport()
        {
            if (textSupportReady)
            {
                return;
            }

            if (TTF_Init() < 0)
            {
                throw std::runtime_error("Could not initialize text.");
            }

            textSupportReady = true;
        }

        bool IsPng(const std::filesystem::path &path)
        {
            std::string extension = path.extension().string();

            for (char &character : extension)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }

            return extension == ".png";
        }

        std::filesystem::path ImageFile(std::string_view name)
        {
            if (imagesDirectory.empty())
            {
                throw std::runtime_error("Could not load image.");
            }

            const std::string filename = std::string(name) + ".png";
            const std::filesystem::path direct =
                std::filesystem::path(imagesDirectory) / filename;

            if (std::filesystem::exists(direct))
            {
                return direct;
            }

            for (const auto &entry :
                 std::filesystem::recursive_directory_iterator(
                     imagesDirectory))
            {
                if (entry.is_regular_file() &&
                    entry.path().filename() == filename)
                {
                    return entry.path();
                }
            }

            throw std::runtime_error("Could not load image.");
        }

        SDL_Texture *LoadImageFile(const std::string &key,
                                   const std::filesystem::path &path)
        {
            EnsureImageSupport();

            SDL_Surface *surface = IMG_Load(path.string().c_str());

            if (surface == nullptr)
            {
                throw std::runtime_error("Could not load image.");
            }

            SDL_Texture *texture =
                SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);

            if (texture == nullptr)
            {
                throw std::runtime_error("Could not load image.");
            }

            images.emplace(key, texture);
            return texture;
        }

        SDL_Texture *ImageTexture(std::string_view name)
        {
            const std::string key{name};
            const auto found = images.find(key);

            if (found != images.end())
            {
                return found->second;
            }

            return LoadImageFile(key, ImageFile(name));
        }

        bool CursorHotspot(SDL_Surface *surface, int &hotspotX, int &hotspotY)
        {
            SDL_Surface *converted =
                SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);

            if (converted == nullptr)
            {
                return false;
            }

            if (converted->format->BytesPerPixel != 4 ||
                SDL_LockSurface(converted) != 0)
            {
                SDL_FreeSurface(converted);
                return false;
            }

            hotspotX = 0;
            hotspotY = 0;

            for (int y = 0; y < converted->h; ++y)
            {
                const auto *row =
                    static_cast<const Uint8 *>(converted->pixels) +
                    static_cast<std::size_t>(y) * converted->pitch;
                int minX = converted->w;
                int maxX = -1;

                for (int x = 0; x < converted->w; ++x)
                {
                    if (row[static_cast<std::size_t>(x) * 4 + 3] > 16)
                    {
                        if (x < minX)
                        {
                            minX = x;
                        }

                        if (x > maxX)
                        {
                            maxX = x;
                        }
                    }
                }

                if (maxX >= 0)
                {
                    hotspotX = (minX + maxX) / 2;
                    hotspotY = y;
                    break;
                }
            }

            SDL_UnlockSurface(converted);
            SDL_FreeSurface(converted);
            return true;
        }

        bool ClipCursorRect(SDL_Rect &source, SDL_Rect &destination,
                            int canvasWidth, int canvasHeight)
        {
            if (destination.x < 0)
            {
                source.x -= destination.x;
                source.w += destination.x;
                destination.w += destination.x;
                destination.x = 0;
            }

            if (destination.y < 0)
            {
                source.y -= destination.y;
                source.h += destination.y;
                destination.h += destination.y;
                destination.y = 0;
            }

            if (destination.x + destination.w > canvasWidth)
            {
                const int overflow =
                    destination.x + destination.w - canvasWidth;
                source.w -= overflow;
                destination.w -= overflow;
            }

            if (destination.y + destination.h > canvasHeight)
            {
                const int overflow =
                    destination.y + destination.h - canvasHeight;
                source.h -= overflow;
                destination.h -= overflow;
            }

            return source.w > 0 && source.h > 0 && destination.w > 0 &&
                   destination.h > 0 && destination.x < canvasWidth &&
                   destination.y < canvasHeight;
        }

        void EnsureBlankCursors()
        {
            if (blankSdlCursor == nullptr)
            {
                SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
                    0, 32, 32, 32, SDL_PIXELFORMAT_RGBA32);

                if (surface != nullptr)
                {
                    if (SDL_LockSurface(surface) == 0)
                    {
                        SDL_memset(surface->pixels, 0,
                                   surface->pitch * surface->h);
                        SDL_UnlockSurface(surface);
                    }

                    blankSdlCursor = SDL_CreateColorCursor(surface, 0, 0);
                    SDL_FreeSurface(surface);
                }
            }

#ifdef _WIN32
            if (blankWinCursor == nullptr)
            {
                const int width = GetSystemMetrics(SM_CXCURSOR);
                const int height = GetSystemMetrics(SM_CYCURSOR);

                if (width > 0 && height > 0)
                {
                    const int stride = (width + 7) / 8;
                    std::vector<BYTE> andMask(
                        static_cast<std::size_t>(stride) * height, 0xFF);
                    std::vector<BYTE> xorMask(
                        static_cast<std::size_t>(stride) * height, 0x00);
                    blankWinCursor =
                        CreateCursor(GetModuleHandle(nullptr), 0, 0, width,
                                     height, andMask.data(), xorMask.data());
                }
            }
#endif
        }

        void UnhookSystemCursor()
        {
#ifdef _WIN32
            if (cursorHookWindow != nullptr && originalCursorProc != nullptr)
            {
                SetWindowLongPtr(
                    cursorHookWindow, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(originalCursorProc));
            }

            cursorHookWindow = nullptr;
            originalCursorProc = nullptr;
#endif
        }

        void InstallSystemCursorHook()
        {
#ifdef _WIN32
            if (window == nullptr || blankWinCursor == nullptr)
            {
                return;
            }

            SDL_SysWMinfo info;
            SDL_VERSION(&info.version);

            if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE ||
                info.subsystem != SDL_SYSWM_WINDOWS)
            {
                return;
            }

            HWND hwnd = info.info.win.window;

            if (hwnd == nullptr || hwnd == cursorHookWindow)
            {
                return;
            }

            UnhookSystemCursor();

            const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
                hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(SuppressSystemCursorProc)));

            if (previous == nullptr)
            {
                return;
            }

            originalCursorProc = previous;
            cursorHookWindow = hwnd;
#endif
        }

        void HideSystemCursor()
        {
            EnsureBlankCursors();
            InstallSystemCursorHook();

            if (blankSdlCursor != nullptr)
            {
                SDL_SetCursor(blankSdlCursor);
                SDL_ShowCursor(SDL_ENABLE);
            }
            else
            {
                SDL_ShowCursor(SDL_DISABLE);
            }

#ifdef _WIN32
            if (blankWinCursor != nullptr)
            {
                SetCursor(blankWinCursor);
            }
#endif
        }

        void ShowSystemCursor()
        {
            UnhookSystemCursor();
            SDL_SetCursor(SDL_GetDefaultCursor());
            SDL_ShowCursor(SDL_ENABLE);
        }

        void DestroyBlankCursors()
        {
            UnhookSystemCursor();

            if (blankSdlCursor != nullptr)
            {
                SDL_FreeCursor(blankSdlCursor);
                blankSdlCursor = nullptr;
            }

#ifdef _WIN32
            if (blankWinCursor != nullptr)
            {
                DestroyCursor(blankWinCursor);
                blankWinCursor = nullptr;
            }
#endif
        }

        void ReleaseCursorConfine()
        {
            if (window != nullptr &&
                (confinedCursorWidth != 0 || confinedCursorHeight != 0))
            {
                SDL_SetWindowMouseRect(window, nullptr);
            }

            confinedCursorWidth = 0;
            confinedCursorHeight = 0;
        }

        void ConfineCursorToWindow(int windowWidth, int windowHeight)
        {
            if (window == nullptr || windowWidth <= 0 || windowHeight <= 0 ||
                (windowWidth == confinedCursorWidth &&
                 windowHeight == confinedCursorHeight))
            {
                return;
            }

            SDL_Rect bounds;
            bounds.x = 0;
            bounds.y = 0;
            bounds.w = windowWidth;
            bounds.h = windowHeight;

            if (SDL_SetWindowMouseRect(window, &bounds) == 0)
            {
                confinedCursorWidth = windowWidth;
                confinedCursorHeight = windowHeight;
            }
        }

        void DrawCursor()
        {
            if (renderer == nullptr || window == nullptr ||
                defaultCursorStyle.empty())
            {
                return;
            }

            const auto style = cursorStyles.find(defaultCursorStyle);

            if (style == cursorStyles.end())
            {
                return;
            }

            const auto found = images.find(style->second.imageName);

            if (found == images.end())
            {
                throw std::runtime_error("Could not draw cursor.");
            }

            SDL_Texture *texture = found->second;
            int width = 0;
            int height = 0;
            int mouseX = 0;
            int mouseY = 0;
            int windowWidth = 0;
            int windowHeight = 0;
            int canvasWidth = 0;
            int canvasHeight = 0;

            SDL_GetWindowSize(window, &windowWidth, &windowHeight);

            if (SDL_QueryTexture(texture, nullptr, nullptr, &width,
                                 &height) != 0 ||
                SDL_GetRendererOutputSize(renderer, &canvasWidth,
                                          &canvasHeight) != 0 ||
                windowWidth <= 0 || windowHeight <= 0 || width <= 0 ||
                height <= 0)
            {
                throw std::runtime_error("Could not draw cursor.");
            }

            const Uint32 windowFlags = SDL_GetWindowFlags(window);

            if ((windowFlags & SDL_WINDOW_INPUT_FOCUS) == 0)
            {
                ReleaseCursorConfine();
                ShowSystemCursor();
                return;
            }

            HideSystemCursor();

            ConfineCursorToWindow(windowWidth, windowHeight);
            SDL_GetMouseState(&mouseX, &mouseY);

            if ((windowFlags & SDL_WINDOW_MOUSE_FOCUS) == 0)
            {
                if (mouseX < 0)
                {
                    mouseX = 0;
                }

                if (mouseY < 0)
                {
                    mouseY = 0;
                }

                if (mouseX >= windowWidth)
                {
                    mouseX = windowWidth - 1;
                }

                if (mouseY >= windowHeight)
                {
                    mouseY = windowHeight - 1;
                }
            }

            const double scaleX =
                static_cast<double>(canvasWidth) / windowWidth;
            const double scaleY =
                static_cast<double>(canvasHeight) / windowHeight;
            SDL_Rect source;
            source.x = 0;
            source.y = 0;
            source.w = width;
            source.h = height;
            SDL_Rect destination;
            destination.x =
                static_cast<int>(mouseX * scaleX) - style->second.hotspotX;
            destination.y =
                static_cast<int>(mouseY * scaleY) - style->second.hotspotY;
            destination.w = width;
            destination.h = height;

            if (!ClipCursorRect(source, destination, canvasWidth,
                                canvasHeight))
            {
                return;
            }

            if (SDL_RenderCopy(renderer, texture, &source, &destination) != 0)
            {
                throw std::runtime_error("Could not draw cursor.");
            }
        }

        void DestroyWindow()
        {
            UnhookSystemCursor();
            ReleaseCursorConfine();
            DestroyImages();

            if (renderer != nullptr)
            {
                SDL_DestroyRenderer(renderer);
                renderer = nullptr;
            }

            if (window != nullptr)
            {
                SDL_DestroyWindow(window);
                window = nullptr;
            }
        }

        int PositionOrCentered(int position)
        {
            if (position == -1)
            {
                return static_cast<int>(SDL_WINDOWPOS_CENTERED);
            }

            return position;
        }

        void ResolveSize(int &width, int &height)
        {
            if (width > 0 && height > 0)
            {
                return;
            }

            SDL_DisplayMode mode;

            if (SDL_GetDesktopDisplayMode(0, &mode) != 0)
            {
                throw std::runtime_error("Could not initialize graphics.");
            }

            if (width <= 0)
            {
                width = mode.w;
            }

            if (height <= 0)
            {
                height = mode.h;
            }
        }
    }

    bool Graphics::IsModule(std::string_view name)
    {
        return name == "Std.Graphics";
    }

    double Graphics::FullscreenFlag()
    {
        return SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    double Graphics::WindowedFlag()
    {
        return 0;
    }

    void Graphics::Initialize(int x, int y, int width, int height,
                              unsigned int flags, std::string_view title)
    {
        if (SDL_WasInit(SDL_INIT_VIDEO) == 0 &&
            SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            throw std::runtime_error("Could not initialize graphics.");
        }

        DestroyWindow();
        ResolveSize(width, height);

        const std::string windowTitle{title};

        window = SDL_CreateWindow(windowTitle.c_str(), PositionOrCentered(x),
                                  PositionOrCentered(y), width, height,
                                  static_cast<Uint32>(flags));

        if (window == nullptr)
        {
            throw std::runtime_error("Could not create window.");
        }

        renderer =
            SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

        if (renderer == nullptr)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }

        ClearBackground();
        Present();

        SDL_RaiseWindow(window);
    }

    void Graphics::SetClearColor(double red, double green, double blue,
                                 double alpha)
    {
        if (renderer == nullptr)
        {
            throw std::runtime_error("Could not set clear color.");
        }

        clearRed = ToChannel(red);
        clearGreen = ToChannel(green);
        clearBlue = ToChannel(blue);
        clearAlpha = ToChannel(alpha);
        ClearBackground();
        Present();
    }

    void Graphics::LoadImages(std::string_view directory)
    {
        if (renderer == nullptr)
        {
            throw std::runtime_error("Could not load images.");
        }

        const std::filesystem::path path{directory};

        if (!std::filesystem::is_directory(path))
        {
            throw std::runtime_error("Could not load images.");
        }

        const std::filesystem::path canonical =
            std::filesystem::weakly_canonical(path);

        DestroyImages();
        imagesDirectory = canonical.string();

        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(canonical))
        {
            if (!entry.is_regular_file() || !IsPng(entry.path()))
            {
                continue;
            }

            LoadImageFile(entry.path().stem().string(), entry.path());
        }
    }

    void Graphics::InitializeText(std::string_view path)
    {
        if (renderer == nullptr)
        {
            throw std::runtime_error("Could not initialize text.");
        }

        const std::filesystem::path file{path};

        if (!std::filesystem::is_regular_file(file))
        {
            throw std::runtime_error("Could not initialize text.");
        }

        EnsureTextSupport();
        DestroyFonts();
        fontFile = std::filesystem::weakly_canonical(file).string();
    }

    void Graphics::AddFontSizes(const std::vector<int> &sizes)
    {
        if (fontFile.empty())
        {
            throw std::runtime_error("Could not add font sizes.");
        }

        for (const int size : sizes)
        {
            if (size <= 0)
            {
                throw std::runtime_error("Could not add font sizes.");
            }

            if (fonts.contains(size))
            {
                continue;
            }

            TTF_Font *font = TTF_OpenFont(fontFile.c_str(), size);

            if (font == nullptr)
            {
                throw std::runtime_error("Could not add font sizes.");
            }

            fonts.emplace(size, font);
        }
    }

    void Graphics::AddCursorStyle(std::string_view styleName,
                                  std::string_view imageName)
    {
        if (renderer == nullptr || styleName.empty() || imageName.empty())
        {
            throw std::runtime_error("Could not add cursor style.");
        }

        const std::string style{styleName};
        const std::string image{imageName};

        if (!images.contains(image))
        {
            throw std::runtime_error("Could not add cursor style.");
        }

        EnsureImageSupport();

        SDL_Surface *surface = IMG_Load(ImageFile(image).string().c_str());

        if (surface == nullptr)
        {
            throw std::runtime_error("Could not add cursor style.");
        }

        int hotspotX = 0;
        int hotspotY = 0;
        const bool hotspotReady = CursorHotspot(surface, hotspotX, hotspotY);
        SDL_FreeSurface(surface);

        if (!hotspotReady)
        {
            throw std::runtime_error("Could not add cursor style.");
        }

        cursorStyles[style] = CursorStyle{image, hotspotX, hotspotY};
    }

    void Graphics::SetDefaultCursorStyle(std::string_view styleName)
    {
        const std::string style{styleName};

        if (!cursorStyles.contains(style))
        {
            throw std::runtime_error("Could not set default cursor style.");
        }

        defaultCursorStyle = style;
        HideSystemCursor();
    }

    void Graphics::DrawString(std::string_view text, double x, double y,
                              int fontSize, bool centered)
    {
        if (renderer == nullptr || text.empty())
        {
            throw std::runtime_error("Could not draw string.");
        }

        const auto foundFont = fonts.find(fontSize);

        if (foundFont == fonts.end())
        {
            throw std::runtime_error("Could not draw string.");
        }

        TTF_Font *font = foundFont->second;

        const std::string key =
            std::to_string(fontSize) + '\n' + std::string(text);
        SDL_Texture *texture = nullptr;
        const auto found = textImages.find(key);

        if (found != textImages.end())
        {
            texture = found->second;
        }
        else
        {
            SDL_Color color{255, 255, 255, 255};
            SDL_Surface *surface = TTF_RenderUTF8_Blended(
                font, std::string(text).c_str(), color);

            if (surface == nullptr)
            {
                throw std::runtime_error("Could not draw string.");
            }

            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);

            if (texture == nullptr)
            {
                throw std::runtime_error("Could not draw string.");
            }

            textImages.emplace(key, texture);
        }

        int textWidth = 0;
        int textHeight = 0;
        int canvasWidth = 0;
        int canvasHeight = 0;

        if (SDL_QueryTexture(texture, nullptr, nullptr, &textWidth,
                             &textHeight) != 0 ||
            SDL_GetRendererOutputSize(renderer, &canvasWidth, &canvasHeight) !=
                0 ||
            canvasWidth <= 0 || canvasHeight <= 0)
        {
            throw std::runtime_error("Could not draw string.");
        }

        const double anchorX = x * canvasWidth;
        const double anchorY = y * canvasHeight;
        SDL_Rect destination;
        destination.w = textWidth;
        destination.h = textHeight;

        if (centered)
        {
            destination.x = static_cast<int>(
                anchorX - static_cast<double>(textWidth) / 2.0);
            destination.y = static_cast<int>(
                anchorY - static_cast<double>(textHeight) / 2.0);
        }
        else
        {
            destination.x = static_cast<int>(anchorX);
            destination.y = static_cast<int>(anchorY);
        }

        if (SDL_RenderCopy(renderer, texture, nullptr, &destination) != 0)
        {
            throw std::runtime_error("Could not draw string.");
        }
    }

    void Graphics::DrawImage(std::string_view name, double x, double y,
                             double width, double height)
    {
        if (renderer == nullptr)
        {
            throw std::runtime_error("Could not draw image.");
        }

        SDL_Texture *texture = ImageTexture(name);
        int canvasWidth = 0;
        int canvasHeight = 0;

        if (SDL_GetRendererOutputSize(renderer, &canvasWidth, &canvasHeight) !=
                0 ||
            canvasWidth <= 0 || canvasHeight <= 0)
        {
            throw std::runtime_error("Could not draw image.");
        }

        SDL_Rect destination;
        destination.x = static_cast<int>(x * canvasWidth);
        destination.y = static_cast<int>(y * canvasHeight);
        destination.w = static_cast<int>(width * canvasWidth);
        destination.h = static_cast<int>(height * canvasHeight);

        if (SDL_RenderCopy(renderer, texture, nullptr, &destination) != 0)
        {
            throw std::runtime_error("Could not draw image.");
        }
    }

    void Graphics::RunUntilClosed(const std::function<void()> &update,
                                  const std::function<void()> &draw,
                                  const std::function<void(int)> &onMouseDown,
                                  const std::function<void(int)> &onKeyDown)
    {
        if (window == nullptr)
        {
            return;
        }

        bool running = true;
        std::unordered_set<SDL_Keycode> pressedKeys;

        while (running)
        {
            SDL_Event event;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT ||
                    (event.type == SDL_WINDOWEVENT &&
                     event.window.event == SDL_WINDOWEVENT_CLOSE))
                {
                    running = false;
                }
                else if (event.type == SDL_WINDOWEVENT &&
                         event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                {
                    pressedKeys.clear();
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN)
                {
                    if (onMouseDown)
                    {
                        onMouseDown(static_cast<int>(event.button.button));
                    }
                }
                else if (event.type == SDL_KEYDOWN)
                {
                    const SDL_Keycode key = event.key.keysym.sym;

                    if (!pressedKeys.contains(key))
                    {
                        pressedKeys.insert(key);

                        if (onKeyDown)
                        {
                            onKeyDown(static_cast<int>(key));
                        }
                    }

                    if (key == SDLK_ESCAPE)
                    {
                        running = false;
                    }
                }
                else if (event.type == SDL_KEYUP)
                {
                    pressedKeys.erase(event.key.keysym.sym);
                }
            }

            if (!running)
            {
                break;
            }

            if (update)
            {
                update();
            }

            ClearBackground();

            if (draw)
            {
                draw();
            }

            DrawCursor();

            Present();

            SDL_Delay(16);
        }
    }

    void Graphics::Shutdown()
    {
        defaultCursorStyle.clear();
        cursorStyles.clear();
        ReleaseCursorConfine();

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
        {
            ShowSystemCursor();
        }

        DestroyBlankCursors();

        DestroyWindow();
        DestroyFonts();
        fontFile.clear();

        if (textSupportReady)
        {
            TTF_Quit();
            textSupportReady = false;
        }

        if (imageSupportReady)
        {
            IMG_Quit();
            imageSupportReady = false;
        }

        if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
        {
            SDL_Quit();
        }
    }
}
