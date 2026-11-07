#include "ScriptTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "TeamTypeClass.h"

#include <cstring>
#include <cstdio>

// ============================================================================
// ScriptTypeClass.cpp -- AI script TYPE implementation
// ============================================================================
// A ScriptTypeClass is the [ScriptTypes] definition of an ordered list of
// script actions.  Each action is a {action, argument} pair.  The layout and
// behaviour mirror the original AIScriptTypeClass (asm 0x234 bytes):
//
//   +0x98  idxScript  (ArrayIndex)
//   +0x9C  isGlobal
//   +0xA0  cntActions (ActionsCount)
//   +0xA4  actions[]  (50 x {idx, arg})
//
// Parsing is intentionally faithful to the asm and therefore does NOT range-
// check the action against any metadata table: ScriptTypeClass_LoadFromINI
// simply calls AIScriptAction_Read ("%d,%d" sscanf) for every key 0..49 whose
// value is non-empty, stopping the accumulation once 50 actions are stored.
// ============================================================================

DynamicVectorClass<ScriptTypeClass*>* ScriptTypeClass::Array = nullptr;

// ============================================================================
// Static lookup helpers
// ============================================================================

// Lookup by ID (asm +0x24).  The original has no dedicated proc for this --
// callers inline the vec_ScriptTypes walk -- so this is a thin convenience
// wrapper over the list.
ScriptTypeClass* ScriptTypeClass::Find(const char* pID) {
    if (pID == nullptr || !Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        ScriptTypeClass* pEntry = Array->GetItem(i);
        if (pEntry != nullptr && !_strcmpi(pEntry->ID, pID))
            return pEntry;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_FindByName_IDandName (0x691B20)
//   Walk vec_ScriptTypes; each entry is compared against ID (asm +0x24) then
//   Name (asm +0x64) with _strcmpi; first hit wins.
// ----------------------------------------------------------------------------
ScriptTypeClass* ScriptTypeClass::FindByName_IDandName(const char* pName) {
    if (pName == nullptr || !Array) return nullptr;

    for (int32 i = 0; i < Array->Count; ++i) {
        ScriptTypeClass* pEntry = Array->GetItem(i);
        if (pEntry == nullptr) continue;

        if (!_strcmpi(pEntry->ID,   pName)) return pEntry;
        if (!_strcmpi(pEntry->Name, pName)) return pEntry;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_FindByName_NameOnly (0x691BB0)
//   Same walk but only the Name field (asm +0x64) is compared.
// ----------------------------------------------------------------------------
ScriptTypeClass* ScriptTypeClass::FindByName_NameOnly(const char* pName) {
    if (pName == nullptr || !Array) return nullptr;

    for (int32 i = 0; i < Array->Count; ++i) {
        ScriptTypeClass* pEntry = Array->GetItem(i);
        if (pEntry != nullptr && !_strcmpi(pEntry->Name, pName))
            return pEntry;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_FindOrAllocate (0x691B30)
//   Sentinels "<none>"/"none" -> null.  Otherwise linear-search ID (asm +0x24);
//   miss -> new AIScriptTypeClass(name), push into vec_ScriptTypes, cache the
//   new index at +0x98.
// ----------------------------------------------------------------------------
ScriptTypeClass* ScriptTypeClass::FindOrAllocate(const char* pName) {
    if (pName == nullptr) return nullptr;
    if (!_strcmpi(pName, "<none>")) return nullptr;
    if (!_strcmpi(pName, "none"))   return nullptr;

    for (int32 i = 0; i < Array->Count; ++i) {
        ScriptTypeClass* pEntry = Array->GetItem(i);
        if (pEntry != nullptr && !_strcmpi(pEntry->ID, pName))
            return pEntry;
    }

    ScriptTypeClass* pNew = GameCreate<ScriptTypeClass>(pName);
    if (pNew != nullptr && Array != nullptr) {
        Array->Add(pNew);
        pNew->ArrayIndex = Array->Count - 1;
    }
    return pNew;
}

// ============================================================================
// Construction / destruction
// ============================================================================

// asm: ScriptTypeClass_CTOR -- AbstractTypeClass_CTOR(name), then idxScript
// and cntActions cleared, then index cached.  The original adds *this* to
// vec_ScriptTypes inside the CTOR; OpenYR registers via FindOrAllocate so the
// inline new is not double-added.
ScriptTypeClass::ScriptTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID),
      ArrayIndex(-1),
      IsGlobal(0),
      ActionsCount(0)
{
    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        ScriptActions[i].Action = 0;
        ScriptActions[i].Argument = 0;
    }
}

// asm: ScriptTypeClass_DTOR / ScriptTypeClass_SDDTOR
ScriptTypeClass::~ScriptTypeClass() {
}

// ============================================================================
// IPersistStream
// ============================================================================

// asm: ScriptTypeClass_GetClassID -- fills the CLSID with the game's
// CLSID_ScriptTypeClass global (16 raw bytes), returns S_OK; E_POINTER
// (0x80004003) if null.
HRESULT ScriptTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) return E_POINTER;
    pClassID->Data1 = 0x54504353;   // 'SCPT' sentinel for ScriptTypeClass
    pClassID->Data2 = 0x5059;
    pClassID->Data3 = 0x4554;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x53;
    return S_OK;
}

// asm: ScriptTypeClass_Load -- chains AbstractClass_Load (implemented by
// AbstractTypeClass in OpenYR) then re-stamps the four interface vtables.
HRESULT ScriptTypeClass::Load(IStream* pStm) {
    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr >= 0) {
        this->LoadTables(pStm);
    }
    return hr;
}

// asm: ScriptTypeClass_Save -- chains AbstractClass_Save (implemented by
// AbstractTypeClass in OpenYR).
HRESULT ScriptTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    return AbstractTypeClass::Save(pStm, fClearDirty);
}

