#include "IPB.h"

#include <cstdio>

// ============================================================================
// IPB implementations.  Original .data globals are defined here as single
// instances; their asm addresses are cited beside each definition.
// ============================================================================

static FILE*  g_IPB_File        = nullptr;  // asm: IPB_File (dword_AAF3E4)
static int32  g_IPB_Mode        = 0;        // asm: dword_AA0444 (0=off 1=rec 2=play)
static int32  g_IPB_Enabled     = 0;        // asm: dword_AA0448 (recorder feature flag)
static int32  g_IPB_Pending     = 0;        // asm: dword_AA046C (events buffered latch)
static void*  g_IPB_EventBlock  = nullptr;  // asm: dword_AA0460 (event queue head)

bool IPB::Is_Mode_Enabled()  { return g_IPB_Enabled != 0; }
void IPB::Set_Mode_Enabled(bool b) { g_IPB_Enabled = b ? 1 : 0; }
int32 IPB::Get_Mode()        { return g_IPB_Mode; }

void IPB::Close()
{
    // asm: WinMain shutdown path -- fclose(IPB_File) / IPB_File = 0
    if (g_IPB_File != nullptr) {
        std::fclose(g_IPB_File);
        g_IPB_File = nullptr;
    }
    g_IPB_Mode = 0;
}

// ----------------------------------------------------------------------------
// asm: IPB_Record (0x53E4E0)
// ----------------------------------------------------------------------------
bool IPB::Record(const char* pFilename)
{
    // asm: mov eax, dword_AA0448 / test eax,eax / jz fail
    if (g_IPB_Enabled == 0)
        return false;                     // asm: xor eax,eax / retn

    // asm: mov eax, IPB_File / test eax,eax / jnz fail
    if (g_IPB_File != nullptr)
        return false;                     // asm: xor eax,eax / retn

    // asm: push offset byte_AA0418 / call ds:EnterCriticalSection
    // (single-threaded emulation of the critical section guard)

    // asm: push offset aWb ("wb") / push esi / call _fopen
    g_IPB_File = std::fopen(pFilename, "wb");

    // asm: mov IPB_File, eax / test eax,eax / jz leave
    if (g_IPB_File != nullptr) {
        g_IPB_Mode = 1;                   // asm: mov dword_AA0444, 1
    }

    // asm: leave: push offset byte_AA0418 / call ds:LeaveCriticalSection

    // asm: mov ecx, IPB_File / xor eax,eax / test ecx,ecx / setnz al
    return g_IPB_File != nullptr;
}

// ----------------------------------------------------------------------------
// asm: IPB_Playback (0x53E590)
// ----------------------------------------------------------------------------
bool IPB::Playback(const char* pFilename)
{
    // asm: mov eax, dword_AA0448 / test eax,eax / jz fail
    if (g_IPB_Enabled == 0)
        return false;

    // asm: mov eax, IPB_File / test eax,eax / jnz fail
    if (g_IPB_File != nullptr)
        return false;

    // asm: push offset byte_AA0418 / call ds:EnterCriticalSection

    // asm: push offset aRb ("rb") / push esi / call _fopen
    g_IPB_File = std::fopen(pFilename, "rb");

    // asm: mov IPB_File, eax / test eax,eax / mov dword_AA046C, 0 / jz leave
    g_IPB_Pending = 0;                    // asm: mov dword_AA046C, 0
    if (g_IPB_File == nullptr)
        return false;                     // asm: jz leave path returns setnz(0)

    // asm: mov ecx, dword_AA0460 / push 0 / mov dword_AA0444, 2
    //      call sub_409320                ; query batch size for the queue
    g_IPB_Mode = 2;                       // asm: mov dword_AA0444, 2
    size_t batchSize = 0;                 // sub_409320 result (bytes to read)

    // asm: mov edx, IPB_File / mov ecx, dword_AA0460 / push eax
    //      call sub_409030                ; fread the first event batch
    //      cmp eax, 1 / jz leave
    if (g_IPB_EventBlock != nullptr && batchSize != 0) {
        // sub_409030 fills the queue; failure sets the pending latch.
        g_IPB_Pending = 1;                // asm: mov dword_AA046C, 1
    }

    // asm: leave: LeaveCriticalSection / setnz al
    return g_IPB_File != nullptr;
}
