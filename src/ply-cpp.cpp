/*───────────────────────────────────────────────────────────────┐
│                                                                │
│     ____      Plywood C++ Runtime Library                      │
│    ╱   ╱╲     https://plywood.dev/                             │
│   ╱___╱╭╮╲                                                     │
│    └──┴┴┴┘    C++ Parser                                       │
│               Documentation: /docs/high-level/cpp-parser.md    │
│                                                                │
└───────────────────────────────────────────────────────────────*/

#include "ply-cpp.h"

namespace ply {
namespace cpp {

//  ▄▄▄▄▄
//  ██  ██ ▄▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀▀  ██  ▀▀ ██▄▄██ ██  ██ ██  ▀▀ ██  ██ ██    ██▄▄██ ▀█▄▄▄  ▀█▄▄▄  ██  ██ ██  ▀▀
//  ██     ██     ▀█▄▄▄  ██▄▄█▀ ██     ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀ ▀█▄▄█▀ ██
//                       ██

struct Preprocessor {
    // All the files opened by the preprocessor (eg. through #include directives).
    struct File {
        String absPath;
        StringView contents;
        String contentsStorage;
        TokenLocationMap tokenLocMap;
    };
    Array<File> files;

    // This B-tree lets us map any token to the chain of includes & macros that it came from.
    struct InputRange {
        // For each InputRange entry whose fileOffset is 0, the location of the enclosing include directive or macro
        // invocation can be found by looking at the preceding InputRange in the BTree and calculating the fileOffset
        // at the end of that range.
        // parentStartOffset tells us the input offset at the *start* of the enclosing file or macro expansion. There
        // should be an InputRange entry at this offset whose fileOffset is 0 and whose fileOrMacroIndex matches the
        // InputRange entry preceding this one.
        u32 inputOffset = 0;
        u32 isMacroExpansion : 1;
        u32 fileOrMacroIndex : 32;
        u32 fileOffset = 0;
        s32 parentRangeIndex = -1;

        InputRange() : isMacroExpansion{0}, fileOrMacroIndex{0} {
        }
        u32 getLookupKey() const {
            return this->inputOffset;
        }
    };
    Array<InputRange> inputRanges;

    // The current include stack. Macro expansions are also pushed here when they are parsed.
    struct IncludedItem {
        u32 inputRangeIndex = 0; // InputRange lookup key of enclosing directive or macro invocation.
        ViewStream vin;
    };
    Array<IncludedItem> includeStack;

    // All the preprocessor definitions that have been defined.
    struct Macro {
        StringView name;
        Map<StringView, u32> args;
        StringView expansion;
        u32 expansionInputOffset = u32(-1); // -1 means predefined.
        bool takesArguments = false;

        StringView getLookupKey() const {
            return this->name;
        }
    };
    Array<Macro> macros; // No item is ever erased, only replaced with a later item.
    Map<StringView, u32> macroMap;

    // This array holds string storage for:
    // - tokens joined by ## token pasting
    // - tokens joined  by \ line continuation
    Array<String> joinedTokenStorage;

    // Flags that influence tokenizer behavior.
    bool atStartOfLine = true;

    // This member is only valid when a token type of Macro is returned.
    // It'll remain valid until the next call to readToken.
    Array<Token> macroArgs;
};

//  ▄▄▄▄▄                                     ▄▄▄▄                 ▄▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄   ██  ▄▄▄▄▄▄▄  ▄▄▄▄▄   ██
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██▄▄██ ██  ▀▀  ██  ██ ██ ██ ██  ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ▀█▄▄▄  ██     ▄██▄ ██ ██ ██ ██▄▄█▀ ▄██▄
//                                                          ██

enum ErrorType {
    Error,
    Warning,
    Note,
};

struct ParserImpl : Parser {
    Tokenizer tkr;
    Preprocessor pp;
    Array<String> diagnostics;
    bool isOnlyPreprocessing = false;
    bool success = true;

    // Backtracking and pushback
    struct PackedToken {
        Token::Type type = Token::EOF;
        u32 inputOffset = 0;
    };
    static constexpr u32 NumTokensPerPage = 2048;
    Array<FixedArray<PackedToken, NumTokensPerPage + 1>> tokens;
    u32 tokenIndex = 0;
    u32 numTokens = 0;
    bool restorePointEnabled = false;

    // Status
    u32 passNumber = 1;

    //---------------------------
    // Error recovery
    static constexpr u32 AcceptOpenCurly = 0x1;
    static constexpr u32 AcceptCloseCurly = 0x2;
    static constexpr u32 AcceptCloseParen = 0x4;
    static constexpr u32 AcceptCloseSquare = 0x8;
    static constexpr u32 AcceptCloseAngle = 0x10;
    static constexpr u32 AcceptComma = 0x20;
    static constexpr u32 AcceptSemicolon = 0x40;

    u32 rawErrorCount = 0; // Increments even when errors are muted.
    bool muteErrors = false;
    u32 outerAcceptFlags = 0;

    //---------------------------

    ParserImpl();
    void errorNoMute(ErrorType type, u32 inputOffset, StringView message);
    void error(ErrorType type, u32 inputOffset, StringView message);
};

struct RestorePoint {
    ParserImpl* parser = nullptr;
    bool wasPreviouslyEnabled = false;
    u32 savedTokenIndex = 0;
    u32 savedErrorCount = 0;

    RestorePoint(ParserImpl* parser) : parser{parser} {
        // Restore points can be nested. For example, when parsing the parameters of the
        // ply::Initializer constructor, there is a restore point when the constructor is
        // optimistically parsed, and another restore point after 'void' when we optimistically try
        // to parse a parameter list:
        //      struct Initializer {
        //          Initializer(void (*init)()) {
        //          ^                ^
        //          |                `---- second restore point
        //          `---- first restore point
        this->wasPreviouslyEnabled = parser->restorePointEnabled;
        parser->restorePointEnabled = true;
        this->savedTokenIndex = parser->tokenIndex;
        this->savedErrorCount = parser->rawErrorCount;
    }
    ~RestorePoint() {
        if (this->parser) {
            this->cancel();
        }
    }
    bool errorOccurred() const {
        return this->parser->rawErrorCount != this->savedErrorCount;
    }
    void backtrack() {
        PLY_ASSERT(this->parser); // Must not have been canceled
        this->parser->tokenIndex = this->savedTokenIndex;
        this->parser->rawErrorCount = this->savedErrorCount;
    }
    void cancel() {
        PLY_ASSERT(this->parser);           // Must not have been canceled
        PLY_ASSERT(!this->errorOccurred()); // no errors occurred
        this->parser->restorePointEnabled = this->wasPreviouslyEnabled;
        this->parser = nullptr;
    }
};

//---------------------------------------------------------------------------
// Error handling
//---------------------------------------------------------------------------

FileLocation getFileLocation(const Preprocessor* pp, u32 inputOffset) {
    s32 inputRangeIndex = binarySearch(pp->inputRanges, inputOffset, FindType::GreaterThan) - 1;
    PLY_ASSERT(inputRangeIndex >= 0);
    const Preprocessor::InputRange* inputRange = &pp->inputRanges[inputRangeIndex];
    while (inputRange->isMacroExpansion) {
        inputRangeIndex = inputRange->parentRangeIndex;
        PLY_ASSERT(inputRangeIndex >= 0);
        PLY_ASSERT(inputRangeIndex + 1 < numericCast<s32>(pp->inputRanges.numItems()));
        PLY_ASSERT(pp->inputRanges[inputRangeIndex + 1].parentRangeIndex == inputRangeIndex);
        inputRange = &pp->inputRanges[inputRangeIndex];
        inputOffset = pp->inputRanges[inputRangeIndex + 1].inputOffset;
    }
    const Preprocessor::File* file = &pp->files[inputRange->fileOrMacroIndex];
    TokenLocation tokenLoc = file->tokenLocMap.getLocationFromOffset(
        numericCast<u32>(inputOffset - inputRange->inputOffset + inputRange->fileOffset));
    return {file->absPath, tokenLoc.lineNumber, tokenLoc.columnNumber};
}

String getFileLocationString(const Preprocessor* pp, u32 inputOffset) {
    FileLocation fileLocation = getFileLocation(pp, inputOffset);
    return String::format("{}({}, {})", fileLocation.absPath, fileLocation.line, fileLocation.column);
}

ParserImpl::ParserImpl() {
    this->tkr.config.tokenizePreprocessorDirectives = true;
    this->tkr.errorCallback = [this](u32 inputOffset, String&& message) {
        // Tokenizer errors don't affect the raw error count.
        this->diagnostics.append(
            String::format("{}: error: {}\n", getFileLocationString(&this->pp, inputOffset), message));
        this->success = false;
    };
}

void ParserImpl::errorNoMute(ErrorType type, u32 inputOffset, StringView message) {
    if (type == Error) {
        this->rawErrorCount++;
    }
    if (!this->restorePointEnabled && !this->muteErrors) {
        StringView typeStr = "error";
        if (type == Warning) {
            typeStr = "warning";
        } else if (type == Note) {
            typeStr = "note";
        }
        this->diagnostics.append(
            String::format("{}: {}: {}\n", getFileLocationString(&this->pp, inputOffset), typeStr, message));
        if (type == Error) {
            this->success = false;
        }
    }
}

inline void ParserImpl::error(ErrorType type, u32 inputOffset, StringView message) {
    this->errorNoMute(type, inputOffset, message);
    this->muteErrors = true;
}

#define FMT_MSG(...) ((!parser->restorePointEnabled && !parser->muteErrors) ? String::format(__VA_ARGS__) : String{})

//---------------------------------------------------------
// Helpers
//---------------------------------------------------------
StringView getTextAtOffset(const Preprocessor* pp, u32 inputOffset, u32 numBytes) {
    s32 inputRangeIndex = binarySearch(pp->inputRanges, inputOffset, FindType::GreaterThan) - 1;
    PLY_ASSERT(inputRangeIndex >= 0);
    const Preprocessor::InputRange* inputRange = &pp->inputRanges[inputRangeIndex];
    if (inputRange->isMacroExpansion) {
        const Preprocessor::Macro& macro = pp->macros[inputRange->fileOrMacroIndex];
        return macro.expansion.substr(inputOffset - inputRange->inputOffset + inputRange->fileOffset, numBytes);
    } else {
        const Preprocessor::File& file = pp->files[inputRange->fileOrMacroIndex];
        return file.contents.substr(inputOffset - inputRange->inputOffset + inputRange->fileOffset, numBytes);
    }
}

void includeFile(ParserImpl* parser, StringView filename, u32 inputOffset) {
    for (StringView includePath : parser->includePaths) {
        String fullPath = joinPath(includePath, filename);
        if (FileSystem::exists(fullPath) == ExistsResult::File) {
            u32 fileIndex = parser->pp.files.numItems();
            Preprocessor::File& file = parser->pp.files.append();
            file.absPath = fullPath;
            file.contentsStorage = FileSystem::loadTextAutodetect(fullPath);
            file.contents = file.contentsStorage;
            file.tokenLocMap = TokenLocationMap::createFromString(file.contents);

            // Add to the include stack.
            Preprocessor::IncludedItem& item = parser->pp.includeStack.append();
            item.inputRangeIndex = parser->pp.inputRanges.numItems();
            item.vin = ViewStream{file.contents};

            // Begin a new range of input.
            Preprocessor::InputRange& newInputRange = parser->pp.inputRanges.append();
            newInputRange.inputOffset = inputOffset;
            newInputRange.isMacroExpansion = 0;
            newInputRange.fileOrMacroIndex = fileIndex;
            newInputRange.parentRangeIndex = parser->pp.includeStack.back(-2).inputRangeIndex;
        }
    }
}

void handlePreprocessorDirective(ParserImpl* parser, StringView directive, u32 inputOffset) {
    ViewStream in{directive};
    StringView cmd = readIdentifier(in);
    if (cmd == "include") {
        skipWhitespace(in);
        StringView rest = readLine(in);
        // FIXME: Do proper parsing of < > vs " "
        includeFile(parser, rest.substr(1, rest.numBytes() - 2), inputOffset);
    } else if (cmd == "define") {
        // Parse macro name.
        skipWhitespace(in);
        StringView name = readIdentifier(in);
        if (name.numBytes() > 0) {
            // Parse macro expansion (may be empty).
            StringView expansion = in.viewRemainingBytes().trim();

            // Append new macro; don't erase old ones because existing InputRanges may still
            // reference them. Instead, update macroMap to point to the newest definition.
            u32 macroIdx = parser->pp.macros.numItems();
            Preprocessor::Macro& macro = parser->pp.macros.append();
            macro.name = name;
            macro.expansion = expansion;
            *parser->pp.macroMap.insert(name).value = macroIdx;
        }
    }
}

Token peekToken(ParserImpl* parser) {
    Token token;
    for (;;) {
        if (parser->tokenIndex >= parser->numTokens) {
            token = readToken(parser->tkr, parser->pp.includeStack.back().vin);
            if (token.type == Token::Identifier) {
                if (u32* macroIdx = parser->pp.macroMap.find(token.text)) {
                    // A preprocessor definition was found.
                    const Preprocessor::Macro& macro = parser->pp.macros[*macroIdx];

                    // We don't want the macro invocation itself to contribute to the logical input
                    // stream length. Rewind the tokenizer's logical offset so that the macro
                    // expansion logically starts at the beginning of the invocation token.
                    parser->tkr.inputOffset = token.inputOffset;

                    // Add to the include stack, which actually contains both includes and macros.
                    Preprocessor::IncludedItem& top = parser->pp.includeStack.append();
                    top.inputRangeIndex = parser->pp.inputRanges.numItems();
                    top.vin = ViewStream{macro.expansion};

                    // Begin a new range of input.
                    Preprocessor::InputRange& newInputRange = parser->pp.inputRanges.append();
                    // The macro expansion occupies the same logical position as the invocation,
                    // so its InputRange starts at the invocation's input offset.
                    newInputRange.inputOffset = token.inputOffset;
                    newInputRange.isMacroExpansion = 1;
                    newInputRange.fileOrMacroIndex = *macroIdx;
                    newInputRange.parentRangeIndex = parser->pp.includeStack.back(-2).inputRangeIndex;

                    // Macro invocations are *not* added to the parser's token list.
                    continue;
                }
            } else if (token.type == Token::EOF) {
                if (parser->pp.includeStack.numItems() > 1) {
                    // The last item in the include stack should correspond to the last input range.
                    PLY_ASSERT(parser->pp.includeStack.back().inputRangeIndex == parser->pp.inputRanges.numItems() - 1);

                    // Begin a new input range for the remainder of the parent file or macro.
                    Preprocessor::InputRange& newInputRange = parser->pp.inputRanges.append();

                    // Sanity check the input offset of the EOF token.
                    const Preprocessor::InputRange& endingInputRange = parser->pp.inputRanges.back(-2);
                    PLY_ASSERT(endingInputRange.inputOffset +
                                   (parser->pp.includeStack.back().vin.getSeekPos() - endingInputRange.fileOffset) ==
                               token.inputOffset);

                    // Get the file offset where we are resuming the parent file or macro.
                    PLY_ASSERT(endingInputRange.parentRangeIndex ==
                               numericCast<s32>(parser->pp.includeStack.back(-2).inputRangeIndex));
                    const Preprocessor::InputRange* oldParentRange =
                        &parser->pp.inputRanges[endingInputRange.parentRangeIndex];
                    u32 oldParentRangeLength = oldParentRange[1].inputOffset - oldParentRange[0].inputOffset;
                    u32 parentFileSeek = numericCast<u32>(parser->pp.includeStack.back(-2).vin.getSeekPos());
                    // For includes (not macro expansions), the logical length of the parent
                    // segment should exactly match how far we've advanced in the parent file.
                    if (!endingInputRange.isMacroExpansion) {
                        PLY_ASSERT(oldParentRange->fileOffset + oldParentRangeLength == parentFileSeek);
                    }

                    // Fill in the new input range.
                    newInputRange.inputOffset = token.inputOffset;
                    newInputRange.isMacroExpansion = oldParentRange->isMacroExpansion;
                    newInputRange.fileOrMacroIndex = oldParentRange->fileOrMacroIndex;
                    // Resume the parent at its current file (or macro) position.
                    newInputRange.fileOffset = parentFileSeek;
                    newInputRange.parentRangeIndex = oldParentRange->parentRangeIndex;

                    // Pop the last item from the include stack.
                    parser->pp.includeStack.pop();
                    parser->pp.includeStack.back().inputRangeIndex = parser->pp.inputRanges.numItems() - 1;
                }
            }

            // Add this token to the parser's token list. Preprocessor directives, comments and whitespace are added to
            // the token list, but not returned to the parser.
            u32 pageIndex = parser->tokenIndex / ParserImpl::NumTokensPerPage;
            if (pageIndex >= parser->tokens.numItems()) {
                parser->tokens.append();
            }
            ParserImpl::PackedToken* packed =
                &parser->tokens[pageIndex][parser->tokenIndex - pageIndex * ParserImpl::NumTokensPerPage];
            packed[0].type = token.type;
            packed[0].inputOffset = token.inputOffset;
            packed[1].inputOffset = token.inputOffset + token.text.numBytes();
            parser->numTokens++;

            // If it's a preprocessor directive, handle it.
            if (token.type == Token::PreprocessorDirective) {
                handlePreprocessorDirective(parser, token.text.substr(1).trim(),
                                            token.inputOffset + token.text.numBytes());
                // The directive may modify the include stack, so restart the loop to read the next token.
                parser->tokenIndex++;
                continue;
            }
        } else {
            u32 pageIndex = parser->tokenIndex / ParserImpl::NumTokensPerPage;
            u32 indexInPage = parser->tokenIndex - pageIndex * ParserImpl::NumTokensPerPage;
            ParserImpl::PackedToken* packed = &parser->tokens[pageIndex][indexInPage];
            token.type = packed[0].type;
            token.inputOffset = packed[0].inputOffset;
            token.text =
                getTextAtOffset(&parser->pp, packed[0].inputOffset, packed[1].inputOffset - packed[0].inputOffset);
        }

        switch (token.type) {
            case Token::PreprocessorDirective:
            case Token::CStyleComment:
            case Token::LineComment:
                parser->tokenIndex++;
                break;

            case Token::Whitespace:
                if (parser->isOnlyPreprocessing)
                    return token;
                parser->tokenIndex++;
                break;

            default:
                return token;
        }
    }
}

inline Token readToken(ParserImpl* parser) {
    Token token = peekToken(parser);
    parser->tokenIndex++;
    return token;
}

bool okToStayInScope(ParserImpl* parser, const Token& token) {
    switch (token.type) {
        case Token::OpenCurly: {
            if (parser->outerAcceptFlags & ParserImpl::AcceptOpenCurly) {
                parser->tokenIndex--;
                return false;
            }
            break;
        }
        case Token::CloseCurly: {
            if (parser->outerAcceptFlags & ParserImpl::AcceptCloseCurly) {
                parser->tokenIndex--;
                return false;
            }
            break;
        }
        case Token::CloseParen: {
            if (parser->outerAcceptFlags & ParserImpl::AcceptCloseParen) {
                parser->tokenIndex--;
                return false;
            }
            break;
        }
        case Token::CloseAngle: {
            if (parser->outerAcceptFlags & ParserImpl::AcceptCloseAngle) {
                parser->tokenIndex--;
                return false;
            }
            break;
        }
        case Token::CloseSquare: {
            if (parser->outerAcceptFlags & ParserImpl::AcceptCloseSquare) {
                parser->tokenIndex--;
                return false;
            }
            break;
        }
        case Token::EOF:
            return false;
        default:
            break;
    }
    return true;
}

struct SetAcceptFlagsInScope {
    ParserImpl* parser;
    u32 prevAcceptFlags = 0;
    bool prevTokenizeRightShift = false;

