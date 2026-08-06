// ============================================================================
// EventClass.cpp - Game event system for single-player missions
// ============================================================================
// Implements scripted mission event management: event queue, condition
// evaluation, event dispatch with callbacks, and save/load state.
// ============================================================================

#include "EventClass.h"
#include "../INI/INIClass.h"
#include "../Game/Game.h"
#include "../COM/IUnknown.h"

#include <cstring>
#include <cstdio>

// ============================================================================
// Construction / destruction
// ============================================================================

EventClass::EventClass() noexcept
    : NextID(1)
{
}

EventClass::~EventClass()
{
    Clear();
}

// ============================================================================
// Internal helpers
// ============================================================================

int32 EventClass::Next_Event_ID()
{
    return NextID++;
}

void EventClass::Dispatch_Event(GameEvent* pEvent)
{
    if (!pEvent) return;

    // Invoke the callback if one is registered.
    if (pEvent->Callback) {
        pEvent->Callback(pEvent->UserData, pEvent->ID);
    }

    // Update the event's firing metadata.
    pEvent->FireCount++;
    pEvent->LastFireFrame = Game::CurrentFrame;

    // Transition the event state.
    if (pEvent->Repeatable) {
        // Repeatable events stay pending so they can fire again.
        pEvent->State = EventState::Pending;
    } else {
        pEvent->State = EventState::Fired;
    }
}

bool EventClass::Check_Condition(const GameEvent* pEvent) const
{
    if (!pEvent || pEvent->State != EventState::Pending) {
        return false;
    }

    switch (pEvent->Type) {
    case GameEventType::TimeElapsed:
        // Fire when the current frame reaches or exceeds the trigger frame.
        return Game::CurrentFrame >= pEvent->TriggerFrame;

    case GameEventType::Custom:
        // Custom events are only fired via explicit Trigger_Event() calls.
        return false;

    default:
        // All other event types are notification-driven, not per-frame.
        return false;
    }
}

// ============================================================================
// Event queue management
// ============================================================================

int32 EventClass::Add_Event(const GameEvent& event)
{
    GameEvent* pNew = GameCreate<GameEvent>();
    if (!pNew) return -1;

    *pNew = event;
    pNew->ID = Next_Event_ID();
    if (pNew->State == EventState::Inactive) {
        pNew->State = EventState::Pending;
    }

    if (!Events.Add(pNew)) {
        GameDelete(pNew);
        return -1;
    }

    return pNew->ID;
}

int32 EventClass::Add_Timer_Event(int32 delayFrames, EventCallback callback,
                                  void* pUserData)
{
    GameEvent evt;
    evt.Type = GameEventType::TimeElapsed;
    evt.State = EventState::Pending;
    evt.TriggerFrame = Game::CurrentFrame + delayFrames;
    evt.Callback = callback;
    evt.UserData = pUserData;
    evt.Repeatable = false;
    return Add_Event(evt);
}

int32 EventClass::Add_Location_Event(int32 cellX, int32 cellY,
                                     EventCallback callback, void* pUserData)
{
    GameEvent evt;
    evt.Type = GameEventType::UnitEnteredCell;
    evt.State = EventState::Pending;
    evt.TriggerCellX = cellX;
    evt.TriggerCellY = cellY;
    evt.Callback = callback;
    evt.UserData = pUserData;
    evt.Repeatable = false;
    return Add_Event(evt);
}

int32 EventClass::Add_Unit_Created_Event(int32 objectType, int32 houseIndex,
                                         EventCallback callback, void* pUserData)
{
    GameEvent evt;
    evt.Type = GameEventType::UnitCreated;
    evt.State = EventState::Pending;
    evt.TriggerObjectType = objectType;
    evt.TriggerHouseIndex = houseIndex;
    evt.Callback = callback;
    evt.UserData = pUserData;
    evt.Repeatable = false;
    return Add_Event(evt);
}

int32 EventClass::Add_Building_Destroyed_Event(int32 objectType, int32 houseIndex,
                                               EventCallback callback, void* pUserData)
{
    GameEvent evt;
    evt.Type = GameEventType::BuildingDestroyed;
    evt.State = EventState::Pending;
    evt.TriggerObjectType = objectType;
    evt.TriggerHouseIndex = houseIndex;
    evt.Callback = callback;
    evt.UserData = pUserData;
    evt.Repeatable = false;
    return Add_Event(evt);
}

bool EventClass::Remove_Event(int32 eventID)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (pEvent && pEvent->ID == eventID) {
            GameDelete(pEvent);
            Events.Remove(i);
            return true;
        }
    }
    return false;
}

