Introduction
============

Plywood is a portable C++ runtime library that can be used as an alternative to the C and C++ Standard Libraries. It aims to deliver everything you need from a runtime library in a small code size and a programmer-friendly API.

Plywood comes bundled with many higher-level C++ libraries, including a full [AI agent harness](/docs/agent-harness.md), providing an easy way to add agentic AI capabilities to existing C++ programs.

Plywood requires no dependencies other than your target platform's native SDK. It can coexist alongside other runtime libraries in the same application, so you can adopt it incrementally. CMake support is provided for the included [sample applications](/docs/sample-apps/index.md), but any build system can be used simply by adding the necessary Plywood source/header files and configuration options.

## Getting Started

Clone the repository from [GitHub](https://github.com/preshing/plywood).

    $ git clone https://github.com/preshing/plywood.git
    $ cd plywood

If [CMake](https://cmake.org/) is installed, the following command will create a build system for the test suite, build it, then run all available tests. On Windows, use `share\build-app.bat` instead.

    $ share/build-app.sh test-suite -run -all