    SetAcceptFlagsInScope(ParserImpl* parser, Token::Type openTokenType) : parser{parser} {
        this->prevAcceptFlags = parser->outerAcceptFlags;
        this->prevTokenizeRightShift = parser->tkr.config.tokenizeRightShift;

        switch (openTokenType) {
            case Token::OpenCurly: {
                parser->outerAcceptFlags = ParserImpl::AcceptCloseCurly;
                parser->tkr.config.tokenizeRightShift = true;
                break;
            }
            case Token::OpenParen: {
                parser->outerAcceptFlags =
                    (parser->outerAcceptFlags | ParserImpl::AcceptCloseParen) & ~ParserImpl::AcceptCloseAngle;
                parser->tkr.config.tokenizeRightShift = true;
                break;
            }
            case Token::OpenAngle: {
                parser->outerAcceptFlags = (parser->outerAcceptFlags | ParserImpl::AcceptCloseAngle);
                parser->tkr.config.tokenizeRightShift = false;
                break;
            }
            case Token::OpenSquare: {
                parser->outerAcceptFlags =
                    (parser->outerAcceptFlags | ParserImpl::AcceptCloseSquare) & ~ParserImpl::AcceptCloseAngle;
                parser->tkr.config.tokenizeRightShift = true;
                break;
            }
            default: {
                PLY_ASSERT(0); // Illegal
                break;
            }
        }
    }

    ~SetAcceptFlagsInScope() {
        parser->outerAcceptFlags = this->prevAcceptFlags;
        parser->tkr.config.tokenizeRightShift = this->prevTokenizeRightShift;
    }
};

//-------------------------------------------------------------------------------------
// skipAnyScope
//
// Returns false if an unexpected token is encountered and an outer scope is expected
// to handle it, as determined by parser->outerAcceptFlags.
//-------------------------------------------------------------------------------------
bool skipAnyScope(ParserImpl* parser, Token* outCloseToken, const Token& openToken) {
    SetAcceptFlagsInScope acceptScope{parser, openToken.type};
    Token::Type closePunc = (Token::Type)((u32) openToken.type + 1);
    for (;;) {
        Token token = readToken(parser);
        if (token.type == closePunc) {
            if (outCloseToken) {
                *outCloseToken = token;
            }
            return true;
        }

        if (!okToStayInScope(parser, token)) {
            parser->errorNoMute(
                Error, token.inputOffset,
                FMT_MSG("expected '{}'", getPunctuationString((Token::Type)((u32) openToken.type + 1))));
            parser->errorNoMute(Note, openToken.inputOffset, FMT_MSG("to match this '{}'", openToken.toString()));
            parser->muteErrors = true;
            return false;
        }

        switch (token.type) {
            case Token::OpenAngle: {
                if (!parser->tkr.config.tokenizeRightShift) {
                    // If we were immediately inside a template-parameter/argument scope < >, treat
                    // < as a nested scope, because we now need to encounter two CloseAngle tokens:
                    skipAnyScope(parser, nullptr, token);
                }
                // If we are not immediately inside a template-parameter/argument scope < >, don't
                // treat < as the beginning of a scope, since it might just be a less-than operator.
                break;
            }
            case Token::OpenCurly:
            case Token::OpenParen:
            case Token::OpenSquare: {
                skipAnyScope(parser, nullptr, token);
                break;
            }
            default: {
            }
        }
    }
}

// Returns false if the given token was pushed back and ends an outer scope. Otherwise, it consumes
// the given token. If the given token begins a new scope, it consumes tokens until either the inner
// scope is closed, or until the inner scope is "canceled" by a closing token that closes an outer
// scope, as determined by parser->outerAcceptFlags. In that case, the closing token is pushed
// back so that the caller can read it next. In each of those cases, it returns true to indicate to
// the caller that the given token was consumed and a new token is available to read.
bool handleUnexpectedToken(ParserImpl* parser, Token* outCloseToken, const Token& token) {
    // FIXME: Merge this with the second half of skipAnyScope:
    if (!okToStayInScope(parser, token))
        return false;

    switch (token.type) {
        case Token::OpenAngle: {
            if (!parser->tkr.config.tokenizeRightShift) {
                // If we were immediately inside a template-parameter/argument scope < >, treat
                // < as a nested scope, because we now need to encounter two Close_Angle tokens:
                skipAnyScope(parser, outCloseToken, token);
                // Ignore the return value of skipAnyScope. If it's false, that means some token
                // canceled the inner scope and was pushed back. We want the caller to read that
                // token next.
            }
            // If we are not immediately inside a template-parameter/argument scope < >, don't
            // treat < as the beginning of a scope, since it might just be a less-than operator.
            return true;
        }
        case Token::OpenCurly:
        case Token::OpenParen:
        case Token::OpenSquare: {
            skipAnyScope(parser, outCloseToken, token);
            // Ignore the return value of skipAnyScope. If it's false, that means some token
            // canceled the inner scope and was pushed back. We want the caller to read that token
            // next.
            return true;
        }
        // FIXME: Log errors for unmatched closing brackets
        default: {
            return true;
        }
    }
}

bool closeScope(ParserImpl* parser, Token* outCloseToken, const Token& openToken) {
    Token closeToken = peekToken(parser);
    if (closeToken.type == openToken.type + 1) {
        parser->tokenIndex++;
        *outCloseToken = closeToken;
    } else {
        parser->error(Error, closeToken.inputOffset,
                      FMT_MSG("expected '{}' before '{}'", (openToken.type == Token::OpenSquare ? ']' : ')'),
                              closeToken.toString()));
        // Consume tokens up to the closing )
        if (!skipAnyScope(parser, nullptr, openToken)) {
            // We didn't get a closing ), but an outer scope will handle it
            PLY_ASSERT(parser->muteErrors);
            return false;
        }
        // Got closing )
        parser->muteErrors = false;
    }
    return true;
}

//----------------------------------------------
// Helpers
//----------------------------------------------

StringView getClassName(const QualifiedID& qid) {
    if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        return identifier->name.text;
    } else if (auto* templateId = qid.var.as<QualifiedID::TemplateID>()) {
        return templateId->name.text;
    } else {
        return {};
    }
}

StringView getCtorDtorName(const QualifiedID& qid) {
    if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        return identifier->name.text;
    } else if (auto* destructor = qid.var.as<QualifiedID::Destructor>()) {
        return destructor->name.text;
    } else if (auto* tmplSpec = qid.var.as<QualifiedID::TemplateID>()) {
        return tmplSpec->name.text;
    }
    return {};
}

String toString(const QualifiedID& qid) {
    MemStream out;

    for (const QualifiedID::Prefix& comp : qid.prefix) {
        if (auto* ident = comp.var.as<QualifiedID::Identifier>()) {
            out.write(ident->name.text);
        } else if (auto* tmplSpec = comp.var.as<QualifiedID::TemplateID>()) {
            out.format("{}<>", tmplSpec->name.text);
        } else if (comp.var.is<QualifiedID::Decltype>()) {
            out.write("decltype()");
        } else {
            PLY_ASSERT(0);
        }
        out.write("::");
    }

    if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        out.write(identifier->name.text);
    } else if (auto* tmplSpec = qid.var.as<QualifiedID::TemplateID>()) {
        out.format("{}<>", tmplSpec->name.text);
    } else if (qid.var.is<QualifiedID::Decltype>()) {
        out.write("decltype()");
    } else if (auto* dtor = qid.var.as<QualifiedID::Destructor>()) {
        out.format("~{}", dtor->name.text);
    } else if (auto* opFunc = qid.var.as<QualifiedID::OperatorFunc>()) {
        out.format("operator{}{}", opFunc->punc.text, opFunc->punc2.text);
    } else if (qid.var.is<QualifiedID::ConversionFunc>()) {
        // FIXME: improve this
        out.write("(conversion)");
    } else if (qid.var.isEmpty()) {
        out.write("(empty)");
    } else {
        PLY_ASSERT(0);
    }

    return out.moveToString();
}

// Used when logging errors
Token getFirstToken(const QualifiedID& qid) {
    if (qid.prefix.numItems() > 0) {
        if (auto* identifier = qid.prefix[0].var.as<QualifiedID::Identifier>()) {
            return identifier->name;
        } else if (auto* tmplSpec = qid.prefix[0].var.as<QualifiedID::TemplateID>()) {
            return tmplSpec->name;
        } else if (auto* dt = qid.prefix[0].var.as<QualifiedID::Decltype>()) {
            return dt->keyword;
        }
        PLY_ASSERT(0); // Shouldn't be possible
    }

    if (qid.var.isEmpty()) {
        return {};
    } else if (auto* identifier = qid.var.as<QualifiedID::Identifier>()) {
        return identifier->name;
    } else if (auto* tmplSpec = qid.var.as<QualifiedID::TemplateID>()) {
        return tmplSpec->name;
    } else if (auto* dt = qid.var.as<QualifiedID::Decltype>()) {
        return dt->keyword;
    } else if (auto* destructor = qid.var.as<QualifiedID::Destructor>()) {
        return destructor->tilde;
    } else if (auto* opFunc = qid.var.as<QualifiedID::OperatorFunc>()) {
        return opFunc->keyword;
    } else if (auto* convFunc = qid.var.as<QualifiedID::ConversionFunc>()) {
        return convFunc->operatorKeyword;
    }
    PLY_ASSERT(0); // Shouldn't be possible
    return {};
}

Token getFirstToken(const Declaration::Entity& entity) {
    if (!entity.declSpecifiers.isEmpty()) {
        const DeclSpecifier& declSpec = *entity.declSpecifiers[0];
        if (auto* keyword = declSpec.var.as<DeclSpecifier::Keyword>()) {
            return keyword->token;
        } else if (auto* linkage = declSpec.var.as<DeclSpecifier::Linkage>()) {
            return linkage->externKeyword;
        } else if (auto* enum_ = declSpec.var.as<DeclSpecifier::Enum>()) {
            return enum_->keyword;
        } else if (auto* class_ = declSpec.var.as<DeclSpecifier::Class>()) {
            return class_->keyword;
        } else if (auto* typeSpec = declSpec.var.as<DeclSpecifier::TypeSpecifier>()) {
            if (typeSpec->elaborateKeyword.isValid())
                return typeSpec->elaborateKeyword;
            return getFirstToken(typeSpec->qid);
        } else if (auto* typeParam = declSpec.var.as<DeclSpecifier::TypeParameter>()) {
            return typeParam->keyword;
        } else if (auto* ellipsis = declSpec.var.as<DeclSpecifier::Ellipsis>()) {
            return ellipsis->token;
        }
    }
    if (!entity.initDeclarators.isEmpty()) {
        const InitDeclarator& initDecl = entity.initDeclarators[0];
        if (!initDecl.qid.isEmpty()) {
            return getFirstToken(initDecl.qid);
        }
    }
    PLY_ASSERT(0);
    return {};
}

Token Declaration::getFirstToken() const {
    if (auto* linkage = this->var.as<Declaration::Linkage>()) {
        return linkage->externKeyword;
    } else if (auto* namespace_ = this->var.as<Declaration::Namespace>()) {
        return namespace_->keyword;
    } else if (auto* entity = this->var.as<Declaration::Entity>()) {
        return cpp::getFirstToken(*entity);
    } else if (auto* template_ = this->var.as<Declaration::Template>()) {
        return template_->keyword;
    } else if (auto* typeAlias = this->var.as<Declaration::TypeAlias>()) {
        return typeAlias->usingKeyword;
    } else if (auto* usingNamespace = this->var.as<Declaration::UsingNamespace>()) {
        return usingNamespace->usingKeyword;
    } else if (auto* staticAssert = this->var.as<Declaration::StaticAssert>()) {
        return staticAssert->keyword;
    } else if (auto* accessSpec = this->var.as<Declaration::AccessSpecifier>()) {
        return accessSpec->keyword;
    }
    PLY_ASSERT(0);
    return {};
}

//-----------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------
enum class ParseQualifiedMode {
    AllowIncomplete,
    RequireComplete,
    RequireCompleteOrEmpty,
};

struct Declarator {
    Owned<DeclProduction> prod;
    QualifiedID qid;
};

struct DeclaratorFlags {
    static const u32 AllowNamed = 1;
    static const u32 AllowAbstract = 2;
};

struct ParsedExpression {
    Token startToken;
    Token endToken;
};

QualifiedID parseQualifiedId(ParserImpl* parser, ParseQualifiedMode mode);
void parseDeclarator(ParserImpl* parser, Declarator& dcor, DeclProduction* nested, u32 dcorFlags);
void parseOptionalTypeIdInitializer(ParserImpl* parser, Initializer& result);
void parseOptionalVariableInitializer(ParserImpl* parser, Initializer& result, bool allowBracedInit);
ParsedExpression parseExpression(ParserImpl* parser, bool optional = false);
Array<Declaration> parseDeclarationList(ParserImpl* parser, Token* outCloseCurly, StringView enclosingClassName);

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//   ▄▄▄▄                ▄▄▄  ▄▄   ▄▄▄ ▄▄            ▄▄     ▄▄▄▄ ▄▄▄▄▄
//  ██  ██ ▄▄  ▄▄  ▄▄▄▄   ██  ▄▄  ██   ▄▄  ▄▄▄▄   ▄▄▄██      ██  ██  ██  ▄▄▄▄
//  ██  ██ ██  ██  ▄▄▄██  ██  ██ ▀██▀▀ ██ ██▄▄██ ██  ██      ██  ██  ██ ▀█▄▄▄
//  ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄██ ▄██▄ ██  ██   ██ ▀█▄▄▄  ▀█▄▄██     ▄██▄ ██▄▄█▀  ▄▄▄█▀
//      ▀▀

