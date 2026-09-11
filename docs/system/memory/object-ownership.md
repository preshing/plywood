Object Ownership (`ply-system.h`)
===============================

## `Owned`

`Owned` is a smart pointer that takes sole ownership of a heap-allocated object. When the `Owned` is destroyed, the object is automatically destroyed and its memory is freed.

`Owned` is movable but not copyable.

`Owned::Owned(Item* ptr)`
> Takes ownership of the given pointer. The object will be destroyed when this `Owned` is destroyed.

`template <typename Derived> Owned::Owned(Owned<Derived>&& other)`
> Move constructor that accepts a derived type. Enables polymorphic ownership.

`Owned& Owned::operator=(Item* ptr)`
> Destroys the currently owned object (if any) and takes ownership of `ptr`.

`Owned& Owned::operator=(Owned&& other)`
> Move assignment. Destroys the current object and takes ownership from `other`.

`template <typename Derived> Owned& Owned::operator=(Owned<Derived>&& other)`
> Move assignment from a derived type.

`Item* Owned::get() const`
> Returns the raw pointer without transferring ownership.

`Owned::operator Item*() const`
> Implicitly converts to a raw pointer. The `Owned` retains ownership.

`Item* Owned::operator->() const`
> Provides member access to the owned object.

`void Owned::clear()`
> Destroys the owned object and resets to empty.

`Item* Owned::release()`
> Releases ownership and returns the raw pointer. The caller becomes responsible for destroying the object.

## `Reference`

`Reference` is a reference-counting smart pointer. Multiple `Reference` objects can share ownership of the same object.

The object is automatically destroyed when the last `Reference` to it is destroyed.

It's thread-safe when the target `Item` type derives from `RefCounted`.

`Reference::Reference()`
> Constructs an empty reference.

`Reference::Reference(item* ptr)`
> Takes a reference to the given object and increments its reference count.

`Reference::Reference(const Reference& ref)`
> Copy constructor. Both references share ownership; the reference count is incremented.

`Reference::Reference(Reference&& ref)`
> Move constructor. Takes ownership without changing the reference count.

`Reference& Reference::operator=(item* ptr)`
> Releases the current reference (decrementing its count) and takes a reference to `ptr`.

`Reference& Reference::operator=(const Reference& ref)`
> Copy assignment. Releases the current reference and shares ownership with `ref`.

`Reference& Reference::operator=(Reference&& ref)`
> Move assignment. Releases the current reference and takes ownership from `ref`.

`item* Reference::operator->() const`
> Provides member access to the referenced object.

`Reference::operator item*() const`
> Implicitly converts to a raw pointer.

`explicit Reference::operator bool() const`
> Returns `true` if this reference points to an object.

`void Reference::clear()`
> Releases the reference, decrementing the object's reference count. May destroy the object.

`item* Reference::release()`
> Releases ownership without decrementing the reference count. The caller becomes responsible for the reference.

## `RefCounted`

`RefCounted` is a base class that provides reference counting functionality. Derive your class from `RefCounted` to use it with `Reference` smart pointers.

`void RefCounted::incRefCount()`
> Increments the reference count. Called automatically by `Reference` when a new reference is created.

`void RefCounted::decRefCount()`
> Decrements the reference count. If the count reaches zero, the object is destroyed. Called automatically by `Reference` when a reference is released.

`s32 RefCounted::getRefCount() const`
> Returns the current reference count. Useful for debugging and assertions.
