/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: /docs/apps/test-suite.md           │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include <ply-agent.h>
#include <ply-json.h>
#include "test-suite.h"

using namespace ply;

// Records a failed expectation and keeps running the remaining cases.
static bool expect(bool condition, StringView description) {
    if (!condition) {
        getStdErr().format("***FAIL*** {}\n", description);
    }
    return condition;
}

// Verifies line splitting, tail finalization and independent buffer copies.
static bool testTranscriptBuffer() {
    bool success = true;
    Transcript::Buffer buffer;
    buffer.append("alpha");
    buffer.append(" beta\nsecond\nthird");
    success &= expect(buffer.lines.numItems() == 2, "append should retain two completed lines");
    success &= expect(buffer.lines[0] == "alpha beta\n", "chunks should combine only within their current line");
    success &= expect(buffer.lines[1] == "second\n", "each completed line should remain separate");

    // A copy duplicates the unfinished tail without changing either buffer's completed lines.
    Transcript::Buffer copy = buffer;
    copy.append(" copy");
    copy.flush();
    buffer.flush();
    success &= expect(copy.lines[2] == "third copy", "a copied tail should be independently appendable");
    success &= expect(buffer.lines[2] == "third", "copying should not mutate the source tail");
    return success;
}

// Verifies the implicit transcript and explicit tool-response finalization points.
static bool testTranscriptUpdater() {
    bool success = true;
    Transcript transcript;
    Owned<TranscriptUpdater> updater = createTranscriptUpdater(&transcript);

    TranscriptEvent event;
    event.operation = TranscriptEvent::BeginMessage;
    event.role = Transcript::Role::User;
    applyTranscriptEvent(updater, event);
    event.operation = TranscriptEvent::AppendText;
    event.text = "partial";
    applyTranscriptEvent(updater, event);
    success &= expect(transcript.turns[0].messages[0]->content.lines.isEmpty(),
                      "an incomplete streamed line should remain in tail");

    // Beginning the next message implicitly finalizes the previous message.
    event = {};
    event.operation = TranscriptEvent::BeginMessage;
    event.role = Transcript::Role::ToolCall;
    event.providerToolCallID = "call_123";
    applyTranscriptEvent(updater, event);
    success &= expect(transcript.turns[0].messages[0]->content.lines[0] == "partial",
                      "BeginMessage should finalize the previous message tail");
    success &= expect(transcript.turns[0].messages[1]->providerToolCallID == "call_123",
                      "BeginMessage should retain the endpoint's tool call ID");

    // Opaque provider items are retained separately from the visible transcript.
    event = {};
    event.operation = TranscriptEvent::AppendProviderOutputItem;
    event.text = R"({"type":"reasoning","encrypted_content":"opaque"})";
    applyTranscriptEvent(updater, event);
    success &= expect(transcript.turns[0].providerOutputItems[0] == event.text,
                      "provider output items should be retained for context replay");

    event = {};
    event.operation = TranscriptEvent::AppendToolResponse;
    event.toolCallID = 1;
    event.text = "first\nlast";
    applyTranscriptEvent(updater, event);
    event.operation = TranscriptEvent::EndToolResponse;
    event.text = {};
    applyTranscriptEvent(updater, event);
    const Transcript::Buffer& response = transcript.turns[0].messages[1]->toolResponse;
    success &= expect(response.lines.numItems() == 2, "EndToolResponse should finalize its incomplete line");
    success &= expect(response.lines[0] == "first\n" && response.lines[1] == "last",
                      "tool response lines should retain their original boundaries");
    return success;
}

// Verifies that buffers can be converted without first flushing their unfinished tail.
static bool testTranscriptBufferToString() {
    Transcript::Buffer buffer;
    buffer.append("quote \" and newline\n");
    buffer.append("slash \\ and UTF-8 ✓");
    return expect(buffer.toString() == "quote \" and newline\nslash \\ and UTF-8 ✓",
                  "toString should join completed lines and the unfinished tail");
}

// Runs all transcript cases and returns true only when each one passes.
TestResult runTranscriptTests() {
    TestResult result;
    result.add(testTranscriptBuffer());
    result.add(testTranscriptUpdater());
    result.add(testTranscriptBufferToString());
    if (options.verbose) {
        getStdOut().write(result.isSuccess() ? "Transcript tests passed\n" : "Transcript tests failed\n");
    }
    return result;
}
