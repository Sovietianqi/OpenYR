#include "CellClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/TerrainClass.h"
#include "../Abstract/OverlayClass.h"
#include "../Abstract/SmudgeClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Game/Game.h"
#include "../IO/CRC.h"
#include "../COM/IUnknown.h"

#include <cstring>

// ============================================================================
// File-local helpers
// ============================================================================
namespace {
    // Compute the radar color index for a given land type.
    int32 Compute_CellColor_ForLand(::LandType landType) {
        switch (landType) {
            case ::LandType::Clear:    return 0;
            case ::LandType::Rough:    return 1;
            case ::LandType::Road:     return 2;
            case ::LandType::Water:    return 3;
            case ::LandType::Rock:     return 4;
            case ::LandType::Wall:     return 5;
            case ::LandType::Tiberium: return 6;
            case ::LandType::Beach:    return 7;
            case ::LandType::Tunnel:   return 8;
            case ::LandType::Railroad: return 9;
            case ::LandType::Weeds:    return 10;
            case ::LandType::Ice:      return 11;
            default:                   return 0;
        }
    }
} // anonymous namespace

// ============================================================================
// CellClass.cpp - Map cell implementation
// ============================================================================
// CellClass is the atomic unit of the game map. Every cell stores its land
// type, tile graphics index, overlay/smudge/terrain references, occupier
// pointer, shroud/fog state and adjacency links. This file implements all
// non-trivial methods declared in CellClass.h:
//
//   * Init / Recalc_Attributes - reset and recompute derived cell state
//   * Load / Save / Compute_CRC - binary persistence and sync verification
//   * Get_CellCoords / Get_Cell_Position - isometric world/pixel coordinates
//   * Is_On_Map / Is_Visible / Is_Discovered - map bounds and fog-of-war
//   * Tiberium / Overlay / Smudge / Terrain accessors and mutators
//   * Land type and height accessors
//   * Occupier management (add / remove / count / type-check)
//   * Buildability checks (Can_Build_On, Is_Buildable, Is_Concrete, etc.)
//   * Cell_Color - radar minimap colour
//   * Apply_Damage - damage propagation to overlay/terrain
//   * Draw_It - render the cell's terrain layer
//   * Set_Shrouded / Unshroud - fog-of-war manipulation
// ============================================================================

// ============================================================================
// Initialization
// ============================================================================

// ----------------------------------------------------------------------------
// Init - reset the cell to a blank, unoccupied state.
// ----------------------------------------------------------------------------
void CellClass::Init() {
    MapCoords = CellStruct(0, 0);
    CellIndex = -1;
    Flags = CellFlags::Empty;
    AltFlags = AltCellFlags::Clear;
    Land = ::LandType::Clear;
    TileType = 0;
    TileSubIndex = 0;
    Overlay = -1;
    OverlayData = 0;
    Smudge = -1;
    SmudgeData = 0;
    Occupier = nullptr;
    Terrain = nullptr;
    CellColor = 0;
    Altitude = 0;
    Slope = 0;
    TiberiumValue = 0;
    WallOwner = -1;
    CrateType = 0;
    unknown_38 = 0;
    unknown_3C = 0;
    unknown_40 = 0;
    unknown_44 = 0;
    for (int32 i = 0; i < 8; ++i) {
        AdjacentCells[i] = nullptr;
    }
}

// ----------------------------------------------------------------------------
// Recalc_Attributes - recompute derived cell attributes from the current
// state. This is called after loading a scenario or when the terrain layer
// changes (e.g. tiberium spread, overlay destruction).
// ----------------------------------------------------------------------------
void CellClass::Recalc_Attributes() {
    // Derive the land type from the tile/overlay state when possible.
    // If the cell has a tiberium overlay, the land type is Tiberium.
    if (Overlay >= 0 && TiberiumValue > 0) {
        Land = ::LandType::Tiberium;
    }

    // If the cell has a wall overlay, the land type is Wall.
    // The original game checks the overlay type's Wall flag; we approximate
    // by checking if the overlay index falls in the wall range (0..4).
    if (Overlay >= 0 && Overlay <= 4 && TiberiumValue == 0) {
        // Only treat as wall if the overlay data indicates a wall
        if (OverlayData > 0) {
            Land = ::LandType::Wall;
        }
    }

    // Compute the radar cell color from the land type.
    CellColor = Compute_CellColor_ForLand(Land);

    // Update the occupancy flag based on the occupier pointer.
    if (Occupier) {
        SetAltFlag(AltCellFlags::ContainsBuilding, true);
    } else {
        SetAltFlag(AltCellFlags::ContainsBuilding, false);
    }
}

