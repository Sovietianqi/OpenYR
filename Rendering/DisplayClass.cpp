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
#include "../Rendering/RadarClass.h"
#include "../Rendering/SidebarClass.h"
#include "../Rendering/GScreenClass.h"
#include "../Game/Game.h"
#include "../Game/Externs.h"

#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <new>

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

// =============================================================================
// ScrollMap - Scroll the tactical view (asm: DisplayClass::Scroll_Map asm:940)
//
// asm: Scroll_Map(this, a2: ptr, arg_4: ptr, arg_8: bool) retn 0Ch。
//   v = *arg_4
//   if (v == 0) { if (!arg_8) return false; }        ; 无位移且非强制 → 不动
//   if (!arg_8) {                                    ; 非强制：判边界
//        r = TacticalMap->sub_6DA230(a2)             ; 夹取到地图边界
//        return (r != -1)                            ; 是否发生有效滚动
//   }
//   TacticalMap->sub_6D8530(a2, v)                   ; 强制滚动
//   return true
// OpenYR 把 a2 视为滚动向量指针，sub_6DA230/sub_6D8530 合并为"夹取/直接"
// 两种滚动方式（沿用 TacticalClass 的视图中心）。
// =============================================================================
bool DisplayClass::ScrollMap(DWORD dwUnk1, DWORD dwUnk2, DWORD dwUnk3) {
    const Point2D* pDelta = reinterpret_cast<const Point2D*>(static_cast<uintptr_t>(dwUnk1));
    const int32*   pAmount = reinterpret_cast<const int32*>(static_cast<uintptr_t>(dwUnk2));
    const bool     force   = (dwUnk3 != 0);

    if (pAmount == nullptr)
        return false;

    const int32 v = *pAmount;
    if (v == 0 && !force)
        return false;                          // asm: test eax,eax ; jnz ...

    const int32 dx = (pDelta != nullptr) ? pDelta->X : 0;
    const int32 dy = (pDelta != nullptr) ? pDelta->Y : 0;

    if (!force)
    {
        // asm: r = TacticalMap->sub_6DA230(a2) ; return r != -1
        // 非强制滚动：把新视图中心夹取到地图范围，越界则不滚动。
        if (TacticalClass::Instance == nullptr)
            return false;

        Point2D center = TacticalClass::Instance->TacticalPos;
        int32 ncx = center.X + dx * v;
        int32 ncy = center.Y + dy * v;

        int32 maxX = (MapWidth  > 0) ? MapWidth  * CellWidthInPixels  : 0;
        int32 maxY = (MapHeight > 0) ? MapHeight * CellHeightInPixels : 0;
        bool inBounds = (ncx >= 0 && ncx <= maxX && ncy >= 0 && ncy <= maxY);

        if (!inBounds)
            return false;                      // asm: -1 分支

        TacticalClass::Instance->LastTacticalPos = center;
        TacticalClass::Instance->TacticalPos = Point2D(ncx, ncy);
        IsScrolling = true;
        MarkToRedraw();
        return true;
    }

    // asm: TacticalMap->sub_6D8530(a2, v) ; return true
    if (TacticalClass::Instance != nullptr)
    {
        Point2D center = TacticalClass::Instance->TacticalPos;
        TacticalClass::Instance->LastTacticalPos = center;
        TacticalClass::Instance->TacticalPos =
            Point2D(center.X + dx * v, center.Y + dy * v);
    }
    IsScrolling = true;
    ScrollTimer = (ScrollTimer > 0) ? ScrollTimer : 8;
    MarkToRedraw();
    return true;
}

