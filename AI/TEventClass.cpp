// ============================================================================
// TEventClass —— 触发条件（原版 EventClass）实现。
//
// 权威来源：asm 的
//   TEventClass_CTOR / TEventClass_SDDTOR
//   TEventClass_Load / TEventClass_Save / sub_71F880(GetClassID)
//   TEventClass_GetSize / _GetAbstractDerivationID / _GetArrayIndex
//   EventClass_GetCRC / EventClass_PointerGotInvalid
//   TEventClass_LoadFromINI / TEventClass_SaveToINI
//   EventClass_HasOccured （运行时核心判定）
//
// 字段语义见头文件；HouseClass 采用语义名访问（asm 偏移见注释）。
// 凡是 asm 读了字段含义不明、而 OpenYR 侧又无等价成员的判定分支，本实现
// 保留分支骨架但落在"最接近的真实成员"上，并在注释中标为 存疑。
// ============================================================================

#include "TEventClass.h"
#include "TriggerClass.h"
#include "TriggerTypeClass.h"
#include "TeamTypeClass.h"
#include "TeamClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../Game/EventClass.h"
#include "../INI/INIClass.h"
#include "../Core/SwizzleManagerClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

DynamicVectorClass<TEventClass*>* TEventClass::Array = nullptr;

// ============================================================================
// 数组管理 / 查找
// ============================================================================

TEventClass* TEventClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TEventClass* item = Array->GetItem(i);
        if (item && item->ID[0] && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

TEventClass* TEventClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TEventClass* found = Find(pID);
    if (found) return found;
    return GameCreate<TEventClass>(pID);
}

TEventClass* TEventClass::FindByName(const char* pName) {
    return Find(pName);
}

// ============================================================================
// asm: TEventClass_CTOR(EventClass *this)
//  基类构造后置默认；把自身登记进 vec_Events 并缓存 idxInArray(+0x24)。
// ============================================================================
TEventClass::TEventClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , EventKind(0)
    , Team(nullptr)
    , Data(0)
    , House(nullptr)
    , EventIndex(0)
    , chainedEvent(nullptr)
    , idxInArray(-1) {
    TechnoININame[0] = '\0';

    if (Array == nullptr) {
        Array = new DynamicVectorClass<TEventClass*>();
    }
    idxInArray = Array->Count;
    Array->AddItem(this);

    if (AbstractClass::Array != nullptr) {
        AbstractClass::Add_To_Array(this);
    }
}

TEventClass::TEventClass(noinit_t) noexcept
    : AbstractTypeClass(noinit)
    , EventKind(0)
    , Team(nullptr)
    , Data(0)
    , House(nullptr)
    , EventIndex(0)
    , chainedEvent(nullptr)
    , idxInArray(-1) {
    TechnoININame[0] = '\0';
}

// ============================================================================
// asm: TEventClass_SDDTOR —— 从 vec_Events 与全局抽象表摘除自身。
// ============================================================================
TEventClass::~TEventClass() {
    if (Array != nullptr && idxInArray >= 0) {
        int32 index = -1;
        for (int32 i = 0; i < Array->Count; ++i) {
            if (Array->GetItem(i) == this) { index = i; break; }
        }
        if (index != -1) {
            Array->Remove(index);
            for (int32 i = index; i < Array->Count; ++i) {
                TEventClass* pItem = Array->GetItem(i);
                if (pItem != nullptr) pItem->idxInArray = i;
            }
        }
    }
    if (AbstractClass::Array != nullptr) {
        AbstractClass::Remove_From_Array(this);
    }
}

// ============================================================================
// asm: sub_71F880 —— GetClassID
// ============================================================================
HRESULT TEventClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    pClassID->Data1 = 0x4C564545;   // 'EEVL' sentinel for EventClass
    pClassID->Data2 = 0x544E;
    pClassID->Data3 = 0x4E4F;
    for (int32 i = 0; i < 8; ++i) {
        pClassID->Data4[i] = 0x45;  // 'E'
    }
    return S_OK;
}

