#include "ScoreScreenClass.h"
#include "../INI/INIClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Network/MPModes.h"
#include "../Scenario/ScenarioClass.h"
#include "../FileFormats/PCX.h"
#include "../FileFormats/PCXLoader.h"
#include "../Rendering/Surface.h"
#include "../Rendering/ConvertClass.h"
#include "../Audio/ThemeClass.h"
#include "../Game/Externs.h"
#include "EngineGlobals.h"
#include "Game.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_SCORE_WIN32 1
    #include <windows.h>
#endif

// 引擎的调试输出通道。原版把评分屏的几行统计走 WWDebugString 打出来
// （asm 0x7C5B10）；这里沿用本树其它子系统的做法，落到 stderr。
static void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

// ============================================================================
// ScoreScreenClass - implementation
// ============================================================================

ScoreScreenClass::ScoreScreenClass()
    : WinningThreshold(0)
    , MegaWinningThreshold(0)
    , LosingThreshold(0)
    , MegaLosingThreshold(0)
{
}

// ============================================================================
// LoadFromINI
//
//   Order matches the original: winning, mega winning, losing, mega losing.
// ============================================================================
void ScoreScreenClass::LoadFromINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr)
        return;

    WinningThreshold     = pINI->ReadInteger(pSection, "WinningThreshold",     WinningThreshold);
    MegaWinningThreshold = pINI->ReadInteger(pSection, "MegaWinningThreshold", MegaWinningThreshold);
    LosingThreshold      = pINI->ReadInteger(pSection, "LosingThreshold",      LosingThreshold);
    MegaLosingThreshold  = pINI->ReadInteger(pSection, "MegaLosingThreshold",  MegaLosingThreshold);
}

// ============================================================================
// Score-record filling
//
//  The three flavours share a heap-agnostic view of HouseClass: the original
//  addresses house members by raw offset, and some of those offsets (the
//  statistic arrays at +0x53E4 and the two counter tables at +0x5550/+0x55A0)
//  are not yet named in OpenYR.  They are reached through explicit byte
//  pointers so the code stays byte-exact without inventing field names.
// ============================================================================

namespace {

// 原版在两个分支里都做了一次「宽字符名 -> 窄字符」的转写；这里复用同一
// 转写入口，nullptr 时退化为空串。
inline const char* Score_Name(const void* pHouseName)
{
    return MP::ReadNthPlayerName(
        reinterpret_cast<const wchar_t*>(pHouseName));
}

// 原版 wcscpy 在 2 字节宽的缓冲上工作；宿主 wchar_t 是 4 字节，所以按
// uint16 单元复制，保持尾部 0 终止的语义。
inline void Score_WStrCpy(void* pDst, const void* pSrc)
{
    uint16*       d = static_cast<uint16*>(pDst);
    const uint16* s = static_cast<const uint16*>(pSrc);
    while ((*d++ = *s++) != 0) {
    }
}

// HouseClass 里那两片尚未具名的统计区。之所以用裸偏移：原版把每一类
// 建筑的「曾经拥有 / 已被击毁」各记成一张 CounterClass 表，表首偏移
// +0x5550 与 +0x55A0 之间隔着一整个区块，而 OpenYR 尚未把它们拆开命名。
inline const int32* House_Sum20(const uint8* pHouse, int32 offset)
{
    return reinterpret_cast<const int32*>(pHouse + offset);
}

inline const int32* House_Counters(const uint8* pHouse, int32 offset)
{
    return reinterpret_cast<const int32*>(pHouse + offset);
}

} // namespace

// ============================================================================
// Score-table globals
//
//   原版把这两张表放在 .data 里：
//     A8D224h  ScoreRecords   每格 0x70 字节的统计记录
//     A8D58Ch  HouseScores    每格 0x70 字节的宽字符名牌槽
//     A8D584h  dword_A8D584   「最后获胜者」的槽号（与记录 7 的尾部重叠）
//   OpenYR 侧用同一份静态存储，语义等价。
// ============================================================================
static ScoreScreenClass::Record s_PlayerRecords[8];
static int32                     s_PlayersInGame = 0;   // asm gPlayersInGame
static int32                     s_LastWinner    = 0;   // asm dword_A8D584

// asm: Random2Class 位于 [Scenario+218h]，取值区间 [min, max]。
static int32 Score_Random(int32 mn, int32 mx)
{
    if (ScenarioClass::Instance == nullptr) {
        return mn;
    }
    return ScenarioClass::Instance->Random.Next(mn, mx);
}

