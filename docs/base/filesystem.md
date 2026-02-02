{title text="The Filesystem" include="ply-base.h" namespace="ply"}

Plywood provides a portable filesystem API that works consistently across Windows and POSIX platforms. All paths arguments are treated as UTF-8 strings.

## `Filesystem`

The `Filesystem` class contains static methods for file and directory operations.

{apiSummary class=Filesystem}
    static FSResult lastResult()
    static Array<DirectoryEntry> listDir(StringView path)
    static FSResult makeDir(StringView path)
    static Path_t pathFormat()
    static String getWorkingDirectory()
    static FSResult setWorkingDirectory(StringView path)
    static ExistsResult exists(StringView path)
    static Owned<Pipe> openPipeForRead(StringView path)
    static Owned<Pipe> openPipeForWrite(StringView path)
    static FSResult moveFile(StringView srcPath, StringView dstPath)
    static FSResult deleteFile(StringView path)
    static FSResult removeDirTree(StringView dirPath)
    static DirectoryEntry getFileInfo(StringView path)
    static FSResult copyFile(StringView srcPath, StringView dstPath)
    static bool isDir(StringView path)
    static DirectoryWalker walk(StringView top)
    static FSResult makeDirs(StringView path)
    static Stream openBinaryForRead(StringView path)
    static Stream openBinaryForWrite(StringView path)
    static Stream openTextForRead(StringView path, const TextFormat& format = get_default_utf8_format())
    static Stream openTextForReadAutodetect(StringView path, TextFormat* outFormat = nullptr)
    static Stream openTextForWrite(StringView path, const TextFormat& format = get_default_utf8_format())
    static String loadBinary(StringView path)
    static String loadText(StringView path, const TextFormat& format)
    static String loadTextAutodetect(StringView path, TextFormat* outFormat = nullptr)
    static FSResult saveBinary(StringView path, StringView contents)
    static FSResult saveText(StringView path, StringView strContents, const TextFormat& format = get_default_utf8_format())
{/apiSummary}

{apiDescriptions class=Filesystem}
static FSResult lastResult()
--
Returns the result code from the most recent filesystem operation.

>>
static Array<DirectoryEntry> listDir(StringView path)
--
Returns an array of directory entries for the given path. Each entry contains the name, size, and type of a file or subdirectory.

>>
static FSResult makeDir(StringView path)
--
Creates a single directory. The parent directory must already exist.

>>
static Path_t pathFormat()
--
Returns the native path format for the current platform (Windows or POSIX).

>>
static String getWorkingDirectory()
--
Returns the current working directory as an absolute path.

>>
static FSResult setWorkingDirectory(StringView path)
--
Changes the current working directory.

>>
static ExistsResult exists(StringView path)
--
Checks if a file or directory exists at the given path.

>>
static Owned<Pipe> openPipeForRead(StringView path)
--
Opens a file for reading and returns a pipe. Returns null if the file doesn't exist.

>>
static Owned<Pipe> openPipeForWrite(StringView path)
--
Opens a file for writing and returns a pipe. Creates the file if it doesn't exist.

>>
static FSResult moveFile(StringView srcPath, StringView dstPath)
--
Moves or renames a file.

>>
static FSResult deleteFile(StringView path)
--
Deletes a file. Does not delete directories.

>>
static FSResult removeDirTree(StringView dirPath)
--
Recursively deletes a directory and all its contents.

>>
static DirectoryEntry getFileInfo(StringView path)
--
Returns information about a file or directory.

>>
static FSResult copyFile(StringView srcPath, StringView dstPath)
--
Copies a file to a new location.

>>
static bool isDir(StringView path)
--
Returns `true` if the path is a directory.

>>
static DirectoryWalker walk(StringView top)
--
Returns an iterator for recursively walking a directory tree.

>>
static FSResult makeDirs(StringView path)
--
Creates a directory and all necessary parent directories.

>>
static Stream openBinaryForRead(StringView path)
--
Opens a file for binary reading and returns a buffered stream.

>>
static Stream openBinaryForWrite(StringView path)
--
Opens a file for binary writing and returns a buffered stream.

>>
static Stream openTextForRead(StringView path, const TextFormat& format = get_default_utf8_format())
--
Opens a file for text reading with the specified encoding.

>>
static Stream openTextForReadAutodetect(StringView path, TextFormat* outFormat = nullptr)
--
Opens a file for text reading, automatically detecting the encoding from byte-order marks.

>>
static Stream openTextForWrite(StringView path, const TextFormat& format = get_default_utf8_format())
--
Opens a file for text writing with the specified encoding.

