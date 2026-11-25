// =============================================================================
// TriggerClass - scenario trigger instance implementation
//
// A TriggerClass binds a TEventClass (the condition) to a TActionClass (the
// consequence).  Triggers are attached to map objects via TagClass and are
// evaluated every frame by ProcessTriggerEvents().  When an event's condition
// becomes satisfied the trigger fires, executing its action and optionally
// cascading to a linked trigger.
//
// State machine:
//   Armed    -> actively checking conditions each frame.
//   Waiting  -> paused for Timer frames before re-arming (delayed re-trigger).
//   Disabled -> explicitly turned off; no evaluation occurs.
//   Fired    -> has fired at least once; non-repeatable triggers stay idle.
//
// Difficulty filtering:
//   Each trigger carries Easy/Normal/Medium/Hard flags.  Before firing we
//   compare against ScenarioClass::Instance->Difficulty so a trigger only
//   activates on the difficulties it was authored for.
// =============================================================================

#include "TriggerClass.h"
#include "TriggerTypeClass.h"
#include "../Scenario/ScenarioClass.h"
#include "TActionClass.h"
#include "TEventClass.h"
#include "TagClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../Game/Externs.h"
#include "../Map/MapClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Core/SwizzleManagerClass.h"

#include <cstring>
#include <cstdio>
#include <ctime>

// -----------------------------------------------------------------------------
// Static array pointer - the global list of every loaded TriggerClass.
// -----------------------------------------------------------------------------
DynamicVectorClass<TriggerClass*>* TriggerClass::Array = nullptr;

// =============================================================================
// Local constants - tuning and safety limits.
// =============================================================================
namespace {
    // Maximum depth of linked-trigger cascades to prevent infinite loops.
    constexpr int32 MAX_LINK_DEPTH = 16;

    // Scenario difficulty constants (mirrors ScenarioClass::Difficulty values).
    constexpr int32 DIFFICULTY_EASY   = 0;
    constexpr int32 DIFFICULTY_NORMAL = 1;
    constexpr int32 DIFFICULTY_HARD   = 2;

    // -------------------------------------------------------------------------
    // DifficultyEnabled - returns true if the trigger should activate given the
    // current scenario difficulty.  If no difficulty flags are set the trigger
    // is treated as active on all difficulties.
    // -------------------------------------------------------------------------
    bool DifficultyEnabled(const TriggerClass* pTrigger) {
        if (!pTrigger) return false;
        if (!ScenarioClass::Instance) return true;

        int32 diff = ScenarioClass::Instance->Difficulty;
        switch (diff) {
            case DIFFICULTY_EASY:   return pTrigger->Easy;
            case DIFFICULTY_NORMAL: return pTrigger->Normal || pTrigger->Medium;
            case DIFFICULTY_HARD:   return pTrigger->Hard;
            default:                return true;
        }
    }

    // -------------------------------------------------------------------------
    // HouseIsActive - returns true if the house pointer is valid and the house
    // has not been defeated.  A null house is considered active (global trigger).
    // -------------------------------------------------------------------------
    bool HouseIsActive(HouseClass* pHouse) {
        if (!pHouse) return true;
        return !pHouse->IsDefeated;
    }

    // Forward declaration - defined after Fire() below.
    void FireLinkedChain(TriggerClass* pTrigger, int32 depth);
} // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================
TriggerClass::TriggerClass(const char* pID) noexcept
    : Type(nullptr), ID(nullptr), IsEnabled(true), TriggerAction(nullptr),
      Event(nullptr), CurrentAction(nullptr), House(nullptr), RelatedHouse(nullptr),
      Name(nullptr), Data(-1), HasBeenFired(false), JustFired(false),
      IsDisabled(false), IsBeingFired(false), IsLinked(false),
      Repeatable(false), Easy(false), Normal(false), Medium(false),
      Hard(false), LinkedTrigger(nullptr), LinkedAction(nullptr),
      Action(nullptr), forceFire(false), Activate(false),
      State(TriggerState::Armed), Destroyed(false), EventsAlreadyFired(0), Timer(0) {
    if (pID) {
        int32 len = static_cast<int32>(strlen(pID)) + 1;
        ID = new char[len];
        if (ID) {
            for (int32 i = 0; i < len; ++i) ID[i] = pID[i];
        }
    }
    // asm: TriggerClass_CTOR 尾部把实例登记进 vec_Triggers。不登记则
    //   ProcessTriggerEvents / GlobalUpdated / FireAllTriggersForEvent
    //   全部遍历不到它 —— 触发器会"存在但永不服务"。
    if (Array == nullptr) {
        Array = new DynamicVectorClass<TriggerClass*>();
    }
    Array->AddItem(this);
}

