#include "Interpreter.hpp"

#include <cmath>
#include <optional>
#include <random>
#include <string_view>
#include <unordered_set>

#include "Coloring.hpp"
#include "Expressions/BinaryExpression.hpp"
#include "Expressions/CallExpression.hpp"
#include "Expressions/IndexExpression.hpp"
#include "Expressions/ListExpression.hpp"
#include "Expressions/MemberExpression.hpp"
#include "Expressions/NumberExpression.hpp"
#include "Expressions/StringExpression.hpp"
#include "Expressions/UnaryExpression.hpp"
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
            std::unordered_map<std::string, bool> fieldConstant;
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
            const std::vector<std::unique_ptr<Statement>> *onKeyUp{nullptr};
            const std::vector<std::unique_ptr<Statement>> *onEnter{nullptr};
            std::string onMouseDownParameter;
            std::string onKeyDownParameter;
            std::string onKeyUpParameter;
        };

        struct ListRef
        {
            int id{0};
        };

        struct SetRef
        {
            int id{0};
        };

        struct Null
        {
        };

        using Value = std::variant<double, std::string, Object, Coloring::Color,
                                   SceneObject, std::vector<double>, Point, Size,
                                   GroupRef, ListRef, SetRef, Null>;

        enum class ControlFlow
        {
            None,
            Continue,
            Return
        };

        struct ListData
        {
            std::string elementType;
            std::vector<Value> elements;
        };

        struct SetData
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
            int nextSetId{1};
            int currentObjectId{0};
            ControlFlow controlFlow{ControlFlow::None};
            Value returnValue{Null{}};
            std::vector<std::vector<Value>> argumentFrames;
            std::size_t argumentDepth{0};
            std::unordered_map<int, std::unordered_map<std::string, Value>>
                objectFields;
            std::unordered_map<int, ListData> lists;
            std::unordered_map<int, SetData> sets;
        };

        const std::string &ObjectClassName(const ExecutionState &state,
                                           const Object &object)
        {
            const auto found = state.objectClasses.find(object.id);

            if (found == state.objectClasses.end())
            {
                throw std::runtime_error("Unknown object.");
            }

            return found->second;
        }

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

        std::string ToStringValue(const Value &value)
        {
            if (const auto *text = std::get_if<std::string>(&value))
            {
                return *text;
            }

            if (const auto *number = std::get_if<double>(&value))
            {
                if (std::isfinite(*number) && std::floor(*number) == *number)
                {
                    return std::to_string(static_cast<long long>(*number));
                }

                std::ostringstream stream;
                stream << *number;
                return stream.str();
            }

            throw std::runtime_error("Expected a value.");
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

            if (std::holds_alternative<Null>(value))
            {
                return false;
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
                if (state.constants.contains(name) && !isConstant)
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
                constants->second.contains(name) && !isConstant)
            {
                throw std::runtime_error("Cannot change a constant.");
            }

            state.groups[state.currentGroup][name] = std::move(value);

            if (isConstant)
            {
                state.groupConstants[state.currentGroup].insert(name);
            }
        }

        std::optional<Value>
        CallObjectMethod(ExecutionState &state, const Object &instance,
                         const std::string &methodName,
                         const std::vector<Value> &arguments);

        std::optional<Value>
        CallGlobalFunction(ExecutionState &state,
                           const FunctionDeclaration &function,
                           const std::vector<Value> &arguments);

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

                const auto className =
                    state.objectClasses.find(state.currentObjectId);

                if (className != state.objectClasses.end())
                {
                    const auto classInfo =
                        state.classes.find(className->second);

                    if (classInfo != state.classes.end() &&
                        classInfo->second.methods.contains(name))
                    {
                        const Object instance{state.currentObjectId};
                        const std::optional<Value> returned = CallObjectMethod(
                            state, instance, name, {});

                        if (!returned.has_value())
                        {
                            throw std::runtime_error("Expected a value.");
                        }

                        return *returned;
                    }
                }
            }

            if (name == "GetMousePosition")
            {
                double x = 0.0;
                double y = 0.0;
                Graphics::GetMousePosition(x, y);
                return Point{x, y};
            }

            throw std::runtime_error("Unknown variable.");
        }

        Value Evaluate(ExecutionState &state, const Expression &expression);

        std::vector<Value> &PushArguments(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Expression>> &arguments);

        void PopArguments(ExecutionState &state);

        Value EvaluateField(ExecutionState &state, const FieldInfo &field);

        bool IsPointType(std::string_view typeName)
        {
            return typeName == "Point" || typeName == "PointD";
        }

        bool IsListType(std::string_view typeName);

        bool IsSetType(std::string_view typeName);

        std::string ListElementType(std::string_view typeName);

        std::string SetElementType(std::string_view typeName);

        ListRef MakeList(ExecutionState &state,
                         const std::string &elementType);

        SetRef MakeSet(ExecutionState &state, const std::string &elementType);

        bool SetContains(ExecutionState &state, const SetRef &set,
                         const Value &element);

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
            Object instance{state.nextObjectId++};
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

        void ExecuteStatement(ExecutionState &state,
                              const Statement &statement);

        bool HasControlFlow(const ExecutionState &state)
        {
            return state.controlFlow != ControlFlow::None;
        }

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

            if (expression.operation == '=' || expression.operation == 'N')
            {
                const Value left = Evaluate(state, *expression.left);
                const Value right = Evaluate(state, *expression.right);
                double equal = 0.0;

                if (std::holds_alternative<Null>(left) ||
                    std::holds_alternative<Null>(right))
                {
                    equal = std::holds_alternative<Null>(left) &&
                                    std::holds_alternative<Null>(right)
                                ? 1.0
                                : 0.0;
                }
                else if (const auto *leftText = std::get_if<std::string>(&left))
                {
                    const auto *rightText = std::get_if<std::string>(&right);

                    if (rightText == nullptr)
                    {
                        throw std::runtime_error("Expected a string.");
                    }

                    equal = *leftText == *rightText ? 1.0 : 0.0;
                }
                else if (const auto *leftObject = std::get_if<Object>(&left))
                {
                    const auto *rightObject = std::get_if<Object>(&right);

                    if (rightObject == nullptr)
                    {
                        throw std::runtime_error("Expected an object.");
                    }

                    equal = leftObject->id == rightObject->id ? 1.0 : 0.0;
                }
                else
                {
                    equal = AsNumber(left) == AsNumber(right) ? 1.0 : 0.0;
                }

                if (expression.operation == 'N')
                {
                    return equal == 0.0 ? 1.0 : 0.0;
                }

                return equal;
            }

            if (expression.operation == '+')
            {
                const Value left = Evaluate(state, *expression.left);
                const Value right = Evaluate(state, *expression.right);

                if (std::holds_alternative<std::string>(left) ||
                    std::holds_alternative<std::string>(right))
                {
                    return ToStringValue(left) + ToStringValue(right);
                }

                return AsNumber(left) + AsNumber(right);
            }

            const double left = AsNumber(Evaluate(state, *expression.left));
            const double right = AsNumber(Evaluate(state, *expression.right));

            switch (expression.operation)
            {
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

            case 'L':
                return left <= right ? 1.0 : 0.0;

            case '%':
                if (right == 0.0)
                {
                    throw std::runtime_error("Expected a non-zero number.");
                }

                return std::fmod(left, right);

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

            if (expression.name == "CInt")
            {
                if (expression.arguments.size() != 1)
                {
                    throw std::runtime_error("Expected one argument.");
                }

                return std::trunc(
                    AsNumber(Evaluate(state, *expression.arguments[0])));
            }

            if (expression.name == "Abs")
            {
                if (expression.arguments.size() != 1)
                {
                    throw std::runtime_error("Expected one argument.");
                }

                return std::fabs(
                    AsNumber(Evaluate(state, *expression.arguments[0])));
            }

            if (expression.name == "RandomInt")
            {
                if (expression.arguments.size() != 2)
                {
                    throw std::runtime_error("Expected two arguments.");
                }

                const double minimum =
                    AsNumber(Evaluate(state, *expression.arguments[0]));
                const double maximum =
                    AsNumber(Evaluate(state, *expression.arguments[1]));

                if (std::floor(minimum) != minimum ||
                    std::floor(maximum) != maximum)
                {
                    throw std::runtime_error("Expected integer bounds.");
                }

                const int minimumValue = static_cast<int>(minimum);
                const int maximumValue = static_cast<int>(maximum);

                if (maximumValue < minimumValue)
                {
                    throw std::runtime_error("Expected a valid range.");
                }

                static std::mt19937 generator{std::random_device{}()};
                std::uniform_int_distribution<int> distribution(minimumValue,
                                                                maximumValue);

                return static_cast<double>(distribution(generator));
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

            if (expression.name == "GetImageSize")
            {
                if (expression.arguments.size() != 1)
                {
                    throw std::runtime_error("Expected one argument.");
                }

                const Value nameValue =
                    Evaluate(state, *expression.arguments[0]);
                int width = 0;
                int height = 0;
                Graphics::GetImageSize(AsString(nameValue), width, height);

                return Size{static_cast<double>(width),
                            static_cast<double>(height)};
            }

            if (expression.name == "GetMousePosition")
            {
                if (!expression.arguments.empty())
                {
                    throw std::runtime_error("Expected zero arguments.");
                }

                double x = 0.0;
                double y = 0.0;
                Graphics::GetMousePosition(x, y);
                return Point{x, y};
            }

            if (IsPointType(expression.name))
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

            if (IsSetType(expression.name))
            {
                if (!expression.arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                return MakeSet(state, SetElementType(expression.name));
            }

            if (state.classes.contains(expression.name))
            {
                if (!expression.arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                return MakeInstance(state, expression.name);
            }

            if (state.currentObjectId != 0)
            {
                const auto className =
                    state.objectClasses.find(state.currentObjectId);

                if (className != state.objectClasses.end())
                {
                    const auto classInfo = state.classes.find(className->second);

                    if (classInfo != state.classes.end() &&
                        classInfo->second.methods.contains(expression.name))
                    {
                        const Object instance{state.currentObjectId};
                        const std::vector<Value> &arguments =
                            PushArguments(state, expression.arguments);
                        std::optional<Value> returned;

                        try
                        {
                            returned = CallObjectMethod(
                                state, instance, expression.name, arguments);
                        }
                        catch (...)
                        {
                            PopArguments(state);
                            throw;
                        }

                        PopArguments(state);

                        if (!returned.has_value())
                        {
                            throw std::runtime_error("Expected a value.");
                        }

                        return *returned;
                    }
                }
            }

            const auto found = state.functions.find(expression.name);

            if (found == state.functions.end())
            {
                throw std::runtime_error("Unknown function.");
            }

            const std::vector<Value> &arguments =
                PushArguments(state, expression.arguments);
            std::optional<Value> result;

            try
            {
                result = CallGlobalFunction(state, *found->second, arguments);
            }
            catch (...)
            {
                PopArguments(state);
                throw;
            }

            PopArguments(state);

            if (!result.has_value())
            {
                throw std::runtime_error("Expected a value.");
            }

            return *result;
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

        std::vector<Value> &PushArguments(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Expression>> &arguments)
        {
            if (state.argumentFrames.empty())
            {
                state.argumentFrames.resize(64);
            }

            if (state.argumentDepth >= state.argumentFrames.size())
            {
                throw std::runtime_error("Call stack too deep.");
            }

            std::vector<Value> &values =
                state.argumentFrames[state.argumentDepth++];
            values.clear();
            values.reserve(arguments.size());

            for (const auto &argument : arguments)
            {
                values.push_back(Evaluate(state, *argument));
            }

            return values;
        }

        void PopArguments(ExecutionState &state)
        {
            if (state.argumentDepth == 0)
            {
                throw std::runtime_error("Unexpected arguments.");
            }

            --state.argumentDepth;
            state.argumentFrames[state.argumentDepth].clear();
        }

        Value EvaluateMember(ExecutionState &state,
                             const MemberExpression &expression)
        {
            if (expression.object->kind == ExpressionKind::Variable)
            {
                const auto &variable =
                    static_cast<const VariableExpression &>(*expression.object);

                if (state.groups.contains(variable.name))
                {
                    return LookupGroupMember(state, variable.name,
                                             expression.memberName);
                }

                if (variable.name == "MouseButtons")
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

                if (variable.name == "Keys" &&
                    expression.memberName.size() == 1)
                {
                    const char letter = expression.memberName[0];

                    if (letter >= 'A' && letter <= 'Z')
                    {
                        return static_cast<double>(SDLK_a + (letter - 'A'));
                    }

                    throw std::runtime_error("Unknown member.");
                }

                if (variable.name == "String")
                {
                    if (expression.memberName == "Empty")
                    {
                        return std::string{};
                    }

                    throw std::runtime_error("Unknown member.");
                }
            }

            const Value object = Evaluate(state, *expression.object);

            if (std::holds_alternative<Null>(object))
            {
                throw std::runtime_error("Expected an object.");
            }

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

            if (const auto *set = std::get_if<SetRef>(&object))
            {
                if (!expression.isCall)
                {
                    if (expression.memberName != "Count")
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    const auto found = state.sets.find(set->id);

                    if (found == state.sets.end())
                    {
                        throw std::runtime_error("Expected a set.");
                    }

                    return static_cast<double>(found->second.elements.size());
                }

                if (expression.memberName == "Contains")
                {
                    if (expression.arguments.size() != 1)
                    {
                        throw std::runtime_error("Expected one argument.");
                    }

                    return SetContains(
                               state, *set,
                               Evaluate(state, *expression.arguments[0]))
                               ? 1.0
                               : 0.0;
                }

                throw std::runtime_error("Unknown member.");
            }

            if (const auto *instance = std::get_if<Object>(&object))
            {
                if (!expression.isCall)
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

                    const auto classInfo = state.classes.find(
                        ObjectClassName(state, *instance));

                    if (classInfo == state.classes.end() ||
                        !classInfo->second.methods.contains(
                            expression.memberName))
                    {
                        throw std::runtime_error("Unknown member.");
                    }
                }

                const std::vector<Value> &arguments =
                    PushArguments(state, expression.arguments);
                std::optional<Value> returned;

                try
                {
                    returned = CallObjectMethod(state, *instance,
                                                expression.memberName,
                                                arguments);
                }
                catch (...)
                {
                    PopArguments(state);
                    throw;
                }

                PopArguments(state);

                if (!returned.has_value())
                {
                    throw std::runtime_error("Expected a value.");
                }

                return *returned;
            }

            if (expression.memberName == "ToString")
            {
                if (!expression.isCall || !expression.arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                return ToStringValue(object);
            }

            throw std::runtime_error("Unknown member.");
        }

        Value Evaluate(ExecutionState &state, const Expression &expression)
        {
            switch (expression.kind)
            {
            case ExpressionKind::Number:
                return static_cast<const NumberExpression &>(expression).value;
            case ExpressionKind::String:
                return static_cast<const StringExpression &>(expression).value;
            case ExpressionKind::Variable:
                return LookupName(
                    state,
                    static_cast<const VariableExpression &>(expression).name);
            case ExpressionKind::Binary:
                return EvaluateBinary(
                    state, static_cast<const BinaryExpression &>(expression));
            case ExpressionKind::Unary:
            {
                const auto &unary =
                    static_cast<const UnaryExpression &>(expression);

                if (unary.operation == '!')
                {
                    return IsTrue(Evaluate(state, *unary.operand)) ? 0.0 : 1.0;
                }

                throw std::runtime_error("Unknown operation.");
            }
            case ExpressionKind::Call:
                return EvaluateCall(
                    state, static_cast<const CallExpression &>(expression));
            case ExpressionKind::Member:
                return EvaluateMember(
                    state, static_cast<const MemberExpression &>(expression));
            case ExpressionKind::Index:
                return EvaluateIndex(
                    state, static_cast<const IndexExpression &>(expression));
            case ExpressionKind::List:
            {
                const auto &list =
                    static_cast<const ListExpression &>(expression);
                std::vector<double> values;
                values.reserve(list.elements.size());

                for (const auto &element : list.elements)
                {
                    values.push_back(AsNumber(Evaluate(state, *element)));
                }

                return values;
            }
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

        void EnableFPSCounter(ExecutionState &state, const FunctionCall &call)
        {
            if (call.arguments.size() != 3)
            {
                throw std::runtime_error("Expected three arguments.");
            }

            const double x = AsNumber(Evaluate(state, *call.arguments[0]));
            const double y = AsNumber(Evaluate(state, *call.arguments[1]));
            const int fontSize = static_cast<int>(
                AsNumber(Evaluate(state, *call.arguments[2])));

            Graphics::EnableFPSCounter(x, y, fontSize);
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

        bool IsGenericType(std::string_view typeName, std::string_view prefix)
        {
            if (!typeName.starts_with(prefix) ||
                typeName.size() < prefix.size() + 3 || typeName.back() != '>' ||
                typeName[prefix.size()] != '<')
            {
                return false;
            }

            int depth = 0;

            for (std::size_t index = prefix.size(); index < typeName.size();
                 ++index)
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

        bool IsListType(std::string_view typeName)
        {
            return IsGenericType(typeName, "List");
        }

        bool IsSetType(std::string_view typeName)
        {
            return IsGenericType(typeName, "Set");
        }

        std::string ListElementType(std::string_view typeName)
        {
            return std::string(typeName.substr(5, typeName.size() - 6));
        }

        std::string SetElementType(std::string_view typeName)
        {
            return std::string(typeName.substr(4, typeName.size() - 5));
        }

        bool IsNullable(std::string_view typeName)
        {
            return !typeName.empty() && typeName.back() == '?';
        }

        std::string UnderlyingType(std::string typeName)
        {
            if (IsNullable(typeName))
            {
                typeName.pop_back();
            }

            return typeName;
        }

        ListRef MakeList(ExecutionState &state, const std::string &elementType)
        {
            const int id = state.nextListId++;
            state.lists.insert({id, ListData{elementType, {}}});
            return ListRef{id};
        }

        SetRef MakeSet(ExecutionState &state, const std::string &elementType)
        {
            const int id = state.nextSetId++;
            state.sets.insert({id, SetData{elementType, {}}});
            return SetRef{id};
        }

        bool ValuesEqual(const Value &left, const Value &right)
        {
            if (std::holds_alternative<Null>(left) ||
                std::holds_alternative<Null>(right))
            {
                return std::holds_alternative<Null>(left) &&
                       std::holds_alternative<Null>(right);
            }

            if (const auto *leftText = std::get_if<std::string>(&left))
            {
                const auto *rightText = std::get_if<std::string>(&right);
                return rightText != nullptr && *leftText == *rightText;
            }

            if (const auto *leftObject = std::get_if<Object>(&left))
            {
                const auto *rightObject = std::get_if<Object>(&right);
                return rightObject != nullptr &&
                       leftObject->id == rightObject->id;
            }

            if (const auto *leftNumber = std::get_if<double>(&left))
            {
                const auto *rightNumber = std::get_if<double>(&right);
                return rightNumber != nullptr && *leftNumber == *rightNumber;
            }

            return false;
        }

        void ExpectElement(ExecutionState &state, const Value &value,
                           std::string_view typeName)
        {
            std::string_view effective = typeName;

            if (!effective.empty() && effective.back() == '?')
            {
                if (std::holds_alternative<Null>(value))
                {
                    return;
                }

                effective.remove_suffix(1);
            }

            if (effective == "Int" || effective == "Double" ||
                effective == "Boolean" || effective == "Keys" ||
                effective == "MouseButtons")
            {
                if (!std::holds_alternative<double>(value))
                {
                    throw std::runtime_error("Expected a number.");
                }

                return;
            }

            if (effective == "String")
            {
                if (!std::holds_alternative<std::string>(value))
                {
                    throw std::runtime_error("Expected a string.");
                }

                return;
            }

            if (IsPointType(effective))
            {
                if (!std::holds_alternative<Point>(value))
                {
                    throw std::runtime_error("Expected a point.");
                }

                return;
            }

            if (effective == "Size")
            {
                if (!std::holds_alternative<Size>(value))
                {
                    throw std::runtime_error("Expected a size.");
                }

                return;
            }

            if (Coloring::IsColorType(effective))
            {
                if (!std::holds_alternative<Coloring::Color>(value))
                {
                    throw std::runtime_error("Expected a color.");
                }

                return;
            }

            if (IsListType(effective))
            {
                const auto *list = std::get_if<ListRef>(&value);

                if (list == nullptr)
                {
                    throw std::runtime_error("Expected a list.");
                }

                const auto found = state.lists.find(list->id);

                if (found == state.lists.end() ||
                    found->second.elementType != ListElementType(effective))
                {
                    throw std::runtime_error("Expected a list.");
                }

                return;
            }

            if (IsSetType(effective))
            {
                const auto *set = std::get_if<SetRef>(&value);

                if (set == nullptr)
                {
                    throw std::runtime_error("Expected a set.");
                }

                const auto found = state.sets.find(set->id);

                if (found == state.sets.end() ||
                    found->second.elementType != SetElementType(effective))
                {
                    throw std::runtime_error("Expected a set.");
                }

                return;
            }

            const auto *instance = std::get_if<Object>(&value);

            if (instance == nullptr ||
                ObjectClassName(state, *instance) != effective)
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

        void AddSetElement(ExecutionState &state, const SetRef &set,
                           Value element)
        {
            const auto found = state.sets.find(set.id);

            if (found == state.sets.end())
            {
                throw std::runtime_error("Expected a set.");
            }

            ExpectElement(state, element, found->second.elementType);

            for (const auto &existing : found->second.elements)
            {
                if (ValuesEqual(existing, element))
                {
                    return;
                }
            }

            found->second.elements.push_back(std::move(element));
        }

        void RemoveSetElement(ExecutionState &state, const SetRef &set,
                              const Value &element)
        {
            const auto found = state.sets.find(set.id);

            if (found == state.sets.end())
            {
                throw std::runtime_error("Expected a set.");
            }

            ExpectElement(state, element, found->second.elementType);

            auto &elements = found->second.elements;

            for (auto iterator = elements.begin(); iterator != elements.end();
                 ++iterator)
            {
                if (ValuesEqual(*iterator, element))
                {
                    elements.erase(iterator);
                    return;
                }
            }
        }

        bool SetContains(ExecutionState &state, const SetRef &set,
                         const Value &element)
        {
            const auto found = state.sets.find(set.id);

            if (found == state.sets.end())
            {
                throw std::runtime_error("Expected a set.");
            }

            ExpectElement(state, element, found->second.elementType);

            for (const auto &existing : found->second.elements)
            {
                if (ValuesEqual(existing, element))
                {
                    return true;
                }
            }

            return false;
        }

        Value DefaultField(ExecutionState &state, const std::string &typeName)
        {
            if (IsNullable(typeName))
            {
                return Null{};
            }

            if (typeName == "Int" || typeName == "Double" ||
                typeName == "Boolean")
            {
                return 0.0;
            }

            if (typeName == "String")
            {
                return std::string{};
            }

            if (IsPointType(typeName))
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

            if (IsSetType(typeName))
            {
                return MakeSet(state, SetElementType(typeName));
            }

            if (typeName == "Keys" || typeName == "MouseButtons")
            {
                return 0.0;
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

            if (IsNullable(field.typeName))
            {
                if (std::holds_alternative<Null>(value))
                {
                    return value;
                }

                ExpectElement(state, value, field.typeName);
                return value;
            }

            if (IsPointType(field.typeName))
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

            if (field.typeName == "Int" || field.typeName == "Double" ||
                field.typeName == "Boolean")
            {
                if (!std::holds_alternative<double>(value))
                {
                    throw std::runtime_error("Expected a number.");
                }

                if (field.typeName == "Int")
                {
                    return std::trunc(AsNumber(value));
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

            if (IsSetType(field.typeName))
            {
                const auto *set = std::get_if<SetRef>(&value);

                if (set == nullptr)
                {
                    throw std::runtime_error("Expected a set.");
                }

                const auto found = state.sets.find(set->id);

                if (found == state.sets.end() ||
                    found->second.elementType != SetElementType(field.typeName))
                {
                    throw std::runtime_error("Expected a set.");
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

            if (instance == nullptr ||
                ObjectClassName(state, *instance) != field.typeName)
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

                if (HasControlFlow(state))
                {
                    return;
                }
            }
        }

        struct SavedVariable
        {
            std::string name;
            bool existed{false};
            Value value;
        };

        void BindParameters(ExecutionState &state,
                            const FunctionDeclaration &function,
                            const std::vector<Value> &arguments,
                            std::vector<SavedVariable> &saved)
        {
            if (arguments.size() != function.parameters.size())
            {
                throw std::runtime_error("Unexpected arguments.");
            }

            saved.reserve(arguments.size());

            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                ExpectElement(state, arguments[index],
                              function.parameters[index].typeName);
            }

            for (const auto &parameter : function.parameters)
            {
                if (state.constants.contains(parameter.name))
                {
                    throw std::runtime_error("Cannot change a constant.");
                }
            }

            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                const std::string &name = function.parameters[index].name;
                SavedVariable savedVariable;
                savedVariable.name = name;
                const auto found = state.variables.find(name);

                if (found != state.variables.end())
                {
                    savedVariable.existed = true;
                    savedVariable.value = std::move(found->second);
                }

                state.variables[name] = arguments[index];
                saved.push_back(std::move(savedVariable));
            }
        }

        void RestoreParameters(ExecutionState &state,
                               const std::vector<SavedVariable> &saved)
        {
            for (const auto &savedVariable : saved)
            {
                if (savedVariable.existed)
                {
                    state.variables[savedVariable.name] = savedVariable.value;
                }
                else
                {
                    state.variables.erase(savedVariable.name);
                }
            }
        }

        std::optional<Value>
        CallGlobalFunction(ExecutionState &state,
                           const FunctionDeclaration &function,
                           const std::vector<Value> &arguments)
        {
            const std::string previousGroup = state.currentGroup;
            const int previousObject = state.currentObjectId;
            state.currentGroup.clear();
            state.currentObjectId = 0;
            std::vector<SavedVariable> saved;
            std::optional<Value> result;

            try
            {
                BindParameters(state, function, arguments, saved);
                ExecuteBlock(state, function.body);

                if (state.controlFlow == ControlFlow::Return)
                {
                    result = std::move(state.returnValue);
                    state.controlFlow = ControlFlow::None;
                    state.returnValue = Null{};

                    if (!function.returnType.empty())
                    {
                        ExpectElement(state, *result, function.returnType);
                    }
                }
            }
            catch (...)
            {
                RestoreParameters(state, saved);
                state.currentGroup = previousGroup;
                state.currentObjectId = previousObject;
                throw;
            }

            RestoreParameters(state, saved);
            state.currentGroup = previousGroup;
            state.currentObjectId = previousObject;
            return result;
        }

        std::optional<Value> CallObjectMethod(
            ExecutionState &state, const Object &instance,
            const std::string &methodName, const std::vector<Value> &arguments)
        {
            const auto classInfo =
                state.classes.find(ObjectClassName(state, instance));

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
            std::vector<SavedVariable> saved;
            std::optional<Value> result;

            try
            {
                BindParameters(state, *function, arguments, saved);
                ExecuteBlock(state, function->body);

                if (state.controlFlow == ControlFlow::Return)
                {
                    result = std::move(state.returnValue);
                    state.controlFlow = ControlFlow::None;
                    state.returnValue = Null{};

                    if (!function->returnType.empty())
                    {
                        ExpectElement(state, *result, function->returnType);
                    }
                }
            }
            catch (...)
            {
                RestoreParameters(state, saved);
                state.currentObjectId = previousObject;
                state.currentGroup = previousGroup;
                throw;
            }

            RestoreParameters(state, saved);
            state.currentObjectId = previousObject;
            state.currentGroup = previousGroup;
            return result;
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

            const auto field = info->second.fieldConstant.find(fieldName);

            if (field == info->second.fieldConstant.end())
            {
                return false;
            }

            return field->second;
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
            if (expression.kind == ExpressionKind::Variable)
            {
                const auto &variable =
                    static_cast<const VariableExpression &>(expression);

                if (state.groups.contains(variable.name))
                {
                    return variable.name;
                }

                return {};
            }

            if (expression.kind == ExpressionKind::Member)
            {
                const auto &member =
                    static_cast<const MemberExpression &>(expression);
                const std::string parent = GroupName(state, *member.object);

                if (!parent.empty())
                {
                    const std::string nested =
                        parent + "." + member.memberName;

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
            if (expression.kind == ExpressionKind::Variable)
            {
                return FindBinding(
                    state,
                    static_cast<const VariableExpression &>(expression).name);
            }

            if (expression.kind == ExpressionKind::Member)
            {
                const auto &member =
                    static_cast<const MemberExpression &>(expression);
                const std::string group = GroupName(state, *member.object);

                if (!group.empty())
                {
                    return GroupMemberSlot(state, group, member.memberName);
                }

                const Slot parent = ResolveSlot(state, *member.object);

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

                const auto field = fields->second.find(member.memberName);

                if (field == fields->second.end())
                {
                    return {};
                }

                const bool isConstant =
                    parent.isConstant ||
                    FieldIsConstant(state, ObjectClassName(state, *instance),
                                    member.memberName);

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
            if (target.kind == ExpressionKind::Variable)
            {
                const auto &variable =
                    static_cast<const VariableExpression &>(target);
                const Slot slot = FindBinding(state, variable.name);

                if (slot.value == nullptr)
                {
                    throw std::runtime_error("Unknown variable.");
                }

                RejectConstant(slot.isConstant);
                *slot.value = std::move(value);
                return;
            }

            if (target.kind == ExpressionKind::Member)
            {
                const auto &member =
                    static_cast<const MemberExpression &>(target);
                const std::string group = GroupName(state, *member.object);

                if (!group.empty())
                {
                    const Slot slot =
                        GroupMemberSlot(state, group, member.memberName);

                    if (slot.value == nullptr)
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(slot.isConstant);
                    *slot.value = std::move(value);
                    return;
                }

                const Slot parent = ResolveSlot(state, *member.object);

                if (parent.value == nullptr)
                {
                    throw std::runtime_error("Cannot assign.");
                }

                if (std::holds_alternative<Null>(*parent.value))
                {
                    throw std::runtime_error("Expected an object.");
                }

                if (auto *size = std::get_if<Size>(parent.value))
                {
                    if (member.memberName != "width" &&
                        member.memberName != "height")
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(parent.isConstant);

                    if (member.memberName == "width")
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
                    if (member.memberName != "x" && member.memberName != "y")
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(parent.isConstant);

                    if (member.memberName == "x")
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

                    const auto field = fields->second.find(member.memberName);

                    if (field == fields->second.end())
                    {
                        throw std::runtime_error("Unknown member.");
                    }

                    RejectConstant(
                        parent.isConstant ||
                        FieldIsConstant(state,
                                        ObjectClassName(state, *instance),
                                        member.memberName));
                    field->second = std::move(value);
                    return;
                }

                throw std::runtime_error("Cannot assign.");
            }

            throw std::runtime_error("Cannot assign.");
        }

        void ExecuteStatement(ExecutionState &state, const Statement &statement)
        {
            switch (statement.kind)
            {
            case StatementKind::Int:
            {
                const auto &declaration =
                    static_cast<const IntStatement &>(statement);
                Value value = Evaluate(state, *declaration.value);

                if (IsPointType(declaration.typeName) &&
                    !std::holds_alternative<Point>(value))
                {
                    throw std::runtime_error("Expected a point.");
                }

                if (declaration.typeName == "Size" &&
                    !std::holds_alternative<Size>(value))
                {
                    throw std::runtime_error("Expected a size.");
                }

                if (declaration.typeName == "Int")
                {
                    value = std::trunc(AsNumber(value));
                }

                if (IsNullable(declaration.typeName) ||
                    state.classes.contains(
                        UnderlyingType(declaration.typeName)))
                {
                    ExpectElement(state, value, declaration.typeName);
                }

                DefineName(state, declaration.name, std::move(value),
                           declaration.isConstant);
                return;
            }
            case StatementKind::Assignment:
            {
                const auto &assignment =
                    static_cast<const AssignmentStatement &>(statement);
                Value value = Evaluate(state, *assignment.value);

                if (assignment.compoundOperation != 0)
                {
                    const double left =
                        AsNumber(Evaluate(state, *assignment.target));
                    const double right = AsNumber(value);

                    if (assignment.compoundOperation == '+')
                    {
                        value = left + right;
                    }
                    else if (assignment.compoundOperation == '-')
                    {
                        value = left - right;
                    }
                    else
                    {
                        throw std::runtime_error("Unknown operation.");
                    }
                }

                AssignTo(state, *assignment.target, std::move(value));
                return;
            }
            case StatementKind::For:
            {
                const auto &loop = static_cast<const ForStatement &>(statement);
                const double start = AsNumber(Evaluate(state, *loop.start));
                const double end = AsNumber(Evaluate(state, *loop.end));

                if (NameIsConstant(state, loop.name))
                {
                    throw std::runtime_error("Cannot change a constant.");
                }

                Value &loopVariable = state.variables[loop.name];

                for (double value = start; value <= end; value += 1.0)
                {
                    loopVariable = value;
                    ExecuteBlock(state, loop.body);

                    if (state.controlFlow == ControlFlow::Continue)
                    {
                        state.controlFlow = ControlFlow::None;
                        continue;
                    }

                    if (state.controlFlow == ControlFlow::Return)
                    {
                        return;
                    }
                }

                return;
            }
            case StatementKind::Continue:
                state.controlFlow = ControlFlow::Continue;
                return;
            case StatementKind::Return:
            {
                const auto &returned =
                    static_cast<const ReturnStatement &>(statement);
                state.returnValue = Evaluate(state, *returned.value);
                state.controlFlow = ControlFlow::Return;
                return;
            }
            case StatementKind::Print:
            {
                const auto &print =
                    static_cast<const PrintStatement &>(statement);
                PrintValue(Evaluate(state, *print.expression));
                return;
            }
            case StatementKind::If:
            {
                const auto &conditional =
                    static_cast<const IfStatement &>(statement);

                if (IsTrue(Evaluate(state, *conditional.condition)))
                {
                    ExecuteBlock(state, conditional.thenBranch);
                }
                else
                {
                    ExecuteBlock(state, conditional.elseBranch);
                }

                return;
            }
            case StatementKind::Function:
            case StatementKind::Class:
            case StatementKind::Scene:
                return;
            case StatementKind::Import:
            {
                const auto &import =
                    static_cast<const ImportStatement &>(statement);

                if (Graphics::IsModule(import.moduleName) ||
                    Coloring::IsModule(import.moduleName) ||
                    ScenesCore::IsModule(import.moduleName))
                {
                    return;
                }

                throw std::runtime_error("Unknown module.");
            }
            case StatementKind::Object:
            {
                const auto &object =
                    static_cast<const ObjectStatement &>(statement);

                if (Coloring::IsColorType(object.typeName))
                {
                    if (object.arguments.size() != 4)
                    {
                        throw std::runtime_error("Expected four arguments.");
                    }

                    const double red =
                        AsNumber(Evaluate(state, *object.arguments[0]));
                    const double green =
                        AsNumber(Evaluate(state, *object.arguments[1]));
                    const double blue =
                        AsNumber(Evaluate(state, *object.arguments[2]));
                    const double alpha =
                        AsNumber(Evaluate(state, *object.arguments[3]));

                    DefineName(state, object.name,
                               Coloring::Color{red, green, blue, alpha},
                               object.isConstant);
                    return;
                }

                if (IsPointType(object.typeName))
                {
                    DefineName(state, object.name,
                               MakePoint(state, object.arguments),
                               object.isConstant);
                    return;
                }

                if (object.typeName == "Size")
                {
                    DefineName(state, object.name,
                               MakeSize(state, object.arguments),
                               object.isConstant);
                    return;
                }

                if (state.sceneTypes.contains(object.typeName))
                {
                    if (!object.arguments.empty())
                    {
                        throw std::runtime_error("Unexpected arguments.");
                    }

                    DefineName(state, object.name, SceneObject{object.typeName},
                               object.isConstant);
                    return;
                }

                const auto classInfo = state.classes.find(object.typeName);

                if (classInfo == state.classes.end())
                {
                    throw std::runtime_error("Unknown class.");
                }

                if (!object.arguments.empty())
                {
                    throw std::runtime_error("Unexpected arguments.");
                }

                DefineName(state, object.name,
                           MakeInstance(state, object.typeName),
                           object.isConstant);
                return;
            }
            case StatementKind::Group:
            {
                const auto &group =
                    static_cast<const GroupDeclaration &>(statement);
                const std::string previousGroup = state.currentGroup;

                if (previousGroup.empty())
                {
                    state.currentGroup = group.name;
                }
                else
                {
                    state.currentGroup = previousGroup + "." + group.name;
                }

                state.groups.try_emplace(state.currentGroup);

                try
                {
                    ExecuteBlock(state, group.body);
                }
                catch (...)
                {
                    state.currentGroup = previousGroup;
                    throw;
                }

                state.currentGroup = previousGroup;
                return;
            }
            case StatementKind::MethodCall:
            {
                const auto &method =
                    static_cast<const MethodCall &>(statement);
                const Value receiver = Evaluate(state, *method.object);

                if (const auto *list = std::get_if<ListRef>(&receiver))
                {
                    if (method.methodName != "Add")
                    {
                        throw std::runtime_error("Unknown method.");
                    }

                    if (method.arguments.size() != 1)
                    {
                        throw std::runtime_error("Expected one argument.");
                    }

                    AddElement(state, *list,
                               Evaluate(state, *method.arguments[0]));
                    return;
                }

                if (const auto *set = std::get_if<SetRef>(&receiver))
                {
                    if (method.arguments.size() != 1)
                    {
                        throw std::runtime_error("Expected one argument.");
                    }

                    if (method.methodName == "Add")
                    {
                        AddSetElement(state, *set,
                                      Evaluate(state, *method.arguments[0]));
                        return;
                    }

                    if (method.methodName == "Remove")
                    {
                        RemoveSetElement(state, *set,
                                         Evaluate(state, *method.arguments[0]));
                        return;
                    }

                    throw std::runtime_error("Unknown method.");
                }

                const auto *instance = std::get_if<Object>(&receiver);

                if (instance == nullptr)
                {
                    throw std::runtime_error("Expected an object.");
                }

                {
                    const std::vector<Value> &arguments =
                        PushArguments(state, method.arguments);

                    try
                    {
                        CallObjectMethod(state, *instance, method.methodName,
                                         arguments);
                    }
                    catch (...)
                    {
                        PopArguments(state);
                        throw;
                    }

                    PopArguments(state);
                }
                return;
            }
            case StatementKind::FunctionCall:
            {
                const auto &call =
                    static_cast<const FunctionCall &>(statement);

                if (call.name == "InitializeGraphics")
                {
                    InitializeGraphics(state, call);
                    return;
                }

                if (call.name == "SetClearColor")
                {
                    SetClearColor(state, call);
                    return;
                }

                if (call.name == "LoadImages")
                {
                    LoadImages(state, call);
                    return;
                }

                if (call.name == "InitializeText")
                {
                    InitializeText(state, call);
                    return;
                }

                if (call.name == "AddFontSizes")
                {
                    AddFontSizes(state, call);
                    return;
                }

                if (call.name == "AddCursorStyle")
                {
                    AddCursorStyle(state, call);
                    return;
                }

                if (call.name == "SetDefaultCursorStyle")
                {
                    SetDefaultCursorStyle(state, call);
                    return;
                }

                if (call.name == "EnableFPSCounter")
                {
                    EnableFPSCounter(state, call);
                    return;
                }

                if (call.name == "DrawImage")
                {
                    DrawImage(state, call);
                    return;
                }

                if (call.name == "DrawString")
                {
                    DrawString(state, call);
                    return;
                }

                if (call.name == "AddScene")
                {
                    AddScene(state, call);
                    return;
                }

                if (call.name == "GoToScene")
                {
                    GoToScene(state, call);
                    return;
                }

                const auto found = state.functions.find(call.name);

                if (found == state.functions.end())
                {
                    throw std::runtime_error("Unknown function.");
                }

                const std::vector<Value> &arguments =
                    PushArguments(state, call.arguments);
                const std::string previousGroup = state.currentGroup;
                state.currentGroup.clear();
                std::vector<SavedVariable> saved;

                try
                {
                    BindParameters(state, *found->second, arguments, saved);
                    ExecuteBlock(state, found->second->body);

                    if (state.controlFlow == ControlFlow::Return)
                    {
                        if (!found->second->returnType.empty())
                        {
                            ExpectElement(state, state.returnValue,
                                          found->second->returnType);
                        }

                        state.controlFlow = ControlFlow::None;
                        state.returnValue = Null{};
                    }
                }
                catch (...)
                {
                    RestoreParameters(state, saved);
                    state.currentGroup = previousGroup;
                    PopArguments(state);
                    throw;
                }

                RestoreParameters(state, saved);
                state.currentGroup = previousGroup;
                PopArguments(state);
                return;
            }
            }

            throw std::runtime_error("Unknown statement.");
        }

        void RegisterFunctions(
            ExecutionState &state,
            const std::vector<std::unique_ptr<Statement>> &statements)
        {
            for (const auto &statement : statements)
            {
                if (statement->kind == StatementKind::Function)
                {
                    const auto *function =
                        static_cast<const FunctionDeclaration *>(
                            statement.get());
                    state.functions[function->name] = function;
                }

                if (statement->kind == StatementKind::Class)
                {
                    const auto *declaration =
                        static_cast<const ClassDeclaration *>(statement.get());
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
                        info.fieldConstant[field.name] = field.isConstant;
                    }

                    if (declaration->hasConstructor)
                    {
                        info.constructor = &declaration->constructor;
                    }

                    state.classes[declaration->name] = std::move(info);
                }

                if (statement->kind == StatementKind::Scene)
                {
                    const auto *scene =
                        static_cast<const SceneDeclaration *>(statement.get());
                    state.sceneTypes[scene->name] = SceneType{
                        &scene->update,
                        &scene->draw,
                        &scene->onMouseDown,
                        &scene->onKeyDown,
                        &scene->onKeyUp,
                        &scene->onEnter,
                        scene->onMouseDownParameter,
                        scene->onKeyDownParameter,
                        scene->onKeyUpParameter};
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

            if (HasControlFlow(state))
            {
                throw std::runtime_error("Unexpected statement.");
            }

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

            if (HasControlFlow(state))
            {
                throw std::runtime_error("Unexpected statement.");
            }
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

        void RunSceneKeyUp(ExecutionState &state, int key)
        {
            if (state.currentSceneType.empty())
            {
                return;
            }

            const auto found = state.sceneTypes.find(state.currentSceneType);

            if (found == state.sceneTypes.end() ||
                found->second.onKeyUp == nullptr)
            {
                return;
            }

            const std::string &parameter = found->second.onKeyUpParameter;
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

            ExecuteBlock(state, *found->second.onKeyUp);

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
        state.variables["Nothing"] = Null{};
        state.constants.insert("Nothing");

        RegisterFunctions(state, statements);
        ExecuteBlock(state, statements);

        if (HasControlFlow(state))
        {
            throw std::runtime_error("Unexpected statement.");
        }

        std::cout.flush();
        Graphics::RunUntilClosed(
            [&state]() { RunSceneUpdate(state); },
            [&state]() { RunSceneDraw(state); },
            [&state](int button) { RunSceneMouseDown(state, button); },
            [&state](int key) { RunSceneKeyDown(state, key); },
            [&state](int key) { RunSceneKeyUp(state, key); });
    }
}
