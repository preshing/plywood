# Introduction

Plywood is a portable C++ runtime library that can be used as an alternative to the C and C++ Standard Libraries. It aims to deliver everything you need from a runtime library in a small code size while offering a programmer-friendly API.

Plywood is easy to integrate into existing C++ projects and has zero external dependencies other than the target platform's native SDK. It coexists well alongside other runtime libraries in a single application, so you can adopt it incrementally. CMake support is provided for the sample applications, but any build system can be used simply by adding the necessary Plywood source/header files and setting configuration options.

## Getting Started

Clone the repository from [GitHub](https://github.com/preshing/plywood).

    $ git clone https://github.com/preshing/plywood.git
    $ cd plywood

If [CMake](https://cmake.org/) is installed, the following command will create a build system for the test suite, build it, then run all available tests. On Windows, use `share\build-app.bat` instead.

    $ share/build-app.sh test-suite -run -all

For more information about the sample applications, see [Sample Applications](/docs/sample-apps/index.md). Otherwise, proceed to the [Project Overview](/docs/project-overview).
