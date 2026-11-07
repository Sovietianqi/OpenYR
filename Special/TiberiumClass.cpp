#include "TiberiumClass.h"

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Abstract/OverlayTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include <Animations/AnimClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Rendering/ConvertClass.h>
#include <Scenario/ScenarioClass.h>
// asm sub_42B1C0（坐标 -> 逐格表的线性下标）在本工程里的实现位置。
#include "VeinholeMonsterClass.h"

#include <cstring>
#include <cstdlib>

// ============================================================================
// TiberiumClass.cpp
//
//  Implements the ore type of the [Tiberiums] list.  The constructor appends
//  the instance to the global registry and stamps its ordinal into TypeIndex;
//  the registry is what Is_Overlay_Idx_Tiberium walks when the map asks which
//  ore an overlay index belongs to.
//
//  LoadFromINI reads Spread / SpreadPercentage / Growth / GrowthPercentage /
//  Value / Power / Color / Debris / Image, with the last one driving a small
//  switch that selects the overlay image window the ore draws from.
// ============================================================================

// ============================================================================
// Static array management
//
// TiberiumClass::Array itself is defined alongside the other type-class arrays
// in Game/Globals.cpp, matching the project's convention.
// ============================================================================

void TiberiumClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TiberiumClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TiberiumClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TiberiumClass*>();
    }
}

void TiberiumClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TiberiumClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void TiberiumClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
        delete (*Array)[i];

    Array->Clear();
}

int32 TiberiumClass::GetCount()
{
    return Array ? Array->Count : 0;
}

TiberiumClass* TiberiumClass::FindByIndex(int32 index)
{
    if (Array == nullptr || index < 0 || index >= Array->Count)
        return nullptr;

    return (*Array)[index];
}

TiberiumClass* TiberiumClass::Find(const char* pID)
{
    if (pID == nullptr || Array == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i) {
        TiberiumClass* pType = (*Array)[i];
        if (pType != nullptr && _strcmpi(pType->ID, pID) == 0)
            return pType;
    }

    return nullptr;
}

TiberiumClass* TiberiumClass::FindOrAllocate(const char* pID)
{
    if (pID == nullptr)
        return nullptr;

    if (TiberiumClass* pFound = Find(pID))
        return pFound;

    return new TiberiumClass(pID);
}

// ============================================================================
// Constructor / destructor
// ============================================================================

TiberiumClass::TiberiumClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , TypeIndex(-1)
    , Spread(0)
    , SpreadPercentage(0.1)     // the binary seeds 0x3FB99999 = 0.1
    , Growth(0)
    , GrowthPercentage(0.1)
    , Value(0)
    , Power(0)
    , Color(0)
    , Debris()
    , Image(nullptr)
    , ImageStart(0)
    , NumImages(0)
    , ImageCount(0)
    , field_F0(0)
    , field_F4(nullptr)
    , field_F8(nullptr)
    , field_FC(nullptr)
    , field_100(0)
    , field_104(0)
    , field_108(0)
    , field_10C(0)
    , field_110(nullptr)
    , field_114(nullptr)
    , field_118(nullptr)
    , field_11C(0)
    , field_120(0)
    , field_124(0)
{
    if (Array == nullptr)
        Init_Array();

    TypeIndex = Array->Count;
    Array->Add(this);
}

TiberiumClass::~TiberiumClass()
{
    // Drop ourselves from the global registry so stored ordinals never point at
    // freed memory, then release whatever the map-generation pass attached.
    if (Array != nullptr) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }

    if (field_F4  != nullptr) YRMemory::Deallocate(field_F4);
    if (field_F8  != nullptr) YRMemory::Deallocate(field_F8);
    if (field_FC  != nullptr) YRMemory::Deallocate(field_FC);
    if (field_110 != nullptr) YRMemory::Deallocate(field_110);
    if (field_114 != nullptr) YRMemory::Deallocate(field_114);
    if (field_118 != nullptr) YRMemory::Deallocate(field_118);
}

// ============================================================================
// COM plumbing
// ============================================================================

HRESULT TiberiumClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    // CLSID_TiberiumClass, copied straight out of .data by the binary.
    pClassID->Data1 = static_cast<uint32>(AbstractType::Tiberium);
    return S_OK;
}

HRESULT TiberiumClass::Load(IStream* pStm)
{
    (void)pStm;
    return S_OK;
}

HRESULT TiberiumClass::Save(IStream* pStm, BOOL fClearDirty)
{
    (void)pStm;
    (void)fClearDirty;
    return S_OK;
}

AbstractType TiberiumClass::WhatAmI() const
{
    return AbstractType::Tiberium;
}

