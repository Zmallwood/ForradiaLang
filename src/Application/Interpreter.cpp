#include "Interpreter.hpp"

#include <cmath>
#include <optional>
#include <unordered_set>

#include "Coloring.hpp"
#include "Expressions/BinaryExpression.hpp"
#include "Expressions/CallExpression.hpp"
#include "Expressions/IndexExpression.hpp"
#include "Expressions/ListExpression.hpp"
#include "Expressions/MemberExpression.hpp"
#include "Expressions/NumberExpression.hpp"
#include "Expressions/StringExpression.hpp"
#include "Expressions/VariableExpression.hpp"
#include "Graphics.hpp"
#include "ScenesCore.hpp"
#include "Statements/AssignmentStatement.hpp"
#include "Statements/ClassDeclaration.hpp"
#include "Statements/ContinueStatement.hpp"
#include "Statements/ForStatement.hpp"
#include "Statements/FunctionCall.hpp"
#include "Statements/FunctionDeclaration.hpp"
#include "Statements/GroupDeclaration.hpp"
#include "Statements/IfStatement.hpp"
#include "Statements/ImportStatement.hpp"
#include "Statements/IntStatement.hpp"
#include "Statements/MethodCall.hpp"
#include "Statements/ObjectStatement.hpp"
#include "Statements/PrintStatement.hpp"
#include "Statements/ReturnStatement.hpp"
#include "Statements/SceneDeclaration.hpp"

namespace ForradiaLang
{
    namespace
    {
        struct Point
        {
            double x{0.0};
            double y{0.0};
        };

        struct Size
        {
            double width{0.0};
            double height{0.0};
        };

        struct GroupRef
        {
            std::string name;
        };

        struct Object
        {
            std::string className;
            int id{0};
        };

        struct FieldInfo
        {
            std::string typeName;
            std::string name;
            const Expression *value{nullptr};
            bool isConstant{false};
        };

        struct ClassInfo
        {
            std::unordered_map<std::string, const FunctionDeclaration *>
                methods;
            std::vector<FieldInfo> fields;
            const std::vector<std::unique_ptr<Statement>> *constructor{nullptr};
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

        struct ListRef
        {
            int id{0};
        };

        using Value = std::variant<double, std::string, Object, Coloring::Color,
                                   SceneObject, std::vector<double>, Point, Size,
                                   GroupRef, ListRef>;

        struct ListData
        {
            std::string elementType;
            std::vector<Value> elements;
        };

        struct ExecutionState
        {
            std::unordered_map<std::string, Value> variables;
            std::unordered_map<std::string, const FunctionDeclaration *>
                functions;
            std::unordered_map<std::string, ClassInfo> classes;
            std::unordered_map<std::string, SceneType> sceneTypes;
            std::unordered_map<std::string, std::string> addedScenes;
            std::unordered_map<std::string,
                               std::unordered_map<std::string, Value>>
                groups;
            std::unordered_set<std::string> constants;
            std::unordered_map<std::string, std::unordered_set<std::string>>
                groupConstants;
            std::unordered_map<int, std::string> objectClasses;
            std::string currentGroup;
            std::string currentSceneType;
            std::filesystem::path sourceDirectory;
            int nextObjectId{1};
            int nextListId{1};
            int currentObjectId{0};
            std::unordered_map<int, std::unordered_map<std::string, Value>>
                objectFields;
            std::unordered_map<int, ListData> lists;
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

        bool NameIsConstant(const ExecutionState &state,
                            const std::string &name)
        {
            if (!state.currentGroup.empty())
            {
                const auto group = state.groups.find(state.currentGroup);

                if (group != state.groups.end() &&
                    group->second.contains(name))
                {
                    const auto constants =
                        state.groupConstants.find(state.currentGroup);

                    return constants != state.groupConstants.end() &&
                           constants->second.contains(name);
                }
            }

            return state.constants.contains(name);
        }

