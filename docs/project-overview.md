Project Overview
================

The Plywood runtime library consists of four main components and several higher-level libraries. Each component can be integrated separately depending on the needs of your project. Several sample applications are also included.

**Main Runtime Library:**

- [System (`ply-system.h`)](/docs/system/index.md): Memory, timers, file systems, threads and processes.
- [Networking (`ply-network.h`)](/docs/networking.md): Client and server-side TCP and HTTP.
- [Math (`ply-math.h`)](/docs/math.md): Scalar and vector math suitable for games and UI layouts.
- [Reflection (`ply-reflect.h`)](/docs/reflect.md): Generic processing of data stored in Plywood data structures.

**Higher-Level Libraries:**

- [Agent Harness (`ply-agent.h`)](/docs/high-level/agent-harness.md): Spawn local agents with custom tools using remote inference providers.
- [JSON Support (`ply-json.h`)](/docs/high-level/json-support.md): Import and export JSON documents.
- [Markdown Parser (`ply-markdown.h`)](/docs/high-level/markdown-parser.md): Import Markdown documents and export to HTML.
- [C++ Parser (`ply-cpp.h`)](/docs/high-level/cpp-parser.md) (experimental): Parse C++ source code and generate syntax trees.
- [Tokenizer (`ply-tokenizer.h`)](/docs/high-level/tokenizer.md): Break input text into common token types.

**Sample Applications:**

- [`agent`](/docs/apps/agent.md): Command-line agent with built-in web UI.
- [`test-suite`](/docs/apps/test-suite.md): Automated tests to validate Plywood's API-correctness.
- [`generate-docs`](/docs/apps/generate-docs.md): Generate the HTML version of Plywood's documentation.
- [`serve-docs`](/docs/apps/serve-docs.md): Serve the HTML documentation locally.
- [`banner-comment`](/docs/apps/banner-comment.md): Generate banner comments.

## Directory Structure

The Plywood repository has the following directory structure. The `build` and `bin` folders aren't stored in Git.

```
plywood/
├── src/                        # Library source code.
│   ├── ply-system.h
│   ├── ply-system.cpp
│   ├── ply-math.h
│   ├── ply-math.cpp
│   └── ...
├── docs/                       # Documentation.
│   ├── table-of-contents.md
│   ├── ...
│   └── build/                  # HTML output files written by generate-docs.
├── apps/                       # Sample applications.
│   ├── agent/
│   │   ├── main.cpp
│   │   ├── CMakeLists.txt
│   │   ├── build/              # Intermediate build folder.
│   │   └── ...
│   ├── test-suite/
│   ├── generate-docs/
│   ├── serve-docs/
│   └── banner-comment/
├── share/
│   ├── build-app.sh            # Sample build script.
│   ├── build-app.bat           # Sample build script (Windows).
│   └── ...
├── bin/                        # Output executables.
│   ├── agent[.exe]
│   └── ...
└── agent.json                  # Agent harness settings.
```

## Namespace

All Plywood functions and types are defined in the `ply` namespace. Some higher-level libraries create nested namespaces, such as `ply::markdown`. When possible, importing `ply` directly into the global namespace is a convenient way to simplify name lookup.

```
#include <ply-markdown.h>

using namespace ply;  // Import into global namespace.

Array<Owned<markdown::Block>> blocks = markdown::parse("Hello, *world!*");
```
