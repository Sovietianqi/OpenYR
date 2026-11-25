#include "TriggerTypeClass.h"
#include "TEventClass.h"
#include "TActionClass.h"
#include "TriggerClass.h"
#include "../Houses/HouseTypeClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Core/SwizzleManagerClass.h"

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
// INI layout handled here (asm-authoritative, section names "Triggers",
// "Events", "Actions"):
//   [Triggers]  <ID> = <house>,<attachedTrigger>,<name>,<disabled>,<easy>,
//                      <normal>,<hard>,<repeat>
//   [Events]    <ID> = <count>[,<eventCmd>...]
//   [Actions]   <ID> = <count>[,<actionCmd>...]
// ============================================================================

DynamicVectorClass<TriggerTypeClass*>* TriggerTypeClass::Array = nullptr;

// ----------------------------------------------------------------------------
// 就地切分逗号串（会改写 buffer）。返回 token 数。供 [Events]/[Actions] 的
// 逐元素解析使用（与 asm 的 strtok 流语义等价）。
// ----------------------------------------------------------------------------
namespace {
    int32 Tokenize(char* buffer, const char* out[], int32 maxOut) {
        int32 n = 0;
        char* p = buffer;
        while (p != nullptr && *p != '\0' && n < maxOut) {
            out[n++] = p;
            char* comma = std::strchr(p, ',');
            if (comma != nullptr) {
                *comma = '\0';
                p = comma + 1;
            } else {
                p = nullptr;
            }
        }
        return n;
    }
}

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

    // 构造函数自登记，勿重复 Add。
    return GameCreate<TriggerTypeClass>(pID);
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
      NextTrigger(nullptr), Owner(nullptr), idxInArray(-1),
      Easy(true), Normal(true), Hard(true), IsDisabled(false),
      IsRepeatable(false), IsEnabled(true) {
    // asm: TriggerTypeClass_CTOR 尾部登记进 vec_TriggerTypes 并缓存下标。
    //   原来只有在 Array 已被别人建好时才缓存下标、且**从不 Add**，
    //   结果 [Triggers] 解析出的每一个类型都是孤儿：Find() 找不到、
    //    TagType 解析时又新建一份，同一条触发器被复制成多份。
    if (Array == nullptr) {
        Array = new DynamicVectorClass<TriggerTypeClass*>();
    }
    idxInArray = Array->Count;
    Array->AddItem(this);
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
// Load - base deserialization, then swizzle the four pointer slots.
// asm: TriggerTypeClass_Load (0x727BE0) swizzles +0xA8/+0xAC/+0xB0/+0xA4.
// ----------------------------------------------------------------------------
HRESULT TriggerTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) return hr;

    static SwizzleManagerClass SwizzleManager;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&NextTrigger));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&FirstEvent));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&FirstAction));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Owner));

    return S_OK;
}

// ----------------------------------------------------------------------------
// Save - base serialization only (pointers are swizzled).
// asm: TriggerTypeClass_Save (0x727C80) just chains AbstractClass_Save.
// ----------------------------------------------------------------------------
HRESULT TriggerTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;
    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;
    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TriggerTypeClass::WhatAmI() const {
    return AbstractType::TriggerType;
}

// asm: TriggerTypeClass_GetAbstractDerivationID (0x727CB0) -> 0x27
AbstractType TriggerTypeClass::GetAbstractDerivationID() const {
    return AbstractType::TriggerType;    // AbstractType value 39 (0x27)
}

// asm: TriggerTypeClass_GetSize (0x727CA0) -> 0xB4
int32 TriggerTypeClass::Size() const {
    return 0xB4;
}

// asm: TriggerTypeClass_GetArrayIndex (0x727CB8) -> +0x98
int32 TriggerTypeClass::GetArrayIndex() const {
    return idxInArray;
}

// ============================================================================
// INI loading / saving
// asm: TriggerTypeClass_LoadFromINI (0x727230) / _SaveToINI (0x7275C0)
// ============================================================================

