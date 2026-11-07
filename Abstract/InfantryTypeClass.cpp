#include <Abstract/InfantryTypeClass.h>
#include <Abstract/InfantryClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Audio/VocClass.h>
#include <Animations/AnimTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <FileFormats/SHP.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Houses/HouseClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// InfantryTypeClass.cpp
//
//  InfantryTypeClass is the type descriptor for every infantry unit - soldiers,
//  engineers, spies, dogs, civilians, cyborgs, etc.  It inherits the techno-
//  type fields and adds:
//
//    * Engineer / thief / spy / dog / Boris special-unit flags
//    * Civilian classification
//    * SHP animation sequence configuration (crawling, prone, deploy fire)
//    * Crush vulnerability flags
//    * Weapon slots (primary / secondary + elite variants)
//
//  This file implements:
//    * Static Array plumbing
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Classification helpers (IsEngineer / Is_Civilian / Is_Spy / ...)
//    * Get_Sequence_Count - returns the number of SHP animation sequences
//    * Resolve_SHP_References - binds infantry SHP art
//    * Get_Cameo_Data / Get_Image_Size / Get_Build_Queue_Type
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<InfantryTypeClass*>* InfantryTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void InfantryTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<InfantryTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<InfantryTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<InfantryTypeClass*>();
    }
}

void InfantryTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<InfantryTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

InfantryTypeClass* InfantryTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        InfantryTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

InfantryTypeClass* InfantryTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// asm: signed int __fastcall InfantryTypeClass_FindIndex(char *arg)   (0x523C90)
//   "<none>" / "none" both map to -1, otherwise a case-insensitive scan of the
//   InfantryTypes vector; the matching entry's index is returned, or -1.
int32 InfantryTypeClass::FindIndex(const char* pID)
{
    if (pID == nullptr)
        return -1;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none"))
        return -1;
    if (Array == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        InfantryTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return i;
    }
    return -1;
}

int32 InfantryTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

void InfantryTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        InfantryTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Constructor
// ============================================================================

InfantryTypeClass::InfantryTypeClass() noexcept
    : TechnoTypeClass(noinit)
{
    Engineer          = false;
    Thief             = false;
    Cow               = false;
    Dog               = false;
    Boris             = false;
    IsArmed_          = false;
    IsMissileSpawn_   = false;
    IsFake_           = false;
    IsDisableable_    = false;
    IsCanBeSuppressed_= false;
    IsCanBeOccupied_  = false;
    IsCanBeDriven_    = false;
    IsCanBeCaptured_  = false;
    IsCanBeRepaired_  = true;
    IsCanBeSold_      = true;
    IsCanBePowered_   = false;
    IsCanBeDestroyed_ = true;
    IsCanBeDamaged_   = true;
    IsCanBeInfiltrated_= false;
    IsCanBeSpied_     = false;
    IsCanBeSabotaged_ = false;
    IsCanBeStolen_    = false;
    IsCanBeHijacked_  = false;

    IsCrushable       = false;
    IsCrushable2      = false;
    IsTeleporter      = false;
    IsChrono          = false;
    IsBomb            = false;
    IsCow_            = false;
    IsDog_            = false;
    IsBoris_          = false;
    IsTilter          = false;
    IsToProtect       = false;
    IsNominal         = false;
    IsRadarInvisible  = false;
    IsDontScore       = false;
    IsNoThreat        = false;
    IsSensorsSight    = false;
    IsHunterSeeker    = false;
    IsIvan            = false;
    IsLeader          = false;
    IsCarryall        = false;
    IsTrain           = false;
    IsConsideredAircraft = false;
    IsConsideredVehicle  = false;
    IsSimpleDeployer  = false;
    IsFirebase        = false;
    IsSonic           = false;
    IsVan             = false;
    IsBalloonHover    = false;
    IsCyborg          = false;
    IsNotHuman        = false;  // Infantry ARE human by default
    IsImmuneToPsionics= false;
    IsImmuneToPoison  = false;
    IsImmuneToRadiation = false;
    IsImmuneToBerserk = false;
    IsImmuneToEMP     = false;

    ThreatPosedValue  = 0.0f;
    DeathWeaponIndex  = -1;
    WeaponCharge      = 0;
    WeaponCount       = 0;
    EliteWeaponCount  = 0;
    HasTurret         = false;
    CanCloak          = false;
    IsVoxel           = false;  // Infantry are SHP, not voxel
    HasDeployer       = false;
    HasUndeployer     = false;
    HasFirewall       = false;

    DeployFireSequence = Sequence::Deploy;

    std::memset(Weapons,        0, sizeof(Weapons));
    std::memset(EliteWeapons,   0, sizeof(EliteWeapons));
    std::memset(padding_InfantryType, 0, sizeof(padding_InfantryType));

    // Infantry are built by the barracks.
    Factory           = AbstractType::Infantry;
    IsTrainable_      = true;
    IsBuildable_      = true;
    IsNotHuman        = false;
    IsOrganic         = true;

    // Sync parent flags
    TechnoTypeClass::IsArmed         = IsArmed_;
    TechnoTypeClass::IsMissileSpawn  = IsMissileSpawn_;
    TechnoTypeClass::IsFake          = IsFake_;
    TechnoTypeClass::IsDisableable   = IsDisableable_;
    TechnoTypeClass::IsCrushable     = IsCrushable;
    TechnoTypeClass::IsBomb          = IsBomb;
    TechnoTypeClass::IsCow           = Cow;
    TechnoTypeClass::IsDog           = Dog;
    TechnoTypeClass::IsBoris         = Boris;
    TechnoTypeClass::IsVoxel_        = IsVoxel;
    TechnoTypeClass::Voxel           = IsVoxel;
}

// ============================================================================
// Destructor
// ============================================================================

InfantryTypeClass::~InfantryTypeClass()
{
    // SHP references are owned by the art system.
}