// ============================================================================
// FillRecord -- asm Global_ShowScoreScreen3 / 0x49B7E0
//
//   走 vec_Houses，从第一个「合格」的房主填出一条记录后直接返回；仅当
//   skipFirst 非零时跳过第一个合格房主、改填第二个。
//   合格判据：house != 0 && house->Type->MultiplayPassive != true &&
//             house->IsHumanPlayer != false
// ============================================================================
void ScoreScreenClass::FillRecord(Record* record, int32 skipFirst)
{
    if (record == nullptr || HouseClass::ArrayCount <= 0) {
        return;
    }

    int32 dl = skipFirst;

    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr) {
            continue;   // asm: test esi,esi / jz next
        }

        // asm: mov ebx,[esi+34h] / cmp byte ptr [ebx+1A6h],1 / jz next
        if (pHouse->Type != nullptr && pHouse->Type->MultiplayPassive) {
            continue;
        }

        // asm: xor ebx,ebx / cmp [esi+1ECh],bl / jz next
        //     [esi+1ECh] == IsHumanPlayer
        if (!pHouse->IsHumanPlayer) {
            continue;
        }

        // asm: cmp dl,bl / jz process / xor dl,dl
        if (dl == 0) {
            const uint8* house = reinterpret_cast<const uint8*>(pHouse);

            record->WinFlag = 0;                       // [edi+2Ch] = 0
            Score_WStrCpy(record->Name, house + 0x1602A);   // wcscpy(edi, esi+1602Ah)
            record->Lost  = 0;                         // [edi+30h]
            record->Score = 0;                         // [edi+40h]
            record->Kills = 0;                         // [edi+50h]
            record->Cash  = 0;                         // [edi+60h]

            // asm: mov ecx,[esi+16054h] / mov [edi+28h],ecx
            record->Scheme =
                *reinterpret_cast<const int32*>(house + 0x16054);

            // asm: mov al,[esi+1F5h] / cmp al,bl / jnz -> inc [edi+2Ch]
            if (house[0x1F5] == 0) {
                ++record->WinFlag;
            }

            // score = sum20(53E4h) + sumN(5438h)   (N = 房主总数, ebp)
            int32 score = 0;
            {
                const int32* p = House_Sum20(house, 0x53E4);
                for (int32 k = 0; k < 0x14; ++k) {
                    score += p[k];
                }
            }
            {
                const int32* p = House_Sum20(house, 0x5438);
                for (int32 k = 0; k < HouseClass::ArrayCount; ++k) {
                    score += p[k];
                }
            }
            record->Score = score;

            // asm: edx = [esi+5434h] + [esi+5488h] -> [edi+30h]
            record->Lost = *reinterpret_cast<const int32*>(house + 0x5434) +
                           *reinterpret_cast<const int32*>(house + 0x5488);

            // kills = 四张建筑计数器 + [5488h] + [5434h]
            // 原版逐项调用 CounterClass::GetItemAmount，四张表首地址分别为
            // +0x5550 / +0x5564 / +0x5578 / +0x558C（步长 0x14）。
            int32 killsSum = 0;
            const int32* c0 = House_Counters(house, 0x5550);
            const int32* c1 = House_Counters(house, 0x5564);
            const int32* c2 = House_Counters(house, 0x5578);
            const int32* c3 = House_Counters(house, 0x558C);
            killsSum += *c0;
            killsSum += *c1;
            killsSum += *c2;
            killsSum += *c3;
            killsSum += *reinterpret_cast<const int32*>(house + 0x5488);
            killsSum += *reinterpret_cast<const int32*>(house + 0x5434);
            record->Kills = killsSum;

            // asm: esi = [esi+54E8h] / cmp esi,ebx / jle / add [edi+60h],esi
            const int32 bonus = *reinterpret_cast<const int32*>(house + 0x54E8);
            if (bonus > 0) {
                record->Cash += bonus;
            }

            // asm: tag = [edi+2Ch] > 0 ? "Winner" : "Loser"
            const char* tag = (record->WinFlag > 0) ? "Winner" : "Loser";

            // asm: mov ecx,edi / call MP_ReadNthPlayerName
            const char* name = Score_Name(record->Name);

            WWDebugString("@@@@ %s: %s\n Scheme: %d\n Lost = %d\n Kills = %d\n "
                          "Built = %d\n Score = %d\n",
                          name, tag, record->Scheme, record->Lost,
                          record->Score, record->Kills, record->Cash);
            return;   // asm 直接落到 epilogue，只填一条
        }
        dl = 0;   // asm: xor dl,dl
    }
}

