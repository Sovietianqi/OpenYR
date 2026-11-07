#include <Abstract/SmudgeTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>
#include <Game/Externs.h>
#include <Abstract/SmudgeClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>

#include <cstring>
#include <cstdio>

// ============================================================================
// SmudgeTypeClass.cpp
//
//  SmudgeTypeClass describes a kind of smudge (scorch mark, crater, bib, etc.)
//  loaded from rules/art INI files.  It inherits ObjectTypeClass and adds:
//
//    * Cell dimensions (width/height in cells)
//    * Frame count and animation flag
//    * Crater / scorch / bib classification
//    * Flat rendering flag (drawn beneath units)
//    * Crater chain parameters (ChainCount / ChainSteps)
//    * Art name (SHP file reference)
//
//  This file implements:
//    * Static Array plumbing (Init_Array / Delete_Array / Delete_All /
//      Find / FindByIndex / GetCount)
//    * Constructor / destructor
//    * LoadFromINI / SaveToINI
//    * ComputeCRC / GetCRC
//    * Get_Size / Is_Crater / Is_Scorch
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<SmudgeTypeClass*>* SmudgeTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void SmudgeTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<SmudgeTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<SmudgeTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<SmudgeTypeClass*>();
    }
}

// asm: InitList_2736 (asm:2) —— 清零 data/count/length。
// asm: InitList_2737 (asm:11) —— capacity/increment 均置 0x80。
void SmudgeTypeClass::sctor()
{
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<SmudgeTypeClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<SmudgeTypeClass*>)));
        if (Array != nullptr)
        {
            new (Array) DynamicVectorClass<SmudgeTypeClass*>();
            Array->Capacity = 0x80;               // asm: mov dword_B0B7D0, 80h
        }
    }
}

// asm: InitList_2736 的对称析构。
void SmudgeTypeClass::sdtor()
{
    Delete_Array();
}

// asm: SmudgeTypeClass_LoadFromINIList (asm:204)
//   遍历全部污渍类型；对剧场类型（[+22Ch] != 0）按 "<ID>.<theater ext>"
//   解析 SHP 到 ImageSHP（+0xA4）。非剧场类型跳过。
void SmudgeTypeClass::LoadFromINIList(int32 idxTheater)
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        SmudgeTypeClass* pType = Array->Items[i];
        if (pType == nullptr)
            continue;

        // asm: mov al,[esi+22Ch] / test al,al / jz loc_6B552B
        if (!pType->IsTheater)
            continue;

        const char* pBase = (pType->Image[0] != '\0') ? pType->Image : pType->ID;
        char base[0x100];
        char full[0x100];
        snprintf(base, sizeof(base), "%s.SHP", pBase);
        pType->TheaterSpecificID(base, idxTheater, full);

        // asm: CCFileClass + Read_File 解出 SHP；文件后端未接通，保持 null。
        pType->ImageSHP = nullptr;
    }
}

// asm: 类型层 SHP 访问面 —— 返回缓存的 SHP 句柄（ObjectTypeClass::GetImage）。
SHPStruct* SmudgeTypeClass::GetImageData() const
{
    return GetImage();
}

void SmudgeTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<SmudgeTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void SmudgeTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        SmudgeTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

SmudgeTypeClass* SmudgeTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        SmudgeTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

SmudgeTypeClass* SmudgeTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// asm: SmudgeType_FindIndexByName (0x6B5440)
//
//   Walks vec_SmudgeTypes comparing each type's ID string (+0x24) with the
//   supplied name via _strcmpi and returns the matching ordinal, or -1.
int32 SmudgeTypeClass::FindIndexByName(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return -1;                                 // asm: or eax,0FFFFFFFFh

    for (int32 i = 0; i < Array->Count; ++i)
    {
        SmudgeTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))              // asm: call __strcmpi
            return i;
    }
    return -1;
}

SmudgeTypeClass* SmudgeTypeClass::FindByName(const char* pID)
{
    return FindByIndex(FindIndexByName(pID));
}

int32 SmudgeTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Constructor
//
// asm: SmudgeTypeClass_CTOR (0x6B52A0, __thiscall(this, char* ID), retn 4)
//   ObjectTypeClass_CTOR(this, ID)
//   [esi+294h] = -1                            ; registry ordinal
//   [esi+298h] = 1   (Width)
//   [esi+29Ch] = 1   (Height)
//   [esi+2A0h] = 0   (Crater)   [esi+2A1h] = 0 (Burn)
//   ObjectTypeClass flags: RadarInvisible=1, Selectable=0, LegalTarget=0,
//                          Insignificant=1, Immune=1
//   register into vec_SmudgeTypes -> [esi+294h] = own ordinal (else -1)
// ============================================================================

