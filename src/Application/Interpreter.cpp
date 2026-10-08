#include "Interpreter.hpp"

#include <cmath>

#include "Coloring.hpp"
#include "Expressions/BinaryExpression.hpp"
#include "Expressions/CallExpression.hpp"
#include "Expressions/ListExpression.hpp"
#include "Expressions/MemberExpression.hpp"
#include "Expressions/NumberExpression.hpp"
#include "Expressions/StringExpression.hpp"
#include "Expressions/VariableExpression.hpp"
#include "Graphics.hpp"
#include "ScenesCore.hpp"
#include "Statements/ClassDeclaration.hpp"
#include "Statements/FunctionCall.hpp"
#include "Statements/FunctionDeclaration.hpp"
#include "Statements/IfStatement.hpp"
#include "Statements/ImportStatement.hpp"
#include "Statements/IntStatement.hpp"
#include "Statements/MethodCall.hpp"
#include "Statements/ObjectStatement.hpp"
#include "Statements/PrintStatement.hpp"
#include "Statements/SceneDeclaration.hpp"

namespace ForradiaLang
{
    namespace
    {
        struct Object
        {
            std::string className;
        };

        struct ClassInfo
        {
            std::unordered_map<std::string, const FunctionDeclaration *>
                methods;
        };

        struct SceneObject
        {
            std::string typeName;
        };

        struct SceneType
        {
            const std::vector<std::unique_ptr<Statement>> *update{nullptr};
            const std::vector<std::unique_ptr<Statement>> *draw{nullptr};
            const std::vector<std::unique_ptr<Statement>> *onMouseDown{
                nullptr};
            const std::vector<std::unique_ptr<Statement>> *onKeyDown{nullptr};
            const std::vector<std::unique_ptr<Statement>> *onEnter{nullptr};
            std::string onMouseDownParameter;
            std::string onKeyDownParameter;
        };

        using Value = std::variant<double, std::string, Object, Coloring::Color,
                                   SceneObject, std::vector<double>>;

        struct ExecutionState
        {
            std::unordered_map<std::string, Value> variables;
            std::unordered_map<std::string, const FunctionDeclaration *>
                functions;
            std::unordered_map<std::string, ClassInfo> classes;
            std::unordered_map<std::string, SceneType> sceneTypes;
            std::unordered_map<std::string, std::string> addedScenes;
            std::string currentSceneType;
            std::filesystem::path sourceDirectory;
        };

        double AsNumber(const Value &value)
        {
            if (const auto *number = std::get_if<double>(&value))
            {
                return *number;
            }

            throw std::runtime_error("Expected a number.");
        }

        Coloring::Color AsColor(const Value &value)
        {
            if (const auto *color = std::get_if<Coloring::Color>(&value))
            {
                return *color;
            }

            throw std::runtime_error("Expected a color.");
        }

        const std::string &AsString(const Value &value)
        {
            if (const auto *text = std::get_if<std::string>(&value))
            {
                return *text;
            }

            throw std::runtime_error("Expected a string.");
        }

        bool IsTrue(const Value &value)
        {
            if (const auto *number = std::get_if<double>(&value))
            {
                return *number != 0.0;
            }

            if (const auto *text = std::get_if<std::string>(&value))
            {
                return !text->empty();
            }

            throw std::runtime_error("Expected a value.");
        }

        void PrintValue(const Value &value)
        {
            if (const auto *text = std::get_if<std::string>(&value))
            {
                std::cout << *text << '\n';
                return;
            }

            if (const auto *number = std::get_if<double>(&value))
            {
                std::cout << *number << '\n';
                return;
            }

            if (const auto *color = std::get_if<Coloring::Color>(&value))
            {
                std::cout << color->red << ' ' << color->green << ' '
                          << color->blue << ' ' << color->alpha << '\n';
                return;
            }

            throw std::runtime_error("Expected a value.");
        }

        Value Evaluate(ExecutionState &state, const Expression &expression);

        void ExecuteBlock(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Statement>> &statements);

        void ExecuteStatement(ExecutionState &state,
                              const Statement &statement);

        Value EvaluateBinary(ExecutionState &state,
                             const BinaryExpression &expression)
        {
            const double left = AsNumber(Evaluate(state, *expression.left));
            const double right = AsNumber(Evaluate(state, *expression.right));

            switch (expression.operation)
            {
            case '+':
                return left + right;

            case '-':
                return left - right;

            case '>':
                return left > right ? 1.0 : 0.0;

            case '<':
                return left < right ? 1.0 : 0.0;

            case '%':
                if (right == 0.0)
                {
                    throw std::runtime_error("Expected a non-zero number.");
                }

                return std::fmod(left, right);

            case '=':
                return left == right ? 1.0 : 0.0;

            default:
                throw std::runtime_error("Unknown operation.");
            }
        }