// ============================================================================
// asm: TEventClass_Load —— 基类反序列化后恢复三个 swizzle 槽
//  （+0x28 chainedEvent / +0x30 Team / +0x54 House）。
// ============================================================================
HRESULT TEventClass::Load(IStream* pStm) {
    if (pStm == nullptr) {
        return E_POINTER;
    }

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) {
        return hr;
    }

    static SwizzleManagerClass SwizzleManager;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&chainedEvent));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Team));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&House));

    return S_OK;
}

// asm: TEventClass_Save —— 仅走基类。
HRESULT TEventClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (pStm == nullptr) {
        return E_POINTER;
    }
    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) {
        return hr;
    }
    return S_OK;
}

AbstractType TEventClass::WhatAmI() const {
    return AbstractType::Event;
}

// asm: TEventClass_GetAbstractDerivationID —— 0x30 = 48 (Event)。
AbstractType TEventClass::GetAbstractDerivationID() const {
    return AbstractType::Event;
}

// asm: TEventClass_GetSize —— 0x58。
int32 TEventClass::Size() const {
    return 0x58;
}

// asm: TEventClass_GetArrayIndex —— [ecx+0x24]。
int32 TEventClass::GetArrayIndex() const {
    return idxInArray;
}

// asm: EventClass_GetCRC —— 基类 CRC 后并入 +0x28 链首 ID、eventKind、
//  +0x30 Team ID、param。
void TEventClass::ComputeCRC(CRCEngine& crc) const {
    Compute_CRC_Abstract(crc);

    if (chainedEvent != nullptr) {
        int32 id = chainedEvent->Fetch_ID();
        crc.AddData(&id, sizeof(id));
    }
    crc.AddData(&EventKind, sizeof(EventKind));
    if (Team != nullptr) {
        int32 id = Team->Fetch_ID();
        crc.AddData(&id, sizeof(id));
    }
    crc.AddData(&Data, sizeof(Data));
}

int32 TEventClass::GetCRC() const {
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// asm: EventClass_PointerGotInvalid(EventClass *this, AbstractClass* gone, char)
//  retn 8 —— 两参。chainedEvent 若命中则前进到链中下一个；Team 命中清空。
// ============================================================================
void TEventClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed) {
    (void)removed;
    if (pInvalid == nullptr) return;

    if (chainedEvent == reinterpret_cast<TEventClass*>(pInvalid)) {
        chainedEvent = chainedEvent->chainedEvent;
    }
    if (Team == reinterpret_cast<TeamTypeClass*>(pInvalid)) {
        Team = nullptr;
    }
}

// ============================================================================
// asm: TEventClass_LoadFromINI(EventClass *this)   (asm:327)
//  串由 TriggerTypeClass_LoadFromINI 喂入；本函数按 asm 的 strtok 顺序解析
//  同一份逻辑（此处保留为对外的显式解析入口）。
// ============================================================================
namespace {
    // 就地切分逗号串（会改写 buffer）。返回 token 数。
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
// asm: TEventClass_LoadFromINI 的逐元素解析。token 形态：
//   kind , tag , value [, name]
//   tag==0 → Data = value(num)          （纯数值参数）
//   tag==1 → Team = TeamTypeClass::FindByString(value)
//   tag==2 → Data = value(num) 且 name 拷入 TechnoININame（18 字节）
// 返回消费的 token 数；<0 表示字段不足。
// ----------------------------------------------------------------------------
int32 TEventClass::ParseTokens(const char* const* tokens, int32 count, int32* pIndex)
{
    if (tokens == nullptr || pIndex == nullptr) return -1;
    const int32 idx = *pIndex;
    if (idx < 0 || idx + 3 > count) return -1;

    const int32 kind   = std::atoi(tokens[idx]);
    const int32 tag    = std::atoi(tokens[idx + 1]);
    const char* value  = tokens[idx + 2];
    const int32 numVal = std::atoi(value);

    // asm: [esi+34h](param) 先清零，再按 tag 回填。
    Data = 0;
    Team = nullptr;
    TechnoININame[0] = '\0';

    int32 consumed = 3;
    const char* name = nullptr;
    if (tag == ParamType_Techno) {
        if (idx + 4 > count) return -1;
        name = tokens[idx + 3];
        consumed = 4;
    }

    switch (tag) {
    case ParamType_Number:
        Data = numVal;
        break;
    case ParamType_Team:
        Team = TeamTypeClass::FindByString(value);
        break;
    case ParamType_Techno:
        Data = numVal;
        if (name != nullptr) {
            // asm: strncpy(Dest, name, 18) 后补 '\0'。
            std::snprintf(TechnoININame, sizeof(TechnoININame), "%s", name);
        }
        break;
    default:
        Data = numVal;
        break;
    }

    EventKind = kind;
    EventIndex = tag;
    *pIndex = idx + consumed;
    return consumed;
}

bool TEventClass::LoadFromINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    char buffer[0x200];
    buffer[0] = '\0';
    if (pINI->GetString("Events", ID, buffer, sizeof(buffer)) <= 0) {
        return false;
    }

