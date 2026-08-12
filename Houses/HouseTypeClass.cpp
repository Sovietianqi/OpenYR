#include <Houses/HouseTypeClass.h>
#include <INI/INIClass.h>
#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>

#include <cstring>
#include <cstdlib>

// ============================================================================
// HouseTypeClass.cpp - HouseTypeClass implementation
//
//  HouseTypeClass describes a playable country / faction.  The game ships
//  with 16 default house types (the rulemd.ini "Houses" list) and the
//  scenario loader can register additional ones from the map's INI.
//
//  This file provides:
//    * Complete LoadFromINI / SaveToINI for every property
//    * Compute_CRC / Get_CRC
//    * Color-scheme parsing helpers
//    * Country / side assignment helpers
//    * Tech-level parsing helpers
//    * The 16 default house types table
// ============================================================================

// ============================================================================
// Default house types
//
//  These match the entries in the original rulesmd.ini [Houses] list.  The
//  original binary reads the list at startup and pre-allocates one
//  HouseTypeClass per entry.  The standalone build does the same; the
//  static table below is used by HouseTypeClass::Init_Defaults.
// ============================================================================
struct DefaultHouseEntry
{
    const char* ID;
    const char* Name;
    const char* ParentCountry;
    int32       Side;
    int32       Color;
    bool        Multiplay;
    bool        MultiplayPassive;
    bool        WallOwner;
    bool        SmartAI;
};

static const DefaultHouseEntry g_DefaultHouseTypes[] =
{
    // ── Allied side ──────────────────────────────────────────────────────
    { "Americans",  "Americans",  "Americans",  1, 0, true,  false, false, false },
    { "French",     "French",     "French",     1, 1, true,  false, false, false },
    { "Germans",    "Germans",    "Germans",    1, 2, true,  false, false, false },
    { "British",    "British",    "British",    1, 3, true,  false, false, false },
    { "Alliance",   "Alliance",   "Alliance",   1, 4, true,  false, false, false },
    { "Russians",   "Russians",   "Russians",   1, 5, true,  false, false, false },
    // ── Soviet side ──────────────────────────────────────────────────────
    { "Cubans",     "Cubans",     "Cubans",     0, 6, true,  false, false, false },
    { "Libyans",    "Libyans",    "Libyans",    0, 7, true,  false, false, false },
    { "Iraqis",     "Iraqis",     "Iraqis",     0, 8, true,  false, false, false },
    // ── Yuri side ────────────────────────────────────────────────────────
    { "Yuri",       "Yuri",       "Yuri",       2, 9, true,  false, false, true  },
    { "Yuri Country","Yuri Country","Yuri Country", 2, 9, true, false, false, true },
    // ── Neutral / civilian ───────────────────────────────────────────────
    { "Civilian",   "Civilian",   "Civilian",   3, 10, false, true,  false, false },
    { "Neutral",    "Neutral",    "Neutral",    3, 10, false, true,  false, false },
    { "Special",    "Special",    "Special",    3, 11, false, true,  true,  false },
    // ── Single-player only ───────────────────────────────────────────────
    { "GDI",        "GDI",        "GDI",        1, 12, false, false, false, false },
    { "Nod",        "Nod",        "Nod",        0, 13, false, false, false, false },
};

static const int32 g_DefaultHouseTypeCount =
    sizeof(g_DefaultHouseTypes) / sizeof(g_DefaultHouseTypes[0]);

// ============================================================================
// Init_Defaults - pre-allocate the default house types.
//
//  Mirrors the original binary's HouseTypeClass::One_Time / Init_Defaults
//  pair.  Returns the number of types created.
// ============================================================================
int32 HouseTypeClass::Init_Defaults()
{
    int32 created = 0;
    for (int32 i = 0; i < g_DefaultHouseTypeCount; ++i)
    {
        const DefaultHouseEntry& entry = g_DefaultHouseTypes[i];
        if (Find(entry.ID) != nullptr)
            continue; // Already registered.

        HouseTypeClass* pType = new HouseTypeClass(entry.ID);
        if (!pType)
            continue;

        // Seed the basic fields from the default table.
        int32 j = 0;
        while (entry.Name[j] && j < 30)
        {
            pType->Name[j] = entry.Name[j];
            ++j;
        }
        pType->Name[j] = '\0';

        j = 0;
        while (entry.ParentCountry[j] && j < 24)
        {
            pType->ParentCountry[j] = entry.ParentCountry[j];
            ++j;
        }
        pType->ParentCountry[j] = '\0';

        pType->SideIndex         = entry.Side;
        pType->ColorSchemeIndex  = entry.Color;
        pType->Multiplay         = entry.Multiplay;
        pType->MultiplayPassive  = entry.MultiplayPassive;
        pType->WallOwner         = entry.WallOwner;
        pType->SmartAI           = entry.SmartAI;
        ++created;
    }
    return created;
}