        void DefineName(ExecutionState &state, const std::string &name,
                        Value value, bool isConstant)
        {
            if (state.currentGroup.empty())
            {
                if (state.constants.contains(name))
                {
                    throw std::runtime_error("Cannot change a constant.");
                }

                state.variables[name] = std::move(value);

                if (isConstant)
                {
                    state.constants.insert(name);
                }

                return;
            }

            const auto constants = state.groupConstants.find(state.currentGroup);

            if (constants != state.groupConstants.end() &&
                constants->second.contains(name))
            {
                throw std::runtime_error("Cannot change a constant.");
            }

            state.groups[state.currentGroup][name] = std::move(value);

            if (isConstant)
            {
                state.groupConstants[state.currentGroup].insert(name);
            }
        }

        Value LookupName(ExecutionState &state, const std::string &name)
        {
            if (!state.currentGroup.empty())
            {
                const auto group = state.groups.find(state.currentGroup);

                if (group != state.groups.end())
                {
                    const auto member = group->second.find(name);

                    if (member != group->second.end())
                    {
                        return member->second;
                    }
                }
            }

            const auto found = state.variables.find(name);

            if (found != state.variables.end())
            {
                return found->second;
            }

            if (state.currentObjectId != 0)
            {
                const auto fields =
                    state.objectFields.find(state.currentObjectId);

                if (fields != state.objectFields.end())
                {
                    const auto field = fields->second.find(name);

                    if (field != fields->second.end())
                    {
                        return field->second;
                    }
                }
            }

            throw std::runtime_error("Unknown variable.");
        }

        Value Evaluate(ExecutionState &state, const Expression &expression);

        Value EvaluateField(ExecutionState &state, const FieldInfo &field);

        bool IsListType(std::string_view typeName);

        std::string ListElementType(std::string_view typeName);

        ListRef MakeList(ExecutionState &state,
                         const std::string &elementType);

        void ExecuteBlock(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Statement>> &statements);

        Object MakeInstance(ExecutionState &state, const std::string &className)
        {
            const auto classInfo = state.classes.find(className);

            if (classInfo == state.classes.end())
            {
                throw std::runtime_error("Unknown class.");
            }

            const std::vector<FieldInfo> fieldInfos = classInfo->second.fields;
            const auto *constructor = classInfo->second.constructor;
            Object instance{className, state.nextObjectId++};
            state.objectClasses[instance.id] = className;
            const int previousObject = state.currentObjectId;
            const std::string previousGroup = state.currentGroup;
            state.currentObjectId = instance.id;
            state.currentGroup.clear();

            try
            {
                for (const auto &field : fieldInfos)
                {
                    Value value = EvaluateField(state, field);
                    state.objectFields[instance.id][field.name] =
                        std::move(value);
                }

                if (constructor != nullptr)
                {
                    ExecuteBlock(state, *constructor);
                }
            }
            catch (...)
            {
                state.currentObjectId = previousObject;
                state.currentGroup = previousGroup;
                throw;
            }

            state.currentObjectId = previousObject;
            state.currentGroup = previousGroup;
            return instance;
        }

        Point MakePoint(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Expression>> &arguments)
        {
            if (arguments.size() != 2)
            {
                throw std::runtime_error("Expected two arguments.");
            }

            return Point{AsNumber(Evaluate(state, *arguments[0])),
                         AsNumber(Evaluate(state, *arguments[1]))};
        }

        Size MakeSize(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Expression>> &arguments)
        {
            if (arguments.size() != 2)
            {
                throw std::runtime_error("Expected two arguments.");
            }

            return Size{AsNumber(Evaluate(state, *arguments[0])),
                        AsNumber(Evaluate(state, *arguments[1]))};
        }

        struct ContinueSignal
        {
        };

        struct ReturnSignal
        {
            Value value;
        };

        void ExecuteStatement(ExecutionState &state,
                              const Statement &statement);

        std::optional<Value> CallObjectMethod(ExecutionState &state,
                                              const Object &instance,
                                              const std::string &methodName);

