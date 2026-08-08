/*========================================================
       ____
      ╱   ╱╲    Plywood C++ Runtime Library
     ╱___╱╭╮╲   https://plywood.dev/
      └──┴┴┴┘
========================================================*/

#pragma once
#include <ply-system.h>
#define PLY_ENABLE_MATH_REFLECT 1
#if defined(PLY_ENABLE_MATH_REFLECT)
#include <ply-math.h>
#endif
#include <ply-json.h>

namespace ply {

#if defined(PLY_ENABLE_MATH_REFLECT)
// Hash functions for math types
inline void addToHash(HashBuilder& hb, const Float2& value) {
    addToHash(hb, value.x);
    addToHash(hb, value.y);
}
inline void addToHash(HashBuilder& hb, const Float3& value) {
    addToHash(hb, value.x);
    addToHash(hb, value.y);
    addToHash(hb, value.z);
}
inline void addToHash(HashBuilder& hb, const Float4& value) {
    addToHash(hb, value.x);
    addToHash(hb, value.y);
    addToHash(hb, value.z);
    addToHash(hb, value.w);
}
void addToHash(HashBuilder& hb, const Mat4x4& value);
#endif

enum class TypeKey {
    Bool = 0,
    S8,
    S16,
    S32,
    S64,
    U8,
    U16,
    U32,
    U64,
    Float,
    Double,
#if defined(PLY_ENABLE_MATH_REFLECT)
    Float2,
    Float3,
    Float4,
    Mat4x4,
    Rect,
#endif
    Struct,
    String,
    Array,
    FixedArray,
    Pointer,
    Owned,
    Map,
    Variant,
    AnyOwnedObject,
    AnyArray,
    Custom,
    Count,
};

struct AnyObject;

// If key is Struct, Array, FixedArray, Pointer, Owned, Map or Variant, a subtype of TypeInfo
// (like StructTypeInfo) contains additional information about the type.
struct TypeInfo {
    TypeKey key = TypeKey::Count;
    u32 fixedSize = 0;
    u32 alignment = 0;

    StringView getName() const;
};

//-------------------------
// AnyObject
//-------------------------
struct AnyObject {
    void* data = nullptr;
    TypeInfo* type = nullptr;

    AnyObject() = default;
    AnyObject(void* data, TypeInfo* type) : data{data}, type{type} {
    }
    template <typename T>
    AnyObject(T* obj) : data{obj}, type{getTypeInfo(obj)} {
    }
    template <typename T>
    T* cast() const {
        PLY_ASSERT(this->type == getTypeInfo((T*) nullptr));
        return (T*) this->data;
    }
    void construct();
    void destruct();
    void copyConstructFrom(const AnyObject& src);
};

//-------------------------
// AnyOwnedObject
//-------------------------
struct AnyOwnedObject : AnyObject {
    AnyOwnedObject() = default;
    template <typename T>
    AnyOwnedObject(T* obj) : AnyObject{obj} {
    }
    ~AnyOwnedObject() {
        this->destruct();
    }
    static AnyOwnedObject create(TypeInfo* type);
};

TypeInfo* getTypeInfo(AnyOwnedObject*);

//-------------------------
// AnyArrayView
//-------------------------
struct AnyArrayView {
    void* items = nullptr;
    TypeInfo* itemType = nullptr;
    u32 numItems = 0;

    AnyArrayView() = default;
    template <typename T>
    AnyArrayView(ArrayView<T> view)
        : items{view.items()}, numItems{view.numItems()}, itemType{getTypeInfo((T*) nullptr)} {
    }
    u32 numBytes() const {
        return this->itemType->fixedSize * this->numItems;
    }
    AnyObject operator[](u32 index) const {
        PLY_ASSERT(index < this->numItems);
        return AnyObject{PLY_PTR_OFFSET(this->items, uptr(index) * this->itemType->fixedSize), this->itemType};
    }
};

//-------------------------
// AnyArray
//-------------------------
struct AnyArray : AnyArrayView {
    u32 allocated = 0;

