#include <Abstract/AbstractTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Rules/RulesClass.h>
#include <Houses/HouseClass.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// AbstractTypeClass.cpp
//
//  AbstractTypeClass is the base "type" descriptor for every factory-driven
//  entity in Yuri's Revenge - buildings, vehicles, infantry, aircraft,
//  overlays, smudges, terrain, voxel anims, etc.  Each concrete type class
//  inherits from AbstractTypeClass and registers instances in the static
//  Array.  This file expands the .cpp with:
//
//    * Static Array allocation / deallocation (Init / Delete)
//    * Lookup helpers (Find, Find_By_Index, Find_Or_Allocate, Delete_All)
//    * Constructor / destructor behaviour that the header leaves as decls
//    * IPersistStream implementations for the type layer (types are not normally
//      streamed individually - the rules INI is the source of truth)
//    * ComputeCRC + GetCRC for the multiplayer / save checksum
//    * Read_INI / Write_INI delegating to the polymorphic LoadFromINI /
//      SaveToINI hooks so callers can dispatch without RTTI checks
//    * Coordinate_From_INI - parses "X,Y,Z" triples from the INI
//    * Get_Owners_Count / Is_Allowed_For_House - ownership bookkeeping
//    * Get_Cost / Get_Build_Limit - economy-side accessors
//
//  The original binary keeps AbstractTypeClass::Array in the .data segment
//  and lets each subclass register itself on construction.  The
//  reconstructed build preserves the same control flow.
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<AbstractTypeClass*>* AbstractTypeClass::Array = nullptr;

// ============================================================================
// Init_Array
//
//  Allocates the global AbstractTypeClass::Array on the game's memory pool.
//  Called once during engine boot before any type-class instance is
//  constructed.  Idempotent - safe to call multiple times.
// ============================================================================
void AbstractTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<AbstractTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<AbstractTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<AbstractTypeClass*>();
    }
}

// ============================================================================
// Delete_Array
//
//  Tears down the global array.  Every entry still present is left alone
//  (the caller is expected to have already destroyed the objects via
//  Delete_All).  The vector's own heap buffer is released by the destructor.
// ============================================================================
void AbstractTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<AbstractTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Get_Count
//
//  Returns the number of currently-registered AbstractTypeClass instances.
// ============================================================================
int32 AbstractTypeClass::Get_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Find
//
//  Linear search through the array for an instance whose ID matches pID
//  (case-insensitive).  Returns nullptr if the array is uninitialised or
//  the ID is not present.
// ============================================================================
AbstractTypeClass* AbstractTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        AbstractTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

// ============================================================================
// Find_By_Index
//
//  Returns the instance at the supplied array index, or nullptr if the
//  index is out of range or the array has not been initialised.
// ============================================================================
AbstractTypeClass* AbstractTypeClass::Find_By_Index(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// ============================================================================
// Find_Or_Allocate
//
//  Searches for an existing instance with the supplied ID.  If found, the
//  existing instance is returned.  Otherwise a new instance is allocated
//  via GameCreate, registered with the array, and returned.  This mirrors
//  the helper the rules-INI loader uses for every type list it reads.
// ============================================================================
AbstractTypeClass* AbstractTypeClass::Find_Or_Allocate(const char* pID)
{
    if (pID == nullptr || pID[0] == '\0')
        return nullptr;

    AbstractTypeClass* existing = Find(pID);
    if (existing != nullptr)
        return existing;

    Init_Array();

    if (Array == nullptr)
        return nullptr;

    AbstractTypeClass* fresh = GameCreate<AbstractTypeClass>(pID);
    if (fresh == nullptr)
        return nullptr;

    if (!Array->Add(fresh))
    {
        GameDelete(fresh);
        return nullptr;
    }

    fresh->ArrayIndex = Array->Count - 1;
    return fresh;
}

// ============================================================================
// Delete_All
//
//  Destroys every registered AbstractTypeClass and clears the array.  The
//  array itself is preserved so subsequent allocations can reuse it.  This
//  is the bulk teardown path invoked between scenarios.
// ============================================================================
void AbstractTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        AbstractTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Constructor (from ID)
//
//  Copies pID into the ID buffer (truncated to 23 chars + NUL), zeroes the
//  UIName/Name fields, and sets the bookkeeping defaults.  The instance is
//  not registered with the array here - that is the caller's responsibility
//  (Find_Or_Allocate does it; raw construction does not).
// ============================================================================
AbstractTypeClass::AbstractTypeClass(const char* pID) noexcept
    : AbstractClass(noinit)
{
    int32 i = 0;
    if (pID != nullptr)
    {
        while (pID[i] != '\0' && i < 23)
        {
            ID[i] = pID[i];
            ++i;
        }
    }
    ID[i] = '\0';

    zero_3C = 0;

    UINameLabel[0] = '\0';
    UIName[0] = L'\0';
    Name[0] = '\0';

    ArrayIndex = -1;
    TechLevel = -1;
    Cost = 0;

    Create_ID_Internal();
}

// ============================================================================
// Constructor (noinit)
//
//  Used by subclasses that perform their own field initialisation.  Leaves
//  every field in its default state.
// ============================================================================
AbstractTypeClass::AbstractTypeClass(noinit_t) noexcept
    : AbstractClass(noinit)
{
    ID[0] = '\0';
    zero_3C = 0;
    UINameLabel[0] = '\0';
    UIName[0] = L'\0';
    Name[0] = '\0';
    ArrayIndex = -1;
    TechLevel = -1;
    Cost = 0;
}

// ============================================================================
// Destructor
//
//  AbstractTypeClass owns no heap allocations itself - its destructor is
//  virtual so subclass destructors run correctly through a base pointer.
// ============================================================================
AbstractTypeClass::~AbstractTypeClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT AbstractTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Abstract);
    return S_OK;
}

