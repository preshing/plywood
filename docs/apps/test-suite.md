`test-suite`
============

Available command-line options:

| Short | Long | Description |
|---|---|---|
| `-s` | `--system` | Run the system test suite. |
| `-n` | `--network` | Run the network test suite. |
| `-u` | `--unicode` | Run the Unicode file loading test suite. |
| `-m` | `--markdown` | Run the Markdown test suite. |
| `-c` | `--cpp` | Run the C++ parser and preprocessor test suites. |
| `-r` | `--regen-cpp` | Regenerate the C++ parser and preprocessor golden files. |
| `-t` | `--transcript` | Run the transcript tests. |
| `-f` | `--fragmentation` | Run the fragmentation test suite. |
| `-a` | `--all` | Run every compiled-in suite; may be combined with the verbose option. |
| `-v` | `--verbose` | Print detailed test progress and output. |
| `-h` | `--help` | Print the available options. |

Multiple suite options run in command-line order. Use the regenerate option in place of the C++ option
to regenerate its golden files.

By default, the executable prints one counted result line for each selected test category. Failure
diagnostics are always printed.

The executable can be restricted at configure time using the `WITH_SYSTEM_TESTS`, `WITH_NETWORK_TESTS`,
`WITH_UNICODE_LOADING_TESTS`, `WITH_MARKDOWN_TESTS`, `WITH_CPP_TESTS`, `WITH_TRANSCRIPT_TESTS` and
`WITH_FRAGMENTATION_TEST` CMake variables. If any variable is nonzero, only nonzero suites are included.
Otherwise, variables set to zero exclude those suites and unspecified suites remain enabled. For example,
`cmake -DWITH_NETWORK_TESTS=1 ..` builds a network-only test runner.
