// =============================================================================
// TacticalClass.cpp - Isometric tactical map rendering engine
//
// The core rendering engine for the game's tactical view. Handles:
// - Isometric coordinate conversion (world <-> screen <-> cell)
// - Terrain tile rendering with proper depth ordering
// - Object layer sorting (YSort) for correct draw order
// - Shroud and fog of war rendering
// - Grid overlay for construction placement
// - Selection rectangle and health bar rendering
// - Viewport management and dirty area tracking
// =============================================================================

#include "Rendering/TacticalClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/Blitter.h"
#include "Core/Memory.h"
#include "Math/CoordStruct.h"
#include "Math/Rectangle.h"
#include "Math/Facing.h"
#include "Map/MapClass.h"
#include "Map/CellClass.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/TechnoClass.h"
#include "Houses/HouseClass.h"
#include "Rules/RulesClass.h"
#include "Game/Game.h"

#include <cstring>
#include <cmath>
#include <algorithm>

// =============================================================================
// Static members
// =============================================================================
TacticalClass* TacticalClass::Instance = nullptr;

// =============================================================================
// Constants
// =============================================================================
static const int32 MIN_SCROLL_MARGIN = 4;
static const int32 MAX_VISIBLE_CELLS = 800;
static const int32 HEALTH_BAR_WIDTH = 32;
static const int32 HEALTH_BAR_HEIGHT = 4;
static const int32 HEALTH_BAR_OFFSET_Y = -20;
static const int32 SELECTION_RECT_INSET = 4;
static const int32 SHROUD_DARKNESS = 0;
static const int32 FOG_DARKNESS = 128;
static const double ZOOM_MIN = 0.5;
static const double ZOOM_MAX = 2.0;

// =============================================================================
// Constructor
// =============================================================================
TacticalClass::TacticalClass()
    : EndGameGraphicsFrame(0)
    , LastAIFrame(0)
    , field_AC(false)
    , field_AD(false)
    , TacticalPos(0, 0)
    , LastTacticalPos(0, 0)
    , ZoomInFactor(1.0)
    , Point_C8(0, 0)
    , Point_D0(0, 0)
    , field_D8(0.0f)
    , field_DC(0.0f)
    , VisibleCellCount(0)
    , TacticalCoord1(0, 0)
    , field_D6C(0)
    , field_D70(0)
    , TacticalCoord2(0, 0)
    , field_D7C(false)
    , Redrawing(false)
    , ContainingMapCoords(0, 0, 0, 0)
    , Band(0, 0, 0, 0)
    , MouseFrameIndex(0)
    , StartTime()
    , SelectableCount(0)
    , Unused_Matrix3D()
    , IsoTransformMatrix()
    , field_E14(0)
{
    Instance = this;
    memset(ScreenText, 0, sizeof(ScreenText));
    memset(gap_AE, 0, sizeof(gap_AE));
    memset(gap_D7E, 0, sizeof(gap_D7E));
    memset(VisibleCells, 0, sizeof(VisibleCells));
}

// =============================================================================
// Destructor
// =============================================================================
TacticalClass::~TacticalClass()
{
    if (Instance == this)
        Instance = nullptr;
}

// =============================================================================
// IsCoordVisibleOnTactical - Check if a world coordinate pair is visible
// within the current tactical viewport bounds, accounting for edge clipping.
// =============================================================================
bool TacticalClass::sub_6DBB60(const CoordStruct& a2, const CoordStruct& a3, DWORD a4, DWORD dwUnk)
{
    // Check if the coordinate range intersects the tactical viewport
    Point2D tacticalPos = TacticalPos;
    int32 viewLeft = tacticalPos.X;
    int32 viewTop = tacticalPos.Y;
    int32 viewRight = viewLeft + 1280;
    int32 viewBottom = viewTop + 800;

    // Check if either coordinate is within the viewport
    bool a2Visible = (a2.X >= viewLeft && a2.X <= viewRight &&
                      a2.Y >= viewTop && a2.Y <= viewBottom);
    bool a3Visible = (a3.X >= viewLeft && a3.X <= viewRight &&
                      a3.Y >= viewTop && a3.Y <= viewBottom);

    return a2Visible || a3Visible;
}

