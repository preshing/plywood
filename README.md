# The Plywood C++ Base Library

Plywood is a low-level C++ library for building cross-platform native software. It provides a simple, portable C++ interface over OS features and commonly-used data structures & algorithms. Its compact size and lack of dependencies make it easy to integrate and fast to compile.

Plywood's features are divided into `.cpp`/`.h` pairs located in the `src` folder. Integrating a feature into your project is a matter of including its header file, compiling its source file and linking with the resulting object file.

* `<ply-base.h>` (4309 lines): Operating system access, commonly-used data structures, Unicode support.
* `<ply-math.h>` (1750 lines): Matrix, vector and quaternion types for graphics and game development.
* `<ply-network.h>` (270 lines): TCP/IP network interface supporting IPv4 and IPv6.
* `<ply-btree.h>` (895 lines): B-Tree implementation for sorted key-value storage.
* `<ply-tokenizer.h>` (153 lines): Common routines for reading tokens from text.
* `<ply-json.h>` (285 lines): JSON parser and serializer.
* `<ply-markdown.h>` (132 lines): Markdown parser with HTML output.
* `<ply-cpp.h>` (439 lines): Experimental C++ parser (mainly for documentation generation purposes).

There are several sample applications in the `apps` folder that demonstrate how to use the library. CMake is required to build them. Detailed instructions can be found in the [Getting Started](https://plywood.dev/docs/getting-started) guide.