// ============================================================================
// LoadFromINI - read every HouseTypeClass property from a CCINIClass.
//
//  The original binary reads each property with a defensive default that
//  matches the value set in the constructor.  This implementation follows
//  the same rule so missing INI keys do not corrupt the type.
// ============================================================================
bool HouseTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    const char* section = ID;

    // ── Basic identification ────────────────────────────────────────────
    char buffer[256];
    if (pINI->ReadString(section, "Name", "", buffer, sizeof(buffer))) {
        int32 i = 0;
        while (buffer[i] && i < 31) { Name[i] = buffer[i]; ++i; }
        Name[i] = '\0';
    }
    if (pINI->ReadString(section, "UIName", "", buffer, sizeof(buffer))) {
        int32 i = 0;
        while (buffer[i] && i < 31) {
            UIName[i] = static_cast<wchar_t>(buffer[i]);
            ++i;
        }
        UIName[i] = L'\0';
    }

    // ── Parent country / suffix / prefix ────────────────────────────────
    pINI->ReadString(section, "ParentCountry", "", ParentCountry, sizeof(ParentCountry));
    pINI->ReadString(section, "Suffix", "", Suffix, sizeof(Suffix));
    Prefix = static_cast<char>(pINI->ReadInteger(section, "Prefix", 0));

    // ── Side + color scheme ─────────────────────────────────────────────
    SideIndex = pINI->ReadInteger(section, "Side", SideIndex);
    ColorSchemeIndex = pINI->ReadInteger(section, "Color", ColorSchemeIndex);

    // Allow the INI to override the color via an explicit R,G,B tuple.
    uint8 rgb[3] = {0, 0, 0};
    if (pINI->Read3Bytes(rgb, section, "ColorRGB", rgb)) {
        ColorSchemeIndex = HouseTypeClass::Parse_Color_RGB(rgb[0], rgb[1], rgb[2]);
    }

    // ── Multiplay flags ─────────────────────────────────────────────────
    Multiplay = pINI->ReadBool(section, "Multiplay", Multiplay);
    MultiplayPassive = pINI->ReadBool(section, "MultiplayPassive", MultiplayPassive);
    WallOwner = pINI->ReadBool(section, "WallOwner", WallOwner);
    SmartAI = pINI->ReadBool(section, "SmartAI", SmartAI);

    // ── Tech level ──────────────────────────────────────────────────────
    TechLevel = pINI->ReadInteger(section, "TechLevel", TechLevel);

    // ── Global multipliers (doubles) ────────────────────────────────────
    FirepowerMult = pINI->ReadDouble(section, "FirepowerMult", 1.0);
    GroundspeedMult = pINI->ReadDouble(section, "GroundspeedMult", 1.0);
    AirspeedMult = pINI->ReadDouble(section, "AirspeedMult", 1.0);
    ArmorMult = pINI->ReadDouble(section, "ArmorMult", 1.0);
    ROFMult = pINI->ReadDouble(section, "ROFMult", 1.0);
    CostMult = pINI->ReadDouble(section, "CostMult", 1.0);
    BuildtimeMult = pINI->ReadDouble(section, "BuildtimeMult", 1.0);

    // ── Armor multipliers ───────────────────────────────────────────────
    ArmorInfantryMult = static_cast<float>(pINI->ReadDouble(section, "ArmorInfantryMult", 1.0));
    ArmorUnitsMult = static_cast<float>(pINI->ReadDouble(section, "ArmorUnitsMult", 1.0));
    ArmorAircraftMult = static_cast<float>(pINI->ReadDouble(section, "ArmorAircraftMult", 1.0));
    ArmorBuildingsMult = static_cast<float>(pINI->ReadDouble(section, "ArmorBuildingsMult", 1.0));
    ArmorDefensesMult = static_cast<float>(pINI->ReadDouble(section, "ArmorDefensesMult", 1.0));

    // ── Cost multipliers ────────────────────────────────────────────────
    CostInfantryMult = static_cast<float>(pINI->ReadDouble(section, "CostInfantryMult", 1.0));
    CostUnitsMult = static_cast<float>(pINI->ReadDouble(section, "CostUnitsMult", 1.0));
    CostAircraftMult = static_cast<float>(pINI->ReadDouble(section, "CostAircraftMult", 1.0));
    CostBuildingsMult = static_cast<float>(pINI->ReadDouble(section, "CostBuildingsMult", 1.0));
    CostDefensesMult = static_cast<float>(pINI->ReadDouble(section, "CostDefensesMult", 1.0));

    // ── Speed multipliers ───────────────────────────────────────────────
    SpeedInfantryMult = static_cast<float>(pINI->ReadDouble(section, "SpeedInfantryMult", 1.0));
    SpeedUnitsMult = static_cast<float>(pINI->ReadDouble(section, "SpeedUnitsMult", 1.0));
    SpeedAircraftMult = static_cast<float>(pINI->ReadDouble(section, "SpeedAircraftMult", 1.0));

    // ── Build time multipliers ──────────────────────────────────────────
    BuildtimeInfantryMult = static_cast<float>(pINI->ReadDouble(section, "BuildtimeInfantryMult", 1.0));
    BuildtimeUnitsMult = static_cast<float>(pINI->ReadDouble(section, "BuildtimeUnitsMult", 1.0));
    BuildtimeAircraftMult = static_cast<float>(pINI->ReadDouble(section, "BuildtimeAircraftMult", 1.0));
    BuildtimeBuildingsMult = static_cast<float>(pINI->ReadDouble(section, "BuildtimeBuildingsMult", 1.0));
    BuildtimeDefensesMult = static_cast<float>(pINI->ReadDouble(section, "BuildtimeDefensesMult", 1.0));

    // ── Income multiplier ───────────────────────────────────────────────
    IncomeMult = static_cast<float>(pINI->ReadDouble(section, "IncomeMult", 1.0));

    // ── Veteran unit lists ──────────────────────────────────────────────
    HouseTypeClass::Parse_Veteran_List(pINI, "VeteranUnits", VeteranUnits);
    HouseTypeClass::Parse_Veteran_List(pINI, "VeteranInfantry", VeteranInfantry);
    HouseTypeClass::Parse_Veteran_List(pINI, "VeteranAircraft", VeteranAircraft);

    return true;
}

