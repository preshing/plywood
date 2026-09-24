/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: docs/apps/test-suite.md            │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include "run-system-tests.h"
#include <ply-json.h>

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Json_

using Options = json::Parser::Options;

// Capture only owned results and diagnostic counts; callback references never escape.
struct ParseObservation {
    json::ParseResult result;
    bool anyError = false;
    u32 warnings = 0;
    u32 errors = 0;
};

static ParseObservation parseForTest(StringView source, const Options& options, bool collectDiagnostics = true) {
    ParseObservation observation;
    Owned<json::Parser> parser = json::Parser::create(options);
    if (collectDiagnostics) {
        parser->setDiagnosticCallback([&](const json::Diagnostic& diagnostic) {
            (diagnostic.errorLevel == json::Diagnostic::Warning ? observation.warnings : observation.errors)++;
        });
    }
    observation.result = parser->parse({}, source);
    observation.anyError = parser->anyError();
    return observation;
}

JSON_TEST_CASE("Valid values and strings") {
    // Check primitive types, EOF boundaries, and formatted containers directly.
    auto boolean = parseForTest("true", Options::makeStrict());
    check(boolean.result.root.isBool() && boolean.result.root.getBool() && boolean.result.numBytes == 4);
    auto falseValue = parseForTest("false", Options::makeStrict());
    check(falseValue.result.root.isBool() && !falseValue.result.root.getBool());
    auto number = parseForTest("123", Options::makeStrict());
    check(number.result.root.isNumber() && number.result.root.getNumber() == 123 && number.result.numBytes == 3);
    auto nullValue = parseForTest("null", Options::makeStrict());
    check(nullValue.result.root.isNull() && nullValue.result.numBytes == 4);
    check(parseForTest("[]", Options::makeStrict()).result.root.isArray());
    check(parseForTest("{}", Options::makeStrict()).result.root.isObject());
    auto object = parseForTest("{\n\"a\":1,\n\"b\":[2,\n3]\n}", Options::makeStrict());
    check(object.result.root.isObject() && object.result.root.get("a").getNumber() == 1 &&
          object.result.root.get("b").arrayView().numItems() == 2 &&
          object.result.root.get("b").get(1).getNumber() == 3 && !object.anyError);

    // Standard strings run in both modes; extensions run only with permissive options.
    struct Case {
        StringView source;
        StringView expected;
        bool strict;
    };
    const Case cases[] = {
        {"\"\\u00e9\\u4e2d\\ud83d\\ude00\"", "\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80", true},
        {"\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", "\"\\/\b\f\n\r\t", true},
        {"\"\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80\"", "\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80", true},
        {"\"true\"", "true", true},
        {"'it\\'s'", "it's", false},
        {"\"\\q\"", "q", false},
        {"\"raw\nline\"", "raw\nline", false},
        {"\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80", "\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80", false},
    };
    for (u32 i = 0; i < PLY_STATIC_ARRAY_SIZE(cases); i++) {
        for (bool strict : {false, true}) {
            if (strict && !cases[i].strict)
                continue;
            auto parsed = parseForTest(cases[i].source, strict ? Options::makeStrict() : Options{});
            if (!check(parsed.result.root.isText() && parsed.result.root.text() == cases[i].expected &&
                       parsed.result.numBytes == cases[i].source.numBytes() && !parsed.anyError)) {
                getStdErr().format("String row {}, strict={}\n", i, strict);
            }
        }
    }
}

