#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;
class PCXClass;

// ============================================================================
// ScoreScreenClass - end-of-game score screen thresholds
//
//  The score screen classifies each player into one of four bands.  The four
//  cut-offs come from the [ScoreScreen] section of the multiplayer INI and
//  are stored alongside the screen's own layout so a saved selection can be
//  restored verbatim.
// ============================================================================

class ScoreScreenClass
{
public:
    ScoreScreenClass();

    // Reads the four band thresholds.  Every key keeps the value it already
    // holds as its fallback, so a partially specified section is stable.
    void LoadFromINI(CCINIClass* pINI, const char* pSection);

    // ==================================================================
    // Score-record layout
    //
    //  A record is 0x70 bytes and is filled by the ShowScoreScreen family.
    //  Three flavours exist, keyed by which table they live in and which
    //  counters they sum:
    //
    //    FillRecord       (asm 0x49B7E0, __thiscall) -- fills ONE caller
    //                     supplied record; used by the single-player
    //                     debrief (~Game/GameInit.cpp).
    //    FillPlayerRecords(asm 0x46DA80, __cdecl)    -- fills the global
    //                     HouseScores/ScoreRecords tables, one entry per
    //                     human player, own-building counters.
    //    FillMPRecords    (asm 0x5C98B0, __cdecl)    -- multiplayer variant:
    //                     walks the same tables but skips `someHouse`,
    //                     uses the *killed* counters and rolls an extra
    //                     cash bonus.
    // ==================================================================

    // ------------------------------------------------------------------
    // The raw record.  Field offsets mirror the original byte layout.
    // ------------------------------------------------------------------
    struct Record
    {
        // 原版是 UTF-16（2 字节宽）的 0x28 字节名牌；宿主 wchar_t 是 4
        // 字节，所以按 uint16 存放，保持整体 0x70 的步长不变。
        uint16  Name[0x14];     // +0x00, 0x28 bytes
        int32   Scheme;         // +0x28
        int32   WinFlag;        // +0x2C -- >0 means "Winner"
        int32   Lost;           // +0x30
        uint8   _pad34[0x0C];   // +0x34
        int32   Score;          // +0x40
        uint8   _pad44[0x0C];   // +0x44
        int32   Kills;          // +0x50
        uint8   _pad54[0x0C];   // +0x54
        int32   Cash;           // +0x60
        uint8   _pad64[0x0C];   // +0x64
    };
    static_assert(sizeof(Record) == 0x70, "ScoreScreenClass::Record layout");

    // Fills `record` from the first eligible house.  `skipFirst` mirrors the
    // original's `dl`: when non-zero the first eligible house is passed over
    // and the second one is used.
    void FillRecord(Record* record, int32 skipFirst);

    // Fills the global HouseScores / ScoreRecords tables (asm 0x46DA80).
    static void FillPlayerRecords();

    // Multiplayer variant (asm 0x5C98B0).
    static void FillMPRecords();

    // ------------------------------------------------------------------
    // asm: Screen_DrawScoreScreen (0x5CA110, __fastcall: ecx = step)
    //
    //  scenario = [Scenario+34B8h]   (== ScenarioClass::PlayerSideIndex)
    //  0 -> ScoreScreenA / 1 -> ScoreScreenS / 2 -> ScoreScreenY
    //  每张表 10 条名字（mp{a,s,y}scrnlbar01..10.pcx），step 越界或
    //  scenario 不是 0..2 时返回 0；命中则把该 PCX 交出来（原版由
    //  PCX_Draw 复制描述符后返回指针）。
    // ------------------------------------------------------------------
    static PCXClass* DrawScoreScreen(int32 step);

    // ------------------------------------------------------------------
    // Band thresholds
    // ------------------------------------------------------------------
    int32 WinningThreshold;
    int32 MegaWinningThreshold;
    int32 LosingThreshold;
    int32 MegaLosingThreshold;

    // ------------------------------------------------------------------
    // asm: Global_DrawScoreScreen (0x5C9B37; stdcall hDlg/uMsg/wParam/lParam,
    //      retn 10h) —— 多人/遭遇战记分屏的对话框过程
    //
    //  原版是 MPScore.cpp 里的裸函数，由 sub_5C9720 的 DialogBoxParam 装进
    //  DLGPROC 槽。它不碰 this（全部状态都在文件级静态表里），故做成静态
    //  成员，与 MP 记分表的三个填充入口放在同一个类里。
    //
    //  处理三条消息：
    //    0Fh  WM_PAINT   —— 取 0x6D2 与"按人数选中的窗格"两块矩形，合出
    //                       一块外扩 (-4,-8) 的区域，压暗 DSurface_Alternate
    //                        （asm: sub_72AA10 + sub_621B80，alpha 9Fh）
    //    111h WM_COMMAND —— 只认控件 6D1h：把 DWLP_USER 指向的 dword 置 1
    //    497h            —— 全量重绘：主题、TXT_GAME/GUI:*Score 标题、耗时
    //                       统计、FillMPRecords、HouseScores 排序、静态窗格
    //                       循环与每玩家四列 + 名字着色循环
    // ------------------------------------------------------------------
    static int32 ScoreScreenDlgProc(void* hDlg, uint32 uMsg, uint32 wParam,
                                    int32 lParam);
};