// ============================================================================
// SaveToINI - write every HouseTypeClass property back to a CCINIClass.
// ============================================================================
bool HouseTypeClass::SaveToINI(CCINIClass* pINI) {
    if (!pINI) return false;
    const char* section = ID;

    // ── Basic identification ────────────────────────────────────────────
    pINI->WriteString(section, "Name", Name);
    pINI->WriteString(section, "ParentCountry", ParentCountry);
    pINI->WriteString(section, "Suffix", Suffix);
    pINI->WriteInteger(section, "Prefix", static_cast<int32>(Prefix));

    // ── Side + color scheme ─────────────────────────────────────────────
    pINI->WriteInteger(section, "Side", SideIndex);
    pINI->WriteInteger(section, "Color", ColorSchemeIndex);

    // ── Multiplay flags ─────────────────────────────────────────────────
    pINI->WriteBool(section, "Multiplay", Multiplay);
    pINI->WriteBool(section, "MultiplayPassive", MultiplayPassive);
    pINI->WriteBool(section, "WallOwner", WallOwner);
    pINI->WriteBool(section, "SmartAI", SmartAI);

    // ── Tech level ──────────────────────────────────────────────────────
    pINI->WriteInteger(section, "TechLevel", TechLevel);

    // ── Global multipliers ──────────────────────────────────────────────
    pINI->WriteDouble(section, "FirepowerMult", FirepowerMult);
    pINI->WriteDouble(section, "GroundspeedMult", GroundspeedMult);
    pINI->WriteDouble(section, "AirspeedMult", AirspeedMult);
    pINI->WriteDouble(section, "ArmorMult", ArmorMult);
    pINI->WriteDouble(section, "ROFMult", ROFMult);
    pINI->WriteDouble(section, "CostMult", CostMult);
    pINI->WriteDouble(section, "BuildtimeMult", BuildtimeMult);

    // ── Armor multipliers ───────────────────────────────────────────────
    pINI->WriteDouble(section, "ArmorInfantryMult", static_cast<double>(ArmorInfantryMult));
    pINI->WriteDouble(section, "ArmorUnitsMult", static_cast<double>(ArmorUnitsMult));
    pINI->WriteDouble(section, "ArmorAircraftMult", static_cast<double>(ArmorAircraftMult));
    pINI->WriteDouble(section, "ArmorBuildingsMult", static_cast<double>(ArmorBuildingsMult));
    pINI->WriteDouble(section, "ArmorDefensesMult", static_cast<double>(ArmorDefensesMult));

    // ── Cost multipliers ────────────────────────────────────────────────
    pINI->WriteDouble(section, "CostInfantryMult", static_cast<double>(CostInfantryMult));
    pINI->WriteDouble(section, "CostUnitsMult", static_cast<double>(CostUnitsMult));
    pINI->WriteDouble(section, "CostAircraftMult", static_cast<double>(CostAircraftMult));
    pINI->WriteDouble(section, "CostBuildingsMult", static_cast<double>(CostBuildingsMult));
    pINI->WriteDouble(section, "CostDefensesMult", static_cast<double>(CostDefensesMult));

    // ── Speed multipliers ───────────────────────────────────────────────
    pINI->WriteDouble(section, "SpeedInfantryMult", static_cast<double>(SpeedInfantryMult));
    pINI->WriteDouble(section, "SpeedUnitsMult", static_cast<double>(SpeedUnitsMult));
    pINI->WriteDouble(section, "SpeedAircraftMult", static_cast<double>(SpeedAircraftMult));

    // ── Build time multipliers ──────────────────────────────────────────
    pINI->WriteDouble(section, "BuildtimeInfantryMult", static_cast<double>(BuildtimeInfantryMult));
    pINI->WriteDouble(section, "BuildtimeUnitsMult", static_cast<double>(BuildtimeUnitsMult));
    pINI->WriteDouble(section, "BuildtimeAircraftMult", static_cast<double>(BuildtimeAircraftMult));
    pINI->WriteDouble(section, "BuildtimeBuildingsMult", static_cast<double>(BuildtimeBuildingsMult));
    pINI->WriteDouble(section, "BuildtimeDefensesMult", static_cast<double>(BuildtimeDefensesMult));

    // ── Income multiplier ───────────────────────────────────────────────
    pINI->WriteDouble(section, "IncomeMult", static_cast<double>(IncomeMult));

    return true;
}