TypeID parseTypeId(ParserImpl* parser) {
    TypeID result;
    s32 typeSpecifierIndex = -1;
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                result.declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else {
                if (typeSpecifierIndex < 0) {
                    parser->muteErrors = false;
                } else {
                    parser->error(Error, token.inputOffset, "type-id cannot have a name");
                }
                typeSpecifierIndex = result.declSpecifiers.numItems();
                DeclSpecifier* declSpec = result.declSpecifiers.append(Heap::create<DeclSpecifier>());
                auto& typeSpec = declSpec->var.switchTo<DeclSpecifier::TypeSpecifier>();
                if (token.text == "typename" || token.text == "struct" || token.text == "class" ||
                    token.text == "union" || token.text == "enum") {
                    typeSpec.elaborateKeyword = token;
                    parser->tokenIndex++;
                }
                typeSpec.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
            }
        } else {
            // Not an identifier. We should have parsed a type specifier by now.
            if (typeSpecifierIndex < 0) {
                parser->error(Error, token.inputOffset,
                              FMT_MSG("expected type specifier before '{}'", token.toString()));
            }
            break;
        }
    }

    // Parse optional abstract declarator.
    Declarator dcor;
    parseDeclarator(parser, dcor, nullptr, DeclaratorFlags::AllowAbstract);
    PLY_ASSERT(dcor.qid.isEmpty());
    result.abstractDcor = std::move(dcor.prod);
    return result;
}

// Consumes as much as it can; unrecognized tokens are returned to caller without logging an error
Array<QualifiedID::Prefix> parseNestedNameSpecifier(ParserImpl* parser) {
    // FIXME: Support leading ::
    Array<QualifiedID::Prefix> prefix;
    for (;;) {
        QualifiedID::Prefix* comp = nullptr;

        Token token = peekToken(parser);
        if (token.type != Token::Identifier)
            break;

        if (token.text == "operator" || token.text == "const" || token.text == "volatile" || token.text == "inline" ||
            token.text == "static" || token.text == "friend")
            break;

        parser->tokenIndex++;
        if (token.text == "decltype") {
            comp = &prefix.append();
            auto& dt = comp->var.switchTo<QualifiedID::Decltype>();
            dt.keyword = token;
            Token puncToken = peekToken(parser);
            if (puncToken.type == Token::OpenParen) {
                parser->tokenIndex++;
                dt.openParen = puncToken;
                skipAnyScope(parser, &dt.closeParen, puncToken);
            } else {
                // expected (
                parser->error(Error, puncToken.inputOffset, FMT_MSG("expected '(' before '{}'", puncToken.toString()));
            }
        } else {
            comp = &prefix.append();
            Token puncToken = peekToken(parser);
            if (puncToken.type == Token::OpenAngle) {
                auto& tmplSpec = comp->var.switchTo<QualifiedID::TemplateID>();
                tmplSpec.name = token;
                parser->tokenIndex++;
                // FIXME: We should only parse < as the start of a template-argument list if we know
                // that the preceding name refers to a template function or type. For now, we assume
                // it always does. If we ever start parsing function bodies, we won't be able to
                // assume this.
                if (parser->passNumber <= 1) {
                    tmplSpec.openAngle = puncToken;

                    // Parse template-argument-list
                    SetAcceptFlagsInScope acceptScope{parser, Token::OpenAngle};
                    PLY_SET_IN_SCOPE(parser->tkr.config.tokenizeRightShift, false);

                    for (;;) {
                        // FIXME: Parse constant expressions here instead of only allowing type IDs

                        // Try to parse a type ID
                        auto& templateArg = tmplSpec.args.append();
                        RestorePoint rp{parser};
                        TypeID typeId = parseTypeId(parser);
                        if (!rp.errorOccurred()) {
                            // Successfully parsed a type ID
                            templateArg.var = std::move(typeId);
                        } else {
                            rp.backtrack();
                            rp.cancel();
                        }

                        for (;;) {
                            Token sepToken = readToken(parser);
                            if (sepToken.type == Token::CloseAngle) {
                                // End of template-argument-list
                                tmplSpec.closeAngle = sepToken;
                                parser->muteErrors = false;
                                goto breakArgs;
                            } else if (sepToken.type == Token::Comma) {
                                // Comma
                                templateArg.comma = sepToken;
                                parser->muteErrors = false;
                                break;
                            } else {
                                // Unexpected token
                                Token endToken;
                                if (!handleUnexpectedToken(parser, &endToken, sepToken))
                                    goto breakOuter;
                            }
                        }
                    }
                breakArgs:;
                } else {
                    PLY_FORCE_CRASH(); // FIXME: implement this
                }
            } else {
                auto& ident = comp->var.switchTo<QualifiedID::Identifier>();
                ident.name = token;
            }
        }

        PLY_ASSERT(comp);

        Token sepToken = peekToken(parser);
        if (sepToken.type != Token::DoubleColon)
            break;
        parser->tokenIndex++;
        comp->doubleColon = sepToken;
    }

breakOuter:
    return prefix;
}

// Consumes as much as it can; unrecognized tokens are returned to caller without logging an error
QualifiedID parseQualifiedId(ParserImpl* parser, ParseQualifiedMode mode) {
    QualifiedID qid;
    qid.prefix = parseNestedNameSpecifier(parser);
    if (qid.prefix.numItems() > 0) {
        QualifiedID::Prefix& tail = qid.prefix.back();
        if (!tail.doubleColon.isValid()) {
            if (auto* ident = tail.var.as<QualifiedID::Identifier>()) {
                qid.var = std::move(*ident);
            } else if (auto* tmplId = tail.var.as<QualifiedID::TemplateID>()) {
                qid.var = std::move(*tmplId);
            } else if (auto* dt = tail.var.as<QualifiedID::Decltype>()) {
                qid.var = std::move(*dt);
            }
            qid.prefix.pop();
        }
    }
    if (qid.var.isEmpty()) {
        Token token = peekToken(parser);
        if (token.type == Token::Tilde) {
            parser->tokenIndex++;
            Token token2 = peekToken(parser);
            if (token2.type != Token::Identifier) {
                // Expected class name after ~
                parser->error(Error, token2.inputOffset,
                              FMT_MSG("expected destructor name before '{}'", token2.toString()));
            } else {
                parser->tokenIndex++;
                auto& dtor = qid.var.switchTo<QualifiedID::Destructor>();
                PLY_ASSERT(token2.text != "decltype"); // FIXME: Support this
                dtor.tilde = token;
                dtor.name = token2;
            }
        } else if (token.type == Token::Identifier) {
            if (token.text == "operator") {
                parser->tokenIndex++;
                auto& opFunc = qid.var.switchTo<QualifiedID::OperatorFunc>();
                opFunc.keyword = token;
                Token opToken = readToken(parser);
                switch (opToken.type) {
                    case Token::LeftShift:
                    case Token::RightShift:
                    case Token::SinglePlus:
                    case Token::DoublePlus:
                    case Token::SingleMinus:
                    case Token::DoubleMinus:
                    case Token::Star:
                    case Token::Arrow:
                    case Token::ForwardSlash:
                    case Token::SingleEqual:
                    case Token::DoubleEqual:
                    case Token::NotEqual:
                    case Token::PlusEqual:
                    case Token::MinusEqual:
                    case Token::StarEqual:
                    case Token::SlashEqual:
                    case Token::OpenAngle:
                    case Token::CloseAngle:
                    case Token::LessThanOrEqual:
                    case Token::GreaterThanOrEqual:
                    case Token::OpenParen:
                    case Token::OpenSquare: {
                        opFunc.punc = opToken;
                        if (opToken.type == Token::OpenParen) {
                            Token opToken2 = readToken(parser);
                            if (opToken2.type == Token::CloseParen) {
                                opFunc.punc2 = opToken2;
                            } else {
                                // Expected ) after (
                                parser->error(Error, opToken2.inputOffset,
                                              FMT_MSG("expected ')' before '{}'", opToken2.toString()));
                                parser->tokenIndex--;
                            }
                        } else if (opToken.type == Token::OpenSquare) {
                            Token opToken2 = readToken(parser);
                            if (opToken2.type == Token::CloseSquare) {
                                opFunc.punc2 = opToken2;
                            } else {
                                parser->error(Error, opToken2.inputOffset,
                                              FMT_MSG("expected ']' before '{}'", opToken2.toString()));
                                parser->tokenIndex--;
                            }
                        }
                        break;
                    }

                    default: {
                        // Expected operator token
                        parser->error(Error, opToken.inputOffset,
                                      FMT_MSG("expected operator token before '{}'", opToken.toString()));
                        parser->tokenIndex--;
                        break;
                    };
                }
            }
        }
    }
    if (((mode == ParseQualifiedMode::RequireComplete) && qid.var.isEmpty()) ||
        ((mode == ParseQualifiedMode::RequireCompleteOrEmpty) && qid.var.isEmpty() && !qid.prefix.isEmpty())) {
        // FIXME: Improve these error messages
        Token token = peekToken(parser);
        parser->error(Error, token.inputOffset, FMT_MSG("expected qualified-id before '{}'", token.toString()));
    }
    return qid;
}

// Consumes as much as it can; unrecognized tokens are returned to caller without logging an error:
void parseConversionTypeId(ParserImpl* parser, QualifiedID::ConversionFunc* conv) {
    bool gotTypeSpecifier = false;
    for (;;) {
        Token token = peekToken(parser);
        if (token.type != Token::Identifier)
            break;

        if (token.text == "const" || token.text == "volatile") {
            parser->tokenIndex++;
            conv->declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
        } else {
            QualifiedID qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
            if (gotTypeSpecifier) {
                // We already got a type specifier.
                // This is not a breaking error; just ignore it and continue from here.
                parser->errorNoMute(Error, getFirstToken(qid).inputOffset, "too many type specifiers");
            } else {
                gotTypeSpecifier = true;
                PLY_ASSERT(!qid.var.isEmpty()); // Shouldn't happen because token was an identifier
                conv->declSpecifiers.append(
                    Heap::create<DeclSpecifier>(DeclSpecifier::TypeSpecifier{{}, std::move(qid)}));
            }
        }
    }

    // Parse the optional (limited) abstract declarator part:
    bool allowQualifier = false;
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::Star || token.type == Token::SingleAmpersand || token.type == Token::DoubleAmpersand) {
            parser->tokenIndex++;
            auto* prod = Heap::create<DeclProduction>();
            auto& ptrTo = prod->var.switchTo<DeclProduction::Indirection>();
            ptrTo.punc = token;
            prod->child = std::move(conv->abstractDcor);
            conv->abstractDcor = std::move(prod);
            allowQualifier = (token.type == Token::Star);
        } else if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile") {
                parser->tokenIndex++;
                if (!allowQualifier) {
                    // Qualifier not allowed here (eg. immediately after comma in declarator
                    // list). This is not a breaking error; just ignore it and continue from here.
                    parser->errorNoMute(Error, token.inputOffset,
                                        FMT_MSG("'{}' qualifier not allowed here", token.text));
                }

                auto* prod = Heap::create<DeclProduction>();
                auto& qualifier = prod->var.switchTo<DeclProduction::Qualifier>();
                qualifier.keyword = token;
                prod->child = std::move(conv->abstractDcor);
                conv->abstractDcor = std::move(prod);
            } else
                break;
        } else
            break;
    }
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄▄               ▄▄▄                        ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██  ██ ██▄▄██ ██     ██   ▄▄▄██ ██  ▀▀  ▄▄▄██  ██   ██  ██ ██  ▀▀ ▀█▄▄▄
//  ██▄▄█▀ ▀█▄▄▄  ▀█▄▄▄ ▄██▄ ▀█▄▄██ ██     ▀█▄▄██  ▀█▄▄ ▀█▄▄█▀ ██      ▄▄▄█▀
//

Parameter parseTemplateParameter(ParserImpl* parser) {
    Parameter result;
    s32 typeSpecifierIndex = -1;
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile" || token.text == "unsigned") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                result.declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else {
                if (token.text == "typename" || token.text == "class") {
                    if (typeSpecifierIndex < 0) {
                        parser->muteErrors = false;
                    } else {
                        parser->error(Error, token.inputOffset, "too many type specifiers");
                    }
                    parser->tokenIndex++;
                    DeclSpecifier* declSpec = result.declSpecifiers.append(Heap::create<DeclSpecifier>());
                    auto& typeParam = declSpec->var.switchTo<DeclSpecifier::TypeParameter>();
                    typeParam.keyword = token;

                    Token t2 = peekToken(parser);
                    if (t2.type == Token::Ellipsis) {
                        parser->tokenIndex++;
                        typeParam.ellipsis = t2;
                    }

                    QualifiedID qid = parseQualifiedId(parser, ParseQualifiedMode::RequireCompleteOrEmpty);
                    if (!qid.prefix.isEmpty()) {
                        if (token.text == "typename") {
                            // Treat this qualified name as non-type template parameter.
                            typeSpecifierIndex = result.declSpecifiers.numItems();
                            auto& typeSpec = declSpec->var.switchTo<DeclSpecifier::TypeSpecifier>();
                            typeSpec.elaborateKeyword = token;
                            typeSpec.qid = std::move(qid);
                            continue;
                        } else {
                            parser->error(Error, getFirstToken(qid).inputOffset,
                                          "template parameter name cannot have a nested name prefix");
                        }
                    } else if (auto* ident = qid.var.as<QualifiedID::Identifier>()) {
                        result.identifier = ident->name;
                    } else if (!qid.isEmpty()) {
                        parser->error(Error, getFirstToken(qid).inputOffset, "expected identifier");
                    }
                    parseOptionalTypeIdInitializer(parser, result.init);
                    return result;
                }

                parser->muteErrors = false;
                if (typeSpecifierIndex >= 0)
                    break; // Parse it as a declarator.

                typeSpecifierIndex = result.declSpecifiers.numItems();
                DeclSpecifier* declSpec = result.declSpecifiers.append(Heap::create<DeclSpecifier>());
                auto& typeSpec = declSpec->var.switchTo<DeclSpecifier::TypeSpecifier>();
                typeSpec.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
            }
        } else {
            // Not an identifier. We should have parsed a type specifier by now.
            if (typeSpecifierIndex < 0) {
                parser->error(Error, token.inputOffset,
                              FMT_MSG("expected template parameter before '{}'", token.toString()));
            }
            break;
        }
    }

    Declarator dcor;
    parseDeclarator(parser, dcor, nullptr, DeclaratorFlags::AllowNamed | DeclaratorFlags::AllowAbstract);
    if (!dcor.qid.isEmpty()) {
        if (!dcor.qid.prefix.isEmpty()) {
            parser->error(Error, getFirstToken(dcor.qid).inputOffset,
                          "template parameter name cannot have a nested-name prefix");
        } else if (!dcor.qid.var.is<QualifiedID::Identifier>()) {
            parser->error(Error, getFirstToken(dcor.qid).inputOffset, "expected identifier");
        } else {
            result.identifier = std::move(dcor.qid.var.as<QualifiedID::Identifier>()->name);
        }
    }
    result.prod = std::move(dcor.prod);
    parseOptionalVariableInitializer(parser, result.init, false);
    return result;
}

