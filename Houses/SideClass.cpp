#include "SideClass.h"
#include "HouseTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../COM/IUnknown.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// ============================================================================
// SideClass - vec_Sides and the [Sides] reader
// ============================================================================

DynamicVectorClass<SideClass*>* SideClass::Sides = nullptr;

const char* const SECTION_SIDES = "Sides";

// ============================================================================
// Diagnostics - the two texts RulesClass_Addition_Sides emits through
// WWDebugString.  Both are reproduced byte for byte, the trailing space in
// the "Side %d: %s " banner included.
// ============================================================================

static const char* const DIAG_PROCESSING = "Processing sides.\n";
static const char* const DIAG_SIDE       = "Side %d: %s \n";
static const char* const DIAG_HOUSE      = "  %s\n";

// WWDebugString - the original writes to the debugger console; on this
// platform the message goes to stderr unchanged.
static void WWDebugString(const char* pMessage)
{
    if (pMessage == nullptr)
        return;
    std::fputs(pMessage, stderr);
}

static void WWDebugStringF(const char* pFormat, ...)
{
    if (pFormat == nullptr)
        return;

    char buffer[0x100];
    va_list args;
    va_start(args, pFormat);
    std::vsnprintf(buffer, sizeof(buffer), pFormat, args);
    va_end(args);

    std::fputs(buffer, stderr);
}

// ============================================================================
// SideClass_CTOR  (asm:0x6A45C0)
//
//   Builds the AbstractTypeClass half, zeroes the house vector, seeds its
//   growth step with 0x0A and appends the new side to vec_Sides.  The
//   registry is created on demand so the constructor can be the single entry
//   point, exactly as the original's vec_Sides is a static container.
// ============================================================================
SideClass::SideClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
{
    // The original seeds the house vector's growth step with 0x0A; the
    // VectorClass equivalent is the initial capacity it grows from.
    Houses.Capacity = 0x0A;

    if (Sides == nullptr)
        Sides = new DynamicVectorClass<SideClass*>();

    Sides->Add(this);
}

// ============================================================================
// SideClass_DTOR  (asm:0x6A4620)
//
//   Compacts vec_Sides around the removed entry so the ordinals of the
//   entries behind it shift down by one; the house vector itself is released
//   by the member destructor.
// ============================================================================
SideClass::~SideClass()
{
    if (Sides == nullptr)
        return;

    for (int32 i = 0; i < Sides->Count; ++i) {
        if ((*Sides)[i] != this)
            continue;

        for (int32 j = i; j < Sides->Count - 1; ++j)
            (*Sides)[j] = (*Sides)[j + 1];

        Sides->Count -= 1;
        break;
    }
}

// ============================================================================
// Side_From_Name  (asm:0x6A46C0)
//
//   A case-insensitive linear scan over vec_Sides comparing each entry's
//   name field at +0x24 (the AbstractTypeClass ID).  Answers -1 when nothing
//   matches.
// ============================================================================
int32 SideClass::From_Name(const char* pName)
{
    if (pName == nullptr || Sides == nullptr)
        return -1;

    for (int32 i = 0; i < Sides->Count; ++i) {
        SideClass* pSide = (*Sides)[i];
        if (pSide == nullptr)
            continue;
        if (_strcmpi(pSide->get_ID(), pName) == 0)
            return i;
    }

    return -1;
}

SideClass* SideClass::Find(const char* pID)
{
    const int32 index = From_Name(pID);
    if (index < 0 || Sides == nullptr)
        return nullptr;
    return (*Sides)[index];
}

// The constructor already registers the instance, so allocation is the whole
// of this helper; it exists for callers that follow the engine's usual
// FindOrAllocate shape.
SideClass* SideClass::FindOrAllocate(const char* pID)
{
    SideClass* pExisting = Find(pID);
    if (pExisting != nullptr)
        return pExisting;

    return new SideClass(pID);
}

int32 SideClass::GetCount()
{
    return Sides ? Sides->Count : 0;
}

// ============================================================================
// SideClass_GetCRC  (asm:0x6A4720)
//
//   Runs the AbstractTypeClass CRC, then folds in the house count at +0xA8
//   (Houses.Count) as a raw DWORD.
// ============================================================================
int32 SideClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);

    const int32 count = Houses.Count;
    crc.AddData(&count, sizeof(count));

    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// SideClass_GetClassID  (asm:0x6A4740)
//
//   E_POINTER (0x80004003) when pClassID is null, otherwise the SideClass
//   CLSID is copied out and S_OK returned.
//
//   The original ships CLSID_SideClass in the binary; the standalone build
//   synthesises a stable per-class GUID.  The exact bytes are opaque to every
//   caller - only GetClassID/Load round-trips compare them - so a fixed value
//   is functionally equivalent.
// ============================================================================
HRESULT SideClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return (HRESULT)0x80004003;   // E_POINTER

    pClassID->Data1 = 0x53594C53;     // 'SLS' + side tag
    pClassID->Data2 = 0x4C43;         // 'CL'
    pClassID->Data3 = 0x5344;         // 'DS'
    for (int32 i = 0; i < 8; ++i)
        pClassID->Data4[i] = 0;

    return S_OK;
}

