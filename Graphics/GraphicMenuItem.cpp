#include "GraphicMenuItem.h"
#include "GraphicMenuImageItem.h"
#include "../INI/INIClass.h"
#include "../Audio/ThemeClass.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// GraphicMenuItem / GraphicMenuShortcutItem
// ============================================================================

GraphicMenuItem::~GraphicMenuItem()
{
    delete[] pSelectVQ;
}

GraphicMenuShortcutItem::GraphicMenuShortcutItem()
    : ID(-1)
    , pKeys(nullptr)
    , field_08(0)
    , field_0C(0)
{
    Type = GraphicMenuItem_Shortcut;
    pSelectVQ = nullptr;
    pPayload = nullptr;
}

GraphicMenuShortcutItem::~GraphicMenuShortcutItem()
{
    delete[] pKeys;
}

// ============================================================================
// GraphicMenuShortcutItem_Read_INI - asm 0x4F3B4E
//
//   "ID" -1 discards the item.  "Keys" holds the key list the shortcut binds
//   to; the object built by sub_4F3C40 keeps a copy of it.
// ============================================================================
GraphicMenuShortcutItem* GraphicMenuShortcutItem::ReadFromINI(
        CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return nullptr;
    }

    const int32 id = pINI->ReadInteger(pSection, "ID", -1);
    if (id == -1) {
        return nullptr;
    }

    char buffer[0x100];
    buffer[0] = '\0';
    pINI->ReadString(pSection, "Keys", "", buffer, sizeof(buffer));

    GraphicMenuShortcutItem* pItem = new GraphicMenuShortcutItem();
    pItem->ID = id;

    const size_t len = std::strlen(buffer);
    pItem->pKeys = new char[len + 1];
    std::memcpy(pItem->pKeys, buffer, len + 1);

    return pItem;
}

// ============================================================================
// GraphicMenuItem_Read_INI - asm 0x4F378F
//
//   Reads "Type" first and dispatches to the matching concrete reader.  A
//   reader that returns null leaves the whole item empty.  When the item was
//   built and "SelectSound" is present, a sound reference is attached.
// ============================================================================
GraphicMenuItem* GraphicMenuItem::ReadFromINI(CCINIClass* pINI,
                                              const char* pSection,
                                              int32 arg0,
                                              int32 arg1)
{
    if (pINI == nullptr || pSection == nullptr) {
        return nullptr;
    }

    char typeName[0x100];
    typeName[0] = '\0';
    pINI->ReadString(pSection, "Type", "", typeName, sizeof(typeName));

    GraphicMenuItem* pItem = nullptr;

    if (_strcmpi(typeName, "Image") == 0)
    {
        GraphicMenuImageItem* pImage =
            GraphicMenuImageItem::ReadFromINI(pINI, pSection);
        if (pImage != nullptr)
        {
            pItem = new GraphicMenuItem();
            pItem->Type = GraphicMenuItem_Image;
            pItem->pPayload = pImage;
        }
    }
    else if (_strcmpi(typeName, "Shortcut") == 0)
    {
        GraphicMenuShortcutItem* pShortcut =
            GraphicMenuShortcutItem::ReadFromINI(pINI, pSection);
        if (pShortcut != nullptr) {
            pItem = pShortcut;
        }
    }
    else if (_strcmpi(typeName, "Anim") == 0)
    {
        pItem = new GraphicMenuItem();
        pItem->Type = GraphicMenuItem_Anim;
        (void)arg0;
        (void)arg1;
    }

    if (pItem == nullptr) {
        return nullptr;
    }

    char selectSound[0x100];
    selectSound[0] = '\0';
    if (pINI->ReadString(pSection, "SelectSound", "", selectSound,
                         sizeof(selectSound)) > 0)
    {
        const size_t len = std::strlen(selectSound);
        pItem->pSelectVQ = new char[len + 1];
        std::memcpy(pItem->pSelectVQ, selectSound, len + 1);
    }

    return pItem;
}

// ============================================================================
// GraphicMenu::GraphicMenu - asm 0x4F1C64
//
//   The section must exist or the menu is invalid.  "Background" is read
//   through the char** helper so an empty value leaves the pointer null.
//   "Theme" resolves through the theme registry.  "ItemMax" defaults to 0x64
//   and drives the "%d" walk; each numbered entry is handed to
//   GraphicMenuItem::ReadFromINI and appended to the item vector when it
//   produces an object.
// ============================================================================
GraphicMenu::GraphicMenu(CCINIClass* pINI, const char* pSection)
    : Items(new DynamicVectorClass<GraphicMenuItem*>())
    , pBackground(nullptr)
    , ThemeIndex(-1)
    , ItemMax(DefaultItemMax)
    , m_Valid(false)
{
    if (pINI == nullptr || pSection == nullptr) {
        return;
    }

    if (pINI->GetSection(pSection) == nullptr) {
        return;
    }

    char buffer[0x100];
    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Background", "", buffer,
                         sizeof(buffer)) > 0)
    {
        const size_t len = std::strlen(buffer);
        pBackground = new char[len + 1];
        std::memcpy(pBackground, buffer, len + 1);
    }

    buffer[0] = '\0';
    if (pINI->ReadString(pSection, "Theme", "", buffer, sizeof(buffer)) > 0)
    {
        ThemeIndex = ThemeClass::FindIndex(buffer);
    }

    ItemMax = pINI->ReadInteger(pSection, "ItemMax", DefaultItemMax);

    for (int32 i = 0; i < ItemMax; ++i)
    {
        char key[0x20];
        std::sprintf(key, "%d", i);

        char subSection[0x100];
        subSection[0] = '\0';
        if (pINI->ReadString(pSection, key, "", subSection,
                             sizeof(subSection)) == 0) {
            continue;
        }

        GraphicMenuItem* pItem =
            GraphicMenuItem::ReadFromINI(pINI, subSection, 0, 0);
        if (pItem != nullptr) {
            Items->Add(pItem);
        }
    }

    m_Valid = true;
}

GraphicMenu::~GraphicMenu()
{
    if (Items != nullptr)
    {
        for (int32 i = 0; i < Items->Count; ++i) {
            delete Items->Items[i];
        }
        delete Items;
    }
    delete[] pBackground;
}
