Plywood C++ Runtime Library
===========================

Plywood is a cross-platform C++ runtime library that can be used as an alternative to the C and C++ Standard Libraries. It aims to deliver everything you need from a runtime library in a small amount of code with an easy-to-use API. Several higher-level libraries are also available.

For a more complete overview of the project, see [Project Overview](docs/project-overview.md).

Official website: https://plywood.dev
GitHub repository: https://github.com/preshing/plywood

## Coding Style

- C++14 language features only.
- Use the provided `.clang-format` rules for formatting.
- Type names in `PascalCase`; variable and function names in `camelCase`.
- Every signiciant code block should begin with a brief comment to explain what it does.
- Use the fewest line of code possible without sacrificing readability.
- Use `this->` to refer to other function and data members within each member function.
- The body of every `if`, `else`, `do`, `while` and `for` statement should be surrounded by curly braces unless it consists of exactly one `continue`, `return`, or `break` statement.