        Value EvaluateCall(ExecutionState &state,
                           const CallExpression &expression)
        {
            if (expression.name == "Now")
            {
                if (!expression.arguments.empty())
                {
                    throw std::runtime_error("Expected zero arguments.");
                }

                return static_cast<double>(SDL_GetTicks());
            }

            throw std::runtime_error("Unknown function.");
        }

        Value EvaluateMember(const MemberExpression &expression)
        {
            if (expression.objectName == "MouseButtons")
            {
                if (expression.memberName == "Left")
                {
                    return static_cast<double>(SDL_BUTTON_LEFT);
                }

                if (expression.memberName == "Right")
                {
                    return static_cast<double>(SDL_BUTTON_RIGHT);
                }
            }

            if (expression.objectName == "Keys" &&
                expression.memberName.size() == 1)
            {
                const char letter = expression.memberName[0];

                if (letter >= 'A' && letter <= 'Z')
                {
                    return static_cast<double>(SDLK_a + (letter - 'A'));
                }
            }

            throw std::runtime_error("Unknown member.");
        }

        Value Evaluate(ExecutionState &state, const Expression &expression)
        {
            if (const auto *number =
                    dynamic_cast<const NumberExpression *>(&expression))
            {
                return number->value;
            }

            if (const auto *text =
                    dynamic_cast<const StringExpression *>(&expression))
            {
                return text->value;
            }

            if (const auto *variable =
                    dynamic_cast<const VariableExpression *>(&expression))
            {
                const auto found = state.variables.find(variable->name);

                if (found == state.variables.end())
                {
                    throw std::runtime_error("Unknown variable.");
                }

                return found->second;
            }

            if (const auto *binary =
                    dynamic_cast<const BinaryExpression *>(&expression))
            {
                return EvaluateBinary(state, *binary);
            }

            if (const auto *call =
                    dynamic_cast<const CallExpression *>(&expression))
            {
                return EvaluateCall(state, *call);
            }

            if (const auto *member =
                    dynamic_cast<const MemberExpression *>(&expression))
            {
                return EvaluateMember(*member);
            }

            if (const auto *list =
                    dynamic_cast<const ListExpression *>(&expression))
            {
                std::vector<double> values;

                for (const auto &element : list->elements)
                {
                    values.push_back(AsNumber(Evaluate(state, *element)));
                }

                return values;
            }

            throw std::runtime_error("Unknown expression.");
        }

        void InitializeGraphics(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 6)
            {
                throw std::runtime_error("Expected six arguments.");
            }

            std::vector<Value> arguments;

            for (const auto &argument : call.arguments)
            {
                arguments.push_back(Evaluate(state, *argument));
            }

            Graphics::Initialize(static_cast<int>(AsNumber(arguments[0])),
                                 static_cast<int>(AsNumber(arguments[1])),
                                 static_cast<int>(AsNumber(arguments[2])),
                                 static_cast<int>(AsNumber(arguments[3])),
                                 static_cast<unsigned int>(
                                     AsNumber(arguments[4])),
                                 AsString(arguments[5]));
        }

        void SetClearColor(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 1)
            {
                throw std::runtime_error("Expected one argument.");
            }

            const Coloring::Color color =
                AsColor(Evaluate(state, *call.arguments[0]));

