#include "DisplayClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Abstract/ObjectClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/CellClass.h"
#include "../Math/CoordStruct.h"
#include "../Math/Rectangle.h"
#include "../Rendering/TacticalClass.h"
#include "../Rendering/Surface.h"

#include <cstring>

// ============================================================================
// Static instance
// ============================================================================

DisplayClass* DisplayClass::Instance = nullptr;

// ============================================================================
// Construction / Destruction
// ============================================================================

DisplayClass::DisplayClass()
    : MapClass()
    , VisibleRect()
    , MapRect()
    , ScreenWidth(640)
    , ScreenHeight(400)
    , ZoomLevel(1.0f)
    , ScrollAmount(4)
    , IsScrolling(false)
    , CursorPosition(0, 0)
    , ViewPosition(0, 0)
    , ScrollTimer(0)
    , CurrentFoundation_CenterCell()
    , CurrentFoundation_TopLeftOffset()
    , CurrentFoundation_Data(nullptr)
    , unknown_1180(false)
    , unknown_1181(false)
    , CurrentFoundationCopy_CenterCell()
    , CurrentFoundationCopy_TopLeftOffset()
    , CurrentFoundationCopy_Data(nullptr)
    , unknown_1190(0)
    , unknown_1194(0)
    , unknown_1198(0)
    , FollowObject(false)
    , ObjectToFollow(nullptr)
    , CurrentBuilding(nullptr)
    , CurrentBuildingType(nullptr)
    , unknown_11AC(0)
    , RepairMode(false)
    , SellMode(false)
    , PowerToggleMode(false)
    , PlanningMode(false)
    , PlaceBeaconMode(false)
    , CurrentSWTypeIndex(0)
    , unknown_11BC(0)
    , unknown_11C0(0)
    , unknown_11C4(0)
    , unknown_11C8(0)
    , unknown_bool_11CC(false)
    , unknown_bool_11CD(false)
    , unknown_bool_11CE(false)
    , DraggingRectangle(false)
    , unknown_bool_11D0(false)
    , unknown_bool_11D1(false)
    , unknown_11D4(0)
    , unknown_11D8(0)
    , unknown_11DC(0)
    , unknown_11E0(0)
    , padding_11E4(0)
{
    Instance = this;
}

DisplayClass::~DisplayClass() {
    if (Instance == this) {
        Instance = nullptr;
    }
}

// ============================================================================
// MapClass overrides
// ============================================================================

HRESULT DisplayClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Chain to parent class
    hr = MapClass::Load(pStm);
    if (hr < 0) return E_FAIL;

    // Read VisibleRect
    hr = pStm->Read(&VisibleRect, sizeof(VisibleRect), &read);
    if (hr < 0 || read != sizeof(VisibleRect)) return E_FAIL;

    // Read MapRect
    hr = pStm->Read(&MapRect, sizeof(MapRect), &read);
    if (hr < 0 || read != sizeof(MapRect)) return E_FAIL;

    // Read screen dimensions
    hr = pStm->Read(&ScreenWidth, sizeof(ScreenWidth), &read);
    if (hr < 0 || read != sizeof(ScreenWidth)) return E_FAIL;
    hr = pStm->Read(&ScreenHeight, sizeof(ScreenHeight), &read);
    if (hr < 0 || read != sizeof(ScreenHeight)) return E_FAIL;

    // Read ZoomLevel
    hr = pStm->Read(&ZoomLevel, sizeof(ZoomLevel), &read);
    if (hr < 0 || read != sizeof(ZoomLevel)) return E_FAIL;

    // Read ScrollAmount
    hr = pStm->Read(&ScrollAmount, sizeof(ScrollAmount), &read);
    if (hr < 0 || read != sizeof(ScrollAmount)) return E_FAIL;

    // Read cursor and view positions
    hr = pStm->Read(&CursorPosition, sizeof(CursorPosition), &read);
    if (hr < 0 || read != sizeof(CursorPosition)) return E_FAIL;
    hr = pStm->Read(&ViewPosition, sizeof(ViewPosition), &read);
    if (hr < 0 || read != sizeof(ViewPosition)) return E_FAIL;

    // Read ScrollTimer
    hr = pStm->Read(&ScrollTimer, sizeof(ScrollTimer), &read);
    if (hr < 0 || read != sizeof(ScrollTimer)) return E_FAIL;

    // Read foundation data
    hr = pStm->Read(&CurrentFoundation_CenterCell, sizeof(CurrentFoundation_CenterCell), &read);
    if (hr < 0 || read != sizeof(CurrentFoundation_CenterCell)) return E_FAIL;
    hr = pStm->Read(&CurrentFoundation_TopLeftOffset, sizeof(CurrentFoundation_TopLeftOffset), &read);
    if (hr < 0 || read != sizeof(CurrentFoundation_TopLeftOffset)) return E_FAIL;
    CurrentFoundation_Data = nullptr;

    hr = pStm->Read(&CurrentFoundationCopy_CenterCell, sizeof(CurrentFoundationCopy_CenterCell), &read);
    if (hr < 0 || read != sizeof(CurrentFoundationCopy_CenterCell)) return E_FAIL;
    hr = pStm->Read(&CurrentFoundationCopy_TopLeftOffset, sizeof(CurrentFoundationCopy_TopLeftOffset), &read);
    if (hr < 0 || read != sizeof(CurrentFoundationCopy_TopLeftOffset)) return E_FAIL;
    CurrentFoundationCopy_Data = nullptr;

    // Read unknown DWORDs
    hr = pStm->Read(&unknown_1190, sizeof(unknown_1190), &read);
    if (hr < 0 || read != sizeof(unknown_1190)) return E_FAIL;
    hr = pStm->Read(&unknown_1194, sizeof(unknown_1194), &read);
    if (hr < 0 || read != sizeof(unknown_1194)) return E_FAIL;
    hr = pStm->Read(&unknown_1198, sizeof(unknown_1198), &read);
    if (hr < 0 || read != sizeof(unknown_1198)) return E_FAIL;

    // Read flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsScrolling        = (flags & 0x00000001) != 0;
    unknown_1180       = (flags & 0x00000002) != 0;
    unknown_1181       = (flags & 0x00000004) != 0;
    FollowObject       = (flags & 0x00000008) != 0;
    RepairMode         = (flags & 0x00000010) != 0;
    SellMode           = (flags & 0x00000020) != 0;
    PowerToggleMode    = (flags & 0x00000040) != 0;
    PlanningMode       = (flags & 0x00000080) != 0;
    PlaceBeaconMode    = (flags & 0x00000100) != 0;
    unknown_bool_11CC  = (flags & 0x00000200) != 0;
    unknown_bool_11CD  = (flags & 0x00000400) != 0;
    unknown_bool_11CE  = (flags & 0x00000800) != 0;
    DraggingRectangle  = (flags & 0x00001000) != 0;
    unknown_bool_11D0  = (flags & 0x00002000) != 0;
    unknown_bool_11D1  = (flags & 0x00004000) != 0;

    // Read ObjectToFollow (int32 index)
    int32 followIndex = -1;
    hr = pStm->Read(&followIndex, sizeof(followIndex), &read);
    if (hr < 0 || read != sizeof(followIndex)) return E_FAIL;
    ObjectToFollow = nullptr;
    if (followIndex >= 0 && ObjectClass::Array && followIndex < ObjectClass::Array->Count) {
        ObjectToFollow = (*ObjectClass::Array)[followIndex];
    }

    // Read CurrentBuilding (int32 index)
    int32 buildingIndex = -1;
    hr = pStm->Read(&buildingIndex, sizeof(buildingIndex), &read);
    if (hr < 0 || read != sizeof(buildingIndex)) return E_FAIL;
    CurrentBuilding = nullptr;
    if (buildingIndex >= 0 && ObjectClass::Array && buildingIndex < ObjectClass::Array->Count) {
        CurrentBuilding = (*ObjectClass::Array)[buildingIndex];
    }

    // Read CurrentBuildingType (string ID)
    char buildingTypeID[0x18];
    hr = pStm->Read(buildingTypeID, sizeof(buildingTypeID), &read);
    if (hr < 0 || read != sizeof(buildingTypeID)) return E_FAIL;
    buildingTypeID[sizeof(buildingTypeID) - 1] = '\0';
    CurrentBuildingType = buildingTypeID[0] ? ObjectTypeClass::Find(buildingTypeID) : nullptr;

    // Read unknown_11AC
    hr = pStm->Read(&unknown_11AC, sizeof(unknown_11AC), &read);
    if (hr < 0 || read != sizeof(unknown_11AC)) return E_FAIL;

    // Read CurrentSWTypeIndex
    hr = pStm->Read(&CurrentSWTypeIndex, sizeof(CurrentSWTypeIndex), &read);
    if (hr < 0 || read != sizeof(CurrentSWTypeIndex)) return E_FAIL;

    // Read unknown DWORDs
    hr = pStm->Read(&unknown_11BC, sizeof(unknown_11BC), &read);
    if (hr < 0 || read != sizeof(unknown_11BC)) return E_FAIL;
    hr = pStm->Read(&unknown_11C0, sizeof(unknown_11C0), &read);
    if (hr < 0 || read != sizeof(unknown_11C0)) return E_FAIL;
    hr = pStm->Read(&unknown_11C4, sizeof(unknown_11C4), &read);
    if (hr < 0 || read != sizeof(unknown_11C4)) return E_FAIL;
    hr = pStm->Read(&unknown_11C8, sizeof(unknown_11C8), &read);
    if (hr < 0 || read != sizeof(unknown_11C8)) return E_FAIL;

    // Read more unknown DWORDs
    hr = pStm->Read(&unknown_11D4, sizeof(unknown_11D4), &read);
    if (hr < 0 || read != sizeof(unknown_11D4)) return E_FAIL;
    hr = pStm->Read(&unknown_11D8, sizeof(unknown_11D8), &read);
    if (hr < 0 || read != sizeof(unknown_11D8)) return E_FAIL;
    hr = pStm->Read(&unknown_11DC, sizeof(unknown_11DC), &read);
    if (hr < 0 || read != sizeof(unknown_11DC)) return E_FAIL;
    hr = pStm->Read(&unknown_11E0, sizeof(unknown_11E0), &read);
    if (hr < 0 || read != sizeof(unknown_11E0)) return E_FAIL;

    // Read padding
    hr = pStm->Read(&padding_11E4, sizeof(padding_11E4), &read);
    if (hr < 0 || read != sizeof(padding_11E4)) return E_FAIL;

    return S_OK;
}

