/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Base Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘   
========================================================*/

#include "test-suite.h"
#include <ply-btree.h>
#include <ply-math.h>

//  ▄▄▄▄▄▄ ▄▄                              ▄▄
//    ██   ██▄▄▄  ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄   ▄▄▄██
//    ██   ██  ██ ██  ▀▀ ██▄▄██  ▄▄▄██ ██  ██
//    ██   ██  ██ ██     ▀█▄▄▄  ▀█▄▄██ ▀█▄▄██
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Thread_

TEST_CASE("Thread join") {
    int value = 0;
    Thread thread([&]() {
        value = 42;
    });
    thread.join();
    check(value == 42);
}

//  ▄▄  ▄▄               ▄▄     ▄▄
//  ██  ██  ▄▄▄▄   ▄▄▄▄  ██▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//  ██▀▀██  ▄▄▄██ ▀█▄▄▄  ██  ██ ██ ██  ██ ██  ██
//  ██  ██ ▀█▄▄██  ▄▄▄█▀ ██  ██ ██ ██  ██ ▀█▄▄██
//                                         ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Hashing_

TEST_CASE("shuffle_bits() 32") {
    Random rand;
    for (u32 i = 0; i < 1000; i++) {
        u32 value = rand.generate_u32();
        u32 shuffled = shuffle_bits(value);
        u32 unshuffled = unshuffle_bits(shuffled);
        check(value == unshuffled);
    }
}

TEST_CASE("shuffle_bits() 64") {
    Random rand;
    for (u32 i = 0; i < 1000; i++) {
        u64 value = rand.generate_u32();
        u64 shuffled = shuffle_bits(value);
        u64 unshuffled = unshuffle_bits(shuffled);
        check(value == unshuffled);
    }
}

//   ▄▄▄▄   ▄▄          ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄  ▄▄ ▄▄▄▄▄   ▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██ ██  ██ ██  ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ██ ██  ██ ▀█▄▄██
//                                 ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX String_

TEST_CASE("String self-assignment") {
    String str = "How now brown cow?";
    str = str.shortened_by(1);
    check(str == "How now brown cow");
}

TEST_CASE("String find") {
    String str = "abcdefgh";
    check(str.find([](char x) { return x == 'c'; }) == 2);
    check(str.find([](char x) { return x == 'z'; }) < 0);
    check(str.find('c') == 2);
    check(str.find('z') < 0);
}

TEST_CASE("String reverse_find") {
    String str = "abcdefgh";
    check(str.reverse_find([](char x) { return x == 'c'; }) == 2);
    check(str.reverse_find([](char x) { return x == 'z'; }) < 0);
    check(str.reverse_find('c') == 2);
    check(str.reverse_find('z') < 0);
}

TEST_CASE("String match identifier") {
    String str = "(hello)";
    StringView identifier;
    check(str.match("'(%i')$", &identifier));
    check(identifier == "hello");
}

TEST_CASE("String match integer") {
    StringView str = "count: 42";
    s32 value = 0;
    check(str.match("count: %d", &value));
    check(value == 42);
}

TEST_CASE("String match negative integer") {
    StringView str = "offset=-123";
    s32 value = 0;
    check(str.match("offset=%d", &value));
    check(value == -123);
}

TEST_CASE("String match float") {
    StringView str = "pi=3.14159";
    double value = 0;
    check(str.match("pi=%f", &value));
    check(value > 3.14 && value < 3.15);
}

TEST_CASE("String match quoted string") {
    StringView str = "name=\"hello world\"";
    String value;
    check(str.match("name=%q", &value));
    check(value == "hello world");
}

TEST_CASE("String match whitespace") {
    StringView str = "hello   world";
    check(str.match("hello *world"));
}

TEST_CASE("String match whitespace with tab") {
    StringView str = "hello\tworld";
    check(str.match("hello world"));
}

TEST_CASE("String match optional whitespace") {
    StringView str1 = "hello world";
    StringView str2 = "helloworld";
    check(str1.match("hello ?world"));
    check(str2.match("hello ?world"));
}

TEST_CASE("String match escape character") {
    StringView str = "%test%";
    StringView id;
    check(str.match("'%%i'%", &id));
    check(id == "test");
}

TEST_CASE("String match group alternation") {
    StringView str1 = "color: red";
    StringView str2 = "color: blue";
    StringView str3 = "color: green";
    check(str1.match("color: (red|blue|green)"));
    check(str2.match("color: (red|blue|green)"));
    check(str3.match("color: (red|blue|green)"));
}

TEST_CASE("String match group alternation fail") {
    StringView str = "color: yellow";
    check(!str.match("color: (red|blue|green)"));
}

TEST_CASE("String match zero or more") {
    StringView str1 = "ab";
    StringView str2 = "aab";
    StringView str3 = "aaab";
    check(str1.match("a*b"));
    check(str2.match("a*b"));
    check(str3.match("a*b"));
}

TEST_CASE("String match zero or more empty") {
    StringView str = "b";
    check(str.match("a*b"));
}

TEST_CASE("String match optional character") {
    StringView str1 = "color";
    StringView str2 = "colour";
    check(str1.match("colou?r"));
    check(str2.match("colou?r"));
}

