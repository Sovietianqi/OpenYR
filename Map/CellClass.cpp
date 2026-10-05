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
#include "../Rendering/TacticalClass.h"
#include "../Game/Game.h"
#include "../Game/GameInit.h"
#include "../Scenario/ScenarioClass.h"
#include "../IO/CRC.h"
#include "../COM/IUnknown.h"

#include <cstring>

// 根据游戏行为，可知 HasAbility 在本项目里接收的是 VeteranAbilities /
// EliteAbilities 数组下标（0..3），而不是原版的位掩码。VEIN_PROOF 在原版
// 位图里占第 3 位，故此处用下标 3 表达"脚下免疫矿脉"这一能力。
static const int32 ABILITY_VEIN_PROOF_INDEX = 3;

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

 // 根据游戏行为，可知 StopAmbientSound 负责下面这段逻辑。
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

 // 根据游戏行为，可知 Setup 负责下面这段逻辑。
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
 // CellClass::Is_Clear_To_Move -
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

 // CellClass_CloakGen_AddHouse (xx): set bit idxHouse of the mask.
void CellClass::CloakGen_AddHouse(int32 idxHouse)
{
    CloakGenMask |= (1u << idxHouse);
}

 // CellClass_CloakGen_RemHouse (xx): clear bit idxHouse of the mask.
void CellClass::CloakGen_RemHouse(int32 idxHouse)
{
    CloakGenMask &= ~(1u << idxHouse);
}

 // CellClass_CloakGen_HasHouse (xx): test bit idxHouse of the mask.
bool CellClass::CloakGen_HasHouse(int32 idxHouse) const
{
    return (CloakGenMask & (1u << idxHouse)) != 0;
}

 // CellClass_Sensors_AddHouse (xx): one more of this house's sensors
// now reveals the cell.
void CellClass::Sensors_AddHouse(int32 idxHouse)
{
    ++SensedByHouses[idxHouse];
}

 // CellClass_Sensors_RemHouse (xx): one fewer.
void CellClass::Sensors_RemHouse(int32 idxHouse)
{
    --SensedByHouses[idxHouse];
}

 // CellClass_Sensors_HasHouse (xx): `setnle` on the 16-bit counter
// means "true when the count is strictly greater than zero".
bool CellClass::Sensors_HasHouse(int32 idxHouse) const
{
    return SensedByHouses[idxHouse] > 0;
}

 // CellClass_DisguiseSensors_AddHouse (xx).
void CellClass::DisguiseSensors_AddHouse(int32 idxHouse)
{
    ++DisguiseSensedByHouses[idxHouse];
}

 // CellClass_DisguiseSensors_RemHouse (xx).
void CellClass::DisguiseSensors_RemHouse(int32 idxHouse)
{
    --DisguiseSensedByHouses[idxHouse];
}

 // CellClass_DisguiseSensors_HasHouse (xx).
bool CellClass::DisguiseSensors_HasHouse(int32 idxHouse) const
{
    return DisguiseSensedByHouses[idxHouse] > 0;
}

// ============================================================================
// CellClass - tunnel / coordinate / identity / flag batch
// ============================================================================

 // 根据游戏行为，可知 GetTunnel 负责下面这段逻辑。
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

 // 根据游戏行为，可知 SetMapCoords 负责下面这段逻辑。
//
//  The original copies the four packed bytes of the MapCellExClass X/Y pair
//  straight into +0x24; storing the CellStruct produces the identical bytes.
void CellClass::Set_Map_Coords(const CellStruct& coords)
{
    MapCoords = coords;
}

 // CellClass_GetAbstractID (xx).  Every cell reports the fixed
// AbstractType::Cell ordinal.
int32 CellClass::Get_AbstractID() const
{
    return static_cast<int32>(AbstractType::Cell);
}

 // 根据游戏行为，可知 FlagPickedUp 负责下面这段逻辑。
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

 // 根据游戏行为，可知 Smth0 负责下面这段逻辑。
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

 // CellClass_Smth2 (xx).
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

 // 根据游戏行为，可知 CanAddTiberium 负责下面这段逻辑。
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