int32 TiberiumClass::Size() const
{
    // mov eax, 128h
    return 0x128;
}

int32 TiberiumClass::GetArrayIndex() const
{
    return TypeIndex;
}

// ============================================================================
// ComputeCRC
//
// The binary folds the parent CRC first, then each scalar in order: Spread,
// Growth, Value, Power, Color, ImageStart, NumImages.
// ============================================================================

void TiberiumClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractTypeClass::ComputeCRC(crc);

    crc.AddData(&Spread, sizeof(Spread));
    crc.AddData(&Growth, sizeof(Growth));
    crc.AddData(&Value, sizeof(Value));
    crc.AddData(&Power, sizeof(Power));
    crc.AddData(&Color, sizeof(Color));
    crc.AddData(&ImageStart, sizeof(ImageStart));
    crc.AddData(&NumImages, sizeof(NumImages));
}

// ============================================================================
// PointerGotInvalid
//
// The binary walks the debris vector backwards and removes every entry that
// matches the expiring pointer, shifting the tail down over the hole.
// ============================================================================

void TiberiumClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed)
{
    (void)removed;

    if (pAbstract == nullptr)
        return;

    for (int32 i = Debris.Count - 1; i >= 0; --i) {
        if (Debris[i] == pAbstract)
            Debris.Remove(i);
    }
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool TiberiumClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* pSection = ID;

    Spread           = pINI->ReadInteger(pSection, "Spread", Spread);
    SpreadPercentage = pINI->ReadDouble(pSection, "SpreadPercentage", SpreadPercentage);
    Growth           = pINI->ReadInteger(pSection, "Growth", Growth);
    GrowthPercentage = pINI->ReadDouble(pSection, "GrowthPercentage", GrowthPercentage);
    Value            = pINI->ReadInteger(pSection, "Value", Value);
    Power            = pINI->ReadInteger(pSection, "Power", Power);
    Color            = pINI->ReadColorSchemeIndex(pSection, "Color", Color);

    char debrisText[0x80];
    debrisText[0] = '\0';
    if (pINI->ReadString(pSection, "Debris", "", debrisText, sizeof(debrisText)) > 0) {
        // A comma separated run of animation IDs; each is resolved against the
        // animation type table and appended in the order it appears.
        char* pToken = std::strtok(debrisText, ",");
        while (pToken != nullptr) {
            if (pToken[0] != '\0') {
                AnimTypeClass* pAnim = AnimTypeClass::FindOrAllocate(pToken);
                if (pAnim != nullptr)
                    Debris.Add(pAnim);
            }
            pToken = std::strtok(nullptr, ",");
        }
    }

    const int32 image = pINI->ReadInteger(pSection, "Image", -1);

    if (image != -1) {
        switch (image) {
            case 3:
                ImageStart = 0x1B;
                NumImages  = 12;
                ImageCount = 12;
                break;

            case 4:
                ImageStart = 0x7F;
                NumImages  = 12;
                ImageCount = 12;
                break;

            case 5:
                ImageStart = 0x93;
                NumImages  = 12;
                ImageCount = 12;
                break;

            default:
                ImageStart = 0x66;
                NumImages  = 12;
                ImageCount = 8;
                break;
        }
    }

    // Resolve the overlay art the ore renders as.
    Image = OverlayTypeClass::FindByIndex(ImageStart);

    return true;
}

// ============================================================================
// CreateFromINIList
// ============================================================================

bool TiberiumClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return true;

    if (Array == nullptr)
        Init_Array();

    if (pINI->GetSection("Tiberiums") == nullptr)
        return true;

    const int32 count = pINI->GetKeyCount("Tiberiums");

    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->GetKeyName("Tiberiums", i);
        if (pKeyName == nullptr)
            continue;

        char name[0x18];
        name[0] = '\0';
        if (pINI->ReadString("Tiberiums", pKeyName, "", name, sizeof(name)) <= 0)
            continue;

        const int32 ordinal = std::atoi(pKeyName);

        TiberiumClass* pType = nullptr;
        if (ordinal < Array->Count)
            pType = (*Array)[ordinal];

        if (pType == nullptr)
            pType = new TiberiumClass(name);

        if (pType != nullptr)
            pType->LoadFromINI(pINI);
    }

    return true;
}

// ============================================================================
// Is_Overlay_Idx_Tiberium
// ============================================================================

