#include "TagClass.h"
#include "TriggerClass.h"
#include "TriggerTypeClass.h"
#include "TagTypeClass.h"
#include "../Abstract/AbstractClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Houses/HouseClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../INI/INIClass.h"
#include "../Math/Timer.h"

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

// asm: vec_Tags_0x4 —— 与 vec_Tags_0x10 平行的第二张标签实例表（同一批
//   TagClass*，incrementAmount 同为 0Ah）。FootClass_UpdatePosition 跨路点
//   区域时遍历它问 IsWPZoneEntry；读档时两张表各自按 length 回填指针。
DynamicVectorClass<TagClass*>* TagClass::Array4 = nullptr;

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
    : AbstractClass(noinit), IsInitialized(true)
    , TagType(nullptr), AttachedTrigger(nullptr), field_2C(0)
    , DefaultCoords{0, 0}, field_34(0), IsExecuting(false) {
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
    // asm: TagClass.AttachedTrigger(+0x28) 是链头，指向第一个挂载触发器。
    AttachedTrigger = TriggerList.Items[0];
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
            AttachedTrigger = (TriggerList.Count > 0) ? TriggerList.Items[0] : nullptr;
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
    AttachedTrigger = nullptr;
}

// ----------------------------------------------------------------------------
// asm: LogicClass_Update 的 Tag 段（L751177…L751336）—— 每帧对单个标签上抛
// 全局/世界事件族。条件来自 Scenario 的变更标志，顺序与 asm 完全一致：
//   34BE(拾取木箱) → 50
//   34AA(VariablesChanged) → 27,28,36,37
//   34AB(AmbientChanged)   → 45,46
//   恒上抛 13,51；任务计时器到点再上抛 14
// 一旦某条事件被某个挂载触发器接住（RaiseEvent 返回真），本标签本帧即停止
// 后续上抛（asm: `test al,al / jnz loc_55B15E`）。
// ----------------------------------------------------------------------------
void TagClass::SpringAll() {
    ScenarioClass* pScen = ScenarioClass::Instance;
    if (pScen == nullptr) return;

    const CellStruct loc = DefaultCoords;

    // 小助手：把一次上抛包成 bool。loc/TagObject/source 沿用 asm 的实参形态。
    auto Raise = [&](int32 kind) -> bool {
        return RaiseEvent(kind, nullptr, loc, false, nullptr);
    };

    if (pScen->PickedUpAnyCrate) {          // asm: ScenarioClass+0x34BE
        if (Raise(50)) return;
    }
    if (pScen->VariablesChanged) {          // asm: ScenarioClass+0x34AA
        if (Raise(27)) return;
        if (Raise(28)) return;
        if (Raise(36)) return;
        if (Raise(37)) return;
    }
    if (pScen->AmbientChanged) {            // asm: ScenarioClass+0x34AB
        if (Raise(45)) return;
        if (Raise(46)) return;
    }
    if (Raise(13)) return;                  // TimeElapsed（恒上抛）
    if (Raise(51)) return;                  // ScenarioTimeElapsed（恒上抛）

    // 任务计时器到点则上抛 14（asm: Scenario+0x11E8 计时器结构）。
    const CDTimerClass& timer = pScen->MissionTimer;
    if (timer.StartTime != -1) {
        const int32 elapsed = Game::CurrentFrame - timer.StartTime;
        if (elapsed >= timer.TimeLeft) {
            Raise(14);
        }
    }
}