// ============================================================================
// RTTI / identity
// ============================================================================

AbstractType ScriptTypeClass::WhatAmI() const {
    return AbstractType::ScriptType;
}

// asm: ScriptTypeClass_GetAbstractDerivationID -- mov eax,1Bh
AbstractType ScriptTypeClass::GetAbstractDerivationID() const {
    return static_cast<AbstractType>(0x1B);
}

// asm: ScriptTypeClass_GetSize -- mov eax,234h
int32 ScriptTypeClass::Size() const {
    return sizeof(ScriptTypeClass);
}

// asm: ScriptTypeClass_GetArrayIndex -- mov eax,[ecx+98h]
int32 ScriptTypeClass::GetArrayIndex() const {
    return ArrayIndex;
}

// asm: ScriptTypeClass_GetCRC -- AbstractTypeClass_GetCRC(buf), then
// CRC_DWORD(cntActions).  OpenYR hashes AbstractTypeClass state into a
// CRCEngine and then the action count, matching asm's field selection.
int32 ScriptTypeClass::GetCRC() const {
    CRCEngine crc;
    ComputeCRC(crc);
    crc.AddData(&ActionsCount, sizeof(ActionsCount));
    return static_cast<int32>(crc.GetCRC());
}

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_PointerGotInvalid (retn 8)
//   If the invalidated object is a TeamType (WhatAmI == 0x23), walk the action
//   list; for every ChangeTeam action (idx 0x12) whose argument indexes the
//   invalid team, re-resolve the live team index and store it back, or clear
//   the argument when no live team matches.
// ----------------------------------------------------------------------------
void ScriptTypeClass::PointerGotInvalid(AbstractClass* pAbstract, bool removed) {
    (void)removed;

    AbstractClass* p = pAbstract;
    if (p == nullptr) return;

    // asm: call [vtable+0x2C] (WhatAmI) / cmp eax, 23h
    if (p->WhatAmI() != AbstractType::TeamType) return;

    if (ActionsCount <= 0) return;

    int32 resolved = -1;
    for (int32 i = 0; i < ActionsCount; ++i) {
        if (ScriptActions[i].Action != 0x12)   // ChangeTeam
            continue;

        // Resolve the *live* index of the invalidated team once, lazily.
        // vec_TeamTypes is TeamTypeClass::Array; the stored argument is the
        // team's index in that global array.
        if (resolved == -1) {
            resolved = 0;
            bool found = false;
            if (TeamTypeClass::Array != nullptr) {
                for (int32 t = 0; t < TeamTypeClass::Array->Count; ++t) {
                    if (TeamTypeClass::Array->GetItem(t) == p) { found = true; break; }
                    ++resolved;
                }
            }
            if (!found) resolved = -1;
        }

        if (resolved < 0) continue;

        int32 arg = ScriptActions[i].Argument;
        if (arg > resolved) {
            ScriptActions[i].Argument = arg - 1;
        } else if (arg == resolved) {
            ScriptActions[i].Argument = 0;
        }
    }
}

