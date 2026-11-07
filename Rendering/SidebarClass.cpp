#include "Rendering/SidebarClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Core/Memory.h"
#include "Houses/HouseClass.h"
#include "Game/Externs.h"
#include "Game/GameInit.h"
#include "Houses/FactoryClass.h"
#include "Abstract/BuildingClass.h"
#include "Abstract/BuildingTypeClass.h"
#include "Abstract/TechnoClass.h"
#include "Rendering/MouseClass.h"
#include "SW/SuperClass.h"
#include "SW/SuperWeaponTypeClass.h"
#include "Audio/VocClass.h"
#include "Audio/VoxClass.h"
#include "Rules/RulesClass.h"
#include "Helpers/AsmUIHelpers.h"
#include "IO/MixFileClass.h"
#include "FileFormats/SHP.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cwchar>

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
    , PlayerInfiltrated(0)
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
    // asm: 图标表与起始下标在构造时清零。
    TabCameoList = TabCameoListClass();
    CameoBaseIndex = 0;
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

// ============================================================================
// asm: SidebarClass__Set_Cameo_Text_bool (0x5D0A30)
//
//   SidebarClass__Set_Cameo_Text_bool proc near
//                   retn    4
//   SidebarClass__Set_Cameo_Text_bool endp
//
// 原版是**空函数**：YR 里"图标文字"开关已改由 GameOptionsClass 侧
// 直接读 SidebarCameoText 决定，侧边栏不再缓存副本。按 asm 保留空实现，
// 不做任何写入（铁律①：不臆造行为）。
// ============================================================================
void SidebarClass::Set_Cameo_Text(bool on)
{
    (void)on;                        // asm: retn 4 —— 无任何写入
}

