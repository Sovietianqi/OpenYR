#include "CellClass.h"
#include <vector>
#include <queue>
#include <utility>
#include "../Abstract/ObjectClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/TerrainClass.h"
#include "../Abstract/OverlayClass.h"
#include "../Abstract/OverlayTypeClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Externs.h"
#include "../Objects/IsometricTileGlobals.h"
#include "../Special/TiberiumClass.h"
#include "../Abstract/SmudgeClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Game/Game.h"
#include "../Game/GameInit.h"
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
    AttachedTag = nullptr;
    CellColor = 0;
    Altitude = 0;
    Slope = 0;
    TiberiumValue = 0;
    WallOwner = -1;
    CrateType = 0;
    IsUnderShroud = false;
    GapsCoveringCell = 0;
    Field_12C = 0;
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
    AttachedTag = nullptr;
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
    // CellClass_GetContainedTiberiumIndex: the cell's overlay index is resolved
    // through the tiberium registry into the ore type ordinal, or -1.
    return Is_Overlay_Idx_Tiberium(Overlay);
}

int32 CellClass::Get_Contained_Tiberium_Index() const {
    return Is_Overlay_Idx_Tiberium(Overlay);
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

// CellClass_StopAmbientSound (asm 0x5F6CB0).
//
//   Silences the ambient sound attached to this cell.  The original prefers
//   the building occupying the cell and only falls back to the terrain object
//   when the cell holds no building.  Both are asked to stop their ambient
//   loop with the "-1" flag.
void CellClass::Silence_Attached_Ambient() {
    ObjectClass* pObject = First_Object(false);
    if (pObject == nullptr) {
        pObject = Terrain;
    }
    if (pObject != nullptr) {
        pObject->StopAmbientSound(-1);
    }
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

    // Thread onto the ground occupancy chain unless it is already linked.
    if (pObj->NextObject == nullptr && FirstObject != pObj) {
        pObj->NextObject = FirstObject;
        FirstObject = pObj;
    }

    Occupier = pObj;
    SetAltFlag(AltCellFlags::ContainsBuilding, true);
}

void CellClass::Remove_Occupier(ObjectClass* pObj) {
    if (!pObj) return;

    // Unlink from the ground occupancy chain.
    if (FirstObject == pObj) {
        FirstObject = pObj->NextObject;
        pObj->NextObject = nullptr;
    } else {
        for (ObjectClass* pPrev = FirstObject; pPrev; pPrev = pPrev->NextObject) {
            if (pPrev->NextObject == pObj) {
                pPrev->NextObject = pObj->NextObject;
                pObj->NextObject = nullptr;
                break;
            }
        }
    }

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

// CellClass_Setup (asm 0x4CC180).
//
//   Re-derives this cell's shroud/fog state against the map's current view
//   rectangle.  A negative `flag` means "re-shroud if the cell has fallen
//   outside the visible rect"; the cell is also fogged whenever it is not
//   within the visible area.  Called for every cell by
//   ActionClass_ResizePlayerView after the view rectangle has been rewritten.
void CellClass::Setup(int32 flag)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    const int32 cx = static_cast<int32>(MapCoords.X);
    const int32 cy = static_cast<int32>(MapCoords.Y);

    const bool inside =
        cx >= pMap->VisibleRectX &&
        cy >= pMap->VisibleRectY &&
        cx <  pMap->VisibleRectX + pMap->VisibleRectWidth &&
        cy <  pMap->VisibleRectY + pMap->VisibleRectHeight;

    if (!inside)
    {
        if (flag < 0)
        {
            Set_Shrouded(true);
        }
        SetFlag(CellFlags::Fogged, true);
        return;
    }

    SetFlag(CellFlags::Fogged, false);
}

// ============================================================================
// Hierarchical pathfinding
// ============================================================================
bool CellClass::Pathfinding_Hierarchical(const CellStruct& from, const CellStruct& to,
                                         DynamicVectorClass<CellStruct>& outPath,
                                         MovementZone zone)
{
    outPath.Clear();

    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || pMap->CellArray == nullptr)
        return false;

    const int32 width  = pMap->MapWidth;
    const int32 height = pMap->MapHeight;
    if (width <= 0 || height <= 0)
        return false;

    // Block abstraction: 8x8 cells per block.
    static const int32 BLOCK = 8;
    const int32 bw = (width  + BLOCK - 1) / BLOCK;
    const int32 bh = (height + BLOCK - 1) / BLOCK;

    const int32 fromBX = from.X / BLOCK;
    const int32 fromBY = from.Y / BLOCK;
    const int32 toBX   = to.X / BLOCK;
    const int32 toBY   = to.Y / BLOCK;

    if (fromBX == toBX && fromBY == toBY)
    {
        // Same block: nothing for the abstraction layer to decide; the
        // per-cell search handles it.
        return false;
    }

    // Representative cell per block: the first passable cell inside the
    // block, used as the connection point for the abstract graph.
    struct BlockNode {
        int32 RepX;
        int32 RepY;
        bool  HasRep;
        int32 G;
        int32 F;
        int32 CameFrom;
        bool  Closed;
    };

    const int32 blockCount = bw * bh;
    std::vector<BlockNode> nodes(blockCount);
    for (int32 by = 0; by < bh; ++by)
    {
        for (int32 bx = 0; bx < bw; ++bx)
        {
            BlockNode& n = nodes[by * bw + bx];
            n.RepX = -1;
            n.RepY = -1;
            n.HasRep = false;
            n.G = 0x7FFFFFFF;
            n.F = 0x7FFFFFFF;
            n.CameFrom = -1;
            n.Closed = false;

            // Scan the block for a passable representative cell.
            for (int32 y = by * BLOCK; y < (by + 1) * BLOCK && !n.HasRep; ++y)
            {
                for (int32 x = bx * BLOCK; x < (bx + 1) * BLOCK && !n.HasRep; ++x)
                {
                    if (x >= width || y >= height)
                        continue;
                    CellClass* pCell = &pMap->CellArray[y * width + x];
                    if (pCell != nullptr && pCell->PassableFor(zone))
                    {
                        n.RepX = x;
                        n.RepY = y;
                        n.HasRep = true;
                    }
                }
            }
        }
    }

    const int32 startIdx = fromBY * bw + fromBX;
    const int32 goalIdx  = toBY * bw + toBX;
    if (!nodes[startIdx].HasRep || !nodes[goalIdx].HasRep)
        return false;

    auto blockDist = [](int32 ax, int32 ay, int32 bx, int32 by) {
        int32 dx = (bx > ax) ? (bx - ax) : (ax - bx);
        int32 dy = (by > ay) ? (by - ay) : (ay - by);
        int32 diag = (dx < dy) ? dx : dy;
        int32 straight = dx + dy - 2 * diag;
        return straight * 256 + diag * 362;
    };

    // A* over the block graph (8-way).
    std::priority_queue<std::pair<int32, int32>,
                        std::vector<std::pair<int32, int32>>,
                        std::greater<std::pair<int32, int32>>> openList;

    nodes[startIdx].G = 0;
    nodes[startIdx].F = blockDist(fromBX, fromBY, toBX, toBY);
    openList.push({ nodes[startIdx].F, startIdx });

    static const int8 ddx[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };
    static const int8 ddy[8] = { -1, -1,  0,  1,  1,  1,  0, -1 };

    while (!openList.empty())
    {
        auto [curF, cur] = openList.top();
        openList.pop();
        if (nodes[cur].Closed)
            continue;
        if (cur == goalIdx)
            break;
        nodes[cur].Closed = true;

        const int32 cx = cur % bw;
        const int32 cy = cur / bw;

        for (int32 d = 0; d < 8; ++d)
        {
            int32 nx = cx + ddx[d];
            int32 ny = cy + ddy[d];
            if (nx < 0 || ny < 0 || nx >= bw || ny >= bh)
                continue;

            int32 nIdx = ny * bw + nx;
            if (nodes[nIdx].Closed || !nodes[nIdx].HasRep)
                continue;

            // Diagonal corner-cut check at the block level: the two
            // orthogonal neighbour blocks must also be reachable.
            if (d & 1)
            {
                int32 aIdx = (cy + ddy[d - 1]) * bw + (cx + ddx[d - 1]);
                int32 bIdx = (cy + ddy[(d + 1) & 7]) * bw + (cx + ddx[(d + 1) & 7]);
                if (!nodes[aIdx].HasRep || !nodes[bIdx].HasRep)
                    continue;
            }

            int32 moveCost = (d & 1) ? 362 : 256;
            int32 tentative = nodes[cur].G + moveCost;
            if (tentative < nodes[nIdx].G)
            {
                nodes[nIdx].G = tentative;
                nodes[nIdx].F = tentative + blockDist(nx, ny, toBX, toBY);
                nodes[nIdx].CameFrom = cur;
                openList.push({ nodes[nIdx].F, nIdx });
            }
        }
    }

    if (nodes[goalIdx].G == 0x7FFFFFFF)
        return false;   // no abstract route

    // Emit the abstract waypoint chain: start cell, each traversed block's
    // representative cell, and the destination.
    std::vector<int32> rev;
    for (int32 c = goalIdx; c != -1; c = nodes[c].CameFrom)
    {
        rev.push_back(c);
        if (c == startIdx)
            break;
    }

    outPath.Add(from);
    for (auto it = rev.rbegin(); it != rev.rend(); ++it)
    {
        const BlockNode& n = nodes[*it];
        outPath.Add(CellStruct(static_cast<int16>(n.RepX), static_cast<int16>(n.RepY)));
    }
    outPath.Add(to);

    return true;
}

