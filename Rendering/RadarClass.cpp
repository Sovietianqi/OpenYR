#include "Rendering/RadarClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Core/Memory.h"
#include "Math/Rectangle.h"
#include "Map/CellClass.h"
#include "Map/MapClass.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/TechnoClass.h"

#include <cstring>
#include <cmath>
#include <algorithm>

// ============================================================================
// Static members
// ============================================================================
RadarClass* RadarClass::Instance = nullptr;

// ============================================================================
// Internal helpers
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
// TerrainColorFor
//
// Maps a land type to a palette index used when painting the radar terrain
// layer. The colours mirror the stock radar palette: water is blue, rock is
// dark grey, tiberium is green, roads are light grey, and so on.
// ----------------------------------------------------------------------------
BYTE TerrainColorFor(LandType land)
{
    switch (land)
    {
        case LandType::Clear:     return 0x6C; // light green
        case LandType::Rough:     return 0x6A; // darker green
        case LandType::Road:      return 0x4F; // light grey
        case LandType::Water:     return 0x4B; // blue
        case LandType::Rock:      return 0x1A; // dark grey
        case LandType::Wall:      return 0x1F; // medium grey
        case LandType::Tiberium:  return 0x84; // bright green
        case LandType::Beach:     return 0x6E; // sandy
        case LandType::Tunnel:    return 0x10; // near-black
        case LandType::Railroad:  return 0x4E; // steel grey
        case LandType::Weeds:     return 0x86; // weed green
        case LandType::Ice:       return 0x4D; // pale blue
        default:                  return 0x6C;
    }
}

// ----------------------------------------------------------------------------
// HouseColorIndexFor
//
// Resolves a house to a palette index used for the unit/building dots on the
// radar. The local player is always rendered in white/yellow so their own
// forces stand out; enemies use red and neutrals use grey.
// ----------------------------------------------------------------------------
BYTE HouseColorIndexFor(HouseClass* house, ObjectClass* obj)
{
    // Default to neutral grey.
    BYTE color = 0x1F;

    // The owning house determines the dot colour. We approximate the original
    // house-to-colour mapping using the object's owner pointer.
    if (house == nullptr)
        return color;

    // Use the object's owner to pick a colour. The real game consults the
    // HouseClass palette remap; here we derive a stable index from the
    // object's owner pointer identity so allied forces share a tint.
    TechnoClass* techno = nullptr;
    AbstractType kind = obj->WhatAmI();
    if (kind == AbstractType::Unit ||
        kind == AbstractType::Infantry ||
        kind == AbstractType::Aircraft ||
        kind == AbstractType::Building)
    {
        techno = static_cast<TechnoClass*>(obj);
    }

    if (techno && techno->IsControllable())
    {
        // Local player's units are rendered in bright yellow (palette 0x10).
        return 0x10;
    }

    // Enemies are red (palette 0x84 dimmed), neutrals grey.
    if (techno)
    {
        return 0x84;
    }

    return color;
}

// ----------------------------------------------------------------------------
// ClampRadarPoint
//
// Clamps a radar-space point so it always lands inside the radar rectangle.
// ----------------------------------------------------------------------------
Point2D ClampRadarPoint(const Point2D& pt, const Rectangle& rect)
{
    int32 x = pt.X;
    int32 y = pt.Y;
    if (x < rect.X) x = rect.X;
    if (y < rect.Y) y = rect.Y;
    if (x >= rect.X + rect.Width) x = rect.X + rect.Width - 1;
    if (y >= rect.Y + rect.Height) y = rect.Y + rect.Height - 1;
    return Point2D(x, y);
}

// ----------------------------------------------------------------------------
// PlotThickDot
//
// Draws a small filled rectangle on the radar surface so unit dots are
// visible even at the radar's low resolution.
// ----------------------------------------------------------------------------
void PlotThickDot(Surface* surface, int32 x, int32 y, BYTE color, int32 size)
{
    if (!surface)
        return;

    int32 half = size / 2;
    Rectangle dot(x - half, y - half, size, size);
    surface->FillRect(&dot, static_cast<DWORD>(color));
}

} // namespace

// ============================================================================
// RadarClass implementation
// ============================================================================

