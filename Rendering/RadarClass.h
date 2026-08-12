#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/Surface.h"
#include "Containers/DynamicVectorClass.h"
#include "Math/Rectangle.h"
#include "Math/Timer.h"

// Forward declarations
class HouseClass;

// ============================================================================
// RadarClass - Radar/minimap rendering
//
// Renders the minimap in the corner of the screen. Shows terrain,
// unit positions, and building foundations. Supports zooming and
// clicking to navigate the tactical view.
// ============================================================================
class NOVTABLE RadarClass : public DisplayClass
{
public:
    static RadarClass* Instance;

    RadarClass();
    virtual ~RadarClass();

    // MapClass methods
    virtual void CreateEmptyMap(
        const Rectangle& pMapRect, bool reuse, int8 nLevel, bool bUnk2);
    virtual void SetVisibleRect(const Rectangle& mapRect);

    // DisplayClass overrides
    virtual MouseCursorType GetLastMouseCursor() override;

    // RadarClass virtual methods
    virtual void DisposeOfArt();
    virtual void* vt_entry_CC(void* out_pUnk, Point2D* pPoint);
    virtual void vt_entry_D0(DWORD dwUnk);
    virtual void Init_For_House();

    // Non-virtual methods
    void Init_Radar();
    void Draw();
    void Update();
    void Click_Render();
    bool IsRadarHidden() const;
    void ToggleRadar();
    void RenderTerrain();
    void RenderUnits();
    void RenderBuildings();
    Point2D WorldToRadar(const CoordStruct& world) const;
    CoordStruct RadarToWorld(const Point2D& radar) const;

    // Properties
    static constexpr int32 RadarWidth = 128;
    static constexpr int32 RadarHeight = 128;
    static constexpr int32 MaxZoomLevel = 4;
    static constexpr int32 MinZoomLevel = 1;

    DWORD unknown_11E8;
    DWORD unknown_11EC;
    DWORD unknown_11F0;
    DWORD unknown_11F4;
    DWORD unknown_11F8;
    DWORD unknown_11FC;
    DWORD unknown_1200;
    DWORD unknown_1204;
    DWORD unknown_1208;
    Rectangle unknown_rect_120C;
    DWORD unknown_121C;
    DWORD unknown_1220;
    DynamicVectorClass<CellStruct> unknown_cells_1124;
    DWORD unknown_123C;
    DWORD unknown_1240;
    DWORD unknown_1244;
    DWORD unknown_1248;
    DWORD unknown_124C;
    DWORD unknown_1250;
    DWORD unknown_1254;
    DWORD unknown_1258;
    DynamicVectorClass<Point2D> unknown_points_125C;
    DWORD unknown_1274;
    DynamicVectorClass<Point2D> FoundationTypePixels[22];
    float RadarSizeFactor;
    int32 unknown_int_148C;
    DWORD unknown_1490;
    DWORD unknown_1494;
    DWORD unknown_1498;
    Rectangle unknown_rect_149C;
    DWORD unknown_14AC;
    DWORD unknown_14B0;
    DWORD unknown_14B4;
    DWORD unknown_14B8;
    bool unknown_bool_14BC;
    bool unknown_bool_14BD;
    DWORD unknown_14C0;
    DWORD unknown_14C4;
    DWORD unknown_14C8;
    DWORD unknown_14CC;
    DWORD unknown_14D0;
    int32 unknown_int_14D4;
    bool IsAvailableNow;
    bool unknown_bool_14D9;
    bool unknown_bool_14DA;
    Rectangle unknown_rect_14DC;
    DWORD unknown_14EC;
    DWORD unknown_14F0;
    DWORD unknown_14F4;
    DWORD unknown_14F8;
    DWORD unknown_14FC;
    CDTimerClass unknown_timer_1500;

private:
    bool IsHidden;
    int32 RadarZoomLevel;
    Surface* RadarSurface;
    Rectangle RadarRect;
};