{title text="Functors" include="ply-base.h" namespace="ply"}

`Functor` is a class template that stores arbitrary callback functions taking both explicit and hidden arguments. Often used to store [lambda expressions](https://en.cppreference.com/w/cpp/language/lambda.html).

It's movable and copyable if the callable is copyable.

{api_summary class=Functor}
-- Additional Constructors
template <typename T> Functor(const T& callable)
-- Invocation
explicit operator bool() const
Return operator()(CallArgs&&... args) const
{/api_summary}

{api_descriptions class=Functor}
template <typename T> Functor(const T& callable)
--
Constructs a functor from any callable object. The callable is copied into internal storage.

>>
explicit operator bool() const
--
Returns `true` if the functor holds a callable, `false` if empty.

>>
Return operator()(CallArgs&&... args) const
--
Invokes the wrapped callable with the given arguments and returns its result.
{/api_descriptions}

{example}
Functor<int(int, int)> add = [](int a, int b) { return a + b; };
int result = add(3, 4);  // result is 7
{/example}