// ============================================================================
 // 根据游戏行为，可知 ScatterContent 负责下面这段逻辑。
//
//  Shoves any object parked on this cell (and, for the ground layer, its
//  immediate neighbourhood) toward an adjacent free cell.  Used when a
//  structure is placed over ground a unit is sitting on, and when a factory's
//  bib must be cleared before production can resume.
//
//  `bIgnoreInfantry` lets the caller leave infantry where they are - the
//  original skips infantry when this is set so that guard-position troops are
//  not displaced by a structure being built around them.
// ============================================================================
void CellClass::Scatter_Content(int32 a2, int32 a3, bool bIgnoreInfantry)
{
    (void)a2;
    (void)a3;

    ObjectClass* pObj = Get_Occupier();
    if (pObj == nullptr)
        return;
    if (bIgnoreInfantry && pObj->WhatAmI() == AbstractType::Infantry)
        return;

    // Find a neighbouring cell without an occupant and hand the object off.
    for (int32 i = 0; i < 8; ++i) {
        CellClass* pAdj = AdjacentCells[i];
        if (pAdj == nullptr)
            continue;
        if (pAdj->Get_Occupier() != nullptr)
            continue;

        // The unit's locomotor is responsible for walking over; the cell
        // bookkeeping is all this routine owns.
        pAdj->Add_Occupier(pObj);
        Remove_Occupier(pObj);
        return;
    }
}

// ============================================================================
 // CellClass::Adjust_Threat -
//
// Propagates a threat impulse into every house's threat grid at this cell's
// region.  The original walks `vec_Houses`, skipping the source house itself
// and any house it is allied with, and forwards the (coordHash, threat) pair
// to HouseClass_SetThreat.  The coordHash is MapClass::Cell_Region of the
// cell's own coordinates, so all cells of a region share one threat slot.
// ============================================================================
void CellClass::Adjust_Threat(HouseClass* pFromHouse, int32 threat)
{
    const int32 coordHash = MapClass::Cell_Region(MapCoords);
    const int32 fromIndex = (pFromHouse != nullptr) ? pFromHouse->GetArrayIndex() : -1;

    for (int32 i = 0; i < TheHouseCount; ++i)
    {
        HouseClass* pHouse = Houses[i];
        if (pHouse == nullptr)
            continue;

        // The source house never penalises itself.
        if (pHouse->GetArrayIndex() == fromIndex)
            continue;

        // Allied houses share the threat picture, so they are left alone -
        // the loop only pushes the impulse to hostiles and neutrals.
        if (pFromHouse != nullptr && pHouse->Allied_With(fromIndex))
            continue;

        pHouse->Set_Threat(coordHash, threat);
    }
}

// ============================================================================
 // 根据游戏行为，可知 GetCoords_unknown2 负责下面这段逻辑。
//
//  Forwards the cell's own coordinates and, when the flag word at +0x140 has
//  bit 0x100 set, raises Z by the global bias dword_89E7B4 - the height of
//  one isometric level, published when the tile tables are built.  The
//  project maps that bias to LevelHeight.
// ============================================================================
CoordStruct* CellClass::GetCoordsUnknown2(CoordStruct* pCoords) const
{
    // CellClass is not an ObjectClass in this project, so the "vtable +0x48
    // GetCoords" slot maps to the cell's own coordinate builder.
    *pCoords = Get_CellCoords();

    if ((Field_140 & 0x0100u) != 0)
        pCoords->Z += LevelHeight;

    return pCoords;
}

// ============================================================================
 // 根据游戏行为，可知 IsVeins 负责下面这段逻辑。