// ----------------------------------------------------------------------------
// LoadFromINI - read this type's command line from [Triggers].
//   "<house>,<attachedTrigger>,<name>,<disabled>,<easy>,<normal>,<hard>,<repeat>"
// then build the event chain described by [Events] and the action chain
// described by [Actions] (each value starts with the element count).
// ----------------------------------------------------------------------------
bool TriggerTypeClass::LoadFromINI(CCINIClass* pINI) {
    if (!pINI) return false;

    char value[0x200];
    value[0] = '\0';
    if (pINI->GetString("Triggers", ID, value, sizeof(value)) <= 0) {
        return false;
    }

    // Token 1: owner country name -> house type.
    char* token = std::strtok(value, ",");
    if (token != nullptr) {
        if (_strcmpi(token, "<none>") == 0) {
            Owner = nullptr;
        } else {
            Owner = HouseTypeClass::Find(token);
        }
    }

    // Token 2: attached/linked trigger type ("" or <none> clears it).
    // The asm clears +0xA8 before parsing, then resolves the name.
    NextTrigger = nullptr;
    token = std::strtok(nullptr, ",");
    if (token != nullptr && _strcmpi(token, "<none>") != 0 && _strcmpi(token, "none") != 0) {
        NextTrigger = FindOrAllocate(token);
    }

    // Token 3: display name (copied into Name, up to 0x30 bytes).
    token = std::strtok(nullptr, ",");
    if (token != nullptr) {
        int32 i = 0;
        while (token[i] && i < static_cast<int32>(sizeof(Name)) - 1) {
            Name[i] = token[i];
            ++i;
        }
        Name[i] = '\0';
    } else {
        Name[0] = '\0';
    }

    // Token 4: disabled flag is stored inverted (0 => enabled).
    token = std::strtok(nullptr, ",");
    IsDisabled = (token != nullptr && std::atoi(token) == 0);
    IsEnabled = !IsDisabled;

    // Token 5: Easy.
    token = std::strtok(nullptr, ",");
    Easy = (token != nullptr && std::atoi(token) != 0);

    // Token 6: Normal.
    token = std::strtok(nullptr, ",");
    Normal = (token != nullptr && std::atoi(token) != 0);

    // Token 7: Hard.
    token = std::strtok(nullptr, ",");
    Hard = (token != nullptr && std::atoi(token) != 0);

    // Token 8: TRANSFERABLE（标签是否随载具被击毁 / 被占领而转移）。
    //   注意：这不是"可重复"标志 —— 重复由 [Tags] 的 PERSISTENCE 决定
    //   （0 一次性 / 1 全灭后一次 / 2 可重复）。TS/RA2/YR 的 [Triggers]
    //   行固定是 HOUSE,LINKED_TRIGGER,NAME,DISABLED,EASY,NORMAL,HARD,
    //   TRANSFERABLE 八段，ModEnc 明确说明 persistence 写在 tag 上。
    //   本树暂无 transferable 的消费方，先落进 IsRepeatable 仅作记录。
    token = std::strtok(nullptr, ",");
    IsRepeatable = (token != nullptr && std::atoi(token) != 0);

    // [Events]: "<count>[,<kind>,<tag>,<value>[,<name>]...]" -> 逐元素解析。
    //   asm: TriggerTypeClass_LoadFromINI+2BF 每读一个计数后的元素就 new
    //   EventClass 并调用 TEventClass_LoadFromINI（strtok 流继续推进）。
    EventList.Clear();
    FirstEvent = nullptr;
    {
        char evBuf[0x200];
        evBuf[0] = '\0';
        if (pINI->GetString("Events", ID, evBuf, sizeof(evBuf)) > 0) {
            const char* toks[96];
            const int32 n = Tokenize(evBuf, toks, 96);
            if (n >= 1) {
                const int32 evCount = std::atoi(toks[0]);
                int32 idx = 1;
                for (int32 i = 0; i < evCount && idx < n; ++i) {
                    TEventClass* pEvent = GameCreate<TEventClass>(nullptr);
                    if (pEvent == nullptr) break;
                    if (pEvent->ParseTokens(toks, n, &idx) < 0) {
                        GameDelete(pEvent);
                        break;
                    }
                    EventList.Add(pEvent);
                }
                FirstEvent = (EventList.Count > 0) ? EventList.Items[0] : nullptr;
            }
        }
    }

    // [Actions]: "<count>[,<kind>,<selector>,<name>,<arg3..arg6>[,<argN>]...]".
    ActionList.Clear();
    FirstAction = nullptr;
    {
        char acBuf[0x200];
        acBuf[0] = '\0';
        if (pINI->GetString("Actions", ID, acBuf, sizeof(acBuf)) > 0) {
            const char* toks[96];
            const int32 n = Tokenize(acBuf, toks, 96);
            if (n >= 1) {
                const int32 acCount = std::atoi(toks[0]);
                int32 idx = 1;
                for (int32 i = 0; i < acCount && idx < n; ++i) {
                    TActionClass* pAction = GameCreate<TActionClass>(nullptr);
                    if (pAction == nullptr) break;
                    if (pAction->ParseTokens(toks, n, &idx) < 0) {
                        GameDelete(pAction);
                        break;
                    }
                    ActionList.Add(pAction);
                }
                FirstAction = (ActionList.Count > 0) ? ActionList.Items[0] : nullptr;
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// SaveToINI - write this type's command line back to [Triggers], then write
// the event and action chains to [Events] / [Actions] under the same key.
//   "%s,%s,%s,%d,%d,%d,%d,%d"
//   house, attachedTrigger, Name, (disabled?1:0), Easy, Normal, Hard, Repeat
// (the disabled field is written inverted vs. the stored +0x9F flag).
// ----------------------------------------------------------------------------
bool TriggerTypeClass::SaveToINI(CCINIClass* pINI) {
    if (!pINI) return false;

    const char* trigName = (NextTrigger != nullptr) ? NextTrigger->ID : "<none>";
    const char* houseName = (Owner != nullptr) ? Owner->ID : "<none>";

    char value[0x200];
    std::snprintf(value, sizeof(value), "%s,%s,%s,%d,%d,%d,%d,%d",
                  houseName, trigName, Name,
                  IsDisabled ? 0 : 1,
                  Easy ? 1 : 0, Normal ? 1 : 0, Hard ? 1 : 0,
                  IsRepeatable ? 1 : 0);
    pINI->WriteString("Triggers", ID, value);

    // Events: "<count>" 后逐个追加元素自身序列化（asm: TEventClass_SaveToINI）。
    {
        char evValue[0x200];
        std::snprintf(evValue, sizeof(evValue), "%d", EventList.Count);
        for (int32 i = 0; i < EventList.Count; ++i) {
            TEventClass* pEvent = EventList.Items[i];
            if (pEvent == nullptr) continue;
            char cmd[0x80];
            pEvent->FormatSaveString(cmd, sizeof(cmd));
            std::strncat(evValue, ",", sizeof(evValue) - std::strlen(evValue) - 1);
            std::strncat(evValue, cmd, sizeof(evValue) - std::strlen(evValue) - 1);
        }
        pINI->WriteString("Events", ID, evValue);
    }

    // Actions: "<count>" then each element's own serialization is appended.
    {
        char acValue[0x200];
        std::snprintf(acValue, sizeof(acValue), "%d", ActionList.Count);
        for (int32 i = 0; i < ActionList.Count; ++i) {
            TActionClass* pAction = ActionList.Items[i];
            if (pAction == nullptr) continue;
            char cmd[64];
            std::snprintf(cmd, sizeof(cmd), ",%d,%d",
                          static_cast<int32>(pAction->ActionKind),
                          pAction->ActionIndex);
            std::strncat(acValue, cmd, sizeof(acValue) - std::strlen(acValue) - 1);
        }
        pINI->WriteString("Actions", ID, acValue);
    }

    return true;
}

// ----------------------------------------------------------------------------
// asm: TriggerTypeClass_CreateFromINIList (0x7275E0)
// Iterate every key of [Triggers]; for each non-<none> key find-or-allocate a
// TriggerTypeClass and call its virtual LoadFromINI (vtable slot +0x64).
// ----------------------------------------------------------------------------
void TriggerTypeClass::CreateFromINIList(CCINIClass* pINI) {
    if (!pINI) return;

    const int32 count = pINI->Section_GetValueCount("Triggers");
    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName("Triggers", i);
        if (pKeyName == nullptr) continue;
        if (_strcmpi(pKeyName, "<none>") == 0 || _strcmpi(pKeyName, "none") == 0) continue;

        TriggerTypeClass* pType = Find(pKeyName);
        if (pType == nullptr) {
            pType = GameCreate<TriggerTypeClass>(pKeyName);
        }
        if (pType != nullptr) {
            pType->LoadFromINI(pINI);
        }
    }
}

// ----------------------------------------------------------------------------
// asm: TriggerTypeClass_SaveToINIList (0x727700)
// Rewrite each [Triggers]/[Events]/[Actions] key back through mangleStuff_,
// then save every registered trigger type.
// ----------------------------------------------------------------------------
void TriggerTypeClass::SaveToINIList(CCINIClass* pINI) {
    if (!pINI) return;

    static const char* const kSections[3] = { "Triggers", "Events", "Actions" };
    for (int32 s = 0; s < 3; ++s) {
        const int32 count = pINI->Section_GetValueCount(kSections[s]);
        for (int32 i = 0; i < count; ++i) {
            const char* pKeyName = pINI->Section_GetKeyName(kSections[s], i);
            if (pKeyName == nullptr) continue;
            char value[0x20];
            value[0] = '\0';
            pINI->GetString(kSections[s], pKeyName, value, sizeof(value));
            // mangleStuff_: re-normalise the hex handle and write it back.
            const int32 hex = static_cast<int32>(std::strtoul(pKeyName, nullptr, 16));
            char rewritten[0x20];
            std::snprintf(rewritten, sizeof(rewritten), "%X", hex);
            pINI->WriteString(kSections[s], pKeyName, rewritten);
        }
    }

    if (Array != nullptr) {
        for (int32 i = 0; i < Array->GetCount(); ++i) {
            TriggerTypeClass* pType = (*Array)[i];
            if (pType != nullptr) {
                pType->SaveToINI(pINI);
            }
        }
    }
}

// ============================================================================
// Chain queries (asm: CrossVerticalZone / IsWPZoneEntry / InvolvesAllowWin /
// InvolvesGlobalChecking / CheckLocals). All walk the event/action chain and
// compare the element kind field (asm +0x2C).
// ============================================================================

// asm: TriggerTypeClass_CrossVerticalZone (0x726F50) - event kind == 0x1A.
bool TriggerTypeClass::CrossVerticalZone() const {
    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (pEvent && static_cast<int32>(pEvent->EventKind) == 0x1A) return true;
    }
    return false;
}

// asm: TriggerClass_CrossHorizontalZone (0x726F80) - event kind == 0x19.
bool TriggerTypeClass::CrossHorizontalZone() const {
    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (pEvent && static_cast<int32>(pEvent->EventKind) == 0x19) return true;
    }
    return false;
}

// asm: TriggerTypeClass_IsWPZoneEntry (0x726FB0) - event kind == 0x18.
bool TriggerTypeClass::IsWPZoneEntry() const {
    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (pEvent && static_cast<int32>(pEvent->EventKind) == 0x18) return true;
    }
    return false;
}

