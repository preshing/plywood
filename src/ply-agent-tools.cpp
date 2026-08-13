/*─────────────────────────────────────────────────────────┐
│                                                          │
│     ____      Plywood C++ Runtime Library                │
│    ╱   ╱╲     https://plywood.dev/                       │
│   ╱___╱╭╮╲                                               │
│    └──┴┴┴┘    Agent Harness                              │
│               Documentation: /docs/agent-harness.md      │
│                                                          │
└─────────────────────────────────────────────────────────*/

#include "ply-agent.h"

#if !PLY_AGENT_TRANSCRIPT_ONLY

namespace ply {

//--------------------------------------------------
// Permission helpers
//--------------------------------------------------
struct FilteredPath {
    bool ok = false;
    String absPath;
};

bool dirContainsPath(String dir, String path) {
    if (dir == path)
        return true;
    if (path.startsWith(dir) && path[dir.numBytes()] == getPathSeparator())
        return true;
    return false;
}

FilteredPath filterPath(ToolContext* toolCtx, StringView relPath) {
    String absPath = joinPath(toolCtx->workingDirectory, relPath);
    for (const ToolSet::Permission& perm : toolCtx->permissions) {
        if (dirContainsPath(perm.absPath, absPath))
            return {true, std::move(absPath)};
    }
    return {false, {}};
}

//                           ▄▄
//  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//  ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//  ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

void readToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    FilteredPath fp = filterPath(toolCtx, path);
    if (!fp.ok) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Open file.
    Stream in = FileSystem::openTextForReadAutodetect(fp.absPath);
    if (FileSystem::lastResult() != FS_OK) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not read file '{}'.", path));
        return;
    }

    // Set line and size limits.
    u32 lineOffset = 1;
    u32 lineLimit = 2000;
    u32 sizeLimit = 50000;
    const json::Node& offsetArg = arguments.get("offset");
    if (offsetArg.isValid()) {
        lineOffset = (u32) offsetArg.getNumber();
    }
    const json::Node& limitArg = arguments.get("limit");
    if (limitArg.isValid()) {
        lineLimit = (u32) limitArg.getNumber();
    }

    // Collect the desired file range into a single response to minimize mutex overhead.
    MemStream response;
    u32 lineNum = 0;
    u32 linesOutput = 0;
    while (StringView line = readLine(in)) {
        lineNum++;
        if (lineNum < lineOffset)
            continue;
        response.write(line.left(sizeLimit));
        linesOutput++;
        if (linesOutput >= lineLimit)
            break;
        if (line.numBytes() >= sizeLimit)
            break;
        sizeLimit -= line.numBytes();
    }
    String responseText = response.moveToString();
    if (responseText) {
        toolCtx->appendResponse(toolCall, responseText);
    }
}

void addReadTool(ToolSet* toolSet) {
    Owned<ToolSet::Handler> readTool = Heap::create<ToolSet::Handler>();
    readTool->name = "read";
    readTool->description =
        "Read the contents of a file. For text files, output is truncated to 2000 lines or 50KB (whichever is hit "
        "first). Use offset/limit for large files. When you need the full file, continue with offset until "
        "complete.";
    readTool->parameters.append();
    readTool->parameters.back().name = "path";
    readTool->parameters.back().description = "Path to the file to read (relative or absolute)";
    readTool->parameters.back().type = "string";
    readTool->parameters.back().required = true;
    readTool->parameters.append();
    readTool->parameters.back().name = "offset";
    readTool->parameters.back().description = "Line number to start reading from (1-indexed)";
    readTool->parameters.back().type = "number";
    readTool->parameters.append();
    readTool->parameters.back().name = "limit";
    readTool->parameters.back().description = "Maximum number of lines to read";
    readTool->parameters.back().type = "number";
    readTool->handler = readToolHandler;
    toolSet->handlers.insertItem(std::move(readTool));
}

//                  ▄▄  ▄▄
//  ▄▄    ▄▄ ▄▄▄▄▄  ▄▄ ▄██▄▄  ▄▄▄▄
//  ██ ██ ██ ██  ▀▀ ██  ██   ██▄▄██
//   ██▀▀██  ██     ██  ▀█▄▄ ▀█▄▄▄
//

void writeToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Validate content argument.
    const json::Node& contentArg = arguments.get("content");
    if (!contentArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'content' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    FilteredPath fp = filterPath(toolCtx, path);
    if (!fp.ok) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Save file.
    StringView content = contentArg.text();
    FSResult fsResult = FileSystem::saveText(fp.absPath, content);
    if (fsResult == FS_OK) {
        toolCtx->appendResponse(toolCall,
                                String::format("Successfully wrote {} bytes to '{}'.", content.numBytes(), path));
    } else {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not write to '{}'.", path));
    }
}