Parameter parseFunctionParameter(ParserImpl* parser) {
    Parameter result;
    s32 typeSpecifierIndex = -1;
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "const" || token.text == "volatile" || token.text == "unsigned") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                result.declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else if (token.text == "typename" || token.text == "struct" || token.text == "class" ||
                       token.text == "union" || token.text == "enum") {
                if (typeSpecifierIndex < 0) {
                    parser->muteErrors = false;
                } else {
                    parser->error(Error, token.inputOffset, "too many type specifiers");
                }
                parser->tokenIndex++;
                DeclSpecifier* declSpec = result.declSpecifiers.append(Heap::create<DeclSpecifier>());
                auto& typeSpec = declSpec->var.switchTo<DeclSpecifier::TypeSpecifier>();
                typeSpec.elaborateKeyword = token;
                typeSpec.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
            } else {
                parser->muteErrors = false;
                if (typeSpecifierIndex >= 0)
                    break; // This must be the declarator part.

                typeSpecifierIndex = result.declSpecifiers.numItems();
                DeclSpecifier* declSpec = result.declSpecifiers.append(Heap::create<DeclSpecifier>());
                auto& typeSpec = declSpec->var.switchTo<DeclSpecifier::TypeSpecifier>();
                typeSpec.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
                // We should check at this point that qid actually refers to a type (if possible!). Consider for example
                // (inside class 'Foo'):
                //      Foo(baz())
                // If 'baz' refers to a type, it's a constructor. Otherwise, it's a function 'baz'
                // returning Foo. If it's not possible to determine in this pass, we obviously have
                // to guess (leaning towards it being a constructor), but the parse tree should
                // store the fact that we guessed somewhere.
            }
        } else {
            // Not an identifier. We should have parsed a type specifier by now.
            if (typeSpecifierIndex < 0) {
                parser->error(Error, token.inputOffset,
                              FMT_MSG("expected parameter type before '{}'", token.toString()));
            }
            break;
        }
    }

    Declarator dcor;
    parseDeclarator(parser, dcor, nullptr, DeclaratorFlags::AllowNamed | DeclaratorFlags::AllowAbstract);
    if (!dcor.qid.isEmpty()) {
        if (!dcor.qid.prefix.isEmpty()) {
            parser->error(Error, getFirstToken(dcor.qid).inputOffset,
                          "parameter name cannot have a nested-name prefix");
        } else if (!dcor.qid.var.is<QualifiedID::Identifier>()) {
            parser->error(Error, getFirstToken(dcor.qid).inputOffset, "expected identifier");
        } else {
            result.identifier = std::move(dcor.qid.var.as<QualifiedID::Identifier>()->name);
        }
    }
    result.prod = std::move(dcor.prod);
    parseOptionalVariableInitializer(parser, result.init, false);
    return result;
}

Array<Token> parseFunctionQualifierSeq(ParserImpl* parser) {
    Array<Token> qualifiers;

    // Read trailing qualifiers
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::Identifier && (token.text == "const" || token.text == "override")) {
            parser->tokenIndex++;
            qualifiers.append(token);
        } else if (token.type == Token::SingleAmpersand || token.type == Token::DoubleAmpersand) {
            parser->tokenIndex++;
            qualifiers.append(token);
        } else {
            break;
        }
    }

    return qualifiers;
}

struct ParseParams {
    Token::Type openPunc = Token::OpenParen;
    Token::Type closePunc = Token::CloseParen;

    static ParseParams Func;
    static ParseParams Template;
};

ParseParams ParseParams::Func = {};
ParseParams ParseParams::Template = {
    Token::OpenAngle,
    Token::CloseAngle,
};

void parseParameterDeclarationList(ParserImpl* parser, Array<Parameter>* params, bool forTemplate) {
    const ParseParams* pp = forTemplate ? &ParseParams::Template : &ParseParams::Func;

    parser->muteErrors = false;

    Token token = peekToken(parser);
    if (token.type == pp->closePunc)
        return; // Empty parameter declaration list

    SetAcceptFlagsInScope acceptScope{parser, pp->openPunc};

    for (;;) {
        // A parameter declaration is expected here.
        Parameter* param = nullptr;
        bool anyTokensConsumed = false;

        Token expectedLoc = peekToken(parser);
        if (expectedLoc.type == Token::Ellipsis && !forTemplate) {
            parser->tokenIndex++;
            // FIXME: Check somewhere that this is the last parameter
            param = &params->append();
            DeclSpecifier* declSpec = Heap::create<DeclSpecifier>();
            auto& ellipsis = declSpec->var.switchTo<DeclSpecifier::Ellipsis>();
            ellipsis.token = expectedLoc;
            param->declSpecifiers.append(declSpec);
            anyTokensConsumed = true;
        } else {
            u32 savedTokenIndex = parser->tokenIndex;
            if (forTemplate) {
                param = &params->append(parseTemplateParameter(parser));
            } else {
                param = &params->append(parseFunctionParameter(parser));
            }
            anyTokensConsumed = (savedTokenIndex != parser->tokenIndex);
        }

        token = peekToken(parser);
        if (token.type == pp->closePunc) {
            // End of parameter declaration list
            break;
        } else if (token.type == Token::Comma) {
            parser->tokenIndex++;
            if (param) {
                param->comma = token;
            }
        } else {
            // Unexpected token
            parser->error(Error, token.inputOffset,
                          FMT_MSG("expected ',' or '{}' before '{}'", (forTemplate ? '>' : ')'), token.toString()));
            parser->tokenIndex++;
            if (anyTokensConsumed) {
                if (!handleUnexpectedToken(parser, nullptr, token))
                    break;
            } else {
                if (!okToStayInScope(parser, token))
                    break;
            }
        }
    }
}

DeclProduction* parseParameterList(ParserImpl* parser, Owned<DeclProduction>** prodToModify) {
    Token openParen = peekToken(parser);
    if (openParen.type != Token::OpenParen) {
        // Currently, we only hit this case when optimistically trying to parse a constructor
        PLY_ASSERT(parser->restorePointEnabled); // Just a sanity check
        parser->error(Error, openParen.inputOffset, FMT_MSG("expected '(' before '{}'", openParen.toString()));
        return nullptr;
    }
    parser->muteErrors = false;

    auto* prod = Heap::create<DeclProduction>();
    auto& func = prod->var.switchTo<DeclProduction::Function>();
    func.openParen = openParen;
    parser->tokenIndex++;
    prod->child = std::move(**prodToModify);
    **prodToModify = prod;
    *prodToModify = &prod->child;

    parseParameterDeclarationList(parser, &func.params, false);
    Token closeParen = peekToken(parser);
    if (closeParen.type == Token::CloseParen) {
        func.closeParen = closeParen;
        parser->tokenIndex++;
        func.qualifiers = parseFunctionQualifierSeq(parser);
    }
    return prod;
}

void parseOptionalTrailingReturnType(ParserImpl* parser, DeclProduction* fnProd) {
    PLY_ASSERT(fnProd);
    PLY_ASSERT(fnProd->var.is<DeclProduction::Function>());
    auto& function = *fnProd->var.as<DeclProduction::Function>();

    Token arrowToken = peekToken(parser);
    if (arrowToken.type == Token::Arrow) {
        parser->tokenIndex++;
        function.arrow = arrowToken;
        // FIXME: Should parse a TypeID here, not just a qualified ID:
        function.trailingRetType = parseTypeId(parser);
    }
}

// When bad tokens are encountered, it consumes them until it encounters a token that an outer scope
// is expected to handle, as determined by parser->outerAcceptFlags. In that case, it returns
// early. If the bad token is one of { ( or [, it calls skipAnyScope().
//
// The first bad token sets parser->muteErrors to true. muteErrors remains true until it reaches
// the next good token. muteErrors may remain true when we return; this can happen, for example,
// when } is encountered, causing us to return early.
void parseDeclarator(ParserImpl* parser, Declarator& dcor, DeclProduction* nested, u32 dcorFlags) {
    dcor.prod = nested;
    bool allowQualifier = false;
    Owned<DeclProduction>* prodToModify = nullptr; // Used in phase two
    bool expectingQualifiedId = false;

    // This is the first phase of parsing a declarator. It handles everything up to trailing
    // function parameter lists and array subscripts.
    //
    // As it reads pointer, reference symbols and cv-qualifiers, it inserts new
    // DeclaratorProductions at the *head* of the current DeclarationProduction chain
    // (dcor.prod) so that they are effectively read right-to-left. For example,
    //      * const &
    // becomes "reference to const pointer" in the DeclarationProduction chain.
    //
    // Pointers can also have nested name specifiers, making them pointer-to-members:
    //      Foo::*
    //
    // If an open parenthesis is encountered during this phase, and the Allow_Abstract flags is
    // set, it first tries to parse a function parameter list; otherwise, or if that fails, it
    // tries to parse a nested declarator. If it's a nested declarator, nested
    // DeclarationProductions are inserted at the head of the current DeclarationProduction
    // chain. In either case, no further pointer/reference/cv-qualifiers are expected after the
    // closing parenthesis, so we break out of the loop and proceed to the second phase.
    //
    // When a qualified ID is encountered, it's considered the name of the declarator (in other
    // words, the declarator is not abstract), and we break out of the loop and proceed to the
    // second phase.

    for (;;) {
        // Try to tokenize a qualified ID.
        QualifiedID qid = parseQualifiedId(parser, ParseQualifiedMode::AllowIncomplete);
        if (!qid.var.isEmpty()) {
            if ((dcorFlags & DeclaratorFlags::AllowNamed) == 0) {
                // Qualified ID is not allowed here
                // FIXME: Should rewind instead of consuming the qualified-id????
                // The caller may log a more informative error at this token! (check test suite)
                parser->errorNoMute(Error, getFirstToken(qid).inputOffset, "type-id cannot have a name");
                // Don't mute errors
            }
            dcor.qid = std::move(qid);
            break; // Got qualified-id
        }
        // qid.unqual is empty, but qid.prefix might be a pointer prefix (as in a
        // pointer-to-member).

        Token token = readToken(parser);
        if (token.type == Token::OpenParen) {
            if (!qid.prefix.isEmpty()) {
                // Should not be preceded by nested name specifier
                parser->errorNoMute(Error, token.inputOffset,
                                    FMT_MSG("'{}' cannot have a nested name prefix", token.toString()));
                // Don't mute errors
            }

            parser->muteErrors = false;

            if ((dcorFlags & DeclaratorFlags::AllowAbstract) != 0) {
                // If abstract declarators are allowed, try to parse a function parameter list
                // first.
                parser->tokenIndex--;
                RestorePoint rp{parser};
                // FIXME: When a restore point is active, handleUnexpectedToken() should always
                // return false. Otherwise, parseParameterList could end up consuming way too many
                // tokens, and it might even incorrectly "pre-tokenize" '>>' as a right-shift
                // operator instead of as two CloseAngles...
                DeclProduction* savedProd = dcor.prod;
                prodToModify = &dcor.prod;
                DeclProduction* fnProd = parseParameterList(parser, &prodToModify);
                if (!rp.errorOccurred()) {
                    // Success. Parse optional trailing return type. If any parse errors occur while
                    // doing so, we won't backtrack.
                    PLY_ASSERT(fnProd);
                    rp.cancel();
                    parseOptionalTrailingReturnType(parser, fnProd);
                    // Break out of the loop and continue with the second phase.
                    break;
                }

                // It didn't parse as a function parameter list.
                // Roll back any productions that were created:
                while (dcor.prod != savedProd) {
                    PLY_ASSERT(dcor.prod);
                    DeclProduction* child = dcor.prod->child.release();
                    dcor.prod = child;
                }
                rp.backtrack();
                rp.cancel();
                token = readToken(parser);
                prodToModify = nullptr;
            }

            // Parse it as a nested declarator.
            Declarator target;
            parseDeclarator(parser, target, dcor.prod.release(), dcorFlags);
            dcor.prod = Heap::create<DeclProduction>();
            auto& parenthesized = dcor.prod->var.switchTo<DeclProduction::Parenthesized>();
            parenthesized.openParen = token;
            dcor.prod->child = std::move(target.prod);
            PLY_ASSERT(dcor.qid.isEmpty());
            dcor.qid = std::move(target.qid);

            if (!closeScope(parser, &parenthesized.closeParen, token))
                return;
            break;
        }

        if (!qid.prefix.isEmpty()) {
            if (token.type != Token::Star) {
                // Should not be preceded by nested name specifier
                parser->errorNoMute(Error, token.inputOffset,
                                    FMT_MSG("'{}' cannot have a nested name prefix", token.toString()));
            }
        }

        if (token.type == Token::Star || token.type == Token::SingleAmpersand || token.type == Token::DoubleAmpersand) {
            parser->muteErrors = false;

            auto* prod = Heap::create<DeclProduction>();
            auto& ptrTo = prod->var.switchTo<DeclProduction::Indirection>();
            ptrTo.prefix = std::move(qid.prefix);
            ptrTo.punc = token;
            prod->child = std::move(dcor.prod);
            dcor.prod = prod;
            allowQualifier = (token.type == Token::Star);
        } else if (token.type == Token::Ellipsis) {
            // FIXME: Make a Production rule for this

            parser->muteErrors = false;
        } else if (token.type == Token::Identifier) {
            PLY_ASSERT(qid.prefix.isEmpty());
            PLY_ASSERT(token.text == "const" || token.text == "volatile" || token.text == "inline" ||
                       token.text == "static" || token.text == "friend");
            if (!allowQualifier) {
                // Qualifier not allowed here
                parser->errorNoMute(Error, token.inputOffset, FMT_MSG("'{}' qualifier not allowed here", token.text));
                // Handle it anyway...
            }

            parser->muteErrors = false;

            auto* prod = Heap::create<DeclProduction>();
            auto& qualifier = prod->var.switchTo<DeclProduction::Qualifier>();
            qualifier.keyword = token;
            prod->child = std::move(dcor.prod);
            dcor.prod = prod;
        } else {
            // End of first phase of parsing a declarator.
            PLY_ASSERT(qid.prefix.isEmpty());
            if ((dcorFlags & DeclaratorFlags::AllowAbstract) == 0) {
                // Note that we still allow "empty" declarators (in other words, abstract
                // declarators with no DeclaratorProductions) even when Allow_Abstract is not
                // specified, so that class definitions like:
                //      struct Foo {};
                // do not log an error.
                //
                // With this in mind, if a declarator name was required but
                // none was given, log an error *only if* some DeclaratorProductions have been
                // created.
                //
                // FIXME: Log an error (or warning?) if it's an empty declarators that *doesn't*
                // define a new class/struct/union, such as:
                //      int;
                if (dcor.prod) {
                    parser->error(Error, token.inputOffset,
                                  FMT_MSG("expected qualified-id before '{}'", token.toString()));
                } else {
                    // No DeclaratorProductions have been created yet. We'll log an error if any are
                    // created in the second phase.
                    expectingQualifiedId = true;
                }
            }
            parser->tokenIndex--;
            break;
        }
    }

    // This is the second phase of parsing a declarator. It parses only trailing function
    // parameter lists and array subscripts. A subchain of DeclaratorProductions is built in the
    // same order that these are encountered, so that they're effectively read left-to-right.
    // For example,
    //      []()
    // becomes "array of functions" in the subchain. This subchain is inserted at the head of
    // dcor.prod, the current Decl_Production chain being built.
    //
    // Note that this phase can take place inside a nested declarator, which means that the
    // caller may continue inserting DeclaratorProductions at the head of the chain after we
    // return.
    //
    // FIXME: make sure this approach works correctly for things like (*x())()

    if (!prodToModify) {
        prodToModify = &dcor.prod;
    }
    for (;;) {
        Token token = peekToken(parser);
        auto checkExpectingQualifiedId = [&]() {
            parser->muteErrors = false;
            if (expectingQualifiedId) {
                parser->error(Error, token.inputOffset, FMT_MSG("expected qualified-id before '{}'", token.toString()));
                expectingQualifiedId = false;
            }
        };

        if (token.type == Token::OpenSquare) {
            parser->tokenIndex++;
            checkExpectingQualifiedId();

            auto* prod = Heap::create<DeclProduction>();
            auto& arrayOf = prod->var.switchTo<DeclProduction::ArrayOf>();
            arrayOf.openSquare = token;
            prod->child = std::move(*prodToModify);
            *prodToModify = prod;
            prodToModify = &prod->child;

            parseExpression(parser, true);

            if (!closeScope(parser, &arrayOf.closeSquare, token))
                return;
        } else if (token.type == Token::OpenParen) {
            checkExpectingQualifiedId();

            DeclProduction* fnProd = parseParameterList(parser, &prodToModify);
            if (fnProd) {
                parseOptionalTrailingReturnType(parser, fnProd);
            }
        } else
            break;
    }
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄        ▄▄  ▄▄   ▄▄        ▄▄▄  ▄▄
//   ██  ▄▄▄▄▄  ▄▄ ▄██▄▄ ▄▄  ▄▄▄▄   ██  ▄▄ ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//   ██  ██  ██ ██  ██   ██  ▄▄▄██  ██  ██   ▄█▀  ██▄▄██ ██  ▀▀ ▀█▄▄▄
//  ▄██▄ ██  ██ ██  ▀█▄▄ ██ ▀█▄▄██ ▄██▄ ██ ▄██▄▄▄ ▀█▄▄▄  ██      ▄▄▄█▀
//

void skipMemberInitializerList(ParserImpl* parser) {
    // Make sure that if { is encountered (even with unexpected placement), we return to caller.
    PLY_SET_IN_SCOPE(parser->outerAcceptFlags, parser->outerAcceptFlags | ParserImpl::AcceptOpenCurly);
    // FIXME: Add a scope to declare that we are parsing a member initializer list, and report this
    // scope in any logged errors (?)

    for (;;) {
        QualifiedID qid = parseQualifiedId(parser, ParseQualifiedMode::AllowIncomplete);
        if (!qid.var.isEmpty()) {
            Token openBraceToken = peekToken(parser);
            if ((openBraceToken.type == Token::OpenParen) || (openBraceToken.type == Token::OpenCurly)) {
                parser->tokenIndex++;
                skipAnyScope(parser, nullptr, openBraceToken);
            } else {
                // expected ( or {
                // FIXME: should report that it was expected after qualified id
                parser->error(Error, openBraceToken.inputOffset,
                              FMT_MSG("expected '{{' or '(' before '{}'", openBraceToken.toString()));
                continue;
            }

            Token nextToken = peekToken(parser);
            if (nextToken.type == Token::OpenCurly) {
                // End of member initializer list.
                parser->muteErrors = false;
                break;
            } else if (nextToken.type == Token::Comma) {
                parser->tokenIndex++;
                parser->muteErrors = false;
            } else {
                parser->error(Error, nextToken.inputOffset, "expected function body after member initializer list");
                break;
            }
        } else {
            Token token = peekToken(parser);
            parser->error(Error, token.inputOffset,
                          FMT_MSG("expected class member or base class name before '{}'", token.toString()));
            if (qid.prefix.isEmpty()) {
                parser->tokenIndex++;
                if (!handleUnexpectedToken(parser, nullptr, token))
                    break;
            }
        }
    }
}

void parseOptionalFunctionBody(ParserImpl* parser, Initializer& result, const Declaration::Entity& entity) {
    result.var = {};
    Token token = peekToken(parser);
    if (token.type == Token::SingleEqual) {
        parser->tokenIndex++;
        auto& assign = result.var.switchTo<Initializer::Assignment>();
        assign.equalSign = token;
        parseExpression(parser); // FIXME: Fill in varInit
        return;
    }
    if (token.type == Token::SingleColon) {
        parser->tokenIndex++;
        auto& funcBody = result.var.switchTo<Initializer::FunctionBody>();
        funcBody.colon = token;
        // FIXME: populate MemberInitializer
        skipMemberInitializerList(parser);
        token = peekToken(parser);
    }
    if (token.type == Token::OpenCurly) {
        parser->tokenIndex++;
        auto& funcBody = result.var.switchTo<Initializer::FunctionBody>();
        funcBody.colon = token;
        skipAnyScope(parser, &funcBody.closeCurly, token);
    }
}

void parseOptionalTypeIdInitializer(ParserImpl* parser, Initializer& result) {
    result.var = {};
    Token token = peekToken(parser);
    if (token.type == Token::SingleEqual) {
        parser->tokenIndex++;
        auto& assign = result.var.switchTo<Initializer::Assignment>();
        assign.equalSign = token;
        token = readToken(parser);
        if (token.text == "0") {
            // FIXME: Support <typename A::B = 0> correctly!
        } else {
            parser->tokenIndex--;
            u32 savedErrorCount = parser->rawErrorCount;
            TypeID typeId = parseTypeId(parser);
            if (savedErrorCount == parser->rawErrorCount) {
                // No errors
                assign.var = std::move(typeId);
            }
        }
    }
}

void parseOptionalVariableInitializer(ParserImpl* parser, Initializer& result, bool allowBracedInit) {
    PLY_ASSERT(result.var.isEmpty());
    Token token = peekToken(parser);
    if (token.type == Token::OpenCurly) {
        // It's a variable initializer
        result.var.switchTo<Initializer::Assignment>();
        parseExpression(parser); // FIXME: Fill in varInit
    } else if (token.type == Token::SingleEqual) {
        parser->tokenIndex++;
        auto& assign = result.var.switchTo<Initializer::Assignment>();
        assign.equalSign = token;
        parseExpression(parser);
        assign.var.switchTo<Owned<Expression>>();
        // FIXME: Fill in
    } else if (token.type == Token::SingleColon) {
        parser->tokenIndex++;
        auto& bitField = result.var.switchTo<Initializer::BitField>();
        bitField.colon = token;
        parseExpression(parser);
    }
}

void parseInitDeclarators(ParserImpl* parser, Declaration::Entity& entity) {
    // A list of zero or more named declarators is accepted here.
    for (;;) {
        Declarator dcor;
        parseDeclarator(parser, dcor, nullptr, DeclaratorFlags::AllowNamed);
        if (dcor.qid.isEmpty())
            break; // Error was already logged
        InitDeclarator& initDcor = entity.initDeclarators.append();
        initDcor.qid = std::move(dcor.qid);
        initDcor.prod = std::move(dcor.prod);
        if (initDcor.prod && initDcor.prod->var.is<DeclProduction::Function>()) {
            parseOptionalFunctionBody(parser, initDcor.init, entity);
            if (initDcor.init.var.is<Initializer::FunctionBody>()) {
                if (entity.initDeclarators.numItems() > 1) {
                    // Note: Mixing function definitions and declarations could be a
                    // higher-level error instead of a parse error.
                    // FIXME: A reference to both declarators should be part of the error
                    // message. For now, we'll just use the open parenthesis token.
                    parser->errorNoMute(Error, initDcor.prod->var.as<DeclProduction::Function>()->openParen.inputOffset,
                                        "can't mix function definitions with other declarations");
                }
            }
            break; // Stop parsing declarators immediately after the function body.
        } else {
            parseOptionalVariableInitializer(parser, initDcor.init, true);
        }
        Token sepToken = peekToken(parser);
        if (sepToken.type == Token::Comma) {
            parser->tokenIndex++;
            if (initDcor.init.var.is<Initializer::FunctionBody>()) {
                // FIXME: It's not very clear from this error message that the comma is the
                // token that triggered an error. In any case, we don't hit this codepath yet,
                // as explained by the above comment.
                PLY_ASSERT(0); // codepath never gets hit at the moment
                parser->errorNoMute(Error, initDcor.prod->var.as<DeclProduction::Function>()->openParen.inputOffset,
                                    "can't mix function definitions with other declarations");
            }
            initDcor.comma = sepToken;
        } else
            break;
    }
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄▄               ▄▄▄                        ▄▄   ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██   ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██  ██ ██▄▄██ ██     ██   ▄▄▄██ ██  ▀▀  ▄▄▄██  ██   ██ ██  ██ ██  ██ ▀█▄▄▄
//  ██▄▄█▀ ▀█▄▄▄  ▀█▄▄▄ ▄██▄ ▀█▄▄██ ██     ▀█▄▄██  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀
//

Array<DeclSpecifier::Class::BaseSpecifier> parseBaseSpecifierList(ParserImpl* parser) {
    Array<DeclSpecifier::Class::BaseSpecifier> baseSpecifiers;
    for (;;) {
        DeclSpecifier::Class::BaseSpecifier baseSpec;

        // Optional access specifier
        Token token = peekToken(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "public" || token.text == "private" || token.text == "protected") {
                parser->tokenIndex++;
                parser->muteErrors = false;
                baseSpec.accessSpec = token;
                token = peekToken(parser);
            }
        }

        // Qualified ID
        baseSpec.baseQid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
        if (baseSpec.baseQid.var.isEmpty())
            break;
        parser->muteErrors = false;
        DeclSpecifier::Class::BaseSpecifier& addedBs = baseSpecifiers.append(std::move(baseSpec));

        // Comma or {
        Token puncToken = peekToken(parser);
        if (puncToken.type == Token::OpenCurly)
            break;
        if (puncToken.type == Token::Comma) {
            parser->tokenIndex++;
            addedBs.comma = token;
        } else {
            parser->tokenIndex++;
            parser->error(Error, puncToken.inputOffset,
                          FMT_MSG("expected ',' or '{{' before '{}'", puncToken.toString()));
            // FIXME: Call handleUnexpectedToken
            break;
        }
    }
    return baseSpecifiers;
}

DeclSpecifier::Class parseClassDeclaration(ParserImpl* parser) {
    DeclSpecifier::Class class_;
    Token token = readToken(parser);
    class_.keyword = token;
    class_.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireCompleteOrEmpty);

    // Read optional virt-specifier sequence
    {
        Token finalTok;
        for (;;) {
            token = readToken(parser);
            if (token.text == "final") {
                if (finalTok.isValid()) {
                    parser->error(Error, token.inputOffset, FMT_MSG("'{}' used more than once", token.text));
                } else {
                    finalTok = token;
                    class_.virtSpecifiers.append(token);
                }
            } else {
                break;
            }
        }
    }

    if (token.type == Token::SingleColon) {
        class_.colon = token;
        class_.baseSpecifiers = parseBaseSpecifierList(parser);
        token = readToken(parser);
    }

    if (token.type == Token::OpenCurly) {
        class_.openCurly = token;
        class_.memberDecls = parseDeclarationList(parser, &class_.closeCurly, getClassName(class_.qid));
    } else {
        parser->tokenIndex--;
    }
    return class_;
}

