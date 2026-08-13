Processes (`ply-system.h`)
========================

`PID getCurrentProcessId()`
> Returns the operating system's process ID for the current process. See also `getCurrentThreadId`.

`String getCurrentExecutablePath()`
> Returns the path to the executable file for the current process.

## `Subprocess`

The `Subprocess` class represents a child process. You can spawn processes, redirect their I/O, and wait for them to complete.

{context class=Subprocess}

`s32 join()`
> Waits for the subprocess to finish and returns its exit code. Must be called before the `Subprocess` object is destroyed.

`static Owned<Subprocess> exec(StringView exePath, ArrayView<const StringView> args, StringView initialDir, const Output& output, const Input& input = Input::open())`
> Spawns a new process. `exePath` is the path to the executable. `args` is an array of command-line arguments. `initialDir` is the working directory for the new process. `output` specifies how to handle stdout/stderr. `input` specifies how to handle stdin.

`static Owned<Subprocess> execArgStr(StringView exePath, StringView argStr, StringView initialDir, const Output& output, const Input& input = Input::open())`
> Like `exec`, but takes the arguments as a single string that will be parsed into individual arguments.

The `Output` and `Input` parameters control I/O redirection:

- `Output::ignore()` — Discards the output
- `Output::inherit()` — Uses the parent process's stdout/stderr
- `Output::pipe(Stream&)` — Redirects to a pipe you can read from
- `Input::open()` — Uses the parent process's stdin
- `Input::pipe(Stream&)` — Redirects from a pipe you can write to

```
// Run a command and capture its output
Stream outputStream;
Owned<Subprocess> proc = Subprocess::exec(
    "/bin/ls", {"-la"},
    "/home/user",
    Subprocess::Output::pipe(outputStream)
);
String output = outputStream.readRemaining();
s32 exitCode = proc->join();
```
