{title text="Object Ownership" include="ply-base.h" namespace="ply"}

## `Owned`

`Owned` is a smart pointer that takes sole ownership of a heap-allocated object. When the `Owned` is destroyed, the object is automatically destroyed and its memory is freed.

`Owned` is movable but not copyable.

{api_summary class=Owned}
-- Additional Constructors
Owned(Item* ptr)
template <typename Derived> Owned(Owned<Derived>&& other)
-- Additional Assignment Operators
Owned& operator=(Item* ptr)
template <typename Derived> Owned& operator=(Owned<Derived>&& other)
-- Item access
Item* get() const
operator Item*() const
Item* operator->() const
-- Modification
void clear()
Item* release()
{/api_summary}

{api_descriptions class=Owned}
Owned(Item* ptr)
--
Takes ownership of the given pointer. The object will be destroyed when this `Owned` is destroyed.

>>
template <typename Derived> Owned(Owned<Derived>&& other)
--
Move constructor that accepts a derived type. Enables polymorphic ownership.

>>
Owned& operator=(Item* ptr)
--
Destroys the currently owned object (if any) and takes ownership of `ptr`.

>>
Owned& operator=(Owned&& other)
--
Move assignment. Destroys the current object and takes ownership from `other`.

>>
template <typename Derived> Owned& operator=(Owned<Derived>&& other)
--
Move assignment from a derived type.

>>
Item* get() const
--
Returns the raw pointer without transferring ownership.

>>
operator Item*() const
--
Implicitly converts to a raw pointer. The `Owned` retains ownership.

>>
Item* operator->() const
--
Provides member access to the owned object.

>>
void clear()
--
Destroys the owned object and resets to empty.

>>
Item* release()
--
Releases ownership and returns the raw pointer. The caller becomes responsible for destroying the object.
{/api_descriptions}

## `Reference`

`Reference` is a reference-counting smart pointer. Multiple `Reference` objects can share ownership of the same object.

{api_summary class=Reference}
-- Constructors
Reference()
Reference(Item* ptr)
Reference(const Reference& ref)
Reference(Reference&& ref)
-- Assignment
Reference& operator=(Item* ptr)
Reference& operator=(const Reference& ref)
Reference& operator=(Reference&& ref)
-- Item access
Item* operator->() const
operator Item*() const
explicit operator bool() const
-- Modification
void clear()
Item* release()
{/api_summary}

The object is automatically destroyed when the last `Reference` to it is destroyed.

It's thread-safe when the target `Item` type derives from `RefCounted`.

{api_descriptions class=Reference}
Reference()
--
Constructs an empty reference.

>>
Reference(item* ptr)
--
Takes a reference to the given object and increments its reference count.

>>
Reference(const Reference& ref)
--
Copy constructor. Both references share ownership; the reference count is incremented.

>>
Reference(Reference&& ref)
--
Move constructor. Takes ownership without changing the reference count.

>>
Reference& operator=(item* ptr)
--
Releases the current reference (decrementing its count) and takes a reference to `ptr`.

>>
Reference& operator=(const Reference& ref)
--
Copy assignment. Releases the current reference and shares ownership with `ref`.

>>
Reference& operator=(Reference&& ref)
--
Move assignment. Releases the current reference and takes ownership from `ref`.

>>
item* operator->() const
--
Provides member access to the referenced object.

>>
operator item*() const
--
Implicitly converts to a raw pointer.

>>
explicit operator bool() const
--
Returns `true` if this reference points to an object.

>>
void clear()
--
Releases the reference, decrementing the object's reference count. May destroy the object.

>>
item* release()
--
Releases ownership without decrementing the reference count. The caller becomes responsible for the reference.
{/api_descriptions}

## `RefCounted`

`RefCounted` is a base class that provides reference counting functionality. Derive your class from `RefCounted` to use it with `Reference` smart pointers.

{api_summary class=RefCounted}
void inc_ref_count()
void dec_ref_count()
s32 get_ref_count() const
{/api_summary}

{api_descriptions class=RefCounted}
void inc_ref_count()
--
Increments the reference count. Called automatically by `Reference` when a new reference is created.

>>
void dec_ref_count()
--
Decrements the reference count. If the count reaches zero, the object is destroyed. Called automatically by `Reference` when a reference is released.

>>
s32 get_ref_count() const
--
Returns the current reference count. Useful for debugging and assertions.
{/api_descriptions}
