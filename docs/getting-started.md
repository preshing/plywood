# Getting Started

[CMake](https://cmake.org/) is required to build the sample applications. A general guide to building CMake-based applications can be found [here](https://preshing.com/20170511/how-to-build-a-cmake-based-project/). Otherwise, here are some quick steps to get you started.

First, clone the Plywood repository:

    $ git clone https://github.com/preshing/plywood.git

Next, navigate to the `apps/test-suite` subdirectory and generate project files for the tests. (If you're running on Windows, use backslashes `\` in the directory path instead.)

    $ cd plywood/apps/test-suite
    $ mkdir build
    $ cd build
    $ cmake ..

If CMake's Visual Studio or Xcode generator was used, you can now open the project files in your IDE. Otherwise, you can build the tests from the command line:

    $ cmake --build .

Finally, run all test suites. If using the Visual Studio generator, the executable will be located at `Debug\test-suite.exe`.

    $ ./test-suite -all

Individual suites can be selected using `-base`, `-unicode`, `-markdown`, `-cpp` and `-frag`.
Multiple options run in command-line order. Use `-regencpp` in place of `-cpp` to regenerate the C++
parser and preprocessor golden files.

The executable can be restricted at configure time using the `WITH_BASE_TESTS`,
`WITH_UNICODE_LOADING_TESTS`, `WITH_MARKDOWN_TESTS`, `WITH_CPP_TESTS` and
`WITH_FRAGMENTATION_TEST` CMake variables. If any variable is nonzero, only nonzero suites are
included. Otherwise, variables set to zero exclude those suites and unspecified suites remain enabled.
For example, `cmake -DWITH_BASE_TESTS=1 ..` builds a base-only test runner.

There are several other sample applications in the `apps` subdirectory that you can also build and run.