void SidebarClass::Init_Clear()
{
    memset(Tabs, 0, sizeof(Tabs));
    TabCameoList = TabCameoListClass();
    CameoBaseIndex = 0;
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

// ============================================================================
// 侧边栏素材表 + SHP 贴图工具
//
// asm 依据：
//   SidebarClass::Init_For_House (0x6A5820 区) 用 MixFileClass<CCFileClass>::
//     Finder 依次装 SIDE1.SHP / SIDE2.SHP / SIDE3.SHP / ADDON.SHP / GCLOCK2.SHP /
//     SELL.SHP / REPAIR.SHP / TAB%02d.SHP / R-DN.SHP / R-UP.SHP，
//     并把返回句柄分别写进 SIDE1_SHP / SIDE2_SHP / SIDE3_SHP / ADDON_SHP /
//     GCLOCK2_SHP 等全局。
//   SidebarClass::One_Time (0x6A4FDB) 与 StripClass::One_Time (0x6A8190) 装
//     DARKEN.SHP（全局 SidebarClass::StripClass::DarkenShape）。
//   sub_6ABD30 在 SIDE1_SHP 为空时改取 SIDEGDI1/2/3.SHP（mode=0 回落）。
//
// 所有贴图都是 frame 0、Brightness 1000（asm 3E8h）、Convert = 侧边栏调色板；
// 本工程没有独立的侧边栏调色板，按工程约定落到 StandardPalette。CC_Draw_Shape
// 的 ⑩ 段在 Palette 为空时直接返回，故 StandardPalette 为空时整批跳过。
// ============================================================================
namespace
{
    // asm 侧边栏局部坐标锚点：someRect.L = 9Eh = 158（本工程里 = 雷达框底
    // 48+110）。下面所有 +N 偏移都是相对这个锚点的。
    constexpr int32 kSidebarArtAnchor   = 158;
    constexpr int32 kButtonTop          = kSidebarArtAnchor + 7;   // B0B4E0
    constexpr int32 kTabRowTop          = kSidebarArtAnchor + 39;  // B0B4EC
    constexpr int32 kButtonLeft         = 20;                      // B0B4DC
    constexpr int32 kSellButtonPitch    = 64;                      // B0B4E4 (40h, YR)
    constexpr int32 kTabPitch           = 32;                      // B0B4F0 (20h, YR)
    constexpr int32 kTabLeft            = 20;                      // B0B4E8 (14h, YR)
    constexpr int32 kCameoLeft          = 22;                      // B0B4F4 - T + ...
    constexpr int32 kArtRetryFrames     = 240;                     // MIX 未就绪时的重试间隔

    // asm: Fetch_Techno_Type —— 定义在本文件后段的匿名命名空间里。
    TechnoTypeClass* Fetch_Techno_Type(int32 abstractID, int32 idx);

    struct SidebarArtCache
    {
        SHPStruct* Side1;
        SHPStruct* Side2;
        SHPStruct* Side3;
        SHPStruct* Addon;
        SHPStruct* Clock;    // GCLOCK2.SHP —— 建造进度饼图覆盖
        SHPStruct* Darken;   // DARKEN.SHP  —— 图标不可用 / 就绪闪烁覆盖
        SHPStruct* Sell;     // SELL.SHP
        SHPStruct* Repair;   // REPAIR.SHP
        SHPStruct* Tab[SidebarClass::MaxTabs];   // TAB%02d.SHP
        int32      LastAttempt;
    };

    SHPStruct* RetrieveSidebarShape(const char* pName)
    {
        if (pName == nullptr)
            return nullptr;
        return Load_SHP(pName);
    }

    // 惰性装载。全部命中后不再查表；缺件时每 kArtRetryFrames 帧重试一次，
    // 避免 MIX 在首帧之后才注册时永远画不出东西，也避免逐帧 CRC 扫描。
    const SidebarArtCache& Sidebar_Art()
    {
        static SidebarArtCache s_art = { nullptr, nullptr, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, nullptr,
                                         { nullptr, nullptr, nullptr, nullptr },
                                         -0x40000000 };

        bool complete = s_art.Side1 != nullptr && s_art.Side2 != nullptr &&
                        s_art.Side3 != nullptr && s_art.Addon != nullptr &&
                        s_art.Clock != nullptr && s_art.Darken != nullptr &&
                        s_art.Sell != nullptr && s_art.Repair != nullptr;
        for (int32 i = 0; i < SidebarClass::MaxTabs && complete; ++i)
            complete = (s_art.Tab[i] != nullptr);
        if (complete)
            return s_art;

        // asm 的 Frame 全局 = 引擎帧号；本工程里等价的是 CurrentFrame
        // （Game::GameLoop 每帧自增），全局 FrameCounter 只在初始化时清零。
        if (CurrentFrame - s_art.LastAttempt < kArtRetryFrames)
            return s_art;
        s_art.LastAttempt = CurrentFrame;

        if (s_art.Side1 == nullptr)
            s_art.Side1 = RetrieveSidebarShape("SIDE1.SHP");
        if (s_art.Side2 == nullptr)
            s_art.Side2 = RetrieveSidebarShape("SIDE2.SHP");
        if (s_art.Side3 == nullptr)
            s_art.Side3 = RetrieveSidebarShape("SIDE3.SHP");
        if (s_art.Addon == nullptr)
            s_art.Addon = RetrieveSidebarShape("ADDON.SHP");
        if (s_art.Clock == nullptr)
            s_art.Clock = RetrieveSidebarShape("GCLOCK2.SHP");
        if (s_art.Darken == nullptr)
            s_art.Darken = RetrieveSidebarShape("DARKEN.SHP");
        if (s_art.Sell == nullptr)
            s_art.Sell = RetrieveSidebarShape("SELL.SHP");
        if (s_art.Repair == nullptr)
            s_art.Repair = RetrieveSidebarShape("REPAIR.SHP");

        // asm sub_6ABD30: SIDE1 缺失 -> SIDEGDI1/2/3.SHP
        if (s_art.Side1 == nullptr)
            s_art.Side1 = RetrieveSidebarShape("SIDEGDI1.SHP");
        if (s_art.Side2 == nullptr)
            s_art.Side2 = RetrieveSidebarShape("SIDEGDI2.SHP");
        if (s_art.Side3 == nullptr)
            s_art.Side3 = RetrieveSidebarShape("SIDEGDI3.SHP");

        for (int32 i = 0; i < SidebarClass::MaxTabs; ++i)
        {
            if (s_art.Tab[i] != nullptr)
                continue;
            char name[16];
            std::snprintf(name, sizeof(name), "TAB%02d.SHP", i);
            s_art.Tab[i] = RetrieveSidebarShape(name);
        }

        return s_art;
    }

    // asm 各 blit 调用点的 CC_Draw_Shape 公共参数：
    //   Remap=0 / ZAdjust=0 / ZGradient=0 / Brightness=3E8h / TintColor=0 /
    //   ZShape=null / ZShapeFrame=0 / XOffset=0 / YOffset=0
    void Sidebar_Blit(Surface* pSrf, const Rectangle& clip, SHPStruct* pSHP,
                      int32 frame, int32 x, int32 y, uint32 flags)
    {
        if (pSHP == nullptr || StandardPalette == nullptr)
            return;
        if (frame < 0 || (pSHP->Frames > 0 && frame >= static_cast<int32>(pSHP->Frames)))
            return;

        Point2D pos(x, y);
        CC_Draw_Shape(pSrf, StandardPalette, pSHP, frame, &pos, &clip,
                      static_cast<BlitterFlags>(flags),
                      0, 0, 0, 1000, 0, nullptr, 0, 0, 0);
    }

    // asm: 每块 SIDE2 的高度取自 SHP 帧高（`movsx eax, word ptr [ebp+4]`）。
    int32 Sidebar_FrameHeight(SHPStruct* pSHP, int32 fallback)
    {
        if (pSHP == nullptr)
            return fallback;
        int32 w = 0;
        int32 h = 0;
        if (pSHP->GetFrameSize(0, w, h) != 0 || h <= 0)
            return fallback;
        return h;
    }

    // 底板：SidebarClass_Draw 0x6A6CCC..0x6A6DEB 的 SIDE1 + N×SIDE2 + SIDE3 +
    // ADDON 链。全部
    //   CC_Draw_Shape(DSurface_Sidebar, SidebarPalette, shp, 0, &pos, &面矩形,
    //                 400h, 0,0,0, 3E8h, 0, 0,0,0,0)
    // SIDE2 从 dword_B0B4F8 起每行贴一块（Y 步进 = SIDE2 帧高），SIDE3 紧接
    // 最后一块，ADDON 紧接 SIDE3（`movsx eax, word ptr [SIDE3+4]`）。
    //
    // includeTopPiece：SIDE1 是"雷达框 + 资金/电力 + 修理出售按钮 + 标签页"
    //   那一整块（0..B0B4F8）。只有在独占 DSurface_Sidebar 的绘制路径上才能贴；
    //   DisplayClass 的合成路径已经把雷达画在同一张面上并随后调用 Draw_UI，
    //   再贴 SIDE1 会把小地图盖掉，故那条路径只补建造格区域（SIDE2/3/ADDON）。
    //
    // 返回实际提交的贴图次数；0 表示素材全缺（调用方决定是否画中性底板）。
    int32 Sidebar_DrawBackdrop(Surface* pSrf, const Rectangle& panel, int32 rows,
                               bool includeTopPiece)
    {
        if (pSrf == nullptr || pSrf->Buffer == nullptr)
            return 0;

        const SidebarArtCache& art = Sidebar_Art();
        Rectangle clip = panel;
        int32 count = 0;

        if (includeTopPiece && art.Side1 != nullptr)
        {
            Sidebar_Blit(pSrf, clip, art.Side1, 0, panel.X, panel.Y, 0x400);
            ++count;
        }

        int32 y = panel.Y + SidebarClass::CameoGridY;
        if (art.Side2 != nullptr)
        {
            const int32 step = Sidebar_FrameHeight(art.Side2, SidebarClass::CameoPitchY + 13);
            for (int32 r = 0; r < rows && y < panel.Y + panel.Height; ++r)
            {
                Sidebar_Blit(pSrf, clip, art.Side2, 0, panel.X, y, 0x400);
                ++count;
                y += step;
            }
        }
        if (art.Side3 != nullptr)
        {
            Sidebar_Blit(pSrf, clip, art.Side3, 0, panel.X, y, 0x400);
            ++count;
            y += Sidebar_FrameHeight(art.Side3, 8);
        }
        if (art.Addon != nullptr)
        {
            Sidebar_Blit(pSrf, clip, art.Addon, 0, panel.X, y, 0x400);
            ++count;
        }
        return count;
    }

    // ShapeButtonClass::Draw_Me 用 [btn+1Eh] 区分"置灰帧(2)"，但素材不一定有
    // 这么多帧；越界帧会被 CC_Draw_Shape 整帧丢弃，故缺帧时退回 frame 0。
    int32 Sidebar_ButtonFrame(SHPStruct* pSHP, int32 frame)
    {
        if (pSHP == nullptr || pSHP->Frames <= 0)
            return 0;
        return (frame < static_cast<int32>(pSHP->Frames)) ? frame : 0;
    }

    // 一个建造格的绘制状态（StripClass::Draw_It 的 var_454/var_478/var_47C）。
    struct SidebarCameoState
    {
        SHPStruct* Shape;          // GetCameo() / SuperWeaponTypeClass.SidebarImage
        bool       Disabled;       // 画 DARKEN.SHP（flags 401h）
        bool       Completed;      // 工厂已完成 -> 不画饼图
        bool       Producing;      // 画 GCLOCK2.SHP
        int32      Progress;       // 0..54
        int32      FlashEndFrame;

        SidebarCameoState()
            : Shape(nullptr), Disabled(false), Completed(false)
            , Producing(false), Progress(0), FlashEndFrame(0)
        {}
    };

    // asm: 图标灰化 = HouseClass::Can_Build(...)==-1 或 ShouldDisableCameo。
    bool Sidebar_CameoDisabled(HouseClass* pHouse, TechnoTypeClass* pType)
    {
        if (pHouse == nullptr || pType == nullptr)
            return true;
        if (!pHouse->Can_Produce_Type(pType))
            return true;
        return pHouse->ShouldDisableCameo(pType);
    }

    void Sidebar_FillCameoFromBuild(SidebarCameoState& out, const BuildType& build,
                                    HouseClass* pHouse)
    {
        TechnoTypeClass* pType = Fetch_Techno_Type(static_cast<int32>(build.ItemType),
                                                   build.ItemIndex);
        if (pType == nullptr)
            return;

        out.Shape    = pType->GetCameo();
        out.Disabled = Sidebar_CameoDisabled(pHouse, pType);

        // asm: var_454 = FactoryClass_GetProgress()；若条目自带进度更大则取均值。
        int32 progress = build.Progress;
        FactoryClass* pFactory = build.CurrentFactory;
        if (pFactory != nullptr)
        {
            const int32 factoryProgress = pFactory->GetProgress();
            progress = (progress > factoryProgress)
                           ? (progress + factoryProgress) / 2
                           : factoryProgress;
            out.Completed = pFactory->IsCompleted();
        }
        else
        {
            out.Completed = (progress >= kProductionMaxBars);
        }

        out.Progress      = progress;
        // asm: 只有"工厂在产"或"已有进度"时才叠 GCLOCK2 饼图；
        //   纯排队（无工厂、进度 0）的图标不覆盖任何东西。
        out.Producing     = !out.Completed && (pFactory != nullptr || progress > 0);
        out.FlashEndFrame = build.FlashEndFrame;
    }

    void Sidebar_FillCameoFromEntry(SidebarCameoState& out, const CameoEntry& entry,
                                    HouseClass* pHouse)
    {
        // asm 6A9747: abstractID == eSpecial(1Fh) -> 超级武器图标，SHP 取
        //   SuperWeaponTypeClass.SidebarImage(+0B8h)，进度取 SuperClass::Anim_Stage。
        if (entry.AbstractID == 0x1F)
        {
            if (pHouse == nullptr || pHouse->SuperWeapons == nullptr ||
                entry.ItemIndex < 0 ||
                entry.ItemIndex >= static_cast<int32>(pHouse->SuperWeapons->Count))
                return;

            SuperClass* pSuper = (*pHouse->SuperWeapons)[entry.ItemIndex];
            if (pSuper == nullptr || pSuper->Type == nullptr)
                return;

            out.Shape     = pSuper->Type->SidebarImage;
            out.Progress  = pSuper->Anim_Stage();
            out.Completed = pSuper->IsReady();
            out.Producing = !out.Completed;
            return;
        }

        TechnoTypeClass* pType = Fetch_Techno_Type(entry.AbstractID, entry.ItemIndex);
        if (pType == nullptr)
            return;

        out.Shape         = pType->GetCameo();
        out.Disabled      = Sidebar_CameoDisabled(pHouse, pType);
        out.Progress      = entry.Progress;
        out.Producing     = (entry.Status == 1 || entry.Status == 2);
        out.Completed     = (entry.Status == 0);
        out.FlashEndFrame = 0;
    }
}

// ----------------------------------------------------------------------------
// Draw_UI - 侧边栏内容（底板 / 修理出售按钮 / 标签页 / 建造格）
//
// 坐标全部是侧边栏局部坐标（DSurface_Sidebar 的 (0,0) 起），由 panel.X/Y 平移到
// 当前目标面。布局常量来自 SidebarClass::StripClass::Init_IO (0x6A8241) 与
// sub_6A5130 (0x6A5130)：
//   建造格 gadget 尺寸 3Ch×30h（60×48），横向步进 B0B4FC=40h(64)，纵向步进
//   B0B500=32h(50)，两列（gadget 下标 = 2*col+row）。
// ----------------------------------------------------------------------------
int32 SidebarClass::Draw_UI(Surface* pSrf, const Rectangle& panel)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return 0;
    if (panel.Width <= 0 || panel.Height <= 0)
        return 0;

    const SidebarArtCache& art = Sidebar_Art();
    Rectangle clip = panel;          // CC_Draw_Shape / FillRectEx 要非 const 裁剪框
    int32 drawn = 0;                 // 提交的绘制次数（[UI] 调试统计用）

    const int32 originX = panel.X;
    const int32 originY = panel.Y;
    const int32 bottom  = panel.Y + panel.Height;

    // ---- 可见建造行数 ----------------------------------------------------
    // asm Draw_It: rows = (someRect.B - B0B4F8 - 18h + someRect.L - 7) / 32h。
    // 本工程把 B0B4F8 折成局部 SidebarClass::CameoGridY，等价于
    //   rows = (面板高 - CameoGridY) / CameoPitchY
    int32 rows = (panel.Height - SidebarClass::CameoGridY) / CameoPitchY;
    if (rows < 1) rows = 1;
    if (rows > 16) rows = 16;

    // ====================================================================
    // 1) 建造格区域底板：每行一块 SIDE2 + SIDE3 + ADDON
    //
    // asm SidebarClass_Draw 0x6A6CCC..0x6A6DEB：flags 400h、frame 0、
    //   Convert = SidebarPalette、Brightness 3E8h。SIDE1（雷达框那一整块）
    //   只在独占侧边栏面的 Draw() 里贴，理由见 Sidebar_DrawBackdrop。
    // ====================================================================
    drawn += Sidebar_DrawBackdrop(pSrf, panel, rows, false);

    // ====================================================================
    // 2) 修理 / 出售按钮（SHP 按钮，不是几何图形）
    //
    // asm 0x6A56xx：Repair 在 gadget(B0B4DC, B0B4E0) = 局部 (14h, 锚点+7)；
    //   Sell   在 X = B0B4DC + B0B4E4 = 14h + 40h = 54h（YR 布局）。
    //   ShapeButtonClass::Draw_Me 的 flags 只有在 [btn+55h] 置位时才加 800h，
    //   常规态为 0；frame = 0（就绪态）。
    // ====================================================================
    Sidebar_Blit(pSrf, clip, art.Repair, 0,
                 originX + kButtonLeft, originY + kButtonTop, 0);
    if (art.Repair != nullptr)
        ++drawn;

    Sidebar_Blit(pSrf, clip, art.Sell, 0,
                 originX + kButtonLeft + kSellButtonPitch, originY + kButtonTop, 0);
    if (art.Sell != nullptr)
        ++drawn;

    // ====================================================================
    // 3) 四个建造分类标签页（TAB%02d.SHP）
    //
    // asm 0x6A541C：X = B0B4E8 + B0B4F0*i = 14h + 20h*i，Y = B0B4EC = 锚点+27h；
    //   按钮号 [btn+24h] = 0CBh + i；frame 由 ShapeButtonClass::Draw_Me 决定：
    //   [btn+1Eh] 置位（该页无图标，被 Gadget_40 置灰）时 frame = 2，否则 0.
    // ====================================================================
    for (int32 i = 0; i < MaxTabs; ++i)
    {
        if (art.Tab[i] == nullptr)
            continue;
        const int32 frame = Sidebar_ButtonFrame(art.Tab[i],
                                                (Tabs[i].CameoCount > 0) ? 0 : 2);
        Sidebar_Blit(pSrf, clip, art.Tab[i], frame,
                     originX + kTabLeft + i * kTabPitch, originY + kTabRowTop, 0);
        ++drawn;
    }

    // ====================================================================
    // 4) 建造格（cameo + 灰化 + 就绪闪烁 + 进度饼图）
    //
    // asm StripClass::Draw_It 0x6A9A43 起：
    //   pos = (Location.X + B0B4FC*col - someRect.T, Location.Y + B0B500*row + 1)
    //       = 局部 (16h + 40h*col, B0B4F8 + 32h*row + 1)
    //   ~CC_Draw_Shape(面, Convert_CAMEO,  type->GetCameo(), 0, &pos, &面矩形,
    //                  400h, 0,0,0, 3E8h, 0, 0,0,0,0)          ; 图标
    //   ~CC_Draw_Shape(面, SidebarPalette, DarkenShape, 0, &pos, &面矩形,
    //                  401h, ... )                            ; 不可用灰化
    //   ~CC_Draw_Shape(面, SidebarPalette, DarkenShape, 0, &pos, &面矩形,
    //                  404h, ... )                            ; 就绪闪烁
    //   ~CC_Draw_Shape(面, SidebarPalette, GCLOCK2_SHP, var_454+1, &pos, &面矩形,
    //                  404h, ... )                            ; 进度饼图
    // ====================================================================
    const bool useEntries = (TabCameoList.Count > 0);
    HouseClass* pHouse = HouseClass::Player;
    const int32 tabIndex = ActiveTabIndex;

    int32 firstRow = 0;
    int32 cameoCount = 0;
    if (useEntries)
    {
        firstRow   = TabCameoList.CurrentRow;
        cameoCount = TabCameoList.Count;
    }
    else if (tabIndex >= 0 && tabIndex < MaxTabs)
    {
        firstRow   = Tabs[tabIndex].TopRowIndex;
        cameoCount = Tabs[tabIndex].CameoCount;
    }

    if (firstRow < 0)
        firstRow = 0;

    const int32 clockFrames = (art.Clock != nullptr && art.Clock->Frames > 0)
                                  ? static_cast<int32>(art.Clock->Frames)
                                  : (kProductionMaxBars + 1);

    for (int32 row = 0; row < rows; ++row)
    {
        for (int32 col = 0; col < CameoColumns; ++col)
        {
            const int32 slot = (firstRow + row) * CameoColumns + col;
            if (slot < 0 || slot >= cameoCount)
                continue;

            SidebarCameoState cameo;
            if (useEntries)
                Sidebar_FillCameoFromEntry(cameo, TabCameoList.Entries[slot], pHouse);
            else
                Sidebar_FillCameoFromBuild(cameo, Tabs[tabIndex].Cameos[slot], pHouse);

            const int32 x = originX + kCameoLeft + col * CameoPitchX;
            const int32 y = originY + CameoGridY + row * CameoPitchY + 1;

            if (y >= bottom)
                break;

            if (cameo.Shape != nullptr)
            {
                Sidebar_Blit(pSrf, clip, cameo.Shape, 0, x, y, 0x400);
                ++drawn;
            }

            // 灰化：Remap|Transparent，用 DARKEN.SHP 盖在原图标上。
            if (cameo.Disabled && art.Darken != nullptr)
            {
                Sidebar_Blit(pSrf, clip, art.Darken, 0, x, y, 0x401);
                ++drawn;
            }

            // 就绪闪烁：asm 用 `cmp FlashEndFrame, Frame` 与 `Frame & 0Fh > 8`
            // 两道闸门，命中时把 DARKEN.SHP 以 50% 半透明再叠一次。
            if (cameo.FlashEndFrame > CurrentFrame &&
                (CurrentFrame & 0x0F) > 8 && art.Darken != nullptr)
            {
                Sidebar_Blit(pSrf, clip, art.Darken, 0, x, y, 0x404);
                ++drawn;
            }

            // 进度：GCLOCK2.SHP 的 frame = 进度 + 1，50% 半透明叠加。
            if (cameo.Producing && art.Clock != nullptr)
            {
                int32 frame = cameo.Progress + 1;
                if (frame < 0)
                    frame = 0;
                if (frame >= clockFrames)
                    frame = clockFrames - 1;
                Sidebar_Blit(pSrf, clip, art.Clock, frame, x, y, 0x404);
                ++drawn;
            }
        }
    }

    return drawn;
}