SmudgeTypeClass::SmudgeTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    HeapID      = -1;
    CellSize    = CellStruct(1, 1);
    Frames      = 0;
    Width       = 1;      // asm: [esi+298h] = 1
    Height      = 1;      // asm: [esi+29Ch] = 1
    IsCrater    = false;  // asm: [esi+2A0h] = 0
    IsScorch    = false;
    IsBib       = false;
    IsAnimated  = false;
    IsFlat      = true;
    IsBurn      = false;  // asm: [esi+2A1h] = 0
    ChainCount  = 0;
    ChainSteps  = 0;

    // asm: the type is its own map symbol / invariant target.
    RadarInvisible = true;
    Selectable     = false;
    LegalTarget    = false;
    Insignificant  = true;
    Immune         = true;

    std::memset(ArtName, 0, sizeof(ArtName));

    // asm: append to vec_SmudgeTypes, then stamp the ordinal into +294h.
    if (Array == nullptr)
        Init_Array();
    if (Array != nullptr)
    {
        Array->Add(this);
        for (int32 i = 0; i < Array->Count; ++i)
        {
            if (Array->Items[i] == this) { HeapID = i; break; }
        }
    }
}

// ============================================================================
// Destructor
// ============================================================================

SmudgeTypeClass::~SmudgeTypeClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT SmudgeTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::SmudgeType);
    return S_OK;
}

// asm: SmudgeTypeClass_LoadFromINI (asm:409, retn 4)
//   Reset(INI) ; ObjectTypeClass_LoadFromINI ; test al,al / jz -> false
//   键序（逐字）：Crater, Burn, Width, Height
//   然后按剧场/非剧场解析 "<Image>.SHP" 到 ImageSHP (+0xA4)。
bool SmudgeTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // asm: ObjectTypeClass_LoadFromINI ; test al,al / jz loc_6B57DA -> false
    if (!ObjectTypeClass::LoadFromINI(pINI))
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // asm: 键序 Crater / Burn / Width / Height。
    IsCrater = pINI->ReadBool(section, "Crater", IsCrater);       // "Crater"
    IsBurn   = pINI->ReadBool(section, "Burn",   IsBurn);         // "Burn"
    Width    = pINI->ReadInteger(section, "Width",  Width);       // "Width"
    Height   = pINI->ReadInteger(section, "Height", Height);      // "Height"

    // asm: 非剧场 -> "<Image>.SHP"；剧场 -> "<Image>.<theater ext>" -> Finder。
    const char* pBase = (Image[0] != '\0') ? Image : section;
    char base[0x100];
    char full[0x100];
    snprintf(base, sizeof(base), "%s.SHP", pBase);
    const int32 idxTheater = (TheScenario != nullptr)
                             ? static_cast<int32>(TheScenario->Theater) : 0;
    TheaterSpecificID(base, idxTheater, full);

    // asm: CCFileClass/Read_File 解出 SHP；本构建的文件后端未接通，句柄保持 null。
    ImageSHP = nullptr;

    return true;
}

// asm: SmudgeTypeClass_Load (asm:585, retn 8)
//   ObjectTypeClass_Load ; 失败返回
//   重打虚表 + ObjectTypeClass_Load3DArt + Load_Theater_Art（无自有字段）。
HRESULT SmudgeTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0)
        return hr;                            // asm: jl loc_6B589F

    ObjectTypeClass::Load3DArt(0);            // asm: ObjectTypeClass_Load3DArt
    ObjectTypeClass::Load_Theater_Art(0);     // asm: ObjectTypeClass_Load_Theater_Art

    return S_OK;                              // asm: xor eax,eax
}

// asm: SmudgeTypeClass_Save (asm:639, retn 0Ch) —— 纯转发 ObjectTypeClass_Save。
HRESULT SmudgeTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    return ObjectTypeClass::Save(pStm, fClearDirty);
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType SmudgeTypeClass::WhatAmI() const
{
    return AbstractType::SmudgeType;
}

int32 SmudgeTypeClass::Size() const
{
    // asm: SmudgeTypeClass_GetSize -- "mov eax, 2A4h ; retn".
    return 0x2A4;
}

// asm: SmudgeTypeClass_GetArrayIndex -- "mov eax, [ecx+294h] ; retn".
int32 SmudgeTypeClass::GetArrayIndex() const
{
    return HeapID;
}

// asm: SmudgeTypeClass_GetAbstractDerivationID -- "mov eax, 1Eh ; retn".
int32 SmudgeTypeClass::GetAbstractDerivationID() const
{
    return 0x1E;
}

// ============================================================================
// Classification helpers
// ============================================================================

CellStruct SmudgeTypeClass::Get_Size() const
{
    return CellSize;
}

bool SmudgeTypeClass::Is_Crater() const
{
    return IsCrater;
}

bool SmudgeTypeClass::Is_Scorch() const
{
    return IsScorch;
}

// ============================================================================
// SaveToINI
// ============================================================================

bool SmudgeTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // Chain the parent.
    ObjectTypeClass::SaveToINI(pINI);

    pINI->WriteBool(section, "Crater",   IsCrater);
    pINI->WriteBool(section, "Scorch",   IsScorch);
    pINI->WriteBool(section, "Bib",      IsBib);
    pINI->WriteBool(section, "Animated", IsAnimated);
    pINI->WriteBool(section, "Flat",     IsFlat);

    int32 sizeVals[2] = { CellSize.X, CellSize.Y };
    pINI->Write2Integers(section, "Size", sizeVals);

    pINI->WriteInteger(section, "Frames",     Frames);
    pINI->WriteInteger(section, "ChainCount", ChainCount);
    pINI->WriteInteger(section, "ChainSteps", ChainSteps);

    if (ArtName[0] != '\0')
        pINI->WriteString(section, "Image", ArtName);

    return true;
}