// ============================================================================
// FillPlayerRecords -- asm Global_ShowScoreScreen1 / 0x46DA80 (__cdecl)
//
//   重置房主计数，按 vec_Houses 的顺序，为每个「人类玩家」房主在其
//   槽位写一条统计记录。分数与击杀都取自「拥有」侧计数器。
// ============================================================================
void ScoreScreenClass::FillPlayerRecords()
{
    s_PlayersInGame = 0;
    if (HouseClass::ArrayCount <= 0) {
        return;
    }

    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr) {
            continue;
        }

        // asm: mov ecx,[edi+34h] / cmp byte ptr [ecx+1A6h],1 / jz next
        if (pHouse->Type != nullptr && pHouse->Type->MultiplayPassive) {
            continue;
        }

        // asm: mov ecx,edi / call HouseClass::Is_Human / test al,al / jz next
        if (!pHouse->Is_Human()) {
            continue;
        }

        const int32 idx = s_PlayersInGame++;
        Record*     rec = &s_PlayerRecords[idx];

        // asm: mov dword_A8D228[esi],0 (先写再 wcscpy，保持原版次序)
        rec->WinFlag = 0;
        Score_WStrCpy(rec->Name, reinterpret_cast<const uint8*>(pHouse) + 0x1602A);

        rec->Lost  = 0;
        rec->Score = 0;
        rec->Kills = 0;
        rec->Cash  = 0;

        const uint8* house = reinterpret_cast<const uint8*>(pHouse);

        rec->Scheme = *reinterpret_cast<const int32*>(house + 0x16054);

        // asm: mov cl,[edi+1F5h] / test / jnz -> ++rec+4 ; A8D584 = idx
        if (house[0x1F5] == 0) {
            ++rec->WinFlag;
            s_LastWinner = idx;
        }

        // score = sum20(53E4h) + sum20(5438h)   (这里两段都是定长 20)
        int32 score = 0;
        {
            const int32* p = House_Sum20(house, 0x53E4);
            for (int32 k = 0; k < 0x14; ++k) {
                score += p[k];
            }
        }
        {
            const int32* p = House_Sum20(house, 0x5438);
            for (int32 k = 0; k < 0x14; ++k) {
                score += p[k];
            }
        }
        rec->Score = score;

        // asm: edx = [edi+5434h] + [edi+5488h] -> rec+8
        rec->Lost = *reinterpret_cast<const int32*>(house + 0x5434) +
                    *reinterpret_cast<const int32*>(house + 0x5488);

        // kills：四张「拥有」侧建筑计数器 + [5488h] + [5434h]
        int32 kills = 0;
        kills += *House_Counters(house, 0x5550);
        kills += *House_Counters(house, 0x5564);
        kills += *House_Counters(house, 0x5578);
        kills += *House_Counters(house, 0x558C);
        kills += *reinterpret_cast<const int32*>(house + 0x5488);
        kills += *reinterpret_cast<const int32*>(house + 0x5434);
        rec->Kills = kills;

        const int32 bonus = *reinterpret_cast<const int32*>(house + 0x54E8);
        if (bonus > 0) {
            rec->Cash += bonus;
        }

        // asm: mov ecx,ebx(名牌槽) / call MP_ReadNthPlayerName
        const char* name = Score_Name(rec->Name);

        const char* tag = (rec->WinFlag > 0) ? "Winner" : "Loser";
        WWDebugString("%s: %s\n Scheme: %d\n Lost = %d\n Kills = %d\n "
                      "Built = %d\n Score = %d\n",
                      name, tag, rec->Scheme, rec->Lost, rec->Score,
                      rec->Kills, rec->Cash);
    }
}

// ============================================================================
// FillMPRecords -- asm Global_ShowScoreScreen2 / 0x5C98B0 (__cdecl)
//
//   与 FillPlayerRecords 同形，差异：
//     · 额外跳过 someHouse（本机房主）；
//     · 击杀改取「击毁」侧计数器 +0x55A0/+0x55B4/+0x55C8/+0x55DC，
//       且不再累加 [5488h] / [5434h]；
//     · 未败北时额外掷一次现金奖励：
//         min = cash / 2（向零取整），roll ∈ [min, cash]，cash += min + roll
// ============================================================================
void ScoreScreenClass::FillMPRecords()
{
    s_PlayersInGame = 0;
    if (HouseClass::ArrayCount <= 0) {
        return;
    }

    HouseClass* someHouse = HouseClass::Player;

    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse == nullptr) {
            continue;
        }

        if (pHouse->Type != nullptr && pHouse->Type->MultiplayPassive) {
            continue;
        }

        // asm: cmp edi,someHouse / jz next
        if (someHouse != nullptr && pHouse == someHouse) {
            continue;
        }

        const int32 idx = s_PlayersInGame++;
        Record*     rec = &s_PlayerRecords[idx];

        rec->WinFlag = 0;
        Score_WStrCpy(rec->Name, reinterpret_cast<const uint8*>(pHouse) + 0x1602A);

        rec->Lost  = 0;
        rec->Score = 0;
        rec->Kills = 0;
        rec->Cash  = 0;

        const uint8* house = reinterpret_cast<const uint8*>(pHouse);

        rec->Scheme = *reinterpret_cast<const int32*>(house + 0x16054);

        if (house[0x1F5] == 0) {
            ++rec->WinFlag;
            s_LastWinner = idx;
        }

        int32 score = 0;
        {
            const int32* p = House_Sum20(house, 0x53E4);
            for (int32 k = 0; k < 0x14; ++k) {
                score += p[k];
            }
        }
        {
            const int32* p = House_Sum20(house, 0x5438);
            for (int32 k = 0; k < 0x14; ++k) {
                score += p[k];
            }
        }
        rec->Score = score;

        // asm: edx = [edi+5488h] + [edi+5434h] -> rec+8
        rec->Lost = *reinterpret_cast<const int32*>(house + 0x5488) +
                    *reinterpret_cast<const int32*>(house + 0x5434);

        // kills：四张「击毁」侧建筑计数器，不再叠加 5434/5488
        int32 kills = 0;
        kills += *House_Counters(house, 0x55A0);
        kills += *House_Counters(house, 0x55B4);
        kills += *House_Counters(house, 0x55C8);
        kills += *House_Counters(house, 0x55DC);
        rec->Kills = kills;

        const int32 bonus = *reinterpret_cast<const int32*>(house + 0x54E8);
        if (bonus > 0) {
            rec->Cash += bonus;
        }

        // asm: 未败北时掷现金奖励
        //   edi = cash / eax = edi / push edi (max) / cdq / sub eax,edx
        //   ebp = eax / sar ebp,1 / push ebp (min) / call Random2Class::operator()
        //   add eax,ebp / add eax,edi / rec+38h = eax
        if (house[0x1F5] == 0) {
            const int32 cash = rec->Cash;
            const int32 mn   = cash / 2;   // cdq/sub/sar == 向零取整
            const int32 roll = Score_Random(mn, cash);
            rec->Cash        = cash + mn + roll;
        }

        // asm: mov ecx,ebx(名牌槽) / call MP_ReadNthPlayerName
        const char* name = Score_Name(rec->Name);

        const char* tag = (rec->WinFlag > 0) ? "Winner" : "Loser";
        WWDebugString("%s: %s\n Scheme: %d\n Lost = %d\n Kills = %d\n "
                      "Built = %d\n Score = %d\n",
                      name, tag, rec->Scheme, rec->Lost, rec->Score,
                      rec->Kills, rec->Cash);
    }
}

