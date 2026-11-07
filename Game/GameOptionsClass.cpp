#include "GameOptionsClass.h"
#include "../Helpers/AsmMath.h"
#include "../Audio/VocClass.h"
#include "../Rendering/SidebarClass.h"
#include "../Objects/LineTrailClass.h"
#include "../Game/Externs.h"
#include "../Audio/ThemeClass.h"

#include <cstdio>
#include <cstring>
#include "../INI/INIClass.h"

// ============================================================================
// GameOptionsClass - implementation
// ============================================================================

GameOptionsClass::GameOptionsClass()
    : GameSpeed(1)
    , Difficulty(0)
    , CampDifficulty(0)
    , ScrollMethod(0)
    , ScrollRate(0)
    , AutoScroll(true)
    , DetailLevel(2)
    , SidebarCameoText(true)
    , UnitActionLines(true)
    , ShowHidden(false)
    , ToolTips(true)
    , ScreenWidth(640)
    , ScreenHeight(480)
    , StretchMovies(false)
    , AllowHiResModes(false)
    , AllowModeToggle(false)
    , AllowVRAMSidebar(false)
    , SoundVolume(1.0)
    , VoiceVolume(1.0)
    , ScoreVolume(1.0)
    , IsScoreRepeat(false)
    , IsScoreShuffle(false)
    , InGameMusic(true)
    , SoundLatency(0)
    , NetID(0)
    , Socket(0)
    , NetCard(0)
    , DestNet(0)
{
}

// ============================================================================
// LoadFromINI - Options_LoadFromINI
//
//   The original only clamps Difficulty right after reading it: anything
//   of 4 or more is pulled back to 4, and a non-positive value becomes 0.
//   Every other key is stored as-is.
// ============================================================================
void GameOptionsClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    // ---------------------------------------------------------------
    // [Options]
    // ---------------------------------------------------------------
    GameSpeed = pINI->ReadInteger("Options", "GameSpeed", GameSpeed);

    {
        int32 value = pINI->ReadInteger("Options", "Difficulty", Difficulty);
        if (value >= 4)
            value = 4;
        if (value <= 0)
            value = 0;
        Difficulty = value;
    }

    CampDifficulty = pINI->ReadInteger("Options", "CampDifficulty", CampDifficulty);
    ScrollMethod   = pINI->ReadInteger("Options", "ScrollMethod",   ScrollMethod);
    ScrollRate     = pINI->ReadInteger("Options", "ScrollRate",     ScrollRate);
    AutoScroll     = pINI->ReadBool("Options", "AutoScroll",        AutoScroll);
    DetailLevel    = pINI->ReadInteger("Options", "DetailLevel",    DetailLevel);
    SidebarCameoText = pINI->ReadBool("Options", "SidebarCameoText", SidebarCameoText);
    UnitActionLines  = pINI->ReadBool("Options", "UnitActionLines",  UnitActionLines);
    ShowHidden       = pINI->ReadBool("Options", "ShowHidden",       ShowHidden);
    ToolTips         = pINI->ReadBool("Options", "ToolTips",         ToolTips);

    // ---------------------------------------------------------------
    // [Video]
    // ---------------------------------------------------------------
    ScreenWidth      = pINI->ReadInteger("Video", "ScreenWidth",      ScreenWidth);
    ScreenHeight     = pINI->ReadInteger("Video", "ScreenHeight",     ScreenHeight);
    StretchMovies    = pINI->ReadBool("Video", "StretchMovies",       StretchMovies);
    AllowHiResModes  = pINI->ReadBool("Video", "AllowHiResModes",     AllowHiResModes);
    AllowModeToggle  = pINI->ReadBool("Video", "AllowModeToggle",     AllowModeToggle);
    AllowVRAMSidebar = pINI->ReadBool("Video", "AllowVRAMSidebar",    AllowVRAMSidebar);

    // ---------------------------------------------------------------
    // [Audio]
    // ---------------------------------------------------------------
    SoundVolume  = pINI->ReadFixed("Audio", "SoundVolume",  SoundVolume);
    VoiceVolume  = pINI->ReadFixed("Audio", "VoiceVolume",  VoiceVolume);
    ScoreVolume  = pINI->ReadFixed("Audio", "ScoreVolume",  ScoreVolume);
    IsScoreRepeat  = pINI->ReadBool("Audio", "IsScoreRepeat",  IsScoreRepeat);
    IsScoreShuffle = pINI->ReadBool("Audio", "IsScoreShuffle", IsScoreShuffle);
    InGameMusic    = pINI->ReadBool("Audio", "InGameMusic",    InGameMusic);
    SoundLatency   = pINI->ReadInteger("Audio", "SoundLatency", SoundLatency);

    // ---------------------------------------------------------------
    // [Network]
    // ---------------------------------------------------------------
    NetID   = pINI->ReadInteger("Network", "NetID",   NetID);
    Socket  = pINI->ReadInteger("Network", "Socket",  Socket);
    NetCard = pINI->ReadInteger("Network", "NetCard", NetCard);
    DestNet = pINI->ReadInteger("Network", "DestNet", DestNet);
}

