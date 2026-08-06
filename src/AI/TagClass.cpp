#include "TagClass.h"
#include "TriggerClass.h"
#include "TriggerTypeClass.h"
#include "../Abstract/AbstractClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Houses/HouseClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// TagClass.cpp - Tag (trigger-link) implementation
// ============================================================================
// A TagClass binds a human-readable name to a list of TriggerClass pointers.
// Map objects that carry a tag will, when their trigger event fires, invoke
// SpringAll() which in turn fires every trigger attached to the tag. Tags are
// the glue between the static map placement (objects) and the dynamic event
// system (triggers). This file implements:
//   * Static registry (Array) management and name-based lookup
//   * Construction / destruction with full member initialisation
//   * Binary stream persistence (Load / Save) of the tag's name, state and
//     attached-trigger list (stored as trigger-name strings for relocation)
//   * Trigger attach / detach / query / clear with duplicate suppression
//   * Object assignment tracking so the editor can tell which map objects
//     reference a given tag
//   * SpringAll() - fire every enabled attached trigger
//   * CRC contribution for save-game integrity
// ============================================================================

DynamicVectorClass<TagClass*>* TagClass::Array = nullptr;

// ----------------------------------------------------------------------------
// Find - locate a tag by its (case-insensitive) name.
// ----------------------------------------------------------------------------
TagClass* TagClass::Find(const char* pName) {
    if (!Array || !pName) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TagClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->Name, pName)) {
            return item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// FindOrAllocate - return an existing tag or create a new one. The sentinel
// strings "<none>" and "none" map to no tag, mirroring the INI convention.
// ----------------------------------------------------------------------------
TagClass* TagClass::FindOrAllocate(const char* pName) {
    if (!pName || !pName[0]) return nullptr;
    if (!_strcmpi(pName, "<none>") || !_strcmpi(pName, "none")) return nullptr;

    TagClass* found = Find(pName);
    if (found) return found;

    TagClass* newItem = GameCreate<TagClass>(pName);
    if (newItem && Array) {
        Array->Add(newItem);
    }
    return newItem;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
TagClass::TagClass(const char* pName) noexcept
    : AbstractClass(noinit), IsInitialized(true) {
    int32 i = 0;
    if (pName) {
        while (pName[i] && i < static_cast<int32>(sizeof(Name)) - 1) {
            Name[i] = pName[i];
            ++i;
        }
    }
    Name[i] = '\0';
}

// ----------------------------------------------------------------------------
// Destructor - release the trigger/object lists. The DynamicVectorClass
// destructor frees its backing storage; we just drop the logical contents.
// ----------------------------------------------------------------------------
TagClass::~TagClass() {
    TriggerList.Clear();
    AssignedObjects.Clear();
    IsInitialized = false;
}

// ----------------------------------------------------------------------------
// GetClassID - unique CLSID for persistence dispatch.
// ----------------------------------------------------------------------------
HRESULT TagClass::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = 0x4A4A4A4A;   // 'JJJJ' sentinel for TagClass
    pClassID->Data2 = 0x4A4A;
    pClassID->Data3 = 0x4A4A;
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0x4A;
    return S_OK;
}

// ----------------------------------------------------------------------------
// Load - read the tag's persistent state from a binary stream.
//
// The on-disk layout is: Name[24] | IsInitialized | TriggerCount | for each
// trigger a 24-byte name string. Trigger pointers are re-resolved by name
// against the TriggerClass registry after the scenario has been loaded.
// ----------------------------------------------------------------------------
HRESULT TagClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;
    char nameBuf[0x18];
    hr = pStm->Read(nameBuf, sizeof(nameBuf), &read);
    if (hr < 0 || read != sizeof(nameBuf)) {
        return E_FAIL;
    }
    std::memcpy(Name, nameBuf, sizeof(Name));
    Name[sizeof(Name) - 1] = '\0';

    uint8 initByte = 0;
    hr = pStm->Read(&initByte, sizeof(initByte), &read);
    if (hr < 0 || read != sizeof(initByte)) {
        return E_FAIL;
    }
    IsInitialized = (initByte != 0);

    int32 trigCount = 0;
    hr = pStm->Read(&trigCount, sizeof(trigCount), &read);
    if (hr < 0 || read != sizeof(trigCount)) {
        return E_FAIL;
    }
    if (trigCount < 0) trigCount = 0;

    TriggerList.Clear();
    for (int32 i = 0; i < trigCount; ++i) {
        char trigName[0x18];
        hr = pStm->Read(trigName, sizeof(trigName), &read);
        if (hr < 0 || read != sizeof(trigName)) {
            return E_FAIL;
        }
        trigName[sizeof(trigName) - 1] = '\0';
        if (trigName[0]) {
            TriggerClass* pTrig = TriggerClass::Find(trigName);
            if (pTrig) {
                TriggerList.Add(pTrig);
            }
        }
    }

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - write the tag's persistent state to a binary stream.
// ----------------------------------------------------------------------------
HRESULT TagClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;
    hr = pStm->Write(Name, sizeof(Name), &written);
    if (hr < 0 || written != sizeof(Name)) {
        return E_FAIL;
    }

    uint8 initByte = IsInitialized ? 1 : 0;
    hr = pStm->Write(&initByte, sizeof(initByte), &written);
    if (hr < 0 || written != sizeof(initByte)) {
        return E_FAIL;
    }

    int32 trigCount = TriggerList.Count;
    hr = pStm->Write(&trigCount, sizeof(trigCount), &written);
    if (hr < 0 || written != sizeof(trigCount)) {
        return E_FAIL;
    }

    for (int32 i = 0; i < TriggerList.Count; ++i) {
        char trigName[0x18];
        std::memset(trigName, 0, sizeof(trigName));
        TriggerClass* pTrig = TriggerList.GetItem(i);
        if (pTrig && pTrig->ID) {
            int32 j = 0;
            while (pTrig->ID[j] && j < static_cast<int32>(sizeof(trigName)) - 1) {
                trigName[j] = pTrig->ID[j];
                ++j;
            }
            trigName[j] = '\0';
        }
        hr = pStm->Write(trigName, sizeof(trigName), &written);
        if (hr < 0 || written != sizeof(trigName)) {
            return E_FAIL;
        }
    }

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// ----------------------------------------------------------------------------
// RTTI / size
// ----------------------------------------------------------------------------
AbstractType TagClass::WhatAmI() const {
    return AbstractType::Tag;
}

int32 TagClass::Size() const {
    return sizeof(TagClass);
}

// ----------------------------------------------------------------------------
// AttachTrigger / Add_Trigger - register a trigger with this tag. Duplicates
// are silently ignored so the same trigger cannot fire twice for one event.
// ----------------------------------------------------------------------------
void TagClass::AttachTrigger(TriggerClass* pTrigger) {
    Add_Trigger(pTrigger);
}

void TagClass::Add_Trigger(TriggerClass* pTrigger) {
    if (!pTrigger) return;
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        if (TriggerList.GetItem(i) == pTrigger) {
            return;     // already attached
        }
    }
    TriggerList.Add(pTrigger);
}

