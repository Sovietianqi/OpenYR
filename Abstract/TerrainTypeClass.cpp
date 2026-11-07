#include <Abstract/TerrainTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <FileFormats/SHP.h>
#include <Game/Externs.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdio>

// ============================================================================
// TerrainTypeClass.cpp
//
//  TerrainTypeClass describes a kind of terrain decoration (tree, rock,
//  marble block, etc.) loaded from rules/art INI files.  It inherits
//  ObjectTypeClass and adds:
//
//    * Classification flags (tree / rocks / marble / tiberium / vein / fog)
//    * Flammability and crushability
//    * Tiberium-spawning parameters (type, radius, chance)
//    * Damage and armor properties
//    * Frame count and fire animation reference
//    * Art name (SHP file reference)
//
//  This file implements:
//    * Static Array plumbing (Init_Array / Delete_Array / Delete_All /
//      Find / FindByIndex / GetCount)
//    * Constructor / destructor
//    * LoadFromINI
//    * ComputeCRC / GetCRC
//    * Is_Tree / Is_Rocks / Spawns_Tiberium
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<TerrainTypeClass*>* TerrainTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void TerrainTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TerrainTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TerrainTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TerrainTypeClass*>();
    }
}

void TerrainTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TerrainTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void TerrainTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        TerrainTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

TerrainTypeClass* TerrainTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        TerrainTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

TerrainTypeClass* TerrainTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 TerrainTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// asm: TerrainTypeClass_FindOrAllocate 的 ID 查询面（大小写不敏感）。
TerrainTypeClass* TerrainTypeClass::FindByName(const char* pID)
{
    return Find(pID);
}

// asm: TerrainTypeClass_CreateFromINIList (asm:218)
//   遍历全部地形类型；对剧场类型（[+22Ch] != 0）按
//   "<ID>.<theater ext>" 解析 SHP 到 ImageSHP（+0xA4）。非剧场类型跳过。
void TerrainTypeClass::CreateFromINIList(int32 idxTheater)
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        TerrainTypeClass* pType = Array->Items[i];
        if (pType == nullptr)
            continue;

        // asm: cmp [esi+22Ch],bl / jz loc_71DD63
        if (!pType->IsTheater)
            continue;

        // asm: __makepath(ID, ".", theaterData.ext) —— 由 ID 与剧场扩展名拼名。
        const char* pBase = (pType->Image[0] != '\0') ? pType->Image : pType->ID;
        char base[0x100];
        char full[0x100];
        snprintf(base, sizeof(base), "%s.SHP", pBase);
        pType->TheaterSpecificID(base, idxTheater, full);

        // asm: CCFileClass + Read_File 解出 SHP；文件后端未接通，保持 null。
        pType->ImageSHP = nullptr;
    }
}

// ============================================================================
// Constructor
//
// asm: TerrainTypeClass_CTOR (asm:30, retn 4)
//   [294h]=-1 ; [298h]=0 ; [29Ch..29Eh]=0 ; [2A8h]=7 ; [2ACh]=7 ; [2A0h]=0
//   [2A4h]=0 ; [2B0h..2B4h]=0 ; [2B8h]=0
//   Create_ID ; 字标志位：[230h]=0 [234h]=1 [22Fh]=1 [231h]=0 [232h]=1
//   注册进 vec_TerrainTypes -> [294h]=ordinal
//   [0A0h]=-1 ; [9Ch]=6 (格子地形=Tiberium? Land 值)
// ============================================================================

TerrainTypeClass::TerrainTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    IsMarble            = false;   // asm: [29Ch] = 0
    IsRocks             = false;   // asm: [29Dh] = 0
    IsTree              = false;   // asm: [29Eh] = 0
    IsTiberium          = false;
    SpawnsTiberium      = false;   // asm: [2B1h] = 0
    IsFlammable         = false;   // asm: [2B2h] = 0
    IsCrushable         = false;
    IsVein              = false;
    IsFog               = false;
    IsAnimated          = false;   // asm: [2B3h] = 0
    SpawnsTiberiumType  = 0;
    SpawnsTiberiumRadius = 0;
    SpawnsTiberiumChance = 0;
    Damage              = 0;       // asm: [2A0h] = 0
    ArmorIndex          = 0;       // asm: [2A4h] = 0
    FrameCount          = 0;       // asm: [2B8h] = 0
    TreeStrength        = -1;      // asm: [0A0h] = -1
    TemperateOccupationBits = 7;   // asm: [2A8h] = 7
    SnowOccupationBits      = 7;   // asm: [2ACh] = 7
    IsVeinhole          = false;   // asm: [2B4h] = 0
    WaterBound          = false;   // asm: [2B0h] = 0

    // 注：asm 另有 [9Ch]=6 的字格地形写入，OpenYR 的 ObjectTypeClass 未抽出
    // 该字段（语义为"地形对象落格时的默认地形"），此处不臆造字段。

    std::memset(ArtName, 0, sizeof(ArtName));
}

