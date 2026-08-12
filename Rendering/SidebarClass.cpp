#include "Rendering/SidebarClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Core/Memory.h"
#include "Houses/HouseClass.h"
#include "Game/Externs.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ============================================================================
// SidebarClass.cpp - Sidebar UI implementation
// ============================================================================
// Manages the right-hand command panel: production tabs (Structures,
// Infantry, Vehicles, Aircraft), the build queue strips, the power bar,
// the credit counter, super-weapon buttons, scrolling, keyboard shortcuts
// and the tooltip system. The reconstruction keeps the public surface from
// SidebarClass.h unchanged while fleshing out the rendering and interaction
// logic that the original binary drives through this class.
//
// Because the header is the source of truth and cannot be modified, all
// new behaviour lives either inside the existing declared methods or in
// file-local free functions that operate on the class's public data members.
// ============================================================================

// ----------------------------------------------------------------------------
// Layout constants (match the original 800x600 sidebar geometry).
// ----------------------------------------------------------------------------
namespace
{
    constexpr int32 kSidebarOriginX       = 640;     // sidebar starts at column 640
    constexpr int32 kSidebarOriginY       = 0;
    constexpr int32 kTabStripHeight       = 24;
    constexpr int32 kPowerBarHeight       = 8;
    constexpr int32 kCreditsBarHeight     = 16;
    constexpr int32 kCameosPerColumn      = 2;       // two cameos per row
    constexpr int32 kCameoSpacingX        = 4;
    constexpr int32 kCameoSpacingY        = 4;
    constexpr int32 kMaxTooltipChars      = 0x42;
    constexpr int32 kScrollRepeatDelay    = 12;      // frames before auto-repeat
    constexpr int32 kScrollRepeatRate     = 3;       // frames between repeats
    constexpr int32 kProductionMaxBars    = 54;      // progress bar segments
    constexpr int32 kSWButtonCount        = 6;       // super-weapon button slots

    // Standard tab labels - displayed in the tab strip and tooltip lookups.
    const char* const kTabNames[SidebarClass::MaxTabs] = {
        "Structures", "Infantry", "Vehicles", "Aircraft"
    };

    // Weak pointer to the rendering surface the sidebar paints onto. Wired by
    // the game's screen manager; tests can override via SetSidebarSurface().
    // DSurface is required because text rendering (DrawText) lives on DSurface,
    // not the base Surface class.
    DSurface* g_SidebarSurface = nullptr;

    // Compute the on-screen rectangle for a given cameo slot in a strip.
    Rectangle ComputeCameoRect(const StripClass& strip, int32 slot)
    {
        if (slot < 0) return Rectangle(0, 0, 0, 0);
        int32 col = slot % kCameosPerColumn;
        int32 row = slot / kCameosPerColumn;
        int32 x = strip.Location.X + col * (SidebarClass::CameoWidth + kCameoSpacingX);
        int32 y = strip.Location.Y + kTabStripHeight +
                  (row - strip.TopRowIndex) * (SidebarClass::CameoHeight + kCameoSpacingY);
        return Rectangle(x, y, SidebarClass::CameoWidth, SidebarClass::CameoHeight);
    }

    // Decide whether a cameo slot is currently visible given the strip's
    // scroll position. Used by hit testing and rendering.
    bool IsCameoVisible(const StripClass& strip, int32 slot)
    {
        if (slot < 0 || slot >= strip.CameoCount) return false;
        int32 row = slot / kCameosPerColumn;
        int32 visibleRows = (strip.Bounds.Height - kTabStripHeight) /
                            (SidebarClass::CameoHeight + kCameoSpacingY);
        if (visibleRows < 1) visibleRows = 1;
        return row >= strip.TopRowIndex &&
               row < strip.TopRowIndex + visibleRows;
    }

