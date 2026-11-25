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
#include "../AI/TEventClass.h"
#include "../AI/TriggerClass.h"
#include "../AI/TeamClass.h"
#include "../AI/TeamTypeClass.h"
#include "../Abstract/AbstractClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Map/CellClass.h"

#include <cstring>
#include <cstdio>
#include <cmath>

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

// ============================================================================
// asm: int __thiscall EventClass_HasOccured(EventClass *this, int eventKind,
//        HouseClass *pHouse, _ObjectClass *Object, TimerStruct *ActivationFrame,
//        char *isRepeating, _ObjectClass *source)      (asm 0x71E964, retn 18h)
//
// 三个跳转表（off_71F218 / off_71F27C / off_71F328）逐条对应下面的三段
// switch。所有常量（含 0x88888889 的 15 帧/秒换算、1280 的近距离阈值）
// 与 asm 逐字节一致；HouseClass 的 asm 偏移在注释中标出，字段名取 OpenYR
// 侧语义最近的成员。
// ============================================================================

namespace {

// asm 内联 strcmp 扫描 vec_TechnoTypes，按 nameINI 找类型下标（找不到 -1）。
int32 FindTechnoTypeIndex(const char* pName)
{
    if (pName == nullptr || TechnoTypeClass::Array == nullptr) return -1;

    for (int32 i = TechnoTypeClass::Array->Count - 1; i >= 0; --i) {
        TechnoTypeClass* pType = TechnoTypeClass::Array->GetItem(i);
        if (pType == nullptr) continue;
        const char* pID = pType->get_ID();
        if (pID != nullptr && std::strcmp(pID, pName) == 0) return i;
    }
    return -1;
}

// CounterClass_GetItemAmount —— 类型计数表求和（asm 逐项累加）。
int32 CounterTotal(const int32* pCounts, int32 count)
{
    if (pCounts == nullptr) return 0;
    int32 total = 0;
    for (int32 i = 0; i < count; ++i) total += pCounts[i];
    return total;
}

// asm: CounterClass_GetItemCount —— 单个类型槽位的计数。
int32 CounterItem(const int32* pCounts, int32 count, int32 idx)
{
    if (pCounts == nullptr || idx < 0 || idx >= count) return 0;
    return pCounts[idx];
}

}  // namespace