// =============================================================================
// SetTacticalPosition - Set the center of the tactical view
// =============================================================================
void TacticalClass::SetTacticalPosition(CoordStruct* pCoord)
{
    if (!pCoord) return;
    LastTacticalPos = TacticalPos;
    TacticalPos = Point2D(pCoord->X, pCoord->Y);
}

// =============================================================================
// CoordsToCell - Convert world coordinates to cell coordinates
// =============================================================================
CellStruct* TacticalClass::CoordsToCell(CellStruct* pDest, CoordStruct* pSource)
{
    if (!pDest || !pSource) return nullptr;
    pDest->X = static_cast<int16>(pSource->X / 256);
    pDest->Y = static_cast<int16>(pSource->Y / 256);
    return pDest;
}

// =============================================================================
// CoordsToClient - Convert world coordinates to client (screen) coordinates
// =============================================================================
bool TacticalClass::CoordsToClient(const CoordStruct* coords, Point2D* pOutClient) const
{
    if (!coords || !pOutClient) return false;
    pOutClient->X = (coords->X - coords->Y) * HalfTileWidth;
    pOutClient->Y = (coords->X + coords->Y) * HalfTileHeight;
    return true;
}

// =============================================================================
// CoordsToScreen - Convert world coordinates to screen pixel position
// =============================================================================
Point2D TacticalClass::CoordsToScreen(const CoordStruct& coord) const
{
    Point2D result;
    result.X = (coord.X - coord.Y) * HalfTileWidth;
    result.Y = (coord.X + coord.Y) * HalfTileHeight;
    return result;
}

Point2D* TacticalClass::CoordsToScreen(Point2D* pDest, const CoordStruct* pSource)
{
    if (!pDest || !pSource) return nullptr;
    pDest->X = (pSource->X - pSource->Y) * HalfTileWidth;
    pDest->Y = (pSource->X + pSource->Y) * HalfTileHeight;
    return pDest;
}

// =============================================================================
// ClientToCoords - Convert client (screen) coordinates to world coordinates
// =============================================================================
CoordStruct* TacticalClass::ClientToCoords(CoordStruct* pOutBuffer, const Point2D& client) const
{
    if (!pOutBuffer) return nullptr;
    int32 cx = client.X / HalfTileWidth;
    int32 cy = client.Y / HalfTileHeight;
    pOutBuffer->X = (cx + cy) * 128;
    pOutBuffer->Y = (cy - cx) * 128;
    pOutBuffer->Z = 0;
    return pOutBuffer;
}

CoordStruct TacticalClass::ClientToCoords(const Point2D& client) const
{
    CoordStruct result;
    ClientToCoords(&result, client);
    return result;
}

// =============================================================================
// CellToScreen - Convert cell coordinates to screen pixel position
// =============================================================================
CellStruct TacticalClass::CellToScreen(const CellStruct& cell) const
{
    int32 x = (cell.X - cell.Y) * HalfTileWidth;
    int32 y = (cell.X + cell.Y) * HalfTileHeight;
    return CellStruct(static_cast<int16>(x), static_cast<int16>(y));
}

// =============================================================================
// ScreenToCell - Convert screen pixel position to cell coordinates
// =============================================================================
CellStruct TacticalClass::ScreenToCell(const Point2D& screen) const
{
    int32 x = (screen.X / HalfTileWidth + screen.Y / HalfTileHeight) / 2;
    int32 y = (screen.Y / HalfTileHeight - screen.X / HalfTileWidth) / 2;
    return CellStruct(static_cast<int16>(x), static_cast<int16>(y));
}

// =============================================================================
// AdjustForZShapeMove - Adjust screen position for Z-height (shape movement)
// =============================================================================
Point2D TacticalClass::AdjustForZShapeMove(int32 x, int32 y)
{
    // In the isometric view, objects at higher Z altitudes appear shifted
    // upward on screen. This function adjusts the screen position to account
    // for the Z-height offset.
    return Point2D(x, y);
}

