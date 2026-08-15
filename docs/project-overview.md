Project Overview
================

The Plywood runtime library consists of four main components and several higher-level libraries. Each component can be integrated separately depending on the needs of your project. Several sample applications are also included.

**Main Runtime Library:**

- [System (`ply-system.h`)](/docs/system/index.md): Memory, timers, file systems, threads and processes.
- [Networking (`ply-network.h`)](/docs/networking.md): Client and server-side TCP and HTTP.
- [2D and 3D Math (`ply-math.h`)](/docs/math.md): Suitable for games and UI layouts.
- [Reflection (`ply-reflect.h`)](/docs/reflect.md): Generic processing of data stored in Plywood data structures.

**Higher-Level Libraries:**

- [Agent Harness (`ply-agent.h`)](/docs/high-level/agent-harness.md): Spawn local agents with custom tools using remote inference providers.
- [JSON Support (`ply-json.h`)](/docs/high-level/json-support.md): Import and export JSON documents.
- [Markdown Parser (`ply-markdown.h`)](/docs/high-level/markdown-parser.md): Import Markdown documents and export to HTML.
- [C++ Parser (`ply-cpp.h`)](/docs/high-level/cpp-parser.md) (experimental): Parse C++ source code and generate syntax trees.
- [Tokenizer (`ply-tokenizer.h`)](/docs/high-level/tokenizer.md): Break input text into common token types.

**Sample Applications:**

- [`agent`](/docs/apps/agent.md): Command-line agent with built-in web UI.
- [`banner-comment`](/docs/apps/banner-comment.md): Generate banner comments.
- [`generate-docs`](/docs/apps/generate-docs.md): Generate the HTML version of Plywood's documentation.
- [`serve-docs`](/docs/apps/serve-docs.md): Serve the HTML documentation locally.
- [`test-suite`](/docs/apps/test-suite.md): Automated tests to validate Plywood's API-correctness.

## Directory Structure

The Plywood repository uses following directory structure.:

- Library source code is in `src/`.
- Documentation is in `docs/`.
- Sample applications are in `apps/`.
- Build scripts are in `share/`.
- Build outputs are written to `bin/`.

```
plywood/
├── .clang-format                   # Coding style used throughout Plywood.
├── agent.json                      # Internal agent harness settings.
├── apps/                           # Sample applications.
│   ├── agent/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── build/
│   │   └── ...
│   ├── banner-comment/
│   ├── generate-docs/
│   ├── serve-docs/
│   └── test-suite/
├── bin/                            # Sample application output folder.
│   ├── agent[.exe]
│   └── ...
├── docs/
│   ├── table-of-contents.md
│   ├── build/                      # generate-docs output folder.
│   └── ...
├── share/
│   ├── build-app.bat               # Sample build script (Windows).
│   ├── build-app.sh                # Sample build script.
│   └── ...
└── src/                            # Runtime library source code.
    ├── ply-system.h
    ├── ply-system.cpp
    ├── ply-math.h
    ├── ply-math.cpp
    └── ...
```

## Namespace

All Plywood functions and types are defined in the `ply` namespace. Some higher-level libraries use nested namespaces, like `ply::markdown`. It's common to omit the `ply::` qualifier by importing the `ply` namespace in user code. Throughout the rest of this documentation, the `ply::` qualifier is usually omitted.

```
#include <ply-markdown.h>

// Import the ply namespace so that the ply:: qualifier isn't needed.
using namespace ply;

Array<Owned<markdown::Span>> spans = markdown::parseInlineSpans("Hello, **world!**");
```

## Coding Style

- C++14 language features only.
- Use the provided `.clang-format` rules for formatting.
- Type names in `PascalCase`; variable and function names in `camelCase`.
- Each code block should begin with a brief comment to explain what it does.
- Use the fewest line of code possible without sacrificing readability.
- When the body of an `if` or `else` statement consists of exactly one `continue`, `return`, or `break` statement, omit curly braces.
