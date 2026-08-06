#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"
#include "Map/MapClass.h"
#include "Math/CoordStruct.h"
#include "Math/Rectangle.h"
#include "Containers/DynamicVectorClass.h"

// Forward declarations
class CCINIClass;
class ObjectTypeClass;
class ObjectClass;
class HouseClass;
class TechnoClass;
class CellClass;
class MapClass;

// MapClass is defined in Map/MapClass.h

// ============================================================================
// ScrollDirType - Scroll direction enum
// ============================================================================
enum class ScrollDirType : int32
{
    None = -1, North = 0, NorthEast = 1, East = 2, SouthEast = 3,
    South = 4, SouthWest = 5, West = 6, NorthWest = 7, Count = 8
};

// ============================================================================
// DisplayClass - Main display manager
//
// Manages the tactical view, map scrolling, mouse interaction, and
// building placement. Inherits from MapClass.
// ============================================================================
class NOVTABLE DisplayClass : public MapClass
{
public:
    static DisplayClass* Instance;

    DisplayClass();
    virtual ~DisplayClass();

    // MapClass overrides
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual void LoadFromINI(CCINIClass* pINI);
    virtual const wchar_t* GetToolTip(uint32 nDlgID);
    virtual void CloseWindow();

    // DisplayClass virtual methods
    virtual bool MapCell(CellStruct* pMapCoord, HouseClass* pHouse);
    virtual bool RevealFogShroud(CellStruct* pMapCoord, HouseClass* pHouse, bool bIncreaseShroudCounter);
    virtual bool MapCellFoggedness(CellStruct* pMapCoord, HouseClass* pHouse);
    virtual bool MapCellVisibility(CellStruct* pMapCoord, HouseClass* pHouse);
    virtual MouseCursorType GetLastMouseCursor() = 0;
    virtual bool ScrollMap(DWORD dwUnk1, DWORD dwUnk2, DWORD dwUnk3);
    virtual void Set_View_Dimensions(const Rectangle& rect);
    virtual void vt_entry_AC(DWORD dwUnk);
    virtual void vt_entry_B0(DWORD dwUnk);
    virtual void vt_entry_B4(Point2D* pPoint);

    // Mouse interaction
    virtual bool ConvertAction(
        const CellStruct& cell, bool bShrouded, ObjectClass* pObject,
        Action action, bool dwUnk);
    virtual void LeftMouseButtonDown(const Point2D& point);
    virtual void LeftMouseButtonUp(
        const CoordStruct& coords, const CellStruct& cell,
        ObjectClass* pObject, Action action, DWORD dwUnk2);
    virtual void RightMouseButtonUp(DWORD dwUnk);

    // Non-virtual methods
    void Init();
    void Draw(bool forced);
    void Update();
    void Pan(int32 dx, int32 dy);
    void Scroll(ScrollDirType dir);
    void CenterOn(const CoordStruct& coord);
    void ZoomIn();
    void ZoomOut();
    void TacticalToScreen(const CoordStruct& coord, Point2D* outPoint);
    void ScreenToTactical(const Point2D& point, CoordStruct* outCoord);
    bool IsInView(const CoordStruct& coord);
    void MarkToRedraw();
    void Read_INI();
    void Help_Text(const wchar_t* text);
    void Mouse_Left_Release();

    // Action decision
    Action DecideAction(const CellStruct& cell, ObjectClass* pObject, DWORD dwUnk);

    // Foundation management
    CellStruct* FoundationBoundsSize(
        CellStruct& outBuffer, const CellStruct* pFoundationData) const;
    CellStruct FoundationBoundsSize(const CellStruct* pFoundationData) const;
    void MarkFoundation(CellStruct* BaseCell, bool Mark);
    void SetActiveFoundation(CellStruct* Coords);

    // Layer management
    void Submit(ObjectClass* pObject);
    void Remove(ObjectClass* pObject);

    // Click processing
    bool ProcessClickCoords(Point2D* src, CellStruct* XYdst, CoordStruct* XYZdst,
                            ObjectClass** Target, BYTE* a5, BYTE* a6);

    // Properties
    Rectangle VisibleRect;
    Rectangle MapRect;
    int32 ScreenWidth;
    int32 ScreenHeight;
    float ZoomLevel;
    int32 ScrollAmount;
    bool IsScrolling;
    Point2D CursorPosition;
    Point2D ViewPosition;
    int32 ScrollTimer;

    // Foundation placement data
    CellStruct CurrentFoundation_CenterCell;
    CellStruct CurrentFoundation_TopLeftOffset;
    CellStruct* CurrentFoundation_Data;
    bool unknown_1180;
    bool unknown_1181;
    CellStruct CurrentFoundationCopy_CenterCell;
    CellStruct CurrentFoundationCopy_TopLeftOffset;
    CellStruct* CurrentFoundationCopy_Data;
    DWORD unknown_1190;
    DWORD unknown_1194;
    DWORD unknown_1198;
    bool FollowObject;
    ObjectClass* ObjectToFollow;
    ObjectClass* CurrentBuilding;
    ObjectTypeClass* CurrentBuildingType;
    DWORD unknown_11AC;
    bool RepairMode;
    bool SellMode;
    bool PowerToggleMode;
    bool PlanningMode;
    bool PlaceBeaconMode;
    int32 CurrentSWTypeIndex;
    DWORD unknown_11BC;
    DWORD unknown_11C0;
    DWORD unknown_11C4;
    DWORD unknown_11C8;
    bool unknown_bool_11CC;
    bool unknown_bool_11CD;
    bool unknown_bool_11CE;
    bool DraggingRectangle;
    bool unknown_bool_11D0;
    bool unknown_bool_11D1;
    DWORD unknown_11D4;
    DWORD unknown_11D8;
    DWORD unknown_11DC;
    DWORD unknown_11E0;
    DWORD padding_11E4;
};