void EventClass::Clear()
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        GameDelete(pEvent);
    }
    Events.Clear();
    NextID = 1;
}

// ============================================================================
// Event lookup
// ============================================================================

const GameEvent* EventClass::Get_Event(int32 index) const
{
    if (index < 0 || index >= Events.Count) return nullptr;
    return Events.GetItem(index);
}

GameEvent* EventClass::Get_Event_Mutable(int32 index)
{
    if (index < 0 || index >= Events.Count) return nullptr;
    return Events.GetItem(index);
}

const GameEvent* EventClass::Find_Event(int32 eventID) const
{
    for (int32 i = 0; i < Events.Count; ++i) {
        const GameEvent* pEvent = Events.GetItem(i);
        if (pEvent && pEvent->ID == eventID) return pEvent;
    }
    return nullptr;
}

GameEvent* EventClass::Find_Event_Mutable(int32 eventID)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (pEvent && pEvent->ID == eventID) return pEvent;
    }
    return nullptr;
}

// ============================================================================
// Event state control
// ============================================================================

bool EventClass::Enable_Event(int32 eventID)
{
    GameEvent* pEvent = Find_Event_Mutable(eventID);
    if (!pEvent) return false;
    if (pEvent->State == EventState::Fired && !pEvent->Repeatable) {
        // Cannot re-enable a non-repeatable fired event.
        return false;
    }
    pEvent->State = EventState::Pending;
    return true;
}

bool EventClass::Disable_Event(int32 eventID)
{
    GameEvent* pEvent = Find_Event_Mutable(eventID);
    if (!pEvent) return false;
    pEvent->State = EventState::Disabled;
    return true;
}

bool EventClass::Reset_Event(int32 eventID)
{
    GameEvent* pEvent = Find_Event_Mutable(eventID);
    if (!pEvent) return false;
    pEvent->State = EventState::Pending;
    pEvent->FireCount = 0;
    pEvent->LastFireFrame = -1;
    return true;
}

// ============================================================================
// Event dispatch
// ============================================================================

void EventClass::Update()
{
    // Evaluate all pending events for per-frame conditions (TimeElapsed).
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent) continue;
        if (pEvent->State != EventState::Pending) continue;

        if (Check_Condition(pEvent)) {
            Dispatch_Event(pEvent);
        }
    }
}

bool EventClass::Trigger_Event(int32 eventID)
{
    GameEvent* pEvent = Find_Event_Mutable(eventID);
    if (!pEvent) return false;
    if (pEvent->State != EventState::Pending) return false;

    // Only Custom-type events (and any event explicitly triggered) fire here.
    Dispatch_Event(pEvent);
    return true;
}

// ============================================================================
// Notification handlers
// ============================================================================

void EventClass::Notify_Unit_Entered_Cell(int32 cellX, int32 cellY,
                                          int32 unitType, int32 houseIndex)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::UnitEnteredCell) continue;

        // Check cell coordinates.
        if (pEvent->TriggerCellX != cellX || pEvent->TriggerCellY != cellY) {
            continue;
        }

        // Check house filter (-1 means any house).
        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }

        // Check unit type filter (-1 means any unit type).
        if (pEvent->TriggerObjectType >= 0 &&
            pEvent->TriggerObjectType != unitType) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_Unit_Created(int32 unitType, int32 houseIndex)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::UnitCreated) continue;

        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }
        if (pEvent->TriggerObjectType >= 0 &&
            pEvent->TriggerObjectType != unitType) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_Unit_Destroyed(int32 unitType, int32 houseIndex)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::UnitDestroyed) continue;

        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }
        if (pEvent->TriggerObjectType >= 0 &&
            pEvent->TriggerObjectType != unitType) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_Building_Created(int32 buildingType, int32 houseIndex)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::BuildingCreated) continue;

        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }
        if (pEvent->TriggerObjectType >= 0 &&
            pEvent->TriggerObjectType != buildingType) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_Building_Destroyed(int32 buildingType, int32 houseIndex)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::BuildingDestroyed) continue;

        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }
        if (pEvent->TriggerObjectType >= 0 &&
            pEvent->TriggerObjectType != buildingType) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_Credits_Changed(int32 houseIndex, int32 currentCredits)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::CreditsReached) continue;

        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }
        if (currentCredits < pEvent->TriggerValue) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_House_Defeated(int32 houseIndex)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::HouseDefeated) continue;

        if (pEvent->TriggerHouseIndex >= 0 &&
            pEvent->TriggerHouseIndex != houseIndex) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

