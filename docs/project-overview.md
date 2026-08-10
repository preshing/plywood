Project Overview
================

Plywood is organized into 9 main components:

- [Operating System](/docs/system/index.md): Memory, timers, file systems, threads and processes.
- [2D and 3D Math](/docs/math.md): Suitable for games and UI layouts.
- [Reflection](/docs/reflect.md): Generic processing of data stored in Plywood containers.
- [Networking](/docs/networking.md): Client and server-side TCP and HTTP.
- [Customizable Tokenizer](/docs/high-level/tokenizer.md): Break input text into various token types.
- [JSON Support](/docs/high-level/json-support.md): Read and write JSON documents.
- [Markdown Parser](/docs/high-level/markdown-parser.md): Parse Markdown documents and convert to HTML.
- [C++ Parser](/docs/high-level/cpp-parser.md): Parse C++ source code and generate syntax trees.
- [Agent Harness](/docs/agent-harness.md): Spawn LLM agents with local tool access using remote inference providers.

## Directory Layout

```
plywood/
├── apps/
├── bin/
├── docs/
│   └── ...
├── share/
└── src/
    └── ...
```

[`ply-system.h`](/docs/system/index.md) is an all-in-one header file where operating system features and common functions and data structures are exposed.
