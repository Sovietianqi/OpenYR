#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ============================================================================
// AudioEngineExtras - original-layout audio engine helpers
//
//  The original binary routes every audio stop/volume/link operation through
//  a small set of tightly-coupled structs whose offsets are hard-coded all
//  over the sound thread.  These reconstructions keep those original offsets
//  so the surrounding logic can be compared 1:1 against the asm.
// ============================================================================

// ----------------------------------------------------------------------------
// AudioMiniBuffer - singly linked ring node (asm AudioMiniBuffer_CountLength,
// 0x4073A0).  The list is circular: each node's Next eventually wraps back to
// the anchor object itself (this), which terminates the walk.
//   +0x00  AudioMiniBuffer* Next
// ----------------------------------------------------------------------------
class AudioMiniBuffer {
public:
    AudioMiniBuffer() noexcept : Next(nullptr) {}

    // asm: AudioMiniBuffer_CountLength (0x4073A0)
    //   edx = [ecx]              ; first node
    //   xor eax, eax             ; count = 0
    //   cmp edx, ecx / jz done   ; empty ring -> 0
    // loop:
    //   mov edx, [edx]           ; advance
    //   inc eax                  ; count++
    //   cmp edx, ecx / jnz loop  ; until we wrap to the anchor
    int32 Count_Length() const
    {
        const AudioMiniBuffer* pAnchor = this;
        const AudioMiniBuffer* pCur = Next;         // asm: mov edx,[ecx]
        int32 count = 0;                            // asm: xor eax,eax
        if (pCur == pAnchor)                        // asm: cmp edx,ecx ; jz
            return 0;

        do {
            pCur = pCur->Next;                      // asm: mov edx,[edx]
            ++count;                                // asm: inc eax
        } while (pCur != pAnchor);                  // asm: cmp edx,ecx ; jnz

        return count;                               // asm: locret (eax)
    }

    AudioMiniBuffer* Next;   // asm +0x00
};

// ----------------------------------------------------------------------------
// AudioMiniBufferList - intrusive FIFO over AudioMiniBuffer-derived nodes.
// asm AudioMiniBufferList_FindNextOrEvenNext (0x407510):
//   eax = [ecx]              ; head
//   test eax, eax / jz ret   ; empty -> leave head null
//   edx = [eax]              ; head->Next
//   add eax, 8               ; return value = head+8 (skip vtable+Next)
//   mov [ecx], edx           ; pop head
// The "+8" reveals the payload lives at +8 of each queued node; the list
// stores the node pointer but consumers address the payload directly.
// ----------------------------------------------------------------------------
class AudioMiniBufferList {
public:
    AudioMiniBufferList() noexcept : Head(nullptr) {}

    // asm: AudioMiniBufferList_FindNextOrEvenNext (0x407510)
    //   Pop the front node and return its payload (node + 8).
    //   Returns nullptr when the list is empty.
    void* Find_Next_Or_Even_Next()
    {
        AudioMiniBuffer* pHead = Head;              // asm: mov eax,[ecx]
        if (pHead == nullptr)                       // asm: test eax,eax ; jz
            return nullptr;                         // asm: retn (eax = 0)

        Head = pHead->Next;                         // asm: mov edx,[eax] / mov [ecx],edx
        return reinterpret_cast<uint8*>(pHead) + 8; // asm: add eax,8
    }

    AudioMiniBuffer* Head;   // asm +0x00
};

// ----------------------------------------------------------------------------
// AudioStreamCtl - the theme/music stream control block referenced through
// AudioStream+14h.  Original offsets:
//   +0x94  AudioStreamCtl*  LinkA   (asm AudioStream_LinkWith94, 0x4A9250)
//   +0x9C  AudioStreamCtl*  LinkB   (asm AudioStream_LinkWith9C, 0x4A9270)
// Both thunks store `value` into the slot reached via [this+14h]; a null
// control pointer crashes in the original, so the guards here are faithful
// no-ops that simply mirror the memory write when the pointer is valid.
// ----------------------------------------------------------------------------
class AudioStreamCtl {
public:
    void* Field_000;
    void* Field_094;   // asm +0x94
    void* Field_09C;   // asm +0x9C
};

class AudioStreamLink {
public:
    AudioStreamLink() noexcept : Ctl(nullptr) {}

    // asm: AudioStream_LinkWith94 (0x4A9250)
    //   mov eax, [ecx+14h] / mov [eax+94h], edx / retn
    void Link_With_94(void* pValue)
    {
        if (Ctl != nullptr)
            Ctl->Field_094 = pValue;
    }

    // asm: AudioStream_LinkWith9C (0x4A9270)
    //   mov eax, [ecx+14h] / mov [eax+9Ch], edx / retn
    void Link_With_9C(void* pValue)
    {
        if (Ctl != nullptr)
            Ctl->Field_09C = pValue;
    }

    AudioStreamCtl* Ctl;   // asm +0x14
};

// ----------------------------------------------------------------------------
// SoundStream - the volume mixer stream.  asm SoundStream_SetVolumeWithSample
// (0x5FA4F0):
//   fld  [volume] / fcomp dbl1_0        ; volume > 1.0 ?
//   ja  keep / else load 1.0            ; clamp to 1.0
//   fst  [this+38h]                     ; store clamped volume
//   fmul num_16384                      ; * 16384.0
//   call Float_To_Int_Floor             ; -> integer mixer units
//   ecx = SoundStream / edx = units
//   call VocSubClass_SetVolume
//   if (doSample) VocClass_Play(RulesData.VocTest, 1.0f, 0x2000)
//   WWDebugString("SoundVolume = %f\n", [this+38h])
// ----------------------------------------------------------------------------
class SoundStream {
public:
    SoundStream() noexcept : Volume(0.0f) {}

    // asm: SoundStream_SetVolumeWithSample (0x5FA4F0)
    void Set_Volume_With_Sample(float volume, bool doSample);

    float Volume;   // asm +0x38
};

// ----------------------------------------------------------------------------
// Stop_Sample (asm 0x407F60) - stops a playing sample guarded by two fields:
//   [this+88h]  must be 0 (not already stopped)
//   [this+14h]  must be non-null (has a Voc engine handle)
// The original enters CriticalSection before invoking sub_4025B0 on the
// handle and leaves it afterwards.
// ----------------------------------------------------------------------------
class StopSampleTarget {
public:
    StopSampleTarget() noexcept : Field_088(0), VocHandle(nullptr) {}

    // asm: Stop_Sample (0x407F60)
    void Stop_Sample();

    void* VocHandle;   // asm +0x14 (engine voice handle)
    int32 Field_088;   // asm +0x88 (stopped / detached flag)
};

// ----------------------------------------------------------------------------
// Globals surfaced by the audio group.
//   g_SoundStream          (asm global "SoundStream")
//   g_CriticalSection_402  (asm "CriticalSection" next to Stop_Sample)
//   g_RulesVocTest         (asm RulesClass+710h -- "VocTest" INI entry handle)
// ----------------------------------------------------------------------------
SoundStream*      Get_SoundStream_Singleton();
void              Audio_CriticalSection_Enter();
void              Audio_CriticalSection_Leave();
class VocClass*   Get_Rules_VocTest();