void EventClass::Notify_Objective_Complete(int32 objectiveID)
{
    for (int32 i = 0; i < Events.Count; ++i) {
        GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent || pEvent->State != EventState::Pending) continue;
        if (pEvent->Type != GameEventType::ObjectiveComplete) continue;

        if (pEvent->TriggerValue != objectiveID) {
            continue;
        }

        Dispatch_Event(pEvent);
    }
}

// ============================================================================
// Event callbacks
// ============================================================================

bool EventClass::Set_Callback(int32 eventID, EventCallback callback, void* pUserData)
{
    GameEvent* pEvent = Find_Event_Mutable(eventID);
    if (!pEvent) return false;
    pEvent->Callback = callback;
    pEvent->UserData = pUserData;
    return true;
}

// ============================================================================
// Save / Load event state (binary stream)
// ============================================================================

bool EventClass::Save(IStream* pStm) const
{
    if (!pStm) return false;

    HRESULT hr;

    // Write the next event ID.
    hr = pStm->Write(&NextID, sizeof(NextID), nullptr);
    if (hr < 0) return false;

    // Write the event count.
    int32 count = Events.Count;
    hr = pStm->Write(&count, sizeof(count), nullptr);
    if (hr < 0) return false;

    // Write each event's serializable state.
    for (int32 i = 0; i < Events.Count; ++i) {
        const GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent) continue;

        hr = pStm->Write(&pEvent->ID, sizeof(pEvent->ID), nullptr);
        if (hr < 0) return false;

        int32 typeInt = static_cast<int32>(pEvent->Type);
        hr = pStm->Write(&typeInt, sizeof(typeInt), nullptr);
        if (hr < 0) return false;

        int32 stateInt = static_cast<int32>(pEvent->State);
        hr = pStm->Write(&stateInt, sizeof(stateInt), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->TriggerFrame, sizeof(pEvent->TriggerFrame), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->TriggerCellX, sizeof(pEvent->TriggerCellX), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->TriggerCellY, sizeof(pEvent->TriggerCellY), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->TriggerObjectType, sizeof(pEvent->TriggerObjectType), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->TriggerHouseIndex, sizeof(pEvent->TriggerHouseIndex), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->TriggerValue, sizeof(pEvent->TriggerValue), nullptr);
        if (hr < 0) return false;

        int32 repeatableInt = pEvent->Repeatable ? 1 : 0;
        hr = pStm->Write(&repeatableInt, sizeof(repeatableInt), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->FireCount, sizeof(pEvent->FireCount), nullptr);
        if (hr < 0) return false;

        hr = pStm->Write(&pEvent->LastFireFrame, sizeof(pEvent->LastFireFrame), nullptr);
        if (hr < 0) return false;
    }

    return true;
}