// -----------------------------------------------------------------------------
// asm: TriggerClass_CTOR(TriggerClass *this, TriggerTypeClass *ptrTrigType)
// Builds a runtime instance from a type template: the type pointer is stored in
// the +0x24 slot and the instance's public API caches are populated from it.
// -----------------------------------------------------------------------------
TriggerClass::TriggerClass(TriggerTypeClass* pType) noexcept
    : Type(pType), ID(nullptr), IsEnabled(true), TriggerAction(nullptr),
      Event(nullptr), CurrentAction(nullptr), House(nullptr), RelatedHouse(nullptr),
      Name(nullptr), Data(-1), HasBeenFired(false), JustFired(false),
      IsDisabled(false), IsBeingFired(false), IsLinked(false),
      Repeatable(false), Easy(false), Normal(false), Medium(false),
      Hard(false), LinkedTrigger(nullptr), LinkedAction(nullptr),
      Action(nullptr), forceFire(false), Activate(false),
      State(TriggerState::Armed), Destroyed(false), EventsAlreadyFired(0), Timer(0) {
    if (pType) {
        Event = pType->FirstEvent;
        TriggerAction = pType->FirstAction;
        Repeatable = pType->IsRepeatable;
        Easy = pType->Easy;
        Normal = pType->Normal;
        Hard = pType->Hard;
        const char* id = pType->get_ID();
        if (id) {
            int32 len = static_cast<int32>(strlen(id)) + 1;
            ID = new char[len];
            if (ID) {
                for (int32 i = 0; i < len; ++i) ID[i] = id[i];
            }
        }
    }
    Reset();
    if (Array == nullptr) {
        Array = new DynamicVectorClass<TriggerClass*>();
    }
    Array->AddItem(this);
}

// =============================================================================
// Destructor - release owned strings and detach from the global table.
//
// 注意：Event / TriggerAction 在 Type 驱动的实例里指向 TriggerTypeClass 的
//   EventList/ActionList 元素，所有权在类型对象上；这里**不能** GameDelete，
//   否则同一条触发链被释放两次（原实现无条件 GameDelete，接上 Type 后会崩）。
//   只有 Type == nullptr 的独立实例才拥有自己的 Event/TriggerAction。
// =============================================================================
TriggerClass::~TriggerClass() {
    if (Type == nullptr) {
        if (TriggerAction) { GameDelete(TriggerAction); TriggerAction = nullptr; }
        if (Event) { GameDelete(Event); Event = nullptr; }
    }
    TriggerAction = nullptr;
    Event = nullptr;
    CurrentAction = nullptr;
    if (ID) { delete[] ID; ID = nullptr; }
    if (Name) { delete[] Name; Name = nullptr; }

    if (Array != nullptr) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if (Array->GetItem(i) == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// =============================================================================
// Find - locate a trigger by its ID string (case-insensitive).
// =============================================================================
TriggerClass* TriggerClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* item = Array->GetItem(i);
        if (item && item->ID && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

// =============================================================================
// FindOrAllocate - find an existing trigger or create a new one.  Returns
// nullptr for the sentinel "<none>" / "none" IDs used in INI files.
// =============================================================================
TriggerClass* TriggerClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TriggerClass* found = Find(pID);
    if (found) return found;
    // 构造函数已把实例登记进 Array，这里不能再 Add 一次（会重复服务）。
    return GameCreate<TriggerClass>(pID);
}

// =============================================================================
// LoadFromINIList - 旧式 "一触发器一 section" 形态的解析器。
//
//   ⚠ 这个形态**不是** RA2/YR 的 [Triggers] 格式。RA2 的每一行固定是
//     HOUSE,LINKED_TRIGGER,NAME,DISABLED,EASY,NORMAL,HARD,TRANSFERABLE，
//     事件与动作另外放在 [Events]/[Actions] 里按同一 ID 索引 —— 那套解析
//     在 TriggerTypeClass::LoadFromINI / CreateFromINIList（这是真正的
//     加载入口，已在 Game/WinMain.cpp 的 2.8 段接线）。
//
//   保留本函数只为兼容早期内部格式与往返测试；**不要**在场景加载路径上
//   调它，否则会把 "Name=" / "Event=" 之类的键当成触发器主体。
// =============================================================================
bool TriggerClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    if (!ID) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);
    if (!pINI->SectionExists(sectionName)) return false;

    char buffer[256];

    // --- Name ---
    pINI->ReadString(sectionName, "Name", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        if (Name) { delete[] Name; Name = nullptr; }
        int32 len = static_cast<int32>(strlen(buffer)) + 1;
        Name = new char[len];
        if (Name) {
            for (int32 i = 0; i < len; ++i) Name[i] = buffer[i];
        }
    }

    // --- Difficulty flags ---
    Easy   = pINI->ReadBool(sectionName, "Easy", Easy);
    Normal = pINI->ReadBool(sectionName, "Normal", Normal);
    Medium = pINI->ReadBool(sectionName, "Medium", Medium);
    Hard   = pINI->ReadBool(sectionName, "Hard", Hard);

    // If no difficulty flags are set, default to all difficulties enabled so
    // the trigger is not silently inert.
    if (!Easy && !Normal && !Medium && !Hard) {
        Easy = Normal = Medium = Hard = true;
    }

    // --- Event reference ---
    char eventId[32];
    pINI->ReadString(sectionName, "Event", "", eventId, sizeof(eventId));
    if (eventId[0] && _strcmpi(eventId, "<none>") != 0) {
        Event = TEventClass::FindOrAllocate(eventId);
    }

    // --- Action reference ---
    char actionId[32];
    pINI->ReadString(sectionName, "Action", "", actionId, sizeof(actionId));
    if (actionId[0] && _strcmpi(actionId, "<none>") != 0) {
        TriggerAction = TActionClass::FindOrAllocate(actionId);
    }

    // --- House ownership ---
    char houseId[32];
    pINI->ReadString(sectionName, "House", "", houseId, sizeof(houseId));
    if (houseId[0] && _strcmpi(houseId, "<none>") != 0) {
        for (int32 i = 0; i < 32; ++i) {
            if (HouseClass::Array[i]) {
                const char* pHouseName = HouseClass::Array[i]->Type->get_ID();
                if (pHouseName && !_strcmpi(pHouseName, houseId)) {
                    House = HouseClass::Array[i];
                    break;
                }
            }
        }
    }

    // --- Linked trigger ---
    char linkedId[32];
    if (linkedId[0] && _strcmpi(linkedId, "<none>") != 0) {
        LinkedTrigger = TriggerClass::FindOrAllocate(linkedId);
        if (LinkedTrigger) IsLinked = true;
    }

    // --- Numeric data ---

    // --- Optional initial timer (delay before first evaluation) ---
    int32 iniTimer = 0;
    if (iniTimer > 0) {
        Timer = iniTimer;
        State = TriggerState::Waiting;
    }

    // --- Optional Disabled flag (start disabled) ---
    bool startDisabled = pINI->ReadBool(sectionName, "Disabled", false);
    if (startDisabled) {
        Disable();
    }

    return true;
}

