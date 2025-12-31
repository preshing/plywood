# Getting Started

[CMake](https://cmake.org/) is required to build the sample applications. A general guide to building CMake-based applications can be found [here](https://preshing.com/20170511/how-to-build-a-cmake-based-project/). Otherwise, here are some quick steps to get you started.

First, clone the Plywood repository:

    $ git clone https://github.com/preshing/plywood.git

Next, navigate to the `apps/base-tests` subdirectory and generate project files for the tests. (If you're running on Windows, use backslashes `\` in the directory path instead.)

    $ cd plywood/apps/base-tests
    $ mkdir build
    $ cd build
    $ cmake ..

If CMake's Visual Studio or Xcode generator was used, you can now open the project files in your IDE. Otherwise, you can build the tests from the command line:

    $ cmake --build .

Finally, run the executable. If using the Visual Studio generator, the executable will be located at `Debug\base-tests.exe`.

    $ ./base-tests

There are several other sample applications in the `apps` subdirectory that you can also build and run.