// ============================================================================
// CellClass::Get_Movement_Cost - asm `LandCharacteristics.Foot[land*9 + speed]`
//
//   The cell's own land type selects the row and the mover's speed class the
//   column.  A zero entry means "this combination cannot traverse the cell at
//   all"; the callers treat that as an outright rejection.
// ============================================================================

int32 CellClass::Get_Movement_Cost(int32 SpeedType) const
{
    const int32 landIndex = static_cast<int32>(Land);
    if (landIndex < 0 || landIndex >= RulesClass::LAND_TYPE_COUNT) {
        return 1;
    }
    if (SpeedType < 0 || SpeedType >= 9) {
        return 1;
    }

    if (TheRules == nullptr) {
        return 1;
    }

    const RulesClass::LandTypeCharacteristics& row =
        TheRules->LandCharacteristics[landIndex];

    // The seven named floats are immediately followed by the Buildable flag,
    // so walking the struct as a float array reproduces the binary's flat
    // indexing exactly.
    const float* values = &row.Hover;
    return static_cast<int32>(values[SpeedType]);
}

// ============================================================================
// CellClass::Is_Clear_To_Move - asm 0x4834DA
//
//   The movement-legal test used when the map searches for a nearby free spot
//   and when a unit is placed.  It is a sequence of cheap rejections followed
//   by the land-characteristics lookup:
//
//     * SpeedType 4 (Winged) always passes - aircraft ignore the ground.
//     * When a5 is not -1 the cell's zone must equal it.
//     * The cell's "land" byte must match a7, except that a bridge cell also
//       accepts a7 == land + 4.
//     * The movement field must be empty after applying the a3 / a4 masks.
//     * A crushable overlay rejects every zone except the crushers.
//     * Finally the land/speed movement cost must be non-zero.
// ============================================================================

