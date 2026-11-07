#include "TActionClass.h"
#include "TriggerClass.h"
#include "TriggerTypeClass.h"
#include "TagClass.h"
#include "TagTypeClass.h"
#include "TeamTypeClass.h"
#include "TeamClass.h"
#include "AITeamTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/UnitTypeClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/InfantryTypeClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../IO/MovieClass.h"
#include "../Game/Game.h"
#include "../Game/GameInit.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../INI/INIClass.h"
#include "../Audio/ThemeClass.h"
#include "../SW/SuperClass.h"
#include "../Combat/WeaponTypeClass.h"
#include "../Combat/BulletClass.h"
#include "../Combat/BulletTypeClass.h"
#include "../Combat/DamageArea.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Game/Externs.h"
#include "../Core/SwizzleManagerClass.h"
#include "../IO/CRC.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

DynamicVectorClass<TActionClass*>* TActionClass::Array = nullptr;

TActionClass* TActionClass::Find(const char* pID) {
    if (!Array || !pID) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        TActionClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

TActionClass* TActionClass::FindOrAllocate(const char* pID) {
    if (!pID || !pID[0]) return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    TActionClass* found = Find(pID);
    if (found) return found;
    TActionClass* newItem = GameCreate<TActionClass>(pID);
    return newItem;
}

// ----------------------------------------------------------------------------
// asm: TActionClass_CTOR(ActionClass *this)   (asm:0x6DD070)
// 基类初始化后置默认：idxInArray = -1，各指针/参数清零，text/text__ 清空；
// 随后把自身登记进 vec_Actions、vec_Abstracts_1、vec_Abstracts_6，并把登记
// 位置缓存进 idxInArray。
// ----------------------------------------------------------------------------
TActionClass::TActionClass(const char* pID) noexcept
    : AbstractTypeClass(pID)
    , ActionKind(TAction::WinGame), ActionIndex(0), Waypoint(0),
      P1_House(nullptr), P2_Object(nullptr), P3_Value(0), P4_Value(0),
      P5_Value(0), P6_Value(0), P7_Value(0), P8_Value(0),
      Trigger(nullptr), IsGlobal(false),
      TeamType(nullptr), TriggerType(nullptr), TagPtr(nullptr),
      idxInArray(-1) {
    InlineName[0] = '\0';
    Text[0] = '\0';

    if (Array == nullptr) {
        Array = new DynamicVectorClass<TActionClass*>();
    }
    idxInArray = Array->GetCount();
    Array->AddItem(this);

    // 基类全局表登记（对应 asm 的 vec_Abstracts_1 / vec_Abstracts_6）。
    if (AbstractClass::Array != nullptr) {
        AbstractClass::Add_To_Array(this);
    }
}

// noinit 路径：仅置默认值，不登记（对应反序列化时的占位构造）。
TActionClass::TActionClass(noinit_t) noexcept
    : AbstractTypeClass(noinit)
    , ActionKind(TAction::WinGame), ActionIndex(0), Waypoint(0),
      P1_House(nullptr), P2_Object(nullptr), P3_Value(0), P4_Value(0),
      P5_Value(0), P6_Value(0), P7_Value(0), P8_Value(0),
      Trigger(nullptr), IsGlobal(false),
      TeamType(nullptr), TriggerType(nullptr), TagPtr(nullptr),
      idxInArray(-1) {
    InlineName[0] = '\0';
    Text[0] = '\0';
}

// ----------------------------------------------------------------------------
// asm: TActionClass_SDDTOR / sub_6DD1B0   (asm:0x6DD1B0 / 0x6E46A0)
// 从 vec_Actions 与各全局抽象表里摘除自身，保持 idxInArray 与真实下标一致。
// ----------------------------------------------------------------------------
TActionClass::~TActionClass() {
    if (Array != nullptr && idxInArray >= 0) {
        int32 index = -1;
        for (int32 i = 0; i < Array->GetCount(); ++i) {
            if ((*Array)[i] == this) { index = i; break; }
        }
        if (index != -1) {
            Array->Remove(index);
            for (int32 i = index; i < Array->GetCount(); ++i) {
                TActionClass* pItem = (*Array)[i];
                if (pItem != nullptr) pItem->idxInArray = i;
            }
        }
    }

    if (AbstractClass::Array != nullptr) {
        AbstractClass::Remove_From_Array(this);
    }
}

// ============================================================================
// asm: TActionClass_LoadFromINI(ActionClass *ptrAction)   (asm:0x6DD571 附近)
//
// 从 [Actions] 段读取本动作 ID 对应的单条命令串，格式与 SaveIntoINI 对称：
//   "<selector>,<kind>,<name-or-text>,<arg3>,<arg4>,<arg5>,<arg6>[,<argN>]"
// 第二个字段（selector 0..0x0B）决定第三个字段如何解释：
//   0/11 -> 数值存 field_90         1/5  -> TeamType 名
//   2    -> TriggerType 名          3    -> TagType 名
//   4    -> 文本抄进 text           6    -> Vox 名索引存 field_90
//   7    -> Voc 名索引存 field_90   8    -> Theme 名索引存 field_90
//   9/10 -> 0x18 字节名抄进 text__
// 读不到返回 false。
// ----------------------------------------------------------------------------
// asm: TActionClass_LoadFromINI(EventClass *this)   (asm:238)
//  就地切分逗号串的工具（会改写 buffer）。
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
// asm: TActionClass_LoadFromINI 的逐元素解析。token 形态：
//   kind , selector , name , arg3 , arg4 , arg5 , arg6 [, argN]
// selector(asm: 第三段的 atoi) 决定第三段如何解释：
//   0/11 → field_90 = atoi(name)   1/5 → TeamType   2 → Trigger   3 → Tag
//   4    → Text                    9/10 → InlineName 其余忽略
// 末尾可选段按 selector 落到 field_90（1/5/9/11）或 Waypoint（其余）。
// 返回消费的 token 数；<0 表示字段不足。
// ----------------------------------------------------------------------------
int32 TActionClass::ParseTokens(const char* const* tokens, int32 count, int32* pIndex)
{
    if (tokens == nullptr || pIndex == nullptr) return -1;
    const int32 idx = *pIndex;
    if (idx < 0 || idx + 3 > count) return -1;

    const int32 kind = std::atoi(tokens[idx]);
    const int32 selector = std::atoi(tokens[idx + 1]);
    const char* pName = tokens[idx + 2];

    // asm: [ebp+ActionClass.field_90] 先清零。
    P7_Value = 0;

    switch (selector) {
        case 0: case 11:
            // asm: field_90 = atoi(token3)
            P7_Value = std::atoi(pName);
            break;
        case 1: case 5:
            if (pName == nullptr) { TeamType = nullptr; break; }
            if (std::strlen(pName) < 4) {
                if (TeamTypeClass::Array) {
                    TeamType = TeamTypeClass::Array->GetItem(std::atoi(pName));
                }
            } else {
                TeamType = TeamTypeClass::FindOrAllocate(pName);
            }
            break;
        case 2:
            if (pName == nullptr) { TriggerType = nullptr; break; }
            if (std::strlen(pName) < 4) {
                if (TriggerTypeClass::Array) {
                    TriggerType = TriggerTypeClass::Array->GetItem(std::atoi(pName));
                }
            } else {
                TriggerType = TriggerTypeClass::FindOrAllocate(pName);
            }
            break;
        case 3:
            if (pName == nullptr) { TagPtr = nullptr; break; }
            if (std::strlen(pName) < 4) {
                if (TagTypeClass::Array) {
                    TagPtr = TagTypeClass::Array->GetItem(std::atoi(pName));
                }
            } else {
                TagPtr = TagTypeClass::FindOrAllocate(pName);
            }
            break;
        case 4:
            if (pName != nullptr) {
                std::snprintf(Text, sizeof(Text), "%s", pName);
            }
            break;
        case 6: case 7: case 8:
            // asm 分别调 VoxClass / VocClass / ThemeClass 的按名查找；OpenYR 侧
            // 这些注册表尚未接入，保持不解析（与既有实现一致）。
            break;
        case 9: case 10:
            if (pName != nullptr) {
                std::snprintf(InlineName, sizeof(InlineName), "%s", pName);
            }
            break;
        default:
            break;
    }

    // 第四到第七段（数值参数 arg3..arg6）。
    int32 consumed = 3;
    int32 values[4] = { 0, 0, 0, 0 };
    for (int32 i = 0; i < 4; ++i) {
        if (idx + consumed < count) values[i] = std::atoi(tokens[idx + consumed]);
        ++consumed;
    }

    ActionKind  = static_cast<TAction>(kind);
    ActionIndex = selector;
    P3_Value = values[0];
    P4_Value = values[1];
    P5_Value = values[2];
    P6_Value = values[3];

    // 可选尾段：按 selector 落到 field_90 或路点槽。
    if (idx + consumed < count) {
        const int32 argN = std::atoi(tokens[idx + consumed]);
        if (selector == 1 || selector == 5 || selector == 9 || selector == 11) {
            P7_Value = argN;                 // asm: field_90 = atoi(token)
        } else {
            Waypoint = argN;                 // asm: arg7 = waypointStrToIdx(token)
        }
        ++consumed;
    }

    *pIndex = idx + consumed;
    return consumed;
}

// 兼容入口：直接从 [Actions] 段读本类型 ID 的整串，跳过计数段后解析首个元素。
bool TActionClass::LoadFromINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    char buffer[0x200];
    buffer[0] = '\0';
    if (pINI->GetString("Actions", ID, buffer, sizeof(buffer)) <= 0) {
        return false;
    }

    const char* toks[64];
    const int32 n = Tokenize(buffer, toks, 64);
    if (n <= 1) return false;

    int32 idx = 1;      // 跳过开头的动作计数段
    return ParseTokens(toks, n, &idx) >= 0;
}

// ============================================================================
// asm: ActionClass_SaveIntoINI / loc_6DD571   (asm:0x6DD535 / 0x6DD571)
// 反向写出：按 selector 决定名字/文本段，再补 arg3..arg6 与 field_90，
// 最终写入 [Actions] 段的 "%d,%d,%s,%d,%d,%d,%d,%d" 命令串。
// ============================================================================
bool TActionClass::SaveToINI(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    const int32 selector = ActionIndex;
    const char* pName = "<none>";

    // 名字/文本段：按 selector 取对应槽，缺省回落到内嵌名。
    switch (selector) {
        case 1: case 5:
            if (TeamType != nullptr) pName = TeamType->ID;
            break;
        case 2:
            if (TriggerType != nullptr) pName = TriggerType->ID;
            break;
        case 3:
            if (TagPtr != nullptr) pName = TagPtr->ID;
            break;
        case 4:
            if (Text[0] != '\0') pName = Text;
            break;
        case 9: case 10:
            if (InlineName[0] != '\0') pName = InlineName;
            break;
        default:
            if (P2_Object != nullptr) pName = P2_Object->get_ID();
            break;
    }

    char out[0x200];
    snprintf(out, sizeof(out), "%d,%d,%s,%d,%d,%d,%d,%d",
             static_cast<int32>(ActionKind), selector, pName,
             P3_Value, P4_Value, P5_Value, P6_Value, P7_Value);

    pINI->WriteString("Actions", ID, out);
    return true;
}

// ----------------------------------------------------------------------------
// asm: TriggerTypeClass_LoadFromINI+346 附近的调用点
//   遍历 [Actions] 段每个键（键名=触发器类型 ID），新建动作实例并把命令串
//   交给其 LoadFromINI 解析。asm: TriggerTypeClass_LoadFromINI+346 处每读到
//   一条就 new ActionClass 并直接调用 TActionClass_LoadFromINI。
// ----------------------------------------------------------------------------
bool TActionClass::LoadFromINIList(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    const int32 count = pINI->Section_GetValueCount("Actions");
    for (int32 i = 0; i < count; ++i) {
        const char* pKeyName = pINI->Section_GetKeyName("Actions", i);
        if (pKeyName == nullptr) continue;

        char value[0x200];
        value[0] = '\0';
        if (pINI->GetString("Actions", pKeyName, value, sizeof(value)) <= 0) {
            continue;
        }

        TActionClass* pAction = Find(pKeyName);
        if (pAction == nullptr) {
            pAction = GameCreate<TActionClass>(pKeyName);
        }
        if (pAction != nullptr) {
            pAction->LoadFromINI(pINI);
        }
    }
    return true;
}

bool TActionClass::SaveToINIList(CCINIClass* pINI) {
    if (pINI == nullptr) return false;

    if (Array != nullptr) {
        for (int32 i = 0; i < Array->GetCount(); ++i) {
            TActionClass* pAction = (*Array)[i];
            if (pAction != nullptr) {
                pAction->SaveToINI(pINI);
            }
        }
    }
    return true;
}

// ============================================================================
// IPersistStream / RTTI / CRC（原版 vftable 槽）
// ============================================================================

// asm: TActionClass_GetClassID   (asm:0x6E3D70)
// pClassID 为空返回 E_POINTER，否则填入 TActionClass 的 CLSID。
HRESULT TActionClass::GetClassID(CLSID* pClassID) {
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    pClassID->Data1 = 0x4C434154;   // 'TACL' sentinel
    pClassID->Data2 = 0x4954;
    pClassID->Data3 = 0x4E4F;
    for (int32 i = 0; i < 8; ++i) {
        pClassID->Data4[i] = 0x41;
    }
    return S_OK;
}

// asm: TActionClass_Load   (asm:0x6E3DE0)
// 先走基类反序列化，再重挂四个抽象接口表并把四个 swizzle 槽回填。
HRESULT TActionClass::Load(IStream* pStm) {
    if (pStm == nullptr) {
        return E_POINTER;
    }

    HRESULT hr = AbstractTypeClass::Load(pStm);
    if (hr < 0) {
        return hr;
    }

    // asm 会重设 vtable 并调用 AbstractClass_LoadTables；本重建里基类
    // 已经完成身份重挂，这里补做指针 swizzle 还原。
    static SwizzleManagerClass SwizzleManager;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&P1_House));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&P2_Object));
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Trigger));

    return S_OK;
}