    // Map an AbstractType to its canonical tab index, used both for placing
    // new cameos and for keyboard navigation.
    int32 AbstractTypeToTab(AbstractType abs)
    {
        switch (abs) {
            case AbstractType::BuildingType: return 0;
            case AbstractType::InfantryType: return 1;
            case AbstractType::UnitType:     return 2;
            case AbstractType::AircraftType: return 3;
            default: return -1;
        }
    }

    // Power-bar colour given a power ratio (produced / drain).
    // <0.0  -> red   (brownout)
    // <0.8  -> yellow (low power)
    // <1.0  -> orange (marginal)
    // >=1.0 -> green  (healthy)
    DWORD PowerBarColorForRatio(float ratio)
    {
        if (ratio < 0.0f)  return 0x0000E0u;  // bright red
        if (ratio < 0.8f)  return 0x00E0E0u;  // yellow
        if (ratio < 1.0f)  return 0x00A0E0u;  // orange
        return 0x00E000u;                     // green
    }

    // Map a scroll delta to a single-row step.
    int32 ScrollStepForDelta(int32 delta)
    {
        if (delta > 0) return 1;
        if (delta < 0) return -1;
        return 0;
    }

    int32 MaxRowForStrip(const StripClass& strip)
    {
        return strip.CameoCount > 0
            ? (strip.CameoCount - 1) / kCameosPerColumn : 0;
    }

    // ---- Rendering primitives ---------------------------------------------

    void DrawBackgroundPanel(DSurface* surf)
    {
        if (!surf) return;
        Rectangle panel(kSidebarOriginX, kSidebarOriginY,
                        SidebarClass::SidebarWidth, 600);
        surf->FillRect(&panel, 0x00000000u);
        Point2D top0(kSidebarOriginX, kSidebarOriginY);
        Point2D top1(kSidebarOriginX + SidebarClass::SidebarWidth - 1,
                     kSidebarOriginY);
        Point2D left0(kSidebarOriginX, kSidebarOriginY);
        Point2D left1(kSidebarOriginX, 600 - 1);
        surf->DrawLine(&top0, &top1, 0x00FFFFFFu);
        surf->DrawLine(&left0, &left1, 0x00FFFFFFu);
    }

    void DrawCreditsBar(DSurface* surf, int32 credits)
    {
        if (!surf) return;
        Rectangle bar(kSidebarOriginX, kSidebarOriginY,
                      SidebarClass::SidebarWidth, kCreditsBarHeight);
        surf->FillRect(&bar, 0u);
        // Render "$<credits>" - clamp to avoid overflow.
        wchar_t buf[24];
        int32 n = 0;
        buf[n++] = L'$';
        if (credits < 0) {
            buf[n++] = L'-';
            credits = -credits;
        }
        if (credits == 0) {
            buf[n++] = L'0';
        } else {
            wchar_t digits[16];
            int32 d = 0;
            while (credits > 0 && d < 15) {
                digits[d++] = L'0' + (credits % 10);
                credits /= 10;
            }
            for (int32 i = d - 1; i >= 0 && n < 23; --i) {
                buf[n++] = digits[i];
            }
        }
        buf[n] = 0;
        Point2D loc(kSidebarOriginX + 8, kSidebarOriginY + 2);
        surf->DrawText(buf, &loc, 0x00FFFFFFu);
    }

    void DrawPowerBar(DSurface* surf, int32 powerProduced, int32 powerDrained)
    {
        if (!surf) return;
        Rectangle bar(kSidebarOriginX,
                      kSidebarOriginY + kCreditsBarHeight,
                      SidebarClass::SidebarWidth, kPowerBarHeight);
        surf->FillRect(&bar, 0u);

        float ratio = 1.0f;
        if (powerDrained > 0) {
            ratio = static_cast<float>(powerProduced) /
                    static_cast<float>(powerDrained);
        }
        DWORD color = PowerBarColorForRatio(ratio);

        int32 drainW = SidebarClass::SidebarWidth;
        int32 prodW  = SidebarClass::SidebarWidth;
        int32 denom = powerProduced > powerDrained ? powerProduced : powerDrained;
        if (denom > 0) {
            drainW = (SidebarClass::SidebarWidth * powerDrained) / denom;
            prodW  = (SidebarClass::SidebarWidth * powerProduced) / denom;
            if (drainW > SidebarClass::SidebarWidth) drainW = SidebarClass::SidebarWidth;
            if (prodW  > SidebarClass::SidebarWidth) prodW  = SidebarClass::SidebarWidth;
        }
        Rectangle drainRect(bar.X, bar.Y, drainW, bar.Height);
        Rectangle prodRect(bar.X, bar.Y, prodW, bar.Height);
        surf->FillRect(&drainRect, 0x00000080u);
        surf->FillRect(&prodRect, color);
    }

