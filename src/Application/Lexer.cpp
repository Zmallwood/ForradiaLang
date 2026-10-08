#include "Lexer.hpp"

namespace ForradiaLang
{
    namespace
    {
        bool IsSpace(char character)
        {
            return character == ' ' || character == '\t';
        }

        bool IsDigit(char character)
        {
            return character >= '0' && character <= '9';
        }

        bool IsAlpha(char character)
        {
            return (character >= 'a' && character <= 'z') ||
                   (character >= 'A' && character <= 'Z');
        }

        TokenTypes TypeForWord(std::string_view word)
        {
            if (word == "If")
            {
                return TokenTypes::If;
            }

            if (word == "Then")
            {
                return TokenTypes::Then;
            }

            if (word == "Else")
            {
                return TokenTypes::Else;
            }

            if (word == "ElseIf")
            {
                return TokenTypes::ElseIf;
            }

            if (word == "And")
            {
                return TokenTypes::And;
            }

            if (word == "Or")
            {
                return TokenTypes::Or;
            }

            if (word == "Not")
            {
                return TokenTypes::Not;
            }

            if (word == "End")
            {
                return TokenTypes::End;
            }

            if (word == "Print")
            {
                return TokenTypes::Print;
            }

            if (word == "Int")
            {
                return TokenTypes::Int;
            }

            if (word == "Double")
            {
                return TokenTypes::Double;
            }

            if (word == "For")
            {
                return TokenTypes::For;
            }

            if (word == "To")
            {
                return TokenTypes::To;
            }

            if (word == "Next")
            {
                return TokenTypes::Next;
            }

            if (word == "Continue")
            {
                return TokenTypes::Continue;
            }

            if (word == "Return")
            {
                return TokenTypes::Return;
            }

            if (word == "Fn")
            {
                return TokenTypes::Fn;
            }

            if (word == "Class")
            {
                return TokenTypes::Class;
            }

            if (word == "Group")
            {
                return TokenTypes::Group;
            }

            if (word == "Const")
            {
                return TokenTypes::Const;
            }

            if (word == "New")
            {
                return TokenTypes::New;
            }

            if (word == "Import")
            {
                return TokenTypes::Import;
            }

            if (word == "Scene")
            {
                return TokenTypes::Scene;
            }

            return TokenTypes::Identifier;
        }
    }