HRESULT DisplayClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Chain to parent class
    hr = MapClass::Save(pStm, fClearDirty);
    if (hr < 0) return E_FAIL;

    // Write VisibleRect
    hr = pStm->Write(&VisibleRect, sizeof(VisibleRect), &written);
    if (hr < 0 || written != sizeof(VisibleRect)) return E_FAIL;

    // Write MapRect
    hr = pStm->Write(&MapRect, sizeof(MapRect), &written);
    if (hr < 0 || written != sizeof(MapRect)) return E_FAIL;

    // Write screen dimensions
    hr = pStm->Write(&ScreenWidth, sizeof(ScreenWidth), &written);
    if (hr < 0 || written != sizeof(ScreenWidth)) return E_FAIL;
    hr = pStm->Write(&ScreenHeight, sizeof(ScreenHeight), &written);
    if (hr < 0 || written != sizeof(ScreenHeight)) return E_FAIL;

    // Write ZoomLevel
    hr = pStm->Write(&ZoomLevel, sizeof(ZoomLevel), &written);
    if (hr < 0 || written != sizeof(ZoomLevel)) return E_FAIL;

    // Write ScrollAmount
    hr = pStm->Write(&ScrollAmount, sizeof(ScrollAmount), &written);
    if (hr < 0 || written != sizeof(ScrollAmount)) return E_FAIL;

    // Write cursor and view positions
    hr = pStm->Write(&CursorPosition, sizeof(CursorPosition), &written);
    if (hr < 0 || written != sizeof(CursorPosition)) return E_FAIL;
    hr = pStm->Write(&ViewPosition, sizeof(ViewPosition), &written);
    if (hr < 0 || written != sizeof(ViewPosition)) return E_FAIL;

    // Write ScrollTimer
    hr = pStm->Write(&ScrollTimer, sizeof(ScrollTimer), &written);
    if (hr < 0 || written != sizeof(ScrollTimer)) return E_FAIL;

    // Write foundation data
    hr = pStm->Write(&CurrentFoundation_CenterCell, sizeof(CurrentFoundation_CenterCell), &written);
    if (hr < 0 || written != sizeof(CurrentFoundation_CenterCell)) return E_FAIL;
    hr = pStm->Write(&CurrentFoundation_TopLeftOffset, sizeof(CurrentFoundation_TopLeftOffset), &written);
    if (hr < 0 || written != sizeof(CurrentFoundation_TopLeftOffset)) return E_FAIL;

    hr = pStm->Write(&CurrentFoundationCopy_CenterCell, sizeof(CurrentFoundationCopy_CenterCell), &written);
    if (hr < 0 || written != sizeof(CurrentFoundationCopy_CenterCell)) return E_FAIL;
    hr = pStm->Write(&CurrentFoundationCopy_TopLeftOffset, sizeof(CurrentFoundationCopy_TopLeftOffset), &written);
    if (hr < 0 || written != sizeof(CurrentFoundationCopy_TopLeftOffset)) return E_FAIL;

    // Write unknown DWORDs
    hr = pStm->Write(&unknown_1190, sizeof(unknown_1190), &written);
    if (hr < 0 || written != sizeof(unknown_1190)) return E_FAIL;
    hr = pStm->Write(&unknown_1194, sizeof(unknown_1194), &written);
    if (hr < 0 || written != sizeof(unknown_1194)) return E_FAIL;
    hr = pStm->Write(&unknown_1198, sizeof(unknown_1198), &written);
    if (hr < 0 || written != sizeof(unknown_1198)) return E_FAIL;

    // Write flags as a bitmask
    uint32 flags = 0;
    if (IsScrolling)       flags |= 0x00000001;
    if (unknown_1180)      flags |= 0x00000002;
    if (unknown_1181)      flags |= 0x00000004;
    if (FollowObject)      flags |= 0x00000008;
    if (RepairMode)        flags |= 0x00000010;
    if (SellMode)          flags |= 0x00000020;
    if (PowerToggleMode)   flags |= 0x00000040;
    if (PlanningMode)      flags |= 0x00000080;
    if (PlaceBeaconMode)   flags |= 0x00000100;
    if (unknown_bool_11CC) flags |= 0x00000200;
    if (unknown_bool_11CD) flags |= 0x00000400;
    if (unknown_bool_11CE) flags |= 0x00000800;
    if (DraggingRectangle) flags |= 0x00001000;
    if (unknown_bool_11D0) flags |= 0x00002000;
    if (unknown_bool_11D1) flags |= 0x00004000;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    // Write ObjectToFollow (int32 index)
    int32 followIndex = -1;
    if (ObjectToFollow && ObjectClass::Array) {
        followIndex = ObjectClass::Find_Index(ObjectToFollow);
    }
    hr = pStm->Write(&followIndex, sizeof(followIndex), &written);
    if (hr < 0 || written != sizeof(followIndex)) return E_FAIL;

    // Write CurrentBuilding (int32 index)
    int32 buildingIndex = -1;
    if (CurrentBuilding && ObjectClass::Array) {
        buildingIndex = ObjectClass::Find_Index(CurrentBuilding);
    }
    hr = pStm->Write(&buildingIndex, sizeof(buildingIndex), &written);
    if (hr < 0 || written != sizeof(buildingIndex)) return E_FAIL;

    // Write CurrentBuildingType (string ID)
    char buildingTypeID[0x18];
    std::memset(buildingTypeID, 0, sizeof(buildingTypeID));
    if (CurrentBuildingType && CurrentBuildingType->get_ID()) {
        const char* srcID = CurrentBuildingType->get_ID();
        int32 j = 0;
        while (srcID[j] && j < static_cast<int32>(sizeof(buildingTypeID)) - 1) {
            buildingTypeID[j] = srcID[j]; ++j;
        }
    }
    hr = pStm->Write(buildingTypeID, sizeof(buildingTypeID), &written);
    if (hr < 0 || written != sizeof(buildingTypeID)) return E_FAIL;

    // Write unknown_11AC
    hr = pStm->Write(&unknown_11AC, sizeof(unknown_11AC), &written);
    if (hr < 0 || written != sizeof(unknown_11AC)) return E_FAIL;

    // Write CurrentSWTypeIndex
    hr = pStm->Write(&CurrentSWTypeIndex, sizeof(CurrentSWTypeIndex), &written);
    if (hr < 0 || written != sizeof(CurrentSWTypeIndex)) return E_FAIL;

    // Write unknown DWORDs
    hr = pStm->Write(&unknown_11BC, sizeof(unknown_11BC), &written);
    if (hr < 0 || written != sizeof(unknown_11BC)) return E_FAIL;
    hr = pStm->Write(&unknown_11C0, sizeof(unknown_11C0), &written);
    if (hr < 0 || written != sizeof(unknown_11C0)) return E_FAIL;
    hr = pStm->Write(&unknown_11C4, sizeof(unknown_11C4), &written);
    if (hr < 0 || written != sizeof(unknown_11C4)) return E_FAIL;
    hr = pStm->Write(&unknown_11C8, sizeof(unknown_11C8), &written);
    if (hr < 0 || written != sizeof(unknown_11C8)) return E_FAIL;

    // Write more unknown DWORDs
    hr = pStm->Write(&unknown_11D4, sizeof(unknown_11D4), &written);
    if (hr < 0 || written != sizeof(unknown_11D4)) return E_FAIL;
    hr = pStm->Write(&unknown_11D8, sizeof(unknown_11D8), &written);
    if (hr < 0 || written != sizeof(unknown_11D8)) return E_FAIL;
    hr = pStm->Write(&unknown_11DC, sizeof(unknown_11DC), &written);
    if (hr < 0 || written != sizeof(unknown_11DC)) return E_FAIL;
    hr = pStm->Write(&unknown_11E0, sizeof(unknown_11E0), &written);
    if (hr < 0 || written != sizeof(unknown_11E0)) return E_FAIL;

    // Write padding
    hr = pStm->Write(&padding_11E4, sizeof(padding_11E4), &written);
    if (hr < 0 || written != sizeof(padding_11E4)) return E_FAIL;

    return S_OK;
}