            Graphics::SetClearColor(color.red, color.green, color.blue,
                                    color.alpha);
        }

        void LoadImages(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 1)
            {
                throw std::runtime_error("Expected one argument.");
            }

            std::filesystem::path directory{
                AsString(Evaluate(state, *call.arguments[0]))};

            if (directory.is_relative())
            {
                directory = state.sourceDirectory / directory;
            }

            Graphics::LoadImages(directory.string());
        }

        void InitializeText(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 1)
            {
                throw std::runtime_error("Expected one argument.");
            }

            std::filesystem::path fontFile{
                AsString(Evaluate(state, *call.arguments[0]))};

            if (fontFile.is_relative())
            {
                fontFile = state.sourceDirectory / fontFile;
            }

            Graphics::InitializeText(fontFile.string());
        }

        void AddFontSizes(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 1)
            {
                throw std::runtime_error("Expected one argument.");
            }

            const Value value = Evaluate(state, *call.arguments[0]);
            const auto *sizes = std::get_if<std::vector<double>>(&value);

            if (sizes == nullptr)
            {
                throw std::runtime_error("Expected a list.");
            }

            std::vector<int> fontSizes;

            for (const double size : *sizes)
            {
                fontSizes.push_back(static_cast<int>(size));
            }

            Graphics::AddFontSizes(fontSizes);
        }

        void AddCursorStyle(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 2)
            {
                throw std::runtime_error("Expected two arguments.");
            }

            const std::string styleName =
                AsString(Evaluate(state, *call.arguments[0]));
            const std::string imageName =
                AsString(Evaluate(state, *call.arguments[1]));

            Graphics::AddCursorStyle(styleName, imageName);
        }

        void SetDefaultCursorStyle(ExecutionState &state,
                                   const FunctionCall &call)
        {
            if (call.arguments.size() != 1)
            {
                throw std::runtime_error("Expected one argument.");
            }

            Graphics::SetDefaultCursorStyle(
                AsString(Evaluate(state, *call.arguments[0])));
        }

        void DrawImage(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 5)
            {
                throw std::runtime_error("Expected five arguments.");
            }

            const std::string name =
                AsString(Evaluate(state, *call.arguments[0]));
            const double x = AsNumber(Evaluate(state, *call.arguments[1]));
            const double y = AsNumber(Evaluate(state, *call.arguments[2]));
            const double width = AsNumber(Evaluate(state, *call.arguments[3]));
            const double height =
                AsNumber(Evaluate(state, *call.arguments[4]));

            Graphics::DrawImage(name, x, y, width, height);
        }

        void DrawString(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 5)
            {
                throw std::runtime_error("Expected five arguments.");
            }

            const std::string text =
                AsString(Evaluate(state, *call.arguments[0]));
            const double x = AsNumber(Evaluate(state, *call.arguments[1]));
            const double y = AsNumber(Evaluate(state, *call.arguments[2]));
            const int fontSize = static_cast<int>(
                AsNumber(Evaluate(state, *call.arguments[3])));
            const bool centered = IsTrue(Evaluate(state, *call.arguments[4]));

            Graphics::DrawString(text, x, y, fontSize, centered);
        }

        void AddScene(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 2)
            {
                throw std::runtime_error("Expected two arguments.");
            }

            const std::string name =
                AsString(Evaluate(state, *call.arguments[0]));
            const Value value = Evaluate(state, *call.arguments[1]);
            const auto *scene = std::get_if<SceneObject>(&value);

            if (scene == nullptr)
            {
                throw std::runtime_error("Expected a scene.");
            }

            if (!state.sceneTypes.contains(scene->typeName))
            {
                throw std::runtime_error("Unknown scene.");
            }

            state.addedScenes[name] = scene->typeName;
        }

        void GoToScene(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 1)
            {
                throw std::runtime_error("Expected one argument.");
            }

            const std::string name =
                AsString(Evaluate(state, *call.arguments[0]));
            const auto found = state.addedScenes.find(name);

            if (found == state.addedScenes.end())
            {
                throw std::runtime_error("Unknown scene.");
            }

            state.currentSceneType = found->second;

            const auto sceneType =
                state.sceneTypes.find(state.currentSceneType);

            if (sceneType == state.sceneTypes.end() ||
                sceneType->second.onEnter == nullptr)
            {
                return;
            }

            ExecuteBlock(state, *sceneType->second.onEnter);
            std::cout.flush();
        }

        void
        ExecuteBlock(ExecutionState &state,
                     const std::vector<std::unique_ptr<Statement>> &statements)
        {
            for (const auto &statement : statements)
            {
                ExecuteStatement(state, *statement);
            }
        }

        void ExecuteStatement(ExecutionState &state, const Statement &statement)
        {
            if (const auto *declaration =
                    dynamic_cast<const IntStatement *>(&statement))
            {
                state.variables[declaration->name] =
                    Evaluate(state, *declaration->value);
                return;
            }

            if (const auto *print =
                    dynamic_cast<const PrintStatement *>(&statement))
            {
                PrintValue(Evaluate(state, *print->expression));
                return;
            }

            if (const auto *conditional =
                    dynamic_cast<const IfStatement *>(&statement))
            {
                if (IsTrue(Evaluate(state, *conditional->condition)))
                {
                    ExecuteBlock(state, conditional->thenBranch);
                }
                else
                {
                    ExecuteBlock(state, conditional->elseBranch);
                }

                return;
            }

            if (dynamic_cast<const FunctionDeclaration *>(&statement) ||
                dynamic_cast<const ClassDeclaration *>(&statement) ||
                dynamic_cast<const SceneDeclaration *>(&statement))
            {
                return;
            }

            if (const auto *import =
                    dynamic_cast<const ImportStatement *>(&statement))
            {
                if (Graphics::IsModule(import->moduleName) ||
                    Coloring::IsModule(import->moduleName) ||
                    ScenesCore::IsModule(import->moduleName))
                {
                    return;
                }

                throw std::runtime_error("Unknown module.");
            }

            if (const auto *object =
                    dynamic_cast<const ObjectStatement *>(&statement))
            {
                if (Coloring::IsColorType(object->typeName))
                {
                    if (object->arguments.size() != 4)
                    {
                        throw std::runtime_error("Expected four arguments.");
                    }

                    const double red =
                        AsNumber(Evaluate(state, *object->arguments[0]));
                    const double green =
                        AsNumber(Evaluate(state, *object->arguments[1]));
                    const double blue =
                        AsNumber(Evaluate(state, *object->arguments[2]));
                    const double alpha =
                        AsNumber(Evaluate(state, *object->arguments[3]));

                    state.variables[object->name] =
                        Coloring::Color{red, green, blue, alpha};
                    return;
                }

                if (state.sceneTypes.contains(object->typeName))
                {
                    if (!object->arguments.empty())
                    {
                        throw std::runtime_error("Unexpected arguments.");
                    }

                    state.variables[object->name] =
                        SceneObject{object->typeName};
                    return;
                }

                if (!state.classes.contains(object->typeName))
                {
                    throw std::runtime_error("Unknown class.");
                }

                if (!object->arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                state.variables[object->name] = Object{object->typeName};
                return;
            }

            if (const auto *method =
                    dynamic_cast<const MethodCall *>(&statement))
            {
                const auto variable = state.variables.find(method->objectName);

                if (variable == state.variables.end())
                {
                    throw std::runtime_error("Unknown variable.");
                }

                const auto *instance = std::get_if<Object>(&variable->second);

                if (instance == nullptr)
                {
                    throw std::runtime_error("Expected an object.");
                }

                const auto classInfo = state.classes.find(instance->className);

                if (classInfo == state.classes.end())
                {
                    throw std::runtime_error("Unknown class.");
                }

                const auto found =
                    classInfo->second.methods.find(method->methodName);

                if (found == classInfo->second.methods.end())
                {
                    throw std::runtime_error("Unknown method.");
                }

                ExecuteBlock(state, found->second->body);
                return;
            }

            if (const auto *call =
                    dynamic_cast<const FunctionCall *>(&statement))
            {
                if (call->name == "InitializeGraphics")
                {
                    InitializeGraphics(state, *call);
                    return;
                }

                if (call->name == "SetClearColor")
                {
                    SetClearColor(state, *call);
                    return;
                }

                if (call->name == "LoadImages")
                {
                    LoadImages(state, *call);
                    return;
                }

                if (call->name == "InitializeText")
                {
                    InitializeText(state, *call);
                    return;
                }

                if (call->name == "AddFontSizes")
                {
                    AddFontSizes(state, *call);
                    return;
                }

                if (call->name == "AddCursorStyle")
                {
                    AddCursorStyle(state, *call);
                    return;
                }

                if (call->name == "SetDefaultCursorStyle")
                {
                    SetDefaultCursorStyle(state, *call);
                    return;
                }

                if (call->name == "DrawImage")
                {
                    DrawImage(state, *call);
                    return;
                }

                if (call->name == "DrawString")
                {
                    DrawString(state, *call);
                    return;
                }

                if (call->name == "AddScene")
                {
                    AddScene(state, *call);
                    return;
                }

                if (call->name == "GoToScene")
                {
                    GoToScene(state, *call);
                    return;
                }

                const auto found = state.functions.find(call->name);

                if (found == state.functions.end())
                {
                    throw std::runtime_error("Unknown function.");
                }

                ExecuteBlock(state, found->second->body);
                return;
            }

            throw std::runtime_error("Unknown statement.");
        }

        void RegisterFunctions(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Statement>> &statements)
        {
            for (const auto &statement : statements)
            {
                if (const auto *function =
                        dynamic_cast<const FunctionDeclaration *>(
                            statement.get()))
                {
                    state.functions[function->name] = function;
                }

                if (const auto *declaration =
                        dynamic_cast<const ClassDeclaration *>(statement.get()))
                {
                    ClassInfo info;

                    for (const auto &method : declaration->methods)
                    {
                        info.methods[method->name] = method.get();
                    }

                    state.classes[declaration->name] = std::move(info);
                }

                if (const auto *scene =
                        dynamic_cast<const SceneDeclaration *>(statement.get()))
                {
                    state.sceneTypes[scene->name] = SceneType{
                        &scene->update,
                        &scene->draw,
                        &scene->onMouseDown,
                        &scene->onKeyDown,
                        &scene->onEnter,
                        scene->onMouseDownParameter,
                        scene->onKeyDownParameter};
                }
            }
        }

        void RunSceneUpdate(ExecutionState &state)
        {
            if (state.currentSceneType.empty())
            {
                return;
            }

            const auto found = state.sceneTypes.find(state.currentSceneType);

            if (found == state.sceneTypes.end() ||
                found->second.update == nullptr)
            {
                return;
            }

            ExecuteBlock(state, *found->second.update);
            std::cout.flush();
        }

        void RunSceneDraw(ExecutionState &state)
        {
            if (state.currentSceneType.empty())
            {
                return;
            }

            const auto found = state.sceneTypes.find(state.currentSceneType);

            if (found == state.sceneTypes.end() ||
                found->second.draw == nullptr)
            {
                return;
            }

            ExecuteBlock(state, *found->second.draw);
        }

        void RunSceneMouseDown(ExecutionState &state, int button)
        {
            if (state.currentSceneType.empty())
            {
                return;
            }

            const auto found = state.sceneTypes.find(state.currentSceneType);

            if (found == state.sceneTypes.end() ||
                found->second.onMouseDown == nullptr)
            {
                return;
            }

            const std::string &parameter = found->second.onMouseDownParameter;
            const bool hasParameter = !parameter.empty();
            const bool hadVariable =
                hasParameter && state.variables.contains(parameter);
            Value previous;

            if (hadVariable)
            {
                previous = state.variables[parameter];
            }

            if (hasParameter)
            {
                state.variables[parameter] = static_cast<double>(button);
            }

            ExecuteBlock(state, *found->second.onMouseDown);

            if (hadVariable)
            {
                state.variables[parameter] = previous;
            }
            else if (hasParameter)
            {
                state.variables.erase(parameter);
            }

            std::cout.flush();
        }

        void RunSceneKeyDown(ExecutionState &state, int key)
        {
            if (state.currentSceneType.empty())
            {
                return;
            }

            const auto found = state.sceneTypes.find(state.currentSceneType);

            if (found == state.sceneTypes.end() ||
                found->second.onKeyDown == nullptr)
            {
                return;
            }

            const std::string &parameter = found->second.onKeyDownParameter;
            const bool hasParameter = !parameter.empty();
            const bool hadVariable =
                hasParameter && state.variables.contains(parameter);
            Value previous;

            if (hadVariable)
            {
                previous = state.variables[parameter];
            }

            if (hasParameter)
            {
                state.variables[parameter] = static_cast<double>(key);
            }

            ExecuteBlock(state, *found->second.onKeyDown);

            if (hadVariable)
            {
                state.variables[parameter] = previous;
            }
            else if (hasParameter)
            {
                state.variables.erase(parameter);
            }

            std::cout.flush();
        }
    }

    void Interpreter::Execute(
        const std::vector<std::unique_ptr<Statement>> &statements,
        const std::filesystem::path &sourceDirectory)
    {
        struct Guard
        {
            ~Guard()
            {
                Graphics::Shutdown();
            }
        } guard;

        ExecutionState state;
        state.sourceDirectory = sourceDirectory;
        state.variables["FRD_Fullscreen"] = Graphics::FullscreenFlag();
        state.variables["FRD_Windowed"] = Graphics::WindowedFlag();
        state.variables["True"] = 1.0;
        state.variables["False"] = 0.0;

        RegisterFunctions(state, statements);
        ExecuteBlock(state, statements);
        std::cout.flush();
        Graphics::RunUntilClosed(
            [&state]() { RunSceneUpdate(state); },
            [&state]() { RunSceneDraw(state); },
            [&state](int button) { RunSceneMouseDown(state, button); },
            [&state](int key) { RunSceneKeyDown(state, key); });
    }
}
