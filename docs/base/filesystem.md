{title text="The Filesystem" include="ply-base.h" namespace="ply"}

Plywood provides a portable filesystem API that works consistently across Windows and POSIX platforms. All paths arguments are treated as UTF-8 strings.

## `Filesystem`

The `Filesystem` class contains static methods for file and directory operations.

{api_summary class=Filesystem}
    static FSResult last_result()
    static Array<DirectoryEntry> list_dir(StringView path)
    static FSResult make_dir(StringView path)
    static Path_t path_format()
    static String get_working_directory()
    static FSResult set_working_directory(StringView path)
    static ExistsResult exists(StringView path)
    static Owned<Pipe> open_pipe_for_read(StringView path)
    static Owned<Pipe> open_pipe_for_write(StringView path)
    static FSResult move_file(StringView src_path, StringView dst_path)
    static FSResult delete_file(StringView path)
    static FSResult remove_dir_tree(StringView dir_path)
    static DirectoryEntry get_file_info(StringView path)
    static FSResult copy_file(StringView src_path, StringView dst_path)
    static bool is_dir(StringView path)
    static DirectoryWalker walk(StringView top)
    static FSResult make_dirs(StringView path)
    static Stream open_binary_for_read(StringView path)
    static Stream open_binary_for_write(StringView path)
    static Stream open_text_for_read(StringView path, const TextFormat& format = get_default_utf8_format())
    static Stream open_text_for_read_autodetect(StringView path, TextFormat* out_format = nullptr)
    static Stream open_text_for_write(StringView path, const TextFormat& format = get_default_utf8_format())
    static String load_binary(StringView path)
    static String load_text(StringView path, const TextFormat& format)
    static String load_text_autodetect(StringView path, TextFormat* out_format = nullptr)
    static FSResult save_binary(StringView path, StringView contents)
    static FSResult save_text(StringView path, StringView str_contents, const TextFormat& format = get_default_utf8_format())
{/api_summary}

{api_descriptions class=Filesystem}
static FSResult last_result()
--
Returns the result code from the most recent filesystem operation.

>>
static Array<DirectoryEntry> list_dir(StringView path)
--
Returns an array of directory entries for the given path. Each entry contains the name, size, and type of a file or subdirectory.

>>
static FSResult make_dir(StringView path)
--
Creates a single directory. The parent directory must already exist.

>>
static Path_t path_format()
--
Returns the native path format for the current platform (Windows or POSIX).

>>
static String get_working_directory()
--
Returns the current working directory as an absolute path.

>>
static FSResult set_working_directory(StringView path)
--
Changes the current working directory.

>>
static ExistsResult exists(StringView path)
--
Checks if a file or directory exists at the given path.

>>
static Owned<Pipe> open_pipe_for_read(StringView path)
--
Opens a file for reading and returns a pipe. Returns null if the file doesn't exist.

>>
static Owned<Pipe> open_pipe_for_write(StringView path)
--
Opens a file for writing and returns a pipe. Creates the file if it doesn't exist.

>>
static FSResult move_file(StringView src_path, StringView dst_path)
--
Moves or renames a file.

>>
static FSResult delete_file(StringView path)
--
Deletes a file. Does not delete directories.

>>
static FSResult remove_dir_tree(StringView dir_path)
--
Recursively deletes a directory and all its contents.

>>
static DirectoryEntry get_file_info(StringView path)
--
Returns information about a file or directory.

>>
static FSResult copy_file(StringView src_path, StringView dst_path)
--
Copies a file to a new location.

>>
static bool is_dir(StringView path)
--
Returns `true` if the path is a directory.

>>
static DirectoryWalker walk(StringView top)
--
Returns an iterator for recursively walking a directory tree.

>>
static FSResult make_dirs(StringView path)
--
Creates a directory and all necessary parent directories.

>>
static Stream open_binary_for_read(StringView path)
--
Opens a file for binary reading and returns a buffered stream.

>>
static Stream open_binary_for_write(StringView path)
--
Opens a file for binary writing and returns a buffered stream.

>>
static Stream open_text_for_read(StringView path, const TextFormat& format = get_default_utf8_format())
--
Opens a file for text reading with the specified encoding.

>>
static Stream open_text_for_read_autodetect(StringView path, TextFormat* out_format = nullptr)
--
Opens a file for text reading, automatically detecting the encoding from byte-order marks.

>>
static Stream open_text_for_write(StringView path, const TextFormat& format = get_default_utf8_format())
--
Opens a file for text writing with the specified encoding.