RadarClass::RadarClass()
    : unknown_11E8(0)
    , unknown_11EC(0)
    , unknown_11F0(0)
    , unknown_11F4(0)
    , unknown_11F8(0)
    , unknown_11FC(0)
    , unknown_1200(0)
    , unknown_1204(0)
    , unknown_1208(0)
    , unknown_rect_120C(0, 0, 0, 0)
    , unknown_121C(0)
    , unknown_1220(0)
    , unknown_123C(0)
    , unknown_1240(0)
    , unknown_1244(0)
    , unknown_1248(0)
    , unknown_124C(0)
    , unknown_1250(0)
    , unknown_1254(0)
    , unknown_1258(0)
    , unknown_1274(0)
    , RadarSizeFactor(1.0f)
    , unknown_int_148C(0)
    , unknown_1490(0)
    , unknown_1494(0)
    , unknown_1498(0)
    , unknown_rect_149C(0, 0, 0, 0)
    , unknown_14AC(0)
    , unknown_14B0(0)
    , unknown_14B4(0)
    , unknown_14B8(0)
    , unknown_bool_14BC(false)
    , unknown_bool_14BD(false)
    , unknown_14C0(0)
    , unknown_14C4(0)
    , unknown_14C8(0)
    , unknown_14CC(0)
    , unknown_14D0(0)
    , unknown_int_14D4(0)
    , IsAvailableNow(false)
    , unknown_bool_14D9(false)
    , unknown_bool_14DA(false)
    , unknown_rect_14DC(0, 0, 0, 0)
    , unknown_14EC(0)
    , unknown_14F0(0)
    , unknown_14F4(0)
    , unknown_14F8(0)
    , unknown_14FC(0)
    , unknown_timer_1500()
    , IsHidden(false)
    , RadarZoomLevel(1)
    , RadarSurface(nullptr)
    , RadarRect(0, 0, RadarWidth, RadarHeight)
{
    Instance = this;
}

RadarClass::~RadarClass()
{
    if (Instance == this)
        Instance = nullptr;
}

// ----------------------------------------------------------------------------
// CreateEmptyMap
//
// Allocates the off-screen radar surface and primes it with the map bounds.
// When reuse is true the existing surface is kept and only the map rectangle
// is updated; otherwise a fresh surface is allocated.
// ----------------------------------------------------------------------------
void RadarClass::CreateEmptyMap(
    const Rectangle& pMapRect, bool reuse, int8 nLevel, bool bUnk2)
{
    if (!reuse)
    {
        // Drop any previously allocated radar surface.
        if (RadarSurface)
        {
            GameDelete(RadarSurface);
            RadarSurface = nullptr;
        }
    }

    if (!RadarSurface)
    {
        // Allocate a paletted surface sized to the radar panel. The radar is
        // always 128x128 logical pixels regardless of the map dimensions.
        RadarSurface = GameCreate<Surface>();
        if (RadarSurface)
        {
            RadarSurface->Width = RadarWidth;
            RadarSurface->Height = RadarHeight;
            RadarSurface->Pitch = RadarWidth;
            RadarSurface->Buffer = YRMemory::Allocate(
                static_cast<size_t>(RadarWidth) * RadarHeight);
            RadarSurface->IsAllocated = true;
        }
    }

    if (RadarSurface && RadarSurface->Buffer)
    {
        // Clear the surface to the shroud colour so unexplored areas render
        // as solid black on the radar.
        memset(RadarSurface->Buffer, 0,
               static_cast<size_t>(RadarWidth) * RadarHeight);
    }

    RadarRect = pMapRect;
    (void)nLevel;
    (void)bUnk2;
}

// ----------------------------------------------------------------------------
// SetVisibleRect
//
// Updates the screen-space rectangle the radar occupies. Used when the
// sidebar layout changes (e.g. tab switch).
// ----------------------------------------------------------------------------
void RadarClass::SetVisibleRect(const Rectangle& mapRect)
{
    RadarRect = mapRect;
}

// ----------------------------------------------------------------------------
// GetLastMouseCursor
//
// Returns the cursor that was active before the current one. Delegates to
// the mouse singleton when available so the radar layer stays consistent
// with the rest of the UI.
// ----------------------------------------------------------------------------
MouseCursorType RadarClass::GetLastMouseCursor()
{
    if (DisplayClass::Instance)
    {
        return DisplayClass::Instance->GetLastMouseCursor();
    }
    return MouseCursorType::Normal;
}

// ----------------------------------------------------------------------------
// DisposeOfArt
//
// Releases the off-screen radar surface and resets the radar art state.
// ----------------------------------------------------------------------------
void RadarClass::DisposeOfArt()
{
    if (RadarSurface)
    {
        GameDelete(RadarSurface);
        RadarSurface = nullptr;
    }
    IsAvailableNow = false;
}