// ============================================================================
// Destructor
// ============================================================================

TerrainTypeClass::~TerrainTypeClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TerrainTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::TerrainType);
    return S_OK;
}

// asm: TerrainTypeClass_Load (asm:577, retn 8)
//   ObjectTypeClass_Load ; 失败返回
//   重打虚表 + ObjectTypeClass_Load3DArt + Load_Theater_Art
//   [2B8h] = &theatreData[Land] 序表项（无自有字段序列化）
HRESULT TerrainTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0)
        return hr;                            // asm: jl loc_71E235

    ObjectTypeClass::Load3DArt(0);            // asm: ObjectTypeClass_Load3DArt
    ObjectTypeClass::Load_Theater_Art(0);     // asm: ObjectTypeClass_Load_Theater_Art

    return S_OK;                              // asm: xor eax,eax
}

// asm: TerrainTypeClass_Save (asm:637, retn 0Ch) —— 纯转发 ObjectTypeClass_Save。
HRESULT TerrainTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    return ObjectTypeClass::Save(pStm, fClearDirty);
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TerrainTypeClass::WhatAmI() const
{
    return AbstractType::TerrainType;
}

// asm: 类型派生标识（供 TerrainClass::ComputeCRC 等按类型取 ID 使用）。
int32 TerrainTypeClass::GetAbstractDerivationID() const
{
    return static_cast<int32>(AbstractType::TerrainType);   // asm: 25h
}

// asm: TerrainTypeClass_GetArrayIndex (asm:847) -- "mov eax,[ecx+294h] ; retn"
//   本实现在无缓存序号字段时按注册表就地求值（等价语义）。
int32 TerrainTypeClass::GetArrayIndex() const
{
    if (Array == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == this)
            return i;
    }
    return -1;
}

// asm: 类型层 INI 读取别名（转发 LoadFromINI）。
bool TerrainTypeClass::Read_INI(CCINIClass* pINI)
{
    return LoadFromINI(pINI);
}

// asm: 类型层 SHP 访问面 —— 返回缓存的 SHP 句柄（ObjectTypeClass::GetImage）。
SHPStruct* TerrainTypeClass::GetImageData() const
{
    return GetImage();
}

int32 TerrainTypeClass::Size() const
{
    return sizeof(TerrainTypeClass);
}

// ============================================================================
// Classification helpers
// ============================================================================

bool TerrainTypeClass::Is_Tree() const
{
    return IsTree;
}

bool TerrainTypeClass::Is_Rocks() const
{
    return IsRocks;
}

bool TerrainTypeClass::Spawns_Tiberium() const
{
    return SpawnsTiberium;
}

// ============================================================================
// LoadFromINI
// ============================================================================