// asm: TriggerTypeClass_InvolvesAllowWin (0x726FE0) - action kind == 0x0F.
bool TriggerTypeClass::InvolvesAllowWin() const {
    for (int32 i = 0; i < ActionList.Count; ++i) {
        TActionClass* pAction = ActionList.Items[i];
        // asm 比较的是 1-based kind 0x0F，ActionKind 存的是下标，故比 TAction::AllowWin。
        if (pAction && static_cast<int32>(pAction->ActionKind) == static_cast<int32>(TAction::AllowWin)) return true;
    }
    return false;
}

// asm: TriggerTypeClass_InvolvesGlobalChecking (0x727010)
// event kind in {0x1B, 0x1C} and the event parameter equals idx.
bool TriggerTypeClass::InvolvesGlobalChecking(int32 idx) const {
    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (!pEvent) continue;
        const int32 kind = static_cast<int32>(pEvent->EventKind);
        if ((kind == 0x1B || kind == 0x1C) && pEvent->Data == idx) return true;
    }
    return false;
}

// asm: TriggerTypeClass_CheckLocals (0x727050)
// event kind in {0x24, 0x25} and the event parameter equals house.
bool TriggerTypeClass::CheckLocals(int32 house) const {
    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (!pEvent) continue;
        const int32 kind = static_cast<int32>(pEvent->EventKind);
        if ((kind == 0x24 || kind == 0x25) && pEvent->Data == house) return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// asm: getEventFlags (0x71F5xx) — map an event kind to a 5-bit flag mask.
// Reproduced from the asm's four per-bit kind tables (bits 0x1/0x2/0x8/0x10
// are table-driven, bit 0x4 is a two-value test).
// ----------------------------------------------------------------------------
static int32 getEventFlags(int32 idx) {
    static const uint8 kind0x1[60] = {
        0,0,1,1,0,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,
        1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,0 };
    static const uint8 kind0x2[50] = {
        0,0,0,1,0,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
        1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,1,1,1,0,0 };
    static const uint8 kind0x8[56] = {
        0,1,0,1,1,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,1,0,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,0,0,0,0 };
    static const uint8 kind0x10[54] = {
        0,1,1,1,1,0,0,1,1,1,1,1,1,1,1,0,1,1,1,0,0,1,1,1,1,1,1,1,0,0,
        1,1,1,1,1,1,1,0,0,0,1,1,0,0,1,1,1,1,1,1,1,1,0,0 };

    int32 flags = 0;
    if (idx >= 0 && idx <= 59 && kind0x1[idx]) flags |= 0x1;
    if (idx >= 0 && idx <= 49 && kind0x2[idx]) flags |= 0x2;
    if (idx == 8 || idx == 24) flags |= 0x4;
    const int32 e = idx - 3;
    if (e >= 0 && e <= 55 && kind0x8[e]) flags |= 0x8;
    const int32 f = idx - 8;
    if (f >= 0 && f <= 53 && kind0x10[f]) flags |= 0x10;
    return flags;
}

// ----------------------------------------------------------------------------
// asm: getActionFlags (0x6E3EE0) — map an action kind to a flag mask.
//   idx = kind - 14; if idx <= 0x61 and the table entry is set, return 2.
// ----------------------------------------------------------------------------
static int32 getActionFlags(int32 kind) {
    static const uint8 kind0x2_0[98] = {
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,0,0 };
    const int32 idx = kind - 14;
    if (idx >= 0 && idx <= 0x61 && kind0x2_0[idx]) return 2;
    return 0;
}

// ----------------------------------------------------------------------------
// asm: TriggerClass_GetFlags (0x7271E0) — the disassembly labels this with the
// wrong class, but it takes a TriggerTypeClass and reads its event/action
// chains, so it is TriggerTypeClass::GetFlags.
// ----------------------------------------------------------------------------
int32 TriggerTypeClass::GetFlags() const {
    int32 flags = 0;

    for (int32 i = 0; i < EventList.Count; ++i) {
        TEventClass* pEvent = EventList.Items[i];
        if (pEvent != nullptr) {
            flags |= getEventFlags(static_cast<int32>(pEvent->EventKind));
        }
    }
    for (int32 i = 0; i < ActionList.Count; ++i) {
        TActionClass* pAction = ActionList.Items[i];
        if (pAction != nullptr) {
            // getActionFlags 的表按 asm 的 1-based kind - 14 索引。
            flags |= getActionFlags(static_cast<int32>(pAction->ActionKind) + 1);
        }
    }
    if (NextTrigger != nullptr) {
        flags |= NextTrigger->GetFlags();
    }
    return flags;
}

// ----------------------------------------------------------------------------
// asm: TriggerTypeClass_PointerGotInvalid (0x7270A0), retn 8.
// If the invalidated object is the attached trigger, advance that slot; then
// walk both chains and forward the invalidation to every element (vtable+0x28),
// advancing each chain head if it points at the invalidated object.
// ----------------------------------------------------------------------------
void TriggerTypeClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed) {
    (void)removed;

    if (NextTrigger != nullptr && NextTrigger == pInvalid) {
        NextTrigger = NextTrigger->NextTrigger;
    }

    // Forward the invalidation down the action chain (asm vtable+0x28), then
    // advance the chain head if it named the invalidated object.
    for (int32 i = 0; i < ActionList.Count; ++i) {
        TActionClass* pAction = ActionList.Items[i];
        if (pAction != nullptr) {
            pAction->PointerGotInvalid(pInvalid);
        }
    }
    if (FirstAction == pInvalid) {
        FirstAction = (ActionList.Count > 1) ? ActionList.Items[1] : nullptr;
    }

    // TEventClass exposes no per-instance invalidation hook in OpenYR and is
    // not an AbstractClass, so only the chain head is advanced here when it
    // happens to alias the invalidated object.
    if (FirstEvent != nullptr
        && reinterpret_cast<AbstractClass*>(FirstEvent) == pInvalid) {
        FirstEvent = (EventList.Count > 1) ? EventList.Items[1] : nullptr;
    }
}