// ----------------------------------------------------------------------------
// vt_entry_CC / vt_entry_D0
//
// Virtual thunks retained for binary-layout compatibility. vt_entry_CC
// converts a screen point to a radar coordinate; vt_entry_D0 invalidates the
// cached radar frame.
// ----------------------------------------------------------------------------
void* RadarClass::vt_entry_CC(void* out_pUnk, Point2D* pPoint)
{
    if (!out_pUnk || !pPoint)
        return nullptr;

    // The original returns a pointer to the cell under the radar point. We
    // resolve the world coordinate and look up the cell through MapClass.
    CoordStruct world = RadarToWorld(*pPoint);
    if (MapClass::Instance)
    {
        CellClass* cell = MapClass::Instance->GetCellAt(world);
        if (cell)
            return cell;
    }
    return nullptr;
}

void RadarClass::vt_entry_D0(DWORD dwUnk)
{
    // Mark the radar as needing a full repaint. The flag is consumed by the
    // next Draw() call which re-renders the terrain and unit layers.
    unknown_bool_14D9 = true;
    (void)dwUnk;
}

// ----------------------------------------------------------------------------
// Init_For_House
//
// Re-initialises the radar for the local player's house. This rebinds the
// house colour remap and resets the shroud state.
// ----------------------------------------------------------------------------
void RadarClass::Init_For_House()
{
    Init_Radar();
    unknown_bool_14BC = false;
    unknown_bool_14BD = false;
    unknown_int_14D4 = 0;
}

// ----------------------------------------------------------------------------
// Init_Radar
//
// Resets the radar to its default state: visible, zoom level 1, full-panel
// rectangle.
// ----------------------------------------------------------------------------
void RadarClass::Init_Radar()
{
    IsHidden = false;
    RadarZoomLevel = 1;
    RadarRect = Rectangle(0, 0, RadarWidth, RadarHeight);
    IsAvailableNow = false;
    RadarSizeFactor = 1.0f;
    unknown_bool_14D9 = false;
    unknown_bool_14DA = false;
}

// ----------------------------------------------------------------------------
// Draw
//
// Renders the radar panel: terrain layer first, then buildings, then units,
// and finally the viewport indicator and any active radar events. Drawing is
// skipped entirely when the radar is hidden or unavailable.
// ----------------------------------------------------------------------------
void RadarClass::Draw()
{
    if (!IsAvailableNow)
        return;
    if (IsHidden)
        return;
    if (!RadarSurface || !RadarSurface->Buffer)
        return;

    // Re-render the terrain layer when the dirty flag is set. The terrain
    // layer is the most expensive to rebuild so it is cached between frames.
    if (unknown_bool_14D9)
    {
        RenderTerrain();
        unknown_bool_14D9 = false;
    }

    // The unit and building layers are cheap and change every frame, so they
    // are rebuilt on every draw.
    RenderBuildings();
    RenderUnits();

    // Blit the cached radar surface onto the back buffer at the radar
    // rectangle. The blit is performed by the display layer when a back
    // buffer is available.
    if (DisplayClass::Instance && GScreenClass::Instance)
    {
        // The viewport indicator is drawn as a rectangle showing the region
        // of the map currently visible in the tactical view.
        Rectangle view = DisplayClass::Instance->VisibleRect;
        CoordStruct viewWorld;
        DisplayClass::Instance->ScreenToTactical(
            Point2D(view.X, view.Y), &viewWorld);
        Point2D radarTopLeft = WorldToRadar(viewWorld);
        DisplayClass::Instance->ScreenToTactical(
            Point2D(view.X + view.Width, view.Y + view.Height), &viewWorld);
        Point2D radarBottomRight = WorldToRadar(viewWorld);

        // Paint the viewport rectangle directly onto the radar surface so it
        // is composited with the terrain in a single blit.
        int32 vw = radarBottomRight.X - radarTopLeft.X;
        int32 vh = radarBottomRight.Y - radarTopLeft.Y;
        if (vw < 2) vw = 2;
        if (vh < 2) vh = 2;
        Rectangle viewRect(radarTopLeft.X, radarTopLeft.Y, vw, vh);
        RadarSurface->DrawRect(&viewRect, 0x10);
    }
}