// ============================================================================
// Serialization
// ============================================================================

// ----------------------------------------------------------------------------
// Load - read the cell's persistent state from a binary stream.
// ----------------------------------------------------------------------------
bool CellClass::Load(IStream* pStm) {
    if (!pStm) return false;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read map coordinates
    hr = pStm->Read(&MapCoords, sizeof(MapCoords), &read);
    if (hr < 0 || read != sizeof(MapCoords)) return false;

    // Read cell index
    hr = pStm->Read(&CellIndex, sizeof(CellIndex), &read);
    if (hr < 0 || read != sizeof(CellIndex)) return false;

    // Read flags
    uint32 flagsVal = 0;
    hr = pStm->Read(&flagsVal, sizeof(flagsVal), &read);
    if (hr < 0 || read != sizeof(flagsVal)) return false;
    Flags = static_cast<CellFlags>(flagsVal);

    uint32 altFlagsVal = 0;
    hr = pStm->Read(&altFlagsVal, sizeof(altFlagsVal), &read);
    if (hr < 0 || read != sizeof(altFlagsVal)) return false;
    AltFlags = static_cast<AltCellFlags>(altFlagsVal);

    // Read land type
    int32 landVal = 0;
    hr = pStm->Read(&landVal, sizeof(landVal), &read);
    if (hr < 0 || read != sizeof(landVal)) return false;
    Land = static_cast<::LandType>(landVal);

    // Read tile and overlay data
    hr = pStm->Read(&TileType, sizeof(TileType), &read);
    if (hr < 0 || read != sizeof(TileType)) return false;
    hr = pStm->Read(&TileSubIndex, sizeof(TileSubIndex), &read);
    if (hr < 0 || read != sizeof(TileSubIndex)) return false;
    hr = pStm->Read(&Overlay, sizeof(Overlay), &read);
    if (hr < 0 || read != sizeof(Overlay)) return false;
    hr = pStm->Read(&OverlayData, sizeof(OverlayData), &read);
    if (hr < 0 || read != sizeof(OverlayData)) return false;
    hr = pStm->Read(&Smudge, sizeof(Smudge), &read);
    if (hr < 0 || read != sizeof(Smudge)) return false;
    hr = pStm->Read(&SmudgeData, sizeof(SmudgeData), &read);
    if (hr < 0 || read != sizeof(SmudgeData)) return false;

    // Read height and slope
    hr = pStm->Read(&Altitude, sizeof(Altitude), &read);
    if (hr < 0 || read != sizeof(Altitude)) return false;
    hr = pStm->Read(&Slope, sizeof(Slope), &read);
    if (hr < 0 || read != sizeof(Slope)) return false;

    // Read tiberium value
    hr = pStm->Read(&TiberiumValue, sizeof(TiberiumValue), &read);
    if (hr < 0 || read != sizeof(TiberiumValue)) return false;

    // Read wall owner and crate type
    hr = pStm->Read(&WallOwner, sizeof(WallOwner), &read);
    if (hr < 0 || read != sizeof(WallOwner)) return false;
    hr = pStm->Read(&CrateType, sizeof(CrateType), &read);
    if (hr < 0 || read != sizeof(CrateType)) return false;

    // Reset transient pointers - these are re-linked by the map loader
    Occupier = nullptr;
    Terrain = nullptr;
    for (int32 i = 0; i < 8; ++i) {
        AdjacentCells[i] = nullptr;
    }

    // Recompute derived attributes
    Recalc_Attributes();

    return true;
}

