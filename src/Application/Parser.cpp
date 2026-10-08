#include "Parser.hpp"
#include "Expressions/BinaryExpression.hpp"
#include "Expressions/CallExpression.hpp"
#include "Expressions/IndexExpression.hpp"
#include "Expressions/ListExpression.hpp"
#include "Expressions/MemberExpression.hpp"
#include "Expressions/NumberExpression.hpp"
#include "Expressions/StringExpression.hpp"
#include "Expressions/VariableExpression.hpp"
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
        struct ParseState
        {
            const std::vector<Token> &tokens;
            std::size_t index{0};
        };

        bool AtEnd(const ParseState &state)
        {
            return state.index >= state.tokens.size();
        }

        const Token &Peek(const ParseState &state)
        {
            if (AtEnd(state))
            {
                throw std::runtime_error("Unexpected end of input.");
            }

            return state.tokens[state.index];
        }

        const Token &Advance(ParseState &state)
        {
            const Token &token = Peek(state);
            ++state.index;
            return token;
        }

        const Token &Expect(ParseState &state, TokenTypes type,
                            const char *message)
        {
            if (AtEnd(state) || Peek(state).type != type)
            {
                throw std::runtime_error(message);
            }

            return Advance(state);
        }

        void SkipNewlines(ParseState &state)
        {
            while (!AtEnd(state) && Peek(state).type == TokenTypes::Newline)
            {
                Advance(state);
            }
        }

        bool IsComparison(TokenTypes type)
        {
            return type == TokenTypes::Equals ||
                   type == TokenTypes::GreaterThan ||
                   type == TokenTypes::GreaterOrEqual ||
                   type == TokenTypes::LessThan;
        }

        bool IsAddition(TokenTypes type)
        {
            return type == TokenTypes::Plus || type == TokenTypes::Minus;
        }

        bool IsMultiplication(TokenTypes type)
        {
            return type == TokenTypes::Star || type == TokenTypes::Slash ||
                   type == TokenTypes::Percent;
        }

        void ParseOptionalParameters(ParseState &state)
        {
            if (AtEnd(state) || Peek(state).type != TokenTypes::LeftParen)
            {
                return;
            }

            Advance(state);

            Expect(state, TokenTypes::RightParen, "Expected ')'.");
        }

        std::unique_ptr<Expression> ParseExpression(ParseState &state);
        std::vector<std::unique_ptr<Expression>>
        ParseArguments(ParseState &state);
        std::string ParseTypeName(ParseState &state);
        std::unique_ptr<Statement> ParseIdentifierStatement(ParseState &state);
        std::unique_ptr<Statement> ParseReturnStatement(ParseState &state);
        std::unique_ptr<Statement> ParseStatement(ParseState &state);
        std::unique_ptr<Statement> ParseConstStatement(ParseState &state);
        std::unique_ptr<Statement> ParseGroup(ParseState &state);

        std::unique_ptr<Expression> ParseList(ParseState &state)
        {
            Expect(state, TokenTypes::LeftBracket, "Expected '['.");

            auto expression = std::make_unique<ListExpression>();

            if (!AtEnd(state) && Peek(state).type != TokenTypes::RightBracket)
            {
                while (!AtEnd(state))
                {
                    expression->elements.push_back(ParseExpression(state));

                    if (AtEnd(state) ||
                        Peek(state).type != TokenTypes::Comma)
                    {
                        break;
                    }

                    Advance(state);
                }
            }

            Expect(state, TokenTypes::RightBracket, "Expected ']'.");
            return expression;
        }

        bool FollowedByGenericCall(const ParseState &state)
        {
            if (AtEnd(state) || Peek(state).type != TokenTypes::LessThan)
            {
                return false;
            }

            int depth = 0;

            for (std::size_t index = state.index; index < state.tokens.size();
                 ++index)
            {
                const TokenTypes type = state.tokens[index].type;

                if (type == TokenTypes::LessThan)
                {
                    ++depth;
                    continue;
                }

                if (type == TokenTypes::GreaterThan)
                {
                    --depth;

                    if (depth == 0)
                    {
                        const std::size_t next = index + 1;

                        return next < state.tokens.size() &&
                               state.tokens[next].type ==
                                   TokenTypes::LeftParen;
                    }

                    continue;
                }

                if (type == TokenTypes::Newline)
                {
                    return false;
                }
            }

            return false;
        }

        void AppendGenericArguments(ParseState &state, std::string &name)
        {
            Expect(state, TokenTypes::LessThan, "Expected '<'.");
            name += '<';
            name += ParseTypeName(state);
            Expect(state, TokenTypes::GreaterThan, "Expected '>'.");
            name += '>';
        }

        std::unique_ptr<Expression>
        ParseIndex(ParseState &state, std::unique_ptr<Expression> object)
        {
            Advance(state);

            auto indexed = std::make_unique<IndexExpression>();
            indexed->object = std::move(object);
            indexed->index = ParseExpression(state);
            Expect(state, TokenTypes::RightBracket, "Expected ']'.");

            return indexed;
        }

        std::unique_ptr<Expression> ParsePrimary(ParseState &state)
        {
            if (!AtEnd(state) && Peek(state).type == TokenTypes::LeftParen)
            {
                Advance(state);
                auto expression = ParseExpression(state);
                Expect(state, TokenTypes::RightParen, "Expected ')'.");
                return expression;
            }

            if (!AtEnd(state) && Peek(state).type == TokenTypes::LeftBracket)
            {
                return ParseList(state);
            }

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Minus)
            {
                Advance(state);

                auto expression = std::make_unique<BinaryExpression>();
                auto zero = std::make_unique<NumberExpression>();
                zero->value = 0;
                expression->left = std::move(zero);
                expression->operation = '-';
                expression->right = ParsePrimary(state);
                return expression;
            }

            const Token &token = Advance(state);

            if (token.type == TokenTypes::Number)
            {
                auto expression = std::make_unique<NumberExpression>();
                expression->value = std::stod(token.value);
                return expression;
            }

            if (token.type == TokenTypes::String)
            {
                auto expression = std::make_unique<StringExpression>();
                expression->value = token.value;
                return expression;
            }

            if (token.type == TokenTypes::Identifier)
            {
                std::unique_ptr<Expression> expression;

                if (!AtEnd(state) &&
                    Peek(state).type == TokenTypes::LessThan &&
                    FollowedByGenericCall(state))
                {
                    std::string name = token.value;
                    AppendGenericArguments(state, name);

                    auto call = std::make_unique<CallExpression>();
                    call->name = std::move(name);
                    call->arguments = ParseArguments(state);
                    expression = std::move(call);
                }
                else if (!AtEnd(state) &&
                         Peek(state).type == TokenTypes::LeftParen)
                {
                    auto call = std::make_unique<CallExpression>();
                    call->name = token.value;
                    call->arguments = ParseArguments(state);
                    expression = std::move(call);
                }
                else
                {
                    auto variable = std::make_unique<VariableExpression>();
                    variable->name = token.value;
                    expression = std::move(variable);
                }

                while (!AtEnd(state) &&
                       (Peek(state).type == TokenTypes::Dot ||
                        Peek(state).type == TokenTypes::LeftBracket))
                {
                    if (Peek(state).type == TokenTypes::LeftBracket)
                    {
                        expression = ParseIndex(state, std::move(expression));
                        continue;
                    }

                    Advance(state);

                    auto member = std::make_unique<MemberExpression>();
                    member->object = std::move(expression);
                    member->memberName =
                        Expect(state, TokenTypes::Identifier,
                               "Expected a name.")
                            .value;
                    expression = std::move(member);
                }

                return expression;
            }

            throw std::runtime_error("Expected a value.");
        }

        std::unique_ptr<Expression> ParseMultiplication(ParseState &state)
        {
            auto expression = ParsePrimary(state);

            while (!AtEnd(state) && IsMultiplication(Peek(state).type))
            {
                const char operation = Advance(state).value[0];
                auto binary = std::make_unique<BinaryExpression>();
                binary->left = std::move(expression);
                binary->operation = operation;
                binary->right = ParsePrimary(state);
                expression = std::move(binary);
            }

            return expression;
        }

        std::unique_ptr<Expression> ParseAddition(ParseState &state)
        {
            auto expression = ParseMultiplication(state);

            while (!AtEnd(state) && IsAddition(Peek(state).type))
            {
                const char operation = Advance(state).value[0];
                auto binary = std::make_unique<BinaryExpression>();
                binary->left = std::move(expression);
                binary->operation = operation;
                binary->right = ParseMultiplication(state);
                expression = std::move(binary);
            }

            return expression;
        }

        std::unique_ptr<Expression> ParseComparison(ParseState &state)
        {
            auto expression = ParseAddition(state);

            while (!AtEnd(state) && IsComparison(Peek(state).type))
            {
                const Token &token = Advance(state);
                char operation = token.value[0];

                if (token.type == TokenTypes::GreaterOrEqual)
                {
                    operation = 'G';
                }

                auto binary = std::make_unique<BinaryExpression>();
                binary->left = std::move(expression);
                binary->operation = operation;
                binary->right = ParseAddition(state);
                expression = std::move(binary);
            }

            return expression;
        }

        std::unique_ptr<Expression> ParseAnd(ParseState &state)
        {
            auto expression = ParseComparison(state);

            while (!AtEnd(state) && Peek(state).type == TokenTypes::And)
            {
                Advance(state);

                auto binary = std::make_unique<BinaryExpression>();
                binary->left = std::move(expression);
                binary->operation = '&';
                binary->right = ParseComparison(state);
                expression = std::move(binary);
            }

            return expression;
        }

        std::unique_ptr<Expression> ParseExpression(ParseState &state)
        {
            auto expression = ParseAnd(state);

            while (!AtEnd(state) && Peek(state).type == TokenTypes::Or)
            {
                Advance(state);

                auto binary = std::make_unique<BinaryExpression>();
                binary->left = std::move(expression);
                binary->operation = '|';
                binary->right = ParseAnd(state);
                expression = std::move(binary);
            }

            return expression;
        }

        bool CanStartExpression(TokenTypes type)
        {
            return type == TokenTypes::Number ||
                   type == TokenTypes::String ||
                   type == TokenTypes::Identifier ||
                   type == TokenTypes::Minus ||
                   type == TokenTypes::LeftParen ||
                   type == TokenTypes::LeftBracket;
        }

        bool FollowedBy(const ParseState &state, TokenTypes type)
        {
            const std::size_t next = state.index + 1;

            return next < state.tokens.size() &&
                   state.tokens[next].type == type;
        }

        bool FollowedByComma(const ParseState &state)
        {
            return FollowedBy(state, TokenTypes::Comma);
        }

        std::vector<std::unique_ptr<Expression>>
        ParseArguments(ParseState &state)
        {
            std::vector<std::unique_ptr<Expression>> arguments;

            if (AtEnd(state))
            {
                return arguments;
            }

            const bool parenthesized =
                Peek(state).type == TokenTypes::LeftParen;

            if (parenthesized)
            {
                Advance(state);

                if (AtEnd(state) ||
                    Peek(state).type == TokenTypes::RightParen)
                {
                    Expect(state, TokenTypes::RightParen, "Expected ')'.");
                    return arguments;
                }
            }
            else if (!CanStartExpression(Peek(state).type))
            {
                return arguments;
            }

            while (!AtEnd(state))
            {
                arguments.push_back(ParseExpression(state));

                if (AtEnd(state))
                {
                    break;
                }

                if (Peek(state).type == TokenTypes::Comma)
                {
                    Advance(state);
                    continue;
                }

                if (CanStartExpression(Peek(state).type))
                {
                    throw std::runtime_error("Expected ','.");
                }

                break;
            }

            if (parenthesized)
            {
                Expect(state, TokenTypes::RightParen, "Expected ')'.");
            }

            return arguments;
        }

        std::vector<std::unique_ptr<Statement>> ParseBlock(ParseState &state,
                                                           bool stopAtElse)
        {
            std::vector<std::unique_ptr<Statement>> statements;

            while (!AtEnd(state))
            {
                SkipNewlines(state);

                if (AtEnd(state))
                {
                    break;
                }

                const TokenTypes type = Peek(state).type;

                if (type == TokenTypes::End)
                {
                    break;
                }

                if (stopAtElse && (type == TokenTypes::Else ||
                                   type == TokenTypes::ElseIf))
                {
                    break;
                }

                statements.push_back(ParseStatement(state));
            }

            return statements;
        }

        std::unique_ptr<Statement> ParseIntStatement(ParseState &state)
        {
            Advance(state);

            auto statement = std::make_unique<IntStatement>();
            statement->name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            Expect(state, TokenTypes::Equals, "Expected '='.");

            statement->value = ParseExpression(state);
            return statement;
        }

        std::unique_ptr<Statement> ParseForStatement(ParseState &state)
        {
            Advance(state);

            auto statement = std::make_unique<ForStatement>();
            statement->name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            Expect(state, TokenTypes::Equals, "Expected '='.");
            statement->start = ParseExpression(state);
            Expect(state, TokenTypes::To, "Expected 'To'.");
            statement->end = ParseExpression(state);

            while (!AtEnd(state))
            {
                SkipNewlines(state);

                if (AtEnd(state) || Peek(state).type == TokenTypes::Next)
                {
                    break;
                }

                statement->body.push_back(ParseStatement(state));
            }

            Expect(state, TokenTypes::Next, "Expected 'Next'.");
            return statement;
        }

        std::unique_ptr<Statement> ParsePrintStatement(ParseState &state)
        {
            Advance(state);

            auto statement = std::make_unique<PrintStatement>();
            statement->expression = ParseExpression(state);
            return statement;
        }

        void ParseIfCondition(ParseState &state, IfStatement &statement)
        {
            statement.condition = ParseExpression(state);

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Then)
            {
                Advance(state);
            }

            statement.thenBranch = ParseBlock(state, true);
        }

        std::unique_ptr<Statement> ParseIfStatement(ParseState &state)
        {
            Advance(state);

            auto statement = std::make_unique<IfStatement>();
            IfStatement *current = statement.get();
            ParseIfCondition(state, *current);

            while (!AtEnd(state) && Peek(state).type == TokenTypes::ElseIf)
            {
                Advance(state);

                auto nested = std::make_unique<IfStatement>();
                IfStatement *nestedIf = nested.get();
                ParseIfCondition(state, *nestedIf);
                current->elseBranch.push_back(std::move(nested));
                current = nestedIf;
            }

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Else)
            {
                Advance(state);
                current->elseBranch = ParseBlock(state, false);
            }

            Expect(state, TokenTypes::End, "Expected 'End'.");

            return statement;
        }

        std::unique_ptr<Statement> ParseStatement(ParseState &state)
        {
            switch (Peek(state).type)
            {
            case TokenTypes::Int:
            case TokenTypes::Double:
                return ParseIntStatement(state);

            case TokenTypes::For:
                return ParseForStatement(state);

            case TokenTypes::If:
                return ParseIfStatement(state);

            case TokenTypes::Print:
                return ParsePrintStatement(state);

            case TokenTypes::Continue:
                Advance(state);
                return std::make_unique<ContinueStatement>();

            case TokenTypes::Return:
                return ParseReturnStatement(state);

            case TokenTypes::Identifier:
                return ParseIdentifierStatement(state);

            case TokenTypes::Const:
                return ParseConstStatement(state);

            case TokenTypes::Group:
                return ParseGroup(state);

            default:
                throw std::runtime_error("Unexpected token.");
            }
        }

        std::unique_ptr<Statement> ParseAssignment(ParseState &state,
                                                   std::unique_ptr<Expression> target)
        {
            Expect(state, TokenTypes::Equals, "Expected '='.");

            auto statement = std::make_unique<AssignmentStatement>();
            statement->target = std::move(target);
            statement->value = ParseExpression(state);
            return statement;
        }

        std::unique_ptr<Statement> ParseIdentifierStatement(ParseState &state)
        {
            const std::string name = Advance(state).value;

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Equals)
            {
                auto target = std::make_unique<VariableExpression>();
                target->name = name;
                return ParseAssignment(state, std::move(target));
            }

            if (name == "SetClearColor" || name == "DrawImage" ||
                name == "DrawString" || name == "LoadImages" ||
                name == "InitializeText" || name == "AddFontSizes")
            {
                auto statement = std::make_unique<FunctionCall>();
                statement->name = name;
                statement->arguments = ParseArguments(state);
                return statement;
            }

            if (!AtEnd(state) && (Peek(state).type == TokenTypes::Dot ||
                                  Peek(state).type == TokenTypes::LeftBracket))
            {
                auto object = std::make_unique<VariableExpression>();
                object->name = name;
                std::unique_ptr<Expression> receiver = std::move(object);

                while (!AtEnd(state) &&
                       Peek(state).type == TokenTypes::LeftBracket)
                {
                    receiver = ParseIndex(state, std::move(receiver));
                }

                while (!AtEnd(state) && Peek(state).type == TokenTypes::Dot)
                {
                    Advance(state);

                    const std::string memberName =
                        Expect(state, TokenTypes::Identifier, "Expected a name.")
                            .value;

                    const bool isMember =
                        !AtEnd(state) &&
                        (Peek(state).type == TokenTypes::Equals ||
                         Peek(state).type == TokenTypes::Dot ||
                         Peek(state).type == TokenTypes::LeftBracket);

                    if (!isMember)
                    {
                        auto statement = std::make_unique<MethodCall>();
                        statement->object = std::move(receiver);
                        statement->methodName = memberName;
                        statement->arguments = ParseArguments(state);
                        return statement;
                    }

                    auto member = std::make_unique<MemberExpression>();
                    member->object = std::move(receiver);
                    member->memberName = memberName;
                    receiver = std::move(member);

                    while (!AtEnd(state) &&
                           Peek(state).type == TokenTypes::LeftBracket)
                    {
                        receiver = ParseIndex(state, std::move(receiver));
                    }
                }

                if (!AtEnd(state) && Peek(state).type == TokenTypes::Equals)
                {
                    return ParseAssignment(state, std::move(receiver));
                }

                throw std::runtime_error("Unexpected token.");
            }

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Identifier &&
                !FollowedByComma(state))
            {
                if (FollowedBy(state, TokenTypes::Equals))
                {
                    auto statement = std::make_unique<IntStatement>();
                    statement->typeName = name;
                    statement->name = Advance(state).value;
                    Expect(state, TokenTypes::Equals, "Expected '='.");
                    statement->value = ParseExpression(state);
                    return statement;
                }

                auto statement = std::make_unique<ObjectStatement>();
                statement->typeName = name;
                statement->name = Advance(state).value;

                if (!AtEnd(state) &&
                    Peek(state).type == TokenTypes::LeftParen)
                {
                    statement->arguments = ParseArguments(state);
                }

                return statement;
            }

            auto statement = std::make_unique<FunctionCall>();
            statement->name = name;
            statement->arguments = ParseArguments(state);

            return statement;
        }

        std::unique_ptr<Statement> ParseConstStatement(ParseState &state)
        {
            Expect(state, TokenTypes::Const, "Expected 'Const'.");

            if (AtEnd(state))
            {
                throw std::runtime_error("Expected a declaration.");
            }

            std::unique_ptr<Statement> statement;
            const TokenTypes type = Peek(state).type;

            if (type == TokenTypes::Int || type == TokenTypes::Double)
            {
                statement = ParseIntStatement(state);
            }
            else if (type == TokenTypes::Identifier)
            {
                statement = ParseIdentifierStatement(state);
            }
            else
            {
                throw std::runtime_error("Expected a declaration.");
            }

            if (auto *declaration =
                    dynamic_cast<IntStatement *>(statement.get()))
            {
                declaration->isConstant = true;
                return statement;
            }

            if (auto *object =
                    dynamic_cast<ObjectStatement *>(statement.get()))
            {
                object->isConstant = true;
                return statement;
            }

            throw std::runtime_error("Expected a declaration.");
        }

        std::unique_ptr<Statement> ParseImportStatement(ParseState &state)
        {
            Advance(state);

            auto statement = std::make_unique<ImportStatement>();
            statement->moduleName =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            while (!AtEnd(state) && Peek(state).type == TokenTypes::Dot)
            {
                Advance(state);

                statement->moduleName += '.';
                statement->moduleName +=
                    Expect(state, TokenTypes::Identifier, "Expected a name.")
                        .value;
            }

            return statement;
        }

        std::unique_ptr<Statement> ParseReturnStatement(ParseState &state)
        {
            Expect(state, TokenTypes::Return, "Expected 'Return'.");

            auto statement = std::make_unique<ReturnStatement>();
            statement->value = ParseExpression(state);
            return statement;
        }

        std::unique_ptr<FunctionDeclaration> ParseFunction(ParseState &state)
        {
            Expect(state, TokenTypes::Fn, "Expected 'Fn'.");

            auto function = std::make_unique<FunctionDeclaration>();
            function->name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            ParseOptionalParameters(state);

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Identifier &&
                Peek(state).value == "As")
            {
                Advance(state);
                function->returnType = ParseTypeName(state);
            }

            function->body = ParseBlock(state, false);

            Expect(state, TokenTypes::End, "Expected 'End'.");

            return function;
        }

        std::string ParseTypeName(ParseState &state)
        {
            std::string name;

            if (!AtEnd(state) && (Peek(state).type == TokenTypes::Int ||
                                  Peek(state).type == TokenTypes::Double))
            {
                name = Advance(state).value;
            }
            else
            {
                name = Expect(state, TokenTypes::Identifier, "Expected a type.")
                           .value;
            }

            if (AtEnd(state) || Peek(state).type != TokenTypes::LessThan)
            {
                return name;
            }

            Advance(state);
            name += '<';
            name += ParseTypeName(state);
            Expect(state, TokenTypes::GreaterThan, "Expected '>'.");
            name += '>';

            return name;
        }

        FieldDeclaration ParseField(ParseState &state)
        {
            FieldDeclaration field;
            field.typeName = ParseTypeName(state);

            field.name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            if (!AtEnd(state) && Peek(state).type == TokenTypes::Equals)
            {
                Advance(state);
                field.value = ParseExpression(state);
            }

            return field;
        }

        std::unique_ptr<Statement> ParseGroup(ParseState &state)
        {
            Expect(state, TokenTypes::Group, "Expected 'Group'.");

            auto declaration = std::make_unique<GroupDeclaration>();
            declaration->name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;
            declaration->body = ParseBlock(state, false);
            Expect(state, TokenTypes::End, "Expected 'End'.");

            return declaration;
        }

        std::unique_ptr<ClassDeclaration> ParseClass(ParseState &state)
        {
            Expect(state, TokenTypes::Class, "Expected 'Class'.");

            auto declaration = std::make_unique<ClassDeclaration>();
            declaration->name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            while (!AtEnd(state))
            {
                SkipNewlines(state);

                if (AtEnd(state) || Peek(state).type == TokenTypes::End)
                {
                    break;
                }

                if (Peek(state).type == TokenTypes::Fn)
                {
                    declaration->methods.push_back(ParseFunction(state));
                    continue;
                }

                if (Peek(state).type == TokenTypes::New)
                {
                    if (declaration->hasConstructor)
                    {
                        throw std::runtime_error("Unexpected token.");
                    }

                    Advance(state);
                    declaration->constructor = ParseBlock(state, false);
                    Expect(state, TokenTypes::End, "Expected 'End'.");
                    declaration->hasConstructor = true;
                    continue;
                }

                if (Peek(state).type == TokenTypes::Const)
                {
                    Advance(state);
                    FieldDeclaration field = ParseField(state);
                    field.isConstant = true;
                    declaration->fields.push_back(std::move(field));
                    continue;
                }

                declaration->fields.push_back(ParseField(state));
            }

            Expect(state, TokenTypes::End, "Expected 'End'.");

            return declaration;
        }

        std::unique_ptr<SceneDeclaration> ParseScene(ParseState &state)
        {
            Expect(state, TokenTypes::Scene, "Expected 'Scene'.");

            auto declaration = std::make_unique<SceneDeclaration>();
            declaration->name =
                Expect(state, TokenTypes::Identifier, "Expected a name.").value;

            while (!AtEnd(state))
            {
                SkipNewlines(state);

                if (AtEnd(state) || Peek(state).type == TokenTypes::End)
                {
                    break;
                }

                if (Peek(state).type == TokenTypes::Update)
                {
                    Advance(state);
                    declaration->update = ParseBlock(state, false);
                    Expect(state, TokenTypes::End, "Expected 'End'.");
                    continue;
                }

                if (Peek(state).type == TokenTypes::Identifier &&
                    Peek(state).value == "Draw")
                {
                    Advance(state);
                    declaration->draw = ParseBlock(state, false);
                    Expect(state, TokenTypes::End, "Expected 'End'.");
                    continue;
                }

                if (Peek(state).type == TokenTypes::OnMouseDown)
                {
                    Advance(state);

                    if (!AtEnd(state) &&
                        Peek(state).type == TokenTypes::LeftParen)
                    {
                        Advance(state);

                        const std::string typeName =
                            Expect(state, TokenTypes::Identifier,
                                   "Expected a type.")
                                .value;

                        if (typeName != "MouseButtons")
                        {
                            throw std::runtime_error("Unknown type.");
                        }

                        declaration->onMouseDownParameter =
                            Expect(state, TokenTypes::Identifier,
                                   "Expected a name.")
                                .value;

                        Expect(state, TokenTypes::RightParen, "Expected ')'.");
                    }

                    declaration->onMouseDown = ParseBlock(state, false);
                    Expect(state, TokenTypes::End, "Expected 'End'.");
                    continue;
                }

                if (Peek(state).type == TokenTypes::OnKeyDown)
                {
                    Advance(state);

                    if (!AtEnd(state) &&
                        Peek(state).type == TokenTypes::LeftParen)
                    {
                        Advance(state);

                        const std::string typeName =
                            Expect(state, TokenTypes::Identifier,
                                   "Expected a type.")
                                .value;

                        if (typeName != "Keys")
                        {
                            throw std::runtime_error("Unknown type.");
                        }

                        declaration->onKeyDownParameter =
                            Expect(state, TokenTypes::Identifier,
                                   "Expected a name.")
                                .value;

                        Expect(state, TokenTypes::RightParen, "Expected ')'.");
                    }

                    declaration->onKeyDown = ParseBlock(state, false);
                    Expect(state, TokenTypes::End, "Expected 'End'.");
                    continue;
                }

                if (Peek(state).type == TokenTypes::OnEnter)
                {
                    Advance(state);
                    declaration->onEnter = ParseBlock(state, false);
                    Expect(state, TokenTypes::End, "Expected 'End'.");
                    continue;
                }

                throw std::runtime_error("Unexpected token.");
            }

            Expect(state, TokenTypes::End, "Expected 'End'.");

            return declaration;
        }
    }

    std::vector<std::unique_ptr<Statement>>
    Parser::Parse(const std::vector<Token> &tokens)
    {
        std::vector<std::unique_ptr<Statement>> statements;

        if (tokens.empty())
        {
            return statements;
        }

        ParseState state{tokens, 0};

        while (!AtEnd(state))
        {
            SkipNewlines(state);

            if (AtEnd(state))
            {
                break;
            }

            if (Peek(state).type == TokenTypes::Fn)
            {
                statements.push_back(ParseFunction(state));
            }
            else if (Peek(state).type == TokenTypes::Import)
            {
                statements.push_back(ParseImportStatement(state));
            }
            else if (Peek(state).type == TokenTypes::Class)
            {
                statements.push_back(ParseClass(state));
            }
            else if (Peek(state).type == TokenTypes::Scene)
            {
                statements.push_back(ParseScene(state));
            }
            else
            {
                statements.push_back(ParseStatement(state));
            }
        }

        return statements;
    }
}