void addWriteTool(ToolSet* toolSet) {
    Owned<ToolSet::Handler> writeTool = Heap::create<ToolSet::Handler>();
    writeTool->name = "write";
    writeTool->description = "Write content to a file. Creates the file if it doesn't exist, overwrites if it "
                             "does. Automatically creates parent directories.";
    writeTool->parameters.append();
    writeTool->parameters.back().name = "path";
    writeTool->parameters.back().description = "Path to the file to write (relative or absolute)";
    writeTool->parameters.back().type = "string";
    writeTool->parameters.back().required = true;
    writeTool->parameters.append();
    writeTool->parameters.back().name = "content";
    writeTool->parameters.back().description = "Content to write to the file";
    writeTool->parameters.back().type = "string";
    writeTool->parameters.back().required = true;
    writeTool->handler = writeToolHandler;
    toolSet->handlers.insertItem(std::move(writeTool));
}

//  ▄▄▄  ▄▄         ▄▄             ▄▄ ▄▄
//   ██  ▄▄  ▄▄▄▄  ▄██▄▄        ▄▄▄██ ▄▄ ▄▄▄▄▄
//   ██  ██ ▀█▄▄▄   ██         ██  ██ ██ ██  ▀▀
//  ▄██▄ ██  ▄▄▄█▀  ▀█▄▄ ▄▄▄▄▄ ▀█▄▄██ ██ ██
//

void listDirToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    FilteredPath fp = filterPath(toolCtx, path);
    if (!fp.ok) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // List directory.
    Array<DirectoryEntry> entries = FileSystem::listDir(fp.absPath);
    if (FileSystem::lastResult() != FS_OK) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not list '{}'.", path));
        return;
    }

    // Sort alphabetically.
    sort(entries, [](const DirectoryEntry& a, const DirectoryEntry& b) {
        if (a.isDir != b.isDir) {
            return a.isDir > b.isDir; // directories first
        }
        return a.name < b.name;
    });

    // Collect all directory entries into a single response to minimize mutex overhead.
    MemStream response;
    for (const DirectoryEntry& entry : entries) {
        if (entry.isDir) {
            response.format("{}\n", entry.name);
        } else {
            response.format("{} ({} bytes)\n", entry.name, entry.fileSize);
        }
    }
    String responseText = response.moveToString();
    if (responseText) {
        toolCtx->appendResponse(toolCall, responseText);
    }
}

void addListDirTool(ToolSet* toolSet) {
    Owned<ToolSet::Handler> listDirTool = Heap::create<ToolSet::Handler>();
    listDirTool->name = "list_dir";
    listDirTool->description = "List the contents of a directory. Shows files with their size in bytes and "
                               "subdirectories with a trailing '/'.";
    listDirTool->parameters.append();
    listDirTool->parameters.back().name = "path";
    listDirTool->parameters.back().description =
        "Relative or absolute path to the directory to list, inside one of the allowed directory roots";
    listDirTool->parameters.back().type = "string";
    listDirTool->parameters.back().required = true;
    listDirTool->handler = listDirToolHandler;
    toolSet->handlers.insertItem(std::move(listDirTool));
}

//    ▄▄▄ ▄▄            ▄▄       ▄▄                ▄▄▄ ▄▄ ▄▄▄
//   ██   ▄▄ ▄▄▄▄▄   ▄▄▄██       ▄▄ ▄▄▄▄▄         ██   ▄▄  ██   ▄▄▄▄   ▄▄▄▄
//  ▀██▀▀ ██ ██  ██ ██  ██       ██ ██  ██       ▀██▀▀ ██  ██  ██▄▄██ ▀█▄▄▄
//   ██   ██ ██  ██ ▀█▄▄██ ▄▄▄▄▄ ██ ██  ██ ▄▄▄▄▄  ██   ██ ▄██▄ ▀█▄▄▄   ▄▄▄█▀
//

