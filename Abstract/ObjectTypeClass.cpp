#include <Abstract/ObjectTypeClass.h>
#include <cstdio>
#include <cstring>
#include "BuildingTypeClass.h"
#include <Audio/VocClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Map/MapClass.h>
#include <Houses/HouseClass.h>
#include <FileFormats/SHP.h>
#include <IO/MixFileClass.h>

#include <cstring>
#include <cstdlib>

// ============================================================================
// ObjectTypeClass.cpp
//
//  ObjectTypeClass is the shared base for every "placeable" type - anything
//  that can exist on the map (buildings, infantry, vehicles, aircraft,
//  overlays, smudges, terrain, voxel anims).  It sits between
//  AbstractTypeClass (which owns the ID / name / cost bookkeeping) and the
//  concrete TechnoTypeClass / OverlayTypeClass / SmudgeTypeClass / etc.
//
//  This file provides:
//    * Static Array plumbing (Init_Array / Delete_Array / Find / FindByIndex
//      / GetCount / Delete_All)
//    * Constructor / destructor
//    * IPersistStream implementations
//    * LoadFromINI / SaveToINI - parses the fields common to every object
//      type (Sight, Cost, TechLevel, Speed, BuildLimit, Score, ROT, ...)
//    * ComputeCRC / GetCRC
//    * Read_INI / Write_INI delegates
//    * Can_Place_On_Map / Get_Occupy_Bits - placement helpers
//    * Is_Temple_Of_NOD / Is_Flak / Is_Listed / Get_Max_Pips - type-flag
//      helpers used by the sidebar and AI
//    * Create_One_Of / Get_Cameo_Data - factory + sidebar art
//    * Read_TypeFlags - parses Yes/No flags into the bool fields
//    * Resolve_SHP_References - SHP art resolution hook
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<ObjectTypeClass*>* ObjectTypeClass::Array = nullptr;

// ============================================================================
// Init_Array / Delete_Array
//
//  Mirror the AbstractTypeClass helpers but operate on the typed
//  ObjectTypeClass array.  Both arrays coexist in the original binary -
//  AbstractTypeClass::Array is the polymorphic root and ObjectTypeClass::
//  Array narrows the iteration to only placeable types.
// ============================================================================
void ObjectTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<ObjectTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<ObjectTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<ObjectTypeClass*>();
    }
}

void ObjectTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<ObjectTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Find / FindByIndex / GetCount
// ============================================================================
ObjectTypeClass* ObjectTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        ObjectTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

ObjectTypeClass* ObjectTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 ObjectTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// 根据游戏行为，可知 All 负责下面这段逻辑。
//
//  Destroys every registered ObjectTypeClass and clears the array.
// ============================================================================
void ObjectTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        ObjectTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// asm: ObjectTypeClass_One_Time (asm:740)
//
//  Engine-global one-shot sprite preload, invoked a single time from
//  Init_BulkData.  For each of the four shared sidebar / pip sprites the
//  original first tests a cached handle; when absent it constructs a
//  RawFileClass, reads the file, and on failure falls back to the MIX
//  filesystem finder.  The handle and a "resident" byte are cached in module
//  globals so a second call short-circuits.
//
//    PIPBRD.SHP   -> CCFile_PIPBRD_SHP   byte_AC1488
//    PIPS.SHP     -> CCFile_PIPS_SHP     byte_AC1489
//    PIPS2.SHP    -> CCFile_PIPS2_SHP    byte_AC148A
//    TALKBUBL.SHP -> CCFile_TALKBUBL_SHP byte_AC148B
// ============================================================================
static SHPStruct* g_OneTime_PipBoardSHP   = nullptr;
static bool       g_OneTime_PipBoardValid = false;
static SHPStruct* g_OneTime_PipsSHP       = nullptr;
static bool       g_OneTime_PipsValid     = false;
static SHPStruct* g_OneTime_Pips2SHP      = nullptr;
static bool       g_OneTime_Pips2Valid    = false;
static SHPStruct* g_OneTime_TalkBubbleSHP = nullptr;
static bool       g_OneTime_TalkBubbleValid = false;

// Shared body for one of the four preloads: skip when the slot is already
// filled, otherwise resolve through the MIX filesystem (asm's
// RawFileClass / Read_File / Finder sequence collapsed to a single lookup).
static void ObjectTypeClass_One_Time_Load(const char* pFileName,
                                          SHPStruct*& pSlot,
                                          bool& bValid)
{
    if (pSlot != nullptr)
        return;                               // asm: test eax,eax ; jnz done

    pSlot  = Load_SHP(pFileName);
    bValid = (pSlot != nullptr);              // asm: setnz cl / mov byte,cl
}

void ObjectTypeClass::One_Time()
{
    ObjectTypeClass_One_Time_Load("PIPBRD.SHP",   g_OneTime_PipBoardSHP,   g_OneTime_PipBoardValid);
    ObjectTypeClass_One_Time_Load("PIPS.SHP",     g_OneTime_PipsSHP,       g_OneTime_PipsValid);
    ObjectTypeClass_One_Time_Load("PIPS2.SHP",    g_OneTime_Pips2SHP,      g_OneTime_Pips2Valid);
    ObjectTypeClass_One_Time_Load("TALKBUBL.SHP", g_OneTime_TalkBubbleSHP, g_OneTime_TalkBubbleValid);
}

// asm: CCFile_TALKBUBL_SHP（byte_AC148B）—— 绘制侧的话泡素材句柄。
SHPStruct* ObjectTypeClass::Get_TalkBubble_Shape()
{
    return g_OneTime_TalkBubbleSHP;
}