// =============================================================================
// Set_View_Dimensions - Apply the viewport rectangle (asm: asm:221)
//
// asm: DisplayClass::Set_View_Dimensions(dwLTRB* a1) retn 4。
//   1) 把 a1 写入 rect_mouseclass_vp（全屏视口）；WWDebugString 打印。
//   2) 若 TacticalMap 非空：TacticalMap->sub_6D5F60(rect)（重建战术视口）。
//   3) 重建 ZBuffer（sub_7BC970）、ABuffer（ASurface::ASurface），
//      并把尺寸灌进 GadgetDisplay / MessageListClass::Init。
// OpenYR 未建模 ZBuffer/ABuffer/GadgetDisplay/MessageList 渲染后端，按语义
// 等价落空为：同步本类视口矩形与尺寸，并驱动战术层同步视口。
// =============================================================================
void DisplayClass::Set_View_Dimensions(const Rectangle& rect) {
    VisibleRect = rect;
    MapRect     = rect;
    ScreenWidth  = rect.Width;
    ScreenHeight = rect.Height;

    // asm: TacticalMap->sub_6D5F60(rect) —— 战术视口随全屏视口重建。
    if (TacticalClass::Instance != nullptr) {
        TacticalClass::Instance->Set_View_Dimensions(rect);
    }

    // asm: WWMouse / GadgetDisplay / MessageListClass 更新本帧输入与文本区域。
    CursorPosition = Point2D(rect.X, rect.Y);
    MarkToRedraw();
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
    // asm: 把"原始动作"结合格子遮挡态与目标对象，换算成实际可执行动作：
    //   1) 目标格被遮罩时，除移动/警戒外一律退回 None（看不到就不给指令）。
    //   2) 无目标对象时，把攻击类动作降级为移动。
    (void)dwUnk;

    if (bShrouded)
    {
        if (action != Action::Move && action != Action::GuardArea)
            return false;
    }

    if (pObject == nullptr)
    {
        if (action == Action::Attack)
            return false;   // 无目标 → 攻击无效（调用方改发移动）
    }

    // 目标格越界视为无效。
    if (MapClass::Instance != nullptr)
    {
        if (cell.X < 0 || cell.Y < 0 ||
            cell.X >= MapClass::Instance->MapWidth ||
            cell.Y >= MapClass::Instance->MapHeight)
            return false;
    }

    return true;
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

// ============================================================================
// UI 层程序化绘制（无 .SHP/.PAL 资源时的引擎内置降级渲染器）
//
// 布局常量全部来自 asm：
//   侧边栏宽度 168        —— .data num_168（asm:2058714）
//   雷达外框 (0,48,168,110) —— RadarClass_OneTime 写 +11E4h/+11ECh，
//                              RadarClass_Draw 的 loc_65336D 写 +120Ch
//   雷达内图 (16,49,140,108) —— RadarClass_OneTime 写 +11E8h/+11F4h/
//                              +11F8h/+11FCh
//   建造格步进 50×63      —— dword_B0B500=32h / dword_B0B4FC=3Fh
// ============================================================================
namespace
{
    constexpr int32 kUiSidebarWidth  = 168;  // num_168
    constexpr int32 kUiPanelColor    = 0x4208;
    constexpr int32 kUiPanelDark     = 0x2104;
    constexpr int32 kUiPanelLight    = 0x8410;
    constexpr int32 kUiTextColor     = 0xFFFF;
    constexpr int32 kUiGoldColor     = 0xFE60;
    constexpr int32 kUiGreenColor    = 0x07E0;
    constexpr int32 kUiRedColor      = 0xF800;

    // 填充并把裁剪做在 [0,面] ∩ clip 内，返回实际写入的像素数。
    int32 UiFill(Surface* pSrf, const Rectangle& clip, const Rectangle& rect, DWORD color)
    {
        if (pSrf == nullptr || pSrf->Buffer == nullptr)
            return 0;
        Rectangle area = rect.Intersection(clip)
                             .Intersection(Rectangle(0, 0, pSrf->GetWidth(), pSrf->GetHeight()));
        if (area.Width <= 0 || area.Height <= 0)
            return 0;
        pSrf->FillRectEx(nullptr, &area, color);
        return area.Width * area.Height;
    }

    // 1px 立体边框：左上亮、右下暗。返回线像素数。
    int32 UiFrame(Surface* pSrf, const Rectangle& clip, const Rectangle& rect,
                  DWORD light, DWORD dark)
    {
        if (pSrf == nullptr || pSrf->Buffer == nullptr)
            return 0;
        if (rect.Width <= 0 || rect.Height <= 0)
            return 0;

        const int32 l = rect.X;
        const int32 t = rect.Y;
        const int32 r = rect.X + rect.Width  - 1;
        const int32 b = rect.Y + rect.Height - 1;

        Point2D a(l, t);
        Point2D bb(r, t);
        Point2D c(r, b);
        Point2D d(l, b);
        Rectangle clipCopy = clip;

        pSrf->DrawLineEx(&clipCopy, &a,  &bb, light);
        pSrf->DrawLineEx(&clipCopy, &a,  &d,  light);
        pSrf->DrawLineEx(&clipCopy, &bb, &c,  dark);
        pSrf->DrawLineEx(&clipCopy, &d,  &c,  dark);
        return (rect.Width * 2 + rect.Height * 2) - 4;
    }

    // 单行文字；返回 1 表示提交了一次文字绘制。
    int32 UiText(Surface* pSrf, const Rectangle& clip, const wchar_t* text,
                 int32 x, int32 y, DWORD color)
    {
        if (pSrf == nullptr || pSrf->Buffer == nullptr || text == nullptr || text[0] == 0)
            return 0;
        Point2D ret(0, 0);
        Point2D loc(x, y);
        Fancy_Text_Print_Wide(ret, text, pSrf, clip, loc, color, 0x0000,
                              TextPrintType::NoShadow);
        return 1;
    }

    // SidebarClass::Instance 尚未建立时的空建造格降级绘制。
    // 布局与 SidebarClass::Draw_UI 完全一致（2 列 × 3 行、48×60、50×63 步进）。
    int32 UiDrawEmptyCameoGrid(Surface* pSrf, const Rectangle& panel)
    {
        if (pSrf == nullptr || pSrf->Buffer == nullptr)
            return 0;

        int32 px = 0;
        Rectangle clip = panel;
        const int32 gridX = panel.X + SidebarClass::CameoGridX;
        const int32 gridY = panel.Y + SidebarClass::CameoGridY;

        for (int32 i = 0; i < SidebarClass::CameoColumns * SidebarClass::CameoRows; ++i)
        {
            const int32 col = i % SidebarClass::CameoColumns;
            const int32 row = i / SidebarClass::CameoColumns;
            Rectangle slot(gridX + col * SidebarClass::CameoPitchX,
                           gridY + row * SidebarClass::CameoPitchY,
                           SidebarClass::CameoSlotW,
                           SidebarClass::CameoSlotH);

            Rectangle area = slot.Intersection(clip)
                                 .Intersection(Rectangle(0, 0, pSrf->GetWidth(), pSrf->GetHeight()));
            if (area.Width <= 0 || area.Height <= 0)
                continue;

            pSrf->FillRectEx(nullptr, &area, 0x2104);
            px += area.Width * area.Height;

            Rectangle inner(slot.X + 2, slot.Y + 2, slot.Width - 4, slot.Height - 4);
            area = inner.Intersection(clip)
                        .Intersection(Rectangle(0, 0, pSrf->GetWidth(), pSrf->GetHeight()));
            if (area.Width <= 0 || area.Height <= 0)
                continue;
            pSrf->FillRectEx(nullptr, &area, 0x18E3);
            px += area.Width * area.Height;

            Point2D tl(slot.X, slot.Y);
            Point2D tr(slot.X + slot.Width - 1, slot.Y);
            Point2D bl(slot.X, slot.Y + slot.Height - 1);
            Point2D br(slot.X + slot.Width - 1, slot.Y + slot.Height - 1);
            pSrf->DrawLineEx(&clip, &tl, &tr, 0x1082);
            pSrf->DrawLineEx(&clip, &tl, &bl, 0x1082);
            pSrf->DrawLineEx(&clip, &bl, &br, 0x5AEB);
            pSrf->DrawLineEx(&clip, &tr, &br, 0x5AEB);
        }
        return px;
    }
}

void DisplayClass::Draw(bool forced) {
    // Render the tactical map. The DisplayClass owns the view position and
    // follow-cam logic; the actual isometric tile and object drawing is
    // delegated to the TacticalClass renderer which reads the view state
    // from this instance. forced != 0 时 UI 帧号用高亮色。

    // Follow-cam: keep the tracked object centred on screen while it moves.
    if (FollowObject && ObjectToFollow && !ObjectToFollow->IsInLimbo) {
        CoordStruct coord;
        ObjectToFollow->GetCoords(&coord);
        CenterOn(coord);
    }

    // Keep the visible rectangle in sync with the current view position so
    // culling tests against VisibleRect stay accurate after scrolls.
    //
    // 视口的权威来源是 TacticalClass 的相机（TacticalPos），不是本类的
    //   ViewPosition：世界→屏幕只有一条换算（CoordsToScreen - GetTacticalOrigin），
    //   地形层和对象层都按它画。相机一旦被 SetTacticalPosition / 跟随
    //   直接改写，ViewPosition 就陈旧了，雷达上的视口框会飘到地图角上。
    //   这里每帧从相机反推一次，ViewPosition 只作为没有战术层时的兜底。
    if (TacticalClass::Instance != nullptr)
    {
        const Point2D origin = TacticalClass::Instance->GetTacticalOrigin();
        ViewPosition.X = origin.X;
        ViewPosition.Y = origin.Y;
    }
    VisibleRect = Rectangle(ViewPosition.X, ViewPosition.Y, ScreenWidth, ScreenHeight);

    // 冒烟诊断：渲染帧计数 + 本帧视口矩形（OPENYR_DEBUG=1 时由 MainLoop
    // 随 [FRAME] 打成 [RENDER]）。
    if (g_bDebugMode)
    {
        ++g_RenderDebugStats.FrameCount;
        g_RenderDebugStats.ViewX = VisibleRect.X;
        g_RenderDebugStats.ViewY = VisibleRect.Y;
        g_RenderDebugStats.ViewW = VisibleRect.Width;
        g_RenderDebugStats.ViewH = VisibleRect.Height;
    }

    if (TacticalClass::Instance) {
        TacticalClass::Instance->Draw();
    }

    // ------------------------------------------------------------------
    // UI 层：侧边栏底板 / 顶部信息条 / 雷达框 / 建造格
    // ------------------------------------------------------------------
    Surface* pSrf = nullptr;
    if (TacticalClass::Instance != nullptr)
        pSrf = TacticalClass::Instance->Target_Surface();
    if ((pSrf == nullptr || pSrf->Buffer == nullptr) && TheGScreen != nullptr)
        pSrf = TheGScreen->Get_Back_Buffer();
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;

    const int32 srfW = pSrf->GetWidth();
    const int32 srfH = pSrf->GetHeight();
    if (srfW <= 0 || srfH <= 0)
        return;

    int32 panelW = kUiSidebarWidth;
    if (panelW > srfW) panelW = srfW;

    // asm num_168：右侧 168px 归侧边栏，左侧战场不受影响。
    const Rectangle panel(srfW - panelW, 0, panelW, srfH);
    const Rectangle surfaceRect(0, 0, srfW, srfH);

    int32 sidebarPx = 0;
    int32 textCount = 0;
    int32 pxTotal  = 0;

    // ---- 1) 侧边栏底板 + 1px 立体边框 ----
    sidebarPx += UiFill(pSrf, surfaceRect, panel, kUiPanelColor);
    sidebarPx += UiFrame(pSrf, surfaceRect, panel, kUiPanelLight, kUiPanelDark);

    // ---- 2) 顶部信息条 ----
    const Rectangle topBar(panel.X + 1, 0, panel.Width - 1, 46);
    pxTotal += UiFill(pSrf, panel, topBar, kUiPanelDark);

    HouseClass* player = HouseClass::Player;
    if (player == nullptr && HouseClass::Array != nullptr &&
        HouseClass::ArrayCount > 0)
        player = HouseClass::Array[0];
    const int32 credits = (player != nullptr) ? player->Credits : 0;
    const int32 powerOut = (player != nullptr) ? player->PowerOutput : 0;
    const int32 powerDrain = (player != nullptr) ? player->PowerDrain : 0;

    wchar_t line[48];
    std::swprintf(line, 48, L"$%d", credits);
    textCount += UiText(pSrf, panel, line, panel.X + 8, 5, kUiGoldColor);

    std::swprintf(line, 48, L"PWR %d/%d", powerOut, powerDrain);
    const DWORD powerColor = (powerDrain > powerOut) ? kUiRedColor : kUiGreenColor;
    textCount += UiText(pSrf, panel, line, panel.X + 8, 17, powerColor);

    if (g_bDebugMode)
    {
        std::swprintf(line, 48, L"FRM %u", g_RenderDebugStats.FrameCount);
        textCount += UiText(pSrf, panel, line, panel.X + 8, 29,
                            forced ? kUiGoldColor : kUiTextColor);
    }

    // ---- 3) 雷达框（asm: 侧边栏局部 (0,48,168,110)）----
    const Rectangle radarFrame(panel.X + RadarClass::RadarLocalX,
                               RadarClass::RadarLocalY,
                               RadarClass::RadarFrameW,
                               RadarClass::RadarFrameH);
    pxTotal += UiFill(pSrf, surfaceRect, radarFrame, kUiPanelDark);

    const Rectangle radarInner(panel.X + RadarClass::RadarMapX,
                               RadarClass::RadarMapY,
                               RadarClass::RadarMapW,
                               RadarClass::RadarMapH);
    pxTotal += UiFill(pSrf, surfaceRect, radarInner, 0x0000);
    pxTotal += UiFrame(pSrf, surfaceRect, radarFrame, kUiPanelLight, 0x0000);

    int32 radarObjects = 0;
    RadarClass* radar = RadarClass::Instance;
    if (radar == nullptr)
        radar = TheRadar;
    if (radar != nullptr)
    {
        radar->Draw_Mini_Map(pSrf, radarInner);
        radarObjects = radar->Last_Mini_Map_Objects();
    }

    // ---- 4) 建造图标格（asm: 2 列 × N 行，步进 50×63）----
    if (SidebarClass::Instance != nullptr)
        pxTotal += SidebarClass::Instance->Draw_UI(pSrf, panel);
    else
        pxTotal += UiDrawEmptyCameoGrid(pSrf, panel);

    if (g_bDebugMode)
    {
        std::fprintf(stderr, "[UI] sidebar=%d radarobj=%d text=%d px=%d\n",
                     sidebarPx, radarObjects, textCount, pxTotal);
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
    // asm: DisplayClass::TacticalClass::Action (asm:1927) —— 由当前模式与点击
    //   目标决定本次左键的动作码。模式优先于目标：
    (void)cell;
    (void)dwUnk;

    if (SellMode)        return Action::Sell;
    if (RepairMode)      return Action::Repair;
    if (PowerToggleMode) return Action::TogglePower;
    if (PlanningMode)    return Action::PlaceWaypoint;
    if (PlaceBeaconMode) return Action::PlaceBeacon;
    if (CurrentSWTypeIndex != -1) return Action::None;   // 超武目标由目标模式决定
    if (CurrentBuildingType != nullptr) return Action::Select;

    if (pObject == nullptr)
        return Action::Move;

    // 有对象：敌方 → 攻击；己方 → 选择。
    if (pObject->Owner != nullptr && HouseClass::Player != nullptr &&
        pObject->Owner != HouseClass::Player)
        return Action::Attack;

    return Action::Select;
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

// File-local render layers. asm 的 vec_ObjectsInLayers 是三张平行层表
// （索引 0..2，对应 WhatAmI 的三个绘制层）；每层各自维护排序与成员数。
// DisplayClass 头未暴露这三张表，故在此以翻译单元局部容器镜像。
static constexpr int32 kDisplayLayerCount = 3;
static DynamicVectorClass<ObjectClass*> g_DisplayLayers[kDisplayLayerCount];

// 返回对象所在的层索引（未登记返回 -1），对应 asm 的 [obj+94h]。
static int32 FindDisplayLayer(ObjectClass* pObject) {
    for (int32 layer = 0; layer < kDisplayLayerCount; ++layer) {
        for (int32 i = 0; i < g_DisplayLayers[layer].Count; ++i) {
            if (g_DisplayLayers[layer][i] == pObject)
                return layer;
        }
    }
    return -1;
}

// =============================================================================
// Submit - Register an object into its tactical layer (asm: asm:781)
//
// asm: DisplayClass::Submit(this, ObjectClass* anim) retn 4。
//   1) 若对象已在某层（[anim+94h] != -1）→ 先 Remove。
//   2) layer = anim->vtable[+78h]()（asm 的层号查询，-1 表示不参与绘制）。
//   3) AddObjectToALayer(vec_ObjectsInLayers[layer], anim, sortable = (layer==2))。
//   4) 成功则 [anim+94h] = layer。
// =============================================================================
void DisplayClass::Submit(ObjectClass* pObject) {
    if (pObject == nullptr)
        return;

    // 1) asm: cmp [anim+94h], -1 ; jz skip ; Remove(anim)
    if (FindDisplayLayer(pObject) != -1)
        Remove(pObject);

    // 2) asm: call [vtable+78h] —— 对象自报绘制层号。OpenYR 未暴露该虚槽；
    // 以"已选中"近似判定选中层(2)，否则归入普通层(0)。
    if (pObject->IsInLimbo)
        return;
    const int32 target = pObject->IsSelected ? 2 : 0;

    if (target < 0 || target >= kDisplayLayerCount)
        return;

    // asm: AddObjectToALayer —— 去重后追加。
    for (int32 i = 0; i < g_DisplayLayers[target].Count; ++i) {
        if (g_DisplayLayers[target][i] == pObject)
            return;
    }
    g_DisplayLayers[target].Add(pObject);

    // asm: vec_VisibleObjects 是“本帧显示层看得见的对象”表，
    //   VisibleObjects_DeleteDead（proc@1644735）每帧末扫它清死对象。
    //   对象一旦进入某个显示层，就同时登记进这张表（原版由 Tactical 侧的
    //   Add 完成；此处与层登记绑定，保证两者同步）。
    if (ObjectClass::VisibleObjects != nullptr) {
        bool listed = false;
        for (int32 i = 0; i < ObjectClass::VisibleObjects->Count; ++i) {
            if (ObjectClass::VisibleObjects->Items[i] == pObject) {
                listed = true;
                break;
            }
        }
        if (!listed)
            ObjectClass::VisibleObjects->Add(pObject);
    }
}

// =============================================================================
// Remove - Unregister an object from its tactical layer (asm: asm:839)
//
// asm: DisplayClass::Remove(this, ObjectClass* a1) retn 4。
//   1) layer = [a1+94h]；为 -1 直接返回。
//   2) 从 vec_ObjectsInLayers[layer] 里摘除（保持后续元素前移）。
//   3) 扫描三张层表确认已摘净，最后 [a1+94h] = -1。
// =============================================================================
void DisplayClass::Remove(ObjectClass* pObject) {
    if (pObject == nullptr)
        return;

    const int32 layer = FindDisplayLayer(pObject);
    if (layer == -1)
        return;                                // asm: cmp [edi+94h], -1 ; jz ret

    for (int32 i = 0; i < g_DisplayLayers[layer].Count; ++i) {
        if (g_DisplayLayers[layer][i] == pObject) {
            g_DisplayLayers[layer].Remove(i);  // asm: 后移覆盖
            break;
        }
    }

    // asm: 再扫全部层表（防重复登记残留）。
    for (int32 l = 0; l < kDisplayLayerCount; ++l) {
        for (int32 i = g_DisplayLayers[l].Count - 1; i >= 0; --i) {
            if (g_DisplayLayers[l][i] == pObject)
                g_DisplayLayers[l].Remove(i);
        }
    }

    // asm: 离开显示层的对象同时从 vec_VisibleObjects 摘除，
    //   避免 VisibleObjects_DeleteDead 扫到已不在画面上的记录。
    if (ObjectClass::VisibleObjects != nullptr) {
        for (int32 i = ObjectClass::VisibleObjects->Count - 1; i >= 0; --i) {
            if (ObjectClass::VisibleObjects->Items[i] == pObject)
                ObjectClass::VisibleObjects->Remove(i);
        }
    }

    // asm: 清除高亮标记（Marked）。
    pObject->Marked = false;
}

// ============================================================================
// Click processing
// ============================================================================

// =============================================================================
// ProcessClickCoords - Resolve a screen click to cell/coords/target (asm: click)
//
// asm: 点击处理先取屏幕坐标，经等距逆变换得世界坐标与落点格；再取该格的
//   Occupier 作为点击目标，并把"是否被遮罩""是否有对象"写回 a5/a6。
//   返回 true 表示坐标解析成功（越界或未初始化则为 false）。
// =============================================================================
bool DisplayClass::ProcessClickCoords(Point2D* src, CellStruct* XYdst, CoordStruct* XYZdst,
                                      ObjectClass** Target, BYTE* a5, BYTE* a6)
{
    if (src == nullptr || XYdst == nullptr || XYZdst == nullptr)
        return false;

    // 屏幕 → 世界（等距逆变换）。
    ScreenToTactical(*src, XYZdst);

    // 世界 → 落点格。
    *XYdst = CellClass::Coord2Cell(*XYZdst);

    if (MapClass::Instance == nullptr)
        return false;

    // 越界即失败。
    if (XYdst->X < 0 || XYdst->Y < 0 ||
        XYdst->X >= MapClass::Instance->MapWidth ||
        XYdst->Y >= MapClass::Instance->MapHeight)
        return false;

    CellClass* pCell = MapClass::Instance->GetCellAt(*XYdst);
    bool shrouded  = (pCell == nullptr) || pCell->IsShrouded();

    ObjectClass* pTarget = nullptr;
    if (pCell != nullptr && !shrouded)
        pTarget = pCell->Get_Occupier();

    if (Target != nullptr)
        *Target = pTarget;
    if (a5 != nullptr)                       // asm: 是否被遮罩
        *a5 = shrouded ? 1 : 0;
    if (a6 != nullptr)                       // asm: 是否有可作用对象
        *a6 = (pTarget != nullptr) ? 1 : 0;

    return true;
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
// ============================================================================
// 显示层原版命名形态
// ============================================================================

MouseCursorType DisplayClass::Cursor_Guard() const
{
    // asm: DisplayClass_Cursor_AttackMove
    return MouseCursorType::GuardArea;
}

MouseCursorType DisplayClass::Cursor_AttackMove() const
{
    // asm: DisplayClass_CanAllSelectedAttack
    // 指令层区分。
    return MouseCursorType::Attack;
}

bool DisplayClass::CanAllSelectedAttack() const
{
    // asm: DisplayClass_ConvertAttack
    // 即整体按不可攻处理；空集默认放行。
    if (TacticalClass::SelectedObjects.Count == 0)
        return true;
    for (int32 i = 0; i < TacticalClass::SelectedObjects.Count; ++i) {
        ObjectClass* pObj = TacticalClass::SelectedObjects.Items[i];
        if (!pObj)
            return false;
    }
    return true;
}

Action DisplayClass::ConvertAttack(Action action)
{
    // asm: char __thiscall DisplayClass_RightMouseDown(MapClass *this, tagPOINT *a2)   (asm:1352047)
    // 发，否则退回移动动作。
    if (!CanAllSelectedAttack())
        return Action::Move;
    return action;
}

// =============================================================================
// RightMouseDown - Right-click handler entry (asm: asm:2073)
//
// asm: DisplayClass_Mouse_Right_Press proc near ; retn 4 —— 该裸名槽在
//   跳转表里直接 retn，真正的右键状态机在 DisplayClass::Mouse_Right_Press。
// =============================================================================
void DisplayClass::RightMouseDown(const Point2D& point)
{
    Mouse_Right_Press(point);
}

// =============================================================================
// Mouse_Right_Press - Right-click mode-cancel state machine (asm: asm:2078)
//
// asm: 按键优先级链，命中第一个"活动模式"就取消它并返回：
//   1) [11A4h]（CurrentBuilding）若有效且 [eax+14h]&1 → 清 CurrentBuilding/
//      CurrentBuildingType/unknown_11AC=-1，Set_Cursor_Shape(0)，返回。
//   2) [11B0h] RepairMode → Repair_Mode_Control(0)，返回。
//   3) [11B1h] SellMode → Sell_Mode_Control(0)，返回。
//   4) [11B2h] PowerToggleMode → Power_Mode_Control(0)，返回。
//   5) [11B8h] CurrentSWTypeIndex != -1 → 置 -1，返回。
//   6) [11B3h] PlanningMode → Waypoint_Mode_Control(0,0)，返回。
//   7) [11B4h] PlaceBeaconMode → Beacon_Mode_Control(0)，返回。
//   8) 都不活动 → Unselect_All（取消全选）。
// =============================================================================
void DisplayClass::Mouse_Right_Press(const Point2D& /*point*/)
{
    // 1) asm: [esi+11A4h] = CurrentBuilding（+对象标志位判断）
    if (CurrentBuilding != nullptr || CurrentBuildingType != nullptr)
    {
        CurrentBuilding      = nullptr;
        CurrentBuildingType  = nullptr;
        unknown_11AC         = 0xFFFFFFFF;   // asm: mov [esi+11ACh], -1
        CurrentFoundation_Data = nullptr;
        MarkToRedraw();
        return;
    }

    // 2) asm: [esi+11B0h] = RepairMode
    if (RepairMode)     { RepairMode = false;      MarkToRedraw(); return; }
    // 3) asm: [esi+11B1h] = SellMode
    if (SellMode)       { SellMode = false;        MarkToRedraw(); return; }
    // 4) asm: [esi+11B2h] = PowerToggleMode
    if (PowerToggleMode){ PowerToggleMode = false; MarkToRedraw(); return; }
    // 5) asm: [esi+11B8h] = CurrentSWTypeIndex
    if (CurrentSWTypeIndex != -1) { CurrentSWTypeIndex = -1; MarkToRedraw(); return; }
    // 6) asm: [esi+11B3h] = PlanningMode（航点模式）
    if (PlanningMode)   { PlanningMode = false;    MarkToRedraw(); return; }
    // 7) asm: [esi+11B4h] = PlaceBeaconMode
    if (PlaceBeaconMode){ PlaceBeaconMode = false; MarkToRedraw(); return; }

    // 8) asm: call Unselect_All
    TacticalClass::DeselectAll();
    MarkToRedraw();
}

// =============================================================================
// Mouse_Left_Press - Arm drag-select (asm: asm:3063)
//
// asm: DisplayClass::Mouse_Left_Press(this, tagPOINT* p) retn 4。
//   若 RepairMode/SellMode/PowerToggleMode 均关、CurrentSWTypeIndex==-1、
//   CurrentBuildingType==0，则：unknown_bool_11D0(DragArmed)=1，
//   拖拽起点 = 当前点（DragStartX/Y = p），拖拽当前点 = 当前点。
//   任一模式活动则本函数不动（该点击由模式处理器消费）。
// =============================================================================
void DisplayClass::Mouse_Left_Press(const Point2D& point)
{
    if (RepairMode || SellMode || PowerToggleMode)
        return;                                   // asm: 11B0/11B1/11B2 分支
    if (CurrentSWTypeIndex != -1)
        return;                                   // asm: 11B8 分支
    if (CurrentBuildingType != nullptr)
        return;                                   // asm: 11A8 分支

    unknown_bool_11D0 = true;                     // asm: [ecx+11D0h] = 1
    unknown_11D4 = point.X;                        // asm: [ecx+11D4h]
    unknown_11D8 = point.Y;                        // asm: [ecx+11D8h]
    unknown_11DC = point.X;                        // asm: [ecx+11DCh]
    unknown_11E0 = point.Y;                        // asm: [ecx+11E0h]
}

// =============================================================================
// Mouse_Left_Held - Drag-select tracking (asm: asm:3119)
//
// asm: DisplayClass::Mouse_Left_Held(this, tagPOINT* xy) retn 4。
//   A) 若 DraggingRectangle(11CF) 已置且非规划/信标模式：
//        把 xy 夹取到视口 [0..W-1]x[0..H-1] 写入 DragCur(11DC/11E0)；
//        若与旧 DragCur 不同 → TacticalMap->field_D7D=1；
//        TacticalMap->sub_6D9FC0(&DragCur)（更新框选带）。
//   B) 否则若 DragArmed(11D0)：算起点(11D4)到 xy 的距离；
//        距离 > 4 像素 → DraggingRectangle=1 ; DragArmed=0 ;
//        非规划/信标模式下：TacticalMap->field_D7D=1；
//        TacticalMap->sub_6D9F80(&DragStart)；MouseClass_SetPointer(0,0)。
// =============================================================================
void DisplayClass::Mouse_Left_Held(const Point2D& point)
{
    if (DraggingRectangle && !PlanningMode && !PlaceBeaconMode)
    {
        // A) 夹取到视口内。
        int32 px = point.X, py = point.Y;
        if (px < 0) px = 0;
        if (px >= ScreenWidth) px = ScreenWidth - 1;
        if (py < 0) py = 0;
        if (py >= ScreenHeight) py = ScreenHeight - 1;

        if (px == unknown_11DC && py == unknown_11E0)
            return;                               // asm: jz loc_4AC4DF

        unknown_11DC = px;
        unknown_11E0 = py;

        if (TacticalClass::Instance != nullptr)
        {
            TacticalClass::Instance->Redrawing = true;
            TacticalClass::Instance->DragX1 = unknown_11D4;
            TacticalClass::Instance->DragY1 = unknown_11D8;
            TacticalClass::Instance->DragX2 = px;
            TacticalClass::Instance->DragY2 = py;
            TacticalClass::Instance->Drag_Select();
        }
        return;
    }

    if (!unknown_bool_11D0)
        return;                                   // asm: jz loc_4AC4DF

    // B) 算起点到当前点的欧氏距离（asm: float_sqrt + Float_To_Int_Floor）。
    const int32 dx = point.X - unknown_11D4;
    const int32 dy = point.Y - unknown_11D8;
    const int32 dist = static_cast<int32>(std::sqrt(static_cast<double>(dx * dx + dy * dy)));
    if (dist <= 4)
        return;                                   // asm: cmp eax,4 ; jle ...

    DraggingRectangle = true;                     // asm: [esi+11CFh] = 1
    unknown_bool_11D0 = false;                    // asm: [esi+11D0h] = 0

    if (PlanningMode || PlaceBeaconMode)
        return;

    if (TacticalClass::Instance != nullptr)
    {
        TacticalClass::Instance->Redrawing = true;
        TacticalClass::Instance->DragX1 = unknown_11D4;
        TacticalClass::Instance->DragY1 = unknown_11D8;
        TacticalClass::Instance->DragX2 = unknown_11D4;
        TacticalClass::Instance->DragY2 = unknown_11D8;
        TacticalClass::Instance->Drag_Select();
    }
}

// =============================================================================
// Mouse_Left_Up - Commit a left-button gesture (asm: asm:2273)
//
// asm: DisplayClass::Mouse_Left_Release(this, int x_94, eActions Action,
//      ObjectClass* clickTarget, int A0) retn 14h —— 真正的左键释放处理：
//   1) 若在框选中 → 由 band 反算选中集（Drag_Select_Dimensions 收尾）。
//   2) 若在放置模式 → Place_Object(落点格)。
//   3) 否则 → 点击选择（Select_At / 取消选择）。
//   收尾：清 DraggingRectangle/DragArmed。
// =============================================================================
void DisplayClass::Mouse_Left_Up(const Point2D& point)
{
    if (DraggingRectangle)
    {
        // 1) 框选提交：把带区交给战术层选择（asm: 反算选中集）。
        if (TacticalClass::Instance != nullptr)
        {
            TacticalClass::Instance->DragX2 = point.X;
            TacticalClass::Instance->DragY2 = point.Y;
            TacticalClass::Instance->Drag_Select();
            TacticalClass::Instance->Drag_Select_Dimensions();
        }
        DraggingRectangle = false;
        unknown_bool_11D0 = false;
        MarkToRedraw();
        return;
    }

    // 换算点击格（asm: ProcessClickCoords 的落点解析）。
    CoordStruct coords;
    ScreenToTactical(point, &coords);
    CellStruct cell = CellClass::Coord2Cell(coords);

    // 2) 放置模式：把预览落位。
    if (CurrentBuildingType != nullptr)
    {
        if (Place_Object(cell))
            Cancel_Placement();
        unknown_bool_11D0 = false;
        return;
    }

    // 3) 普通点选：命中则加入选择集，未命中则清空选择。
    if (TacticalClass::Instance != nullptr)
    {
        bool hit = TacticalClass::Instance->Select_At(point, false);
        if (!hit)
            TacticalClass::DeselectAll();
    }

    unknown_bool_11D0 = false;
    MarkToRedraw();
}

// =============================================================================
// Mouse_Move - Cursor tracking / hover (asm: asm:768)
//
// asm: DisplayClass_MouseMove(this, int* elementID, dwXY* mouseCoords) retn
//   0Ch —— 记录最新光标位置并刷新 UI 悬停态。
// =============================================================================
void DisplayClass::Mouse_Move(const Point2D& point)
{
    CursorPosition = point;

    // asm: 悬停格 → 帮助文本/光标形状由 Gadget 层随最新坐标刷新；
    // 若处于放置模式则保持屏幕 live，供脚印预览跟随光标。
    if (CurrentBuildingType != nullptr)
        MarkToRedraw();
}

// =============================================================================
// Place_Object - Commit a building placement (asm: Mouse_Left_Release 放置分支)
//
// asm: 放置时遍历当前购买对象的脚印（FoundationBoundsSize / SetActiveFoundation
//   维护的 CurrentFoundation_Data），逐格 Map->Is_Placement_Allowed 校验；
//   全部通过则实际建造并消耗资金、退出放置模式；否则保持放置模式。
// =============================================================================
bool DisplayClass::Place_Object(const CellStruct& cell)
{
    if (CurrentBuildingType == nullptr)
        return false;

    // 落点中心格。
    CurrentFoundation_CenterCell = cell;

    bool allValid = true;

    if (CurrentFoundation_Data != nullptr)
    {
        const CellStruct* pIter = CurrentFoundation_Data;
        while (pIter->X != 0x7FFF || pIter->Y != 0x7FFF)
        {
            CellStruct target;
            target.X = static_cast<int16>(cell.X + pIter->X);
            target.Y = static_cast<int16>(cell.Y + pIter->Y);
            ++pIter;

            if (!MapClass::Instance->Is_Placement_Allowed(target))
            {
                allValid = false;
                break;
            }
        }
    }
    else
    {
        allValid = MapClass::Instance->Is_Placement_Allowed(cell);
    }

    if (!allValid)
        return false;

    // asm: 建造成功后清除购买对象与脚印数据（对象创建由 BuildingClass 工厂
    // 承担；此处按语义等价停下在"校验通过"这一步，交由上层触发工厂）。
    MarkToRedraw();
    return true;
}

// =============================================================================
// Cancel_Placement - Abort building placement (asm: 右键取消分支)
//
// asm: 清 CurrentBuilding/CurrentBuildingType/未知索引 -1，并释放脚印数据。
// =============================================================================
void DisplayClass::Cancel_Placement()
{
    CurrentBuilding     = nullptr;
    CurrentBuildingType = nullptr;
    unknown_11AC        = 0xFFFFFFFF;
    CurrentFoundation_Data     = nullptr;
    CurrentFoundationCopy_Data = nullptr;
    unknown_bool_11CE  = false;
    MarkToRedraw();
}

// =============================================================================
// Process_Input - Per-frame input dispatch (asm: Main_Loop 输入段)
//
// asm: 主循环每帧把鼠标/拖拽状态喂进显示层：若正在拖拽则据最新坐标更新
//   框选带；滚动的惯性计时递减；把最新带区交给战术层重绘。
// =============================================================================
void DisplayClass::Process_Input()
{
    if (DraggingRectangle && TacticalClass::Instance != nullptr)
    {
        TacticalClass::Instance->DragX1 = unknown_11D4;
        TacticalClass::Instance->DragY1 = unknown_11D8;
        TacticalClass::Instance->DragX2 = unknown_11DC;
        TacticalClass::Instance->DragY2 = unknown_11E0;
        TacticalClass::Instance->Drag_Select();
    }

    Update();
}

// =============================================================================
// AI - Per-frame display update (asm: DisplayClass::AI)
//
// asm: 逐帧：跟随相机把视图中心挪到被跟随对象；滚动惯性计时递减；
//   若本帧有变化则标记重绘。
// =============================================================================
void DisplayClass::AI()
{
    // 跟随相机（asm: FollowObject 分支：把 ObjectToFollow 居中）。
    if (FollowObject && ObjectToFollow != nullptr && !ObjectToFollow->IsInLimbo)
    {
        CoordStruct coord;
        ObjectToFollow->GetCoords(&coord);
        CenterOn(coord);
        MarkToRedraw();
    }

    // 滚动惯性（asm: ScrollTimer 递减）。
    Update();

    // 正在滚动时保持屏幕live。
    if (IsScrolling)
        MarkToRedraw();
}

void DisplayClass::Refresh()
{
    // asm: DisplayClass::Refresh —— 把脏区并集重绘并清空脏区。
    if (TacticalClass::Instance != nullptr)
    {
        TacticalClass::Instance->ContainingMapCoords = Rectangle(0, 0, 0, 0);
    }
    MarkToRedraw();
}

void DisplayClass::Restore()
{
    // asm: DisplayClass::Restore —— 把备份面贴回主面（合成后端未建模，
    // 语义等价为标记整屏重绘）。
    MarkToRedraw();
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知裸名按键入口直接走对应的 Mouse_ 前缀处理面。
// ------------------------------------------------------------------------
void DisplayClass::Left_Press()
{
    Mouse_Left_Press(CursorPosition);
}

void DisplayClass::Left_Release()
{
    Mouse_Left_Up(CursorPosition);
}

void DisplayClass::Left_Held()
{
    Mouse_Left_Held(CursorPosition);
}

void DisplayClass::Left_Up()
{
    Mouse_Left_Up(CursorPosition);
}

void DisplayClass::Right_Press()
{
    Mouse_Right_Press(CursorPosition);
}

// ============================================================================
// 视频模式切换：重建五张全局表面
//
// asm: void_RecreateSurfaces (0x533F70)
//   __usercall(ecx -> rect1/ebp, edx -> rect2/edi; arg_4, arg_8); retn 0Ch
//   恒返回 al = 1。唯一调用点 SetVideoMode+41Eh。
//
// 逐段对应：
//   533F70  WWDebugString("Allocating new surfaces\n")
//   534005  Alternate   : 非空 -> "Deleting AlternateSurface\n"  -> vt[0](1) -> 清槽
//   534013  Hidden      : 同上（"Deleting HiddenSurface\n"）
//   53403E  Composite   : 同上（"Deleting CompositeSurface\n"）
//   534069  Tile        : 同上（"Deleting TileSurface\n"）
//   534094  Sidebar     : 同上（"Deleting SidebarSurface\n"）
//   5340BF  bl = (bVideoBackBuffer == 0) ; var_1 = (unk_A8EB96 == 0)
//   5340C7  Hidden 第一趟（仅 arg_8 低字节 != 0）
//   534146  Composite  (a4 = bl,  a5 = 1)
//   5341B4  Tile       (a4 = bl,  a5 = 1)
//   53422B  Composite/Tile 的 [surf+19h] 不一致 -> 双双删掉重建成 (1,1)
//   5342D6  Sidebar    (a4 = var_1, a5 = 0)
//   534352  Hidden 第二趟（仅 arg_8 低字节 == 0）
//   5343CB  Alternate  (a4 = 1, a5 = 0)
//
// vt[0] 是 DSurface_SDDTOR（标量删除析构，参数 1）—— 本工程即 delete。
// vt+18h(0) 是 vt_DSurface 第 6 槽 sub_7BBAB0：取整面矩形后交给
// sub_4BB620 填充，即"新建面先整面清 0"—— 本工程即 DSurface::Fill(0)。
// ============================================================================

// asm: unk_A8EB96
uint8 g_UnkA8EB96 = 0;

namespace {

// 与原版 WWDebugString 同一出口（Rendering 侧走 stderr）。
void RS_Debug(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

// asm: mov eax,DSurface_X / cmp eax,esi / jz skip —— 打印 + 删除 + 清槽。
void RS_DeleteSurface(DSurface*& pSlot, const char* pDebug)
{
    if (pSlot == nullptr)
        return;                                  // asm: cmp X, esi / jz

    RS_Debug(pDebug);
    delete pSlot;                                // asm: push 1 / call [eax]（vt[0]）
    pSlot = nullptr;                             // asm: mov X, esi
}

// asm: push 24h / call new + DSurfaceClass(w, h, a4, a5)；分配失败给 nullptr。
DSurface* RS_MakeSurface(int32 w, int32 h, bool bSystemMemory, bool bEnable3D)
{
    return new (std::nothrow) DSurface(w, h, bSystemMemory, bEnable3D);
}

// asm 每段重建的尾巴：vt+18h(0) 清面，再按 [surf+19h] 选 "VRAM" /
// "SYSTEM MEMORY" 打印。asm 在此处无条件解引用新面；空指针只可能来自
// 分配失败，仍然保留判空以免踩空。
void RS_FinishSurface(DSurface* pSurface, int32 w, int32 h, const char* pFormat)
{
    if (pSurface == nullptr)
        return;

    pSurface->Fill(0);                           // asm: vt+18h(0)
    RS_Debug(pFormat, w, h,
             pSurface->IsInVideoRam ? "VRAM" : "SYSTEM MEMORY");
}

} // namespace

int32 DisplayClass::RecreateSurfaces(const LTRBStruct& visRect,
                                     const LTRBStruct& altRect,
                                     const LTRBStruct& tileRect,
                                     const LTRBStruct& sidebarRect)
{
    RS_Debug("Allocating new surfaces\n");                 // 533F70

    RS_DeleteSurface(DSurface_Alternate, "Deleting AlternateSurface\n");
    RS_DeleteSurface(DSurface_Hidden,    "Deleting HiddenSurface\n");
    RS_DeleteSurface(DSurface_Composite, "Deleting CompositeSurface\n");
    RS_DeleteSurface(DSurface_Tile,      "Deleting TileSurface\n");
    RS_DeleteSurface(DSurface_Sidebar,   "Deleting SidebarSurface\n");

    // asm 5340BF-5340D2:
    //   bl    = (bVideoBackBuffer == 0)   setz bl
    //   var_1 = (unk_A8EB96 == 0)         setz [esp+var_1]
    const bool bSystemMemory = (Game::bVideoBackBuffer == 0);
    const bool bSidebarSysMem = (g_UnkA8EB96 == 0);

    // asm: mov al, byte ptr [esp+arg_8] —— arg_8 是指针，读的是指针值低字节；
    // 调用点传栈上矩形地址，故这一位由地址对齐决定，两支互斥。
    const bool bHiddenFirstPass =
        (reinterpret_cast<uintptr_t>(&sidebarRect) & 0xFFu) != 0;

    // --- Hidden 第一趟（asm 5340C7-53413F）----------------------------------
    if (bHiddenFirstPass) {
        const int32 w = visRect.Right;                     // asm: [ebp+8]
        const int32 h = visRect.Bottom;                    // asm: [ebp+0Ch]
        if (w > 0 && h > 0) {
            DSurface_Hidden = RS_MakeSurface(w, h, false, false);
            RS_FinishSurface(DSurface_Hidden, w, h,
                             "HiddenSurface (%dx%d) %s\n");
        }
    }

    // --- Composite（asm 534146-5341AE）：a4 = bl，a5 = 1 --------------------
    {
        const int32 w = altRect.Right;                     // asm: [edi+8]
        const int32 h = altRect.Bottom;                    // asm: [edi+0Ch]
        if (w > 0 && h > 0) {
            DSurface_Composite = RS_MakeSurface(w, h, bSystemMemory, true);
            RS_FinishSurface(DSurface_Composite, w, h,
                             "CompositeSurface (%dx%d) %s ENABLE_3D\n");
        }
    }

    // --- Tile（asm 5341B4-534222）：矩形取自 a3，a4/a5 与 Composite 相同 ----
    {
        const int32 w = tileRect.Right;                    // asm: [esi+8]
        const int32 h = tileRect.Bottom;                   // asm: [esi+0Ch]
        if (w > 0 && h > 0) {
            DSurface_Tile = RS_MakeSurface(w, h, bSystemMemory, true);
            RS_FinishSurface(DSurface_Tile, w, h,
                             "TileSurface (%dx%d) %s ENABLE_3D\n");
        }
    }

    // --- VRAM 不一致（asm 53422B-5342D6）：两张面都存在且 [surf+19h] 不同时
    //     双双删掉，改按 (a4=1, a5=1) 强制系统内存重建；这条路径不再调
    //     vt+18h，也不打印。------------------------------------------------
    if (DSurface_Composite != nullptr && DSurface_Tile != nullptr &&
        (DSurface_Tile->IsInVideoRam != DSurface_Composite->IsInVideoRam)) {
        delete DSurface_Composite;                         // asm: vt[0](1)
        DSurface_Composite = nullptr;
        delete DSurface_Tile;
        DSurface_Tile = nullptr;

        RS_Debug("Moving TileSurface and CompositeSurface into system memory\n");

        DSurface_Composite = RS_MakeSurface(altRect.Right, altRect.Bottom,
                                            true, true);
        DSurface_Tile = RS_MakeSurface(tileRect.Right, tileRect.Bottom,
                                       true, true);
    }

    // --- Sidebar（asm 5342D6-534346）：矩形取自 a4 本身，a4 = var_1，a5 = 0 --
    {
        const int32 w = sidebarRect.Right;                 // asm: [esi+8]
        const int32 h = sidebarRect.Bottom;                // asm: [esi+0Ch]
        if (w > 0 && h > 0) {
            DSurface_Sidebar = RS_MakeSurface(w, h, bSidebarSysMem, false);
            RS_FinishSurface(DSurface_Sidebar, w, h,
                             "SidebarSurface (%dx%d) %s\n");
        }
    }

    // --- Hidden 第二趟（asm 534352-5343C5）：仅 arg_8 低字节 == 0 ----------
    if (!bHiddenFirstPass) {
        const int32 w = visRect.Right;
        const int32 h = visRect.Bottom;
        if (w > 0 && h > 0) {
            DSurface_Hidden = RS_MakeSurface(w, h, false, false);
            RS_FinishSurface(DSurface_Hidden, w, h,
                             "HiddenSurface (%dx%d) %s\n");
        }
    }

    // --- Alternate（asm 5343CB-534438）：a4 = 1，a5 = 0 --------------------
    {
        const int32 w = visRect.Right;
        const int32 h = visRect.Bottom;
        if (w > 0 && h > 0) {
            DSurface_Alternate = RS_MakeSurface(w, h, true, false);
            RS_FinishSurface(DSurface_Alternate, w, h,
                             "AlternateSurface (%dx%d) %s\n");
        }
    }

    return 1;                                              // asm: mov al,1
}

// ============================================================================
// asm: vec_ObjectsInLayers 的只读访问器
//
// 三张层表在 asm 里是 .data 里的三组 {data, length} 对（stride 18h，
// 首项起点 0Ch 字节在 length 之前），本文件以 g_DisplayLayers 镜像。
// 同步日志的 MAP LAYER 段按层号顺序逐条转储，故只需这两条查询。
// ============================================================================
int32 DisplayClass::GetLayerCount()
{
    return kDisplayLayerCount;
}

int32 DisplayClass::GetLayerObjectCount(int32 layer)
{
    if (layer < 0 || layer >= kDisplayLayerCount) {
        return 0;
    }
    return g_DisplayLayers[layer].Count;
}

ObjectClass* DisplayClass::GetLayerObject(int32 layer, int32 index)
{
    if (layer < 0 || layer >= kDisplayLayerCount) {
        return nullptr;
    }
    if (index < 0 || index >= g_DisplayLayers[layer].Count) {
        return nullptr;
    }
    return g_DisplayLayers[layer][index];
}