    AnyArray() = default;
    AnyArray(TypeInfo* itemType) {
        this->itemType = itemType;
    }
    AnyArray(const AnyArray& other);
    AnyArray(AnyArray&& other);
    AnyArray& operator=(const AnyArray& other);
    AnyArray& operator=(AnyArray&& other);
    ~AnyArray();

    void reserve(u32 numItems);
    void resize(u32 numItems);
    void clear();
    void alloc(u32 numItems);
};

TypeInfo* getTypeInfo(AnyArray*);

//-------------------------
// StructTypeInfo
//-------------------------
struct StructTypeInfo : TypeInfo {
    struct Member {
        String name;
        u32 offset = 0;
        TypeInfo* type = nullptr;

        // Access a member at runtime using a StructTypeInfo::Member instance.
        AnyObject apply(AnyObject parent) const {
            return AnyObject{PLY_PTR_OFFSET(parent.data, this->offset), this->type};
        }
    };

    String name;
    Array<Member> members;

    // Thunks
    void (*construct)(StructTypeInfo* typeInfo, void* obj) = nullptr;
    void (*destruct)(StructTypeInfo* typeInfo, void* obj) = nullptr;
    void (*copyConstruct)(StructTypeInfo* typeInfo, void* dst, const void* src) = nullptr;

    // Constructor
    template <typename T>
    StructTypeInfo(T*, StringView name) : TypeInfo{TypeKey::Struct, sizeof(T), alignof(T)}, name{name} {
        this->construct = [](StructTypeInfo*, void* obj) { new (obj) T; };
        this->destruct = [](StructTypeInfo*, void* obj) { ((T*) obj)->~T(); };
        this->copyConstruct = [](StructTypeInfo*, void* dst, const void* src) { new (dst) T(*(const T*) src); };
    }

