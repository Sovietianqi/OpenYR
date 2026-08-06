#include "TriggerTypeClass.h"
#include "TEventClass.h"
#include "TActionClass.h"
#include "TriggerClass.h"
#include "../Houses/HouseClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// TriggerTypeClass.cpp - Trigger type (events + actions) implementation
// ============================================================================
// A TriggerTypeClass defines the static template for a trigger: a list of
// events (TEventClass) that must be satisfied and a list of actions
// (TActionClass) to execute when the trigger fires. TriggerClass instances
// reference a TriggerTypeClass at runtime.
//
// This file implements:
//   * Static registry (Array) management and name-based lookup
//   * Construction / destruction with full member initialisation
//   * Binary stream persistence (Load / Save) of events and actions
//   * INI parsing that resolves event/action names to pointers
//   * CRC computation for multiplayer sync verification
//   * Event/action list management: add / remove / query / clear
//   * Enable/disable, repeat control, next-trigger chaining
// ============================================================================

DynamicVectorClass<TriggerTypeClass*>* TriggerTypeClass::Array = nullptr;

// ----------------------------------------------------------------------------
// Find - locate a trigger type by its (case-insensitive) ID.
// ----------------------------------------------------------------------------
TriggerTypeClass* TriggerTypeClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) {
            return item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// FindOrAllocate - return an existing trigger type or allocate a new one.
// ----------------------------------------------------------------------------
TriggerTypeClass* TriggerTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;

    TriggerTypeClass* found = Find(pID);
    if (found) return found;

    TriggerTypeClass* newItem = GameCreate<TriggerTypeClass>(pID);
    if (newItem && Array) {
        Array->Add(newItem);
    }
    return newItem;
}

// ----------------------------------------------------------------------------
// GetCount - number of registered trigger types.
// ----------------------------------------------------------------------------
int32 TriggerTypeClass::GetCount() {
    return Array ? Array->Count : 0;
}

// ============================================================================
// Construction / destruction
// ============================================================================

TriggerTypeClass::TriggerTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), FirstEvent(nullptr), FirstAction(nullptr),
      NextTrigger(nullptr), IsEnabled(true), IsRepeatable(false),
      Easy(true), Normal(true), Hard(true), TriggerFlags(0) {
}

TriggerTypeClass::~TriggerTypeClass() {
    EventList.Clear();
    ActionList.Clear();
    FirstEvent = nullptr;
    FirstAction = nullptr;
    NextTrigger = nullptr;
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TriggerTypeClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x54524754;   // 'TRGT' sentinel for TriggerTypeClass
    pClassID->Data2 = 0x5954;
    pClassID->Data3 = 0x5045;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x54;
    return S_OK;
}

