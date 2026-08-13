Project Overview
================

The Plywood C++ runtime library consists of four main modules. You don't have to use all of them; each one can be integrated separately depending on the needs of your project.

- [System](/docs/system/index.md): Memory, timers, file systems, threads and processes.
- [Networking](/docs/networking.md): Client and server-side TCP and HTTP.
- [2D and 3D Math](/docs/math.md): Suitable for games and UI layouts.
- [Reflection](/docs/reflect.md): Generic processing of data stored in Plywood data structures.

Plywood also comes with several higher-level C++ libraries:

- [Agent Harness](/docs/high-level/agent-harness.md): Spawn local AI agents with custom tools using remote inference providers.
- [JSON Support](/docs/high-level/json-support.md): Import and export JSON documents.
- [Markdown Parser](/docs/high-level/markdown-parser.md): Import Markdown documents and export to HTML.
- [C++ Parser](/docs/high-level/cpp-parser.md) (experimental): Parse C++ source code and generate syntax trees.
- [Tokenizer](/docs/high-level/tokenizer.md): Break input text into common token types.

Sample applications are also included:

- [`agent`](/docs/apps/agent.md): Command-line agent with built-in web UI.
- [`banner-comment`](/docs/apps/banner-comment.md): Generate banner comments.
- [`generate-docs`](/docs/apps/generate-docs.md): Generate the HTML version of Plywood's documentation.
- [`serve-docs`](/docs/apps/serve-docs.md): Serve the HTML documentation locally.
- [`test-suite`](/docs/apps/test-suite.md): Automated tests to validate Plywood's API-correctness.

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