    void DrawTabStrip(DSurface* surf, int32 activeTab)
    {
        if (!surf) return;
        int32 tabW = SidebarClass::SidebarWidth / SidebarClass::MaxTabs;
        for (int32 i = 0; i < SidebarClass::MaxTabs; ++i) {
            Rectangle tabRect(kSidebarOriginX + i * tabW,
                              kSidebarOriginY + kCreditsBarHeight + kPowerBarHeight,
                              tabW, kTabStripHeight);
            DWORD fillColor = (i == activeTab) ? 0x0000A0A0u : 0x00404040u;
            surf->FillRect(&tabRect, fillColor);
            const char* name = kTabNames[i];
            wchar_t wbuf[24];
            int32 n = 0;
            for (int32 c = 0; name[c] && n < 23; ++c) {
                wbuf[n++] = static_cast<wchar_t>(name[c]);
            }
            wbuf[n] = 0;
            Point2D loc(tabRect.X + 4, tabRect.Y + 4);
            surf->DrawText(wbuf, &loc, 0x00FFFFFFu);
        }
    }

    void DrawStrip(DSurface* surf, StripClass& strip)
    {
        if (!surf) return;
        for (int32 i = 0; i < strip.CameoCount; ++i) {
            if (!IsCameoVisible(strip, i)) continue;
            Rectangle r = ComputeCameoRect(strip, i);
            DWORD fill = 0x00808080u;
            if (strip.Cameos[i].Progress > 0 &&
                strip.Cameos[i].Progress < kProductionMaxBars) {
                fill = 0x00A0A040u;
            } else if (strip.Cameos[i].Progress >= kProductionMaxBars) {
                fill = 0x0040A040u;  // ready
            }
            surf->FillRect(&r, fill);
            // Progress overlay (fills from the bottom up).
            if (strip.Cameos[i].Progress > 0) {
                int32 barH = (r.Height * strip.Cameos[i].Progress) /
                             kProductionMaxBars;
                Rectangle prog(r.X, r.Y + r.Height - barH, r.Width, barH);
                surf->FillRect(&prog, 0x0000E0E0u);
            }
            // Flash overlay (when a cameo was just built or ready).
            if (strip.Cameos[i].FlashEndFrame > 0) {
                surf->DrawRect(&r, 0x00FFFFFFu);
            }
        }
    }

    void DrawSuperWeaponButtons(DSurface* surf)
    {
        if (!surf) return;
        int32 swY = kSidebarOriginY + 480 - 32;
        int32 swW = 32;
        int32 swH = 32;
        for (int32 i = 0; i < kSWButtonCount; ++i) {
            Rectangle r(kSidebarOriginX + i * (swW + 2), swY, swW, swH);
            surf->FillRect(&r, 0x00404040u);
            surf->DrawRect(&r, 0x00808080u);
        }
    }

    void DrawTooltipOverlay(DSurface* surf, const wchar_t* text)
    {
        if (!surf || !text || text[0] == 0) return;
        Rectangle tip(kSidebarOriginX + 4, 600 - 40,
                      SidebarClass::SidebarWidth - 8, 24);
        surf->FillRect(&tip, 0x00C0C0C0u);
        Point2D loc(tip.X + 4, tip.Y + 4);
        surf->DrawText(text, &loc, 0x00000000u);
    }

