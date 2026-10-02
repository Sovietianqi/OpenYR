#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Map/MapSelection.h"

class CCINIClass;

// ============================================================================
// MapSelectClass
//
//   Backing layout for the World Domination Tour selection screen built by
//   sub_7681E0 (asm 0x7681E0).  The screen reads a MAPSELMD.INI style
//   [section] whose name is derived from the current thematic prefix and the
//   configured "MAPS" entry.
//
//   Field offsets below follow the original object (+0x44 onward):
//     +44  OriginX / OriginY        (computed centring offsets)
//     +4C  BackgroundRect
//     +54  BackButtonRect
//     +64  pBackButton
//     +6C  BackButtonNormalFrame
//     +70  BackButtonDepressedFrame
//     +78  pBackButtonPalette
//     +7C  TextRect
//     +8C  TooltipRect
//     +9C  TitleRect
//     +AC  BackTextRect
// ============================================================================

class MapSelectClass
{
public:
    MapSelectClass();
    ~MapSelectClass();

    static MapSelectClass* GetInstance();

    // sub_7681E0: reads the whole screen definition for one thematic prefix.
    void ReadScreen(CCINIClass* pINI, const char* pPrefix);

    // sub_7691E0: loads the default palette referenced by "DefaultPalette".
    bool LoadDefaultPalette(const char* pPaletteName);

    // sub_5CEEF0: loads MAPSELMD.INI and builds the mission selection list.
    bool LoadMissionList();

    // sub_76CBF0: reads one World Domination Tour target definition.
    void ReadTarget(CCINIClass* pINI, const char* pSection);

    // sub_768F50: constructs one territory/logo animation entry.
    void* CreateAnimation(CCINIClass* pINI, const char* pPrefix,
                          const char* pKey, int32 index);

    void Clear();

    // ── Geometry ────────────────────────────────────────────────────────
    int32               OriginX;
    int32               OriginY;

    RectangleStruct     BackgroundRect;   // "TextRect" base anchor region
    RectangleStruct     BackButtonRect;   // "BackButtonRectangle"
    RectangleStruct     TextRect;         // "TextRect"
    RectangleStruct     TooltipRect;      // "TooltipRect"
    RectangleStruct     TitleRect;        // "TitleRect"
    RectangleStruct     BackTextRect;     // "BackTextRect"

    int32               SideBarOriginX;   // derived: SideBarSize.Width
    int32               SideBarOriginY;
    int32               HelpBarOriginX;   // derived: HelpBarSize.Height

    // ── Screen size ─────────────────────────────────────────────────────
    int32               SizeWidth;        // "Size" (default 0x280)
    int32               SizeHeight;       // "Size" (default 0x190)

    // ── Animation / artwork names ───────────────────────────────────────
    char                ScreenName[0x100];        // "MAPS"
    char                ArtPrefix[0x100];         // "Art_" + ScreenWidth
    char                Art800Name[0x100];        // "Art_800"
    char                OpeningName[0x100];       // "Opening"
    char                BackgroundName[0x100];    // "Background"
    char                ClickMapName[0x100];      // "ClickMap"
    char                SideBarName[0x100];       // "SideBar"
    char                HelpBarName[0x100];       // "HelpBar"
    char                BackButtonName[0x100];    // "BackButton"
    char                BackButtonHighlighted[0x100]; // "BackButtonHighlighted"
    char                BackButtonPalette[0x100]; // "BackButtonPalette"
    char                OverlayPrefix[0x80];      // "OverlayPrefix"
    char                OverlayPalette[0x80];     // "OverlayPalette"
    char                NameKeyPrefix[0x80];      // "NameKeyPrefix"
    char                DefaultPalette[0x80];     // "DefaultPalette"

    // ── Frame indices ───────────────────────────────────────────────────
    int32               BackButtonNormalFrame;    // "BackButtonNormalFrame"
    int32               BackButtonDepressedFrame; // "BackButtonDepressedFrame"

    // ── Menu items ──────────────────────────────────────────────────────
    void*               pOpeningAnim;       // +0x18
    void*               pBackgroundAnim;    // +0x1C
    void*               pHelpBarAnim;       // +0x20
    void*               pClickMapSurface;   // +0x24
    int32               ClickMapAux;        // +0x28 (OverlayPrefix payload)
    int32               TerritoryCount;     // +0x3C

    bool                ShowDefaultPalette;

    // ── World Domination Tour target (sub_76CBF0) ───────────────────────
    char                ZoomingTarget[0x100];   // "ZoomingTarget"
    char                TargetPalette[0x100];   // "TargetPalette"
    char                TargetName[0x100];      // "Target"
    int32               TargetDividingFrame;    // "TargetDividingFrame"

    // ── Mission selection list (sub_5CEEF0) ─────────────────────────────
    DynamicVectorClass<MapSelection*> MissionList;

    // ── Territory name (sub_788180) ─────────────────────────────────────
    // The tour reads "Territory%02d" from the mission INI, resolves the
    // region, then reads the region's "Name" from either the "NorthAmerica"
    // or the "Europe" section of that INI depending on the region flag.
    wchar_t*            TerritoryName;      // wcsdup of the resolved name
    bool                TerritoryIsEurope;

    // Reads the display name for territory index nIndex.
    void ReadTerritoryName(CCINIClass* pINI, int32 nIndex);

    // Reads the VoiceOvers / SOUNDS / LAYOUTS name lists.
    bool ReadLayouts(CCINIClass* pINI);

    static const char* const SECTION_NORTH_AMERICA;  // "NorthAmerica"
    static const char* const SECTION_EUROPE;         // "Europe"
    static const char* const KEY_NAME;               // "Name"
    static const char* const DEFAULT_NAME;           // "A territory"

    // ── Tour mission entry (WorldDominationTour::Selection) ─────────────
    //   The selection screen reads, in order: "VoiceOvers" (sub_76C970),
    //   "SOUNDS" (sub_76CA60), "LAYOUTS", then the target block through
    //   sub_76CBF0 and the screen itself through sub_7681E0.
    char                Layouts[0x100];      // "LAYOUTS"
    char                VoiceOvers[0x100];   // "VoiceOvers"
    char                Sounds[0x100];       // "SOUNDS"
};