void parseEnumBody(ParserImpl* parser, DeclSpecifier::Enum* en) {
    parser->muteErrors = false;
    SetAcceptFlagsInScope acceptScope{parser, Token::OpenCurly};

    for (;;) {
        Token token = readToken(parser);
        if (token.type == Token::CloseCurly) {
            // Done
            parser->muteErrors = false;
            en->closeCurly = token;
            break;
        } else if (token.type == Token::Identifier) {
            parser->muteErrors = false;

            // Create enor
            DeclSpecifier::Enum::Item& enor = en->enumerators.append();
            enor.text = token;
            parseOptionalVariableInitializer(parser, enor.init, false);
            Token token2 = readToken(parser);
            bool done = false;
            if (token2.type == Token::Comma) {
                parser->muteErrors = false;
                enor.comma = token2;
            } else if (token2.type == Token::CloseCurly) {
                // Done
                parser->muteErrors = false;
                en->closeCurly = token2;
                done = true;
            } else {
                // expected , or } after enum member
                if (token2.type == Token::Identifier) {
                    parser->error(Error, token2.inputOffset, "missing ',' between enumerators");
                }
                // Other tokens will generate an error on next loop iteration
                parser->tokenIndex--;
            }
            if (done)
                break;
        } else {
            // expected enumerator or }
            parser->error(Error, token.inputOffset,
                          FMT_MSG("expected enumerator or '}}' before '{}'", token.toString()));
            if (!handleUnexpectedToken(parser, nullptr, token))
                return;
        }
    }
}

DeclSpecifier::Enum parseEnumDeclaration(ParserImpl* parser) {
    DeclSpecifier::Enum en;
    en.keyword = readToken(parser);
    Token token2 = peekToken(parser);
    if ((token2.type == Token::Identifier) && (token2.text == "class")) {
        parser->tokenIndex++;
        en.classKeyword = token2;
    }

    en.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireCompleteOrEmpty);

    Token sepToken = peekToken(parser);
    if (sepToken.type == Token::SingleColon) {
        parser->tokenIndex++;
        if (en.qid.isEmpty()) {
            parser->errorNoMute(Error, sepToken.inputOffset, "scoped enum requires a name");
        }
        en.colon = sepToken;
        en.base = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
    }

    Token token3 = peekToken(parser);
    if (token3.type == Token::OpenCurly) {
        parser->tokenIndex++;
        en.openCurly = token3;
        parseEnumBody(parser, &en);
    }
    return en;
}

bool looksLikeCtorDtor(StringView enclosingClassName, const QualifiedID& qid) {
    if (enclosingClassName.isEmpty()) {
        if (qid.prefix.numItems() < 1)
            return false;

        StringView ctorDtorName = getCtorDtorName(qid);
        if (ctorDtorName.isEmpty())
            return false;

        const QualifiedID::Prefix& tail = qid.prefix.back();
        if (auto* ident = tail.var.as<QualifiedID::Identifier>()) {
            PLY_ASSERT(ident->name.isValid());
            return ctorDtorName == ident->name.text;
        } else if (auto* tmplId = tail.var.as<QualifiedID::TemplateID>()) {
            PLY_ASSERT(tmplId->name.isValid());
            return ctorDtorName == tmplId->name.text;
        }

        return false;
    } else {
        if (qid.prefix.numItems() > 0)
            return false;

        StringView ctorDtorName = getCtorDtorName(qid);
        return ctorDtorName == enclosingClassName;
    }
}

Declaration parseEntityDeclaration(ParserImpl* parser, StringView enclosingClassName) {
    Declaration result;
    auto& entity = result.var.switchTo<Declaration::Entity>();
    u32 startInputOffset = peekToken(parser).inputOffset;
    u32 savedErrorCount = parser->rawErrorCount;

    // Parse the decl-specifier sequence.
    s32 typeSpecifierIndex = -1;
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::Identifier) {
            if (token.text == "extern") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                Token literal = peekToken(parser);
                if (literal.type == Token::StringLiteral) {
                    parser->tokenIndex++;
                    entity.declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Linkage{token, literal}));
                } else {
                    entity.declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
                }
            } else if (token.text == "inline" || token.text == "const" || token.text == "volatile" ||
                       token.text == "static" || token.text == "friend" || token.text == "virtual" ||
                       token.text == "constexpr" || token.text == "thread_local" || token.text == "unsigned" ||
                       token.text == "mutable" || token.text == "explicit") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                entity.declSpecifiers.append(Heap::create<DeclSpecifier>(DeclSpecifier::Keyword{token}));
            } else if (token.text == "alignas") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                // FIXME: Implement Decl_Specifier::AlignAs
                // Note: alignas is technically part of the attribute-specifier-seq in the
                // grammar, which means it can only appear before the decl-specifier-seq. But
                // for now, let's just accept it here:
                Token openParen = readToken(parser);
                if (openParen.type != Token::OpenParen) {
                    parser->error(Error, openParen.inputOffset,
                                  FMT_MSG("expected '(' before '{}'", openParen.toString()));
                    continue;
                }
                // FIXME: Accept integral constant expression here too
                TypeID typeId = parseTypeId(parser);
                Token closeParen;
                if (!closeScope(parser, &closeParen, openParen))
                    break;
            } else if (token.text == "typedef") {
                parser->muteErrors = false;
                parser->tokenIndex++;
                // FIXME: Store this token in the parse tree
            } else if (token.text == "struct" || token.text == "class" || token.text == "union") {
                parser->muteErrors = false;
                // FIXME: for TemplateParams, "class" should be treated like "typename".
                // Otherwise, it seems C++20 may actually support structs as non-type template
                // parameters, so we should revisit this eventually.
                if (typeSpecifierIndex >= 0) {
                    // Already got type specifier
                    parser->error(Error, token.inputOffset, "too many type specifiers");
                }
                DeclSpecifier::Class class_ = parseClassDeclaration(parser);
                typeSpecifierIndex = entity.declSpecifiers.numItems();
                entity.declSpecifiers.append(Heap::create<DeclSpecifier>(std::move(class_)));
            } else if (token.text == "enum") {
                parser->muteErrors = false;
                if (typeSpecifierIndex >= 0) {
                    parser->error(Error, token.inputOffset, "too many type specifiers");
                }
                DeclSpecifier::Enum en = parseEnumDeclaration(parser);
                typeSpecifierIndex = entity.declSpecifiers.numItems();
                entity.declSpecifiers.append(Heap::create<DeclSpecifier>(std::move(en)));
            } else if ((token.text == "operator") && (typeSpecifierIndex < 0)) {
                parser->muteErrors = false;
                parser->tokenIndex++;
                // It's a conversion function
                InitDeclarator& initDcor = entity.initDeclarators.append();
                auto& convFunc = initDcor.qid.var.switchTo<QualifiedID::ConversionFunc>();
                convFunc.operatorKeyword = token;
                parseConversionTypeId(parser, &convFunc);
                // Ensure there's an open parenthesis
                Token openParen = peekToken(parser);
                if (openParen.type == Token::OpenParen) {
                    parser->tokenIndex++;
                    initDcor.prod = Heap::create<DeclProduction>();
                    auto& func = initDcor.prod->var.switchTo<DeclProduction::Function>();
                    func.openParen = openParen;
                    parseParameterDeclarationList(parser, &func.params, false);
                    Token closeParen = peekToken(parser);
                    if (closeParen.type == Token::CloseParen) {
                        parser->tokenIndex++;
                        func.closeParen = closeParen;
                        func.qualifiers = parseFunctionQualifierSeq(parser);
                        parseOptionalFunctionBody(parser, initDcor.init, entity);
                    }
                    return result;
                } else {
                    parser->error(Error, openParen.inputOffset,
                                  FMT_MSG("expected '(' before '{}'", openParen.toString()));
                }
                break;
            } else {
                parser->muteErrors = false;
                if (typeSpecifierIndex >= 0)
                    // We already got a type specifier, so this must be the declarator part.
                    break;

                parser->tokenIndex++;
                Token typename_;
                QualifiedID qid;
                if (token.text == "typename") {
                    typename_ = token;
                    Token ellipsis;
                    Token t2 = peekToken(parser);
                    if (t2.type == Token::Ellipsis) {
                        parser->tokenIndex++;
                        ellipsis = t2;
                    }
                    qid = parseQualifiedId(parser, ParseQualifiedMode::RequireCompleteOrEmpty);
                    if (ellipsis.isValid()) {
                        parser->error(Error, ellipsis.inputOffset,
                                      FMT_MSG("expected qualified-id before '{}'", ellipsis.toString()));
                    }
                } else {
                    parser->tokenIndex--;
                    qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
                    PLY_ASSERT(!qid.isEmpty()); // Shouldn't happen because token was an Identifier
                }

                if (!typename_.isValid() && looksLikeCtorDtor(enclosingClassName, qid)) {
                    // Try (optimistically) to parse it as a constructor.
                    // We need a restore point in order to recover from Foo(bar())
                    RestorePoint rp{parser};
                    Declarator ctorDcor;
                    Owned<DeclProduction>* prodToModify = &ctorDcor.prod;
                    parseParameterList(parser, &prodToModify);
                    if (!rp.errorOccurred()) {
                        // It's a constructor
                        PLY_ASSERT(ctorDcor.prod && ctorDcor.prod->var.is<DeclProduction::Function>());
                        rp.cancel();
                        InitDeclarator& initDcor = entity.initDeclarators.append();
                        initDcor.prod = std::move(ctorDcor.prod);
                        PLY_ASSERT(ctorDcor.qid.isEmpty());
                        initDcor.qid = std::move(qid);
                        parseOptionalFunctionBody(parser, initDcor.init, entity);
                        return result;
                    }
                    // It failed to parse as a constructor. Treat this token as part of a
                    // entity type specifier instead.
                    rp.backtrack();
                }

                // In C++, all declarations must be explicitly typed; there is no "default
                // int". Therefore, this must be a entity type specifier.
                if (typename_.isValid() && qid.prefix.isEmpty()) {
                    Token firstToken = getFirstToken(qid);
                    parser->error(Error, firstToken.inputOffset,
                                  FMT_MSG("expected nested name prefix before '{}'", firstToken.toString()));
                }

                typeSpecifierIndex = entity.declSpecifiers.numItems();
                DeclSpecifier* declSpec = entity.declSpecifiers.append(Heap::create<DeclSpecifier>());
                auto& typeSpec = declSpec->var.switchTo<DeclSpecifier::TypeSpecifier>();
                typeSpec.elaborateKeyword = typename_;
                typeSpec.qid = std::move(qid);
            }
        } else {
            // Not an identifier. Parse the remainder as a declarator list (eg. may start with * or &).
            // Don't log an error if no type specifier was encountered yet, because the declarator may name a
            // destructor.
            break;
        }
    }

    // Parse init-declarators.
    parseInitDeclarators(parser, entity);

    bool isTypeDeclaration = false;
    for (const DeclSpecifier* declSpec : entity.declSpecifiers) {
        if (declSpec->var.is<DeclSpecifier::Class>() || declSpec->var.is<DeclSpecifier::Enum>()) {
            isTypeDeclaration = true;
            break;
        }
    }
    if ((savedErrorCount == parser->rawErrorCount) && entity.initDeclarators.isEmpty() && !isTypeDeclaration) {
        parser->errorNoMute(Error, startInputOffset, "declaration does not declare anything");
    }

    return result;
}