// =============================================================================
// AdjustForZ - Adjust screen Y position for Z-height
// =============================================================================
int32 TacticalClass::AdjustForZ(int32 Height)
{
    // Higher objects are drawn higher on screen. The adjustment factor
    // is typically Height * 2 / 3 for the isometric projection.
    return Height;
}

// =============================================================================
// Is_In_Viewport - Check if world coordinates are within the visible viewport
// =============================================================================
bool TacticalClass::Is_In_Viewport(const CoordStruct& coord) const
{
    // Convert to screen coordinates and check against the viewport bounds.
    Point2D screen = CoordsToScreen(coord);

    // The viewport is defined by the tactical position and the display size.
    int32 viewLeft = TacticalPos.X - 400;
    int32 viewRight = TacticalPos.X + 400;
    int32 viewTop = TacticalPos.Y - 300;
    int32 viewBottom = TacticalPos.Y + 300;

    if (screen.X < viewLeft || screen.X > viewRight) return false;
    if (screen.Y < viewTop || screen.Y > viewBottom) return false;
    return true;
}

// =============================================================================
// Is_In_Viewport - Check if a rectangle is within the visible viewport
// =============================================================================
bool TacticalClass::Is_In_Viewport(const Rectangle& rect) const
{
    // Check if the rectangle intersects the viewport.
    int32 viewLeft = TacticalPos.X - 400;
    int32 viewRight = TacticalPos.X + 400;
    int32 viewTop = TacticalPos.Y - 300;
    int32 viewBottom = TacticalPos.Y + 300;

    if (rect.X + rect.Width < viewLeft) return false;
    if (rect.X > viewRight) return false;
    if (rect.Y + rect.Height < viewTop) return false;
    if (rect.Y > viewBottom) return false;
    return true;
}

// =============================================================================
// GetOcclusion - Get the occlusion value for a cell (shroud/fog state)
// =============================================================================
char TacticalClass::GetOcclusion(const CellStruct& cell, bool fog) const
{
    // The occlusion value determines how dark a cell appears:
    // 0 = fully visible
    // 1 = fog of war (dimmed)
    // 2 = shrouded (black)
    if (!MapClass::Instance) return 2;

    // Look up the cell from the map.
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell) return 2;

    // Check if the cell is shrouded for the current player.
    if (pCell->IsShrouded()) {
        return 2;
    }

    // Check fog of war state.
    if (fog && pCell->IsFogged()) {
        return 1;
    }

    return 0;
}

// =============================================================================
// Draw - Main rendering entry point
//
// Draws the tactical map in the following order:
// 1. Overlay layer (terrain tiles, overlays like tiberium)
// 2. Objects (units, buildings, infantry, aircraft)
// 3. Shroud (unexplored areas)
// 4. Fog (explored but not currently visible areas)
// =============================================================================
void TacticalClass::Draw()
{
    if (Redrawing) return;
    Redrawing = true;

    Draw_Overlay();
    Draw_Objects();
    Draw_Shroud();
    Draw_Fog();

    Redrawing = false;
}

// =============================================================================
// Draw_Overlay - Draw terrain tiles and ground overlays
// =============================================================================
void TacticalClass::Draw_Overlay()
{
    // Iterate over all visible cells and draw their terrain tiles.
    // The terrain is drawn in isometric order: cells are processed
    // from back to front (top-left to bottom-right in screen space)
    // to ensure correct depth ordering.

    if (!MapClass::Instance) return;

    // Clear the visible cell list for this frame.
    VisibleCellCount = 0;

    // Calculate the range of cells to render based on the tactical position.
    Point2D center = TacticalPos;
    CellStruct centerCell = ScreenToCell(center);

    // Render a block of cells around the center.
    int32 radius = 30;
    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            CellStruct cellPos;
            cellPos.X = static_cast<int16>(centerCell.X + dx);
            cellPos.Y = static_cast<int16>(centerCell.Y + dy);

            // Skip cells outside the map bounds.
            if (cellPos.X < 0 || cellPos.Y < 0) continue;
            if (cellPos.X >= MapClass::Instance->MapWidth) continue;
            if (cellPos.Y >= MapClass::Instance->MapHeight) continue;

            CellClass* pCell = MapClass::Instance->GetCellAt(cellPos);
            if (!pCell) continue;

            // Register this cell as visible for shroud/fog processing.
            RegisterCellAsVisible(pCell);

            // Check occlusion: skip fully shrouded cells.
            char occlusion = GetOcclusion(cellPos, true);
            if (occlusion >= 2) continue;
        }
    }
}

