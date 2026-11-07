#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// GameOptionsClass - the persistent SUN.INI settings
//
//  Every entry is a user preference written by the options dialog and read
//  back at start-up.  Options_LoadFromINI merges the file over this object,
//  exactly as the original does, so a missing key leaves the current value
//  untouched.
// ============================================================================

class GameOptionsClass
{
public:
    GameOptionsClass();

    // Reads [Options], [Video], [Audio] and [Network] from the INI file.
    void LoadFromINI(CCINIClass* pINI);

    // ========================================================================
    // asm: sub_5FB160 (0x5FB160)
    //
    //   把三个音量（Sound/Voice/Score）夹到 [0,1] 后下发到混音器，并同步
    //   ScoreRepeat / ScoreShuffle / 侧边栏文本等派生开关。
    //
    //   asm 逐段（每个通道同一套模式）：
    //     fld [esi+38h] / fcom 1.0 / test ah,1 / jnz -> 保留；否则载入 1.0
    //     fst [esi+38h] / fmul 16384 / Float_To_Int_Floor
    //     mov ecx,SoundStream / mov edx,eax / call VocSubClass_SetVolume
    //     WWDebugString("SoundVolume = %f\n", [esi+38h])
    //     -- 同上：[+3Ch] -> 255 缩放 + VoiceStream + "VoiceVolume = %f"
    //     -- 同上：[+40h] -> 16384 + MovieStream + 255 -> Theme ->
    //        "ScoreVolume = %f"
    //     mov al,[esi+44h] / mov unk_A83D20,al
    //     WWDebugString("ScoreRepeat is %s\n", [esi+44h]==1 ? "ON" : "OFF")
    //     mov al,[esi+46h] / mov unk_A83D22,al
    //     WWDebugString("ScoreShuffle is %s\n", [esi+46h]==1 ? "ON" : "OFF")
    //     mov al,[esi+1Dh] / push eax / mov ecx,offset Map
    //                     / call SidebarClass__Set_Cameo_Text_bool
    //     mov cl,[esi+1Eh] ; ...
    //
    //   注：AsmComplex2 里的 `VoiceStream_SetVolumeWithSample`（注释标
    //   0x5FA570）实际是这段的一个**截断片段**，签名与语义都不对——
    //   0x5FA570 在 asm 中并不存在。这里按 asm 原文归位到 GameOptionsClass
    //   （铁律①：以 asm 为唯一权威）。
    //
    //   OpenYR 侧混音器经 VocManagerClass::GetInstance()->SetVolume 下发，
    //   与原版 VocSubClass_SetVolume 等价。
    // ========================================================================
    void Apply_Audio_Settings();

    // ========================================================================
    // asm: OptionsDialog_WndProc (0x55FA00, __fastcall, ecx = hDlg)
    //
    //  选项对话框 WndProc 的"把控件值回写进 Options"那一段（sub_55FC80+7F
    //  调用）。逐控件（GetDlgItem + SendMessageA）：
    //    0x52B BM_GETCHECK -> neg/sbb/and 2 -> dword_A8EB78（0 或 2），
    //          值变化时再调 sub_4AE450(Map) 刷新
    //    0x50F BM_GETCHECK -> unk_A8EB64
    //    0x601 0xF0        -> unk_A8EB7E = (v==1) ; 再调 SetUnitActionLines
    //    0x604 0xF0        -> ShowHidden = (v==1)
    //    0x602 0xF0        -> dword_A8EB80 = (v==1)
    //    0x52A 0x400       -> unk_A8EB70 = 6 - v
    //    0x52F 0x400       -> Options::Set_Score_Volume(v*0.1)；
    //                         若 ScoreVolume == 0 则 Queue_Song(A83D14 ?:
    //                         A83D18) + Theme::Stop()
    //    0x532 0x400       -> Set_Sound_Volume(v*0.1)（ecx 同样是 Options）
    //    0x536 0x400       -> Set_Voice_Volume(v*0.1)
    //
    //  绑定关系说明：asm 里 Set_Score_Volume / SoundStream_SetVolumeWithSample
    //  / VoiceStream_SetVolumeWithSample 三个被调入口的 ecx 都是 `offset
    //  Options`，实测其体会写 [ecx+38h]/[3Ch]/[40h] —— 即 OptionsClass 自己的
    //  三个音量字段（后两个的符号名有误导性）。本项目把它们统一走
    //  Apply_Audio_Settings，故三处都是"写字段 + Apply_Audio_Settings"。
    // ========================================================================
    void Sync_From_Dialog(void* hDlg);