// ----------------------------------------------------------------------------
// asm: LogicClass_Update+0x32 的每帧循环体（vec_Tags_0x10）。
// 遍历全部标签，逐个 SpringAll。这是原版"触发/事件每帧驱动"的正式入口。
// ----------------------------------------------------------------------------
void TagClass::ProcessTagEvents() {
    if (Array == nullptr) return;
    for (int32 i = 0; i < Array->Count; ++i) {
        TagClass* pTag = Array->GetItem(i);
        if (pTag != nullptr) {
            pTag->SpringAll();
        }
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

// ------------------------------------------------------------------------
// asm: TagClass_GetCRC (asm:0x1518836)
// 基类 CRC 之后并入：类型 ID、挂载触发器首节点的 ID、field_34 标志。
// ------------------------------------------------------------------------
int32 TagClass::GetCRC() const
{
    CRCEngine crc;
    Compute_CRC(crc);

    if (TagType != nullptr) {
        const char* id = TagType->get_ID();
        if (id != nullptr) {
            crc.AddData(id, static_cast<int32>(std::strlen(id)) + 1);
        }
    }
    if (AttachedTrigger != nullptr && AttachedTrigger->Type != nullptr) {
        const char* id = AttachedTrigger->Type->get_ID();
        if (id != nullptr) {
            crc.AddData(id, static_cast<int32>(std::strlen(id)) + 1);
        }
    }
    int32 flag = field_34;
    crc.AddData(&flag, sizeof(flag));
    return static_cast<int32>(crc.GetCRC());
}

// asm: TagClass_GetSize —— 固定结构大小 0x38。
int32 TagClass::GetSize() const
{
    return 0x38;
}

// asm: TagClass_GetAbstractDerivationID —— 0x2C。
int32 TagClass::GetAbstractDerivationID() const
{
    return 0x2C;
}

// ------------------------------------------------------------------------
// asm: char __fastcall TagClass_IsTriggerRepeating(TagClass *this)   (asm:1519429)
// 沿挂载触发器链逐个询问其类型（TriggerClass+0x24 -> TriggerTypeClass+0xA0）
// 是否可重复触发，任一为真即返回真。
// ------------------------------------------------------------------------
bool TagClass::IsTriggerRepeating() const
{
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        TriggerClass* pTrigger = TriggerList.Items[i];
        if (pTrigger != nullptr && pTrigger->Type != nullptr && pTrigger->Type->IsRepeatable)
            return true;
    }
    return false;
}

// asm: byte __fastcall TagClass_CrossVertical(TagClass *a1)   (asm:0x6E5300)
// 直接把询问转交给本标签的类型（TagType->CrossVerticalZone）。
bool TagClass::CrossVertical(int32 zone)
{
    (void)zone;     // asm 只取 this，区域值由类型侧决定。
    return TagType != nullptr && TagType->CrossVerticalZone();
}

// asm: byte __thiscall TagClass_CrossHorizontal(TagClass *this)   (asm:0x6E5320)
bool TagClass::CrossHorizontal(int32 zone)
{
    (void)zone;
    return TagType != nullptr && TagType->CrossHorizontalZone();
}

// asm: TagClass_IsWPZoneEntry (asm:0x6E5340)
bool TagClass::IsWPZoneEntry() const
{
    return TagType != nullptr && TagType->IsWPZoneEntry();
}

// asm: byte __fastcall TagClass_DoesAllowWin(TagClass *a1)   (asm:1518741)
bool TagClass::DoesAllowWin() const
{
    return TagType != nullptr && TagType->ContainsAllowWin();
}

// asm: char __thiscall TagClass_IsAttachedToTrigger(TagClass *this,
//        TriggerClass *targetTrigger)   (asm:0x6E5370)
// 沿挂载触发器链逐个比较实例指针，命中即真。
bool TagClass::IsAttachedToTrigger(TriggerTypeClass* pTargetTrigger) const
{
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        TriggerClass* pT = TriggerList.Items[i];
        if (pT != nullptr && pT->Type == pTargetTrigger)
            return true;
    }
    return false;
}

bool TagClass::IsAttachedToTrigger(TriggerClass* pTrigger) const
{
    for (int32 i = 0; i < TriggerList.Count; ++i) {
        if (TriggerList.Items[i] == pTrigger)
            return true;
    }
    return false;
}