TEST_CASE("String match group zero or more") {
    StringView str1 = "start end";
    StringView str2 = "start foo end";
    StringView str3 = "start foo foo foo end";
    check(str1.match("start (foo )*end"));
    check(str2.match("start (foo )*end"));
    check(str3.match("start (foo )*end"));
}

TEST_CASE("String match nested groups") {
    check(StringView{""}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple, banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"apple,banana"}.match("((apple|banana)(, *)?)*$"));
    check(StringView{"banana, apple, banana"}.match("((apple|banana)(, *)?)*$"));
    check(!StringView{"orange"}.match("((apple|banana)(, *)?)*$"));
}

TEST_CASE("String match end anchor") {
    StringView str = "hello";
    check(str.match("hello$"));
    check(!str.match("hell$"));
}

TEST_CASE("String match multiple captures") {
    StringView str = "point(10, 20)";
    s32 x = 0, y = 0;
    check(str.match("point'(%d, %d')", &x, &y));
    check(x == 10);
    check(y == 20);
}

TEST_CASE("String match optional format specifier") {
    s32 value = -1;
    check(StringView{"item"}.match("item%d?$", &value));
    check(value == -1);  // Unchanged since no number present
    check(StringView{"item42"}.match("item%d?$", &value));
    check(value == 42);
    check(!StringView{"item42extra"}.match("item%d?$", &value));
}