>>
static String load_binary(StringView path)
--
Loads an entire file into memory as raw bytes.

>>
static String load_text(StringView path, const TextFormat& format)
--
Loads an entire text file into memory, converting to UTF-8.

>>
static String load_text_autodetect(StringView path, TextFormat* out_format = nullptr)
--
Loads an entire text file, auto-detecting the encoding.

>>
static FSResult save_binary(StringView path, StringView contents)
--
Writes raw bytes to a file, replacing any existing contents.

>>
static FSResult save_text(StringView path, StringView str_contents, const TextFormat& format = get_default_utf8_format())
--
Writes text to a file with the specified encoding.
{/api_descriptions}

### `TextFormat`

`TextFormat` describes the encoding and line-ending style of a text file. Common formats include UTF-8, UTF-16 (big and little endian), and various legacy encodings.

## Manipulating Paths

These functions help you parse and construct file paths in a platform-independent way.

{api_summary class=Path}
char get_path_separator()
StringView get_drive_letter(StringView path)
bool is_absolute_path(StringView path)
bool is_relative_path(StringView path)
String make_absolute_path(StringView path)
String make_relative_path(StringView ancestor, StringView descendant)
SplitPath split_path(StringView path)
SplitExtension split_file_extension(StringView path)
Array<StringView> split_path_full(StringView path)
String join_path(StringViews&&... path_component_args)
{/api_summary}

{api_descriptions class=Path}
char get_path_separator()
--
Returns the native path separator character: `'/'` on POSIX, `'\\'` on Windows.

>>
StringView get_drive_letter(StringView path)
--
Returns the drive letter prefix (e.g., `"C:"`) from a Windows path, or an empty view on POSIX.

>>
bool is_absolute_path(StringView path)
--
Returns `true` if the path is absolute (starts with `/` on POSIX or a drive letter on Windows).

>>
bool is_relative_path(StringView path)
--
Returns `true` if the path is relative.

>>
String make_absolute_path(StringView path)
--
Converts a relative path to an absolute path using the current working directory.

>>
String make_relative_path(StringView ancestor, StringView descendant)
--
Returns `descendant` as a path relative to `ancestor`.

>>
SplitPath split_path(StringView path)
--
Splits a path into directory and filename components.

>>
SplitExtension split_file_extension(StringView path)
--
Splits a filename into base name and extension.

>>
Array<StringView> split_path_full(StringView path)
--
Splits a path into all of its individual components.

>>
String join_path(StringViews&&... path_component_args)
--
Joins path components with the native separator.
{/api_descriptions}

{example}
String full_path = Path::join_path("home", "user", "documents", "file.txt");
// On POSIX: "home/user/documents/file.txt"
// On Windows: "home\\user\\documents\\file.txt"

auto [dir, file] = Path::split_path("/home/user/test.cpp");
// dir is "/home/user", file is "test.cpp"
{/example}

## `DirectoryWatcher`

{api_summary class=DirectoryWatcher}
DirectoryWatcher()
DirectoryWatcher(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback)
void start(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback)
void stop()
{/api_summary}

`DirectoryWatcher` monitors a directory tree for file and directory changes, invoking a callback whenever modifications are detected. This is useful for applications that need to react to file changes in real-time, such as live-reload servers or file synchronization tools.

When `must_recurse` is `false`, the `path` refers to a specific file that changed.

{api_descriptions class=DirectoryWatcher}
DirectoryWatcher()
--
Default constructor. Creates an inactive watcher. Call `start()` to begin watching.

>>
DirectoryWatcher(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback)
--
Constructs and immediately starts watching the directory at `root`.

>>
void start(StringView root, Functor<void(StringView path, bool must_recurse)>&& callback)
--
Begins watching the directory tree rooted at `root`. The `callback` will be invoked from a background thread whenever changes are detected. The watcher must not already be running.

>>
void stop()
--
Stops the background watcher thread and waits for it to finish. After calling `stop()`, you may call `start()` again with a new root and callback. The destructor calls `stop()` automatically.
{/api_descriptions}

{example}
// Watch a directory and log changes
DirectoryWatcher watcher;
watcher.start("/path/to/watch", [](StringView path, bool must_recurse) {
    if (must_recurse) {
        // Too many changes or directory modified; rescan this path
        puts(String::format("Rescan needed: {}", path).bytes);
    } else {
        // Single file changed
        puts(String::format("File changed: {}", path).bytes);
    }
});

// ... do other work ...

// Stop watching (also happens automatically in destructor)
watcher.stop();
{/example}