        Value EvaluateBinary(ExecutionState &state,
                             const BinaryExpression &expression)
        {
            if (expression.operation == '&')
            {
                if (!IsTrue(Evaluate(state, *expression.left)))
                {
                    return 0.0;
                }

                return IsTrue(Evaluate(state, *expression.right)) ? 1.0 : 0.0;
            }

            if (expression.operation == '|')
            {
                if (IsTrue(Evaluate(state, *expression.left)))
                {
                    return 1.0;
                }

                return IsTrue(Evaluate(state, *expression.right)) ? 1.0 : 0.0;
            }

            const double left = AsNumber(Evaluate(state, *expression.left));
            const double right = AsNumber(Evaluate(state, *expression.right));

            switch (expression.operation)
            {
            case '+':
                return left + right;

            case '-':
                return left - right;

            case '*':
                return left * right;

            case '/':
                if (right == 0.0)
                {
                    throw std::runtime_error("Expected a non-zero number.");
                }

                return left / right;

            case '>':
                return left > right ? 1.0 : 0.0;

            case 'G':
                return left >= right ? 1.0 : 0.0;

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

            if (expression.name == "ConvertWidthToHeight")
            {
                if (expression.arguments.size() != 1)
                {
                    throw std::runtime_error("Expected one argument.");
                }

                const double width =
                    AsNumber(Evaluate(state, *expression.arguments[0]));

                return Graphics::ConvertWidthToHeight(width);
            }

            if (expression.name == "Point")
            {
                return MakePoint(state, expression.arguments);
            }

            if (expression.name == "Size")
            {
                return MakeSize(state, expression.arguments);
            }

            if (IsListType(expression.name))
            {
                if (!expression.arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                return MakeList(state, ListElementType(expression.name));
            }

            if (state.classes.contains(expression.name))
            {
                if (!expression.arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                return MakeInstance(state, expression.name);
            }

            throw std::runtime_error("Unknown function.");
        }

        std::size_t AsIndex(const Value &value)
        {
            const double number = AsNumber(value);

            if (number < 0.0 || std::floor(number) != number)
            {
                throw std::runtime_error("Expected an index.");
            }

            return static_cast<std::size_t>(number);
        }

        Value EvaluateIndex(ExecutionState &state,
                            const IndexExpression &expression)
        {
            const std::size_t index =
                AsIndex(Evaluate(state, *expression.index));
            const Value object = Evaluate(state, *expression.object);
            const auto *list = std::get_if<ListRef>(&object);

            if (list == nullptr)
            {
                throw std::runtime_error("Expected a list.");
            }

            const auto found = state.lists.find(list->id);

            if (found == state.lists.end())
            {
                throw std::runtime_error("Expected a list.");
            }

            if (index >= found->second.elements.size())
            {
                throw std::runtime_error("Unknown index.");
            }

            return found->second.elements[index];
        }

        Value LookupGroupMember(ExecutionState &state,
                                const std::string &groupName,
                                const std::string &memberName)
        {
            const auto group = state.groups.find(groupName);

            if (group != state.groups.end())
            {
                const auto member = group->second.find(memberName);

                if (member != group->second.end())
                {
                    return member->second;
                }
            }

            const std::string nested = groupName + "." + memberName;

            if (state.groups.contains(nested))
            {
                return GroupRef{nested};
            }

            throw std::runtime_error("Unknown member.");
        }

        Value EvaluateMember(ExecutionState &state,
                             const MemberExpression &expression)
        {
            if (const auto *variable = dynamic_cast<const VariableExpression *>(
                    expression.object.get()))
            {
                if (state.groups.contains(variable->name))
                {
                    return LookupGroupMember(state, variable->name,
                                             expression.memberName);
                }

                if (variable->name == "MouseButtons")
                {
                    if (expression.memberName == "Left")
                    {
                        return static_cast<double>(SDL_BUTTON_LEFT);
                    }

                    if (expression.memberName == "Right")
                    {
                        return static_cast<double>(SDL_BUTTON_RIGHT);
                    }

                    throw std::runtime_error("Unknown member.");
                }

                if (variable->name == "Keys" &&
                    expression.memberName.size() == 1)
                {
                    const char letter = expression.memberName[0];

                    if (letter >= 'A' && letter <= 'Z')
                    {
                        return static_cast<double>(SDLK_a + (letter - 'A'));
                    }

                    throw std::runtime_error("Unknown member.");
                }
            }

            const Value object = Evaluate(state, *expression.object);

            if (const auto *group = std::get_if<GroupRef>(&object))
            {
                return LookupGroupMember(state, group->name,
                                         expression.memberName);
            }

            if (const auto *point = std::get_if<Point>(&object))
            {
                if (expression.memberName == "x")
                {
                    return point->x;
                }

                if (expression.memberName == "y")
                {
                    return point->y;
                }

                throw std::runtime_error("Unknown member.");
            }

            if (const auto *size = std::get_if<Size>(&object))
            {
                if (expression.memberName == "width")
                {
                    return size->width;
                }

                if (expression.memberName == "height")
                {
                    return size->height;
                }

                throw std::runtime_error("Unknown member.");
            }

            if (const auto *list = std::get_if<ListRef>(&object))
            {
                if (expression.memberName != "Count")
                {
                    throw std::runtime_error("Unknown member.");
                }

                const auto found = state.lists.find(list->id);

                if (found == state.lists.end())
                {
                    throw std::runtime_error("Expected a list.");
                }

                return static_cast<double>(found->second.elements.size());
            }

            if (const auto *instance = std::get_if<Object>(&object))
            {
                const auto fields = state.objectFields.find(instance->id);

                if (fields != state.objectFields.end())
                {
                    const auto field =
                        fields->second.find(expression.memberName);

                    if (field != fields->second.end())
                    {
                        return field->second;
                    }
                }

                const auto classInfo = state.classes.find(instance->className);

                if (classInfo == state.classes.end() ||
                    !classInfo->second.methods.contains(expression.memberName))
                {
                    throw std::runtime_error("Unknown member.");
                }

                const std::optional<Value> returned =
                    CallObjectMethod(state, *instance, expression.memberName);

                if (!returned.has_value())
                {
                    throw std::runtime_error("Expected a value.");
                }

                return *returned;
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
                return LookupName(state, variable->name);
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
                return EvaluateMember(state, *member);
            }

            if (const auto *index =
                    dynamic_cast<const IndexExpression *>(&expression))
            {
                return EvaluateIndex(state, *index);
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

        bool IsListType(std::string_view typeName)
        {
            if (!typeName.starts_with("List<") || typeName.size() < 7 ||
                typeName.back() != '>')
            {
                return false;
            }

            int depth = 0;

            for (std::size_t index = 4; index < typeName.size(); ++index)
            {
                const char character = typeName[index];

                if (character == '<')
                {
                    ++depth;
                }
                else if (character == '>')
                {
                    --depth;
                }

                if (depth == 0)
                {
                    return index + 1 == typeName.size();
                }
            }

            return false;
        }

        std::string ListElementType(std::string_view typeName)
        {
            return std::string(typeName.substr(5, typeName.size() - 6));
        }

        ListRef MakeList(ExecutionState &state, const std::string &elementType)
        {
            const int id = state.nextListId++;
            state.lists.insert({id, ListData{elementType, {}}});
            return ListRef{id};
        }

        void ExpectElement(ExecutionState &state, const Value &value,
                           const std::string &typeName)
        {
            if (typeName == "Int" || typeName == "Double")
            {
                if (!std::holds_alternative<double>(value))
                {
                    throw std::runtime_error("Expected a number.");
                }

                return;
            }

            if (typeName == "String")
            {
                if (!std::holds_alternative<std::string>(value))
                {
                    throw std::runtime_error("Expected a string.");
                }

                return;
            }

            if (typeName == "Point")
            {
                if (!std::holds_alternative<Point>(value))
                {
                    throw std::runtime_error("Expected a point.");
                }

                return;
            }

            if (typeName == "Size")
            {
                if (!std::holds_alternative<Size>(value))
                {
                    throw std::runtime_error("Expected a size.");
                }

                return;
            }

            if (Coloring::IsColorType(typeName))
            {
                if (!std::holds_alternative<Coloring::Color>(value))
                {
                    throw std::runtime_error("Expected a color.");
                }

                return;
            }

            if (IsListType(typeName))
            {
                const auto *list = std::get_if<ListRef>(&value);

                if (list == nullptr)
                {
                    throw std::runtime_error("Expected a list.");
                }

                const auto found = state.lists.find(list->id);

                if (found == state.lists.end() ||
                    found->second.elementType != ListElementType(typeName))
                {
                    throw std::runtime_error("Expected a list.");
                }

                return;
            }

            const auto *instance = std::get_if<Object>(&value);

            if (instance == nullptr || instance->className != typeName)
            {
                throw std::runtime_error("Expected an object.");
            }
        }

        void AddElement(ExecutionState &state, const ListRef &list,
                        Value element)
        {
            const auto found = state.lists.find(list.id);

            if (found == state.lists.end())
            {
                throw std::runtime_error("Expected a list.");
            }

            ExpectElement(state, element, found->second.elementType);
            found->second.elements.push_back(std::move(element));
        }

        Value DefaultField(ExecutionState &state, const std::string &typeName)
        {
            if (typeName == "Int" || typeName == "Double")
            {
                return 0.0;
            }

            if (typeName == "String")
            {
                return std::string{};
            }

            if (typeName == "Point")
            {
                return Point{};
            }

            if (typeName == "Size")
            {
                return Size{};
            }

            if (Coloring::IsColorType(typeName))
            {
                return Coloring::Color{};
            }

            if (IsListType(typeName))
            {
                return MakeList(state, ListElementType(typeName));
            }

            if (!state.classes.contains(typeName))
            {
                throw std::runtime_error("Unknown type.");
            }

            return MakeInstance(state, typeName);
        }

        Value EvaluateField(ExecutionState &state, const FieldInfo &field)
        {
            if (field.value == nullptr)
            {
                return DefaultField(state, field.typeName);
            }

            const Value value = Evaluate(state, *field.value);

            if (field.typeName == "Point")
            {
                if (!std::holds_alternative<Point>(value))
                {
                    throw std::runtime_error("Expected a point.");
                }

                return value;
            }

            if (field.typeName == "Size")
            {
                if (!std::holds_alternative<Size>(value))
                {
                    throw std::runtime_error("Expected a size.");
                }

                return value;
            }

            if (field.typeName == "Int" || field.typeName == "Double")
            {
                if (!std::holds_alternative<double>(value))
                {
                    throw std::runtime_error("Expected a number.");
                }

                return value;
            }

            if (field.typeName == "String")
            {
                if (!std::holds_alternative<std::string>(value))
                {
                    throw std::runtime_error("Expected a string.");
                }

                return value;
            }

            if (IsListType(field.typeName))
            {
                const auto *list = std::get_if<ListRef>(&value);

                if (list == nullptr)
                {
                    throw std::runtime_error("Expected a list.");
                }

                const auto found = state.lists.find(list->id);

                if (found == state.lists.end() ||
                    found->second.elementType != ListElementType(field.typeName))
                {
                    throw std::runtime_error("Expected a list.");
                }

                return value;
            }

            if (Coloring::IsColorType(field.typeName))
            {
                if (!std::holds_alternative<Coloring::Color>(value))
                {
                    throw std::runtime_error("Expected a color.");
                }

                return value;
            }

            const auto found = state.classes.find(field.typeName);

            if (found == state.classes.end())
            {
                throw std::runtime_error("Unknown type.");
            }

            const auto *instance = std::get_if<Object>(&value);

            if (instance == nullptr || instance->className != field.typeName)
            {
                throw std::runtime_error("Expected an object.");
            }

            return value;
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

        std::optional<Value> CallObjectMethod(ExecutionState &state,
                                              const Object &instance,
                                              const std::string &methodName)
        {
            const auto classInfo = state.classes.find(instance.className);

            if (classInfo == state.classes.end())
            {
                throw std::runtime_error("Unknown class.");
            }

            const auto found = classInfo->second.methods.find(methodName);

            if (found == classInfo->second.methods.end())
            {
                throw std::runtime_error("Unknown method.");
            }

            const FunctionDeclaration *function = found->second;
            const int previousObject = state.currentObjectId;
            const std::string previousGroup = state.currentGroup;
            state.currentObjectId = instance.id;
            state.currentGroup.clear();

            try
            {
                ExecuteBlock(state, function->body);
            }
            catch (const ReturnSignal &returned)
            {
                state.currentObjectId = previousObject;
                state.currentGroup = previousGroup;

                if (!function->returnType.empty())
                {
                    ExpectElement(state, returned.value, function->returnType);
                }

                return returned.value;
            }
            catch (...)
            {
                state.currentObjectId = previousObject;
                state.currentGroup = previousGroup;
                throw;
            }

            state.currentObjectId = previousObject;
            state.currentGroup = previousGroup;
            return std::nullopt;
        }

        struct Slot
        {
            Value *value{nullptr};
            bool isConstant{false};
        };

        bool FieldIsConstant(const ExecutionState &state,
                             const std::string &className,
                             const std::string &fieldName)
        {
            const auto info = state.classes.find(className);

            if (info == state.classes.end())
            {
                return false;
            }

            for (const auto &field : info->second.fields)
            {
                if (field.name == fieldName)
                {
                    return field.isConstant;
                }
            }

            return false;
        }

        Slot FindBinding(ExecutionState &state, const std::string &name)
        {
            if (!state.currentGroup.empty())
            {
                const auto group = state.groups.find(state.currentGroup);

                if (group != state.groups.end())
                {
                    const auto member = group->second.find(name);

                    if (member != group->second.end())
                    {
                        const auto constants =
                            state.groupConstants.find(state.currentGroup);
                        const bool isConstant =
                            constants != state.groupConstants.end() &&
                            constants->second.contains(name);

                        return Slot{&member->second, isConstant};
                    }
                }
            }

            const auto found = state.variables.find(name);

            if (found != state.variables.end())
            {
                return Slot{&found->second, state.constants.contains(name)};
            }

            if (state.currentObjectId != 0)
            {
                const auto fields = state.objectFields.find(state.currentObjectId);

                if (fields != state.objectFields.end())
                {
                    const auto field = fields->second.find(name);

                    if (field != fields->second.end())
                    {
                        const auto className =
                            state.objectClasses.find(state.currentObjectId);
                        const bool isConstant =
                            className != state.objectClasses.end() &&
                            FieldIsConstant(state, className->second, name);

                        return Slot{&field->second, isConstant};
                    }
                }
            }

            return {};
        }

        std::string GroupName(const ExecutionState &state,
                              const Expression &expression)
        {
            if (const auto *variable =
                    dynamic_cast<const VariableExpression *>(&expression))
            {
                if (state.groups.contains(variable->name))
                {
                    return variable->name;
                }

                return {};
            }

            if (const auto *member =
                    dynamic_cast<const MemberExpression *>(&expression))
            {
                const std::string parent = GroupName(state, *member->object);

                if (!parent.empty())
                {
                    const std::string nested =
                        parent + "." + member->memberName;

                    if (state.groups.contains(nested))
                    {
                        return nested;
                    }
                }
            }

            return {};
        }

        Slot GroupMemberSlot(ExecutionState &state, const std::string &groupName,
                             const std::string &memberName)
        {
            const auto group = state.groups.find(groupName);

            if (group == state.groups.end())
            {
                return {};
            }

            const auto member = group->second.find(memberName);

            if (member == group->second.end())
            {
                return {};
            }

            const auto constants = state.groupConstants.find(groupName);
            const bool isConstant = constants != state.groupConstants.end() &&
                                    constants->second.contains(memberName);

            return Slot{&member->second, isConstant};
        }

        Slot ResolveSlot(ExecutionState &state, const Expression &expression)
        {
            if (const auto *variable =
                    dynamic_cast<const VariableExpression *>(&expression))
            {
                return FindBinding(state, variable->name);
            }

            if (const auto *member =
                    dynamic_cast<const MemberExpression *>(&expression))
            {
                const std::string group = GroupName(state, *member->object);

                if (!group.empty())
                {
                    return GroupMemberSlot(state, group, member->memberName);
                }

                const Slot parent = ResolveSlot(state, *member->object);

                if (parent.value == nullptr)
                {
                    return {};
                }

                const auto *instance = std::get_if<Object>(parent.value);

                if (instance == nullptr)
                {
                    return {};
                }

                const auto fields = state.objectFields.find(instance->id);

                if (fields == state.objectFields.end())
                {
                    return {};
                }

                const auto field = fields->second.find(member->memberName);

                if (field == fields->second.end())
                {
                    return {};
                }

                const bool isConstant =
                    parent.isConstant ||
                    FieldIsConstant(state, instance->className,
                                    member->memberName);

                return Slot{&field->second, isConstant};
            }

            return {};
        }

        void RejectConstant(bool isConstant)
        {
            if (isConstant)
            {
                throw std::runtime_error("Cannot change a constant.");
            }
        }

        void AssignTo(ExecutionState &state, const Expression &target,
                      Value value)
        {
            if (const auto *variable =
                    dynamic_cast<const VariableExpression *>(&target))
            {
                const Slot slot = FindBinding(state, variable->name);

                if (slot.value == nullptr)
                {
                    throw std::runtime_error("Unknown variable.");
                }

                RejectConstant(slot.isConstant);
                *slot.value = std::move(value);
                return;
            }

            if (const auto *member =
                    dynamic_cast<const MemberExpression *>(&target))
            {
                const std::string group = GroupName(state, *member->object);

                if (!group.empty())
                {
                    const Slot slot =
                        GroupMemberSlot(state, group, member->memberName);

                    if (slot.value == nullptr)
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(slot.isConstant);
                    *slot.value = std::move(value);
                    return;
                }

                const Slot parent = ResolveSlot(state, *member->object);

                if (parent.value == nullptr)
                {
                    throw std::runtime_error("Cannot assign.");
                }

                if (auto *size = std::get_if<Size>(parent.value))
                {
                    if (member->memberName != "width" &&
                        member->memberName != "height")
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(parent.isConstant);

                    if (member->memberName == "width")
                    {
                        size->width = AsNumber(value);
                    }
                    else
                    {
                        size->height = AsNumber(value);
                    }

                    return;
                }

                if (auto *point = std::get_if<Point>(parent.value))
                {
                    if (member->memberName != "x" && member->memberName != "y")
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(parent.isConstant);

                    if (member->memberName == "x")
                    {
                        point->x = AsNumber(value);
                    }
                    else
                    {
                        point->y = AsNumber(value);
                    }

                    return;
                }

                const auto *instance = std::get_if<Object>(parent.value);

                if (instance != nullptr)
                {
                    const auto fields = state.objectFields.find(instance->id);

                    if (fields == state.objectFields.end())
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    const auto field = fields->second.find(member->memberName);

                    if (field == fields->second.end())
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(
                        parent.isConstant ||
                        FieldIsConstant(state, instance->className,
                                        member->memberName));
                    field->second = std::move(value);
                    return;
                }

                throw std::runtime_error("Cannot assign.");
            }

            throw std::runtime_error("Cannot assign.");
        }

        void ExecuteStatement(ExecutionState &state, const Statement &statement)
        {
            if (const auto *declaration =
                    dynamic_cast<const IntStatement *>(&statement))
            {
                Value value = Evaluate(state, *declaration->value);

                if (declaration->typeName == "Point" &&
                    !std::holds_alternative<Point>(value))
                {
                    throw std::runtime_error("Expected a point.");
                }

                if (declaration->typeName == "Size" &&
                    !std::holds_alternative<Size>(value))
                {
                    throw std::runtime_error("Expected a size.");
                }

                DefineName(state, declaration->name, std::move(value),
                           declaration->isConstant);
                return;
            }

            if (const auto *assignment =
                    dynamic_cast<const AssignmentStatement *>(&statement))
            {
                AssignTo(state, *assignment->target,
                         Evaluate(state, *assignment->value));
                return;
            }

            if (const auto *loop =
                    dynamic_cast<const ForStatement *>(&statement))
            {
                const double start = AsNumber(Evaluate(state, *loop->start));
                const double end = AsNumber(Evaluate(state, *loop->end));

                if (NameIsConstant(state, loop->name))
                {
                    throw std::runtime_error("Cannot change a constant.");
                }

                for (double value = start; value <= end; value += 1.0)
                {
                    state.variables[loop->name] = value;

                    try
                    {
                        ExecuteBlock(state, loop->body);
                    }
                    catch (const ContinueSignal &)
                    {
                    }
                }

                return;
            }

            if (dynamic_cast<const ContinueStatement *>(&statement))
            {
                throw ContinueSignal{};
            }

            if (const auto *returned =
                    dynamic_cast<const ReturnStatement *>(&statement))
            {
                throw ReturnSignal{Evaluate(state, *returned->value)};
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

                    DefineName(state, object->name,
                               Coloring::Color{red, green, blue, alpha},
                               object->isConstant);
                    return;
                }

                if (object->typeName == "Point")
                {
                    DefineName(state, object->name,
                               MakePoint(state, object->arguments),
                               object->isConstant);
                    return;
                }

                if (object->typeName == "Size")
                {
                    DefineName(state, object->name,
                               MakeSize(state, object->arguments),
                               object->isConstant);
                    return;
                }

                if (state.sceneTypes.contains(object->typeName))
                {
                    if (!object->arguments.empty())
                    {
                        throw std::runtime_error("Unexpected arguments.");
                    }

                    DefineName(state, object->name, SceneObject{object->typeName},
                               object->isConstant);
                    return;
                }

                const auto classInfo = state.classes.find(object->typeName);

                if (classInfo == state.classes.end())
                {
                    throw std::runtime_error("Unknown class.");
                }

                if (!object->arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                DefineName(state, object->name,
                           MakeInstance(state, object->typeName),
                           object->isConstant);
                return;
            }

            if (const auto *group =
                    dynamic_cast<const GroupDeclaration *>(&statement))
            {
                const std::string previousGroup = state.currentGroup;

                if (previousGroup.empty())
                {
                    state.currentGroup = group->name;
                }
                else
                {
                    state.currentGroup = previousGroup + "." + group->name;
                }

                state.groups.try_emplace(state.currentGroup);

                try
                {
                    ExecuteBlock(state, group->body);
                }
                catch (...)
                {
                    state.currentGroup = previousGroup;
                    throw;
                }

                state.currentGroup = previousGroup;
                return;
            }

            if (const auto *method =
                    dynamic_cast<const MethodCall *>(&statement))
            {
                const Value receiver = Evaluate(state, *method->object);

                if (const auto *list = std::get_if<ListRef>(&receiver))
                {
                    if (method->methodName != "Add")
                    {
                        throw std::runtime_error("Unknown method.");
                    }

                    if (method->arguments.size() != 1)
                    {
                        throw std::runtime_error("Expected one argument.");
                    }

                    AddElement(state, *list,
                               Evaluate(state, *method->arguments[0]));
                    return;
                }

                const auto *instance = std::get_if<Object>(&receiver);

                if (instance == nullptr)
                {
                    throw std::runtime_error("Expected an object.");
                }

                CallObjectMethod(state, *instance, method->methodName);
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

                const std::string previousGroup = state.currentGroup;
                state.currentGroup.clear();

                try
                {
                    ExecuteBlock(state, found->second->body);
                }
                catch (const ReturnSignal &returned)
                {
                    state.currentGroup = previousGroup;

                    if (!found->second->returnType.empty())
                    {
                        ExpectElement(state, returned.value,
                                      found->second->returnType);
                    }

                    return;
                }
                catch (...)
                {
                    state.currentGroup = previousGroup;
                    throw;
                }

                state.currentGroup = previousGroup;
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

                    for (const auto &field : declaration->fields)
                    {
                        info.fields.push_back(FieldInfo{field.typeName, field.name,
                                                             field.value.get(),
                                                             field.isConstant});
                    }

                    if (declaration->hasConstructor)
                    {
                        info.constructor = &declaration->constructor;
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
                if (NameIsConstant(state, parameter))
                {
                    throw std::runtime_error("Cannot change a constant.");
                }

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
                if (NameIsConstant(state, parameter))
                {
                    throw std::runtime_error("Cannot change a constant.");
                }

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

        try
        {
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
        catch (const ContinueSignal &)
        {
            throw std::runtime_error("Unexpected statement.");
        }
        catch (const ReturnSignal &)
        {
            throw std::runtime_error("Unexpected statement.");
        }
    }
}