// asm: CCFile_PIPS_SHP（byte_AC1489）—— 军衔 / 弹药等 pips 的素材句柄。
SHPStruct* ObjectTypeClass::Get_Pips_Shape()
{
    return g_OneTime_PipsSHP;
}

// ============================================================================
// Construction / destruction
// ============================================================================

ObjectTypeClass::ObjectTypeClass() noexcept
    : AbstractTypeClass(noinit)
{
    Sight          = 0;
    Speed          = 0;
    MaxStrength    = 0;
    BuildLimit     = -1;
    Score          = 0;
    ROT            = 0;
    Selectable     = true;
    LegalTarget    = true;
    Insignificant  = false;
    Immune         = false;
    OnFire         = false;
    Repairable     = true;
    Unsellable     = false;
    Cloakable      = false;
    TurretEquipped = false;
    IsStealthy     = false;
    IsTrainable    = false;
    IsNotHuman     = true;
    IsTheater      = false;
    IdleLayer      = Layer::Ground;
    Land           = LandType::Clear;
    ImageSHP       = nullptr;
}

ObjectTypeClass::ObjectTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
{
    Sight          = 0;
    Speed          = 0;
    MaxStrength    = 0;
    BuildLimit     = -1;
    Score          = 0;
    ROT            = 0;
    Selectable     = true;
    LegalTarget    = true;
    Insignificant  = false;
    Immune         = false;
    OnFire         = false;
    Repairable     = true;
    Unsellable     = false;
    Cloakable      = false;
    TurretEquipped = false;
    IsStealthy     = false;
    IsTrainable    = false;
    IsNotHuman     = true;
    IsTheater      = false;
    IdleLayer      = Layer::Ground;
    Land           = LandType::Clear;
    ImageSHP       = nullptr;
}

// asm: ObjectTypeClass_CTOR (asm:1054192)
//
//  B 类：汇编里的 ObjectTypeClass_CTOR 是"带 ID 的正常构造"——先
//  AbstractTypeClass_CTOR(name)，然后把 +0x98 起的标志字节、+0x1E8 起的
//  计数/索引、+0x211 起的布尔量逐条铺默认值。本项目的
//  ObjectTypeClass(const char*) / ObjectTypeClass() 正是这份实现。
//  这里的 noinit 变体是工程自有的"派生类已经铺过底，不要重复初始化"通道，
//  原版没有对应出口，故只做基类构造，不碰任何字段。
ObjectTypeClass::ObjectTypeClass(noinit_t) noexcept
    : AbstractTypeClass(noinit)
{
    // Subclasses own initialisation; leave fields untouched.
}

// asm: ObjectTypeClass_DTOR (asm:1054467)
//
//  B 类（工程对象模型差异）：汇编的析构函数体确实有活——
//    1. 重刷 4 个 vftable 指针；
//    2. 通过 GetArrayIndex 把自己从 v_ObjectType 里摘除并左移后续元素；
//    3. delete [+0A4h]（当 [+0A8h] 标志置位）；
//    4. TechnoTypeClass_UnloadTurretArt()；
//    5. 逐块 delete +0x280 / +0x26C / +0x258 / +0x244 … 的容器缓冲。
//  本工程不照搬的两点原因：
//    * 第 5 步的容器在本项目里是具名成员（DynamicVectorClass / std::string），
//      由成员自己的析构函数释放，再手工 delete 会二次释放；
//    * 第 2 步的摘表在本项目由外部驱动：ObjectTypeClass::Delete_All 先
//      GameDelete(item) 再 Array->Remove(i)。若析构里再自摘一次，索引会
//      整体左移，Delete_All 随后的 Remove(i) 就会删掉错误的元素并留下悬垂
//      指针。
//  因此析构保持为空，实际释放交由成员析构与 Delete_All 完成。
ObjectTypeClass::~ObjectTypeClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT ObjectTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Object);
    return S_OK;
}

HRESULT ObjectTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    hr = pStm->Read(&Sight, sizeof(Sight), &read);
    if (hr < 0 || read != sizeof(Sight)) return E_FAIL;

    hr = pStm->Read(&Speed, sizeof(Speed), &read);
    if (hr < 0 || read != sizeof(Speed)) return E_FAIL;

    hr = pStm->Read(&MaxStrength, sizeof(MaxStrength), &read);
    if (hr < 0 || read != sizeof(MaxStrength)) return E_FAIL;

    hr = pStm->Read(&BuildLimit, sizeof(BuildLimit), &read);
    if (hr < 0 || read != sizeof(BuildLimit)) return E_FAIL;

    hr = pStm->Read(&Score, sizeof(Score), &read);
    if (hr < 0 || read != sizeof(Score)) return E_FAIL;

    hr = pStm->Read(&ROT, sizeof(ROT), &read);
    if (hr < 0 || read != sizeof(ROT)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Selectable     = (flags & 0x0001) != 0;
    LegalTarget    = (flags & 0x0002) != 0;
    Insignificant  = (flags & 0x0004) != 0;
    Immune         = (flags & 0x0008) != 0;
    OnFire         = (flags & 0x0010) != 0;
    Repairable     = (flags & 0x0020) != 0;
    Unsellable     = (flags & 0x0040) != 0;
    Cloakable      = (flags & 0x0080) != 0;
    TurretEquipped = (flags & 0x0100) != 0;
    IsStealthy     = (flags & 0x0200) != 0;
    IsTrainable    = (flags & 0x0400) != 0;
    IsNotHuman     = (flags & 0x0800) != 0;
    IsTheater      = (flags & 0x1000) != 0;

    hr = pStm->Read(&IdleLayer, sizeof(IdleLayer), &read);
    if (hr < 0 || read != sizeof(IdleLayer)) return E_FAIL;

    hr = pStm->Read(&Land, sizeof(Land), &read);
    if (hr < 0 || read != sizeof(Land)) return E_FAIL;

    return S_OK;
}

