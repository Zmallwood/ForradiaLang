#include "Application.hpp"
#include "Coloring.hpp"
#include "Graphics.hpp"
#include "Interpreter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "ScenesCore.hpp"
#include "Statements/ImportStatement.hpp"

#include <filesystem>
#include <unordered_set>

namespace ForradiaLang
{
    namespace
    {
        std::string ReadSource(const std::filesystem::path &path)
        {
            std::ifstream file(path);

            if (!file.is_open())
            {
                throw std::runtime_error("Could not open file.");
            }

            std::stringstream buffer;

            buffer << file.rdbuf();

            return buffer.str();
        }

        std::vector<std::unique_ptr<Statement>>
        ParseSource(const std::filesystem::path &path)
        {
            Lexer lexer;
            Parser parser;

            return parser.Parse(lexer.Tokenize(ReadSource(path)));
        }

        std::filesystem::path
        ModuleFile(const std::filesystem::path &directory,
                   const std::string &moduleName)
        {
            std::string relative = moduleName;

            for (char &character : relative)
            {
                if (character == '.')
                {
                    character = '/';
                }
            }

            return directory / (relative + ".frd");
        }

        std::filesystem::path
        ResolveModule(const std::filesystem::path &importingFile,
                      const std::string &moduleName)
        {
            const auto local =
                ModuleFile(importingFile.parent_path(), moduleName);

            if (std::filesystem::exists(local))
            {
                return local;
            }

            auto directory = importingFile.parent_path();

            while (!directory.empty())
            {
                const auto parent = directory.parent_path();

                if (parent == directory)
                {
                    break;
                }

                directory = parent;

                const auto candidate = ModuleFile(directory, moduleName);

                if (std::filesystem::exists(candidate))
                {
                    return candidate;
                }
            }

            return local;
        }

        std::vector<std::unique_ptr<Statement>>
        LoadProgram(const std::filesystem::path &path,
                    std::unordered_set<std::string> &loaded)
        {
            if (!std::filesystem::exists(path))
            {
                throw std::runtime_error("Could not open file.");
            }

            const std::string canonical =
                std::filesystem::weakly_canonical(path).string();

            if (!loaded.insert(canonical).second)
            {
                return {};
            }

            auto statements = ParseSource(path);
            std::vector<std::unique_ptr<Statement>> expanded;

            for (auto &statement : statements)
            {
                const auto *import =
                    dynamic_cast<const ImportStatement *>(statement.get());

                if (import == nullptr)
                {
                    expanded.push_back(std::move(statement));
                    continue;
                }

                if (import->moduleName.starts_with("Std."))
                {
                    if (!Graphics::IsModule(import->moduleName) &&
                        !Coloring::IsModule(import->moduleName) &&
                        !ScenesCore::IsModule(import->moduleName))
                    {
                        throw std::runtime_error("Unknown module.");
                    }

                    expanded.push_back(std::move(statement));
                    continue;
                }

                const auto modulePath = ResolveModule(path, import->moduleName);
                auto imported = LoadProgram(modulePath, loaded);

                for (auto &importedStatement : imported)
                {
                    expanded.push_back(std::move(importedStatement));
                }
            }

            return expanded;
        }
    }

    void Application::Run(std::string_view filename)
    {
        try
        {
            std::unordered_set<std::string> loaded;
            auto statements = LoadProgram(
                std::filesystem::path(std::string(filename)), loaded);

            Interpreter interpreter;
            const auto sourceFile = std::filesystem::weakly_canonical(
                std::filesystem::path(std::string(filename)));

#ifdef _WIN32
            SetConsoleOutputCP(CP_UTF8);
#endif

            interpreter.Execute(statements, sourceFile.parent_path());
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}
