#include "INIViewState.h"
#include "../INI/INIClass.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>

// ============================================================================
// INIViewState - the SysTreeView32 / SysListView32 persistence callback
// (sub_7768F0, asm 0x7768F0)
//
//   These are the Win32 entry points the original reaches through the .idata
//   import table.  Declaring them here keeps the translation unit free of
//   <windows.h> while still calling the real API on Windows builds.
// ============================================================================

#if defined(_WIN32)
#include <windows.h>
#else
// Non-Windows builds: the control dispatch is inert, but the INI key formats
// below are still exercised so the reader stays compiled and testable.
typedef void* HWND;
typedef unsigned int UINT;
typedef long long LPARAM;
typedef unsigned long long WPARAM;
static int GetClassNameA(HWND, char*, int) { return 0; }
static LPARAM SendMessageA(HWND, UINT, WPARAM, LPARAM) { return 0; }
static int wsprintfA(char* pBuffer, const char* pFormat, ...) {
    va_list args;
    va_start(args, pFormat);
    const int n = std::vsprintf(pBuffer, pFormat, args);
    va_end(args);
    return n;
}
#endif

namespace INIViewState {

const char* const VIEW_SECTION = "MultiPlayer";

// ============================================================================
// Apply
// ============================================================================
bool Apply(void* hWnd, CCINIClass* pINI, const char* pSection)
{
    if (hWnd == nullptr || pINI == nullptr) {
        return false;
    }

    const char* pSectionName = (pSection != nullptr) ? pSection : VIEW_SECTION;

    char className[0x40];
    className[0] = '\0';
    GetClassNameA(reinterpret_cast<HWND>(hWnd), className, sizeof(className));

    // ── SysTreeView32 ───────────────────────────────────────────────────
    if (std::strcmp(className, "SysTreeView32") == 0)
    {
        // The first root item; the loop then walks siblings.
        LPARAM hItem = SendMessageA(reinterpret_cast<HWND>(hWnd),
                                    TVM_GETNEXTITEM, TVGN_ROOT, 0);
        if (hItem == 0) {
            return true;
        }

        int32 index = 0;
        while (hItem != 0)
        {
            ++index;

            char key[0x40];
            className[0] = '\0';
            std::sprintf(key, "TV%d", index);

            const bool checked = pINI->ReadBool(pSectionName, key, false);

            SendMessageA(reinterpret_cast<HWND>(hWnd), TVM_SETITEMSTATE, hItem,
                         checked ? ListView_SetCheck : ListView_ClearCheck);

            hItem = SendMessageA(reinterpret_cast<HWND>(hWnd),
                                 TVM_GETNEXTITEM, TVGN_NEXT, hItem);
        }

        return true;
    }

    // ── SysListView32 ───────────────────────────────────────────────────
    if (std::strcmp(className, "SysListView32") == 0)
    {
        if (hWnd == nullptr) {
            return true;
        }

        for (int32 index = 0; index < 10; ++index)
        {
            char key[0x40];
            std::sprintf(key, "LV%d", index);

            const LPARAM hItem =
                SendMessageA(reinterpret_cast<HWND>(hWnd), LVM_GETITEMTEXT,
                             index, 0);

            const int32 value = pINI->ReadInteger(pSectionName, key,
                                                  static_cast<int32>(hItem));

            // Values at or above 1000 are dropped, exactly as the original
            // compares against 0x3E8 before forwarding to LVM_SETITEMTEXT.
            if (static_cast<uint32>(value) >= ListView_MaxValue) {
                continue;
            }

            SendMessageA(reinterpret_cast<HWND>(hWnd), LVM_SETITEMTEXT, index,
                         value & 0xFFFF);
        }

        return true;
    }

    return false;
}

} // namespace INIViewState