JSON_TEST_CASE("Parsing policies") {
    // Change one policy at a time to catch coupling between otherwise strict options.
    struct Case {
        Options::Policy Options::*member;
        StringView source;
    };
    const Case cases[] = {
        {&Options::unquotedKeys, "{a:1}"},
        {&Options::unquotedStrings, "word"},
        {&Options::equalsSign, "{\"a\"=1}"},
        {&Options::alternateSeparators, "[1;2]"},
        {&Options::alternateSeparators, "{\"a\":1;\"b\":2}"},
        {&Options::looseSeparators, "[1\n2]"},
        {&Options::looseSeparators, "{\"a\":1\n\"b\":2}"},
        {&Options::looseSeparators, "[1 2]"},
        {&Options::looseSeparators, "[1,,2]"},
        {&Options::looseSeparators, "[,1]"},
        {&Options::looseSeparators, "[1,]"},
        {&Options::looseSeparators, "{\"a\":1,}"},
        {&Options::singleQuotedStrings, "'text'"},
        {&Options::singleQuotedStrings, "{'key':1}"},
        {&Options::unescapedControlChars, "\"line\nfeed\""},
        {&Options::arbitraryEscapeChars, "\"\\q\""},
        {&Options::arbitraryEscapeChars, "\"\\'\""},
        {&Options::nonUTF8Strings, "\"\xc0\x80\""},
        {&Options::duplicateKeys, "{\"a\":1,\"a\":2}"},
        {&Options::trailingInput, "true \"unterminated"},
    };
    for (u32 i = 0; i < PLY_STATIC_ARRAY_SIZE(cases); i++) {
        for (auto policy : {Options::Permissive, Options::WarnAndContinue, Options::FatalError}) {
            Options options = Options::makeStrict();
            options.*cases[i].member = policy;
            auto parsed = parseForTest(cases[i].source, options);
            bool fatal = policy == Options::FatalError;
            if (!check(parsed.result.root.isValid() == !fatal && parsed.anyError == fatal &&
                       parsed.warnings == (policy == Options::WarnAndContinue ? 1u : 0u) &&
                       parsed.errors == (fatal ? 1u : 0u))) {
                getStdErr().format("Policy row {}, mode={}\n", i, (u32) policy);
            }
            // One representative input verifies that callbacks do not control acceptance.
            if (i == 0) {
                auto silent = parseForTest(cases[i].source, options, false);
                if (!check(silent.result.root.isValid() == !fatal && silent.anyError == fatal)) {
                    getStdErr().format("Silent policy mode={}\n", (u32) policy);
                }
            }
        }
    }
}

JSON_TEST_CASE("Malformed input") {
    // Malformed escapes, incomplete strings and missing fractions fail even in permissive mode.
    const StringView cases[] = {"\"\\u123\"",     "\"\\uZZZZ\"",   "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u0041\"",
                                "\"unterminated", "[\"\\uZZZZ\"]", "1.",          "1.e2"};
    for (u32 i = 0; i < PLY_STATIC_ARRAY_SIZE(cases); i++) {
        for (bool strict : {false, true}) {
            auto parsed = parseForTest(cases[i], strict ? Options::makeStrict() : Options{});
            if (!check(!parsed.result.root.isValid() && parsed.anyError && parsed.errors > 0)) {
                getStdErr().format("Malformed row {}, strict={}\n", i, strict);
            }
        }
    }
    // Strict strings also reject truncated, surrogate and out-of-range UTF-8.
    const StringView utf8[] = {"\"\xe2\x82\"", "\"\xed\xa0\x80\"", "\"\xf4\x90\x80\x80\""};
    for (u32 i = 0; i < PLY_STATIC_ARRAY_SIZE(utf8); i++) {
        auto parsed = parseForTest(utf8[i], Options::makeStrict());
        if (!check(!parsed.result.root.isValid() && parsed.anyError)) {
            getStdErr().format("Invalid UTF-8 row {}\n", i);
        }
    }
}

JSON_TEST_CASE("Duplicates and trailing input") {
    // A null property still counts as an existing value when resolving duplicates.
    for (bool overrideEarlier : {false, true}) {
        Options options;
        options.duplicateKeysOverrideEarlier = overrideEarlier;
        auto parsed = parseForTest("{\"a\":null,\"a\":2}", options);
        check(parsed.result.root.isObject() && !parsed.anyError &&
              (overrideEarlier ? parsed.result.root.get("a").getNumber() == 2 : parsed.result.root.get("a").isNull()));
    }
    // Exercise extensions together and verify the accepted root's boundary.
    Options options;
    options.trailingInput = Options::WarnAndContinue;
    auto mixed = parseForTest("{a = 'x'; b: [1 2,]} trailing", options);
    check(mixed.result.root.get("a").text() == "x" && mixed.result.root.get("b").arrayView().numItems() == 2 &&
          mixed.result.numBytes == StringView{"{a = 'x'; b: [1 2,]}"}.numBytes() && mixed.warnings == 1 &&
          !mixed.anyError);
    // Consume final whitespace, but stop at the root when accepting further input.
    for (auto policy : {Options::Permissive, Options::WarnAndContinue, Options::FatalError}) {
        options = Options::makeStrict();
        options.trailingInput = policy;
        auto complete = parseForTest("true \n", options);
        auto trailing = parseForTest("true  false", options);
        bool fatal = policy == Options::FatalError;
        if (!check(complete.result.root.isBool() && complete.result.numBytes == 6 && !complete.anyError &&
                   trailing.result.root.isValid() == !fatal && trailing.anyError == fatal &&
                   (fatal || trailing.result.numBytes == 4))) {
            getStdErr().format("Trailing mode={}\n", (u32) policy);
        }
    }
}
