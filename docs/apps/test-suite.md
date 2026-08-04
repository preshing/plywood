Individual suites can be selected using `-base`, `-unicode`, `-markdown`, `-cpp` and `-frag`.
Multiple options run in command-line order. Use `-regencpp` in place of `-cpp` to regenerate the C++
parser and preprocessor golden files.

The executable can be restricted at configure time using the `WITH_BASE_TESTS`,
`WITH_UNICODE_LOADING_TESTS`, `WITH_MARKDOWN_TESTS`, `WITH_CPP_TESTS` and
`WITH_FRAGMENTATION_TEST` CMake variables. If any variable is nonzero, only nonzero suites are
included. Otherwise, variables set to zero exclude those suites and unspecified suites remain enabled.
For example, `cmake -DWITH_BASE_TESTS=1 ..` builds a base-only test runner.
