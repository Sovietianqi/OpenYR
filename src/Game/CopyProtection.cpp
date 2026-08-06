#include "Game/CopyProtection.h"
#include "Game/Game.h"
#include "Core/Definitions.h"
#include "Core/Macros.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

#ifdef PLATFORM_LINUX
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// CopyProtection.cpp - Launcher-aware copy protection layer.
//
//  The original Yuri's Revenge binary shipped with a launcher (game.exe)
//  that performed CD-ROM validation, serial-number checks, and anti-tamper
//  sweeps.  The game binary itself delegated most of the work to the
//  launcher via a named-pipe / shared-memory heartbeat protocol.
//
//  This file preserves the structure of the original control flow while
//  implementing the checks in a portable way for the standalone build.
//  Every check has a clearly documented "original binary" comment so the
//  reconstruction stays faithful even when the standalone implementation
//  is a no-op.
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// Internal state
// ═══════════════════════════════════════════════════════════════════════════

bool         g_CopyProtection_Active      = false;
unsigned int g_CopyProtection_LastCheck   = 0;

// ── Additional internal state (not exposed in the header) ──────────────────

static bool         s_CDROM_Validated       = false;
static bool         s_Serial_Verified       = false;
static bool         s_Tamper_Check_Passed   = false;
static uint32       s_Disk_Fingerprint      = 0;
static uint32       s_Serial_Number         = 0;
static int          s_Last_Error_Code       = 0;
static char         s_Last_Error_Message[256] = {0};

// ── Well-known constants from the original binary ──────────────────────────

// The original game looks for this volume label on the install CD.
static const char*  s_Expected_Volume_Label = "RA2YURI";

// The original game's launcher creates a named mutex with this string.
static const char*  s_Launcher_Mutex_Name   = "YURI_LAUNCHER_MUTEX";

// Heartbeat interval (milliseconds) - the original binary uses 1000ms.
static const unsigned int s_Heartbeat_Interval_MS = 1000;

// ═══════════════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_SetError - record the most recent failure for the UI to
// display.  The original binary uses a numeric code + a localised string
// table entry; the standalone build stores both inline.
// ----------------------------------------------------------------------------
static void CopyProtection_SetError(int code, const char* pMessage)
{
    s_Last_Error_Code = code;
    if (pMessage)
    {
        int32 i = 0;
        while (pMessage[i] && i < 255)
        {
            s_Last_Error_Message[i] = pMessage[i];
            ++i;
        }
        s_Last_Error_Message[i] = '\0';
    }
    else
    {
        s_Last_Error_Message[0] = '\0';
    }
}

// ----------------------------------------------------------------------------
// CopyProtection_Generate_Serial - derives a serial number from the
// installation's volume label + the current timestamp.  The original binary
// reads a pre-burned serial from the CD-ROM's TOC; the standalone build
// synthesises one deterministically so save games stay consistent within a
// single install.
// ----------------------------------------------------------------------------
static uint32 CopyProtection_Generate_Serial()
{
    uint32 seed = 0;

    // Hash the expected volume label.
    for (int32 i = 0; s_Expected_Volume_Label[i]; ++i)
    {
        seed = seed * 31u + static_cast<uint8>(s_Expected_Volume_Label[i]);
    }

    // Mix in the current time so two installs on the same machine differ.
    seed ^= static_cast<uint32>(std::time(nullptr));

    // LCG step to spread the bits.
    seed = seed * 1103515245u + 12345u;
    return seed & 0x7FFFFFFFu;
}

// ----------------------------------------------------------------------------
// CopyProtection_Compute_Disk_Fingerprint - in the original binary this
// reads the CD-ROM's TOC and hashes the first sessions's start LBA + the
// disc's volume descriptor.  The standalone build hashes the install
// directory's inode + mtime.
// ----------------------------------------------------------------------------
static uint32 CopyProtection_Compute_Disk_Fingerprint()
{
    uint32 fp = 0;

#ifdef PLATFORM_LINUX
    struct stat st;
    if (stat(".", &st) == 0)
    {
        fp ^= static_cast<uint32>(st.st_ino);
        fp ^= static_cast<uint32>(st.st_mtime);
    }
#else
    // No portable fingerprint source - fall back to a constant so the
    // check is deterministic.
    fp = 0xC0FFEE01u;
#endif

    fp = fp * 1103515245u + 12345u;
    return fp & 0x7FFFFFFFu;
}

