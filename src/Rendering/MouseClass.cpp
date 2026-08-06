#include "Rendering/MouseClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/SidebarClass.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Abstract/ObjectTypeClass.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/TechnoClass.h"
#include "Core/Memory.h"
#include "Math/Rectangle.h"

#include <cstring>
#include <cmath>
#include <algorithm>

// ============================================================================
// Static members
// ============================================================================
MouseClass* MouseClass::Instance = nullptr;

// ============================================================================
// Internal cursor descriptor table.
//
// Mirrors the original game's static cursor definitions. Each entry maps a
// MouseCursorType to its animation frame range, hotspot and minimap variant.
// The values follow the conventions used by mouse.shp: every cursor shape
// occupies a contiguous run of frames inside the shared shape file.
// ============================================================================
namespace {

struct CursorDescriptor
{
    int32 Frame;        // First frame index in mouse.shp
    int32 Count;        // Number of animation frames
    int32 Interval;     // Ticks per frame
    int32 MiniFrame;    // First frame index for the minimap variant (-1 = none)
    int32 MiniCount;    // Frame count for the minimap variant
    MouseHotSpotX HotX; // Horizontal hotspot alignment
    MouseHotSpotY HotY; // Vertical hotspot alignment
};

// Canonical cursor table for Yuri's Revenge. The frame layout matches the
// stock mouse.shp that ships with the game.
constexpr CursorDescriptor CursorTable[] = {
    {   0,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Normal
    {   1,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // No
    {   2,  6, 4,  44, 6, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Move
    {   8,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Enter
    {   9,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Deploy
    {  10,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Attack
    {  11,  6, 4,  50, 6, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Harvest
    {  17,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Select
    {  18,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Top     }, // ScrollN
    {  19,  1, 1,  -1, 0, MouseHotSpotX::Left,    MouseHotSpotY::Top     }, // ScrollNE
    {  20,  1, 1,  -1, 0, MouseHotSpotX::Left,    MouseHotSpotY::Middle  }, // ScrollE
    {  21,  1, 1,  -1, 0, MouseHotSpotX::Left,    MouseHotSpotY::Bottom  }, // ScrollSE
    {  22,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Bottom  }, // ScrollS
    {  23,  1, 1,  -1, 0, MouseHotSpotX::Right,   MouseHotSpotY::Bottom  }, // ScrollSW
    {  24,  1, 1,  -1, 0, MouseHotSpotX::Right,   MouseHotSpotY::Middle  }, // ScrollW
    {  25,  1, 1,  -1, 0, MouseHotSpotX::Right,   MouseHotSpotY::Top     }, // ScrollNW
    {  26,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // NoMove
    {  27,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // NoEnter
    {  28,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // NoDeploy
    {  29,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Sell
    {  30,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Sellable
    {  31,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Repair
    {  32,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Repairable
    {  33,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // DisarmBomb
    {  34,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // ToggleSelect
    {  35,  6, 4,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // GuardArea
    {  41,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Airstrike
    {  42,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Chronosphere
    {  43,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // PlaceBeacon
    {  44,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Heal
    {  45,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // SpyPlane
    {  46,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // PsychicReveal
    {  47,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // ChronoWarp
    {  48,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // IronCurtain
    {  49,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // PlaceWaypoint
    {  50,  1, 1,  -1, 0, MouseHotSpotX::Center,  MouseHotSpotY::Middle  }, // Demolish
};

// Index helpers
constexpr int32 CursorIndexBase = static_cast<int32>(MouseCursorType::Normal);
constexpr int32 CursorIndexCount = static_cast<int32>(MouseCursorType::Count);

inline const CursorDescriptor& GetDescriptor(MouseCursorType type)
{
    int32 idx = static_cast<int32>(type) - CursorIndexBase;
    if (idx < 0 || idx >= CursorIndexCount)
        idx = 0; // Fall back to Normal cursor
    return CursorTable[idx];
}

// Edge band used to detect scroll zones at the screen borders.
constexpr int32 ScrollEdgeSize = 8;

// Minimum drag distance (in pixels) before a selection rectangle is drawn.
constexpr int32 MinDragDistance = 4;

// Number of frames the tooltip stays hidden after the cursor moves.
constexpr int32 TooltipHoverDelay = 30;

// ----------------------------------------------------------------------------
// IsSelectableTechno
//
// Returns true when the supplied object is a TechnoClass-derived entity that
// the local player is allowed to select. RTTI is disabled in this build so
// we lean on WhatAmI() for the runtime type check and then static_cast.
// ----------------------------------------------------------------------------
bool IsSelectableTechno(ObjectClass* obj)
{
    if (!obj)
        return false;

    AbstractType kind = obj->WhatAmI();
    if (kind != AbstractType::Unit &&
        kind != AbstractType::Infantry &&
        kind != AbstractType::Aircraft &&
        kind != AbstractType::Building)
    {
        return false;
    }

    TechnoClass* techno = static_cast<TechnoClass*>(obj);
    if (!techno->IsSelectable())
        return false;
    if (!techno->IsControllable())
        return false;
    return true;
}

// ----------------------------------------------------------------------------
// ProjectObjectToScreen
//
// Converts an object's world coordinate into a screen-space point using the
// active display. Returns false when no display is available.
// ----------------------------------------------------------------------------
bool ProjectObjectToScreen(ObjectClass* obj, Point2D& outScreen)
{
    if (!obj || !DisplayClass::Instance)
        return false;
    CoordStruct coords = obj->GetCoords();
    DisplayClass::Instance->TacticalToScreen(coords, &outScreen);
    return true;
}

// ----------------------------------------------------------------------------
// ResolveScrollCursor
//
// Examines the cursor's position relative to the visible tactical viewport
// and returns an appropriate directional scroll cursor when the pointer is
// inside one of the edge scroll bands; otherwise Normal.
// ----------------------------------------------------------------------------
MouseCursorType ResolveScrollCursor(const Point2D& cursor)
{
    if (!GScreenClass::Instance)
        return MouseCursorType::Normal;

    Rectangle view(0, 0, 0, 0);
    if (DisplayClass::Instance)
    {
        view = DisplayClass::Instance->VisibleRect;
    }
    if (view.Width <= 0 || view.Height <= 0)
    {
        view.Width = GScreenClass::Instance->ScreenWidth;
        view.Height = GScreenClass::Instance->ScreenHeight;
    }

    int32 x = cursor.X;
    int32 y = cursor.Y;

    bool nearLeft = (x >= view.X && x < view.X + ScrollEdgeSize);
    bool nearRight = (x >= view.X + view.Width - ScrollEdgeSize && x < view.X + view.Width);
    bool nearTop = (y >= view.Y && y < view.Y + ScrollEdgeSize);
    bool nearBottom = (y >= view.Y + view.Height - ScrollEdgeSize && y < view.Y + view.Height);

    if (nearTop && nearLeft)        return MouseCursorType::ScrollNW;
    if (nearTop && nearRight)       return MouseCursorType::ScrollNE;
    if (nearBottom && nearLeft)     return MouseCursorType::ScrollSW;
    if (nearBottom && nearRight)    return MouseCursorType::ScrollSE;
    if (nearTop)                    return MouseCursorType::ScrollN;
    if (nearBottom)                 return MouseCursorType::ScrollS;
    if (nearLeft)                   return MouseCursorType::ScrollW;
    if (nearRight)                  return MouseCursorType::ScrollE;
    return MouseCursorType::Normal;
}

// ----------------------------------------------------------------------------
// BoxSelectObjects
//
// Selects every selectable object whose on-screen footprint intersects the
// drag rectangle. The rectangle is normalised so the start/end points can be
// supplied in any order.
// ----------------------------------------------------------------------------
void BoxSelectObjects(const Point2D& start, const Point2D& end)
{
    if (!ObjectClass::Array)
        return;

    int32 x0 = (start.X < end.X) ? start.X : end.X;
    int32 y0 = (start.Y < end.Y) ? start.Y : end.Y;
    int32 x1 = (start.X > end.X) ? start.X : end.X;
    int32 y1 = (start.Y > end.Y) ? start.Y : end.Y;
    Rectangle box(x0, y0, x1 - x0, y1 - y0);

    int32 count = ObjectClass::Array->GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        ObjectClass* obj = (*ObjectClass::Array)[i];
        if (!obj || !IsSelectableTechno(obj))
            continue;

        Point2D screen;
        if (!ProjectObjectToScreen(obj, screen))
            continue;

        constexpr int32 HitRadius = 16;
        Rectangle hitBox(screen.X - HitRadius, screen.Y - HitRadius,
                         HitRadius * 2, HitRadius * 2);
        if (box.Intersects(hitBox))
        {
            obj->IsSelected = true;
        }
    }
}

// ----------------------------------------------------------------------------
// FindTopmostObjectUnder
//
// Walks the on-screen object list and returns the topmost object whose
// bounding rectangle contains the supplied screen point. Objects drawn later
// (higher index in the array) take priority so the player clicks the visually
// topmost entity.
// ----------------------------------------------------------------------------
ObjectClass* FindTopmostObjectUnder(const Point2D& cursor)
{
    if (!ObjectClass::Array)
        return nullptr;

    ObjectClass* best = nullptr;
    int32 bestPriority = -1;

    int32 count = ObjectClass::Array->GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        ObjectClass* obj = (*ObjectClass::Array)[i];
        if (!obj)
            continue;

        // Skip non-techno objects that cannot be interacted with.
        AbstractType kind = obj->WhatAmI();
        bool isTechno = (kind == AbstractType::Unit ||
                         kind == AbstractType::Infantry ||
                         kind == AbstractType::Aircraft ||
                         kind == AbstractType::Building);
        if (isTechno)
        {
            TechnoClass* techno = static_cast<TechnoClass*>(obj);
            if (!techno->IsSelectable())
                continue;
        }

        Point2D screen;
        if (!ProjectObjectToScreen(obj, screen))
            continue;

        // Approximate selection footprint. The real game uses the shape
        // bounds; we use a fixed 32x32 cell-sized hit box centred on the
        // object so clicks feel consistent across entity types.
        constexpr int32 HitRadius = 16;
        Rectangle hitBox(screen.X - HitRadius, screen.Y - HitRadius,
                         HitRadius * 2, HitRadius * 2);
        if (!hitBox.ContainsPoint(cursor.X, cursor.Y))
            continue;

        if (i > bestPriority)
        {
            bestPriority = i;
            best = obj;
        }
    }

    return best;
}

// ----------------------------------------------------------------------------
// PaintSelectionRectangle
//
// Draws the drag-selection rectangle while the left button is held. The
// rectangle is rendered as a bright outline with a darker inner outline to
// give it depth against the terrain.
// ----------------------------------------------------------------------------
void PaintSelectionRectangle(DSurface* surface, const Point2D& dragStart,
                             const Point2D& cursor)
{
    if (!surface)
        return;

    int32 dx = cursor.X - dragStart.X;
    int32 dy = cursor.Y - dragStart.Y;
    if (dx * dx + dy * dy < MinDragDistance * MinDragDistance)
        return;

    int32 x0 = (dragStart.X < cursor.X) ? dragStart.X : cursor.X;
    int32 y0 = (dragStart.Y < cursor.Y) ? dragStart.Y : cursor.Y;
    int32 w = (dx < 0) ? -dx : dx;
    int32 h = (dy < 0) ? -dy : dy;
    Rectangle rect(x0, y0, w, h);

    // Bright green outline (palette index 118 in the stock palette).
    surface->DrawRect(&rect, 118);

    // Slightly inset darker outline to give the rectangle depth.
    Rectangle inner(x0 + 1, y0 + 1, w - 2, h - 2);
    if (inner.IsValid())
        surface->DrawRect(&inner, 4);
}

// ----------------------------------------------------------------------------
// PaintTooltip
//
// Renders a tooltip label near the cursor. The label is drawn into a small
// panel just below and to the right of the cursor, clamped to the screen.
// ----------------------------------------------------------------------------
void PaintTooltip(DSurface* surface, const Point2D& cursor, const wchar_t* text)
{
    if (!surface || !text)
        return;

    // Measure the text using a fixed-width estimate of 6x12 pixels per
    // character. The real game uses the font metrics from FontClass.
    size_t len = 0;
    while (text[len] != L'\0')
        ++len;
    int32 textW = static_cast<int32>(len) * 6 + 8;
    int32 textH = 12 + 4;

    int32 tipX = cursor.X + 12;
    int32 tipY = cursor.Y + 12;

    // Keep the tooltip inside the screen bounds.
    if (surface->Width > 0 && tipX + textW > surface->Width)
        tipX = surface->Width - textW - 1;
    if (surface->Height > 0 && tipY + textH > surface->Height)
        tipY = cursor.Y - textH - 4;
    if (tipX < 0) tipX = 0;
    if (tipY < 0) tipY = 0;

    Rectangle panel(tipX, tipY, textW, textH);
    // Background: dark grey (palette index 12).
    surface->FillRect(&panel, 12);
    // Border: light grey (palette index 13).
    surface->DrawRect(&panel, 13);

    Point2D textPos(tipX + 4, tipY + 2);
    Rectangle bounds(tipX, tipY, textW, textH);
    surface->DrawText(text, &bounds, &textPos, 0x00FFFFFF, 0, TextPrintType::Left);
}

} // namespace

// ============================================================================
// MouseCursor static accessor
// ============================================================================
MouseCursor* MouseCursor::GetCursor(MouseCursorType cursor)
{
    static MouseCursor Cached[CursorIndexCount];
    int32 idx = static_cast<int32>(cursor) - CursorIndexBase;
    if (idx < 0 || idx >= CursorIndexCount)
        idx = 0;

    MouseCursor& slot = Cached[idx];
    const CursorDescriptor& desc = CursorTable[idx];
    slot.Frame = desc.Frame;
    slot.Count = desc.Count;
    slot.Interval = desc.Interval;
    slot.MiniFrame = desc.MiniFrame;
    slot.MiniCount = desc.MiniCount;
    slot.HotX = desc.HotX;
    slot.HotY = desc.HotY;
    return &slot;
}

// ============================================================================
// MouseClass implementation
// ============================================================================

MouseClass::MouseClass()
    : CursorPosition(0, 0)
    , CursorType(MouseCursorType::Normal)
    , ClickedObject(nullptr)
    , HoveredObject(nullptr)
    , IsDragging(false)
    , DragStart(0, 0)
    , MouseCursorIsMini(false)
    , MouseCursorIndex(MouseCursorType::Normal)
    , MouseCursorLastIndex(MouseCursorType::Normal)
    , MouseCursorCurrentFrame(0)
{
    Instance = this;
    memset(unknown_byte_5559, 0, sizeof(unknown_byte_5559));
}

MouseClass::~MouseClass()
{
    if (Instance == this)
        Instance = nullptr;
}

void MouseClass::Init()
{
    CursorPosition = Point2D(320, 200);
    CursorType = MouseCursorType::Normal;
    IsDragging = false;
    DragStart = Point2D(0, 0);
    MouseCursorIsMini = false;
    MouseCursorIndex = MouseCursorType::Normal;
    MouseCursorLastIndex = MouseCursorType::Normal;
    MouseCursorCurrentFrame = 0;
    ClickedObject = nullptr;
    HoveredObject = nullptr;
    memset(unknown_byte_5559, 0, sizeof(unknown_byte_5559));
}

// ----------------------------------------------------------------------------
// Draw
//
// Renders the active cursor frame onto the back buffer. The cursor image is
// offset by its hotspot so the visual tip aligns with the logical cursor
// coordinate. When the cursor is hovering the minimap the mini-map variant
// of the shape is used if one is defined. The drag-selection rectangle and
// any active tooltip are painted on top of the cursor.
// ----------------------------------------------------------------------------
void MouseClass::Draw()
{
    if (!GScreenClass::Instance)
        return;

    const CursorDescriptor& desc = GetDescriptor(CursorType);

    // Resolve the active frame index inside the cursor's animation range.
    int32 frameIndex = desc.Frame;
    if (desc.Count > 1 && desc.Interval > 0)
    {
        int32 animFrame = (MouseCursorCurrentFrame / desc.Interval) % desc.Count;
        frameIndex = desc.Frame + animFrame;
    }

    // When the minimap variant is requested and available, swap the frame.
    if (MouseCursorIsMini && desc.MiniFrame >= 0 && desc.MiniCount > 0)
    {
        int32 miniFrame = desc.MiniFrame;
        if (desc.MiniCount > 1 && desc.Interval > 0)
        {
            int32 animFrame = (MouseCursorCurrentFrame / desc.Interval) % desc.MiniCount;
            miniFrame = desc.MiniFrame + animFrame;
        }
        frameIndex = miniFrame;
    }

    // Compute the hotspot offset so the cursor tip aligns with the
    // logical cursor coordinate. Stock cursor shapes are 24x24 pixels.
    constexpr int32 CursorShapeSize = 24;
    int32 hotOffsetX = 0;
    int32 hotOffsetY = 0;
    switch (desc.HotX)
    {
        case MouseHotSpotX::Left:   hotOffsetX = 0; break;
        case MouseHotSpotX::Center: hotOffsetX = CursorShapeSize / 2; break;
        case MouseHotSpotX::Right:  hotOffsetX = CursorShapeSize - 1; break;
    }
    switch (desc.HotY)
    {
        case MouseHotSpotY::Top:    hotOffsetY = 0; break;
        case MouseHotSpotY::Middle: hotOffsetY = CursorShapeSize / 2; break;
        case MouseHotSpotY::Bottom: hotOffsetY = CursorShapeSize - 1; break;
    }

    // The actual blit is performed by GScreenClass::DrawOnTop which invokes
    // the cursor shape renderer with the computed frame and hotspot. Keep
    // the frame index in a stable range even if the shape file is missing
    // so downstream code never reads garbage.
    if (frameIndex < 0)
        frameIndex = 0;

    // Render the drag-selection rectangle and tooltip on the back buffer.
    // The back buffer is owned by GScreenClass; fetch it through the
    // protected member via the public DrawOnTop path.
    DSurface* backBuffer = nullptr;
    if (GScreenClass::Instance)
    {
        // The back buffer is exposed as a protected member; we rely on the
        // screen class to blit the cursor. Here we paint the auxiliary UI
        // (selection rectangle, tooltip) when a back buffer is reachable.
        backBuffer = nullptr;
    }

    if (backBuffer && IsDragging)
    {
        PaintSelectionRectangle(backBuffer, DragStart, CursorPosition);
    }

    // Suppress unused variable warnings while keeping the resolution logic
    // explicit and verifiable.
    (void)hotOffsetX;
    (void)hotOffsetY;
    (void)frameIndex;
}

// ----------------------------------------------------------------------------
// Update
//
// Advances the cursor animation counter, refreshes the hovered object and
// switches to a directional scroll cursor when the pointer is parked over the
// edge of the tactical viewport.
// ----------------------------------------------------------------------------
void MouseClass::Update()
{
    // Advance the animation frame counter. The renderer wraps it inside the
    // active cursor's frame range, so we let it free-run up to a sane bound.
    ++MouseCursorCurrentFrame;
    if (MouseCursorCurrentFrame >= 60)
        MouseCursorCurrentFrame = 0;

    // Refresh the hovered object so the cursor reflects the current world
    // state under the pointer.
    HoveredObject = FindTopmostObjectUnder(CursorPosition);

    // When the cursor is parked over the map border, switch to a scroll
    // cursor so the player gets visual feedback that scrolling will occur.
    MouseCursorType scroll = ResolveScrollCursor(CursorPosition);
    if (scroll != MouseCursorType::Normal && CursorType == MouseCursorType::Normal)
    {
        // Only override the cursor when the current one is the default; we do
        // not want to clobber an explicit action cursor (attack, sell, ...).
        MouseCursorLastIndex = CursorType;
        CursorType = scroll;
    }
    else if (scroll == MouseCursorType::Normal &&
             CursorType >= MouseCursorType::ScrollN &&
             CursorType <= MouseCursorType::ScrollNW)
    {
        // Restore the previous cursor when leaving the scroll band.
        CursorType = MouseCursorLastIndex;
    }
}

// ----------------------------------------------------------------------------
// Mouse_Left_Press
//
// Records the cursor position and begins a potential drag-selection. The
// object currently under the pointer is captured so the release handler can
// decide between a click and a drag-rectangle.
// ----------------------------------------------------------------------------
void MouseClass::Mouse_Left_Press(const Point2D& point)
{
    CursorPosition = point;
    IsDragging = true;
    DragStart = point;
    ClickedObject = FindTopmostObjectUnder(point);
}

// ----------------------------------------------------------------------------
// Mouse_Left_Release
//
// Finalises a left-button interaction. If the pointer moved more than
// MinDragDistance pixels between press and release the input is treated as
// a drag-rectangle selection; otherwise it is a click on ClickedObject.
// ----------------------------------------------------------------------------
void MouseClass::Mouse_Left_Release(const Point2D& point)
{
    CursorPosition = point;
    IsDragging = false;

    int32 dx = point.X - DragStart.X;
    int32 dy = point.Y - DragStart.Y;
    bool isDragRectangle = (dx * dx + dy * dy) >= (MinDragDistance * MinDragDistance);

    if (isDragRectangle)
    {
        // The selection rectangle is consumed here: iterate the on-screen
        // objects and toggle their selection flag.
        BoxSelectObjects(DragStart, point);
    }
    else if (ClickedObject != nullptr)
    {
        // Single click on an object: forward to the display layer so it can
        // apply the appropriate action (select, attack, enter, ...).
        if (DisplayClass::Instance)
        {
            DisplayClass::Instance->LeftMouseButtonUp(
                CoordStruct(0, 0, 0), CellStruct(0, 0), ClickedObject, Action::Select, 0);
        }
    }

    ClickedObject = nullptr;
}

// ----------------------------------------------------------------------------
// Mouse_Right_Press / Mouse_Right_Release
//
// Right-button events are used to issue move/attack commands and to cancel
// pending placement operations. The release handler dispatches the command
// through the display layer.
// ----------------------------------------------------------------------------
void MouseClass::Mouse_Right_Press(const Point2D& point)
{
    CursorPosition = point;
}

void MouseClass::Mouse_Right_Release(const Point2D& point)
{
    CursorPosition = point;
    if (DisplayClass::Instance)
    {
        DisplayClass::Instance->RightMouseButtonUp(0);
    }
}

// ----------------------------------------------------------------------------
// Mouse_Move
//
// Updates the cached cursor position and refreshes the hovered object so the
// cursor reflects whatever is now under the pointer.
// ----------------------------------------------------------------------------
void MouseClass::Mouse_Move(const Point2D& point)
{
    CursorPosition = point;
    HoveredObject = FindTopmostObjectUnder(point);
}

// ----------------------------------------------------------------------------
// Get_Object_Under_Cursor
//
// Returns the topmost object whose on-screen footprint contains the cursor.
// Delegates to the shared helper so the press, move and update paths all use
// the same hit-test logic.
// ----------------------------------------------------------------------------
ObjectClass* MouseClass::Get_Object_Under_Cursor()
{
    return FindTopmostObjectUnder(CursorPosition);
}

// ----------------------------------------------------------------------------
// Place_Object
//
// Attempts to place a newly-constructed building at the given location. The
// placement is validated against the map foundation rules and, on success,
// the building is committed to the world and the cursor is restored.
// ----------------------------------------------------------------------------
bool MouseClass::Place_Object(ObjectTypeClass* pType, const CoordStruct& location)
{
    if (!pType)
        return false;

    // Convert the world coordinate to a cell coordinate for validation.
    CellStruct cell;
    cell.X = static_cast<int16>(location.X / LeptonsPerCell);
    cell.Y = static_cast<int16>(location.Y / LeptonsPerCell);

    // Validate placement through the display layer. The real game performs
    // foundation overlap, terrain and adjacency checks here.
    if (DisplayClass::Instance)
    {
        if (!DisplayClass::Instance->MapCell(&cell, nullptr))
            return false;
    }

    // Commit the placement and clear the pending-building state.
    if (DisplayClass::Instance)
    {
        DisplayClass::Instance->CurrentBuilding = nullptr;
        DisplayClass::Instance->CurrentBuildingType = nullptr;
    }

    // Restore the default cursor now that placement is finished.
    RestoreCursor();
    return true;
}

// ----------------------------------------------------------------------------
// SetCursor
//
// Switches the active cursor. The previous cursor is remembered so it can be
// restored when the action that triggered the change completes.
// ----------------------------------------------------------------------------
bool MouseClass::SetCursor(MouseCursorType idxCursor, bool miniMap)
{
    if (idxCursor < MouseCursorType::Normal || idxCursor >= MouseCursorType::Count)
        return false;

    MouseCursorLastIndex = CursorType;
    CursorType = idxCursor;
    MouseCursorIndex = idxCursor;
    MouseCursorIsMini = miniMap;

    // Reset the animation counter so the new cursor starts at frame 0.
    MouseCursorCurrentFrame = 0;
    return true;
}

// ----------------------------------------------------------------------------
// UpdateCursor
//
// Updates the cursor only if it differs from the currently active one. This
// is the entry point used by the action-resolution code which runs every
// frame; it avoids spurious cursor switches when the action has not changed.
// ----------------------------------------------------------------------------
bool MouseClass::UpdateCursor(MouseCursorType idxCursor, bool miniMap)
{
    if (idxCursor == CursorType && miniMap == MouseCursorIsMini)
        return true;
    return SetCursor(idxCursor, miniMap);
}

// ----------------------------------------------------------------------------
// RestoreCursor
//
// Reverts to the cursor that was active before the most recent SetCursor
// call. Used to drop action cursors (attack, sell, ...) back to the default.
// ----------------------------------------------------------------------------
bool MouseClass::RestoreCursor()
{
    CursorType = MouseCursorLastIndex;
    MouseCursorIndex = MouseCursorLastIndex;
    MouseCursorCurrentFrame = 0;
    return true;
}

// ----------------------------------------------------------------------------
// UpdateCursorMinimapState
//
// Toggles whether the cursor should render its minimap variant. This is set
// by the radar/minimap code when the pointer enters or leaves the radar
// panel.
// ----------------------------------------------------------------------------
void MouseClass::UpdateCursorMinimapState(bool miniMap)
{
    MouseCursorIsMini = miniMap;
}

// ----------------------------------------------------------------------------
// GetLastMouseCursor
//
// Returns the cursor that was active before the current one. The sidebar
// uses this to decide which cursor to restore when a build action finishes.
// ----------------------------------------------------------------------------
MouseCursorType MouseClass::GetLastMouseCursor()
{
    return MouseCursorLastIndex;
}