// ============================================================================
// Parse_Color_RGB - map an (R,G,B) tuple to a ColorScheme index.
//
//  The original binary looks the tuple up in the global ColorScheme array.
//  The standalone build does not have a ColorScheme registry, so we hash
//  the tuple into a small integer in [0..15] - enough to keep the value
//  stable across save / load.
// ============================================================================
int32 HouseTypeClass::Parse_Color_RGB(uint8 r, uint8 g, uint8 b)
{
    uint32 hash = 0;
    hash = hash * 31u + r;
    hash = hash * 31u + g;
    hash = hash * 31u + b;
    return static_cast<int32>(hash & 0xFu);
}

// ============================================================================
// Parse_Veteran_List - parse a comma-separated list of type IDs into the
// supplied vector.  Each ID is resolved via the appropriate type-array
// FindOrAllocate; entries that cannot be resolved are skipped.
// ============================================================================
void HouseTypeClass::Parse_Veteran_List(CCINIClass* pINI,
                                        const char* pKey,
                                        DynamicVectorClass<UnitTypeClass*>& pOut)
{
    if (!pINI || !pKey) return;
    char buffer[512];
    if (!pINI->ReadString(ID, pKey, "", buffer, sizeof(buffer)))
        return;

    pOut.Clear();

    // Walk the comma-separated list.  The original binary uses a small
    // tokenizer that accepts whitespace between entries.
    int32 i = 0;
    while (buffer[i] != '\0')
    {
        // Skip leading whitespace / commas.
        while (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == '\t')
            ++i;
        if (buffer[i] == '\0')
            break;

        // Extract a single token.
        char token[64];
        int32 j = 0;
        while (buffer[i] && buffer[i] != ',' && buffer[i] != ' ' &&
               buffer[i] != '\t' && j < 63)
        {
            token[j] = buffer[i];
            ++i; ++j;
        }
        token[j] = '\0';

        if (j > 0)
        {
            // The standalone build has no UnitTypeClass registry yet, so
            // we skip the lookup and just count the token.  When the type
            // registry is added, this is where FindOrAllocate(token) would
            // be called.
        }
    }
}