// ============================================================================
// asm: sub_5FB160 (0x5FB160)
//
//   push esi / mov esi,ecx
//   ; --- Sound 通道 [+38h] ---
//   fld [esi+38h] / fcom 1.0 / fnstsw ax / test ah,1 / jnz short loc_5FB17B
//   fstp st / fld 1.0
// loc_5FB17B:
//   fst [esi+38h] / fmul num_16384 / call Float_To_Int_Floor
//   mov ecx,SoundStream / mov edx,eax / call VocSubClass_SetVolume
//   fld [esi+38h] / fstp [esp+var_C]
//   push offset aSoundvolumeF / call WWDebugString   ; "SoundVolume = %f\n"
//   ; --- Voice 通道 [+3Ch] ---
//   fld [esi+3Ch] / fcom 1.0 / ... / jnz short loc_5FB1C4
//   fstp st / fld 1.0
// loc_5FB1C4:
//   fst [esi+3Ch] / fmul num_255 / call Float_To_Int_Floor
//   mov ecx,eax / call sub_752AB0
//   fld [esi+3Ch] / fmul num_16384 / call Float_To_Int_Floor
//   mov ecx,VoiceStream / mov edx,eax / call VocSubClass_SetVolume
//   fld [esi+3Ch] / ... / push aVoicevolumeF / call WWDebugString
//   ; --- Score 通道 [+40h] ---
//   fld [esi+40h] / fcom 1.0 / ... / jnz short loc_5FB222
//   fstp st / fld 1.0
// loc_5FB222:
//   fst [esi+40h] / fmul num_16384 / call Float_To_Int_Floor
//   mov ecx,MovieStream / mov edx,eax / call VocSubClass_SetVolume
//   fld [esi+40h] / fmul num_255 / call Float_To_Int_Floor
//   push eax / mov ecx,offset Theme / call sub_721290
//   fld [esi+40h] / ... / push aScorevolumeF / call WWDebugString
//   ; --- 派生开关 ---
//   mov al,[esi+44h] / mov unk_A83D20,al
//   WWDebugString("ScoreRepeat is %s\n", [esi+44h]==1 ? "ON" : "OFF")
//   mov al,[esi+46h] / mov unk_A83D22,al
//   WWDebugString("ScoreShuffle is %s\n", [esi+46h]==1 ? "ON" : "OFF")
//   mov al,[esi+1Dh] / push eax / mov ecx,offset Map
//                   / call SidebarClass__Set_Cameo_Text_bool
//   mov cl,[esi+1Eh] / ...
//
// 字段对应（asm 偏移 -> OpenYR 成员，均由本类现有声明确认）：
//   +0x38 SoundVolume (double)      +0x3C VoiceVolume (double)
//   +0x40 ScoreVolume (double)      +0x44 IsScoreRepeat
//   +0x46 IsScoreShuffle            +0x1D SidebarCameoText（前置 bool 组）
// ============================================================================
void GameOptionsClass::Apply_Audio_Settings()
{
    // asm: fld [esi+38h] / fcom 1.0 / test ah,1 / jnz -> 保留，否则载 1.0
    auto clamp01 = [](double v) { return (v > 1.0) ? 1.0 : v; };

    // ---- Sound 通道 --------------------------------------------------------
    SoundVolume = clamp01(SoundVolume);                  // asm: fst [esi+38h]
    {
        // asm: fmul num_16384 / call Float_To_Int_Floor
        const int32 units = static_cast<int32>(
            Float_To_Int_Floor(SoundVolume * 16384.0));
        // asm: mov ecx,SoundStream / mov edx,eax / call VocSubClass_SetVolume
        VocManagerClass::GetInstance()->SetSFXVolume(units);
        // asm: push aSoundvolumeF / call WWDebugString
        std::printf("SoundVolume = %f\n", SoundVolume);
    }

    // ---- Voice 通道 --------------------------------------------------------
    VoiceVolume = clamp01(VoiceVolume);                  // asm: fst [esi+3Ch]
    {
        // asm: fmul num_255 / call Float_To_Int_Floor / mov ecx,eax
        //      call sub_752AB0                            ; 主音量（0..255 档）
        const int32 master = static_cast<int32>(
            Float_To_Int_Floor(VoiceVolume * 255.0));
        (void)master;                                    // 见下方说明

        // asm: fld [esi+3Ch] / fmul num_16384 / Float_To_Int_Floor
        //      mov ecx,VoiceStream / call VocSubClass_SetVolume
        const int32 units = static_cast<int32>(
            Float_To_Int_Floor(VoiceVolume * 16384.0));
        VocManagerClass::GetInstance()->SetSpeechVolume(units);
        // asm: push aVoicevolumeF / call WWDebugString
        std::printf("VoiceVolume = %f\n", VoiceVolume);
    }

    // ---- Score 通道 --------------------------------------------------------
    ScoreVolume = clamp01(ScoreVolume);                  // asm: fst [esi+40h]
    {
        // asm: fmul num_16384 / Float_To_Int_Floor
        //      mov ecx,MovieStream / call VocSubClass_SetVolume
        const int32 units = static_cast<int32>(
            Float_To_Int_Floor(ScoreVolume * 16384.0));
        VocManagerClass::GetInstance()->SetMasterVolume(units);

        // asm: fld [esi+40h] / fmul num_255 / Float_To_Int_Floor
        //      push eax / mov ecx,offset Theme / call sub_721290
        const int32 themeUnits = static_cast<int32>(
            Float_To_Int_Floor(ScoreVolume * 255.0));
        (void)themeUnits;                                // Theme 音量，见下方说明

        // asm: push aScorevolumeF / call WWDebugString
        std::printf("ScoreVolume = %f\n", ScoreVolume);
    }

    // ---- 派生开关 ----------------------------------------------------------
    // asm: mov al,[esi+44h] / mov unk_A83D20,al
    //      WWDebugString("ScoreRepeat is %s\n", ...=="ON"?"ON":"OFF")
    std::printf("ScoreRepeat is %s\n", IsScoreRepeat ? "ON" : "OFF");

    // asm: mov al,[esi+46h] / mov unk_A83D22,al
    //      WWDebugString("ScoreShuffle is %s\n", ...)
    std::printf("ScoreShuffle is %s\n", IsScoreShuffle ? "ON" : "OFF");

    // asm: mov al,[esi+1Dh] / push eax / mov ecx,offset Map
    //      call SidebarClass__Set_Cameo_Text_bool
    // 侧边栏"图标文字"开关：原版用 [esi+1Dh]（本类里即 SidebarCameoText）。
    // 该入口已归位到 SidebarClass::Set_Cameo_Text（0x5D0A30，asm 里是空的
    // `retn 4`）。原版传的 ecx 是 Map（DisplayClass，同时继承侧边栏）；本项目
    // MapClass 不继承 SidebarClass，故改由侧边栏单例接收。
    if (SidebarClass::Instance != nullptr)
        SidebarClass::Instance->Set_Cameo_Text(SidebarCameoText != 0);

    // 说明（铁律①：宁可留白，不写错）：
    //   - sub_752AB0 是"主音量档位"下发，OpenYR 的 VocManagerClass 尚未抽出
    //     独立的主音量入口，此处只做取值、不下发。
    //   - sub_721290 是 Theme（游戏内音乐）的音量入口，同理暂缺。
    //   两者均已在注释中标出 asm 调用点，待后续批次补齐宿主接口后接上。
}