    // ---- Tab/strip manipulation -------------------------------------------

    void SelectTab(SidebarClass& sb, int32 tab)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) return;
        if (tab == sb.ActiveTabIndex) return;
        sb.ActiveTabIndex = tab;
        sb.Tabs[tab].NeedsRedraw = true;
        sb.SidebarNeedsRedraw = true;
    }

    void ScrollStrip(SidebarClass& sb, int32 tab, int32 delta)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) return;
        StripClass& strip = sb.Tabs[tab];
        int32 step = ScrollStepForDelta(delta);
        if (step == 0) return;
        int32 newRow = strip.TopRowIndex + step;
        int32 maxRow = MaxRowForStrip(strip);
        if (newRow < 0) newRow = 0;
        if (newRow > maxRow) newRow = maxRow;
        if (newRow != strip.TopRowIndex) {
            strip.TopRowIndex = newRow;
            strip.NeedsRedraw = true;
            sb.SidebarNeedsRedraw = true;
        }
    }

    void ScrollStripHome(SidebarClass& sb, int32 tab)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) return;
        StripClass& strip = sb.Tabs[tab];
        if (strip.TopRowIndex != 0) {
            strip.TopRowIndex = 0;
            strip.NeedsRedraw = true;
            sb.SidebarNeedsRedraw = true;
        }
    }

    void ScrollStripEnd(SidebarClass& sb, int32 tab)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) return;
        StripClass& strip = sb.Tabs[tab];
        int32 maxRow = MaxRowForStrip(strip);
        if (strip.TopRowIndex != maxRow) {
            strip.TopRowIndex = maxRow;
            strip.NeedsRedraw = true;
            sb.SidebarNeedsRedraw = true;
        }
    }

    void ClearQueue(SidebarClass& sb, int32 tab)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) return;
        StripClass& strip = sb.Tabs[tab];
        for (int32 i = 0; i < strip.CameoCount; ++i) {
            strip.Cameos[i] = BuildType();
        }
        strip.CameoCount = 0;
        strip.TopRowIndex = 0;
        strip.NeedsRedraw = true;
        sb.SidebarNeedsRedraw = true;
    }

    // ---- Hit testing ------------------------------------------------------

    int32 HitTestCameo(const SidebarClass& sb, int32 tab, const Point2D& pt)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) return -1;
        const StripClass& strip = sb.Tabs[tab];
        for (int32 i = 0; i < strip.CameoCount; ++i) {
            Rectangle r = ComputeCameoRect(strip, i);
            if (r.ContainsPoint(pt.X, pt.Y)) return i;
        }
        return -1;
    }

    int32 HitTestTab(const Point2D& pt)
    {
        int32 tabW = SidebarClass::SidebarWidth / SidebarClass::MaxTabs;
        if (pt.Y < kSidebarOriginY ||
            pt.Y >= kSidebarOriginY + kTabStripHeight) return -1;
        if (pt.X < kSidebarOriginX ||
            pt.X >= kSidebarOriginX + SidebarClass::SidebarWidth) return -1;
        int32 idx = (pt.X - kSidebarOriginX) / tabW;
        if (idx < 0 || idx >= SidebarClass::MaxTabs) return -1;
        return idx;
    }

    // ---- Tooltip ----------------------------------------------------------

    void BuildTabTooltip(wchar_t* buf, int32 bufSize, int32 tab)
    {
        if (tab < 0 || tab >= SidebarClass::MaxTabs) { buf[0] = 0; return; }
        const char* name = kTabNames[tab];
        int32 n = 0;
        const wchar_t* prefix = L"Tab: ";
        for (int32 i = 0; prefix[i] && n < bufSize - 1; ++i, ++n) {
            buf[n] = prefix[i];
        }
        for (int32 i = 0; name[i] && n < bufSize - 1; ++i, ++n) {
            buf[n] = static_cast<wchar_t>(name[i]);
        }
        buf[n] = 0;
    }

    void BuildCameoTooltip(wchar_t* buf, int32 bufSize, int32 cameoIdx)
    {
        int32 n = 0;
        const wchar_t* prefix = L"Item #";
        for (int32 i = 0; prefix[i] && n < bufSize - 1; ++i, ++n) {
            buf[n] = prefix[i];
        }
        int32 value = cameoIdx;
        if (value == 0) {
            buf[n++] = L'0';
        } else {
            wchar_t digits[16];
            int32 d = 0;
            while (value > 0 && d < 15) {
                digits[d++] = L'0' + (value % 10);
                value /= 10;
            }
            for (int32 i = d - 1; i >= 0 && n < bufSize - 1; --i) {
                buf[n++] = digits[i];
            }
        }
        buf[n] = 0;
    }

    void SetTooltip(wchar_t* dst, int32 dstSize, const wchar_t* src)
    {
        if (!src) { dst[0] = 0; return; }
        int32 i = 0;
        for (; i < dstSize - 1 && src[i] != 0; ++i) {
            dst[i] = src[i];
        }
        dst[i] = 0;
    }

    void UpdateTooltipForPoint(SidebarClass& sb, const Point2D& pt)
    {
        int32 tab = HitTestTab(pt);
        if (tab >= 0) {
            wchar_t buf[kMaxTooltipChars];
            BuildTabTooltip(buf, kMaxTooltipChars, tab);
            SetTooltip(SidebarClass::TooltipBuffer, kMaxTooltipChars, buf);
            return;
        }
        int32 cameo = HitTestCameo(sb, sb.ActiveTabIndex, pt);
        if (cameo >= 0) {
            wchar_t buf[kMaxTooltipChars];
            BuildCameoTooltip(buf, kMaxTooltipChars, cameo);
            SetTooltip(SidebarClass::TooltipBuffer, kMaxTooltipChars, buf);
            return;
        }
        SidebarClass::TooltipBuffer[0] = 0;
    }

    bool HandleKey(SidebarClass& sb, int32 keyCode)
    {
        switch (keyCode) {
            case '1': case '2': case '3': case '4':
                SelectTab(sb, keyCode - '1');
                return true;
            case 0x21:  // Page Up
                ScrollStrip(sb, sb.ActiveTabIndex, -1);
                return true;
            case 0x22:  // Page Down
                ScrollStrip(sb, sb.ActiveTabIndex, +1);
                return true;
            case 0x24:  // Home
                ScrollStripHome(sb, sb.ActiveTabIndex);
                return true;
            case 0x23:  // End
                ScrollStripEnd(sb, sb.ActiveTabIndex);
                return true;
            case 0x1B:  // Escape
                ClearQueue(sb, sb.ActiveTabIndex);
                return true;
            default:
                return false;
        }
    }
} // anonymous namespace

