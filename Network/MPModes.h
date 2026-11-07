#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "MPGameModeClass.h"

// ============================================================================
// MP - multiplayer game mode initializers
//
//  根据游戏行为，可知每个多人模式由一个初始化器承载：创建模式对象、
//  写入标题与模式号后登记进模式总表。六个模式（遭遇战、自由混战、
//  合作、围城、邪恶联盟、手动遭遇战）共用同一套初始化流程，差异只
//  在模式号与标题。
// ============================================================================

class MP {
public:
    // 模式初始化器族：各自按模式号建单例并登记。
    static MPGameModeClass* Battle_Init(const wchar_t* pTitle1, const wchar_t* pTitle2,
                                        const char* pATitle1, const char* pATitle2,
                                        bool flag, int32 idxMPMode);
    static MPGameModeClass* FFA_Init(const wchar_t* pTitle1, const wchar_t* pTitle2,
                                     const char* pATitle1, const char* pATitle2,
                                     bool flag, int32 idxMPMode);
    static MPGameModeClass* Coop_Init(const wchar_t* pTitle1, const wchar_t* pTitle2,
                                      const char* pATitle1, const char* pATitle2,
                                      bool flag, int32 idxMPMode);
    static MPGameModeClass* Siege_Init(const wchar_t* pTitle1, const wchar_t* pTitle2,
                                       const char* pATitle1, const char* pATitle2,
                                       bool flag, int32 idxMPMode);
    static MPGameModeClass* Unholy_Init(const wchar_t* pTitle1, const wchar_t* pTitle2,
                                        const char* pATitle1, const char* pATitle2,
                                        bool flag, int32 idxMPMode);
    static MPGameModeClass* ManBattle_Init(const wchar_t* pTitle1, const wchar_t* pTitle2,
                                           const char* pATitle1, const char* pATitle2,
                                           bool flag, int32 idxMPMode);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知合作模式的存档项在模式旗标与活动旗标都置位时
    // 才写出。
    // ------------------------------------------------------------------------
    static bool Coop_Save(MPGameModeClass* pMode);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知玩家名表是每个条目 1KB 的宽字符缓冲，按下标
    // 寻址读出第 N 个玩家名；下标越界返回空串。
    // ------------------------------------------------------------------------
    static bool ReadNthPlayerName(wchar_t* pOut, int32 index);

    // ------------------------------------------------------------------------
    // asm: MP_ReadNthPlayerName (0x7350C0, __thiscall, ecx = 宽字符名)
    //
    //   把 ecx 指向的宽字符玩家名转写成窄字符，落进 8 槽轮转缓冲的一格，
    //   并返回该格指针。原版逐字节搬低位、缓冲上限 0x400 字节，且每次都把
    //   尾字节钉成 0（含 [eax+3FFh] 的越界哨兵）。槽位游标 idxCurPlayer
    //   在返回前加一并对 8 取模。
    //
    //   pWide 为空时返回空串。
    // ------------------------------------------------------------------------
    static const char* ReadNthPlayerName(const wchar_t* pWide);

    // 玩家名表：最多 8 名玩家、每名 1KB 缓冲（与原版布局一致）。
    static wchar_t PlayerNames[8][512];
};
