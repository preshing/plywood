Processes (`ply-system.h`)
========================

`PID getCurrentProcessId()`
> Returns the operating system's process ID for the current process. See also `getCurrentThreadId`.

`String getCurrentExecutablePath()`
> Returns the path to the executable file for the current process.

`String getEnvironmentVariable(StringView name)`
> Returns the value of the environment variable specified by `name`. Returns an empty string if the variable is unset.

## `Subprocess`

The `Subprocess` class represents a child process. You can spawn processes, redirect their I/O and wait for them to complete. Not supported on iOS.

{context class=Subprocess}

`s32 join()`
> Waits for the subprocess to exit and returns its exit code. Does not wait for any descendant processes spawned by the subprocess.

`JoinResult joinWithTimeout(s32 timeoutMillis)`
> Waits for the subprocess to exit with a timeout. A negative timeout value waits indefinitely. Does not wait for any descendant processes that may have been spawned by the subprocess. `JoinResult` has the following members:
>
> | | |
> | --- | --- |
> | `bool joined` | `true` if the subprocess exited; `false` if the timeout was reached. |
> | `s32 exitCode` | The exit code, if the subprocess exited. |

`bool terminate()`
> Forcibly terminates the subprocess. If the subprocess hasn't been joined, and `Options::terminateProcessTree` was enabled when the process was created, then its subprocess tree is also terminated. If `terminate()` is called while another thread is waiting on a join function, the waiting thread is released.
>
> The terminated subprocess closes its standard I/O pipes when it exits. If `terminate()` is called while another thread is waiting inside a `read()` from one of those pipes, the waiting thread is released.

`static Owned<Subprocess> exec(StringView exePath, ArrayView<const StringView> args, StringView initialDir, const Output& output, const Input& input = Input::open(), const Options& options = {})`
> Spawns a new process. `exePath` is the path to the executable. `args` is an array of command-line arguments. `initialDir` is the working directory for the new process.
>
> Pass any of the following values to `output`:
>
> | | |
> | --- | --- |
> | `Output::ignore()` | Discards the output. |
> | `Output::inherit()` | Inherits the parent process's stdout/stderr. |
> | `Output::openSeparate()` | Opens separate pipes to read from stdout/stderr. |
> | `Output::openMerged()` | Opens a single pipe to read from both stdout and stderr. |
> | `Output::openStdOutOnly()` | Opens a pipe to read from stdout and ignores stderr. |
>
> Pass any of the following values to `input`:
>
> | | |
> | --- | --- |
> | `Input::ignore()` | Sends no input. |
> | `Input::inherit()` | Inherits the parent process's stdin. |
> | `Input::open()` | Opens a pipe to write to stdin. |
>
> `Subprocess::Options` has the following members:
>
> | | |
> | --- | --- |
> | `terminateProcessTree` | If `true` and the subprocess spawns its own subprocesses, calling `terminate()` terminates the entire subprocess tree. Default is `false`. |
>
> ```
> // Run a command and capture its output
> Stream outputStream;
> Owned<Subprocess> proc = Subprocess::exec(
>     "/bin/ls", {"-la"},
>     "/home/user",
>     Subprocess::Output::pipe(outputStream)
> );
> String output = outputStream.readRemaining();
> s32 exitCode = proc->join();
> ```
