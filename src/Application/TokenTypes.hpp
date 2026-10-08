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
        Star,
        Slash,
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
        ElseIf,
        End,
        Print,
        Int,
        Double,
        For,
        To,
        Next,
        Fn,
        Class,
        Import,
        Scene,
        Update,
        OnMouseDown,
        OnKeyDown,
        OnEnter
    };
}