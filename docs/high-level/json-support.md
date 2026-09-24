`ply-json.h`: JSON Support
==========================

The JSON module parses text into a mutable tree of `Node` objects and serializes those trees back to JSON. The parser
can enforce strict JSON or independently allow, warn about or reject several relaxed syntax extensions. All functions
and types in this module are defined in the `ply::json` namespace.

## Parsing JSON

Create a `Parser` with the desired options, optionally install a diagnostic callback, then call `parse()`. Parsers are
intended for a single input document.

```cpp
json::Parser::Options options = json::Parser::Options::makeStrict();
Owned<json::Parser> parser = json::Parser::create(options);
parser->setDiagnosticCallback([&](const json::Diagnostic& diagnostic) {
    parser->printDiagnostic(diagnostic, getStdErr());
});

json::ParseResult result = parser->parse("settings.json", sourceText);
if (result.root) {
    StringView name = result.root.get("name").text();
}
```

### `Parser::Options`

Each syntax option uses one of the following handling policies:

| `Parser::Options::Policy` | Description |
| --- | --- |
| `Permissive` | Accept the extension without reporting a diagnostic. |
| `WarnAndContinue` | Report a warning, accept the extension and continue parsing. |
| `FatalError` | Report an error and stop parsing. |

`Parser::Options` is permissive by default. Set individual fields to select a policy for each extension, or use
`makeStrict()` to reject all of them.

| Option | Relaxed syntax controlled by the option |
| --- | --- |
| `u32 tabSize` | Sets the tab width used when converting byte offsets to diagnostic columns. Default is 4; a value of 0 is treated as 1. |
| `Policy trailingInput` | Allows non-whitespace input after parsing the root JSON expression. |
| `Policy unquotedKeys` | Allows object property names without quotes, as in `{name: "Ada"}`. |
| `Policy unquotedStrings` | Treats an unquoted word that is not `true`, `false`, `null` or a number as a string. |
| `Policy equalsSign` | Allows `=` instead of `:` after an object property name. |
| `Policy alternateSeparators` | Allows semicolons `;` instead of commas `,` between object properties and array values. |
| `Policy looseSeparators` | Allows missing commas, including newline-only separators, and redundant separators. |
| `Policy singleQuotedStrings` | Allows strings and property names enclosed in single quotes. |
| `Policy nonUTF8Strings` | Allows invalid UTF-8 byte sequences in strings. |
| `Policy unescapedControlChars` | Allows unescaped control characters in strings. |
| `Policy arbitraryEscapeChars` | Allows nonstandard escape sequences by discarding the backslash and retaining the escaped character. |
| `Policy duplicateKeys` | Allows an object to contain the same property name more than once. |
| `bool duplicateKeysOverrideEarlier` | If `true` and a property name occurs more than once in an object, the later property overrides the first one. |

`static Options Parser::Options::makeStrict()`
> Returns options that set every `Policy` field to `FatalError`.

Numbers such as `1.` are always rejected: a decimal point must be followed by a digit.
Malformed Unicode escapes and unpaired surrogates are always errors. In single-quoted strings,
`\'` escapes the closing quote independently of `arbitraryEscapeChars`.

### `Parser`

`static Owned<Parser> Parser::create(const Options& options)`
> Creates a parser with a copy of `options`. Retain the returned `Owned<Parser>` until parsing and diagnostic handling
> are complete.

`void Parser::destroy()`
> Destroys a parser created by `create()`. `Owned<Parser>` calls this automatically.

`void Parser::setDiagnosticCallback(Functor<void(const Diagnostic& diagnostic)>&& callback)`
> Installs the callback used for warnings and errors. The `Diagnostic` object is only valid for the duration of
> the callback.

`ParseResult Parser::parse(StringView path, StringView srcView)`
> Parses the root JSON expression from `srcView`. A fatal error returns an invalid root. `ParseResult` contains the following data members:

| Member | Description |
| --- | --- |
| `Node root` | The parsed root value, or an invalid node after a fatal error. |
| `TokenLocationMap tokenLocMap` | Maps source byte offsets to line and column locations. |
| `u32 numBytes` | Number of input bytes belonging to the accepted parse. When non-whitespace trailing input is accepted, this marks the end of the root value; otherwise, it includes consumed trailing whitespace. |

`bool Parser::anyError() const`
> Returns true if the parser reported a fatal error. Warnings do not set this flag.

`void Parser::printDiagnostic(const Diagnostic& diagnostic, Stream& out) const`
> Writes a diagnostic with its line, column, severity, message and nested parse context. Call this while the diagnostic
> callback is active.

### `Diagnostic`