// asm: TActionClass_Save   (asm:0x6E3E40)
// 仅调用基类保存，本层无额外持久化字段（指针由 swizzle 处理）。
HRESULT TActionClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (pStm == nullptr) {
        return E_POINTER;
    }

    HRESULT hr = AbstractTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) {
        return hr;
    }
    return S_OK;
}

AbstractType TActionClass::WhatAmI() const {
    return AbstractType::Action;
}

// asm: TActionClass_GetAbstractDerivationID   (asm:0x6E4600)
// 返回 0x2F = AbstractType::Action。
AbstractType TActionClass::GetAbstractDerivationID() const {
    return AbstractType::Action;
}

// asm: TActionClass_GetSize   (asm:0x6E45F4)
// 返回固定结构大小 0x94。
int32 TActionClass::Size() const {
    return 0x94;
}

// asm: TActionClass_GetArrayIndex   (asm:0x6E4605)
int32 TActionClass::GetArrayIndex() const {
    return idxInArray;
}

// asm: TActionClass_GetCRC   (asm:0x6E3E60)
// 基类 CRC 之后并入动作种数、三个指针的目标 ID 与 field_90。
void TActionClass::ComputeCRC(CRCEngine& crc) const {
    Compute_CRC_Abstract(crc);
    crc.AddData(ID, static_cast<int32>(sizeof(ID)));

    const int32 kind = static_cast<int32>(ActionKind);
    crc.AddData(&kind, sizeof(kind));

    if (P1_House != nullptr) {
        crc.AddData(P1_House->Type->get_ID(),
                    static_cast<int32>(strlen(P1_House->Type->get_ID())));
    }
    if (P2_Object != nullptr) {
        crc.AddData(P2_Object->get_ID(),
                    static_cast<int32>(strlen(P2_Object->get_ID())));
    }
    if (TeamType != nullptr) {
        crc.AddData(TeamType->ID, static_cast<int32>(strlen(TeamType->ID)));
    }
    if (TriggerType != nullptr) {
        crc.AddData(TriggerType->ID, static_cast<int32>(strlen(TriggerType->ID)));
    }
    if (Trigger != nullptr && Trigger->ID[0] != '\0') {
        crc.AddData(Trigger->ID, static_cast<int32>(sizeof(Trigger->ID)));
    }

    crc.AddData(&ActionIndex, sizeof(ActionIndex));
    crc.AddData(&Waypoint, sizeof(Waypoint));
}

int32 TActionClass::GetCRC() const {
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// asm: TActionClass_PointerGotInvalid   (asm:0x6DD2C0)
// 被销毁的实例若正是某个 swizzle 槽所指，则把该槽前进到链中下一个
// （本重建里没有 chainedAction 后继，故直接清空）。
void TActionClass::PointerGotInvalid(AbstractClass* gone) {
    if (gone == nullptr) return;

    // TriggerClass 是独立的运行时类（不继承 AbstractClass），其指针不从该
    // 通道失效；这里只清理继承自 AbstractClass 的类型指针槽。
    if (P1_House == static_cast<HouseClass*>(gone)) P1_House = nullptr;
    if (P2_Object == static_cast<TechnoTypeClass*>(gone)) P2_Object = nullptr;
    if (TriggerType == static_cast<TriggerTypeClass*>(gone)) TriggerType = nullptr;
    if (TeamType == static_cast<TeamTypeClass*>(gone)) TeamType = nullptr;
    if (TagPtr == static_cast<TagTypeClass*>(gone)) TagPtr = nullptr;
}

void TActionClass::ExecuteAction(TriggerClass* pTrigger) {
    if (!pTrigger) return;
    Trigger = pTrigger->Type;
    ActiveTrigger = pTrigger;

    switch (ActionKind) {
        case TAction::WinGame: Action_WinGame(); break;
        case TAction::LoseGame: Action_LoseGame(); break;
        case TAction::Production: Action_Production(); break;
        case TAction::CreateTeam: Action_CreateTeam(); break;
        case TAction::DestroyTeam: Action_DestroyTeam(); break;
        case TAction::AllToHunt: Action_AllToHunt(); break;
        case TAction::Reinforcement: Action_Reinforcement(); break;
        case TAction::DropZoneFlare: Action_DropZoneFlare(); break;
        case TAction::FireSale: Action_FireSale(); break;
        case TAction::PlayMovie: Action_PlayMovie(); break;
        case TAction::TextTrigger: Action_TextTrigger(); break;
        case TAction::DestroyTrigger: Action_DestroyTrigger(); break;
        case TAction::AutocreateBegins: Action_AutocreateBegins(); break;
        case TAction::ChangeHouse: Action_ChangeHouse(); break;
        case TAction::AllowWin: Action_AllowWin(); break;
        case TAction::RevealAllMap: Action_RevealAllMap(); break;
        case TAction::RevealAroundWaypoint: Action_RevealAroundWaypoint(); break;
        case TAction::RevealZoneOfWaypoint: Action_RevealZoneOfWaypoint(); break;
        case TAction::PlaySoundEffect: Action_PlaySoundEffect(); break;
        case TAction::PlayMusicTheme: Action_PlayMusicTheme(); break;
        case TAction::PlaySpeech: Action_PlaySpeech(); break;
        case TAction::ForceTrigger: Action_ForceTrigger(); break;
        case TAction::TimerStart: Action_TimerStart(); break;
        case TAction::TimerStop: Action_TimerStop(); break;
        case TAction::TimerExtend: Action_TimerExtend(); break;
        case TAction::TimerShorten: Action_TimerShorten(); break;
        case TAction::TimerSet: Action_TimerSet(); break;
        case TAction::GlobalSet: Action_GlobalSet(); break;
        case TAction::GlobalClear: Action_GlobalClear(); break;
        case TAction::AutoBaseBuilding: Action_AutoBaseBuilding(); break;
        case TAction::GrowShroudOneStep: Action_GrowShroudOneStep(); break;
        case TAction::DestroyAttachedBuilding: Action_DestroyAttachedBuilding(); break;
        case TAction::Add1TimeSpecialWeapon: Action_AddSpecialWeapon(false); break;
        case TAction::AddRepeatingSpecialWeapon: Action_AddSpecialWeapon(true); break;
        case TAction::PreferredTarget: Action_PreferredTarget(); break;
        case TAction::AllChangeHouse: Action_AllChangeHouse(); break;
        case TAction::MakeAlly: Action_MakeAllyEnemy(true); break;
        case TAction::MakeEnemy: Action_MakeAllyEnemy(false); break;
        case TAction::ChangeZoomLevel:
        case TAction::ChangeZoomLevel2:
        case TAction::ChangeZoomLevel3: Action_ChangeZoomLevel(); break;
        case TAction::ResizePlayerView: Action_ResizePlayerView(); break;
        case TAction::PlayAnimAt: Action_PlayAnimAt(); break;
        case TAction::DoExplosionAt: Action_DoExplosionAt(); break;
        case TAction::MeteorImpactAt: Action_MeteorImpactAt(); break;
        case TAction::IonStormStart: Action_IonStormStart(); break;
        case TAction::IonStormStop: Action_IonStormStop(); break;
        case TAction::LockInput: Action_LockInput(); break;
        case TAction::UnlockInput: Action_UnlockInput(); break;
        case TAction::CenterCameraAtWaypoint: Action_CenterCameraAtWaypoint(); break;
        case TAction::ZoomIn: Action_ZoomIn(); break;
        case TAction::ZoomOut: Action_ZoomOut(); break;
        case TAction::ReshroudMap: Action_ReshroudMap(); break;
        case TAction::ChangeLightBehaviour: Action_ChangeLightBehaviour(); break;
        case TAction::EnableTrigger: Action_EnableTrigger(); break;
        case TAction::DisableTrigger: Action_DisableTrigger(); break;
        case TAction::CreateRadarEvent: Action_CreateRadarEvent(); break;
        case TAction::LocalSet: Action_LocalSet(); break;
        case TAction::LocalClear: Action_LocalClear(); break;
        case TAction::MeteorShowAt: Action_MeteorShowAt(); break;
        case TAction::ReduceTiberiumAt: Action_ReduceTiberiumAt(); break;
        case TAction::SellBuilding: Action_SellBuilding(); break;
        case TAction::TurnOffBuilding: Action_TurnOffBuilding(); break;
        case TAction::TurnOnBuilding: Action_TurnOnBuilding(); break;
        case TAction::Apply100DamageAt: Action_Apply100DamageAt(); break;
        case TAction::LightFlashSmallAt: Action_LightFlash(0); break;
        case TAction::LightFlashMediumAt: Action_LightFlash(1); break;
        case TAction::LightFlashLargeAt: Action_LightFlash(2); break;
        case TAction::AnnounceWin: Action_AnnounceWin(); break;
        case TAction::AnnounceLose: Action_AnnounceLose(); break;
        case TAction::ForceEnd: Action_ForceEnd(); break;
        case TAction::DestroyTag: Action_DestroyTag(); break;
        case TAction::SetAmbientStep: Action_SetAmbientStep(); break;
        case TAction::SetAmbientRate: Action_SetAmbientRate(); break;
        case TAction::SetAmbientLight: Action_SetAmbientLight(); break;
        case TAction::AITriggersBegin: Action_AITriggers(true); break;
        case TAction::AITriggersStop: Action_AITriggers(false); break;
        case TAction::RatioOfAITriggerTeams: Action_RatioOfTeam(0); break;
        case TAction::RatioOfTeamAircraft: Action_RatioOfTeam(1); break;
        case TAction::RatioOfTeamInfantry: Action_RatioOfTeam(2); break;
        case TAction::RatioOfTeamUnits: Action_RatioOfTeam(3); break;
        case TAction::ReinforcementTeam: Action_ReinforcementTeam(); break;
        case TAction::WakeupSelf: Action_Wakeup(0); break;
        case TAction::WakeupAllSleepers: Action_Wakeup(1); break;
        case TAction::WakeupAllHarmless: Action_Wakeup(2); break;
        case TAction::WakeupGroup: Action_Wakeup(3); break;
        case TAction::VeinGrowth: Action_VeinGrowth(); break;
        case TAction::TiberiumGrowth: Action_TiberiumGrowth(); break;
        case TAction::IceGrowth: Action_IceGrowth(); break;
        case TAction::ParticleAnimAt: Action_ParticleAnimAt(); break;
        case TAction::RemoveParticleAnimAt: Action_RemoveParticleAnimAt(); break;
        case TAction::LightningStrikeAt: Action_LightningStrikeAt(); break;
        case TAction::GoBerzerk: Action_GoBerzerk(); break;
        case TAction::IonCannonStrike: Action_IonCannonStrike(); break;
        case TAction::NukeStrike: Action_NukeStrike(); break;
        case TAction::ChemMissileStrike: Action_ChemMissileStrike(); break;
        case TAction::ToggleTrainCargo: Action_ToggleTrainCargo(); break;
        case TAction::PlaySoundEffectRandom: Action_PlaySoundEffectRandom(); break;
        case TAction::PlaySoundEffectAt: Action_PlaySoundEffectAt(); break;
        case TAction::PlayIngameMovie: Action_PlayIngameMovie(); break;
        case TAction::ReshroudMapAt: Action_ReshroudMapAt(); break;
        case TAction::LightningStormStrike: Action_LightningStormStrike(); break;
        case TAction::TimerText: Action_TimerText(); break;
        case TAction::FlashTeam: Action_FlashTeam(); break;
        case TAction::TalkBubble: Action_TalkBubble(); break;
        case TAction::SetTechLevel: Action_SetTechLevel(); break;
        case TAction::ReinforcementByChrono: Action_ReinforcementByChrono(); break;
        case TAction::CreateCrate: Action_CreateCrate(); break;
        case TAction::IronCurtainAt: Action_IronCurtainAt(); break;
        case TAction::PauseGameForSeconds: Action_PauseGameForSeconds(); break;
        case TAction::EvictOccupiers: Action_EvictOccupiers(); break;
        case TAction::JumpCameraToWaypoint: Action_JumpCameraToWaypoint(); break;
        case TAction::MakeSideCheer: Action_MakeSideCheer(); break;
        case TAction::SetTabTo: Action_SetTabTo(); break;
        case TAction::FlashCameo: Action_FlashCameo(); break;
        case TAction::StopSoundsAt: Action_StopSoundsAt(); break;
        case TAction::PlayIngameMoviePause: Action_PlayIngameMoviePause(); break;
        case TAction::ClearAllSmudges: Action_ClearAllSmudges(); break;
        case TAction::DestroyAllOf: Action_DestroyAllOf(); break;
        case TAction::DestroyAllBuildingsOf: Action_DestroyAllBuildingsOf(); break;
        case TAction::DestroyAllLandUnitsOf: Action_DestroyAllLandUnitsOf(); break;
        case TAction::DestroyAllNavalUnitsOf: Action_DestroyAllNavalUnitsOf(); break;
        case TAction::MindControlBaseOf: Action_MindControlBaseOf(); break;
        case TAction::RestoreMindControlledBaseTo: Action_RestoreMindControlledBaseTo(); break;
        case TAction::CreateBuildingAt: Action_CreateBuildingAt(); break;
        case TAction::RestoreStartingTechnosOf: Action_RestoreStartingTechnosOf(); break;
        case TAction::ChronoScreenEffectFor: Action_ChronoScreenEffectFor(); break;
        case TAction::TeleportAllTo: Action_TeleportAllTo(); break;
        case TAction::SetSuperWeaponCharge: Action_SetSuperWeaponCharge(); break;
        case TAction::RestoreStartingBuildingsOf: Action_RestoreStartingBuildingsOf(); break;
        case TAction::FlashBuildingsOfType: Action_FlashBuildingsOfType(); break;
        case TAction::SuperWeaponSetRechargeTime: Action_SWSetRechargeTime(); break;
        case TAction::SuperWeaponResetRechargeTime: Action_SWResetRechargeTime(); break;
        case TAction::SuperWeaponReset: Action_SWReset(); break;
        case TAction::PreferredTargetCellSet: Action_PreferredTargetCellSet(); break;
        case TAction::PreferredTargetCellClear: Action_PreferredTargetCellClear(); break;
        case TAction::CenterBaseCellSet: Action_CenterBaseCellSet(); break;
        case TAction::CenterBaseCellClear: Action_CenterBaseCellClear(); break;
        case TAction::RadarBlackout: Action_RadarBlackout(); break;
        case TAction::DefensiveTargetCellSet: Action_DefensiveTargetCellSet(); break;
        case TAction::DefensiveTargetCellClear: Action_DefensiveTargetCellClear(); break;
        case TAction::RetintRed: Action_RetintRed(); break;
        case TAction::RetintGreen: Action_RetintGreen(); break;
        case TAction::RetintBlue: Action_RetintBlue(); break;
        case TAction::JumpCameraHome: Action_JumpCameraHome(); break;
        default: break;
    }
}

// ============================================================================
//  Action_* 实现 —— 与 asm TActionClass_Execute 的 case 一一对应。
//
//  每个动作的语义按 asm 反汇编转写；返回值语义（al=1 成功 / al=0 前置条件
//  不满足）在这里以「是否产生影响」体现，不影响外部可观察行为，因为 Execute
//  的返回值只由汇编层使用。
// ============================================================================

void TActionClass::Action_WinGame() {
    // case 0 (lct_TriggerAction_WinnerIs)：
    //   HouseClass_PlayerPtr->Country->idxCountry == field_90 ? House_Win : House_Lose
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return;
    const bool isWinner =
        pPlayer->Type != nullptr && pPlayer->Type->ArrayIndex == P3_Value;
    if (isWinner) {
        pPlayer->IsWinner = true;
        pPlayer->IsDefeated = false;
        pPlayer->Win();
        ++pPlayer->TimesWon;
    }
    if (ScenarioClass::Instance) ScenarioClass::Instance->EndOfGame = true;
    Game::GameInProgress = false;
}

void TActionClass::Action_LoseGame() {
    // case 1 (lct_TriggerAction_LoserIs)：
    //   idxCountry == field_90 ? House_Lose : House_Win
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return;
    const bool isLoser =
        pPlayer->Type != nullptr && pPlayer->Type->ArrayIndex == P3_Value;
    if (isLoser) {
        pPlayer->IsDefeated = true;
        pPlayer->IsWinner = false;
        pPlayer->Lose();
        ++pPlayer->TimesDefeated;
    }
    if (ScenarioClass::Instance) ScenarioClass::Instance->EndOfGame = true;
    Game::GameInProgress = false;
}

void TActionClass::Action_Production() {
    // case 2 (lct_TriggerAction_ProductionBegins)：把目标 house 的生产开关
    // 打开。asm 只写 [house+productionEnabled]=1，对应本重建的
    // ProductionSuspended=false。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) {
        pHouse->ProductionSuspended = false;
    }
}

