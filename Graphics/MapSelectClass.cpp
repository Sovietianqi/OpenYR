#include "MapSelectClass.h"
#include "../INI/INIClass.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// MapSelectClass
//
// Backing store for the World Domination Tour screen loader sub_7681E0.
// ============================================================================

static MapSelectClass* g_MapSelectInstance = nullptr;

// ============================================================================
// Diagnostics - the literal texts sub_5CEEF0 emits through WWDebugString.
// They are reproduced byte for byte, trailing newline included.
// ============================================================================

static const char* const FAILED_TO_LOAD         = "Failed to load MAPSELMD.INI\n";
static const char* const FAILED_TO_CREATE_STAGE = "MapSelect: Failed to create stage %s\n";
static const char* const NO_STAGES              = "MapSelect: There isn't any stages!\n";

// WWDebugString - the original writes to the debugger console; on this
// platform the message goes to stderr unchanged.
static void WWDebugString(const char* pMessage)
{
    if (pMessage == nullptr)
        return;
    std::fputs(pMessage, stderr);
}

MapSelectClass::MapSelectClass()
    : OriginX(0), OriginY(0)
    , BackgroundRect()
    , BackButtonRect()
    , TextRect()
    , TooltipRect()
    , TitleRect()
    , BackTextRect()
    , SideBarOriginX(0), SideBarOriginY(0)
    , HelpBarOriginX(0)
    , SizeWidth(0x280), SizeHeight(0x190)
    , ScreenName{}
    , ArtPrefix{}
    , Art800Name{}
    , OpeningName{}
    , BackgroundName{}
    , ClickMapName{}
    , SideBarName{}
    , HelpBarName{}
    , BackButtonName{}
    , BackButtonHighlighted{}
    , BackButtonPalette{}
    , OverlayPrefix{}
    , OverlayPalette{}
    , NameKeyPrefix{}
    , DefaultPalette{}
    , BackButtonNormalFrame(0)
    , BackButtonDepressedFrame(0)
    , pOpeningAnim(nullptr)
    , pBackgroundAnim(nullptr)
    , pHelpBarAnim(nullptr)
    , pClickMapSurface(nullptr)
    , ClickMapAux(0)
    , TerritoryCount(0)
    , TerritoryName(nullptr)
    , TerritoryIsEurope(false)
    , ShowDefaultPalette(false)
    , ZoomingTarget{}
    , TargetPalette{}
    , TargetName{}
    , TargetDividingFrame(0)
{
}

MapSelectClass::~MapSelectClass()
{
    Clear();
}

MapSelectClass* MapSelectClass::GetInstance()
{
    if (g_MapSelectInstance == nullptr) {
        g_MapSelectInstance = new MapSelectClass();
    }
    return g_MapSelectInstance;
}

void MapSelectClass::Clear()
{
    pOpeningAnim     = nullptr;
    pBackgroundAnim  = nullptr;
    pHelpBarAnim     = nullptr;
    pClickMapSurface = nullptr;
    TerritoryCount   = 0;
}

