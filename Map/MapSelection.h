#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;

// ============================================================================
// MapSelection
//
//   One selectable map in the single player map chooser.  Built by
//   sub_5CF8E0 (asm 0x5CF8E0) from a section of the mission INI.
//
//   Layout follows the original:
//     +00  char*  pScenario            ("Scenario")
//     +04  char*  pDescription         ("Description")
//     +08  char*  pVoiceOver           ("VoiceOver", localised)
//     +0C  char*  pMapVQ               ("MapVQ", localised)
//     +10  char*  pOverlays            ("Overlays")
//     +14  int32  OverlayCount         (+20h in the original object)
//     +18  char*  Extra[2]
//     +20  char*  pClickMap            ("ClickMap", localised)
//     +24  DynamicVectorClass<...>     targets
//     +3C  DynamicVectorClass<...>     text entries
//     +54  DynamicVectorClass<MapSelection*> child selections
// ============================================================================

class MapSelection
{
public:
    MapSelection();
    ~MapSelection();

    // sub_5CF8E0: pSection is the INI section to read.
    bool ReadFromINI(CCINIClass* pINI, const char* pSection);

    void Clear();

    char*       pScenario;
    char*       pDescription;
    char*       pVoiceOver;
    char*       pMapVQ;
    char*       pOverlays;
    int32       OverlayCount;
    char*       Extra[2];
    char*       pClickMap;

    DynamicVectorClass<int32>  Targets;
    DynamicVectorClass<int32>  TextEntries;
    DynamicVectorClass<MapSelection*> Children;
};