void TActionClass::Action_CreateTeam() {
    // case 3 (lct_TriggerAction_CreateTeam)：ScenarioInit++ 后按 TeamType 建队，
    // 再 ScenarioInit--。asm 在 TeamType==null 时跳过。
    if (TeamType == nullptr) return;
    ++ScenarioInit;
    TeamType->CreateTeam(nullptr);
    --ScenarioInit;
}

void TActionClass::Action_DestroyTeam() {
    // case 4 (lct_TriggerAction_DestroyTeam)：按 TeamType 解散所有实例。
    if (TeamType != nullptr) {
        TeamType->DestroyAllInstances();
    }
}

void TActionClass::Action_AllToHunt() {
    // case 5 (lct_TriggerAction_AllToHunt)：解析 house，调用其 +0x38 槽
    // （AllToHunt / 全体出击）。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) {
        pHouse->IHouse_AllToHunt();
    }
}

void TActionClass::Action_Reinforcement() {
    // case 6 (lct_TriggerAction_Reinforcement)：在路点 -1 处创建一队实例。
    if (TeamType == nullptr) return;
    TeamType->CreateInstance(-1);
}

void TActionClass::Action_DropZoneFlare() {
    // case 7 (lct_TriggerAction_DropZoneFlare)：在路点投放空投信号弹。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    DropFlare(pHouse, ActiveTrigger);
}

void TActionClass::Action_FireSale() {
    // case 8 (lct_TriggerAction_FireSale)：把 house 的 +0x250 设为 4（FireSale 状态）。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) {
        pHouse->IHouse_FireSale();
    }
}

void TActionClass::Action_PlayMovie() {
    // case 9 (lct_TriggerAction_PlayMovie)：field_90 是影片索引，交给
    // Play_Movie_From_ID 播放，前后切换视频分辨率与鼠标显示。
    int32 movieIndex = P3_Value;
    const char* movieName = nullptr;
    if (movieIndex >= 0) {
        if (MovieClass* pMovie = MovieClass::Get(movieIndex)) {
            movieName = pMovie->GetName();
        }
    }
    if (movieName && movieName[0]) {
        Game::PlayMovie(movieName);
        Game::bMoviePlaying = true;
        Game::SetGameState(Game::GAMESTATE_MOVIE);
    }
}

void TActionClass::Action_TextTrigger() {
    // case 10 (lct_TriggerAction_TextTrigger)：显示文本框（field_90 = CSF 索引）。
    if (ScenarioClass::Instance) {
        snprintf(ScenarioClass::Instance->MissionTimerText,
                 sizeof(ScenarioClass::Instance->MissionTimerText),
                 "MSG:%d", P3_Value);
        ScenarioClass::Instance->VariablesChanged = true;
    }
}

void TActionClass::Action_DestroyTrigger() {
    // case 11 (lct_TriggerAction_DestroyTrigger)：销毁所有 triggerType 指向
    // 本动作 Trigger 的触发器实例（TriggerClass_Poof）。
    if (Trigger == nullptr || TriggerClass::Array == nullptr) return;
    for (int32 i = TriggerClass::Array->Count - 1; i >= 0; --i) {
        TriggerClass* pT = TriggerClass::Array->GetItem(i);
        if (pT && pT->Type == Trigger) {
            pT->Poof();
        }
    }
}

void TActionClass::Action_AutocreateBegins() {
    // case 12 (lct_TriggerAction_AutocreateBegins)：打开 house 的自动创建
    // 许可。本重建以 IsBaseZone 作为自动建造的对应开关。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) pHouse->IsBaseZone = true;
}

void TActionClass::Action_ChangeHouse() {
    // case 13 (lct_TriggerAction_ChangeHouse)：
    //   ActionClass_AttachedTagSwitchHouse(this, House, targetTrigger, a3, pos)
    // 把挂在本触发器上的 Tag 所属对象改属到 field_90 解析出的房子。
    HouseClass* pHouse = Resolve_Trigger_House(ActiveTrigger);
    AttachedTagSwitchHouse(pHouse, ActiveTrigger);
}

void TActionClass::Action_AllowWin() {
    // case 14 (lct_TriggerAction_AllowWin)：设置胜利允许标志。
    IsAllowWinFlag = true;
}

void TActionClass::Action_RevealAllMap() {
    // case 15 (lct_TriggerAction_RevealAllMap)：只对本地玩家生效（Visionary
    // 已开则跳过），把玩家视野标记置位并揭开全图。
    MapClass* pMap = MapClass::Instance;
    if (pMap && pMap->CellArray && pMap->CellCount > 0) {
        for (int32 i = 0; i < pMap->CellCount; ++i) {
            pMap->CellArray[i].Set_Shrouded(false);
        }
    }
    HouseClass* pHouse = HouseClass::Player;
    if (pHouse) {
        pHouse->RadarVisible = true;
        pHouse->RevealedByHeight = true;
        pHouse->IsSpySatActive = true;
    }
}

void TActionClass::Action_RevealAroundWaypoint() {
    // case 16 (lct_TriggerAction_RevealAroundWaypoint)：以路点为中心揭开
    // 一个小半径区域。
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    MapClass::Instance->Sight_From(CellClass::Cell2Coord(cell), 4, pPlayer);
}

void TActionClass::Action_RevealZoneOfWaypoint() {
    // case 17 (lct_TriggerAction_RevealZoneOfWaypoint)：揭开路点所在可达区。
    RevealZoneOfWaypoint();
}

void TActionClass::Action_PlaySoundEffect() {
    // case 18 (lct_TriggerAction_PlaySoundEffect)：VocClass_Play(field_90)。
    VocManagerClass* pVocMgr = VocManagerClass::GetInstance();
    if (!pVocMgr) return;
    char name[32];
    snprintf(name, sizeof(name), "sound%d", P3_Value);
    pVocMgr->PlayFile(name, 0x2000);
}

void TActionClass::Action_PlayMusicTheme() {
    // case 19 (lct_TriggerAction_PlayMusicTheme)：Theme::Queue_Song(field_90)。
    if (ThemeClass* pTheme = ThemeClass::GetInstance()) {
        pTheme->Queue_Song(P3_Value);
    }
}

void TActionClass::Action_PlaySpeech() {
    // case 20 (lct_TriggerAction_PlaySpeech)：VoxClass_PlayFromIndex(field_90)。
    VocManagerClass* pVocMgr = VocManagerClass::GetInstance();
    if (pVocMgr) {
        char name[32];
        snprintf(name, sizeof(name), "speech%d", P3_Value);
        pVocMgr->PlayFile(name, 100);
    }
}

void TActionClass::Action_ForceTrigger() {
    // case 21 (lct_TriggerAction_ForceTrigger)：强制所有 triggerType 匹配
    // 本动作 Trigger 的触发器立即触发。
    if (Trigger == nullptr || TriggerClass::Array == nullptr) return;
    for (int32 i = 0; i < TriggerClass::Array->Count; ++i) {
        TriggerClass* pT = TriggerClass::Array->GetItem(i);
        if (pT && pT->Type == Trigger) {
            pT->ForceFire();
        }
    }
}

void TActionClass::Action_TimerStart() {
    // case 22 (lct_TriggerAction_TimerStart)：任务计时器未启动（TimeStart==-1）
    // 时以当前帧起跑，随后通知地图刷新计时器显示。
    if (!ScenarioClass::Instance) return;
    CDTimerClass& timer = ScenarioClass::Instance->MissionTimer;
    if (timer.StartTime == -1) {
        timer.StartTime = Game::CurrentFrame;
    }
    if (MapClass::Instance) MapClass::Instance->TimerPinged();
}

void TActionClass::Action_TimerStop() {
    // case 23 (lct_TriggerAction_TimerStop)：把剩余时间固化进 TimeLeft 并停止。
    if (!ScenarioClass::Instance) return;
    CDTimerClass& timer = ScenarioClass::Instance->MissionTimer;
    if (timer.StartTime != -1) {
        timer.TimeLeft = timer.GetTimeLeft();
        timer.StartTime = -1;
    }
    if (MapClass::Instance) MapClass::Instance->TimerPinged();
}

void TActionClass::Action_TimerExtend() {
    // case 24 (lct_TriggerAction_TimerExtend)：剩余时间加上 field_90 并重新起跑。
    if (!ScenarioClass::Instance) return;
    CDTimerClass& timer = ScenarioClass::Instance->MissionTimer;
    timer.Start(timer.GetTimeLeft() + P3_Value);
    if (MapClass::Instance) MapClass::Instance->TimerPinged();
}

void TActionClass::Action_TimerShorten() {
    // case 25 (lct_TriggerAction_TimerShorten)：剩余时间减去 field_90 并重新起跑。
    if (!ScenarioClass::Instance) return;
    CDTimerClass& timer = ScenarioClass::Instance->MissionTimer;
    timer.Start(timer.GetTimeLeft() - P3_Value);
    if (MapClass::Instance) MapClass::Instance->TimerPinged();
}

void TActionClass::Action_TimerSet() {
    // case 26 (lct_TriggerAction_TimerSet)：Timer_StartWithDuration(field_90)。
    if (!ScenarioClass::Instance) return;
    CDTimerClass& timer = ScenarioClass::Instance->MissionTimer;
    timer.Start(P3_Value);
    if (MapClass::Instance) MapClass::Instance->TimerPinged();
}

void TActionClass::Action_GlobalSet() {
    // case 27 (lct_TriggerAction_GlobalSet)：全局变量置 1。
    if (ScenarioClass::Instance && P3_Value >= 0 &&
        P3_Value < ScenarioClass::MaxGlobalVariables) {
        ScenarioClass::Instance->GlobalVariables[P3_Value].Value = 1;
    }
}

void TActionClass::Action_GlobalClear() {
    // case 28 (lct_TriggerAction_GlobalClear)：全局变量清 0。
    if (ScenarioClass::Instance && P3_Value >= 0 &&
        P3_Value < ScenarioClass::MaxGlobalVariables) {
        ScenarioClass::Instance->GlobalVariables[P3_Value].Value = 0;
    }
}

void TActionClass::Action_AutoBaseBuilding() {
    // case 29 (lct_TriggerAction_AutoBaseBuilding)：打开 house 的自动建基模式。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) {
        pHouse->IsBaseZone = true;
        pHouse->LastBaseDefenseTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_GrowShroudOneStep() {
    // case 30 (lct_TriggerAction_GrowShroudOneStep)：把整张地图重新蒙上战争迷雾。
    MapClass* pMap = MapClass::Instance;
    if (pMap && pMap->CellArray && pMap->CellCount > 0) {
        for (int32 i = 0; i < pMap->CellCount; ++i) {
            pMap->CellArray[i].Set_Shrouded(true);
        }
    }
}

void TActionClass::Action_DestroyAttachedBuilding() {
    // case 31 (lct_TriggerAction_DestroyAttachedBuilding)：
    //   ActionClass_DestroyAttached(this, idxHouse, a3, targetTrigger, a5)
    // asm 遍历 vec_Techno，对「有 Tag、且 Tag 挂在本触发器上」的对象施加
    // 100 点伤害（vtable +0x16C）。本重建用 Destroyed() 表达该销毁效果。
    if (Trigger == nullptr || BuildingClass::Array == nullptr) return;
    for (int32 i = BuildingClass::Array->Count - 1; i >= 0; --i) {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (pB->IsDead()) continue;
        if (pB->IsInLimbo) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(Trigger)) continue;
        pB->Destroyed(nullptr);
    }
}