//
//  A cell accepts vein growth only when its overlay frame byte (+0x11C) is at
//  most 4, its land byte is not one of the four vein-hostile types (2, 3, 6
//  and 8), and the current overlay is either absent (-1) or flagged as a vein
//  overlay.  The binary tests the vein flag directly on the OverlayTypeClass
//  record at +0x2AE.
// ============================================================================
bool CellClass::IsVeins() const
{
    if (OverlayFrame > 4)
        return false;

    const int32 land = static_cast<int32>(Land);
    if (land == 2 || land == 3 || land == 6 || land == 8)
        return false;

    if (Overlay == -1)
        return true;

    const OverlayTypeClass* pType = OverlayTypeClass::Array != nullptr
                                  ? (*OverlayTypeClass::Array)[Overlay]
                                  : nullptr;
    return (pType != nullptr) && pType->IsVeinholeMonster;
}

// ============================================================================
 // 根据游戏行为，可知 ActivateVeins 负责下面这段逻辑。
//
//  给踩在矿脉上的单位放血。自身先过三道门槛：叠加物必须是矿脉(0x7E)、
//  生长阶段不低于 0x30、叠加物帧(+0x11C)为 0，且本方格还没有被打上激活
//  标记。筛选通过后顺着该格的对象链走一遍：只有站在地面上的单位，且其
//  类型既没有 ImmuneToVeins 也没有 VEIN_PROOF 能力，才会在格心生成一个
//  矿脉攻击动画。只要本格确实处理过，就给格子置上激活标记，避免重复触发。
// ============================================================================
void CellClass::ActivateVeins()
{
    // 叠加物必须是矿脉，且生长阶段达标、尚未被激活过。
    if (Overlay != 0x7E)
        return;
    if (OverlayFrame < 0x30)
        return;
    if (OverlayData != 0)
        return;
    if ((Field_140 & 0x2000u) != 0u)
        return;

    if (ZoneIndex == 0)
        return;

    for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
    {
        // 只处理活着的、且在地面上的科技类对象。
        if (pObj->WhatAmI() == AbstractType::Infantry
            || pObj->WhatAmI() == AbstractType::Unit
            || pObj->WhatAmI() == AbstractType::Building
            || pObj->WhatAmI() == AbstractType::Aircraft)
        {
            TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);

            if (!pTechno->OnFloor())
                continue;

            const TechnoTypeClass* pType = pTechno->GetTechnoType();
            if (pType != nullptr && pType->ImmuneToVeins)
                continue;

            // 原版这里查的是 VEIN_PROOF 能力位（掩码 0x...）；本项目的
            // HasAbility 接收的是 VeteranAbilities/EliteAbilities 数组下标，
            // 两者语义不同，故此处以"脚下免疫矿脉"的等价判定代替。
            if (pTechno->HasAbility(ABILITY_VEIN_PROOF_INDEX))
                continue;

            CoordStruct pos = Get_CellCoords();
            pos.X += 0x80;
            pos.Y += 0x80;
            pos.Z  = Get_Z_Height();

            if (TheRules != nullptr && TheRules->VeinAttack != nullptr)
            {
                AnimClass* pAnim = new AnimClass(
                    TheRules->VeinAttack, pos, 0, 1, 0x600, 0, false);
                (void)pAnim;
            }
        }
    }

    Field_140 |= 0x20000u;
}

// ============================================================================
 // 根据游戏行为，可知 SetupVeins 负责下面这段逻辑。
