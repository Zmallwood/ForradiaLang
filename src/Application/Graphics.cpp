#include "Graphics.hpp"

#include <filesystem>

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
        std::unordered_map<std::string, SDL_Texture *> images;
        std::unordered_map<std::string, SDL_Texture *> textImages;
        std::unordered_map<int, TTF_Font *> fonts;

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

        void DestroyWindow()
        {
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
                                  const std::function<void()> &draw)
    {
        if (window == nullptr)
        {
            return;
        }

        bool running = true;

        while (running)
        {
            SDL_Event event;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT ||
                    (event.type == SDL_WINDOWEVENT &&
                     event.window.event == SDL_WINDOWEVENT_CLOSE) ||
                    (event.type == SDL_KEYDOWN &&
                     event.key.keysym.sym == SDLK_ESCAPE))
                {
                    running = false;
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

            Present();

            SDL_Delay(16);
        }
    }

    void Graphics::Shutdown()
    {
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