    std::vector<Token> Lexer::Tokenize(std::string_view source)
    {
        std::vector<Token> tokens;

        for (std::size_t index = 0; index < source.size();)
        {
            const char character = source[index];

            if (character == '\r' || character == '\n')
            {
                while (index < source.size() &&
                       (source[index] == '\r' || source[index] == '\n'))
                {
                    if (source[index] == '\r' && index + 1 < source.size() &&
                        source[index + 1] == '\n')
                    {
                        index += 2;
                    }
                    else
                    {
                        ++index;
                    }
                }

                tokens.push_back({TokenTypes::Newline, "\n"});
                continue;
            }

            if (IsSpace(character))
            {
                ++index;
                continue;
            }

            if (character == '_' &&
                (index + 1 >= source.size() ||
                 !(IsAlpha(source[index + 1]) || IsDigit(source[index + 1]) ||
                   source[index + 1] == '_')))
            {
                std::size_t lookAhead = index + 1;

                while (lookAhead < source.size() && IsSpace(source[lookAhead]))
                {
                    ++lookAhead;
                }

                if (lookAhead < source.size() && source[lookAhead] == '\'')
                {
                    while (lookAhead < source.size() &&
                           source[lookAhead] != '\r' &&
                           source[lookAhead] != '\n')
                    {
                        ++lookAhead;
                    }
                }

                if (lookAhead < source.size() &&
                    (source[lookAhead] == '\r' || source[lookAhead] == '\n'))
                {
                    index = lookAhead;

                    if (source[index] == '\r' && index + 1 < source.size() &&
                        source[index + 1] == '\n')
                    {
                        index += 2;
                    }
                    else
                    {
                        ++index;
                    }

                    continue;
                }
            }

            if (character == '\'')
            {
                while (index < source.size() && source[index] != '\r' &&
                       source[index] != '\n')
                {
                    ++index;
                }

                continue;
            }

            if (IsDigit(character))
            {
                const std::size_t start = index;

                while (index < source.size() && IsDigit(source[index]))
                {
                    ++index;
                }

                if (index + 1 < source.size() && source[index] == '.' &&
                    IsDigit(source[index + 1]))
                {
                    ++index;

                    while (index < source.size() && IsDigit(source[index]))
                    {
                        ++index;
                    }
                }

                tokens.push_back(
                    {TokenTypes::Number,
                     std::string(source.substr(start, index - start))});
                continue;
            }

            if (IsAlpha(character) || character == '_')
            {
                const std::size_t start = index;

                while (index < source.size() &&
                       (IsAlpha(source[index]) || IsDigit(source[index]) ||
                        source[index] == '_'))
                {
                    ++index;
                }

                const std::string word(source.substr(start, index - start));

                tokens.push_back({TypeForWord(word), word});
                continue;
            }

            if (character == '"')
            {
                ++index;

                const std::size_t start = index;

                while (index < source.size() && source[index] != '"')
                {
                    ++index;
                }

                tokens.push_back(
                    {TokenTypes::String,
                     std::string(source.substr(start, index - start))});

                if (index < source.size())
                {
                    ++index;
                }

                continue;
            }

            if (character == '=' && index + 1 < source.size() &&
                source[index + 1] == '=')
            {
                tokens.push_back({TokenTypes::Equals, "=="});
                index += 2;
                continue;
            }

            if (character == '>' && index + 1 < source.size() &&
                source[index + 1] == '=')
            {
                tokens.push_back({TokenTypes::GreaterOrEqual, ">="});
                index += 2;
                continue;
            }

            if (character == '+' && index + 1 < source.size() &&
                source[index + 1] == '=')
            {
                tokens.push_back({TokenTypes::PlusEquals, "+="});
                index += 2;
                continue;
            }

            if (character == '-' && index + 1 < source.size() &&
                source[index + 1] == '=')
            {
                tokens.push_back({TokenTypes::MinusEquals, "-="});
                index += 2;
                continue;
            }

            TokenTypes type = TokenTypes::Identifier;
            bool matched = false;

            switch (character)
            {
            case '+':
                type = TokenTypes::Plus;
                matched = true;
                break;

            case '-':
                type = TokenTypes::Minus;
                matched = true;
                break;

            case '*':
                type = TokenTypes::Star;
                matched = true;
                break;

            case '/':
                type = TokenTypes::Slash;
                matched = true;
                break;

            case '%':
                type = TokenTypes::Percent;
                matched = true;
                break;

            case '=':
                type = TokenTypes::Equals;
                matched = true;
                break;

            case '>':
                type = TokenTypes::GreaterThan;
                matched = true;
                break;

            case '<':
                type = TokenTypes::LessThan;
                matched = true;
                break;

            case '(':
                type = TokenTypes::LeftParen;
                matched = true;
                break;

            case ')':
                type = TokenTypes::RightParen;
                matched = true;
                break;

            case '[':
                type = TokenTypes::LeftBracket;
                matched = true;
                break;

            case ']':
                type = TokenTypes::RightBracket;
                matched = true;
                break;

            case '.':
                type = TokenTypes::Dot;
                matched = true;
                break;

            case ',':
                type = TokenTypes::Comma;
                matched = true;
                break;

            case '?':
                type = TokenTypes::Question;
                matched = true;
                break;

            default:
                break;
            }

            if (matched)
            {
                tokens.push_back({type, std::string(1, character)});
            }

            ++index;
        }

        return tokens;
    }
}