// ============================================================================
// Static members
// ============================================================================
SidebarClass* SidebarClass::Instance = nullptr;
wchar_t SidebarClass::TooltipBuffer[0x42] = {0};

// ============================================================================
// SidebarClass implementation
// ============================================================================

SidebarClass::SidebarClass()
    : unknown_5394(0)
    , unknown_5398(0)
    , ActiveTabIndex(0)
    , unknown_53A0(0)
    , HideObjectNameInTooltip(false)
    , IsSidebarActive(false)
    , SidebarNeedsRedraw(false)
    , SidebarBackgroundNeedsRedraw(false)
    , unknown_bool_53A8(false)
    , unknown_550C(0)
    , DiplomacyNumHouses(0)
    , unknown_bool_5514(false)
    , unknown_bool_5515(false)
{
    Instance = this;
    memset(TooltipBuffer, 0, sizeof(TooltipBuffer));
    memset(Tabs, 0, sizeof(Tabs));
    memset(DiplomacyHouses, 0, sizeof(DiplomacyHouses));
    memset(DiplomacyKills, 0, sizeof(DiplomacyKills));
    memset(DiplomacyOwned, 0, sizeof(DiplomacyOwned));
    memset(DiplomacyPowerDrain, 0, sizeof(DiplomacyPowerDrain));
    memset(DiplomacyColors, 0, sizeof(DiplomacyColors));
    memset(unknown_544C, 0, sizeof(unknown_544C));
    memset(unknown_546C, 0, sizeof(unknown_546C));
    memset(unknown_548C, 0, sizeof(unknown_548C));
    memset(unknown_54AC, 0, sizeof(unknown_54AC));
    memset(unknown_54CC, 0, sizeof(unknown_54CC));
    memset(unknown_54EC, 0, sizeof(unknown_54EC));
    memset(padding_5516, 0, sizeof(padding_5516));

    // Initialise each strip's identity and bounds.
    for (int32 i = 0; i < MaxTabs; ++i) {
        Tabs[i].Index = i;
        Tabs[i].AllowedToDraw = true;
        Tabs[i].NeedsRedraw = true;
        Tabs[i].TopRowIndex = 0;
        Tabs[i].CameoCount = 0;
        Tabs[i].Progress = 0;
        Tabs[i].Location = Point2D(kSidebarOriginX, kSidebarOriginY);
        Tabs[i].Bounds = Rectangle(kSidebarOriginX, kSidebarOriginY,
                                   SidebarWidth, 0);
    }
}

