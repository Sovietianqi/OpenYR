#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

template<typename T>
class VectorClass {
public:
    T* Items;
    int32 Count;
    int32 Capacity;

    VectorClass() : Items(nullptr), Count(0), Capacity(0) {}

    // ------------------------------------------------------------------------
    // asm: DynamicVector_Byte_CTOR (0x5108D0) —— VectorClass<bool>(len, data)
    //   [this+4]=0 ; [this+8]=len ; byte[0Ch]=1 ; byte[0Dh]=0 ; vt=VectorClass
    //   len == 0 -> retn
    //   data != 0 -> [this+4]=data           ; 接管外部缓冲，不再分配
    //   否则       -> [this+4]=new(len) ; byte[0Dh]=1
    //
    // 降级点：原版用 +0Dh（IsAllocated）区分"接管 / 自有"，本模板没有这个
    // 标志，析构一律 free —— 因此这里只在 data == nullptr 时走分配，接管
    // 外部缓冲的调用点在本工程不存在。
    // ------------------------------------------------------------------------
    VectorClass(int32 length, T* pData)
        : Items(pData), Count(0), Capacity(0)
    {
        if (length == 0)
            return;                                 // asm: jz loc_510961
        if (Items == nullptr)
            Items = static_cast<T*>(std::malloc(sizeof(T) * static_cast<size_t>(length)));
        Capacity = length;                          // asm: mov [esi+8],eax
    }

    // ------------------------------------------------------------------------
    // asm: tl_int_SDDTOR (0x5109E0) —— TypeList<int> 的 scalar deleting dtor
    //   [this]=VectorClass<int>::vftable ; data && IsAllocated -> delete data
    //   IsAllocated=0 ; Capacity=0 ; test flag,1 -> delete this
    // 这就是编译器为本模板生成的析构 + deleting-destructor 形态，无手写逻辑。
    // ------------------------------------------------------------------------
    ~VectorClass() {
        if (Items) {
            std::free(Items);
            Items = nullptr;
        }
    }

    T& operator[](int32 index) { return Items[index]; }
    const T& operator[](int32 index) const { return Items[index]; }

    bool Add(const T& item) {
        if (Count >= Capacity) {
            int32 newCap = Capacity == 0 ? 8 : Capacity * 2;
            T* newItems = static_cast<T*>(std::realloc(Items, newCap * sizeof(T)));
            if (!newItems) return false;
            Items = newItems;
            Capacity = newCap;
        }
        Items[Count++] = item;
        return true;
    }

    bool Remove(int32 index) {
        if (index < 0 || index >= Count) return false;
        for (int32 i = index; i < Count - 1; ++i) {
            Items[i] = Items[i + 1];
        }
        --Count;
        return true;
    }

    void Clear() { Count = 0; }
    int32 GetCount() const { return Count; }
    int32 GetCapacity() const { return Capacity; }

    // ------------------------------------------------------------------------
    // asm: VecSymlinks_Find2 (0x523430)
    //
    // 8 字节步长表上的经典二分查找（symlink 表）。asm 逐段：
    //   push ebx/esi/edi
    //   test edx,edx / jle -> return 0            ; 空表
    //   mov eax,[esp+0Ch+IID] / mov ebx,[eax]     ; key = *(int32*)pIID
    // loc_52344F:
    //   mov eax,esi / cdq / sub eax,edx / sar eax,1   ; mid = lo/2
    //   mov edi,[ecx+eax*8]                       ; val = base[mid].key (+0)
    //   lea edx,[ecx+eax*8]                       ; entry = &base[mid]
    //   cmp edi,ebx / jle loc_523464
    //   mov esi,eax / jmp loc_523470              ; val >  key -> lo = mid
    // loc_523464:
    //   jz loc_52347C                             ; val == key -> return entry
    //   lea ecx,[edx+8]                           ; base = &base[mid+1]
    //   or edx,-1 / sub edx,eax / add esi,edx     ; lo = lo - mid - 1
    // loc_523470:
    //   test esi,esi / jg loc_52344F              ; while lo > 0
    // loc_523474: xor eax,eax / retn 4            ; 未命中 -> 0
    // loc_52347C: mov eax,edx / retn 4            ; 命中 -> 条目指针
    //
    // 注意：表项是 8 字节的外部记录（键在 +0），不是 T 本身，所以这里接收
    // 裸指针而非依赖 Items 类型；返回值是**条目指针**。
    // ------------------------------------------------------------------------
    static const void* Find_Entry_8(const void* pArray, int32 count,
                                    const void* pKey)
    {
        // asm: test edx,edx / jle -> xor eax,eax
        if (count <= 0 || pKey == nullptr)
            return nullptr;

        // asm: mov ebx,[eax]  -- 键是 key 对象首 dword
        const int32 key = *reinterpret_cast<const int32*>(pKey);

        const uint8* pBase = static_cast<const uint8*>(pArray);  // asm: ecx
        int32 lo = count;                                        // asm: esi

        while (lo > 0)                                    // asm: loc_52344F
        {
            // asm: mov eax,esi / cdq / sub eax,edx / sar eax,1
            const int32 mid = lo / 2;

            // asm: lea edx,[ecx+eax*8]
            const uint8* pEntry = pBase + static_cast<intptr_t>(mid) * 8;
            // asm: mov edi,[ecx+eax*8]
            const int32 val = *reinterpret_cast<const int32*>(pEntry);

            if (val > key) {                              // asm: jle 跳过
                lo = mid;                                 // asm: mov esi,eax
                continue;
            }
            if (val == key)                               // asm: jz loc_52347C
                return pEntry;

            // asm: lea ecx,[edx+8] / or edx,-1 / sub edx,eax / add esi,edx
            pBase = pEntry + 8;
            lo = lo - mid - 1;
        }

        return nullptr;                                   // asm: loc_523474
    }
};

// ============================================================================
// asm: VecSymlinks_Comparator (0x41C610)
//
//   int __cdecl VecSymlinks_Comparator(const void* a, const void* b)
//
//   给 _qsort 用的三路比较：表项是 8 字节的 Symlink 记录（键在 +0），
//   比较的是**指针所指向记录的首 dword**，不是指针本身。
//
//     eax = *(int32*)a ; ecx = *(int32*)b
//     cmp eax,ecx / jnz  -> 相等返回 0
//     xor edx,edx / setnl dl / dec edx / and edx,0FFFFFFFEh / inc edx
//        -> a > b 返回 +1，否则 -1（setnl 是**有符号**谓词）
//
//   调用点：vec_TechTypeLinks / 其它 8 字节 symlink 表的排序
//   （AbstractClass::Fetch_ID 之后的 _qsort）。
// ============================================================================
inline int32 VecSymlinks_Comparator(const void* pA, const void* pB)
{
    const int32 a = *static_cast<const int32*>(pA);
    const int32 b = *static_cast<const int32*>(pB);

    if (a == b)                       // asm: cmp eax,ecx / jnz
        return 0;                     // asm: xor eax,eax / retn

    return (a > b) ? 1 : -1;          // asm: setnl/dl-dec-and-inc 链
}