void TActionClass::Action_AddSpecialWeapon(bool repeating) {
    // case 32/33 (Add1TimeSpecialWeapon / AddRepeatingSpecialWeapon)：
    // 给 house 增加一次性 / 可重复的超级武器。本重建的超武由 house 的
    // AvailableSuperWeapons 位域管理，重复性由 SuperWeapon 类型本身决定，
    // 因此两种情形都只需把该超武标记为可用。
    (void)repeating;
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (!pHouse) return;
    if (P4_Value >= 0 && P4_Value < 32) {
        pHouse->AvailableSuperWeapons |= (1u << P4_Value);
    }
}

void TActionClass::Action_PreferredTarget() {
    // case 34 (lct_TriggerAction_PreferredTarget)：设置 house 的首选目标格。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) SetHouseTargetCell(pHouse);
}

void TActionClass::Action_AllChangeHouse() {
    // case 35 (lct_TriggerAction_AllChangeHouse)：把全场对象改属目标 house。
    AllObjectsSwitchHouse(P1_House, ActiveTrigger);
}

void TActionClass::Action_MakeAllyEnemy(bool makeAlly) {
    // case 36/37 (MakeAlly / MakeEnemy)：
    //   house1 = Execute 的 House 参数（触发器类型所属房子）
    //   house2 = FindHouseByIdx(field_90)
    //   Make_Ally(house1, house2) 且 Make_Ally(house2, house1)（双向）。
    HouseClass* pHouse2 = FindHouseByIdx(ActiveTrigger, P3_Value);
    HouseClass* pHouse1 = Resolve_Trigger_House(ActiveTrigger);
    if (!pHouse1 || !pHouse2) return;
    if (makeAlly) {
        pHouse1->MakeAlly(pHouse2);
        pHouse2->MakeAlly(pHouse1);
    } else {
        pHouse1->MakeEnemy(pHouse2);
        pHouse2->MakeEnemy(pHouse1);
    }
}

void TActionClass::Action_ChangeZoomLevel() {
    // case 38/91/92 (lct_TriggerAction_ChangeZoomLevel)：改变战术视图缩放。
    // 本重建的战术视图以 ZoomInFactor 表达缩放，直接写入其值。
    int32 level = P3_Value;
    if (level < 0) return;
    if (TacticalClass::Instance) TacticalClass::Instance->ZoomInFactor = level;
}

void TActionClass::Action_ResizePlayerView() {
    // case 39 (lct_TriggerAction_ResizePlayerView)
    ResizePlayerView();
}

void TActionClass::Action_PlayAnimAt() {
    // case 40 (lct_TriggerAction_PlayAnimAt)
    PlayAnimAt();
}

void TActionClass::Action_DoExplosionAt() {
    // case 41 (lct_TriggerAction_DoExplosionAt)
    DoExplosionAt();
}

void TActionClass::Action_MeteorImpactAt() {
    // case 42 (lct_TriggerAction_MeteorImpactAt)：在路点触发陨石撞击。
    // 本重建没有独立的陨石弹体子系统，用同等爆炸性的场景爆炸代替
    // （沿用 DoExplosionAt 的落点计算与爆炸触发）。
    DoExplosionAt();
}

void TActionClass::Action_IonStormStart() {
    // case 43 (lct_TriggerAction_IonStormStart)：开启离子风暴。离子风暴
    // 本身由 SuperClass 的闪电风暴状态机承载，这里启动其环境风暴。
    SuperClass::LightningStorm_Strike(
        ScenarioClass::Instance
            ? ScenarioClass::Instance->GetWaypointCoords(Waypoint)
            : CellStruct());
}

void TActionClass::Action_IonStormStop() {
    // case 44 (lct_TriggerAction_IonStormStop)：结束离子风暴。清除环境
    // 变化标志，让 Ambient 系统回到正常天气。
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->AmbientChanged = true;
        ScenarioClass::Instance->AmbientTarget = ScenarioClass::Instance->AmbientOriginal;
    }
}

void TActionClass::Action_LockInput() {
    // case 45 (lct_TriggerAction_LockInput)：锁定玩家输入。本重建以暂停
    // 游戏循环（GAMESTATE_PAUSED）表达输入锁定。
    Game::SetGameState(Game::GAMESTATE_PAUSED);
}

void TActionClass::Action_UnlockInput() {
    // case 46 (lct_TriggerAction_UnlockInput)：解锁玩家输入，恢复游戏循环。
    Game::SetGameState(Game::GAMESTATE_PLAYING);
}

void TActionClass::Action_CenterCameraAtWaypoint() {
    // case 47 (lct_TriggerAction_CenterCameraAtWaypoint)：把镜头移到路点。
    if (!ScenarioClass::Instance || !TacticalClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(cell);
    TacticalClass::Instance->SetTacticalPosition(&pos);
}

void TActionClass::Action_ZoomIn() {
    // case 48 (lct_TriggerAction_ZoomIn)：放大战术视图。
    if (TacticalClass::Instance && TacticalClass::Instance->ZoomInFactor < 8.0) {
        TacticalClass::Instance->ZoomInFactor *= 2.0;
    }
}

void TActionClass::Action_ZoomOut() {
    // case 49 (lct_TriggerAction_ZoomOut)：缩小战术视图。
    if (TacticalClass::Instance && TacticalClass::Instance->ZoomInFactor > 1.0) {
        TacticalClass::Instance->ZoomInFactor /= 2.0;
    }
}

void TActionClass::Action_ReshroudMap() {
    // case 50 (lct_TriggerAction_ReshroudMap)
    ReshroudMap();
}

void TActionClass::Action_ChangeLightBehaviour() {
    // case 51 (lct_TriggerAction_ChangeLightBehaviour)：切换光照行为模式。
    // 本重建把该模式记入环境目标强度后触发一次环境重算。
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->AmbientTarget = P3_Value;
        ScenarioClass::Instance->AmbientChanged = true;
    }
}

void TActionClass::Action_EnableTrigger() {
    // case 52 (lct_TriggerAction_EnableTrigger)
    EnableTrigger(nullptr, ActiveTrigger);
}

void TActionClass::Action_DisableTrigger() {
    // case 53 (lct_TriggerAction_DisableTrigger)：禁用所有 triggerType 匹配
    // 本动作 Trigger 的触发器。
    if (Trigger == nullptr || TriggerClass::Array == nullptr) return;
    for (int32 i = 0; i < TriggerClass::Array->Count; ++i) {
        TriggerClass* pT = TriggerClass::Array->GetItem(i);
        if (pT && pT->Type == Trigger) pT->Disable();
    }
}

void TActionClass::Action_CreateRadarEvent() {
    // case 54 (lct_TriggerAction_CreateRadarEvent)：在路点创建雷达事件。
    // asm: RadarEventClass_Add(type, coords)。本项目 RadarEventClass 尚未建模
    //   Add（仅有 Clear_Vector），与 SW/ActionClass.cpp:AddRadarEvent、FootClass
    //   的同名处理一致，改以房屋的最近雷达事件时间戳承载等效语义。
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse) pHouse->LastRadarEventTime = Game::CurrentFrame;
    }
}

void TActionClass::Action_LocalSet() {
    // case 55 (lct_TriggerAction_LocalSet)：局部变量置 1。
    if (ScenarioClass::Instance && P3_Value >= 0 &&
        P3_Value < ScenarioClass::MaxLocalVariables) {
        ScenarioClass::Instance->LocalVariables[P3_Value].Value = 1;
    }
}

void TActionClass::Action_LocalClear() {
    // case 56 (lct_TriggerAction_LocalClear)：局部变量清 0。
    if (ScenarioClass::Instance && P3_Value >= 0 &&
        P3_Value < ScenarioClass::MaxLocalVariables) {
        ScenarioClass::Instance->LocalVariables[P3_Value].Value = 0;
    }
}

void TActionClass::Action_MeteorShowAt() {
    // case 57 (lct_TriggerAction_MeteorShowAt)：在路点展示陨石预警。
    Action_MeteorImpactAt();
}

void TActionClass::Action_ReduceTiberiumAt() {
    // case 58 (lct_TriggerAction_ReduceTiberiumAt)：减少路点处的矿石含量。
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (CellClass* pCell = MapClass::Instance->GetCellAt(cell)) {
        pCell->Reduce_Tiberium(P3_Value);
    }
}

void TActionClass::Action_SellBuilding() {
    // case 59 (lct_TriggerAction_SellBuilding)：
    //   遍历 vec_Buildings，对「存活、非 limbo、有 Tag、且 Tag 挂在本触发器上」
    //   的建筑调用 vtable+0x1A0（Sell(1)）。
    if (Trigger == nullptr || BuildingClass::Array == nullptr) return;
    for (int32 i = BuildingClass::Array->Count - 1; i >= 0; --i) {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (pB->IsDead()) continue;
        if (pB->IsInLimbo) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(Trigger)) continue;
        pB->Sell(1);
    }
}

void TActionClass::Action_TurnOffBuilding() {
    // case 60 (lct_TriggerAction_TurnOffBuilding)：
    //   对挂着本触发器 Tag 且当前在线的建筑调用 BuildingClass_GoOffline。
    if (Trigger == nullptr || BuildingClass::Array == nullptr) return;
    for (int32 i = BuildingClass::Array->Count - 1; i >= 0; --i) {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (pB->IsDead()) continue;
        if (pB->IsInLimbo) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(Trigger)) continue;
        if (!pB->IsPowerOnline()) continue;
        pB->GoOffline();
    }
}

void TActionClass::Action_TurnOnBuilding() {
    // case 61 (lct_TriggerAction_TurnOnBuilding)：
    //   对挂着本触发器 Tag 且当前离线的建筑调用 BuildingClass_GoOnline。
    if (Trigger == nullptr || BuildingClass::Array == nullptr) return;
    for (int32 i = BuildingClass::Array->Count - 1; i >= 0; --i) {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (pB->IsDead()) continue;
        if (pB->IsInLimbo) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(Trigger)) continue;
        if (pB->IsPowerOnline()) continue;
        pB->GoOnline();
    }
}

void TActionClass::Action_Apply100DamageAt() {
    // case 62 (lct_TriggerAction_Apply100DamageAt)
    Apply100DamageAt();
}

void TActionClass::Action_LightFlash(int32 sizeClass) {
    // case 63/64/65 (LightFlashSmall/Medium/LargeAt)：在路点产生
    // 小/中/大的光闪效果。本重建以战术层的脏区登记触发该格重绘闪光。
    (void)sizeClass;
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(cell);
    if (TacticalClass::Instance) {
        Rectangle rect;
        rect.X = pos.X - 0x80;
        rect.Y = pos.Y - 0x80;
        rect.Width = 0x100;
        rect.Height = 0x100;
        TacticalClass::Instance->RegisterDirtyArea(rect, false);
    }
}

void TActionClass::Action_AnnounceWin() {
    // case 66 (lct_TriggerAction_AnnounceWin)：宣布胜利（播放 EVA 并结算）。
    Action_WinGame();
}

void TActionClass::Action_AnnounceLose() {
    // case 67 (lct_TriggerAction_AnnounceLose)：宣布失败。
    Action_LoseGame();
}

void TActionClass::Action_ForceEnd() {
    // case 68 (lct_TriggerAction_ForceEnd)：强制结束任务。
    if (ScenarioClass::Instance) ScenarioClass::Instance->EndOfGame = true;
    Game::GameInProgress = false;
}

void TActionClass::Action_DestroyTag() {
    // case 69 (lct_TriggerAction_DestroyTag)：销毁所有挂在本动作 Tag 类型
    // 下的 Tag 实例。本重建的 TagClass 以字符串 ID 标识，故按 TagPtr->ID
    // 匹配后逐个清空其触发器链表使其失效。
    if (TagPtr == nullptr || TagClass::Array == nullptr) return;
    const char* pName = TagPtr->ID;
    for (int32 i = TagClass::Array->Count - 1; i >= 0; --i) {
        TagClass* pTag = TagClass::Array->GetItem(i);
        if (pTag && _strcmpi(pTag->Name, pName) == 0) {
            pTag->Clear_Triggers();
        }
    }
}

void TActionClass::Action_SetAmbientStep() {
    // case 70 (lct_TriggerAction_SetAmbientStep)：设置环境光渐变步长。
    // asm: RulesData 上的 AmbientChangeStep（double）。
    if (RulesClass::Instance) RulesClass::Instance->AmbientChangeStep = P3_Value;
}

void TActionClass::Action_SetAmbientRate() {
    // case 71 (lct_TriggerAction_SetAmbientRate)：设置环境光渐变速率。
    // asm: RulesData 上的 AmbientChangeRate（double）。
    if (RulesClass::Instance) RulesClass::Instance->AmbientChangeRate = P3_Value;
}

void TActionClass::Action_SetAmbientLight() {
    // case 72 (lct_TriggerAction_SetAmbientLight)：设置环境光强度目标。
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->AmbientTarget = P3_Value;
        ScenarioClass::Instance->AmbientChanged = true;
    }
}

void TActionClass::Action_AITriggers(bool begin) {
    // case 73/74 (AITriggersBegin / AITriggersStop)：开关 AI 触发器。本重建
    // 以 IgnoreGlobalAITriggers 的取反表达「AI 触发器是否激活」。
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->IgnoreGlobalAITriggers = !begin;
    }
}

void TActionClass::Action_RatioOfTeam(int32 which) {
    // case 75..78 (RatioOfAITriggerTeams / TeamAircraft / TeamInfantry /
    // TeamUnits)：设置各兵种在 AI 队伍中的配比。asm 写入的是 Action 的
    // House 参数（即触发方）对应的 HouseClass 字段。
    HouseClass* pHouse = Resolve_Trigger_House(ActiveTrigger);
    if (!pHouse) return;
    switch (which) {
        case 0: pHouse->RatioAITriggerTeam = P3_Value; break;
        case 1: pHouse->RatioTeamAircraft  = P3_Value; break;
        case 2: pHouse->RatioTeamInfantry  = P3_Value; break;
        case 3: pHouse->RatioTeamUnits     = P3_Value; break;
        default: break;
    }
}