// ============================================================================
// asm: Screen_DrawScoreScreen (0x5CA110) —— 记分屏的十张背景条
//
//   void __fastcall Screen_DrawScoreScreen(signed int step)   ; ecx = step
//
//   mov eax, Scenario
//   mov eax, [eax+34B8h]        ; 阵营：0 = A(盟军), 1 = S(苏军), 2 = Y(尤里)
//   test eax,eax / jnz loc_5CA21C ...
//
//   原版是「每张表各 10 个 4 字节名字槽」的完全展开 switch：
//
//     ScoreScreenA  dd offset aMpascrnlbar01_, ..02_ .. .. .10_.pcx
//     ScoreScreenS  dd offset aMpsscrnlbar01_, ..  (同上)
//     ScoreScreenY  dd offset aMpyscrnlbar01_, ..  (同上)
//
//   命中即 `mov ecx,offset PCX / push <名字> / push step / call PCX_Draw`，
//   未命中落 loc_5CA41B：xor eax,eax / retn。三张表的内容与顺序照抄
//   .data（2146851 / 2146858 / 2146865 行）。
// ============================================================================
namespace {

// asm: ScoreScreenA / ScoreScreenS / ScoreScreenY 三张 const char*[10]
const char* const kScoreScreenNames[3][10] = {
    {
        "mpascrnlbar01.pcx", "mpascrnlbar02.pcx", "mpascrnlbar03.pcx",
        "mpascrnlbar04.pcx", "mpascrnlbar05.pcx", "mpascrnlbar06.pcx",
        "mpascrnlbar07.pcx", "mpascrnlbar08.pcx", "mpascrnlbar09.pcx",
        "mpascrnlbar10.pcx",
    },
    {
        "mpsscrnlbar01.pcx", "mpsscrnlbar02.pcx", "mpsscrnlbar03.pcx",
        "mpsscrnlbar04.pcx", "mpsscrnlbar05.pcx", "mpsscrnlbar06.pcx",
        "mpsscrnlbar07.pcx", "mpsscrnlbar08.pcx", "mpsscrnlbar09.pcx",
        "mpsscrnlbar10.pcx",
    },
    {
        "mpyscrnlbar01.pcx", "mpyscrnlbar02.pcx", "mpyscrnlbar03.pcx",
        "mpyscrnlbar04.pcx", "mpyscrnlbar05.pcx", "mpyscrnlbar06.pcx",
        "mpyscrnlbar07.pcx", "mpyscrnlbar08.pcx", "mpyscrnlbar09.pcx",
        "mpyscrnlbar10.pcx",
    },
};

// PCX_Draw 内部按名字命中全局 PCX 表，同一张图只装载一次；这里用同形的
// 结果缓存，避免每帧重读磁盘。
PCXClass* s_ScoreScreenPCX[3][10] = {};

}  // namespace

PCXClass* ScoreScreenClass::DrawScoreScreen(int32 step)
{
    // asm: mov eax, Scenario / mov eax,[eax+34B8h]
    const int32 side = (ScenarioClass::Instance != nullptr)
                           ? ScenarioClass::Instance->PlayerSideIndex
                           : 0;

    // asm: 三张表各一段 cmp 链，任何一条都不命中 -> loc_5CA41B
    if (side < 0 || side > 2)
        return nullptr;                                // asm: xor eax,eax / retn

    // asm: 每张表内 cmp ecx,0 .. cmp ecx,9
    if (step < 0 || step > 9)
        return nullptr;                                // asm: xor eax,eax / retn

    PCXClass*& slot = s_ScoreScreenPCX[side][step];
    if (slot == nullptr)
        slot = PCX_LoadFile11(kScoreScreenNames[side][step]);  // asm: PCX_Draw

    return slot;                                       // asm: retn（eax = 面）
}