// =============================================================================
// SaveToINIList - serialise the trigger configuration back to INI.
// =============================================================================
bool TriggerClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return false;
    if (!ID) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", ID);

    pINI->WriteString(sectionName, "Name", Name ? Name : "");
    pINI->WriteBool(sectionName, "Repeatable", Repeatable);
    pINI->WriteBool(sectionName, "Easy", Easy);
    pINI->WriteBool(sectionName, "Normal", Normal);
    pINI->WriteBool(sectionName, "Medium", Medium);
    pINI->WriteBool(sectionName, "Hard", Hard);

    if (Event) pINI->WriteString(sectionName, "Event", Event->ID);
    else pINI->WriteString(sectionName, "Event", "<none>");

    if (TriggerAction) pINI->WriteString(sectionName, "Action", TriggerAction->ID);
    else pINI->WriteString(sectionName, "Action", "<none>");

    if (House) pINI->WriteString(sectionName, "House", House->Type->get_ID());
    else pINI->WriteString(sectionName, "House", "<none>");

    if (LinkedTrigger) pINI->WriteString(sectionName, "LinkedTrigger", LinkedTrigger->ID);
    else pINI->WriteString(sectionName, "LinkedTrigger", "<none>");

    pINI->WriteInteger(sectionName, "Data", Data);

    if (Timer > 0) {
        pINI->WriteInteger(sectionName, "Timer", Timer);
    }

    return true;
}

// =============================================================================
// Update - per-frame service pass.
//
// asm 中触发器的"条件判定 + 动作执行"完全由 TagClass_RaiseEvent 驱动（见
// TagClass::RaiseEvent），不存在"每帧自查条件再开火"的路径。本函数因此只承担
// 每帧服务职责：脉冲标志复位、Waiting 计时器递减、到点后重新武装。
// =============================================================================
void TriggerClass::Update() {
    if (JustFired) {
        JustFired = false;
    }

    // 已"原地消失"的触发器不再服务。
    if (Destroyed) return;

    if (State == TriggerState::Waiting) {
        if (Timer > 0) {
            --Timer;
            if (Timer > 0) return;
        }
        // 计时器归零 —— 重新武装（asm Reset 会把 enabled 复位为 1）。
        State = TriggerState::Armed;
        IsEnabled = true;
        IsDisabled = false;
    }
}

// =============================================================================
// CheckConditions - evaluate the trigger's event condition plus the force-fire
// and one-shot activate flags.
// =============================================================================
bool TriggerClass::CheckConditions() {
    // Primary: the attached event reports satisfaction.
    if (Event && Event->IsSatisfied(this)) {
        return true;
    }

    // Forced fire (set via ForceFire or SetEnabled with a force flag).
    if (forceFire) {
        forceFire = false;
        return true;
    }

    // One-shot activation flag used by external systems.
    if (Activate && !HasBeenFired) {
        Activate = false;
        return true;
    }

    return false;
}