void DisplayClass::LoadFromINI(CCINIClass* pINI) {
    Read_INI();
}

const wchar_t* DisplayClass::GetToolTip(uint32 nDlgID) {
    // Return context-sensitive tooltip text for the current cursor/UI
    // context. The dialog id selects a UI element; when no element-specific
    // tooltip applies, the active command/placement mode supplies a
    // fallback so the status bar always has descriptive text.
    (void)nDlgID;

    if (RepairMode) {
        static const wchar_t tip[] = L"Repair";
        return tip;
    }
    if (SellMode) {
        static const wchar_t tip[] = L"Sell";
        return tip;
    }
    if (PlanningMode) {
        static const wchar_t tip[] = L"Planning Mode";
        return tip;
    }
    if (PlaceBeaconMode) {
        static const wchar_t tip[] = L"Place Beacon";
        return tip;
    }
    if (CurrentBuildingType) {
        static const wchar_t tip[] = L"Placement";
        return tip;
    }
    return nullptr;
}

void DisplayClass::CloseWindow() {
    // Tear down the display window: cancel any active placement or command
    // mode, release tracked objects, and reset the scrolling/follow state so
    // a subsequent session starts from a clean baseline.
    CurrentBuilding = nullptr;
    CurrentBuildingType = nullptr;

    RepairMode = false;
    SellMode = false;
    PowerToggleMode = false;
    PlanningMode = false;
    PlaceBeaconMode = false;

    FollowObject = false;
    ObjectToFollow = nullptr;

    DraggingRectangle = false;
    IsScrolling = false;
    ScrollTimer = 0;

    // Release the active foundation data so it is not left dangling.
    CurrentFoundation_Data = nullptr;
    CurrentFoundationCopy_Data = nullptr;

    MarkToRedraw();
}

// ============================================================================
// DisplayClass virtual methods
// ============================================================================

bool DisplayClass::MapCell(CellStruct* pMapCoord, HouseClass* pHouse) {
    // Mark a cell as mapped (terrain revealed) for the observing house.
    // A mapped cell has its terrain drawn even when no friendly unit is
    // currently within sight, which is the foundation of the fog-of-war
    // "last known" terrain display.
    if (!pMapCoord) return false;

    CellClass* pCell = GetCellAt(*pMapCoord);
    if (!pCell) return false;

    // Reveal both the center and the edges so the tile and its neighbours
    // render without a shroud seam, and mark it explored/mapped.
    pCell->SetFlag(CellFlags::CenterRevealed, true);
    pCell->SetFlag(CellFlags::EdgeRevealed, true);
    pCell->SetFlag(CellFlags::Explored, true);
    pCell->SetAltFlag(AltCellFlags::Mapped, true);

    return true;
}

bool DisplayClass::RevealFogShroud(CellStruct* pMapCoord, HouseClass* pHouse, bool bIncreaseShroudCounter) {
    // Reveal the shroud/fog over a cell for the observing house. This is
    // called whenever a friendly unit or structure gains line-of-sight to
    // the cell. The shroud counter (when incremented) prevents the shroud
    // from regrowing while at least one observer still sees the cell.
    if (!pMapCoord) return false;

    CellClass* pCell = GetCellAt(*pMapCoord);
    if (!pCell) return false;

    // Lift the shroud and clear fog so the live state of the cell shows.
    pCell->SetFlag(CellFlags::CenterRevealed, true);
    pCell->SetFlag(CellFlags::EdgeRevealed, true);
    pCell->SetFlag(CellFlags::Explored, true);
    pCell->SetFlag(CellFlags::Fogged, false);
    pCell->SetAltFlag(AltCellFlags::NoFog, true);

    // The shroud-growth counter is tracked per-house in the original
    // engine; here the NoFog alt-flag stands in for "currently observed".
    (void)bIncreaseShroudCounter;
    (void)pHouse;

    return true;
}

bool DisplayClass::MapCellFoggedness(CellStruct* pMapCoord, HouseClass* pHouse) {
    // Returns true when the cell is explored but not currently visible to
    // the observing house (fog of war). Fogged terrain is drawn dimmed and
    // hides live object positions.
    if (!pMapCoord) return false;

    CellClass* pCell = GetCellAt(*pMapCoord);
    if (!pCell) return false;

    (void)pHouse;
    return pCell->IsFogged();
}

bool DisplayClass::MapCellVisibility(CellStruct* pMapCoord, HouseClass* pHouse) {
    // Returns true when the cell is currently visible (revealed) to the
    // observing house. A visible cell renders its terrain, overlays and
    // live object positions at full brightness.
    if (!pMapCoord) return false;

    CellClass* pCell = GetCellAt(*pMapCoord);
    if (!pCell) return false;

    (void)pHouse;
    return pCell->IsRevealed();
}

bool DisplayClass::ScrollMap(DWORD dwUnk1, DWORD dwUnk2, DWORD dwUnk3) {
    // Scroll the tactical view by the supplied pixel deltas. dwUnk1 and
    // dwUnk2 are interpreted as signed horizontal/vertical deltas; dwUnk3,
    // when non-zero, overrides the scroll coast duration (in frames).
    int32 dx = static_cast<int32>(dwUnk1);
    int32 dy = static_cast<int32>(dwUnk2);

    if (dx == 0 && dy == 0) return false;

    ViewPosition.X += dx;
    ViewPosition.Y += dy;

    // Keep the view engaged so the render loop keeps the screen live while
    // the scroll coasts to a stop.
    IsScrolling = true;
    ScrollTimer = (dwUnk3 != 0) ? static_cast<int32>(dwUnk3) : 8;

    MarkToRedraw();
    return true;
}

void DisplayClass::Set_View_Dimensions(const Rectangle& rect) {
    VisibleRect = rect;
    ScreenWidth = rect.Width;
    ScreenHeight = rect.Height;
}

void DisplayClass::vt_entry_AC(DWORD dwUnk) {
    // Vtable stub 0xAC: force a redraw of the tactical display. The
    // parameter is an opaque flag preserved for binary compatibility; a
    // non-zero value requests an immediate (un-deferred) dirty notification
    // so the next render pass repaints the whole viewport.
    MarkToRedraw();

    if (TacticalClass::Instance) {
        TacticalClass::Instance->RegisterDirtyArea(
            Rectangle(0, 0, ScreenWidth, ScreenHeight), dwUnk != 0);
    }
}

void DisplayClass::vt_entry_B0(DWORD dwUnk) {
    // Vtable stub 0xB0: refresh the visible viewport after a scroll, resize
    // or mode change. Recompute the screen-space visible rectangle from the
    // current view position and re-engage the scroll coast timer so the
    // renderer keeps the freshly exposed tiles live.
    VisibleRect = Rectangle(ViewPosition.X, ViewPosition.Y, ScreenWidth, ScreenHeight);

    if (dwUnk != 0 && ScrollTimer <= 0) {
        ScrollTimer = 1;
    }

    MarkToRedraw();
}

void DisplayClass::vt_entry_B4(Point2D* pPoint) {
    // Vtable stub 0xB4: update the cached cursor position used for edge-
    // scroll detection and tooltip hit-testing.
    if (pPoint) {
        CursorPosition = *pPoint;
    }
}