// ============================================================================
// INI loading
// ============================================================================

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_LoadFromINI (0x691890, retn 4)
//   INIClass_Reset(ini); chain AbstractTypeClass_LoadFromINI;
//   ActionsCount = 0;
//   for (i = 0; i < 50; ++i):
//       sprintf(key, "%d", i); trim(key);
//       if (ini.GetString(ID, key, "", buf, 0x80) > 0):
//           AIScriptAction_Read(&scrAction, buf);
//           if (ActionsCount < 50):
//               actions[ActionsCount++] = scrAction;
//   return true.
// ----------------------------------------------------------------------------
bool ScriptTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    pINI->Reset();                                   // INIClass_Reset(a1)
    if (!AbstractTypeClass::LoadFromINI(pINI))       // AbstractTypeClass_LoadFromINI
        return false;                                // loc_691962: xor al,al

    ActionsCount = 0;

    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        char key[32];
        sprintf(key, "%d", i);
        // asm calls trim(key) -- the integer key needs no trimming, and the
        // resulting string is identical, so the call is a no-op here.

        char buf[0x80];
        buf[0] = '\0';
        if (pINI->GetString(this->ID, key, buf, sizeof(buf)) <= 0)
            continue;

        // AIScriptAction_Read: sscanf(buf, "%d,%d", &action, &argument).
        int32 action = 0;
        int32 argument = 0;
        sscanf(buf, "%d,%d", &action, &argument);

        if (ActionsCount < MAX_SCRIPT_ACTIONS_COUNT) {
            ScriptActions[ActionsCount].Action   = action;
            ScriptActions[ActionsCount].Argument = argument;
            ++ActionsCount;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_SaveIntoINI (0x6917A0, retn 4)
//   chain AbstractTypeClass_SaveIntoINI; for i in 0..49:
//       sprintf(key, "%d", i); trim(key);
//       if (i < ActionsCount):
//           sub_723CE0(&actions[i], buf);          // "%d,%d"
//           ini.WriteString(ID, key, buf);
//       else:
//           mangleStuff_(ini, ID, key);            // delete stale key
//   return true.
// ----------------------------------------------------------------------------
bool ScriptTypeClass::SaveIntoINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    if (!AbstractTypeClass::SaveToINI(pINI))
        return false;

    for (int32 i = 0; i < MAX_SCRIPT_ACTIONS_COUNT; ++i) {
        char key[32];
        sprintf(key, "%d", i);

        if (i < ActionsCount) {
            char value[32];
            sprintf(value, "%d,%d",
                    ScriptActions[i].Action,
                    ScriptActions[i].Argument);
            pINI->WriteString(this->ID, key, value);
        } else {
            // mangleStuff_: clear the stale numbered key.
            pINI->WriteString(this->ID, key, "");
        }
    }
    return true;
}

// ============================================================================
// Action stream accessors
// ============================================================================

// asm: appends a {action, argument} pair if capacity remains.
void ScriptTypeClass::AddCommand(int32 action, int32 argument) {
    if (ActionsCount < 0 || ActionsCount >= MAX_SCRIPT_ACTIONS_COUNT) return;
    ScriptActions[ActionsCount].Action   = action;
    ScriptActions[ActionsCount].Argument = argument;
    ++ActionsCount;
}

// asm: reads the pair at a valid index (asm +0xA4 + index*8).
bool ScriptTypeClass::GetCommand(int32 index, int32* pAction, int32* pArgument) const {
    if (index < 0 || index >= ActionsCount) return false;
    if (pAction)   *pAction   = ScriptActions[index].Action;
    if (pArgument) *pArgument = ScriptActions[index].Argument;
    return true;
}

bool ScriptTypeClass::HasCommand(int32 action) const {
    for (int32 i = 0; i < ActionsCount; ++i) {
        if (ScriptActions[i].Action == action) return true;
    }
    return false;
}

// Human-readable action name for the debugger/editor.  The original stores
// these strings in the executable's read-only data; OpenYR provides a small
// table covering the documented ScriptAction values and falls back to a
// numeric rendering for anything outside it.
const char* ScriptTypeClass::GetActionName(int32 action) {
    static const char* const kNames[] = {
        "Attack", "AttackWaypoint", "MoveToWaypoint", "MoveToCell",
        "GuardArea", "JumpToLine", "PlayerCheck", "Wait", "Unload",
        "Deploy", "Follow", "LoadIntoTransport", "Spy", "Patrol",
        "EnterTunnel", "ChronoWarp", "ChronoSphere", "IronCurtain",
        "Sell", "Repair", "SelfDestruct", "ChangeTeam", "ChangeScript",
        "ChangeMission", "Fear", "Retreat", "Scatter", "Stop", "Sleep",
        "Group", "Recruit", "Flash", "LoadOntoTransports", "Chronominimum",
        "ChronoMaximum", "ForceMove", "Circle", "SearchAndDestroy",
        "Harmless", "Suicide", "Recycle", "Repeat", "Protect", "Sticky",
        "Emergency", "TakeCover", "Gibber", "IronCurtainMe",
        "ChronoSphereMe", "Win", "Lose"
    };
    const int32 count = static_cast<int32>(sizeof(kNames) / sizeof(kNames[0]));
    if (action < 0 || action >= count) return "Unknown";
    return kNames[action];
}

// ----------------------------------------------------------------------------
// asm: ScriptTypeClass_CreateFromINIList
//   Walks the [ScriptTypes] section; skips "<none>"/"none" sentinels; reuses
//   an existing entry with the same ID or allocates a new one, then calls the
//   entry's LoadFromINI (vtable +0x64) and stamps isGlobal.
// ----------------------------------------------------------------------------
void ScriptTypeClass::CreateFromINIList(CCINIClass* pINI) {
    if (pINI == nullptr) return;

    static const char* const SECTION = "ScriptTypes";
    const int32 count = pINI->GetKeyCount(SECTION);

    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->GetKeyName(SECTION, i);
        if (pKeyName == nullptr) continue;

        char name[0x18];
        name[0] = '\0';
        if (pINI->GetString(SECTION, pKeyName, name, sizeof(name)) <= 0)
            continue;

        if (!_strcmpi(name, "<none>") || !_strcmpi(name, "none"))
            continue;

        ScriptTypeClass* pItem = FindOrAllocate(name);
        if (pItem == nullptr) continue;

        pItem->LoadFromINI(pINI);
        pItem->IsGlobal = 0;
    }
}