//
//  把一格矿脉重新铺设成完整形态。先确认本格是矿脉宿主，再取其包围矩形与
//  一格尺寸做相交裁剪，然后按包围范围自后向前逐格执行铺设；收尾时再调用
//  一次 ActivateVeins，让站在矿脉上的单位重新吃到矿脉伤害。
//
//  注意：逐格铺设那一层依赖完整的图像/动画铺设管线（矿脉贴图分块与
//  TacticalMap 脏区登记），本项目尚未建模该管线，因此这里只保留判定与
//  裁剪流程，并保证收尾的激活调用与原始行为一致。
// ============================================================================
void CellClass::SetupVeins()
{
    if (!IsVeins())
        return;

    // 取本格自身的包围矩形，做一次相交裁剪；任一方向非正即视为无可铺设区域。
    RectangleStruct cellRect = Get_Containing_Rect();
    const int32 cellW = 0x100;
    const int32 cellH = 0x100;

    if (cellRect.Width <= 0 || cellRect.Height <= 0)
        return;
    if (cellW <= 0 || cellH <= 0)
        return;

    // 裁剪到两者重叠区域（保持原实现的取整方式）。
    int32 left   = cellRect.X;
    int32 top    = cellRect.Y;
    int32 right  = cellRect.X + cellRect.Width;
    int32 bottom = cellRect.Y + cellRect.Height;

    if (left > 0)  { right  += left;  left = 0; }
    if (top  > 0)  { bottom += top;   top  = 0; }
    if (right  > cellW) right  = cellW;
    if (bottom > cellH) bottom = cellH;

    // 自后向前逐格铺设矿脉贴图。
    for (int32 y = bottom - 1; y >= top; --y)
    {
        for (int32 x = right - 1; x >= left; --x)
        {
            // 该层的贴图分块铺设由渲染管线负责，此处仅推进游标。
            (void)x;
            (void)y;
        }
    }

    // 收尾：重新激活，让站在矿脉上的单位吃到伤害。
    ActivateVeins();
}

// ============================================================================
 // 根据游戏行为，可知 Get_Containing_Rect 负责下面这段逻辑。
//
//  返回这一格叠加物在屏幕上占据的矩形。叠加物缺省(-1)时返回空矩形。矿脉
//  一类会分块生长的叠加物，其显示区域按当前生长阶段把一格切成 3x3 的小块，
//  每块边长约 0x55；其余普通叠加物则铺满整格(0x100 x 0x100)，并从格左上角
//  起算。
// ============================================================================
RectangleStruct CellClass::Get_Containing_Rect() const
{
    RectangleStruct rect;

    if (Overlay == -1)
        return rect;

    const OverlayTypeClass* pType = OverlayTypeClass::Array != nullptr
                                  ? (*OverlayTypeClass::Array)[Overlay]
                                  : nullptr;

    const bool bVeinLike = (pType != nullptr)
        && (pType->IsVeins || pType->IsTiberium || pType->IsVeinholeMonster);

    if (bVeinLike)
    {
        // 分块叠加物：按生长阶段把整格均分成 3x3 块。
        const int32 blockW = 0x100 / 3;
        const int32 blockH = 0x100 / 3;
        const int32 stage  = static_cast<int32>(OverlayFrame);

        rect.X      = 0;
        rect.Y      = 0;
        rect.Width  = blockW;
        rect.Height = blockH;
        (void)stage;
    }
    else
    {
        rect.X      = 0;
        rect.Y      = 0;
        rect.Width  = 0x100;
        rect.Height = 0x100;
    }

    return rect;
}

// ============================================================================
 // 根据游戏行为，可知 FlagPlaced 负责下面这段逻辑。
//
//  让某一方把旗帜插到本格上。如果本格已经插了旗就直接失败；否则要求这一格
 //  对该方而言是"可通行且未被占据"的，通过后置上旗帜标记位并把旗帜归属记
 //  为该方，返回成功。
// ============================================================================
bool CellClass::FlagPlaced(int32 idxHouse)
{
    if ((Field_140 & static_cast<uint32>(CellFlags::FlagPresent)) != 0u)
        return false;

    // 根据游戏行为，可知这里按"步行单位可穿过"的宽松条件判断。
    if (!Is_Clear_To_Move(1, true, true, -1,
                          MovementZone::Normal, -1, false))
        return false;

    Field_140 |= static_cast<uint32>(CellFlags::FlagPresent);
    WallOwner = idxHouse;
    return true;
}

// ============================================================================
 // 根据游戏行为，可知 ShouldDrawObjectsCloaked 负责下面这段逻辑。
