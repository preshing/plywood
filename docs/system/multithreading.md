Multithreading (`ply-system.h`)
==============================

`TID getCurrentThreadId()`
> Returns the operating system's thread ID for the current thread. See also `getCurrentProcessId`.

`void sleepMillis(u32 millis)`
> Suspends the current thread for the specified number of milliseconds.

## `Thread`

A `Thread` represents a separate thread of execution.

`bool Thread::isValid()`
> Returns `true` if the thread object represents a running or joinable thread.

`void Thread::run<Callable>(Callable& callable)`
> Starts a new thread that executes the given callable object. The callable can be a lambda, functor, or any object with `operator()`.

`void Thread::detach()`
> Releases the `Thread` object's ownership of the running thread without waiting for it to finish. After this call, `isValid()` returns `false` and the thread continues running independently.

`void Thread::join()`
> Blocks until the thread finishes execution, then releases the thread handle. After this call, `isValid()` returns `false`.

Destroying a valid `Thread` object implicitly detaches it if you have not already called `join()` or `detach()`.

## `Atomic`

`Atomic` provides atomic operations on integer types with explicit memory ordering via a `MemoryOrder` argument.

```
enum class MemoryOrder {
    Relaxed,
    Acquire,
    Release,
    AcqRel,
};
```

`Atomic<T>::Atomic(T value = 0)`
> Constructs an atomic with the given initial value.

`Atomic<T>::Atomic(const Atomic<T>& other)`
> Copy constructor with no memory ordering guarantees.

`void Atomic<T>::operator=(const Atomic<T>& other)`
> Copy assignment with no memory ordering guarantees. Should only be called when there is no concurrent access to the destination.

`T Atomic<T>::load(MemoryOrder order) const`
> Atomically reads the value with the specified memory order.

`void Atomic<T>::store(T value, MemoryOrder order)`
> Atomically writes the value with the specified memory order.

`T Atomic<T>::compareExchange(T expected, T desired, MemoryOrder order)`
> If the current value equals `expected`, replaces it with `desired`. Returns the previous value.

`T Atomic<T>::exchange(T desired, MemoryOrder order)`
> Atomically replaces the value and returns the previous value.

`T Atomic<T>::fetchAdd(T operand, MemoryOrder order)`
> Atomically adds `operand` to the value and returns the previous value.

`T Atomic<T>::fetchSub(T operand, MemoryOrder order)`
> Atomically subtracts `operand` from the value and returns the previous value.

`T Atomic<T>::fetchAnd(T operand, MemoryOrder order)`
> Atomically performs bitwise AND with `operand` and returns the previous value.

`T Atomic<T>::fetchOr(T operand, MemoryOrder order)`
> Atomically performs bitwise OR with `operand` and returns the previous value.

## `ThreadLocal`

`ThreadLocal` provides per-thread storage. Each thread sees its own independent value.

`ThreadLocal<T>::ThreadLocal()`
> Constructs a thread-local variable. Each thread's value is initially zero/null.

`ThreadLocal<T>::ThreadLocal(const ThreadLocal&) = delete;`
> Thread-local variables cannot be copied.

`U ThreadLocal<T>::load() const`
> Returns the current thread's value.

`void ThreadLocal<T>::store(T value)`
> Sets the current thread's value.

`Scope ThreadLocal<T>::setInScope(T value)`
> Sets the value for the duration of a scope. The previous value is restored when the scope ends.

## `Mutex`

A `Mutex` provides mutual exclusion to protect shared data. Use `LockGuard` for RAII-style locking.

`void Mutex::lock()`
> Acquires the mutex, blocking if another thread holds it.

`bool Mutex::tryLock()`
> Attempts to acquire the mutex without blocking. Returns `true` if successful.

`void Mutex::unlock()`
> Releases the mutex.

`LockGuard<MutexType>` is a RAII wrapper that locks a mutex in its constructor and unlocks it in its destructor:

```
LockGuard<Mutex> guard{myMutex};  // The mutex is locked here.
// ... critical section ...
// The mutex is unlocked when the guard goes out of scope.
```

## `ConditionVariable`

A `ConditionVariable` allows threads to wait for a condition to become true. Always use with a mutex to protect the condition.

`void ConditionVariable::wait(LockGuard<Mutex>& lockGuard)`
> Atomically releases the mutex and waits for a signal. Re-acquires the mutex before returning.

`void ConditionVariable::timedWait(LockGuard<Mutex>& lockGuard, u32 waitMillis)`
> Like `wait`, but returns after `waitMillis` milliseconds even if not signaled.

`void ConditionVariable::wakeAll()`
> Wakes all threads waiting on this condition variable.

## `ReadWriteLock`

A `ReadWriteLock` allows multiple readers or a single writer.

`void ReadWriteLock::lockExclusive()`
> Acquires exclusive (write) access. Blocks until all readers and writers have released the lock.

`void ReadWriteLock::unlockExclusive()`
> Releases exclusive access.

`void ReadWriteLock::lockShared()`
> Acquires shared (read) access. Multiple threads can hold shared access simultaneously.

`void ReadWriteLock::unlockShared()`
> Releases shared access.

## `Semaphore`

A `Semaphore` is a signaling mechanism that maintains a count. Threads can wait for the count to be positive and decrement it, or signal to increment the count.

`void Semaphore::wait()`
> Blocks until the count is positive, then decrements it.

`void Semaphore::signal(u32 count = 1)`
> Increments the count by `count`, potentially waking waiting threads.