// =============================================================================
// Fire - execute the trigger's action and cascade to linked triggers.
//
// The linked-trigger cascade is depth-limited to MAX_LINK_DEPTH to prevent
// infinite loops in cyclic trigger graphs.
// =============================================================================
void TriggerClass::Fire() {
    if (HasBeenFired && !Repeatable) return;

    IsBeingFired = true;

    // Execute the primary action.
    if (TriggerAction) {
        TriggerAction->ExecuteAction(this);
    }

    HasBeenFired = true;
    JustFired = true;
    IsBeingFired = false;

    // Cascade to the linked trigger (if any) with depth limiting.
    if (LinkedTrigger && IsLinked) {
        FireLinkedChain(LinkedTrigger, 1);
    }

    // Non-repeatable triggers disable themselves after firing.
    if (!Repeatable) {
        IsEnabled = false;
        State = TriggerState::Fired;
    } else {
        // Repeatable triggers re-arm for the next occurrence.
        State = TriggerState::Armed;
    }
}

// =============================================================================
// FireLinkedChain - recursively fire linked triggers up to a maximum depth.
// This is a file-local helper used by Fire().
// =============================================================================
namespace {
    void FireLinkedChain(TriggerClass* pTrigger, int32 depth) {
        if (!pTrigger) return;
        if (depth > MAX_LINK_DEPTH) return;

        // Respect the linked trigger's own repeatable/already-fired rules.
        if (pTrigger->HasBeenFired && !pTrigger->Repeatable) return;
        if (!pTrigger->IsEnabled || pTrigger->IsDisabled) return;

        if (pTrigger->TriggerAction) {
            pTrigger->TriggerAction->ExecuteAction(pTrigger);
        }
        pTrigger->HasBeenFired = true;
        pTrigger->JustFired = true;

        if (!pTrigger->Repeatable) {
            pTrigger->IsEnabled = false;
            pTrigger->State = TriggerState::Fired;
        }

        // Recurse into the next link.
        if (pTrigger->LinkedTrigger && pTrigger->IsLinked) {
            FireLinkedChain(pTrigger->LinkedTrigger, depth + 1);
        }
    }
} // anonymous namespace

// =============================================================================
// asm: TriggerClass_Enable (asm:0x726930) —— enabled(+0x44)=1 后直接跳 Reset。
// =============================================================================
void TriggerClass::Enable() {
    IsEnabled = true;
    IsDisabled = false;
    Reset();
}

// =============================================================================
// asm: TriggerClass_Disable (asm:0x726940) —— 仅把 enabled(+0x44) 清 0。
// =============================================================================
void TriggerClass::Disable() {
    IsEnabled = false;
    IsDisabled = true;
    State = TriggerState::Disabled;
}

// =============================================================================
// Spring - respond to an external event notification (e.g. a unit entering a
// cell, a building being destroyed).  If the trigger's event matches, fire.
// =============================================================================
void TriggerClass::Spring(TriggerEventType eventType, AbstractClass* pObject, CellStruct cell) {
    if (!IsEnabled || IsDisabled) return;
    if (HasBeenFired && !Repeatable) return;
    if (!DifficultyEnabled(this)) return;
    if (!HouseIsActive(House)) return;

    if (State == TriggerState::Waiting) return;

    if (Event && Event->MatchEvent(eventType, pObject, cell, this)) {
        Fire();
    }
}

// =============================================================================
// ForceFire - schedule the trigger to fire on the next Update().
// =============================================================================
void TriggerClass::ForceFire() {
    forceFire = true;
    // Make sure the trigger is in a state where Update() will honour the flag.
    if (State == TriggerState::Disabled) {
        Enable();
    }
}

// =============================================================================
// SetEnabled - convenience wrapper that calls Enable or Disable.
// =============================================================================
void TriggerClass::SetEnabled(bool enabled) {
    if (enabled) {
        Enable();
    } else {
        Disable();
    }
}

// =============================================================================
// IsSatisfied - check whether the trigger's conditions are currently met.
// =============================================================================
bool TriggerClass::IsSatisfied() {
    if (!IsEnabled || IsDisabled) return false;
    if (HasBeenFired && !Repeatable) return false;
    if (!DifficultyEnabled(this)) return false;
    return CheckConditions();
}

// =============================================================================
// IsSatisfied - house-filtered variant: only returns true if the trigger
// belongs to (or is global for) the specified house.
// =============================================================================
bool TriggerClass::IsSatisfied(HouseClass* pHouse) {
    if (!pHouse) return false;
    // If the trigger has a specific house and it does not match, it is not
    // satisfied for the queried house.
    if (House && House != pHouse) return false;
    if (!IsEnabled || IsDisabled) return false;
    if (HasBeenFired && !Repeatable) return false;
    if (!DifficultyEnabled(this)) return false;
    return CheckConditions();
}