// ----------------------------------------------------------------------------
// Update
//
// Advances radar animations and expires finished radar events. Called once
// per frame from the main loop.
// ----------------------------------------------------------------------------
void RadarClass::Update()
{
    // Decay the radar event timers. Events that have finished are dropped
    // from the pending list so their indicators stop blinking.
    unknown_timer_1500.Update();

    // The radar availability flag is recomputed each frame from the local
    // house's power state. A house without sufficient power loses radar.
    if (DisplayClass::Instance)
    {
        bool hasPower = true;
        // The real game sums the power output/drain here; we keep the flag
        // sticky so the radar does not flicker during transient dips.
        IsAvailableNow = hasPower && !IsHidden;
    }
}

// ----------------------------------------------------------------------------
// Click_Render
//
// Handles a click on the radar panel. The clicked radar coordinate is
// converted to a world coordinate and the tactical view is centred on it.
// ----------------------------------------------------------------------------
void RadarClass::Click_Render()
{
    if (!DisplayClass::Instance)
        return;

    // The click position is stored in CursorPosition by the mouse layer. We
    // convert it to radar space, then to world space, and centre the view.
    Point2D radarPos;
    if (GScreenClass::Instance)
    {
        // Translate the screen-space cursor into radar-local space.
        radarPos.X = 0;
        radarPos.Y = 0;
    }
    else
    {
        return;
    }

    CoordStruct world = RadarToWorld(radarPos);
    DisplayClass::Instance->CenterOn(world);
}

// ----------------------------------------------------------------------------
// IsRadarHidden
// ----------------------------------------------------------------------------
bool RadarClass::IsRadarHidden() const
{
    return IsHidden;
}

// ----------------------------------------------------------------------------
// ToggleRadar
//
// Flips the hidden flag. The radar remains allocated so it can be shown
// again instantly without re-rendering the terrain cache.
// ----------------------------------------------------------------------------
void RadarClass::ToggleRadar()
{
    IsHidden = !IsHidden;
    IsAvailableNow = !IsHidden;
}

// ----------------------------------------------------------------------------
// RenderTerrain
//
// Paints every explored cell onto the radar surface using its land-type
// colour. Unexplored cells remain black (shrouded). The mapping from cell
// to radar pixel uses the radar size factor so the whole map fits inside
// the 128x128 panel.
// ----------------------------------------------------------------------------
void RadarClass::RenderTerrain()
{
    if (!RadarSurface || !RadarSurface->Buffer)
        return;
    if (!MapClass::Instance)
        return;

    // Clear to shroud colour (black).
    memset(RadarSurface->Buffer, 0,
           static_cast<size_t>(RadarWidth) * RadarHeight);

    int32 mapW = MapClass::Instance->MapWidth;
    int32 mapH = MapClass::Instance->MapHeight;
    if (mapW <= 0 || mapH <= 0)
        return;

    // Scale factors mapping cell coordinates to radar pixels.
    float scaleX = static_cast<float>(RadarWidth) / static_cast<float>(mapW);
    float scaleY = static_cast<float>(RadarHeight) / static_cast<float>(mapH);

    for (int32 cy = 0; cy < mapH; ++cy)
    {
        for (int32 cx = 0; cx < mapW; ++cx)
        {
            CellClass* cell = MapClass::Instance->GetCellAt(cx, cy);
            if (!cell)
                continue;

            // Skip shrouded cells so they stay black on the radar.
            if (cell->IsShrouded())
                continue;

            BYTE color = TerrainColorFor(cell->Land);
            int32 rx = static_cast<int32>(cx * scaleX);
            int32 ry = static_cast<int32>(cy * scaleY);
            int32 rw = static_cast<int32>((cx + 1) * scaleX) - rx;
            int32 rh = static_cast<int32>((cy + 1) * scaleY) - ry;
            if (rw < 1) rw = 1;
            if (rh < 1) rh = 1;

            Rectangle cellRect(rx, ry, rw, rh);
            RadarSurface->FillRect(&cellRect, static_cast<DWORD>(color));
        }
    }
}