// ============================================================================
// Mouse interaction
// ============================================================================

bool DisplayClass::ConvertAction(
    const CellStruct& cell, bool bShrouded, ObjectClass* pObject,
    Action action, bool dwUnk)
{
    return false;
}

void DisplayClass::LeftMouseButtonDown(const Point2D& point) {
    CursorPosition = point;
}

void DisplayClass::LeftMouseButtonUp(
    const CoordStruct& coords, const CellStruct& cell,
    ObjectClass* pObject, Action action, DWORD dwUnk2)
{
}

void DisplayClass::RightMouseButtonUp(DWORD dwUnk) {
    // Right-click is the universal "cancel" action in the tactical view:
    // it abandons any in-progress building placement, clears the repair /
    // sell / power / plan / beacon command modes, and drops any active
    // drag-selection rectangle. The current unit selection is left intact
    // so the player can re-issue orders immediately.
    (void)dwUnk;

    bool changed = false;

    if (CurrentBuilding || CurrentBuildingType) {
        CurrentBuilding = nullptr;
        CurrentBuildingType = nullptr;
        changed = true;
    }

    if (RepairMode)      { RepairMode = false;      changed = true; }
    if (SellMode)        { SellMode = false;        changed = true; }
    if (PowerToggleMode) { PowerToggleMode = false; changed = true; }
    if (PlanningMode)    { PlanningMode = false;    changed = true; }
    if (PlaceBeaconMode) { PlaceBeaconMode = false; changed = true; }

    if (DraggingRectangle) {
        DraggingRectangle = false;
        changed = true;
    }

    // Clear the active foundation so the placement grid stops tracking.
    CurrentFoundation_Data = nullptr;

    if (changed) {
        MarkToRedraw();
    }
}

// ============================================================================
// Non-virtual methods
// ============================================================================

void DisplayClass::Init() {
    ScreenWidth = 640;
    ScreenHeight = 400;
    ZoomLevel = 1.0f;
    ScrollAmount = 4;
    IsScrolling = false;
    ViewPosition = Point2D(0, 0);
    ScrollTimer = 0;
}

void DisplayClass::Draw(bool forced) {
    // Render the tactical map. The DisplayClass owns the view position and
    // follow-cam logic; the actual isometric tile and object drawing is
    // delegated to the TacticalClass renderer which reads the view state
    // from this instance.
    (void)forced;

    // Follow-cam: keep the tracked object centred on screen while it moves.
    if (FollowObject && ObjectToFollow && !ObjectToFollow->IsInLimbo) {
        CoordStruct coord;
        ObjectToFollow->GetCoords(&coord);
        CenterOn(coord);
    }

    // Keep the visible rectangle in sync with the current view position so
    // culling tests against VisibleRect stay accurate after scrolls.
    VisibleRect = Rectangle(ViewPosition.X, ViewPosition.Y, ScreenWidth, ScreenHeight);

    if (TacticalClass::Instance) {
        TacticalClass::Instance->Draw();
    }
}

void DisplayClass::Update() {
    if (IsScrolling) {
        --ScrollTimer;
        if (ScrollTimer <= 0) {
            IsScrolling = false;
        }
    }
}

void DisplayClass::Pan(int32 dx, int32 dy) {
    ViewPosition.X += dx * ScrollAmount;
    ViewPosition.Y += dy * ScrollAmount;
}

void DisplayClass::Scroll(ScrollDirType dir) {
    static const int32 scrollDirs[8][2] = {
        { 0, -1 },  // North
        { 1, -1 },  // NorthEast
        { 1, 0 },   // East
        { 1, 1 },   // SouthEast
        { 0, 1 },   // South
        { -1, 1 },  // SouthWest
        { -1, 0 },  // West
        { -1, -1 }, // NorthWest
    };

    int32 idx = static_cast<int32>(dir);
    if (idx >= 0 && idx < 8) {
        Pan(scrollDirs[idx][0], scrollDirs[idx][1]);
    }
}

void DisplayClass::CenterOn(const CoordStruct& coord) {
    // Convert world coordinates to screen position
    int32 screenX = (coord.X - coord.Y) * (CellWidthInPixels / 2) / LeptonsPerCell;
    int32 screenY = (coord.X + coord.Y) * (CellHeightInPixels / 2) / LeptonsPerCell;

    ViewPosition.X = screenX - ScreenWidth / 2;
    ViewPosition.Y = screenY - ScreenHeight / 2;
}

void DisplayClass::ZoomIn() {
    ZoomLevel *= 2.0f;
    if (ZoomLevel > 4.0f) {
        ZoomLevel = 4.0f;
    }
}

void DisplayClass::ZoomOut() {
    ZoomLevel *= 0.5f;
    if (ZoomLevel < 0.25f) {
        ZoomLevel = 0.25f;
    }
}

void DisplayClass::TacticalToScreen(const CoordStruct& coord, Point2D* outPoint) {
    if (!outPoint) return;

    // Isometric projection: world (X, Y) to screen (x, y)
    int32 screenX = (coord.X - coord.Y) * (CellWidthInPixels / 2) / LeptonsPerCell;
    int32 screenY = (coord.X + coord.Y) * (CellHeightInPixels / 2) / LeptonsPerCell;

    // Adjust for Z (height) and apply zoom
    screenY -= (coord.Z / LevelHeight);

    outPoint->X = static_cast<int32>((screenX - ViewPosition.X) * ZoomLevel);
    outPoint->Y = static_cast<int32>((screenY - ViewPosition.Y) * ZoomLevel);
}

void DisplayClass::ScreenToTactical(const Point2D& point, CoordStruct* outCoord) {
    if (!outCoord) return;

    // Reverse isometric projection: screen (x, y) to world (X, Y)
    int32 worldX = (point.X / (CellWidthInPixels / 2) + point.Y / (CellHeightInPixels / 2)) * LeptonsPerCell / 2;
    int32 worldY = (point.Y / (CellHeightInPixels / 2) - point.X / (CellWidthInPixels / 2)) * LeptonsPerCell / 2;

    outCoord->X = worldX;
    outCoord->Y = worldY;
    outCoord->Z = 0;
}

bool DisplayClass::IsInView(const CoordStruct& coord) {
    Point2D screenPoint;
    TacticalToScreen(coord, &screenPoint);

    return screenPoint.X >= 0 && screenPoint.X < ScreenWidth &&
           screenPoint.Y >= 0 && screenPoint.Y < ScreenHeight;
}

void DisplayClass::MarkToRedraw() {
    // Flag that the display needs to be redrawn next frame by marking the
    // entire viewport dirty on the tactical renderer. The renderer unions
    // this with any existing dirty region, so repeated calls are cheap.
    if (TacticalClass::Instance) {
        TacticalClass::Instance->RegisterDirtyArea(
            Rectangle(0, 0, ScreenWidth, ScreenHeight), false);
    }
}

void DisplayClass::Read_INI() {
    // Apply the engine's default display parameters. In the original binary
    // these are read from the [General] / [Scroll] sections of RULESMD.INI;
    // here we restore the documented defaults so the display behaves
    // identically before a scenario-specific INI overrides them.
    ScreenWidth = 640;
    ScreenHeight = 400;
    ZoomLevel = 1.0f;
    ScrollAmount = 4;
    IsScrolling = false;
    ScrollTimer = 0;

    FollowObject = false;
    ObjectToFollow = nullptr;

    VisibleRect = Rectangle(ViewPosition.X, ViewPosition.Y, ScreenWidth, ScreenHeight);
}

void DisplayClass::Help_Text(const wchar_t* text) {
    // Cache the supplied help/tooltip string for overlay display. The text
    // is stored in a function-local static buffer because the DisplayClass
    // header exposes no dedicated storage for transient help text.
    static wchar_t helpTextBuffer[256] = {0};

    if (!text) {
        helpTextBuffer[0] = L'\0';
        return;
    }

    // Copy up to one less than the buffer capacity and null-terminate.
    size_t i = 0;
    for (; i < 255 && text[i] != L'\0'; ++i) {
        helpTextBuffer[i] = text[i];
    }
    helpTextBuffer[i] = L'\0';
}

void DisplayClass::Mouse_Left_Release() {
    // Finalize a left-button drag. If a selection rectangle was being
    // dragged across the map, commit it (the actual unit selection is
    // resolved by LeftMouseButtonUp from the band coordinates); either way
    // the drag state is cleared so the rectangle stops tracking the cursor.
    if (DraggingRectangle) {
        DraggingRectangle = false;
        MarkToRedraw();
    }
}

// ============================================================================
// Action decision
// ============================================================================