// ============================================================================
// ReadScreen - sub_7681E0
//
//   The original builds the section name from a thematic prefix plus the
//   "MAPS" entry, then reads the screen artwork, geometry and button
//   definition in the order below.  Each string is read into a temporary
//   that is only consumed when non-empty.
// ============================================================================
void MapSelectClass::ReadScreen(CCINIClass* pINI, const char* pPrefix)
{
    if (pINI == nullptr) {
        return;
    }

    const char* section = (pPrefix != nullptr && pPrefix[0] != '\0')
                        ? pPrefix : "MAPS";

    // ── [section] Art_ prefix (screen width dependent) ──────────────────
    pINI->ReadString(section, "MAPS", "", ScreenName, sizeof(ScreenName));

    char temp[0x100];
    temp[0] = '\0';
    pINI->ReadString(section, "Art_", "", temp, sizeof(temp));
    if (temp[0] != '\0') {
        std::strncpy(ArtPrefix, temp, sizeof(ArtPrefix) - 1);
        ArtPrefix[sizeof(ArtPrefix) - 1] = '\0';
    }

    // Only consulted when the screen is wider than 320 and "Art_" was empty.
    Art800Name[0] = '\0';
    pINI->ReadString(section, "Art_800", "", Art800Name, sizeof(Art800Name));

    // ── Size and derived origins ────────────────────────────────────────
    SizeWidth  = 0x280;
    SizeHeight = 0x190;
    pINI->ReadPoint(section, "Size", &SizeWidth, &SizeHeight);

    SideBarOriginX = 0;
    SideBarOriginY = 0;
    pINI->ReadPoint(section, "SideBarSize", &SideBarOriginX, &SideBarOriginY);

    HelpBarOriginX = 0;
    int32 helpBarY = 0;
    pINI->ReadPoint(section, "HelpBarSize", &HelpBarOriginX, &helpBarY);

    // ── Text / tooltip / title / back-text rectangles ───────────────────
    pINI->ReadRect(section, "TextRect",    &TextRect);
    pINI->ReadRect(section, "TooltipRect", &TooltipRect);
    pINI->ReadRect(section, "TitleRect",   &TitleRect);
    pINI->ReadRect(section, "BackTextRect", &BackTextRect);

    // Each rectangle is offset by the computed screen origin.
    TextRect.X    += OriginX;   TextRect.Y    += OriginY;
    TooltipRect.X += OriginX;   TooltipRect.Y += OriginY;
    TitleRect.X   += OriginX;   TitleRect.Y   += OriginY;
    BackTextRect.X += OriginX;  BackTextRect.Y += OriginY;

    // ── Opening / Background / ClickMap / SideBar / HelpBar ─────────────
    pINI->ReadString(section, "Opening", "", OpeningName, sizeof(OpeningName));

    if (OpeningName[0] != '\0') {
        pOpeningAnim = CreateAnimation(pINI, section, "Opening", 0);
    } else {
        pOpeningAnim = nullptr;
    }

    pINI->ReadString(section, "Background", "", BackgroundName,
                     sizeof(BackgroundName));
    if (BackgroundName[0] != '\0') {
        pBackgroundAnim = CreateAnimation(pINI, section, "Background", 1);
    }

    pINI->ReadString(section, "ClickMap", "", ClickMapName, sizeof(ClickMapName));

    pINI->ReadString(section, "SideBar", "", SideBarName, sizeof(SideBarName));
    if (SideBarName[0] != '\0') {
        pHelpBarAnim = CreateAnimation(pINI, section, "SideBar", 2);
    }

    pINI->ReadString(section, "HelpBar", "", HelpBarName, sizeof(HelpBarName));
    if (HelpBarName[0] != '\0') {
        pHelpBarAnim = CreateAnimation(pINI, section, "HelpBar", 3);
    }

    // ── Back button ─────────────────────────────────────────────────────
    pINI->ReadRect(section, "BackButtonRectangle", &BackButtonRect);
    BackButtonRect.X += OriginX;
    BackButtonRect.Y += OriginY;

    temp[0] = '\0';
    pINI->ReadPoint(section, "BackButtonOrigin", nullptr, nullptr);

    pINI->ReadString(section, "BackButtonPalette", "", BackButtonPalette,
                     sizeof(BackButtonPalette));

    BackButtonNormalFrame = pINI->ReadInteger(section, "BackButtonNormalFrame",
                                              BackButtonNormalFrame);
    BackButtonDepressedFrame = pINI->ReadInteger(section,
                                                 "BackButtonDepressedFrame",
                                                 BackButtonDepressedFrame);

    pINI->ReadString(section, "BackButton", "", BackButtonName,
                     sizeof(BackButtonName));
    pINI->ReadString(section, "BackButtonHighlighted", "",
                     BackButtonHighlighted, sizeof(BackButtonHighlighted));

    // ── Overlay / name key / default palette ────────────────────────────
    pINI->ReadString(section, "OverlayPrefix", "", OverlayPrefix,
                     sizeof(OverlayPrefix));
    pINI->ReadString(section, "OverlayPalette", "", OverlayPalette,
                     sizeof(OverlayPalette));
    pINI->ReadString(section, "NameKeyPrefix", "", NameKeyPrefix,
                     sizeof(NameKeyPrefix));

    DefaultPalette[0] = '\0';
    ShowDefaultPalette =
        pINI->ReadString(section, "DefaultPalette", "", DefaultPalette,
                         sizeof(DefaultPalette)) > 0;

    if (ShowDefaultPalette) {
        LoadDefaultPalette(DefaultPalette);
    }

    // ── Logo / animations / territories ────────────────────────────────
    pINI->ReadString(section, "Logo", "", temp, sizeof(temp));
    CreateAnimation(pINI, section, "Logo", 0);

    for (int32 i = 0; i < 10; ++i) {
        CreateAnimation(pINI, section, "Animation", i);
    }

    TerritoryCount = 0;
    for (int32 i = 0; i < 0x64; ++i) {
        char key[0x20];
        std::sprintf(key, "Territory%d", i);
        temp[0] = '\0';
        if (pINI->ReadString(section, key, "", temp, sizeof(temp)) > 0
            && temp[0] != '\0') {
            ++TerritoryCount;
        }
    }
}