// Returns false if no input was read.
Declaration parseDeclarationInternal(ParserImpl* parser, StringView enclosingClassName) {
    Declaration result;
    Token token = peekToken(parser);

    if (token.type == Token::Identifier) {
        if (token.text == "extern") {
            // Possible linkage specification
            parser->muteErrors = false;
            RestorePoint rp{parser};

            Token token2 = readToken(parser);
            if (token2.type != Token::StringLiteral) {
                rp.backtrack();
                parseEntityDeclaration(parser, enclosingClassName);
            } else {
                Token token3 = readToken(parser);
                if (token3.type == Token::OpenCurly) {
                    // It's a linkage specification block, such as
                    //      extern "C" {
                    //          ...
                    //      }
                    rp.cancel();
                    auto& linkage = result.var.switchTo<Declaration::Linkage>();
                    linkage.externKeyword = token;
                    linkage.literal = token2;
                    linkage.openCurly = token3;
                    linkage.childDecls = parseDeclarationList(parser, &linkage.closeCurly, {});
                } else {
                    // It's a linkage specifier for the current declaration, such as
                    //      extern "C" void foo();
                    //      ^^^^^^^^^^
                    // FIXME: Make Declaration type for this
                    rp.backtrack();
                    parseEntityDeclaration(parser, enclosingClassName);
                }
            }
        } else if (token.text == "public" || token.text == "private" || token.text == "protected") {
            // Access specifier
            parser->tokenIndex++;
            parser->muteErrors = false;
            Token puncToken = peekToken(parser);
            if (puncToken.type == Token::SingleColon) {
                parser->tokenIndex++;
                auto& accessSpec = result.var.switchTo<Declaration::AccessSpecifier>();
                accessSpec.keyword = token;
                accessSpec.colon = puncToken;
            } else {
                // expected :
                parser->error(Error, puncToken.inputOffset, FMT_MSG("expected ':' before '{}'", puncToken.toString()));
            }
        } else if (token.text == "static_assert") {
            // static_assert
            parser->tokenIndex++;
            parser->muteErrors = false;
            Token puncToken = peekToken(parser);
            if (puncToken.type != Token::OpenParen) {
                // expected (
                parser->error(Error, puncToken.inputOffset, FMT_MSG("expected '(' before '{}'", puncToken.toString()));
            } else {
                parser->tokenIndex++;
                Token closeToken;
                bool continueNormally = skipAnyScope(parser, &closeToken, puncToken);
                if (continueNormally) {
                    auto& sa = result.var.switchTo<Declaration::StaticAssert>();
                    sa.keyword = token;
                    sa.openParen = puncToken;
                    sa.closeParen = closeToken;
                }
            }
        } else if (token.text == "namespace") {
            // namespace
            parser->tokenIndex++;
            parser->muteErrors = false;
            auto& ns = result.var.switchTo<Declaration::Namespace>();
            ns.keyword = token;

            Token token = peekToken(parser);
            if (token.type == Token::Identifier) {
                // FIXME: Ensure it's not a reserved word
                ns.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
                token = peekToken(parser);
            }

            if (token.type == Token::OpenCurly) {
                parser->tokenIndex++;
                ns.openCurly = token;
                ns.childDecls = parseDeclarationList(parser, &ns.closeCurly, {});
            } else {
                // expected {
                parser->error(Error, token.inputOffset, FMT_MSG("expected '{{' before '{}'", token.toString()));
            }
        } else if (token.text == "template") {
            // template
            parser->tokenIndex++;
            parser->muteErrors = false;
            auto& tmpl = result.var.switchTo<Declaration::Template>();
            tmpl.keyword = token;
            Token token2 = peekToken(parser);
            if (token2.type == Token::OpenAngle) {
                tmpl.openAngle = token2;
                parser->tokenIndex++;
                PLY_SET_IN_SCOPE(parser->tkr.config.tokenizeRightShift, false);
                parseParameterDeclarationList(parser, &tmpl.params, true);
                Token closeAngle = peekToken(parser);
                if (closeAngle.type == Token::CloseAngle) {
                    parser->tokenIndex++;
                    tmpl.closeAngle = closeAngle;
                }
            }
            tmpl.childDecl = Heap::create<Declaration>(parseDeclarationInternal(parser, enclosingClassName));
        } else if (token.text == "using") {
            // using directive or type alias
            parser->tokenIndex++;
            parser->muteErrors = false;
            Token token2 = readToken(parser);
            if (token2.type == Token::Identifier && token2.text == "namespace") {
                auto& usingDir = result.var.switchTo<Declaration::UsingNamespace>();
                usingDir.usingKeyword = token;
                usingDir.namespaceKeyword = token2;
                usingDir.qid = parseQualifiedId(parser, ParseQualifiedMode::RequireComplete);
            } else {
                auto& alias = result.var.switchTo<Declaration::TypeAlias>();
                alias.usingKeyword = token;
                alias.name = token2;

                Token equalToken = peekToken(parser);
                if (equalToken.type != Token::SingleEqual) {
                    // expected =
                    parser->error(Error, equalToken.inputOffset,
                                  FMT_MSG("expected '=' before '{}'", equalToken.toString()));
                } else {
                    parser->tokenIndex++;
                    alias.equals = equalToken;
                    alias.typeId = parseTypeId(parser);
                }
            }
        } else {
            result = parseEntityDeclaration(parser, enclosingClassName);
        }
    } else if (token.type == Token::Semicolon) {
        parser->tokenIndex++;
        /*
        Declaration::Empty empty;
        empty.semicolon = token;
        Declaration decl;
        decl.var = std::move(empty);
        addDeclarationToCurrentScope(parser, std::move(decl));
        */
    } else if (token.type == Token::Tilde) {
        result = parseEntityDeclaration(parser, enclosingClassName);
    } else {
        parser->tokenIndex++;
        parser->error(Error, token.inputOffset, FMT_MSG("expected declaration before '{}'", token.toString()));
    }
    return result;
}

Array<Declaration> parseDeclarationList(ParserImpl* parser, Token* outCloseCurly, StringView enclosingClassName) {
    // Always handle close curly at this scope, even if it's file scope:
    SetAcceptFlagsInScope acceptScope{parser, Token::OpenCurly};
    Array<Declaration> result;

    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::EOF) {
            if (outCloseCurly) {
                parser->error(Error, token.inputOffset, FMT_MSG("expected '}' before '{}'", token.toString()));
            }
            break;
        } else if (token.type == Token::CloseCurly) {
            parser->tokenIndex++;
            if (outCloseCurly) {
                *outCloseCurly = token;
                break;
            }
            parser->error(Error, token.inputOffset, FMT_MSG("expected declaration before '{}'", token.toString()));
            continue;
        }

        result.append(parseDeclarationInternal(parser, enclosingClassName));

        bool semicolonRequired = true;
        if (auto* entity = result.back().var.as<Declaration::Entity>()) {
            if (entity->initDeclarators.numItems() > 0) {
                semicolonRequired = !entity->initDeclarators.back().init.var.is<Initializer::FunctionBody>();
            }
        }

        Token semicolon = peekToken(parser);
        if (semicolon.type == Token::Semicolon) {
            parser->tokenIndex++;
            parser->muteErrors = false;
        } else if (semicolonRequired) {
            parser->error(Error, semicolon.inputOffset, FMT_MSG("expected ';' before '{}'", semicolon.toString()));
        }
    }
    return result;
}

Array<Declaration> parseTranslationUnit(ParserImpl* parser) {
    Array<Declaration> result = parseDeclarationList(parser, nullptr, {});
    Token eofTok = peekToken(parser);
    PLY_ASSERT(eofTok.type == Token::EOF); // EOF is the only possible token here
    PLY_UNUSED(eofTok);
    return result;
}

//  ▄▄▄▄▄                       ▄▄
//  ██  ██  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀▀   ▄▄▄██ ██  ▀▀ ▀█▄▄▄  ██ ██  ██ ██  ██
//  ██     ▀█▄▄██ ██      ▄▄▄█▀ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀
//  ▄▄▄▄▄                                           ▄▄
//  ██    ▄▄  ▄▄ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄
//  ██▀▀   ▀██▀  ██  ██ ██  ▀▀ ██▄▄██ ▀█▄▄▄  ▀█▄▄▄  ██ ██  ██ ██  ██ ▀█▄▄▄
//  ██▄▄▄ ▄█▀▀█▄ ██▄▄█▀ ██     ▀█▄▄▄   ▄▄▄█▀  ▄▄▄█▀ ██ ▀█▄▄█▀ ██  ██  ▄▄▄█▀
//               ██

void consumeSpecifier(ParserImpl* parser) {
    for (;;) {
        Token token = peekToken(parser);
        if (token.type == Token::OpenAngle) {
            // Template type
            // FIXME: Does < always indicate a template type here?
            // FIXME: This needs to handle "Tmpl<(2 > 1)>" and ""Tmpl<(2 >> 1)>"
            parser->tokenIndex++;
            PLY_SET_IN_SCOPE(parser->tkr.config.tokenizeRightShift, false);
            Token closeToken;
            skipAnyScope(parser, &closeToken, token);
            token = peekToken(parser);
        }
        if (token.type == Token::DoubleColon) {
            parser->tokenIndex++;
            Token specToken = peekToken(parser);
            if (specToken.type == Token::Identifier) {
                parser->tokenIndex++;
            } else {
                // expected identifier after ::
                parser->error(Error, specToken.inputOffset,
                              FMT_MSG("expected identifier before '{}'", specToken.toString()));
                return;
            }
        } else
            return;
    }
}

void parseCaptureList(ParserImpl* parser) {
    Token token = readToken(parser);
    if (token.type != Token::CloseSquare) {
        // FIXME: accept an actual capture list instead of just an empty list
        parser->error(Error, token.inputOffset, FMT_MSG("expected ']' before '{}'", token.toString()));
    }
}

