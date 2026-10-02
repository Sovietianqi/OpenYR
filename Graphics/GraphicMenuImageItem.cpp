#include "GraphicMenuImageItem.h"
#include "../INI/INIClass.h"

#include <cstring>

// ============================================================================
// GraphicMenuImageItem - GraphicMenuImageItem_Read_INI (asm 0x4F314D)
// ============================================================================

static char* DupString(const char* pSource)
{
    if (pSource == nullptr) {
        return nullptr;
    }
    const size_t len = std::strlen(pSource);
    char* pCopy = new char[len + 1];
    std::memcpy(pCopy, pSource, len + 1);
    return pCopy;
}

GraphicMenuImageItem::GraphicMenuImageItem()
    : ID(-1)
    , OriginX(0)
    , OriginY(0)
    , ActiveRect()
    , pImage(nullptr)
    , pHighlighted(nullptr)
    , pDisabled(nullptr)
    , pHighlightSound(nullptr)
    , pSelectVQ(nullptr)
{
}

GraphicMenuImageItem::~GraphicMenuImageItem()
{
    Clear();
}

void GraphicMenuImageItem::Clear()
{
    delete[] pImage;
    delete[] pHighlighted;
    delete[] pDisabled;
    delete[] pHighlightSound;
    delete[] pSelectVQ;

    pImage          = nullptr;
    pHighlighted    = nullptr;
    pDisabled       = nullptr;
    pHighlightSound = nullptr;
    pSelectVQ       = nullptr;
}

GraphicMenuImageItem* GraphicMenuImageItem::ReadFromINI(CCINIClass* pINI,
                                                        const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return nullptr;
    }

    const int32 id = pINI->ReadInteger(pSection, "ID", -1);
    if (id == -1) {
        return nullptr;
    }

    GraphicMenuImageItem* pItem = new GraphicMenuImageItem();
    pItem->ID = id;

    // Origin is added to the ActiveRect origin, exactly as in the original.
    pItem->OriginX = 0;
    pItem->OriginY = 0;
    pINI->ReadPoint(pSection, "Origin", &pItem->OriginX, &pItem->OriginY);

    pINI->ReadRect(pSection, "ActiveRect", &pItem->ActiveRect);
    pItem->ActiveRect.X += pItem->OriginX;
    pItem->ActiveRect.Y += pItem->OriginY;

    char buffer[0x100];
    buffer[0] = '\0';
    pINI->ReadString(pSection, "Image", "", buffer, sizeof(buffer));
    pItem->pImage = DupString(buffer);

    buffer[0] = '\0';
    pINI->ReadString(pSection, "Highlighted", "", buffer, sizeof(buffer));
    pItem->pHighlighted = DupString(buffer);

    buffer[0] = '\0';
    pINI->ReadString(pSection, "Disabled", "", buffer, sizeof(buffer));
    pItem->pDisabled = DupString(buffer);

    buffer[0] = '\0';
    pINI->ReadString(pSection, "HighlightSound", "", buffer, sizeof(buffer));
    pItem->pHighlightSound = DupString(buffer);

    buffer[0] = '\0';
    pINI->ReadString(pSection, "SelectVQ", "", buffer, sizeof(buffer));
    pItem->pSelectVQ = DupString(buffer);

    return pItem;
}
