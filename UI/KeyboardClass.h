#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;

// ============================================================================
// KeyboardClass - the keyboard command binding table.
//
 //   KeyboardClass_LoadFromINI opens "KEYBOARDMD.INI" and walks
//   the [Hotkey] section.  Each key names a command and each value is the
//   scan-code the command is bound to; the entry is only kept when the key
//   matches a command already present in vec_Commands and the value is not
//   zero.  The pairs are stored eight bytes apart:
//
//     KeyboardCommand.VTable[i * 8 + 0]  int32  scan code
//     KeyboardCommand.VTable[i * 8 + 4]  void*  command object
//
//   The storage starts at 10 entries and grows through sub_538B80.
//
//   A load failure reports "Unable to load KEYBOARDMD.INI\n" and returns
//   false.
// ============================================================================

// One command in the keyboard registry.  The original reaches the command
// name through vtable slot +4 (Get_Name).
class CommandClass
{
public:
    virtual ~CommandClass();
    virtual const char* Get_Name() const = 0;
};

class KeyboardClass
{
public:
    KeyboardClass();
    ~KeyboardClass();

 // 根据游戏行为，可知 LoadFromINI 负责下面这段逻辑。
    static bool LoadFromINI();

    // Clear the binding table.
    static void Clear();

    // Register a command object so [Hotkey] can bind to it.
    static void RegisterCommand(CommandClass* pCommand);

    // ── Binding table ────────────────────────────────────────────────────
    struct HotkeyBinding {
        int32           ScanCode;
        CommandClass*   pCommand;
    };

    static DynamicVectorClass<HotkeyBinding>* Bindings;

    // Registry of known commands, mirrored from vec_Commands.
    static DynamicVectorClass<CommandClass*>* Commands;
};
