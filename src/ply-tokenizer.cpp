/*──────────────────────────────────────────────────────────────┐
│                                                               │
│     ____      Plywood C++ Runtime Library                     │
│    ╱   ╱╲     https://plywood.dev/                            │
│   ╱___╱╭╮╲                                                    │
│    └──┴┴┴┘    Tokenizer                                       │
│               Documentation: docs/high-level/tokenizer.md     │
│                                                               │
└──────────────────────────────────────────────────────────────*/

#include "ply-tokenizer.h"

namespace ply {

//  ▄▄▄▄▄ ▄▄ ▄▄▄               ▄▄                         ▄▄   ▄▄                     ▄▄   ▄▄
//  ██    ▄▄  ██   ▄▄▄▄        ██     ▄▄▄▄   ▄▄▄▄  ▄▄▄▄  ▄██▄▄ ▄▄  ▄▄▄▄  ▄▄▄▄▄        ███▄███  ▄▄▄▄  ▄▄▄▄▄
//  ██▀▀  ██  ██  ██▄▄██       ██    ██  ██ ██     ▄▄▄██  ██   ██ ██  ██ ██  ██       ██▀█▀██  ▄▄▄██ ██  ██
//  ██    ██ ▄██▄ ▀█▄▄▄  ▄▄▄▄▄ ██▄▄▄ ▀█▄▄█▀ ▀█▄▄▄ ▀█▄▄██  ▀█▄▄ ██ ▀█▄▄█▀ ██  ██ ▄▄▄▄▄ ██   ██ ▀█▄▄██ ██▄▄█▀
//                                                                                                   ██

inline void updateLineAndColumn(u32& lineNumber, u32& columnNumber, u32 codePoint, u32 tabSize) {
    if (codePoint == '\n') {
        lineNumber++;
        columnNumber = 1;
    } else if (codePoint == '\t') {
        columnNumber += tabSize - (columnNumber % tabSize);
    } else if (codePoint >= 32) {
        columnNumber++;
    }
}

TokenLocationMap TokenLocationMap::createFromString(StringView src, u32 tabSize) {
    ViewStream in{src};
    TokenLocationMap result;
    result.view = src;
    result.tabSize = tabSize;
    u32 lineNumber = 1;
    u32 columnNumber = 1;
    u32 lineStartOfs = 0;

    u32 ofs = 0;
    u32 nextChunkOfs = 256;
    result.table.append({1, 0, 1, 0});
    for (;;) {
        DecodeResult decoded = decodeUnicode(in, UnicodeType::UTF8);
        if (decoded.numBytes == 0)
            break;

        u32 nextOfs = ofs + decoded.numBytes;
        if (nextOfs > nextChunkOfs) {
            result.table.append({lineNumber, nextChunkOfs - lineStartOfs, columnNumber, ofs - nextChunkOfs});
            nextChunkOfs += 256;
        }
        ofs = nextOfs;

        updateLineAndColumn(lineNumber, columnNumber, decoded.point, tabSize);
        if (decoded.point == '\n') {
            lineStartOfs = ofs;
        }
    }
    if (ofs == nextChunkOfs) {
        result.table.append({lineNumber, nextChunkOfs - lineStartOfs, columnNumber, ofs - nextChunkOfs});
    }
    return result;
}

TokenLocation TokenLocationMap::getLocationFromOffset(u32 fileOffset) const {
    PLY_ASSERT(fileOffset <= this->view.numBytes());
    const TokenLocation& fileLocation = this->table[fileOffset >> 8];
    u32 chunkOfs = fileOffset & ~0xff;
    const char* lineStart = this->view.bytes() + (chunkOfs - fileLocation.numBytesIntoLine);
    StringView src = this->view;
    src = src.substr(chunkOfs - fileLocation.numBytesIntoColumn);
    const char* target = this->view.bytes() + fileOffset;
    u32 lineNumber = fileLocation.lineNumber;
    u32 columnNumber = fileLocation.columnNumber;

    for (;;) {
        if (src.bytes() >= target) {
            u32 nb = numericCast<u32>(target - src.bytes());
            // FIXME: numBytesIntoLine is incorrect here:
            return {lineNumber, numericCast<u32>(target - lineStart), columnNumber, nb};
        }

        DecodeResult decoded = decodeUnicode(src, UnicodeType::UTF8);
        src = src.substr(decoded.numBytes);

        updateLineAndColumn(lineNumber, columnNumber, decoded.point, this->tabSize);
    }
}

//  ▄▄▄▄▄▄        ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██
//

StringView getPunctuationString(Token::Type tok) {
    switch (tok) {
        case Token::OpenCurly:
            return "{";
        case Token::CloseCurly:
            return "}";
        case Token::OpenParen:
            return "(";
        case Token::CloseParen:
            return ")";
        case Token::OpenAngle:
            return "<";
        case Token::CloseAngle:
            return ">";
        case Token::OpenSquare:
            return "[";
        case Token::CloseSquare:
            return "]";
        case Token::Semicolon:
            return ";";
        case Token::SingleColon:
            return ":";
        case Token::DoubleColon:
            return "::";
        case Token::SingleEqual:
            return "=";
        case Token::DoubleEqual:
            return "==";
        case Token::NotEqual:
            return "!=";
        case Token::PlusEqual:
            return "+=";
        case Token::MinusEqual:
            return "-=";
        case Token::Comma:
            return ",";
        case Token::QuestionMark:
            return "?";
        case Token::ForwardSlash:
            return "/";
        case Token::Star:
            return "*";
        case Token::Percent:
            return "%";
        case Token::SingleAmpersand:
            return "&";
        case Token::DoubleAmpersand:
            return "&&";
        case Token::SingleVerticalBar:
            return "|";
        case Token::DoubleVerticalBar:
            return "||";
        case Token::SinglePlus:
            return "+";
        case Token::DoublePlus:
            return "++";
        case Token::SingleMinus:
            return "-";
        case Token::DoubleMinus:
            return "--";
        case Token::LeftShift:
            return "<<";
        case Token::RightShift:
            return ">>";
        case Token::Dot:
            return ".";
        case Token::Tilde:
            return "~";
        case Token::Hash:
            return "#";
        case Token::DoubleHash:
            return "##";
        case Token::Bang:
            return "!";
        case Token::Ellipsis:
            return "...";
        case Token::LineComment:
            return "//";
        case Token::CStyleComment:
            return "/*";
        case Token::LessThanOrEqual:
            return "<=";
        case Token::GreaterThanOrEqual:
            return ">=";
        case Token::Arrow:
            return "->";
        case Token::StarEqual:
            return "*=";
        case Token::SlashEqual:
            return "/=";
        case Token::Caret:
            return "^";
        default: {
            PLY_ASSERT(0);
            return "???";
        }
    }
}

StringView Token::toString() const {
    switch (this->type) {
        case Token::Unrecognized:
        case Token::Whitespace:
        case Token::Identifier:
        case Token::StringLiteral:
        case Token::NumericLiteral: {
            return this->text;
        }
        case Token::EOF: {
            return "end-of-file";
        }
        default: {
            return getPunctuationString(this->type);
        }
    }
}

//  ▄▄▄▄▄▄        ▄▄                   ▄▄
//    ██    ▄▄▄▄  ██  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄
//    ██   ██  ██ ██▄█▀  ██▄▄██ ██  ██ ██   ▄█▀  ██▄▄██ ██  ▀▀
//    ██   ▀█▄▄█▀ ██ ▀█▄ ▀█▄▄▄  ██  ██ ██ ▄██▄▄▄ ▀█▄▄▄  ██
//

inline void error(Tokenizer& tkr, const char* pos, String&& message) {
    if (tkr.errorCallback) {
        tkr.errorCallback(tkr.inputOffset + numericCast<u32>(pos - tkr.startByte), std::move(message));
    }
}

Tokenizer::Config Tokenizer::Config::cppMode() {
    return {};
}

Tokenizer::Config Tokenizer::Config::jsonMode() {
    // Recognize permissive JSON tokens; the parser determines which extensions are accepted.
    Config config;
    config.tokenizeCompoundPunctuation = false;
    config.allowStringPrefixes = false;
    config.allowUnescapedNewlinesInStrings = true;
    config.allowLeadingMinusInNumbers = true;
    config.tokenizeHexadecimalNumbers = false;
    config.allowNumericSuffixes = false;
    config.allowLeadingZerosInNumbers = false;
    config.requireDigitsAfterDecimalPoint = true;
    config.allowHyphensInIdentifiers = true;
    config.allowDotsInIdentifiers = true;
    config.tokenizeRightShift = false;
    config.tokenizePreprocessorDirectives = false;
    config.tokenizeCStyleComments = false;
    config.tokenizeLineComments = false;
    config.allowLineContinuations = false;
    return config;
}

void readNumericLiteral(Tokenizer& tkr, ViewStream& in) {
    // Scan the spelling without converting its value or changing the stream's error state.
    auto isDigit = [&]() { return in.makeReadable() && *in.curByte >= '0' && *in.curByte <= '9'; };
    if (in.makeReadable() && *in.curByte == '-') {
        in.curByte++;
    }
    bool leadingZero = in.makeReadable() && *in.curByte == '0';
    if (leadingZero) {
        in.curByte++;
        if (tkr.config.tokenizeHexadecimalNumbers && in.makeReadable() && *in.curByte == 'x') {
            in.curByte++;
            while (in.makeReadable()) {
                char c = *in.curByte;
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                    break;
                in.curByte++;
            }
            goto suffix;
        }
    }
    if (!leadingZero || tkr.config.allowLeadingZerosInNumbers) {
        while (isDigit()) {
            in.curByte++;
        }
    }
    if (in.makeReadable() && *in.curByte == '.') {
        const char* decimal = in.curByte++;
        if (tkr.config.requireDigitsAfterDecimalPoint && !isDigit()) {
            error(tkr, decimal, "expected digits after decimal point");
        }
        while (isDigit()) {
            in.curByte++;
        }
    }
    if (in.makeReadable() && (*in.curByte == 'e' || *in.curByte == 'E')) {
        in.curByte++;
        if (in.makeReadable() && (*in.curByte == '+' || *in.curByte == '-')) {
            in.curByte++;
        }
        while (isDigit()) {
            in.curByte++;
        }
    }
suffix:
    if (!tkr.config.allowNumericSuffixes)
        return;
    if (in.makeReadable() && (*in.curByte == 'f')) {
        in.curByte++;
    } else {
        if (in.makeReadable() && (*in.curByte == 'U')) {
            in.curByte++;
        }
        if (in.makeReadable() && (*in.curByte == 'L')) {
            in.curByte++;
            if (in.makeReadable() && (*in.curByte == 'L')) {
                in.curByte++;
            }
        }
    }
}

void readStringLiteral(Tokenizer& tkr, ViewStream& in, char quotePunc) {
    PLY_ASSERT((quotePunc == '"') || (quotePunc == '\''));
    for (;;) {
        if (!in.makeReadable()) {
            error(tkr, in.curByte, "unexpected end-of-file in string literal");
            break;
        }
        char c = *in.curByte;
        in.curByte++;
        if (c == '\\') {
            if (!in.makeReadable()) {
                error(tkr, in.curByte, "unexpected end-of-file in string literal");
                break;
            }
            in.curByte++;
        } else if (c == '\n' && !tkr.config.allowUnescapedNewlinesInStrings) {
            error(tkr, in.curByte, "unexpected end-of-line in string literal");
            break;
        } else if (c == quotePunc)
            break;
    }
}

bool readDelimiterAndRawStringLiteral(Tokenizer& tkr, ViewStream& in) {
    PLY_ASSERT(in.hasRemainingBytes() && (*in.curByte == '"'));
    in.curByte++;

    // read delimiter
    const char* delimiterStart = in.curByte;
    for (;;) {
        if (!in.makeReadable()) {
            // End of file while reading raw string delimiter
            error(tkr, in.curByte, "unexpected end-of-file in raw string delimiter");
            return false;
        }
        char c = *in.curByte;
        if (c == '(')
            break;
        // FIXME: Recognize more whitespace characters
        if (isWhite(c) || c == ')' || c == '\\') {
            // Invalid character in delimiter
            error(tkr, in.curByte, "invalid character in raw string delimiter");
            return false;
        }
        in.curByte++;
    }

    // FIXME: Enforce maximum length of delimiter (at most 16 characters)
    const char* delimiterEnd = in.curByte;
    in.curByte++;

    // Read remainder of string
    for (;;) {
        if (!in.makeReadable()) {
            // End of file in string literal
            error(tkr, in.curByte, "unexpected end-of-file in string literal");
            return false;
        }
        char c = *in.curByte;
        in.curByte++;
        if (c == ')') {
            // Try to match delimiter
            const char* d = delimiterStart;
            for (;;) {
                if (d == delimiterEnd) {
                    if (!in.makeReadable()) {
                        // End of file while matching closing "
                        error(tkr, in.curByte, "unexpected end-of-file in string literal");
                        return false;
                    }
                    c = *in.curByte;
                    if (c == '"') {
                        // End of string literal
                        in.curByte++;
                        return true;
                    }
                }
                if (!in.makeReadable()) {
                    // End of file while matching delimiter
                    error(tkr, in.curByte, "unexpected end-of-file in string literal");
                    return false;
                }
                c = *in.curByte;
                in.curByte++;
                if (c != *d)
                    break; // No match here
                d++;
            }
        }
    }
}

Token::Type readIdentifierOrLiteral(Tokenizer& tkr, ViewStream& in) {
    // FIXME: Optionally skip line continuations inside here.
    // This implementation is a little too obfuscated anyway.
    PLY_ASSERT(in.hasRemainingBytes());

    u32 mask[8] = {0, 0, 0x87fffffe, 0x7fffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    mask[1] |= 0x10;      // '$'
    mask[1] |= 0x3ff0000; // accept digits (we already know the first character is non-digit)

    if (tkr.config.allowHyphensInIdentifiers) {
        mask[1] |= 1u << ('-' & 31);
    }
    if (tkr.config.allowDotsInIdentifiers) {
        mask[1] |= 1u << ('.' & 31);
    }

    const char* startByte = in.curByte;
    for (;;) {
        if (!in.makeReadable()) {
            PLY_ASSERT(in.curByte != startByte);
            return Token::Identifier;
        }
        char c = *in.curByte;
        if ((mask[(u8) c >> 5] & (1u << ((u8) c & 31))) == 0) {
            if (c == '"' && tkr.config.allowStringPrefixes && tkr.config.tokenizeDoubleQuotedStrings) {
                if (in.curByte == startByte + 1 && *startByte == 'R') {
                    readDelimiterAndRawStringLiteral(tkr, in);
                } else {
                    // Treat it as a string prefix
                    in.curByte++;
                    readStringLiteral(tkr, in, c);
                }
                return Token::StringLiteral;
            } else {
                if (startByte == in.curByte) {
                    // Garbage token
                    error(tkr, in.curByte, "garbage characters encountered");
                    in.curByte++;
                    return Token::Unrecognized;
                } else {
                    return Token::Identifier;
                }
            }
        }
        in.curByte++;
    }
}

Token readToken(Tokenizer& tkr, ViewStream& in) {
    Token token;
    token.inputOffset = tkr.inputOffset;
    token.type = Token::Unrecognized;
    if (!in.makeReadable()) {
        token.type = Token::EOF;
        return token;
    }

    tkr.startByte = in.curByte;
    bool wasAtStartOfLine = tkr.state.atStartOfLine;
    tkr.state.atStartOfLine = false;
    auto canReadSecondChar = [&](bool compound = true) {
        if (tkr.config.allowLineContinuations && (in.numRemainingBytes() >= 2) && (*in.curByte == '\\') &&
            (*(in.curByte + 1) == '\n')) {
            in.curByte += 2;
        }
        return (!compound || tkr.config.tokenizeCompoundPunctuation) && in.makeReadable();
    };

retry:
    char c = *in.curByte;
    switch (c) {
        case '\n':
        case '\r':
        case '\t':
        case ' ': {
            // Skip whitespace while keeping track of start of line
            token.type = Token::Whitespace;
            tkr.state.atStartOfLine = wasAtStartOfLine;
            while (in.makeReadable()) {
                switch (*in.curByte) {
                    case '\n':
                        tkr.state.atStartOfLine = true;
                    case '\r':
                    case '\t':
                    case ' ':
                        in.curByte++;
                        break;
                    case '\\':
                        if (tkr.config.allowLineContinuations && (in.numRemainingBytes() >= 2) &&
                            (in.curByte[1] == '\n')) {
                            in.curByte += 2;
                            break;
                        }
                    default:
                        goto endOfWhite;
                }
            }
        endOfWhite:
            break;
        }

        case '#': {
            in.curByte++;
            if (wasAtStartOfLine && tkr.config.tokenizePreprocessorDirectives) {
                token.type = Token::PreprocessorDirective;
                // Read directive up to its terminating newline.
                for (;;) {
                    if (!in.makeReadable())
                        break;
                    char c = *in.curByte++;
                    if (c == '\n')
                        break;
                    // Skip \ newline escapes.
                    if (c == '\\' && in.hasRemainingBytes() && (*in.curByte == '\n')) {
                        in.curByte++;
                    }
                }
                tkr.state.atStartOfLine = true;
            } else {
                if (canReadSecondChar() && (*in.curByte == '#')) {
                    in.curByte++;
                    token.type = Token::DoubleHash;
                } else {
                    token.type = Token::Hash;
                }
            }
            break;
        }

        case '/': {
            in.curByte++;
            token.type = Token::ForwardSlash;
            if (canReadSecondChar(false)) {
                if ((*in.curByte == '/') && tkr.config.tokenizeLineComments) {
                    in.curByte++;
                    token.type = Token::LineComment;
                    readLine(in);
                    tkr.state.atStartOfLine = true;
                } else if ((*in.curByte == '*') && tkr.config.tokenizeCStyleComments) {
                    token.type = Token::CStyleComment;
                    in.curByte++;
                    for (;;) {
                        if (!in.makeReadable()) {
                            error(tkr, in.curByte, "unexpected end-of-file in C-style comment");
                            break;
                        } else if (*in.curByte == '*') {
                            in.curByte++;
                            if (!in.makeReadable()) {
                                error(tkr, in.curByte, "unexpected end-of-file in C-style comment");
                                break;
                            } else if (*in.curByte == '/') {
                                in.curByte++;
                                break;
                            }
                        } else {
                            in.curByte++;
                        }
                    }
                } else if (tkr.config.tokenizeCompoundPunctuation && *in.curByte == '=') {
                    token.type = Token::SlashEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '{': {
            token.type = Token::OpenCurly;
            in.curByte++;
            break;
        }

        case '}': {
            token.type = Token::CloseCurly;
            in.curByte++;
            break;
        }

        case ';': {
            token.type = Token::Semicolon;
            in.curByte++;
            break;
        }

        case '(': {
            token.type = Token::OpenParen;
            in.curByte++;
            break;
        }

        case ')': {
            token.type = Token::CloseParen;
            in.curByte++;
            break;
        }

        case '<': {
            token.type = Token::OpenAngle;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '<') {
                    token.type = Token::LeftShift;
                    in.curByte++;
                } else if (*in.curByte == '=') {
                    token.type = Token::LessThanOrEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '>': {
            token.type = Token::CloseAngle;
            in.curByte++;
            if (canReadSecondChar()) {
                if (tkr.config.tokenizeRightShift && (*in.curByte == '>')) {
                    token.type = Token::RightShift;
                    in.curByte++;
                } else if (*in.curByte == '=') {
                    token.type = Token::GreaterThanOrEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '[': {
            token.type = Token::OpenSquare;
            in.curByte++;
            break;
        }

        case ']': {
            token.type = Token::CloseSquare;
            in.curByte++;
            break;
        }

        case ':': {
            token.type = Token::SingleColon;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == ':') {
                    token.type = Token::DoubleColon;
                    in.curByte++;
                }
            }
            break;
        }

        case ',': {
            token.type = Token::Comma;
            in.curByte++;
            break;
        }

        case '?': {
            token.type = Token::QuestionMark;
            in.curByte++;
            break;
        }

        case '=': {
            token.type = Token::SingleEqual;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '=') {
                    token.type = Token::DoubleEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '*': {
            in.curByte++;
            token.type = Token::Star;
            if (canReadSecondChar()) {
                if (*in.curByte == '=') {
                    token.type = Token::StarEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '%': {
            token.type = Token::Percent;
            in.curByte++;
            break;
        }

        case '&': {
            token.type = Token::SingleAmpersand;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '&') {
                    token.type = Token::DoubleAmpersand;
                    in.curByte++;
                }
            }
            break;
        }

        case '|': {
            token.type = Token::SingleVerticalBar;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '|') {
                    token.type = Token::DoubleVerticalBar;
                    in.curByte++;
                }
            }
            break;
        }

        case '+': {
            token.type = Token::SinglePlus;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '+') {
                    token.type = Token::DoublePlus;
                    in.curByte++;
                } else if (*in.curByte == '=') {
                    token.type = Token::PlusEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '-': {
            if (tkr.config.allowLeadingMinusInNumbers) {
                token.type = Token::NumericLiteral;
                readNumericLiteral(tkr, in);
                break;
            }
            if (tkr.config.allowHyphensInIdentifiers) {
                token.type = readIdentifierOrLiteral(tkr, in);
                break;
            }
            token.type = Token::SingleMinus;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '-') {
                    token.type = Token::DoubleMinus;
                    in.curByte++;
                } else if (*in.curByte == '=') {
                    token.type = Token::MinusEqual;
                    in.curByte++;
                } else if (*in.curByte == '>') {
                    token.type = Token::Arrow;
                    in.curByte++;
                }
            }
            break;
        }

        case '.': {
            if (tkr.config.allowDotsInIdentifiers) {
                token.type = readIdentifierOrLiteral(tkr, in);
                break;
            }
            token.type = Token::Dot;
            in.curByte++;
            if (canReadSecondChar()) {
                if (in.numRemainingBytes() >= 2 && in.curByte[0] == '.' && in.curByte[1] == '.') {
                    token.type = Token::Ellipsis;
                    in.curByte += 2;
                }
            }
            break;
        }

        case '~': {
            token.type = Token::Tilde;
            in.curByte++;
            break;
        }

        case '^': {
            token.type = Token::Caret;
            in.curByte++;
            break;
        }

        case '!': {
            token.type = Token::Bang;
            in.curByte++;
            if (canReadSecondChar()) {
                if (*in.curByte == '=') {
                    token.type = Token::NotEqual;
                    in.curByte++;
                }
            }
            break;
        }

        case '\'': {
            if (tkr.config.tokenizeSingleQuotedStrings) {
                token.type = Token::StringLiteral;
                in.curByte++;
                readStringLiteral(tkr, in, '\'');
            }
            break;
        }

        case '"': {
            if (tkr.config.tokenizeDoubleQuotedStrings) {
                token.type = Token::StringLiteral;
                in.curByte++;
                readStringLiteral(tkr, in, '"');
            }
            break;
        }

        case '\\': {
            if (tkr.config.allowLineContinuations && (in.numRemainingBytes() >= 2) && (in.curByte[1] == '\n')) {
                in.curByte += 2;
                goto retry;
            }
            break;
        }
    }

    if (token.type == Token::Unrecognized) {
        if (c >= '0' && c <= '9') {
            token.type = Token::NumericLiteral;
            readNumericLiteral(tkr, in);
        } else {
            token.type = readIdentifierOrLiteral(tkr, in);
        }
    } else if (token.type >= Token::Punctuation) {
        // Get hardcoded punctuation string in case there was a mid-token line continuation.
        token.text = getPunctuationString(token.type);
    }

    token.text = {tkr.startByte, in.curByte};
    PLY_ASSERT(token.text.numBytes() > 0);
    tkr.inputOffset += token.text.numBytes();
    tkr.startByte = nullptr;
    return token;
}

} // namespace ply