// ============================================================================
// asm: InfantryTypeClass_Load (0x524960) / InfantryTypeClass_Save (0x524B60)
//
//   Both chain into the techno-type serializer first, then persist the
//   infantry-only tail in the asm order:
//     [0E98h] VoiceComment 音效计数 + [0E8Ch] 的 dword 数组
//     [0E60h] DeadBodies 计数 + [0E54h] 的槽位
//     [0E7Ch] DeathAnims 计数 + [0E70h] 的槽位
//     0x5E8 字节 SequenceClass 区
//   本实现的 SequenceData 是内联 42×0x24 数组，整块读写与 asm 等长；两个动画
//   列表在原版是 VectorClass<AnimTypeClass*>，此处以"计数 + AnimType 下标"
//   的 4 字节槽位落盘，既保持字节宽度，又避免把裸指针写进存档。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: INIClass_Get_Vector_Voc_525430 (0x525430)
//
//   GetString("VoiceComment") → strtok(",") → 每个非空 token 先 VocClass_Find
//   （未命中则丢弃）再 VocClass_FindIndex，把下标追加进 TypeList<int>。
//   本实现用 VocClass::FindIndex 一步完成"查找 + 取下标"（未命中返回 -1）。
// ----------------------------------------------------------------------------
static void Parse_VoiceComment_Sounds(const char* pText, DynamicVectorClass<int32>& out)
{
    out.Clear();
    if (pText == nullptr)
        return;

    const char* p = pText;
    while (*p != '\0')
    {
        while (*p == ',')          // strtok 会合并连续分隔符
            ++p;
        if (*p == '\0')
            break;

        const char* pBegin = p;
        while (*p != '\0' && *p != ',')
            ++p;

        char name[0x40];
        size_t len = static_cast<size_t>(p - pBegin);
        if (len >= sizeof(name))
            len = sizeof(name) - 1;
        std::memcpy(name, pBegin, len);
        name[len] = '\0';

        const int32 index = VocClass::FindIndex(name);
        if (index >= 0)
            out.Add(index);
    }
}

HRESULT InfantryTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0)
        return hr;

    ULONG read = 0;

    // asm: [this+0E98h] count, then [this+0E8Ch] dwords  (VoiceComment 音效表).
    {
        int32 intCount = 0;
        hr = pStm->Read(&intCount, sizeof(intCount), &read);
        if (hr < 0 || read != sizeof(intCount))
            return E_FAIL;
        if (intCount < 0)
            return E_FAIL;
        VoiceCommentSounds.Clear();
        for (int32 i = 0; i < intCount; ++i)
        {
            int32 value = 0;
            hr = pStm->Read(&value, sizeof(value), &read);
            if (hr < 0 || read != sizeof(value))
                return E_FAIL;
            VoiceCommentSounds.Add(value);
        }
    }

    // asm: [this+0E60h] count, then [this+0E54h] anim slots (DeadBodies).
    // 指针槽在存档里改写为 AnimTypeClass 的数组下标，避免跨进程裸指针。
    {
        int32 count = 0;
        hr = pStm->Read(&count, sizeof(count), &read);
        if (hr < 0 || read != sizeof(count))
            return E_FAIL;
        if (count < 0)
            return E_FAIL;
        DeadBodies = nullptr;
        for (int32 i = 0; i < count; ++i)
        {
            int32 index = -1;
            hr = pStm->Read(&index, sizeof(index), &read);
            if (hr < 0 || read != sizeof(index))
                return E_FAIL;
            if (DeadBodies == nullptr && index >= 0)
                DeadBodies = AnimTypeClass::FindByIndex(index);
        }
    }

    // asm: [this+0E7Ch] count, then [this+0E70h] anim slots (DeathAnims).
    {
        int32 count = 0;
        hr = pStm->Read(&count, sizeof(count), &read);
        if (hr < 0 || read != sizeof(count))
            return E_FAIL;
        if (count < 0)
            return E_FAIL;
        DeathAnims = nullptr;
        for (int32 i = 0; i < count; ++i)
        {
            int32 index = -1;
            hr = pStm->Read(&index, sizeof(index), &read);
            if (hr < 0 || read != sizeof(index))
                return E_FAIL;
            if (DeathAnims == nullptr && index >= 0)
                DeathAnims = AnimTypeClass::FindByIndex(index);
        }
    }

    // asm: 读 0x5E8 字节整块填进 Sequence 区（此处为内联 SequenceData）。
    hr = pStm->Read(SequenceData, static_cast<ULONG>(sizeof(SequenceData)), &read);
    if (hr < 0 || read != sizeof(SequenceData))
        return E_FAIL;

    return S_OK;
}

HRESULT InfantryTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    // asm: call TechnoTypeClass_Save first and bail on a negative status.
    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0)
        return hr;

    ULONG written = 0;

    // asm: [0E98h] count，随后逐条写 [0E8Ch] 的 dword。
    const int32 intCount = VoiceCommentSounds.Count;
    hr = pStm->Write(&intCount, sizeof(intCount), &written);
    if (hr < 0 || written != sizeof(intCount))
        return E_FAIL;
    for (int32 i = 0; i < intCount; ++i)
    {
        int32 value = VoiceCommentSounds.GetItem(i);
        hr = pStm->Write(&value, sizeof(value), &written);
        if (hr < 0 || written != sizeof(value))
            return E_FAIL;
    }

    // asm: DeadBodies 列表 - 计数 + 每条一个 4 字节槽位（存 AnimType 下标）。
    const int32 deadIndex = (DeadBodies != nullptr && DeadBodies->get_ID() != nullptr)
                          ? AnimTypeClass::FindIndex(DeadBodies->get_ID()) : -1;
    const int32 deadCount = (deadIndex >= 0) ? 1 : 0;
    hr = pStm->Write(&deadCount, sizeof(deadCount), &written);
    if (hr < 0 || written != sizeof(deadCount))
        return E_FAIL;
    if (deadCount != 0)
    {
        hr = pStm->Write(&deadIndex, sizeof(deadIndex), &written);
        if (hr < 0 || written != sizeof(deadIndex))
            return E_FAIL;
    }

    // asm: DeathAnims 列表 - 同构。
    const int32 deathIndex = (DeathAnims != nullptr && DeathAnims->get_ID() != nullptr)
                           ? AnimTypeClass::FindIndex(DeathAnims->get_ID()) : -1;
    const int32 deathCount = (deathIndex >= 0) ? 1 : 0;
    hr = pStm->Write(&deathCount, sizeof(deathCount), &written);
    if (hr < 0 || written != sizeof(deathCount))
        return E_FAIL;
    if (deathCount != 0)
    {
        hr = pStm->Write(&deathIndex, sizeof(deathIndex), &written);
        if (hr < 0 || written != sizeof(deathIndex))
            return E_FAIL;
    }

    // asm: 从 [0E3Ch] 写 0x5E8 字节 Sequence 区。
    hr = pStm->Write(SequenceData, static_cast<ULONG>(sizeof(SequenceData)), &written);
    if (hr < 0 || written != sizeof(SequenceData))
        return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType InfantryTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::InfantryType;
}

bool InfantryTypeClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    return _strcmpi(this->ID, pID) == 0;
}

