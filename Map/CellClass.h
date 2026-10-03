#pragma once

template <class T> class DynamicVectorClass;

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>
#include <Houses/HouseClass.h>

// ============================================================================
// CellClass - Represents a single map cell (tile)
// Inherits AbstractClass in the original, but here we define it as a standalone
// POD-like structure for simplicity, matching the original layout
// ============================================================================

class ObjectClass;
class HouseClass;
class BuildingClass;
class OverlayClass;
class SmudgeClass;
class TerrainClass;
class TagClass;
class IStream;
class CRCEngine;

// Cell flags bitfield
enum class CellFlags : uint32 {
    Empty           = 0x0,
    CenterRevealed  = 0x1,
    EdgeRevealed    = 0x2,
    IsWaypoint      = 0x4,
    Explored        = 0x8,
    FlagPresent     = 0x10,
    FlagToShroud    = 0x20,
    IsPlot          = 0x40,
    BridgeOwner     = 0x80,
    BridgeHead      = 0x100,
    HasTiberium     = 0x200,
    BridgeBody      = 0x400,
    BridgeDir       = 0x800,
    PixelFX         = 0x1000,
    IsShrouded      = 0x2000,
    HasOverlay      = 0x4000,
    Veinhole        = 0x8000,
    DrawDarkenIfInAir = 0x10000,
    AnimAttached    = 0x20000,
    Tube            = 0x40000,
    EMPPresent      = 0x80000,
    HorizontalLineEventTag = 0x100000,
    VerticalLineEventTag   = 0x200000,
    Fogged          = 0x400000,

    Revealed        = CenterRevealed | EdgeRevealed,
    Bridge          = BridgeHead | BridgeBody
};