// FIXME: This needs work.
// It's enough to parse the initializers used by Plywood, but there are definitely lots of
// expressions it doesn't handle.
ParsedExpression parseExpression(ParserImpl* parser, bool optional) {
    Token startToken = readToken(parser);
    Token endToken;
    switch (startToken.type) {
        case Token::Identifier: {
            // FIXME: This should use parseQualifiedId instead
            consumeSpecifier(parser);
            Token token2 = peekToken(parser);
            if (token2.type == Token::OpenParen) {
                // Function arguments
                parser->tokenIndex++;
                SetAcceptFlagsInScope acceptScope{parser, Token::OpenParen};
                for (;;) {
                    Token token3 = peekToken(parser);
                    if (token3.type == Token::CloseParen) {
                        parser->tokenIndex++;
                        endToken = token3;
                        break; // end of arguments
                    } else {
                        parseExpression(parser);
                        Token token4 = readToken(parser);
                        if (token4.type == Token::Comma) {
                        } else if (token4.type == Token::CloseParen) {
                            endToken = token4;
                            break; // end of arguments
                        } else {
                            // expected , or ) after argument
                            parser->error(Error, token4.inputOffset,
                                          FMT_MSG("expected ',' or ')' before '{}'", token4.toString()));
                            if (!handleUnexpectedToken(parser, nullptr, token4))
                                break;
                        }
                    }
                }
            } else if (token2.type == Token::OpenCurly) {
                // It's a braced initializer (list).
                // FIXME: Not sure, but maybe this case should use a "low priority" curly (???)
                // Because if ';' is encountered, we should perhaps end the outer declaration.
                // And if an outer ) is matched, it should maybe cancel the initializer.
                // However, if we do that, it will be inconsisent with the behavior of
                // skipAnyScope(). Does that matter?
                parser->tokenIndex++;
                SetAcceptFlagsInScope acceptScope{parser, Token::OpenCurly};
                for (;;) {
                    Token token3 = peekToken(parser);
                    if (token3.type == Token::CloseCurly) {
                        parser->tokenIndex++;
                        endToken = token3;
                        break; // end of arguments
                    } else {
                        parseExpression(parser);
                        Token token4 = readToken(parser);
                        if (token4.type == Token::Comma) {
                        } else if (token4.type == Token::CloseCurly) {
                            endToken = token4;
                            break; // end of arguments
                        } else {
                            // expected , or } after argument
                            parser->error(Error, token4.inputOffset,
                                          FMT_MSG("expected ',' or '}}' before '{}'", token4.toString()));
                            if (!handleUnexpectedToken(parser, nullptr, token4))
                                break;
                        }
                    }
                }
            } else {
                // Can't consume any more of expression
                endToken = startToken;
            }
            break;
        }

        case Token::NumericLiteral: {
            // Consume it
            endToken = startToken;
            break;
        }

        case Token::StringLiteral: {
            endToken = startToken;
            for (;;) {
                // concatenate multiple string literals
                Token token = peekToken(parser);
                if (token.type != Token::StringLiteral)
                    break;
                parser->tokenIndex++;
                endToken = token;
            }
            break;
        }

        case Token::OpenParen: {
            SetAcceptFlagsInScope acceptScope{parser, Token::OpenParen};
            parseExpression(parser);
            Token token2 = peekToken(parser);
            if (token2.type == Token::CloseParen) {
                // Treat as a C-style cast.
                // FIXME: This should only be done if the inner expression identifies a type!
                // Otherwise, it's just a parenthesized expression:
                parser->tokenIndex++;
                endToken = parseExpression(parser, true).endToken;
            } else {
                // expected ) after expression
                Token closeParen;
                closeScope(parser, &closeParen, startToken); // This will log an error
                endToken = closeParen;
            }
            break;
        }

        case Token::OpenCurly: {
            for (;;) {
                Token token2 = peekToken(parser);
                if (token2.type == Token::CloseCurly) {
                    parser->tokenIndex++;
                    endToken = token2;
                    break;
                } else {
                    parseExpression(parser);
                    Token token4 = readToken(parser);
                    if (token4.type == Token::Comma) {
                    } else if (token4.type == Token::CloseCurly) {
                        endToken = token4;
                        break; // end of braced initializer
                    } else {
                        // expected , or } after expression
                        parser->error(Error, token4.inputOffset,
                                      FMT_MSG("expected ',' or '}}' before '{}'", token4.toString()));
                        if (!handleUnexpectedToken(parser, nullptr, token4))
                            break;
                    }
                }
            }
            break;
        }

        case Token::Bang:
        case Token::SingleAmpersand:
        case Token::SingleMinus: {
            endToken = parseExpression(parser).endToken;
            break;
        }

        case Token::OpenSquare: {
            // lambda expression
            parseCaptureList(parser);
            Token openParen = peekToken(parser);
            if (openParen.type == Token::OpenParen) {
                parser->tokenIndex++;
                Array<Parameter> unusedParams;
                parseParameterDeclarationList(parser, &unusedParams, false);
                Token closeParen = peekToken(parser);
                if (closeParen.type == Token::CloseParen) {
                    parser->tokenIndex++;
                }
            } else {
                parser->error(Error, openParen.inputOffset, FMT_MSG("expected '(' before '{}'", openParen.toString()));
            }
            Token token2 = peekToken(parser);
            if (token2.type == Token::Arrow) {
                parser->tokenIndex++;
                Declaration::Entity entity;
                parseTypeId(parser);
                token2 = peekToken(parser);
            }
            if (token2.type != Token::OpenCurly) {
                parser->error(Error, token2.inputOffset, FMT_MSG("expected '{{' before '{}'", token2.toString()));
            } else {
                parser->tokenIndex++;
                Token closeToken;
                skipAnyScope(parser, &closeToken, token2);
                endToken = closeToken;
            }
            break;
        }

        default: {
            if (optional) {
                parser->tokenIndex--;
            } else {
                PLY_ASSERT(0);
            }
            return {};
        }
    }

    Token token = peekToken(parser);
    switch (token.type) {
        case Token::CloseAngle: {
            if (!parser->tkr.config.tokenizeRightShift) {
                break;
            } else {
                parser->tokenIndex++;
                endToken = parseExpression(parser).endToken;
            }
        };

        case Token::SingleVerticalBar:
        case Token::DoubleEqual:
        case Token::NotEqual:
        case Token::OpenAngle:
        case Token::LessThanOrEqual:
        case Token::GreaterThanOrEqual:
        case Token::LeftShift:
        case Token::RightShift:
        case Token::SinglePlus:
        case Token::SingleMinus:
        case Token::Percent:
        case Token::Arrow:
        case Token::Star:
        case Token::Dot:
        case Token::ForwardSlash: {
            parser->tokenIndex++;
            endToken = parseExpression(parser).endToken;
            break;
        }

        case Token::QuestionMark: {
            parser->tokenIndex++;
            parseExpression(parser);
            token = peekToken(parser);
            if (token.type != Token::SingleColon) {
                // expected : after expression
                // FIXME: It would be cool the mention, in the error message, that the colon is
                // needed to match the '?' that was encountered earlier
                parser->error(Error, token.inputOffset, FMT_MSG("expected ':' before '{}'", token.toString()));
            } else {
                parser->tokenIndex++;
                endToken = parseExpression(parser).endToken;
            }
            break;
        };

        default:
            break;
    }
    return {startToken, endToken};
}

//  ▄▄▄▄▄         ▄▄     ▄▄▄  ▄▄            ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄
//  ██  ██ ▄▄  ▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄     ██  ██ ██  ██  ██
//  ██▀▀▀  ██  ██ ██  ██  ██  ██ ██        ██▀▀██ ██▀▀▀   ██
//  ██     ▀█▄▄██ ██▄▄█▀ ▄██▄ ██ ▀█▄▄▄     ██  ██ ██     ▄██▄
//

Owned<Parser> Parser::create() {
    return Heap::create<ParserImpl>();
}

void Parser::destroy() {
    Heap::destroy(static_cast<ParserImpl*>(this));
}

void setInput(ParserImpl* parser, StringView absPath, StringView contents) {
    Preprocessor::File& file = parser->pp.files.append();
    file.absPath = absPath;
    file.contents = contents;
    file.tokenLocMap = TokenLocationMap::createFromString(contents);

    parser->pp.inputRanges.insert({});

    Preprocessor::IncludedItem& item = parser->pp.includeStack.append();
    item.vin = ViewStream{file.contents};
}

void applyPreprocessorDefinitions(ParserImpl* parser) {
    for (PreprocessorDefinition& def : parser->predefinedDefs) {
        // Add to macroMap.
        u32 macroIdx = parser->pp.macros.numItems();
        PLY_ASSERT(!parser->pp.macroMap.find(def.name)); // Adding twice is probably a mistake.
        *parser->pp.macroMap.insert(def.name).value = macroIdx;

        // Add to macros.
        Preprocessor::Macro& macro = parser->pp.macros.append();
        macro.name = def.name;
        macro.expansion = def.expansion;
    }
}

PreprocessResult Parser::preprocess(StringView absPath, StringView src) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    setInput(parser, absPath, src);
    applyPreprocessorDefinitions(parser);
    parser->isOnlyPreprocessing = true;

    MemStream mem;
    for (;;) {
        Token token = readToken(parser);
        if (token.type == Token::EOF) {
            break;
        }
        mem.write(token.toString());
    }

    PreprocessResult result;
    result.output = mem.moveToString();
    result.success = parser->success;
    result.diagnostics = std::move(parser->diagnostics);
    return result;
}

ParseResult Parser::parseFile(StringView absPath, StringView src) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    setInput(parser, absPath, src);
    applyPreprocessorDefinitions(parser);

    ParseResult result;
    result.declarations = parseTranslationUnit(parser);
    result.success = parser->success;
    result.diagnostics = std::move(parser->diagnostics);
    return result;
}

Declaration Parser::parseDeclaration(StringView input, StringView enclosingClassName) {
    ParserImpl* parser = static_cast<ParserImpl*>(this);
    setInput(parser, {}, input);
    applyPreprocessorDefinitions(parser);
    return parseDeclarationInternal(parser, enclosingClassName);
}

FileLocation Parser::getFileLocation(u32 inputOffset) const {
    const ParserImpl* parser = static_cast<const ParserImpl*>(this);
    return cpp::getFileLocation(&parser->pp, inputOffset);
}

//   ▄▄▄▄                 ▄▄
//  ██  ▀▀ ▄▄  ▄▄ ▄▄▄▄▄  ▄██▄▄  ▄▄▄▄  ▄▄  ▄▄
//   ▀▀▀█▄ ██  ██ ██  ██  ██    ▄▄▄██  ▀██▀
//  ▀█▄▄█▀ ▀█▄▄██ ██  ██  ▀█▄▄ ▀█▄▄██ ▄█▀▀█▄
//          ▄▄▄█▀
//  ▄▄  ▄▄ ▄▄        ▄▄     ▄▄▄  ▄▄        ▄▄      ▄▄   ▄▄
//  ██  ██ ▄▄  ▄▄▄▄▄ ██▄▄▄   ██  ▄▄  ▄▄▄▄▄ ██▄▄▄  ▄██▄▄ ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██ ██ ██  ██ ██  ██  ██  ██ ██  ██ ██  ██  ██   ██ ██  ██ ██  ██
//  ██  ██ ██ ▀█▄▄██ ██  ██ ▄██▄ ██ ▀█▄▄██ ██  ██  ▀█▄▄ ██ ██  ██ ▀█▄▄██
//             ▄▄▄█▀                 ▄▄▄█▀                         ▄▄▄█▀

struct NodeVisitor {
    const ParserImpl* parser = nullptr;
    Array<TokenSpan> spans;
    const QualifiedID* insideQid = nullptr;
    bool needsSpace = false;

    void append(TokenSpan::Color color, const Token& token) {
        TokenSpan& span = this->spans.append();
        span.color = color;
        span.token = token;
        span.qid = insideQid;
    }
    void appendSpace() {
        TokenSpan& span = this->spans.append();
        span.isSpace = true;
        span.qid = insideQid;
        this->needsSpace = false;
    }
};

void syntaxHighlightDeclSpecifiers(NodeVisitor* visitor, ArrayView<const Owned<DeclSpecifier>> declSpecifiers);
void syntaxHighlightDeclarator(NodeVisitor* visitor, Variant<const QualifiedID*, const Token*> name,
                               const DeclProduction* prod);

void syntaxHighlightQid(NodeVisitor* visitor, TokenSpan::Color color, const QualifiedID& qid) {
    PLY_SET_IN_SCOPE(visitor->insideQid, &qid);
    for (const QualifiedID::Prefix& p : qid.prefix) {
        if (auto* ident = p.var.as<QualifiedID::Identifier>()) {
            visitor->append(TokenSpan::Type, ident->name);
        } else if (auto* tmplId = p.var.as<QualifiedID::TemplateID>()) {
            visitor->append(TokenSpan::Type, tmplId->name);
            visitor->append(TokenSpan::None, tmplId->openAngle);
            visitor->needsSpace = false;
            for (const QualifiedID::TemplateID::Arg& arg : tmplId->args) {
                if (auto* typeId = arg.var.as<TypeID>()) {
                    syntaxHighlightDeclSpecifiers(visitor, typeId->declSpecifiers);
                    syntaxHighlightDeclarator(visitor, {}, typeId->abstractDcor);
                }
            }
            visitor->append(TokenSpan::None, tmplId->closeAngle);
        } else {
            PLY_ASSERT(0); // Not supported yet
        }
        if (p.doubleColon.isValid()) {
            visitor->append(TokenSpan::None, p.doubleColon);
        }
    }

    if (auto* ident = qid.var.as<QualifiedID::Identifier>()) {
        visitor->append(color, ident->name);
    } else if (auto* tmplId = qid.var.as<QualifiedID::TemplateID>()) {
        visitor->append(color, tmplId->name);
        visitor->append(TokenSpan::None, tmplId->openAngle);
        visitor->needsSpace = false;
        for (const QualifiedID::TemplateID::Arg& arg : tmplId->args) {
            if (auto* typeId = arg.var.as<TypeID>()) {
                syntaxHighlightDeclSpecifiers(visitor, typeId->declSpecifiers);
                syntaxHighlightDeclarator(visitor, {}, typeId->abstractDcor);
            }
        }
        visitor->append(TokenSpan::None, tmplId->closeAngle);
    } else if (auto* dtor = qid.var.as<QualifiedID::Destructor>()) {
        visitor->append(color, dtor->tilde);
        visitor->append(color, dtor->name);
    } else if (auto* opFunc = qid.var.as<QualifiedID::OperatorFunc>()) {
        visitor->append(color, opFunc->keyword);
        visitor->append(color, opFunc->punc);
        if (opFunc->punc2.isValid()) {
            visitor->append(color, opFunc->punc2);
        }
    } else if (auto* convFunc = qid.var.as<QualifiedID::ConversionFunc>()) {
        visitor->append(color, convFunc->operatorKeyword);
        visitor->needsSpace = true;
        syntaxHighlightDeclSpecifiers(visitor, convFunc->declSpecifiers);
        syntaxHighlightDeclarator(visitor, {}, convFunc->abstractDcor);
    } else {
        PLY_ASSERT(0); // Not supported yet
    }
}

void syntaxHighlightDeclSpecifiers(NodeVisitor* visitor, ArrayView<const Owned<DeclSpecifier>> declSpecifiers) {
    for (const DeclSpecifier* declSpec : declSpecifiers) {
        if (visitor->needsSpace) {
            visitor->appendSpace();
        }
        if (auto* keyword = declSpec->var.as<DeclSpecifier::Keyword>()) {
            visitor->append(TokenSpan::None, keyword->token);
        } else if (auto* typeId = declSpec->var.as<DeclSpecifier::TypeSpecifier>()) {
            if (typeId->elaborateKeyword.isValid()) {
                visitor->append(TokenSpan::None, typeId->elaborateKeyword);
            }
            syntaxHighlightQid(visitor, TokenSpan::Type, typeId->qid);
        } else if (auto* typeParam = declSpec->var.as<DeclSpecifier::TypeParameter>()) {
            visitor->append(TokenSpan::None, typeParam->keyword);
            if (typeParam->ellipsis.isValid()) {
                visitor->append(TokenSpan::None, typeParam->ellipsis);
            }
        }
        visitor->needsSpace = true;
    }
}

void syntaxHighlightDeclarator(NodeVisitor* visitor, Variant<const QualifiedID*, const Token*> name,
                               const DeclProduction* prod) {
    // First, flatten the chain.
    // FIXME: We should really do this at parse time.
    Array<const DeclProduction*> prodChain;
    for (const DeclProduction* p = prod; p; p = p->child) {
        prodChain.append(p);
    }

    // Next, create parentheses groups.
    struct ParenGroup {
        u32 first;
        u32 leading;
        u32 last;
    };
    Array<ParenGroup> parenGroups;
    {
        u32 first = 0;
        s32 trailing = -1;
        for (u32 i = 0; i < prodChain.numItems(); i++) {
            if (prodChain[i]->var.is<DeclProduction::ArrayOf>() || prodChain[i]->var.is<DeclProduction::Function>()) {
                trailing = i;
            }
            if (prodChain[i]->var.is<DeclProduction::Parenthesized>()) {
                return; // FIXME
                parenGroups.append({first, (u32) (trailing + 1), i});
                first = i + 1;
                trailing = first;
            }
        }
        parenGroups.append({first, (u32) (trailing + 1), prodChain.numItems()});
    }

    // Visit leading productions of each group.
    for (s32 g = parenGroups.numItems() - 1; g >= 0; g--) {
        const ParenGroup& group = parenGroups[g];
        for (s32 i = group.last - 1; i >= (s32) group.leading; i--) {
            if (auto* indirect = prodChain[i]->var.as<DeclProduction::Indirection>()) {
                visitor->append(TokenSpan::None, indirect->punc);
            } else if (auto* qualifier = prodChain[i]->var.as<DeclProduction::Qualifier>()) {
                if (visitor->needsSpace) {
                    visitor->appendSpace();
                }
                visitor->append(TokenSpan::None, qualifier->keyword);
                visitor->needsSpace = true;
            } else {
                PLY_ASSERT(0);
            }
        }
        if (g > 0) {
            // Open parenthesis
            PLY_ASSERT(group.first > 0);
            auto* paren = prodChain[group.first - 1]->var.as<DeclProduction::Parenthesized>();
            if (visitor->needsSpace) {
                visitor->appendSpace();
            }
            visitor->append(TokenSpan::None, paren->openParen);
            visitor->needsSpace = false;
        }
    }

    // Visit qualified-id.
    if (const Token** token = name.as<const Token*>()) {
        if ((**token).isValid()) {
            if (visitor->needsSpace) {
                visitor->appendSpace();
            }
            visitor->append(TokenSpan::Variable, **token);
            visitor->needsSpace = true;
        }
    } else if (const QualifiedID** qid = name.as<const QualifiedID*>()) {
        if (visitor->needsSpace) {
            visitor->appendSpace();
        }
        syntaxHighlightQid(visitor, TokenSpan::Symbol, **qid);
        visitor->needsSpace = true;
    }

    // Visit trailing productions of each group.
    for (u32 g = 0; g < parenGroups.numItems(); g++) {
        const ParenGroup& group = parenGroups[g];
        for (u32 i = group.first; i < group.leading; i++) {
            if (auto* arrayOf = prodChain[i]->var.as<DeclProduction::ArrayOf>()) {
                visitor->append(TokenSpan::None, arrayOf->openSquare);
                visitor->append(TokenSpan::None, arrayOf->closeSquare);
                visitor->needsSpace = false;
            } else if (auto* function = prodChain[i]->var.as<DeclProduction::Function>()) {
                visitor->append(TokenSpan::None, function->openParen);
                visitor->needsSpace = false;
                // Visit function parameters.
                for (const Parameter& param : function->params) {
                    syntaxHighlightDeclSpecifiers(visitor, param.declSpecifiers);
                    syntaxHighlightDeclarator(visitor, &param.identifier, param.prod);
                    if (param.comma.isValid()) {
                        visitor->append(TokenSpan::None, param.comma);
                        visitor->appendSpace();
                    }
                }
                visitor->append(TokenSpan::None, function->closeParen);
                for (const Token& token : function->qualifiers) {
                    visitor->appendSpace();
                    visitor->append(TokenSpan::None, token);
                }
                visitor->needsSpace = true;
            } else {
                PLY_ASSERT(0);
            }
        }
        if (g + 1 < parenGroups.numItems()) {
            // Close parenthesis
            auto* paren = prodChain[group.last]->var.as<DeclProduction::Parenthesized>();
            visitor->append(TokenSpan::None, paren->closeParen);
            visitor->needsSpace = true;
        }
    }
}