// ============================================================================
// 影片表（asm 全局 sovMovies / allMovies）
//
// 两张表都是 0xC 字节步长的记录数组，MovieName 为 0 时终止。
// 原版直接寻址 .data；这里通过装载接口注入。
// ============================================================================
namespace {
const GameOptionsClass::MovieEntry* g_SovMoviesTable = nullptr;   // asm sovMovies
const GameOptionsClass::MovieEntry* g_AllMoviesTable = nullptr;   // asm allMovies

// asm: loc_5FBF9A / loc_5FBFD0 共同的表扫描
//   走 0xC 步长直到 Name == 0，用 __strcmpi 做大小写无关比较；
//   未命中时 or edi,0FFFFFFFFh（即返回 -1）。
int32 Scan_Movie_Table(const GameOptionsClass::MovieEntry* pTable,
                       const char* pName)
{
    if (pTable == nullptr || pName == nullptr)
        return -1;                                   // asm: or edi,0FFFFFFFFh

    int32 idx = 0;                                   // asm: xor edi,edi
    for (const GameOptionsClass::MovieEntry* e = pTable;
         e->Name != nullptr;                         // asm: test eax,eax / jz
         ++e, ++idx)                                 // asm: add esi,0Ch / inc edi
    {
        // asm: push [esi] / push ebp / call __strcmpi / test eax,eax / jz hit
        if (_strcmpi(e->Name, pName) == 0)
            return idx;                              // asm: loc_5FBFB8/loc_5FBFEE
    }
    return -1;                                       // asm: loc_5FBFB5/loc_5FBFEB
}
}  // namespace