// =============================================================================
// SetEvent - attach an event object to this trigger.
// =============================================================================
void TriggerClass::SetEvent(TEventClass* pEvent) {
    Event = pEvent;
}

// =============================================================================
// SetAction - attach an action object to this trigger.
// =============================================================================
void TriggerClass::SetAction(TActionClass* pAction) {
    TriggerAction = pAction;
}

// =============================================================================
// SetHouse - set the owning house for this trigger.
// =============================================================================
void TriggerClass::SetHouse(HouseClass* pHouse) {
    House = pHouse;
}

// =============================================================================
// SetLinkedTrigger - establish a link to another trigger.  When this trigger
// fires, the linked trigger also fires (subject to its own enable rules).
// =============================================================================
void TriggerClass::SetLinkedTrigger(TriggerClass* pTrigger) {
    LinkedTrigger = pTrigger;
    IsLinked = (pTrigger != nullptr);
}

// =============================================================================
// SetData - store an arbitrary integer data value used by the action.
// =============================================================================
void TriggerClass::SetData(int32 data) {
    Data = data;
}

// =============================================================================
// SetTimer - arm a delay timer.  The trigger enters the Waiting state and will
// not evaluate conditions until the timer counts down to zero.
// =============================================================================
void TriggerClass::SetTimer(int32 frames) {
    Timer = frames;
    if (frames > 0) {
        State = TriggerState::Waiting;
    } else {
        // A zero or negative timer immediately re-arms.
        State = TriggerState::Armed;
    }
}

// =============================================================================
// ProcessTriggerEvents - static per-frame service entry point.  遍历所有触发器
// 执行每帧服务（Waiting 计时器递减与重新武装）。触发器的"条件判定 + 动作执行"
// 由 TagClass::ProcessTagEvents → TagClass::RaiseEvent 驱动，见 asm L751177+。
// =============================================================================
void TriggerClass::ProcessTriggerEvents() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* trigger = Array->GetItem(i);
        if (trigger) trigger->Update();
    }
}

// =============================================================================
// ResetAllTriggers - reset every loaded trigger to its initial armed state.
// Typically called at scenario start.
// =============================================================================
void TriggerClass::ResetAllTriggers() {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* trigger = Array->GetItem(i);
        if (trigger) trigger->Reset();
    }
}

// =============================================================================
// FireAllTriggersForEvent - broadcast an external event to every trigger.
// Used by game systems (e.g. combat, discovery) to notify all triggers of a
// world event in a single pass.
// =============================================================================
void TriggerClass::FireAllTriggersForEvent(TriggerEventType eventType,
                                           AbstractClass* pObject,
                                           CellStruct cell) {
    if (!Array) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* trigger = Array->GetItem(i);
        if (trigger) trigger->Spring(eventType, pObject, cell);
    }
}

// ============================================================================
// TriggerClass - related-house accessors (asm 0x726910 / 0x726920)
// ============================================================================
HouseClass* TriggerClass::FindRelatedHouse() const
{
    return RelatedHouse;
}

void TriggerClass::SetRelatedHouse(HouseClass* pHouse)
{
    RelatedHouse = pHouse;
}

// ============================================================================
// asm 0x726350 — TriggerClass::Reset
//
// 沿类型的事件链（Type->EventList，对应 asm +0xAC 的 FirstEvent 链）逐个检查
// 事件种类：
//   kind == 0x0D (TimeElapsed)  —— CreationTimer 起点设为 Frame、时长 Data*15；
//   kind == 0x33 (随机延迟事件) —— 时长 = (Data/2 + Random(0,Data)) * 15；
// 命中上述两类时，清掉该事件在 EventsAlreadyFired 里的"已发生"位。
// 另外把本实例自身的运行状态复位为"武装"。
// ============================================================================
void TriggerClass::Reset()
{
    HasBeenFired = false;
    JustFired = false;
    IsEnabled = true;
    IsDisabled = false;
    IsBeingFired = false;
    forceFire = false;
    Activate = false;
    State = TriggerState::Armed;
    Timer = 0;
    Destroyed = false;

    if (Type == nullptr) return;

    int32 bit = 0;
    for (int32 i = 0; i < Type->EventList.Count; ++i, ++bit) {
        TEventClass* pEvent = Type->EventList.Items[i];
        if (pEvent == nullptr) continue;
        const int32 kind = pEvent->EventKind;
        if (kind == 0x0D) {
            // TimeElapsed: 时长按"秒 -> 帧"换算（asm: Data*3*5 = Data*15）。
            Timer = pEvent->Data * 15;
        } else if (kind == 0x33) {
            // Randomized delay: 半程 + 随机抖动，再换算成帧（asm: *15）。
            const int32 span = pEvent->Data;
            const int32 rnd = (span > 0 && TheScenario != nullptr)
                ? TheScenario->Random.Next(0, span)
                : 0;
            Timer = (span / 2 + rnd) * 15;
        } else {
            // 其余事件种类不触碰 CreationTimer / 已发生位（asm 仅在命中分支 and-not）。
            continue;
        }
        EventsAlreadyFired &= ~(1 << bit);
    }
}