Action DisplayClass::DecideAction(const CellStruct& cell, ObjectClass* pObject, DWORD dwUnk) {
    // Determine the available action for the given cell and object
    return Action::None;
}

// ============================================================================
// Foundation management
// ============================================================================

CellStruct* DisplayClass::FoundationBoundsSize(
    CellStruct& outBuffer, const CellStruct* pFoundationData) const
{
    if (!pFoundationData) return nullptr;

    outBuffer.X = 0;
    outBuffer.Y = 0;

    // Walk through the foundation data to find the bounding size
    const CellStruct* iter = pFoundationData;
    while (iter->X != 0x7FFF || iter->Y != 0x7FFF) {
        if (iter->X > outBuffer.X) outBuffer.X = iter->X;
        if (iter->Y > outBuffer.Y) outBuffer.Y = iter->Y;
        ++iter;
    }

    return &outBuffer;
}

CellStruct DisplayClass::FoundationBoundsSize(const CellStruct* pFoundationData) const {
    CellStruct buf;
    if (!pFoundationData) return buf;
    FoundationBoundsSize(buf, pFoundationData);
    return buf;
}

void DisplayClass::MarkFoundation(CellStruct* BaseCell, bool Mark) {
    // Mark or unmark foundation cells for building placement
    (void)BaseCell;
    (void)Mark;
}

void DisplayClass::SetActiveFoundation(CellStruct* Coords) {
    CurrentFoundation_CenterCell = *Coords;
}

// ============================================================================
// Layer management
// ============================================================================

// File-local render layer. The DisplayClass header exposes no storage for the
// per-layer object list, so it is kept here as a translation-unit-local
// container, mirroring the original engine's per-layer object arrays.
static DynamicVectorClass<ObjectClass*> g_DisplayLayer;

void DisplayClass::Submit(ObjectClass* pObject) {
    // Register an object with the display's render layer so it is considered
    // for drawing each frame. Objects that are in limbo are skipped, and an
    // object already present is not added twice (preserving draw order).
    if (!pObject || pObject->IsInLimbo) return;

    for (int32 i = 0; i < g_DisplayLayer.Count; ++i) {
        if (g_DisplayLayer[i] == pObject) return;
    }

    g_DisplayLayer.Add(pObject);
}

void DisplayClass::Remove(ObjectClass* pObject) {
    // Remove an object from the render layer (e.g. when it enters limbo or is
    // destroyed). Maintains list ordering by shifting successors down, which
    // keeps the relative Y-sorted draw order stable for the remaining objects.
    if (!pObject) return;

    for (int32 i = 0; i < g_DisplayLayer.Count; ++i) {
        if (g_DisplayLayer[i] == pObject) {
            g_DisplayLayer.Remove(i);
            return;
        }
    }
}

// ============================================================================
// Click processing
// ============================================================================

bool DisplayClass::ProcessClickCoords(Point2D* src, CellStruct* XYdst, CoordStruct* XYZdst,
                                      ObjectClass** Target, BYTE* a5, BYTE* a6)
{
    // Process click coordinates to determine target cell and object
    return false;
}

// ============================================================================
// File-local helper functions
//
//  These provide scroll acceleration, edge-scroll detection, zoom smoothing,
//  isometric coordinate conversion utilities, fade transitions, screen flash
//  effects, and view-culling helpers used by the display manager.  Because
//  the DisplayClass header cannot be modified, these utilities are declared
//  as free functions in the anonymous namespace and operate on the public
//  state exposed by DisplayClass.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Scroll tuning constants
// --------------------------------------------------------------------------
constexpr int32  SCROLL_ACCEL_FRAMES      = 8;    // frames to reach full speed
constexpr int32  SCROLL_EDGE_THRESHOLD    = 4;    // pixels from window edge
constexpr int32  SCROLL_MOMENTUM_DECAY    = 4;    // momentum decay per frame
constexpr float  ZOOM_MIN                 = 0.25f;
constexpr float  ZOOM_MAX                 = 4.0f;
constexpr float  ZOOM_STEP                = 1.25f;
constexpr int32  FADE_DEFAULT_FRAMES      = 15;
constexpr int32  FLASH_DEFAULT_FRAMES     = 30;

// --------------------------------------------------------------------------
// Scroll state - tracks the current scroll velocity, momentum, and the
// active edge-scroll direction. Kept as file-local state so the helpers
// can mutate it without touching the header.
// --------------------------------------------------------------------------
struct ScrollState {
    int32 VelocityX;
    int32 VelocityY;
    int32 MomentumX;
    int32 MomentumY;
    int32 AccelFrames;
    int32 EdgeDir; // bitmask of ScrollDirType values
    bool  IsActive;

    ScrollState()
        : VelocityX(0), VelocityY(0),
          MomentumX(0), MomentumY(0),
          AccelFrames(0), EdgeDir(0), IsActive(false) {}
};

ScrollState g_ScrollState;

// --------------------------------------------------------------------------
// Fade state - tracks the current screen fade (alpha, target, speed).
// --------------------------------------------------------------------------
struct FadeState {
    int32 CurrentAlpha;   // 0..255
    int32 TargetAlpha;
    int32 StepPerFrame;
    bool  IsActive;

    FadeState() : CurrentAlpha(0), TargetAlpha(0), StepPerFrame(0), IsActive(false) {}
};

FadeState g_FadeState;

// --------------------------------------------------------------------------
// Flash state - tracks the current screen flash overlay.
// --------------------------------------------------------------------------
struct FlashState {
    int32      FramesRemaining;
    int32      TotalFrames;
    ColorStruct Color;
    int32      MaxAlpha;
    bool       IsActive;

    FlashState()
        : FramesRemaining(0), TotalFrames(0),
          Color(0, 0, 0), MaxAlpha(0), IsActive(false) {}
};

FlashState g_FlashState;

// --------------------------------------------------------------------------
// ClampZoom - Clamps a zoom factor to the valid range.
// --------------------------------------------------------------------------
float ClampZoom(float zoom)
{
    if (zoom < ZOOM_MIN) return ZOOM_MIN;
    if (zoom > ZOOM_MAX) return ZOOM_MAX;
    return zoom;
}

// --------------------------------------------------------------------------
// ApplyZoomSmoothing - Returns the next zoom value approaching the target
// at the given rate. Used for smooth zoom transitions.
// --------------------------------------------------------------------------
float ApplyZoomSmoothing(float current, float target, float rate)
{
    if (rate <= 0.0f) return target;
    float diff = target - current;
    if (diff > -rate && diff < rate) return target;
    return current + diff * rate;
}