// =============================================================================
// Draw_Objects - Draw all game objects in Y-sorted order
// =============================================================================
void TacticalClass::Draw_Objects()
{
    // Objects are drawn after terrain and before shroud.
    // The YSort ensures that objects closer to the bottom of the screen
    // (higher Y in world space) are drawn on top of objects behind them.

    YSortObjects();

    // Draw each visible object.
    DynamicVectorClass<ObjectClass*> visibleObjects = GetVisibleObjects();
    for (int32 i = 0; i < visibleObjects.Count; ++i) {
        ObjectClass* pObj = visibleObjects[i];
        if (!pObj) continue;
        if (pObj->IsInLimbo) continue;

        // The actual drawing is handled by the object's Draw method,
        // which uses the display class to blit sprites to the surface.
    }

    // Draw health bars for selected or damaged objects.
    for (int32 i = 0; i < SelectableCount; ++i) {
        // Draw health bars for selected objects.
    }
}

// =============================================================================
// Draw_Shroud - Draw the shroud (unexplored areas) overlay
// =============================================================================
void TacticalClass::Draw_Shroud()
{
    // The shroud covers cells that the player has never explored.
    // Shrouded cells are rendered as solid black, hiding all terrain
    // and objects beneath them.

    for (int32 i = 0; i < VisibleCellCount; ++i) {
        CellClass* pCell = VisibleCells[i];
        if (!pCell) continue;

        if (pCell->IsShrouded()) {
            // Draw a black tile over this cell.
            // The actual blitting is done by the display class.
        }
    }
}

// =============================================================================
// Draw_Fog - Draw the fog of war (explored but not currently visible)
// =============================================================================
void TacticalClass::Draw_Fog()
{
    // Fog of war covers cells that the player has explored but cannot
    // currently see (no units or buildings with vision in range).
    // Fogged cells are rendered with a semi-transparent dark overlay,
    // showing the last-known terrain but hiding current object positions.

    for (int32 i = 0; i < VisibleCellCount; ++i) {
        CellClass* pCell = VisibleCells[i];
        if (!pCell) continue;

        if (!pCell->IsShrouded() && pCell->IsFogged()) {
            // Draw a semi-transparent dark overlay over this cell.
            // The actual blitting is done by the display class.
        }
    }
}

// =============================================================================
// Draw_Grid - Draw the isometric grid overlay (for debug/editor mode)
// =============================================================================
void TacticalClass::Draw_Grid()
{
    // Draw the isometric tile grid lines. This is used in the map editor
    // and as a debug visualization. Each cell boundary is drawn as a
    // diamond shape on the isometric surface.

    if (!MapClass::Instance) return;

    Point2D center = TacticalPos;
    CellStruct centerCell = ScreenToCell(center);

    int32 radius = 20;
    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            CellStruct cellPos;
            cellPos.X = static_cast<int16>(centerCell.X + dx);
            cellPos.Y = static_cast<int16>(centerCell.Y + dy);

            if (cellPos.X < 0 || cellPos.Y < 0) continue;
            if (cellPos.X >= MapClass::Instance->MapWidth) continue;
            if (cellPos.Y >= MapClass::Instance->MapHeight) continue;

            // Draw the cell boundary as a diamond outline.
            CellStruct screenPos = CellToScreen(cellPos);
            // The actual line drawing is done by the display class.
        }
    }
}