int32 InfantryTypeClass::Size() const
{
    return sizeof(InfantryTypeClass);
}

AbstractType InfantryTypeClass::GetClassID() const
{
    return AbstractType::InfantryType;
}

const char* InfantryTypeClass::get_ID() const
{
    return this->ID;
}

const wchar_t* InfantryTypeClass::GetUIName() const
{
    return this->UIName;
}

// ============================================================================
// Classification helpers
// ============================================================================

bool InfantryTypeClass::IsEngineer() const         { return Engineer; }
bool InfantryTypeClass::IsThief() const            { return Thief; }
bool InfantryTypeClass::IsCow() const              { return Cow; }
bool InfantryTypeClass::IsDog() const              { return Dog; }
bool InfantryTypeClass::IsBoris() const            { return Boris; }
bool InfantryTypeClass::IsArmed() const            { return IsArmed_; }
bool InfantryTypeClass::IsMissileSpawn() const     { return IsMissileSpawn_; }
bool InfantryTypeClass::IsFake() const             { return IsFake_; }
bool InfantryTypeClass::IsDisableable() const      { return IsDisableable_; }
bool InfantryTypeClass::IsCanBeSuppressed() const  { return IsCanBeSuppressed_; }
bool InfantryTypeClass::IsCanBeOccupied() const    { return IsCanBeOccupied_; }
bool InfantryTypeClass::IsCanBeDriven() const      { return IsCanBeDriven_; }
bool InfantryTypeClass::IsCanBeCaptured() const    { return IsCanBeCaptured_; }
bool InfantryTypeClass::IsCanBeRepaired() const    { return IsCanBeRepaired_; }
bool InfantryTypeClass::IsCanBeSold() const        { return IsCanBeSold_; }
bool InfantryTypeClass::IsCanBePowered() const     { return IsCanBePowered_; }
bool InfantryTypeClass::IsCanBeDestroyed() const   { return IsCanBeDestroyed_; }
bool InfantryTypeClass::IsCanBeDamaged() const     { return IsCanBeDamaged_; }
bool InfantryTypeClass::IsCanBeInfiltrated() const { return IsCanBeInfiltrated_; }
bool InfantryTypeClass::IsCanBeSpied() const       { return IsCanBeSpied_; }
bool InfantryTypeClass::IsCanBeSabotaged() const   { return IsCanBeSabotaged_; }
bool InfantryTypeClass::IsCanBeStolen() const      { return IsCanBeStolen_; }
bool InfantryTypeClass::IsCanBeHijacked() const    { return IsCanBeHijacked_; }

// ============================================================================
// Extended infantry accessors
// ============================================================================

bool InfantryTypeClass::Is_Civilian() const
{
    // A civilian infantry unit is one that belongs to the civilian house and
    // is not a combatant.  In the full binary this is determined by the
    // owner house type; the standalone build uses the Insignificant flag
    // combined with the absence of weapons.
    return IsInsignificant_ && WeaponCount == 0 && !Engineer && !Thief;
}

bool InfantryTypeClass::Is_Spy() const
{
    // In Yuri's Revenge, the spy is a special infantry type identified by
    // the Thief flag (the spy inherits from the thief class in the binary).
    // The standalone build checks both Thief and a specific "spy" name match.
    return Thief;
}

int32 InfantryTypeClass::Get_Sequence_Count() const
{
    // Returns the number of SHP animation sequences defined for this
    // infantry type.  The Sequence enum defines 35 possible sequences
    // (Ready, Guard, Prone, Walk, FireUp, Down, etc.).  Not all sequences
    // are used by every infantry type; the full binary checks the SHP
    // frame count for each sequence.  Here we return the full count.
    return static_cast<int32>(Sequence::Count);
}

Point2D InfantryTypeClass::Get_Image_Size() const
{
    // Infantry are SHP-rendered.  The image size is derived from the SHP
    // bounds.  If Resolve_SHP_References hasn't run yet, return a default.
    if (ImageSize.X > 0 || ImageSize.Y > 0)
        return ImageSize;

    // Default infantry SHP size.
    return Point2D(28, 28);
}

AbstractType InfantryTypeClass::Get_Build_Queue_Type() const
{
    // Infantry are produced by the barracks.
    return AbstractType::Infantry;
}

// ============================================================================
// 根据游戏行为，可知 SHP_References 负责下面这段逻辑。
//
//  Binds the infantry SHP image.  Infantry use SHP files (not voxels) for
//  their rendering, with multiple sequences for walking, firing, prone, etc.
// ============================================================================

void InfantryTypeClass::Resolve_SHP_References()
{
    // Chain the parent for cameo and base SHP references.
    TechnoTypeClass::Resolve_SHP_References();

    // Infantry SHP image defaults to the ImageFile name.  The parent's
    // Resolve_SHP_References already attempts to load ImageShape from
    // ImageFile; here we just set the image size if the shape is loaded.
    if (ImageShape != nullptr)
    {
        if (ImageSize.X == 0 && ImageSize.Y == 0)
        {
            // The full binary reads the SHP header for width/height.
            // Default to standard infantry size.
            ImageSize.X = 28;
            ImageSize.Y = 28;
        }
    }
}

// ============================================================================
// 根据游戏行为，可知 Cameo_Data 负责下面这段逻辑。
//
//  Returns the cameo SHP for the sidebar build button.  Falls back to the
//  parent implementation if no infantry-specific cameo is set.
// ============================================================================

SHPStruct* InfantryTypeClass::Get_Cameo_Data() const
{
    // Use the cameo shape resolved by Resolve_SHP_References.
    if (CameoShape != nullptr)
        return CameoShape;

    // Fall back to the parent implementation.
    return ObjectTypeClass::Get_Cameo_Data();
}

// ============================================================================
// LoadFromINI
// ============================================================================