inline CellFlags operator|(CellFlags a, CellFlags b) {
    return static_cast<CellFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline CellFlags operator&(CellFlags a, CellFlags b) {
    return static_cast<CellFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}
inline CellFlags operator~(CellFlags a) {
    return static_cast<CellFlags>(~static_cast<uint32>(a));
}

// Alt cell flags
enum class AltCellFlags : uint32 {
    CellMarked      = 0x1,
    ContainsBuilding    = 0x2,
    HasTerrain      = 0x4,
    Mapped              = 0x8,
    NoFog               = 0x10,
    HasSmudge       = 0x20,
    IsPassable      = 0x40,
    IsIrradiated    = 0x80,
    IsBridged       = 0x100,

    Clear               = Mapped | NoFog
};

inline AltCellFlags operator|(AltCellFlags a, AltCellFlags b) {
    return static_cast<AltCellFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline AltCellFlags operator&(AltCellFlags a, AltCellFlags b) {
    return static_cast<AltCellFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

// ============================================================================
// CellClass
// ============================================================================
class CellClass {
public:
    static constexpr int32 LeapArray_Step = 256;
    static constexpr int32 CellWidth = 256;
    static constexpr int32 CellHeight = 256;

    CellClass();
    ~CellClass();

    // ========================================================================
    // Coordinate conversion
    // ========================================================================
    static CellStruct Coord2Cell(const CoordStruct& crd);
    static CoordStruct Cell2Coord(const CellStruct& cell);
    static CoordStruct Cell2Coord(int32 x, int32 y);
    static CoordStruct Cell2Coord(int32 cellIndex);

    static int32 Cell2CellIndex(const CellStruct& cell);
    static CellStruct CellIndex2Cell(int32 cellIndex);

    static int32 Coord2CellIndex(const CoordStruct& crd);

    // ========================================================================
    // Terrain checks
    // ========================================================================
    bool IsClear() const;
    bool ContainsWater() const;
    bool IsWater() const;
    bool IsLand() const;
    bool IsRock() const;
    bool IsWall() const;
    bool IsTiberium() const;
    bool IsBridge() const;
    bool IsTunnel() const;
    bool IsRamp() const;
    bool IsRailroad() const;
    bool IsWeeds() const;
    bool IsIce() const;
    bool IsBeach() const;
    bool IsRoad() const;
    bool IsRough() const;
    bool IsOccupied() const;
    bool IsPassable() const;

    bool PassableFor(MovementZone zone) const;
    bool CanEnterTunnelHere() const;

    // ========================================================================
    // Tile-classification predicates (the binary's CellClass_Tile_Is* family)
    //
    // Every one of these reads the cell's isometric tile type index (+0x38)
    // and compares it against the special tile-set ordinals the tileset INI
    // published into the tile_* globals.  A zero-length range means the key
    // was -1 in the theater (tile set absent), in which case the predicate is
    // simply false.
    // ========================================================================
    bool Tile_IsWater() const;
    bool Tile_IsBridge() const;
    bool Tile_IsWoodBridge() const;
    bool Tile_IsCliff() const;
    bool Tile_IsDestroyableCliff() const;
    bool Tile_IsRamp() const;
    bool Tile_IsGreen() const;
    bool Tile_IsBlank() const;
    bool Tile_IsNotWater() const;
    bool Tile_IsShorePieces() const;
    bool Tile_IsWet() const;
    bool Tile_IsMiscPave() const;
    bool Tile_IsPave() const;
    bool Tile_IsDirtRoad() const;
    bool Tile_IsPavedRoad() const;
    bool Tile_IsPavedRoadEnd() const;
    bool Tile_IsPavedRoadSlope() const;
    bool Tile_IsMedian() const;
    bool Tile_IsClearToSandLat() const;
    bool Tile_IsATunnel() const;

    // CellClass::Is_Clear_To_Move (asm 0x4834DA) - the movement-legal cell
    // test used by the map's "find a nearby spot" and unit-placement paths.
    //
    //   SpeedType      - the mover's speed class (SpeedType::Foot ..).
    //   a3             - clear the low 5 bits of the movement field before the
    //                    zero test (the caller wants to ignore certain flags).
    //   a4             - also clear the "crushable" bits.
    //   a5             - the zone the cell is expected to belong to, or -1.
    //   MovementZone   - the mover's movement zone.
    //   a7             - the expected "land" byte at +0x11B, or -1.
    //   boo            - treat the mover as a bridge-layer.
    bool Is_Clear_To_Move(int32 SpeedType, bool a3, bool a4, int32 a5,
                          MovementZone zone, int32 a7, bool boo) const;

    // Radiation
    //
    // RadLevel is the accumulated radiation dose at this cell (a double in the
    // original, at +0xF0); anything above 1.0 makes the cell "radiated" and the
    // RadSiteClass update loop ticks the level down each frame.  RadSite is the
    // owning RadSiteClass pointer (+0xF8).
    void RadLevel_Increase(double amount);
    void RadLevel_Decrease(double amount);
    void Set_Rad_Site(void* pRadSite);
    bool Is_Radiated() const;
    int32 Get_RadLevel() const;

    // Raw per-SpeedType movement cost byte for this cell, taken from the land
    // characteristics table indexed by LandType.
    int32 Get_Movement_Cost(int32 SpeedType) const;

    // The coarse land byte the original stores at +0x11B.  Derived from the
    // land type, since that is what the assembly's table lookups key on.
    int32 Get_Land_Byte() const;

    // Ground and bridge movement bitfields (+0x124 / +0x128).
    uint8 Get_Movement_Field() const;
    uint8 Get_Movement_Field_Bridge() const;

    // Hierarchical pathfinding: route through an 8x8-cell block abstraction
    // first, then refine inside the traversed blocks.  Returns false when no
    // block-level route exists (the caller then falls back to the regular
    // per-cell search).
    static bool Pathfinding_Hierarchical(const CellStruct& from, const CellStruct& to,
                                         DynamicVectorClass<CellStruct>& outPath,
                                         MovementZone zone);

    // ========================================================================
    // Adjacency
    // ========================================================================
    CellClass* AdjacentCell(DirType dir) const;

    // ========================================================================
    // Misc checks
    // ========================================================================
    bool Cell_Seems_Ok() const;
    bool Goodie_Check() const;
    int32 GetContainedTiberiumValue() const;
    bool IsShrouded() const;
    bool IsFogged() const;
    bool IsRevealed() const;

    // ========================================================================
    // Flag helpers
    // ========================================================================
    bool HasFlag(CellFlags flag) const {
        return (static_cast<uint32>(Flags) & static_cast<uint32>(flag)) != 0;
    }
    void SetFlag(CellFlags flag, bool value) {
        if (value)
            Flags = static_cast<CellFlags>(static_cast<uint32>(Flags) | static_cast<uint32>(flag));
        else
            Flags = static_cast<CellFlags>(static_cast<uint32>(Flags) & ~static_cast<uint32>(flag));
    }

    bool HasAltFlag(AltCellFlags flag) const {
        return (static_cast<uint32>(AltFlags) & static_cast<uint32>(flag)) != 0;
    }
    void SetAltFlag(AltCellFlags flag, bool value) {
        if (value)
            AltFlags = static_cast<AltCellFlags>(static_cast<uint32>(AltFlags) | static_cast<uint32>(flag));
        else
            AltFlags = static_cast<AltCellFlags>(static_cast<uint32>(AltFlags) & ~static_cast<uint32>(flag));
    }

    // ========================================================================
    // Extended cell management API (implemented in CellClass.cpp)
    // ========================================================================

    // Initialization / state management
    void Init();
    void Recalc_Attributes();

    // Serialization
    bool Load(IStream* pStm);
    bool Save(IStream* pStm) const;
    void Compute_CRC(CRCEngine& crc) const;

    // Coordinate accessors
    CoordStruct Get_CellCoords() const;
    CoordStruct Get_Cell_Position() const;
    Point2D Get_Cell_Screen_Position(int32 originX, int32 originY) const;

    // Map bounds / visibility
    bool Is_On_Map() const;
    bool Is_Visible() const;
    bool Is_Discovered() const;

    // Tiberium management
    int32 Get_Tiberium_Type() const;
    int32 Get_Tiberium_Value() const;
    void Set_Tiberium(int32 type, int32 value);

    // The binary's CellClass_GetContainedTiberiumIndex: resolves the overlay
    // the cell carries into the tiberium type ordinal, or -1.
    int32 Get_Contained_Tiberium_Index() const;

    // CellClass_TiberiumInCell - the ore amount the cell holds: the tiberium
    // type's Value multiplied by (growth stage + 1).  Zero when the cell's
    // overlay is not ore.
    int32 Tiberium_In_Cell() const;

    // CellClass_ContainsTiberium - the bare land-type test the harvest code
    // performs before it bothers resolving the ore type.
    bool Contains_Tiberium() const;

    // Overlay management
    int32 Get_Overlay() const;
    int32 Get_Overlay_Type() const;
    void Set_Overlay(int32 overlayIndex, int32 overlayData = 0);

    // Smudge management
    int32 Get_Smudge() const;
    int32 Get_Smudge_Type() const;
    void Set_Smudge(int32 smudgeIndex, int32 smudgeData = 0);

    // Terrain object management
    TerrainClass* Get_Terrain() const;

    // CellClass_StopAmbientSound (asm 0x5F6CB0).  Recursively silences the
    // ambient sound of whatever sits on this cell - the building first, and
    // failing that the terrain object.
    void Silence_Attached_Ambient();
    void Set_Terrain(TerrainClass* pTerrain);
    void Clear_Terrain();

    // Land type accessors
    ::LandType Get_Land_Type() const;
    void Set_Land_Type(::LandType land);

    // Height accessors
    int32 Get_Ground_Height() const;
    int32 Get_Z_Height() const;

    // Occupier management
    int32 Get_Occupier_Count() const;
    ObjectClass* Get_Occupier() const;
    void Add_Occupier(ObjectClass* pObj);
    void Remove_Occupier(ObjectClass* pObj);

    // Occupier type checks
    bool Has_Unit() const;
    bool Has_Building() const;
    bool Has_Infantry() const;

    // ========================================================================
    // Occupancy-list lookups
    //
    //  The binary threads every object on a cell onto one of two singly-linked
    //  lists through ObjectClass::NextObject (+0x30): the ground list (+0xE4)
    //  and the altitude list (+0xE8).  The Get* helpers pick a list from their
    //  bool argument and return the first entry whose WhatAmI() matches.
    // ========================================================================
    ObjectClass* First_Object(bool alt = false) const;
    ObjectClass* GetUnit(bool alt = false) const;
    ObjectClass* GetAircraft(bool alt = false) const;
    ObjectClass* GetInfantry(bool alt = false) const;

    // ========================================================================
    // Terrain / tiberium helpers
    // ========================================================================
    bool  CanAddTiberium() const;

    // Foundation
    bool Is_Foundation() const;
    bool Get_Foundation() const;

    // Radar color
    int32 Cell_Color() const;

    // Buildability
    bool Can_Build_On() const;
    bool Is_Buildable() const;
    bool Is_Concrete() const;
    bool Is_Cliff() const;

    // Slope
    int32 Get_Slope() const;
    bool Is_Sloped() const;

    // Damage
    void Apply_Damage(int32 damage, int32 warheadType);

    // Rendering
    void Draw_It(int32 originX, int32 originY) const;

    // Shroud management
    void Set_Shrouded(bool shrouded);
    void Unshroud();

    // CellClass_Setup (asm 0x4CC180).  Re-derives this cell's shroud state
    // from the map's current view rectangle: the cell is checked against the
    // visible rect and either re-shrouded (`flag` < 0) or marked fogged.
    // Called for every cell when the player's view is resized.
    void Setup(int32 flag);

    // ========================================================================
    // Per-house sensor / cloak-generator bookkeeping
    //
    //  Every cell tracks, for each house index, whether that house has a
    //  cloak generator covering it (a 32-bit bitmask, asm +0x78), how many of
    //  that house's sensor structures reveal it (a 16-bit counter at
    //  +0x7C + idx*2) and how many of that house's disguise sensors cover it
    //  (a 16-bit counter at +0xAC + idx*2).  The building update code adds and
    //  removes these contributions as structures power on and off.
    // ========================================================================
    void CloakGen_AddHouse(int32 idxHouse);
    void CloakGen_RemHouse(int32 idxHouse);
    bool CloakGen_HasHouse(int32 idxHouse) const;
    void Sensors_AddHouse(int32 idxHouse);
    void Sensors_RemHouse(int32 idxHouse);
    bool Sensors_HasHouse(int32 idxHouse) const;
    void DisguiseSensors_AddHouse(int32 idxHouse);
    void DisguiseSensors_RemHouse(int32 idxHouse);
    bool DisguiseSensors_HasHouse(int32 idxHouse) const;

    // ========================================================================
    // Tunnel / coordinate / identity helpers
    // ========================================================================
    // CellClass_GetTunnel (asm 0x484F2B): resolves TubeIndex against the
    // tunnel table and returns the tunnel cell, or null when the index is
    // negative or out of range.
    CellClass* Get_Tunnel() const;
    // CellClass_SetMapCoords (asm 0x47D3B8): copies the four-byte packed
    // X/Y pair into the cell's +0x24 coordinate slot.
    void Set_Map_Coords(const CellStruct& coords);
    // CellClass_GetAbstractID (asm 0x482Axx): the fixed AbstractType ordinal
    // every cell reports (AbstractType::Cell).
    int32 Get_AbstractID() const;

    // ========================================================================
    // Flag / shroud flag helpers
    // ========================================================================
    // CellClass_FlagPickedUp (asm 0x4834xx): clears the "flag placed" bit
    // (0x10) of the cell's +0x140 flag word and resets the attached object
    // index at +0x50 to -1.  Returns true when the bit had been set.
    bool Flag_Picked_Up();
    // CellClass_Smth0 (asm 0x487635): decrements the gap counter at +0x130,
    // wrapping a count of exactly 1 back down through zero.
    void Smth0();
    // CellClass_Smth2 (asm 0x4876xx): sets bits 0x18 of the shroud flag word
    // and, when the gap counter is still positive, tags 0x20 onto +0x140.
    void Smth2();

    // ========================================================================
    // Properties
    // ========================================================================
    CellStruct      MapCoords;
    int32           CellIndex;
    CellFlags       Flags;
    AltCellFlags    AltFlags;
    ::LandType      Land;
    int32           TileType;
    int32           TileSubIndex;
    int32           Overlay;
    int32           OverlayData;
    int32           Smudge;
    int32           SmudgeData;
    ObjectClass*    Occupier;
    TerrainClass*   Terrain;
    TagClass*       AttachedTag;
    // ── Cell occupancy lists (+0xE4 / +0xE8) ──────────────────────────────
    // The binary threads every object standing on a cell onto one of two
    // singly-linked lists through ObjectClass::NextObject (+0x30): the ground
    // list at +0xE4 and the altitude ("flying") list at +0xE8.  GetUnit /
    // GetAircraft / GetInfantry pick a list via their bool argument and walk
    // it looking for the matching abstract type.
    ObjectClass*    FirstObject;
    ObjectClass*    AltObject;
    int32           CellColor;
    int32           Altitude;
    int32           Slope;
    int32           TiberiumValue;
    double          RadLevel;           // +0xF0
    void*           RadSite;            // +0xF8
    int32           WallOwner;
    int32           CrateType;
    DWORD           unknown_38;
    DWORD           unknown_3C;
    DWORD           unknown_40;
    DWORD           unknown_44;

    // ── Movement / zone state ─────────────────────────────────────────────
    // These back the assembly's per-cell bytes consulted by
    // CellClass::Is_Clear_To_Move: the coarse "passability" byte (+0x11A), the
    // "land" byte (+0x11B), the ground and bridge movement bitfields
    // (+0x124 / +0x128) and the pathfinding zone index (+0xE4).
    uint8           Passability;        // +0x11A
    uint8           LandByte;           // +0x11B
    uint8           OverlayFrame;       // +0x11E (overlay growth stage / frame)
    uint8           MovementField;      // +0x124
    uint8           MovementFieldBridge;// +0x128
    int32           ZoneIndex;          // +0xE4
    int16           TubeIndex;          // +0x118 (index into vec_Tubes, -1 = none)

    // ── Shroud bookkeeping ────────────────────────────────────────────────
    // IsUnderShroud / GapsCoveringCell / Field_12C are the three slots
    // MapClass::Shroud_The_Map clears and re-seeds for every cell on the map.
    bool            IsUnderShroud;      // +0x12C+1 - cell hidden by the shroud
    int32           GapsCoveringCell;   // +0x130   - number of gap generators covering it
    uint32          Field_12C;          // +0x12C   - shroud flag bits (bit 0x18 cleared on reset)

    // ── Per-house sensor / cloak-generator state ──────────────────────────
    // CloakGenMask: bit N set when house N has a cloak generator covering
    // this cell (asm +0x78).  SensedByHouses[idx]: how many of house idx's
    // sensor structures reveal this cell (asm +0x7C).  DisguiseSensedByHouses
    // [idx]: how many of house idx's disguise sensors cover it (asm +0xAC).
    uint32          CloakGenMask;
    uint16          SensedByHouses[HouseClass::MaxHouses];
    uint16          DisguiseSensedByHouses[HouseClass::MaxHouses];
    uint8           pad_Sensors[2];

    // ── Flag / object-index slots ─────────────────────────────────────────
    // Field_50 is the 32-bit "attached object" index the flag code resets to
    // -1 (asm +0x50).  Field_140 is the cell flag word whose bit 0x10 records
    // that a flag is planted here (asm +0x140).
    int32           Field_50;
    uint32          Field_140;

    CellClass*      AdjacentCells[8]; // N, NE, E, SE, S, SW, W, NW
};

// The tunnel table the tunnel predicates index.  CellClass_Tile_IsATunnel
// bounds its tube index against this vector's length before it consults the
// cell's land type.
extern int32            TubeCount;
extern DynamicVectorClass<CellClass*>* vec_Tubes;

// ============================================================================
// Coordinate conversion implementations
// ============================================================================
inline CellStruct CellClass::Coord2Cell(const CoordStruct& crd) {
    return CellStruct(
        static_cast<int16>(crd.X / LeapArray_Step),
        static_cast<int16>(crd.Y / LeapArray_Step)
    );
}

inline CoordStruct CellClass::Cell2Coord(const CellStruct& cell) {
    return CoordStruct(
        static_cast<int32>(cell.X) * LeapArray_Step,
        static_cast<int32>(cell.Y) * LeapArray_Step,
        0
    );
}

inline CoordStruct CellClass::Cell2Coord(int32 x, int32 y) {
    return CoordStruct(x * LeapArray_Step, y * LeapArray_Step, 0);
}

inline CoordStruct CellClass::Cell2Coord(int32 cellIndex) {
    return Cell2Coord(CellIndex2Cell(cellIndex));
}

inline int32 CellClass::Cell2CellIndex(const CellStruct& cell) {
    return (static_cast<int32>(cell.Y) << 9) + static_cast<int32>(cell.X);
}

inline CellStruct CellClass::CellIndex2Cell(int32 cellIndex) {
    return CellStruct(
        static_cast<int16>(cellIndex & 0x1FF),
        static_cast<int16>((cellIndex >> 9) & 0x1FF)
    );
}

inline int32 CellClass::Coord2CellIndex(const CoordStruct& crd) {
    return Cell2CellIndex(Coord2Cell(crd));
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
inline CellClass::CellClass()
    : MapCoords(0, 0)
    , CellIndex(-1)
    , Flags(CellFlags::Empty)
    , AltFlags(AltCellFlags::Clear)
    , Land(::LandType::Clear)
    , TileType(0)
    , TileSubIndex(0)
    , Overlay(-1)
    , OverlayData(0)
    , Smudge(-1)
    , SmudgeData(0)
    , Occupier(nullptr)
    , Terrain(nullptr)
    , AttachedTag(nullptr)
    , FirstObject(nullptr)
    , AltObject(nullptr)
    , CellColor(0)
    , Altitude(0)
    , Slope(0)
    , TiberiumValue(0)
    , RadLevel(0.0)
    , RadSite(nullptr)
    , WallOwner(-1)
    , CrateType(0)
    , unknown_38(0)
    , unknown_3C(0)
    , unknown_40(0)
    , unknown_44(0)
    , Passability(0)
    , LandByte(0)
    , OverlayFrame(0)
    , MovementField(0)
    , MovementFieldBridge(0)
    , ZoneIndex(-1)
    , TubeIndex(-1)
    , CloakGenMask(0)
    , Field_50(-1)
    , Field_140(0)
{
    for (int32 i = 0; i < 8; ++i) AdjacentCells[i] = nullptr;
    for (int32 i = 0; i < HouseClass::MaxHouses; ++i)
    {
        SensedByHouses[i] = 0;
        DisguiseSensedByHouses[i] = 0;
    }
}

inline CellClass::~CellClass() {
}

// ========================================================================
// Terrain checks
// ========================================================================
inline bool CellClass::IsClear() const {
    return Land == ::LandType::Clear;
}

inline bool CellClass::ContainsWater() const {
    return Land == ::LandType::Water;
}

inline bool CellClass::IsWater() const {
    return Land == ::LandType::Water;
}

inline bool CellClass::IsLand() const {
    return Land != ::LandType::Water && Land != ::LandType::Rock;
}

inline bool CellClass::IsRock() const {
    return Land == ::LandType::Rock;
}

inline bool CellClass::IsWall() const {
    return Land == ::LandType::Wall;
}

inline bool CellClass::IsTiberium() const {
    return Land == ::LandType::Tiberium;
}

inline bool CellClass::IsBridge() const {
    return HasFlag(CellFlags::Bridge);
}

inline bool CellClass::IsTunnel() const {
    return Land == ::LandType::Tunnel;
}

inline bool CellClass::IsRamp() const {
    return Slope > 0;
}

inline bool CellClass::IsRailroad() const {
    return Land == ::LandType::Railroad;
}

inline bool CellClass::IsWeeds() const {
    return Land == ::LandType::Weeds;
}

inline bool CellClass::IsIce() const {
    return Land == ::LandType::Ice;
}

inline bool CellClass::IsBeach() const {
    return Land == ::LandType::Beach;
}

inline bool CellClass::IsRoad() const {
    return Land == ::LandType::Road;
}

inline bool CellClass::IsRough() const {
    return Land == ::LandType::Rough;
}

inline bool CellClass::IsOccupied() const {
    return Occupier != nullptr || HasAltFlag(AltCellFlags::ContainsBuilding);
}

inline bool CellClass::IsPassable() const {
    return !IsOccupied() && !IsWall() && !IsRock();
}

inline bool CellClass::PassableFor(MovementZone zone) const {
    if (IsWall() || IsRock()) return false;
    if (zone == MovementZone::Water || zone == MovementZone::WaterBeach) {
        return IsWater() || IsBeach();
    }
    if (zone == MovementZone::Amphibious || zone == MovementZone::AmphibiousCrusher ||
        zone == MovementZone::AmphibiousDestroyer) {
        return true;
    }
    if (zone == MovementZone::Fly) return true;
    return !IsWater();
}

inline bool CellClass::CanEnterTunnelHere() const {
    return HasFlag(CellFlags::Tube) && IsTunnel();
}

// ========================================================================
// Adjacency
// ========================================================================
inline CellClass* CellClass::AdjacentCell(DirType dir) const {
    int32 idx = static_cast<int32>(static_cast<uint8>(dir) >> 5);
    if (idx < 0 || idx > 7) return nullptr;
    return AdjacentCells[idx];
}

// ========================================================================
// Misc checks
// ========================================================================
inline bool CellClass::Cell_Seems_Ok() const {
    return CellIndex >= 0 && !IsWall();
}

inline bool CellClass::Goodie_Check() const {
    return IsClear() && !IsOccupied();
}

inline int32 CellClass::GetContainedTiberiumValue() const {
    return TiberiumValue;
}

inline bool CellClass::IsShrouded() const {
    return !HasFlag(CellFlags::Revealed);
}

inline bool CellClass::IsFogged() const {
    return HasFlag(CellFlags::Fogged);
}

inline bool CellClass::IsRevealed() const {
    return HasFlag(CellFlags::Revealed);
}