// ----------------------------------------------------------------------------
// Load - read the trigger type's persistent state from a binary stream.
// ----------------------------------------------------------------------------
HRESULT TriggerTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsEnabled    = (flags & 0x01) != 0;
    IsRepeatable = (flags & 0x02) != 0;
    Easy         = (flags & 0x04) != 0;
    Normal       = (flags & 0x08) != 0;
    Hard         = (flags & 0x10) != 0;

    hr = pStm->Read(&TriggerFlags, sizeof(TriggerFlags), &read);
    if (hr < 0 || read != sizeof(TriggerFlags)) return E_FAIL;

    // Read event count and event names
    int32 eventCount = 0;
    hr = pStm->Read(&eventCount, sizeof(eventCount), &read);
    if (hr < 0 || read != sizeof(eventCount)) return E_FAIL;
    if (eventCount < 0) eventCount = 0;

    EventList.Clear();
    for (int32 i = 0; i < eventCount; ++i) {
        char eventName[0x18];
        hr = pStm->Read(eventName, sizeof(eventName), &read);
        if (hr < 0 || read != sizeof(eventName)) return E_FAIL;
        eventName[sizeof(eventName) - 1] = '\0';
        if (eventName[0]) {
            TEventClass* pEvent = TEventClass::Find(eventName);
            if (pEvent) EventList.Add(pEvent);
        }
    }

    // Read action count and action names
    int32 actionCount = 0;
    hr = pStm->Read(&actionCount, sizeof(actionCount), &read);
    if (hr < 0 || read != sizeof(actionCount)) return E_FAIL;
    if (actionCount < 0) actionCount = 0;

    ActionList.Clear();
    for (int32 i = 0; i < actionCount; ++i) {
        char actionName[0x18];
        hr = pStm->Read(actionName, sizeof(actionName), &read);
        if (hr < 0 || read != sizeof(actionName)) return E_FAIL;
        actionName[sizeof(actionName) - 1] = '\0';
        if (actionName[0]) {
            TActionClass* pAction = TActionClass::Find(actionName);
            if (pAction) ActionList.Add(pAction);
        }
    }

    // Read next trigger name
    char nextTriggerName[0x18];
    hr = pStm->Read(nextTriggerName, sizeof(nextTriggerName), &read);
    if (hr < 0 || read != sizeof(nextTriggerName)) return E_FAIL;
    nextTriggerName[sizeof(nextTriggerName) - 1] = '\0';
    NextTrigger = nextTriggerName[0] ? Find(nextTriggerName) : nullptr;

    // Update first pointers
    FirstEvent = (EventList.Count > 0) ? EventList.Items[0] : nullptr;
    FirstAction = (ActionList.Count > 0) ? ActionList.Items[0] : nullptr;

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - write the trigger type's persistent state to a binary stream.
// ----------------------------------------------------------------------------
HRESULT TriggerTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    uint32 flags = 0;
    if (IsEnabled)    flags |= 0x01;
    if (IsRepeatable) flags |= 0x02;
    if (Easy)         flags |= 0x04;
    if (Normal)       flags |= 0x08;
    if (Hard)         flags |= 0x10;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&TriggerFlags, sizeof(TriggerFlags), &written);
    if (hr < 0 || written != sizeof(TriggerFlags)) return E_FAIL;

    int32 eventCount = EventList.Count;
    hr = pStm->Write(&eventCount, sizeof(eventCount), &written);
    if (hr < 0 || written != sizeof(eventCount)) return E_FAIL;

    for (int32 i = 0; i < EventList.Count; ++i) {
        char eventName[0x18];
        std::memset(eventName, 0, sizeof(eventName));
        TEventClass* pEvent = EventList.Items[i];
        if (pEvent && pEvent->ID) {
            int32 j = 0;
            while (pEvent->ID[j] && j < static_cast<int32>(sizeof(eventName)) - 1) {
                eventName[j] = pEvent->ID[j]; ++j;
            }
        }
        hr = pStm->Write(eventName, sizeof(eventName), &written);
        if (hr < 0 || written != sizeof(eventName)) return E_FAIL;
    }

    int32 actionCount = ActionList.Count;
    hr = pStm->Write(&actionCount, sizeof(actionCount), &written);
    if (hr < 0 || written != sizeof(actionCount)) return E_FAIL;

    for (int32 i = 0; i < ActionList.Count; ++i) {
        char actionName[0x18];
        std::memset(actionName, 0, sizeof(actionName));
        TActionClass* pAction = ActionList.Items[i];
        if (pAction && pAction->ID) {
            int32 j = 0;
            while (pAction->ID[j] && j < static_cast<int32>(sizeof(actionName)) - 1) {
                actionName[j] = pAction->ID[j]; ++j;
            }
        }
        hr = pStm->Write(actionName, sizeof(actionName), &written);
        if (hr < 0 || written != sizeof(actionName)) return E_FAIL;
    }

    char nextTriggerName[0x18];
    std::memset(nextTriggerName, 0, sizeof(nextTriggerName));
    if (NextTrigger && NextTrigger->ID) {
        int32 j = 0;
        while (NextTrigger->ID[j] && j < static_cast<int32>(sizeof(nextTriggerName)) - 1) {
            nextTriggerName[j] = NextTrigger->ID[j]; ++j;
        }
    }
    hr = pStm->Write(nextTriggerName, sizeof(nextTriggerName), &written);
    if (hr < 0 || written != sizeof(nextTriggerName)) return E_FAIL;

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TriggerTypeClass::WhatAmI() const {
    return AbstractType::TriggerType;
}

int32 TriggerTypeClass::Size() const {
    return sizeof(TriggerTypeClass);
}

// ============================================================================
// INI loading
//
// The original game stores trigger types with their own INI section. The
// section contains keys for Enabled, Repeatable, difficulty flags, and
// comma-separated lists of event and action IDs.
// ============================================================================