int32 Is_Overlay_Idx_Tiberium(int32 overlayIndex)
{
    if (overlayIndex == -1)
        return -1;

    OverlayTypeClass* pOverlay = OverlayTypeClass::FindByIndex(overlayIndex);
    if (pOverlay == nullptr)
        return -1;

    if (!pOverlay->Is_Tiberium())
        return -1;

    if (TiberiumClass::Array == nullptr)
        return -1;

    const int32 total = TiberiumClass::Array->Count;

    for (int32 i = 0; i < total; ++i) {
        TiberiumClass* pType = (*TiberiumClass::Array)[i];
        if (pType == nullptr)
            continue;

        const int32 start = pType->ImageStart;

        if (overlayIndex >= start && overlayIndex < start + pType->NumImages)
            return pType->TypeIndex;

        if (overlayIndex >= start + pType->NumImages &&
            overlayIndex < start + pType->NumImages + pType->ImageCount)
            return pType->TypeIndex;
    }

    return -1;
}

// ============================================================================
// asm: Tiberiums_SetupRemap (0x4261D0)
//
//   mov eax,vec_Anims.length / test eax,eax / jle loc_42626B
//   xor edi,edi
// loc_4261E4:
//   mov eax,vec_Anims.data / mov esi,[eax+edi*4] / test esi,esi / jz next
//   mov ecx,[esi+0C8h] / test ecx,ecx / jz next
//   mov al,[ecx+360h] / test al,al / jz next        ; 非 tiberium 辉光跳过
//   lea edx,[esi+9Ch]                                  ; coord = {+9C,+A0,+A4}
//   call MapClass_Get_Target_Cell / test eax,eax / jz next
//   mov ecx,eax / call CellClass_GetContainedTiberiumIndex
//   cmp eax,0FFFFFFFFh / jz next
//   mov ecx,vec_Tiberiums.data / mov eax,[ecx+eax*4] / test eax,eax / jz next
//   mov edx,[eax+0C0h]                                 ; tiberium 颜色下标
//   mov eax,ColorScheme.data / mov ecx,[eax+edx*4] / test ecx,ecx / jz next
//   mov edx,[ecx+30Ch] / mov [esi+0D4h],edx
// next:
//   inc edi / cmp edi,vec_Anims.length / jl loc_4261E4
//
//   原版直接寻址 vec_Anims / vec_Tiberiums / ColorScheme.data。这里改走
//   真实类的静态数组与公开访问器，遍历与过滤顺序保持逐句一致。
// ============================================================================
void TiberiumClass::Setup_Remap()
{
    // asm: mov eax,vec_Anims.length / test eax,eax / jle loc_42626B
    if (AnimClass::Array == nullptr || AnimClass::Array->Count <= 0)
        return;

    const int32 count = AnimClass::Array->Count;

    for (int32 i = 0; i < count; ++i)                // asm: loc_4261E4
    {
        // asm: mov esi,[vec_Anims.data + edi*4] / test esi,esi / jz next
        AnimClass* pAnim = (*AnimClass::Array)[i];
        if (pAnim == nullptr)
            continue;

        // asm: mov ecx,[esi+0C8h] / test ecx,ecx / jz next
        AnimTypeClass* pType = pAnim->Type;
        if (pType == nullptr)
            continue;

        // asm: mov al,[ecx+360h] / test al,al / jz next
        // asm 的 +0x360 是 AnimTypeClass 上的"tiberium 辉光"标志；该字段在
        // OpenYR 尚未确证偏移（铁律①：宁可留白，不写错），暂用类型上的
        // IsTiberium 语义位代替，后续核实 +0x360 后再收紧。
        if (!pType->IsTiberium)
            continue;

        // asm: lea edx,[esi+9Ch]  -- coord = { +9C, +A0, +A4 }
        CoordStruct coord = pAnim->Location;

        // asm: call MapClass_Get_Target_Cell / test eax,eax / jz next
        if (MapClass::Instance == nullptr)
            continue;
        CellClass* pCell = MapClass::Instance->GetCellAt(coord);
        if (pCell == nullptr)
            continue;

        // asm: call CellClass_GetContainedTiberiumIndex / cmp eax,-1 / jz next
        const int32 tibIdx = pCell->Get_Contained_Tiberium_Index();
        if (tibIdx == -1)
            continue;

        // asm: mov ecx,vec_Tiberiums.data / mov eax,[ecx+eax*4] / jz next
        if (TiberiumClass::Array == nullptr)
            continue;
        if (tibIdx < 0 || tibIdx >= TiberiumClass::Array->Count)
            continue;
        TiberiumClass* pTib = (*TiberiumClass::Array)[tibIdx];
        if (pTib == nullptr)
            continue;

        // asm: mov edx,[eax+0C0h]   -- tiberium 颜色方案下标
        const int32 schemeIdx = pTib->Color;

        // asm: mov eax,ColorScheme.data / mov ecx,[eax+edx*4] / jz next
        if (ColorScheme::Array == nullptr)
            continue;
        if (schemeIdx < 0 || schemeIdx >= ColorScheme::Array->Count)
            continue;
        ColorScheme* pScheme = (*ColorScheme::Array)[schemeIdx];
        if (pScheme == nullptr)
            continue;

        // asm: mov edx,[ecx+30Ch] / mov [esi+0D4h],edx
        // ColorScheme 的 +0x30C 落在 unknown_314[0x1C] 之前，OpenYR 侧尚未
        // 具名；用偏移访问器取值，避免臆造字段名。
        const int32 remap = *reinterpret_cast<const int32*>(
            reinterpret_cast<const uint8*>(pScheme) + 0x30C);
        pAnim->RenderRemapIndex = remap;   // asm: mov [esi+0D4h],edx
    }
}