// ----------------------------------------------------------------------------
// asm: TriggerTypeClass_FindInstance (0x726630)
// Walk vec_Triggers; return the TriggerClass whose type pointer matches; if
// none, allocate a new TriggerClass bound to this type.
// ----------------------------------------------------------------------------
TriggerClass* TriggerTypeClass::FindInstance(TriggerTypeClass* pType) {
    if (pType == nullptr) return nullptr;

    if (TriggerClass::Array != nullptr) {
        for (int32 i = 0; i < TriggerClass::Array->Count; ++i) {
            TriggerClass* pTrigger = TriggerClass::Array->GetItem(i);
            if (pTrigger != nullptr && pTrigger->ID != nullptr
                && _strcmpi(pTrigger->ID, pType->ID) == 0) {
                return pTrigger;
            }
        }
    }
    return GameCreate<TriggerClass>(pType->ID);
}

TriggerTypeClass* TriggerTypeClass::FindInstance(const char* pID) {
    return Find(pID);
}

// ============================================================================
// CRC computation
// asm: TriggerTypeClass_GetCRC (0x727B50)
// Base CRC, then feed the owner's ID, the attached trigger's ID, the first
// event's ID and the first action's ID (each via the element's virtual id).
// ============================================================================
int32 TriggerTypeClass::GetCRC() const {
    CRCEngine crc;
    ComputeCRC(crc);

    if (Owner != nullptr) {
        const char* id = static_cast<const AbstractTypeClass*>(Owner)->get_ID();
        crc.AddData(id, static_cast<int32>(std::strlen(id)) + 1);
    }
    if (NextTrigger != nullptr) {
        crc.AddData(NextTrigger->ID, static_cast<int32>(std::strlen(NextTrigger->ID)) + 1);
    }
    if (FirstEvent != nullptr && FirstEvent->ID != nullptr) {
        crc.AddData(FirstEvent->ID, static_cast<int32>(std::strlen(FirstEvent->ID)) + 1);
    }
    if (FirstAction != nullptr && FirstAction->ID != nullptr) {
        const char* id = static_cast<const AbstractTypeClass*>(FirstAction)->get_ID();
        crc.AddData(id, static_cast<int32>(std::strlen(id)) + 1);
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
    IsDisabled = !enabled;
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