HRESULT AbstractTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    hr = pStm->Read(ID, sizeof(ID), &read);
    if (hr < 0 || read != sizeof(ID)) return E_FAIL;
    ID[sizeof(ID) - 1] = '\0';

    // Read zero_3C
    hr = pStm->Read(&zero_3C, sizeof(zero_3C), &read);
    if (hr < 0 || read != sizeof(zero_3C)) return E_FAIL;

    // Read UINameLabel
    hr = pStm->Read(UINameLabel, sizeof(UINameLabel), &read);
    if (hr < 0 || read != sizeof(UINameLabel)) return E_FAIL;
    UINameLabel[sizeof(UINameLabel) - 1] = '\0';

    // Read UIName
    hr = pStm->Read(UIName, sizeof(UIName), &read);
    if (hr < 0 || read != sizeof(UIName)) return E_FAIL;

    // Read Name
    hr = pStm->Read(Name, sizeof(Name), &read);
    if (hr < 0 || read != sizeof(Name)) return E_FAIL;
    Name[sizeof(Name) - 1] = '\0';

    // Read ArrayIndex
    hr = pStm->Read(&ArrayIndex, sizeof(ArrayIndex), &read);
    if (hr < 0 || read != sizeof(ArrayIndex)) return E_FAIL;

    // Read TechLevel
    hr = pStm->Read(&TechLevel, sizeof(TechLevel), &read);
    if (hr < 0 || read != sizeof(TechLevel)) return E_FAIL;

    // Read Cost
    hr = pStm->Read(&Cost, sizeof(Cost), &read);
    if (hr < 0 || read != sizeof(Cost)) return E_FAIL;

    return S_OK;
}

