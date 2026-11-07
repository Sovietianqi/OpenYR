#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// IPB - Input Playback Buffer recorder (asm globals IPB_File, dword_AA0444,
// dword_AA0448, dword_AA0460, dword_AA046C, byte_AA0418 critical section).
//
//  The recorder opens a file ("wb") on demand and flips the global mode
//  flag; the playback side opens it ("rb"), resets the pending-event latch
//  and pulls the first event batch through sub_409320/sub_409030.
// ============================================================================

class IPB {
public:
    // ------------------------------------------------------------------------
    // asm: IPB_Record (0x53E4E0) - this = filename (char*)
    //
    //   eax = dword_AA0448 / test eax,eax / jz fail   ; recording enabled?
    //   eax = IPB_File / test eax,eax / jnz fail      ; already open?
    //   EnterCriticalSection(byte_AA0418)
    //   fopen(this, "wb")
    //   IPB_File = eax / test eax,eax / jz leave
    //   dword_AA0444 = 1                              ; mode = RECORD
    // leave:
    //   LeaveCriticalSection(byte_AA0418)
    //   ecx = IPB_File / setnz al                     ; return (file != 0)
    // ------------------------------------------------------------------------
    static bool Record(const char* pFilename);

    // ------------------------------------------------------------------------
    // asm: IPB_Playback (0x53E590) - this = filename (char*)
    //
    //   eax = dword_AA0448 / test / jz fail
    //   eax = IPB_File / test / jnz fail
    //   EnterCriticalSection(byte_AA0418)
    //   fopen(this, "rb") / IPB_File = eax
    //   dword_AA046C = 0                              ; pending-event latch
    //   test eax,eax / jz leave
    //   ecx = dword_AA0460 ; push 0 ; dword_AA0444 = 2 ; call sub_409320
    //   edx = IPB_File ; ecx = dword_AA0460 ; push eax ; call sub_409030
    //   cmp eax,1 / jz leave
    //   dword_AA046C = 1                              ; events buffered
    // leave:
    //   LeaveCriticalSection(byte_AA0418)
    //   ecx = IPB_File / setnz al
    // ------------------------------------------------------------------------
    static bool Playback(const char* pFilename);

    // Helpers mirroring the raw globals for the rest of the engine.
    static bool Is_Mode_Enabled();        // asm: dword_AA0448
    static void Set_Mode_Enabled(bool b); // asm: dword_AA0448
    static int32 Get_Mode();              // asm: dword_AA0444 (0/1/2)
    static void Close();                  // fclose + clear (WinMain shutdown)
};