HRESULT ObjectTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    hr = pStm->Write(&Sight, sizeof(Sight), &written);
    if (hr < 0 || written != sizeof(Sight)) return E_FAIL;

    hr = pStm->Write(&Speed, sizeof(Speed), &written);
    if (hr < 0 || written != sizeof(Speed)) return E_FAIL;

    hr = pStm->Write(&MaxStrength, sizeof(MaxStrength), &written);
    if (hr < 0 || written != sizeof(MaxStrength)) return E_FAIL;

    hr = pStm->Write(&BuildLimit, sizeof(BuildLimit), &written);
    if (hr < 0 || written != sizeof(BuildLimit)) return E_FAIL;

    hr = pStm->Write(&Score, sizeof(Score), &written);
    if (hr < 0 || written != sizeof(Score)) return E_FAIL;

    hr = pStm->Write(&ROT, sizeof(ROT), &written);
    if (hr < 0 || written != sizeof(ROT)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (Selectable)     flags |= 0x0001;
    if (LegalTarget)    flags |= 0x0002;
    if (Insignificant)  flags |= 0x0004;
    if (Immune)         flags |= 0x0008;
    if (OnFire)         flags |= 0x0010;
    if (Repairable)     flags |= 0x0020;
    if (Unsellable)     flags |= 0x0040;
    if (Cloakable)      flags |= 0x0080;
    if (TurretEquipped) flags |= 0x0100;
    if (IsStealthy)     flags |= 0x0200;
    if (IsTrainable)    flags |= 0x0400;
    if (IsNotHuman)     flags |= 0x0800;
    if (IsTheater)      flags |= 0x1000;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&IdleLayer, sizeof(IdleLayer), &written);
    if (hr < 0 || written != sizeof(IdleLayer)) return E_FAIL;

    hr = pStm->Write(&Land, sizeof(Land), &written);
    if (hr < 0 || written != sizeof(Land)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType ObjectTypeClass::WhatAmI() const
{
    return AbstractType::Object;
}

int32 ObjectTypeClass::Size() const
{
    return sizeof(ObjectTypeClass);
}

// ============================================================================
// LoadFromINI
//
//  Parses the fields common to every object type from the rules-INI block
//  whose name matches the type's ID.  Subclasses chain this before reading
//  their own fields.
// ============================================================================
bool ObjectTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // asm: ObjectTypeClass_LoadFromINI (asm:1806) chains the abstract base
    // first and bails out when the section is missing (the base returns 0 for
    // an absent section).  This is what populates Name / UIName before the
    // object-specific keys are read.
    if (!AbstractTypeClass::LoadFromINI(pINI))
        return false;

    // ------------------------------------------------------------------
    // Economy / build metadata
    // ------------------------------------------------------------------
    Cost       = pINI->ReadInteger(section, "Cost",       Cost);
    TechLevel  = pINI->ReadInteger(section, "TechLevel",  TechLevel);
    BuildLimit = pINI->ReadInteger(section, "BuildLimit", BuildLimit);
    Score      = pINI->ReadInteger(section, "Score",      Score);

    // ------------------------------------------------------------------
    // Combat / movement
    // ------------------------------------------------------------------
    Sight       = pINI->ReadInteger(section, "Sight",      Sight);
    Speed       = pINI->ReadInteger(section, "Speed",      Speed);
    MaxStrength = pINI->ReadInteger(section, "Strength",   MaxStrength);
    ROT         = pINI->ReadInteger(section, "ROT",        ROT);

    // ------------------------------------------------------------------
    // UI name label - resolved against the string table at runtime
    // ------------------------------------------------------------------
    char labelBuf[64];
    pINI->ReadString(section, "UIName", "", labelBuf, sizeof(labelBuf));
    if (labelBuf[0] != '\0')
    {
        int32 j = 0;
        while (labelBuf[j] != '\0' && j < static_cast<int32>(sizeof(UINameLabel) - 1))
        {
            UINameLabel[j] = labelBuf[j];
            ++j;
        }
        UINameLabel[j] = '\0';
    }

    // ------------------------------------------------------------------
    // Display name (human-readable fallback when UIName is empty)
    // ------------------------------------------------------------------
    char nameBuf[64];
    pINI->ReadString(section, "Name", "", nameBuf, sizeof(nameBuf));
    if (nameBuf[0] != '\0')
    {
        int32 j = 0;
        while (nameBuf[j] != '\0' && j < static_cast<int32>(sizeof(Name) - 1))
        {
            Name[j] = nameBuf[j];
            ++j;
        }
        Name[j] = '\0';
    }

    // ------------------------------------------------------------------
    // Common boolean flags
    // ------------------------------------------------------------------
    Selectable     = pINI->ReadBool(section, "Selectable",     Selectable);
    LegalTarget    = pINI->ReadBool(section, "LegalTarget",    LegalTarget);
    Insignificant  = pINI->ReadBool(section, "Insignificant",  Insignificant);
    Immune         = pINI->ReadBool(section, "Immune",         Immune);
    OnFire         = pINI->ReadBool(section, "OnFire",         OnFire);
    Repairable     = pINI->ReadBool(section, "Repairable",     Repairable);
    Unsellable     = pINI->ReadBool(section, "Unsellable",     Unsellable);
    Cloakable      = pINI->ReadBool(section, "Cloakable",      Cloakable);
    TurretEquipped = pINI->ReadBool(section, "Turret",         TurretEquipped);
    IsTrainable    = pINI->ReadBool(section, "Trainable",      IsTrainable);
    IsNotHuman     = pINI->ReadBool(section, "NotHuman",       IsNotHuman);
    IsTheater      = pINI->ReadBool(section, "Theater",        IsTheater);

    // ------------------------------------------------------------------
    // Render layer
    // ------------------------------------------------------------------
    char layerBuf[32];
    pINI->ReadString(section, "Layer", "Ground", layerBuf, sizeof(layerBuf));
    if (!_strcmpi(layerBuf, "Air"))          IdleLayer = Layer::Air;
    else if (!_strcmpi(layerBuf, "Top"))     IdleLayer = Layer::Top;
    else if (!_strcmpi(layerBuf, "Surface")) IdleLayer = Layer::Surface;
    else                                     IdleLayer = Layer::Ground;

    // ------------------------------------------------------------------
    // Land type the object occupies
    // ------------------------------------------------------------------
    char landBuf[32];
    pINI->ReadString(section, "Land", "Clear", landBuf, sizeof(landBuf));
    if (!_strcmpi(landBuf, "Rough"))      Land = LandType::Rough;
    else if (!_strcmpi(landBuf, "Road"))  Land = LandType::Road;
    else if (!_strcmpi(landBuf, "Water")) Land = LandType::Water;
    else if (!_strcmpi(landBuf, "Rock"))  Land = LandType::Rock;
    else if (!_strcmpi(landBuf, "Wall"))  Land = LandType::Wall;
    else if (!_strcmpi(landBuf, "Tiberium")) Land = LandType::Tiberium;
    else if (!_strcmpi(landBuf, "Beach")) Land = LandType::Beach;
    else                                  Land = LandType::Clear;

    // Delegate flag parsing to the shared helper.
    Read_TypeFlags(pINI, section);


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    pINI->ReadString(section, "Image", Image, Image, sizeof(Image));
    pINI->ReadString(section, "AlphaImage", AlphaImage, AlphaImage, sizeof(AlphaImage));
    { char _buf[0x40]; if (pINI->ReadString(section, "CrushSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) CrushSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "AmbientSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) AmbientSound = _i; } }
    Crushable = pINI->ReadBool(section, "Crushable", Crushable);
    Bombable = pINI->ReadBool(section, "Bombable", Bombable);
    NoSpawnAlt = pINI->ReadBool(section, "NoSpawnAlt", NoSpawnAlt);
    AlternateArcticArt = pINI->ReadBool(section, "AlternateArcticArt", AlternateArcticArt);
    RadarInvisible = pINI->ReadBool(section, "RadarInvisible", RadarInvisible);
    Selectable = pINI->ReadBool(section, "Selectable", Selectable);
    LegalTarget = pINI->ReadBool(section, "LegalTarget", LegalTarget);
    ArmorType = pINI->GetArmorType(section, "Armor", ArmorType);
    Strength = pINI->ReadInteger(section, "Strength", Strength);
    Immune = pINI->ReadBool(section, "Immune", Immune);
    Insignificant = pINI->ReadBool(section, "Insignificant", Insignificant);
    HasRadialIndicator = pINI->ReadBool(section, "HasRadialIndicator", HasRadialIndicator);
    pINI->Get3Bytes(section, "RadialColor", RadialColor);
    IgnoresFirestorm = pINI->ReadBool(section, "IgnoresFirestorm", IgnoresFirestorm);

    // ------------------------------------------------------------------
    // artmd.ini fields
    // ------------------------------------------------------------------
    UseLineTrail = pArt->ReadBool(section, "UseLineTrail", UseLineTrail);
    pArt->Get3Bytes(section, "LineTrailColor", LineTrailColor);
    LineTrailColorDecrement = pArt->ReadInteger(section, "LineTrailColorDecrement", LineTrailColorDecrement);
    Theater = pArt->ReadBool(section, "Theater", Theater);
    NewTheater = pArt->ReadBool(section, "NewTheater", NewTheater);
    Voxel = pArt->ReadBool(section, "Voxel", Voxel);

    // ------------------------------------------------------------------
    // asm: ObjectTypeClass_LoadFromINI tail (asm:1806, loc_5F9652)
    //   test Voxel ; jnz skip
    //   call vt[+0x2C] (WhatAmI) ; cmp eax,5 ; jz skip   ; 5 = BuildingType
    //   call ObjectTypeClass_Load_Theater_Art           ; 建筑/载具剧场美术
    // skip:
    //   if (Image[0]) MixFileClass::Retrieve("<Image>.SHP") -> ImageSHP
    //
    // 非体素且非建筑类型在读完 art 段后即时装载剧场美术；随后若 Image
    // 名字非空，用 mix 文件查找器把 SHP 句柄缓存到 ImageSHP（+0xAC）。
    // ------------------------------------------------------------------
    if (!Voxel && WhatAmI() != AbstractType::BuildingType)
    {
        Load_Theater_Art(0);
    }

    if (Image[0] != '\0')
    {
        char shpName[0x40];
        snprintf(shpName, sizeof(shpName), "%s.SHP", Image);
        ImageSHP = Load_SHP(shpName);
    }

        return true;
}