bool EventClass::Load(IStream* pStm)
{
    if (!pStm) return false;

    Clear();

    HRESULT hr;
    DWORD bytesRead = 0;

    // Read the next event ID.
    hr = pStm->Read(&NextID, sizeof(NextID), &bytesRead);
    if (hr < 0 || bytesRead != sizeof(NextID)) return false;

    // Read the event count.
    int32 count = 0;
    hr = pStm->Read(&count, sizeof(count), &bytesRead);
    if (hr < 0 || bytesRead != sizeof(count)) return false;

    // Read each event.
    for (int32 i = 0; i < count; ++i) {
        GameEvent* pEvent = GameCreate<GameEvent>();
        if (!pEvent) return false;

        hr = pStm->Read(&pEvent->ID, sizeof(pEvent->ID), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->ID)) { GameDelete(pEvent); return false; }

        int32 typeInt = 0;
        hr = pStm->Read(&typeInt, sizeof(typeInt), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(typeInt)) { GameDelete(pEvent); return false; }
        pEvent->Type = static_cast<GameEventType>(typeInt);

        int32 stateInt = 0;
        hr = pStm->Read(&stateInt, sizeof(stateInt), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(stateInt)) { GameDelete(pEvent); return false; }
        pEvent->State = static_cast<EventState>(stateInt);

        hr = pStm->Read(&pEvent->TriggerFrame, sizeof(pEvent->TriggerFrame), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->TriggerFrame)) { GameDelete(pEvent); return false; }

        hr = pStm->Read(&pEvent->TriggerCellX, sizeof(pEvent->TriggerCellX), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->TriggerCellX)) { GameDelete(pEvent); return false; }

        hr = pStm->Read(&pEvent->TriggerCellY, sizeof(pEvent->TriggerCellY), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->TriggerCellY)) { GameDelete(pEvent); return false; }

        hr = pStm->Read(&pEvent->TriggerObjectType, sizeof(pEvent->TriggerObjectType), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->TriggerObjectType)) { GameDelete(pEvent); return false; }

        hr = pStm->Read(&pEvent->TriggerHouseIndex, sizeof(pEvent->TriggerHouseIndex), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->TriggerHouseIndex)) { GameDelete(pEvent); return false; }

        hr = pStm->Read(&pEvent->TriggerValue, sizeof(pEvent->TriggerValue), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->TriggerValue)) { GameDelete(pEvent); return false; }

        int32 repeatableInt = 0;
        hr = pStm->Read(&repeatableInt, sizeof(repeatableInt), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(repeatableInt)) { GameDelete(pEvent); return false; }
        pEvent->Repeatable = (repeatableInt != 0);

        hr = pStm->Read(&pEvent->FireCount, sizeof(pEvent->FireCount), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->FireCount)) { GameDelete(pEvent); return false; }

        hr = pStm->Read(&pEvent->LastFireFrame, sizeof(pEvent->LastFireFrame), &bytesRead);
        if (hr < 0 || bytesRead != sizeof(pEvent->LastFireFrame)) { GameDelete(pEvent); return false; }

        // Callbacks and user data are not serialized; they must be re-registered
        // after loading.
        pEvent->Callback = nullptr;
        pEvent->UserData = nullptr;

        Events.Add(pEvent);
    }

    return true;
}

// ============================================================================
// Save / Load event state (INI)
// ============================================================================

bool EventClass::Save_To_INI(CCINIClass* pINI) const
{
    if (!pINI) return false;

    // Write the event count.
    pINI->WriteInteger("GameEvents", "Count", Events.Count);

    // Write each event's state as a key=value pair.
    for (int32 i = 0; i < Events.Count; ++i) {
        const GameEvent* pEvent = Events.GetItem(i);
        if (!pEvent) continue;

        char keyBuf[32];
        char valBuf[256];

        // Write the event state: "ID,Type,State,TriggerFrame,FireCount"
        std::snprintf(keyBuf, sizeof(keyBuf), "Event%d", i);
        std::snprintf(valBuf, sizeof(valBuf), "%d,%d,%d,%d,%d",
                      pEvent->ID,
                      static_cast<int32>(pEvent->Type),
                      static_cast<int32>(pEvent->State),
                      pEvent->TriggerFrame,
                      pEvent->FireCount);
        pINI->WriteString("GameEvents", keyBuf, valBuf);
    }

    return true;
}

bool EventClass::Load_From_INI(CCINIClass* pINI)
{
    if (!pINI) return false;

    int32 count = pINI->ReadInteger("GameEvents", "Count", 0);
    if (count <= 0) return true;

    for (int32 i = 0; i < count; ++i) {
        char keyBuf[32];
        std::snprintf(keyBuf, sizeof(keyBuf), "Event%d", i);

        char valBuf[256];
        valBuf[0] = '\0';
        pINI->ReadString("GameEvents", keyBuf, "", valBuf, sizeof(valBuf));
        if (valBuf[0] == '\0') continue;

        // Parse "ID,Type,State,TriggerFrame,FireCount"
        int32 id = 0, typeInt = 0, stateInt = 0, triggerFrame = 0, fireCount = 0;
        std::sscanf(valBuf, "%d,%d,%d,%d,%d",
                    &id, &typeInt, &stateInt, &triggerFrame, &fireCount);

        // Try to find an existing event with this ID; if found, update its
        // state.  If not found, create a new one.
        GameEvent* pEvent = Find_Event_Mutable(id);
        if (!pEvent) {
            GameEvent evt;
            evt.ID = id;
            evt.Type = static_cast<GameEventType>(typeInt);
            evt.State = static_cast<EventState>(stateInt);
            evt.TriggerFrame = triggerFrame;
            evt.FireCount = fireCount;
            Add_Event(evt);
            // Update NextID to avoid collisions.
            if (id >= NextID) NextID = id + 1;
        } else {
            pEvent->Type = static_cast<GameEventType>(typeInt);
            pEvent->State = static_cast<EventState>(stateInt);
            pEvent->TriggerFrame = triggerFrame;
            pEvent->FireCount = fireCount;
        }
    }

    return true;
}