// ----------------------------------------------------------------------------
// Save - write the cell's persistent state to a binary stream.
// ----------------------------------------------------------------------------
bool CellClass::Save(IStream* pStm) const {
    if (!pStm) return false;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(&MapCoords, sizeof(MapCoords), &written);
    if (hr < 0 || written != sizeof(MapCoords)) return false;

    hr = pStm->Write(&CellIndex, sizeof(CellIndex), &written);
    if (hr < 0 || written != sizeof(CellIndex)) return false;

    uint32 flagsVal = static_cast<uint32>(Flags);
    hr = pStm->Write(&flagsVal, sizeof(flagsVal), &written);
    if (hr < 0 || written != sizeof(flagsVal)) return false;

    uint32 altFlagsVal = static_cast<uint32>(AltFlags);
    hr = pStm->Write(&altFlagsVal, sizeof(altFlagsVal), &written);
    if (hr < 0 || written != sizeof(altFlagsVal)) return false;

    int32 landVal = static_cast<int32>(Land);
    hr = pStm->Write(&landVal, sizeof(landVal), &written);
    if (hr < 0 || written != sizeof(landVal)) return false;

    hr = pStm->Write(&TileType, sizeof(TileType), &written);
    if (hr < 0 || written != sizeof(TileType)) return false;
    hr = pStm->Write(&TileSubIndex, sizeof(TileSubIndex), &written);
    if (hr < 0 || written != sizeof(TileSubIndex)) return false;
    hr = pStm->Write(&Overlay, sizeof(Overlay), &written);
    if (hr < 0 || written != sizeof(Overlay)) return false;
    hr = pStm->Write(&OverlayData, sizeof(OverlayData), &written);
    if (hr < 0 || written != sizeof(OverlayData)) return false;
    hr = pStm->Write(&Smudge, sizeof(Smudge), &written);
    if (hr < 0 || written != sizeof(Smudge)) return false;
    hr = pStm->Write(&SmudgeData, sizeof(SmudgeData), &written);
    if (hr < 0 || written != sizeof(SmudgeData)) return false;
    hr = pStm->Write(&Altitude, sizeof(Altitude), &written);
    if (hr < 0 || written != sizeof(Altitude)) return false;
    hr = pStm->Write(&Slope, sizeof(Slope), &written);
    if (hr < 0 || written != sizeof(Slope)) return false;
    hr = pStm->Write(&TiberiumValue, sizeof(TiberiumValue), &written);
    if (hr < 0 || written != sizeof(TiberiumValue)) return false;
    hr = pStm->Write(&WallOwner, sizeof(WallOwner), &written);
    if (hr < 0 || written != sizeof(WallOwner)) return false;
    hr = pStm->Write(&CrateType, sizeof(CrateType), &written);
    if (hr < 0 || written != sizeof(CrateType)) return false;

    return true;
}

// ----------------------------------------------------------------------------
// Compute_CRC - feed the cell's gameplay-relevant fields into the CRC engine.
// ----------------------------------------------------------------------------
void CellClass::Compute_CRC(CRCEngine& crc) const {
    crc.AddData(&MapCoords, sizeof(MapCoords));
    crc.AddData(&CellIndex, sizeof(CellIndex));
    uint32 flagsVal = static_cast<uint32>(Flags);
    crc.AddData(&flagsVal, sizeof(flagsVal));
    int32 landVal = static_cast<int32>(Land);
    crc.AddData(&landVal, sizeof(landVal));
    crc.AddData(&TileType, sizeof(TileType));
    crc.AddData(&TileSubIndex, sizeof(TileSubIndex));
    crc.AddData(&Overlay, sizeof(Overlay));
    crc.AddData(&OverlayData, sizeof(OverlayData));
    crc.AddData(&Smudge, sizeof(Smudge));
    crc.AddData(&SmudgeData, sizeof(SmudgeData));
    crc.AddData(&Altitude, sizeof(Altitude));
    crc.AddData(&Slope, sizeof(Slope));
    crc.AddData(&TiberiumValue, sizeof(TiberiumValue));
    crc.AddData(&WallOwner, sizeof(WallOwner));
    crc.AddData(&CrateType, sizeof(CrateType));
}