// ============================================================================
// SmthA / SmthB -- 地图生成期的 tiberium 分布暂存区重建
//
//   asm: TiberiumClass_SmthA (0x722D00) / TiberiumClass_SmthB (0x722240)
//
//   两个例程逐字节同构，唯一差别就是下面五个偏移/例程。原版把整段循环体
//   写了两遍，这里抽成一份共用骨架，由参数指定偏移。
//
//   CounterClass 的裸布局（0x14 字节）：
//     +0x00 count   +0x04 area   +0x08 data   +0x0C 0   +0x10 -1
// ============================================================================
namespace {

inline void* Tib_New(uint32 size)
{
    return YRMemory::Allocate(size);
}

inline void Tib_Delete(void* p)
{
    if (p != nullptr) {
        YRMemory::Deallocate(p);
    }
}

// 清空主槽里的计数表：先按「数量」把数据块清零（原版循环写到 count 个
// 之后还会多写一格），再删数据块与表体本身。
inline void Tib_ReleaseCounter(void* pCounter)
{
    if (pCounter == nullptr) {
        return;
    }

    uint8* ctr = static_cast<uint8*>(pCounter);

    // asm: mov edx,[eax] / xor ecx,ecx / cmp edx,ebx / jl done
    //      loc: mov edx,[eax+8] / inc ecx / mov [edx+ecx*4-4],ebx
    //           mov edx,[eax] / cmp ecx,edx / jle loc
    const int32 count = *reinterpret_cast<int32*>(ctr + 0x00);
    if (count >= 0) {
        for (int32 k = 1; k <= count + 1; ++k) {
            int32* data = *reinterpret_cast<int32**>(ctr + 0x08);
            data[k - 1] = 0;
        }
    }

    // asm: mov [eax],ebx / 删 [esi+8] / 删 esi
    *reinterpret_cast<int32*>(ctr + 0x00) = 0;
    Tib_Delete(*reinterpret_cast<void**>(ctr + 0x08));
    Tib_Delete(ctr);
}

} // namespace

