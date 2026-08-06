#pragma once

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>

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
    int32           CellColor;
    int32           Altitude;
    int32           Slope;
    int32           TiberiumValue;
    int32           WallOwner;
    int32           CrateType;
    DWORD           unknown_38;
    DWORD           unknown_3C;
    DWORD           unknown_40;
    DWORD           unknown_44;
    CellClass*      AdjacentCells[8]; // N, NE, E, SE, S, SW, W, NW
};

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
    , CellColor(0)
    , Altitude(0)
    , Slope(0)
    , TiberiumValue(0)
    , WallOwner(-1)
    , CrateType(0)
    , unknown_38(0)
    , unknown_3C(0)
    , unknown_40(0)
    , unknown_44(0)
{
    for (int32 i = 0; i < 8; ++i) AdjacentCells[i] = nullptr;
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