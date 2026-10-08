#pragma once

namespace ForradiaLang
{
    enum class TokenTypes
    {
        Identifier,
        Number,
        String,
        Plus,
        Minus,
        Percent,
        Equals,
        GreaterThan,
        LessThan,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket,
        Dot,
        Comma,
        Newline,
        If,
        Then,
        Else,
        End,
        Print,
        Int,
        Fn,
        Class,
        Import,
        Scene,
        Update,
        Draw
    };
}