    // Look up a Member by name.
    const Member* lookup(StringView name) const {
        for (const Member& member : this->members) {
            if (member.name == name)
                return &member;
        }
        return nullptr;
    }
    // For use in Set<>.
    StringView getLookupKey() const {
        return name;
    }
};

// Initialization helper.
struct Initializer {
    Initializer(void (*fn)()) {
        fn();
    }
};

// Macro to declare a runtime StructTypeInfo from within a struct type definition.
#define PLY_DECLARE_TYPE_INFO(type) friend ::ply::StructTypeInfo* getTypeInfo(type*);

// clang-format off
#define PLY_STRUCT_BEGIN(type) \
	::ply::StructTypeInfo* getTypeInfo(type*) { \
	    static StructTypeInfo typeInfo{(type*) nullptr, #type}; \
	    return &typeInfo; \
	} \
    ::ply::Initializer PLY_UNIQUE_VARIABLE(Init_){[]() { \
		using T = type; \
		using ::ply::getTypeInfo; \
		::ply::StructTypeInfo* info = getTypeInfo((T*) nullptr); \
		info->members = {

#define PLY_STRUCT_MEMBER(member) \
	{ #member, offsetof(T, member), getTypeInfo((decltype(::ply::declval<T>().member)*) nullptr) },
    
#define PLY_STRUCT_END() \
		}; \
    }};
#define PLY_LOOKUP_MEMBER(structType, member) \
    (getTypeInfo((structType*) nullptr)->lookup(#member))
// clang-format on

//-------------------------
// PointerTypeInfo
//-------------------------
struct PointerTypeInfo : TypeInfo {
    TypeInfo* targetType;

    PointerTypeInfo(TypeInfo* targetType)
        : TypeInfo{TypeKey::Pointer, sizeof(void*), alignof(void*)}, targetType{targetType} {
    }
};

template <typename T>
PointerTypeInfo* getTypeInfo(T**) {
    static PointerTypeInfo typeInfo{getTypeInfo((T*) nullptr)};
    return &typeInfo;
};

//-------------------------
// OwnedTypeInfo
//-------------------------
struct OwnedTypeInfo : TypeInfo {
    TypeInfo* targetType;

    OwnedTypeInfo(TypeInfo* targetType)
        : TypeInfo{TypeKey::Owned, sizeof(void*), alignof(void*)}, targetType{targetType} {
    }
};

template <typename T>
OwnedTypeInfo* getTypeInfo(Owned<T>*) {
    static OwnedTypeInfo typeInfo{getTypeInfo((T*) nullptr)};
    return &typeInfo;
};

//-------------------------
// FixedArrayTypeInfo
//-------------------------
struct FixedArrayTypeInfo : TypeInfo {
    TypeInfo* itemType;
    u32 numItems;

    FixedArrayTypeInfo(TypeInfo* itemType, u32 numItems)
        : TypeInfo{TypeKey::FixedArray, itemType->fixedSize * numItems, itemType->alignment}, itemType{itemType},
          numItems{numItems} {
    }
};

// C-style arrays
template <typename T, int numItems>
FixedArrayTypeInfo* getTypeInfo(T (*)[numItems]) {
    static FixedArrayTypeInfo typeInfo{getTypeInfo((T*) nullptr), u32(numItems)};
    return &typeInfo;
};

//-------------------------
// ArrayTypeInfo
//-------------------------
struct BaseArray {
    void* items = nullptr;
    u32 numItems = 0;
    u32 allocated = 0;

    void clear(TypeInfo* itemType);
    void reserve(TypeInfo* itemType, u32 numItems);
    void resize(TypeInfo* itemType, u32 numItems);
};

struct ArrayTypeInfo : TypeInfo {
    TypeInfo* itemType;

    ArrayTypeInfo(TypeInfo* itemType)
        : TypeInfo{TypeKey::Array, sizeof(BaseArray), alignof(BaseArray)}, itemType{itemType} {
    }
};

template <typename T>
ArrayTypeInfo* getTypeInfo(Array<T>*) {
    static ArrayTypeInfo typeInfo{getTypeInfo((T*) nullptr)};
    return &typeInfo;
};

//-------------------------
// MapTypeInfo
//-------------------------
struct BaseMap {
    s32* indices = nullptr;
    u32 numIndices = 0;
    u32 numAllocatedIndices = 0;
    void* items = nullptr;
    u32 numItems = 0;
    u32 allocated = 0;
};

struct MapTypeInfo : TypeInfo {
    TypeInfo* keyType;
    TypeInfo* valueType;

    MapTypeInfo(TypeInfo* keyType, TypeInfo* valueType)
        : TypeInfo{TypeKey::Map, sizeof(BaseMap), alignof(BaseMap)}, keyType{keyType}, valueType{valueType} {
    }
};

template <typename Key, typename Value>
MapTypeInfo* getTypeInfo(Map<Key, Value>*) {
    static MapTypeInfo typeInfo{getTypeInfo((Key*) nullptr), getTypeInfo((Value*) nullptr)};
    return &typeInfo;
}

//-------------------------
// VariantTypeInfo
//-------------------------
struct VariantTypeInfo : TypeInfo {
    Array<TypeInfo*> subtypes;

    template <typename T>
    VariantTypeInfo(T*, std::initializer_list<TypeInfo*> subtypes)
        : TypeInfo{TypeKey::Variant, sizeof(T), alignof(T)}, subtypes{subtypes} {
    }
};

template <typename... Subtypes>
VariantTypeInfo* getTypeInfo(Variant<Subtypes...>*) {
    static VariantTypeInfo typeInfo{(Variant<Subtypes...>*) nullptr, {getTypeInfo((Subtypes*) nullptr)...}};
    return &typeInfo;
};

//-------------------------
// CustomTypeInfo
//-------------------------
struct ReadFromJsonArgs {
    Array<Owned<StructTypeInfo>> synthStructTypes;
};

struct CustomTypeInfo : TypeInfo {
    String name;

    void (*construct)(void* obj) = nullptr;
    void (*destruct)(void* obj) = nullptr;
    void (*copyConstruct)(void* dst, const void* src) = nullptr;
    void (*readFromJson)(ReadFromJsonArgs& args, AnyObject obj, const json::Node& root) = nullptr;
    void (*writeToJson)(const json::WriteOptions& options, AnyObject obj, json::Node& result) = nullptr;

    template <typename T>
    CustomTypeInfo(T*) : TypeInfo{TypeKey::Custom, sizeof(T), alignof(T)} {
        this->construct = [](void* obj) { new (obj) T; };
        this->destruct = [](void* obj) { ((T*) obj)->~T(); };
        this->copyConstruct = [](void* dst, const void* src) { copyConstructWrapper((T*) dst, (const T*) src); };
    };
};

//-----------------------------------
// Primitive types
//-----------------------------------
TypeInfo* getTypeInfo(bool*);
TypeInfo* getTypeInfo(s8*);
TypeInfo* getTypeInfo(s16*);
TypeInfo* getTypeInfo(s32*);
TypeInfo* getTypeInfo(s64*);
TypeInfo* getTypeInfo(u8*);
TypeInfo* getTypeInfo(u16*);
TypeInfo* getTypeInfo(u32*);
TypeInfo* getTypeInfo(u64*);
TypeInfo* getTypeInfo(float*);
TypeInfo* getTypeInfo(double*);
TypeInfo* getTypeInfo(String*);
#if defined(PLY_ENABLE_MATH_REFLECT)
TypeInfo* getTypeInfo(Float2*);
TypeInfo* getTypeInfo(Float3*);
TypeInfo* getTypeInfo(Float4*);
TypeInfo* getTypeInfo(Mat4x4*);
TypeInfo* getTypeInfo(Rect*);
#endif

//-----------------------------------
// JSON conversion
//-----------------------------------
namespace json {

StructTypeInfo* getTypeInfo(Node::Number*);
StructTypeInfo* getTypeInfo(Node::Bool*);
StructTypeInfo* getTypeInfo(Node::Text*);
StructTypeInfo* getTypeInfo(Node::Array*);
StructTypeInfo* getTypeInfo(Node::Object*);
StructTypeInfo* getTypeInfo(Node*);

} // namespace json

// anyObj must point to an existing valid object of the prescribed type.
void readFromJson(ReadFromJsonArgs& args, AnyObject obj, const json::Node& root);

// Serialize a reflected C++ object to a json::Node tree.
// The result can be written to a stream using json::write or json::toString.
json::Node convertToJson(const json::WriteOptions& writeOptions, AnyObject obj);

//-----------------------------------
// CommandLineParser
//-----------------------------------
struct CmdLineArgHandler {
    String arg;
    const StructTypeInfo::Member* dataMember;
    String description;
};

struct CommandLineParser {
    // These members must be initialized before calling apply.
    // - When dataMember is a bool, the argument sets it to true.
    // - When dataMember is a String, the command line argument must be followed by a string.
    //   Multiple formats are accepted: -foo bar, -foo"bar", -foo=bar, -foo="bar"
    Array<CmdLineArgHandler> handlers;
    Functor<void(ArrayView<const char*> args, u32 index)> defaultHandler;

    // Gets filled in by apply.
    String executablePath;

    // Constructor.
    CommandLineParser(Array<CmdLineArgHandler>&& handlers) : handlers{std::move(handlers)} {
    }

    // Prints the available command line options to stderr.
    void printAvailableOptions(bool withHeader = false) const;

    // Returns true if argument parsing succeeds.
    // Otherwise, prints an error message to stderr, then returns false.
    bool apply(int argc, const char* argv[], AnyObject obj);
};

} // namespace ply