// asm: TerrainTypeClass_LoadFromINI (asm:313, retn 4)
//   Reset(INI) ; ObjectTypeClass_LoadFromINI ; test al,al / jz -> false
//   if (TreeStrength[0A0h] == -1) TreeStrength = Rules->[1144h]
//   键序（逐字）：IsVeinhole, WaterBound, SpawnsTiberium, IsFlammable,
//   Foundation(art INI), 然后 RadColor(来自 SHP 帧色) / RadarColor /
//   IsAnimated / AnimationRate / AnimationProbability /
//   TemperateOccupationBits / SnowOccupationBits
bool TerrainTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // asm: ObjectTypeClass_LoadFromINI ; test al,al / jz loc_71E0B4 -> false
    if (!ObjectTypeClass::LoadFromINI(pINI))
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // asm: cmp dword ptr [esi+0A0h],0FFFFFFFFh / jnz loc_71DEE2
    //     —— 类型未显式给出时回落到 Rules+1144h（[General] TreeStrength）。
    //     原版 RulesClass 构造把该槽预置 0x19，故缺省（未读到键）时仍取 0x19。
    if (TreeStrength == -1)
    {
        const int32 rulesTreeStrength = (TheRules != nullptr) ? TheRules->TreeStrength : 0;
        TreeStrength = (rulesTreeStrength > 0) ? rulesTreeStrength : 0x19;
    }

    // asm: 键序 IsVeinhole / WaterBound / SpawnsTiberium / IsFlammable。
    IsVeinhole     = pINI->ReadBool(section, "IsVeinhole", IsVeinhole);
    if (IsVeinhole)
    {
        HasRadialIndicator = false;   // asm: mov [HasRadialIndicator],0
        IsFlammable        = true;    // asm: byte ptr [esi+231h],1
    }
    WaterBound     = pINI->ReadBool(section, "WaterBound", WaterBound);
    SpawnsTiberium = pINI->ReadBool(section, "SpawnsTiberium", SpawnsTiberium);
    IsFlammable    = pINI->ReadBool(section, "IsFlammable", IsFlammable);

    // asm: Foundation 来自 art INI（section = Image 名）。
    const char* artSection = (Image[0] != '\0') ? Image : section;
    FoundationValue = CCINIClass::INI_Art.GetFoundation(artSection, "Foundation", FoundationValue);

    // asm: 非剧场 -> "<Image>.SHP" -> Finder -> ImageSHP（文件后端未接通）。
    if (!IsTheater)
    {
        char base[0x100];
        char full[0x100];
        snprintf(base, sizeof(base), "%s.SHP", artSection);
        TheaterSpecificID(base, 0, full);
        ImageSHP = nullptr;
    }

    // asm: 若 SHP 有效，用帧色回填 RadarColor。
    if (ImageSHP != nullptr)
    {
        ColorStruct frameColor = ImageSHP->GetColor(0);
        RadarColor[0] = frameColor.R;
        RadarColor[1] = frameColor.G;
        RadarColor[2] = frameColor.B;
    }

    // asm: 显式 RadarColor 覆盖。
    pINI->Get3Bytes(section, "RadarColor", RadarColor);

    // asm: IsAnimated / AnimationRate / AnimationProbability /
    //      TemperateOccupationBits / SnowOccupationBits。
    IsAnimated     = pINI->ReadBool(section, "IsAnimated", IsAnimated);
    AnimationRate  = pINI->ReadInteger(section, "AnimationRate", AnimationRate);
    AnimationProbability = pINI->ReadFixed(section, "AnimationProbability", AnimationProbability);
    TemperateOccupationBits = pINI->ReadInteger(section, "TemperateOccupationBits", TemperateOccupationBits);
    SnowOccupationBits = pINI->ReadInteger(section, "SnowOccupationBits", SnowOccupationBits);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

// asm: TerrainTypeClass_GetCRC (asm:506)
//   AbstractTypeClass_GetCRC(buf)
//   CRC_DWORD(HeapID[294h]) ; CRC_bool(WaterBound[2B0h]) ; CRC_bool(SpawnsTiberium[2B1h])
//   CRC_bool(IsFlammable[2B2h]) ; CRC_DWORD(Foundation[298h])
//   CRC_bool(IsAnimated[2B3h]) ; CRC_DWORD(AnimationRate[2A0h])
//   CRC_float(AnimationProbability[2A4h])
void TerrainTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    const int32 heapId = GetArrayIndex();            // asm: [294h]
    crc.AddData(&heapId,             sizeof(heapId));
    crc.AddData(&WaterBound,         sizeof(WaterBound));
    crc.AddData(&SpawnsTiberium,     sizeof(SpawnsTiberium));
    crc.AddData(&IsFlammable,        sizeof(IsFlammable));
    crc.AddData(&FoundationValue,    sizeof(FoundationValue));
    crc.AddData(&IsAnimated,         sizeof(IsAnimated));
    crc.AddData(&AnimationRate,      sizeof(AnimationRate));
    crc.AddData(&AnimationProbability, sizeof(AnimationProbability));
}

int32 TerrainTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// TerrainTypeClass - static lookup helpers
// ============================================================================

TerrainTypeClass* TerrainTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TerrainTypeClass* found = Find(pID);
    if (found) return found;
    if (!Array) Init_Array();
    TerrainTypeClass* newItem = GameCreate<TerrainTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}
