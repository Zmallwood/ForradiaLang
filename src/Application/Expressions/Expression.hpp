#pragma once

namespace ForradiaLang
{
    enum class ExpressionKind
    {
        Number,
        String,
        Variable,
        Binary,
        Unary,
        Call,
        Member,
        Index,
        List
    };

    class Expression
    {
      public:
        explicit Expression(ExpressionKind kind) : kind(kind)
        {
        }

        virtual ~Expression() = default;

        ExpressionKind kind;
    };
}
