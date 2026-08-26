`generate-docs`
===============

Generates the documentation website in `docs/build` from the Markdown pages listed in `docs/contents.json`.

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-w` | `--watch` | Watch for changes and regenerate the documentation. |
| `-h` | `--help` | Print the available options. |

Without the watch option, the app generates the documentation once and exits. Watch mode is available on platforms
that support directory watching.

Each page entry must contain a unique `/docs/*.md` path that resolves to an existing Markdown file. If any page path
is malformed, duplicated or missing, generation stops before changing the last successful output.