HRESULT AbstractTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    hr = pStm->Write(&zero_3C, sizeof(zero_3C), &written);
    if (hr < 0 || written != sizeof(zero_3C)) return E_FAIL;

    hr = pStm->Write(UINameLabel, sizeof(UINameLabel), &written);
    if (hr < 0 || written != sizeof(UINameLabel)) return E_FAIL;

    hr = pStm->Write(UIName, sizeof(UIName), &written);
    if (hr < 0 || written != sizeof(UIName)) return E_FAIL;

    hr = pStm->Write(Name, sizeof(Name), &written);
    if (hr < 0 || written != sizeof(Name)) return E_FAIL;

    hr = pStm->Write(&ArrayIndex, sizeof(ArrayIndex), &written);
    if (hr < 0 || written != sizeof(ArrayIndex)) return E_FAIL;

    hr = pStm->Write(&TechLevel, sizeof(TechLevel), &written);
    if (hr < 0 || written != sizeof(TechLevel)) return E_FAIL;

    hr = pStm->Write(&Cost, sizeof(Cost), &written);
    if (hr < 0 || written != sizeof(Cost)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType AbstractTypeClass::WhatAmI() const
{
    return AbstractType::Abstract;
}

int32 AbstractTypeClass::Size() const
{
    return sizeof(AbstractTypeClass);
}

// ============================================================================
// ComputeCRC
//
//  Feeds the common type-class fields into the CRC stream.  Concrete
//  subclasses chain this before adding their own state.
// ============================================================================
void AbstractTypeClass::ComputeCRC(CRCEngine& crc) const
{
    Compute_CRC_Abstract(crc);

    crc.AddData(ID,           static_cast<int32>(sizeof(ID)));
    crc.AddData(&zero_3C,     sizeof(zero_3C));
    crc.AddData(UINameLabel,  static_cast<int32>(sizeof(UINameLabel)));
    crc.AddData(UIName,       static_cast<int32>(sizeof(UIName)));
    crc.AddData(Name,         static_cast<int32>(sizeof(Name)));
    crc.AddData(&ArrayIndex,  sizeof(ArrayIndex));
    crc.AddData(&TechLevel,   sizeof(TechLevel));
    crc.AddData(&Cost,        sizeof(Cost));
}

// ============================================================================
// GetCRC
//
//  Convenience wrapper that runs ComputeCRC against a fresh engine and
//  returns the resulting 32-bit digest.  Used by the multiplayer resync
//  code which needs a flat integer rather than a streaming hash.
// ============================================================================
int32 AbstractTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// LoadTheaterSpecificArt
//
//  Reloads theater-specific art assets (SHP shapes, VXL models, palettes)
//  for the given theater type.  The base AbstractTypeClass owns no art data,
//  so this implementation validates the theater parameter and returns without
//  loading anything.  Concrete subclasses override this:
//
//    BuildingTypeClass  - reloads the building SHP with the theater palette
//    TerrainTypeClass   - reloads terrain art (trees, rocks) per theater
//    OverlayTypeClass   - reloads overlay art (bridges, walls) per theater
//
//  The theater type values map to the TheaterType enum (Temperate, Snow,
//  Urban, Desert, Lunar, NewUrban).  A value of -1 indicates "no theater"
//  and causes an early return.
// ============================================================================
void AbstractTypeClass::LoadTheaterSpecificArt(int32 th_type)
{
    // Validate the theater parameter.  Invalid or "none" theaters are a
    // no-op for the base class (and for subclasses that lack theater art).
    if (th_type < 0)
        return;

    // AbstractTypeClass has no Shape / VXL / palette fields to reload.
    // The IsTheater flag on ObjectTypeClass controls whether a subclass
    // needs theater-specific art; the base class has no such flag.
}

// ============================================================================
// LoadFromINI
//
//  Reads the type fields common to every AbstractTypeClass descendant from
//  the rules-INI block whose name matches the type's ID.  These are the
//  fields owned by this class: Name, UIName (label), TechLevel and Cost.
//  Concrete subclasses (ObjectTypeClass, TechnoTypeClass, ...) chain this
//  before reading their own fields.
//
//  Returns true if the INI section for this type's ID exists (i.e. the type
//  has a rules block); false if the INI is null, the ID is empty, or the
//  section is absent.
// ============================================================================
bool AbstractTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // If the type has no INI section there is nothing to read.
    if (!pINI->SectionExists(section))
        return false;

    // ------------------------------------------------------------------
    // UI name label - the string-table reference (e.g. "NAME:GAPOWR").
    // Stored in UINameLabel; the Unicode UIName is resolved from the
    // string table at runtime, not from the INI directly.
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
    // Display name - human-readable fallback when UIName is empty.
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
    // Tech level - determines when the type becomes buildable.
    // -1 (the default) means "not buildable".  Passing the current value
    // as the default preserves it when the key is absent.
    // ------------------------------------------------------------------
    TechLevel = pINI->ReadInteger(section, "TechLevel", TechLevel);

    // ------------------------------------------------------------------
    // Cost - build cost in credits.
    // ------------------------------------------------------------------
    Cost = pINI->ReadInteger(section, "Cost", Cost);

    return true;
}

// ============================================================================
// SaveToINI
//
//  Writes the common type fields back to the INI under the section named
//  after the type's ID.  Used by the map editor when saving custom
//  rulesets.  Returns true on success; false if the INI is null or the ID
//  is empty.
// ============================================================================
bool AbstractTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    if (UINameLabel[0] != '\0')
        pINI->WriteString(section, "UIName", UINameLabel);
    if (Name[0] != '\0')
        pINI->WriteString(section, "Name", Name);

    pINI->WriteInteger(section, "TechLevel", TechLevel);
    pINI->WriteInteger(section, "Cost", Cost);

    return true;
}

// ============================================================================
// Read_INI / Write_INI
//
//  AbstractClass declares these as virtual no-op bases.  AbstractTypeClass
//  overrides them to delegate to the polymorphic LoadFromINI / SaveToINI
//  hooks so that callers can dispatch through the base interface without
//  RTTI checks.
// ============================================================================
bool AbstractTypeClass::Read_INI(CCINIClass* pINI)
{
    return LoadFromINI(pINI);
}

bool AbstractTypeClass::Write_INI(CCINIClass* pINI) const
{
    if (pINI == nullptr)
        return false;
    // const-cast is safe here - SaveToINI is logically const even though
    // the virtual signature takes a non-const INI handle.
    return const_cast<AbstractTypeClass*>(this)->SaveToINI(pINI);
}