// ============================================================================
// Coordinate accessors
//
// The isometric grid maps cell (X, Y) to world coordinates by:
//   WorldX = (X - Y) * (LeptonsPerCell / 2)
//   WorldY = (X + Y) * (LeptonsPerCell / 4)
// The screen position is the same formula but in pixels:
//   PixelX = originX + (X - Y) * (CellWidthInPixels / 2)
//   PixelY = originY + (X + Y) * (CellHeightInPixels / 2)
// The Z coordinate is the cell's altitude * LevelHeight.
// ============================================================================

CoordStruct CellClass::Get_CellCoords() const {
    CoordStruct coord;
    coord.X = (static_cast<int32>(MapCoords.X) - static_cast<int32>(MapCoords.Y))
              * (LeptonsPerCell / 2);
    coord.Y = (static_cast<int32>(MapCoords.X) + static_cast<int32>(MapCoords.Y))
              * (LeptonsPerCell / 4);
    coord.Z = Altitude * LevelHeight;
    return coord;
}

CoordStruct CellClass::Get_Cell_Position() const {
    return Get_CellCoords();
}

Point2D CellClass::Get_Cell_Screen_Position(int32 originX, int32 originY) const {
    Point2D pt;
    pt.X = originX
         + (static_cast<int32>(MapCoords.X) - static_cast<int32>(MapCoords.Y))
           * (CellWidthInPixels / 2);
    pt.Y = originY
         + (static_cast<int32>(MapCoords.X) + static_cast<int32>(MapCoords.Y))
           * (CellHeightInPixels / 2);
    // Subtract altitude so taller cells appear higher on screen
    pt.Y -= Altitude * (CellHeightInPixels / 2);
    return pt;
}

// ============================================================================
// Map bounds / visibility
// ============================================================================

bool CellClass::Is_On_Map() const {
    if (CellIndex < 0) return false;
    if (!MapClass::Instance) return false;
    return CellIndex < MapClass::Instance->MapSize;
}

bool CellClass::Is_Visible() const {
    // A cell is visible to the player if it has been revealed and is not
    // currently shrouded by fog of war.
    return HasFlag(CellFlags::Revealed) && !HasFlag(CellFlags::Fogged);
}

bool CellClass::Is_Discovered() const {
    // A cell is "discovered" (explored) if it has ever been revealed to the
    // player, even if it is currently fogged.
    return HasFlag(CellFlags::Explored) || HasFlag(CellFlags::Revealed);
}

// ============================================================================
// Tiberium management
// ============================================================================

int32 CellClass::Get_Tiberium_Type() const {
    // The tiberium type is encoded in the OverlayData field when the cell
    // contains a tiberium overlay. In the original game, overlay indices
    // 0x01-0x04 correspond to the four tiberium types.
    if (Overlay < 0 || TiberiumValue <= 0) return -1;
    return OverlayData & 0xFF;
}

int32 CellClass::Get_Tiberium_Value() const {
    return TiberiumValue;
}

void CellClass::Set_Tiberium(int32 type, int32 value) {
    if (value < 0) value = 0;
    if (value > 12) value = 12;  // maximum growth level
    TiberiumValue = value;
    if (value > 0) {
        OverlayData = (OverlayData & ~0xFF) | (type & 0xFF);
        if (Overlay < 0) Overlay = 0;  // ensure overlay is set
        Land = ::LandType::Tiberium;
    } else {
        // When tiberium is depleted, restore the underlying land type
        if (Land == ::LandType::Tiberium) {
            Land = ::LandType::Clear;
        }
    }
    Recalc_Attributes();
}

// ============================================================================
// Overlay management
// ============================================================================

int32 CellClass::Get_Overlay() const {
    return Overlay;
}

int32 CellClass::Get_Overlay_Type() const {
    return Overlay;
}

void CellClass::Set_Overlay(int32 overlayIndex, int32 overlayData) {
    Overlay = overlayIndex;
    OverlayData = overlayData;
    Recalc_Attributes();
}

// ============================================================================
// Smudge management
// ============================================================================

int32 CellClass::Get_Smudge() const {
    return Smudge;
}

int32 CellClass::Get_Smudge_Type() const {
    return Smudge;
}

