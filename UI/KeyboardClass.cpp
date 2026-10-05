#include "KeyboardClass.h"
#include "../INI/INIClass.h"
#include "../IO/CCFileClass.h"

#include <cstring>

// ============================================================================
// KeyboardClass - KEYBOARDMD.INI [Hotkey]
// ============================================================================

DynamicVectorClass<KeyboardClass::HotkeyBinding>* KeyboardClass::Bindings = nullptr;
DynamicVectorClass<CommandClass*>*               KeyboardClass::Commands = nullptr;

CommandClass::~CommandClass()
{
}

KeyboardClass::KeyboardClass()
{
}

KeyboardClass::~KeyboardClass()
{
}

void KeyboardClass::RegisterCommand(CommandClass* pCommand)
{
    if (pCommand == nullptr) {
        return;
    }
    if (Commands == nullptr) {
        Commands = new DynamicVectorClass<CommandClass*>();
    }
    Commands->Add(pCommand);
}

void KeyboardClass::Clear()
{
    if (Bindings != nullptr) {
        Bindings->Clear();
    }
}

// ============================================================================
 // KeyboardClass_LoadFromINI -
//
//   "KEYBOARDMD.INI" is opened through a stack CCFileClass.  The binding
//   table is discarded and rebuilt; the [Hotkey] section is then walked key
//   by key.  Only keys that name a registered command and values that are
//   non-zero produce a binding.
// ============================================================================
bool KeyboardClass::LoadFromINI()
{
    if (Bindings == nullptr) {
        Bindings = new DynamicVectorClass<HotkeyBinding>();
    }

    CCFileClass file("KEYBOARDMD.INI");
    if (!file.Open(0)) {
        // "Unable to load KEYBOARDMD.INI\n"
        return false;
    }

    CCINIClass ini;
    const bool loaded = ini.LoadFile(&file);
    file.Close();
    if (!loaded) {
        // "Unable to load KEYBOARDMD.INI\n"
        return false;
    }

    Bindings->Clear();

    const int32 count = ini.GetKeyCount("Hotkey");
    if (count <= 0) {
        return true;
    }

    for (int32 i = 0; i < count; ++i)
    {
        const char* pKeyName = ini.GetKeyName("Hotkey", i);
        if (pKeyName == nullptr) {
            continue;
        }

        const int32 scanCode = ini.ReadInteger("Hotkey", pKeyName, 0);
        if (scanCode == 0) {
            continue;
        }

        // Find the matching command by name.
        CommandClass* pMatched = nullptr;
        if (Commands != nullptr)
        {
            for (int32 c = 0; c < Commands->Count; ++c)
            {
                CommandClass* pCommand = Commands->Items[c];
                if (pCommand == nullptr) {
                    continue;
                }
                const char* pName = pCommand->Get_Name();
                if (pName != nullptr && std::strcmp(pName, pKeyName) == 0) {
                    pMatched = pCommand;
                    break;
                }
            }
        }

        if (pMatched == nullptr) {
            continue;
        }

        HotkeyBinding binding;
        binding.ScanCode = scanCode;
        binding.pCommand = pMatched;
        Bindings->Add(binding);
    }

    return true;
}