// ═══════════════════════════════════════════════════════════════════════════
// CD-ROM validation
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_Validate_CDROM - verify that the install CD is present.
//
//  Original binary:
//   1. Enumerate CD-ROM drives via GetLogicalDrives / GetDriveType.
//   2. For each, open the root and read the volume label.
//   3. Compare the label against the expected value.
//   4. If a match is found, hash the disc's TOC and cache the result.
//
//  Standalone build:
//   * Always returns true.  The standalone build ships as a digital download
//     and does not require the original CD.
// ----------------------------------------------------------------------------
static bool CopyProtection_Validate_CDROM()
{
    if (s_CDROM_Validated)
        return true;

    // The original binary would walk the drive list here.  In the
    // standalone build we treat any install as valid.

    s_CDROM_Validated = true;
    s_Disk_Fingerprint = CopyProtection_Compute_Disk_Fingerprint();
    return true;
}

// ----------------------------------------------------------------------------
// CopyProtection_Get_Disk_Fingerprint - returns the cached disc fingerprint.
// ----------------------------------------------------------------------------
uint32 CopyProtection_Get_Disk_Fingerprint()
{
    if (!s_CDROM_Validated)
        CopyProtection_Validate_CDROM();
    return s_Disk_Fingerprint;
}

// ═══════════════════════════════════════════════════════════════════════════
// Serial number generation / checking
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_Verify_Serial - generate and verify the install serial.
//
//  Original binary:
//   1. Read the serial from the registry key HKLM\Software\Westwood\RA2.
//   2. Re-derive the expected serial from the CD-ROM TOC.
//   3. Compare.  Mismatch -> bail with "Please reinstall".
//
//  Standalone build:
//   * Always succeeds; the serial is generated on first call and cached.
// ----------------------------------------------------------------------------
static bool CopyProtection_Verify_Serial()
{
    if (s_Serial_Verified)
        return true;

    s_Serial_Number = CopyProtection_Generate_Serial();
    s_Serial_Verified = true;
    return true;
}

// ----------------------------------------------------------------------------
// CopyProtection_Get_Serial_Number - returns the cached serial.
// ----------------------------------------------------------------------------
uint32 CopyProtection_Get_Serial_Number()
{
    if (!s_Serial_Verified)
        CopyProtection_Verify_Serial();
    return s_Serial_Number;
}

// ═══════════════════════════════════════════════════════════════════════════
// Anti-tamper checks
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_Anti_Tamper_Check - lightweight self-integrity probe.
//
//  Original binary:
//   1. Compute a CRC of the .text section at startup.
//   2. Periodically recompute and compare.
//   3. Mismatch -> terminate with a generic "Game data error".
//
//  Standalone build:
//   * Always returns true.  A full implementation would need the build
//     system to emit a .checksum section; that is out of scope for the
//     reconstruction.
// ----------------------------------------------------------------------------
static bool CopyProtection_Anti_Tamper_Check()
{
    if (s_Tamper_Check_Passed)
        return true;

    s_Tamper_Check_Passed = true;
    return true;
}