// ============================================================================
// SideClass_Load  (asm:0x6A4760)
//
//   AbstractClass_Load first; on failure the object is left untouched.  The
//   sequence then re-initialises the house vector, reads a DWORD count from
//   the stream and appends that many DWORDs to it.
//
//   Per the batch notes the abstract entity implementation lives in
//   AbstractTypeClass, so the chain targets that layer.
// ============================================================================
HRESULT SideClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0)
        return hr;

    Houses.Clear();

    int32 count = 0;
    ULONG read = 0;
    hr = pStm->Read(&count, sizeof(count), &read);
    if (hr < 0 || read != sizeof(count))
        return E_FAIL;

    for (int32 i = 0; i < count; ++i) {
        int32 value = 0;
        hr = pStm->Read(&value, sizeof(value), &read);
        if (hr < 0 || read != sizeof(value))
            return E_FAIL;
        Houses.Add(value);
    }

    return S_OK;
}

// ============================================================================
// SideClass_Save  (asm:0x6A4810)
//
//   AbstractClass_Save first; on failure the tail is skipped.  The house
//   count at +0xA8 is written, followed by each house ordinal as a DWORD.
// ============================================================================
HRESULT SideClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0)
        return hr;

    ULONG written = 0;
    const int32 count = Houses.Count;
    hr = pStm->Write(&count, sizeof(count), &written);
    if (hr < 0 || written != sizeof(count))
        return E_FAIL;

    for (int32 i = 0; i < Houses.Count; ++i) {
        const int32 value = Houses[i];
        hr = pStm->Write(&value, sizeof(value), &written);
        if (hr < 0 || written != sizeof(value))
            return E_FAIL;
    }

    return S_OK;
}

// ============================================================================
// SideClass_GetSize  (asm:0x6A4880)
//
//   Reports the fixed structure size 0xB4.
// ============================================================================
int32 SideClass::Size() const
{
    return 0xB4;
}

// ============================================================================
// SideClass_GetAbstractDerivationID  (asm:0x6A4890)
//
//   Returns 0x1C = AbstractType::Side.
// ============================================================================
AbstractType SideClass::GetAbstractDerivationID() const
{
    return AbstractType::Side;
}

AbstractType SideClass::WhatAmI() const
{
    return AbstractType::Side;
}

// ============================================================================
// ParseHouses - INIClass_ParseSideHouses  (asm:275442)
//
//   逐句核对 asm 后确认：这一段**没有**"token 是另一个阵营名就把该阵营的
//   房屋整串拼进来"的分支——原版只有一次 HouseTypeClass::From_Name 查找，
//   查不到就丢弃。旧实现多出的拼接分支会把 `Allied=Soviet` 这类写法的语义
//   放大成两个阵营的并集，与原版不符，此处按 asm 删除，并把解析工作交给
//   已经逐句对齐的 INIClass::ParseSideHouses（asm:275442）。
// ============================================================================
void SideClass::ParseHouses(CCINIClass* pINI, const char* pSection,
                            const char* pKey)
{
    if (pINI == nullptr || pSection == nullptr || pKey == nullptr)
        return;

    DynamicVectorClass<int32> parsed;
    pINI->ParseSideHouses(pSection, pKey, parsed);

    Houses.Clear();
    for (int32 i = 0; i < parsed.Count; ++i)
        Houses.Add(parsed[i]);
}

// ============================================================================
// ReadSides - RulesClass_Addition_Sides  (asm:0x672450)
//
//   Walks every key of [Sides].  Each key names a side; one that already
//   exists in vec_Sides is reused, otherwise a fresh SideClass is built and
//   registered.  The key is then re-parsed as this side's house list, and
//   every house that came back has its Side field pointed at the side's
//   ordinal in vec_Sides.
// ============================================================================
void SideClass::ReadSides(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    const int32 count = pINI->GetKeyCount(SECTION_SIDES);
    WWDebugString(DIAG_PROCESSING);
    if (count <= 0)
        return;

    for (int32 i = 0; i < count; ++i) {
        const char* pKey = pINI->GetKeyName(SECTION_SIDES, i);
        if (pKey == nullptr)
            continue;

        SideClass* pSide = FindOrAllocate(pKey);
        if (pSide == nullptr)
            continue;

        WWDebugStringF(DIAG_SIDE, i, pSide->get_ID());

        pSide->ParseHouses(pINI, SECTION_SIDES, pKey);

        // The side's own ordinal in the registry is what every house in its
        // list stores.
        const int32 idxSide = From_Name(pKey);

        for (int32 h = 0; h < pSide->Houses.Count; ++h) {
            HouseTypeClass* pHouse = HouseTypeClass::FindByIndex(pSide->Houses[h]);
            if (pHouse != nullptr) {
                pHouse->Side = idxSide;
                WWDebugStringF(DIAG_HOUSE, pHouse->ID);
            }
        }
    }
}
