Project Overview
================

Plywood is organized into 9 main components:

- [Operating System](/docs/system/index.md): Timers, filesystems, processes, threads, virtual memory and basic data containers.
- [2D and 3D Math](/docs/math.md): Suitable for games and UI layouts.
- [Reflection](/docs/reflect.md): Generic processing of data stored in Plywood containers.
- [Networking](/docs/networking.md): Client and server-side TCP and HTTP.
- [Customizable Tokenizer](/docs/text/tokenizer.md): Break input text into various token types.
- [JSON Support](/docs/text/json-support.md): Read and write JSON documents. Automatically convert to/from Plywood containers.
- [Markdown Parser](/docs/text/markdown-parser.md): Parse Markdown documents and convert to HTML.
- [C++ Parser](/docs/text/cpp-parser.md): Parse C++ source code and generate syntax trees.
- [Agent Harness](/docs/agent-harness.md): Spawn LLM agents with local tool access using remote inference providers.

## Directory Structure

    plywood/
    ├── apps/
    ├── bin/
    ├── docs/
    │   └── ...
    ├── share/
    └── src/
        └── ...

[`ply-system.h`](/docs/system/index.md) is an all-in-one header file where operating system features and common functions and data structures are exposed.
