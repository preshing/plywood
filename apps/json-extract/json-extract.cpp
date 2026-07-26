/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#include <ply-json.h>

using namespace ply;

// This sample app scans a text file for embedded JSON objects and re-emits the objects it finds in a structured form.
// Usage:
//     json-extract <input-file> [-eval <expression>] [output-file]
// When no expression is provided, the app reads the input file in text mode, looks for '{' characters, and attempts
// to parse a JSON object starting at each candidate offset. Successful parses are collected into a JSON array whose
// items report the zero-based match index, the source line and column, and the parsed object itself.
// When -e or -eval is provided, the app still performs the same extraction pass first, then evaluates a simple path
// expression such as a[1].parsed.messages[0].content against the resulting array. If the selected value is a JSON
// string, its raw contents are written without JSON escaping; otherwise the selected JSON value is printed normally.

struct ExprStep {
    enum Type {
        Property,
        Index,
    };

    Type type = Property;
    String name;
    u32 index = 0;
};

// Reads the next non-whitespace token from an expression.
Token readExprToken(Tokenizer& tkr, ViewStream& in) {
    for (;;) {
        Token token = readToken(tkr, in);
        if (token.type == Token::Whitespace || token.type == Token::CStyleComment || token.type == Token::LineComment) {
            continue;
        }
        return token;
    }
}

// Parses an unsigned integer array index token.
bool parseIndexToken(const Token& token, u32* index, String* errorMessage) {
    if (token.type != Token::NumericLiteral) {
        *errorMessage = String::format("Expected array index, got {}", token.toString());
        return false;
    }

    ViewStream in{token.text};
    u64 value = readU64FromText(in);
    if (in.inputError || in.curByte != in.endByte) {
        *errorMessage = String::format("Invalid array index \"{}\"", token.text);
        return false;
    }

    *index = numericCast<u32>(value);
    return true;
}

// Parses an expression like a[1].parsed.messages[0].content.
bool parseExpression(StringView expression, Array<ExprStep>* steps, String* errorMessage) {
    Tokenizer tkr;
    ViewStream in{expression};

    tkr.errorCallback = [errorMessage](u32 inputOffset, String&& message) {
        if (!*errorMessage) {
            *errorMessage = String::format("Expression error at byte {}: {}", inputOffset + 1, message);
        }
    };

    Token token = readExprToken(tkr, in);
    if (token.type != Token::Identifier) {
        *errorMessage = String::format("Expected root identifier, got {}", token.toString());
        return false;
    }

    for (;;) {
        token = readExprToken(tkr, in);
        if (token.type == Token::EOF) {
            return !*errorMessage;
        }

        if (token.type == Token::Dot) {
            Token property = readExprToken(tkr, in);
            if (property.type != Token::Identifier) {
                *errorMessage = String::format("Expected property name after '.', got {}", property.toString());
                return false;
            }
            ExprStep& step = steps->append();
            step.type = ExprStep::Property;
            step.name = property.text;
            continue;
        }

        if (token.type == Token::OpenSquare) {
            Token indexToken = readExprToken(tkr, in);
            u32 index = 0;
            if (!parseIndexToken(indexToken, &index, errorMessage)) {
                return false;
            }

            Token closeSquare = readExprToken(tkr, in);
            if (closeSquare.type != Token::CloseSquare) {
                *errorMessage = String::format("Expected ']' after array index, got {}", closeSquare.toString());
                return false;
            }

            ExprStep& step = steps->append();
            step.type = ExprStep::Index;
            step.index = index;
            continue;
        }

        *errorMessage = String::format("Unexpected token {}", token.toString());
        return false;
    }
}

// Evaluates an expression against the extracted JSON array.
const json::Node* evaluateExpression(const json::Node& root, const Array<ExprStep>& steps, String* errorMessage) {
    const json::Node* node = &root;
    for (u32 i = 0; i < steps.numItems(); i++) {
        const ExprStep& step = steps[i];
        if (step.type == ExprStep::Property) {
            if (!node->isObject()) {
                *errorMessage = String::format("Step {} expected an object before property \"{}\"", i, step.name);
                return nullptr;
            }

            const json::Node& child = node->get(step.name);
            if (!child.isValid()) {
                *errorMessage = String::format("Property \"{}\" was not found", step.name);
                return nullptr;
            }
            node = &child;
        } else {
            if (!node->isArray()) {
                *errorMessage = String::format("Step {} expected an array before index {}", i, step.index);
                return nullptr;
            }
            if (step.index >= node->arrayView().numItems()) {
                *errorMessage = String::format("Array index {} is out of range", step.index);
                return nullptr;
            }
            node = &node->get(step.index);
        }
    }
    return node;
}