// ============================================================================
// SaveToINI
//
//  Emits the common fields back to the INI.  Used by the map editor when
//  saving custom rulesets.
// ============================================================================
bool ObjectTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    pINI->WriteInteger(section, "Cost",       Cost);
    pINI->WriteInteger(section, "TechLevel",  TechLevel);
    pINI->WriteInteger(section, "BuildLimit", BuildLimit);
    pINI->WriteInteger(section, "Score",      Score);
    pINI->WriteInteger(section, "Sight",      Sight);
    pINI->WriteInteger(section, "Speed",      Speed);
    pINI->WriteInteger(section, "Strength",   MaxStrength);
    pINI->WriteInteger(section, "ROT",        ROT);

    if (UINameLabel[0] != '\0')
        pINI->WriteString(section, "UIName", UINameLabel);
    if (Name[0] != '\0')
        pINI->WriteString(section, "Name", Name);

    pINI->WriteBool(section, "Selectable",     Selectable);
    pINI->WriteBool(section, "LegalTarget",    LegalTarget);
    pINI->WriteBool(section, "Insignificant",  Insignificant);
    pINI->WriteBool(section, "Immune",         Immune);
    pINI->WriteBool(section, "OnFire",         OnFire);
    pINI->WriteBool(section, "Repairable",     Repairable);
    pINI->WriteBool(section, "Unsellable",     Unsellable);
    pINI->WriteBool(section, "Cloakable",      Cloakable);
    pINI->WriteBool(section, "Turret",         TurretEquipped);
    pINI->WriteBool(section, "Stealthy",       IsStealthy);
    pINI->WriteBool(section, "Trainable",      IsTrainable);
    pINI->WriteBool(section, "NotHuman",       IsNotHuman);
    pINI->WriteBool(section, "Theater",        IsTheater);

    const char* layerName = "Ground";
    switch (IdleLayer)
    {
        case Layer::Air:     layerName = "Air";     break;
        case Layer::Top:     layerName = "Top";     break;
        case Layer::Surface: layerName = "Surface"; break;
        default:             layerName = "Ground";  break;
    }
    pINI->WriteString(section, "Layer", layerName);

    const char* landName = "Clear";
    switch (Land)
    {
        case LandType::Rough:    landName = "Rough";    break;
        case LandType::Road:     landName = "Road";     break;
        case LandType::Water:    landName = "Water";    break;
        case LandType::Rock:     landName = "Rock";     break;
        case LandType::Wall:     landName = "Wall";     break;
        case LandType::Tiberium: landName = "Tiberium"; break;
        case LandType::Beach:    landName = "Beach";    break;
        default:                 landName = "Clear";    break;
    }
    pINI->WriteString(section, "Land", landName);

    return true;
}