SidebarClass::~SidebarClass()
{
    if (Instance == this)
        Instance = nullptr;
}

void SidebarClass::Init()
{
    ActiveTabIndex = 0;
    IsSidebarActive = true;
    SidebarNeedsRedraw = true;
    SidebarBackgroundNeedsRedraw = true;
    HideObjectNameInTooltip = false;
    DiplomacyNumHouses = 0;

    // Reset every strip to a clean slate.
    for (int32 i = 0; i < MaxTabs; ++i) {
        Tabs[i].Index = i;
        Tabs[i].AllowedToDraw = true;
        Tabs[i].NeedsRedraw = true;
        Tabs[i].TopRowIndex = 0;
        Tabs[i].CameoCount = 0;
        Tabs[i].Progress = 0;
    }
}

void SidebarClass::Init_IO()
{
    // Wire up input. In the standalone build this merely marks the strips
    // ready to receive mouse/keyboard events.
    for (int32 i = 0; i < MaxTabs; ++i) {
        Tabs[i].AllowedToDraw = true;
        Tabs[i].NeedsRedraw = true;
    }
    SidebarNeedsRedraw = true;
}

void SidebarClass::Init_Clear()
{
    memset(Tabs, 0, sizeof(Tabs));
    ActiveTabIndex = 0;
    SidebarNeedsRedraw = true;
    for (int32 i = 0; i < MaxTabs; ++i) {
        Tabs[i].Index = i;
        Tabs[i].AllowedToDraw = true;
    }
}

void SidebarClass::Init_For_House()
{
    Init();
}

void SidebarClass::SidebarNeedsRepaint(int32 mode)
{
    SidebarNeedsRedraw = true;
    if (mode == 1)
        SidebarBackgroundNeedsRedraw = true;
    if (mode == 2) {
        // Full repaint: mark every strip dirty.
        for (int32 i = 0; i < MaxTabs; ++i) {
            Tabs[i].NeedsRedraw = true;
        }
    }
}

void SidebarClass::RepaintSidebar(int32 tab)
{
    if (tab >= 0 && tab < MaxTabs) {
        ActiveTabIndex = tab;
        Tabs[tab].NeedsRedraw = true;
    }
    SidebarNeedsRedraw = true;
}

