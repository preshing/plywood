Introduction
============

Plywood is a cross-platform C++ runtime library that can be used as an alternative to the C and C++ Standard Libraries. It aims to deliver everything you need from a runtime library in a small amount of code with an easy-to-use API.

Plywood comes bundled with several higher-level C++ libraries, including a full [agent harness](/docs/high-level/agent-harness.md), providing an easy way to add agentic AI capabilities to existing C++ programs.

Plywood requires no dependencies other than your target platform's native SDK. It can coexist alongside other runtime libraries in the same application, so you can adopt it incrementally. CMake support is provided for the [sample applications](/docs/apps/index.md), but any build system can be used just by adding a few Plywood source/header files and setting configuration options.

## Getting Started

Clone the repository from [GitHub](https://github.com/preshing/plywood).

```
$ git clone https://github.com/preshing/plywood.git
$ cd plywood
```

Make sure you have a C++ compiler installed using your system package manager or by installing a C++ IDE such as Visual Studio or Xcode. If [CMake](https://cmake.org/) is installed, the following command will create a build system for Plywood's test suite, build it, then run all available tests. On Windows, use `share\build-app.bat` instead.

```
$ share/build-app.sh test-suite --run --all
```