// --------------------------------------------------------------------------
// ComputeZoomFromDelta - Returns a new zoom factor after applying a wheel
// delta (positive = zoom in, negative = zoom out).
// --------------------------------------------------------------------------
float ComputeZoomFromDelta(float currentZoom, int32 delta)
{
    if (delta == 0) return currentZoom;
    float factor = (delta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
    int32 mag = (delta > 0) ? delta : -delta;
    for (int32 i = 1; i < mag; ++i) {
        factor *= (delta > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
    }
    return ClampZoom(currentZoom * factor);
}

// --------------------------------------------------------------------------
// IsometricToScreen - Core isometric projection from world (X,Y,Z) to
// screen (x,y). Exposed as a helper so other modules can compute screen
// positions without going through the instance.
// --------------------------------------------------------------------------
Point2D IsometricToScreen(const CoordStruct& coord, const Point2D& view, float zoom)
{
    int32 screenX = (coord.X - coord.Y) * (CellWidthInPixels / 2) / LeptonsPerCell;
    int32 screenY = (coord.X + coord.Y) * (CellHeightInPixels / 2) / LeptonsPerCell;
    screenY -= (coord.Z / LevelHeight);

    Point2D result;
    result.X = static_cast<int32>((screenX - view.X) * zoom);
    result.Y = static_cast<int32>((screenY - view.Y) * zoom);
    return result;
}

// --------------------------------------------------------------------------
// ScreenToIsometric - Inverse projection from screen (x,y) to world (X,Y).
// --------------------------------------------------------------------------
CoordStruct ScreenToIsometric(const Point2D& point, const Point2D& view, float zoom)
{
    if (zoom <= 0.0f) zoom = 1.0f;
    float invZoom = 1.0f / zoom;

    int32 sx = static_cast<int32>((point.X) * invZoom + view.X);
    int32 sy = static_cast<int32>((point.Y) * invZoom + view.Y);

    // Inverse of the isometric transform:
    //   sx = (X - Y) * (CW/2) / LPC
    //   sy = (X + Y) * (CH/2) / LPC
    // Solving for X and Y:
    int32 termX = sx * LeptonsPerCell / (CellWidthInPixels / 2);
    int32 termY = sy * LeptonsPerCell / (CellHeightInPixels / 2);

    CoordStruct result;
    result.X = (termX + termY) / 2;
    result.Y = (termY - termX) / 2;
    result.Z = 0;
    return result;
}

// --------------------------------------------------------------------------
// IsCoordInViewRect - Returns true if a screen-space point lies within the
// given rectangle (typically the visible viewport).
// --------------------------------------------------------------------------
bool IsPointInViewRect(const Point2D& point, int32 width, int32 height)
{
    return point.X >= 0 && point.X < width &&
           point.Y >= 0 && point.Y < height;
}

// --------------------------------------------------------------------------
// IsCoordInView - Returns true if a world coordinate projects into the
// visible viewport with the given margin (in pixels).
// --------------------------------------------------------------------------
bool IsCoordInView(const CoordStruct& coord, const Point2D& view, float zoom,
                   int32 width, int32 height, int32 margin)
{
    Point2D screen = IsometricToScreen(coord, view, zoom);
    return screen.X >= -margin && screen.X < width + margin &&
           screen.Y >= -margin && screen.Y < height + margin;
}

// --------------------------------------------------------------------------
// ComputeScrollEdge - Returns a bitmask of active scroll directions based
// on the cursor position relative to the screen edges.
// --------------------------------------------------------------------------
int32 ComputeScrollEdge(const Point2D& cursor, int32 width, int32 height, int32 threshold)
{
    if (threshold <= 0) threshold = SCROLL_EDGE_THRESHOLD;

    int32 mask = 0;
    if (cursor.X < threshold)        mask |= (1 << static_cast<int32>(ScrollDirType::West));
    if (cursor.X > width - threshold) mask |= (1 << static_cast<int32>(ScrollDirType::East));
    if (cursor.Y < threshold)        mask |= (1 << static_cast<int32>(ScrollDirType::North));
    if (cursor.Y > height - threshold) mask |= (1 << static_cast<int32>(ScrollDirType::South));

    // Combine into diagonals where appropriate.
    bool west  = (mask & (1 << static_cast<int32>(ScrollDirType::West)))  != 0;
    bool east  = (mask & (1 << static_cast<int32>(ScrollDirType::East)))  != 0;
    bool north = (mask & (1 << static_cast<int32>(ScrollDirType::North))) != 0;
    bool south = (mask & (1 << static_cast<int32>(ScrollDirType::South))) != 0;

    if (north && west) mask |= (1 << static_cast<int32>(ScrollDirType::NorthWest));
    if (north && east) mask |= (1 << static_cast<int32>(ScrollDirType::NorthEast));
    if (south && west) mask |= (1 << static_cast<int32>(ScrollDirType::SouthWest));
    if (south && east) mask |= (1 << static_cast<int32>(ScrollDirType::SouthEast));

    return mask;
}

// --------------------------------------------------------------------------
// ScrollDirToDelta - Converts a scroll direction to a (dx, dy) unit delta.
// --------------------------------------------------------------------------
void ScrollDirToDelta(ScrollDirType dir, int32& outDx, int32& outDy)
{
    static const int32 deltas[8][2] = {
        {  0, -1 }, // North
        {  1, -1 }, // NorthEast
        {  1,  0 }, // East
        {  1,  1 }, // SouthEast
        {  0,  1 }, // South
        { -1,  1 }, // SouthWest
        { -1,  0 }, // West
        { -1, -1 }, // NorthWest
    };
    int32 idx = static_cast<int32>(dir);
    if (idx < 0 || idx >= 8) {
        outDx = 0;
        outDy = 0;
        return;
    }
    outDx = deltas[idx][0];
    outDy = deltas[idx][1];
}

// --------------------------------------------------------------------------
// UpdateScrollVelocity - Advances the scroll velocity toward the target
// direction with smooth acceleration.
// --------------------------------------------------------------------------
void UpdateScrollVelocity(ScrollState& state, int32 targetDx, int32 targetDy,
                          int32 maxSpeed)
{
    if (maxSpeed <= 0) maxSpeed = 8;

    if (targetDx == 0 && targetDy == 0) {
        // Decelerate when no input.
        if (state.VelocityX > 0) {
            state.VelocityX -= 1;
            if (state.VelocityX < 0) state.VelocityX = 0;
        } else if (state.VelocityX < 0) {
            state.VelocityX += 1;
            if (state.VelocityX > 0) state.VelocityX = 0;
        }
        if (state.VelocityY > 0) {
            state.VelocityY -= 1;
            if (state.VelocityY < 0) state.VelocityY = 0;
        } else if (state.VelocityY < 0) {
            state.VelocityY += 1;
            if (state.VelocityY > 0) state.VelocityY = 0;
        }
        state.AccelFrames = 0;
        state.IsActive = (state.VelocityX != 0 || state.VelocityY != 0);
        return;
    }

    // Accelerate toward the target.
    if (state.AccelFrames < SCROLL_ACCEL_FRAMES) {
        ++state.AccelFrames;
    }

    float t = static_cast<float>(state.AccelFrames) /
              static_cast<float>(SCROLL_ACCEL_FRAMES);
    int32 speed = static_cast<int32>(t * maxSpeed);
    if (speed < 1) speed = 1;

    state.VelocityX = targetDx * speed;
    state.VelocityY = targetDy * speed;
    state.IsActive = true;
}

// --------------------------------------------------------------------------
// ApplyScrollMomentum - Adds residual momentum to the view position so the
// camera continues to drift briefly after the user stops scrolling.
// --------------------------------------------------------------------------
Point2D ApplyScrollMomentum(const Point2D& view, ScrollState& state)
{
    Point2D result = view;
    result.X += state.MomentumX;
    result.Y += state.MomentumY;

    // Decay momentum.
    if (state.MomentumX > 0) {
        state.MomentumX -= SCROLL_MOMENTUM_DECAY;
        if (state.MomentumX < 0) state.MomentumX = 0;
    } else if (state.MomentumX < 0) {
        state.MomentumX += SCROLL_MOMENTUM_DECAY;
        if (state.MomentumX > 0) state.MomentumX = 0;
    }
    if (state.MomentumY > 0) {
        state.MomentumY -= SCROLL_MOMENTUM_DECAY;
        if (state.MomentumY < 0) state.MomentumY = 0;
    } else if (state.MomentumY < 0) {
        state.MomentumY += SCROLL_MOMENTUM_DECAY;
        if (state.MomentumY > 0) state.MomentumY = 0;
    }
    return result;
}

// --------------------------------------------------------------------------
// ClampViewToBounds - Clamps the view position so the camera cannot scroll
// past the map edges.
// --------------------------------------------------------------------------
Point2D ClampViewToBounds(const Point2D& view, int32 mapWidth, int32 mapHeight,
                          int32 screenW, int32 screenH)
{
    Point2D result = view;
    int32 maxX = (mapWidth * CellWidthInPixels) - screenW;
    int32 maxY = (mapHeight * CellHeightInPixels * 2) - screenH;
    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;
    if (result.X < 0) result.X = 0;
    if (result.Y < 0) result.Y = 0;
    if (result.X > maxX) result.X = maxX;
    if (result.Y > maxY) result.Y = maxY;
    return result;
}

// --------------------------------------------------------------------------
// CenterViewOn - Returns the view position that centers the given world
// coordinate on screen.
// --------------------------------------------------------------------------
Point2D CenterViewOn(const CoordStruct& coord, int32 screenW, int32 screenH)
{
    int32 screenX = (coord.X - coord.Y) * (CellWidthInPixels / 2) / LeptonsPerCell;
    int32 screenY = (coord.X + coord.Y) * (CellHeightInPixels / 2) / LeptonsPerCell;
    screenY -= (coord.Z / LevelHeight);
    return Point2D(screenX - screenW / 2, screenY - screenH / 2);
}

// --------------------------------------------------------------------------
// StartFade - Begins a screen fade transition toward the target alpha.
// --------------------------------------------------------------------------
void StartFade(FadeState& state, int32 targetAlpha, int32 frames)
{
    if (frames <= 0) frames = FADE_DEFAULT_FRAMES;
    state.TargetAlpha = targetAlpha;
    int32 diff = targetAlpha - state.CurrentAlpha;
    if (diff < 0) diff = -diff;
    state.StepPerFrame = (diff + frames - 1) / frames;
    if (state.StepPerFrame <= 0) state.StepPerFrame = 1;
    state.IsActive = true;
}

// --------------------------------------------------------------------------
// UpdateFade - Advances the fade transition by one frame and returns the
// current alpha.
// --------------------------------------------------------------------------
int32 UpdateFade(FadeState& state)
{
    if (!state.IsActive) return state.CurrentAlpha;

    if (state.CurrentAlpha < state.TargetAlpha) {
        state.CurrentAlpha += state.StepPerFrame;
        if (state.CurrentAlpha >= state.TargetAlpha) {
            state.CurrentAlpha = state.TargetAlpha;
            state.IsActive = false;
        }
    } else if (state.CurrentAlpha > state.TargetAlpha) {
        state.CurrentAlpha -= state.StepPerFrame;
        if (state.CurrentAlpha <= state.TargetAlpha) {
            state.CurrentAlpha = state.TargetAlpha;
            state.IsActive = false;
        }
    }
    return state.CurrentAlpha;
}

// --------------------------------------------------------------------------
// StartFlash - Begins a screen flash with the given color and duration.
// --------------------------------------------------------------------------
void StartFlash(FlashState& state, const ColorStruct& color, int32 frames, int32 maxAlpha)
{
    if (frames <= 0) frames = FLASH_DEFAULT_FRAMES;
    if (maxAlpha <= 0) maxAlpha = 200;
    if (maxAlpha > 255) maxAlpha = 255;
    state.Color = color;
    state.TotalFrames = frames;
    state.FramesRemaining = frames;
    state.MaxAlpha = maxAlpha;
    state.IsActive = true;
}

// --------------------------------------------------------------------------
// UpdateFlash - Advances the flash effect by one frame and returns the
// current alpha (0 if the flash has ended).
// --------------------------------------------------------------------------
int32 UpdateFlash(FlashState& state)
{
    if (!state.IsActive) return 0;
    if (state.FramesRemaining <= 0) {
        state.IsActive = false;
        return 0;
    }
    --state.FramesRemaining;
    // Fade out linearly.
    float t = static_cast<float>(state.FramesRemaining) /
              static_cast<float>(state.TotalFrames);
    int32 alpha = static_cast<int32>(t * state.MaxAlpha);
    if (alpha < 0) alpha = 0;
    if (alpha > state.MaxAlpha) alpha = state.MaxAlpha;
    if (state.FramesRemaining == 0) {
        state.IsActive = false;
    }
    return alpha;
}

// --------------------------------------------------------------------------
// GetFlashColor - Returns the current flash color.
// --------------------------------------------------------------------------
ColorStruct GetFlashColor(const FlashState& state)
{
    return state.Color;
}

// --------------------------------------------------------------------------
// IsFading - Returns true if a fade transition is in progress.
// --------------------------------------------------------------------------
bool IsFading(const FadeState& state)
{
    return state.IsActive;
}

// --------------------------------------------------------------------------
// IsFlashing - Returns true if a screen flash is in progress.
// --------------------------------------------------------------------------
bool IsFlashing(const FlashState& state)
{
    return state.IsActive;
}

// --------------------------------------------------------------------------
// LerpColor - Linearly interpolates between two colors.
// --------------------------------------------------------------------------
ColorStruct LerpColor(const ColorStruct& a, const ColorStruct& b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return ColorStruct(
        static_cast<uint8>(a.R + (b.R - a.R) * t),
        static_cast<uint8>(a.G + (b.G - a.G) * t),
        static_cast<uint8>(a.B + (b.B - a.B) * t),
        static_cast<uint8>(a.A + (b.A - a.A) * t));
}

// --------------------------------------------------------------------------
// ComputeShakeOffset - Returns a screen-shake offset for the given frame
// and intensity. Used by explosions and superweapon detonations.
// --------------------------------------------------------------------------
Point2D ComputeShakeOffset(int32 frame, int32 intensity, int32 duration)
{
    if (intensity <= 0 || duration <= 0) return Point2D(0, 0);
    if (frame >= duration) return Point2D(0, 0);

    float t = 1.0f - static_cast<float>(frame) / static_cast<float>(duration);
    int32 amp = static_cast<int32>(intensity * t);
    if (amp <= 0) return Point2D(0, 0);

    // Pseudo-random offsets derived from the frame counter.
    int32 dx = ((frame * 73) % (amp * 2 + 1)) - amp;
    int32 dy = ((frame * 137) % (amp * 2 + 1)) - amp;
    return Point2D(dx, dy);
}

// --------------------------------------------------------------------------
// CellToScreenCenter - Returns the screen-space center of a map cell.
// --------------------------------------------------------------------------
Point2D CellToScreenCenter(const CellStruct& cell, const Point2D& view, float zoom)
{
    CoordStruct coord = CoordMath::CellToCoord(cell);
    coord.X += LeptonsPerCell / 2;
    coord.Y += LeptonsPerCell / 2;
    return IsometricToScreen(coord, view, zoom);
}

// --------------------------------------------------------------------------
// ScreenToCell - Converts a screen-space point to the cell it lies within.
// --------------------------------------------------------------------------
CellStruct ScreenToCell(const Point2D& point, const Point2D& view, float zoom)
{
    CoordStruct coord = ScreenToIsometric(point, view, zoom);
    return CoordMath::CoordToCell(coord);
}

// --------------------------------------------------------------------------
// ComputeVisibleCellRange - Computes the range of map cells visible in the
// current viewport. Writes the result into outMin and outMax.
// --------------------------------------------------------------------------
void ComputeVisibleCellRange(const Point2D& view, float zoom,
                             int32 screenW, int32 screenH,
                             CellStruct& outMin, CellStruct& outMax)
{
    Point2D topLeft(0, 0);
    Point2D bottomRight(screenW, screenH);
    outMin = ScreenToCell(topLeft, view, zoom);
    outMax = ScreenToCell(bottomRight, view, zoom);

    // Add a one-cell margin to catch partially visible cells.
    if (outMin.X > 0) outMin.X = static_cast<int16>(outMin.X - 1);
    if (outMin.Y > 0) outMin.Y = static_cast<int16>(outMin.Y - 1);
    outMax.X = static_cast<int16>(outMax.X + 1);
    outMax.Y = static_cast<int16>(outMax.Y + 1);
}

// --------------------------------------------------------------------------
// DescribeDisplayState - Returns a short string describing the display
// state for debugging overlays.
// --------------------------------------------------------------------------
const char* DescribeDisplayState(const DisplayClass& display)
{
    if (display.IsScrolling) return "Scrolling";
    if (display.FollowObject && display.ObjectToFollow) return "Following";
    if (display.DraggingRectangle) return "Dragging";
    return "Static";
}

// --------------------------------------------------------------------------
// ShouldRedraw - Returns true if the display should be redrawn this frame
// based on active scroll, fade, or flash effects.
// --------------------------------------------------------------------------
bool ShouldRedraw(const DisplayClass& display)
{
    if (display.IsScrolling) return true;
    if (g_FadeState.IsActive) return true;
    if (g_FlashState.IsActive) return true;
    if (display.FollowObject && display.ObjectToFollow) return true;
    return false;
}

} // namespace

// ============================================================================
// File-local entry points that bridge the DisplayClass to the helper
// functions above. These are kept as file-local free functions so the
// header does not need to change, yet other translation units can invoke
// them when needed.
// ============================================================================

extern "C" {

// ----------------------------------------------------------------------------
// Display_ClampZoom - Clamp a zoom factor to the valid range.
// ----------------------------------------------------------------------------
float Display_ClampZoom(float zoom)
{
    return ClampZoom(zoom);
}

// ----------------------------------------------------------------------------
// Display_ApplyZoomSmoothing - Smooth zoom transition step.
// ----------------------------------------------------------------------------
float Display_ApplyZoomSmoothing(float current, float target, float rate)
{
    return ApplyZoomSmoothing(current, target, rate);
}

// ----------------------------------------------------------------------------
// Display_ComputeZoomFromDelta - Zoom factor from a wheel delta.
// ----------------------------------------------------------------------------
float Display_ComputeZoomFromDelta(float currentZoom, int32 delta)
{
    return ComputeZoomFromDelta(currentZoom, delta);
}

// ----------------------------------------------------------------------------
// Display_IsometricToScreen - World to screen projection.
// ----------------------------------------------------------------------------
Point2D Display_IsometricToScreen(const CoordStruct* pCoord,
                                  const Point2D* pView, float zoom)
{
    if (!pCoord || !pView) return Point2D(0, 0);
    return IsometricToScreen(*pCoord, *pView, zoom);
}

// ----------------------------------------------------------------------------
// Display_ScreenToIsometric - Screen to world projection.
// ----------------------------------------------------------------------------
CoordStruct Display_ScreenToIsometric(const Point2D* pPoint,
                                      const Point2D* pView, float zoom)
{
    if (!pPoint || !pView) return CoordStruct(0, 0, 0);
    return ScreenToIsometric(*pPoint, *pView, zoom);
}

// ----------------------------------------------------------------------------
// Display_IsCoordInView - Viewport culling check.
// ----------------------------------------------------------------------------
bool Display_IsCoordInView(const CoordStruct* pCoord, const Point2D* pView,
                           float zoom, int32 width, int32 height, int32 margin)
{
    if (!pCoord || !pView) return false;
    return IsCoordInView(*pCoord, *pView, zoom, width, height, margin);
}

// ----------------------------------------------------------------------------
// Display_ComputeScrollEdge - Edge scroll direction bitmask.
// ----------------------------------------------------------------------------
int32 Display_ComputeScrollEdge(const Point2D* pCursor, int32 width, int32 height, int32 threshold)
{
    if (!pCursor) return 0;
    return ComputeScrollEdge(*pCursor, width, height, threshold);
}

// ----------------------------------------------------------------------------
// Display_ScrollDirToDelta - Direction to (dx, dy).
// ----------------------------------------------------------------------------
void Display_ScrollDirToDelta(ScrollDirType dir, int32* outDx, int32* outDy)
{
    if (!outDx || !outDy) return;
    ScrollDirToDelta(dir, *outDx, *outDy);
}

// ----------------------------------------------------------------------------
// Display_UpdateScrollVelocity - Advance scroll velocity.
// ----------------------------------------------------------------------------
void Display_UpdateScrollVelocity(int32 targetDx, int32 targetDy, int32 maxSpeed)
{
    UpdateScrollVelocity(g_ScrollState, targetDx, targetDy, maxSpeed);
}

// ----------------------------------------------------------------------------
// Display_ApplyScrollMomentum - Apply momentum to a view position.
// ----------------------------------------------------------------------------
Point2D Display_ApplyScrollMomentum(const Point2D* pView)
{
    if (!pView) return Point2D(0, 0);
    return ApplyScrollMomentum(*pView, g_ScrollState);
}

// ----------------------------------------------------------------------------
// Display_GetScrollVelocity - Current scroll velocity.
// ----------------------------------------------------------------------------
void Display_GetScrollVelocity(int32* outVx, int32* outVy)
{
    if (!outVx || !outVy) return;
    *outVx = g_ScrollState.VelocityX;
    *outVy = g_ScrollState.VelocityY;
}

// ----------------------------------------------------------------------------
// Display_SetScrollMomentum - Set residual momentum.
// ----------------------------------------------------------------------------
void Display_SetScrollMomentum(int32 mx, int32 my)
{
    g_ScrollState.MomentumX = mx;
    g_ScrollState.MomentumY = my;
}

// ----------------------------------------------------------------------------
// Display_ClampViewToBounds - Clamp view to map bounds.
// ----------------------------------------------------------------------------
Point2D Display_ClampViewToBounds(const Point2D* pView, int32 mapW, int32 mapH,
                                  int32 screenW, int32 screenH)
{
    if (!pView) return Point2D(0, 0);
    return ClampViewToBounds(*pView, mapW, mapH, screenW, screenH);
}

// ----------------------------------------------------------------------------
// Display_CenterViewOn - View position to center on a coord.
// ----------------------------------------------------------------------------
Point2D Display_CenterViewOn(const CoordStruct* pCoord, int32 screenW, int32 screenH)
{
    if (!pCoord) return Point2D(0, 0);
    return CenterViewOn(*pCoord, screenW, screenH);
}

// ----------------------------------------------------------------------------
// Display_StartFade - Begin a fade transition.
// ----------------------------------------------------------------------------
void Display_StartFade(int32 targetAlpha, int32 frames)
{
    StartFade(g_FadeState, targetAlpha, frames);
}

// ----------------------------------------------------------------------------
// Display_UpdateFade - Advance the fade by one frame.
// ----------------------------------------------------------------------------
int32 Display_UpdateFade()
{
    return UpdateFade(g_FadeState);
}

// ----------------------------------------------------------------------------
// Display_GetFadeAlpha - Current fade alpha (0..255).
// ----------------------------------------------------------------------------
int32 Display_GetFadeAlpha()
{
    return g_FadeState.CurrentAlpha;
}

// ----------------------------------------------------------------------------
// Display_IsFading - Is a fade in progress?
// ----------------------------------------------------------------------------
bool Display_IsFading()
{
    return IsFading(g_FadeState);
}

// ----------------------------------------------------------------------------
// Display_StartFlash - Begin a screen flash.
// ----------------------------------------------------------------------------
void Display_StartFlash(const ColorStruct* pColor, int32 frames, int32 maxAlpha)
{
    if (!pColor) {
        ColorStruct white(255, 255, 255);
        StartFlash(g_FlashState, white, frames, maxAlpha);
    } else {
        StartFlash(g_FlashState, *pColor, frames, maxAlpha);
    }
}

// ----------------------------------------------------------------------------
// Display_UpdateFlash - Advance the flash by one frame.
// ----------------------------------------------------------------------------
int32 Display_UpdateFlash()
{
    return UpdateFlash(g_FlashState);
}

// ----------------------------------------------------------------------------
// Display_GetFlashColor - Current flash color.
// ----------------------------------------------------------------------------
ColorStruct Display_GetFlashColor()
{
    return GetFlashColor(g_FlashState);
}

// ----------------------------------------------------------------------------
// Display_IsFlashing - Is a flash in progress?
// ----------------------------------------------------------------------------
bool Display_IsFlashing()
{
    return IsFlashing(g_FlashState);
}

// ----------------------------------------------------------------------------
// Display_LerpColor - Interpolate between two colors.
// ----------------------------------------------------------------------------
ColorStruct Display_LerpColor(const ColorStruct* pA, const ColorStruct* pB, float t)
{
    if (!pA || !pB) return ColorStruct(0, 0, 0);
    return LerpColor(*pA, *pB, t);
}

// ----------------------------------------------------------------------------
// Display_ComputeShakeOffset - Screen shake offset.
// ----------------------------------------------------------------------------
Point2D Display_ComputeShakeOffset(int32 frame, int32 intensity, int32 duration)
{
    return ComputeShakeOffset(frame, intensity, duration);
}

// ----------------------------------------------------------------------------
// Display_CellToScreenCenter - Screen center of a cell.
// ----------------------------------------------------------------------------
Point2D Display_CellToScreenCenter(const CellStruct* pCell, const Point2D* pView, float zoom)
{
    if (!pCell || !pView) return Point2D(0, 0);
    return CellToScreenCenter(*pCell, *pView, zoom);
}

// ----------------------------------------------------------------------------
// Display_ScreenToCell - Cell under a screen point.
// ----------------------------------------------------------------------------
CellStruct Display_ScreenToCell(const Point2D* pPoint, const Point2D* pView, float zoom)
{
    if (!pPoint || !pView) return CellStruct(0, 0);
    return ScreenToCell(*pPoint, *pView, zoom);
}

// ----------------------------------------------------------------------------
// Display_ComputeVisibleCellRange - Visible cell range.
// ----------------------------------------------------------------------------
void Display_ComputeVisibleCellRange(const Point2D* pView, float zoom,
                                     int32 screenW, int32 screenH,
                                     CellStruct* pOutMin, CellStruct* pOutMax)
{
    if (!pView || !pOutMin || !pOutMax) return;
    ComputeVisibleCellRange(*pView, zoom, screenW, screenH, *pOutMin, *pOutMax);
}

// ----------------------------------------------------------------------------
// Display_DescribeState - Debug state string.
// ----------------------------------------------------------------------------
const char* Display_DescribeState(const DisplayClass* pDisplay)
{
    if (!pDisplay) return "None";
    return DescribeDisplayState(*pDisplay);
}

// ----------------------------------------------------------------------------
// Display_ShouldRedraw - Should the display redraw this frame?
// ----------------------------------------------------------------------------
bool Display_ShouldRedraw(const DisplayClass* pDisplay)
{
    if (!pDisplay) return false;
    return ShouldRedraw(*pDisplay);
}

// ----------------------------------------------------------------------------
// Display_ResetScrollState - Clear all scroll state.
// ----------------------------------------------------------------------------
void Display_ResetScrollState()
{
    g_ScrollState = ScrollState();
}

// ----------------------------------------------------------------------------
// Display_ResetFadeState - Clear the fade state.
// ----------------------------------------------------------------------------
void Display_ResetFadeState()
{
    g_FadeState = FadeState();
}

// ----------------------------------------------------------------------------
// Display_ResetFlashState - Clear the flash state.
// ----------------------------------------------------------------------------
void Display_ResetFlashState()
{
    g_FlashState = FlashState();
}

} // extern "C"