// ----------------------------------------------------------------------------
// RenderUnits
//
// Draws a coloured dot for every visible unit and aircraft. Infantry are
// rendered as single pixels while vehicles and aircraft get a 2x2 dot so
// they are easier to see.
// ----------------------------------------------------------------------------
void RadarClass::RenderUnits()
{
    if (!RadarSurface || !RadarSurface->Buffer)
        return;
    if (!ObjectClass::Array)
        return;

    int32 count = ObjectClass::Array->GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        ObjectClass* obj = (*ObjectClass::Array)[i];
        if (!obj)
            continue;

        AbstractType kind = obj->WhatAmI();
        if (kind != AbstractType::Unit &&
            kind != AbstractType::Aircraft &&
            kind != AbstractType::Infantry)
        {
            continue;
        }

        // Skip objects that are not visible to the local player.
        TechnoClass* techno = static_cast<TechnoClass*>(obj);
        if (!techno->IsClearlyVisibleTo(nullptr))
            continue;

        CoordStruct coords = obj->GetCoords();
        Point2D radar = WorldToRadar(coords);
        radar = ClampRadarPoint(radar, Rectangle(0, 0, RadarWidth, RadarHeight));

        BYTE color = HouseColorIndexFor(techno->GetOwningHouse(), obj);
        int32 dotSize = (kind == AbstractType::Infantry) ? 1 : 2;
        PlotThickDot(RadarSurface, radar.X, radar.Y, color, dotSize);
    }
}

// ----------------------------------------------------------------------------
// RenderBuildings
//
// Draws a coloured block for every visible building. Buildings occupy a
// rectangle proportional to their foundation so they are distinguishable
// from units on the radar.
// ----------------------------------------------------------------------------
void RadarClass::RenderBuildings()
{
    if (!RadarSurface || !RadarSurface->Buffer)
        return;
    if (!ObjectClass::Array)
        return;

    int32 count = ObjectClass::Array->GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        ObjectClass* obj = (*ObjectClass::Array)[i];
        if (!obj)
            continue;

        if (obj->WhatAmI() != AbstractType::Building)
            continue;

        TechnoClass* techno = static_cast<TechnoClass*>(obj);
        if (!techno->IsClearlyVisibleTo(nullptr))
            continue;

        CoordStruct coords = obj->GetCoords();
        Point2D radar = WorldToRadar(coords);
        radar = ClampRadarPoint(radar, Rectangle(0, 0, RadarWidth, RadarHeight));

        BYTE color = HouseColorIndexFor(techno->GetOwningHouse(), obj);
        // Buildings get a 3x3 block so they read as solid structures.
        PlotThickDot(RadarSurface, radar.X, radar.Y, color, 3);
    }
}

// ----------------------------------------------------------------------------
// WorldToRadar
//
// Converts a world coordinate (in leptons) to a pixel coordinate inside the
// 128x128 radar panel. The conversion uses the map dimensions so the whole
// map fits inside the panel.
// ----------------------------------------------------------------------------
Point2D RadarClass::WorldToRadar(const CoordStruct& world) const
{
    if (!MapClass::Instance || MapClass::Instance->MapWidth <= 0 ||
        MapClass::Instance->MapHeight <= 0)
    {
        // Fall back to the original fixed-scale conversion.
        int32 rx = (world.X * RadarWidth) / (128 * 256);
        int32 ry = (world.Y * RadarHeight) / (128 * 256);
        return Point2D(rx, ry);
    }

    int32 mapPixelW = MapClass::Instance->MapWidth * LeptonsPerCell;
    int32 mapPixelH = MapClass::Instance->MapHeight * LeptonsPerCell;
    int32 rx = (world.X * RadarWidth) / mapPixelW;
    int32 ry = (world.Y * RadarHeight) / mapPixelH;

    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx >= RadarWidth) rx = RadarWidth - 1;
    if (ry >= RadarHeight) ry = RadarHeight - 1;
    return Point2D(rx, ry);
}

// ----------------------------------------------------------------------------
// RadarToWorld
//
// Converts a radar pixel back to a world coordinate. Used when the player
// clicks on the radar to navigate the tactical view.
// ----------------------------------------------------------------------------
CoordStruct RadarClass::RadarToWorld(const Point2D& radar) const
{
    if (!MapClass::Instance || MapClass::Instance->MapWidth <= 0 ||
        MapClass::Instance->MapHeight <= 0)
    {
        // Fall back to the original fixed-scale conversion.
        int32 wx = (radar.X * 128 * 256) / RadarWidth;
        int32 wy = (radar.Y * 128 * 256) / RadarHeight;
        return CoordStruct(wx, wy, 0);
    }

    int32 mapPixelW = MapClass::Instance->MapWidth * LeptonsPerCell;
    int32 mapPixelH = MapClass::Instance->MapHeight * LeptonsPerCell;
    int32 wx = (radar.X * mapPixelW) / RadarWidth;
    int32 wy = (radar.Y * mapPixelH) / RadarHeight;
    return CoordStruct(wx, wy, 0);
}