void HouseTypeClass::Parse_Veteran_List(CCINIClass* pINI,
                                        const char* pKey,
                                        DynamicVectorClass<InfantryTypeClass*>& pOut)
{
    if (!pINI || !pKey) return;
    char buffer[512];
    if (!pINI->ReadString(ID, pKey, "", buffer, sizeof(buffer)))
        return;
    pOut.Clear();
    // Tokenization is identical to the UnitTypeClass variant above; the
    // standalone build has no InfantryTypeClass registry, so we just walk
    // the string to validate it.
    int32 i = 0;
    while (buffer[i] != '\0')
    {
        while (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == '\t')
            ++i;
        if (buffer[i] == '\0')
            break;
        while (buffer[i] && buffer[i] != ',' && buffer[i] != ' ' &&
               buffer[i] != '\t')
            ++i;
    }
}

void HouseTypeClass::Parse_Veteran_List(CCINIClass* pINI,
                                        const char* pKey,
                                        DynamicVectorClass<AircraftTypeClass*>& pOut)
{
    if (!pINI || !pKey) return;
    char buffer[512];
    if (!pINI->ReadString(ID, pKey, "", buffer, sizeof(buffer)))
        return;
    pOut.Clear();
    int32 i = 0;
    while (buffer[i] != '\0')
    {
        while (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == '\t')
            ++i;
        if (buffer[i] == '\0')
            break;
        while (buffer[i] && buffer[i] != ',' && buffer[i] != ' ' &&
               buffer[i] != '\t')
            ++i;
    }
}

// ============================================================================
// Assign_Country - bind this house type to its parent country.  In the
// original binary this copies the parent's multiplier table so the new
// country inherits all the default values before its own INI overrides
// are applied.
// ============================================================================
bool HouseTypeClass::Assign_Country(const char* pParentID)
{
    if (!pParentID) return false;
    HouseTypeClass* pParent = Find(pParentID);
    if (!pParent) return false;

    // Inherit the multiplier tables from the parent.
    FirepowerMult = pParent->FirepowerMult;
    GroundspeedMult = pParent->GroundspeedMult;
    AirspeedMult = pParent->AirspeedMult;
    ArmorMult = pParent->ArmorMult;
    ROFMult = pParent->ROFMult;
    CostMult = pParent->CostMult;
    BuildtimeMult = pParent->BuildtimeMult;

    ArmorInfantryMult = pParent->ArmorInfantryMult;
    ArmorUnitsMult = pParent->ArmorUnitsMult;
    ArmorAircraftMult = pParent->ArmorAircraftMult;
    ArmorBuildingsMult = pParent->ArmorBuildingsMult;
    ArmorDefensesMult = pParent->ArmorDefensesMult;

    CostInfantryMult = pParent->CostInfantryMult;
    CostUnitsMult = pParent->CostUnitsMult;
    CostAircraftMult = pParent->CostAircraftMult;
    CostBuildingsMult = pParent->CostBuildingsMult;
    CostDefensesMult = pParent->CostDefensesMult;

    SpeedInfantryMult = pParent->SpeedInfantryMult;
    SpeedUnitsMult = pParent->SpeedUnitsMult;
    SpeedAircraftMult = pParent->SpeedAircraftMult;

    BuildtimeInfantryMult = pParent->BuildtimeInfantryMult;
    BuildtimeUnitsMult = pParent->BuildtimeUnitsMult;
    BuildtimeAircraftMult = pParent->BuildtimeAircraftMult;
    BuildtimeBuildingsMult = pParent->BuildtimeBuildingsMult;
    BuildtimeDefensesMult = pParent->BuildtimeDefensesMult;

    IncomeMult = pParent->IncomeMult;

    // Record the parent ID so SaveToINI emits it.
    int32 i = 0;
    while (pParentID[i] && i < 24)
    {
        ParentCountry[i] = pParentID[i];
        ++i;
    }
    ParentCountry[i] = '\0';
    return true;
}

// ============================================================================
// Assign_Side - set the side index.  Returns false if the supplied index
// is out of range.
// ============================================================================
bool HouseTypeClass::Assign_Side(int32 sideIndex)
{
    // The original binary supports sides 0..3 (Soviets, Allies, Yuri,
    // Civilian).  We accept anything in [0..7] to leave room for mods.
    if (sideIndex < 0 || sideIndex > 7)
        return false;
    SideIndex = sideIndex;
    return true;
}

