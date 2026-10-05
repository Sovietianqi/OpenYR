#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class CCINIClass;

// ============================================================================
// TreeView / ListView INI persistence
//
 //   sub_7768F0 is the callback stored in the dialog message
//   map for the options screen.  It dispatches on the Win32 class name of the
//   control it is handed:
//
//     "SysTreeView32"  -> walks every root item with TVM_GETNEXTITEM (0x110A)
//                         and restores each one's check state from a "TV%d"
//                         boolean, driving TVM_SETITEMSTATE (0x1102) with
//                         TVIS_CHECKED (2) or the plain state (1).
//
//     "SysListView32"  -> walks items 0..9 with LVM_GETITEMTEXT (0x101D) and
//                         restores each one's numeric value from an "LV%d"
//                         integer through LVM_SETITEMTEXT (0x101E).  Values
//                         of 1000 or more are clamped away.
//
//   The functions below keep the exact key formats and ordering.  The raw
//   Win32 handles are passed through an opaque pointer so this file stays
//   independent of <windows.h>.
// ============================================================================

namespace INIViewState {

// Returns true once the control has been processed (or was not one of the two
// supported classes).  hWnd is the control handle, pSection the INI section.
bool Apply(void* hWnd, CCINIClass* pINI, const char* pSection);

// The INI section used by the options dialog when persisting view state.
extern const char* const VIEW_SECTION;

// Message / flag constants, named for readability.
enum ViewMessage : uint32 {
    TVM_GETNEXTITEM  = 0x110A,
    TVM_SETITEMSTATE = 0x1102,
    TVM_GETITEMSTATE = 0x1102,
    LVM_GETITEMTEXT  = 0x101D,
    LVM_SETITEMTEXT  = 0x101E,

    TVGN_ROOT        = 0x0000,
    TVGN_NEXT        = 0x0001,

    // Index of the "check" state image; 2 sets it, 1 clears it.
    ListView_SetCheck = 2,
    ListView_ClearCheck = 1,

    ListView_MaxValue = 1000
};

} // namespace INIViewState