bool TriggerTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);
    if (!pINI->SectionExists(sectionName)) return false;

    // Read display name
    char nameBuf[0x31];
    pINI->ReadString(sectionName, "Name", "", nameBuf, sizeof(nameBuf));
    if (nameBuf[0]) {
        int32 i = 0;
        while (nameBuf[i] && i < static_cast<int32>(sizeof(Name)) - 1) {
            Name[i] = nameBuf[i];
            ++i;
        }
        Name[i] = '\0';
    }

    // Read flags
    IsEnabled = pINI->ReadBool(sectionName, "Enabled", IsEnabled);
    IsRepeatable = pINI->ReadBool(sectionName, "Repeatable", IsRepeatable);
    Easy = pINI->ReadBool(sectionName, "Easy", Easy);
    Normal = pINI->ReadBool(sectionName, "Normal", Normal);
    Hard = pINI->ReadBool(sectionName, "Hard", Hard);

    // If no difficulty flags are set, default to all enabled.
    if (!Easy && !Normal && !Hard) {
        Easy = Normal = Hard = true;
    }

    // Read events as a comma-separated list of event IDs
    char eventListStr[256];
    pINI->ReadString(sectionName, "Events", "", eventListStr, sizeof(eventListStr));
    EventList.Clear();
    if (eventListStr[0]) {
        char* token = eventListStr;
        char* comma = strchr(eventListStr, ',');
        while (token) {
            if (comma) *comma = '\0';
            // Trim leading whitespace
            while (*token == ' ') ++token;
            if (*token && _strcmpi(token, "<none>") != 0) {
                TEventClass* pEvent = TEventClass::Find(token);
                if (pEvent) {
                    EventList.Add(pEvent);
                }
            }
            if (comma) {
                token = comma + 1;
                comma = strchr(token, ',');
            } else {
                token = nullptr;
            }
        }
    }

    // Read actions as a comma-separated list of action IDs
    char actionListStr[256];
    pINI->ReadString(sectionName, "Actions", "", actionListStr, sizeof(actionListStr));
    ActionList.Clear();
    if (actionListStr[0]) {
        char* token = actionListStr;
        char* comma = strchr(actionListStr, ',');
        while (token) {
            if (comma) *comma = '\0';
            while (*token == ' ') ++token;
            if (*token && _strcmpi(token, "<none>") != 0) {
                TActionClass* pAction = TActionClass::Find(token);
                if (pAction) {
                    ActionList.Add(pAction);
                }
            }
            if (comma) {
                token = comma + 1;
                comma = strchr(token, ',');
            } else {
                token = nullptr;
            }
        }
    }

    // Read next trigger reference
    char nextTriggerName[0x18];
    pINI->ReadString(sectionName, "NextTrigger", "", nextTriggerName, sizeof(nextTriggerName));
    if (nextTriggerName[0] && _strcmpi(nextTriggerName, "<none>") != 0) {
        NextTrigger = Find(nextTriggerName);
    } else {
        NextTrigger = nullptr;
    }

    // Update first pointers
    FirstEvent = (EventList.Count > 0) ? EventList.Items[0] : nullptr;
    FirstAction = (ActionList.Count > 0) ? ActionList.Items[0] : nullptr;

    return true;
}

// ============================================================================
// CRC computation
// ============================================================================

int32 TriggerTypeClass::GetCRC() const {
    CRCEngine crc;
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));

    uint32 flags = 0;
    if (IsEnabled)    flags |= 0x01;
    if (IsRepeatable) flags |= 0x02;
    if (Easy)         flags |= 0x04;
    if (Normal)       flags |= 0x08;
    if (Hard)         flags |= 0x10;
    crc.AddData(&flags, sizeof(flags));
    crc.AddData(&TriggerFlags, sizeof(TriggerFlags));

    int32 eventCount = EventList.Count;
    crc.AddData(&eventCount, sizeof(eventCount));
    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (pEvent && pEvent->ID) {
            int32 idLen = static_cast<int32>(strlen(pEvent->ID));
            crc.AddData(pEvent->ID, idLen);
        }
    }

    int32 actionCount = ActionList.Count;
    crc.AddData(&actionCount, sizeof(actionCount));
    for (int32 i = 0; i < ActionList.Count; ++i) {
        TActionClass* pAction = ActionList.Items[i];
        if (pAction && pAction->ID) {
            int32 idLen = static_cast<int32>(strlen(pAction->ID));
            crc.AddData(pAction->ID, idLen);
        }
    }

    if (NextTrigger) {
        crc.AddData(NextTrigger->ID, static_cast<int32>(sizeof(NextTrigger->ID)));
    }

    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Event / action attachment (legacy linked-list API)