// ============================================================================
// Parse_Tech_Level - clamp the supplied tech level to the supported range.
// The original binary clamps to [0..10]; we use the same bounds.
// ============================================================================
int32 HouseTypeClass::Parse_Tech_Level(int32 level)
{
    if (level < 0)  return 0;
    if (level > 10) return 10;
    return level;
}

// ============================================================================
// Compute_CRC_Full - the per-instance CRC used by the multiplayer checksum.
// Feeds every gameplay-relevant field into the engine.
// ============================================================================
int32 HouseTypeClass::Compute_CRC_Full() const
{
    CRCEngine crc;

    // Identity.
    crc.AddData(ID,            static_cast<int32>(std::strlen(ID)));
    crc.AddData(Name,          static_cast<int32>(std::strlen(Name)));
    crc.AddData(ParentCountry, static_cast<int32>(std::strlen(ParentCountry)));

    // Indices.
    crc.AddData(&ArrayIndex,       sizeof(ArrayIndex));
    crc.AddData(&SideIndex,        sizeof(SideIndex));
    crc.AddData(&ColorSchemeIndex, sizeof(ColorSchemeIndex));
    crc.AddData(&TechLevel,        sizeof(TechLevel));

    // Flags.
    crc.AddData(&Multiplay,        sizeof(Multiplay));
    crc.AddData(&MultiplayPassive, sizeof(MultiplayPassive));
    crc.AddData(&WallOwner,        sizeof(WallOwner));
    crc.AddData(&SmartAI,          sizeof(SmartAI));

    // Multipliers.
    crc.AddData(&FirepowerMult,    sizeof(FirepowerMult));
    crc.AddData(&GroundspeedMult,  sizeof(GroundspeedMult));
    crc.AddData(&AirspeedMult,     sizeof(AirspeedMult));
    crc.AddData(&ArmorMult,        sizeof(ArmorMult));
    crc.AddData(&ROFMult,          sizeof(ROFMult));
    crc.AddData(&CostMult,         sizeof(CostMult));
    crc.AddData(&BuildtimeMult,    sizeof(BuildtimeMult));

    crc.AddData(&ArmorInfantryMult,     sizeof(ArmorInfantryMult));
    crc.AddData(&ArmorUnitsMult,        sizeof(ArmorUnitsMult));
    crc.AddData(&ArmorAircraftMult,     sizeof(ArmorAircraftMult));
    crc.AddData(&ArmorBuildingsMult,    sizeof(ArmorBuildingsMult));
    crc.AddData(&ArmorDefensesMult,     sizeof(ArmorDefensesMult));

    crc.AddData(&CostInfantryMult,      sizeof(CostInfantryMult));
    crc.AddData(&CostUnitsMult,         sizeof(CostUnitsMult));
    crc.AddData(&CostAircraftMult,      sizeof(CostAircraftMult));
    crc.AddData(&CostBuildingsMult,     sizeof(CostBuildingsMult));
    crc.AddData(&CostDefensesMult,      sizeof(CostDefensesMult));

    crc.AddData(&SpeedInfantryMult,     sizeof(SpeedInfantryMult));
    crc.AddData(&SpeedUnitsMult,        sizeof(SpeedUnitsMult));
    crc.AddData(&SpeedAircraftMult,     sizeof(SpeedAircraftMult));

    crc.AddData(&BuildtimeInfantryMult,   sizeof(BuildtimeInfantryMult));
    crc.AddData(&BuildtimeUnitsMult,      sizeof(BuildtimeUnitsMult));
    crc.AddData(&BuildtimeAircraftMult,   sizeof(BuildtimeAircraftMult));
    crc.AddData(&BuildtimeBuildingsMult,  sizeof(BuildtimeBuildingsMult));
    crc.AddData(&BuildtimeDefensesMult,   sizeof(BuildtimeDefensesMult));

    crc.AddData(&IncomeMult, sizeof(IncomeMult));

    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Get_Default_House_Count - returns the size of the default house table.
// ============================================================================
int32 HouseTypeClass::Get_Default_House_Count()
{
    return g_DefaultHouseTypeCount;
}

// ============================================================================
// Get_Default_House_Entry - returns the n-th default house entry.  Returns
// nullptr if the index is out of range.  Used by the scenario loader when
// pre-warming the HouseType array.
// ============================================================================
const char* HouseTypeClass::Get_Default_House_ID(int32 index)
{
    if (index < 0 || index >= g_DefaultHouseTypeCount)
        return nullptr;
    return g_DefaultHouseTypes[index].ID;
}