    const char* toks[64];
    const int32 n = Tokenize(buffer, toks, 64);
    if (n <= 1) return false;

    // [Events] 串首段是事件计数，解析单元素时跳过它。
    int32 idx = 1;
    return ParseTokens(toks, n, &idx) >= 0;
}

// ============================================================================
// asm: int __thiscall TEventClass_SaveToINI(EventClass *this, char *a2)   (asm:1666141)
//
//   注意：这里的 asm 名字虽然叫 SaveToINI，但它接的是 **字符串缓冲**，是
//   TriggerTypeClass_SaveToINI 逐元素拼 [Events] 串时调用的写出器；它与
//   AbstractTypeClass 的 INI 虚槽（asm 名 AbstractTypeClass_SaveIntoINI）
//   不是同一个函数。本工程的对应入口是 FormatSaveString（见下）。
//
//   asm 原有三个出口（全部用 wsprintfA 追加写到 a2 + strlen(a2)）：
//     Team(+30h) != 0  → "%d,%d,%s"     (EventKind, 1, Team->ID)
//     selector == 2Bh  → "%d,%d,%d,%s"  (EventKind, 2, Data, TechnoININame@+38h)
//     否则             → "%d,%d,%d"     (EventKind, 0, Data)
//
//   其中 selector 不是 EventKind 本身：asm 先做
//       switch (EventKind - 1)   // 61 个 case，跳转表 off_71F470 + 间接表 byte_71F4A0
//   把 12 个目标值之一算进 eax，再 `cmp eax, 2Bh`。按 byte_71F4A0 逐字节还原，
//   只有 **EventKind == 60 / 61** 两个种类会得到 2Bh（即头文件里
//   TechnoTypeNotOnMap / SecondsHaveElapsed）。
// ============================================================================
bool TEventClass::SaveToINI(CCINIClass* pINI)
{
    // asm 里 EventClass 没有覆写 INI 虚槽，落到基类 AbstractTypeClass_SaveIntoINI
    // （写 Name / UIName / TechLevel / Cost …）。原先这里直接 `return true;`
    // 会把这套通用写出整个吞掉，故改为显式委托基类。
    return AbstractTypeClass::SaveToINI(pINI);
}

// TriggerTypeClass 直接调用：把本事件序列化成 asm 的 a2 串。
void TEventClass::FormatSaveString(char* buffer, int32 bufferSize) const {
    if (!buffer || bufferSize <= 0) return;

    // asm 的 switch(EventKind-1) 只把 60/61 两种 kind 归到 2Bh 那一支
    // （见上方 TEventClass_SaveToINI 的跳转表还原），其余一律落到"%d,%d,%d"。
    const bool needsTechnoName = (EventKind == 60 || EventKind == 61);

    if (Team != nullptr) {
        std::snprintf(buffer, bufferSize, "%d,%d,%s",
                      EventKind, ParamType_Team, Team->ID);
    } else if (needsTechnoName) {
        std::snprintf(buffer, bufferSize, "%d,%d,%d,%s",
                      EventKind, ParamType_Techno, Data, TechnoININame);
    } else {
        std::snprintf(buffer, bufferSize, "%d,%d,%d",
                      EventKind, ParamType_Number, Data);
    }
}