// asm: void __thiscall TriggerClass_CheckGlobals(TriggerClass *this, int a2)   (asm:1645728)
// 从本实例起沿 +0x28（LinkedTrigger，即挂载链的后继）逐级检查：任一实例的类型
// 事件链引用了该全局变量，则把该实例 Reset。返回是否发生过重置。
bool TriggerClass::CheckGlobals(int32 idx)
{
    bool anyReset = false;
    for (TriggerClass* p = this; p != nullptr; p = p->LinkedTrigger) {
        if (p->Type != nullptr && p->Type->InvolvesGlobalChecking(idx)) {
            p->Reset();
            anyReset = true;
        }
    }
    return anyReset;
}

// asm: void __thiscall TriggerClass_CheckLocals(TriggerClass *this, int House_)   (asm:1645774)
// 与 CheckGlobals 同构，判定依据换成"本地变量是否被引用"（house 下标参数）。
bool TriggerClass::CheckLocals(HouseClass* pHouse)
{
    const int32 houseIdx = (pHouse != nullptr) ? pHouse->GetArrayIndex() : -1;
    bool anyReset = false;
    for (TriggerClass* p = this; p != nullptr; p = p->LinkedTrigger) {
        if (p->Type != nullptr && p->Type->CheckLocals(houseIdx)) {
            p->Reset();
            anyReset = true;
        }
    }
    return anyReset;
}

// asm: char __fastcall TriggerClass_CrossHorizontalZone (0x726F80) — 事件种类 == 0x19
bool TriggerClass::CrossHorizontalZone() const
{
    return Type != nullptr && Type->CrossHorizontalZone();
}

// asm: char __fastcall TriggerTypeClass_CrossVerticalZone (0x726F50) — 事件种类 == 0x1A
bool TriggerClass::CrossVerticalZone() const
{
    return Type != nullptr && Type->CrossVerticalZone();
}

// asm: byte __thiscall _TriggerClass_IsWPZoneEntry (0x726FB0) — 事件种类 == 0x18
bool TriggerClass::IsWPZoneEntry() const
{
    return Type != nullptr && Type->IsWPZoneEntry();
}

// Legacy zone-parameterised overloads: a zone matches when the trigger's first
// event parameter equals the supplied zone.  Kept for TagClass compatibility.
bool TriggerClass::CrossHorizontalZone(int32 zone) const
{
    return Event != nullptr && Event->Data == zone;
}

bool TriggerClass::CrossVerticalZone(int32 zone) const
{
    return Event != nullptr && Event->Data == zone;
}

bool TriggerClass::CrossedVertical(int32 zone) const
{
    return Event != nullptr && Event->Data == zone;
}

// asm: char __thiscall TriggerClass_CrossedHorizontal (0x726256)
// If there is a linked trigger, check this instance's horizontal-zone event and
// recurse into the linked trigger; otherwise just test this instance.
bool TriggerClass::CrossedHorizontal() const
{
    if (LinkedTrigger != nullptr) {
        return CrossHorizontalZone() || LinkedTrigger->CrossedHorizontal();
    }
    return CrossHorizontalZone();
}

// asm: byte __thiscall TriggerClass_CrossedVertical (0x7262AE)
bool TriggerClass::CrossedVertical() const
{
    if (LinkedTrigger != nullptr) {
        return CrossVerticalZone() || LinkedTrigger->CrossedVertical();
    }
    return CrossVerticalZone();
}

// asm: byte __thiscall TriggerClass_FireActions(TriggerClass *this, _ObjectClass *Obj, wXY pos)   (asm:1646071)
//
// 若启用（+0x44）且未销毁（+0x30），沿类型的动作链（asm +0xB0 FirstAction，其
// 后继挂在每个动作的 +0x28）逐个执行；每个动作的关联方由
// TriggerTypeClass.House 解析（asm 中经 HouseClass_FindByIndex_NoMP），
// OpenYR 侧由 TActionClass::ExecuteAction 内部解析。
// 返回是否有任一动作被真正应用。
bool TriggerClass::FireActions(AbstractClass* pObject, CellStruct pos)
{
    (void)pObject;      // 动作的对象实参由 TActionClass 侧按需从 Trigger 取用。
    (void)pos;

    // asm: [edi+44h](enabled)==0 -> return 0; [edi+30h](Destroyed)!=0 -> return 0
    if (!IsEnabled || Destroyed) return false;
    if (Type == nullptr) return false;

    bool fired = false;
    IsBeingFired = true;

    static const bool s_TrigDbg = (std::getenv("OPENYR_TRIGDBG") != nullptr);
    if (s_TrigDbg) {
        std::fprintf(stderr, "[TRIGFIRE] frame=%d this=%p id=%s type=%p actions=%d destroyed=%d\n",
                     (int)Game::CurrentFrame, (void*)this,
                     (ID != nullptr) ? ID : "?", (void*)Type,
                     (int)Type->ActionList.Count, Destroyed ? 1 : 0);
    }

    // OpenYR 把 asm 的单链表 +0xB0 存成动态数组 ActionList（顺序一致）。
    for (int32 i = 0; i < Type->ActionList.Count; ++i) {
        TActionClass* pAction = Type->ActionList.Items[i];
        if (pAction == nullptr) continue;
        CurrentAction = pAction;
        pAction->ExecuteAction(this);
        fired = true;       // asm: 动作返回真则置 bl=1
    }

    CurrentAction = nullptr;
    IsBeingFired = false;
    return fired;
}