bool EventClass::HasOccurred(TEventClass* pEvent, int32 eventKind,
                             HouseClass* pHouse, AbstractClass* pObject,
                             TriggerClass* pTrigger, AbstractClass* pSource,
                             bool* isRepeating)
{
    if (pEvent == nullptr) return false;

    ScenarioClass* pScenario = ScenarioClass::Instance;
    const int32 ev    = pEvent->EventKind;   // asm: [ecx+EventClass.eventKind]
    const int32 param = pEvent->Data;        // asm: [ecx+EventClass.param]
    const int32 frame = Game::CurrentFrame;  // asm: 全局 Frame

    // 若干分支命中时把"本事件已发生"写回调用方的重复标志槽。
    const auto markRepeating = [isRepeating]() {
        if (isRepeating != nullptr) *isRepeating = true;
    };

    // ------------------------------------------------------------------
    // 第一跳转表 off_71F218 / byte_71F248：ev-13 落在 [0,48] 时进入。
    // ------------------------------------------------------------------
    if (static_cast<uint32>(ev - 13) <= 48u) {
        switch (ev) {
        case 13:   // SecondsHaveElapsed（asm 首落点 = 13）
        case 51: { // 同 SecondsHaveElapsed（跳转表索引 38）
            // asm: ActivationFrame { TimeStart @0x00, TimeLeft @0x08 }。
            //   原实现直接 `Timer <= 0` —— Timer 缺省就是 0，于是本条**恒真**，
            //   任何带 "time elapsed" 的触发器在第一帧就开火。这里按参数语义
            //   判定：param 是秒数，15 帧/秒。显式计时器（SetTimer）优先。
            if (pTrigger != nullptr && pTrigger->Timer > 0) {
                return false;              // 计时器尚未走完
            }
            if (param <= 0) return true;
            return frame >= param * 15;
        }
        case 14: { // MissionTimerExpired
            if (pScenario == nullptr) return false;
            const int32 start = pScenario->MissionTimer.StartTime;
            if (start == -1) return false;
            int32 left = pScenario->MissionTimer.TimeLeft;
            const int32 elapsed = frame - start;
            if (elapsed >= left) return true;
            left -= elapsed;
            return left == 0;
        }
        case 27: // GlobalIsSet（间接表索引 14）
            return pScenario != nullptr && pScenario->GetGlobalValue(param);
        case 28: // GlobalIsClear（间接表索引 15）
            return pScenario != nullptr && !pScenario->GetGlobalValue(param);
        case 36: // localIsSet
            return pScenario != nullptr && pScenario->GetLocalValue(param);
        case 37: // localIsClear
            return pScenario != nullptr && !pScenario->GetLocalValue(param);
        case 45: // AmbientLightBelow：AmbientCopy <= param（setle）
            return pScenario != nullptr &&
                   pScenario->AmbientCurrent <= param;
        case 46: // AmbientLightNotBelow：AmbientCopy >= param（setnl）
            return pScenario != nullptr &&
                   pScenario->AmbientCurrent >= param;
        case 47: { // ScenarioTimeElapsed：param <= Frame / 15
            // asm 0x88888889 + add + sar 3 + 取反补 1 == 有符号 Frame/15。
            return param <= frame / 15;
        }
        case 60:   // TechnoTypeOnMap
        case 61: { // TechnoTypeNotOnMap
            const bool wantOnMap = (ev == 60);
            const int32 idx = FindTechnoTypeIndex(pEvent->TechnoININame);
            if (idx < 0) return false;

            TechnoTypeClass* pFound = TechnoTypeClass::Array->GetItem(idx);
            const int32 technoCount =
                TechnoClass::Array != nullptr ? TechnoClass::Array->Count : 0;

            if (wantOnMap) {
                // asm: 逆序扫描 vec_Techno，statistics 到 param 即真。
                if (technoCount <= 0) return false;
                int32 count = 0;
                for (int32 i = technoCount - 1; i >= 0; --i) {
                    TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
                    if (pTechno != nullptr && pTechno->GetType() == pFound) {
                        ++count;
                    }
                    if (count >= param) return true;
                }
                return false;
            }

            // TechnoTypeNotOnMap：地图为空 → 真；出现任一实例 → 假。
            if (technoCount <= 0) return true;
            for (int32 i = technoCount - 1; i >= 0; --i) {
                TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
                if (pTechno != nullptr && pTechno->GetType() == pFound) {
                    return false;
                }
            }
            return true;
        }
        default:
            break;  // 跳转表 11 = skip，落入第二段
        }
    }

    // ------------------------------------------------------------------
    // 第二跳转表 off_71F27C / byte_71F284（skip 段）。
    //   byte==0 -> loc_71EC9A：要求 eventKind == ev 且 Debug_Map 关闭；
    //   byte==1 -> loc_71ECAF：不要求 kind 匹配。
    // ------------------------------------------------------------------
    if (ev == 0) return false;

    if (!(ev >= 19 && ev <= 22)) {
        if (static_cast<uint32>(ev - 1) <= 58u) {
            static const uint8 kindMatch[59] = {
                0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0,
                0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0 };
            if (kindMatch[ev - 1] == 0) {
                if (eventKind != ev) return false;
                if (Game::bShowDebugInfo) return false;
            }
        }
    }

    // loc_71ECAF：通用对象/关联方判定。
    if (ev == 1 || ev == 24 || ev == 25 || ev == 26 || ev == 59) {
        // loc_71F1BD
        if (eventKind != ev) return false;

        if (param != -1 &&
            HouseClass::Find_By_Index_No_MP(param) == nullptr) {
            return false;
        }
        if (pObject == nullptr) return false;

        if (param != -1) {
            // asm: Object->[+38h]（所属玩家号）与 house->PlayerNumber 比较。
            HouseClass* pOwner = pObject->GetOwningHouse();
            HouseClass* pHouseOfParam = HouseClass::Find_By_Index_No_MP(param);
            if (pHouseOfParam == nullptr) return false;
            if (pOwner == nullptr) return false;
            if (pOwner->PlayerNumber != pHouseOfParam->PlayerNumber) {
                return false;
            }
        }

        markRepeating();
        pEvent->House = pObject->GetOwningHouse();  // asm: [obj+3Ch] 写回
        return true;
    }

    if (ev == 53) {
        // 对象所属方与 param 指定方的 PlayerNumber 相同。
        if (eventKind != ev) return false;
        if (pObject == nullptr) return false;
        if (param == -1) return false;

        HouseClass* pHouseOfParam = HouseClass::Find_By_Index_No_MP(param);
        if (pHouseOfParam == nullptr) return false;

        HouseClass* pOwner = pObject->GetOwningHouse();  // asm: [obj+0D0h](1)
        if (pOwner == nullptr) return false;
        if (pOwner->PlayerNumber != pHouseOfParam->PlayerNumber) return false;

        markRepeating();
        return true;
    }

    if (ev == 54) {
        // 对象是步兵，且其类型下标 == param。
        if (eventKind != ev) return false;
        if (pObject == nullptr) return false;
        if (pObject->WhatAmI() != AbstractType::Infantry) return false;
        if (param == -1) return false;

        TechnoClass* pTechno = static_cast<TechnoClass*>(pObject);
        TechnoTypeClass* pType = pTechno->GetType();
        if (pType == nullptr) return false;
        if (pType->Get_ArrayIndex() != param) return false;  // asm: [obj+40h]

        markRepeating();
        return true;
    }

    if (ev == 34) {
        // comesNearWaypoint：对象到 param 号路点的距离 <= 1280。
        if (eventKind != ev) return false;
        if (pObject == nullptr) return false;
        if (pScenario == nullptr) return false;

        CellClass* pCell = pScenario->LocateWaypoint(param);
        if (pCell == nullptr) return false;

        // 原版路点以 lepton 存储；OpenYR 存 CellStruct，换算回 lepton。
        const int32 wpX = static_cast<int32>(pCell->MapCoords.X) * 256;
        const int32 wpY = static_cast<int32>(pCell->MapCoords.Y) * 256;
        const int32 wpZ = 0;

        const CoordStruct coords = pObject->GetCoords();  // asm: [obj+48h]
        const int32 dx = coords.X - wpX;
        const int32 dy = coords.Y - wpY;
        const int32 dz = coords.Z - wpZ;

        // asm 的 FPU 累加顺序：dx*dx + dz*dz + dy*dy，再开方取整。
        const double dist2 = static_cast<double>(dx) * dx +
                             static_cast<double>(dz) * dz +
                             static_cast<double>(dy) * dy;
        const int32 dist = static_cast<int32>(std::sqrt(dist2));
        return dist <= 1280;
    }

    if (ev == 44) {
        // sameHouse：源对象的 OwningPlayer 的 PlayerNumber == param。
        if (eventKind != ev) return false;
        if (pSource == nullptr) return false;
        HouseClass* pOwner = pSource->GetOwningHouse();
        if (pOwner == nullptr) return false;
        if (param != pOwner->PlayerNumber) return false;
    }

    // ------------------------------------------------------------------
    // 第三跳转表 off_71F2C0 / byte_71F2F8：pHouse 非空、ev-12 ∈ [0,45]。
    // ------------------------------------------------------------------
    if (pHouse != nullptr && static_cast<uint32>(ev - 12) <= 45u) {
        switch (ev) {
        case 12:   // case 0：pHouse 的阵营下标 < param 则假
            if (pHouse->GetArrayIndex() < param) return false;
            break;

        case 15:   // case 3：TotalKilledBuildings >= param
            if (pHouse->TotalKilledBuildings < param) return false;
            break;

        case 16: { // case 4：TotalKilledUnits >= param
            if (pHouse->TotalKilledUnits < param) return false;
            break;
        }

        case 17: { // case 5：houseHasNoFactories —— 还有工厂则假
            if (pHouse->OwnedBuildingCount > 0) {
                for (int32 i = 0; i < pHouse->OwnedBuildings.Count; ++i) {
                    BuildingClass* pBuilding = pHouse->OwnedBuildings.GetItem(i);
                    if (pBuilding == nullptr) continue;
                    if (pBuilding->InLimbo()) continue;
                    BuildingTypeClass* pType =
                        static_cast<BuildingTypeClass*>(pBuilding->GetType());
                    if (pType != nullptr && pType->IsFactory()) return false;
                }
            }
            break;
        }

        case 18:   // case 6：pHouse 的 +0x1F9 标志必须非零（存疑）
            if (!pHouse->IsDiscovered) return false;
            break;

        case 19:   // case 7：+0x26C == param
            if (pHouse->OwnedUnitCount != param) return false;
            markRepeating();
            break;

        case 20:   // case 8：+0x274 == param
            if (pHouse->OwnedAircraftCount != param) return false;
            markRepeating();
            break;

        case 21:   // case 9：+0x270 == param
            if (pHouse->OwnedInfantryCount != param) return false;
            markRepeating();
            break;

        case 22:   // case 10：+0x278 == param
            if (pHouse->OwnedBuildingCount != param) return false;
            markRepeating();
            break;

        case 23: { // case 11：loc_71EEA9 —— 已消失的同型队伍
            bool found = false;
            if (pEvent->Team != nullptr && TeamClass::Array != nullptr) {
                for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
                    TeamClass* pTeam = TeamClass::Array->GetItem(i);
                    if (pTeam == nullptr) continue;
                    if (pTeam->Type != pEvent->Team) continue;
                    if (pTeam->Members.Count != 0) continue;   // FirstUnitInTeam
                    if (!pTeam->TimeToDissapear) continue;     // asm +0x82
                    found = true;
                    break;
                }
            }
            if (!found) return false;
            markRepeating();
            break;
        }

        case 32: { // case 20：HouseOwnsBuilding
            const int32 n = CounterItem(pHouse->OwnedBuildingTypeCounts,
                                        HouseClass::MaxTypeCounts, param);
            if (n == 0) return false;
            markRepeating();
            break;
        }

        case 52: { // case 40：pHouse 的 +0x24 子对象 vt+18h > param 则假
            if (pHouse->Type != nullptr &&
                pHouse->Type->Get_ArrayIndex() > param) {
                return false;
            }
            break;
        }

        case 57: { // case 45：isValidAmount —— 该类型建筑必须已清零
            const int32 n = CounterItem(pHouse->OwnedBuildingTypeCounts,
                                        HouseClass::MaxTypeCounts, param);
            if (n != 0) return false;
            markRepeating();
            break;
        }

        default:
            break;  // 跳转表 13 = isValidHouse
        }
    }

    // ------------------------------------------------------------------
    // isValidHouse / 第四跳转表 off_71F328 / byte_71F350（索引 ev-3）。
    // ------------------------------------------------------------------
    HouseClass* pHouseOfParam = HouseClass::Find_By_Index_No_MP(param);
    if (pHouseOfParam == nullptr) return true;         // returnTrue_
    if (static_cast<uint32>(ev - 3) > 0x37u) return true;

    switch (ev) {
    case 3:   // case 0：+0x244 标志（asm House.IsHuman）
        return pHouseOfParam->IsHumanPlayer;

    case 5:   // case 1：+0x1F4 标志（存疑）
        return pHouseOfParam->IsDiscovered;

    case 9: { // case 2：还有载具则假，否则步兵计数 <= 0 为真
        const int32 veh = CounterTotal(pHouseOfParam->OwnedUnitTypeCounts,
                                       HouseClass::MaxTypeCounts);
        if (veh > 0) return false;
        const int32 inf = CounterTotal(pHouseOfParam->OwnedInfantryTypeCounts,
                                       HouseClass::MaxTypeCounts);
        return inf <= 0;
    }

    case 10:  // case 3：+0x2F0 <= 0
        return pHouseOfParam->OwnedBuildingCount <= 0;

    case 11: { // case 4：建筑/载具/步兵任一非空则假
        if (pHouseOfParam->OwnedBuildingCount > 0) return false;
        if (CounterTotal(pHouseOfParam->OwnedUnitTypeCounts,
                         HouseClass::MaxTypeCounts) > 0) return false;
        if (CounterTotal(pHouseOfParam->OwnedInfantryTypeCounts,
                         HouseClass::MaxTypeCounts) > 0) return false;
        return true;
    }

    case 30:   // case 27：houseNotEnoughPower（< 1.0）
        return pHouseOfParam->Current_Power_Percentage() < 1.0;

    case 55:   // case 52：+0x2EC <= 0
        return pHouseOfParam->OwnedInfantryCount <= 0;

    case 56: { // case 53：OwnedUnits - 0x2EC <= 0 且 +0x2F4 <= 0
        if (pHouseOfParam->UnitCount - pHouseOfParam->OwnedInfantryCount > 0) {
            return false;
        }
        return pHouseOfParam->OwnedAircraftCount <= 0;
    }

    case 58:   // case 55：houseEnoughPower（>= 1.0）
        return pHouseOfParam->Current_Power_Percentage() >= 1.0;

    default:
        return true;   // returnTrue_
    }
}