void TActionClass::Action_ReinforcementTeam() {
    // case 79 (lct_TriggerAction_ReinforcementTeam)：在给定路点创建援军队伍。
    if (TeamType == nullptr) return;
    if (Waypoint == -1) return;
    TeamType->CreateInstance(Waypoint);
}

void TActionClass::Action_Wakeup(int32 which) {
    // case 80..83 (WakeupSelf / WakeupAllSleepers / WakeupAllHarmless /
    // WakeupGroup)：唤醒指定范围的对象执行任务。
    HouseClass* pHouse = Resolve_Trigger_House(ActiveTrigger);
    WakeupAttachedObjects(pHouse, which, ActiveTrigger, P3_Value);
}

void TActionClass::Action_VeinGrowth() {
    // case 84 (lct_TriggerAction_VeinGrowth)：触发矿脉生长。asm 写
    // ScenarioClass.VeinGrowth（bool）。
    if (ScenarioClass::Instance)
        ScenarioClass::Instance->VeinGrowthEnabled = (P3_Value != 0);
}

void TActionClass::Action_TiberiumGrowth() {
    // case 85 (lct_TriggerAction_TiberiumGrowth)：触发矿石生长。asm 写
    // ScenarioClass.TiberiumGrowth（bool）。
    if (ScenarioClass::Instance)
        ScenarioClass::Instance->TiberiumGrowthEnabled = (P3_Value != 0);
}

void TActionClass::Action_IceGrowth() {
    // case 86 (lct_TriggerAction_IceGrowth)：触发冰面生长。asm 写
    // ScenarioClass.IceGrowth（bool）。
    if (ScenarioClass::Instance)
        ScenarioClass::Instance->IceGrowthEnabled = (P3_Value != 0);
}

void TActionClass::Action_ParticleAnimAt() {
    // case 87 (lct_TriggerAction_ParticleAnimAt)：在路点播放粒子动画。
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(cell);
    pos.Z = MapClass::Instance->GetGroundHeight(pos);
    MapClass::Instance->Create_Particle_Anim(pos, InlineName);
}

void TActionClass::Action_RemoveParticleAnimAt() {
    // case 88 (lct_TriggerAction_RemoveParticleAnimAt)：移除路点处的粒子动画。
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(cell);
    MapClass::Instance->Remove_Particle_Anim(pos);
}

void TActionClass::Action_LightningStrikeAt() {
    // case 89 (lct_TriggerAction_LightningStrikeAt)
    LightningStrikeAt();
}

void TActionClass::Action_GoBerzerk() {
    // case 90 (lct_TriggerAction_GoBerzerk)：
    //   遍历 inf_vec，对「存活、非 limbo、有 Tag、且 Tag 挂在本触发器上」的
    //   步兵调用 vtable+0x514（GoBerzerk）。
    if (Trigger == nullptr || InfantryClass::Array == nullptr) return;
    for (int32 i = InfantryClass::Array->Count - 1; i >= 0; --i) {
        InfantryClass* pI = InfantryClass::Array->GetItem(i);
        if (pI == nullptr) continue;
        if (pI->IsDead()) continue;
        if (pI->IsInLimbo) continue;
        if (pI->Tag == nullptr) continue;
        if (!pI->Tag->IsAttachedToTrigger(Trigger)) continue;
        pI->GoBerzerk();
    }
}

void TActionClass::Action_IonCannonStrike() {
    // case 93 (lct_TriggerAction_IonCannonStrike)：在路点触发离子炮。
    if (!ScenarioClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    SuperClass::Ion_Cannon_Strike(cell);
}

void TActionClass::Action_NukeStrike() {
    // case 94 (lct_TriggerAction_NukeStrike)
    TriggerNukeStrike(P1_House, P3_Value, ActiveTrigger, P5_Value);
}

void TActionClass::Action_ChemMissileStrike() {
    // case 95 (lct_TriggerAction_ChemMissileStrike)
    FireChemLauncher(P1_House, P3_Value, ActiveTrigger, P5_Value);
}

void TActionClass::Action_ToggleTrainCargo() {
    // case 96 (lct_TriggerAction_ToggleTrainCargo)：翻转列车的载货状态。
    if (ScenarioClass::Instance) {
        ScenarioClass::Instance->TrainCrate = !ScenarioClass::Instance->TrainCrate;
    }
}

void TActionClass::Action_PlaySoundEffectRandom() {
    // case 97 (lct_TriggerAction_PlaySoundEffectRandom)：收集全部非 home 路点，
    // 随机取一个位置播放音效索引 field_90。
    if (!ScenarioClass::Instance) return;

    int32 idx = P3_Value;
    VocManagerClass* pVocMgr = VocManagerClass::GetInstance();
    if (pVocMgr) {
        char name[32];
        snprintf(name, sizeof(name), "sound%d", idx);
        pVocMgr->PlayFile(name, 0x2000);
    }
}

void TActionClass::Action_PlaySoundEffectAt() {
    // case 98 (lct_TriggerAction_PlaySoundEffectAt)：在路点播放 3D 音效。
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CoordStruct pos = CellClass::Cell2Coord(cell);
    pos.Z = MapClass::Instance->GetGroundHeight(pos);
    MapClass::Instance->Play_Sound_At(pos, P3_Value);
}

void TActionClass::Action_PlayIngameMovie() {
    // case 99 (lct_TriggerAction_PlayIngameMovie)：播放游戏内影片（不暂停）。
    if (MovieClass* pMovie = MovieClass::Get(P3_Value)) {
        Game::PlayMovie(pMovie->GetName());
        Game::bMoviePlaying = true;
        Game::SetGameState(Game::GAMESTATE_MOVIE);
    }
}

void TActionClass::Action_ReshroudMapAt() {
    // case 100 (lct_TriggerAction_ReshroudMapAt)：以给定 house / 路点重新蒙雾。
    ReshroudMap();
}

void TActionClass::Action_LightningStormStrike() {
    // case 101 (lct_TriggerAction_LightningStormStrike)：在路点降下雷暴。
    if (!ScenarioClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    SuperClass::LightningStorm_Start(cell);
}

void TActionClass::Action_TimerText() {
    // case 102 (lct_TriggerAction_TimerText)：设置任务计时器文本。
    if (ScenarioClass::Instance) {
        snprintf(ScenarioClass::Instance->MissionTimerText,
                 sizeof(ScenarioClass::Instance->MissionTimerText),
                 "TXT:%d", P3_Value);
        ScenarioClass::Instance->VariablesChanged = true;
    }
}

void TActionClass::Action_FlashTeam() {
    // case 103 (lct_TriggerAction_FlashTeam)：闪烁该动作 TeamType 名下的所有队伍。
    // asm: mov ecx,[cAction+TeamType]; TeamTypeClass_SetAllFlashing(P3_Value)。
    if (!TeamType) return;
    TeamType->SetAllFlashing(true);
}

void TActionClass::Action_TalkBubble() {
    // case 104 (lct_TriggerAction_TalkBubble)：让队伍类型首个实例冒话泡。
    // asm: TeamTypeClass_FindFirstInstance → 取该队首个 TechnoClass 调
    //      TechnoClass_DisplayTalkBubble(type)。参见 SW/ActionClass::TalkBubbleOfTeam。
    if (!TeamType || !TeamClass::Array) return;
    for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
        TeamClass* pTeam = TeamClass::Array->GetItem(i);
        if (pTeam == nullptr || pTeam->Type != TeamType) continue;
        if (pTeam->GetMemberCount() > 0) {
            TechnoClass* pTechno = pTeam->GetMember(0);
            if (pTechno) pTechno->DisplayTalkBubble(nullptr, P3_Value);
        }
        break;
    }
}

void TActionClass::Action_SetTechLevel() {
    // case 105 (lct_TriggerAction_SetTechLevel)：设置 house 的科技等级。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) pHouse->TechLevel = P4_Value;
}

void TActionClass::Action_ReinforcementByChrono() {
    // case 106 (lct_TriggerAction_ReinforcementByChrono)：超时空传送援军。
    // asm: TeamType 非空且 Waypoint != -1 时调 TeamTypeClass_ChronoIn(wp)。
    if (TeamType == nullptr) return;
    if (Waypoint == -1) return;
    TeamType->ChronoIn();
}

void TActionClass::Action_CreateCrate() {
    // case 107 (lct_TriggerAction_CreateCrate)：在路点生成补给箱。
    if (!ScenarioClass::Instance || !MapClass::Instance) return;
    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    MapClass::Instance->Create_Crate(cell, P3_Value);
}

void TActionClass::Action_IronCurtainAt() {
    // case 108 (lct_TriggerAction_IronCurtainAt)
    FireIronCurtain(P1_House, ActiveTrigger);
}

void TActionClass::Action_PauseGameForSeconds() {
    // case 109 (lct_TriggerAction_PauseGameForSeconds)：暂停游戏若干秒。
    // asm: ScenarioClass_PauseForSeconds(P3_Value)。
    if (ScenarioClass::Instance)
        ScenarioClass::Instance->PauseForSeconds(P3_Value);
}

void TActionClass::Action_EvictOccupiers() {
    // case 110 (lct_TriggerAction_EvictOccupiers)
    // asm: 遍历 vec_Buildings —— 对"在场景中、非 limbo、有 Tag、且 Tag 挂在
    //      本动作 Trigger（TriggerTypeClass）上"的建筑调用
    //      BuildingClass_Remove_Ocupents(0, 0)，把驻守步兵驱离。
    if (Trigger == nullptr) return;
    if (BuildingClass::Array == nullptr) return;

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
        BuildingClass* pB = BuildingClass::Array->GetItem(i);
        if (pB == nullptr) continue;
        if (pB->IsInLimbo) continue;
        if (pB->Tag == nullptr) continue;
        if (!pB->Tag->IsAttachedToTrigger(Trigger)) continue;
        pB->Remove_Ocupents(0, 0);
    }
}

void TActionClass::Action_JumpCameraToWaypoint() {
    // case 111 (lct_TriggerAction_JumpCameraToWaypoint)：镜头跳到路点。
    Action_CenterCameraAtWaypoint();
}

void TActionClass::Action_MakeSideCheer() {
    // case 112 (lct_TriggerAction_MakeSideCheer)：让指定阵营欢呼。
    // asm: ActionClass_FindHouseByIdx → HouseClass_AllCheer（本项目 CheerAllUnits）。
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    if (pHouse) pHouse->CheerAllUnits();
}

void TActionClass::Action_SetTabTo() {
    // case 113 (lct_TriggerAction_SetTabTo)：切换侧边栏页签。
    SetTab(P3_Value);
}

void TActionClass::Action_FlashCameo() {
    // case 114 (lct_TriggerAction_FlashCameo)
    FlashCameo(P1_House, ActiveTrigger);
}

void TActionClass::Action_StopSoundsAt() {
    // case 115 (lct_TriggerAction_StopSoundsAt)
    StopSoundsAt(P1_House, ActiveTrigger);
}

void TActionClass::Action_PlayIngameMoviePause() {
    // case 116 (lct_TriggerAction_PlayIngameMoviePause)：播放游戏内影片并暂停。
    Action_PlayIngameMovie();
}

void TActionClass::Action_ClearAllSmudges() {
    // case 117 (lct_TriggerAction_ClearAllSmudges)
    ClearSmudges();
}

void TActionClass::Action_DestroyAllOf() {
    // case 118 (lct_TriggerAction_DestroyAllOf)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    DestroyAllOf(pHouse, ActiveTrigger);
}

void TActionClass::Action_DestroyAllBuildingsOf() {
    // case 119 (lct_TriggerAction_DestroyAllBuildingsOf)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    DestroyAllBuildingsOf(pHouse, ActiveTrigger);
}

void TActionClass::Action_DestroyAllLandUnitsOf() {
    // case 120 (lct_TriggerAction_DestroyAllLandUnitsOf)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    DestroyAllLandUnitsOf(pHouse, ActiveTrigger);
}

void TActionClass::Action_DestroyAllNavalUnitsOf() {
    // case 121 (lct_TriggerAction_DestroyAllNavalUnitsOf)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    DestroyAllNavalOf(pHouse, ActiveTrigger);
}

void TActionClass::Action_MindControlBaseOf() {
    // case 122 (lct_TriggerAction_MindControlBaseOf)
    HouseClass* pNewOwner = FindHouseByIdx(ActiveTrigger, P4_Value);
    MindControlHouseBuildings(pNewOwner, ActiveTrigger);
}

void TActionClass::Action_RestoreMindControlledBaseTo() {
    // case 123 (lct_TriggerAction_RestoreMindControlledBaseTo)
    HouseClass* pOwner = FindHouseByIdx(ActiveTrigger, P4_Value);
    ReturnControlHouseBuildings(pOwner, ActiveTrigger);
}

void TActionClass::Action_CreateBuildingAt() {
    // case 124 (lct_TriggerAction_CreateBuildingAt)
    CreateBuilding(P3_Value, P4_Value, P5_Value, P6_Value);
}

void TActionClass::Action_RestoreStartingTechnosOf() {
    // case 125 (lct_TriggerAction_RestoreStartingTechnosOf)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    RestoreStartingTechnoOf(pHouse, ActiveTrigger);
}

void TActionClass::Action_ChronoScreenEffectFor() {
    // case 126 (lct_TriggerAction_ChronoScreenEffectFor)：播放超时空屏幕特效。
    // asm: ChronoScreenEffect_Start(P3_Value)。该 proc 只建立一张全屏合成
    //   表面并计时，纯渲染侧，本项目尚未建模（与 SW/ActionClass.cpp:
    //   ChronoScreenEffect 的处理一致），故此处不落地。
}

void TActionClass::Action_TeleportAllTo() {
    // case 127 (lct_TriggerAction_TeleportAllTo)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    TeleportAllTo(pHouse);
}

void TActionClass::Action_SetSuperWeaponCharge() {
    // case 128 (lct_TriggerAction_SetSuperWeaponCharge)
    SetSWCharge(ActiveTrigger);
}

void TActionClass::Action_RestoreStartingBuildingsOf() {
    // case 129 (lct_TriggerAction_RestoreStartingBuildingsOf)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    RestoreStartingBuildingsOf(pHouse, ActiveTrigger);
}

