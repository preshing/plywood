/*─────────────────────────────────────────────────────────┐
│                                                          │
│     ____      Plywood C++ Runtime Library                │
│    ╱   ╱╲     https://plywood.dev/                       │
│   ╱___╱╭╮╲                                               │
│    └──┴┴┴┘    Reflection                                 │
│               Documentation: /docs/reflect.md            │
│                                                          │
└─────────────────────────────────────────────────────────*/

#include "ply-reflect.h"

namespace ply {

#if defined(PLY_ENABLE_MATH_REFLECT)
void addToHash(HashBuilder& hb, const Mat4x4& value) {
    addToHash(hb, value.col[0]);
    addToHash(hb, value.col[1]);
    addToHash(hb, value.col[2]);
    addToHash(hb, value.col[3]);
}

void addToHash(HashBuilder& hb, const Rect& value) {
    addToHash(hb, value.mins);
    addToHash(hb, value.maxs);
}
#endif

TypeInfo* getTypeInfo(bool*) {
    static TypeInfo typeInfo{TypeKey::Bool, sizeof(bool), alignof(bool)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(s8*) {
    static TypeInfo typeInfo{TypeKey::S8, sizeof(s8), alignof(s8)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(s16*) {
    static TypeInfo typeInfo{TypeKey::S16, sizeof(s16), alignof(s16)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(s32*) {
    static TypeInfo typeInfo{TypeKey::S32, sizeof(s32), alignof(s32)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(s64*) {
    static TypeInfo typeInfo{TypeKey::S64, sizeof(s64), alignof(s64)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(u8*) {
    static TypeInfo typeInfo{TypeKey::U8, sizeof(u8), alignof(u8)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(u16*) {
    static TypeInfo typeInfo{TypeKey::U16, sizeof(u16), alignof(u16)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(u32*) {
    static TypeInfo typeInfo{TypeKey::U32, sizeof(u32), alignof(u32)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(u64*) {
    static TypeInfo typeInfo{TypeKey::U64, sizeof(u64), alignof(u64)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(float*) {
    static TypeInfo typeInfo{TypeKey::Float, sizeof(float), alignof(float)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(double*) {
    static TypeInfo typeInfo{TypeKey::Double, sizeof(double), alignof(double)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(String*) {
    static TypeInfo typeInfo{TypeKey::String, sizeof(String), alignof(String)};
    return &typeInfo;
}
#if defined(PLY_ENABLE_MATH_REFLECT)
TypeInfo* getTypeInfo(Float2*) {
    static TypeInfo typeInfo{TypeKey::Float2, sizeof(Float2), alignof(Float2)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(Float3*) {
    static TypeInfo typeInfo{TypeKey::Float3, sizeof(Float3), alignof(Float3)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(Float4*) {
    static TypeInfo typeInfo{TypeKey::Float4, sizeof(Float4), alignof(Float4)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(Mat4x4*) {
    static TypeInfo typeInfo{TypeKey::Mat4x4, sizeof(Mat4x4), alignof(Mat4x4)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(Rect*) {
    static TypeInfo typeInfo{TypeKey::Rect, sizeof(Rect), alignof(Rect)};
    return &typeInfo;
}
#endif
TypeInfo* getTypeInfo(AnyOwnedObject*) {
    static TypeInfo typeInfo{TypeKey::AnyOwnedObject, sizeof(AnyOwnedObject), alignof(AnyOwnedObject)};
    return &typeInfo;
}
TypeInfo* getTypeInfo(AnyArray*) {
    static TypeInfo typeInfo{TypeKey::AnyArray, sizeof(AnyArray), alignof(AnyArray)};
    return &typeInfo;
}

//-----------------------------------
// json::Node
//-----------------------------------

namespace json {

PLY_STRUCT_BEGIN(Node::Number)
PLY_STRUCT_MEMBER(value)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Node::Bool)
PLY_STRUCT_MEMBER(value)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Node::Text)
PLY_STRUCT_MEMBER(text)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Node::Array)
PLY_STRUCT_MEMBER(items)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Node::Object)
PLY_STRUCT_MEMBER(items)
PLY_STRUCT_END()

PLY_STRUCT_BEGIN(Node)
PLY_STRUCT_MEMBER(fileOfs)
PLY_STRUCT_MEMBER(var)
PLY_STRUCT_END()

} // namespace json

//-----------------------------------
// AnyObject construct/destruct
//-----------------------------------

void AnyObject::construct() {
    switch (this->type->key) {
        case TypeKey::AnyOwnedObject:
        case TypeKey::AnyArray:
        case TypeKey::Bool:
        case TypeKey::S8:
        case TypeKey::S16:
        case TypeKey::S32:
        case TypeKey::S64:
        case TypeKey::U8:
        case TypeKey::U16:
        case TypeKey::U32:
        case TypeKey::U64:
        case TypeKey::Float:
        case TypeKey::Double:
#if defined(PLY_ENABLE_MATH_REFLECT)
        case TypeKey::Float2:
        case TypeKey::Float3:
        case TypeKey::Float4:
        case TypeKey::Mat4x4:
        case TypeKey::Rect:
#endif
        case TypeKey::Pointer:
        case TypeKey::Owned:
        case TypeKey::String:
        case TypeKey::Array:
        case TypeKey::Map:
        case TypeKey::Variant: {
            memset(this->data, 0, this->type->fixedSize);
            break;
        }
        case TypeKey::Struct: {
            auto* structType = (StructTypeInfo*) this->type;
            structType->construct(structType, this->data);
            break;
        }
        case TypeKey::FixedArray: {
            auto* fixedArrType = (FixedArrayTypeInfo*) this->type;
            for (u32 i = 0; i < fixedArrType->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(this->data, uptr(i) * fixedArrType->itemType->fixedSize),
                               fixedArrType->itemType};
                item.construct();
            }
            break;
        }
        case TypeKey::Custom: {
            auto* customType = (CustomTypeInfo*) this->type;
            PLY_ASSERT(customType->construct);
            customType->construct(this->data);
            break;
        }
        default: {
            PLY_ASSERT(0);
            break;
        }
    }
}

void AnyObject::destruct() {
    switch (this->type->key) {
        case TypeKey::Bool:
        case TypeKey::S8:
        case TypeKey::S16:
        case TypeKey::S32:
        case TypeKey::S64:
        case TypeKey::U8:
        case TypeKey::U16:
        case TypeKey::U32:
        case TypeKey::U64:
        case TypeKey::Float:
        case TypeKey::Double:
#if defined(PLY_ENABLE_MATH_REFLECT)
        case TypeKey::Float2:
        case TypeKey::Float3:
        case TypeKey::Float4:
        case TypeKey::Mat4x4:
        case TypeKey::Rect:
#endif
        case TypeKey::Pointer: {
            // Trivially destructible; nothing to do.
            break;
        }
        case TypeKey::String: {
            ((String*) this->data)->~String();
            break;
        }
        case TypeKey::Struct: {
            auto* structType = (StructTypeInfo*) this->type;
            structType->destruct(structType, this->data);
            break;
        }
        case TypeKey::Array: {
            auto* arrType = (ArrayTypeInfo*) this->type;
            BaseArray* baseArr = (BaseArray*) this->data;
            for (u32 i = 0; i < baseArr->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(baseArr->items, uptr(i) * arrType->itemType->fixedSize),
                               arrType->itemType};
                item.destruct();
            }
            Heap::free(baseArr->items);
            new (baseArr) BaseArray;
            break;
        }
        case TypeKey::FixedArray: {
            auto* fixedArrType = (FixedArrayTypeInfo*) this->type;
            for (u32 i = 0; i < fixedArrType->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(this->data, uptr(i) * fixedArrType->itemType->fixedSize),
                               fixedArrType->itemType};
                item.destruct();
            }
            break;
        }
        case TypeKey::Owned: {
            auto* ownedType = (OwnedTypeInfo*) this->type;
            void* ptr = *(void**) this->data;
            if (ptr) {
                AnyObject target{ptr, ownedType->targetType};
                target.destruct();
                Heap::free(ptr);
            }
            break;
        }
        case TypeKey::AnyOwnedObject: {
            auto* ownedObj = (AnyOwnedObject*) this->data;
            if (ownedObj->data) {
                ownedObj->destruct();
                Heap::free(ownedObj->data);
            }
            break;
        }
        case TypeKey::Map: {
            auto* mapType = (MapTypeInfo*) this->type;
            BaseMap* baseMap = (BaseMap*) this->data;
            // Compute item layout.
            u32 keySize = mapType->keyType->fixedSize;
            u32 valueOffset = alignToPowerOf2(keySize, mapType->valueType->alignment);
            u32 itemAlign = max(mapType->keyType->alignment, mapType->valueType->alignment);
            u32 itemSize = alignToPowerOf2(valueOffset + mapType->valueType->fixedSize, itemAlign);
            // Destruct each key and value.
            for (u32 i = 0; i < baseMap->numItems; i++) {
                void* itemPtr = PLY_PTR_OFFSET(baseMap->items, uptr(i) * itemSize);
                AnyObject keyObj{itemPtr, mapType->keyType};
                keyObj.destruct();
                AnyObject valueObj{PLY_PTR_OFFSET(itemPtr, valueOffset), mapType->valueType};
                valueObj.destruct();
            }
            Heap::free(baseMap->items);
            Heap::free(baseMap->indices);
            break;
        }
        case TypeKey::Variant: {
            auto* variantType = (VariantTypeInfo*) this->type;
            u32 subtype = *(u32*) this->data;
            if (subtype > 0) {
                // Compute offset of the variant storage, which follows the u32 subtype field,
                // padded to the alignment of the most-aligned subtype.
                u32 align = 4;
                for (TypeInfo* sub : variantType->subtypes) {
                    align = max(align, sub->alignment);
                }
                PLY_ASSERT(subtype <= variantType->subtypes.numItems());
                TypeInfo* activeType = variantType->subtypes[subtype - 1];
                AnyObject value{PLY_PTR_OFFSET(this->data, align), activeType};
                value.destruct();
            }
            break;
        }
        case TypeKey::Custom: {
            auto* customType = (CustomTypeInfo*) this->type;
            PLY_ASSERT(customType->destruct);
            customType->destruct(this->data);
            break;
        }
        default: {
            PLY_ASSERT(0);
            break;
        }
    }
}

void AnyObject::copyConstructFrom(const AnyObject& src) {
    PLY_ASSERT(this->type == src.type);
    switch (this->type->key) {
        case TypeKey::Bool:
        case TypeKey::S8:
        case TypeKey::S16:
        case TypeKey::S32:
        case TypeKey::S64:
        case TypeKey::U8:
        case TypeKey::U16:
        case TypeKey::U32:
        case TypeKey::U64:
        case TypeKey::Float:
        case TypeKey::Double:
#if defined(PLY_ENABLE_MATH_REFLECT)
        case TypeKey::Float2:
        case TypeKey::Float3:
        case TypeKey::Float4:
        case TypeKey::Mat4x4:
        case TypeKey::Rect:
#endif
        case TypeKey::Pointer: {
            memcpy(this->data, src.data, this->type->fixedSize);
            break;
        }
        case TypeKey::String: {
            new (this->data) String(*(const String*) src.data);
            break;
        }
        case TypeKey::Struct: {
            auto* structType = (StructTypeInfo*) this->type;
            structType->copyConstruct(structType, this->data, src.data);
            break;
        }
        case TypeKey::Array: {
            auto* arrType = (ArrayTypeInfo*) this->type;
            BaseArray* dstArr = (BaseArray*) this->data;
            const BaseArray* srcArr = (const BaseArray*) src.data;
            dstArr->numItems = srcArr->numItems;
            if (srcArr->numItems > 0) {
                dstArr->items = Heap::alloc(arrType->itemType->fixedSize * srcArr->numItems);
                dstArr->allocated = srcArr->numItems;
                for (u32 i = 0; i < srcArr->numItems; i++) {
                    AnyObject dstItem{PLY_PTR_OFFSET(dstArr->items, uptr(i) * arrType->itemType->fixedSize),
                                      arrType->itemType};
                    AnyObject srcItem{PLY_PTR_OFFSET(srcArr->items, uptr(i) * arrType->itemType->fixedSize),
                                      arrType->itemType};
                    dstItem.copyConstructFrom(srcItem);
                }
            }
            break;
        }
        case TypeKey::FixedArray: {
            auto* fixedArrType = (FixedArrayTypeInfo*) this->type;
            for (u32 i = 0; i < fixedArrType->numItems; i++) {
                AnyObject dstItem{PLY_PTR_OFFSET(this->data, uptr(i) * fixedArrType->itemType->fixedSize),
                                  fixedArrType->itemType};
                AnyObject srcItem{PLY_PTR_OFFSET(src.data, uptr(i) * fixedArrType->itemType->fixedSize),
                                  fixedArrType->itemType};
                dstItem.copyConstructFrom(srcItem);
            }
            break;
        }
        case TypeKey::Owned: {
            auto* ownedType = (OwnedTypeInfo*) this->type;
            void* srcPtr = *(void**) src.data;
            if (srcPtr) {
                void* dstPtr = Heap::alloc(ownedType->targetType->fixedSize);
                AnyObject dstTarget{dstPtr, ownedType->targetType};
                AnyObject srcTarget{srcPtr, ownedType->targetType};
                dstTarget.copyConstructFrom(srcTarget);
                *(void**) this->data = dstPtr;
            }
            break;
        }
        case TypeKey::AnyOwnedObject: {
            auto* dstOwnedObj = (AnyOwnedObject*) this->data;
            auto* srcOwnedObj = (const AnyOwnedObject*) src.data;
            if (srcOwnedObj->data) {
                dstOwnedObj->data = Heap::alloc(srcOwnedObj->type->fixedSize);
                dstOwnedObj->type = srcOwnedObj->type;
                dstOwnedObj->copyConstructFrom(*srcOwnedObj);
            }
            break;
        }
        case TypeKey::Map: {
            auto* mapType = (MapTypeInfo*) this->type;
            BaseMap* dstMap = (BaseMap*) this->data;
            const BaseMap* srcMap = (const BaseMap*) src.data;
            dstMap->numIndices = srcMap->numIndices;
            dstMap->numAllocatedIndices = srcMap->numAllocatedIndices;
            if (srcMap->numAllocatedIndices > 0) {
                dstMap->indices = (s32*) Heap::alloc(sizeof(s32) * srcMap->numAllocatedIndices);
                memcpy(dstMap->indices, srcMap->indices, sizeof(s32) * srcMap->numAllocatedIndices);
            }
            dstMap->numItems = srcMap->numItems;
            dstMap->allocated = srcMap->allocated;
            if (srcMap->allocated > 0) {
                u32 keySize = mapType->keyType->fixedSize;
                u32 valueOffset = alignToPowerOf2(keySize, mapType->valueType->alignment);
                u32 itemAlign = max(mapType->keyType->alignment, mapType->valueType->alignment);
                u32 itemSize = alignToPowerOf2(valueOffset + mapType->valueType->fixedSize, itemAlign);
                dstMap->items = Heap::alloc(itemSize * srcMap->allocated);
                for (u32 i = 0; i < srcMap->numItems; i++) {
                    void* srcItemPtr = PLY_PTR_OFFSET(srcMap->items, uptr(i) * itemSize);
                    void* dstItemPtr = PLY_PTR_OFFSET(dstMap->items, uptr(i) * itemSize);
                    AnyObject dstKey{dstItemPtr, mapType->keyType};
                    AnyObject srcKey{srcItemPtr, mapType->keyType};
                    dstKey.copyConstructFrom(srcKey);
                    AnyObject dstValue{PLY_PTR_OFFSET(dstItemPtr, valueOffset), mapType->valueType};
                    AnyObject srcValue{PLY_PTR_OFFSET(srcItemPtr, valueOffset), mapType->valueType};
                    dstValue.copyConstructFrom(srcValue);
                }
            }
            break;
        }
        case TypeKey::Variant: {
            auto* variantType = (VariantTypeInfo*) this->type;
            u32 subtype = *(const u32*) src.data;
            *(u32*) this->data = subtype;
            if (subtype > 0) {
                u32 align = 4;
                for (TypeInfo* sub : variantType->subtypes) {
                    align = max(align, sub->alignment);
                }
                PLY_ASSERT(subtype <= variantType->subtypes.numItems());
                TypeInfo* activeType = variantType->subtypes[subtype - 1];
                AnyObject dstValue{PLY_PTR_OFFSET(this->data, align), activeType};
                AnyObject srcValue{PLY_PTR_OFFSET(src.data, align), activeType};
                dstValue.copyConstructFrom(srcValue);
            }
            break;
        }
        default: {
            PLY_ASSERT(0);
            break;
        }
    }
}

//-----------------------------------
// AnyOwnedObject
//-----------------------------------
static AnyOwnedObject create(TypeInfo* type) {
    AnyOwnedObject result;
    result.data = Heap::alloc(type->fixedSize);
    result.type = type;
    result.construct();
    return result;
}

//-----------------------------------
// AnyArray
//-----------------------------------

AnyArray::AnyArray(const AnyArray& other) {
    this->itemType = other.itemType;
    if (other.numItems > 0) {
        this->alloc(other.numItems);
        for (u32 i = 0; i < other.numItems; i++) {
            (*this)[i].copyConstructFrom(other[i]);
        }
        this->numItems = other.numItems;
    }
}

AnyArray::AnyArray(AnyArray&& other) {
    this->items = other.items;
    this->itemType = other.itemType;
    this->numItems = other.numItems;
    this->allocated = other.allocated;
    new (&other) AnyArray();
}

AnyArray& AnyArray::operator=(const AnyArray& other) {
    if (this != &other) {
        this->~AnyArray();
        new (this) AnyArray(other);
    }
    return *this;
}

AnyArray& AnyArray::operator=(AnyArray&& other) {
    if (this != &other) {
        this->~AnyArray();
        new (this) AnyArray(std::move(other));
    }
    return *this;
}

AnyArray::~AnyArray() {
    for (u32 i = 0; i < this->numItems; i++) {
        (*this)[i].destruct();
    }
    Heap::free(this->items);
}

void BaseArray::clear(TypeInfo* itemType) {
    if (this->items) {
        for (u32 i = 0; i < this->numItems; i++) {
            AnyObject item{PLY_PTR_OFFSET(this->items, uptr(i) * itemType->fixedSize), itemType};
            item.destruct();
        }
        Heap::free(this->items);
        this->items = nullptr;
        this->numItems = 0;
        this->allocated = 0;
    }
}

void BaseArray::reserve(TypeInfo* itemType, u32 numItems) {
    if (numItems > this->allocated) {
        if (this->allocated == 0) {
            this->allocated = numItems;
        } else {
            this->allocated = roundUpToPowerOf2(numItems);
        }
        this->items = Heap::realloc(this->items, uptr(itemType->fixedSize) * this->allocated);
    }
}

void BaseArray::resize(TypeInfo* itemType, u32 numItems) {
    for (u32 i = numItems; i < this->numItems; i++) {
        AnyObject item{PLY_PTR_OFFSET(this->items, uptr(i) * itemType->fixedSize), itemType};
        item.destruct();
    }
    this->reserve(itemType, numItems);
    for (u32 i = this->numItems; i < numItems; i++) {
        AnyObject item{PLY_PTR_OFFSET(this->items, uptr(i) * itemType->fixedSize), itemType};
        item.construct();
    }
    this->numItems = numItems;
}

void AnyArray::resize(u32 numItems) {
    for (u32 i = numItems; i < this->numItems; i++) {
        (*this)[i].destruct();
    }
    this->reserve(numItems);
    for (u32 i = this->numItems; i < numItems; i++) {
        (*this)[i].construct();
    }
    this->numItems = numItems;
}

void AnyArray::reserve(u32 numItems) {
    if (numItems > this->allocated) {
        if (this->allocated == 0) {
            this->allocated = numItems;
        } else {
            this->allocated = roundUpToPowerOf2(numItems); // FIXME: Generalize to other resize strategies?
        }
        this->items = Heap::realloc(this->items, uptr(this->itemType->fixedSize) * this->allocated);
    }
}

void AnyArray::clear() {
    this->~AnyArray();
    new (this) AnyArray;
}

void AnyArray::alloc(u32 numItems) {
    this->allocated = numItems;
    this->items = Heap::alloc(uptr(this->itemType->fixedSize) * this->allocated);
    this->numItems = 0;
}

//-----------------------------------
// JSON conversion
//-----------------------------------

// Resolve a "type" JSON node to a TypeInfo*. Strings map to primitive types;
// objects with "key":"struct" and "members" synthesize a new StructTypeInfo.
static TypeInfo* resolveTypeFromJson(ReadFromJsonArgs& args, const json::Node& typeNode) {
    if (typeNode.isText()) {
        StringView name = typeNode.text();
        if (name == "bool")
            return getTypeInfo((bool*) nullptr);
        if (name == "s8")
            return getTypeInfo((s8*) nullptr);
        if (name == "s16")
            return getTypeInfo((s16*) nullptr);
        if (name == "s32")
            return getTypeInfo((s32*) nullptr);
        if (name == "s64")
            return getTypeInfo((s64*) nullptr);
        if (name == "u8")
            return getTypeInfo((u8*) nullptr);
        if (name == "u16")
            return getTypeInfo((u16*) nullptr);
        if (name == "u32")
            return getTypeInfo((u32*) nullptr);
        if (name == "u64")
            return getTypeInfo((u64*) nullptr);
        if (name == "float")
            return getTypeInfo((float*) nullptr);
        if (name == "double")
            return getTypeInfo((double*) nullptr);
        if (name == "String")
            return getTypeInfo((String*) nullptr);
#if defined(PLY_ENABLE_MATH_REFLECT)
        if (name == "Float2")
            return getTypeInfo((Float2*) nullptr);
        if (name == "Float3")
            return getTypeInfo((Float3*) nullptr);
        if (name == "Float4")
            return getTypeInfo((Float4*) nullptr);
        if (name == "Mat4x4")
            return getTypeInfo((Mat4x4*) nullptr);
        if (name == "Rect")
            return getTypeInfo((Rect*) nullptr);
#endif
        PLY_ASSERT(0 && "Unknown type name");
        return nullptr;
    }

    // Must be an object describing a struct with "key" and "members"
    StringView key = typeNode.get("key").text();
    PLY_ASSERT(key == "struct");

    const json::Node& membersNode = typeNode.get("members");
    PLY_ASSERT(membersNode.isObject());

    // Resolve all member types first, and compute layout.
    Array<StructTypeInfo::Member> members;
    u32 structAlign = 1;
    u32 structSize = 0;
    for (const auto& entry : membersNode.object().items.items()) {
        TypeInfo* memberType = resolveTypeFromJson(args, entry.value);
        u32 offset = alignToPowerOf2(structSize, memberType->alignment);
        members.append({String(entry.key), offset, memberType});
        structSize = offset + memberType->fixedSize;
        structAlign = max(structAlign, memberType->alignment);
    }
    structSize = alignToPowerOf2(structSize, structAlign);

    // Allocate and initialize the synthesized StructTypeInfo
    void* mem = Heap::alloc(sizeof(StructTypeInfo));
    StructTypeInfo* info = new (mem) StructTypeInfo{(s8*) nullptr, {}};
    info->fixedSize = structSize;
    info->alignment = structAlign;
    info->construct = [](StructTypeInfo* typeInfo, void* obj) {
        // Construct each member (zero-init first, then construct for non-trivial types).
        memset(obj, 0, typeInfo->fixedSize);
        StructTypeInfo* st = (StructTypeInfo*) typeInfo;
        for (const auto& member : st->members) {
            AnyObject memberObj{PLY_PTR_OFFSET(obj, member.offset), member.type};
            memberObj.construct();
        }
    };
    info->destruct = [](StructTypeInfo* typeInfo, void* obj) {
        // Destruct each member in reverse order.
        StructTypeInfo* st = (StructTypeInfo*) typeInfo;
        for (u32 i = st->members.numItems(); i > 0; i--) {
            const auto& member = st->members[i - 1];
            AnyObject memberObj{PLY_PTR_OFFSET(obj, member.offset), member.type};
            memberObj.destruct();
        }
    };
    info->copyConstruct = [](StructTypeInfo* typeInfo, void* dst, const void* src) {
        // Copy-construct each member from the source.
        StructTypeInfo* st = (StructTypeInfo*) typeInfo;
        for (const auto& member : st->members) {
            AnyObject dstMember{PLY_PTR_OFFSET(dst, member.offset), member.type};
            AnyObject srcMember{PLY_PTR_OFFSET(src, member.offset), member.type};
            dstMember.copyConstructFrom(srcMember);
        }
    };

    // Move member list to the StructTypeInfo
    info->members = std::move(members);

    // Append to list of synthesized structs
    args.synthStructTypes.append(Owned<StructTypeInfo>::adopt(info));

    return info;
}

StringView TypeInfo::getName() const {
    switch (this->key) {
        case TypeKey::Bool:
            return "bool";
        case TypeKey::S8:
            return "s8";
        case TypeKey::S16:
            return "s16";
        case TypeKey::S32:
            return "s32";
        case TypeKey::S64:
            return "s64";
        case TypeKey::U8:
            return "u8";
        case TypeKey::U16:
            return "u16";
        case TypeKey::U32:
            return "u32";
        case TypeKey::U64:
            return "u64";
        case TypeKey::Float:
            return "float";
        case TypeKey::Double:
            return "double";
#if defined(PLY_ENABLE_MATH_REFLECT)
        case TypeKey::Float2:
            return "Float2";
        case TypeKey::Float3:
            return "Float3";
        case TypeKey::Float4:
            return "Float4";
        case TypeKey::Mat4x4:
            return "Mat4x4";
        case TypeKey::Rect:
            return "Rect";
#endif
        case TypeKey::String:
            return "String";
        case TypeKey::Struct:
            return ((StructTypeInfo*) this)->name;
        default:
            return {};
    }
}

// Deserialize a JSON node into an AnyObject. Dispatches on the object's TypeKey.
void readFromJson(ReadFromJsonArgs& args, AnyObject obj, const json::Node& root) {
    switch (obj.type->key) {
        case TypeKey::AnyOwnedObject: {
            // Expect { "type": ..., "value": ... }
            TypeInfo* innerType = resolveTypeFromJson(args, root.get("type"));
            AnyOwnedObject* ownedObj = (AnyOwnedObject*) obj.data;
            ownedObj->data = Heap::alloc(innerType->fixedSize);
            ownedObj->type = innerType;
            ownedObj->construct();
            readFromJson(args, {ownedObj->data, innerType}, root.get("value"));
            break;
        }
        case TypeKey::AnyArray: {
            // Expect { "type": ..., "data": [...] }
            TypeInfo* itemType = resolveTypeFromJson(args, root.get("type"));
            AnyArray* anyArr = (AnyArray*) obj.data;
            anyArr->itemType = itemType;
            const json::Node& dataNode = root.get("data");
            const auto& items = dataNode.arrayView();
            anyArr->alloc(items.numItems());
            for (u32 i = 0; i < items.numItems(); i++) {
                AnyObject item{PLY_PTR_OFFSET(anyArr->items, uptr(i) * itemType->fixedSize), itemType};
                item.construct();
                readFromJson(args, item, items[i]);
            }
            anyArr->numItems = items.numItems();
            break;
        }
        case TypeKey::Bool: {
            *(bool*) obj.data = root.getBool();
            break;
        }
        case TypeKey::S8: {
            *(s8*) obj.data = numericCast<s8>(root.getNumber());
            break;
        }
        case TypeKey::S16: {
            *(s16*) obj.data = numericCast<s16>(root.getNumber());
            break;
        }
        case TypeKey::S32: {
            *(s32*) obj.data = numericCast<s32>(root.getNumber());
            break;
        }
        case TypeKey::S64: {
            *(s64*) obj.data = numericCast<s64>(root.getNumber());
            break;
        }
        case TypeKey::U8: {
            *(u8*) obj.data = numericCast<u8>(root.getNumber());
            break;
        }
        case TypeKey::U16: {
            *(u16*) obj.data = numericCast<u16>(root.getNumber());
            break;
        }
        case TypeKey::U32: {
            *(u32*) obj.data = numericCast<u32>(root.getNumber());
            break;
        }
        case TypeKey::U64: {
            *(u64*) obj.data = numericCast<u64>(root.getNumber());
            break;
        }
        case TypeKey::Float: {
            *(float*) obj.data = (float) root.getNumber();
            break;
        }
        case TypeKey::Double: {
            *(double*) obj.data = root.getNumber();
            break;
        }
#if defined(PLY_ENABLE_MATH_REFLECT)
        case TypeKey::Float2: {
            const auto& arr = root.arrayView();
            PLY_ASSERT(arr.numItems() == 2);
            Float2* v = (Float2*) obj.data;
            v->x = (float) arr[0].getNumber();
            v->y = (float) arr[1].getNumber();
            break;
        }
        case TypeKey::Float3: {
            const auto& arr = root.arrayView();
            PLY_ASSERT(arr.numItems() == 3);
            Float3* v = (Float3*) obj.data;
            v->x = (float) arr[0].getNumber();
            v->y = (float) arr[1].getNumber();
            v->z = (float) arr[2].getNumber();
            break;
        }
        case TypeKey::Float4: {
            const auto& arr = root.arrayView();
            PLY_ASSERT(arr.numItems() == 4);
            Float4* v = (Float4*) obj.data;
            v->x = (float) arr[0].getNumber();
            v->y = (float) arr[1].getNumber();
            v->z = (float) arr[2].getNumber();
            v->w = (float) arr[3].getNumber();
            break;
        }
        case TypeKey::Mat4x4: {
            const auto& jCols = root.arrayView();
            PLY_ASSERT(jCols.numItems() == 4);
            Mat4x4* m = (Mat4x4*) obj.data;
            for (u32 c = 0; c < 4; c++) {
                const auto& jRow = jCols[c].arrayView();
                PLY_ASSERT(jRow.numItems() == 4);
                Float4* v = &(*m)[c];
                v->x = (float) jRow[0].getNumber();
                v->y = (float) jRow[1].getNumber();
                v->z = (float) jRow[2].getNumber();
                v->w = (float) jRow[3].getNumber();
            }
            break;
        }
        case TypeKey::Rect: {
            const auto& arr = root.arrayView();
            PLY_ASSERT(arr.numItems() == 4);
            float* f = (float*) obj.data;
            for (u32 i = 0; i < 4; i++) {
                f[i] = (float) arr[i].getNumber();
            }
            break;
        }
#endif
        case TypeKey::String: {
            new (obj.data) String(root.text());
            break;
        }
        case TypeKey::Struct: {
            auto* structType = (StructTypeInfo*) obj.type;
            for (const auto& member : structType->members) {
                AnyObject memberObj{PLY_PTR_OFFSET(obj.data, member.offset), member.type};

                if (member.type->key == TypeKey::Variant) {
                    // Variant member: fields are hoisted to the parent object level
                    // with a "type" discriminator in the same object.
                    // Delegate to the Variant case which handles this layout.
                    readFromJson(args, memberObj, root);
                } else {
                    readFromJson(args, memberObj, root.get(member.name));
                }
            }
            break;
        }
        case TypeKey::Array: {
            auto* arrType = (ArrayTypeInfo*) obj.type;
            BaseArray* baseArr = (BaseArray*) obj.data;
            const auto& items = root.arrayView();
            baseArr->resize(arrType->itemType, items.numItems());
            for (u32 i = 0; i < baseArr->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(baseArr->items, uptr(i) * arrType->itemType->fixedSize),
                               arrType->itemType};
                readFromJson(args, item, items[i]);
            }
            break;
        }
        case TypeKey::FixedArray: {
            auto* fixedArrType = (FixedArrayTypeInfo*) obj.type;
            const auto& items = root.arrayView();
            PLY_ASSERT(items.numItems() == fixedArrType->numItems);
            for (u32 i = 0; i < fixedArrType->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(obj.data, uptr(i) * fixedArrType->itemType->fixedSize),
                               fixedArrType->itemType};
                readFromJson(args, item, items[i]);
            }
            break;
        }
        case TypeKey::Map: {
            auto* mapType = (MapTypeInfo*) obj.type;
            BaseMap* baseMap = (BaseMap*) obj.data;
            u32 keySize = mapType->keyType->fixedSize;
            u32 valueOffset = alignToPowerOf2(keySize, mapType->valueType->alignment);
            u32 itemAlign = max(mapType->keyType->alignment, mapType->valueType->alignment);
            u32 itemSize = alignToPowerOf2(valueOffset + mapType->valueType->fixedSize, itemAlign);
            PLY_ASSERT(mapType->keyType == getTypeInfo((String*) nullptr) &&
                       "Map keys must be String for JSON deserialization");
            u32 numItems = root.object().items.items().numItems();
            if (numItems > 0) {
                baseMap->items = Heap::alloc(itemSize * numItems);
                baseMap->allocated = numItems;
                baseMap->indices = (s32*) Heap::alloc(sizeof(s32) * roundUpToPowerOf2(numItems));
                baseMap->numAllocatedIndices = roundUpToPowerOf2(numItems);
                u32 index = 0;
                for (const auto& entry : root.object().items) {
                    void* itemPtr = PLY_PTR_OFFSET(baseMap->items, uptr(index) * itemSize);
                    // Construct key
                    new (itemPtr) String(entry.key);
                    // Construct value
                    AnyObject valueObj{PLY_PTR_OFFSET(itemPtr, valueOffset), mapType->valueType};
                    valueObj.construct();
                    readFromJson(args, valueObj, entry.value);
                    index++;
                }
                baseMap->numItems = numItems;
                baseMap->numIndices = numItems;
            }
            break;
        }
        case TypeKey::Owned: {
            // Expect { "type": ..., "value": ... }
            auto* ownedType = (OwnedTypeInfo*) obj.type;
            void* ptr = Heap::alloc(ownedType->targetType->fixedSize);
            AnyObject target{ptr, ownedType->targetType};
            target.construct();
            readFromJson(args, target, root);
            *(void**) obj.data = ptr;
            break;
        }
        case TypeKey::Pointer: {
            PLY_FORCE_CRASH(); // Not supported
            break;
        }
        case TypeKey::Custom: {
            auto* customType = (CustomTypeInfo*) obj.type;
            PLY_ASSERT(customType->readFromJson);
            customType->readFromJson(args, obj, root);
            break;
        }
        case TypeKey::Variant: {
            auto* variantType = (VariantTypeInfo*) obj.type;
            PLY_ASSERT(root.isObject());
            StringView typeName = root.get("type").text();
            PLY_ASSERT(typeName);

            // Look up the type name among the variant's subtypes
            TypeInfo* activeType = nullptr;
            u32 subtypeIndex = 0;
            for (u32 i = 0; i < variantType->subtypes.numItems(); i++) {
                TypeInfo* candidate = variantType->subtypes[i];
                if (candidate->getName() == typeName) {
                    activeType = candidate;
                    subtypeIndex = i + 1; // 1-based
                    break;
                }
            }
            PLY_ASSERT(activeType && "Variant subtype not found");

            // Set subtype index and construct the active type
            *(u32*) obj.data = subtypeIndex;
            u32 storageOffset = max((u32) sizeof(u32), variantType->alignment);
            AnyObject value{PLY_PTR_OFFSET(obj.data, storageOffset), activeType};
            value.construct();

            if (activeType->key == TypeKey::Struct) {
                // Struct fields are inlined alongside "type" in the same object.
                readFromJson(args, value, root);
            } else {
                // Non-struct types use a "value" wrapper.
                readFromJson(args, value, root.get("value"));
            }
            break;
        }
        default: {
            PLY_ASSERT(0 && "Unsupported TypeKey in readFromJson");
            break;
        }
    }
}

// Serialize a reflected C++ object to a json::Node tree.
// Dispatches on the object's TypeKey and recursively converts C++ data to JSON nodes.
json::Node convertToJson(const json::WriteOptions& writeOptions, AnyObject obj) {
    switch (obj.type->key) {
        case TypeKey::Bool: {
            return json::Node{json::Node::Bool{*(bool*) obj.data}};
        }
        case TypeKey::S8: {
            return json::Node::Number{static_cast<double>(*(s8*) obj.data)};
        }
        case TypeKey::S16: {
            return json::Node::Number{static_cast<double>(*(s16*) obj.data)};
        }
        case TypeKey::S32: {
            return json::Node::Number{static_cast<double>(*(s32*) obj.data)};
        }
        case TypeKey::S64: {
            return json::Node::Number{static_cast<double>(*(s64*) obj.data)};
        }
        case TypeKey::U8: {
            return json::Node::Number{static_cast<double>(*(u8*) obj.data)};
        }
        case TypeKey::U16: {
            return json::Node::Number{static_cast<double>(*(u16*) obj.data)};
        }
        case TypeKey::U32: {
            return json::Node::Number{static_cast<double>(*(u32*) obj.data)};
        }
        case TypeKey::U64: {
            return json::Node::Number{static_cast<double>(*(u64*) obj.data)};
        }
        case TypeKey::Float: {
            return json::Node::Number{static_cast<double>(*(float*) obj.data)};
        }
        case TypeKey::Double: {
            return json::Node::Number{*(double*) obj.data};
        }
#if defined(PLY_ENABLE_MATH_REFLECT)
        case TypeKey::Float2: {
            Float2* v = (Float2*) obj.data;
            json::Node arr{json::Node::Array{}};
            arr.array().append(json::Node::Number{v->x});
            arr.array().append(json::Node::Number{v->y});
            return arr;
        }
        case TypeKey::Float3: {
            Float3* v = (Float3*) obj.data;
            json::Node arr{json::Node::Array{}};
            arr.array().append(json::Node::Number{v->x});
            arr.array().append(json::Node::Number{v->y});
            arr.array().append(json::Node::Number{v->z});
            return arr;
        }
        case TypeKey::Float4: {
            Float4* v = (Float4*) obj.data;
            json::Node arr{json::Node::Array{}};
            arr.array().append(json::Node::Number{v->x});
            arr.array().append(json::Node::Number{v->y});
            arr.array().append(json::Node::Number{v->z});
            arr.array().append(json::Node::Number{v->w});
            return arr;
        }
        case TypeKey::Mat4x4: {
            Mat4x4* m = (Mat4x4*) obj.data;
            json::Node arr{json::Node::Array{}};
            for (u32 c = 0; c < 4; c++) {
                json::Node row{json::Node::Array{}};
                row.array().append(json::Node::Number{(*m)[c].x});
                row.array().append(json::Node::Number{(*m)[c].y});
                row.array().append(json::Node::Number{(*m)[c].z});
                row.array().append(json::Node::Number{(*m)[c].w});
                arr.array().append(std::move(row));
            }
            return arr;
        }
        case TypeKey::Rect: {
            float* f = (float*) obj.data;
            json::Node arr{json::Node::Array{}};
            arr.array().append(json::Node::Number{f[0]});
            arr.array().append(json::Node::Number{f[1]});
            arr.array().append(json::Node::Number{f[2]});
            arr.array().append(json::Node::Number{f[3]});
            return arr;
        }
#endif
        case TypeKey::String: {
            return json::Node::Text{String{*(String*) obj.data}};
        }
        case TypeKey::Struct: {
            auto* structType = (StructTypeInfo*) obj.type;
            json::Node result{json::Node::Object{}};
            for (const auto& member : structType->members) {
                AnyObject memberObj{PLY_PTR_OFFSET(obj.data, member.offset), member.type};

                if (member.type->key == TypeKey::Variant) {
                    // Flatten Variant members: merge the Variant's fields into the parent object
                    json::Node variantNode = convertToJson(writeOptions, memberObj);
                    if (variantNode.isObject()) {
                        for (const auto& entry : variantNode.object().items.items()) {
                            result.set(entry.key, json::Node{entry.value});
                        }
                    }
                } else {
                    json::Node value = convertToJson(writeOptions, memberObj);
                    if (value.isValid()) {
                        result.set(member.name, std::move(value));
                    }
                }
            }
            return result;
        }
        case TypeKey::Array: {
            auto* arrType = (ArrayTypeInfo*) obj.type;
            BaseArray* baseArr = (BaseArray*) obj.data;
            json::Node result{json::Node::Array{}};
            for (u32 i = 0; i < baseArr->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(baseArr->items, uptr(i) * arrType->itemType->fixedSize),
                               arrType->itemType};
                result.array().append(convertToJson(writeOptions, item));
            }
            return result;
        }
        case TypeKey::FixedArray: {
            auto* fixedArrType = (FixedArrayTypeInfo*) obj.type;
            json::Node result{json::Node::Array{}};
            for (u32 i = 0; i < fixedArrType->numItems; i++) {
                AnyObject item{PLY_PTR_OFFSET(obj.data, uptr(i) * fixedArrType->itemType->fixedSize),
                               fixedArrType->itemType};
                result.array().append(convertToJson(writeOptions, item));
            }
            return result;
        }
        case TypeKey::Map: {
            auto* mapType = (MapTypeInfo*) obj.type;
            BaseMap* baseMap = (BaseMap*) obj.data;
            PLY_ASSERT(mapType->keyType == getTypeInfo((String*) nullptr) &&
                       "Map keys must be String for JSON serialization");
            u32 keySize = mapType->keyType->fixedSize;
            u32 valueOffset = alignToPowerOf2(keySize, mapType->valueType->alignment);
            u32 itemAlign = max(mapType->keyType->alignment, mapType->valueType->alignment);
            u32 itemSize = alignToPowerOf2(valueOffset + mapType->valueType->fixedSize, itemAlign);
            json::Node result{json::Node::Object{}};
            for (u32 i = 0; i < baseMap->numItems; i++) {
                void* itemPtr = PLY_PTR_OFFSET(baseMap->items, uptr(i) * itemSize);
                StringView key = *(String*) itemPtr;
                AnyObject valueObj{PLY_PTR_OFFSET(itemPtr, valueOffset), mapType->valueType};
                result.set(key, convertToJson(writeOptions, valueObj));
            }
            return result;
        }
        case TypeKey::Owned: {
            auto* ownedType = (OwnedTypeInfo*) obj.type;
            void* ptr = *(void**) obj.data;
            if (ptr) {
                return convertToJson(writeOptions, {ptr, ownedType->targetType});
            }
            return json::Node{}; // null owned pointer -> invalid node
        }
        case TypeKey::Pointer: {
            PLY_ASSERT(0 && "Pointer types not supported in convertToJson");
            return json::Node{};
        }
        case TypeKey::Variant: {
            auto* variantType = (VariantTypeInfo*) obj.type;
            u32 subtype = *(u32*) obj.data;
            if (subtype == 0) {
                return json::Node{}; // Empty variant -> invalid node
            }
            // Compute alignment padding following the u32 subtype tag.
            u32 align = 4;
            for (TypeInfo* sub : variantType->subtypes) {
                align = max(align, sub->alignment);
            }
            PLY_ASSERT(subtype <= variantType->subtypes.numItems());
            TypeInfo* activeType = variantType->subtypes[subtype - 1];
            AnyObject value{PLY_PTR_OFFSET(obj.data, align), activeType};

            // Inline struct fields alongside the discriminator.
            // For non-object values (primitives), wrap with "value".
            json::Node result{json::Node::Object{}};
            result.set("type", json::Node::Text{String{activeType->getName()}});

            json::Node valueNode = convertToJson(writeOptions, value);
            if (valueNode.isObject()) {
                for (const auto& entry : valueNode.object().items.items()) {
                    result.set(entry.key, json::Node{entry.value});
                }
            } else if (valueNode.isValid()) {
                result.set("value", std::move(valueNode));
            }
            return result;
        }
        case TypeKey::AnyOwnedObject: {
            auto* ownedObj = (AnyOwnedObject*) obj.data;
            if (ownedObj->data && ownedObj->type) {
                return convertToJson(writeOptions, {ownedObj->data, ownedObj->type});
            }
            return json::Node{};
        }
        case TypeKey::AnyArray: {
            AnyArray* anyArr = (AnyArray*) obj.data;
            json::Node result{json::Node::Array{}};
            for (u32 i = 0; i < anyArr->numItems; i++) {
                result.array().append(convertToJson(writeOptions, (*anyArr)[i]));
            }
            return result;
        }
        case TypeKey::Custom: {
            auto* customType = (CustomTypeInfo*) obj.type;
            PLY_ASSERT(customType->writeToJson);
            json::Node result;
            customType->writeToJson(writeOptions, obj, result);
            return result;
        }
        default: {
            PLY_ASSERT(0 && "Unsupported TypeKey in convertToJson");
            return json::Node{};
        }
    }
}

// Prints the executable path and available command line options to stderr.
void CommandLineParser::printAvailableOptions(bool withHeader) const {
    Stream err = getStdErr();
    if (withHeader) {
        err.write("\nAvailable options:\n");
    }
    for (const CmdLineArgHandler& handler : this->handlers) {
        err.format("  {}", handler.arg);
        if (handler.dataMember->type->key == TypeKey::String)
            err.write(" <value>");
        err.format(": {}\n", handler.description);
    }
}

// Returns true if argument parsing succeeds.
// Otherwise, prints an error message and usage summary to stderr, then returns false.
bool CommandLineParser::apply(int argc, const char* argv[], AnyObject obj) {
    PLY_ASSERT(obj.data && obj.type && obj.type->key == TypeKey::Struct);
    this->executablePath = argv[0];

    for (int i = 1; i < argc; i++) {
        StringView arg = argv[i];

        // Non-option argument: dispatch to the default handler, if any.
        if (!arg.startsWith('-')) {
            if (this->defaultHandler) {
                this->defaultHandler(ArrayView<const char*>{argv + 1, u32(argc - 1)}, numericCast<u32>(i - 1));
            } else {
                getStdErr().format("Unknown option: {}\n", arg);
                return false;
            }
            continue;
        }

        // Split an inline value at '=' (eg. -foo=bar or -foo="bar").
        StringView namePart = arg;
        StringView inlineValue;
        bool hasInlineValue = false;
        s32 eqPos = arg.find('=');
        if (eqPos >= 0) {
            namePart = arg.left(u32(eqPos));
            inlineValue = arg.substr(u32(eqPos) + 1);
            hasInlineValue = true;
        }

        // Find a handler whose name matches namePart.
        const CmdLineArgHandler* matched = nullptr;
        for (const CmdLineArgHandler& handler : this->handlers) {
            if (namePart == handler.arg) {
                matched = &handler;
                break;
            }
        }

        // If there's no exact match, try a concatenated quoted value (eg. -foo"bar").
        if (!matched && !hasInlineValue) {
            for (const CmdLineArgHandler& handler : this->handlers) {
                if (handler.dataMember->type->key != TypeKey::String)
                    continue;
                if (arg.startsWith(handler.arg)) {
                    StringView rest = arg.substr(handler.arg.numBytes());
                    if (rest && rest[0] == '"') {
                        matched = &handler;
                        inlineValue = rest;
                        hasInlineValue = true;
                        break;
                    }
                }
            }
        }

        if (!matched) {
            getStdErr().format("Unknown option: {}\n", arg);
            return false;
        }

        // Apply the matched handler to the corresponding data member.
        const StructTypeInfo::Member* m = matched->dataMember;
        void* memberPtr = PLY_PTR_OFFSET(obj.data, m->offset);
        if (m->type->key == TypeKey::Bool) {
            *reinterpret_cast<bool*>(memberPtr) = true;
        } else if (m->type->key == TypeKey::String) {
            StringView valueView = inlineValue;
            if (hasInlineValue) {
                // Strip a single pair of surrounding double quotes, if present.
                if (valueView.numBytes() >= 2 && valueView[0] == '"' && valueView[valueView.numBytes() - 1] == '"') {
                    valueView = valueView.substr(1, valueView.numBytes() - 2);
                }
            } else {
                // Consume the next argument as the value.
                if (i + 1 >= argc) {
                    getStdErr().format("Option {} requires a value\n", matched->arg);
                    return false;
                }
                valueView = argv[++i];
            }
            *reinterpret_cast<String*>(memberPtr) = String{valueView};
        } else {
            PLY_ASSERT(0 && "Unsupported member type for command line option");
        }
    }

    return true;
}

} // namespace ply