    // ------------------------------------------------------------------
    // [Options]
    // ------------------------------------------------------------------
    int32  GameSpeed;
    int32  Difficulty;
    int32  CampDifficulty;
    int32  ScrollMethod;
    int32  ScrollRate;
    bool   AutoScroll;
    int32  DetailLevel;
    bool   SidebarCameoText;
    bool   UnitActionLines;
    bool   ShowHidden;
    bool   ToolTips;

    // ------------------------------------------------------------------
    // [Video]
    // ------------------------------------------------------------------
    int32  ScreenWidth;
    int32  ScreenHeight;
    bool   StretchMovies;
    bool   AllowHiResModes;
    bool   AllowModeToggle;
    bool   AllowVRAMSidebar;

    // ------------------------------------------------------------------
    // [Audio]
    // ------------------------------------------------------------------
    double SoundVolume;
    double VoiceVolume;
    double ScoreVolume;
    bool   IsScoreRepeat;
    bool   IsScoreShuffle;
    bool   InGameMusic;
    int32  SoundLatency;

    // ------------------------------------------------------------------
    // [Network]
    // ------------------------------------------------------------------
    int32  NetID;
    int32  Socket;
    int32  NetCard;
    int32  DestNet;

    // ========================================================================
    // 影片解锁进度（asm 权威布局 GameOptionsClass sizeof=0xB8）
    //
    //   +0x4C  LastUnlockedSovMovie   苏联战役已解锁到第几部
    //   +0x50  LastUnlockedAllMovie   盟军战役已解锁到第几部
    //
    // 相邻字段（+0x44 IsScoreRepeat / +0x45 InGameMusic / +0x46
    // IsScoreShuffle / +0x48 SoundLatency(dw) / +0x4A Socket(dw)）与 asm
    // 结构体定义一致，这里沿用本类既有的成员序。
    // ========================================================================
    int32  LastUnlockedSovMovie;
    int32  LastUnlockedAllMovie;

    // ========================================================================
    // asm: GameOptionsClass_UnlockMovieIfNeeded (0x5FBF50)
    //
    //   char *__thiscall GameOptionsClass_UnlockMovieIfNeeded(
    //       GameOptionsClass *this, char *movieName)
    //
    //   在 sovMovies / allMovies 两张影片表里查 movieName，把命中下标推进
    //   到对应的"已解锁"游标（只增不减）。asm 逐段：
    //
    //     mov eax,sovMovies.MovieName / push ebx/ebp/esi/edi
    //     mov ebp,[esp+8+Str1] / xor edi,edi / mov ebx,ecx / mov esi,offset sovMovies
    //     test eax,eax / jz short loc_5FBFB5
    //   loc_5FBF9A:
    //     mov eax,[esi] / push eax / push ebp / call __strcmpi
    //     add esp,8 / test eax,eax / jz short loc_5FBFB8
    //     mov eax,[esi+0Ch] / add esi,0Ch / inc edi / test eax,eax / jnz loc_5FBF9A
    //   loc_5FBFB5: or edi,0FFFFFFFFh
    //   loc_5FBFB8:
    //     cmp edi,[ebx+4Ch] / jle short loc_5FBFC0
    //     mov [ebx+LastUnlockedSovMovie],edi
    //   loc_5FBFC0:                            ; allMovies 同一套走法 -> [+50h]
    //
    //   影片表为 0xC 字节步长、MovieName 为 0 时终止（跳表结构）。
    // ========================================================================
    void Unlock_Movie_If_Needed(const char* movieName);

    // 影片表装载（asm: 全局 sovMovies / allMovies 指针表的宿主）。
    struct MovieEntry
    {
        const char* Name;   // +0x00 (0 表示表尾)
        int32       field_4;// +0x04
        int32       field_8;// +0x08
    };
    static void Set_Sov_Movies_Table(const MovieEntry* table);
    static void Set_All_Movies_Table(const MovieEntry* table);
};