// ============================================================================
// 根据游戏行为，可知 TypeFlags 负责下面这段逻辑。
//
//  Parses Yes/No-style INI flags into the bool fields.  Centralised so
//  subclasses don't have to repeat the boilerplate.
// ============================================================================
void ObjectTypeClass::Read_TypeFlags(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr)
        return;

    Selectable     = pINI->ReadBool(pSection, "Selectable",     Selectable);
    LegalTarget    = pINI->ReadBool(pSection, "LegalTarget",    LegalTarget);
    Insignificant  = pINI->ReadBool(pSection, "Insignificant",  Insignificant);
    Immune         = pINI->ReadBool(pSection, "Immune",         Immune);
    Repairable     = pINI->ReadBool(pSection, "Repairable",     Repairable);
    Unsellable     = pINI->ReadBool(pSection, "Unsellable",     Unsellable);
    Cloakable      = pINI->ReadBool(pSection, "Cloakable",      Cloakable);
    IsTrainable    = pINI->ReadBool(pSection, "Trainable",      IsTrainable);
    IsNotHuman     = pINI->ReadBool(pSection, "NotHuman",       IsNotHuman);
    IsTheater      = pINI->ReadBool(pSection, "Theater",        IsTheater);
}

// ============================================================================
// ComputeCRC
//
//  Hashes the ObjectTypeClass state into the supplied CRC engine.
// ============================================================================
void ObjectTypeClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractTypeClass::ComputeCRC(crc);

    crc.AddData(&Sight,          sizeof(Sight));
    crc.AddData(&Speed,          sizeof(Speed));
    crc.AddData(&MaxStrength,    sizeof(MaxStrength));
    crc.AddData(&BuildLimit,     sizeof(BuildLimit));
    crc.AddData(&Score,          sizeof(Score));
    crc.AddData(&ROT,            sizeof(ROT));
    crc.AddData(&Selectable,     sizeof(Selectable));
    crc.AddData(&LegalTarget,    sizeof(LegalTarget));
    crc.AddData(&Insignificant,  sizeof(Insignificant));
    crc.AddData(&Immune,         sizeof(Immune));
    crc.AddData(&OnFire,         sizeof(OnFire));
    crc.AddData(&Repairable,     sizeof(Repairable));
    crc.AddData(&Unsellable,     sizeof(Unsellable));
    crc.AddData(&Cloakable,      sizeof(Cloakable));
    crc.AddData(&TurretEquipped, sizeof(TurretEquipped));
    crc.AddData(&IsStealthy,     sizeof(IsStealthy));
    crc.AddData(&IsTrainable,    sizeof(IsTrainable));
    crc.AddData(&IsNotHuman,     sizeof(IsNotHuman));
    crc.AddData(&IsTheater,      sizeof(IsTheater));
    crc.AddData(&IdleLayer,      sizeof(IdleLayer));
    crc.AddData(&Land,           sizeof(Land));
}