bool CellClass::Is_Clear_To_Move(int32 SpeedType, bool a3, bool a4, int32 a5,
                                 MovementZone zone, int32 a7, bool boo) const
{
    // Aircraft never need a ground path.
    if (SpeedType == 4) {
        return true;
    }

    // Zone check (the binary compares against MapClass_CanLocationBeReached,
    // whose result is the cell's zone index).
    if (a5 != -1) {
        if (ZoneIndex != a5) {
            return false;
        }
    }

    // Expected land byte, with the bridge adjustment.
    const bool onBridge = IsBridge();
    if (a7 != -1) {
        const int32 landByte = Get_Land_Byte();
        if (a7 != landByte) {
            if (!(onBridge && a7 == landByte + 4)) {
                return false;
            }
        }
    } else if (onBridge && !boo) {
        // A bridge cell that the caller did not mark as a bridge-layer mover.
        // (The binary checks this against the *requested* land; with a7 == -1
        // it only rejects when the mover is not bridge-aware.)
    }

    // Movement field: pick the bridge or the ground variant, then apply the
    // caller's masks and require the result to be empty.
    uint8 movement = 0;
    const bool wantBridgeVariant = (a7 == -1) ||
                                   (onBridge && a7 != Get_Land_Byte());
    if (onBridge && wantBridgeVariant) {
        movement = Get_Movement_Field_Bridge();
    } else {
        movement = Get_Movement_Field();
    }

    uint32 bits = movement;
    if (a3) {
        bits &= 0xE0u;      // keep only the high three bits
    }
    if (a4) {
        bits &= 0x5Fu;      // drop the "crushable" bit
    }
    if (bits != 0) {
        return false;
    }

    // A crushable overlay narrows the set of zones that may enter.
    //
    // The binary loads the overlay type and bails straight to the cost test
    // when either the overlay or its Crushable byte is absent.  Otherwise it
    // dispatches on the movement zone:
    //
    //   Destroyer / AmphibiousDestroyer / InfantryDestroyer  -> allowed
    //   Crusher / AmphibiousCrusher                          -> allowed only
    //                                                           when the
    //                                                           overlay is not
    //                                                           Crushable
    //   CrusherAll                                           -> allowed
    //   anything else                                        -> rejected
    bool overlayCleared = false;

    if (Overlay != -1) {
        OverlayTypeClass* pOverlayType = OverlayTypeClass::FindByIndex(Overlay);
        if (pOverlayType != nullptr && pOverlayType->Crushable) {
            const bool destroyerZone =
                zone == MovementZone::Destroyer ||               // 2
                zone == MovementZone::AmphibiousDestroyer ||     // 3
                zone == MovementZone::InfantryDestroyer;         // 8

            const bool crusherZone =
                zone == MovementZone::Crusher ||                 // 1
                zone == MovementZone::AmphibiousCrusher;         // 4

            if (destroyerZone || zone == MovementZone::CrusherAll) {
                overlayCleared = true;
            } else if (crusherZone) {
                // A crusher may only flatten a non-crushable wall.
                if (!pOverlayType->Crushable) {
                    overlayCleared = true;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    // Land / speed movement cost.  Zero means the mover cannot traverse this
    // land type at all; the overlay note above is the only way past it.
    const int32 cost = Get_Movement_Cost(SpeedType);
    if (cost == 0 && !overlayCleared) {
        return false;
    }

    return true;
}

// ============================================================================
// CellClass::Get_Land_Byte
//
//   The original keeps a small integer at +0x11B that groups the twelve land
//   types into the coarse classes the movement table is indexed by.  The
//   caller-side comparisons (a7 == land, a7 == land + 4 for bridges) work on
//   this value, so it is exposed directly from the land type.
// ============================================================================

int32 CellClass::Get_Land_Byte() const
{
    return static_cast<int32>(Land);
}

// ============================================================================
// CellClass::Get_Movement_Field / Get_Movement_Field_Bridge
//
//   Two per-cell bytes the assembly tests after masking.  A non-zero result
//   means the cell carries some movement restriction that the requesting mover
//   did not opt out of.
// ============================================================================

uint8 CellClass::Get_Movement_Field() const
{
    return MovementField;
}

uint8 CellClass::Get_Movement_Field_Bridge() const
{
    return MovementFieldBridge;
}

// ============================================================================
// CellClass::Tile_Is*  - the tile-classification predicates
//
//   Every entry reads the cell's isometric tile type index (+0x38) and tests
//   it against the special tile-set ordinals that the tileset INI published
//   through IsometricTileType::Apply_Special_Tile_Indices.  A range test is
//   written as "ordinal <= index < ordinal + count"; the guard on the ordinal
//   being -1 (tile set missing from this theater) makes the whole range test
//   fall through to false, which is why the assembly compares against
//   0FFFFFFFFh first.
//
//   Tile_IsCliff is the big one: it accepts the cliff set, all four waterfalls
//   (with a directional face test on the passability byte at +0x11A), the
//   cliff ramps, the water caves, both bridge sets and finally the destroyable
//   and water cliff ranges.
// ============================================================================

int32                                   TubeCount = 0;
DynamicVectorClass<CellClass*>*         vec_Tubes = nullptr;

namespace {

// The index ranges the binary compares against are always "ordinal .. ordinal
// + span".  A missing tile set publishes -1, and the assembly tests that up
// front, so a negative ordinal can never match a non-negative index.
inline bool TileRange(int32 index, int32 ordinal, int32 span)
{
    return ordinal != -1 && index >= ordinal && index < ordinal + span;
}

} // namespace

bool CellClass::Tile_IsWater() const
{
    return TileRange(TileType, tile_WaterSet, 0x0E);
}

bool CellClass::Tile_IsNotWater() const
{
    return !TileRange(TileType, tile_WaterSet, 0x0E);
}

bool CellClass::Tile_IsBridge() const
{
    return TileRange(TileType, tile_BridgeSet, 0x10);
}

bool CellClass::Tile_IsWoodBridge() const
{
    return TileRange(TileType, tile_WoodBridgeSet, 0x10);
}

bool CellClass::Tile_IsDestroyableCliff() const
{
    // Two adjacent tiles: the base and the base + 1.
    return TileType == tile_DestroyableCliffs
        || TileType == tile_DestroyableCliffs + 1;
}

bool CellClass::Tile_IsRamp() const
{
    if (TileType >= tile_RampBase && TileType < tile_RampBase + 0x14)
        return true;

    return TileType >= tile_RampSmooth && TileType < tile_RampSmooth + 0x0C;
}

bool CellClass::Tile_IsGreen() const
{
    if (TileType == tile_GreenTile)
        return true;

    return TileRange(TileType, tile_ClearToGreenLat, 0x10);
}

bool CellClass::Tile_IsBlank() const
{
    return TileType == 0xFFFF || TileType == 0;
}

bool CellClass::Tile_IsShorePieces() const
{
    return TileRange(TileType, tile_ShorePieces, 0x2A);
}

bool CellClass::Tile_IsWet() const
{
    if (TileRange(TileType, tile_ShorePieces, 0x2A))
        return true;

    if (TileRange(TileType, tile_WaterSet, 0x0E))
        return true;

    if (TileRange(TileType, tile_WaterfallEast, 4))
        return true;

    if (TileRange(TileType, tile_WaterfallWest, 4))
        return true;

    if (TileRange(TileType, tile_WaterfallSouth, 4))
        return true;

    return TileRange(TileType, tile_WaterfallNorth, 4);
}

bool CellClass::Tile_IsMiscPave() const
{
    return TileRange(TileType, tile_MiscPaveTile, 0x0E);
}

bool CellClass::Tile_IsPave() const
{
    return TileRange(TileType, tile_PaveTile, 0x10);
}

bool CellClass::Tile_IsDirtRoad() const
{
    if (TileRange(TileType, tile_DirtRoadJunction, 0x0B))
        return true;

    if (TileRange(TileType, tile_DirtRoadCurve, 0x18))
        return true;

    return TileRange(TileType, tile_DirtRoadStraight, 0x42);
}

bool CellClass::Tile_IsPavedRoad() const
{
    return TileRange(TileType, tile_PavedRoads, 0x0F);
}

bool CellClass::Tile_IsPavedRoadEnd() const
{
    return TileRange(TileType, tile_PavedRoadEnds, 4);
}

bool CellClass::Tile_IsPavedRoadSlope() const
{
    return TileRange(TileType, tile_PavedRoadSlopes, 4);
}

bool CellClass::Tile_IsMedian() const
{
    return TileRange(TileType, tile_Medians, 0x0E);
}

bool CellClass::Tile_IsClearToSandLat() const
{
    return TileRange(TileType, tile_ClearToSandLat, 0x10);
}

bool CellClass::Tile_IsATunnel() const
{
    // A tunnel tile is one whose tube index (+0x118) is a live index into
    // vec_Tubes *and* whose land type is the Tunnel classification.
    if (TubeIndex < 0)
        return false;

    if (TubeIndex >= TubeCount)
        return false;

    return Land == ::LandType::Tunnel;
}

bool CellClass::Tile_IsCliff() const
{
    if (TileRange(TileType, tile_CliffSet, 0x28))
        return true;

    // Each waterfall is a run of four tiles.  The first and the fourth of the
    // run are the two "faces" the cliff test rejects when the cell's
    // passability byte names that same face; the middle two always count.
    if (TileRange(TileType, tile_WaterfallEast, 4)) {
        if (TileType == tile_WaterfallEast)
            return Passability != 0 && Passability != 4;
        if (TileType != tile_WaterfallEast + 3)
            return true;
    }

    if (TileRange(TileType, tile_WaterfallWest, 4)) {
        if (TileType == tile_WaterfallWest)
            return Passability != 1 && Passability != 3;
        if (TileType != tile_WaterfallWest + 3)
            return true;
    }

    if (TileRange(TileType, tile_WaterfallSouth, 4)) {
        if (TileType == tile_WaterfallSouth)
            return Passability != 0 && Passability != 1;
        if (TileType != tile_WaterfallSouth + 3)
            return true;
    }

    if (TileRange(TileType, tile_WaterfallNorth, 4)) {
        if (TileType == tile_WaterfallNorth)
            return Passability != 2 && Passability != 3;
        if (TileType != tile_WaterfallNorth + 3)
            return true;
    }

    if (TileRange(TileType, tile_CliffRamps, 0x14))
        return true;

    if (TileRange(TileType, tile_WaterCaves, 4))
        return true;

    if (TileRange(TileType, tile_BridgeSet, 0x10))
        return true;

    if (TileRange(TileType, tile_WoodBridgeSet, 0x10))
        return true;

    if (TileRange(TileType, tile_DestroyableCliffs, 2))
        return true;

    return TileRange(TileType, tile_WaterCliffs, 0x1C);
}

// ============================================================================
// CellClass radiation
//
//   RadLevel is a double the RadSiteClass update loop accumulates into each
//   cell it covers.  The four small predicates here are the ones the rest of
//   the engine asks for: Is_Radiated (anything above 1.0), and Get_RadLevel
//   (the level clamped to RulesData->RadLevelMax and then floored to an int,
//   which is the value FootClass::AI feeds into the damage roll).
// ============================================================================

void CellClass::RadLevel_Increase(double amount)
{
    RadLevel += amount;
}

void CellClass::RadLevel_Decrease(double amount)
{
    RadLevel -= amount;

    // The original tests the result against zero and stores a hard zero when
    // it went negative, so a cell never retains a negative dose.
    if (RadLevel < 0.0)
        RadLevel = 0.0;
}

void CellClass::Set_Rad_Site(void* pRadSite)
{
    RadSite = pRadSite;
}

bool CellClass::Is_Radiated() const
{
    return RadLevel > 1.0;
}

int32 CellClass::Get_RadLevel() const
{
    // fld maxLevel / fld RadLevel / fcomp st(1) / test ah,1 / jz floor.
    // The compare only branches on the unordered flag; for every ordered pair
    // the value left on the FPU stack is the RadLevel pushed second and the
    // max level is simply dropped.  The result is therefore the floor of the
    // cell's dose with no clamp, which is what is reproduced here.
    const int32 maxLevel = TheRules->RadLevelMax;
    (void)maxLevel;

    return static_cast<int32>(RadLevel);
}

// ============================================================================
// CellClass::Tiberium_In_Cell / Contains_Tiberium
//
//   TiberiumInCell resolves the cell's overlay into an ore type and answers
//   that type's Value multiplied by (growth stage + 1), where the growth stage
//   is the byte the overlay bookkeeping keeps at +0x11E.  An overlay that is
//   not ore, or a cell with no overlay, yields zero.
//
//   ContainsTiberium is the bare land-type test the harvest code uses first.
// ============================================================================

int32 CellClass::Tiberium_In_Cell() const
{
    const int32 typeIndex = Is_Overlay_Idx_Tiberium(Overlay);
    if (typeIndex == -1)
        return 0;

    TiberiumClass* pType = TiberiumClass::FindByIndex(typeIndex);
    if (pType == nullptr)
        return 0;

    return pType->Value * (static_cast<int32>(OverlayFrame) + 1);
}

bool CellClass::Contains_Tiberium() const
{
    return Land == ::LandType::Tiberium;
}

// ============================================================================
// CellClass - per-house sensor / cloak-generator bookkeeping
//
//  every accessor below is a literal transcription of the assembly.
//  CloakGen stores a bit per house index in a 32-bit mask at +0x78; the
//  sensor and disguise-sensor counters are 16-bit words indexed by the house
//  ordinal at +0x7C and +0xAC respectively.  The building update code calls
//  Add/Rem as cloaking and sensor structures power on and off.
// ============================================================================

// CellClass_CloakGen_AddHouse (asm 0x4879xx): set bit idxHouse of the mask.
void CellClass::CloakGen_AddHouse(int32 idxHouse)
{
    CloakGenMask |= (1u << idxHouse);
}

// CellClass_CloakGen_RemHouse (asm 0x4879xx): clear bit idxHouse of the mask.
void CellClass::CloakGen_RemHouse(int32 idxHouse)
{
    CloakGenMask &= ~(1u << idxHouse);
}

// CellClass_CloakGen_HasHouse (asm 0x4879xx): test bit idxHouse of the mask.
bool CellClass::CloakGen_HasHouse(int32 idxHouse) const
{
    return (CloakGenMask & (1u << idxHouse)) != 0;
}

// CellClass_Sensors_AddHouse (asm 0x4878xx): one more of this house's sensors
// now reveals the cell.
void CellClass::Sensors_AddHouse(int32 idxHouse)
{
    ++SensedByHouses[idxHouse];
}

// CellClass_Sensors_RemHouse (asm 0x4878xx): one fewer.
void CellClass::Sensors_RemHouse(int32 idxHouse)
{
    --SensedByHouses[idxHouse];
}

// CellClass_Sensors_HasHouse (asm 0x4878xx): `setnle` on the 16-bit counter
// means "true when the count is strictly greater than zero".
bool CellClass::Sensors_HasHouse(int32 idxHouse) const
{
    return SensedByHouses[idxHouse] > 0;
}

// CellClass_DisguiseSensors_AddHouse (asm 0x4879xx).
void CellClass::DisguiseSensors_AddHouse(int32 idxHouse)
{
    ++DisguiseSensedByHouses[idxHouse];
}

// CellClass_DisguiseSensors_RemHouse (asm 0x4879xx).
void CellClass::DisguiseSensors_RemHouse(int32 idxHouse)
{
    --DisguiseSensedByHouses[idxHouse];
}

// CellClass_DisguiseSensors_HasHouse (asm 0x4879xx).
bool CellClass::DisguiseSensors_HasHouse(int32 idxHouse) const
{
    return DisguiseSensedByHouses[idxHouse] > 0;
}

// ============================================================================
// CellClass - tunnel / coordinate / identity / flag batch
// ============================================================================

// CellClass_GetTunnel (asm 0x484F2B).
//
//  Validates the 16-bit tube index at +0x118 against the tunnel table length
//  and returns the cell the table holds; a negative index or an out-of-range
//  one yields null.
CellClass* CellClass::Get_Tunnel() const
{
    if (TubeIndex < 0)
        return nullptr;

    if (vec_Tubes == nullptr)
        return nullptr;

    if (TubeIndex >= vec_Tubes->Count)
        return nullptr;

    return vec_Tubes->Items[TubeIndex];
}

// CellClass_SetMapCoords (asm 0x47D3B8).
//
//  The original copies the four packed bytes of the MapCellExClass X/Y pair
//  straight into +0x24; storing the CellStruct produces the identical bytes.
void CellClass::Set_Map_Coords(const CellStruct& coords)
{
    MapCoords = coords;
}

// CellClass_GetAbstractID (asm 0x482Axx).  Every cell reports the fixed
// AbstractType::Cell ordinal.
int32 CellClass::Get_AbstractID() const
{
    return static_cast<int32>(AbstractType::Cell);
}

// CellClass_FlagPickedUp (asm 0x4834A0).
//
//  Clears the "flag is planted" bit (0x10) from the flag word at +0x140 and
//  resets the attached-object index at +0x50 to -1.  Returns true only when
//  the bit had actually been set.
bool CellClass::Flag_Picked_Up()
{
    if ((Field_140 & 0x10u) == 0)
        return false;

    Field_140 &= ~0x10u;
    Field_50 = -1;
    return true;
}

// CellClass_Smth0 (asm 0x487635).
//
//  Decrements the gap counter at +0x130, but a count of exactly 1 first
//  collapses to 0 so that the subsequent decrement wraps it to -1; the net
//  effect is that a single remaining gap generator takes the counter to -1
//  rather than 0.
void CellClass::Smth0()
{
    if (GapsCoveringCell == 1)
        GapsCoveringCell = 0;

    --GapsCoveringCell;
}

// CellClass_Smth2 (asm 0x4876xx).
//
//  Sets the shroud flag bits 0x18 on +0x12C and, while the gap counter is
//  still positive, tags 0x20 onto the flag word at +0x140.
void CellClass::Smth2()
{
    Field_12C |= 0x18u;

    if (GapsCoveringCell > 0)
        Field_140 |= 0x20u;
}

// ============================================================================
// Occupancy-list lookups
//
//  The binary keeps two singly-linked lists per cell, threaded through
//  ObjectClass::NextObject (+0x30): the ground list (+0xE4) and the altitude
//  ("flying") list (+0xE8).  Every lookup picks one of the two with its bool
//  argument, then walks the chain comparing WhatAmI() against the requested
//  abstract type.  A null head yields null immediately.
// ============================================================================

ObjectClass* CellClass::First_Object(bool alt) const {
    return alt ? AltObject : FirstObject;
}

ObjectClass* CellClass::GetUnit(bool alt) const {
    if (!GameActive) return nullptr;

    ObjectClass* pObj = First_Object(alt);
    while (pObj) {
        if (pObj->WhatAmI() == AbstractType::Unit)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

ObjectClass* CellClass::GetAircraft(bool alt) const {
    if (!GameActive) return nullptr;

    ObjectClass* pObj = First_Object(alt);
    while (pObj) {
        if (pObj->WhatAmI() == AbstractType::Aircraft)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

ObjectClass* CellClass::GetInfantry(bool alt) const {
    if (!GameActive) return nullptr;

    ObjectClass* pObj = First_Object(alt);
    while (pObj) {
        if (pObj->WhatAmI() == AbstractType::Infantry)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

// CellClass_CanAddTiberium (asm 0x4838F0).
//
//  A cell may receive new tiberium only when it is inside the radar range,
//  its flag word does not already carry the growth-suppressing bits 0x500,
//  and it holds no building.  (The trailing refinement in the original also
//  rejects cells whose tiberium value would exceed the type's cap, which is
//  folded into the caller.)
bool CellClass::CanAddTiberium() const {
    if (!MapClass::Instance || !MapClass::Instance->In_Radar(MapCoords, true))
        return false;

    if (Field_140 & 0x500u)
        return false;

    if (GameActive) {
        ObjectClass* pObj = FirstObject;
        while (pObj) {
            if (pObj->WhatAmI() == AbstractType::Building)
                return false;
            pObj = pObj->NextObject;
        }
    }

    return true;
}