bool InfantryTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // Chain the parent first.
    TechnoTypeClass::LoadFromINI(pINI);

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Special-unit classification
    // ------------------------------------------------------------------
    Engineer = pINI->ReadBool(section, "Engineer", Engineer);
    Thief    = pINI->ReadBool(section, "Thief",    Thief);

    IsCow_   = Cow;
    IsDog_   = Dog;
    IsBoris_ = Boris;

    // Sync parent
    TechnoTypeClass::IsCow  = Cow;
    TechnoTypeClass::IsDog  = Dog;
    TechnoTypeClass::IsBoris = Boris;

    // ------------------------------------------------------------------
    // Combat flags (with underscore-suffixed own fields)
    // ------------------------------------------------------------------
    IsMissileSpawn_   = pINI->ReadBool(section, "MissileSpawn",   IsMissileSpawn_);
    IsDisableable_    = pINI->ReadBool(section, "Disableable",    IsDisableable_);

    TechnoTypeClass::IsArmed        = IsArmed_;
    TechnoTypeClass::IsMissileSpawn = IsMissileSpawn_;
    TechnoTypeClass::IsFake         = IsFake_;
    TechnoTypeClass::IsDisableable  = IsDisableable_;

    // ------------------------------------------------------------------
    // CanBeXxx interaction flags
    // ------------------------------------------------------------------
    IsCanBeOccupied_    = pINI->ReadBool(section, "CanBeOccupied",   IsCanBeOccupied_);
    IsCanBeCaptured_    = pINI->ReadBool(section, "Capturable",   IsCanBeCaptured_);
    IsCanBeRepaired_    = pINI->ReadBool(section, "Repairable",   IsCanBeRepaired_);
    IsCanBePowered_     = pINI->ReadBool(section, "Powered",    IsCanBePowered_);
    IsCanBeSpied_       = pINI->ReadBool(section, "Spyable",      IsCanBeSpied_);

    // ------------------------------------------------------------------
    // Crush / movement flags
    // ------------------------------------------------------------------
    IsCrushable   = pINI->ReadBool(section, "Crushable",   IsCrushable);
    IsCrushable2  = pINI->ReadBool(section, "Crushable",  IsCrushable2);
    IsTeleporter  = pINI->ReadBool(section, "Teleporter",  IsTeleporter);
    IsBomb        = pINI->ReadBool(section, "Bomb",        IsBomb);

    TechnoTypeClass::IsCrushable  = IsCrushable;
    TechnoTypeClass::IsCrushable2 = IsCrushable2;
    TechnoTypeClass::IsTeleporter = IsTeleporter;
    TechnoTypeClass::IsChrono     = IsChrono;
    TechnoTypeClass::IsBomb       = IsBomb;

    // ------------------------------------------------------------------
    // Pips
    // ------------------------------------------------------------------
    Pip       = pINI->ReadPipIdx(section, "Pip",       Pip);
    OccupyPip = pINI->ReadPipIdx(section, "OccupyPip", OccupyPip);

    // ------------------------------------------------------------------
    // Voxel / SHP classification
    // ------------------------------------------------------------------
    IsVoxel    = pINI->ReadBool(section, "Voxel", IsVoxel);
    Voxel      = IsVoxel;
    IsVoxel_   = IsVoxel;

    HasTurret     = pINI->ReadBool(section, "Turret",     HasTurret);
    CanCloak      = pINI->ReadBool(section, "Cloakable",  CanCloak);
    HasDeployer   = pINI->ReadBool(section, "Deployer",   HasDeployer);

    Turret     = HasTurret;
    Cloak      = CanCloak;
    Deployer   = HasDeployer;
    Undeployer = HasUndeployer;
    Firewall   = HasFirewall;

    // ------------------------------------------------------------------
    // Speed / ROT
    // ------------------------------------------------------------------
    Speed    = pINI->ReadInteger(section, "Speed",    Speed);
    ROT      = pINI->ReadInteger(section, "ROT",      ROT);
    TurretROT= pINI->ReadInteger(section, "ROT", TurretROT);

    // ------------------------------------------------------------------
    // Weapons
    // ------------------------------------------------------------------
    WeaponCount = pINI->ReadInteger(section, "WeaponCount", WeaponCount);
    if (WeaponCount < 0) WeaponCount = 0;
    if (WeaponCount > 2) WeaponCount = 2;
    for (int32 i = 0; i < WeaponCount; ++i)
    {
        std::memset(&Weapons[i], 0, sizeof(WeaponStruct));
    }

    if (EliteWeaponCount < 0) EliteWeaponCount = 0;
    if (EliteWeaponCount > 2) EliteWeaponCount = 2;
    for (int32 i = 0; i < EliteWeaponCount; ++i)
    {
        std::memset(&EliteWeapons[i], 0, sizeof(WeaponStruct));
    }

    DeathWeaponIndex = pINI->ReadInteger(section, "DeathWeapon", -1);

    // ------------------------------------------------------------------
    // Deploy fire sequence
    // ------------------------------------------------------------------
    int32 deploySeq = pINI->ReadInteger(section, "DeployFire",
                                        static_cast<int32>(DeployFireSequence));
    DeployFireSequence = static_cast<Sequence>(deploySeq);

    // ------------------------------------------------------------------
    // Threat / score
    // ------------------------------------------------------------------
    ThreatPosedValue = pINI->ReadFloat(section, "ThreatPosed", ThreatPosedValue);
    Score            = pINI->ReadInteger(section, "Score", Score);

    // ------------------------------------------------------------------
    // Special classification flags
    // ------------------------------------------------------------------
    IsIvan            = pINI->ReadBool(section, "Ivan",            IsIvan);
    IsCarryall        = pINI->ReadBool(section, "Carryall",        IsCarryall);
    IsSimpleDeployer  = pINI->ReadBool(section, "Deployer",  IsSimpleDeployer);
    IsSonic           = pINI->ReadBool(section, "Sonic",           IsSonic);
    IsBalloonHover    = pINI->ReadBool(section, "BalloonHover",    IsBalloonHover);
    IsCyborg          = pINI->ReadBool(section, "Cyborg",          IsCyborg);
    IsConsideredAircraft = pINI->ReadBool(section, "ConsideredAircraft", IsConsideredAircraft);

    TechnoTypeClass::IsIvan            = IsIvan;
    TechnoTypeClass::IsLeader          = IsLeader;
    TechnoTypeClass::IsCarryall        = IsCarryall;
    TechnoTypeClass::IsTrain           = IsTrain;
    TechnoTypeClass::IsSimpleDeployer  = IsSimpleDeployer;
    TechnoTypeClass::IsFirebase        = IsFirebase;
    TechnoTypeClass::IsSonic           = IsSonic;
    TechnoTypeClass::IsVan             = IsVan;
    TechnoTypeClass::IsBalloonHover    = IsBalloonHover;
    TechnoTypeClass::IsCyborg          = IsCyborg;
    TechnoTypeClass::IsConsideredAircraft = IsConsideredAircraft;
    TechnoTypeClass::IsConsideredVehicle  = IsConsideredVehicle;

    // ------------------------------------------------------------------
    // Immunity flags
    // ------------------------------------------------------------------
    IsImmuneToPsionics  = pINI->ReadBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    IsImmuneToPoison    = pINI->ReadBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    IsImmuneToRadiation = pINI->ReadBool(section, "ImmuneToRadiation", IsImmuneToRadiation);

    // ------------------------------------------------------------------
    // Misc flags
    // ------------------------------------------------------------------
    IsToProtect       = pINI->ReadBool(section, "ToProtect",       IsToProtect);
    IsNominal         = pINI->ReadBool(section, "Nominal",         IsNominal);
    IsRadarInvisible  = pINI->ReadBool(section, "RadarInvisible",  IsRadarInvisible);
    IsDontScore       = pINI->ReadBool(section, "DontScore",       IsDontScore);
    IsNoThreat        = pINI->ReadBool(section, "NoThreat",        IsNoThreat);
    IsSensorsSight    = pINI->ReadBool(section, "SensorsSight",    IsSensorsSight);
    IsHunterSeeker    = pINI->ReadBool(section, "HunterSeeker",    IsHunterSeeker);

    IsNaval            = pINI->ReadBool(section, "Naval",            IsNaval);
    IsLand             = pINI->ReadBool(section, "Land",             IsLand);
    IsAir              = pINI->ReadBool(section, "Air",              IsAir);
    IsNotHuman         = pINI->ReadBool(section, "NotHuman",         IsNotHuman);
    IsOrganic          = pINI->ReadBool(section, "Organic",          IsOrganic);
    IsNeutral          = pINI->ReadBool(section, "Neutral",          IsNeutral);

    // ------------------------------------------------------------------
    // Factory type
    // ------------------------------------------------------------------
    char factoryBuf[32];
    pINI->ReadString(section, "Factory", "Infantry", factoryBuf, sizeof(factoryBuf));
    if (!_strcmpi(factoryBuf, "Building"))   Factory = AbstractType::Building;
    else if (!_strcmpi(factoryBuf, "Infantry")) Factory = AbstractType::Infantry;
    else if (!_strcmpi(factoryBuf, "Unit"))  Factory = AbstractType::Unit;
    else if (!_strcmpi(factoryBuf, "Aircraft")) Factory = AbstractType::Aircraft;
    else                                     Factory = AbstractType::Infantry;


    // generated-ini-reads
    // ------------------------------------------------------------------
    // Full key set - every field keeps its current value when the key
    // is absent, so partially specified sections stay valid.
    // ------------------------------------------------------------------
    CCINIClass* pArt = &CCINIClass::INI_Art;
    if (pArt == nullptr)
        pArt = pINI;

    Pip = pINI->ReadPipIdx(section, "Pip", Pip);
    OccupyPip = pINI->ReadPipIdx(section, "OccupyPip", OccupyPip);
    { char _buf[0x40]; if (pINI->ReadString(section, "OccupyWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) OccupyWeapon = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "EliteOccupyWeapon", "", _buf, sizeof(_buf)) > 0) { WeaponTypeClass* _p = WeaponTypeClass::FindOrAllocate(_buf); if (_p) EliteOccupyWeapon = _p; } }
    pINI->ReadString(section, "VoiceComment", VoiceComment, VoiceComment, sizeof(VoiceComment));
    // asm: [0E88h] = INIClass_Get_Vector_Voc_525430(section, "VoiceComment")
    Parse_VoiceComment_Sounds(VoiceComment, VoiceCommentSounds);
    { char _buf[0x40]; if (pINI->ReadString(section, "DeadBodies", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DeadBodies = _p; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "DeathAnims", "", _buf, sizeof(_buf)) > 0) { AnimTypeClass* _p = AnimTypeClass::FindOrAllocate(_buf); if (_p) DeathAnims = _p; } }
    Cyborg = pINI->ReadBool(section, "Cyborg", Cyborg);
    NotHuman = pINI->ReadBool(section, "NotHuman", NotHuman);
    { char _buf[0x40]; if (pINI->ReadString(section, "EnterWaterSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) EnterWaterSound = _i; } }
    { char _buf[0x40]; if (pINI->ReadString(section, "LeaveWaterSound", "", _buf, sizeof(_buf)) > 0) { int32 _i = VocClass::FindIndexOfName(_buf); if (_i >= 0) LeaveWaterSound = _i; } }
    Fearless = pINI->ReadBool(section, "Fearless", Fearless);
    Fraidycat = pINI->ReadBool(section, "Fraidycat", Fraidycat);
    Infiltrate = pINI->ReadBool(section, "Infiltrate", Infiltrate);
    Ivan = pINI->ReadBool(section, "Ivan", Ivan);
    Occupier = pINI->ReadBool(section, "Occupier", Occupier);
    Assaulter = pINI->ReadBool(section, "Assaulter", Assaulter);
    DetectionDistance = pINI->ReadInteger(section, "DetectionDistance", DetectionDistance);
    HarvestRate = pINI->ReadInteger(section, "HarvestRate", HarvestRate);
    C4 = pINI->ReadBool(section, "C4", C4);
    Civilian = pINI->ReadBool(section, "Civilian", Civilian);
    Engineer = pINI->ReadBool(section, "Engineer", Engineer);
    TiberiumProof = pINI->ReadBool(section, "TiberiumProof", TiberiumProof);
    Agent = pINI->ReadBool(section, "Agent", Agent);
    Thief = pINI->ReadBool(section, "Thief", Thief);
    VehicleThief = pINI->ReadBool(section, "VehicleThief", VehicleThief);
    Doggie = pINI->ReadBool(section, "Doggie", Doggie);
    Deployer = pINI->ReadBool(section, "Deployer", Deployer);
    DeployedCrushable = pINI->ReadBool(section, "DeployedCrushable", DeployedCrushable);
    UseOwnName = pINI->ReadBool(section, "UseOwnName", UseOwnName);
    JumpJetTurn = pINI->ReadBool(section, "JumpJetTurn", JumpJetTurn);

    // ------------------------------------------------------------------
    // artmd.ini fields
    // ------------------------------------------------------------------
    Crawls = pArt->ReadBool(section, "Crawls", Crawls);
    FireUp = pArt->ReadInteger(section, "FireUp", FireUp);
    FireProne = pArt->ReadInteger(section, "FireProne", FireProne);
    SecondaryFire = pArt->ReadInteger(section, "SecondaryFire", SecondaryFire);
    SecondaryProne = pArt->ReadInteger(section, "SecondaryProne", SecondaryProne);

        return true;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool InfantryTypeClass::SaveToINI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    const_cast<InfantryTypeClass*>(this)->TechnoTypeClass::SaveToINI(pINI);

    pINI->WriteInteger(section, "WeaponCount",      WeaponCount);
    pINI->WriteInteger(section, "EliteWeaponCount", EliteWeaponCount);
    pINI->WriteInteger(section, "DeathWeapon",      DeathWeaponIndex);
    pINI->WriteInteger(section, "WeaponCharge",     WeaponCharge);
    pINI->WriteInteger(section, "DeployFire", static_cast<int32>(DeployFireSequence));
    pINI->WriteFloat(section,  "ThreatPosed",      ThreatPosedValue);

    pINI->WriteBool(section, "Engineer", Engineer);
    pINI->WriteBool(section, "Thief",    Thief);
    pINI->WriteBool(section, "Cow",      Cow);
    pINI->WriteBool(section, "Dog",      Dog);
    pINI->WriteBool(section, "Boris",    Boris);

    pINI->WriteBool(section, "Armed",         IsArmed_);
    pINI->WriteBool(section, "MissileSpawn",  IsMissileSpawn_);
    pINI->WriteBool(section, "Fake",          IsFake_);
    pINI->WriteBool(section, "Disableable",   IsDisableable_);

    pINI->WriteBool(section, "CanSuppressed",    IsCanBeSuppressed_);
    pINI->WriteBool(section, "CanBeOccupied",    IsCanBeOccupied_);
    pINI->WriteBool(section, "CanBeDriven",      IsCanBeDriven_);
    pINI->WriteBool(section, "Capturable",    IsCanBeCaptured_);
    pINI->WriteBool(section, "Repairable",    IsCanBeRepaired_);
    pINI->WriteBool(section, "CanBeSold",        IsCanBeSold_);
    pINI->WriteBool(section, "Powered",     IsCanBePowered_);
    pINI->WriteBool(section, "CanBeDestroyed",   IsCanBeDestroyed_);
    pINI->WriteBool(section, "CanBeDamaged",     IsCanBeDamaged_);
    pINI->WriteBool(section, "CanBeInfiltrated", IsCanBeInfiltrated_);
    pINI->WriteBool(section, "Spyable",       IsCanBeSpied_);
    pINI->WriteBool(section, "CanBeSabotaged",   IsCanBeSabotaged_);
    pINI->WriteBool(section, "CanBeStolen",      IsCanBeStolen_);
    pINI->WriteBool(section, "CanBeHijacked",    IsCanBeHijacked_);

    pINI->WriteBool(section, "Crushable",   IsCrushable);
    pINI->WriteBool(section, "Crushable",  IsCrushable2);
    pINI->WriteBool(section, "Teleporter",  IsTeleporter);
    pINI->WriteBool(section, "Chrono",      IsChrono);
    pINI->WriteBool(section, "Bomb",        IsBomb);

    pINI->WriteBool(section, "Voxel",     IsVoxel);
    pINI->WriteBool(section, "Turret",    HasTurret);
    pINI->WriteBool(section, "Cloakable", CanCloak);
    pINI->WriteBool(section, "Deployer",  HasDeployer);
    pINI->WriteBool(section, "Undeployer",HasUndeployer);
    pINI->WriteBool(section, "Firewall",  HasFirewall);

    pINI->WriteBool(section, "Ivan",            IsIvan);
    pINI->WriteBool(section, "Leader",          IsLeader);
    pINI->WriteBool(section, "Carryall",        IsCarryall);
    pINI->WriteBool(section, "Train",           IsTrain);
    pINI->WriteBool(section, "Deployer",  IsSimpleDeployer);
    pINI->WriteBool(section, "Firebase",        IsFirebase);
    pINI->WriteBool(section, "Sonic",           IsSonic);
    pINI->WriteBool(section, "Van",             IsVan);
    pINI->WriteBool(section, "BalloonHover",    IsBalloonHover);
    pINI->WriteBool(section, "Cyborg",          IsCyborg);
    pINI->WriteBool(section, "ConsideredAircraft", IsConsideredAircraft);
    pINI->WriteBool(section, "ConsideredVehicle",  IsConsideredVehicle);

    pINI->WriteBool(section, "ImmuneToPsionics",  IsImmuneToPsionics);
    pINI->WriteBool(section, "ImmuneToPoison",    IsImmuneToPoison);
    pINI->WriteBool(section, "ImmuneToRadiation", IsImmuneToRadiation);
    pINI->WriteBool(section, "ImmuneToBerserk",   IsImmuneToBerserk);
    pINI->WriteBool(section, "ImmuneToEMP",       IsImmuneToEMP);

    pINI->WriteBool(section, "Tilter",          IsTilter);
    pINI->WriteBool(section, "ToProtect",       IsToProtect);
    pINI->WriteBool(section, "Nominal",         IsNominal);
    pINI->WriteBool(section, "RadarInvisible",  IsRadarInvisible);
    pINI->WriteBool(section, "DontScore",       IsDontScore);
    pINI->WriteBool(section, "NoThreat",        IsNoThreat);
    pINI->WriteBool(section, "SensorsSight",    IsSensorsSight);
    pINI->WriteBool(section, "HunterSeeker",    IsHunterSeeker);

    pINI->WriteBool(section, "Naval",            IsNaval);
    pINI->WriteBool(section, "Land",             IsLand);
    pINI->WriteBool(section, "Air",              IsAir);
    pINI->WriteBool(section, "NotHuman",         IsNotHuman);
    pINI->WriteBool(section, "Organic",          IsOrganic);
    pINI->WriteBool(section, "Neutral",          IsNeutral);
    pINI->WriteBool(section, "Infiltratable",    IsInfiltratable);
    pINI->WriteBool(section, "Stealthy",         IsStealthy);
    pINI->WriteBool(section, "Healable",         IsHealable);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

void InfantryTypeClass::ComputeCRC(CRCEngine& crc) const
{
    TechnoTypeClass::ComputeCRC(crc);

    crc.AddData(&Engineer,           sizeof(Engineer));
    crc.AddData(&Thief,              sizeof(Thief));
    crc.AddData(&Cow,                sizeof(Cow));
    crc.AddData(&Dog,                sizeof(Dog));
    crc.AddData(&Boris,              sizeof(Boris));
    crc.AddData(&IsArmed_,           sizeof(IsArmed_));
    crc.AddData(&IsMissileSpawn_,    sizeof(IsMissileSpawn_));
    crc.AddData(&IsFake_,            sizeof(IsFake_));
    crc.AddData(&IsDisableable_,     sizeof(IsDisableable_));
    crc.AddData(&IsCanBeSuppressed_, sizeof(IsCanBeSuppressed_));
    crc.AddData(&IsCanBeOccupied_,   sizeof(IsCanBeOccupied_));
    crc.AddData(&IsCanBeDriven_,     sizeof(IsCanBeDriven_));
    crc.AddData(&IsCanBeCaptured_,   sizeof(IsCanBeCaptured_));
    crc.AddData(&IsCanBeRepaired_,   sizeof(IsCanBeRepaired_));
    crc.AddData(&IsCanBeSold_,       sizeof(IsCanBeSold_));
    crc.AddData(&IsCanBePowered_,    sizeof(IsCanBePowered_));
    crc.AddData(&IsCanBeDestroyed_,  sizeof(IsCanBeDestroyed_));
    crc.AddData(&IsCanBeDamaged_,    sizeof(IsCanBeDamaged_));
    crc.AddData(&IsCanBeInfiltrated_,sizeof(IsCanBeInfiltrated_));
    crc.AddData(&IsCanBeSpied_,      sizeof(IsCanBeSpied_));
    crc.AddData(&IsCanBeSabotaged_,  sizeof(IsCanBeSabotaged_));
    crc.AddData(&IsCanBeStolen_,     sizeof(IsCanBeStolen_));
    crc.AddData(&IsCanBeHijacked_,   sizeof(IsCanBeHijacked_));
    crc.AddData(&IsCrushable,        sizeof(IsCrushable));
    crc.AddData(&IsCrushable2,       sizeof(IsCrushable2));
    crc.AddData(&IsTeleporter,       sizeof(IsTeleporter));
    crc.AddData(&IsChrono,           sizeof(IsChrono));
    crc.AddData(&IsBomb,             sizeof(IsBomb));
    crc.AddData(&IsCyborg,           sizeof(IsCyborg));
    crc.AddData(&IsBalloonHover,     sizeof(IsBalloonHover));
    crc.AddData(&ThreatPosedValue,   sizeof(ThreatPosedValue));
    crc.AddData(&DeathWeaponIndex,   sizeof(DeathWeaponIndex));
    crc.AddData(&WeaponCharge,       sizeof(WeaponCharge));
    crc.AddData(&WeaponCount,        sizeof(WeaponCount));
    crc.AddData(Weapons,             static_cast<int32>(sizeof(Weapons)));
    crc.AddData(&EliteWeaponCount,   sizeof(EliteWeaponCount));
    crc.AddData(EliteWeapons,        static_cast<int32>(sizeof(EliteWeapons)));
    crc.AddData(&HasTurret,          sizeof(HasTurret));
    crc.AddData(&CanCloak,           sizeof(CanCloak));
    crc.AddData(&IsVoxel,            sizeof(IsVoxel));
    crc.AddData(&HasDeployer,        sizeof(HasDeployer));
    crc.AddData(&HasUndeployer,      sizeof(HasUndeployer));
    crc.AddData(&HasFirewall,        sizeof(HasFirewall));
    crc.AddData(&DeployFireSequence, sizeof(DeployFireSequence));
}

int32 InfantryTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// InfantryTypeClass - static lookup helpers
// ============================================================================

InfantryTypeClass* InfantryTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    InfantryTypeClass* found = Find(pID);
    if (found) return found;
    if (!Array) Init_Array();
    InfantryTypeClass* newItem = GameCreate<InfantryTypeClass>();
    if (newItem)
    {
        strncpy(newItem->ID, pID, sizeof(newItem->ID) - 1);
        newItem->ID[sizeof(newItem->ID) - 1] = '\0';
    }
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// ============================================================================
// InfantryTypeClass sequence data
//
//   Every entry of the Sequences table owns a 0x24 byte record.  The first
//   dword is the facing index (the "N"/"NE"/... token from the artmd.ini
//   value); the next two dwords are frame counters; the two VocClass slots
//   and their frame numbers fill the rest.  The original walks the records
//   with a 0x24 stride from SequenceData[0].
// ============================================================================

namespace {

// The 42 sequence names the original's "Sequences" table holds, in order.
const char* const g_SequenceNames[] = {
    "Ready", "Guard", "Prone", "Walk", "FireUp", "Down",
    "Crawl", "Up", "FireProne", "Idle1", "Idle2", "Die1",
    "Die2", "Die3", "Die4", "Die5", "Tread", "Swim",
    "WetIdle1", "WetIdle2", "WetDie1", "WetDie2", "WetAttack", "Hover",
    "Fly", "Tumble", "FireFly", "Deploy", "Deployed", "DeployedFire",
    "DeployedIdle", "Undeploy", "Cheer", "Paradrop", "AirDeathStart",
    "AirDeathFalling", "AirDeathFinish", "Panic", "Shovel", "Carry",
    "SecondaryFire", "SecondaryProne"
};

const int32 g_SequenceCount =
    sizeof(g_SequenceNames) / sizeof(g_SequenceNames[0]);

// Facing tokens in the order the original compares them.
const char* const g_FacingNames[] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"
};

int32 FacingFromName(const char* pName)
{
    for (int32 i = 0; i < 8; ++i)
    {
        if (std::strcmp(pName, g_FacingNames[i]) == 0) {
            return i;
        }
    }
    return 0;
}

} // namespace

const char* InfantryTypeClass::GetSequenceName(int32 index)
{
    if (index < 0 || index >= g_SequenceCount) {
        return nullptr;
    }
    return g_SequenceNames[index];
}

int32 InfantryTypeClass::GetSequenceNameCount()
{
    return g_SequenceCount;
}

// ============================================================================
// InfantryTypeClass::GetSequenceRecordIndex
//
//   把项目的 Sequence 枚举翻成原版 Sequences 表的记录下标（0..41），
//   该下标同时是 artmd.ini 解析时的写入顺序，也是 sub_518D80 取
//   StartFrame/FrameCount/FramesPerDirection 的下标。
//   原版表里独有、项目枚举没有对应项的名字（Up / Tread / WetAttack /
//   Hover / Cheer / Panic / Shovel / Carry / AirDeath* / Secondary*）由
//   artmd.ini 直接按记录下标写入，不需要从这里映射。
// ============================================================================
int32 InfantryTypeClass::GetSequenceRecordIndex(Sequence seq)
{
    switch (seq)
    {
        case Sequence::Ready:         return 0;
        case Sequence::Guard:         return 1;
        case Sequence::Prone:         return 2;
        case Sequence::Walk:          return 3;
        case Sequence::FireUp:        return 4;
        case Sequence::Down:          return 5;
        case Sequence::Crawl:         return 6;
        case Sequence::FireProne:     return 8;
        case Sequence::Idle1:         return 9;
        case Sequence::Idle2:         return 10;
        case Sequence::Die1:          return 11;
        case Sequence::Die2:          return 12;
        case Sequence::Die3:          return 13;
        case Sequence::Die4:          return 14;
        case Sequence::Die5:          return 15;
        case Sequence::Swim:          return 17;
        case Sequence::WetIdle1:      return 18;
        case Sequence::WetIdle2:      return 19;
        case Sequence::WetDie1:       return 20;
        case Sequence::WetDie2:       return 21;
        case Sequence::Fly:           return 24;
        case Sequence::Tumble:        return 25;
        case Sequence::FireFly:       return 26;
        case Sequence::Deploy:        return 27;
        case Sequence::Deployed:      return 28;
        case Sequence::DeployedFire:  return 29;
        case Sequence::DeployedIdle:  return 30;
        case Sequence::Undeploy:      return 31;
        case Sequence::Paradrop:      return 33;
        default:                      return 1;   // Guard
    }
}

// ============================================================================
 // InfantryTypeClass_ParseSequence -
//
//   artmd.ini's section for this infantry is scanned twice per sequence.  The
//   "<Name>" key carries a "%d,%d,%d,%s" value: two frame numbers, a count,
//   and a facing token that selects the facing slot.  The "<Name>Sounds" key
//   then carries a whitespace separated run of "<frame>,<voc name>" pairs,
//   each of which is resolved through VocClass::FindIndexOfName and stored in
//   the sequence's two sound slots.
//
//   ParseSequence exits early when the section has no "Sequence" key at all.
// ============================================================================
void InfantryTypeClass::ParseSequence()
{
    // The section is the art image tag, normally this type's Image name.
    const char* pImage = Image;
    if (pImage == nullptr || pImage[0] == '\0') {
        pImage = ID;
    }

    char section[0x20];
    section[0] = '\0';

    CCINIClass* pArt = CCINIClass::GetArtINI();
    if (pArt == nullptr) {
        return;
    }

    if (pArt->ReadString(pImage, "Sequence", "", section,
                         sizeof(section)) <= 0) {
        return;
    }

    for (int32 seqIndex = 0; seqIndex < g_SequenceCount; ++seqIndex)
    {
        SequenceDataRec& data = SequenceDataAt(seqIndex);

        char value[0x20];
        value[0] = '\0';
        if (pArt->ReadString(section, g_SequenceNames[seqIndex], "", value,
                             sizeof(value)) <= 0) {
            continue;
        }

        // asm: sscanf(value, "%d,%d,%d,%s",
        //             &rec[+00], &rec[+04], &rec[+08], facing)
        char facing[0x20];
        facing[0] = '\0';
        if (std::sscanf(value, "%d,%d,%d,%s", &data.StartFrame,
                        &data.FrameCount, &data.FramesPerDirection,
                        facing) < 4) {
            continue;
        }

        data.Facing = FacingFromName(facing);

        char key[0x40];
        std::sprintf(key, "%sSounds", g_SequenceNames[seqIndex]);

        char sounds[0x20];
        sounds[0] = '\0';
        if (pArt->ReadString(section, key, "", sounds, sizeof(sounds)) <= 0) {
            continue;
        }

        int32 slot = 0;
        char* pToken = std::strtok(sounds, " ,\t");
        while (pToken != nullptr)
        {
            const int32 frame = std::atoi(pToken);

            pToken = std::strtok(nullptr, " ,\t");
            if (pToken == nullptr) {
                break;
            }

            const int32 sound = VocClass::FindIndexOfName(pToken);
            if (sound != -1 && slot < 2)
            {
                // asm: slot 取 rec[+10]（已填槽数），帧号写 +14/+1C、
                //      Voc 索引写 +18/+20；填完 slot++ 回写 +10。
                data.SoundFrame[slot] = frame;
                data.SoundIndex[slot] = sound;
                ++slot;
                data.SoundCount = slot;
            }

            pToken = std::strtok(nullptr, " ,\t");
        }
    }
}

// =============================================================================
// InfantryTypeClass_CreateInfantry
// Allocate an InfantryClass for this type and bind its Type back-pointer.
// Returns nullptr if the allocation failed.
// =============================================================================
InfantryClass* InfantryTypeClass::CreateInfantry(HouseClass* pOwner) const
{
    InfantryClass* pInf = new InfantryClass(pOwner);
    if (pInf) {
        pInf->Type = const_cast<InfantryTypeClass*>(this);
    }
    return pInf;
}

// ============================================================================
// InfantryTypeClass_GetRepairStep (0x524730)
//   mov eax, RulesData ; mov eax, [eax+(RulesClass.CreateInfantrySound+155Ch)]
//   The "IRepairStep" General key, i.e. the infantry repair step.
// ============================================================================
int32 InfantryTypeClass::GetRepairStep() const
{
    return RulesClass::Instance->IRepairStep;
}

// ============================================================================
// InfantryTypeClass_GetRepairStepCost (0x524740)
//   xor eax, eax ; retn
//   Infantry never charge a per-step repair cost.
// ============================================================================
int32 InfantryTypeClass::GetRepairStepCost() const
{
    return 0;
}

// ============================================================================
// InfantryTypeClass_SpawnAtCoords (0x523B40)
//   new InfantryClass(this, Owner) ; pick a free sublocation inside the target
//   cell ; if the sublocation differs from the default one, move the fresh
//   infantry there, otherwise drop it at the cell centre.  Returns the spawned
//   infantry, or nullptr when the allocation failed.
// ============================================================================
InfantryClass* InfantryTypeClass::SpawnAtCoords(const CoordStruct& coord, HouseClass* pOwner)
{
    InfantryClass* pInf = new InfantryClass(pOwner);
    if (pInf == nullptr)
        return nullptr;

    pInf->Type = this;

    if (TheMap == nullptr)
        return pInf;

    // asm: convert the passed cell into leptons and locate the map cell.
    const CellStruct cell = CellStruct(static_cast<int16>(coord.X >> 8),
                                       static_cast<int16>(coord.Y >> 8));
    CellClass* pCell = TheMap->GetCellAt(cell);
    if (pCell == nullptr)
        return pInf;

    CoordStruct sub = coord;
    if (pCell->PickInfantrySublocation(&sub, coord, 0, 0, 0))
    {
        // asm: vtable[0D8h] - place the object at the chosen sublocation.
        pInf->SetLocation(sub);
    }
    else
    {
        // asm: fall back to the cell centre (the default coords path).
        pInf->SetLocation(coord);
    }

    return pInf;
}