// ============================================================================
// 共用骨架。偏移全部是从 this 起的字节偏移。
// ============================================================================
void TiberiumClass::ResetSeedScratch(int32 offMain, int32 offAux1,
                                     int32 offAux2, int32 offFlag,
                                     void (*pFinalizer)(TiberiumClass*))
{
    if (Array == nullptr) {
        return;
    }

    uint8* const self = reinterpret_cast<uint8*>(this);

    // ---- 1) 释放主槽计数表 ------------------------------------------------
    // asm: mov eax,[edi+main] / cmp eax,ebx / jz skip
    Tib_ReleaseCounter(*reinterpret_cast<void**>(self + offMain));
    *reinterpret_cast<void**>(self + offMain) = nullptr;

    // ---- 2) 释放两个辅助缓冲 ---------------------------------------------
    // asm: mov eax,[edi+aux1] / cmp eax,ebx / jz skip / 删 / 置 0
    if (void* v = *reinterpret_cast<void**>(self + offAux1)) {
        Tib_Delete(v);
        *reinterpret_cast<void**>(self + offAux1) = nullptr;
    }
    if (void* v = *reinterpret_cast<void**>(self + offAux2)) {
        Tib_Delete(v);
        *reinterpret_cast<void**>(self + offAux2) = nullptr;
    }

    // ---- 3) 清旗标 --------------------------------------------------------
    // asm: mov [edi+flag],ebx
    *reinterpret_cast<int32*>(self + offFlag) = 0;

    // ---- 4) 按地图面积重建两个辅助缓冲 -----------------------------------
    // asm: call MapClass_GetArea / mov esi,eax / lea ecx,[esi*8] / push / call new
    //      若分配失败则原版把 esi-1 的结果丢掉、直接写 0
    const int32 area = (MapClass::Instance != nullptr)
                           ? MapClass::Instance->GetArea()
                           : 0;

    void* aux1 = Tib_New(static_cast<uint32>(area) * 8u);
    *reinterpret_cast<void**>(self + offAux1) = aux1;

    // asm: call MapClass_GetArea / push eax / call new
    void* aux2 = Tib_New(static_cast<uint32>(area));
    *reinterpret_cast<void**>(self + offAux2) = aux2;

    // ---- 5) 重建主槽计数表 ------------------------------------------------
    // asm: push size CounterClass (14h) / call new
    uint8* ctr = static_cast<uint8*>(Tib_New(0x14));
    if (ctr != nullptr) {
        // asm: call MapClass_GetArea（第三次探测）
        const int32 area3 = (MapClass::Instance != nullptr)
                                ? MapClass::Instance->GetArea()
                                : 0;

        // asm: mov [esi+0Ch],ebx / mov dword [esi+10h],-1
        //      / mov [esi],ebx / mov [esi+4],eax
        *reinterpret_cast<int32*>(ctr + 0x0C) = 0;
        *reinterpret_cast<int32*>(ctr + 0x10) = -1;
        *reinterpret_cast<int32*>(ctr + 0x00) = 0;
        *reinterpret_cast<int32*>(ctr + 0x04) = area3;

        // asm: lea edx,[eax*4+4] / push edx / call new / mov [esi+8],eax
        void* data = Tib_New(static_cast<uint32>(area3) * 4u + 4u);
        *reinterpret_cast<void**>(ctr + 0x08) = data;

        // asm 清零循环同样是「写 count+1 次」
        if (area3 >= 0) {
            for (int32 k = 1; k <= area3 + 1; ++k) {
                int32* dp = *reinterpret_cast<int32**>(ctr + 0x08);
                dp[k - 1] = 0;
            }
        }
    }

    // ---- 6) 落槽并收尾 ----------------------------------------------------
    // asm: mov [edi+main],esi / mov ecx,edi / call sub_7233A0 (或 sub_7228B0)
    *reinterpret_cast<void**>(self + offMain) = ctr;
    if (pFinalizer != nullptr) {
        pFinalizer(this);
    }
}

// ============================================================================
// SmthA -- asm TiberiumClass_SmthA (0x722D00)
//
//   每个 tiberium 类型刷新一遍「A 组」暂存区（偏移 +0x0F0 一族），收尾走
//   sub_7233A0。调用者：MapSeedClass_Generate 系列。
// ============================================================================
void TiberiumClass::SmthA()
{
    if (Array == nullptr) {
        return;
    }

    for (int32 i = 0; i < Array->Count; ++i) {
        TiberiumClass* pTib = (*Array)[i];
        if (pTib == nullptr) {
            continue;   // 原版会踩空指针；这里按工程惯例跳过
        }
        pTib->ResetSeedScratch(0x110, 0x118, 0x114, 0x10C,
                               &TiberiumClass::SmthA_Finalizer);
    }
}

// ============================================================================
// SmthB -- asm TiberiumClass_SmthB (0x722240)
//
//   与 SmthA 同形，作用于「B 组」暂存区（偏移 +0x0F4 一族），收尾走
//   sub_7228B0。调用者：MapSeedClass_Generate+A40。
// ============================================================================
void TiberiumClass::SmthB()
{
    if (Array == nullptr) {
        return;
    }

    for (int32 i = 0; i < Array->Count; ++i) {
        TiberiumClass* pTib = (*Array)[i];
        if (pTib == nullptr) {
            continue;
        }
        pTib->ResetSeedScratch(0x0F4, 0x0FC, 0x0F8, 0x0F0,
                               &TiberiumClass::SmthB_Finalizer);
    }
}

