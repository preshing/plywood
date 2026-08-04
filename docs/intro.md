# Introduction

Plywood is a cross-platform C++ runtime library that can be used as an alternative to the standard C and C++ libraries. It aims to deliver everything you need from a runtime library in a small package using a simple API.

It includes a built-in harness for coding agents

## Getting Started

[CMake](https://cmake.org/) is required to build the sample applications.

On macOS and Linux, run the following command. On Windows, run `share\build-app.bat` instead.

    $ git clone https://github.com/preshing/plywood.git
    $ cd plywood
    $ share/build-app.sh test-suite -run -all

It uses Visual Studio automatically if installed.

[Link to sample applications page]

## Project Structure

    plywood/
    ├── apps/
    ├── bin/
    ├── docs/
    │   └── ...
    ├── share/
    └── src/
        └── ...

[`ply-base.h`](docs/base) is an all-in-one header file where operating system features and common functions and data structures are exposed.
