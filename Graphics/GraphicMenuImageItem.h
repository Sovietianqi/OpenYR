#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// GraphicMenuImageItem
//
//   One image backed clickable entry inside a graphic menu definition.
//   GraphicMenuImageItem_Read_INI (asm 0x4F314D) reads it from a section of
//   the menu INI; the item is discarded when "ID" is missing (-1).
//
//   Layout follows the original:
//     +00  int32 ID                    ("ID", must not be -1)
//     +04  Point Origin                ("Origin", added to ActiveRect)
//     +0C  Rectangle ActiveRect        ("ActiveRect", offset by Origin)
//     +1C  char* pImage                ("Image")
//     +20  char* pHighlighted          ("Highlighted")
//     +24  char* pDisabled             ("Disabled")
//     +28  char* pHighlightSound       ("HighlightSound")
//     +2C  char* pSelectVQ             ("SelectVQ")
// ============================================================================

class GraphicMenuImageItem
{
public:
    GraphicMenuImageItem();
    ~GraphicMenuImageItem();

    // GraphicMenuImageItem_Read_INI: pSection is the menu section to read.
    static GraphicMenuImageItem* ReadFromINI(CCINIClass* pINI,
                                             const char* pSection);

    void Clear();

    int32           ID;
    int32           OriginX;
    int32           OriginY;
    RectangleStruct ActiveRect;

    char*           pImage;
    char*           pHighlighted;
    char*           pDisabled;
    char*           pHighlightSound;
    char*           pSelectVQ;
};
