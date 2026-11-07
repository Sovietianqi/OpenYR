#include "AudioEngineExtras.h"
#include "VocClass.h"
#include "../Helpers/AsmMath.h"
#include "../Game/EngineGlobals.h"

#include <cstdio>

// ============================================================================
// Original-layout audio engine helpers -- implementations.
// Every routine cites the asm label it mirrors.  Globals that the original
// binary kept in .data are defined here as single definitions so all call
// sites agree on one instance.
// ============================================================================

// ----------------------------------------------------------------------------
// asm globals:
//   SoundStream       -- the mixer stream singleton (referenced by
//                        SoundStream_SetVolumeWithSample through ecx)
//   CriticalSection   -- guard around Stop_Sample's engine call
//   RulesClass+710h   -- "VocTest" sample handle loaded from RULES.INI
// ----------------------------------------------------------------------------
static SoundStream      g_SoundStream_Singleton;
static int32            g_AudioCriticalSectionDepth = 0;
static VocClass*        g_RulesVocTest = nullptr;

SoundStream* Get_SoundStream_Singleton()
{
    return &g_SoundStream_Singleton;
}

// ----------------------------------------------------------------------------
// The original uses win32 CRITICAL_SECTION primitives
// (EnterCriticalSection/LeaveCriticalSection).  On this build the section is
// emulated with a depth counter; the observable ordering guarantees of the
// single-threaded sound path are preserved.
// ----------------------------------------------------------------------------
void Audio_CriticalSection_Enter()
{
    ++g_AudioCriticalSectionDepth;   // asm: call ds:EnterCriticalSection
}

void Audio_CriticalSection_Leave()
{
    if (g_AudioCriticalSectionDepth > 0)
        --g_AudioCriticalSectionDepth;   // asm: call ds:LeaveCriticalSection
}

VocClass* Get_Rules_VocTest()
{
    return g_RulesVocTest;   // asm: mov ecx, [RulesData+710h]
}

void Set_Rules_VocTest(VocClass* pVoc)
{
    g_RulesVocTest = pVoc;
}

// ============================================================================
// SoundStream_SetVolumeWithSample  (asm: SoundStream_SetVolumeWithSample,
// 0x5FA4F0)
//
//   fld     [esp+volume]
//   fcomp   ds:dbl1_0                    ; compare against 1.0
//   fnstsw  ax / test ah,1 / jz use_1_0  ; volume > 1.0 -> keep
//   fld     [esp+volume]                 ; keep original
//   jmp     store
// use_1_0:
//   fld     ds:dbl1_0                    ; clamp to 1.0
// store:
//   fst     [esi+38h]                    ; Volume = clamped
//   fmul    ds:num_16384                 ; * 16384.0
//   call    Float_To_Int_Floor           ; trunc toward floor
//   mov     ecx, SoundStream / mov edx, eax
//   call    VocSubClass_SetVolume        ; engine volume in 1/16384 units
//   mov     al, [esp+doSample] / test al,al / jz done
//   mov     eax, RulesData
//   push    0 / push 3F800000h (1.0f) / mov edx, 2000h
//   mov     ecx, [eax+710h]              ; VocTest sample
//   call    VocClass_Play
// done:
//   fld     [esi+38h] / WWDebugString("SoundVolume = %f\n", ...)
// ============================================================================
void SoundStream::Set_Volume_With_Sample(float volume, bool doSample)
{
    // asm: fld volume / fcomp dbl1_0 / fnstsw ax / test ah,1
    //      jz -> load 1.0 else load volume  (i.e. clamp to at most 1.0)
    const float clamped = (volume > 1.0f) ? 1.0f : volume;

    Volume = clamped;                                        // asm: fst [esi+38h]

    // asm: fmul ds:num_16384 / call Float_To_Int_Floor
    const double scaled = static_cast<double>(clamped) * 16384.0;
    const int32 mixerUnits = static_cast<int32>(Float_To_Int_Floor(scaled));

    // asm: mov ecx, SoundStream ; mov edx, eax ; call VocSubClass_SetVolume
    // VocSubClass_SetVolume(channel, units) -- the engine mixer singleton.
    VocManagerClass::GetInstance()->SetVolume(0, mixerUnits);

    // asm: mov al,[esp+doSample] ; test al,al ; jz done
    if (doSample) {
        // asm: mov eax, RulesData / mov ecx, [eax+710h]
        //      push 0 ; push 1.0f ; mov edx, 2000h ; call VocClass_Play
        if (g_RulesVocTest != nullptr) {
            g_RulesVocTest->SetVolume(mixerUnits);
            g_RulesVocTest->Play();
        }
    }

    // asm: WWDebugString("SoundVolume = %f\n", [esi+38h])
    std::printf("SoundVolume = %f\n", static_cast<double>(Volume));
}

// ============================================================================
// Stop_Sample  (asm: Stop_Sample, 0x407F60)
//
//   push esi / mov esi, ecx
//   mov eax, [esi+88h] / test eax,eax / jnz done   ; already stopped?
//   mov eax, [esi+14h] / test eax,eax / jz done    ; no engine handle?
//   push offset CriticalSection / call EnterCriticalSection
//   mov ecx, [esi+14h] / call sub_4025B0           ; engine voice kill
//   push offset CriticalSection / call LeaveCriticalSection
// done:
//   pop esi / retn
// ============================================================================
void StopSampleTarget::Stop_Sample()
{
    if (Field_088 != 0)                 // asm: mov eax,[esi+88h] ; jnz done
        return;

    if (VocHandle == nullptr)           // asm: mov eax,[esi+14h] ; jz done
        return;

    Audio_CriticalSection_Enter();      // asm: call ds:EnterCriticalSection

    // asm: mov ecx,[esi+14h] ; call sub_4025B0
    // sub_4025B0 releases the engine voice behind the handle; the engine
    // side is abstracted here through the shared VocClass stop hook.
    VocManagerClass::GetInstance()->Stop(0);

    Audio_CriticalSection_Leave();      // asm: call ds:LeaveCriticalSection
}
