#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Abstract/AbstractTypeClass.h"
#include "Abstract/TechnoTypeClass.h"
#include "Containers/DynamicVectorClass.h"

#include <cstring>

// Forward declarations
class FactoryClass;
class StageClass;

// ============================================================================
// BuildType - A single build queue item
// ============================================================================
struct BuildType
{
    int32 ItemIndex;
    AbstractType ItemType;
    bool IsAlt;
    FactoryClass* CurrentFactory;
    DWORD unknown_10;
    int32 Progress;     // 0 to 54, construction progress
    int32 FlashEndFrame;

    BuildType()
        : ItemIndex(-1), ItemType(AbstractType::None), IsAlt(false)
        , CurrentFactory(nullptr), unknown_10(0), Progress(0)
        , FlashEndFrame(0)
    {}

    BuildType(int32 itemIndex, AbstractType itemType)
        : ItemIndex(itemIndex), ItemType(itemType), IsAlt(false)
        , CurrentFactory(nullptr), unknown_10(0), Progress(0)
        , FlashEndFrame(0)
    {}

    bool operator==(const BuildType& rhs) const
    {
        return ItemIndex == rhs.ItemIndex && ItemType == rhs.ItemType;
    }

    bool operator!=(const BuildType& rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const BuildType& rhs) const
    {
        if (ItemType != rhs.ItemType)
            return static_cast<uint32>(ItemType) < static_cast<uint32>(rhs.ItemType);
        return ItemIndex < rhs.ItemIndex;
    }

    static bool SortsBefore(
        AbstractType leftType, int32 leftIndex,
        AbstractType rightType, int32 rightIndex)
    {
        if (leftType != rightType)
            return static_cast<uint32>(leftType) < static_cast<uint32>(rightType);
        return leftIndex < rightIndex;
    }
};

// ============================================================================
// StripClass - A single production tab strip
// ============================================================================
struct StripClass
{
    int32 Progress;             // 0 to 54, scroll animation
    bool AllowedToDraw;
    BYTE align_1D[3];
    Point2D Location;
    Rectangle Bounds;
    int32 Index;
    bool NeedsRedraw;
    BYTE unknown_3D;
    BYTE unknown_3E;
    BYTE unknown_3F;
    DWORD unknown_40;
    int32 TopRowIndex;          // Scroll position
    DWORD unknown_48;
    DWORD unknown_4C;
    DWORD unknown_50;
    int32 CameoCount;
    BuildType Cameos[75];

    StripClass()
        : Progress(0), AllowedToDraw(false), Location(0, 0)
        , Bounds(0, 0, 0, 0), Index(0), NeedsRedraw(false)
        , unknown_3D(0), unknown_3E(0), unknown_3F(0)
        , unknown_40(0), TopRowIndex(0), unknown_48(0)
        , unknown_4C(0), unknown_50(0), CameoCount(0)
    {
        memset(align_1D, 0, sizeof(align_1D));
    }
};

// ============================================================================
// PowerClass - Power management (base class for SidebarClass)
// ============================================================================
class NOVTABLE PowerClass
{
public:
    virtual ~PowerClass() = default;
    virtual void Draw(DWORD dwUnk) = 0;
    virtual void RedrawSidebar(int32 mode = 0) = 0;
};

// ============================================================================
// SidebarClass - Sidebar UI (build queue, production tabs)
//
// Manages the sidebar panel showing build queues, production tabs,
// and diplomacy information. Supports up to 4 tabs and 75 items per tab.
// ============================================================================
class NOVTABLE SidebarClass : public PowerClass
{
public:
    static SidebarClass* Instance;
    static wchar_t TooltipBuffer[0x42];

    SidebarClass();
    virtual ~SidebarClass();

    // PowerClass overrides
    virtual void Draw(DWORD dwUnk) override;
    virtual void RedrawSidebar(int32 mode = 0) override;

    // SidebarClass virtual methods
    virtual bool vt_entry_D8(int32 nUnknown);

    // Non-virtual methods
    void Init();
    void Init_IO();
    void Init_Clear();
    void Init_For_House();
    void SidebarNeedsRepaint(int32 mode = 0);
    void RepaintSidebar(int32 tab = 0);
    bool AddCameo(AbstractType absType, int32 idxType);
    bool Add_To_List(TechnoTypeClass* pType);
    bool Remove_From_List(TechnoTypeClass* pType);
    void Strip_Update();
    void Tab_Update();
    void One_Time();

    // Which tab does an object type belong to?
    static int32 GetObjectTabIdx(AbstractType abs, int32 idxType, int32 unused);
    static int32 GetObjectTabIdx(AbstractType abs, BuildCat buildCat, bool isNaval);

    // Properties
    static constexpr int32 MaxTabs = 4;
    static constexpr int32 MaxCameosPerTab = 75;
    static constexpr int32 SidebarWidth = 160;
    static constexpr int32 CameoWidth = 64;
    static constexpr int32 CameoHeight = 48;

    StripClass Tabs[MaxTabs];
    DWORD unknown_5394;
    DWORD unknown_5398;
    int32 ActiveTabIndex;
    DWORD unknown_53A0;
    bool HideObjectNameInTooltip;
    bool IsSidebarActive;
    bool SidebarNeedsRedraw;
    bool SidebarBackgroundNeedsRedraw;
    bool unknown_bool_53A8;

    // Diplomacy information
    HouseClass* DiplomacyHouses[8];
    int32 DiplomacyKills[8];
    int32 DiplomacyOwned[8];
    int32 DiplomacyPowerDrain[8];
    ColorScheme* DiplomacyColors[8];
    DWORD unknown_544C[8];
    DWORD unknown_546C[8];
    DWORD unknown_548C[8];
    DWORD unknown_54AC[8];
    DWORD unknown_54CC[8];
    DWORD unknown_54EC[8];
    BYTE unknown_550C;
    int32 DiplomacyNumHouses;
    bool unknown_bool_5514;
    bool unknown_bool_5515;
    BYTE padding_5516[2];
};