// ============================================================================
// 收尾例程 —— asm sub_7233A0（SmthA）/ sub_7228B0（SmthB）
//
//   两个例程逐字节同构，只有「暂存槽偏移 + 逐格判据」不同：
//     sub_7233A0  main +0x110 / aux1 +0x118 / aux2 +0x114 / count +0x10C
//                 判据 sub_483620
//     sub_7228B0  main +0x0F4 / aux1 +0x0FC / aux2 +0x0F8 / count +0x0F0
//                 判据 sub_483690
//
//   数学含义（逐句对照 asm）：
//     1. 把主槽计数表（1-based 二叉堆）的已用槽位清零、Count 归零；
//     2. 把 aux2 的 area 个字节清零（aux2 是「本类型矿已登记」的逐格标记表）；
//     3. 用 CellIterator 走遍全图，对每一格：
//          a. 该格 overlay 解的 tiberium 序号必须等于本类型 TypeIndex（+0x98）；
//          b. 通过本组的逐格判据（见下面两个 predicate）；
//          c. 往 aux1 的第 n 条记录（8 字节 = {MapCoords 打包 4 字节, float 键}）
//             写入该格坐标，键初始化为 0.0f，n = count 槽，然后 n++；
//          d. 以该记录为元素做二叉堆上浮（键相同则立即停止，等价于按扫描
//             顺序追加），跳过条件是 Count+1 >= 容量（= area）；
//          e. 用 sub_42B1C0（本工程的 VeinholeMonsterClass::Growth_Index）
//             把坐标换成线性下标，把 aux2 那一位记号置 1。
//
//   注意：堆是 1-based，data[0] 恒空置；元素只是 aux1 缓冲里的裸指针，键恒为
//   0.0f，所以上浮循环实际只做「追加」，主槽维护的 Count / MaxEntry / MinEntry
//   三个字段才是这一轮扫描留下的可用结果。
// ============================================================================
namespace {

// 主槽 CounterClass 的裸布局（0x14 字节，见 ResetSeedScratch 的分配处）。
struct SeedCounterView {
    int32   Count;      // +0x00 已入堆的元素数
    int32   Capacity;   // +0x04 容量（= MapClass::GetArea()）
    void**  Data;       // +0x08 堆数组，1-based，data[0] 空置
    void*   MaxEntry;   // +0x0C 记录指针上界
    void*   MinEntry;   // +0x10 记录指针下界
};

// 记录 = { MapCoords（+0x00，4 字节）, float 键（+0x04） }
inline float SeedEntry_Key(const void* pEntry)
{
    return *reinterpret_cast<const float*>(static_cast<const uint8*>(pEntry) + 4);
}

// asm: loc_723495 的上浮循环（sub_7228B0 的同段在 loc_7229A5）
//
//   edx = 新元素槽位, edi = edx >> 1
//   cmp edx, [ebp+4] / jnb → 表满，整段插入被跳过（Count 不动）
//   fld [parent+4] / fcomp 新键 / test ah,41h / jnz → 父键 <= 新键即停
inline void SeedHeap_Push(SeedCounterView* pCounter, void* pEntry)
{
    const int32 slot = pCounter->Count + 1;
    if (slot >= pCounter->Capacity) {
        return;
    }

    const float key = SeedEntry_Key(pEntry);

    int32 hole   = slot;
    int32 parent = slot >> 1;

    while (hole > 1) {
        void* pParent = pCounter->Data[parent];
        // 与 fcomp + `test ah,41h` 等价：父键 <= 新键（含 NaN）即停止上浮。
        if (!(SeedEntry_Key(pParent) > key)) {
            break;
        }
        pCounter->Data[hole] = pParent;
        hole = parent;
        parent >>= 1;
    }

    pCounter->Data[hole] = pEntry;

    pCounter->Count = slot;
    if (pEntry > pCounter->MaxEntry) {
        pCounter->MaxEntry = pEntry;
    }
    if (pEntry < pCounter->MinEntry) {
        pCounter->MinEntry = pEntry;
    }
}

// 取该格 overlay 对应的 tiberium 对象；无效返回 nullptr。
inline TiberiumClass* SeedCell_Tiberium(int32 tibIdx)
{
    if (TiberiumClass::Array == nullptr) {
        return nullptr;
    }
    if (tibIdx < 0 || tibIdx >= TiberiumClass::Array->Count) {
        return nullptr;
    }
    return (*TiberiumClass::Array)[tibIdx];
}

// ============================================================================
// asm: sub_483620 —— SmthA 的逐格判据（能否继续「长矿」）
//
//   mov eax,Scenario / mov cl,[eax+34A6h] / test cl,cl / jnz → TiberiumGrowthEnabled
//   mov ecx,[esi+44h] / call __IsOverlayIdxTiberium / cmp eax,-1
//   mov cl,[esi+11Ch] / test cl,cl / jz  → VeinStage 必须为 0
//   mov ecx,[eax+0E4h]（ImageStart）/ mov dl,[esi+11Eh]（OverlayFrame）
//   dec ecx / cmp edx,ecx / jl → 要求 OverlayFrame < ImageStart - 1
//   fld [eax+0B0h]（GrowthPercentage）/ fcomp 1e-5 / test ah,1 / jz
//                               → 要求 GrowthPercentage >= 1e-5
// ============================================================================
bool SeedCell_CanGrow(CellClass* pCell)
{
    if (ScenarioClass::Instance == nullptr) {
        return false;
    }
    // asm: ScenarioClass +0x34A6
    if (!ScenarioClass::Instance->TiberiumGrowthEnabled) {
        return false;
    }

    const int32 tibIdx = pCell->Get_Contained_Tiberium_Index();
    if (tibIdx == -1) {
        return false;
    }

    // asm: mov cl,[esi+11Ch] / test cl,cl / jz
    if (pCell->VeinStage != 0) {
        return false;
    }

    TiberiumClass* pTib = SeedCell_Tiberium(tibIdx);
    if (pTib == nullptr) {
        return false;
    }

    // asm: dec ecx / cmp edx,ecx / jl  → frame < ImageStart - 1
    if (static_cast<int32>(pCell->OverlayFrame) >= pTib->ImageStart - 1) {
        return false;
    }

    // asm: fcomp 1e-5 / test ah,1 / jz → GrowthPercentage >= 1e-5（NaN 视同不达标）
    if (!(pTib->GrowthPercentage >= 1.0e-5)) {
        return false;
    }

    return true;
}

// ============================================================================
// asm: sub_483690 —— SmthB 的逐格判据（能否继续「蔓延」）
//
//   mov eax,Scenario / test byte ptr [eax],80h / jz → ScenarioClass::flags 的 bit 7
//   mov ecx,[esi+44h] / call __IsOverlayIdxTiberium / cmp edi,-1
//   mov cl,[esi+11Eh]（frame）/ cdq;sub;sar 1（= idx/2，向零取整）
//   cmp ecx,eax / jg → 要求 frame > idx/2
//   mov al,[esi+11Ch] / test al,al / jz → VeinStage 必须为 0
//   fld [tib+0A0h]（SpreadPercentage）/ fcomp 1e-5 / test ah,1 / jz
//   mov eax,[esi+0E4h]（FirstObject）/ test eax,eax / setz
//                               → 该格地面对象链必须为空
// ============================================================================
bool SeedCell_CanSpread(CellClass* pCell)
{
    if (ScenarioClass::Instance == nullptr) {
        return false;
    }
    // asm: test byte ptr [Scenario], 80h —— ScenarioClass 首个 dword（flags）的 bit 7。
    //      原版的 ScenarioClass 没有虚表，+0x00 就是 flags；本工程把它放在成员
    //      flags 上（ScenarioClass.h 已标注 asm +0x00）。
    if ((ScenarioClass::Instance->flags & 0x80) == 0) {
        return false;
    }

    const int32 tibIdx = pCell->Get_Contained_Tiberium_Index();
    if (tibIdx == -1) {
        return false;
    }

    // asm: cdq / sub eax,edx / sar eax,1 → idx/2（向零取整；此处 idx >= 0）
    if (static_cast<int32>(pCell->OverlayFrame) <= tibIdx / 2) {
        return false;
    }

    if (pCell->VeinStage != 0) {
        return false;
    }

    TiberiumClass* pTib = SeedCell_Tiberium(tibIdx);
    if (pTib == nullptr) {
        return false;
    }

    if (!(pTib->SpreadPercentage >= 1.0e-5)) {
        return false;
    }

    // asm: mov eax,[esi+0E4h] / test eax,eax / setz → 只取地面无对象的格子
    if (pCell->FirstObject != nullptr) {
        return false;
    }

    return true;
}

// ============================================================================
// 两个收尾例程的共用骨架
// ============================================================================
void Scan_Seed_Cells(TiberiumClass* pThis, int32 offMain, int32 offAux1,
                     int32 offAux2, int32 offCount,
                     bool (*pPredicate)(CellClass*))
{
    uint8* const self = reinterpret_cast<uint8*>(pThis);

    SeedCounterView* const pCounter =
        static_cast<SeedCounterView*>(*reinterpret_cast<void**>(self + offMain));
    if (pCounter == nullptr) {
        return;     // 原版直接踩空；本工程按惯例跳过
    }

    // ---- 1) 清空计数表数据区（写 count 个 dword）并把 Count 归零 ----------
    // asm: cmp [eax],0 / jl skip ; loc: mov edx,[eax+8] / inc ecx /
    //      mov [edx+ecx*4-4],0 / cmp ecx,[eax] / jle loc
    if (pCounter->Count >= 0) {
        for (int32 k = 1; k <= pCounter->Count; ++k) {
            pCounter->Data[k - 1] = nullptr;
        }
    }
    pCounter->Count = 0;

    if (MapClass::Instance == nullptr) {
        return;
    }

    // ---- 2) aux2（逐格标记表）清零：area 个字节 --------------------------
    // asm: call MapClass_GetArea / dec eax / js ; loc: dec eax / mov byte[ecx+eax+1],0
    const int32 area = MapClass::Instance->GetArea();
    uint8* const pMarked =
        static_cast<uint8*>(*reinterpret_cast<void**>(self + offAux2));
    if (pMarked != nullptr && area > 0) {
        std::memset(pMarked, 0, static_cast<size_t>(area));
    }

    uint8* const pEntries =
        static_cast<uint8*>(*reinterpret_cast<void**>(self + offAux1));
    int32* const pSlot = reinterpret_cast<int32*>(self + offCount);

    // ---- 3) 逐格扫描 ------------------------------------------------------
    MapClass::Instance->CellIterator_Reset();

    for (CellClass* pCell = MapClass::Instance->CellIterator_NextCell();
         pCell != nullptr;
         pCell = MapClass::Instance->CellIterator_NextCell())
    {
        // asm: call CellClass_GetContainedTiberiumIndex / cmp eax,[esi+98h] / jnz
        if (pCell->Get_Contained_Tiberium_Index() != pThis->TypeIndex) {
            continue;
        }

        if (!pPredicate(pCell)) {
            continue;
        }

        if (pEntries == nullptr) {
            continue;   // 分配失败：原版会踩空
        }

        // asm: mov edx,[esi+count] / lea ebx,[aux1+edx*8] /
        //      mov edi,[cell+24h] / mov [ebx],edi / mov dword [ebx+4],0
        const int32 slot = *pSlot;
        uint8* const pEntry = pEntries + slot * 8;
        *reinterpret_cast<uint32*>(pEntry) =
            *reinterpret_cast<const uint32*>(&pCell->MapCoords);
        *reinterpret_cast<float*>(pEntry + 4) = 0.0f;

        // asm: inc eax / mov [esi+count],eax —— 记录槽位计数先自增，
        //      随后才做容量判定，故计数与堆的 Count 在表满后会分叉。
        *pSlot = slot + 1;

        SeedHeap_Push(pCounter, pEntry);

        // asm: call sub_42B1C0 / mov ecx,[esi+aux2] / mov byte ptr [eax+ecx],1
        const int32 mark = VeinholeMonsterClass::Growth_Index(pCell->MapCoords);
        if (mark >= 0 && mark < area) {     // 原版不判界；负号下标会越界写
            pMarked[mark] = 1;
        }
    }
}

} // namespace