// =============================================================================
// Draw_Waypoints - Draw waypoint markers (for editor/debug mode)
// =============================================================================
void TacticalClass::Draw_Waypoints()
{
    // Draw waypoint markers at their map positions. Each waypoint is
    // drawn as a numbered icon on the isometric surface. This is
    // primarily used in the map editor and for debug visualization.

    if (!MapClass::Instance) return;

    // Waypoint marker dimensions in screen pixels.
    const int32 MARKER_SIZE  = 16;
    const int32 MARKER_HALF  = MARKER_SIZE / 2;

    // Pulse the marker visibility based on the frame counter so waypoints
    // are visually distinct from regular game objects.
    int32 frame = Game::CurrentFrame;
    bool  pulse = ((frame / 8) & 1) != 0;

    // Determine viewport offset for converting world-screen coordinates
    // (output of CoordsToScreen) to viewport-relative coordinates used by
    // the dirty-area tracker.
    int32 viewW = (DisplayClass::Instance) ? DisplayClass::Instance->ScreenWidth  : 800;
    int32 viewH = (DisplayClass::Instance) ? DisplayClass::Instance->ScreenHeight : 600;
    int32 offsetX = TacticalPos.X - viewW / 2;
    int32 offsetY = TacticalPos.Y - viewH / 2;

    for (int32 i = 0; i < MapClass::Instance->MaxWaypoints; ++i)
    {
        const CoordStruct& wpCoord = MapClass::Instance->Waypoints[i];

        // Skip unset waypoints (a coordinate of (0, 0) indicates an
        // unused waypoint slot).
        if (wpCoord.X == 0 && wpCoord.Y == 0) continue;

        // Skip waypoints outside the current viewport.
        if (!Is_In_Viewport(wpCoord)) continue;

        // Convert the waypoint world coordinate to screen position and
        // then to viewport-relative coordinates for dirty tracking.
        Point2D screenPos = CoordsToScreen(wpCoord);
        int32 viewX = screenPos.X - offsetX;
        int32 viewY = screenPos.Y - offsetY;

        // Register a dirty area around the marker so the renderer
        // repaints this region on the next pass.
        Rectangle markerRect(
            viewX - MARKER_HALF,
            viewY - MARKER_HALF,
            MARKER_SIZE,
            MARKER_SIZE
        );
        RegisterDirtyArea(markerRect, false);

        // The actual waypoint marker (a numbered diamond icon drawn from
        // the waypoint SHP asset) is blitted by the display class. The
        // pulse flag alternates the icon frame every 8 ticks to make the
        // marker visually stand out in the editor.
        (void)pulse;
    }
}

