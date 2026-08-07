Multithreading (`ply-base.h`)
==============================

Multithreading is the co-ordination of multiple threads of execution within a single process. Access atomic operations, thread-local variables, mutexes, condition variables, semaphores and read-write locks.

`TID getCurrentThreadId()`
> Returns the operating system's thread ID for the current thread. See also `getCurrentProcessId`.

`void sleepMillis(u32 millis)`
> Suspends the current thread for the specified number of milliseconds.

## `Thread`

A `Thread` represents a separate thread of execution.

{context class=Thread}

`bool isValid()`
> Returns `true` if the thread object represents a running or joinable thread.

`void run<Callable>(Callable& callable)`
> Starts a new thread that executes the given callable object. The callable can be a lambda, functor, or any object with `operator()`.

`void detach()`
> Releases the `Thread` object's ownership of the running thread without waiting for it to finish. After this call, `isValid()` returns `false` and the thread continues running independently.

`void join()`
> Blocks until the thread finishes execution, then releases the thread handle. After this call, `isValid()` returns `false`.

Destroying a valid `Thread` object implicitly detaches it if you have not already called `join()` or `detach()`.

## `Atomic`

`Atomic` provides atomic operations on integer types with explicit memory ordering via a `MemoryOrder` argument. Must be aligned to the size of its template argument.

```
enum MemoryOrder {
    Relaxed,
    Acquire,
    Release,
    AcqRel
};
```

{context class=Atomic}

`Atomic(T value = 0)`
> Constructs an atomic with the given initial value.

`Atomic(const Atomic<T>& other)`
> Copy constructor with no memory ordering guarantees.

`void operator=(const Atomic<T>& other)`
> Copy assignment with no memory ordering guarantees. Should only be called when there is no concurrent access to the destination.

`T load(MemoryOrder order) const`
> Atomically reads the value with the specified memory order.

`void store(T value, MemoryOrder order)`
> Atomically writes the value with the specified memory order.

`T compareExchange(T expected, T desired, MemoryOrder order)`
> If the current value equals `expected`, replaces it with `desired`. Returns the previous value.

`T exchange(T desired, MemoryOrder order)`
> Atomically replaces the value and returns the previous value.

`T fetchAdd(T operand, MemoryOrder order)`
> Atomically adds `operand` to the value and returns the previous value.

`T fetchSub(T operand, MemoryOrder order)`
> Atomically subtracts `operand` from the value and returns the previous value.

`T fetchAnd(T operand, MemoryOrder order)`
> Atomically performs bitwise AND with `operand` and returns the previous value.

`T fetchOr(T operand, MemoryOrder order)`
> Atomically performs bitwise OR with `operand` and returns the previous value.

## `ThreadLocal`

`ThreadLocal` provides per-thread storage. Each thread sees its own independent value.

{context class=ThreadLocal}

`ThreadLocal()`
> Constructs a thread-local variable. Each thread's value is initially zero/null.

`ThreadLocal(const ThreadLocal&) = delete;`
> Thread-local variables cannot be copied.

`U load() const`
> Returns the current thread's value.

`void store(T value)`
> Sets the current thread's value.

`Scope setInScope(T value)`
> Sets the value for the duration of a scope. The previous value is restored when the scope ends.

## `Mutex`

A `Mutex` provides mutual exclusion to protect shared data. Use `LockGuard` for RAII-style locking.

{context class=Mutex}

`void lock()`
> Acquires the mutex, blocking if another thread holds it.

`bool tryLock()`
> Attempts to acquire the mutex without blocking. Returns `true` if successful.

`void unlock()`
> Releases the mutex.

`LockGuard<MutexType>` is a RAII wrapper that locks a mutex in its constructor and unlocks it in its destructor:

    LockGuard<Mutex> guard{myMutex};  // mutex is locked
    // ... critical section ...
    // mutex is unlocked when guard goes out of scope

## `ConditionVariable`

A `ConditionVariable` allows threads to wait for a condition to become true. Always use with a mutex to protect the condition.

{context class=ConditionVariable}

`void wait(LockGuard<Mutex>& lockGuard)`
> Atomically releases the mutex and waits for a signal. Re-acquires the mutex before returning.

`void timedWait(LockGuard<Mutex>& lockGuard, u32 waitMillis)`
> Like `wait`, but returns after `waitMillis` milliseconds even if not signaled.

`void wakeAll()`
> Wakes all threads waiting on this condition variable.

## `ReadWriteLock`

A `ReadWriteLock` allows multiple readers or a single writer. This is efficient when reads are much more common than writes.

{context class=ReadWriteLock}

`void lockExclusive()`
> Acquires exclusive (write) access. Blocks until all readers and writers have released the lock.

`void unlockExclusive()`
> Releases exclusive access.

`void lockShared()`
> Acquires shared (read) access. Multiple threads can hold shared access simultaneously.

`void unlockShared()`
> Releases shared access.

## `Semaphore`

A `Semaphore` is a signaling mechanism that maintains a count. Threads can wait for the count to be positive and decrement it, or signal to increment the count.

{context class=Semaphore}

`void wait()`
> Blocks until the count is positive, then decrements it.

`void signal(u32 count = 1)`
> Increments the count by `count`, potentially waking waiting threads.
