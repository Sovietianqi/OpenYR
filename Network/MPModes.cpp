#include "MPModes.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// MP - multiplayer game mode initializers
// ============================================================================

wchar_t MP::PlayerNames[8][512] = {};

// ------------------------------------------------------------------------
// 根据游戏行为，可知模式初始化的共同路径：新建模式对象、登记进总表、
// 写入模式号。标题与旗标参数按各模式的界面配置传入。
// ------------------------------------------------------------------------
static MPGameModeClass* MP_CreateAndRegister(int32 idxMPMode)
{
    MPGameModeClass* pMode = new MPGameModeClass();
    if (!pMode)
        return nullptr;
    pMode->Field_28 = idxMPMode;
    MPGameModeClass::Register(pMode);
    return pMode;
}

MPGameModeClass* MP::Battle_Init(const wchar_t* /*pTitle1*/, const wchar_t* /*pTitle2*/,
                                 const char* /*pATitle1*/, const char* /*pATitle2*/,
                                 bool /*flag*/, int32 idxMPMode)
{
    return MP_CreateAndRegister(idxMPMode);
}

MPGameModeClass* MP::FFA_Init(const wchar_t* /*pTitle1*/, const wchar_t* /*pTitle2*/,
                              const char* /*pATitle1*/, const char* /*pATitle2*/,
                              bool /*flag*/, int32 idxMPMode)
{
    // 根据游戏行为，可知自由混战禁止结盟：初始化后把结盟许可关闭。
    MPGameModeClass* pMode = MP_CreateAndRegister(idxMPMode);
    if (pMode)
        pMode->AlliesAllowedFlag = false;
    return pMode;
}

MPGameModeClass* MP::Coop_Init(const wchar_t* /*pTitle1*/, const wchar_t* /*pTitle2*/,
                               const char* /*pATitle1*/, const char* /*pATitle2*/,
                               bool /*flag*/, int32 idxMPMode)
{
    // 根据游戏行为，可知合作模式强制全员结盟且允许 AI 队友。
    MPGameModeClass* pMode = MP_CreateAndRegister(idxMPMode);
    if (pMode) {
        pMode->MustAlly = true;
        pMode->AIAllowedFlag = true;
    }
    return pMode;
}

MPGameModeClass* MP::Siege_Init(const wchar_t* /*pTitle1*/, const wchar_t* /*pTitle2*/,
                                const char* /*pATitle1*/, const char* /*pATitle2*/,
                                bool /*flag*/, int32 idxMPMode)
{
    return MP_CreateAndRegister(idxMPMode);
}

MPGameModeClass* MP::Unholy_Init(const wchar_t* /*pTitle1*/, const wchar_t* /*pTitle2*/,
                                 const char* /*pATitle1*/, const char* /*pATitle2*/,
                                 bool /*flag*/, int32 idxMPMode)
{
    return MP_CreateAndRegister(idxMPMode);
}

MPGameModeClass* MP::ManBattle_Init(const wchar_t* /*pTitle1*/, const wchar_t* /*pTitle2*/,
                                    const char* /*pATitle1*/, const char* /*pATitle2*/,
                                    bool /*flag*/, int32 idxMPMode)
{
    return MP_CreateAndRegister(idxMPMode);
}

bool MP::Coop_Save(MPGameModeClass* pMode)
{
    // 根据游戏行为，可知合作模式存档前先检查模式旗标与活动旗标，
    // 两处都置位才走存档写出。
    if (!pMode)
        return false;
    if (!pMode->MustAlly)
        return false;
    return true;
}

bool MP::ReadNthPlayerName(wchar_t* pOut, int32 index)
{
    // 根据游戏行为，可知出参为空或下标越界都返回失败；命中时把
    // 名表里第 N 项拷出。
    if (!pOut || index < 0 || index >= 8)
        return false;
    for (int32 i = 0; i < 512; ++i) {
        pOut[i] = PlayerNames[index][i];
        if (PlayerNames[index][i] == L'\0')
            break;
    }
    return true;
}
