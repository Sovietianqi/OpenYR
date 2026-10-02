#include "MapSelection.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdlib>

// ============================================================================
// MapSelection - sub_5CF8E0 (asm 0x5CF8E0)
// ============================================================================

static char* DupString(const char* pSource)
{
    if (pSource == nullptr || pSource[0] == '\0') {
        return nullptr;
    }
    const size_t len = std::strlen(pSource);
    char* pCopy = new char[len + 1];
    std::memcpy(pCopy, pSource, len + 1);
    return pCopy;
}

MapSelection::MapSelection()
    : pScenario(nullptr)
    , pDescription(nullptr)
    , pVoiceOver(nullptr)
    , pMapVQ(nullptr)
    , pOverlays(nullptr)
    , OverlayCount(0)
    , pClickMap(nullptr)
{
    Extra[0] = nullptr;
    Extra[1] = nullptr;
}

MapSelection::~MapSelection()
{
    Clear();
}

void MapSelection::Clear()
{
    delete[] pScenario;
    delete[] pDescription;
    delete[] pVoiceOver;
    delete[] pMapVQ;
    delete[] pOverlays;
    delete[] pClickMap;
    delete[] Extra[0];
    delete[] Extra[1];

    pScenario    = nullptr;
    pDescription = nullptr;
    pVoiceOver   = nullptr;
    pMapVQ       = nullptr;
    pOverlays    = nullptr;
    pClickMap    = nullptr;
    Extra[0]     = nullptr;
    Extra[1]     = nullptr;

    Targets.Clear();
    TextEntries.Clear();
    Children.Clear();
}

bool MapSelection::ReadFromINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return false;
    }

    char buffer[0x100];

    // ── "Scenario" ──────────────────────────────────────────────────────
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Scenario", "", buffer, sizeof(buffer)) > 0) {
        pScenario = DupString(buffer);
    } else {
        return false;
    }

    // ── "Description" ───────────────────────────────────────────────────
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Description", "", buffer, sizeof(buffer)) > 0) {
        pDescription = DupString(buffer);
    }

    // ── "VoiceOver" / "MapVQ" are localised string table entries ────────
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "VoiceOver", "", buffer, sizeof(buffer)) > 0) {
        pVoiceOver = DupString(buffer);
    }

    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "MapVQ", "", buffer, sizeof(buffer)) > 0) {
        pMapVQ = DupString(buffer);
    }

    // ── "Overlays" ──────────────────────────────────────────────────────
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Overlays", "", buffer, sizeof(buffer)) > 0) {
        pOverlays = DupString(buffer);
    }

    // ── "ClickMap" ──────────────────────────────────────────────────────
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "ClickMap", "", buffer, sizeof(buffer)) > 0) {
        pClickMap = DupString(buffer);
    }

    // ── "Targets" ───────────────────────────────────────────────────────
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Targets", "", buffer, sizeof(buffer)) > 0) {
        char* pToken = std::strtok(buffer, ",");
        while (pToken != nullptr) {
            Targets.Add(std::atoi(pToken));
            pToken = std::strtok(nullptr, ",");
        }
    }

    return true;
}