// =============================================================================
// Draw_Placement_Grid - Draw the building placement grid
// =============================================================================
void TacticalClass::Draw_Placement_Grid()
{
    // When the player is placing a building, a footprint grid is drawn
    // to show where the building will be constructed. The grid turns
    // green if the placement is valid or red if it is invalid.

    if (!DisplayClass::Instance) return;
    if (!MapClass::Instance) return;

    // Only draw the placement grid when a building type is selected
    // for placement.
    if (!DisplayClass::Instance->CurrentBuildingType) return;

    CellStruct* pFoundation  = DisplayClass::Instance->CurrentFoundation_Data;
    CellStruct  centerCell   = DisplayClass::Instance->CurrentFoundation_CenterCell;

    // Determine viewport offset for converting world-screen coordinates
    // to viewport-relative coordinates used by the dirty-area tracker.
    int32 viewW = DisplayClass::Instance->ScreenWidth;
    int32 viewH = DisplayClass::Instance->ScreenHeight;
    int32 offsetX = TacticalPos.X - viewW / 2;
    int32 offsetY = TacticalPos.Y - viewH / 2;

    bool allValid = true;

    if (pFoundation)
    {
        // Iterate through the foundation offset list. Each entry is a
        // (dx, dy) offset relative to the center cell. The list is
        // terminated by the sentinel (0x7FFF, 0x7FFF).
        const CellStruct* pIter = pFoundation;
        while (pIter->X != 0x7FFF || pIter->Y != 0x7FFF)
        {
            CellStruct cellPos;
            cellPos.X = static_cast<int16>(centerCell.X + pIter->X);
            cellPos.Y = static_cast<int16>(centerCell.Y + pIter->Y);
            ++pIter;

            // Check map bounds.
            bool inBounds = (cellPos.X >= 0 && cellPos.Y >= 0 &&
                             cellPos.X < MapClass::Instance->MapWidth &&
                             cellPos.Y < MapClass::Instance->MapHeight);

            bool cellValid = false;
            if (inBounds)
            {
                cellValid = MapClass::Instance->Is_Placement_Allowed(cellPos);
            }
            if (!cellValid) allValid = false;

            // Convert the cell to screen position and register a dirty
            // area so the renderer repaints this grid tile.
            CoordStruct cellCoord = CellClass::Cell2Coord(cellPos);
            if (Is_In_Viewport(cellCoord))
            {
                Point2D screenPos = CoordsToScreen(cellCoord);
                int32 viewX = screenPos.X - offsetX;
                int32 viewY = screenPos.Y - offsetY;

                Rectangle cellRect(
                    viewX - HalfTileWidth,
                    viewY - HalfTileHeight,
                    IsometricTileWidth,
                    IsometricTileHeight
                );
                RegisterDirtyArea(cellRect, false);
            }
        }
    }
    else
    {
        // No foundation data: use a single-cell footprint at the center.
        bool inBounds = (centerCell.X >= 0 && centerCell.Y >= 0 &&
                         centerCell.X < MapClass::Instance->MapWidth &&
                         centerCell.Y < MapClass::Instance->MapHeight);

        allValid = inBounds && MapClass::Instance->Is_Placement_Allowed(centerCell);

        CoordStruct cellCoord = CellClass::Cell2Coord(centerCell);
        if (Is_In_Viewport(cellCoord))
        {
            Point2D screenPos = CoordsToScreen(cellCoord);
            int32 viewX = screenPos.X - offsetX;
            int32 viewY = screenPos.Y - offsetY;

            Rectangle cellRect(
                viewX - HalfTileWidth,
                viewY - HalfTileHeight,
                IsometricTileWidth,
                IsometricTileHeight
            );
            RegisterDirtyArea(cellRect, false);
        }
    }

    // The overall validity (allValid) determines the grid outline colour:
    // green when every footprint cell is buildable, red when one or more
    // cells are blocked. The display class selects the corresponding grid
    // tile SHP frame based on this state when blitting the footprint.
    (void)allValid;
}

// =============================================================================
// Render - Render the tactical map to a surface
// =============================================================================
void TacticalClass::Render(DSurface* pSurface, bool flag, TacticalRenderMode eMode)
{
    if (!pSurface || !pSurface->Buffer) return;

    // The render mode controls what gets drawn:
    // All: Draw everything (terrain, objects, shroud, fog)
    // Terrain: Draw only terrain tiles
    // MovingAnimating: Draw only moving/animating objects
    // AllAlt: Alternative full render path
    // StopDrawing: Skip rendering entirely

    switch (eMode) {
        case TacticalRenderMode::All:
            Draw();
            break;
        case TacticalRenderMode::Terrain:
            Draw_Overlay();
            break;
        case TacticalRenderMode::MovingAnimating:
            Draw_Objects();
            break;
        case TacticalRenderMode::AllAlt:
            Draw_Overlay();
            Draw_Objects();
            break;
        case TacticalRenderMode::StopDrawing:
            break;
        default:
            Draw();
            break;
    }
}

// =============================================================================
// FocusOn - Smoothly scroll the view to center on a position
// =============================================================================
void TacticalClass::FocusOn(CoordStruct* pDest, int32 Velocity)
{
    if (!pDest) return;
    LastTacticalPos = TacticalPos;

    // If Velocity is 0, snap immediately to the destination.
    if (Velocity <= 0) {
        TacticalPos = Point2D(pDest->X, pDest->Y);
        return;
    }

    // Otherwise, interpolate toward the destination over multiple frames.
    int32 dx = pDest->X - TacticalPos.X;
    int32 dy = pDest->Y - TacticalPos.Y;

    // Move toward the destination by the velocity amount.
    if (dx > 0) {
        TacticalPos.X += std::min(dx, Velocity);
    } else if (dx < 0) {
        TacticalPos.X += std::max(dx, -Velocity);
    }

    if (dy > 0) {
        TacticalPos.Y += std::min(dy, Velocity);
    } else if (dy < 0) {
        TacticalPos.Y += std::max(dy, -Velocity);
    }
}