void TiberiumClass::SmthA_Finalizer(TiberiumClass* pThis)
{
    Scan_Seed_Cells(pThis, 0x110, 0x118, 0x114, 0x10C, &SeedCell_CanGrow);
}

void TiberiumClass::SmthB_Finalizer(TiberiumClass* pThis)
{
    Scan_Seed_Cells(pThis, 0x0F4, 0x0FC, 0x0F8, 0x0F0, &SeedCell_CanSpread);
}

// ============================================================================
// asm: TiberiumClass 的 overlay 图像窗口（ImageStart / ImageCount）
//
//   矿石每一帧对应 [ImageStart, ImageStart + ImageCount) 里的一个 overlay
//   索引；越界的帧号钳到首/尾。Get_Image 供 CellClass 绘制矿石时取图。
// ============================================================================
int32 TiberiumClass::Get_Image(int32 frame) const
{
    if (ImageStart < 0 || ImageCount <= 0) {
        return ImageStart;
    }
    int32 idx = frame;
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= ImageCount) {
        idx = ImageCount - 1;
    }
    return ImageStart + idx;
}

// ============================================================================
// 蔓延掷骰 —— CellClass::Grow_Tiberium 在空地（密度 0）上无条件允许蔓延；
//   已有矿的格子按 SpreadPercentage 概率蔓延。随机源取 Scenario.Random。
// ============================================================================
bool TiberiumClass::Should_Spread(int32 cellDensity) const
{
    if (Spread <= 0 || SpreadPercentage <= 0.0) {
        return false;
    }
    // asm: 空地（无矿）优先铺开，绕过概率判定。
    if (cellDensity <= 0) {
        return true;
    }
    if (ScenarioClass::Instance == nullptr) {
        return false;
    }
    const int32 roll = ScenarioClass::Instance->Random.Next(0, 1000000);
    return (static_cast<double>(roll) / 1000000.0) < SpreadPercentage;
}

// ============================================================================
// 生长掷骰 —— 已有矿的格子按 GrowthPercentage 概率提升矿量。
// ============================================================================
bool TiberiumClass::Should_Grow(int32 cellDensity) const
{
    if (Growth <= 0 || GrowthPercentage <= 0.0) {
        return false;
    }
    // 空格子没有可生长的矿量。
    if (cellDensity <= 0) {
        return false;
    }
    if (ScenarioClass::Instance == nullptr) {
        return false;
    }
    const int32 roll = ScenarioClass::Instance->Random.Next(0, 1000000);
    return (static_cast<double>(roll) / 1000000.0) < GrowthPercentage;
}