bool SidebarClass::AddCameo(AbstractType absType, int32 idxType)
{
    // Prefer the canonical tab for the type, but fall back to any tab with
    // room. This matches the original game's "build anywhere" behaviour when
    // a tab is full.
    int32 preferred = AbstractTypeToTab(absType);
    if (preferred >= 0 && preferred < MaxTabs) {
        StripClass& strip = Tabs[preferred];
        if (strip.CameoCount < MaxCameosPerTab) {
            strip.Cameos[strip.CameoCount] = BuildType(idxType, absType);
            ++strip.CameoCount;
            strip.NeedsRedraw = true;
            SidebarNeedsRedraw = true;
            return true;
        }
    }

    for (int32 i = 0; i < MaxTabs; ++i)
    {
        if (i == preferred) continue;
        StripClass& strip = Tabs[i];
        if (strip.CameoCount < MaxCameosPerTab)
        {
            strip.Cameos[strip.CameoCount] = BuildType(idxType, absType);
            ++strip.CameoCount;
            strip.NeedsRedraw = true;
            SidebarNeedsRedraw = true;
            return true;
        }
    }
    return false;
}

bool SidebarClass::Add_To_List(TechnoTypeClass* pType)
{
    if (!pType) return false;
    return AddCameo(pType->WhatAmI(), pType->GetArrayIndex());
}

bool SidebarClass::Remove_From_List(TechnoTypeClass* pType)
{
    if (!pType) return false;
    // Search the preferred tab first, then all others.
    int32 preferred = AbstractTypeToTab(pType->WhatAmI());
    int32 order[MaxTabs];
    int32 orderCount = 0;
    if (preferred >= 0 && preferred < MaxTabs) {
        order[orderCount++] = preferred;
    }
    for (int32 i = 0; i < MaxTabs; ++i) {
        if (i == preferred) continue;
        order[orderCount++] = i;
    }

    for (int32 o = 0; o < orderCount; ++o) {
        int32 tab = order[o];
        StripClass& strip = Tabs[tab];
        for (int32 i = 0; i < strip.CameoCount; ++i) {
            if (strip.Cameos[i].ItemType == pType->WhatAmI() &&
                strip.Cameos[i].ItemIndex == pType->GetArrayIndex()) {
                // Shift remaining entries down and clear the tail to avoid
                // dangling factory/progress state.
                for (int32 j = i; j < strip.CameoCount - 1; ++j) {
                    strip.Cameos[j] = strip.Cameos[j + 1];
                }
                strip.Cameos[strip.CameoCount - 1] = BuildType();
                --strip.CameoCount;
                strip.NeedsRedraw = true;
                SidebarNeedsRedraw = true;
                return true;
            }
        }
    }
    return false;
}

void SidebarClass::Strip_Update()
{
    // Resolve each strip's dirty flag and propagate to the sidebar. Also
    // clamps scroll positions so the strip never scrolls past its content.
    for (int32 i = 0; i < MaxTabs; ++i)
    {
        StripClass& strip = Tabs[i];
        if (strip.CameoCount <= 0) {
            strip.TopRowIndex = 0;
        } else {
            int32 maxRow = MaxRowForStrip(strip);
            if (strip.TopRowIndex > maxRow) strip.TopRowIndex = maxRow;
            if (strip.TopRowIndex < 0) strip.TopRowIndex = 0;
        }
        if (strip.NeedsRedraw) {
            strip.NeedsRedraw = false;
            SidebarNeedsRedraw = true;
        }
    }
}

void SidebarClass::Tab_Update()
{
    // Recompute tab geometry and mark the active tab for redraw. Called once
    // per frame by the main loop before Draw().
    int32 y = kSidebarOriginY + kCreditsBarHeight + kPowerBarHeight;
    for (int32 i = 0; i < MaxTabs; ++i) {
        Tabs[i].Index = i;
        Tabs[i].Location = Point2D(kSidebarOriginX, y);
        int32 stripHeight = 480 - kTabStripHeight - (y - kSidebarOriginY);
        if (stripHeight < 0) stripHeight = 0;
        Tabs[i].Bounds = Rectangle(kSidebarOriginX, y, SidebarWidth, stripHeight);
    }
    if (ActiveTabIndex >= 0 && ActiveTabIndex < MaxTabs) {
        Tabs[ActiveTabIndex].NeedsRedraw = true;
    }
}