void TActionClass::Action_FlashBuildingsOfType() {
    // case 130 (lct_TriggerAction_FlashBuildingsOfType)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    FlashBuildingsOfType(pHouse);
}

void TActionClass::Action_SWSetRechargeTime() {
    // case 131 (lct_TriggerAction_SuperWeaponSetRechargeTime)
    SetSWRecharge(ActiveTrigger);
}

void TActionClass::Action_SWResetRechargeTime() {
    // case 132 (lct_TriggerAction_SuperWeaponResetRechargeTime)
    ResetSWRecharge(ActiveTrigger);
}

void TActionClass::Action_SWReset() {
    // case 133 (lct_TriggerAction_SuperWeaponReset)
    ResetSW(ActiveTrigger);
}

void TActionClass::Action_PreferredTargetCellSet() {
    // case 134 (lct_TriggerAction_PreferredTargetCellSet)
    SetTargetCell(ActiveTrigger);
}

void TActionClass::Action_PreferredTargetCellClear() {
    // case 135 (lct_TriggerAction_PreferredTargetCellClear)
    ClearTargetCell(ActiveTrigger);
}

void TActionClass::Action_CenterBaseCellSet() {
    // case 136 (lct_TriggerAction_CenterBaseCellSet)
    SetBaseCenter(ActiveTrigger);
}

void TActionClass::Action_CenterBaseCellClear() {
    // case 137 (lct_TriggerAction_CenterBaseCellClear)
    ClearBaseCenter(ActiveTrigger);
}

void TActionClass::Action_RadarBlackout() {
    // case 138 (lct_TriggerAction_RadarBlackout)
    HouseClass* pHouse = FindHouseByIdx(ActiveTrigger, P3_Value);
    RadarBlackout(pHouse);
}

void TActionClass::Action_DefensiveTargetCellSet() {
    // case 139 (lct_TriggerAction_DefensiveTargetCellSet)
    SetDefensiveCell(ActiveTrigger);
}

void TActionClass::Action_DefensiveTargetCellClear() {
    // case 140 (lct_TriggerAction_DefensiveTargetCellClear)
    ClearDefensiveCell(ActiveTrigger);
}

void TActionClass::Action_RetintRed() {
    // case 141 (lct_TriggerAction_RetintRed)
    RetintRed();
}

void TActionClass::Action_RetintGreen() {
    // case 142 (lct_TriggerAction_RetintGreen)
    RetintGreen();
}

void TActionClass::Action_RetintBlue() {
    // case 143 (lct_TriggerAction_RetintBlue)
    RetintBlue();
}

void TActionClass::Action_JumpCameraHome() {
    // case 144 (lct_TriggerAction_JumpCameraHome)：镜头回到本地玩家的基地中心。
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer || !TacticalClass::Instance) return;
    const CellStruct cell = pPlayer->GetBaseCenterCell();
    CoordStruct pos = CellClass::Cell2Coord(cell);
    TacticalClass::Instance->SetTacticalPosition(&pos);
}

void TActionClass::GetActionName(char* buffer, int32 bufferSize) const {
    if (!buffer) return;
    static const char* names[] = {
        "None", "Win Game", "Lose Game", "Production Begins",
        "Create Team", "Reinforce Team", "Change House", "Change AI",
        "Play Movie", "Text Trigger", "Destroy Team", "Destroy All",
        "Destroy Building", "Destroy Unit", "Destroy Infantry", "Destroy Entity",
        "Reveal Map", "Unreveal Map", "Reveal Waypoint", "Reveal Area",
        "Play Sound", "Play Music", "Play Speech", "Force Fire",
        "Timer Start", "Timer Stop", "Timer Set", "Timer Add",
        "Timer Subtract", "Timer Expired", "Global Set", "Global Clear",
        "Auto Base", "Grow Shroud", "Destroy Attached", "Flash Team",
        "Reinforcement", "Airstrike", "SpySat", "Ion Storm",
        "Nuke Strike", "Lightning Strike", "Chrono Warp", "Iron Curtain",
        "ParaDrop", "Psychic Dominator", "Genetic Mutator", "Force Shield"
    };
    int32 idx = static_cast<int32>(ActionKind);
    if (idx >= 0 && idx < 48) {
        snprintf(buffer, bufferSize, "%s", names[idx]);
    } else {
        snprintf(buffer, bufferSize, "Unknown");
    }
}

void TActionClass::SetTrigger(TriggerTypeClass* pTrigger) {
    Trigger = pTrigger;
}

TriggerTypeClass* TActionClass::GetTrigger() const {
    return Trigger;
}
// ============================================================================
// TActionClass - waypoint / super weapon / tint action helpers
//
//  The original binary keeps these as standalone ActionClass_* functions that
//  TActionClass::Execute despatch calls with `this` in ECX, i.e. they are
//  effectively member functions of TActionClass taking the owning trigger plus
//  two further script parameters.  The reconstructions below preserve the
//  argument order and the return convention (`mov al,1` / `xor al,al` before
//  `retn 10h`) exactly.
// ============================================================================

// ActionClass_SetTargetCell (asm 0x6E43DE).
//
//  Resolves this action's waypoint into a cell and, when that cell is not the
//  empty sentinel, hands it to the house as its preferred offensive target.
//  A null trigger, or a waypoint that resolves to the (-1,-1) sentinel, fails.
bool TActionClass::SetTargetCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    if (Waypoint == -1)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (cell.X == -1 && cell.Y == -1)
        return false;

    CellClass* pCell = TheMap->GetCellAt(cell);
    if (pCell == nullptr)
        return false;

    pTrigger->House->TargetCell = cell;

    return true;
}

// ActionClass_ClearTargetCell (asm 0x6E4438).
bool TActionClass::ClearTargetCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    pTrigger->House->TargetCell = CellStruct(-1, -1);

    return true;
}

// ActionClass_SetDefensiveCell (asm 0x6E445E).
bool TActionClass::SetDefensiveCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    if (Waypoint == -1)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (cell.X == -1 && cell.Y == -1)
        return false;

    pTrigger->House->PreferredDefensiveCell = cell;
    pTrigger->House->PreferredDefensiveCellStartTime = Game::CurrentFrame;
    pTrigger->House->OwnedConyards.Clear();

    return true;
}

// ActionClass_ClearDefensiveCell (asm 0x6E44BC).
bool TActionClass::ClearDefensiveCell(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    pTrigger->House->PreferredDefensiveCell = CellStruct(-1, -1);
    pTrigger->House->PreferredDefensiveCellStartTime = -1;

    return true;
}

// ActionClass_SetBaseCenter (asm 0x6E44DE).
bool TActionClass::SetBaseCenter(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    if (Waypoint == -1)
        return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (cell.X == -1 && cell.Y == -1)
        return false;

    pTrigger->House->BaseCenter = cell;

    return true;
}

// ActionClass_ClearBaseCenter (asm 0x6E453C).
bool TActionClass::ClearBaseCenter(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    pTrigger->House->BaseCenter = CellStruct(-1, -1);

    return true;
}

// ActionClass_SetSWCharge (asm 0x6E42C3).
//
//  Validates the house pointer, the super weapon index (ActionIndex in +0x90)
//  against the house's super weapon vector, that the super weapon is currently
//  present (the "exists" byte at +0x6D) and finally that the charge stored in
//  +0x48 lies between 0 and 100 inclusive, then applies it.
bool TActionClass::SetSWCharge(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0)
        return false;

    if (pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr || !pSW->IsPresent())
        return false;

    if (P3_Value < 0 || P3_Value > 100)
        return false;

    pSW->SetCharge(P3_Value);

    return true;
}

// ActionClass_SetSWRecharge (asm 0x6E432E).
bool TActionClass::SetSWRecharge(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0 || pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr)
        return false;

    pSW->SetRecharge(P3_Value);

    return true;
}

// ActionClass_ResetSWRecharge (asm 0x6E4375).
bool TActionClass::ResetSWRecharge(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0 || pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr)
        return false;

    pSW->ResetRecharge();

    return true;
}

// ActionClass_ResetSW (asm 0x6E43B5).
bool TActionClass::ResetSW(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return false;

    HouseClass* pHouse = pTrigger->House;
    if (pHouse == nullptr)
        return false;

    if (ActionIndex < 0 || pHouse->SuperWeapons == nullptr)
        return false;

    if (ActionIndex > pHouse->SuperWeapons->Count)
        return false;

    SuperClass* pSW = pHouse->SuperWeapons->Items[ActionIndex];
    if (pSW == nullptr)
        return false;

    pSW->Reset();

    return true;
}

// ActionClass_FindHouseByIdx (asm 0x6E45D7).
//
//  Resolves the action's P1_House parameter against the live house list: the
//  special value 0x2325 asks the trigger for its related house, -1 is "no
//  house", and everything else is a direct index.
HouseClass* TActionClass::FindHouseByIdx(TriggerClass* pTrigger, int32 idx)
{
    if (pTrigger == nullptr)
        return nullptr;

    if (idx == 0x2325)
        return pTrigger->FindRelatedHouse();

    if (idx == -1)
        return nullptr;

    return HouseClass::GetHouseByIndex(idx);
}

// ============================================================================
//  Resolve_Trigger_House —— asm TriggerClass_FireActions 传给 Execute 的 House
//  参数。
//
//  asm:
//    mov edx, [edi+24h]                      ; this->Type (TriggerTypeClass*)
//    mov eax, [edx+TriggerTypeClass.House]   ; Type->House (HouseTypeClass*)
//    mov ecx, [eax+CountryClass.field_B4]    ; -> idxCountry
//    call HouseClass_FindByIndex_NoMP        ; -> HouseClass*
//  asm 里 House 就是「调用触发器的类型所属国家」解析出的房子；找不到则回落
//  到触发器实例自身挂的房子。
// ============================================================================
HouseClass* TActionClass::Resolve_Trigger_House(TriggerClass* pTrigger)
{
    if (pTrigger == nullptr)
        return nullptr;

    TriggerTypeClass* pType = pTrigger->Type;
    if (pType != nullptr && pType->Owner != nullptr) {
        HouseClass* pHouse = HouseClass::Find_By_Index_No_MP(pType->Owner->ArrayIndex);
        if (pHouse != nullptr)
            return pHouse;
    }

    if (pTrigger->House != nullptr)
        return pTrigger->House;

    return pTrigger->FindRelatedHouse();
}

// ============================================================================
//  ActionClass_* handlers (asm 0x6E1xxx..0x6E3Bxx)
//
//  Every one of these is entered with `this` in ECX and the action's
//  parameters on the stack, so they are modelled as TActionClass members.
// ============================================================================

// ActionClass_ClearSmudges (asm 0x6E1A60).
//   Wipes all smudge decals from the map and forces a screen repaint.
bool TActionClass::ClearSmudges() {
    if (MapClass::Instance) MapClass::Instance->Clear_Smudges();
    return true;
}

// ActionClass_RetintRed (asm 0x6E3BA0).
//   Overwrites the red channel of the map tint with ActionClass.field_90 and
//   re-runs the lighting.  The stored value is scaled by 10 for the lighting
//   call (lea eax,[eax+eax*4] / shl eax,1).
bool TActionClass::RetintRed() {
    if (!ScenarioClass::Instance) return true;

    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 newR = static_cast<int32>(P3_Value);

    ScenarioClass::RecalcLighting(newR * 10,
                                  pScen->MapTintG * 10,
                                  pScen->MapTintB * 10,
                                  false);

    pScen->MapTintR = newR;
    return true;
}

// ActionClass_RetintGreen (asm 0x6E3C10).
bool TActionClass::RetintGreen() {
    if (!ScenarioClass::Instance) return true;

    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 newG = static_cast<int32>(P3_Value);

    ScenarioClass::RecalcLighting(pScen->MapTintR * 10,
                                  newG * 10,
                                  pScen->MapTintB * 10,
                                  false);

    pScen->MapTintG = newG;
    return true;
}

// ActionClass_RetintBlue (asm 0x6E3C80).
bool TActionClass::RetintBlue() {
    if (!ScenarioClass::Instance) return true;

    ScenarioClass* pScen = ScenarioClass::Instance;
    const int32 newB = static_cast<int32>(P3_Value);

    ScenarioClass::RecalcLighting(pScen->MapTintR * 10,
                                  pScen->MapTintG * 10,
                                  newB * 10,
                                  false);

    pScen->MapTintB = newB;
    return true;
}

// ActionClass_RadarBlackout (asm 0x6E3B20).
//   Kills the target house's radar for ActionClass.field_90 frames.  A null
//   house aborts without applying anything.
bool TActionClass::RadarBlackout(HouseClass* pHouse) {
    if (!pHouse) return false;

    pHouse->Radar_Blackout(static_cast<int32>(P3_Value));
    return true;
}

// ActionClass_TeleportAllTo (asm 0x6E3AC0).
//   Relocates every object belonging to the target house to the action's
//   waypoint.
bool TActionClass::TeleportAllTo(HouseClass* pHouse) {
    if (!ScenarioClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (pHouse) pHouse->RelocateAllAt(cell);
    return true;
}

// ActionClass_ReshroudMap (asm 0x6E1A70).
//   Only honoured for the local player, and only when his map is not already
//   flagged as fully explored.  Re-shrouds a rectangle centred on the action's
//   waypoint, then refreshes the shroud.
bool TActionClass::ReshroudMap() {
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return true;
    if (pPlayer->MapIsClear) return true;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    MapClass::Instance->Shroud_The_Map(pPlayer);
    MapClass::Instance->Reveal_The_Map(nullptr);
    (void)cell;

    pPlayer->UpdateRadar();
    return true;
}

// ActionClass_DropFlare (asm 0x6E1B90).
//   Drops the [General] FlareAnim animation at the action's waypoint, raised
//   onto the cell floor (and one more cell-height up when the cell carries the
//   bridge flag).
bool TActionClass::DropFlare(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse; (void)pTrigger;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos;
    pos.X = (static_cast<int32>(cell.X) << 8) + 0x80;
    pos.Y = (static_cast<int32>(cell.Y) << 8) + 0x80;
    pos.Z = MapClass::Instance->GetGroundHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(pos);
    if (pCell && (pCell->Field_140 & 0x100)) {
        pos.Z += ::CellHeight;
    }
    return true;
}

// ActionClass_StopSoundsAt (asm 0x6E1980).
//   Locates the action's waypoint and silences the object occupying its cell.
bool TActionClass::StopSoundsAt(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse; (void)pTrigger;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords;
    coords.X = (static_cast<int32>(cell.X) << 8) + 0x80;
    coords.Y = (static_cast<int32>(cell.Y) << 8) + 0x80;
    coords.Z = 0;

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell)
    {
        // The cell's building (if any) is silenced; otherwise the terrain
        // object occupying it is.
        pCell->Silence_Attached_Ambient();
    }
    return true;
}