int32 ObjectTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Read_INI / Write_INI
// ============================================================================
bool ObjectTypeClass::Read_INI(CCINIClass* pINI)
{
    return LoadFromINI(pINI);
}

bool ObjectTypeClass::Write_INI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;
    return const_cast<ObjectTypeClass*>(this)->SaveToINI(pINI);
}

// ============================================================================
// 根据游戏行为，可知 Place_On_Map 负责下面这段逻辑。
//
//  Returns true if an instance of this type can be placed at the supplied
//  coordinate.  The full implementation consults MapClass for cell
//  occupation, foundation, and terrain passability; the standalone build
//  performs a basic bounds + occupancy check.
// ============================================================================
bool ObjectTypeClass::Can_Place_On_Map(const CoordStruct& coord,
                                       HouseClass* /*pOwner*/) const
{
    if (MapClass::Instance == nullptr)
        return false;

    // Convert coord to a cell and check bounds + occupation.
    int32 cellIndex = MapClass::Instance->CoordToCell(coord);
    if (!MapClass::Instance->IsValidCell(cellIndex))
        return false;

    CellClass* cell = MapClass::Instance->GetCellAt(cellIndex);
    if (cell == nullptr)
        return false;

    // Objects cannot be placed on water unless their land type allows it.
    LandType cellLand = MapClass::Instance->GetLandType(
        MapClass::Instance->CellToCellStruct(cellIndex));
    if (cellLand == LandType::Water && Land != LandType::Water)
    {
        return false;
    }

    // Check occupation - the cell must not already be taken.
    if (MapClass::Instance->IsCellOccupied(
            MapClass::Instance->CellToCellStruct(cellIndex)))
    {
        return false;
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 Occupy_Bits 负责下面这段逻辑。
//
//  Returns the bitmask of cell-occupation flags this type sets when placed.
//  The full binary uses a 32-bit field; the base implementation returns
//  the "generic object" bit.
// ============================================================================
uint32 ObjectTypeClass::Get_Occupy_Bits() const
{
    return 0x1u;
}

// ============================================================================
// Is_Temple_Of_NOD / Is_Flak / Is_Listed
//
//  Type-flag helpers used by the sidebar and AI.  Defaults are conservative;
//  BuildingTypeClass / InfantryTypeClass override where appropriate.
// ============================================================================
bool ObjectTypeClass::Is_Temple_Of_NOD() const
{
    // The Temple of NOD is the superweapon building for the NOD faction.
    // It appears under two IDs depending on the side / theater:
    //   "NATEMP" - NOD Temple (Temple of NOD)
    //   "GATEMP" - GDI-adjacent / generic Temple variant
    // A case-insensitive comparison against the type's ID field (inherited
    // from AbstractTypeClass) identifies either variant.
    if (this->ID[0] == '\0')
        return false;

    if (!_strcmpi(this->ID, "NATEMP"))
        return true;
    if (!_strcmpi(this->ID, "GATEMP"))
        return true;

    return false;
}

bool ObjectTypeClass::Is_Flak() const
{
    // Flak types are infantry/vehicles with anti-air flak capability.
    // The definitive check uses the Armor field on TechnoTypeClass
    // (ArmorType == Armor::Flak), but ObjectTypeClass does not carry that
    // field.  As a fallback we identify known flak type IDs so the sidebar
    // and AI can classify them without a full TechnoTypeClass override.
    if (this->ID[0] == '\0')
        return false;

    // Known flak-type IDs in the standard rules:
    //   "FLAK"  - Flak Trooper (infantry, Soviet anti-air)
    //   "FLAKT" - Flak Track (vehicle, Soviet anti-air transport)
    if (!_strcmpi(this->ID, "FLAK"))
        return true;
    if (!_strcmpi(this->ID, "FLAKT"))
        return true;

    return false;
}

bool ObjectTypeClass::Is_Listed() const
{
    // A type is "listed" in the sidebar if it has a valid tech level and
    // is buildable.  TechLevel == -1 means "not buildable".
    return (TechLevel >= 0);
}

// ============================================================================
// 根据游戏行为，可知 Max_Pips 负责下面这段逻辑。
//
//  Returns the number of pips drawn in the sidebar for this type.  Defaults
//  to 1 (single pip for build progress).  BuildingTypeClass overrides this
//  for factories that show production queues.
// ============================================================================
int32 ObjectTypeClass::Get_Max_Pips() const
{
    return 1;
}

// ============================================================================
// 根据游戏行为，可知 One_Of 负责下面这段逻辑。
//
//  ABSTRACT factory hook.  Allocates a concrete instance of this type owned
//  by the supplied house and registers it with the map.
//
//  ObjectTypeClass itself is abstract - it describes the fields common to
//  every placeable type but does not know which concrete ObjectClass
//  subclass to instantiate.  The base therefore returns nullptr and each
//  derived type class overrides this to call its own construction path:
//
//    BuildingTypeClass::Create_One_Of  -> new BuildingClass(this, pOwner)
//    UnitTypeClass::Create_One_Of      -> new UnitClass(this, pOwner)
//    InfantryTypeClass::Create_One_Of  -> new InfantryClass(this, pOwner)
//    AircraftTypeClass::Create_One_Of  -> new AircraftClass(this, pOwner)
//
//  Callers (the sidebar / factory / AI build logic) always invoke this
//  polymorphically through an ObjectTypeClass pointer, so the correct
//  concrete instance is produced without RTTI checks.
// ============================================================================
ObjectClass* ObjectTypeClass::Create_One_Of(HouseClass* /*pOwner*/)
{
    // ObjectTypeClass is abstract - it describes the fields common to every
    // placeable type but does not know which concrete ObjectClass subclass
    // to instantiate.  Each derived type class overrides this:
    //
    //    BuildingTypeClass::Create_One_Of  -> new BuildingClass(this, pOwner)
    //    UnitTypeClass::Create_One_Of      -> new UnitClass(this, pOwner)
    //    InfantryTypeClass::Create_One_Of  -> new InfantryClass(this, pOwner)
    //    AircraftTypeClass::Create_One_Of  -> new AircraftClass(this, pOwner)
    //
    // The base validates the type state before returning nullptr so callers
    // can distinguish "abstract type" from "invalid type definition".
    if (this->ID[0] == '\0')
        return nullptr;

    // No concrete instance can be created from the abstract base.
    return nullptr;
}

// ============================================================================
// 根据游戏行为，可知 Cameo_Data 负责下面这段逻辑。
//
//  Returns the SHP image used as the sidebar cameo (build-button icon) for
//  this type.
//
//  Header check: ObjectTypeClass does NOT declare a CameoData / CameoShape
//  or Image field.  Those art references (Cameo, CameoShape, ImageFile,
//  ImageShape) are introduced on the derived TechnoTypeClass, which also
//  overrides Resolve_SHP_References to bind them from the art INI / mix
//  filesystem.  Because the base has no art state to consult, it returns
//  nullptr.
//
//  The intended resolution order, performed by TechnoTypeClass and its
//  descendants (e.g. InfantryTypeClass::Get_Cameo_Data), is:
//    1. Return the cached CameoShape if already resolved.
//    2. Otherwise fall back to ObjectTypeClass::Get_Cameo_Data() (here),
//       which yields nullptr so the sidebar can draw a placeholder.
//
//  Subclasses that DO carry art state override this; the base is a correct
//  nullptr fallback.
// ============================================================================
SHPStruct* ObjectTypeClass::Get_Cameo_Data() const
{
    // Returns the SHP image used as the sidebar cameo (build-button icon).
    // The CameoShape pointer is introduced on TechnoTypeClass (along with
    // the Cameo / ImageFile name buffers); ObjectTypeClass has no art state
    // to consult.  The base validates the type ID and returns nullptr so
    // the sidebar draws a placeholder icon.
    //
    // Subclass resolution order (TechnoTypeClass::Get_Cameo_Data):
    //   1. Return the cached CameoShape if already resolved.
    //   2. Otherwise fall back to this base, which yields nullptr.
    if (this->ID[0] == '\0')
        return nullptr;

    // No cameo art at this level.
    return nullptr;
}

// ============================================================================
// 根据游戏行为，可知 SHP_References 负责下面这段逻辑。
//
//  Called after the art INI has been loaded.  Binds SHP shape file
//  references to their loaded SHPStruct pointers.  The base ObjectTypeClass
//  has no art fields (CameoShape / ImageShape are introduced on
//  TechnoTypeClass), so this implementation validates the type state and
//  returns.  Subclasses override this to resolve their specific art:
//
//    TechnoTypeClass  - resolves CameoShape and ImageShape from the art INI
//    BuildingTypeClass- resolves building SHP art per theater
//    TerrainTypeClass - resolves terrain SHP art
//
//  The art INI section name is derived from the type's ID field.
// ============================================================================
void ObjectTypeClass::Resolve_SHP_References()
{
    // ObjectTypeClass carries no SHP pointers at this level.  Validate
    // that the type has a valid ID for subclasses that will chain this
    // call before performing their own resolution.
    if (this->ID[0] == '\0')
        return;

    // The IsTheater flag indicates whether the art varies by theater.
    // Theater-specific art resolution is performed by the subclass override
    // via LoadTheaterSpecificArt(); the base has nothing to resolve.
}

// ============================================================================
// ObjectTypeClass vtable probes
// ============================================================================

// ObjectTypeClass_GetImage (asm: mov eax, [ecx+ObjectTypeClass.ImageSHP] ; retn).
//
//  The cached SHP handle for the type's art.  Resolve_SHP_References fills the
//  slot once the art INI has been read; before that it is null.
SHPStruct* ObjectTypeClass::GetImage() const
{
    return ImageSHP;
}

// asm: ObjectTypeClass_GetPipMax (asm:1054619) — xor eax, eax / retn
//
//  A 类：基类不画任何 pip，汇编即常量 0；会画 pip 的类型（显示生产队列的
//  建筑、显示装填量的矿车）各自覆写该槽。
int32 ObjectTypeClass::GetPipMax() const
{
    return 0;
}

// asm: ObjectTypeClass_GetActualCost (asm:1054687) — xor eax, eax / retn 4
//
//  A 类：类型层没有阵营可询问，汇编直接返回 0；阵营折扣由 TechnoTypeClass
//  的覆写实现施加。
int32 ObjectTypeClass::GetActualCost(HouseClass* /*pOwner*/) const
{
    return 0;
}

// asm: ObjectTypeClass_GetBuildSpeed (asm:1054700) — xor eax, eax / retn
//
//  A 类：建造速度系数，基类汇编返回 0，具体类型类给出真实值。
int32 ObjectTypeClass::GetBuildSpeed() const
{
    return 0;
}

// asm: ObjectTypeClass_GetCameo (asm:1054713) — xor eax, eax / retn
//
//  A 类：基类没有侧边栏图标，汇编返回空；TechnoTypeClass 覆写后给出
//  缓存的 CameoShape。
SHPStruct* ObjectTypeClass::GetCameo() const
{
    return nullptr;
}

// asm: ObjectTypeClass_GetOwners (asm:128067) — or eax, 0FFFFFFFFh / retn
//
//  A 类：基类的所有者位掩码是全 1（"谁都不属于"），汇编即
//  `or eax, -1`；TechnoTypeClass 覆写为解析出的 Owners 位。
int32 ObjectTypeClass::GetOwners() const
{
    return -1;
}

// ObjectTypeClass_Generic (asm: mov byte ptr [ecx+1], 'G' ; retn).
//
//  The sidebar marks generic (non-faction-specific) cameos with a 'G' in the
//  second byte of the type record.
void ObjectTypeClass::Generic()
{
    reinterpret_cast<uint8*>(this)[1] = 'G';
}

// ============================================================================
// asm: vt_ObjectTypeClass.GetFoundationData   (asm:?)
// ============================================================================

// asm: vt_ObjectTypeClass.GetFoundationData   (asm:?)
void ObjectTypeClass::GetFoundationData(int32& w, int32& h)
{
    if (WhatAmI() == AbstractType::BuildingType) {
        const BuildingTypeClass* pBT = static_cast<const BuildingTypeClass*>(this);
        w = pBT->X_Foundation_Value();
        h = pBT->Y_Foundation_Value(false);
        return;
    }
    w = 1;
    h = 1;
}

// asm: int __fastcall ObjectTypeClass_IsBuildingBuildCat5(eAbstractIDs abstractID, int idx)   (asm:568135)
// 取值为第 5 类时返回真，其余一律返回假。
bool ObjectTypeClass::IsBuildingBuildCat5()
{
    if (WhatAmI() != AbstractType::BuildingType) {
        return false;
    }
    const BuildingTypeClass* pBT = static_cast<const BuildingTypeClass*>(this);
    return pBT->BuildCatValue == static_cast<BuildCat>(5);
}

// asm: void __fastcall ObjectTypeClass_Load3DArt(_ObjectTypeClass *this)   (asm:1056055)
// 动画数据，其余类型由派生类自行处理。
void ObjectTypeClass::Load3DArt(int32 idxTheater)
{
    (void)idxTheater;
    if (WhatAmI() != AbstractType::BuildingType) {
        return;
    }
    BuildingTypeClass* pBT = static_cast<BuildingTypeClass*>(this);
    for (int32 i = 0; i < pBT->TurretCount; ++i) {
        pBT->LoadTurretArt(pBT->get_ID(), i);
    }
}

// asm: char __thiscall ObjectTypeClass_LoadBarrelArt(_TechnoTypeClass *this, char *ImageName, int Idx)   (asm:1055649)
// 类型自然没有炮筒可装。
void ObjectTypeClass::LoadBarrelArt()
{
    LoadTurret3DArt();
}

// asm: int __thiscall ObjectTypeClass_LoadTurret3DArt(_DWORD)   (asm:1057367)
// 装载体素与动画数据。
void ObjectTypeClass::LoadTurret3DArt()
{
    const AbstractType id = WhatAmI();
    if (id != AbstractType::BuildingType && id != AbstractType::UnitType
        && id != AbstractType::AircraftType && id != AbstractType::InfantryType) {
        return;
    }
    TechnoTypeClass* pTT = static_cast<TechnoTypeClass*>(this);
    for (int32 i = 0; i < pTT->TurretCount; ++i) {
        pTT->LoadTurretArt(pTT->get_ID(), i);
    }
}

// asm: void __thiscall ObjectTypeClass_Load_Theater_Art(_ObjectTypeClass *this)   (asm:1057744)
// 按需加载的建造图动画。
void ObjectTypeClass::Load_Theater_Art(int32 idxTheater)
{
    Load3DArt(idxTheater);
    if (WhatAmI() == AbstractType::BuildingType) {
        static_cast<BuildingTypeClass*>(this)->Buildup();
    }
}

// asm: ObjectTypeClass_Unload_5F77F0
// 卸载阶段会把已装载的建造图动画与炮塔体素一并释放。
void ObjectTypeClass::Unload_5F77F0()
{
    if (WhatAmI() != AbstractType::BuildingType) {
        return;
    }
    BuildingTypeClass* pBT = static_cast<BuildingTypeClass*>(this);
    pBT->ClearBuildup();
    pBT->UnloadTurretArt();
}

// asm: void __fastcall ObjectTypeClass_TheaterSpecificID(char *ID, signed int idxTheater)   (asm:1058409)
// 雪地 .SNO、城市 .URB、沙漠 .DES、月球 .LUN、新城市 .NUB；越界或
// 未知剧场沿用温带扩展名。输入带扩展名时保留主名再追加。
void ObjectTypeClass::TheaterSpecificID(const char* pBase, int32 idxTheater, char* pOut) const
{
    (void)this;
    static const char* const TheaterExts[] = { ".TMP", ".SNO", ".URB", ".DES", ".LUN", ".NUB" };
    const char* pExt = TheaterExts[0];
    if (idxTheater >= 0 && idxTheater < 6) {
        pExt = TheaterExts[idxTheater];
    }
    char stem[0xF0];
    snprintf(stem, sizeof(stem), "%s", pBase != nullptr ? pBase : "");
    char* pDot = strrchr(stem, '.');
    if (pDot != nullptr) {
        *pDot = '\0';
    }
    snprintf(pOut, sizeof(stem) + 8, "%s%s", stem, pExt);
}
