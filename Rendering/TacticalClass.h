#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Abstract/AbstractClass.h"
#include "Rendering/Surface.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/ConvertClass.h"
#include "Math/CoordStruct.h"
#include "Math/Rectangle.h"
#include "Math/Matrix3D.h"
#include "Math/Facing.h"
#include "Math/Timer.h"
#include "Containers/DynamicVectorClass.h"

// Forward declarations
class DSurface;
class CellClass;
class ObjectClass;
class TechnoClass;
class HouseClass;
class IsometricTileClass;

// ============================================================================
// TacticalSelectableStruct - Selectable object info
// ============================================================================
struct TacticalSelectableStruct
{
    TechnoClass* Techno;
    int32 X;
    int32 Y;
};

// ============================================================================
// TacticalRenderMode - Rendering mode for tactical map
// ============================================================================
enum class TacticalRenderMode : int32
{
    All = 0,
    Terrain = 1,
    MovingAnimating = 2,
    AllAlt = 3,
    StopDrawing = 4,
    Mode5 = 5,
};

// ============================================================================
// TacticalClass - Tactical map rendering (isometric tile engine)
//
// The core isometric rendering engine for C&C. Handles terrain tile
// rendering, object layer sorting (YSort), shroud/fog of war, grid
// overlays, and isometric coordinate conversion.
// ============================================================================
class NOVTABLE TacticalClass : public AbstractClass
{
public:
    static TacticalClass* Instance;

    TacticalClass();
    virtual ~TacticalClass();

    // Virtual methods
    virtual bool sub_6DBB60(const CoordStruct& a2, const CoordStruct& a3, DWORD a4, DWORD dwUnk);

    // Isometric coordinate conversion
    void SetTacticalPosition(CoordStruct* pCoord);
    CellStruct* CoordsToCell(CellStruct* pDest, CoordStruct* pSource);
    bool CoordsToClient(const CoordStruct* coords, Point2D* pOutClient) const;
    Point2D CoordsToScreen(const CoordStruct& coord) const;
    Point2D* CoordsToScreen(Point2D* pDest, const CoordStruct* pSource);
    CoordStruct* ClientToCoords(CoordStruct* pOutBuffer, const Point2D& client) const;
    CoordStruct ClientToCoords(const Point2D& client) const;

    // Cell operations
    CellStruct CellToScreen(const CellStruct& cell) const;
    CellStruct ScreenToCell(const Point2D& screen) const;
    static Point2D AdjustForZShapeMove(int32 x, int32 y);
    static int32 AdjustForZ(int32 Height);

    // Viewport testing
    bool Is_In_Viewport(const CoordStruct& coord) const;
    bool Is_In_Viewport(const Rectangle& rect) const;
    char GetOcclusion(const CellStruct& cell, bool fog) const;

    // Rendering
    void Draw();
    void Draw_Overlay();
    void Draw_Objects();
    void Draw_Shroud();
    void Draw_Fog();
    void Draw_Grid();
    void Draw_Waypoints();
    void Draw_Placement_Grid();
    void Render(DSurface* pSurface, bool flag, TacticalRenderMode eMode);
    void FocusOn(CoordStruct* pDest, int32 Velocity);

    // Layer sorting (YSort) for correct render order
    void YSortObjects();

    // Visible object management
    void Visible_Objects();
    DynamicVectorClass<ObjectClass*> GetVisibleObjects();

    // Dirty area management
    void RegisterDirtyArea(Rectangle Area, bool bUnk);
    void RegisterCellAsVisible(CellClass* pCell);

    // Selection
    void AddSelectable(TechnoClass* pTechno, int32 x, int32 y);
    void ClearSelectables();

    // Timer drawing
    static int32 DrawTimer(
        int32 index, ColorScheme* Scheme, int32 Time,
        wchar_t* Text, Point2D* someXY1, Point2D* someXY2);

    // Matrix operations
    Point2D* ApplyMatrix_Pixel(Point2D* coords, Point2D* offset);

    // Properties
    static constexpr int32 IsometricTileWidth = 60;
    static constexpr int32 IsometricTileHeight = 30;
    static constexpr int32 HalfTileWidth = 30;
    static constexpr int32 HalfTileHeight = 15;

    wchar_t ScreenText[64];
    int32 EndGameGraphicsFrame;
    int32 LastAIFrame;
    bool field_AC;
    bool field_AD;
    BYTE gap_AE[2];
    Point2D TacticalPos;
    Point2D LastTacticalPos;
    double ZoomInFactor;
    Point2D Point_C8;
    Point2D Point_D0;
    float field_D8;
    float field_DC;
    int32 VisibleCellCount;
    CellClass* VisibleCells[800];
    Point2D TacticalCoord1;
    DWORD field_D6C;
    DWORD field_D70;
    Point2D TacticalCoord2;
    bool field_D7C;
    bool Redrawing;
    BYTE gap_D7E[2];
    Rectangle ContainingMapCoords;
    LTRBStruct Band;
    DWORD MouseFrameIndex;
    CDTimerClass StartTime;
    int32 SelectableCount;
    Matrix3D Unused_Matrix3D;
    Matrix3D IsoTransformMatrix;
    DWORD field_E14;
};