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
    // 根据游戏行为，可知 +0x3C 这一字节是"本标签页需要闪烁提示"的标记，
    // 由渗透事件等外部原因点亮。
    BYTE NeedsFlash;
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
        , NeedsFlash(0)
        , unknown_40(0), TopRowIndex(0), unknown_48(0)
        , unknown_4C(0), unknown_50(0), CameoCount(0)
    {
        memset(align_1D, 0, sizeof(align_1D));
    }
};

// ============================================================================
// CameoEntry - one entry of the per-tab cameo table
//
// asm: SidebarClass_ProcessCameoClick 用 `lea ebx,[esi+ecx*4] / shl ebx,2`
// （13 * 4 = 52 字节/条）遍历 [tab+0x58] 这张表，字段偏移与下面的 lea 链一一
// 对应：
//   +0x00 ItemIndex   类型数组下标        (asm: [ebx+eax+58h])
//   +0x04 AbstractID  类型类别            (asm: [ebx+eax+5Ch])
//   +0x08 BuildCat    建造分类            (asm: [ebx+eax+60h])
//   +0x0C Factory     该图标绑定的生产线  (asm: [ebx+eax+64h])
//   +0x10 Status      0 空闲 / 1 建造中 / 2 暂停 (asm: [esi+ecx], esi=tab+68h+52i)
//   +0x14 Progress    建造进度
//   +0x1C StartFrame  进度起始帧
//   +0x20 ThisSlot    原版写入未初始化的 this 栈槽（见 6AB5B3），保留字段
//   +0x24 Frames      预计帧数
//   +0x28 Frames2     同上（原版重复写一次）
// ============================================================================
struct CameoEntry
{
    int32        ItemIndex;      // +0x00
    int32        AbstractID;     // +0x04
    int32        BuildCat;       // +0x08
    FactoryClass* Factory;       // +0x0C
    int32        Status;         // +0x10
    int32        Progress;       // +0x14
    int32        unknown_18;     // +0x18
    int32        StartFrame;     // +0x1C
    int32        ThisSlot;       // +0x20
    int32        Frames;         // +0x24
    int32        Frames2;        // +0x28
    int32        unknown_2C;     // +0x2C
    int32        unknown_30;     // +0x30

    CameoEntry()
        : ItemIndex(-1), AbstractID(0), BuildCat(0), Factory(nullptr)
        , Status(0), Progress(0), unknown_18(0), StartFrame(0)
        , ThisSlot(0), Frames(0), Frames2(0), unknown_2C(0), unknown_30(0)
    {}
};

// Number of cameo records per tab (asm: 75 条 x 52 字节 + 0x58 表头 = 3988)。
static constexpr int32 kTabCameoCount = 75;

// ============================================================================
// TabCameoListClass - the cameo table of the active tab
//
// asm: SidebarClass+0x2C 保存指向该结构的指针；原版整个对象 3988 字节（与
// unk_880D68 每档 3988 字节的步长一致）。字段按原版偏移保留。
//   +0x00 CurrentRow      当前滚动行（cameoIdx = [this+30h] + CurrentRow*2）
//   +0x38 TabIndex        标签页序号（unk_880D68[TabIndex*3988]）
//   +0x3C NeedsRedraw     标签页脏标记
//   +0x54 Count           本页图标总数
//   +0x58 Entries         图标记录
// ============================================================================
struct TabCameoListClass
{
    int32       CurrentRow;        // +0x00
    uint8       unknown_04[0x34];  // +0x04 .. +0x37
    int32       TabIndex;          // +0x38
    uint8       NeedsRedraw;       // +0x3C
    uint8       unknown_3D[0x17];  // +0x3D .. +0x53
    int32       Count;             // +0x54
    CameoEntry  Entries[kTabCameoCount];  // +0x58

    TabCameoListClass() : CurrentRow(0), TabIndex(0), NeedsRedraw(0), Count(0)
    {
        for (int32 i = 0; i < static_cast<int32>(sizeof(unknown_04)); ++i)
            unknown_04[i] = 0;
        for (int32 i = 0; i < static_cast<int32>(sizeof(unknown_3D)); ++i)
            unknown_3D[i] = 0;
    }
};