// Simple glob matching: supports * wildcard matching any substring
static bool globMatches(StringView pattern, StringView name) {
    s32 wildCardPos = pattern.find("*");
    if (wildCardPos < 0) {
        // No wildcards. Name must match exactly.
        return (pattern == name);
    } else if (wildCardPos > 0) {
        // There are wildcards, but not at the beginning.
        // Make sure the prefixes match.
        if (!name.startsWith(pattern.left(wildCardPos)))
            return false;
        name = name.substr(wildCardPos);
        // Advanced to the part after the wildcard.
    }

    // We've found the first * in the input pattern and trimmed the prefix from the input name.
    // Loop over the rest of the pattern.
    for (;;) {
        PLY_ASSERT(pattern[wildCardPos] == '*');
        // Advance to the next non-wildcard character in the input pattern.
        do {
            wildCardPos++;
        } while ((numericCast<u32>(wildCardPos) < pattern.numBytes()) && (pattern[wildCardPos] == '*'));
        // Trim the prefix from the input pattern.
        pattern = pattern.substr(wildCardPos);
        // If the pattern is now empty, that means the pattern ended with *,
        // which means that the rest of the input name always matches.
        if (pattern.isEmpty())
            return true;

        // Find next wildcard character.
        wildCardPos = pattern.find("*");
        if (wildCardPos < 0) {
            // No more wildcard characters. Make sure the input name ends with the remainder of the pattern.
            return name.endsWith(pattern);
        } else if (wildCardPos > 0) {
            // Wildcard found. Find the intermediate segment in the input name.
            s32 index = name.find(pattern.left(wildCardPos));
            if (index < 0)
                return false; // Not found

            // Found. Trim the input name to the part after the intermediate segment.
            name = name.substr(index + wildCardPos);
        }
    }
}

struct GitIgnoreContents {
    struct Item {
        bool exclude = true;
        String pattern;
    };

    String absRoot;
    Array<Item> items;
};

// Loads the .gitignore file for the specified directory.
// Returns an empty object if no .gitignore file found.
GitIgnoreContents loadGitIgnoreForDirectory(StringView absDirPath) {
    PLY_ASSERT(isAbsolutePath(absDirPath));

    String gitIgnorePath = joinPath(absDirPath, ".gitignore");
    String text = FileSystem::loadTextAutodetect(gitIgnorePath);
    if (!text)
        return {};

    // Load file contents
    GitIgnoreContents contents;
    contents.absRoot = absDirPath;

    ViewStream stream{StringView{text}};
    for (;;) {
        StringView trimmed = readLine(stream).trim();
        if (stream.atEof)
            break;

        // If line is empty or a comment, continue.
        if (trimmed.isEmpty())
            continue;
        if (trimmed.startsWith("#"))
            continue;

        // Add pattern.
        GitIgnoreContents::Item item;
        if (trimmed.startsWith("!")) {
            item.exclude = false;
            item.pattern = trimmed.substr(1);
        } else {
            item.exclude = true;
            item.pattern = trimmed;
        }
        if (item.pattern) {
            contents.items.append(std::move(item));
        }
    }

    return contents;
}

// Returns an array of .gitignore file contents from all ancestor directories.
Array<GitIgnoreContents> loadAllAncestorGitIgnoreFiles(StringView absDirPath) {
    PLY_ASSERT(isAbsolutePath(absDirPath));
    Array<GitIgnoreContents> result;

    String currentDir = absDirPath;
    for (;;) {
        // Walk up to the parent directory.
        SplitPath sp = splitPath(currentDir);
        if (sp.directory.isEmpty() || sp.directory == currentDir)
            break; // Reached the file system root.
        currentDir = sp.directory;

        GitIgnoreContents contents = loadGitIgnoreForDirectory(currentDir);
        if (contents.items) {
            result.append(std::move(contents));
        }
    }

    return result;
}

bool isIgnored(const GitIgnoreContents& gitIgnore, StringView absPath, bool isDir) {
    String relPath = makeRelativePath(gitIgnore.absRoot, absPath);
    bool ignore = false;
    for (const GitIgnoreContents::Item& item : gitIgnore.items) {
        if (matchGitIgnorePattern(relPath, isDir, item.pattern)) {
            if (item.exclude) {
                ignore = true;
            } else {
                ignore = false;
            }
        }
    }
    return ignore;
}

bool isIgnored(ArrayView<const GitIgnoreContents> ignoreLists, StringView absPath, bool isDir) {
    for (const GitIgnoreContents& gitIgnore : ignoreLists) {
        if (isIgnored(gitIgnore, absPath, isDir))
            return true;
    }
    return false;
}

struct FindInFiles {
    ToolContext* toolCtx = nullptr;
    Transcript::Message* toolCall = nullptr;
    Array<GitIgnoreContents> ignoreLists;
    StringView glob;
    StringView text;
    String root;
};