void SidebarClass::One_Time()
{
    Init();
    Tab_Update();
}

int32 SidebarClass::GetObjectTabIdx(AbstractType abs, int32 idxType, int32 unused)
{
    (void)idxType;
    (void)unused;
    // Default: route to first tab (Structures = 0, Infantry = 1, Vehicles = 2, etc.)
    switch (abs)
    {
        case AbstractType::BuildingType: return 0;
        case AbstractType::InfantryType: return 1;
        case AbstractType::UnitType:     return 2;
        case AbstractType::AircraftType: return 3;
        default: return 0;
    }
}

int32 SidebarClass::GetObjectTabIdx(AbstractType abs, BuildCat buildCat, bool isNaval)
{
    if (isNaval) return 3;
    // Naval units always go to the Vehicles/Aircraft tab depending on motion.
    if (buildCat == BuildCat::Combat && abs == AbstractType::UnitType) {
        return 2;
    }
    return GetObjectTabIdx(abs, 0, 0);
}

// ----------------------------------------------------------------------------
// Draw - main sidebar paint entry point.
//
// The original game's Draw performs a layered paint:
//   1. Background panel
//   2. Credits bar
//   3. Power bar
//   4. Tab strip (four tab buttons)
//   5. Active strip's cameos with progress overlays
//   6. Super-weapon buttons
//   7. Tooltip overlay
//
// We mirror that ordering here, pulling live credit and power figures
// from the local player's HouseClass so the bars reflect actual game
// state rather than static values.
// ----------------------------------------------------------------------------
void SidebarClass::Draw(DWORD dwUnk)
{
    (void)dwUnk;
    if (!IsSidebarActive) return;

    DSurface* surf = g_SidebarSurface;

    // Layer 1: background panel.
    if (SidebarBackgroundNeedsRedraw) {
        SidebarBackgroundNeedsRedraw = false;
        DrawBackgroundPanel(surf);
    }

    // Resolve the local player's house for live credit/power display.
    // Houses[] is indexed by ThePlayerIndex (set during scenario start).
    int32 credits = 0;
    int32 powerProduced = 0;
    int32 powerDrained = 0;
    if (ThePlayerIndex >= 0 && ThePlayerIndex < TheHouseCount &&
        Houses[ThePlayerIndex] != nullptr) {
        HouseClass* player = Houses[ThePlayerIndex];
        credits = player->Credits;
        powerProduced = player->PowerOutput;
        powerDrained = player->PowerDrain;
    }

    // Layer 2: credits bar.
    DrawCreditsBar(surf, credits);

    // Layer 3: power bar.
    DrawPowerBar(surf, powerProduced, powerDrained);

    // Layer 4: tab strip.
    DrawTabStrip(surf, ActiveTabIndex);

    // Layer 5: active strip.
    if (ActiveTabIndex >= 0 && ActiveTabIndex < MaxTabs) {
        DrawStrip(surf, Tabs[ActiveTabIndex]);
    }

    // Layer 6: super-weapon buttons.
    DrawSuperWeaponButtons(surf);

    // Layer 7: tooltip overlay (drawn last so it sits on top of everything).
    DrawTooltipOverlay(surf, TooltipBuffer);

    SidebarNeedsRedraw = false;
}

void SidebarClass::RedrawSidebar(int32 mode)
{
    SidebarNeedsRepaint(mode);
}

bool SidebarClass::vt_entry_D8(int32 nUnknown)
{
    (void)nUnknown;
    // The original virtual slot D8 is used to refresh the production queue
    // state after a factory event. We mirror that by marking every strip
    // dirty so the next Draw() rebuilds the queue visuals.
    for (int32 i = 0; i < MaxTabs; ++i) {
        Tabs[i].NeedsRedraw = true;
    }
    SidebarNeedsRedraw = true;
    return true;
}