void CellClass::Set_Smudge(int32 smudgeIndex, int32 smudgeData) {
    Smudge = smudgeIndex;
    SmudgeData = smudgeData;
}

// ============================================================================
// Terrain object management
// ============================================================================

TerrainClass* CellClass::Get_Terrain() const {
    return Terrain;
}

void CellClass::Set_Terrain(TerrainClass* pTerrain) {
    Terrain = pTerrain;
}

void CellClass::Clear_Terrain() {
    Terrain = nullptr;
}

// ============================================================================
// Land type accessors
// ============================================================================

::LandType CellClass::Get_Land_Type() const {
    return Land;
}

void CellClass::Set_Land_Type(::LandType land) {
    Land = land;
    CellColor = Compute_CellColor_ForLand(land);
}

// ============================================================================
// Height accessors
// ============================================================================

int32 CellClass::Get_Ground_Height() const {
    return Altitude;
}

int32 CellClass::Get_Z_Height() const {
    return Altitude * LevelHeight;
}

// ============================================================================
// Occupier management
// ============================================================================

int32 CellClass::Get_Occupier_Count() const {
    return Occupier ? 1 : 0;
}

ObjectClass* CellClass::Get_Occupier() const {
    return Occupier;
}

void CellClass::Add_Occupier(ObjectClass* pObj) {
    if (!pObj) return;
    Occupier = pObj;
    SetAltFlag(AltCellFlags::ContainsBuilding, true);
}

void CellClass::Remove_Occupier(ObjectClass* pObj) {
    if (!pObj) return;
    if (Occupier == pObj) {
        Occupier = nullptr;
        SetAltFlag(AltCellFlags::ContainsBuilding, false);
    }
}

// ============================================================================
// Occupier type checks
// ============================================================================

bool CellClass::Has_Unit() const {
    if (!Occupier) return false;
    return Occupier->WhatAmI() == AbstractType::Unit;
}

bool CellClass::Has_Building() const {
    if (!Occupier) return false;
    return Occupier->WhatAmI() == AbstractType::Building;
}

bool CellClass::Has_Infantry() const {
    if (!Occupier) return false;
    return Occupier->WhatAmI() == AbstractType::Infantry;
}

// ============================================================================
// Foundation
// ============================================================================

bool CellClass::Is_Foundation() const {
    // A cell is part of a building foundation if it has a building occupier
    // or the ContainsBuilding alt flag is set.
    return HasAltFlag(AltCellFlags::ContainsBuilding) || Has_Building();
}

bool CellClass::Get_Foundation() const {
    return Is_Foundation();
}

// ============================================================================
// Radar cell color
//
// The radar minimap renders each cell as a single pixel whose colour is
// derived from the cell's land type. The original game uses a lookup table
// that maps LandType to an 8-bit palette index.
// ============================================================================

int32 CellClass::Cell_Color() const {
    return CellColor;
}

// ============================================================================
// Buildability
// ============================================================================

bool CellClass::Can_Build_On() const {
    // A cell is buildable if it is clear land, not occupied, not water,
    // not a wall, not a cliff, and not shrouded.
    if (IsOccupied()) return false;
    if (IsWater()) return false;
    if (IsWall()) return false;
    if (IsRock()) return false;
    if (Is_Cliff()) return false;
    if (IsShrouded()) return false;
    if (Terrain != nullptr) return false;
    if (Overlay >= 0 && TiberiumValue > 0) return false;  // can't build on tiberium
    return true;
}

bool CellClass::Is_Buildable() const {
    return Can_Build_On();
}

bool CellClass::Is_Concrete() const {
    // A cell is "concrete" (paved) if its land type is Road or if it has
    // a concrete-type overlay. This affects movement speed for vehicles.
    return Land == ::LandType::Road;
}

bool CellClass::Is_Cliff() const {
    // Cliffs are identified by slope values in the cliff range.
    // The original game uses slope types 1-4 for directional cliffs.
    return Slope >= 1 && Slope <= 4;
}

// ============================================================================
// Slope
// ============================================================================

int32 CellClass::Get_Slope() const {
    return Slope;
}

bool CellClass::Is_Sloped() const {
    return Slope > 0;
}

