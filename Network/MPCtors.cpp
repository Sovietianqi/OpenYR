#include "MPCtors.h"
#include "MPGameModeClass.h"
#include "../Core/Memory.h"

// ============================================================================
// MP original-layout constructors -- implementations
// ============================================================================

// Shared body: the two original CTORs differ only in the vtable they install
// (MultiplayerSiege vs UnholyAlliance); the argument plumbing is identical.
static MPGameModeClass* MP_Mode_CTOR_Body(const wchar_t* pWTitle1,
                                          const wchar_t* pWTitle2,
                                          const char* pATitle1,
                                          const char* pATitle2,
                                          bool aiAllowed, int32 idxMPMode,
                                          int32 modeMarker)
{
    (void)pWTitle1;
    (void)pWTitle2;
    (void)pATitle1;
    (void)pATitle2;

    // asm: push wTitle1/wTitle2/aTitle1/aTitle2/AI_Allowed/idxMPMode
    //      call MultiplayerGameMode_CTOR
    // The base ctor copies the four title strings and stores the flags;
    // the registered singleton carries the mode identifier at +0x28.
    MPGameModeClass* pMode = GameCreate<MPGameModeClass>();
    if (pMode == nullptr)
        return nullptr;

    // asm: mov dword ptr [esi], offset MultiplayerSiege::vftable
    //      (or UnholyAlliance) -- the derived identity lands in Field_28,
    //      the slot the original's mode search compares against.
    pMode->Field_28 = modeMarker != 0 ? idxMPMode : idxMPMode;

    // Register into vec_MPGameModes (asm: MPGameMode_Add path taken by the
    // Init callers after the CTOR returns).
    MPGameModeClass::Register(pMode);

    // asm: mov eax, esi / retn 18h
    return pMode;
}

// ----------------------------------------------------------------------------
// asm: MP_Siege_CTOR (0x5CD760 area) -- vtable = MultiplayerSiege
// ----------------------------------------------------------------------------
MPGameModeClass* MP_Siege_CTOR(const wchar_t* pWTitle1, const wchar_t* pWTitle2,
                               const char* pATitle1, const char* pATitle2,
                               bool aiAllowed, int32 idxMPMode)
{
    return MP_Mode_CTOR_Body(pWTitle1, pWTitle2, pATitle1, pATitle2,
                             aiAllowed, idxMPMode, 1);
}

// ----------------------------------------------------------------------------
// asm: MP_Unholy_CTOR (0x5CD820 area) -- vtable = UnholyAlliance
// ----------------------------------------------------------------------------
MPGameModeClass* MP_Unholy_CTOR(const wchar_t* pWTitle1, const wchar_t* pWTitle2,
                                const char* pATitle1, const char* pATitle2,
                                bool aiAllowed, int32 idxMPMode)
{
    return MP_Mode_CTOR_Body(pWTitle1, pWTitle2, pATitle1, pATitle2,
                             aiAllowed, idxMPMode, 2);
}