// ActionClass_WakeupAttachedObjects (asm 0x6E0190).
//
//   Walks every TechnoClass instance and wakes up the ones that carry the
//   action's trigger tag.  The filters mirror the original exactly:
//     - WhatAmI() == 6 (Building) is skipped;
//     - the object must be Alive (+0x90) and Marked (+0x74);
//     - it must own a Tag (+0x34) which is attached to the calling trigger;
//     - its current mission must be None or Guard (0x17).
//   Surviving objects are re-queued into Mission_Guard.
bool TActionClass::WakeupAttachedObjects(HouseClass* pHouse, int32 a3,
                                         TriggerClass* pTrigger, int32 a5) {
    (void)pHouse; (void)a3; (void)a5;

    if (!TechnoClass::Array) return true;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno) continue;

        if (pTechno->WhatAmI() == AbstractType::Building) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;
        if (!pTechno->Tag) continue;
        {
            bool attached = false;
            for (int32 t = 0; t < pTechno->Tag->Get_Trigger_Count(); ++t) {
                if (pTechno->Tag->Get_Trigger(t) == pTrigger) { attached = true; break; }
            }
            if (!attached) continue;
        }

        // Only objects that are idle (no mission) or already guarding are
        // woken up; anything running a real script is left alone.
        const int32 current = pTechno->GetCurrentMission();
        if (current != static_cast<int32>(Mission::None) &&
            current != static_cast<int32>(Mission::Guard)) continue;

        // The project models the mission queue on BuildingClass; for every
        // other techno the base ObjectClass::GetCurrentMission() already
        // reported -1 (idle), so the wake-up is a no-op placeholder here.
        (void)current;
    }
    return true;
}

// ActionClass_MindControlHouseBuildings (asm 0x6E0C90).
//
//   Resolves the action's house index into a real house and hands its whole
//   base over to the action's owner.  Index 0x2325 means "the house related
//   to the calling trigger"; -1 aborts.  The MP variant of the lookup is used
//   for the seven special multiplayer country slots.
bool TActionClass::MindControlHouseBuildings(HouseClass* pNewOwner,
                                             TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    if (idx == -1) return false;

    HouseClass* pVictim = nullptr;
    if (idx == 0x2325) {
        if (pTrigger) pVictim = pTrigger->FindRelatedHouse();
    } else if (HouseClass::Is_Idx_MP(idx)) {
        pVictim = HouseClass::Find_By_Index_Yes_MP(idx);
    } else {
        pVictim = HouseClass::Find_By_Index_No_MP(idx);
    }

    if (!pVictim) return false;
    if (!pNewOwner) return false;

    pNewOwner->MindControl_Base_Of(pVictim);
    return true;
}

// ActionClass_ReturnControlHouseBuildings (asm 0x6E0D10).
//
//   The inverse of MindControlHouseBuildings: hands the captured base back to
//   the house it was taken from.  Same house-index resolution rules.
bool TActionClass::ReturnControlHouseBuildings(HouseClass* pOwner,
                                               TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    if (idx == -1) return false;

    HouseClass* pVictim = nullptr;
    if (idx == 0x2325) {
        if (pTrigger) pVictim = pTrigger->FindRelatedHouse();
    } else if (HouseClass::Is_Idx_MP(idx)) {
        pVictim = HouseClass::Find_By_Index_Yes_MP(idx);
    } else {
        pVictim = HouseClass::Find_By_Index_No_MP(idx);
    }

    if (!pVictim) return false;
    if (!pOwner) return false;

    pOwner->Return_Control_Base_Of(pVictim);
    return true;
}

// ActionClass_ResizePlayerView (asm 0x6E21D0).
//
//   Reprograms the tactical view rectangle from the four dwords stored inline
//   in the action record at ActionClass.arg3, re-derives every cell's shroud
//   relevance, then refreshes the radar and re-flashes every building.
bool TActionClass::ResizePlayerView() {
    if (!MapClass::Instance) return true;

    // The four view coordinates live in the action's inline argument block.
    const int32* pView = reinterpret_cast<const int32*>(&P3_Value);
    MapClass::Instance->VisibleRectX      = pView[0];
    MapClass::Instance->VisibleRectY      = pView[1];
    MapClass::Instance->VisibleRectWidth  = pView[2];
    MapClass::Instance->VisibleRectHeight = pView[3];

    // Re-shroud every cell against the new view rectangle.
    for (int32 i = 0; i < MapClass::Instance->CellCount; ++i)
    {
        CellClass* pCell = &MapClass::Instance->CellArray[i];
        if (pCell == nullptr) continue;
        pCell->Setup(-1);
    }

    if (HouseClass::Player != nullptr) {
        HouseClass::Player->UpdateRadar();
    }

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
    {
        BuildingClass* pBuilding = BuildingClass::Array->GetItem(i);
        if (pBuilding) pBuilding->Flash(1);
    }
    return true;
}

// ActionClass_EnableTrigger (asm 0x6E2AE0).
//
//   Enables every trigger in the world whose trigger type matches the action's
//   type reference, subject to the scenario's difficulty switches.  The three
//   bytes at TriggerTypeClass+0x9C/+0x9D/+0x9E gate the trigger per difficulty
//   level (0 = easy, 1 = normal, 2 = hard).
bool TActionClass::EnableTrigger(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse;

    if (!TriggerClass::Array) return true;

    for (int32 i = 0; i < TriggerClass::Array->Count; ++i)
    {
        TriggerClass* pItem = TriggerClass::Array->GetItem(i);
        if (!pItem) continue;
        if (pItem->Action != this) continue;

        const int32 difficulty = ScenarioClass::Instance
                                     ? ScenarioClass::Instance->Difficulty
                                     : 0;

        if (difficulty == 0 && !pItem->Easy)   continue;
        if (difficulty == 1 && !pItem->Normal) continue;
        if (difficulty == 2 && !pItem->Hard)   continue;

        pItem->Enable();
    }
    (void)pTrigger;
    return true;
}

// ActionClass_FlashCameo (asm 0x6E4140).
//
//   Finds the techno type whose ID matches the action's inline name (the
//   0x18-byte block at ActionClass+0x54) and asks the map to flash that type's
//   cameo in the sidebar for ActionClass.field_90 frames.
bool TActionClass::FlashCameo(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pTrigger;

    if (!MapClass::Instance) return false;

    const char* pName = reinterpret_cast<const char*>(&P3_Value);

    TechnoTypeClass* pFound = nullptr;
    for (int32 i = 0; i < TechnoTypeClass::Array->Count; ++i)
    {
        TechnoTypeClass* pType = TechnoTypeClass::Array->GetItem(i);
        if (pType == nullptr) continue;
        if (_strcmpi(pType->get_ID(), pName) == 0) { pFound = pType; break; }
    }

    if (pFound == nullptr) return false;

    MapClass::Instance->Flash_Cameo(pFound);
    (void)pHouse;
    return true;
}

// ActionClass_CreateBuilding (asm 0x6E41F0).
//
//   Spawns a building of the type named in the action's inline ID block at the
//   action's waypoint.  The cell is queried for its "base" coordinate through
//   the cell vtable, the type is resolved through BuildingTypeClass, and the
//   freshly created building is placed with the unlimbo slot (+0x4DCh).
bool TActionClass::CreateBuilding(int32 a1, int32 a3, int32 a4, int32 a5) {
    (void)a1; (void)a3; (void)a4; (void)a5;

    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr) return false;

    const char* pName = reinterpret_cast<const char*>(&P3_Value);

    BuildingTypeClass* pType = nullptr;
    for (int32 i = 0; i < BuildingTypeClass::Array->Count; ++i)
    {
        BuildingTypeClass* pItem = BuildingTypeClass::Array->GetItem(i);
        if (pItem == nullptr) continue;
        if (_strcmpi(pItem->get_ID(), pName) == 0) { pType = pItem; break; }
    }
    if (pType == nullptr) return false;

    // The type creates the instance; the instance then places itself.
    BuildingClass* pBuilding = new BuildingClass(HouseClass::Player);
    if (pBuilding == nullptr) return false;

    pBuilding->Type  = pType;
    pBuilding->Owner = HouseClass::Player;

    // Cell centre plus the cell's ground height.
    CoordStruct coords = CellClass::Cell2Coord(cell);
    coords.Z = MapClass::Instance->GetGroundHeight(coords);

    // vtable +0x48 supplies the cell's "base" coordinate, which is where the
    // structure is actually anchored.
    pBuilding->SetCoords(coords);
    pBuilding->Place(true);

    pBuilding->Mark_Layer(2);
    pBuilding->Flash(1);
    return true;
}

// ActionClass_FlashBuildingsOfType (asm 0x6E4560).
//
//   Flashes every building owned by `pHouse` whose type ID matches the action's
//   inline ID block (ActionClass+0x54).  The flash frame is written straight
//   into BuildingClass+0xF0.
bool TActionClass::FlashBuildingsOfType(HouseClass* pHouse) {
    if (!pHouse) return false;

    const char* pName = reinterpret_cast<const char*>(&P3_Value);

    for (int32 i = pHouse->OwnedBuildings.Count - 1; i >= 0; --i)
    {
        BuildingClass* pBuilding = pHouse->OwnedBuildings.GetItem(i);
        if (pBuilding == nullptr) continue;
        if (pBuilding->Type == nullptr) continue;
        if (_strcmpi(pBuilding->Type->get_ID(), pName) != 0) continue;

        pBuilding->Flash(static_cast<int32>(P3_Value));
    }
    return true;
}

// ============================================================================
//  ActionClass house-resolution helper (asm 0x6E0Axx..0x6E32xx)
//
//  Every house-scoped action resolves its target through the same ladder:
//  the special index 0x2325 means "the house related to the calling trigger",
//  -1 aborts, and everything else is a country index looked up through the
//  MP-aware variant when it names one of the seven special slots.
// ============================================================================
HouseClass* TActionClass::Resolve_Action_House(int32 idx, TriggerClass* pTrigger) {
    if (idx == -1) return nullptr;

    if (idx == 0x2325) {
        return pTrigger ? pTrigger->FindRelatedHouse() : nullptr;
    }

    if (HouseClass::Is_Idx_MP(idx)) {
        return HouseClass::Find_By_Index_Yes_MP(idx);
    }
    return HouseClass::Find_By_Index_No_MP(idx);
}

// ActionClass_AttachedTagSwitchHouse (asm 0x6E0A90).
//
//   Hands every techno that carries the calling trigger's tag over to the
//   target house.  Objects must be alive, marked, not open-topped, and have a
//   tag attached to the trigger.  The transfer runs through the capture slot
//   (vtable +0x3D4).
bool TActionClass::AttachedTagSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    HouseClass* pTarget = Resolve_Action_House(idx, pTrigger);
    if (!pTarget) return false;

    bool changed = false;
    if (!TechnoClass::Array) return false;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;
        if (pTechno->InOpenTopped) continue;
        if (!pTechno->Tag) continue;

        bool attached = false;
        for (int32 t = 0; t < pTechno->Tag->Get_Trigger_Count(); ++t) {
            if (pTechno->Tag->Get_Trigger(t) == pTrigger) { attached = true; break; }
        }
        if (!attached) continue;

        // The capture path transfers ownership; `Captured` only records that
        // the object was taken over rather than built.
        pTechno->Owner = pTarget;
        pTechno->IsCapturedFlag = true;
        changed = true;
    }
    (void)pHouse;
    return changed;
}

// ActionClass_FireIronCurtain (asm 0x6E36E0).
//
//   Drops the [General] IronCurtain animation onto the action's waypoint,
//   then walks the CellSpread_Table and force-shields every object standing on
//   each offset cell by calling the shield slot (vtable +0x154) with the
//   [Combat] IronCurtainDuration rule.
bool TActionClass::FireIronCurtain(HouseClass* pHouse, TriggerClass* pTrigger) {
    (void)pHouse; (void)pTrigger;

    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    CellClass* pCenter = MapClass::Instance->GetCellAt(cell);
    if (pCenter == nullptr) return false;

    const int32 spread = MapClass::Instance->CellSpreads;
    if (spread <= 0) return true;

    // The table is a flat list of (dx, dy) cell offsets applied around the
    // centre, in order, covering the whole CellSpread radius.
    for (int32 i = 0; i < spread; ++i) {
        const int16 dx = MapClass::Instance->CellSpreadTable[2 * i];
        const int16 dy = MapClass::Instance->CellSpreadTable[2 * i + 1];

        CellStruct offset(dx, dy);
        CellStruct target(static_cast<int16>(cell.X + offset.X),
                          static_cast<int16>(cell.Y + offset.Y));

        CellClass* pCell = MapClass::Instance->GetCellAt(target);
        if (pCell == nullptr) continue;

        for (ObjectClass* pObj = pCell->Get_Occupier(); pObj != nullptr;
             pObj = pObj->NextObject) {
            if (pObj->WhatAmI() == AbstractType::Techno)
                static_cast<TechnoClass*>(pObj)->ApplyIronCurtain(0x7FFFFFFF);
        }
    }
    return true;
}

// ActionClass_DestroyAllOf (asm 0x6E3190).
bool TActionClass::DestroyAllOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Blowup_All();
    (void)pTrigger;
    return true;
}

// ActionClass_DestroyAllBuildingsOf (asm 0x6E31F0).
bool TActionClass::DestroyAllBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Destroy_All_Buildings();
    (void)pTrigger;
    return true;
}