// ============================================================================
// Damage application
//
// When a cell takes damage (e.g. from an explosion), the damage is applied
// to the cell's overlay (walls, bridges) and terrain objects (trees, rocks).
// ============================================================================

void CellClass::Apply_Damage(int32 damage, int32 warheadType) {
    if (damage <= 0) return;

    // Damage terrain objects (trees, etc.)
    if (Terrain) {
        // The original game calls Terrain->TakeDamage(damage, warhead).
        // If the terrain is destroyed, it is removed from the cell.
        TerrainClass* pTerrain = Terrain;
        pTerrain->Health -= damage;
        if (pTerrain->Health <= 0) {
            Clear_Terrain();
            // The terrain object destruction spawns debris/animation
            // via the game's damage system.
        }
    }

    // Damage overlays (walls, bridges)
    if (Overlay >= 0) {
        // Walls have health stored in OverlayData. When a wall's health
        // reaches zero, the wall is destroyed and the overlay is removed.
        if (Land == ::LandType::Wall) {
            int32 wallHealth = OverlayData;
            wallHealth -= damage;
            if (wallHealth <= 0) {
                Overlay = -1;
                OverlayData = 0;
                Land = ::LandType::Clear;
                WallOwner = -1;
            } else {
                OverlayData = wallHealth;
            }
        }

        // Bridges are damaged and can be destroyed
        if (IsBridge()) {
            // Bridge destruction is handled by the map's bridge system.
            // We mark the bridge as damaged by clearing the bridge flags.
            if (damage >= 100) {
                SetFlag(CellFlags::Bridge, false);
            }
        }
    }

    // Damage tiberium (reduce the tiberium value)
    if (TiberiumValue > 0) {
        TiberiumValue -= damage / 10;
        if (TiberiumValue <= 0) {
            TiberiumValue = 0;
            if (Land == ::LandType::Tiberium) {
                Land = ::LandType::Clear;
            }
        }
    }

    Recalc_Attributes();
}

// ============================================================================
// Rendering
//
// Draw_It renders the cell's terrain layer to the screen. In the original
// game this is a complex function that draws the tile graphic, overlay,
// smudge, and terrain object. For the reconstruction we provide the
// structural framework.
// ============================================================================

void CellClass::Draw_It(int32 originX, int32 originY) const {
    // Calculate the screen position of this cell
    Point2D screenPos = Get_Cell_Screen_Position(originX, originY);

    // The actual pixel rendering is performed by the display subsystem.
    // This method orchestrates the draw order:
    //   1. Base tile (terrain graphic)
    //   2. Smudge (scorch marks, craters)
    //   3. Overlay (walls, tiberium)
    //   4. Terrain object (trees, rocks)
    //   5. Occupier (units, buildings, infantry)
    //
    // Each layer is only drawn if the cell is visible (not shrouded).
    // Shrouded cells draw the shroud graphic instead.

    (void)screenPos;  // screen position is used by the display subsystem

    // In a full implementation, this would call:
    //   DisplayClass::Draw_Tile(screenPos, TileType, TileSubIndex);
    //   if (Smudge >= 0) DisplayClass::Draw_Smudge(screenPos, Smudge, SmudgeData);
    //   if (Overlay >= 0) DisplayClass::Draw_Overlay(screenPos, Overlay, OverlayData);
    //   if (Terrain) Terrain->Draw_It(screenPos);
    //   if (Occupier) Occupier->Draw_It(screenPos);
}

// ============================================================================
// Shroud management
// ============================================================================

void CellClass::Set_Shrouded(bool shrouded) {
    if (shrouded) {
        // Clear the revealed flags to shroud the cell
        Flags = static_cast<CellFlags>(
            static_cast<uint32>(Flags) &
            ~static_cast<uint32>(CellFlags::Revealed));
    } else {
        // Set the revealed flags to unshroud the cell
        SetFlag(CellFlags::Revealed, true);
        SetFlag(CellFlags::Explored, true);
    }
}

void CellClass::Unshroud() {
    SetFlag(CellFlags::Revealed, true);
    SetFlag(CellFlags::Explored, true);
    SetFlag(CellFlags::Fogged, false);
}