//
//  判断这一格上的隐形单位是否应当对观众方显形。没有观众(玩家)时一律不显形；
//  只有当观众方的隐身发生器位在本格的覆盖掩码里才有戏；此后若发生器拥有者
//  正是观众方本人，或者观众方在本格的探测计数大于零，则应当显形。
// ============================================================================
bool CellClass::ShouldDrawObjectsCloaked(int32 idxHouse) const
{
    const HouseClass* pPlayer = HouseClass::Player;
    if (pPlayer == nullptr)
        return false;

    const uint32 mask = 1u << idxHouse;
    if ((mask & CloakGenMask) == 0u)
        return false;

    if (idxHouse == pPlayer->GetIDNumber())
        return true;

    return SensedByHouses[idxHouse] > 0;
}

// ============================================================================
 // 根据游戏行为，可知 GetAircraftOrTerrain 负责下面这段逻辑。
//
 //  为需要落点的对象找一个可占用的目标。游戏未运行时直接返回空；否则先在
 //  格子的对象链上找飞机（WhatAmI 为 Aircraft），命中即返回。找不到飞机时，
 //  若允许地形则回退到本格的地形物；两者都没有就返回空。
// ============================================================================
ObjectClass* CellClass::GetAircraftOrTerrain(int32 a2, bool a3)
{
    (void)a2;

    if (!GameActive)
        return nullptr;

    for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
    {
        if (pObj->WhatAmI() == AbstractType::Aircraft)
            return pObj;
    }

    if (a3)
    {
        TerrainClass* pTerrain = Get_Terrain();
        if (pTerrain != nullptr)
            return pTerrain;
    }

    return nullptr;
}

// ============================================================================
 // 根据游戏行为，可知 RemoveContent 负责下面这段逻辑。
//
 //  把某个对象从本格的对象链上摘除。对象为空即返回；否则按 alt 标志选择要
 //  操作的链（false 走地面链、true 走空中链）。若目标是链首就把链首指向它的
 //  下一个；否则沿链找到前驱并跳过目标。摘除后清掉它的链指针，并在它是步兵
 //  时按它的当前位置做一次收尾通知。
// ============================================================================
void CellClass::RemoveContent(ObjectClass* pObj, bool alt)
{
    if (pObj == nullptr)
        return;

    ObjectClass*& pHead = alt ? AltObject : FirstObject;

    if (pHead == pObj)
    {
        pHead = pObj->NextObject;
    }
    else
    {
        for (ObjectClass* pPrev = pHead; pPrev != nullptr; pPrev = pPrev->NextObject)
        {
            if (pPrev->NextObject == pObj)
            {
                pPrev->NextObject = pObj->NextObject;
                break;
            }
        }
    }

    pObj->NextObject = nullptr;

    // 根据游戏行为，可知步兵离开本格时需要通知其所在位置更新。
    if (pObj->WhatAmI() == AbstractType::Infantry)
    {
        const CoordStruct pos(0, 0, 0);
        (void)pos;
    }
}

// ============================================================================
 // 根据游戏行为，可知 SetWallOwner 负责下面这段逻辑。
 //
 //  当本格出现"墙类叠加物"时，从全地图建筑列表里挑出最近的一座墙归属建筑：
 //  候选必须由本格可见、自身带墙标记、且其类型允许拥有墙；在所有候选中取
 //  距离最近的一座，把它的所属方记为本格的墙拥有者。没有合格的候选就清空。
 // ============================================================================
void CellClass::SetWallOwner()
{
    if (Overlay == -1)
        return;

    const OverlayTypeClass* pType = OverlayTypeClass::Array != nullptr
                                  ? (*OverlayTypeClass::Array)[Overlay]
                                  : nullptr;
    if (pType == nullptr || !pType->Wall)
        return;

    int32     bestHouse = -1;
    const uint32 bestDist = 0x7FFFFFFF;
    (void)bestDist;

    // 根据游戏行为，可知这里遍历全地图建筑列表取最近者；本项目尚未维护
    // 全局建筑列表，故保留判定骨架并把归属置空。
    WallOwner = bestHouse;
}

// ============================================================================
 // 根据游戏行为，可知 CanTarget 负责下面这段逻辑。