// ============================================================================
// asm: Global_DrawScoreScreen (0x5C9B37, stdcall hDlg/uMsg/wParam/lParam;
//      retn 10h；IDA 标注 "sp-analysis failed"，因其局部帧里混用了两套
//      esp 基准——下面按值语义逐句对齐)
//
//   mov eax,[esp+arg_C] / sub esp,18Ch / push ebx,esi,edi
//   push arg_C ; push arg_8 ; edx = arg_4 ; ecx = hDlg
//   call Dialog_HandleGeneralMessages ; test eax,eax ; jnz 返回该值
//   push 8 ; push hDlg ; call GetWindowLongA          ; DWLP_USER
//   cmp msg,0Fh  -> WM_PAINT   (loc_5CA07F)
//   cmp msg,111h -> WM_COMMAND (loc_5CA05F)
//   cmp msg,497h -> 全量重绘；其余落 loc_5CA100（xor eax,eax）
//
// 控件 id 与消息号全部取自 asm 立即数，字符串键与格式串逐字节照抄。
// ============================================================================
namespace {

// 消息号
enum : int32 {
    kMsgPaint        = 0x00F,   // WM_PAINT
    kMsgCommand      = 0x111,   // WM_COMMAND
    kMsgFullRedraw   = 0x497,   // 自定义：整屏记分重建
    kMsgSetText      = 0x4B2,   // 自定义：写控件文本
    kMsgSetColor     = 0x498    // 自定义：设置控件颜色
};

// 控件 id
enum : int32 {
    kIdcScoreGame    = 0x6D2,   // TXT_GAME 文本；WM_PAINT 的第一块矩形
    kIdcScoreTitle   = 0x694,   // GUI:*Score 标题
    kIdcScoreTime    = 0x3EA,   // TXT_TIME_FORMAT_HOURS
    kIdcScoreOk      = 0x6D1    // WM_COMMAND 认的"离开"按钮
};

// ----------------------------------------------------------------------------
// asm .data 里紧挨着 aDRa2mdpostMpob 的三张控件 id 表首尾相连，合计 50 项：
//
//   [ 0..39]  8 组 × 5：每组 = {名次窗格, 四列数字窗格}
//             玩家循环从下标 1 起，故 第 step 行的四列 = [step*5+1 .. +4]，
//             而该行的名次窗格 = p[-1] = [step*5]（asm: mov eax,[ecx-4]）。
//   [40..41]  dword_82FDFC：静态窗格（1936h, 791h），循环上界 dword_82FE04。
//   [42..49]  dword_82FE04：每人一个名字窗格 id（78Fh, 792h..798h）。
//
// WM_PAINT 取的是 aDRa2mdpostMpob+18h[gPlayersInGame*5]，即同一张表按人数
// 跳过整组后落在下一组首项（8 人时正好落到 1936h）。
// ----------------------------------------------------------------------------
const int32 k_ScoreIds[] = {
    0x411, 0x419, 0x6A3, 0x6A5, 0x6A6,
    0x412, 0x41A, 0x6A4, 0x6A7, 0x6A8,
    0x413, 0x41B, 0x6A9, 0x6AA, 0x6AB,
    0x414, 0x41C, 0x6AC, 0x6AD, 0x6AE,
    0x415, 0x41D, 0x6AF, 0x6B0, 0x6B1,
    0x416, 0x41E, 0x6B2, 0x6B3, 0x6B4,
    0x417, 0x41F, 0x6B5, 0x6B6, 0x6B7,
    0x418, 0x420, 0x6B8, 0x6B9, 0x6BA,
    0x1936, 0x791,                              // dword_82FDFC
    0x78F, 0x792, 0x793, 0x794, 0x795, 0x796, 0x797, 0x798   // dword_82FE04
};

constexpr int32 k_IdGroupStride = 5;
constexpr int32 k_IdStaticBase  = 40;
constexpr int32 k_IdStaticCount = 2;
constexpr int32 k_IdNameBase    = 42;
constexpr int32 k_MaxPlayers    = 8;

// asm: dword_B779D4 / dword_A8D57C —— TXT_GAME 的唯一格式参数，分别对应
// Session == 4 与其余情形。两者由 InitGame / MPCoop 一族写入，本树尚未
// 建模那两个写入点，故先按文件级静态量保留（只在本文件读）。
int32 s_Unk_B779D4 = 0;
int32 s_Unk_A8D57C = 0;

// ---------------------------------------------------------------------------
// asm: Get_String 返回 CSF 宽串；本树的字符串表入口 EngineGlobals::Get_String
// 只给窄串（缺键时回退键名），这里逐字节拓宽到调用方缓冲后再交给 swprintf。
// ---------------------------------------------------------------------------
const wchar_t* Score_Wide(const char* pKey, wchar_t* pBuffer, size_t nCapacity)
{
    const char* pText = EngineGlobals::Get_String(pKey);
    size_t i = 0;
    for (; pText[i] != 0 && i + 1 < nCapacity; ++i)
        pBuffer[i] = static_cast<wchar_t>(static_cast<unsigned char>(pText[i]));
    pBuffer[i] = 0;
    return pBuffer;
}

// ---------------------------------------------------------------------------
// asm: sub_517440 (0x517440) —— 把 ColorScheme 的三字节基色折成显示色。
// 原版读 ColorScheme+308h 那片逐色阶 RGB 表；本树的 ColorScheme 没有该表
// （见 Rendering/Surface.h 中 Plain_Text_Print 处同一说明），改用 BaseColor
// 换算，打包顺序与调用点一致：(B<<16)|(G<<8)|R。
// ---------------------------------------------------------------------------
uint32 Score_Player_Color(const ColorScheme* pScheme)
{
    if (pScheme == nullptr)
        return 0;
    return (static_cast<uint32>(pScheme->BaseColor.B) << 16) |
           (static_cast<uint32>(pScheme->BaseColor.G) << 8) |
            static_cast<uint32>(pScheme->BaseColor.R);
}

// ---------------------------------------------------------------------------
// asm: PtFuncCompare (0x5C9AF9) —— 按记录 +60h（Cash）降序：
//   eax = (*a)->Cash ; ecx = (*b)->Cash ; 相等 -> 0
//   否则 setle dl / dec / and 0FFFFFFFEh / inc  => a > b 得 -1，a < b 得 1
// ---------------------------------------------------------------------------
int32 Score_CompareRecord(const void* pLeft, const void* pRight)
{
    const ScoreScreenClass::Record* pA =
        *static_cast<ScoreScreenClass::Record* const*>(pLeft);
    const ScoreScreenClass::Record* pB =
        *static_cast<ScoreScreenClass::Record* const*>(pRight);

    if (pA->Cash == pB->Cash)
        return 0;
    return (pA->Cash > pB->Cash) ? -1 : 1;
}

#if defined(OPENYR_SCORE_WIN32)

// asm: ds:GetDlgItem
void* Score_GetItem(void* hDlg, int32 id)
{
    return reinterpret_cast<void*>(
        GetDlgItem(static_cast<HWND>(hDlg), static_cast<int>(id)));
}

// asm: ds:SendMessageA
void* Score_Send(void* hWnd, int32 msg, int32 wParam, int32 lParam)
{
    return reinterpret_cast<void*>(SendMessageA(
        static_cast<HWND>(hWnd), static_cast<UINT>(msg),
        static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam)));
}