// asm: int __fastcall TriggerClass_GlobalUpdated(int idx)   (asm:1519466)
// 全局变量变化：对所有触发器广播复检（asm 遍历 vec_Tags_all 的每条挂载链，
// 对链上每个实例调用 CheckGlobals；OpenYR 用扁平 TriggerClass::Array 等价承接）。
void TriggerClass::GlobalUpdated(int32 idx)
{
    if (Array == nullptr) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* pTrigger = Array->GetItem(i);
        if (pTrigger != nullptr) {
            pTrigger->CheckGlobals(idx);
        }
    }
}

// asm: int __fastcall TriggerClass_LocalUpdated(int idx)   (asm:1519504)
void TriggerClass::LocalUpdated(int32 idx)
{
    if (Array == nullptr) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TriggerClass* pTrigger = Array->GetItem(i);
        if (pTrigger != nullptr) {
            HouseClass* pHouse = HouseClass::Find_By_Index_No_MP(idx);
            pTrigger->CheckLocals(pHouse);
        }
    }
}

// asm: char __thiscall TriggerClass_HaveAllEventsOccured(TriggerClass *this, int eventKind,
//        _ObjectClass *Object, char a4, char isRepeating, int a6)   (asm:1645919, retn 14h)
//
// 逐个检查类型事件链；EventsAlreadyFired 中已置位的事件直接跳过。
//   assumeSatisfied (asm a4) 非零 → 跳过全部逐事件判定，直接返回真。
//   每个事件调用 EventClass_HasOccured；返回真时把事件解析出的关联方写入
//   RelatedHouse(+0x2C)，并在 eventRepeating 为真时置该事件的"已发生"位。
// 全部事件命中且 repeatingType 为真时调用 Reset()。
bool TriggerClass::HaveAllEventsOccured(int32 eventKind, AbstractClass* pObject,
                                        bool assumeSatisfied, bool repeatingType)
{
    // asm: enabled(+0x44)==0 或 Destroyed(+0x30)!=0 → 直接假。
    if (!IsEnabled || Destroyed) return false;

    bool result = true;

    if (!assumeSatisfied) {
        if (Type != nullptr) {
            // 事件关联方：asm 由 TriggerTypeClass.House -> 国家下标 -> HouseClass 解析。
            HouseClass* pHouse = nullptr;
            if (Type->Owner != nullptr && HouseClass::Array != nullptr) {
                for (int32 h = 0; h < HouseClass::ArrayCount; ++h) {
                    HouseClass* pH = HouseClass::Array[h];
                    if (pH != nullptr && pH->Type == Type->Owner) { pHouse = pH; break; }
                }
            }
            if (pHouse == nullptr) pHouse = House;

            int32 bit = 0;
            for (int32 i = 0; i < Type->EventList.Count; ++i, ++bit) {
                TEventClass* pEvent = Type->EventList.Items[i];
                if (pEvent == nullptr) continue;

                const int32 mask = 1 << bit;
                if (EventsAlreadyFired & mask) {
                    continue;       // 已发生的事件跳过
                }

                bool eventRepeating = repeatingType;
                if (!pEvent->Evaluate(eventKind, pHouse, pObject, this, &eventRepeating)) {
                    result = false;
                    break;
                }

                // asm: EventClass_GetHouse 非空则写回 RelatedHouse(+0x2C)。
                if (pEvent->House != nullptr) {
                    RelatedHouse = pEvent->House;
                }
                // asm: flags 置位（GetEventFlags_ / GetEventFlags__ 合格）则或上该位。
                if (eventRepeating) {
                    EventsAlreadyFired |= mask;
                }
            }
        } else if (Event != nullptr) {
            bool eventRepeating = repeatingType;
            result = Event->Evaluate(eventKind, House, pObject, this, &eventRepeating);
        }
    }

    // asm: 结果非零且 isRepeating 为真 → Reset() 重新武装。
    if (result && repeatingType) {
        Reset();
    }
    return result;
}