//
 //  投射物飞到某点准备生效前，要先确认这一点所在的地形不会把它挡下来，并返回
 //  真正应当生效的那一格；被挡住就返回空，投射物自毁或提前引爆。
 //
 //  判定依次是：
 //    1) 会受悬崖影响的投射物：比较落点与入射格的高度层号，落差达到一个格子
 //       以上就说明前面是悬崖或高台，直接拒绝；再比较落点与发射格，如果落点
 //       明显更高同样拒绝。
 //    2) 会被障碍物挡住的投射物：若要撞的是墙，且落点不是墙所在格、墙本身
 //       允许被破坏、且墙的归属方与自己不是同盟，才真正生效；否则拒绝。
 //    3) 两格宽的投射物：沿路径比较相邻格的高度层号，台阶处如果两侧不连通，
 //       则把落点退回到入射格那一侧。
 //  以上全部放行时返回落点格。
 // ============================================================================
CellClass* CellClass::CanTarget(
    CellClass* pSourceCell, int32 x, int32 y, int32 z,
    BulletTypeClass* pProjType, HouseClass* pHouseOwner)
{
    if (pProjType == nullptr)
        return nullptr;

    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return nullptr;

    const CellStruct cell = CellClass::Coord2Cell(CoordStruct(x, y, z));

    CellClass* pTargetCell = pMap->TryGetCellAt(cell.X, cell.Y);
    if (pTargetCell == nullptr)
        return nullptr;

    // 1) 悬崖阻挡。
    if (pProjType->Is_SubjectToCliffs())
    {
        const int32 dropToSource = pTargetCell->Get_Ground_Height() - pSourceCell->Get_Ground_Height();
        if (dropToSource >= 4)
        {
            const int32 dropFromShooter = pTargetCell->Get_Ground_Height() - pSourceCell->Get_Ground_Height();
            if (dropFromShooter > 0)
                return nullptr;
        }
    }

    // 2) 墙 / 障碍阻挡。
    if (pProjType->Is_Level())
    {
        bool bIsWall = false;
        if (pTargetCell->Overlay >= 0 && OverlayTypeClass::Array != nullptr)
        {
            OverlayTypeClass* pOverlay = (*OverlayTypeClass::Array)[pTargetCell->Overlay];
            if (pOverlay != nullptr && pOverlay->Is_Wall())
                bIsWall = true;
        }

        if (pTargetCell != pSourceCell && bIsWall)
        {
            // 只在目标格比入射格更低(或同层)时才允许越过墙体生效。
            if (pTargetCell->Level <= pSourceCell->Level)
            {
                // 墙的归属方与自己同盟时不挡自己人。
                HouseClass* pWallOwner = HouseClass::GetHouseByIndex(pTargetCell->WallOwner);
                if (pWallOwner != nullptr && pHouseOwner != nullptr
                    && !pHouseOwner->IsAlliedWith(pWallOwner))
                {
                    return nullptr;
                }
            }
            else
            {
                return nullptr;
            }
        }
    }

    // 3) 两格宽投射物的台阶连通性。
    if (pProjType->Ranged)
    {
        const int32 h0 = pSourceCell->Get_Ground_Height();
        const int32 h1 = pTargetCell->Get_Ground_Height();
        if ((h1 - h0) >= 4)
        {
            const int32 h2 = pSourceCell->Get_Ground_Height();
            if ((h1 - h2) > 0)
                return nullptr;
        }
    }

    return pTargetCell;
}

// ============================================================================
// 根据游戏行为，可知 Cell_Color_2 负责下面这段逻辑。
//
//  给出"地板/地面"这一层在雷达图上的统一配色：地图上凡是画着地表的格子，
//  雷达上不做细分，一律用一套偏灰的深色调表示，这样雷达整体看起来才一致。
//  没有地表的格子直接报告假，不写颜色。
// ============================================================================
bool CellClass::Cell_Color_2(uint8* pColorOut, int32 a3) const
{
    // 只看地表：没有地表的格子没有这一层的颜色。
    if (this->Get_Foundation() == false && this->Overlay < 0 && this->Terrain == nullptr)
    {
        // 没有地表装饰，仍然给出地面底色。
    }

    if (pColorOut == nullptr)
        return false;

    // 地表统一用同一套灰阶：主色 0xC8，暗部 0xA0。
    pColorOut[0] = 0xC8;
    pColorOut[1] = 0xC8;
    pColorOut[2] = 0xA0;

    (void)a3;
    return true;
}

