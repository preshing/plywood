`ply-tokenizer.h`: Tokenizer
============================

Create a `Tokenizer` object and call `readToken()` to break an input stream into tokens with source offsets.

```cpp
Tokenizer tokenizer;
tokenizer.config = Tokenizer::Config::jsonMode();
tokenizer.errorCallback = [](u32 inputOffset, String&& message) {
    getStdErr().format("Byte {}: {}\n", inputOffset, message);
};

StringView source = "{\"count\": 3}";
ViewStream input{source};
for (;;) {
    Token token = readToken(tokenizer, input);
    if (token.type == Token::EOF)
        break;
    getStdOut().format("{}: {}\n", token.inputOffset, token.text);
}
```

## `Tokenizer`

`Tokenizer` has the following public data members.

| Member | Description |
| --- | --- |
| `u32 inputOffset` | Byte offset of the next token in the input. `readToken()` advances it by each token's byte length. |
| `Config config` | Tokenization settings. Defaults to `Config::cppMode()`; assign a preset or change individual fields before reading. |
| `Functor<void(u32 inputOffset, String&& message)> errorCallback` | Optional callback for lexical errors. The offset identifies the error in the input. Reporting an error does not stop tokenization. |

`Tokenizer::Config` has the following public data members.

| Member | Description |
| --- | --- |
| `bool tokenizeCompoundPunctuation` | Recognize compound operators such as `::`, `==`, and `--`. |
| `bool tokenizeRightShift` | Recognize `>>` when compound punctuation is enabled. |
| `bool tokenizePreprocessorDirectives` | Recognize directives at the start of a line, allowing preceding whitespace. |
| `bool tokenizeCStyleComments` | Recognize `/* ... */`, independently of compound punctuation. |
| `bool tokenizeLineComments` | Recognize `// ...`, independently of compound punctuation. |
| `bool tokenizeSingleQuotedStrings` | Recognize single-quoted strings. |
| `bool tokenizeDoubleQuotedStrings` | Recognize double-quoted strings. |
| `bool allowStringPrefixes` | Recognize prefixes before enabled double-quoted strings, including raw strings. |
| `bool allowUnescapedNewlinesInStrings` | Continue scanning a quoted string across an unescaped newline. |
| `bool allowLeadingMinusInNumbers` | Scan a leading minus with numeric syntax, before considering identifier or operator syntax. |
| `bool tokenizeHexadecimalNumbers` | Recognize `0x` hexadecimal literals. |
| `bool allowNumericSuffixes` | Consume C++ suffixes `f`, `U`, `L`, and `LL`. |
| `bool allowLeadingZerosInNumbers` | Continue scanning integer digits after an initial zero. |
| `bool requireDigitsAfterDecimalPoint` | Report a decimal point without following digits. |
| `bool allowHyphensInIdentifiers` | Allow hyphens in identifiers, including at the start unless numeric scanning takes precedence. |
| `bool allowDotsInIdentifiers` | Allow dots in identifiers, including at the start. |
| `bool allowLineContinuations` | Enable additional backslash-newline handling; support inside identifiers and numbers remains incomplete. |

`static Tokenizer::Config Tokenizer::Config::cppMode()`
> Returns the default C++ tokenization configuration.

`static Tokenizer::Config Tokenizer::Config::jsonMode()`
> Returns a configuration suitable for Plywood's permissive JSON parser. Individual settings can be overridden.

`Token readToken(Tokenizer& tkr, ViewStream& in)`
> Reads the next token from `in` using the configuration and state in `tkr`. Returns a token of type `Token::EOF`
> at the end of the input.