// ------------------------------------------------------------------------
// asm: char __thiscall TagClass_RaiseEvent(TagClass *this, eEventKind eventKind,
//        _ObjectClass *TagObject, wXY loc, char isRepeating, _TechnoClass *source)
//        (asm:1518810, retn 14h)
//
// 每帧/每次世界事件的核心上抛入口。按 asm 控制流：
//   1) Debug_Map / IsExecuting(+0x35) / field_34(+0x34) / TagType(+0x24) 任一
//      为真/空则直接返回 false。
//   2) IsExecuting = true，沿挂载触发器链（AttachedTrigger，即 TriggerList）
//      逐个调用 HaveAllEventsOccured；事件全中时按 TagType->Repeating 分派：
//        Repeating==0 → FireActions + Poof，timeToDie=1，置 doTagReplacement
//        Repeating==1 → field_2C==1 时 FireActions+Poof，否则仅置 doTagReplacement
//        Repeating==2 → 仅 FireActions（可重复，不消失）
//        Repeating>2 → 不执行
//   3) IsExecuting = false；doTagReplacement 时清掉对象的 Tag 槽；timeToDie
//      时把标签本体交由回收（OpenYR 以 Destroyed 标记承接）。
// 返回：本次是否有触发器真正执行了动作。
// ------------------------------------------------------------------------
bool TagClass::RaiseEvent(int32 eventKind, AbstractClass* pTagObject, CellStruct loc,
                          bool isRepeating, AbstractClass* pSource)
{
    (void)pSource;

    // asm: mov al, Debug_Map / test / jnz returnFalse —— 调试地图下不触发。
    if (Game::bShowDebugInfo) return false;
    if (IsExecuting) return false;          // asm: [this+0x35]
    if (field_34 != 0) return false;        // asm: [this+0x34]
    if (TagType == nullptr) return false;   // asm: [this+0x24]

    bool result = false;
    bool doTagReplacement = false;
    bool timeToDie = false;

    IsExecuting = true;

    for (int32 i = 0; i < TriggerList.Count; ++i) {
        TriggerClass* pTrigger = TriggerList.Items[i];
        if (pTrigger == nullptr) continue;

        // asm: isRepeating 实参 = (TagType->Repeating == 2)。
        const bool repeatingType = (TagType->Repeating == 2);
        if (!pTrigger->HaveAllEventsOccured(eventKind, pTagObject, isRepeating, repeatingType))
            continue;

        const int32 repeat = TagType->Repeating;
        if (repeat == 0) {
            pTrigger->FireActions(pTagObject, loc);
            pTrigger->Poof();
            result = true;
            timeToDie = true;
            doTagReplacement = true;
        } else if (repeat == 1) {
            if (field_2C == 1) {
                pTrigger->FireActions(pTagObject, loc);
                pTrigger->Poof();
                timeToDie = true;
                result = true;
            } else {
                doTagReplacement = true;
            }
        } else if (repeat == 2) {
            pTrigger->FireActions(pTagObject, loc);
            result = true;
        }
        // repeat > 2：asm 三级 dec 后落到链尾，不执行任何动作。
    }

    IsExecuting = false;

    if (doTagReplacement) {
        ObjectClass* pObj = static_cast<ObjectClass*>(pTagObject);
        if (pObj != nullptr && pObj->Tag == this) {
            pObj->Tag = nullptr;            // asm: ObjectClass::Attach_Trigger(obj, 0)
        }
        if (loc.X != DefaultCoords.X || loc.Y != DefaultCoords.Y) {
            // asm: MapClass::Coord_Cell + 格级重绘。OpenYR 尚无格级重绘入口，
            // 保留坐标判定以便后续接入。
        }
    }

    if (timeToDie) {
        // asm: Detach + 加入 vec_VisibleObjects 待回收。OpenYR 暂无该回收表，
        // 由 Poof() 置位的触发器 Destroyed 标记承接"原地消失"语义。
    }

    return result;
}