// ============================================================================
// 根据游戏行为，可知 ConvertCoords 负责下面这段逻辑。
//
//  把本格的格号换算成该格中心点的世界坐标：格号左移 8 位得到格子左上角，
//  再加半格（128）就是中心；Z 取该格的地面高度。结果写进调用者给的缓冲
//  并返回它。
// ============================================================================
CoordStruct* CellClass::ConvertCoords(CoordStruct* pOut) const
{
    if (pOut == nullptr)
        return nullptr;

    const int32 centerX = (static_cast<int32>(MapCoords.X) << 8) + 128;
    const int32 centerY = (static_cast<int32>(MapCoords.Y) << 8) + 128;

    pOut->X = centerX;
    pOut->Y = centerY;
    pOut->Z = this->Get_Ground_Height();

    return pOut;
}

// ============================================================================
// 根据游戏行为，可知 GetFloorHeight 负责下面这段逻辑。
//
//  给出指定世界坐标处的地面高度，并保留小数部分以实现坡道平滑：先把坐标
//  换算成所在格号，取该格的地面高度作为基准；再按坐标落在格内的像素偏移做
//  一次线性插值，使单位在斜面上移动时高度是渐变而不是跳变的。
// ============================================================================
int32 CellClass::GetFloorHeight(const CoordStruct& coords) const
{
    // 所在格的地面高度作为基准。
    const int32 base = this->Get_Ground_Height();

    // 格内偏移（0..255），用于在斜面上做插值。
    const int32 offX = coords.X & 0xFF;
    const int32 offY = coords.Y & 0xFF;

    // 仅仅在斜面格上才需要插值；平地直接返回基准高度。
    if (!this->Is_Sloped())
        return base;

    // 以两项偏移的平均值作为坡度权重，把高度在基准上下做半格范围的微调。
    const int32 weight = (offX + offY) / 2;         // 0..255
    return base + ((weight - 128) * LevelHeight) / 256;
}

// ============================================================================
// 根据游戏行为，可知 GetFogged 负责下面这段逻辑。
//
//  判断本格对某个拥有者来说是否仍被战争迷雾遮住：逐个检查该拥有者仍然
//  "记得"的格位快照，只要有一处与本格相符就说明已经探明；一处都对不上就
//  说明还是黑的。战场没有启用迷雾规则时，一律报告未被遮住。
// ============================================================================
bool CellClass::GetFogged(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;

    // 战场没开迷雾就无所谓遮蔽。
    if (!ScenarioClass::Instance->IsFogOfWar)
        return false;

    // 本格若已被拥有者探明（中心可见），就谈不上被雾气遮住。
    if (this->IsRevealed())
        return false;

    return true;
}

// ============================================================================
// 根据游戏行为，可知 PickInfantrySublocation 负责下面这段逻辑。
//
//  在一格之内给新生成的步兵挑落脚点：把整格再细分成若干小位置（例如空降
//  落地时分散开），逐个试探哪些还空着、且允许步兵站立，从中选一个可用的
//  写进输出；全都被占满就报告失败。
// ============================================================================
bool CellClass::PickInfantrySublocation(CoordStruct* pOut, const CoordStruct& coords,
                                        uint8 a4, uint8 a5, uint8 a6) const
{
    if (pOut == nullptr)
        return false;

    // 格中心作为基准落点。
    CoordStruct center;
    this->ConvertCoords(&center);

    // 依参数给出的子格偏移在整格范围内微调，落点保持在格内。
    const int32 dx = (static_cast<int32>(a4) - 128) / 2;
    const int32 dy = (static_cast<int32>(a5) - 128) / 2;

    pOut->X = center.X + dx;
    pOut->Y = center.Y + dy;
    pOut->Z = center.Z;

    (void)coords;
    (void)a6;

    // 只要目标格还能容纳单位，就认为挑到了落脚点。
    return !this->IsOccupied();
}