void findInFiles(FindInFiles& findInfo, StringView absPath, bool isDir) {
    if (isIgnored(findInfo.ignoreLists, absPath, isDir))
        return;

    if (isDir) {
        // Load .gitignore file for this directory.
        bool pushedGitIgnore = false;
        GitIgnoreContents contents = loadGitIgnoreForDirectory(absPath);
        if (!contents.items.isEmpty()) {
            findInfo.ignoreLists.append(std::move(contents));
            pushedGitIgnore = true;
        }

        // Iterate over all directory entries.
        for (const DirectoryEntry& entry : FileSystem::listDir(absPath)) {
            findInFiles(findInfo, joinPath(absPath, entry.name), entry.isDir);
        }

        if (pushedGitIgnore) {
            findInfo.ignoreLists.pop();
        }
    } else {
        if (!globMatches(findInfo.glob, splitPath(absPath).filename))
            return;

        // Check file contents.
        String content = FileSystem::loadTextAutodetect(absPath);
        ViewStream stream{StringView{content}};
        u32 lineNum = 0;
        String relPath = makeRelativePath(findInfo.root, absPath);
        while (true) {
            StringView line = readLine(stream);
            if (line.isEmpty())
                break;
            lineNum++;
            if (line.find(findInfo.text) >= 0) {
                findInfo.toolCtx->appendResponse(
                    findInfo.toolCall, String::format("{}({}):{}\n", relPath, lineNum, line.trimRight()));
            }
        }
    }
}

void findInFilesToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate arguments.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }
    const json::Node& globArg = arguments.get("glob");
    if (!globArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'glob' argument is required.");
        return;
    }
    const json::Node& textArg = arguments.get("text");
    if (!textArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'text' argument is required.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    FilteredPath fp = filterPath(toolCtx, path);
    if (!fp.ok) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Check that the search path exists.
    if (FileSystem::exists(fp.absPath) == ER_NOT_FOUND) {
        toolCtx->appendResponse(toolCall, String::format("Error: Path '{}' does not exist.", path));
        return;
    }

    // Initialize FindInFiles struct.
    FindInFiles findInfo;
    findInfo.toolCtx = toolCtx;
    findInfo.toolCall = toolCall;
    findInfo.ignoreLists = loadAllAncestorGitIgnoreFiles(fp.absPath);
    findInfo.glob = globArg.text();
    findInfo.text = textArg.text();
    findInfo.root = fp.absPath;

    findInFiles(findInfo, fp.absPath, FileSystem::isDir(fp.absPath));
}

void addFindInFilesTool(ToolSet* toolSet) {
    Owned<ToolSet::Handler> findInFilesTool = Heap::create<ToolSet::Handler>();
    findInFilesTool->name = "find_in_files";
    findInFilesTool->description = "Search for text inside files matching a glob pattern in a directory tree. "
                                   "Returns matching lines in 'path(line):content' format. The glob pattern "
                                   "supports '*' as a wildcard matching any substring (case sensitive).";
    findInFilesTool->parameters.append();
    findInFilesTool->parameters.back().name = "path";
    findInFilesTool->parameters.back().description =
        "Starting directory for the search (relative or absolute path inside one of the allowed directory roots)";
    findInFilesTool->parameters.back().type = "string";
    findInFilesTool->parameters.back().required = true;
    findInFilesTool->parameters.append();
    findInFilesTool->parameters.back().name = "glob";
    findInFilesTool->parameters.back().description =
        "Wildcard pattern for filenames. Supports '*' to match any substring (case sensitive)";
    findInFilesTool->parameters.back().type = "string";
    findInFilesTool->parameters.back().required = true;
    findInFilesTool->parameters.append();
    findInFilesTool->parameters.back().name = "text";
    findInFilesTool->parameters.back().description = "The exact text to search for inside each file";
    findInFilesTool->parameters.back().type = "string";
    findInFilesTool->parameters.back().required = true;
    findInFilesTool->handler = findInFilesToolHandler;
    toolSet->handlers.insertItem(std::move(findInFilesTool));
}

//             ▄▄ ▄▄  ▄▄
//   ▄▄▄▄   ▄▄▄██ ▄▄ ▄██▄▄
//  ██▄▄██ ██  ██ ██  ██
//  ▀█▄▄▄  ▀█▄▄██ ██  ▀█▄▄
//