// ============================================================================

void TriggerTypeClass::AttachEvent(TEventClass* pEvent) {
    Add_Event(pEvent);
}

void TriggerTypeClass::AttachAction(TActionClass* pAction) {
    Add_Action(pAction);
}

// ============================================================================
// Event / action management API
// ============================================================================

int32 TriggerTypeClass::Get_Event_Count() const {
    return EventList.Count;
}

int32 TriggerTypeClass::Get_Action_Count() const {
    return ActionList.Count;
}

TEventClass* TriggerTypeClass::Get_Event(int32 index) const {
    if (index < 0 || index >= EventList.Count) return nullptr;
    return EventList.Items[index];
}

TActionClass* TriggerTypeClass::Get_Action(int32 index) const {
    if (index < 0 || index >= ActionList.Count) return nullptr;
    return ActionList.Items[index];
}

// ----------------------------------------------------------------------------
// Add_Event - append an event to the trigger type. Duplicates are ignored.
// ----------------------------------------------------------------------------
void TriggerTypeClass::Add_Event(TEventClass* pEvent) {
    if (!pEvent) return;
    for (int32 i = 0; i < EventList.Count; ++i) {
        if (EventList.Items[i] == pEvent) return;
    }
    EventList.Add(pEvent);
    FirstEvent = EventList.Items[0];
}

// ----------------------------------------------------------------------------
// Add_Action - append an action to the trigger type. Duplicates are ignored.
// ----------------------------------------------------------------------------
void TriggerTypeClass::Add_Action(TActionClass* pAction) {
    if (!pAction) return;
    for (int32 i = 0; i < ActionList.Count; ++i) {
        if (ActionList.Items[i] == pAction) return;
    }
    ActionList.Add(pAction);
    FirstAction = ActionList.Items[0];
}

void TriggerTypeClass::Remove_Event(int32 index) {
    if (index < 0 || index >= EventList.Count) return;
    EventList.Remove(index);
    FirstEvent = (EventList.Count > 0) ? EventList.Items[0] : nullptr;
}

void TriggerTypeClass::Remove_Action(int32 index) {
    if (index < 0 || index >= ActionList.Count) return;
    ActionList.Remove(index);
    FirstAction = (ActionList.Count > 0) ? ActionList.Items[0] : nullptr;
}

void TriggerTypeClass::Clear_Events() {
    EventList.Clear();
    FirstEvent = nullptr;
}

void TriggerTypeClass::Clear_Actions() {
    ActionList.Clear();
    FirstAction = nullptr;
}

// ============================================================================
// Enable / repeat / chaining
// ============================================================================

bool TriggerTypeClass::Is_Enabled() const {
    return IsEnabled;
}

void TriggerTypeClass::Set_Enabled(bool enabled) {
    IsEnabled = enabled;
}

TriggerTypeClass* TriggerTypeClass::Get_Next_Trigger() const {
    return NextTrigger;
}

void TriggerTypeClass::Set_Next_Trigger(TriggerTypeClass* pTrigger) {
    NextTrigger = pTrigger;
}

bool TriggerTypeClass::Is_Repeating() const {
    return IsRepeatable;
}

void TriggerTypeClass::Set_Repeating(bool repeat) {
    IsRepeatable = repeat;
}

// ----------------------------------------------------------------------------
// Is_Allowed_Difficulty - check if this trigger type is active for the given
// difficulty level (0=Easy, 1=Normal, 2=Hard).
// ----------------------------------------------------------------------------
bool TriggerTypeClass::Is_Allowed_Difficulty(int32 difficulty) const {
    switch (difficulty) {
        case 0:  return Easy;
        case 1:  return Normal;
        case 2:  return Hard;
        default: return false;
    }
}