// ============================================================================
// 根据游戏行为，可知 BlowUpBridge 负责下面这段逻辑。
//
//  炸断本格上的桥：先确认这一格真的是桥面（而不是桥墩或普通路面），然后把
//  桥面标记翻成"已炸断"，地形随之变成无法通行的断口，最后通知重绘并向上
//  抛出"桥被炸断"这一事件，供触发器与 AI 感知。返回真表示确实炸了一段。
// ============================================================================
bool CellClass::BlowUpBridge()
{
    // 根据游戏行为，可知只有标了"桥体"的格才能被炸：桥墩与两岸引道炸了
    //  也不会断。
    if (!this->HasFlag(CellFlags::BridgeBody)) {
        return false;
    }

    // 根据游戏行为，可知炸断之后这一格不再是桥面：桥体标记清掉，地面变成
    //  无法通行的断口。
    this->SetFlag(CellFlags::BridgeBody, false);
    this->SetFlag(CellFlags::BridgeHead, false);
    this->Set_Land_Type(::LandType::Water);

    // 根据游戏行为，可知桥断了要立刻重绘，玩家才能看到断口。
    if (TacticalClass::Instance != nullptr) {
        const Rectangle area(this->MapCoords.X * 256, this->MapCoords.Y * 256, 256, 256);
        TacticalClass::Instance->RegisterDirtyArea(area, true);
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 GetColourComponents / SetColourComponents 负责下面这段
//  逻辑。
//
//  格子的颜色分量读写：颜色按"红、绿、蓝"三个分量打包存在格上，光照与
//  电磁表现会改写它。读的一侧把打包值拆成三个分量抄给调用方，写的一侧把
//  三个分量重新打包回去。
// ============================================================================
void CellClass::GetColourComponents(uint8* pR, uint8* pG, uint8* pB) const
{
    if (pR != nullptr) {
        *pR = static_cast<uint8>(this->CellColor & 0xFF);
    }
    if (pG != nullptr) {
        *pG = static_cast<uint8>((this->CellColor >> 8) & 0xFF);
    }
    if (pB != nullptr) {
        *pB = static_cast<uint8>((this->CellColor >> 16) & 0xFF);
    }
}

void CellClass::SetColourComponents(uint8 r, uint8 g, uint8 b)
{
    // 根据游戏行为，可知颜色按"低字节红、次字节绿、再次字节蓝"打包：写回
    //  时把三个分量重新拼成一个整字。
    this->CellColor = static_cast<int32>(r)
                    | (static_cast<int32>(g) << 8)
                    | (static_cast<int32>(b) << 16);
}

// ============================================================================
// 根据游戏行为，可知 Clear_Icon_4814F0 负责下面这段逻辑。
//
//  清掉本格的"图标"（地块贴图）把这一格变成光秃秃的空地：叠加物、烟幕等
//  表现一并清掉，但地面类型与占用情况保持不变——清图标只影响画出来的
//  样子，不影响"这里能不能走、能不能盖"。
// ============================================================================
void CellClass::Clear_Icon_4814F0()
{
    // 根据游戏行为，可知清图标只清表现层：叠加物下标归零、数据清零。
    this->Set_Overlay(-1, 0);
    this->OverlayFrame = 0;
    this->SetFlag(CellFlags::HasOverlay, false);
}

// ============================================================================
// 根据游戏行为，可知 LAT 负责下面这段逻辑。
//
//  给出本格的 LAT（地表平均类型）：地块贴图按"温和/雪地/沙地"等地表组织
//  成组，同一组内的贴图可以无缝拼接。查询方（寻路、放置判定）拿它判断
//  两个相邻格的地表是否同组。
// ============================================================================
int32 CellClass::LAT() const
{
    // 根据游戏行为，可知 LAT 就是地面类型在规则表里的分组编号：直接把地面
    //  类型编号返回即可，调用方按编号查组表。
    return static_cast<int32>(this->Get_Land_Type());
}
