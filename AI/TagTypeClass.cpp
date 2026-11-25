#include "TagTypeClass.h"
#include "TriggerTypeClass.h"
#include "TriggerClass.h"
#include "TagClass.h"
#include "../Abstract/AbstractTypeClass.h"
#include "../Abstract/AbstractClass.h"
#include "../Core/SwizzleManagerClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

// ============================================================================
// TagTypeClass - 标签类型（[Tags] 段里的每条定义）
//
// 语义对齐 asm TagTypeClass.asm。字段 Trigger/Repeating/idxInArray 对应
// asm 的 +0xA0/+0x9C/+0x98；基类 AbstractTypeClass 提供 ID 等持久字段。
// ============================================================================

DynamicVectorClass<TagTypeClass*>* TagTypeClass::Array = nullptr;

// ----------------------------------------------------------------------------
// asm: TagTypeClass_CTOR(TagTypeClass *this, char *name)   (asm:0x6E5B20)
// 基类初始化后置默认：idxInArray = -1、Repeating = 0、Trigger = 0，
// 随后把自身登记进类型表并把登记位置作为 idxInArray 缓存下来。
// ----------------------------------------------------------------------------
TagTypeClass::TagTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , Trigger(nullptr)
    , Repeating(0)
    , idxInArray(-1)
{
    if (Array == nullptr) {
        Array = new DynamicVectorClass<TagTypeClass*>();
    }
    idxInArray = Array->GetCount();
    Array->AddItem(this);
}

