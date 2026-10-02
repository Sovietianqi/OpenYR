#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;
class GraphicMenuImageItem;

// ============================================================================
// GraphicMenuItem - base of every clickable item inside a graphic menu.
//
//   The original's vtable slots line up with the three concrete item kinds:
//
//     +00  vtable
//     +04  int32 Type              ("Type", "Image" / "Shortcut" / "Anim")
//     +08  char* pSelectVQ         ("SelectSound", optional)
//     +0C  void* pPayload          the concrete item object
//
//   GraphicMenuItem_Read_INI (asm 0x4F378F) dispatches on the "Type" key:
//     "Image"    -> GraphicMenuImageItem_Read_INI
//     "Shortcut" -> GraphicMenuShortcutItem_Read_INI
//     "Anim"     -> GraphicMenuAnimItem_Read_INI_4F2C70
//   The concrete reader fills the payload and returns the derived object,
//   which the dispatcher stores at +0C.  When "SelectSound" names a file the
//   dispatcher allocates a 0x10 byte sound ref and stores it at +0C (the
//   original reuses the slot once the sound is present), which is why the
//   audio reference is wrapped in its own small handle here.
// ============================================================================

class GraphicMenuItem
{
public:
    virtual ~GraphicMenuItem();

    // GraphicMenuItem_Read_INI (asm 0x4F378F)
    static GraphicMenuItem* ReadFromINI(CCINIClass* pINI,
                                        const char* pSection,
                                        int32 arg0,
                                        int32 arg1);

    int32           Type;
    char*           pSelectVQ;
    void*           pPayload;
};

// ============================================================================
// GraphicMenuShortcutItem - a keyboard-driven menu entry.
//
//   GraphicMenuShortcutItem_Read_INI (asm 0x4F3B4E) reads "ID" and "Keys";
//   an "ID" of -1 discards the item.  The constructor called at the end
//   (sub_4F3C40) allocates the 0x28 byte object that binds the id to the
//   key list.  Type is always Shortcut (1).
// ============================================================================

class GraphicMenuShortcutItem : public GraphicMenuItem
{
public:
    GraphicMenuShortcutItem();
    virtual ~GraphicMenuShortcutItem() override;

    static GraphicMenuShortcutItem* ReadFromINI(CCINIClass* pINI,
                                                const char* pSection);

    int32   ID;
    char*   pKeys;
    int32   field_08;
    int32   field_0C;
};

// Item type tokens the "Type" key accepts.
enum GraphicMenuItemType : int32 {
    GraphicMenuItem_Image    = 0,
    GraphicMenuItem_Shortcut = 1,
    GraphicMenuItem_Anim     = 2
};

// ============================================================================
// GraphicMenu - one [<name>] section of a graphic menu INI.
//
//   GraphicMenu::GraphicMenu (asm 0x4F1C64) reads:
//     "Background"  optional image name (read via INIClass_GetString_charPP)
//     "Theme"       optional theme name, resolved through the theme index
//     "ItemMax"     int, default 0x64, the number of "%d" entries to walk
//
//   Each numbered "%d" entry names a sub-section that GraphicMenuItem reads.
//   The item list starts with a capacity of 10.
// ============================================================================

class GraphicMenu
{
public:
    GraphicMenu(CCINIClass* pINI, const char* pSection);
    ~GraphicMenu();

    bool IsValid() const { return m_Valid; }

    static constexpr int32 DefaultItemMax = 0x64;

    DynamicVectorClass<GraphicMenuItem*>* Items;

    char*   pBackground;
    int32   ThemeIndex;
    int32   ItemMax;

    bool    m_Valid;
};