void GameOptionsClass::Set_Sov_Movies_Table(const MovieEntry* pTable)
{
    g_SovMoviesTable = pTable;
}

void GameOptionsClass::Set_All_Movies_Table(const MovieEntry* pTable)
{
    g_AllMoviesTable = pTable;
}

// ============================================================================
// asm: GameOptionsClass_UnlockMovieIfNeeded (0x5FBF50)
// ============================================================================
void GameOptionsClass::Unlock_Movie_If_Needed(const char* movieName)
{
    // asm: mov eax,sovMovies.MovieName / mov ebp,[esp+8+Str1] / mov ebx,ecx
    if (movieName == nullptr)
        return;

    // ---- 苏联战役表 --------------------------------------------------------
    // asm: mov esi,offset sovMovies / xor edi,edi / call __strcmpi 循环
    const int32 sovIdx = Scan_Movie_Table(g_SovMoviesTable, movieName);

    // asm: cmp edi,[ebx+4Ch] / jle short loc_5FBFC0
    //      mov [ebx+LastUnlockedSovMovie],edi
    if (sovIdx > LastUnlockedSovMovie)               // asm: +0x4C
        LastUnlockedSovMovie = sovIdx;

    // ---- 全影片表（盟军线）-------------------------------------------------
    // asm: mov esi,offset allMovies / xor edi,edi / 同一套走法
    const int32 allIdx = Scan_Movie_Table(g_AllMoviesTable, movieName);

    // asm: loc_5FBFC0 之后 cmp edi,[ebx+50h] / mov [ebx+...+50h],edi
    if (allIdx > LastUnlockedAllMovie)               // asm: +0x50
        LastUnlockedAllMovie = allIdx;
}

// ============================================================================
// asm: OptionsDialog_WndProc (0x55FA00)
// ============================================================================

