#pragma once

namespace ForradiaLang
{
    enum class StatementKind
    {
        Int,
        Assignment,
        For,
        Continue,
        Return,
        Print,
        If,
        Function,
        Class,
        Scene,
        Import,
        Object,
        Group,
        MethodCall,
        FunctionCall
    };

    class Statement
    {
      public:
        explicit Statement(StatementKind kind) : kind(kind)
        {
        }

        virtual ~Statement() = default;

        StatementKind kind;
    };
}
