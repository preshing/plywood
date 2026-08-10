`generate-docs`
===============

Generates the documentation website in `docs/build` from the Markdown pages listed in `docs/contents.json`.

Each page entry must contain a unique `/docs/*.md` path that resolves to an existing Markdown file. If any page path
is malformed, duplicated or missing, generation stops before changing the last successful output.