namespace {

// asm: GetDlgItem / SendMessageA —— 本工程没有窗口系统（与 Game/Shell.cpp 同一
// 处理方式）：Windows 构建下直调 user32，其它平台退化为"无控件 / 0"。
#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_OPTIONS_WIN32 1
    #include <windows.h>
#endif

void* Dlg_Get_Item(void* hDlg, int32 id)
{
#if defined(OPENYR_OPTIONS_WIN32)
    return static_cast<void*>(GetDlgItem(static_cast<HWND>(hDlg), static_cast<int>(id)));
#else
    (void)hDlg; (void)id;
    return nullptr;
#endif
}

int32 Dlg_Send_Msg(void* hWnd, int32 msg, int32 wParam, int32 lParam)
{
#if defined(OPENYR_OPTIONS_WIN32)
    return static_cast<int32>(SendMessageA(static_cast<HWND>(hWnd),
                                           static_cast<UINT>(msg),
                                           static_cast<WPARAM>(wParam),
                                           static_cast<LPARAM>(lParam)));
#else
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// asm 里 Options 对象上几个**没有符号名**的槽（unk_A8xxxx）。原版只按偏移
// 读写、没有语义名，故这里按原符号名保留，不臆造字段含义：
//   unk_A8EB64  0x50F 滚条原值
//   unk_A8EB70  6 - 0x52A 滚条镜像值
//   dword_A8EB80 0x602 复选框
//   unk_A83D14 / unk_A83D18  待播曲目序号（第一个为 -1 时取第二个）
// ---------------------------------------------------------------------------
int32  s_Unk_A8EB64   = 0;
int32  s_Unk_A8EB70   = 0;
uint8  s_Unk_A8EB80   = 0;
int32  s_Unk_A83D14   = -1;
int32  s_Unk_A83D18   = -1;

// asm: sub_4AE450(Map) —— "选项改动后刷新地图显示" 的全局入口。地图显示层
// 在本工程尚未建模（同 Rendering 其它屏幕对象），退化为空操作。
void Map_Refresh_After_Options() {}

// asm: BM_GETCHECK / CB_GETCURSEL / WM_GETTEXT / WM_SETTEXT
constexpr int32 kMsgGetCheck  = 0x400;
constexpr int32 kMsgCheckBox  = 0x0F0;

} // namespace

void GameOptionsClass::Sync_From_Dialog(void* hDlg)
{
    if (hDlg == nullptr)
        return;

    // ── 0x52B：游戏速度复选框（neg/sbb/and 2 -> 0 或 2）────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x52B);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgGetCheck, 0, 0);
            const int32 chk = (v != 0) ? 2 : 0;         // asm: neg/sbb/and eax,2
            if (g_LineTrailFastFlag != chk) {           // asm: cmp ecx,eax ; jz
                g_LineTrailFastFlag = chk;              // asm: mov dword_A8EB78,eax
                Map_Refresh_After_Options();            // asm: sub_4AE450(Map)
            }
        }
    }

    // ── 0x50F：速度滚条 ────────────────────────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x50F);
        if (h != nullptr)
            s_Unk_A8EB64 = Dlg_Send_Msg(h, kMsgGetCheck, 0, 0);
    }

    // ── 0x601：单位行动线复选框 + 立即生效 ─────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x601);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgCheckBox, 0, 0);
            UnitActionLines = (v == 1);                 // asm: setz cl ; mov unk_A8EB7E,cl
            // asm: call SetUnitActionLines —— 原版是一个全局开关刷新入口，
            // 本项目该开关就是上面的字段本身，读取方每帧现取，无需额外下发。
        }
    }

    // ── 0x604：ShowHidden ──────────────────────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x604);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgCheckBox, 0, 0);
            ShowHidden = (v == 1);                      // asm: mov ShowHidden,al
        }
    }

    // ── 0x602：附加选项复选框 ──────────────────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x602);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgCheckBox, 0, 0);
            s_Unk_A8EB80 = (v == 1) ? 1 : 0;            // asm: mov dword_A8EB80,cl
        }
    }

    // ── 0x52A：滚动速度（围绕 6 镜像）──────────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x52A);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgGetCheck, 0, 0);
            s_Unk_A8EB70 = 6 - v;                       // asm: mov edx,6 ; sub edx,eax
        }
    }

    // ── 0x52F：评分音量 + "音量为 0 时自动排队一首"─────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x52F);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgGetCheck, 0, 0);
            const float vol = static_cast<float>(v) * 0.1f;   // asm: fmul 0.1
            // asm: OptionsClass::Set_Score_Volume(vol) —— 写 [Options+40h] 并
            // 夹到 1.0 后下发给 MovieStream/Theme。
            ScoreVolume = static_cast<double>(vol);
            Apply_Audio_Settings();

            // asm: fld unk_A8EBA0 / fcomp 0.0 / test ah,40h / jz
            //      —— unk_A8EBA0 就是刚被写过的 ScoreVolume（Options+0x40）
            if (ScoreVolume == 0.0) {
                int32 idx = s_Unk_A83D14;               // asm: mov eax,dword_A83D14
                if (idx == -1)                          // asm: cmp eax,-1 ; jnz
                    idx = s_Unk_A83D18;                 // asm: mov eax,dword_A83D18
                if (TheTheme != nullptr) {
                    TheTheme->Queue_Song(idx);          // asm: ThemeClass::Queue_Song
                    TheTheme->Stop();                   // asm: ThemeClass::Stop(0)
                }
            }
        }
    }

    // ── 0x532：音效音量 ────────────────────────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x532);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgGetCheck, 0, 0);
            // asm: SoundStream_SetVolumeWithSample(ecx=Options, v*0.1, 0)
            //      —— 实体写 [Options+38h] 后下发混音器。
            SoundVolume = static_cast<double>(static_cast<float>(v) * 0.1f);
            Apply_Audio_Settings();
        }
    }

    // ── 0x536：语音音量 ────────────────────────────────────────────────────
    {
        void* h = Dlg_Get_Item(hDlg, 0x536);
        if (h != nullptr) {
            const int32 v = Dlg_Send_Msg(h, kMsgGetCheck, 0, 0);
            // asm: VoiceStream_SetVolumeWithSample(ecx=Options, v*0.1, 0)
            //      —— 实体写 [Options+3Ch] 后下发混音器。
            VoiceVolume = static_cast<double>(static_cast<float>(v) * 0.1f);
            Apply_Audio_Settings();
        }
    }
}