// Appends one extracted JSON object to the output array.
void appendMatch(json::Node& results, u32 index, const TokenLocationMap& locMap, u32 fileOfs, json::Node&& parsed) {
    TokenLocation loc = locMap.getLocationFromOffset(fileOfs);

    json::Node item{json::Node::Object{}, fileOfs};
    item.set("index", json::Node{json::Node::Number{double(index)}, fileOfs});
    item.set("line", json::Node{json::Node::Number{double(loc.lineNumber)}, fileOfs});
    item.set("column", json::Node{json::Node::Number{double(loc.columnNumber)}, fileOfs});
    item.set("parsed", std::move(parsed));
    results.array().append(std::move(item));
}

// Scans text for JSON objects and returns the extracted result array.
json::Node extractJsonObjects(StringView srcView) {
    json::Node results{json::Node::Array{}};
    TokenLocationMap locMap = TokenLocationMap::createFromString(srcView);

    u32 scanOfs = 0;
    while (scanOfs < srcView.numBytes()) {
        if (srcView[scanOfs] != '{') {
            scanOfs++;
            continue;
        }

        json::Parser parser;
        parser.setErrorCallback([](const json::ParseError&) {});
        parser.setGreedy(false);
        json::Parser::Result result = parser.parse({}, srcView.substr(scanOfs));
        u32 numBytesConsumed = max(result.numBytes, 1u);
        if (result.root.isValid() && result.root.isObject()) {
            appendMatch(results, results.array().numItems(), locMap, scanOfs, std::move(result.root));
        }
        scanOfs += numBytesConsumed;
    }

    return results;
}

// Writes a node either as raw string content or as JSON.
void writeEvaluatedResult(Stream& out, const json::Node& node) {
    if (node.isText()) {
        out.write(node.text());
    } else {
        json::write(out, node);
        out.write('\n');
    }
}

// Writes the default extracted array output.
void writeResults(Stream& out, const json::Node& results) {
    json::write(out, results);
    out.write('\n');
}

// Prints command-line usage.
void printUsage(Stream out) {
    out.write("usage: json-extract <input-file> [-e <expression>] [output-file]\n");
}

int main(int argc, const char* argv[]) {
#if defined(PLY_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) {
        printUsage(getStdErr());
        return 1;
    }

    StringView inputPath = argv[1];
    StringView expression;
    StringView outputPath;

    if (argc == 3) {
        outputPath = argv[2];
    } else if (argc >= 4) {
        StringView option = argv[2];
        if (option != "-e" && option != "-eval") {
            printUsage(getStdErr());
            return 1;
        }

        expression = argv[3];
        if (argc >= 5) {
            outputPath = argv[4];
        }
        if (argc > 5) {
            printUsage(getStdErr());
            return 1;
        }
    }

    String src = Filesystem::loadTextAutodetect(inputPath);
    if (!src && Filesystem::lastResult() != FS_OK) {
        getStdErr().format("error: unable to read {}\n", inputPath);
        return 1;
    }

    json::Node results = extractJsonObjects(src);

    Stream out = outputPath ? Filesystem::openTextForWrite(outputPath) : getStdOut();
    if (!out) {
        getStdErr().format("error: unable to write {}\n", outputPath);
        return 1;
    }

    if (expression) {
        Array<ExprStep> steps;
        String errorMessage;
        if (!parseExpression(expression, &steps, &errorMessage)) {
            getStdErr().format("error: {}\n", errorMessage);
            return 1;
        }

        String evalError;
        const json::Node* result = evaluateExpression(results, steps, &evalError);
        if (!result) {
            getStdErr().format("error: {}\n", evalError);
            return 1;
        }

        writeEvaluatedResult(out, *result);
    } else {
        writeResults(out, results);
    }

    return 0;
}
