/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-cpp.h>

using namespace ply;
using namespace ply::cpp;

void runParserTests() {
    String testSuitePath = joinPath(CPP_TESTS_PATH, "parser-tests.txt");
    Stream in = Filesystem::openTextForReadAutodetect(testSuitePath);
    MemStream out;
    for (;;) {
        String line;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith(">>"))
                break;
        }
        if (!line)
            break;

        out.write(line);
        MemStream src;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith("--"))
                break;
            src.write(line);
            out.write(line);
        }
        out.write("--\n");

        Owned<Parser> parser = Parser::create();
        ParseResult result = parser->parseFile({}, src.moveToString());
        if (result.diagnostics) {
            for (StringView diag : result.diagnostics) {
                out.write(diag);
            }
        }
        out.write("\n\n");
    }
    in.close();

    Filesystem::saveText(testSuitePath, out.moveToString());
}

void runPreprocessorTests() {
    String testSuitePath = joinPath(CPP_TESTS_PATH, "preprocessor-tests.txt");
    Stream in = Filesystem::openTextForReadAutodetect(testSuitePath);
    MemStream out;
    for (;;) {
        String line;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith(">>"))
                break;
        }
        if (!line)
            break;

        out.write(line);
        MemStream src;
        for (;;) {
            line = readLine(in);
            if (!line)
                break;
            if (line.startsWith("--"))
                break;
            src.write(line);
            out.write(line);
        }
        out.write("--\n");

        Owned<Parser> parser = Parser::create();
        PreprocessResult result = parser->preprocess("<test file>", src.moveToString());
        if (result.diagnostics) {
            for (StringView diag : result.diagnostics) {
                out.write(diag);
            }
        } else {
            out.write(result.output);
        }
        out.write("\n\n");
    }
    in.close();

    Filesystem::saveText(testSuitePath, out.moveToString());
}

void parsePlywoodSource() {
    String srcFolder = joinPath(CPP_TESTS_PATH, "../../src");
    String filePath = joinPath(srcFolder, "ply-base.h");
    String src = Filesystem::loadTextAutodetect(filePath);
    Owned<Parser> parser = Parser::create();
    parser->includePaths.append(srcFolder);
    ParseResult result = parser->parseFile(filePath, src);
    Stream out = getStdOut();
    for (StringView diagnostic : result.diagnostics) {
        out.write(diagnostic);
    }
    out.close();
    for (const Declaration& decl : result.declarations) {
        parser->dumpDeclaration(decl);
    }
}

void parseThisFile() {
    String src = Filesystem::loadTextAutodetect(__FILE__);
    Owned<Parser> parser = Parser::create();
    ParseResult result = parser->parseFile(__FILE__, src);
    for (const Declaration& decl : result.declarations) {
        parser->dumpDeclaration(decl);
    }
}

int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    // runParserTests();
    runPreprocessorTests();
    // parsePlywoodSource();
    // parseThisFile();

    return 0;
}
