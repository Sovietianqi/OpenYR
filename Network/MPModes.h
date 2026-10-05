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

    // 玩家名表：最多 8 名玩家、每名 1KB 缓冲（与原版布局一致）。
    static wchar_t PlayerNames[8][512];
};