void syntaxHighlightInitializer(NodeVisitor* visitor, const Initializer& init) {
    if (init.var.as<Initializer::Assignment>()) {
        // Not supported yet
    } else if (init.var.as<Initializer::FunctionBody>()) {
        // Not supported yet
    } else if (init.var.as<Initializer::BitField>()) {
        // Not supported yet
    }
}

void syntaxHighlightDeclaration(NodeVisitor* visitor, const Declaration& decl) {
    if (auto* entity = decl.var.as<Declaration::Entity>()) {
        syntaxHighlightDeclSpecifiers(visitor, entity->declSpecifiers);
        for (const InitDeclarator& initDecl : entity->initDeclarators) {
            syntaxHighlightDeclarator(visitor, &initDecl.qid, initDecl.prod);
            syntaxHighlightInitializer(visitor, initDecl.init);
            if (initDecl.comma.isValid()) {
                visitor->append(TokenSpan::None, initDecl.comma);
                visitor->appendSpace();
            }
        }
    } else if (auto* tmpl = decl.var.as<Declaration::Template>()) {
        visitor->append(TokenSpan::None, tmpl->keyword);
        visitor->appendSpace();
        visitor->append(TokenSpan::None, tmpl->openAngle);
        visitor->needsSpace = false;
        for (const Parameter& param : tmpl->params) {
            syntaxHighlightDeclSpecifiers(visitor, param.declSpecifiers);
            syntaxHighlightDeclarator(visitor, &param.identifier, param.prod);
            if (param.comma.isValid()) {
                visitor->append(TokenSpan::None, param.comma);
                visitor->appendSpace();
            }
        }
        visitor->append(TokenSpan::None, tmpl->closeAngle);
        visitor->needsSpace = true;
        syntaxHighlightDeclaration(visitor, *tmpl->childDecl);
    }
}

Array<TokenSpan> Parser::syntaxHighlight(const Declaration& decl) const {
    NodeVisitor visitor;
    visitor.parser = static_cast<const ParserImpl*>(this);
    syntaxHighlightDeclaration(&visitor, decl);
    return std::move(visitor.spans);
}

//  ▄▄▄▄▄         ▄▄                        ▄▄▄▄          ▄▄                  ▄▄
//  ██  ██  ▄▄▄▄  ██▄▄▄  ▄▄  ▄▄  ▄▄▄▄▄     ██  ██ ▄▄  ▄▄ ▄██▄▄ ▄▄▄▄▄  ▄▄  ▄▄ ▄██▄▄
//  ██  ██ ██▄▄██ ██  ██ ██  ██ ██  ██     ██  ██ ██  ██  ██   ██  ██ ██  ██  ██
//  ██▄▄█▀ ▀█▄▄▄  ██▄▄█▀ ▀█▄▄██ ▀█▄▄██     ▀█▄▄█▀ ▀█▄▄██  ▀█▄▄ ██▄▄█▀ ▀█▄▄██  ▀█▄▄
//                               ▄▄▄█▀                         ██

struct DumpContext {
    Stream* out = nullptr;
    const ParserImpl* parser = nullptr;
    u32 indentLevel = 0;

    String indent() const {
        return StringView{"  "} * this->indentLevel;
    }
};

void dumpDeclaration(DumpContext& ctx, const Declaration& decl);
void dumpExpression(DumpContext& ctx, const Expression* expr);
void dumpStatement(DumpContext& ctx, const Statement& stmt);

void dumpDeclSpecifier(DumpContext& ctx, const DeclSpecifier& declSpec) {
    using Var = decltype(declSpec.var);
    switch (declSpec.var.getSubtypeIndex()) {
        case Var::indexOf<DeclSpecifier::Keyword>: {
            auto* keyword = declSpec.var.as<DeclSpecifier::Keyword>();
            ctx.out->format("{}Keyword '{}'\n", ctx.indent(), keyword->token.text);
            break;
        }
        case Var::indexOf<DeclSpecifier::Linkage>: {
            auto* langLinkage = declSpec.var.as<DeclSpecifier::Linkage>();
            ctx.out->format("{}Linkage '{}'\n", ctx.indent(), langLinkage->literal.text);
            break;
        }
        case Var::indexOf<DeclSpecifier::Class>: {
            auto* class_ = declSpec.var.as<DeclSpecifier::Class>();
            ctx.out->format("{}Class {} '{}'\n", ctx.indent(), class_->keyword.text, toString(class_->qid));
            if (class_->virtSpecifiers.numItems() > 0) {
                ctx.out->format("{}  virt_specifiers:", ctx.indent());
                for (const Token& virtSpec : class_->virtSpecifiers) {
                    ctx.out->format(" {}", virtSpec.text);
                }
                ctx.out->write("\n");
            }
            if (class_->baseSpecifiers.numItems() > 0) {
                ctx.out->format("{}  base_specifiers:", ctx.indent());
                StringView comma;
                for (const DeclSpecifier::Class::BaseSpecifier& baseSpec : class_->baseSpecifiers) {
                    ctx.out->format("{} {} {}", comma, baseSpec.accessSpec.text, toString(baseSpec.baseQid));
                    comma = ",";
                }
                ctx.out->write("\n");
            }
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            for (const Declaration& decl : class_->memberDecls) {
                dumpDeclaration(ctx, decl);
            }
            break;
        }
        case Var::indexOf<DeclSpecifier::Enum>: {
            auto* enum_ = declSpec.var.as<DeclSpecifier::Enum>();
            ctx.out->format("{}Enum{}{} '{}'\n", ctx.indent(), enum_->classKeyword.isValid() ? " " : "",
                            enum_->classKeyword.text, toString(enum_->qid));
            if (!enum_->base.isEmpty()) {
                ctx.out->format("{}  base: '{}'\n", ctx.indent(), toString(enum_->base));
            }
            for (const DeclSpecifier::Enum::Item& enor : enum_->enumerators) {
                ctx.out->format("{}  '{}'\n", ctx.indent(), enor.text.text);
                PLY_ASSERT(enor.init.var.isEmpty()); // Not supported yet
            }
            break;
        }
        case Var::indexOf<DeclSpecifier::TypeSpecifier>: {
            auto* typeSpec = declSpec.var.as<DeclSpecifier::TypeSpecifier>();
            ctx.out->format("{}TypeSpecifier '{}'\n", ctx.indent(), toString(typeSpec->qid));
            break;
        }
        default: {
            PLY_ASSERT(0); // Not supported yet
            break;
        }
    }
}

void dumpDeclaratorProduction(DumpContext& ctx, const DeclProduction* prod) {
    if (!prod)
        return;

    using Var = decltype(prod->var);
    switch (prod->var.getSubtypeIndex()) {
        case Var::indexOf<DeclProduction::Parenthesized>: {
            ctx.out->format("{}Parenthesized\n", ctx.indent());
            break;
        }
        case Var::indexOf<DeclProduction::Indirection>: {
            auto* pointerTo = prod->var.as<DeclProduction::Indirection>();
            ctx.out->format("{}Indirection ", ctx.indent());
            PLY_ASSERT(pointerTo->prefix.isEmpty()); // Not supported yet
            ctx.out->format("'{}'\n", pointerTo->punc.text);
            break;
        }
        case Var::indexOf<DeclProduction::ArrayOf>: {
            // auto* arrayOf = prod->var.as<DeclProduction::ArrayOf>();
            ctx.out->format("{}ArrayOf\n", ctx.indent());
            // FIXME: dump size
            break;
        }
        case Var::indexOf<DeclProduction::Function>: {
            auto* function = prod->var.as<DeclProduction::Function>();
            ctx.out->format("{}Function\n", ctx.indent());
            if (!function->params.isEmpty()) {
                PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
                for (const Parameter& param : function->params) {
                    ctx.out->format("{}Parameter '{}'\n", ctx.indent(), param.identifier.text);
                    PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
                    for (const DeclSpecifier* declSpec : param.declSpecifiers) {
                        dumpDeclSpecifier(ctx, *declSpec);
                    }
                    dumpDeclaratorProduction(ctx, param.prod);
                    PLY_ASSERT(param.init.var.isEmpty()); // Not supported yet
                }
            }
            //          PLY_ASSERT(function->qualifiers.tokens.isEmpty()); // Not supported yet
            // PLY_ASSERT(isEmpty(function->trailingRetType)); // Not supported yet
            break;
        }
        case Var::indexOf<DeclProduction::Qualifier>: {
            auto* qualifier = prod->var.as<DeclProduction::Qualifier>();
            ctx.out->format("{}Qualifier '{}'\n", ctx.indent(), qualifier->keyword.text);
            break;
        }
        default: {
            PLY_ASSERT(0); // Invalid
            break;
        }
    }
    PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
    dumpDeclaratorProduction(ctx, prod->child);
}

void dumpInitDeclarator(DumpContext& ctx, const InitDeclarator& initDecl) {
    ctx.out->format("{}InitDeclarator '{}'\n", ctx.indent(), toString(initDecl.qid));
    {
        PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
        dumpDeclaratorProduction(ctx, initDecl.prod);
    }
    using Var = decltype(initDecl.init.var);
    switch (initDecl.init.var.getSubtypeIndex()) {
        case 0: {
            break; // Empty
        }
        case Var::indexOf<Initializer::Assignment>: {
            auto* assignment = initDecl.init.var.as<Initializer::Assignment>();
            if (auto* expression = assignment->var.as<Owned<Expression>>()) {
                ctx.out->format("{}Assignment (expression)\n", ctx.indent());
                PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
                dumpExpression(ctx, expression->get());
            } else if (auto* typeId = assignment->var.as<TypeID>()) {
                ctx.out->format("{}Assignment (type_id)\n", ctx.indent());
                PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
                for (const DeclSpecifier* declSpec : typeId->declSpecifiers) {
                    dumpDeclSpecifier(ctx, *declSpec);
                }
                dumpDeclaratorProduction(ctx, typeId->abstractDcor);
            } else {
                PLY_ASSERT(0);
            }
            break;
        }
        case Var::indexOf<Initializer::FunctionBody>: {
            auto* functionBody = initDecl.init.var.as<Initializer::FunctionBody>();
            ctx.out->format("{}FunctionBody\n", ctx.indent());
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            for (const Initializer::FunctionBody::MemberInitializer& memberInit : functionBody->memberInits) {
                ctx.out->format("{}MemberInitializer '{}'\n", ctx.indent(), toString(memberInit.qid));
                PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
                dumpExpression(ctx, memberInit.expr);
            }
            for (const Statement& statement : functionBody->statements) {
                dumpStatement(ctx, statement);
            }
            break;
        }
        case Var::indexOf<Initializer::BitField>: {
            auto* bitField = initDecl.init.var.as<Initializer::BitField>();
            ctx.out->format("{}BitField\n", ctx.indent());
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            dumpExpression(ctx, bitField->expr);
            break;
        }
        default: {
            PLY_ASSERT(0); // Invalid
            break;
        }
    }
}

void dumpDeclaration(DumpContext& ctx, const Declaration& decl) {
    auto formatLoc = [&](const Token& token) {
        FileLocation fileLoc = ctx.parser->getFileLocation(token.inputOffset);
        return String::format("{}({})", splitPath(fileLoc.absPath).filename, fileLoc.line);
    };
    using Var = decltype(decl.var);
    switch (decl.var.getSubtypeIndex()) {
        case Var::indexOf<Declaration::Linkage>: {
            auto* linkage = decl.var.as<Declaration::Linkage>();
            ctx.out->format("{}{}: Linkage '{}'\n", ctx.indent(), formatLoc(linkage->externKeyword),
                            linkage->literal.text);
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            for (const Declaration& decl : linkage->childDecls) {
                dumpDeclaration(ctx, decl);
            }
            break;
        }
        case Var::indexOf<Declaration::Namespace>: {
            auto* ns = decl.var.as<Declaration::Namespace>();
            ctx.out->format("{}{}: Namespace '{}'\n", ctx.indent(), formatLoc(ns->keyword), toString(ns->qid));
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            for (const Declaration& decl : ns->childDecls) {
                dumpDeclaration(ctx, decl);
            }
            break;
        }
        case Var::indexOf<Declaration::Entity>: {
            auto* entity = decl.var.as<Declaration::Entity>();
            ctx.out->format("{}{}: Entity\n", ctx.indent(), formatLoc(getFirstToken(*entity)));
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            for (const DeclSpecifier* declSpec : entity->declSpecifiers) {
                dumpDeclSpecifier(ctx, *declSpec);
            }
            for (const InitDeclarator& initDecl : entity->initDeclarators) {
                dumpInitDeclarator(ctx, initDecl);
            }
            break;
        }
        case Var::indexOf<Declaration::Template>: {
            auto* tmpl = decl.var.as<Declaration::Template>();
            ctx.out->format("{}{}: Template'\n", ctx.indent(), formatLoc(tmpl->keyword));
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            dumpDeclaration(ctx, *tmpl->childDecl);
            break;
        }
        case Var::indexOf<Declaration::TypeAlias>: {
            auto* alias = decl.var.as<Declaration::TypeAlias>();
            ctx.out->format("{}{}: TypeAlias '{}'\n", ctx.indent(), formatLoc(alias->usingKeyword), alias->name.text);
            PLY_SET_IN_SCOPE(ctx.indentLevel, ctx.indentLevel + 1);
            for (const DeclSpecifier* declSpec : alias->typeId.declSpecifiers) {
                dumpDeclSpecifier(ctx, *declSpec);
            }
            dumpDeclaratorProduction(ctx, alias->typeId.abstractDcor);
            break;
        }
        case Var::indexOf<Declaration::UsingNamespace>: {
            auto* usingDirective = decl.var.as<Declaration::UsingNamespace>();
            ctx.out->format("{}{}: UsingNamespace '{}'\n", ctx.indent(), formatLoc(usingDirective->usingKeyword),
                            toString(usingDirective->qid));
            break;
        }
        case Var::indexOf<Declaration::StaticAssert>: {
            auto* staticAssert = decl.var.as<Declaration::StaticAssert>();
            ctx.out->format("{}{}: StaticAssert\n", ctx.indent(), formatLoc(staticAssert->keyword));
            // Dump expression
            break;
        }
        case Var::indexOf<Declaration::AccessSpecifier>: {
            auto* accessSpec = decl.var.as<Declaration::AccessSpecifier>();
            ctx.out->format("{}{}: AccessSpecifier '{}'\n", ctx.indent(), formatLoc(accessSpec->keyword),
                            accessSpec->keyword.text);
            break;
        }
        case 0: {
            ctx.out->format("{}{}: Declaration (empty)\n", ctx.indent(), formatLoc(decl.semicolon));
            break;
        }
        default: {
            PLY_ASSERT(0);
            break;
        }
    }
}

void dumpExpression(DumpContext& ctx, const Expression* expr) {
}

void dumpStatement(DumpContext& ctx, const Statement& stmt) {
}

void Parser::dumpDeclaration(const Declaration& decl) const {
    const ParserImpl* parser = static_cast<const ParserImpl*>(this);
    Stream out = getStdOut();
    DumpContext ctx;
    ctx.out = &out;
    ctx.parser = parser;
    cpp::dumpDeclaration(ctx, decl);
}

} // namespace cpp
} // namespace ply