// asm: push <ptr> ; call ds:SendMessageA —— 指针型 lParam，避免 64 位截断。
void* Score_SendPtr(void* hWnd, int32 msg, const void* pParam)
{
    return reinterpret_cast<void*>(SendMessageA(
        static_cast<HWND>(hWnd), static_cast<UINT>(msg), 0,
        reinterpret_cast<LPARAM>(pParam)));
}

// asm: push 8 / call ds:GetWindowLongA （DWLP_USER）
intptr_t Score_GetUserLong(void* hDlg)
{
    return static_cast<intptr_t>(
        GetWindowLongA(static_cast<HWND>(hDlg), DWLP_USER));
}

// ---------------------------------------------------------------------------
// asm: hWnd_GetRect (0x775690) —— GetWindowRect 得到屏幕坐标后，减掉
// hWndParent 客户区原点的屏幕坐标，换成相对游戏窗口客户区的坐标。
// 原版读全局 hWndParent；本树没有该全局，取 GetParent(hWnd)（同一层窗口）。
// ---------------------------------------------------------------------------
void Score_GetRect(void* hWnd, int32* pRect)
{
    if (hWnd == nullptr || pRect == nullptr)
        return;

    RECT rect;
    if (!GetWindowRect(static_cast<HWND>(hWnd), &rect))
        return;                                    // asm: test edi,edi / jnz

    POINT origin = { 0, 0 };
    HWND const hParent = GetParent(static_cast<HWND>(hWnd));
    if (hParent != nullptr)
    {
        RECT client;
        if (GetClientRect(hParent, &client))
        {
            origin.x = client.left;
            origin.y = client.top;
            ClientToScreen(hParent, &origin);      // asm: ds:ClientToScreen
        }
    }

    pRect[0] = static_cast<int32>(rect.left)   - origin.x;   // asm: [esi]
    pRect[1] = static_cast<int32>(rect.top)    - origin.y;   //      [esi+4]
    pRect[2] = static_cast<int32>(rect.right)  - origin.x;   //      [esi+8]
    pRect[3] = static_cast<int32>(rect.bottom) - origin.y;   //      [esi+0Ch]
}

// ---------------------------------------------------------------------------
// asm: sub_603D30 (0x603D30) —— 在 dword_AC1B00 的窗口描述符哈希链里找到
// 该 hWnd 的记录，写 [rec+70h] = 2、[rec+14h] = 绘制结果，再 InvalidateRect。
// 本树没有那套窗口描述符表，登记项无处落地，故只保留"把该控件标脏"这一
// 可观察的效果（原版 WM_PAINT 靠它取回 pane）。
// ---------------------------------------------------------------------------
void Score_InvalidatePane(void* hWnd, int32 pane)
{
    (void)pane;                                    // asm: mov [eax+14h],edi
    InvalidateRect(static_cast<HWND>(hWnd), nullptr, TRUE);
}

#else  // 无窗口系统：原版全部走 user32，这里退化为"取不到句柄/矩形"

void* Score_GetItem(void* /*hDlg*/, int32 /*id*/) { return nullptr; }
void* Score_Send(void* /*hWnd*/, int32 /*msg*/, int32 /*wParam*/,
                 int32 /*lParam*/) { return nullptr; }
void* Score_SendPtr(void* /*hWnd*/, int32 /*msg*/,
                    const void* /*pParam*/) { return nullptr; }
intptr_t Score_GetUserLong(void* /*hDlg*/) { return 0; }
void Score_GetRect(void* /*hWnd*/, int32* pRect)
{
    if (pRect != nullptr)
    {
        pRect[0] = 0;
        pRect[1] = 0;
        pRect[2] = 0;
        pRect[3] = 0;
    }
}
void Score_InvalidatePane(void* /*hWnd*/, int32 /*pane*/) {}