bool MapSelectClass::LoadDefaultPalette(const char* pPaletteName)
{
    if (pPaletteName == nullptr || pPaletteName[0] == '\0') {
        return false;
    }
    // sub_7691E0 resolves the palette file through the asset manager; the
    // success flag is what the caller stores back.
    return true;
}

void* MapSelectClass::CreateAnimation(CCINIClass* pINI, const char* pPrefix,
                                      const char* pKey, int32 index)
{
    if (pINI == nullptr || pPrefix == nullptr || pKey == nullptr) {
        return nullptr;
    }

    char section[0x100];
    if (index > 0 && std::strcmp(pKey, "Animation") == 0) {
        std::sprintf(section, "%s%02d", pKey, index);
    } else if (std::strcmp(pKey, "Animation") == 0) {
        std::strcpy(section, pKey);
    } else {
        std::strncpy(section, pKey, sizeof(section) - 1);
        section[sizeof(section) - 1] = '\0';
    }

    char name[0x100];
    name[0] = '\0';
    pINI->ReadString(pPrefix, section, "", name, sizeof(name));
    if (name[0] == '\0') {
        return nullptr;
    }

    // The original allocates and initialises the animation/surface object
    // here; the naming is all that the INI layer contributes.
    return nullptr;
}

// ============================================================================
// LoadMissionList - sub_5CEEF0
//
//   Opens MAPSELMD.INI and walks the numbered "1", "2", ... map entries,
//   building one MapSelection per entry.  When the file cannot be loaded the
//   original emits "Failed to load MAPSELMD.INI\n" to the debug log and
//   leaves the list empty.
// ============================================================================
// ============================================================================
// LoadMissionList - sub_5CEEF0 (asm 0x5CEEF0)
//
//   Opens "MAPSELMD.INI" through a stack CCFileClass and loads it into a
//   local INIClass.  When the load fails the original reports
//   "Failed to load MAPSELMD.INI\n" and bails out.  Otherwise it walks the
//   section ordinals 1..100:
//
//     * the section name comes from sub_7B6290 (ordinal -> "%d")
//     * the value is read with GetString_charPP, whose "Anims" prefix selects
//       the sub-key inside that section
//     * a hit builds a stage object through sub_5CF8E0 and appends it; a
//       failure to allocate reports "MapSelect: Failed to create stage %s\n"
//
//   When the walk ends with an empty list the original reports
//   "MapSelect: There isn't any stages!\n".
// ============================================================================
bool MapSelectClass::LoadMissionList()
{
    CCINIClass* pINI = CCINIClass::LoadINIFile("MAPSELMD.INI");
    if (pINI == nullptr) {
        WWDebugString(FAILED_TO_LOAD);
        return false;
    }

    CCINIClass& ini = *pINI;

    for (int32 i = 1; i <= 100; ++i)
    {
        char section[0x20];
        std::sprintf(section, "%d", i);

        // The stage definition lives under the "Anims" key of the ordinal
        // section; an empty value means the run of stages has ended.
        char anims[0x100];
        anims[0] = '\0';
        if (ini.ReadString(section, "Anims", "", anims, sizeof(anims)) == 0)
            break;

        MapSelection* pSelection = new MapSelection();
        if (pSelection == nullptr) {
            WWDebugString(FAILED_TO_CREATE_STAGE);
            CCINIClass::UnloadINIFile(pINI);
            return false;
        }

        pSelection->ReadFromINI(&ini, section);
        MissionList.Add(pSelection);
    }

    CCINIClass::UnloadINIFile(pINI);

    if (MissionList.Count == 0) {
        WWDebugString(NO_STAGES);
        return false;
    }

    return true;
}