// ----------------------------------------------------------------------------
// DetachTrigger / Remove_Trigger - unregister a trigger.
// ----------------------------------------------------------------------------
void TagClass::DetachTrigger(TriggerClass* pTrigger) {
    Remove_Trigger(pTrigger);
}

void TagClass::Remove_Trigger(TriggerClass* pTrigger) {
    if (!pTrigger) return;
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        if (TriggerList.GetItem(i) == pTrigger) {
            TriggerList.Remove(i);
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// Get_Trigger_Count - number of triggers currently attached.
// ----------------------------------------------------------------------------
int32 TagClass::Get_Trigger_Count() const {
    return TriggerList.Count;
}

// ----------------------------------------------------------------------------
// Get_Trigger - fetch a trigger by index (bounds-checked).
// ----------------------------------------------------------------------------
TriggerClass* TagClass::Get_Trigger(int32 index) const {
    if (index < 0 || index >= TriggerList.Count) return nullptr;
    return TriggerList.GetItem(index);
}

// ----------------------------------------------------------------------------
// Clear_Triggers - drop every attached trigger.
// ----------------------------------------------------------------------------
void TagClass::Clear_Triggers() {
    TriggerList.Clear();
}

// ----------------------------------------------------------------------------
// SpringAll - fire every enabled trigger attached to this tag. A trigger that
// is disabled or has already fired (and is non-repeatable) is skipped.
// ----------------------------------------------------------------------------
void TagClass::SpringAll() {
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        TriggerClass* pTrig = TriggerList.GetItem(i);
        if (!pTrig) continue;
        if (!pTrig->IsEnabled || pTrig->IsDisabled) continue;
        if (pTrig->HasBeenFired && !pTrig->Repeatable) continue;

        pTrig->Fire();
    }
}

// ----------------------------------------------------------------------------
// Assign_To_Object - record that a map object references this tag. Used by the
// editor/RTTI layer to track tag usage; duplicates are ignored.
// ----------------------------------------------------------------------------
void TagClass::Assign_To_Object(AbstractClass* pObject) {
    if (!pObject) return;
    for (int32 i = 0; i < AssignedObjects.Count; ++i) {
        if (AssignedObjects.GetItem(i) == pObject) {
            return;
        }
    }
    AssignedObjects.Add(pObject);
}

// ----------------------------------------------------------------------------
// Is_Assigned - true if the given object is registered as using this tag.
// ----------------------------------------------------------------------------
bool TagClass::Is_Assigned(AbstractClass* pObject) const {
    if (!pObject) return false;
    for (int32 i = 0; i < AssignedObjects.Count; ++i) {
        if (AssignedObjects.GetItem(i) == pObject) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Compute_CRC - feed the tag's name and trigger count into the CRC stream.
// ----------------------------------------------------------------------------
void TagClass::Compute_CRC(CRCEngine& crc) const {
    crc.AddData(Name, static_cast<int32>(sizeof(Name)));
    crc.AddData(&IsInitialized, sizeof(IsInitialized));
    int32 count = TriggerList.Count;
    crc.AddData(&count, sizeof(count));
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        TriggerClass* pTrig = TriggerList.GetItem(i);
        if (pTrig && pTrig->ID) {
            int32 idLen = static_cast<int32>(std::strlen(pTrig->ID));
            crc.AddData(pTrig->ID, idLen);
        } else {
            int32 zero = 0;
            crc.AddData(&zero, sizeof(zero));
        }
    }
}