TEST_CASE("String match optional group with format specifier") {
    s32 num = -1;
    String text;
    // Test optional group with alternation containing format specifiers
    check(StringView{"start end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(num == -1);  // Unchanged
    check(text.is_empty());
    num = -1;
    check(StringView{"start num=42 end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(num == 42);
    num = -1;
    text = {};
    check(StringView{"start text=\"hello\" end"}.match("start (num=%d|text=%q)? ?end$", &num, &text));
    check(text == "hello");
}

TEST_CASE("String match literal characters") {
    StringView str = "abc123xyz";
    check(str.match("abc123xyz"));
    check(!str.match("abc456xyz"));
}

//   ▄▄▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄▄▄▄   ▄▄▄▄  ▄▄  ▄▄
//  ██▀▀██ ██  ▀▀ ██  ▀▀  ▄▄▄██ ██  ██
//  ██  ██ ██     ██     ▀█▄▄██ ▀█▄▄██
//                               ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Array_

//--------------------------------
// Constructors
//--------------------------------
TEST_CASE("Array default constructor") {
    Array<u32> a;
    check(a == ArrayView<const u32>{});
}

TEST_CASE("Array construct from braced initializer list") {
    Array<u32> a = {4, 5, 6};
    check(a == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array move constructor") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = std::move(a);
    check(a == ArrayView<const u32>{});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Copy construct Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array construct from Fixed_Array") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Move construct Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b = std::move(a);
    check(a.is_empty());
    check(b == ArrayView<const StringView>{"hello", "there"});
}

//--------------------------------
// Assignment Operators
//--------------------------------
TEST_CASE("Array assign from braced initializer list") {
    Array<u32> a;
    a = {4, 5, 6};
    check(a == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array copy assignment") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b;
    b = a;
    check(a == ArrayView<const u32>{4, 5, 6});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array move assignment") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b = a;
    b = std::move(a);
    check(a == ArrayView<const u32>{});
    check(b == ArrayView<const u32>{4, 5, 6});
}

TEST_CASE("Array assign, no move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array assign from Fixed_Array") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b = a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Move assign Array<String>") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b = std::move(a);
    check(a.is_empty());
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array move assign from Fixed_Array") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b = std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array self-assignment") {
    Array<u32> a = {1, 1, 2, 3, 5, 8};
    a = a.subview(1);
    check(a == ArrayView<const u32>{1, 2, 3, 5, 8});
}

//--------------------------------
// Element Access
//--------------------------------
TEST_CASE("Array subscript lookup") {
    const Array<u32> a = {4, 5, 6};
    check(a[0] == 4);
    check(a[1] == 5);
    check(a[2] == 6);
}

TEST_CASE("Array subscript modification") {
    Array<u32> a = {4, 5, 6};
    a[1] = 7;
    check(a == ArrayView<const u32>{4, 7, 6});
}

TEST_CASE("Array back lookup") {
    const Array<u32> a = {4, 5, 6};
    check(a.back() == 6);
    check(a.back(-2) == 5);
}

TEST_CASE("Array back modification") {
    Array<u32> a = {4, 5, 6};
    a.back() = 7;
    check(a == ArrayView<const u32>{4, 5, 7});
}

TEST_CASE("Array iteration") {
    Array<u32> a = {4, 5, 6};
    u32 prev = 3;
    for (u32 i : a) {
        check(i == prev + 1);
        prev = i;
    }
}

TEST_CASE("Array iteration 2") {
    Array<u32> a = {4, 5, 6};
    Array<u32> b;
    for (u32 i : a) {
        b.append(i);
    }
    check(b == ArrayView<const u32>{4, 5, 6});
}

//--------------------------------
// Capacity
//--------------------------------
TEST_CASE("Array operator bool") {
    Array<u32> a;
    check(!(bool) a);
    a = {4, 5, 6};
    check((bool) a);
}

TEST_CASE("Array is_empty") {
    Array<u32> a;
    check(a.is_empty());
    a = {4, 5, 6};
    check(!a.is_empty());
}

TEST_CASE("Array num_items") {
    Array<u32> a;
    check(a.num_items() == 0);
    a = {4, 5, 6};
    check(a.num_items() == 3);
}

//--------------------------------
// Modifers
//--------------------------------
TEST_CASE("Array clear") {
    Array<u32> a = {4, 5, 6};
    a.clear();
    check(a == ArrayView<const u32>{});
}

// FIXME: Add reserve() test?
// Ideally it would measure the number of allocations performed under the hood.

TEST_CASE("Array resize") {
    Array<u32> a;
    a.resize(3);
    check(a.num_items() == 3);
}

TEST_CASE("Array resize 2") {
    Array<String> a;
    a.resize(3);
    check(a == ArrayView<const StringView>{{}, {}, {}});
}

TEST_CASE("Array append, no move semantics") {
    String s0 = "hello";
    String s1 = "there";
    Array<String> a;
    a.append(s0);
    a.append(s1);
    check(s0 == "hello");
    check(s1 == "there");
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array append with move semantics") {
    String s0 = "hello";
    String s1 = "there";
    Array<String> a;
    a.append(std::move(s0));
    a.append(std::move(s1));
    check(s0 == "");
    check(s1 == "");
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array append, no move semantics") {
    String s = "hello";
    Array<String> a;
    a.append(s);
    check(s == "hello");
    check(a == ArrayView<const StringView>{"hello"});
}

TEST_CASE("Array append String with move semantics") {
    String s = "hello";
    Array<String> a;
    a.append(std::move(s));
    check(s == "");
    check(a == ArrayView<const StringView>{"hello"});
}

TEST_CASE("Array extend from braced initializer list") {
    Array<String> a;
    a += {"hello", "there"};
    check(a == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend, no move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend with move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend from FixedArray, no move semantics") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array extend from FixedArray with move semantics") {
    FixedArray<String, 2> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Extend Array<String> without move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += a;
    check(a == ArrayView<const StringView>{"hello", "there"});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Extend Array<String> with move semantics") {
    Array<String> a = {"hello", "there"};
    Array<String> b;
    b += std::move(a);
    check(a == ArrayView<const StringView>{{}, {}});
    check(b == ArrayView<const StringView>{"hello", "there"});
}

TEST_CASE("Array pop") {
    Array<u32> a = {4, 5, 6};
    a.pop();
    check(a == ArrayView<const u32>{4, 5});
    a.pop(2);
    check(a == ArrayView<const u32>{});
}

TEST_CASE("Array insert") {
    Array<u32> a = {4, 5, 6};
    a.insert(2) = 7;
    check(a == ArrayView<const u32>{4, 5, 7, 6});
}

TEST_CASE("Array insert 2") {
    Array<String> a = {"hello", "there"};
    a.insert(1, 2);
    check(a == ArrayView<const StringView>{"hello", {}, {}, "there"});
}

TEST_CASE("Array erase") {
    Array<u32> a = {4, 5, 6};
    a.erase(0);
    check(a == ArrayView<const u32>{5, 6});

    Array<u32> b = {4, 5, 6, 7};
    b.erase(1, 2);
    check(b == ArrayView<const u32>{4, 7});
}

TEST_CASE("Array erase_quick") {
    Array<u32> a = {4, 5, 6};
    a.erase_quick(0);
    check(a == ArrayView<const u32>{6, 5});

    Array<u32> b = {4, 5, 6, 7, 8, 9, 10};
    b.erase_quick(1, 2);
    check(b == ArrayView<const u32>{4, 9, 10, 7, 8});
}

//   ▄▄▄▄          ▄▄
//  ██  ▀▀  ▄▄▄▄  ▄██▄▄
//   ▀▀▀█▄ ██▄▄██  ██
//  ▀█▄▄█▀ ▀█▄▄▄   ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Set_

struct TestHistogramBucket {
    u32 population = 0;
    u32 num_times_occurred = 0;
};

TEST_CASE("Set stress test u32") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {
        {0, 0},
        {1, 0},
        {2, 0},
        {4, 0},
        {8, 0},
        {16, 0},
        {32, 0},
        {64, 0},
        {128, 0},
        {256, 0},
    };
    u32 sum_of_all_populations = 0;
    u32 num_inserts_were_found = 0;
    u32 num_absent_finds = 0;

    // Test setup.
    Set<u32> set;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 2500; iters++) {
        // Ensure the set and mirror array have the same number of items.
        PLY_ASSERT(set.items.num_items() == arr.num_items());

        // Decide what population size the set should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generate_float();
        PLY_ASSERT(exp > 0); // Guaranteed because generate_float returns numbers < 1.
        float random_population = -logf(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desired_population = (u32) clamp(random_population - 4.f, 0.f, 512.f);

        // Add items to the set if needed.
        while (desired_population > set.items.num_items()) {
            u32 value_to_insert = r.generate_u32() % 1000;
            if (set.insert(value_to_insert).was_found) {
                num_inserts_were_found++;
                check(find(arr, value_to_insert) >= 0);
            } else {
                arr.append(value_to_insert);
            }
        }

        // Remove items from the set if needed.
        while (desired_population < arr.num_items()) {
            u32 index_to_remove = r.generate_u32() % arr.num_items();
            u32 value_to_remove = arr[index_to_remove];
            bool was_found = set.erase(value_to_remove);
            check(was_found);
            arr.erase_quick(index_to_remove);
        }

        // Check its population.
        check(desired_population == set.items.num_items());
        check(desired_population == arr.num_items());
        for (s32 i = histogram.num_items() - 1; i >= 0; i--) {
            if (desired_population >= histogram[i].population) {
                histogram[i].num_times_occurred++;
                break;
            }
        }
        sum_of_all_populations += desired_population;

        // Test find.
        sort(arr);
        for (u32 i = 0; i < arr.num_items(); i++) {
            check(set.find(arr[i]));
            if (i > 0) {
                check(arr[i] > arr[i - 1]); // No duplicates.
                u32 delta = arr[i] - arr[i - 1];
                if (delta > 1) {
                    u32 absent_key = arr[i - 1] + 1 + (r.generate_u32() % (delta - 1));
                    check(!set.find(absent_key));
                    num_absent_finds++;
                }
            }
        }
    }
}

//  ▄▄▄▄▄  ▄▄▄▄▄▄
//  ██  ██   ██   ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄
//  ██▀▀█▄   ██   ██  ▀▀ ██▄▄██ ██▄▄██
//  ██▄▄█▀   ██   ██     ▀█▄▄▄  ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX BTree_

TEST_CASE("BTree stress test u32") {
    // Metrics collection.
    Array<TestHistogramBucket> histogram = {
        {0, 0},
        {1, 0},
        {2, 0},
        {4, 0},
        {8, 0},
        {16, 0},
        {32, 0},
        {64, 0},
        {128, 0},
        {256, 0},
    };
    u32 sum_of_all_populations = 0;
    u32 num_duplicate_items = 0;

    // Test setup.
    BTree<u32> btree;
    Array<u32> arr;
    Random r{0};

    // Main test loop.
    for (u32 iters = 0; iters < 2500; iters++) {
        // Ensure the B-tree and mirror array have the same number of items.
        PLY_ASSERT(btree.num_items == arr.num_items());

        // Decide what population size the B-tree should have next.
        // We'll generate a random number using a Poisson distribution.
        float exp = 1.f - r.generate_float();
        PLY_ASSERT(exp > 0); // Guaranteed because generate_float returns numbers < 1.
        float random_population = -logf(exp) * 40; // A Poisson distribution yielding an average value of 40.
        // Convert to integer and skew the distribution downwards so that the zero population occurs more often.
        u32 desired_population = (u32) clamp(random_population - 4.f, 0.f, 512.f);

        // Add items to the B-tree if needed.
        while (desired_population > arr.num_items()) {
            u32 value_to_insert = r.generate_u32() % 1000;
            arr.append(value_to_insert);
            btree.insert(value_to_insert);
#if defined(PLY_WITH_ASSERTS)
            btree.validate();
#endif
        }

        // Remove items from the B-tree if needed.
        while (desired_population < arr.num_items()) {
            u32 index_to_remove = r.generate_u32() % arr.num_items();
            u32 value_to_remove = arr[index_to_remove];
            bool was_found = btree.erase(value_to_remove);
#if defined(PLY_WITH_ASSERTS)
            btree.validate();
#endif
            check(was_found);
            arr.erase_quick(index_to_remove);
        }

        // Check its population.
        check(desired_population == arr.num_items());
        for (s32 i = histogram.num_items() - 1; i >= 0; i--) {
            if (desired_population >= histogram[i].population) {
                histogram[i].num_times_occurred++;
                break;
            }
        }
        sum_of_all_populations += desired_population;

        // Test iteration.
        sort(arr);
        auto iter = btree.get_first_item();
        for (u32 i = 0; i < arr.num_items(); i++) {
            check(iter);
            check(*iter == arr[i]);
            if ((i > 0) && (arr[i] == arr[i - 1])) {
                num_duplicate_items++;
            }
            iter++;
        }
        check(!iter);

        // Test reverse iteration.
        iter = btree.get_last_item();
        for (s32 i = arr.num_items() - 1; i >= 0; i--) {
            check(iter);
            check(*iter == arr[i]);
            iter--;
        }
        check(!iter);

        // Test find.
        for (u32 i = 0; i < arr.num_items(); i++) {
            check(btree.find(arr[i]));
        }
    }
}

//  ▄▄   ▄▄               ▄▄                ▄▄
//  ██   ██  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄██▄▄
//   ██ ██   ▄▄▄██ ██  ▀▀ ██  ▄▄▄██ ██  ██  ██
//    ▀█▀   ▀█▄▄██ ██     ██ ▀█▄▄██ ██  ██  ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Variant_

struct FruitBowl {
    struct Apple {
        String type;
    };
    struct Banana {
        bool peeled = false;
    };
    struct Cherry {
        u32 num_on_stem = 1;
    };
    struct Date {
    };

    Variant<Apple, Banana, Cherry, Date> fruit;
};

TEST_CASE("Variant template") {
    FruitBowl bowl;
    check(!bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(!bowl.fruit.is<FruitBowl::Date>());

    bowl.fruit = FruitBowl::Apple{"Cortland"};
    check(bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(!bowl.fruit.is<FruitBowl::Date>());
    check(bowl.fruit.as<FruitBowl::Apple>()->type == "Cortland");

    bowl.fruit = FruitBowl::Date{};
    check(!bowl.fruit.is<FruitBowl::Apple>());
    check(!bowl.fruit.is<FruitBowl::Banana>());
    check(!bowl.fruit.is<FruitBowl::Cherry>());
    check(bowl.fruit.is<FruitBowl::Date>());

    FruitBowl bowl2{FruitBowl::Banana{}};
    check(bowl2.fruit.is<FruitBowl::Banana>());
    check(!bowl2.fruit.as<FruitBowl::Banana>()->peeled);

    FruitBowl bowl3 = {FruitBowl::Cherry{}};
    check(bowl3.fruit.is<FruitBowl::Cherry>());
    check(bowl3.fruit.as<FruitBowl::Cherry>()->num_on_stem == 1);
}

//   ▄▄▄▄  ▄▄▄                       ▄▄  ▄▄   ▄▄
//  ██  ██  ██   ▄▄▄▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄ ▄██▄▄ ██▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄
//  ██▀▀██  ██  ██  ██ ██  ██ ██  ▀▀ ██  ██   ██  ██ ██ ██ ██ ▀█▄▄▄
//  ██  ██ ▄██▄ ▀█▄▄██ ▀█▄▄█▀ ██     ██  ▀█▄▄ ██  ██ ██ ██ ██  ▄▄▄█▀
//               ▄▄▄█▀

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Algorithm_

TEST_CASE("binary_search() basic functionality") {
    Array<u32> arr = {1, 3, 5, 7, 9, 11, 13, 15};
    
    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binary_search(arr, 5, FindGreaterThanOrEqual) == 2);
    check(binary_search(arr, 7, FindGreaterThanOrEqual) == 3);
    check(binary_search(arr, 1, FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, 15, FindGreaterThanOrEqual) == 7);
    
    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binary_search(arr, 4, FindGreaterThanOrEqual) == 2);
    check(binary_search(arr, 6, FindGreaterThanOrEqual) == 3);
    check(binary_search(arr, 0, FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, 20, FindGreaterThanOrEqual) == 8);
}

TEST_CASE("binary_search() with FindGreaterThan condition") {
    Array<u32> arr = {1, 3, 5, 7, 9, 11, 13, 15};
    
    // Test finding existing elements with Find_Greater_Than
    check(binary_search(arr, 5, FindGreaterThan) == 3);
    check(binary_search(arr, 7, FindGreaterThan) == 4);
    check(binary_search(arr, 1, FindGreaterThan) == 1);
    check(binary_search(arr, 15, FindGreaterThan) == 8);
    
    // Test finding non-existing elements with Find_Greater_Than
    check(binary_search(arr, 4, FindGreaterThan) == 2);
    check(binary_search(arr, 6, FindGreaterThan) == 3);
    check(binary_search(arr, 0, FindGreaterThan) == 0);
    check(binary_search(arr, 20, FindGreaterThan) == 8);
}

TEST_CASE("binary_search() empty array") {
    Array<u32> empty_arr;
    
    // Empty array should always return 0 for any search
    check(binary_search(empty_arr, 5, FindGreaterThanOrEqual) == 0);
    check(binary_search(empty_arr, 5, FindGreaterThan) == 0);
    check(binary_search(empty_arr, 0, FindGreaterThanOrEqual) == 0);
    check(binary_search(empty_arr, 100, FindGreaterThan) == 0);
}

TEST_CASE("binary_search() single element") {
    Array<u32> single_arr = {42};
    
    // Test with single element array
    check(binary_search(single_arr, 42, FindGreaterThanOrEqual) == 0);
    check(binary_search(single_arr, 42, FindGreaterThan) == 1);
    check(binary_search(single_arr, 40, FindGreaterThanOrEqual) == 0);
    check(binary_search(single_arr, 40, FindGreaterThan) == 0);
    check(binary_search(single_arr, 50, FindGreaterThanOrEqual) == 1);
    check(binary_search(single_arr, 50, FindGreaterThan) == 1);
}

TEST_CASE("binary_search() with duplicates") {
    Array<u32> arr = {1, 3, 3, 3, 5, 7, 7, 9};
    
    // Test finding duplicates with Find_Greater_Than_Or_Equal (should find first occurrence)
    check(binary_search(arr, 3, FindGreaterThanOrEqual) == 1);
    check(binary_search(arr, 7, FindGreaterThanOrEqual) == 5);
    
    // Test finding duplicates with Find_Greater_Than (should find first element after duplicates)
    check(binary_search(arr, 3, FindGreaterThan) == 4);
    check(binary_search(arr, 7, FindGreaterThan) == 7);
    
    // Test finding elements between duplicates
    check(binary_search(arr, 4, FindGreaterThanOrEqual) == 4);
    check(binary_search(arr, 4, FindGreaterThan) == 4);
    check(binary_search(arr, 6, FindGreaterThanOrEqual) == 5);
    check(binary_search(arr, 6, FindGreaterThan) == 5);
}

TEST_CASE("binary_search() all same elements") {
    Array<u32> arr = {5, 5, 5, 5, 5};
    
    // Test with all same elements
    check(binary_search(arr, 5, FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, 5, FindGreaterThan) == 5);
    check(binary_search(arr, 3, FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, 3, FindGreaterThan) == 0);
    check(binary_search(arr, 7, FindGreaterThanOrEqual) == 5);
    check(binary_search(arr, 7, FindGreaterThan) == 5);
}

TEST_CASE("binary_search() with custom type") {
    struct TestItem {
        u32 value;
        String name;
    
        u32 get_lookup_key() const {
            return value;
        }
    };

    Array<TestItem> arr = {
        {10, "ten"},
        {20, "twenty"},
        {30, "thirty"},
        {40, "forty"},
        {50, "fifty"},
    };
    
    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binary_search(arr, 30, FindGreaterThanOrEqual) == 2);
    check(binary_search(arr, 40, FindGreaterThanOrEqual) == 3);
    check(binary_search(arr, 10, FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, 50, FindGreaterThanOrEqual) == 4);
    
    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binary_search(arr, 25, FindGreaterThanOrEqual) == 2);
    check(binary_search(arr, 35, FindGreaterThanOrEqual) == 3);
    check(binary_search(arr, 5, FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, 60, FindGreaterThanOrEqual) == 5);
    
    // Test finding existing elements with Find_Greater_Than
    check(binary_search(arr, 30, FindGreaterThan) == 3);
    check(binary_search(arr, 40, FindGreaterThan) == 4);
    check(binary_search(arr, 10, FindGreaterThan) == 1);
    check(binary_search(arr, 50, FindGreaterThan) == 5);
    
    // Test finding non-existing elements with Find_Greater_Than
    check(binary_search(arr, 25, FindGreaterThan) == 2);
    check(binary_search(arr, 35, FindGreaterThan) == 3);
    check(binary_search(arr, 5, FindGreaterThan) == 0);
    check(binary_search(arr, 60, FindGreaterThan) == 5);
}

TEST_CASE("binary_search() with String type") {
    Array<String> arr = {"apple", "banana", "cherry", "date", "elderberry"};
    
    // Test finding existing elements with Find_Greater_Than_Or_Equal
    check(binary_search(arr, "cherry", FindGreaterThanOrEqual) == 2);
    check(binary_search(arr, "date", FindGreaterThanOrEqual) == 3);
    check(binary_search(arr, "apple", FindGreaterThanOrEqual) == 0);
    check(binary_search(arr, "elderberry", FindGreaterThanOrEqual) == 4);
    
    // Test finding non-existing elements with Find_Greater_Than_Or_Equal
    check(binary_search(arr, "blueberry", FindGreaterThanOrEqual) == 2);
    check(binary_search(arr, "coconut", FindGreaterThanOrEqual) == 3);
    check(binary_search(arr, "apricot", FindGreaterThanOrEqual) == 1);
    check(binary_search(arr, "fig", FindGreaterThanOrEqual) == 5);
    
    // Test finding existing elements with Find_Greater_Than
    check(binary_search(arr, "cherry", FindGreaterThan) == 3);
    check(binary_search(arr, "date", FindGreaterThan) == 4);
    check(binary_search(arr, "apple", FindGreaterThan) == 1);
    check(binary_search(arr, "elderberry", FindGreaterThan) == 5);
    
    // Test finding non-existing elements with Find_Greater_Than
    check(binary_search(arr, "blueberry", FindGreaterThan) == 2);
    check(binary_search(arr, "coconut", FindGreaterThan) == 3);
    check(binary_search(arr, "apricot", FindGreaterThan) == 1);
    check(binary_search(arr, "fig", FindGreaterThan) == 5);
}

TEST_CASE("binary_search() with different numeric types") {
    // Test with float array
    Array<float> float_arr = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    check(binary_search(float_arr, 3.3f, FindGreaterThanOrEqual) == 2);
    check(binary_search(float_arr, 3.0f, FindGreaterThanOrEqual) == 2);
    check(binary_search(float_arr, 3.3f, FindGreaterThan) == 3);
    check(binary_search(float_arr, 6.0f, FindGreaterThanOrEqual) == 5);
    
    // Test with double array
    Array<double> double_arr = {1.1, 2.2, 3.3, 4.4, 5.5};
    check(binary_search(double_arr, 3.3, FindGreaterThanOrEqual) == 2);
    check(binary_search(double_arr, 3.0, FindGreaterThanOrEqual) == 2);
    check(binary_search(double_arr, 3.3, FindGreaterThan) == 3);
    check(binary_search(double_arr, 6.0, FindGreaterThanOrEqual) == 5);
    
    // Test with s32 array
    Array<s32> s32_arr = {-5, -3, -1, 1, 3, 5};
    check(binary_search(s32_arr, -1, FindGreaterThanOrEqual) == 2);
    check(binary_search(s32_arr, 0, FindGreaterThanOrEqual) == 3);
    check(binary_search(s32_arr, -1, FindGreaterThan) == 3);
    check(binary_search(s32_arr, 10, FindGreaterThanOrEqual) == 6);
}

//  ▄▄  ▄▄        ▄▄                  ▄▄
//  ██  ██ ▄▄▄▄▄  ▄▄  ▄▄▄▄  ▄▄▄▄   ▄▄▄██  ▄▄▄▄
//  ██  ██ ██  ██ ██ ██    ██  ██ ██  ██ ██▄▄██
//  ▀█▄▄█▀ ██  ██ ██ ▀█▄▄▄ ▀█▄▄█▀ ▀█▄▄██ ▀█▄▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Unicode_

TEST_CASE("Decode truncated UTF-8") {
  // e3 80 82 is the valid UTF-8 encoding of U+3002
  // e3 80 is the truncated version of it
  // As such, it should be decoded as two 8-bit characters
  OutPipeConvertUnicode conv{MemStream{}, UTF16_LE};
  conv.write("\xe3\x80");
  conv.flush(false);
  String result = static_cast<MemStream&>(conv.child_out).move_to_string();
  check(result == StringView{"\xe3\x00\x80\x00", 4});
}

//  ▄▄▄▄▄▄                ▄▄   ▄▄▄▄▄                                ▄▄
//    ██    ▄▄▄▄  ▄▄  ▄▄ ▄██▄▄ ██     ▄▄▄▄  ▄▄▄▄▄  ▄▄▄▄▄▄▄   ▄▄▄▄  ▄██▄▄
//    ██   ██▄▄██  ▀██▀   ██   ██▀▀  ██  ██ ██  ▀▀ ██ ██ ██  ▄▄▄██  ██
//    ██   ▀█▄▄▄  ▄█▀▀█▄  ▀█▄▄ ██    ▀█▄▄█▀ ██     ██ ██ ██ ▀█▄▄██  ▀█▄▄
//

#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Text_Format_

struct ExtractedFormat {
    bool is_valid = false;
    TextFormat format;
};

ExtractedFormat extract_format_from_name(StringView name) {
    TextFormat tf;

    Array<StringView> components = name.split_byte('.');
    if (components.num_items() != 4)
        return {false, {}};

    if (components[1] == "utf8") {
        tf.unicode_type = UTF8;
    } else if (components[1] == "utf16le") {
        tf.unicode_type = UTF16_LE;
    } else if (components[1] == "utf16be") {
        tf.unicode_type = UTF16_BE;
    } else if (components[1] == "win1252") {
        tf.unicode_type = NOT_UNICODE;
    } else {
        return {false, {}};
    }

    if (components[2] == "lf") {
        tf.new_line = TextFormat::LF;
    } else if (components[2] == "crlf") {
        tf.new_line = TextFormat::CRLF;
    } else {
        return {false, {}};
    }

    if (components[3] == "bom") {
        tf.bom = true;
    } else if (components[3] == "nobom") {
        tf.bom = false;
    } else {
        return {false, {}};
    }

    return {true, tf};
}

TEST_CASE("Autodetect file encodings") {
    String tests_folder = join_path(BASE_LIBRARY_TESTS_PATH, "text-files");
    u32 entry_count = 0;
    for (const DirectoryEntry& entry : Filesystem::list_dir(tests_folder)) {
        if (!entry.is_dir && entry.name.ends_with(".txt")) {
            ExtractedFormat expected_format = extract_format_from_name(entry.name.shortened_by(4));
            check(expected_format.is_valid);

            TextFormat detected_format;
            String contents = Filesystem::load_text_autodetect(join_path(tests_folder, entry.name), &detected_format);
            check(detected_format.unicode_type == expected_format.format.unicode_type);
            check(detected_format.new_line == expected_format.format.new_line);
            check(detected_format.bom == expected_format.format.bom);

            auto compare_to = Filesystem::load_binary(join_path(tests_folder, entry.name.split_byte('.')[0] + ".utf8.lf.nobom.txt"));
            check(contents == compare_to);
            entry_count++;
        }
    }
    check(entry_count == 50);
}

//   ▄▄▄▄   ▄▄
//  ██  ▀▀ ▄██▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄  ▄▄▄▄▄▄▄
//   ▀▀▀█▄  ██   ██  ▀▀ ██▄▄██  ▄▄▄██ ██ ██ ██
//  ▀█▄▄█▀  ▀█▄▄ ██     ▀█▄▄▄  ▀█▄▄██ ██ ██ ██
//

TEST_CASE("Mem stream temp buffer") {
    Random random{0};
    for (u32 i = 0; i < 100; i++) {
        MemStream mem;
        u32 file_size = Stream::BUFFER_SIZE * 10;
        u32 offset = 0;
        while (offset < file_size) {
            check(offset == mem.get_seek_pos());
            u32 num_consecutive_bytes = (random.generate_u32() % (Stream::MAX_CONSECUTIVE_BYTES / 2)) + (Stream::MAX_CONSECUTIVE_BYTES / 2);
            check(mem.make_writable(min(num_consecutive_bytes, file_size - offset)));
            while (mem.cur_byte < mem.end_byte) {
                *mem.cur_byte++ = (u8) shuffle_bits(offset++);
                if (--num_consecutive_bytes == 0)
                    break;
            }
        }
        mem.seek_to(0);
        offset = 0;
        while (offset < file_size) {
            check(offset == mem.get_seek_pos());
            u32 num_consecutive_bytes = (random.generate_u32() % (Stream::MAX_CONSECUTIVE_BYTES / 2)) + (Stream::MAX_CONSECUTIVE_BYTES / 2);
            mem.make_readable(num_consecutive_bytes);
            check(mem.at_eof == (mem.num_remaining_bytes() == 0));
            if (mem.at_eof)
                break;
            while (mem.cur_byte < mem.end_byte) {
                check((u8) *mem.cur_byte++ == (u8) shuffle_bits(offset++));
                if (--num_consecutive_bytes == 0)
                    break;
            }
        }
        check(offset == file_size);
    }
}

//  ▄▄▄▄▄  ▄▄                      ▄▄                        ▄▄    ▄▄         ▄▄         ▄▄
//  ██  ██ ▄▄ ▄▄▄▄▄   ▄▄▄▄   ▄▄▄▄ ▄██▄▄  ▄▄▄▄  ▄▄▄▄▄  ▄▄  ▄▄ ██ ▄▄ ██  ▄▄▄▄  ▄██▄▄  ▄▄▄▄ ██▄▄▄   ▄▄▄▄  ▄▄▄▄▄
//  ██  ██ ██ ██  ▀▀ ██▄▄██ ██     ██   ██  ██ ██  ▀▀ ██  ██ ▀█▄██▄█▀  ▄▄▄██  ██   ██    ██  ██ ██▄▄██ ██  ▀▀
//  ██▄▄█▀ ██ ██     ▀█▄▄▄  ▀█▄▄▄  ▀█▄▄ ▀█▄▄█▀ ██     ▀█▄▄██  ██▀▀██  ▀█▄▄██  ▀█▄▄ ▀█▄▄▄ ██  ██ ▀█▄▄▄  ██
//                                                     ▄▄▄█▀

#if 0 // Disabled test
#undef TEST_CASE_PREFIX
#define TEST_CASE_PREFIX Directory_Watcher_

template <typename Item>
class MessageQueue {
    Mutex mutex;
    ConditionVariable cv;
    Array<Item> items;

public:
    void push(Item&& item) {
        LockGuard<Mutex> lock{mutex};
        items.append(std::move(item));
        cv.wake_one();
    }
    bool pop(Item& item, u32 timeout_ms = 2000) {
        u64 time_limit = get_cpu_ticks() + (u64) (timeout_ms * get_cpu_ticks_per_second() / 1000.f);
        LockGuard<Mutex> lock{mutex};
        while (items.is_empty()) {
            u64 now = get_cpu_ticks();
            s32 remaining_ms = (s32) ((time_limit - now) * (1000.f / get_cpu_ticks_per_second()));
            if (remaining_ms <= 0)
                return false;
            cv.timed_wait(lock, remaining_ms);
        }
        item = std::move(items[0]);
        items.erase(0);
        return true;
    }
};

TEST_CASE("DirectoryWatcher") {
    struct Event {
        String path;
        bool must_recurse;
    };
    MessageQueue<Event> message_queue;

    auto wait_for_event = [&](const Event& expected_event) -> bool {
        for (;;) {
            Event event;
            if (!message_queue.pop(event))
                return false;
            if (event.path == expected_event.path && event.must_recurse == expected_event.must_recurse)
                return true;
        }
    };

    // Set up temp directory.
    String temp_dir = join_path(BUILD_DIR, "temp-dir-watcher");
    Filesystem::remove_dir_tree(temp_dir); // Clean up from any previous run
    Filesystem::make_dir(temp_dir);

    // Start the watcher.
    DirectoryWatcher watcher;
    watcher.start(temp_dir, [&](StringView path, bool must_recurse) {
        message_queue.push({path, must_recurse});
    });

    // Create a file in the temp directory.
    Filesystem::save_text(join_path(temp_dir, "first_file.txt"), "Hello, world!\n");
    check(wait_for_event({"first_file.txt", false}));

    // Create a subdirectory.
    Filesystem::make_dir(join_path(temp_dir, "subdir"));
    check(wait_for_event({"subdir", true}));

    // Modify the first file.
    Filesystem::save_text(join_path(temp_dir, "first_file.txt"), "Modified content!\n");
    check(wait_for_event({"first_file.txt", false}));

    // Create a file in the subdirectory.
    Filesystem::save_text(join_path(temp_dir, "subdir", "second_file.txt"), "Another file\n");
    check(wait_for_event({join_path("subdir", "second_file.txt"), false}));

    // Delete the first file.
    Filesystem::delete_file(join_path(temp_dir, "first_file.txt"));
    check(wait_for_event({"first_file.txt", false}));

    watcher.stop();
    Filesystem::remove_dir_tree(temp_dir);
}
#endif
