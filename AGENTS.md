Plywood C++ Runtime Library
===========================

Plywood is a cross-platform C++ runtime library that can be used as an alternative to the C and C++ Standard Libraries. It aims to deliver everything you need from a runtime library in a small amount of code with an easy-to-use API. Several higher-level libraries are also included.

Full documentation is available in the `docs/` folder. An online version is hosted at https://plywood.dev/docs. Start with [Introduction](docs/introduction.md) and [Project Overview](docs/project-overview.md).

## Coding Style

- Use C++14 language features only.
- Use the provided `.clang-format` for formatting.
- Type and enumerator names in `PascalCase`; variable and function names in `camelCase`.
- Every significant code block should begin with a brief comment to explain what it does.
- Use the fewest line of code possible without sacrificing readability.
- Always use `this->` to refer to member functions and member variables inside the same class.
- The body of every `if`, `else`, `do`, `while` and `for` statement must be surrounded by `{` curly braces `}`, unless it consists of a single `continue`, `return`, or `break` statement, in which case curly braces must be omitted.
- Use the `banner-comment` sample application to generate banner comments that label significant source file sections.