// asm: TriggerClass_InvolvesAllowWin (0x7265F6) — 本实例与后继实例的类型动作链
// 任一含 "允许胜利" 动作即为真。
bool TriggerClass::InvolvesAllowWin() const
{
    if (Type != nullptr && Type->InvolvesAllowWin()) return true;
    if (LinkedTrigger != nullptr) return LinkedTrigger->InvolvesAllowWin();
    return false;
}

// asm: int __thiscall TriggerClass_Poof(TriggerClass *this)   (asm:1646286)
// 置 Destroyed(+0x30)=1，随后把本体加入 vec_VisibleObjects 待统一回收。
// OpenYR 无该回收表，额外把 enabled 清掉避免已"消失"的触发器继续被上抛。
void TriggerClass::Poof()
{
    Destroyed = true;
    IsEnabled = false;
    State = TriggerState::Disabled;
}

// asm: sub_726780 — returns the +0x30 "Destroyed" flag.
bool TriggerClass::IsDestroyed() const
{
    return Destroyed;
}

// ============================================================================
// asm 0x7266D0 — TriggerClass_PointerGotInvalid(TriggerClass*, AbstractClass*, ...)
// retn 8 → one explicit argument.  Unified signature (see briefing §3).
//   +0x24 type    -> cleared when invalidated
//   +0x28 linked  -> advanced to the linked trigger's own successor
//   +0x2C house   -> cleared when invalidated
// ============================================================================
void TriggerClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;
    // asm: +0x24 type 命中则清 0。
    if (reinterpret_cast<AbstractClass*>(Type) == pInvalid) {
        Type = nullptr;
    }
    // asm: +0x28 AttachedTrigger 命中则前移到链中下一个。
    if (reinterpret_cast<AbstractClass*>(LinkedTrigger) == pInvalid
        && LinkedTrigger != nullptr) {
        LinkedTrigger = LinkedTrigger->LinkedTrigger;
    }
    // asm: +0x2C related house 命中则清 0。
    if (reinterpret_cast<AbstractClass*>(RelatedHouse) == pInvalid) {
        RelatedHouse = nullptr;
    }
}

// ============================================================================
// asm 0x726740 — TriggerClass_GetCRC
// 基类 CRC 后并入：类型 ID、链后继的类型 ID、Destroyed、enabled、（按当前帧
// 折算出的）剩余时间、EventsAlreadyFired。
// ============================================================================
int32 TriggerClass::GetCRC() const
{
    CRCEngine crc;

    if (Type != nullptr) {
        const char* id = Type->get_ID();
        if (id != nullptr) {
            crc.AddData(id, static_cast<int32>(strlen(id)) + 1);
        }
    }
    if (LinkedTrigger != nullptr && LinkedTrigger->Type != nullptr) {
        const char* id = LinkedTrigger->Type->get_ID();
        if (id != nullptr) {
            crc.AddData(id, static_cast<int32>(strlen(id)) + 1);
        }
    }

    crc.AddData(&Destroyed, sizeof(Destroyed));
    crc.AddData(&IsEnabled, sizeof(IsEnabled));

    int32 remaining = Timer;
    crc.AddData(&remaining, sizeof(remaining));
    crc.AddData(&EventsAlreadyFired, sizeof(EventsAlreadyFired));
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// asm 0x726758 — TriggerClass_GetClassID
// ============================================================================
HRESULT TriggerClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr) return E_POINTER;
    pClassID->Data1 = 0xC02D1590;
    pClassID->Data2 = 0x0A2A;
    pClassID->Data3 = 0x11D2;
    static const uint8 tail[8] = {0xAC, 0xA7, 0x00, 0x60, 0x08, 0x05, 0x5B, 0xB5};
    for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = tail[i];
    return S_OK;
}

// ============================================================================
// asm 0x726894 — TriggerClass_Load
// 基类反序列化 + LoadTables 之后，对三个指针槽做 swizzle 还原：
//   +0x24 triggerType / +0x28 AttachedTrigger / +0x2C related house。
// ============================================================================
HRESULT TriggerClass::Load(IStream* pStm)
{
    if (pStm == nullptr) return E_POINTER;

    static SwizzleManagerClass SwizzleManager;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Type));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&LinkedTrigger));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&RelatedHouse));
    return S_OK;
}

// ============================================================================
// asm 0x7268D0 — TriggerClass_Save —— 仅走基类保存，本层无额外持久化字段。
// ============================================================================
HRESULT TriggerClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr) return E_POINTER;
    if (fClearDirty) {
        // 基类 AbstractClass::Save 会清 Dirty；此处等价承接。
    }
    return S_OK;
}

// ============================================================================
// asm 0x7268D0 — TriggerClass_GetSize (0x48)
// ============================================================================
int32 TriggerClass::Size() const
{
    return 0x48;
}

// ============================================================================
// asm 0x7268D8 — TriggerClass_GetAbstractDerivationID (0x26)
// ============================================================================
int32 TriggerClass::GetAbstractDerivationID() const
{
    return 0x26;
}

