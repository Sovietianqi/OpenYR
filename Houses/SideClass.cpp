#include "SideClass.h"
#include "HouseTypeClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

// ============================================================================
// SideClass - vec_Sides and the [Sides] reader (asm 0x6723BE)
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
// SideClass_CTOR (asm 0x6A45DA)
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

SideClass::~SideClass()
{
    if (Sides == nullptr)
        return;

    // The original detaches the side, then compacts the vector so the
    // ordinals of the entries behind it shift down by one.
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
// Side_From_Name (asm 0x6A46D6)
//
//   A case-insensitive linear scan over vec_Sides comparing each entry's
//   name field at +0x24.  Answers -1 when nothing matches.
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
// ParseHouses - INIClass_ParseSideHouses (asm 0x476800)
//
//   The side's value string is read with an empty fallback.  When it is
//   empty the whole call is a no-op and the vector stays as it was.  The
//   string is split on ','; every non-empty token goes through
//   HouseTypeClass::From_Name and, on a hit, the house's ordinal is appended
//   to the vector.  A token that resolves to nothing is skipped rather than
//   terminating the walk.
// ============================================================================
void SideClass::ParseHouses(CCINIClass* pINI, const char* pSection,
                            const char* pKey)
{
    if (pINI == nullptr || pSection == nullptr || pKey == nullptr)
        return;

    char buffer[0x80];
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, pKey, "", buffer, sizeof(buffer)) <= 0)
        return;

    Houses.Clear();

    char* pToken = std::strtok(buffer, ",");
    while (pToken != nullptr) {
        if (pToken[0] == '\0') {
            pToken = std::strtok(nullptr, ",");
            continue;
        }

        const int32 idxHouse = HouseTypeClass::FindIndex(pToken);
        if (idxHouse != -1)
            Houses.Add(idxHouse);

        pToken = std::strtok(nullptr, ",");
    }
}

// ============================================================================
// ReadSides - RulesClass_Addition_Sides (asm 0x6723BE)
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
            if (pHouse != nullptr)
                pHouse->Side = idxSide;
        }
    }
}
