{title text="Threads" include="ply-base.h" namespace="ply"}

Plywood provides portable threading primitives that wrap the platform's native threading API. These include threads, mutexes, condition variables, and atomic operations.

{api_summary}
PID get_current_process_id()
TID get_current_thread_id()
void sleep_millis(u32 millis)
{/api_summary}

{api_descriptions}
PID get_current_process_id()
--
Returns the operating system's process ID for the current process.

>>
TID get_current_thread_id()
--
Returns the operating system's thread ID for the current thread.

>>
void sleep_millis(u32 millis)
--
Suspends the current thread for the specified number of milliseconds.
{/api_descriptions}

## `Thread`

A `Thread` represents a separate thread of execution.

{api_summary class=Thread}
bool is_valid()
void run<Callable>(Callable& callable)
void join()
{/api_summary}

{api_descriptions class=Thread}
bool is_valid()
--
Returns `true` if the thread object represents a running or joinable thread.

>>
void run<Callable>(Callable& callable)
--
Starts a new thread that executes the given callable object. The callable can be a lambda, functor, or any object with `operator()`.

>>
void join()
--
Blocks until the thread finishes execution. Must be called before the `Thread` object is destroyed.
{/api_descriptions}

## `Atomic`

`Atomic` provides atomic operations on integer types with explicit memory ordering. The memory ordering is specified in the function name: `acquire` for load operations, `release` for store operations, and `acq_rel` for read-modify-write operations.

{api_summary class=Atomic}
Atomic(T value = 0)
Atomic(const Atomic<T>& other)
void operator=(const Atomic<T>& other)
T load_nonatomic() const
T load_acquire() const
void store_nonatomic(T value)
void store_release(T value)
T compare_exchange_acq_rel(T expected, T desired)
T exchange_acq_rel(T desired)
T fetch_add_acq_rel(T operand)
T fetch_sub_acq_rel(T operand)
T fetch_and_acq_rel(T operand)
T fetch_or_acq_rel(T operand)
{/api_summary}

{api_descriptions class=Atomic}
Atomic(T value = 0)
--
Constructs an atomic with the given initial value.

>>
Atomic(const Atomic<T>& other)
--
Copy constructor. Performs an atomic load from `other`.

>>
void operator=(const Atomic<T>& other)
--
Copy assignment. Performs an atomic load and store.

>>
T load_nonatomic() const
--
Reads the value without atomic guarantees. Only safe when no other thread is writing.

>>
T load_acquire() const
--
Atomically reads the value with acquire semantics.

>>
void store_nonatomic(T value)
--
Writes the value without atomic guarantees. Only safe when no other thread is reading.

>>
void store_release(T value)
--
Atomically writes the value with release semantics.

>>
T compare_exchange_acq_rel(T expected, T desired)
--
If the current value equals `expected`, replaces it with `desired`. Returns the previous value.

>>
T exchange_acq_rel(T desired)
--
Atomically replaces the value and returns the previous value.

>>
T fetch_add_acq_rel(T operand)
--
Atomically adds `operand` to the value and returns the previous value.

>>
T fetch_sub_acq_rel(T operand)
--
Atomically subtracts `operand` from the value and returns the previous value.

>>
T fetch_and_acq_rel(T operand)
--
Atomically performs bitwise AND with `operand` and returns the previous value.

>>
T fetch_or_acq_rel(T operand)
--
Atomically performs bitwise OR with `operand` and returns the previous value.
{/api_descriptions}

## `ThreadLocal`

`ThreadLocal` provides per-thread storage. Each thread sees its own independent value.

{api_summary class=ThreadLocal}
ThreadLocal()
ThreadLocal(const ThreadLocal&) = delete;
U load() const
void store(T value)
Scope set_in_scope(T value)
{/api_summary}

{api_descriptions class=ThreadLocal}
ThreadLocal()
--
Constructs a thread-local variable. Each thread's value is initially zero/null.

>>
ThreadLocal(const ThreadLocal&) = delete;
--
Thread-local variables cannot be copied.

>>
U load() const
--
Returns the current thread's value.

>>
void store(T value)
--
Sets the current thread's value.

>>
Scope set_in_scope(T value)
--
Sets the value for the duration of a scope. The previous value is restored when the scope ends.
{/api_descriptions}

## `Mutex`

A `Mutex` provides mutual exclusion to protect shared data. Use `LockGuard` for RAII-style locking.

{api_summary class=Mutex}
void lock()
bool try_lock()
void unlock()
{/api_summary}

{api_descriptions class=Mutex}
void lock()
--
Acquires the mutex, blocking if another thread holds it.

>>
bool try_lock()
--
Attempts to acquire the mutex without blocking. Returns `true` if successful.

>>
void unlock()
--
Releases the mutex.
{/api_descriptions}

`LockGuard<MutexType>` is a RAII wrapper that locks a mutex in its constructor and unlocks it in its destructor:

    LockGuard<Mutex> guard{my_mutex};  // mutex is locked
    // ... critical section ...
    // mutex is unlocked when guard goes out of scope

## `ConditionVariable`

A `ConditionVariable` allows threads to wait for a condition to become true. Always use with a mutex to protect the condition.

{api_summary class=ConditionVariable}
void wait(LockGuard<Mutex>& lock_guard)
void timed_wait(LockGuard<Mutex>& lock_guard, u32 wait_millis)
void wake_all()
{/api_summary}

{api_descriptions class=ConditionVariable}
void wait(LockGuard<Mutex>& lock_guard)
--
Atomically releases the mutex and waits for a signal. Re-acquires the mutex before returning.

>>
void timed_wait(LockGuard<Mutex>& lock_guard, u32 wait_millis)
--
Like `wait`, but returns after `wait_millis` milliseconds even if not signaled.

>>
void wake_all()
--
Wakes all threads waiting on this condition variable.
{/api_descriptions}

## `ReadWriteLock`

A `ReadWriteLock` allows multiple readers or a single writer. This is efficient when reads are much more common than writes.

{api_summary class=ReadWriteLock}
void lock_exclusive()
void unlock_exclusive()
void lock_shared()
void unlock_shared()
{/api_summary}

{api_descriptions class=ReadWriteLock}
void lock_exclusive()
--
Acquires exclusive (write) access. Blocks until all readers and writers have released the lock.

>>
void unlock_exclusive()
--
Releases exclusive access.

>>
void lock_shared()
--
Acquires shared (read) access. Multiple threads can hold shared access simultaneously.

>>
void unlock_shared()
--
Releases shared access.
{/api_descriptions}

## `Semaphore`

A `Semaphore` is a signaling mechanism that maintains a count. Threads can wait for the count to be positive and decrement it, or signal to increment the count.

{api_summary class=Semaphore}
void wait()
void signal(u32 count = 1)
{/api_summary}

{api_descriptions class=Semaphore}
void wait()
--
Blocks until the count is positive, then decrements it.

>>
void signal(u32 count = 1)
--
Increments the count by `count`, potentially waking waiting threads.
{/api_descriptions}