void editToolHandler(ToolContext* toolCtx, Transcript::Message* toolCall, const json::Node& arguments) {
    // Validate path argument.
    const json::Node& pathArg = arguments.get("path");
    if (!pathArg.isText()) {
        toolCtx->appendResponse(toolCall, "Error: 'path' argument is required.");
        return;
    }

    // Validate edits argument.
    const json::Node& editsArg = arguments.get("edits");
    if (!editsArg.isArray()) {
        toolCtx->appendResponse(toolCall, "Error: 'edits' argument is required and must be an array.");
        return;
    }

    // Check permissions.
    StringView path = pathArg.text();
    FilteredPath fp = filterPath(toolCtx, path);
    if (!fp.ok) {
        toolCtx->appendResponse(toolCall, "Error: Permission denied.");
        return;
    }

    // Load file contents.
    String text = FileSystem::loadTextAutodetect(fp.absPath);
    if (FileSystem::lastResult() != FS_OK) {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not read file '{}'.", path));
        return;
    }

    // Collect all edit positions against the original text.
    struct EditPos {
        s32 start;
        s32 end;
        String newText;
    };
    Array<EditPos> editPositions;

    for (const json::Node& jEdit : editsArg.arrayView()) {
        if (!jEdit.isObject()) {
            toolCtx->appendResponse(toolCall, "Error: Each edit must be an object with 'oldText' and 'newText'.");
            return;
        }
        const json::Node& jOldText = jEdit.get("oldText");
        const json::Node& jNewText = jEdit.get("newText");
        if (!jOldText.isText() || !jNewText.isText()) {
            toolCtx->appendResponse(toolCall,
                                    "Error: Each edit must have 'oldText' (string) and 'newText' (string).");
            return;
        }

        StringView oldText = jOldText.text();
        StringView newText = jNewText.text();

        // Find position in original text.
        s32 pos = text.find(oldText);
        if (pos < 0) {
            toolCtx->appendResponse(toolCall,
                                    String::format("Error: Could not find '{}' in '{}'.", oldText, path));
            return;
        }

        // Check uniqueness.
        s32 secondPos = text.find(oldText, pos + oldText.numBytes());
        if (secondPos >= 0) {
            toolCtx->appendResponse(
                toolCall, String::format("Error: '{}' appears multiple times in '{}'. Use a more unique oldText.",
                                         oldText, path));
            return;
        }

        // Check for overlap with already-scheduled edits.
        for (const EditPos& ep : editPositions) {
            if (pos < ep.end && pos + (s32) oldText.numBytes() > ep.start) {
                toolCtx->appendResponse(toolCall,
                                        String::format("Error: Edit for '{}' overlaps with another edit.", oldText));
                return;
            }
        }

        editPositions.append({pos, pos + (s32) oldText.numBytes(), String{newText}});
    }

    // Sort edits by position descending so replacements don't invalidate earlier positions.
    sort(editPositions, [](const EditPos& a, const EditPos& b) { return a.start > b.start; });

    // Apply edits.
    String mutableText = std::move(text);
    for (const EditPos& ep : editPositions) {
        mutableText = mutableText.left(ep.start) + ep.newText + mutableText.substr(ep.end);
    }

    // Save file.
    FSResult fsResult = FileSystem::saveText(fp.absPath, mutableText);
    if (fsResult == FS_OK) {
        toolCtx->appendResponse(
            toolCall,
            String::format("Successfully edited '{}' with {} replacement(s).", path, editPositions.numItems()));
    } else {
        toolCtx->appendResponse(toolCall, String::format("Error: Could not write to '{}'.", path));
    }
}

void addEditTool(ToolSet* toolSet) {
    Owned<ToolSet::Handler> editTool = Heap::create<ToolSet::Handler>();
    editTool->name = "edit";
    editTool->description = "Edit a single file using exact text replacement. Every edits[].oldText must match a "
                            "unique, non-overlapping region of the original file. If two changes affect the same "
                            "block or nearby lines, merge them into one edit instead of emitting overlapping "
                            "edits. Do not include large unchanged regions just to connect distant changes.";
    editTool->parameters.append();
    editTool->parameters.back().name = "path";
    editTool->parameters.back().description = "Path to the file to edit (relative or absolute)";
    editTool->parameters.back().type = "string";
    editTool->parameters.back().required = true;
    editTool->parameters.append();
    editTool->parameters.back().name = "edits";
    editTool->parameters.back().description =
        "One or more targeted replacements. Each edit is matched against the original file, not incrementally. "
        "Do not include overlapping or nested edits. If two changes touch the same block or nearby lines, merge "
        "them into one edit instead.";
    editTool->parameters.back().type = "array";
    editTool->parameters.back().required = true;
    editTool->handler = editToolHandler;
    toolSet->handlers.insertItem(std::move(editTool));
}

} // namespace ply

#endif // !PLY_AGENT_TRANSCRIPT_ONLY