// 说明：上述偏移取自 32 位原版（指针 4 字节）。OpenYR 里用真实成员字段承载，
//   64 位构建下 sizeof 会因指针变宽而变大，故这里不做尺寸静态断言。

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

    // ========================================================================
    // 侧边栏内容绘制（SHP 贴图 + 建造格进度覆盖）。
    //
    //   asm 依据：
    //     * 侧边栏宽度 num_168 = 168（asm .data 2058714）；
    //     * 建造格尺寸：StripClass::Init_IO 给 gadget 写 14h=3Ch(宽 60)、
    //       18h=30h(高 48)；横向步进 dword_B0B4FC = 40h = 64（YR 布局）、
    //       纵向步进 dword_B0B500 = 32h = 50（sub_6A5130）；
    //     * 原点：Location.X = someRect.T + 16h，Draw_It 再减 someRect.T
    //       → 局部 X = 16h = 22；Location.Y = dword_B0B4F8 = someRect.L + 45h，
    //       Draw_It 再加 1 → 局部 Y = 158 + 69 + 1 = 228（随侧边栏缩放）。
    //     * gadget 下标 = 2*col + row（两列），可见行数 = (面高-锚点-56h)/32h。
    //
    //   panel 是侧边栏在目标面上的绝对矩形；返回提交的绘制次数（[UI] 统计）。
    // ========================================================================
    int32 Draw_UI(Surface* pSrf, const Rectangle& panel);

    // asm 侧边栏/建造格布局常量（见上）。
    static constexpr int32 PanelWidth     = 168;  // num_168
    static constexpr int32 CameoPitchX    = 64;   // dword_B0B4FC (40h, YR)
    static constexpr int32 CameoPitchY    = 50;   // dword_B0B500 (32h)
    static constexpr int32 CameoColumns   = 2;    // Draw_It: gadget = 2*col+row
    static constexpr int32 CameoRows      = 7;    // 600 高：(600-227)/50
    static constexpr int32 CameoSlotW     = 60;   // Init_IO gadget+14h = 3Ch
    static constexpr int32 CameoSlotH     = 48;   // Init_IO gadget+18h = 30h
    static constexpr int32 CameoGridX     = 22;   // B0B4F4 - someRect.T = 16h
    static constexpr int32 CameoGridY     = 227;  // dword_B0B4F8 - someRect.L = 45h

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

    // asm: SidebarClass__Set_Cameo_Text_bool (0x5D0A30)
    //   __thiscall，retn 4 —— 原版函数体只有 `retn 4`，没有任何写入。
    //   调用点：Options_LoadFromINI 传 GameOptionsClass.SidebarCameoText。
    void Set_Cameo_Text(bool on);

    // ========================================================================
    // asm: SidebarClass_ProcessCameoClick (0x6AACB0，1205 行，retn 0Ch)
    //
    //   signed int __thiscall SidebarClass_ProcessCameoClick(
    //       SidebarClass *this, int a2, int a3, char a4)
    //
    // 侧边栏图标点击的总入口：
    //   a2  修饰位（bit0 左键 / bit3 忽略 / bit4 右键）
    //   a3  原样回传给 Control_7C 的事件参数
    //   a4  bit0 = 本次是左键点击（Shift 连点即左键 + 已排队）
    // 返回恒为 1（原版 `mov eax,1`）。
    // ========================================================================
    int32 ProcessCameoClick(int32 a2, int32 a3, char a4);

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
    // ------------------------------------------------------------------
    // 根据游戏行为，可知 +0x539C 保存"最近一次因间谍渗透而被点亮的房屋编号"，
    // 供 SetAircraftTab 判定本次渗透事件是否命中本机控制的房屋。
    // ------------------------------------------------------------------
    int32 PlayerInfiltrated;
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

    // ------------------------------------------------------------------------
    // asm: SidebarClass+0x2C / +0x30 —— 当前标签页的图标表与起始下标。
    //   原版 +0x2C 是指向 TabCameoListClass 的指针；OpenYR 用内嵌成员承载
    //   同一张表（指针一旦在 Init 里挂上就不会再为空，故等价）。
    // ------------------------------------------------------------------------
    TabCameoListClass TabCameoList;   // +0x2C
    int32             CameoBaseIndex; // +0x30
};