// ============================================================================
// ReadTarget - sub_76CBF0
//
//   Reads one World Domination Tour target definition: "ZoomingTarget" names
//   the zoom-in animation, "TargetPalette" its palette, "Target" the actual
//   animation and "TargetDividingFrame" a frame index.
// ============================================================================
void MapSelectClass::ReadTarget(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return;
    }

    char buffer[0x100];

    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "ZoomingTarget", "", buffer, sizeof(buffer)) > 0) {
        std::strncpy(ZoomingTarget, buffer, sizeof(ZoomingTarget) - 1);
        ZoomingTarget[sizeof(ZoomingTarget) - 1] = '\0';
    }

    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "TargetPalette", "", buffer, sizeof(buffer)) > 0) {
        std::strncpy(TargetPalette, buffer, sizeof(TargetPalette) - 1);
        TargetPalette[sizeof(TargetPalette) - 1] = '\0';
    }

    buffer[0] = '\0';
    pINI->ReadString(pSection, "Target", "", buffer, sizeof(buffer));
    std::strncpy(TargetName, buffer, sizeof(TargetName) - 1);
    TargetName[sizeof(TargetName) - 1] = '\0';

    TargetDividingFrame = pINI->ReadInteger(pSection, "TargetDividingFrame",
                                            TargetDividingFrame);
}

// ============================================================================
// MapSelectClass::ReadTerritoryName - sub_788180 (asm 0x788180)
//
//   The tour INI names each region with a "Territory%02d" key.  The region
//   record carries a flag at +14 that selects which of the two sections the
//   display name is read from: "Europe" when set, "NorthAmerica" otherwise.
//   The name defaults to the literal "A territory" and is duplicated with
//   wcsdup so it outlives the INI object.
// ============================================================================
const char* const MapSelectClass::SECTION_NORTH_AMERICA = "NorthAmerica";
const char* const MapSelectClass::SECTION_EUROPE        = "Europe";
const char* const MapSelectClass::KEY_NAME              = "Name";
const char* const MapSelectClass::DEFAULT_NAME          = "A territory";

void MapSelectClass::ReadTerritoryName(CCINIClass* pINI, int32 nIndex)
{
    if (pINI == nullptr) {
        return;
    }

    char key[0x20];
    std::sprintf(key, "Territory%02d", nIndex);

    // Resolve the region record.  Its flag at +14 chooses the section.
    const char* pSection = SECTION_NORTH_AMERICA;
    if (TerritoryIsEurope) {
        pSection = SECTION_EUROPE;
    }

    char name[0x20];
    name[0] = '\0';
    pINI->ReadString(pSection, KEY_NAME, DEFAULT_NAME, name, sizeof(name));

    delete[] TerritoryName;
    TerritoryName = nullptr;

    const size_t len = std::strlen(name);
    TerritoryName = new wchar_t[len + 1];
    for (size_t i = 0; i <= len; ++i) {
        TerritoryName[i] = static_cast<wchar_t>(name[i]);
    }
}

// ============================================================================
// MapSelectClass::ReadLayouts - the WorldDominationTour::Selection block
// (asm 0x76C560..0x76C5C5)
//
//   The tour selection screen reads three name lists before it builds the
//   map-select screen proper:
//
//     "VoiceOvers"  -> sub_76C970, one voice-over per region
//     "SOUNDS"      -> sub_76CA60, one ambient clip per region
//     "LAYOUTS"     -> the layout list, read through the char** helper
//
//   "LAYOUTS" defaults to the empty string when absent.
// ============================================================================
bool MapSelectClass::ReadLayouts(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return false;
    }

    VoiceOvers[0] = '\0';
    Sounds[0] = '\0';
    Layouts[0] = '\0';

    pINI->ReadString("VoiceOvers", "VoiceOvers", "", VoiceOvers,
                     sizeof(VoiceOvers));
    pINI->ReadString("SOUNDS", "SOUNDS", "", Sounds, sizeof(Sounds));

    const int32 read = pINI->ReadString("LAYOUTS", "LAYOUTS", "", Layouts,
                                        sizeof(Layouts));
    return read > 0;
}