// ============================================================================
// get_ID / get_Name
//
//  ID is the rules-INI identifier (e.g. "GAPOWR"); Name is the human-
//  readable label populated by the UI-name lookup.  get_ID is hot-path
//  code (called by Find), get_Name is used by the sidebar / tooltips.
// ============================================================================
const char* AbstractTypeClass::get_ID() const
{
    return this->ID;
}

const char* AbstractTypeClass::get_Name() const
{
    if (this->Name[0] != '\0')
        return this->Name;
    return this->ID;
}

// ============================================================================
// Coordinate_From_INI
//
//  Parses an "X,Y,Z" triple from the supplied INI key.  Falls back to
//  defaultCoord if the key is missing or malformed.  Used by the scenario
//  loader for waypoint / drop-zone coordinates.
// ============================================================================
CoordStruct AbstractTypeClass::Coordinate_From_INI(CCINIClass* pINI,
                                                   const char* pSection,
                                                   const char* pKey,
                                                   const CoordStruct& defaultCoord)
{
    if (pINI == nullptr || pSection == nullptr || pKey == nullptr)
        return defaultCoord;

    int32 values[3] = { defaultCoord.X, defaultCoord.Y, defaultCoord.Z };
    pINI->Read3Integers(values, pSection, pKey, values);
    return CoordStruct(values[0], values[1], values[2]);
}

// ============================================================================
// Get_Owners_Count
//
//  Returns the number of houses that are permitted to build this type.  In
//  the original binary this is driven by the Owner= / RequiredHouses= /
//  ForbiddenHouses= INI keys; the standalone build scans the HouseTypeClass
//  array.  Here we approximate by returning the side count from RulesClass
//  if available, otherwise 0.
// ============================================================================
int32 AbstractTypeClass::Get_Owners_Count() const
{
    // The full implementation walks HouseTypeClass::Array and counts how
    // many houses pass Is_Allowed_For_House.  Without a populated house
    // list we return 0 to indicate "no explicit owners".
    if (RulesClass::Instance == nullptr)
        return 0;

    int32 count = 0;
    // Walk the AbstractClass array looking for HouseTypeClass instances.
    // The full binary keeps a dedicated HouseTypeClass::Array; here we
    // approximate by counting houses through the abstract registry.
    const DynamicVectorClass<AbstractClass*>* absArray = AbstractClass::Get_Array_Ptr();
    if (absArray == nullptr)
        return 0;

    for (int32 i = 0; i < absArray->Count; ++i)
    {
        AbstractClass* item = absArray->Items[i];
        if (item == nullptr)
            continue;
        if (item->WhatAmI() == AbstractType::HouseType)
        {
            // Treat the house type as a candidate owner.  The actual
            // ownership test is performed by Is_Allowed_For_House against
            // concrete HouseClass instances.
            ++count;
        }
    }
    return count;
}

// ============================================================================
// Is_Allowed_For_House
//
//  Returns true if the supplied house is allowed to build / own instances
//  of this type.  The full binary consults the RequiredHouses / Forbidden-
//  Houses bitfields; the standalone build defaults to "allowed" so that
//  scenario placement works without an explicit owner list.
// ============================================================================
bool AbstractTypeClass::Is_Allowed_For_House(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;

    // The base implementation is permissive.  Concrete subclasses
    // (TechnoTypeClass in particular) override this to consult the
    // Owner= / RequiredHouses= / ForbiddenHouses= lists.
    return true;
}

// ============================================================================
// Get_Build_Limit
//
//  Returns the maximum number of instances of this type that a single
//  house may concurrently own.  A return of -1 means "no limit".
//
//  Header check: AbstractTypeClass does NOT declare a BuildLimit field.
//  The BuildLimit member is introduced on the derived ObjectTypeClass
//  (see ObjectTypeClass.h).  Because the base has no limit state to
//  consult, it returns -1 to signal "unlimited".
//
//  Note: ObjectTypeClass currently does not override this accessor, so
//  the -1 fallback applies even for object types that carry a BuildLimit
//  field.  In the original binary ObjectTypeClass overrides Get_Build_Limit
//  to return its BuildLimit member; the reconstructed build leaves that
//  override to a future expansion and relies on the base -1 here.
// ============================================================================
int32 AbstractTypeClass::Get_Build_Limit() const
{
    // AbstractTypeClass does not declare a BuildLimit field; the member is
    // introduced on the derived ObjectTypeClass.  Because -fno-rtti is
    // enabled we cannot dynamic_cast to ObjectTypeClass to read its
    // BuildLimit, so the base returns -1 ("no limit") as the conservative
    // default.  ObjectTypeClass overrides this to return its BuildLimit
    // member, which is initialized to -1 and populated from the INI
    // BuildLimit= key.
    return -1;
}
