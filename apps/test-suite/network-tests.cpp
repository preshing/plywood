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
    if (!listener) {
        // Some environments prohibit opening listening sockets; still reject other failures.
        check(Network::lastResult() == NetResult::AccessDenied);
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
        stoppedAccept.store(!connection && Network::lastResult() == NetResult::NotListening, MemoryOrder::Release);
        acceptReturned.store(true, MemoryOrder::Release);
    }};
    aboutToAccept.wait();
    sleepMillis(50);
    check(!acceptReturned.load(MemoryOrder::Acquire));

    // The pending accept must return null with the stop-specific result.
    listener->stopListening();
    acceptThread.join();
    check(acceptReturned.load(MemoryOrder::Acquire));
    check(stoppedAccept.load(MemoryOrder::Acquire));
    check(!listener->isListening());

    listener.clear();
    Network::shutdown();
}
