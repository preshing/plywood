/*────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ____      Plywood C++ Runtime Library                       │
│    ╱   ╱╲     https://plywood.dev/                              │
│   ╱___╱╭╮╲                                                      │
│    └──┴┴┴┘    test-suite                                        │
│               Documentation: /docs/apps/test-suite.md           │
│                                                                 │
└────────────────────────────────────────────────────────────────*/

#include "run-system-tests.h"
#include <ply-network.h>

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Network_

NETWORK_TEST_CASE("TCPListener::stopListening unblocks accept") {
    Network::initialize(IPv4);
    Owned<TCPListener> listener = TCPListener::create(0);
    if (!check(listener)) {
        Network::shutdown();
        return;
    }

    // Start an accept call and give it time to block before stopping the listener.
    Semaphore aboutToAccept;
    Atomic<bool> acceptReturned = false;
    Atomic<bool> stoppedAccept = false;
    Thread acceptThread{[&] {
        aboutToAccept.signal();
        Owned<TCPConnection> connection = listener->accept();
        stoppedAccept.store(!connection && Network::lastResult() == IPResult::NOT_LISTENING, Release);
        acceptReturned.store(true, Release);
    }};
    aboutToAccept.wait();
    sleepMillis(50);
    check(!acceptReturned.load(Acquire));

    // The pending accept must return null with the stop-specific result.
    listener->stopListening();
    acceptThread.join();
    check(acceptReturned.load(Acquire));
    check(stoppedAccept.load(Acquire));
    check(!listener->isListening());

    listener.clear();
    Network::shutdown();
}
