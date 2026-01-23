{title text="Processes" include="ply-base.h" namespace="ply"}

{api_summary}
PID get_current_process_id()
String get_current_executable_path()
{/api_summary}

{api_descriptions}
PID get_current_process_id()
--
Returns the operating system's process ID for the current process. See also `get_current_thread_id`.

>>
String get_current_executable_path()
--
Returns the path to the executable file for the current process.
{/api_descriptions}

## `Subprocess`

The `Subprocess` class represents a child process. You can spawn processes, redirect their I/O, and wait for them to complete.

{api_summary class=Subprocess}
s32 join()
static Owned<Subprocess> exec(StringView exe_path, ArrayView<const StringView> args, StringView initial_dir, const Output& output, const Input& input = Input::open())
static Owned<Subprocess> exec_arg_str(StringView exe_path, StringView arg_str, StringView initial_dir, const Output& output, const Input& input = Input::open())
{/api_summary}

{api_descriptions class=Subprocess}
s32 join()
--
Waits for the subprocess to finish and returns its exit code. Must be called before the `Subprocess` object is destroyed.

>>
static Owned<Subprocess> exec(StringView exe_path, ArrayView<const StringView> args, StringView initial_dir, const Output& output, const Input& input = Input::open())
--
Spawns a new process. `exe_path` is the path to the executable. `args` is an array of command-line arguments. `initial_dir` is the working directory for the new process. `output` specifies how to handle stdout/stderr. `input` specifies how to handle stdin.

>>
static Owned<Subprocess> exec_arg_str(StringView exe_path, StringView arg_str, StringView initial_dir, const Output& output, const Input& input = Input::open())
--
Like `exec`, but takes the arguments as a single string that will be parsed into individual arguments.
{/api_descriptions}

The `Output` and `Input` parameters control I/O redirection:

- `Output::ignore()` — Discards the output
- `Output::inherit()` — Uses the parent process's stdout/stderr
- `Output::pipe(Stream&)` — Redirects to a pipe you can read from
- `Input::open()` — Uses the parent process's stdin
- `Input::pipe(Stream&)` — Redirects from a pipe you can write to

{example}
// Run a command and capture its output
Stream output_stream;
Owned<Subprocess> proc = Subprocess::exec(
    "/bin/ls", {"-la"},
    "/home/user",
    Subprocess::Output::pipe(output_stream)
);
String output = output_stream.read_remaining();
s32 exit_code = proc->join();
{/example}
