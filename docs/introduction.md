# Introduction

Plywood is a cross-platform C++ runtime library that can be used as an alternative to the Standard C and C++ Libraries. It aims to deliver everything you need from a runtime library in a straightforward API using as few lines of code as possible.

Plywood's modularity and compact size make it easy to add to existing C++ projects or use as the starting point of existing projects without any dependencies other than a C++ compiler and the system SDK of your target platform.

## Getting Started

[CMake](https://cmake.org/) is required to build the sample applications.

On macOS and Linux, run the following command. On Windows, run `share\build-app.bat` instead.

    $ git clone https://github.com/preshing/plywood.git
    $ cd plywood
    $ share/build-app.sh test-suite -run -all

It uses Visual Studio automatically if installed.

[Learn more about the sample applications.](/docs/sample-apps/index.md)