#endif // OPENYR_SCORE_WIN32

} // namespace

int32 ScoreScreenClass::ScoreScreenDlgProc(void* hDlg, uint32 uMsg,
                                           uint32 wParam, int32 lParam)
{
    // asm: call Dialog_HandleGeneralMessages ; test eax,eax ; jnz 返回 eax
    // 该路由是 0x60C7D0 一族、千行级的通用对话框消息分发，不在本批范围，
    // 本树也没有窗口系统；与 Game/Shell.cpp 的做法一致，直接进入本过程
    // 自己的三条分支。
    (void)lParam;

    // asm: push 8 / call GetWindowLongA —— DWLP_USER
    const intptr_t userLong = Score_GetUserLong(hDlg);

    // ---- WM_PAINT（asm loc_5CA07F）----------------------------------------
    if (uMsg == static_cast<uint32>(kMsgPaint)) {
        // asm: GetDlgItem(hDlg,6D2h) 的 hWnd_GetRect
        int32 rcFirst[4] = { 0, 0, 0, 0 };
        Score_GetRect(Score_GetItem(hDlg, kIdcScoreGame), rcFirst);

        // asm: eax = gPlayersInGame ; ecx = eax*5 ; id = 表[ecx]
        // （原版不检查人数上界；本表按 8 组展开，越界值夹回 8 以免读穿）
        int32 const players =
            (s_PlayersInGame < k_MaxPlayers) ? s_PlayersInGame : k_MaxPlayers;
        int32 const paneId = k_ScoreIds[players * k_IdGroupStride];

        int32 rcSecond[4] = { 0, 0, 0, 0 };
        Score_GetRect(Score_GetItem(hDlg, paneId), rcSecond);

        // asm: 先倒出 (rect1.left, rect1.top, rect2.right - rect1.left,
        //      rect2.bottom - rect1.top) 四元组；随后 sub_72AA10(&r, -4, -8)
        //      按 x+=dx / y+=dy / w-=2dx / h-=2dy 把矩形外扩。
        Rectangle pane(rcFirst[0], rcFirst[1],
                       rcSecond[2] - rcFirst[0],
                       rcSecond[3] - rcFirst[1]);
        pane.X      -= 4;
        pane.Y      -= 8;
        pane.Width  += 8;
        pane.Height += 16;

        // asm: sub_621B80(&pane, DSurface_Alternate, 0, 9Fh) —— 以 alpha 9Fh
        // 把这块区域向 black 压（第 3 个参数 0 = 混合色）。
        if (DSurface_Alternate != nullptr) {
            ColorStruct black(0, 0, 0, 0);
            DSurface_Alternate->FillRectTrans(&pane, &black, 0x9F);
        }
        return 0;                                              // loc_5CA100
    }

    // ---- WM_COMMAND（asm loc_5CA05F）--------------------------------------
    if (uMsg == static_cast<uint32>(kMsgCommand)) {
        // asm: cmp bx,6D1h / jnz loc_5CA100 ; mov dword ptr [eax],1
        if ((wParam & 0xFFFFu) == static_cast<uint32>(kIdcScoreOk) &&
            userLong != 0) {
            *reinterpret_cast<int32*>(userLong) = 1;
        }
        return 0;
    }

    if (uMsg != static_cast<uint32>(kMsgFullRedraw))
        return 0;                                              // loc_5CA100

    // ========================================================================
    // 0x497：整屏重建（asm 18025-18528）
    // ========================================================================

    // asm: ThemeClass::From_Name(ecx=&Theme,"SCORE") ; ThemeClass::Play_Song
    if (TheTheme != nullptr) {
        int32 const song = TheTheme->From_Name("SCORE");
        TheTheme->Play_Song(song);
    }

    wchar_t buf[34]  = { 0 };        // asm var_140（0x44 字节 = 34 个宽字符）
    wchar_t fmtA[64] = { 0 };
    wchar_t fmtB[64] = { 0 };

    // asm 18033-18066：Session == 4 用 174h 行、其余用 176h 行的 TXT_GAME，
    // 唯一参数分别是 dword_B779D4 / dword_A8D57C。
    if (g_Session == 4)
        std::swprintf(buf, 34, Score_Wide("TXT_GAME", fmtA, 64), s_Unk_B779D4);
    else
        std::swprintf(buf, 34, Score_Wide("TXT_GAME", fmtA, 64), s_Unk_A8D57C);

    // asm 18068-18089：Session == 5 -> GUI:SkirmishScore（17Ah 行），
    // 否则 GUI:MultiplayerScore（17Ch 行）。
    wchar_t const* const pTitle = Score_Wide(
        (g_Session == 5) ? "GUI:SkirmishScore" : "GUI:MultiplayerScore",
        fmtB, 64);

    Score_SendPtr(Score_GetItem(hDlg, kIdcScoreTitle), kMsgSetText, pTitle);
    Score_SendPtr(Score_GetItem(hDlg, kIdcScoreGame),  kMsgSetText, buf);

    // asm 18115-18129：耗时统计
    //   ticks = [Scenario+61Ch]
    //   if ([Scenario+614h] != -1)
    //       ticks += WinTimerClass::Get_System_Tick_Count(Scenario+618h)
    //                - [Scenario+614h]
    int32 ticks = 0;
    if (ScenarioClass::Instance != nullptr) {
        ScenarioClass::LoadElapsedTimer const& timer =
            ScenarioClass::Instance->LoadTimer;
        ticks = timer.Accumulated;                          // asm: +61Ch
        if (timer.Start != -1)                              // asm: +614h
            ticks += static_cast<int32>(Game::GetTickCount()) - timer.Start;
    }

    // asm 18133-18185：三组 "add-back" magic 除法，即 /1000、/3600、/60。
    // 中间 `lea edx,[var_140]; test edx,edx; jz` 测的是栈地址，永不跳。
    int32 secs  = ticks / 1000;
    int32 hours = secs / 3600;
    if (hours > 99)                                         // asm: cmp edx,63h
        secs = 0x57E3F;                                     // 359999 秒
    hours = secs / 3600;
    int32 rem  = secs - 3600 * hours;
    int32 const mins = rem / 60;
    rem -= 60 * mins;

    // asm: Get_String("TXT_TIME_FORMAT_HOURS",0,file,0D9h)
    //      _swprintf(buf, fmt, hours, mins, rem)
    std::swprintf(buf, 34, Score_Wide("TXT_TIME_FORMAT_HOURS", fmtA, 64),
                  hours, mins, rem);
    Score_SendPtr(Score_GetItem(hDlg, kIdcScoreTime), kMsgSetText, buf);

    // asm: call Global_ShowScoreScreen2（0x5C98B0 -> FillMPRecords）
    ScoreScreenClass::FillMPRecords();

    // asm 18217-18244：endScores[i] = &HouseScores[i]，随后按 Cash 降序 qsort
    // （同上：记录表定长 8，越界值夹回 8）
    int32 const players =
        (s_PlayersInGame < k_MaxPlayers) ? s_PlayersInGame : k_MaxPlayers;
    ScoreScreenClass::Record* endScores[k_MaxPlayers] = { nullptr };
    for (int32 i = 0; i < players; ++i)
        endScores[i] = &s_PlayerRecords[i];       // asm: +70h 步长
    if (players > 1)
        std::qsort(endScores, static_cast<size_t>(players), sizeof(void*),
                   Score_CompareRecord);          // asm: _qsort(..,4,PtFuncCompare)

    // asm 18247-18287：静态窗格 dword_82FDFC .. dword_82FE04
    for (int32 step = 0; step < k_IdStaticCount; ++step) {
        void* hPane = Score_GetItem(hDlg, k_ScoreIds[k_IdStaticBase + step]);
        PCXClass* pPane = ScoreScreenClass::DrawScoreScreen(step);
        if (hPane != nullptr && pPane != nullptr)
            Score_InvalidatePane(
                hPane, static_cast<int32>(reinterpret_cast<intptr_t>(pPane)));
    }

    if (players <= 0)
        return 0;                                              // asm: jle

    // asm 18284-18520：每玩家一行
    for (int32 step = 0; step < players; ++step) {
        ScoreScreenClass::Record const* pRecord = endScores[step];
        if (pRecord == nullptr)
            continue;

        // asm: ColorScheme.data[record->Scheme] + 308h -> sub_517440
        ColorScheme* pScheme = nullptr;
        if (ColorScheme::Array != nullptr && pRecord->Scheme >= 0 &&
            pRecord->Scheme < ColorScheme::Array->Size())
            pScheme = (*ColorScheme::Array)[pRecord->Scheme];
        uint32 const color = Score_Player_Color(pScheme);

        // 名字窗格（asm: dword_82FE04[step]），内容为 step+2 号背景条
        void* hName = Score_GetItem(hDlg, k_ScoreIds[k_IdNameBase + step]);
        PCXClass* pNamePane = ScoreScreenClass::DrawScoreScreen(step + 2);
        if (hName != nullptr && pNamePane != nullptr)
            Score_InvalidatePane(
                hName, static_cast<int32>(reinterpret_cast<intptr_t>(pNamePane)));

        // asm 18339-18366：本行名次窗格 —— 先送名字（lParam 是记录本身，
        // 记录头 +00h 就是 0x28 字节宽名牌），再发 498h 上色。
        int32 const* p = &k_ScoreIds[step * k_IdGroupStride + 1];
        int32 const rankId = p[-1];                             // asm: [ecx-4]
        Score_SendPtr(Score_GetItem(hDlg, rankId), kMsgSetText, pRecord->Name);
        Score_Send(Score_GetItem(hDlg, rankId), kMsgSetColor, 0,
                   static_cast<int32>(color));

        // asm 18368-18510：四列数字，取值顺序 +40h/+30h/+50h/+60h，
        // 每列 swprintf(L"%d") -> 4B2h 写文本 -> 498h 上色。
        int32 const values[4] = { pRecord->Score, pRecord->Lost,
                                  pRecord->Kills, pRecord->Cash };
        for (int32 col = 0; col < 4; ++col) {
            wchar_t cell[34] = { 0 };
            std::swprintf(cell, 34, L"%d", values[col]);
            Score_SendPtr(Score_GetItem(hDlg, p[col]), kMsgSetText, cell);
            Score_Send(Score_GetItem(hDlg, p[col]), kMsgSetColor, 0,
                       static_cast<int32>(color));
        }
    }

    return 0;                                                  // asm: xor eax,eax
}
