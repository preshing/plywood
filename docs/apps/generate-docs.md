`generate-docs`
===============

Generates the documentation website in `docs/build` from the Markdown pages listed in `docs/table-of-contents.md`.

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-w` | `--watch` | Watch for changes and regenerate the documentation. |
| `-h` | `--help` | Print the available options. |

Without the watch option, the app generates the documentation once and exits. Watch mode is available on platforms
that support directory watching.