// 解析 asm 的保存串：field0=eventKind, field1=tag, field2=value[,field3=name]。
bool TEventClass::ParseSaveString(const char* pString) {
    if (!pString || !pString[0]) return false;

    char work[192];
    std::snprintf(work, sizeof(work), "%s", pString);

    // 手动切分（避免依赖 strtok 的全局状态）。
    char* fields[4] = { nullptr, nullptr, nullptr, nullptr };
    int32 nFields = 0;
    char* p = work;
    while (*p && nFields < 4) {
        fields[nFields++] = p;
        char* comma = std::strchr(p, ',');
        if (!comma) break;
        *comma = '\0';
        p = comma + 1;
    }
    if (nFields < 3) return false;

    EventKind = std::atoi(fields[0]);
    EventIndex = std::atoi(fields[1]);   // tag
    const int32 value = std::atoi(fields[2]);

    Team = nullptr;
    TechnoININame[0] = '\0';
    Data = 0;

    switch (EventIndex) {
    case ParamType_Number:
        Data = value;
        break;
    case ParamType_Team:
        Team = TeamTypeClass::FindByString(fields[2]);
        break;
    case ParamType_Techno:
        Data = value;
        if (fields[3]) {
            std::snprintf(TechnoININame, sizeof(TechnoININame), "%s", fields[3]);
        }
        break;
    default:
        Data = value;
        break;
    }
    return true;
}

// ============================================================================
// asm: EventClass_HasOccured(EventClass *this, int eventKind, HouseClass* pHouse,
//      AbstractClass* Object, TimerStruct* ActivationFrame, char* isRepeating,
//      AbstractClass* source)                       (asm 0x71E964, retn 18h)
//
// 完整判定收口在 EventClass::HasOccurred（Game/EventClass.cpp）；此处的
// Evaluate 只保留原调用签名，转发时以触发器为激活计时源、触发对象为事件源。
// ============================================================================
bool TEventClass::Evaluate(int32 eventKind, HouseClass* pHouse,
                           AbstractClass* pObject, TriggerClass* pTrigger,
                           bool* isRepeating) {
    return EventClass::HasOccurred(this, eventKind, pHouse, pObject,
                                   pTrigger, pObject, isRepeating);
}

// ============================================================================
// 兼容层：IsSatisfied / MatchEvent —— TriggerClass 的现有调用点。
// ============================================================================

bool TEventClass::IsSatisfied(TriggerClass* pTrigger) const {
    if (!pTrigger) return false;
    TEventClass* self = const_cast<TEventClass*>(this);
    bool repeating = false;
    return self->Evaluate(static_cast<int32>(EventKind),
                          pTrigger->House, nullptr, pTrigger, &repeating);
}

bool TEventClass::MatchEvent(TriggerEventType eventType, AbstractClass* pObject,
                             CellStruct cell, TriggerClass* pTrigger) const {
    (void)cell;
    if (!pTrigger) return false;
    TEventClass* self = const_cast<TEventClass*>(this);
    bool repeating = false;
    return self->Evaluate(static_cast<int32>(eventType),
                          pTrigger->House, pObject, pTrigger, &repeating);
}

// ============================================================================
// 事件名（触发器对话框用）。名称体系不臆造：仅对 asm 跳转表能确证的少数
// 种类给出可读名，其余回落到 "Event <n>"。
// ============================================================================
void TEventClass::GetEventName(char* buffer, int32 bufferSize) const {
    if (!buffer || bufferSize <= 0) return;

    const char* name;
    switch (EventKind) {
    case 0:  name = "None"; break;
    case 13: name = "Time Elapsed"; break;
    case 14: name = "Mission Timer Expired"; break;
    case 26: name = "Global Is Set"; break;
    case 27: name = "Global Is Clear"; break;
    case 36: name = "Local Is Set"; break;
    case 37: name = "Local Is Clear"; break;
    case 49: name = "Ambient Light Below"; break;
    case 50: name = "Ambient Light Not Below"; break;
    case 51: name = "Scenario Time Elapsed"; break;
    case 59: name = "Techno Type On Map"; break;
    case 60: name = "Techno Type Not On Map"; break;
    default:
        std::snprintf(buffer, bufferSize, "Event %d", EventKind);
        return;
    }
    std::snprintf(buffer, bufferSize, "%s", name);
}
