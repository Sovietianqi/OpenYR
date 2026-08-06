#pragma once

#include "Core/Definitions.h"

// ============================================================================
// CopyProtection - launcher-aware copy protection layer
//
//  In the original Yuri's Revenge binary, these functions communicate with
//  the official game launcher (game.exe) to verify that the user is running
//  a legitimate copy of the game.  The launcher performed CD-ROM validation,
//  serial-number checks, and anti-tamper sweeps via a named-pipe / shared
//  memory heartbeat protocol.
//
//  In the standalone (reconstructed) build, every check is a harmless no-op
//  that always reports success.  The structure is preserved so that the
//  overall control flow in WinMain.cpp matches the original binary as
//  closely as possible.
//
//  All functions and state are C-linkage style free functions/globals so
//  they can be called from anywhere without a class instance.
// ============================================================================

// ── Public API ────────────────────────────────────────────────────────────

// Returns true if the launcher process is considered running.
// Original binary: checks for a named mutex / shared memory segment.
bool CopyProtection_IsLauncherRunning();

// Sends a heartbeat message to the launcher.
// Original binary: writes {frame_count, crc, timestamp} to shared memory.
void CopyProtection_NotifyLauncher();

// Cleanly shuts down the copy-protection layer and releases resources.
void CopyProtection_Shutdown();

// Periodic check called every N frames in the main loop.
// Verifies CD-ROM, serial, and anti-tamper in sequence.
void CopyProtection_Check();

// Runs all checks once at startup.  Returns true if the install is
// considered legitimate.
bool CopyProtection_Initialize();

// Convenience accessor: true if every check has passed at least once.
bool CopyProtection_Is_Valid();

// ── CD-ROM / disk fingerprinting ──────────────────────────────────────────

// Returns the cached disc fingerprint (computes it on first call).
uint32 CopyProtection_Get_Disk_Fingerprint();

// Computes a fingerprint of the install directory and caches it.
bool CopyProtection_Fingerprint_Install();

// ── Serial number ─────────────────────────────────────────────────────────

// Returns the cached install serial number (generates it on first call).
uint32 CopyProtection_Get_Serial_Number();

// ── Executable verification ───────────────────────────────────────────────

// Verifies the running executable's integrity.  Always succeeds in the
// standalone build.
bool CopyProtection_Verify_Executable();

// ── Error reporting ───────────────────────────────────────────────────────

// Returns the most recent error code (0 = no error).
int CopyProtection_Get_Last_Error();

// Copies the most recent error message into the caller's buffer.
// Returns the message length (excluding terminator).
int CopyProtection_Get_Last_Error_Message(char* pBuffer, int32 nBufferSize);

// ── Internal state (visible for the main loop) ────────────────────────────

extern bool         g_CopyProtection_Active;
extern unsigned int g_CopyProtection_LastCheck;