| Member | Description |
| --- | --- |
| `Diagnostic::Level errorLevel` | Either `Diagnostic::Warning` or `Diagnostic::Error`. |
| `u32 fileOfs` | Byte offset of the warning or error in the input. |
| `String message` | Human-readable diagnostic message. |
| `const Array<Diagnostic::Scope>& context` | Nested object, property, duplicate-property and array scopes active at the diagnostic location. |

The diagnostic callback receives errors for malformed JSON as well as warnings or errors selected by `Options`.
`printDiagnostic()` uses the parser's `TokenLocationMap` to turn these byte offsets into line and column numbers.

## `Node`

`Node` stores one JSON value in its public `var` member and records its source byte offset in `fileOfs`. A parsed tree
uses the following alternatives:

| JSON value | `Node` alternative |
| --- | --- |
| Boolean | `Node::Bool` |
| Number | `Node::Number` containing a `double` |
| String | `Node::Text` |
| Array | `Node::Array` |
| Object | `Node::Object` |
| Null | `Node::Null` |

A default-constructed `Node` is invalid. Invalid nodes are used for parse failures and failed lookups; they are
distinct from a valid `Node::Null` value.

```cpp
json::Node root{json::Node::Object{}};
root.set("name", json::Node::Text{"Ada"});
root.set("active", json::Node::Bool{true});

json::Node numbers{json::Node::Array{}};
numbers.array().append(json::Node::Number{1});
numbers.array().append(json::Node::Number{2});
root.set("numbers", std::move(numbers));
```

### Construction and validity

`Node::Node()`
> Creates an invalid node.

`Node::Node(const Bool& value, u32 fileOfs = 0)`
`Node::Node(const Number& value, u32 fileOfs = 0)`
`Node::Node(Text&& value, u32 fileOfs = 0)`
`Node::Node(Array&& value, u32 fileOfs = 0)`
`Node::Node(Object&& value, u32 fileOfs = 0)`
`Node::Node(const Null& value, u32 fileOfs = 0)`
> Creates a node containing the selected alternative. Programmatically constructed nodes use offset 0 unless another
> offset is supplied.

`bool Node::isValid() const`
`explicit Node::operator bool() const`
> Returns whether the node contains a value. JSON `null` is valid and therefore returns true.

### Booleans, numbers, strings and null

`bool Node::isBool() const`
> Returns whether the node contains `Node::Bool`.

`bool Node::getBool() const`
> Returns the stored Boolean, or false when the node has another type.

`void Node::setBool(bool value)`
> Replaces the node with a Boolean value.

`bool Node::isNumber() const`
> Returns whether the node contains `Node::Number`.

`double Node::getNumber() const`
> Returns the stored number, or 0 when the node has another type.

`void Node::setNumber(double value)`
> Replaces the node with a number.

`bool Node::isText() const`
> Returns whether the node contains `Node::Text`.

`StringView Node::text() const`
> Returns the stored string, or an empty view when the node has another type.

`void Node::setText(String&& text)`
> Replaces the node with a string, taking ownership of `text`.

`bool Node::isNull() const`
> Returns whether the node contains a valid JSON null value.

`void Node::setNull()`
> Replaces the node with JSON null.

### Arrays

`bool Node::isArray() const`
> Returns whether the node contains `Node::Array`.

`Node& Node::get(u32 index)`
`const Node& Node::get(u32 index) const`
> Returns the array element at `index`. A wrong node type or out-of-range index returns the shared invalid node.

`ArrayView<const Node> Node::arrayView() const`
> Returns a read-only view of the array elements, or an empty view when the node has another type.

`Array<Node>& Node::array()`
> Returns the mutable array storage. The node must contain `Node::Array`.

### Objects

`bool Node::isObject() const`
> Returns whether the node contains `Node::Object`.

`Node& Node::get(StringView key)`
`const Node& Node::get(StringView key) const`
> Returns the property named `key`. A wrong node type or missing property returns the shared invalid node.

`void Node::set(StringView key, Node&& value)`
> Adds or replaces a property when the node contains an object. The function has no effect on another node type.

`void Node::remove(StringView key)`
> Removes a property when the node contains an object. The function has no effect on another node type.

`Object& Node::object()`
`const Object& Node::object() const`
> Returns the underlying object storage. The mutable overload requires an object node; the const overload returns an
> empty object for another node type.

## Writing JSON

Objects are written in their stored order.

`void write(Stream& out, const Node& node, const WriteOptions& options = {})`
> Serializes `node` directly to `out`.

`String toString(const Node& node, const WriteOptions& options = {})`
> Serializes `node` and returns the resulting string.

`WriteOptions` has the following data members:

| Name | Description |
| --- | --- |
| `bool includeWhitespace` | Writes indented, multi-line JSON when true and compact JSON when false. Default is true. |


```cpp
String compact = json::toString(root, {false});
Stream out = getStdOut();
json::write(out, root);
```