// noinit 路径：仅置默认值，不登记（对应反序列化时的占位构造）。
TagTypeClass::TagTypeClass(noinit_t) noexcept
    : AbstractTypeClass(noinit)
    , Trigger(nullptr)
    , Repeating(0)
    , idxInArray(-1)
{
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass::~TagTypeClass   (asm:0x6E5CC0)
// 释放 Trigger 链上挂载的实例，并从各处登记表里摘除自身。
// ----------------------------------------------------------------------------
TagTypeClass::~TagTypeClass()
{
    // 释放挂在本类型上的所有触发器实例链（asm 沿 +0xA8 后继逐级回收）。
    TriggerTypeClass* pChain = Trigger;
    while (pChain != nullptr) {
        TriggerTypeClass* pNext = pChain->NextTrigger;
        pChain->NextTrigger = nullptr;
        delete pChain;
        pChain = pNext;
    }
    Trigger = nullptr;

    // 从注册表里摘下自身，保持 idxInArray 缓存与真实下标一致。
    if (Array != nullptr) {
        int32 index = -1;
        for (int32 i = 0; i < Array->GetCount(); ++i) {
            if ((*Array)[i] == this) {
                index = i;
                break;
            }
        }
        if (index != -1) {
            Array->Remove(index);
            for (int32 i = index; i < Array->GetCount(); ++i) {
                TagTypeClass* pItem = (*Array)[i];
                if (pItem != nullptr) {
                    pItem->idxInArray = i;
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_CreateInstance(TagTypeClass *source)   (asm:0x6E5230)
// 在 vec_Tags_all 里按 +0x24（TagClass.Name，对应本类型 ID，顺序与
// vec_TagTypes 一致）反查已存在的实例，命中直接复用；否则新建 TagClass。
// ----------------------------------------------------------------------------
TagClass* TagTypeClass::CreateInstance(TagTypeClass* pSource)
{
    if (pSource == nullptr) {
        return nullptr;
    }

    if (TagClass::Array != nullptr) {
        for (int32 i = 0; i < TagClass::Array->GetCount(); ++i) {
            TagClass* pExisting = (*TagClass::Array)[i];
            if (pExisting != nullptr && _strcmpi(pExisting->Name, pSource->ID) == 0) {
                return pExisting;
            }
        }
    }

    if (TagClass::Array == nullptr) {
        TagClass::Array = new DynamicVectorClass<TagClass*>();
    }
    TagClass* pNew = GameCreate<TagClass>(pSource->ID);
    if (pNew != nullptr) {
        TagClass::Array->AddItem(pNew);
    }
    return pNew;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_CreateFromINIList   (asm:0x6E5E60)
// 遍历 [Tags] 段的每个键：读其命令串，按键名解析出十六进制句柄并登记到
// SwizzleManager，再在类型表里找到（或新建）对应 TagTypeClass；随后调用
// 该类型的虚拟 LoadFromINI。键名为 <none>/none 时跳过。
// ----------------------------------------------------------------------------
void TagTypeClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    const int32 count = pINI->Section_GetValueCount("Tags");
    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName("Tags", i);
        if (pKeyName == nullptr) {
            continue;
        }

        // [Tags] 行是 "PERSISTENCE,NAME,TRIGGER_ID"，名字段可能较长；
        //   原来用 0x18 缓冲，稍长的标签名会把第三段截断 → 触发器丢失。
        char value[0x100];
        value[0] = '\0';
        if (pINI->GetString("Tags", pKeyName, value, sizeof(value)) <= 0) {
            continue;
        }

        TagTypeClass* pTag = nullptr;
        if (_strcmpi(pKeyName, "<none>") != 0 && _strcmpi(pKeyName, "none") != 0) {
            // 键名可以是标签 ID，也可以是十六进制句柄。
            pTag = Find(pKeyName);
            if (pTag == nullptr) {
                pTag = GameCreate<TagTypeClass>(pKeyName);
            }
        }

        // 把键名解析出的十六进制句柄登记为标签实例的还原地址。
        static SwizzleManagerClass SwizzleManager;
        const int32 hex = (int32)strtoul(pKeyName, nullptr, 16);
        SwizzleManager.Here_I_Am(hex, pTag);

        if (pTag != nullptr) {
            pTag->LoadFromINI(pINI);

            // ── 运行期实例化 ────────────────────────────────────────────
            // asm: TagTypeClass_CreateInstance 造 TagClass，随后按
            //   TagType->Trigger 链为每个触发器类型造一条 TriggerClass
            //   并 AttachTrigger。缺这一步则 [Tags] 只有"类型"，没有
            //   可服务的实例，ProcessTagEvents 遍历的是空表。
            TagClass* pRuntime = CreateInstance(pTag);
            if (pRuntime != nullptr) {
                pRuntime->TagType = pTag;
                for (TriggerTypeClass* pTypeChain = pTag->Trigger;
                     pTypeChain != nullptr; pTypeChain = pTypeChain->NextTrigger) {
                    const char* pTrigId = pTypeChain->get_ID();
                    if (pTrigId == nullptr || pTrigId[0] == '\0') continue;

                    TriggerClass* pTrig = TriggerClass::FindOrAllocate(pTrigId);
                    if (pTrig == nullptr) continue;
                    pTrig->Type = pTypeChain;
                    pTrig->Repeatable = (pTag->Repeating == 2);
                    pTrig->Easy   = pTypeChain->Easy;
                    pTrig->Normal = pTypeChain->Normal;
                    pTrig->Hard   = pTypeChain->Hard;
                    pRuntime->AttachTrigger(pTrig);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_SaveToINIList   (asm:0x6E5F90)
// 回填 [Tags] 段中每个已存在键的命令串，然后遍历类型表逐条写出自身。
// ----------------------------------------------------------------------------
void TagTypeClass::SaveToINIList(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return;
    }

    const int32 count = pINI->Section_GetValueCount("Tags");
    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName("Tags", i);
        if (pKeyName == nullptr) {
            continue;
        }

        char value[0x20];
        value[0] = '\0';
        pINI->GetString("Tags", pKeyName, value, sizeof(value));

        // 回写（mangleStuff_：按 hex 句柄重新规范化后写回）。
        const int32 hex = (int32)strtoul(pKeyName, nullptr, 16);
        char rewritten[0x20];
        std::snprintf(rewritten, sizeof(rewritten), "%X", hex);
        pINI->WriteString("Tags", pKeyName, rewritten);
    }

    if (Array != nullptr) {
        for (int32 i = 0; i < Array->GetCount(); ++i) {
            TagTypeClass* pTag = (*Array)[i];
            if (pTag != nullptr) {
                pTag->SaveToINI(pINI);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_GetClassID   (asm:0x6E6390)
// pClassID 为空返回 E_POINTER，否则填入 TagTypeClass 的 CLSID。
// ----------------------------------------------------------------------------
HRESULT TagTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    pClassID->Data1 = 0x4D474154;   // 'TAGM' sentinel
    pClassID->Data2 = 0x5059;
    pClassID->Data3 = 0x4554;
    for (int32 i = 0; i < 8; ++i) {
        pClassID->Data4[i] = 0x54;
    }
    return S_OK;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_Load   (asm:0x6E63C0)
// 先走基类反序列化，再对 Trigger 指针做 swizzle 还原。
// ----------------------------------------------------------------------------
HRESULT TagTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr) {
        return E_POINTER;
    }

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) {
        return hr;
    }

    static SwizzleManagerClass SwizzleManager;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Trigger));

    return S_OK;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_Save   (asm:0x6E6440)
// 仅调用基类保存，本层无额外持久化字段（Trigger 由 swizzle 处理）。
// ----------------------------------------------------------------------------
HRESULT TagTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr) {
        return E_POINTER;
    }

    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) {
        return hr;
    }
    return S_OK;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_GetAbstractDerivationID   (asm:0x6E64A0)
// 返回 0x2D = AbstractType::TagType。
// ----------------------------------------------------------------------------
AbstractType TagTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::TagType;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_GetSize   (asm:0x6E64A8)
// 返回固定结构大小 0xA4。
// ----------------------------------------------------------------------------
int32 TagTypeClass::Size() const
{
    return 0xA4;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_GetArrayIndex   (asm:0x6E64E8)
// ----------------------------------------------------------------------------
int32 TagTypeClass::GetArrayIndex() const
{
    return idxInArray;
}

AbstractType TagTypeClass::WhatAmI() const
{
    return AbstractType::TagType;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_GetCRC   (asm:0x6E63F0)
// 基类 CRC 之后并入 Repeating（asm +0x9C，此处为重复触发标志）。
// ----------------------------------------------------------------------------
int32 TagTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    const int32 repeating = Repeating;
    crc.AddData(&repeating, sizeof(repeating));
    return static_cast<int32>(crc.GetCRC());
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_LoadFromINI(CCINIClass *INI)   (asm:0x6E6070)
// 读 [Tags] 段中本类型 ID 对应的串：
//   "%d,%s,%s" -> 数值存 +0x9C(Repeating)，第二段为 ID 后缀（跳过），
//   第三段交给 TriggerTypeClass_FindOrAllocate 得到 Trigger。
// 读不到返回 false。
// ----------------------------------------------------------------------------
bool TagTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return false;
    }

    char buffer[0x80];
    buffer[0] = '\0';
    if (pINI->GetString("Tags", ID, buffer, sizeof(buffer)) <= 0) {
        return false;
    }

    // 第一段：PERSISTENCE —— 0 = 一次性（任一对象命中即移除）、
    //   1 = 全灭后一次（全部挂载对象都命中才触发）、2 = 可重复。
    char* token = std::strtok(buffer, ",");
    Repeating = (token != nullptr) ? std::atoi(token) : 0;
    if (Repeating < 0) Repeating = 0;
    if (Repeating > 2) Repeating = 2;

    // 第二段：标签显示名（仅地图编辑器使用）。
    std::strtok(nullptr, ",");

    // 第三段：触发器 ID。<none> / 缺段时清空。
    token = std::strtok(nullptr, ",");
    if (token != nullptr && _strcmpi(token, "<none>") != 0 && _strcmpi(token, "none") != 0) {
        Trigger = TriggerTypeClass::FindOrAllocate(token);
    } else {
        Trigger = nullptr;
    }

    return true;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_SaveToINI(CCINIClass *this, int a2)   (asm:0x6E6160)
// Trigger 为空时写 "%s,<none>"，否则写 "%d,%s,%s"：
//   数值 + 第二段（空）+ 触发器类型 ID。
// ----------------------------------------------------------------------------
bool TagTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr) {
        return false;
    }

    char out[0x80];
    if (Trigger == nullptr) {
        std::snprintf(out, sizeof(out), "%s,<none>", ID);
    } else {
        std::snprintf(out, sizeof(out), "%d,%s,%s",
                      Repeating, "", Trigger->ID);
    }

    pINI->WriteString("Tags", ID, out);
    return true;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_GetFlags   (asm:0x6E6210)
// 沿 Trigger -> AttachedTrigger 链累积每个实例的标志位。
// ----------------------------------------------------------------------------
int32 TagTypeClass::GetFlags() const
{
    int32 flags = 0;
    TriggerTypeClass* pTrigger = Trigger;
    while (pTrigger != nullptr) {
        flags |= pTrigger->GetFlags();
        pTrigger = pTrigger->NextTrigger;
    }
    return flags;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_ContainsAllowWin   (asm:0x6E6220)
// 链上任一实例涉及允许胜利即返回真。
// ----------------------------------------------------------------------------
bool TagTypeClass::ContainsAllowWin() const
{
    TriggerTypeClass* pTrigger = Trigger;
    while (pTrigger != nullptr) {
        if (pTrigger->InvolvesAllowWin()) {
            return true;
        }
        pTrigger = pTrigger->NextTrigger;
    }
    return false;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_CrossHorizontalZone   (asm:0x6E6250)
// 链上任一实例命中横向区域检查即返回真。
// ----------------------------------------------------------------------------
bool TagTypeClass::CrossHorizontalZone() const
{
    TriggerTypeClass* pTrigger = Trigger;
    while (pTrigger != nullptr) {
        if (pTrigger->CrossHorizontalZone()) {
            return true;
        }
        pTrigger = pTrigger->NextTrigger;
    }
    return false;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_CrossVerticalZone   (asm:0x6E6280)
// 链上任一实例命中纵向区域检查即返回真。
// ----------------------------------------------------------------------------
bool TagTypeClass::CrossVerticalZone() const
{
    TriggerTypeClass* pTrigger = Trigger;
    while (pTrigger != nullptr) {
        if (pTrigger->CrossVerticalZone()) {
            return true;
        }
        pTrigger = pTrigger->NextTrigger;
    }
    return false;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_IsWPZoneEntry   (asm:0x6E62B0)
// 链上任一实例是路点区域项即返回真。
// ----------------------------------------------------------------------------
bool TagTypeClass::IsWPZoneEntry() const
{
    TriggerTypeClass* pTrigger = Trigger;
    while (pTrigger != nullptr) {
        if (pTrigger->IsWPZoneEntry()) {
            return true;
        }
        pTrigger = pTrigger->NextTrigger;
    }
    return false;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_PointerGotInvalid(TagTypeClass *this, AbstractClass* a2)  (asm:0x6E5E30)
// 若被销毁的实例正是链头，则把 Trigger 指向链中下一个实例。
// ----------------------------------------------------------------------------
void TagTypeClass::PointerGotInvalid(AbstractClass* gone)
{
    TriggerTypeClass* pGone = static_cast<TriggerTypeClass*>(gone);
    if (Trigger == pGone) {
        Trigger = pGone->NextTrigger;
    }
}

// ============================================================================
// 静态查找
// ============================================================================

// ----------------------------------------------------------------------------
// asm: TagTypeClass_FindByNameOrID   (asm:0x6E5E70)
// 依次比较类型 ID（+0x24）与名称（+0x64），大小写不敏感。
// ----------------------------------------------------------------------------
TagTypeClass* TagTypeClass::FindByNameOrID(const char* pName)
{
    if (pName == nullptr || Array == nullptr) {
        return nullptr;
    }

    for (int32 i = 0; i < Array->GetCount(); ++i) {
        TagTypeClass* pTag = (*Array)[i];
        if (pTag == nullptr) {
            continue;
        }
        if (_strcmpi(pName, pTag->ID) == 0 || _strcmpi(pName, pTag->Name) == 0) {
            return pTag;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_Find   (按 ID 精确查找，大小写不敏感)
// ----------------------------------------------------------------------------
TagTypeClass* TagTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr) {
        return nullptr;
    }

    for (int32 i = 0; i < Array->GetCount(); ++i) {
        TagTypeClass* pTag = (*Array)[i];
        if (pTag != nullptr && _strcmpi(pID, pTag->ID) == 0) {
            return pTag;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// asm: TagTypeClass_FindOrAllocate   (asm:0x6E6310)
// <none>/none 返回空；命中返回之，否则新建并登记。
// ----------------------------------------------------------------------------
TagTypeClass* TagTypeClass::FindOrAllocate(const char* pID)
{
    if (pID == nullptr || pID[0] == '\0') {
        return nullptr;
    }
    if (_strcmpi(pID, "<none>") == 0 || _strcmpi(pID, "none") == 0) {
        return nullptr;
    }

    TagTypeClass* pFound = Find(pID);
    if (pFound != nullptr) {
        return pFound;
    }

    return GameCreate<TagTypeClass>(pID);
}