// ============================================================================
// CRC
// ============================================================================

// asm: SmudgeTypeClass_GetCRC (asm:526)
//   AbstractTypeClass_GetCRC(buf)
//   CRC_DWORD(HeapID) ; CRC_bool(Crater) ; CRC_bool(Burn)
//   CRC_DWORD(Width)  ; CRC_DWORD(Height)
void SmudgeTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&HeapID,   sizeof(HeapID));   // asm: [esi+294h]
    crc.AddData(&IsCrater, sizeof(IsCrater)); // asm: [esi+2A0h]
    crc.AddData(&IsBurn,   sizeof(IsBurn));   // asm: [esi+2A1h]
    crc.AddData(&Width,    sizeof(Width));    // asm: [esi+298h]
    crc.AddData(&Height,   sizeof(Height));   // asm: [esi+29Ch]
}

int32 SmudgeTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}


// ============================================================================
// SmudgeTypeClass - static lookup helpers
// ============================================================================
SmudgeTypeClass* SmudgeTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    SmudgeTypeClass* found = Find(pID);
    if (found) return found;
    if (!Array) Init_Array();
    if (!Array) return nullptr;
    SmudgeTypeClass* newItem = GameCreate<SmudgeTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

// ============================================================================
// SmudgeTypeClass_SpawnSmudgeAt  (asm: SmudgeTypeClass_SpawnSmudgeAt, 0x6B5540)
//
//   push size OverlayClass          ; SmudgeClass shares the Overlay allocation
//   call new
//   test eax, eax
//   jz   fail
//   movsx ecx, word ptr [edx]       ; cell.X
//   shl  ecx, 8
//   add  ecx, 80h                   ; X = cell.X*256 + 0x80 (cell center)
//   movsx edx, word ptr [edx+2]     ; cell.Y
//   shl  edx, 8
//   add  edx, 80h                   ; Y = cell.Y*256 + 0x80
//   push -1                         ; owner = none
//   lea  ecx, [coord]
//   call SmudgeClass_CTOR
//   test eax, eax
//   jz   fail
//   mov al, 1
//
// The original stores the pixel-space location inside the instance; in this
// translation the location lives in the per-cell Smudge slot, so the instance
// is created, stamped with this type, and registered on the target cell.
// Returns false only when allocation fails.
// ============================================================================
bool SmudgeTypeClass::SpawnSmudgeAt(CellStruct* pCell)
{
    if (pCell == nullptr)
        return false;                     // asm would deref arg_0 unconditionally

    // asm: call new ; mov ecx, eax ; call SmudgeClass_CTOR(..., -1)
    SmudgeClass* pSmudge = GameCreateArgs<SmudgeClass>(static_cast<HouseClass*>(nullptr));
    if (pSmudge == nullptr)
        return false;                     // asm: jz loc_6B55B0 -> xor al,al

    pSmudge->Type = this;                 // ctor argument #1 in the original
    pSmudge->IsActive = true;

    // Bridge to this codebase's placement model: stamp the type ordinal into
    // the target cell's smudge slot.  The pixel-space coordinate computation
    // (cell*256 + 0x80) is performed by the rendering side here.
    if (MapClass::Instance != nullptr) {
        CellClass* pTargetCell = MapClass::Instance->GetCellAt(*pCell);
        if (pTargetCell != nullptr) {
            int32 typeIdx = -1;
            for (int32 i = 0; i < GetCount(); ++i) {   // locate own registry index
                if (FindByIndex(i) == this) { typeIdx = i; break; }
            }
            pTargetCell->Set_Smudge(typeIdx);
        }
    }

    return true;                          // asm: mov al, 1
}

// ============================================================================
// asm: SmudgeTypeClass_CreateSmudge (0x6B55C0) __thiscall(this) retn 4
//
//   分配一个"裸"SmudgeClass：锚点取共享的 {0,0,0} 提示块，owner = -1。
//   与 SpawnSmudgeAt 的区别在于**不做格子写入**——只建对象，位置由调用方
//   随后通过 Unlimbo/SetCoords 指定。
// ============================================================================
SmudgeClass* SmudgeTypeClass::CreateSmudge()
{
    // asm: call new ; mov ecx,eax ; call SmudgeClass_CTOR(..., -1)
    SmudgeClass* pSmudge = GameCreateArgs<SmudgeClass>(static_cast<HouseClass*>(nullptr));
    if (pSmudge == nullptr)
        return nullptr;                   // asm: 分配失败 → 返回 0

    pSmudge->Type     = this;             // ctor 的第 1 个实参
    pSmudge->IsActive = true;

    // asm 的共享 {0,0,0} 提示块：锚点坐标保持原点。
    CoordStruct origin{0, 0, 0};
    pSmudge->SetLocation(origin);

    return pSmudge;                       // asm: mov eax, esi (对象指针)
}