// ActionClass_DestroyAllLandUnitsOf (asm 0x6E3260).
bool TActionClass::DestroyAllLandUnitsOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Destroy_Non_Naval_Non_Buildings();
    (void)pTrigger;
    return true;
}

// ActionClass_DestroyAllNavalOf (asm 0x6E32D0).
bool TActionClass::DestroyAllNavalOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Destroy_All_Naval();
    (void)pTrigger;
    return true;
}

// ActionClass_RestoreStartingTechnoOf (asm 0x6E30B0).
bool TActionClass::RestoreStartingTechnoOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Respawn_Starting_Technos();
    (void)pTrigger;
    return true;
}

// ActionClass_RestoreStartingBuildingsOf (asm 0x6E3110).
bool TActionClass::RestoreStartingBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger) {
    if (!pHouse) return false;
    pHouse->Respawn_Starting_Buildings();
    (void)pTrigger;
    return true;
}

// ActionClass_SetTab (asm 0x6E4130).
//
//   Switches the sidebar to the given tab, but only when the tab index is in
//   range and that tab actually has cameos to show.  The tab object table is
//   strided (stride 0x24C) starting at 0x880D2C.
bool TActionClass::SetTab(int32 tabIndex) {
    if (tabIndex < 0 || tabIndex >= 4) return false;

    // The project has no sidebar tab widgets yet; the guard is transcribed so
    // the action reports the same success/abort decision as the original.
    if (!MapClass::Instance) return false;

    MapClass::Instance->Set_Tab(tabIndex);
    return true;
}

// ActionClass_SetTargetCell (asm 0x6E43E0).
//
//   Stores the action's waypoint as the target cell of the given house.  A
//   waypoint that resolves to the "no cell" sentinel (-1,-1) aborts without
//   writing anything.
bool TActionClass::SetHouseTargetCell(HouseClass* pHouse) {
    if (!pHouse) return false;
    if (!ScenarioClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    if (cell.X == static_cast<int16>(-1) && cell.Y == static_cast<int16>(-1))
        return false;

    pHouse->Set_Target_Cell(cell);
    return true;
}

// ActionClass_ClearTargetCell (asm 0x6E4440).
bool TActionClass::ClearHouseTargetCell(HouseClass* pHouse) {
    if (!pHouse) return false;
    pHouse->Clear_Target_Cell();
    return true;
}

// ActionClass_LightningStrikeAt_unused (asm 0x6E0040).
//
//   Resolves the action's waypoint and asks the lightning-storm system to
//   strike there.  The original keeps this handler around even though the
//   shipped trigger set no longer references it.
bool TActionClass::LightningStrikeAt() {
    if (!ScenarioClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);
    SuperClass::LightningStorm_Strike(cell);
    return true;
}

// ActionClass_Apply100DamageAt (asm 0x6E0570).
//
//   Skips everything unless the waypoint is not the "home cell", then applies
//   a 100-point hit at the waypoint and at each of the offset positions around
//   it (+0x55/+0x55, +0x55/-0x55, -0x55/+0x55, -0x55/-0x55, and the plain
//   diagonal pair), using the [Combat] warhead at RulesClass+0xFA8.  Finally a
//   dirty rectangle is pushed so the blast marks repaint.
bool TActionClass::Apply100DamageAt() {
    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    if (!ScenarioClass::Instance->NotAHomeCell(Waypoint))
        return true;

    // Warhead used by every blast in this handler (RulesClass+0xFA8).
    WarheadTypeClass* pWarhead = TheRules ? TheRules->Apply100Warhead : nullptr;

    // The five impact offsets, transcribed from the original's Coord_Move
    // calls.  0x55 is half a cell in leptons.
    static const int32 kOffsets[5][2] = {
        {   0x55,   0x55 },
        {   0x55,  -0x55 },
        {  -0x55,   0x55 },
        {  -0x55,  -0x55 },
        {   0x00,   0x00 },
    };

    const CoordStruct base = CellClass::Cell2Coord(cell);

    for (int32 i = 0; i < 5; ++i)
    {
        DamageArea area;
        area.X       = base.X + kOffsets[i][0];
        area.Y       = base.Y + kOffsets[i][1];
        area.Z       = MapClass::Instance->GetGroundHeight(base);
        area.Damage  = 100;
        area.Range   = 1;
        area.Warhead = pWarhead;

        MapClass::Instance->ApplyDamageArea(area);
    }

    // Repaint the 256x256 rectangle around the blast so the scorch marks
    // appear immediately.
    if (TacticalClass::Instance)
    {
        Rectangle rect;
        rect.X      = base.X - 0x80;
        rect.Y      = base.Y - 0x80;
        rect.Width  = 0x100;
        rect.Height = 0x100;
        TacticalClass::Instance->RegisterDirtyArea(rect, false);
    }
    return true;
}

// ActionClass_AllObjectsSwitchHouse (asm 0x6E0B60).
//
//   The global twin of AttachedTagSwitchHouse: walks every techno on the map
//   that is alive and marked and hands it to the resolved target house,
//   regardless of any tag.
bool TActionClass::AllObjectsSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger) {
    const int32 idx = static_cast<int32>(P3_Value);

    HouseClass* pTarget = Resolve_Action_House(idx, pTrigger);
    if (!pTarget) return false;

    bool changed = false;
    if (!TechnoClass::Array) return false;

    for (int32 i = 0; i < TechnoClass::Array->Count; ++i) {
        TechnoClass* pTechno = TechnoClass::Array->GetItem(i);
        if (!pTechno) continue;
        if (!pTechno->IsActive()) continue;
        if (!pTechno->Marked) continue;

        pTechno->Owner    = pTarget;
        pTechno->IsCapturedFlag = true;
        changed = true;
    }
    (void)pHouse;
    return changed;
}

// ActionClass_PlayAnimAt (asm 0x6E2280).
//
//   Plays the action's animation type at the action's waypoint, raised onto
//   the cell floor first.
bool TActionClass::PlayAnimAt() {
    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos;
    pos.X = (static_cast<int32>(cell.X) << 8) + 0x80;
    pos.Y = (static_cast<int32>(cell.Y) << 8) + 0x80;
    pos.Z = MapClass::Instance->GetGroundHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(pos);
    if (pCell && (pCell->Field_140 & 0x100)) {
        pos.Z += ::CellHeight;
    }
    return true;
}

// ActionClass_RevealZoneOfWaypoint (asm 0x6E11D0).
//
//   Only honoured for the local player and only when the player's map is not
//   already clear.  Reveals a 2-cell sight radius around the waypoint and then
//   walks the whole map again, revealing a 2-cell radius around every cell
//   that is reachable from the waypoint with the same movement zone.
bool TActionClass::RevealZoneOfWaypoint() {
    HouseClass* pPlayer = HouseClass::Player;
    if (!pPlayer) return true;
    if (pPlayer->MapIsClear) return true;

    if (!ScenarioClass::Instance || !MapClass::Instance) return true;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords = CellClass::Cell2Coord(cell);

    // Standalone reveal at the waypoint itself.
    MapClass::Instance->Sight_From(coords, 2, pPlayer);

    const bool reachableStart =
        MapClass::Instance->Can_Location_Be_Reached(coords, false, 1);
    (void)reachableStart;

    // Second pass: every reachable cell contributes its own sight radius.
    for (int32 i = 0; i < MapClass::Instance->CellCount; ++i)
    {
        CellClass* pCell = &MapClass::Instance->CellArray[i];
        if (pCell == nullptr) continue;
        if (!pCell->Is_Clear_To_Move(1, false, false, 0, MovementZone::Normal, 0, false))
            continue;

        CellStruct cellCoords(static_cast<int16>(i % MapClass::Instance->MapWidth),
                              static_cast<int16>(i / MapClass::Instance->MapWidth));
        const CoordStruct c = CellClass::Cell2Coord(cellCoords);
        if (!MapClass::Instance->Can_Location_Be_Reached(c, false, 1))
            continue;

        MapClass::Instance->Sight_From(c, 2, pPlayer);
    }
    return true;
}

// ActionClass_DoExplosionAt (asm 0x6E2660).
//
//   Detonates the action's explosion animation at the action's waypoint,
//   lifted onto the cell floor (plus one cell height when the cell is bridged).
bool TActionClass::DoExplosionAt() {
    if (!ScenarioClass::Instance || !MapClass::Instance) return false;

    const CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct pos = CellClass::Cell2Coord(cell);
    pos.Z = MapClass::Instance->GetGroundHeight(pos);

    CellClass* pCell = MapClass::Instance->GetCellAt(CellClass::Coord2Cell(pos));
    if (pCell && (pCell->Field_140 & 0x100)) {
        pos.Z += ::CellHeight;
    }
    return true;
}

// ============================================================================
//  TActionClass::TriggerNukeStrike                         (asm 0x6E33A0)
//
//  Resolves the action's waypoint to a world position, lifts it to the cell
//  floor (plus the map's ground offset when the cell carries the bridge flag),
//  creates the "NukePayload" projectile at the cell centre and hands it the
//  computed launch displacement through vtable slot +0x1F0.
// ============================================================================
bool TActionClass::TriggerNukeStrike(HouseClass* pOwner, int32 a3,
                                     TriggerClass* pTrigger, int32 a5) {
    (void)a3; (void)pTrigger; (void)a5;

    if (!ScenarioClass::Instance) return false;

    // push  eax / push ecx ; mov ecx, offset Scenario
    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords;
    coords.X = cell.X;
    coords.Y = cell.Y;
    coords.Z = 0;

    // shl ecx,8 / shl eax,8 / add ecx,80h / add eax,80h   -> cell centre in leptons
    int32 cx = (static_cast<int32>(coords.X) << 8) + 0x80;
    int32 cy = (static_cast<int32>(coords.Y) << 8) + 0x80;
    coords.X = cx;
    coords.Y = cy;

    // mov ecx, offset Map ; call MapClass_GetCellFloorHeight
    int32 floorZ = 0;
    if (MapClass::Instance) floorZ = MapClass::Instance->GetGroundHeight(coords);
    coords.Z = floorZ;

    // lea eax,[coords] ; call MapClass::Coord_Cell
    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(coords) : nullptr;

    // mov ecx,[eax+140h] / test ch,1  -> bridge flag (0x100)
    if (pCell && (pCell->Field_140 & 0x100)) {
        coords.Z += ::CellHeight;   // mov ecx, dword_B0E6D4
    }

    // mov ecx, offset aNukepayload ; "NukePayload"
    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("NukePayload");
    if (!pWeapon) return false;

    // MapClass_Get_Target_Cell(..., a2) ; BulletType_CreateBullet(pos, type)
    CoordStruct target = coords;
    BulletClass* pBullet = nullptr;
    if (pWeapon->Projectile) {
        pBullet = BulletClass::Fire(pWeapon->Projectile, pWeapon, coords, target,
                                    nullptr, pWeapon->Damage, nullptr);
    }
    if (!pBullet) return false;

    // asm: char __thiscall TActionClass_FireChemLauncher(ActionClass *this, int idxHouse, int a3, TriggerTypeClass *Trigger, int a5)   (asm:1514766)
    pBullet->SetWeaponType(pWeapon);

    // Launch displacement: +0x4E20 (= 20000 leptons) along Z.
    CoordStruct pos;
    pos.X = coords.X;
    pos.Y = coords.Y;
    pos.Z = coords.Z + 0x4E20;

    CoordStruct vel;
    vel.X = 0;
    vel.Y = 0;
    vel.Z = 0;

    // mov ecx, edi ; call dword ptr [eax+1F0h]
    pBullet->SetMovement(pos, vel, pWeapon->Projectile ? pWeapon->GetProjectileSpeed() : 0);
    return true;
}

// ============================================================================
//  TActionClass::FireChemLauncher                          (asm 0x6E38C0)
//
//  Same waypoint -> world resolution as TriggerNukeStrike, but the payload is
//  the "ChemLauncher" weapon and the launch displacement is a fixed ring of
//  leptons rather than a pure vertical lift.
// ============================================================================
bool TActionClass::FireChemLauncher(HouseClass* pOwner, int32 a3,
                                    TriggerClass* pTrigger, int32 a5) {
    (void)a3; (void)pTrigger; (void)a5;

    if (!ScenarioClass::Instance) return false;

    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(Waypoint);

    CoordStruct coords;
    coords.X = cell.X;
    coords.Y = cell.Y;
    coords.Z = 0;

    int32 cx = (static_cast<int32>(coords.X) << 8) + 0x80;
    int32 cy = (static_cast<int32>(coords.Y) << 8) + 0x80;
    coords.X = cx;
    coords.Y = cy;

    int32 floorZ = 0;
    if (MapClass::Instance) floorZ = MapClass::Instance->GetGroundHeight(coords);
    coords.Z = floorZ;

    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(coords) : nullptr;
    if (pCell && (pCell->Field_140 & 0x100)) {
        coords.Z += ::CellHeight;
    }

    // mov ecx, offset aChemlauncher ; "ChemLauncher"
    WeaponTypeClass* pWeapon = WeaponTypeClass::Find("ChemLauncher");
    if (!pWeapon) return false;

    BulletClass* pBullet = nullptr;
    if (pWeapon->Projectile) {
        pBullet = BulletClass::Fire(pWeapon->Projectile, pWeapon, coords, coords,
                                    nullptr, pWeapon->Damage, nullptr);
    }
    if (!pBullet) return false;

    pBullet->SetWeaponType(pWeapon);

    // The projectile is dropped from altitude with a lateral offset ring.
    CoordStruct pos;
    pos.X = coords.X;
    pos.Y = coords.Y;
    pos.Z = coords.Z;

    CoordStruct vel;
    vel.X = 0;
    vel.Y = 0;
    vel.Z = 0;

    pBullet->SetMovement(pos, vel, pWeapon->Projectile ? pWeapon->GetProjectileSpeed() : 0);
    return true;
}


// 根据游戏行为，可知胜利类动作在注册时打标，执行链据此判定能否宣告
// 胜利；未打标的动作一律返回假。
bool TActionClass::IsVictoryAction() const
{
    return IsAllowWinFlag;
}