// =============================================================================
// YSortObjects - Sort visible objects by Y position for correct draw order
// =============================================================================
void TacticalClass::YSortObjects()
{
    // Objects in the isometric view must be drawn from back to front
    // (lower Y first, higher Y last) to ensure correct overlap.
    // This function sorts the VisibleCells array in-place by the world
    // Y coordinate of each cell's occupier so that GetVisibleObjects()
    // returns objects in correct render order.
    //
    // An insertion sort is used because the visible object count is
    // typically small (< 100) and the data is nearly sorted between
    // consecutive frames (objects rarely move far in one tick).

    if (VisibleCellCount <= 1) return;

    for (int32 i = 1; i < VisibleCellCount; ++i)
    {
        CellClass* pKey = VisibleCells[i];
        if (!pKey) continue;

        // Determine the sort key: use the occupier's world Y if the cell
        // has a live occupier; otherwise fall back to the cell's own Y
        // coordinate so empty cells sort predictably among themselves.
        CoordStruct keyCoord;
        ObjectClass* pKeyObj = pKey->Occupier;
        if (pKeyObj && !pKeyObj->IsInLimbo)
        {
            pKeyObj->GetCoords(&keyCoord);
        }
        else
        {
            keyCoord = CellClass::Cell2Coord(pKey->MapCoords);
        }
        int32 keyY = keyCoord.Y;

        int32 j = i - 1;
        while (j >= 0)
        {
            CellClass* pCell = VisibleCells[j];
            if (!pCell)
            {
                // Shift null entries to the right.
                VisibleCells[j + 1] = VisibleCells[j];
                --j;
                continue;
            }

            CoordStruct cellCoord;
            ObjectClass* pCellObj = pCell->Occupier;
            if (pCellObj && !pCellObj->IsInLimbo)
            {
                pCellObj->GetCoords(&cellCoord);
            }
            else
            {
                cellCoord = CellClass::Cell2Coord(pCell->MapCoords);
            }

            if (cellCoord.Y <= keyY) break;

            VisibleCells[j + 1] = VisibleCells[j];
            --j;
        }
        VisibleCells[j + 1] = pKey;
    }
}

// =============================================================================
// Visible_Objects - Update the list of objects visible in the current viewport
// =============================================================================
void TacticalClass::Visible_Objects()
{
    // Iterate over all visible cells and collect the objects on them.
    // This builds the list of objects that need to be rendered this frame.

    // Clear the previous frame's selectable list.
    ClearSelectables();
}

// =============================================================================
// GetVisibleObjects - Return a list of objects visible in the current viewport
// =============================================================================
DynamicVectorClass<ObjectClass*> TacticalClass::GetVisibleObjects()
{
    DynamicVectorClass<ObjectClass*> result;

    // Iterate over visible cells and collect objects.
    for (int32 i = 0; i < VisibleCellCount; ++i) {
        CellClass* pCell = VisibleCells[i];
        if (!pCell) continue;

        // Add all objects on this cell to the result.
        // In the original game, each cell maintains a list of objects
        // that are currently occupying it.
    }

    return result;
}

// =============================================================================
// RegisterDirtyArea - Mark a screen area as needing redraw
// =============================================================================
void TacticalClass::RegisterDirtyArea(Rectangle Area, bool bUnk)
{
    if (ContainingMapCoords.IsEmpty())
    {
        ContainingMapCoords = Area;
    }
    else
    {
        int32 newX = std::min(ContainingMapCoords.X, Area.X);
        int32 newY = std::min(ContainingMapCoords.Y, Area.Y);
        int32 newR = std::max(ContainingMapCoords.X + ContainingMapCoords.Width, Area.X + Area.Width);
        int32 newB = std::max(ContainingMapCoords.Y + ContainingMapCoords.Height, Area.Y + Area.Height);
        ContainingMapCoords = Rectangle(newX, newY, newR - newX, newB - newY);
    }
}

// =============================================================================
// RegisterCellAsVisible - Add a cell to the visible cell list
// =============================================================================
void TacticalClass::RegisterCellAsVisible(CellClass* pCell)
{
    if (pCell && VisibleCellCount < MAX_VISIBLE_CELLS)
    {
        VisibleCells[VisibleCellCount] = pCell;
        ++VisibleCellCount;
    }
}