>>
static String loadBinary(StringView path)
--
Loads an entire file into memory as raw bytes.

>>
static String loadText(StringView path, const TextFormat& format)
--
Loads an entire text file into memory, converting to UTF-8.

>>
static String loadTextAutodetect(StringView path, TextFormat* outFormat = nullptr)
--
Loads an entire text file, auto-detecting the encoding.

>>
static FSResult saveBinary(StringView path, StringView contents)
--
Writes raw bytes to a file, replacing any existing contents.

>>
static FSResult saveText(StringView path, StringView strContents, const TextFormat& format = get_default_utf8_format())
--
Writes text to a file with the specified encoding.
{/apiDescriptions}

### `TextFormat`

`TextFormat` describes the encoding and line-ending style of a text file. Common formats include UTF-8, UTF-16 (big and little endian), and various legacy encodings.

## Manipulating Paths

These functions help you parse and construct file paths in a platform-independent way.

{apiSummary class=Path}
char getPathSeparator()
StringView getDriveLetter(StringView path)
bool isAbsolutePath(StringView path)
bool isRelativePath(StringView path)
String makeAbsolutePath(StringView path)
String makeRelativePath(StringView ancestor, StringView descendant)
SplitPath splitPath(StringView path)
SplitExtension splitFileExtension(StringView path)
Array<StringView> splitPathFull(StringView path)
String joinPath(StringViews&&... pathComponentArgs)
{/apiSummary}

{apiDescriptions class=Path}
char getPathSeparator()
--
Returns the native path separator character: `'/'` on POSIX, `'\\'` on Windows.

>>
StringView getDriveLetter(StringView path)
--
Returns the drive letter prefix (e.g., `"C:"`) from a Windows path, or an empty view on POSIX.

>>
bool isAbsolutePath(StringView path)
--
Returns `true` if the path is absolute (starts with `/` on POSIX or a drive letter on Windows).

>>
bool isRelativePath(StringView path)
--
Returns `true` if the path is relative.

>>
String makeAbsolutePath(StringView path)
--
Converts a relative path to an absolute path using the current working directory.

>>
String makeRelativePath(StringView ancestor, StringView descendant)
--
Returns `descendant` as a path relative to `ancestor`.

>>
SplitPath splitPath(StringView path)
--
Splits a path into directory and filename components.

>>
SplitExtension splitFileExtension(StringView path)
--
Splits a filename into base name and extension.

>>
Array<StringView> splitPathFull(StringView path)
--
Splits a path into all of its individual components.

>>
String joinPath(StringViews&&... pathComponentArgs)
--
Joins path components with the native separator.
{/apiDescriptions}

{example}
String fullPath = Path::joinPath("home", "user", "documents", "file.txt");
// On POSIX: "home/user/documents/file.txt"
// On Windows: "home\\user\\documents\\file.txt"

auto [dir, file] = Path::splitPath("/home/user/test.cpp");
// dir is "/home/user", file is "test.cpp"
{/example}

## `DirectoryWatcher`

{apiSummary class=DirectoryWatcher}
DirectoryWatcher()
DirectoryWatcher(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback)
void start(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback)
void stop()
{/apiSummary}

`DirectoryWatcher` monitors a directory tree for file and directory changes, invoking a callback whenever modifications are detected. This is useful for applications that need to react to file changes in real-time, such as live-reload servers or file synchronization tools.

When `mustRecurse` is `false`, the `path` refers to a specific file that changed.

`DirectoryWatcher` is only supported on Windows and macOS and is not enabled by default. To enable it, define `PLY_WITH_DIRECTORY_WATCHER` in your project settings. If enabled on macOS, you must also link with the CoreServices framework.

{apiDescriptions class=DirectoryWatcher}
DirectoryWatcher()
--
Default constructor. Creates an inactive watcher. Call `start()` to begin watching.

>>
DirectoryWatcher(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback)
--
Constructs and immediately starts watching the directory at `root`.

>>
void start(StringView root, Functor<void(StringView path, bool mustRecurse)>&& callback)
--
Begins watching the directory tree rooted at `root`. The `callback` will be invoked from a background thread whenever changes are detected. The watcher must not already be running.

>>
void stop()
--
Stops the background watcher thread and waits for it to finish. After calling `stop()`, you may call `start()` again with a new root and callback. The destructor calls `stop()` automatically.
{/apiDescriptions}

{example}
// Watch a directory and log changes
DirectoryWatcher watcher;
watcher.start("/path/to/watch", [](StringView path, bool mustRecurse) {
    if (mustRecurse) {
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