// ----------------------------------------------------------------------------
// Draw - main sidebar paint entry point.
//
// 原版的侧边栏底板是 SidebarClass_Draw 里那条 SIDE1.SHP + N×SIDE2.SHP +
// SIDE3.SHP + ADDON.SHP 的贴图链（见 Sidebar_DrawBackdrop），资金/电力与雷达
// 由 TabClass / RadarClass 画到同一张面上。本工程只在素材（含 StandardPalette）
// 全部缺失时才退回到中性色块 + 文字，绝不拿矩形冒充 SIDE1.SHP 的美术。
// ----------------------------------------------------------------------------
void SidebarClass::Draw(DWORD dwUnk)
{
    // dwUnk 非 0 时强制重画底板（原版强制重绘路径）。
    if (dwUnk != 0)
        SidebarBackgroundNeedsRedraw = true;

    Surface* srf = g_SidebarSurface;
    if (srf == nullptr && TacticalClass::Instance != nullptr)
        srf = TacticalClass::Instance->Target_Surface();
    if ((srf == nullptr || srf->Buffer == nullptr) && TheGScreen != nullptr)
        srf = TheGScreen->Get_Back_Buffer();
    if (srf == nullptr || srf->Buffer == nullptr)
        return;

    const int32 srfW = srf->GetWidth();
    const int32 srfH = srf->GetHeight();
    if (srfW <= 0 || srfH <= 0)
        return;

    int32 panelW = PanelWidth;
    if (panelW > srfW) panelW = srfW;
    Rectangle panel(srfW - panelW, 0, panelW, srfH);

    int32 rows = (panel.Height - SidebarClass::CameoGridY) / CameoPitchY;
    if (rows < 1) rows = 1;

    const int32 backdrop = Sidebar_DrawBackdrop(srf, panel, rows, true);
    if (backdrop == 0)
    {
        // 素材缺失：只需要一块中性底，让后面的几何/文字可读。
        srf->FillRectEx(nullptr, &panel, 0x4208);
        Rectangle leftEdge(panel.X, 0, 1, panel.Height);
        srf->FillRectEx(nullptr, &leftEdge, 0x8410);

        Rectangle topBar(panel.X + 1, 0, panel.Width - 1, 46);
        srf->FillRectEx(nullptr, &topBar, 0x2104);

        HouseClass* player = HouseClass::Player;
        const int32 credits = (player != nullptr) ? player->Credits : 0;
        const int32 powerOut = (player != nullptr) ? player->PowerOutput : 0;
        const int32 powerDrain = (player != nullptr) ? player->PowerDrain : 0;

        wchar_t line[48];
        std::swprintf(line, 48, L"$%d", credits);
        Point2D ret(0, 0);
        Point2D cLoc(panel.X + 8, 5);
        Fancy_Text_Print_Wide(ret, line, srf, panel, cLoc, 0xFE60, 0x0000,
                              TextPrintType::NoShadow);

        std::swprintf(line, 48, L"PWR %d/%d", powerOut, powerDrain);
        Point2D pLoc(panel.X + 8, 17);
        Fancy_Text_Print_Wide(ret, line, srf, panel, pLoc,
                              (powerDrain > powerOut) ? 0xF800 : 0x07E0,
                              0x0000, TextPrintType::NoShadow);
    }

    Draw_UI(srf, panel);

    SidebarBackgroundNeedsRedraw = false;
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

// ============================================================================
// asm: SidebarClass_ProcessCameoClick (0x6AACB0，1205 行，retn 0Ch)
//
// 侧边栏图标点击总入口。原版把"改生产状态"的请求打包成网络事件
// （Networking_AddEvent_ProduceAbandonSuspend / AddEvent_Place /
// AddEvent_SWPlace）投进事件队列，由事件处理器落到真实的生产接口上。
// OpenYR 的 Networking 已换成另一套事件模型，这里按铁律③直调最终落点：
//   ev_PRODUCE / ev_SUSPEND / ev_ABANDON(_ALL)
//       -> HouseClass::BeginProductionOf / SuspendProductionOf / AbandonProductionOf
//   ev_PLACE      -> HouseClass::UnitFromFactory（产品出厂）
//   ev_SPECIAL_PLACE -> SuperClass::Launch（无目标超武就地引爆）
// 点击分支：左键建造/继续、左键取消/暂停、右键取消/暂停、Shift 连点排队、
// 超武图标就绪判定、建筑与防御的建造厂归并。
// ============================================================================

namespace
{
    // asm: Factory_BuildSpeedMult (.data) —— 建造速度倍率，由网络事件
    //   ev_RESPONSE_TIME 写入；侧边栏用它折算进度条格数。
    int32 Factory_BuildSpeedMult = 0;

    // asm: unk_88099A / 88099B / 88099C —— 与 PlaceTech 一起被清空的放置状态
    //   三联字节（sub_4FB840 与 6AAF46 分支）。原版是 .data 里相邻的三个字节。
    uint8 unk_88099A = 0;
    uint8 unk_88099B = 0;
    uint8 unk_88099C = 0;

    // asm: unk_880D68 —— 每档 3988 字节（= 一张 TabCameoListClass 的大小）。
    //   产线被挂起时把 [标签页序号 * 3988] 处置 1。
    uint8 unk_880D68[SidebarClass::MaxTabs * 3988];

    // asm: Fetch_Techno_Type(abstractID, idx) —— 按（类型类别, 下标）取类型。
    //   OpenYR 的 TechnoTypeClass::Array 把各子类合成统一索引表，故直接按
    //   全局下标取；abstractID 仅用于原版的分类数组选择。
    TechnoTypeClass* Fetch_Techno_Type(int32 abstractID, int32 idx)
    {
        (void)abstractID;
        return TechnoTypeClass::FindByIndex(idx);
    }

    // asm: Is_Techno —— 判断一个对象是不是 Techno 系（四个子类之一）。
    bool Is_Techno_Object(ObjectClass* pObj)
    {
        if (pObj == nullptr)
            return false;
        switch (pObj->WhatAmI()) {
        case AbstractType::Unit:
        case AbstractType::Infantry:
        case AbstractType::Aircraft:
        case AbstractType::Building:
            return true;
        default:
            return false;
        }
    }
}

int32 SidebarClass::ProcessCameoClick(int32 a2, int32 a3, char a4)
{
    // ---- 局部量（全部先声明，后面的 goto 才能跨标签） ----------------------
    DisplayClass*  pMap   = DisplayClass::Instance;
    HouseClass*    pHouse = HouseClass::Player;
    TabCameoListClass& tab = this->TabCameoList;

    CameoEntry*     pEntry        = nullptr;
    int32           cameoIdx      = 0;
    int32           idx           = 0;   // asm: idx 槽（后段复用为 naval 字节）
    int32           abstractId    = 0;   // asm: ebp
    int32           buildCat      = 0;   // asm: BuildCat 槽
    FactoryClass*   pEntryFactory = nullptr;  // asm: Fact_ 槽（后段复用为 naval）
    TechnoTypeClass* pType        = nullptr;  // asm: var_84
    TechnoTypeClass* pType2       = nullptr;  // 重新取的类型
    SuperClass*     pSuper        = nullptr;
    SuperWeaponTypeClass* pSWType = nullptr;
    TechnoClass*    pProduct      = nullptr;
    TechnoClass*    pHandoff      = nullptr;
    FactoryClass*   pFac          = nullptr;
    bool            leftClick     = false;  // asm: var_99（后段复用为 disable）
    bool            paused        = false;  // asm: var_8D
    bool            enableNew     = false;  // asm: var_8E
    bool            counter       = false;  // asm: var_8F
    int32           progressVal   = 0;
    int32           curProgress   = 0;
    int32           avg           = 0;
    int32           frames        = 0;
    int32           buildSpeedMult = 0;
    int32           funds         = 0;
    int32           need          = 0;
    int32           cost          = 0;
    int32           lower         = 0;
    int32           q             = 0;
    int32           q54           = 0;
    int32           x             = 0;
    int32           hi            = 0;
    int64           full          = 0;

    if (pMap == nullptr || pHouse == nullptr) {
        return 1;
    }

    // asm 6AAD0C: MouseClass_SetPointer(Map, 0, 0) —— 复位鼠标指针图案。
    if (MouseClass::Instance != nullptr) {
        MouseClass::Instance->SetPointer(0, false);
    }

    // asm 8422: cameoIdx = [this+30h] + CurrentRow * 2
    cameoIdx = this->CameoBaseIndex + tab.CurrentRow * 2;

    // asm 8450: 越界（cameoIdx >= [tab+54h]）-> a2 = 0，走收尾。
    if (cameoIdx < 0 || cameoIdx >= tab.Count) {
        a2 = 0;
        goto cc_tail;
    }

    pEntry      = &tab.Entries[cameoIdx];
    idx         = pEntry->ItemIndex;
    abstractId  = pEntry->AbstractID;
    buildCat    = pEntry->BuildCat;
    pEntryFactory = pEntry->Factory;

    // asm 8445: var_99 = a4 & 1 —— 本次是不是"左键"。
    leftClick = (a4 & 1) != 0;

    // asm 8452: 超武图标（abstractID == 1Fh）走独立分支。
    if (abstractId == 0x1F) {
        goto cc_sw;
    }

    // asm 8455: type = Fetch_Techno_Type(abstractID, idx)；空 -> 收尾。
    pType = Fetch_Techno_Type(abstractId, idx);
    if (pType == nullptr) {
        a2 = 0;
        goto cc_tail;
    }
    if ((a2 & 8) != 0) {          // asm: and al,0F7h —— 清掉 bit3
        a2 &= ~8;
    }

    // asm 8474: bit4 = 右键；否则走左键主路径。
    if ((a2 & 0x10) != 0) {
        goto cc_right;
    }
    goto cc_main;

// ---------------------------------------------------------------------------
cc_sw:   // asm 6AAE6D: 超级武器图标
    if (idx == -1) {
        a2 = 0;
        goto cc_tail;
    }
    if ((a2 & 8) != 0) {
        a2 &= ~8;
    }
    // asm 6AAE90: 右键 -> dword_8809A0 = -1（取消超武目标模式）。
    if ((a2 & 0x10) != 0) {
        pMap->CurrentSWTypeIndex = -1;
    }
    // asm 6AAE9A: 左键才继续；idx 必须落在超武表范围内。
    if ((a2 & 1) == 0) {
        goto cc_tail;
    }
    if (pHouse->SuperWeapons == nullptr ||
        static_cast<uint32>(idx) >=
            static_cast<uint32>(pHouse->SuperWeapons->Count)) {
        goto cc_tail;
    }

    // asm 6AAEB4: VocClass_Play(RulesData->GUIBuildSound, 1.0f, 0)
    if (RulesClass::Instance != nullptr) {
        VocClass::PlayIndexAtPos(RulesClass::Instance->GUIBuildSound, nullptr, 0);
    }

    // asm 6AAEC7: super = [house+258h][idx]
    pSuper = (*pHouse->SuperWeapons)[idx];
    if (pSuper == nullptr) {
        goto cc_tail;
    }
    // asm 6AAED2: 未就绪 -> SuperClass_IgnoreClick（提示音），不进入目标模式。
    if (!pSuper->IsReadyToFire()) {
        pSuper->IgnoreClick(true);
        goto cc_tail;
    }

    pSWType = pSuper->Type;
    // asm 6AAEE9: Action == 0 -> 无需选目标，直接就地引爆（ev_SPECIAL_PLACE，
    //   原版把 4 字节 CELL 清零后投递，Launch 内部按默认坐标处理）。
    if (pSWType != nullptr && pSWType->Action == 0) {
        pSuper->Launch(CellStruct(0, 0));
        goto cc_tail;
    }

    // asm 6AAF46: 进入选目标模式。
    pMap->Cancel_Placement();          // Set_Cursor_Shape(0) 的等价实现
    pMap->RepairMode = false;          // Repair_Mode_Control(0)
    pMap->SellMode   = false;          // Sell_Mode_Control(0)
    unk_88099A = unk_88099B = unk_88099C = 0;
    pMap->CurrentSWTypeIndex = idx;    // dword_8809A0 = idx
    TacticalClass::DeselectAll();      // Unselect_All
    VoxClass::PlayFromName("EVA_SelectTarget", -1, -1);
    goto cc_tail;

// ---------------------------------------------------------------------------
cc_right:   // asm 6AADB5: 右键
    if (pEntryFactory != nullptr) {
        // asm 6AADFD: 若待放置位置挂着 Techno，先清掉放置状态。
        if (Is_Techno_Object(pMap->CurrentBuilding)) {
            pMap->Cancel_Placement();
        }
        // asm 6AADFD: 生产线在推进且未被玩家挂起 -> 暂停；否则整单取消。
        if (pEntryFactory->Is_Producing() && !pEntryFactory->IsOnHold()) {
            // asm 6AAFE1: EVA_OnHold + ev_SUSPEND
            if (RulesClass::Instance != nullptr) {
                VocClass::PlayIndexAtPos(
                    RulesClass::Instance->GUIMainButtonSound, nullptr, 0);
            }
            VoxClass::PlayFromName("EVA_OnHold", -1, -1);
            if (pType != nullptr) {
                pHouse->SuspendProductionOf(pType);
            }
            // asm 6AB088: unk_880D68[[tab+38h] * 3988] = 1
            if (tab.TabIndex >= 0 && tab.TabIndex < MaxTabs) {
                unk_880D68[tab.TabIndex * 3988] = 1;
            }
        }
        else {
            // asm 6AAE13: EVA_Canceled + ev_ABANDON_ALL（左键）/ ev_ABANDON
            if (RulesClass::Instance != nullptr) {
                VocClass::PlayIndexAtPos(
                    RulesClass::Instance->GUIMainButtonSound, nullptr, 0);
            }
            VoxClass::PlayFromName("EVA_Canceled", -1, -1);
            if (pType != nullptr) {
                pHouse->AbandonProductionOf(pType);
            }
        }
        // 原版两条路径都落到 6AB28F。
        goto cc_main;
    }

    // asm 6AB0AC: 图标绑定的工厂槽为空 —— 直接对"已排队"的图标下手。
    if (pEntry->Status == 1) {
        pEntry->Status = 2;            // 1 -> 2（暂停）
        tab.NeedsRedraw = 1;
        GScreen_FlagToRedraw(nullptr, 0);
        if (RulesClass::Instance != nullptr) {
            VocClass::PlayIndexAtPos(RulesClass::Instance->GUIMainButtonSound,
                                     nullptr, 0);
        }
        VoxClass::PlayFromName("EVA_OnHold", -1, -1);
        if (pType != nullptr) {
            pHouse->SuspendProductionOf(pType);
        }
    }

    // asm 6AB18D: 取工厂，若该类型确实排在它的队列里就取消。
    pFac = pHouse->Fetch_Factory(abstractId,
                                 pType != nullptr && pType->Naval, buildCat);
    if (pFac != nullptr && pType != nullptr && pFac->IsXQueued(pType)) {
        if (RulesClass::Instance != nullptr) {
            VocClass::PlayIndexAtPos(RulesClass::Instance->GUIMainButtonSound,
                                     nullptr, 0);
        }
        pHouse->AbandonProductionOf(pType);
    }
    goto cc_main;

// ---------------------------------------------------------------------------
cc_main:   // asm 6AB28F: 左键主路径
    if ((a2 & 1) == 0) {
        goto cc_tail;
    }
    // asm 6AB296: 没有工厂，或"在推进但未被挂起" -> 走开工/继续分支。
    if (pEntryFactory == nullptr) {
        goto cc_resume;
    }
    if (pEntryFactory->Is_Producing() && !pEntryFactory->IsOnHold()) {
        goto cc_resume;
    }

    // asm 6AB2BB: 产品是否已完成？
    if (!pEntryFactory->IsCompleted()) {
        goto cc_notCompleted;
    }

    // asm 6AB2C3: 已完成 -> 取在线产品。
    pProduct = pEntryFactory->Get_Product();
    if (pProduct == nullptr) {
        // asm 6AB2CE: 特殊奖励项（-1 = 没有）-> 选超武槽 1。
        if (pEntryFactory->GetSpecialItem() == -1) {
            goto cc_tail;
        }
        pMap->CurrentSWTypeIndex = 1;
        goto cc_tail;
    }

    // asm 6AB2F6: 产品就绪提示音。
    if (RulesClass::Instance != nullptr) {
        VocClass::PlayIndexAtPos(RulesClass::Instance->GUIMainButtonSound,
                                 nullptr, 0);
    }

    // asm 6AB30F: product->vt+190h(0, 0) 取"要交付的对象"。OpenYR 的工厂在线
    //   产品本身就是待交付对象，故直接沿用 pProduct；对象已失效则按
    //   "无法执行"处理。
    pHandoff = pProduct;
    if (pHandoff == nullptr || pHandoff->IsDead()) {
        // asm 6AB325: ev_ABANDON + EVA_UnableToComply
        if (pType != nullptr) {
            pHouse->AbandonProductionOf(pType);
        }
        VoxClass::PlayFromName("EVA_UnableToComply", -1, -1);
        goto cc_tail;
    }

    // asm 6AB3BB: 建筑 -> sub_4FB840，进入放置模式。
    if (pHandoff->WhatAmI() == AbstractType::Building) {
        BuildingClass* pBldg = static_cast<BuildingClass*>(pHandoff);
        TacticalClass::DeselectAll();        // Unselect_All
        pMap->RepairMode = false;            // Repair_Mode_Control(0)
        pMap->SellMode   = false;            // Sell_Mode_Control(0)
        unk_88099A = unk_88099B = unk_88099C = 0;
        pMap->CurrentSWTypeIndex = -1;
        pMap->CurrentBuilding     = pBldg;   // unk_88098C
        pMap->CurrentBuildingType = pBldg->Type;  // ptr_BuildingTypeClass_ToPlace
        pMap->unknown_11AC = static_cast<DWORD>(pHouse->PlayerNumber);
        pMap->MarkToRedraw();
        goto cc_tail;
    }

    // asm 6AB3D9: ev_PLACE —— 把在线产品从工厂放出来（集结点、登记计数）。
    pHouse->UnitFromFactory(pProduct, pEntryFactory->GetFactoryBuilding(), false);
    goto cc_tail;

// ---------------------------------------------------------------------------
cc_notCompleted:   // asm 6AB463: 左键，工厂还没做完
    if (RulesClass::Instance != nullptr) {
        VocClass::PlayIndexAtPos(RulesClass::Instance->GUIMainButtonSound,
                                 nullptr, 0);
    }
    // asm 6AB47B: 步兵（abstractID == 10h）播 EVA_Training，其余 EVA_Building。
    VoxClass::PlayFromName(abstractId == 0x10 ? "EVA_Training" : "EVA_Building",
                           -1, -1);

    // asm 6AB4A4: 重新取类型，只为了拿到 naval 字节。
    pType2 = Fetch_Techno_Type(abstractId, idx);

    // asm 6AB4CC: EnableCounter(player, abstractID, type->Naval, BuildCat)。
    pHouse->Enable_Counter(abstractId, pType != nullptr && pType->Naval, buildCat);

    // asm 6AB4E6: ev_PRODUCE —— 再排一个（isNaval 由类型自带）。
    if (pType2 != nullptr) {
        pHouse->BeginProductionOf(pType2);
    }

    // asm 6AB573: status = 1；进度取平均后写回。
    pEntry->Status = 1;
    progressVal = (pEntryFactory != nullptr) ? pEntryFactory->GetProgress() : 0;
    curProgress = pEntry->Progress;
    avg = (curProgress > progressVal) ? (progressVal + curProgress) / 2
                                      : progressVal;
    pEntry->Progress = avg;

    // asm 6AB5B3: 进度结构（+14h/+8/+0Ch/+10h，以及 +28h）。
    frames = (pEntryFactory != nullptr)
                 ? pEntryFactory->GetBuildTimeFrames() + 1
                 : 1;
    pEntry->Frames2    = frames;          // +0x28
    pEntry->StartFrame = CurrentFrame;    // +0x1C
    pEntry->ThisSlot   = 0;               // +0x20（原版写未初始化的栈槽）
    pEntry->Frames     = frames;          // +0x24

    tab.NeedsRedraw = 1;                  // asm: [tab+3Ch] = 1
    GScreen_FlagToRedraw(nullptr, 0);
    SidebarNeedsRedraw = true;            // asm: byte_884B8F = 1
    goto cc_tail;

// ---------------------------------------------------------------------------
cc_resume:   // asm 6AB602: 没有工厂，或"在推进且未被挂起"
    pEntryFactory = pHouse->Fetch_Factory(
        abstractId, pType != nullptr && pType->Naval, buildCat);

    // asm 6AB60C: 重新取类型（naval 字节写到 Fact_ 槽的低字节）。
    pType2 = Fetch_Techno_Type(abstractId, idx);

    // asm 9262: var_99 = ShouldDisableCameo(player, type)
    leftClick = pHouse->ShouldDisableCameo(pType2);

    // asm 9264: paused = (在推进 && 未被挂起) || 有在线产品 || 队列非空。
    paused = false;
    if (pEntryFactory != nullptr) {
        if ((pEntryFactory->Is_Producing() && !pEntryFactory->IsOnHold()) ||
            pEntryFactory->Get_Product() != nullptr ||
            pEntryFactory->Has_Queued()) {
            paused = true;
        }
    }

    if (paused) {
        // asm 6AB67F: var_8D = 1；建筑（abstractID == 7）报"无法执行"。
        if (abstractId == 7) {
            VoxClass::PlayFromName("EVA_UnableToComply", -1, -1);
            goto cc_tail;
        }
    }
    else {
        // asm 6AB69D: var_8D = 0；未置灰时播建造语音。
        if (!leftClick) {
            VoxClass::PlayFromName(
                abstractId == 0x10 ? "EVA_Training" : "EVA_Building", -1, -1);
        }
    }

    // asm 6AB6CE: counter = GetCounter(player, abstractID, type->Naval, BuildCat)
    counter = pHouse->Get_Counter(abstractId, pType != nullptr && pType->Naval,
                                  buildCat);

    enableNew = false;
    if (!leftClick) {
        if (RulesClass::Instance != nullptr) {
            VocClass::PlayIndexAtPos(RulesClass::Instance->GUIMainButtonSound,
                                     nullptr, 0);
        }
        // asm 6AB72C: 没有工厂且计数为零 -> 点亮计数并记下"本次新开"。
        if (pEntryFactory == nullptr && !counter) {
            pHouse->Enable_Counter(abstractId,
                                   pType != nullptr && pType->Naval, buildCat);
            enableNew = true;
        }
        // asm 6AB741: ev_PRODUCE（isNaval 由类型自带）。
        if (pType2 != nullptr) {
            pHouse->BeginProductionOf(pType2);
        }
    }

    // asm 6AB7CC / 6AB7D4 / 6AB7E0: 只要（新开、无计数、非暂停、非置灰）
    //   任一不满足就收尾。
    if (!enableNew) {
        if (counter || paused || leftClick) {
            goto cc_tail;
        }
    }

    // asm 9431: status = 1；标签页置脏并请求重绘。
    pEntry->Status = 1;
    tab.NeedsRedraw = 1;
    GScreen_FlagToRedraw(nullptr, 0);

    // asm 9443: 再取一次类型，用来估算进度条格数。
    pType2 = Fetch_Techno_Type(abstractId, idx);
    if (pType2 == nullptr) {
        goto cc_tail;
    }

    // asm 9452: need = (GetActualCost(owner) * Factory_BuildSpeedMult) / 54。
    buildSpeedMult = Factory_BuildSpeedMult;
    funds = pType2->GetActualCost(pHouse);          // asm: vt+84h(type, house)
    x = static_cast<int32>(static_cast<uint32>(funds) *
                           static_cast<uint32>(buildSpeedMult));
    full = static_cast<int64>(0x4BDA12F7) * static_cast<int64>(x);
    hi   = static_cast<int32>(full >> 32);
    need = hi / 16;                                 // asm: sar 4 + 符号修正

    // asm 9465: 可用资金不足 -> 收尾（asm: vt+18h(house+24h)）。
    if (pHouse->GetAvailableMoney() < need) {
        goto cc_tail;
    }
    // asm 9477: vt+94h(type, 1, 0, 0, house) -> 可否建造。
    if (!pHouse->CanBuild(pType2)) {
        goto cc_tail;
    }

    // asm 9489: cost = vt+88h(type)；建筑且是墙则按 Rules 的墙体系数折算。
    cost = pType2->GetCost();
    if (pType2->WhatAmI() == AbstractType::BuildingType &&
        static_cast<BuildingTypeClass*>(pType2)->IsWall()) {
        const double wallFactor = (RulesClass::Instance != nullptr)
                                      ? RulesClass::Instance
                                            ->WallBuildSpeedCoefficient
                                      : 0.0;
        cost = static_cast<int32>(std::floor(static_cast<double>(cost) *
                                             wallFactor));
    }

    // asm 6AB8DC: cost = max(cost, BuildSpeedMult + 15)。
    lower = buildSpeedMult + 15;
    if (cost <= lower) {
        cost = lower;
    }

    // asm 6AB8E9: q = cost / 54，再按 54*q < lower 进一位。
    full = static_cast<int64>(0x4BDA12F7) * static_cast<int64>(cost);
    hi   = static_cast<int32>(full >> 32);
    q    = hi / 16;
    q54  = static_cast<int32>(static_cast<uint32>(q) * 54u);
    if (q54 < lower) {
        ++q;
    }
    // asm 6AB907: 夹到 [1, 0FFh]。
    if (q < 1) {
        q = 1;
    }
    if (q > 0xFF) {
        q = 0xFF;
    }

    // asm 6AB920: 清零进度并写入进度结构。
    pEntry->Progress   = 0;
    pEntry->Frames2    = q;               // +0x28
    pEntry->StartFrame = CurrentFrame;    // +0x1C
    pEntry->ThisSlot   = 0;               // +0x20（原版写未初始化的栈槽）
    pEntry->Frames     = q;               // +0x24
    goto cc_tail;

// ---------------------------------------------------------------------------
cc_tail:   // asm 6AB94F / 6AB95A: Control_7C(Map, a2, a3, 0)；返回 1
    // 原版把点击结果交回控件层（ShapeButtonClass::Control_7C）：它负责刷新
    // 控件状态并转发给子控件。OpenYR 的控件层以"标脏 + 请求重绘"表达同一
    // 结果，故这里把本标签页与地图一起置脏。
    (void)a3;
    tab.NeedsRedraw = 1;
    pMap->MarkToRedraw();
    return 1;
}