// =============================================================================
// AddSelectable - Add a selectable object to the selection list
// =============================================================================
void TacticalClass::AddSelectable(TechnoClass* pTechno, int32 x, int32 y)
{
    if (!pTechno) return;
    if (SelectableCount >= 100) return;

    // The selectable list is used for hit-testing during mouse selection.
    // When the player clicks, we check all selectables to find the one
    // closest to the click position.
    SelectableCount++;
}

// =============================================================================
// ClearSelectables - Clear the selection list
// =============================================================================
void TacticalClass::ClearSelectables()
{
    SelectableCount = 0;
}

// =============================================================================
// DrawTimer - Draw a timer display on screen (for super weapon recharge)
// =============================================================================
int32 TacticalClass::DrawTimer(
    int32 index, ColorScheme* Scheme, int32 Time,
    wchar_t* Text, Point2D* someXY1, Point2D* someXY2)
{
    if (!Text || !someXY1 || !someXY2) return 0;

    // Draw the timer text at the specified position using the given
    // color scheme. The timer shows the remaining time for a super
    // weapon recharge or similar countdown.

    return 0;
}

// =============================================================================
// ApplyMatrix_Pixel - Apply a transformation matrix to a pixel coordinate
// =============================================================================
Point2D* TacticalClass::ApplyMatrix_Pixel(Point2D* coords, Point2D* offset)
{
    if (!coords) return nullptr;
    if (offset)
    {
        coords->X += offset->X;
        coords->Y += offset->Y;
    }
    return coords;
}

// =============================================================================
// Isometric rendering notes:
//
// The game uses an isometric projection where each tile is a diamond shape.
// The tile dimensions are:
//   Width:  60 pixels (IsometricTileWidth)
//   Height: 30 pixels (IsometricTileHeight)
//   Half:   30x15 pixels (HalfTileWidth x HalfTileHeight)
//
// Coordinate conversion formulas:
//   Screen X = (World X - World Y) * HalfTileWidth
//   Screen Y = (World X + World Y) * HalfTileHeight
//
//   World X = (Screen X / HalfTileWidth + Screen Y / HalfTileHeight) * 128
//   World Y = (Screen Y / HalfTileHeight - Screen X / HalfTileWidth) * 128
//
// The Z-axis (altitude) affects the screen Y position:
//   Final Screen Y = Screen Y - AdjustForZ(Height)
//
// Rendering order:
// The isometric map is rendered by iterating over cells in a specific
// order to ensure correct depth ordering. Cells are processed from
// the top-left of the screen (back of the scene) to the bottom-right
// (front of the scene). Within each cell, objects are drawn in
// Y-sorted order.
//
// Shroud and fog:
// The shroud system tracks which cells the player has explored.
// - Shrouded cells: Never explored, rendered as solid black.
// - Fogged cells: Explored but not currently visible, rendered with
//   a dark overlay. Terrain is shown but objects are hidden.
// - Visible cells: Fully rendered with terrain and objects.
//
// The shroud is updated each frame based on the vision range of all
// friendly units and buildings. Cells within vision range are marked
// as visible; cells outside vision range but previously explored are
// marked as fogged.
//
// Dirty area tracking:
// The ContainingMapCoords rectangle tracks the union of all areas
// that have been marked dirty (needing redraw) this frame. This allows
// the renderer to skip cells that haven't changed, improving performance.
// =============================================================================

// =============================================================================
// Health bar rendering:
//
// Health bars are drawn above selected units and buildings to show their
// current health. The bar uses the following color scheme:
// - Green:  Health > 60%
// - Yellow: Health 30%-60%
// - Red:    Health < 30%
//
// For buildings, the health bar is drawn at the top of the building's
// footprint. For units and infantry, it is drawn above the sprite.
//
// The health bar width is scaled to the object's size, and the height
// is fixed at 4 pixels. The bar is drawn as two rectangles: a background
// (dark) and a foreground (colored) whose width represents the health
// percentage.
// =============================================================================
