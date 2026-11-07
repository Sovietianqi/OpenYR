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

// ============================================================================
// MP::ReadNthPlayerName (narrow) -- asm MP_ReadNthPlayerName (0x7350C0)
//
//   8 个 1KB 的窄字符槽轮转复用，返回值指向本次写入的那一格。调用方只在
//   打印之前用一次，所以在下一轮调用覆盖它之前一直是有效的。
// ============================================================================
static char   s_NarrowPlayerNames[8][0x400];
static uint32 s_NarrowPlayerNameSlot = 0;   // asm: idxCurPlayer

const char* MP::ReadNthPlayerName(const wchar_t* pWide)
{
    // asm: test ecx,ecx / jnz loc_7350C7 / xor eax,eax / retn
    if (pWide == nullptr) {
        return "";
    }

    char* dest = s_NarrowPlayerNames[s_NarrowPlayerNameSlot];

    // asm: mov edi,400h / esi = eax ; 逐 2 字节取低位、最多 0x400 次
    int32 remaining = 0x400;
    const wchar_t* src = pWide;
    if (*src != L'\0') {
        do {
            if (remaining == 0) {
                break;   // asm: test edi,edi / jz loc_7350FB
            }
            // asm: mov dl,[ecx] / add ecx,2 / mov [esi],dl / inc esi
            *dest++ = static_cast<char>(*src & 0xFF);
            ++src;
            // asm: cmp edi,0FFFFFFFFh / jz +1 / dec edi
            if (remaining != -1) {
                --remaining;
            }
        } while (*src != L'\0');
    }

    // asm: mov byte ptr [esi],0 / mov byte ptr [eax+3FFh],0
    char* result  = s_NarrowPlayerNames[s_NarrowPlayerNameSlot];
    dest[0]       = '\0';
    result[0x3FF] = '\0';

    // asm: mov ecx,idxCurPlayer / inc ecx / and ecx,7 / mov idxCurPlayer,ecx
    s_NarrowPlayerNameSlot = (s_NarrowPlayerNameSlot + 1) & 7;

    return result;
}