// ----------------------------------------------------------------------------
// CopyProtection_Verify_Executable - verify the running executable's size
// matches the expected value.  The original binary hardcodes the size in
// the launcher; the standalone build does not enforce it.
// ----------------------------------------------------------------------------
bool CopyProtection_Verify_Executable()
{
    // Always succeed in the standalone build.
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Disk fingerprinting
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_Fingerprint_Install - computes a fingerprint of the install
// directory and caches it.  Returns true on success.
// ----------------------------------------------------------------------------
bool CopyProtection_Fingerprint_Install()
{
    s_Disk_Fingerprint = CopyProtection_Compute_Disk_Fingerprint();
    s_CDROM_Validated = true;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Error reporting
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_Get_Last_Error - returns the most recent error code.
// ----------------------------------------------------------------------------
int CopyProtection_Get_Last_Error()
{
    return s_Last_Error_Code;
}

// ----------------------------------------------------------------------------
// CopyProtection_Get_Last_Error_Message - copies the most recent error
// message into the caller's buffer.  Returns the message length.
// ----------------------------------------------------------------------------
int CopyProtection_Get_Last_Error_Message(char* pBuffer, int32 nBufferSize)
{
    if (!pBuffer || nBufferSize <= 0)
        return 0;

    int32 i = 0;
    while (s_Last_Error_Message[i] && i < nBufferSize - 1)
    {
        pBuffer[i] = s_Last_Error_Message[i];
        ++i;
    }
    pBuffer[i] = '\0';
    return i;
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - launcher heartbeat protocol
// ═══════════════════════════════════════════════════════════════════════════

// ----------------------------------------------------------------------------
// CopyProtection_IsLauncherRunning
//
//  In the original binary, this function checks for the presence of the
//  launcher process (game.exe) by looking for a named mutex or shared
//  memory segment.  The standalone build always reports the launcher as
//  running so the main loop does not bail out.
// ----------------------------------------------------------------------------
bool CopyProtection_IsLauncherRunning()
{
    // In the original binary:
    //   1. OpenFileMapping(Launcher_Mutex_Name) -> if it succeeds, the
    //      launcher is running.
    //   2. Fall back to FindWindow("YURI_LAUNCHER", nullptr).
    //   3. If neither works, return false and the main loop exits.

    g_CopyProtection_Active = true;
    return true;
}

// ----------------------------------------------------------------------------
// CopyProtection_NotifyLauncher
//
//  In the original game, this sends a heartbeat message to the launcher
//  to indicate that the game is still running and hasn't been tampered with.
// ----------------------------------------------------------------------------
void CopyProtection_NotifyLauncher()
{
    // In the original binary:
    //   1. Open the launcher's named pipe / shared memory.
    //   2. Write a heartbeat message: { frame_count, crc, timestamp }.
    //   3. Read back a verification response.
    //
    // Standalone: refresh the timestamp only.

    g_CopyProtection_LastCheck = Game::GetTickCount();
}

// ----------------------------------------------------------------------------
// CopyProtection_Shutdown
//
//  In the original game, this sends a shutdown notification to the launcher
//  and cleans up any shared resources.
// ----------------------------------------------------------------------------
void CopyProtection_Shutdown()
{
    // Notify the launcher that we are going away cleanly.
    if (g_CopyProtection_Active)
    {
        // In the original binary this would write a "shutting down" record
        // to the shared memory block.  The standalone build just clears
        // the local state.
    }

    g_CopyProtection_Active    = false;
    g_CopyProtection_LastCheck = 0;
    s_CDROM_Validated          = false;
    s_Serial_Verified          = false;
    s_Tamper_Check_Passed      = false;
    s_Disk_Fingerprint         = 0;
    s_Serial_Number            = 0;
    s_Last_Error_Code          = 0;
    s_Last_Error_Message[0]    = '\0';
}

// ----------------------------------------------------------------------------
// CopyProtection_Check
//
//  Periodic check called every N frames in the main loop.  The original
//  game verifies that the launcher is still running and hasn't been
//  terminated.  If the launcher is gone, the game exits.
// ----------------------------------------------------------------------------
void CopyProtection_Check()
{
    // Throttle the check to the heartbeat interval.
    unsigned int now = Game::GetTickCount();

    if (g_CopyProtection_LastCheck != 0 &&
        (now - g_CopyProtection_LastCheck) < s_Heartbeat_Interval_MS)
        return;

    g_CopyProtection_LastCheck = now;

    if (!g_CopyProtection_Active)
        return;

    // Step 1: CD-ROM validation.
    if (!CopyProtection_Validate_CDROM())
    {
        CopyProtection_SetError(1, "CD-ROM not found. Please insert the game disc.");
        return;
    }

    // Step 2: Serial number check.
    if (!CopyProtection_Verify_Serial())
    {
        CopyProtection_SetError(2, "Serial number verification failed.");
        return;
    }

    // Step 3: Anti-tamper sweep.
    if (!CopyProtection_Anti_Tamper_Check())
    {
        CopyProtection_SetError(3, "Game data integrity check failed.");
        return;
    }

    // Step 4: Heartbeat the launcher.
    CopyProtection_NotifyLauncher();
}

// ----------------------------------------------------------------------------
// CopyProtection_Initialize - run all checks once at startup.  Returns true
// if the install is considered legitimate.
// ----------------------------------------------------------------------------
bool CopyProtection_Initialize()
{
    // Reset the cached state so a re-init after a failed check works.
    s_CDROM_Validated     = false;
    s_Serial_Verified     = false;
    s_Tamper_Check_Passed = false;

    if (!CopyProtection_IsLauncherRunning())
    {
        CopyProtection_SetError(4, "Launcher not running. Please start the game via the launcher.");
        return false;
    }

    if (!CopyProtection_Validate_CDROM())
    {
        CopyProtection_SetError(1, "CD-ROM not found. Please insert the game disc.");
        return false;
    }

    if (!CopyProtection_Verify_Serial())
    {
        CopyProtection_SetError(2, "Serial number verification failed.");
        return false;
    }

    if (!CopyProtection_Anti_Tamper_Check())
    {
        CopyProtection_SetError(3, "Game data integrity check failed.");
        return false;
    }

    g_CopyProtection_Active    = true;
    g_CopyProtection_LastCheck = Game::GetTickCount();
    return true;
}

// ----------------------------------------------------------------------------
// CopyProtection_Is_Valid - convenience accessor that returns true if every
// check has passed at least once.
// ----------------------------------------------------------------------------
bool CopyProtection_Is_Valid()
{
    return s_CDROM_Validated && s_Serial_Verified && s_Tamper_Check_Passed;
}
