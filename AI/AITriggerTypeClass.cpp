#include "AITriggerTypeClass.h"
#include "AITeamTypeClass.h"
#include "TeamTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"
#include "../SW/SuperWeaponTypeClass.h"
#include "../INI/INIClass.h"

#include <cstring>
#include <cstdio>
#include <cmath>

DynamicVectorClass<AITriggerTypeClass*>* AITriggerTypeClass::Array = nullptr;

AITriggerTypeClass* AITriggerTypeClass::Find(const char* pID) {
    if (!Array) return nullptr;
    for (int32 i = 0; i < Array->Count; ++i) {
        AITriggerTypeClass* item = Array->GetItem(i);
        if (item && !_strcmpi(item->ID, pID)) return item;
    }
    return nullptr;
}

AITriggerTypeClass* AITriggerTypeClass::FindOrAllocate(const char* pID) {
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    AITriggerTypeClass* found = Find(pID);
    if (found) return found;
    AITriggerTypeClass* newItem = GameCreate<AITriggerTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}

AITriggerTypeClass::AITriggerTypeClass(const char* pID) noexcept
    : AbstractTypeClass(pID), ConditionType(AITriggerCondition::HouseOwns),
      IsGlobal(0), OwnerHouseType(AITriggerHouseType::None), IsEnabled(true),
      HouseIndex(-1), SideIndex(-1), TechLevel(1), unknown_B4(0),
      Weight_Current(1.0), Weight_Minimum(0.0), Weight_Maximum(10.0),
      IsForSkirmish(false), IsForBaseDefense(false),
      Enabled_Easy(true), Enabled_Normal(true), Enabled_Hard(true),
      ConditionObject(nullptr), Team1(nullptr), Team2(nullptr),
      TimesExecuted(0), TimesCompleted(0), unknown_10C(0) {
    for (int32 i = 0; i < 4; ++i) {
        Conditions[i].ComparatorType = 0;
        Conditions[i].ComparatorOperand = 0;
    }
}

AITriggerTypeClass::~AITriggerTypeClass() {
}

HRESULT AITriggerTypeClass::GetClassID(CLSID* pClassID) {
    if (pClassID) {
        pClassID->Data1 = 0xA1A1A1A1;
        for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
        return S_OK;
    }
    return E_POINTER;
}

HRESULT AITriggerTypeClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read ID
    char idBuf[0x18];
    hr = pStm->Read(idBuf, sizeof(idBuf), &read);
    if (hr < 0 || read != sizeof(idBuf)) return E_FAIL;
    std::memcpy(ID, idBuf, sizeof(ID));
    ID[sizeof(ID) - 1] = '\0';

    // Read scalar fields
    int32 condType = 0;
    hr = pStm->Read(&condType, sizeof(condType), &read);
    if (hr < 0 || read != sizeof(condType)) return E_FAIL;
    ConditionType = static_cast<AITriggerCondition>(condType);

    hr = pStm->Read(&IsGlobal, sizeof(IsGlobal), &read);
    if (hr < 0 || read != sizeof(IsGlobal)) return E_FAIL;

    int32 ownerType = 0;
    hr = pStm->Read(&ownerType, sizeof(ownerType), &read);
    if (hr < 0 || read != sizeof(ownerType)) return E_FAIL;
    OwnerHouseType = static_cast<AITriggerHouseType>(ownerType);

    // Read packed bool flags
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsEnabled       = (flags & 0x01) != 0;
    IsForSkirmish   = (flags & 0x02) != 0;
    IsForBaseDefense = (flags & 0x04) != 0;
    Enabled_Easy    = (flags & 0x08) != 0;
    Enabled_Normal  = (flags & 0x10) != 0;
    Enabled_Hard    = (flags & 0x20) != 0;

    hr = pStm->Read(&HouseIndex, sizeof(HouseIndex), &read);
    if (hr < 0 || read != sizeof(HouseIndex)) return E_FAIL;
    hr = pStm->Read(&SideIndex, sizeof(SideIndex), &read);
    if (hr < 0 || read != sizeof(SideIndex)) return E_FAIL;
    hr = pStm->Read(&TechLevel, sizeof(TechLevel), &read);
    if (hr < 0 || read != sizeof(TechLevel)) return E_FAIL;
    hr = pStm->Read(&unknown_B4, sizeof(unknown_B4), &read);
    if (hr < 0 || read != sizeof(unknown_B4)) return E_FAIL;

    hr = pStm->Read(&Weight_Current, sizeof(Weight_Current), &read);
    if (hr < 0 || read != sizeof(Weight_Current)) return E_FAIL;
    hr = pStm->Read(&Weight_Minimum, sizeof(Weight_Minimum), &read);
    if (hr < 0 || read != sizeof(Weight_Minimum)) return E_FAIL;
    hr = pStm->Read(&Weight_Maximum, sizeof(Weight_Maximum), &read);
    if (hr < 0 || read != sizeof(Weight_Maximum)) return E_FAIL;

    // Read condition object ID and resolve.
    char condObjID[0x18];
    hr = pStm->Read(condObjID, sizeof(condObjID), &read);
    if (hr < 0 || read != sizeof(condObjID)) return E_FAIL;
    condObjID[sizeof(condObjID) - 1] = '\0';
    ConditionObject = condObjID[0] ? static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(condObjID)) : nullptr;

    // Read team1 ID and resolve.
    char team1ID[0x18];
    hr = pStm->Read(team1ID, sizeof(team1ID), &read);
    if (hr < 0 || read != sizeof(team1ID)) return E_FAIL;
    team1ID[sizeof(team1ID) - 1] = '\0';
    Team1 = team1ID[0] ? TeamTypeClass::Find(team1ID) : nullptr;

    // Read team2 ID and resolve.
    char team2ID[0x18];
    hr = pStm->Read(team2ID, sizeof(team2ID), &read);
    if (hr < 0 || read != sizeof(team2ID)) return E_FAIL;
    team2ID[sizeof(team2ID) - 1] = '\0';
    Team2 = team2ID[0] ? TeamTypeClass::Find(team2ID) : nullptr;

    // Read condition comparators (fixed array of 4).
    for (int32 i = 0; i < 4; ++i) {
        hr = pStm->Read(&Conditions[i].ComparatorType, sizeof(Conditions[i].ComparatorType), &read);
        if (hr < 0 || read != sizeof(Conditions[i].ComparatorType)) return E_FAIL;
        hr = pStm->Read(&Conditions[i].ComparatorOperand, sizeof(Conditions[i].ComparatorOperand), &read);
        if (hr < 0 || read != sizeof(Conditions[i].ComparatorOperand)) return E_FAIL;
    }

    hr = pStm->Read(&TimesExecuted, sizeof(TimesExecuted), &read);
    if (hr < 0 || read != sizeof(TimesExecuted)) return E_FAIL;
    hr = pStm->Read(&TimesCompleted, sizeof(TimesCompleted), &read);
    if (hr < 0 || read != sizeof(TimesCompleted)) return E_FAIL;
    hr = pStm->Read(&unknown_10C, sizeof(unknown_10C), &read);
    if (hr < 0 || read != sizeof(unknown_10C)) return E_FAIL;

    return S_OK;
}

HRESULT AITriggerTypeClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write ID
    hr = pStm->Write(ID, sizeof(ID), &written);
    if (hr < 0 || written != sizeof(ID)) return E_FAIL;

    // Write scalar fields
    int32 condType = static_cast<int32>(ConditionType);
    hr = pStm->Write(&condType, sizeof(condType), &written);
    if (hr < 0 || written != sizeof(condType)) return E_FAIL;

    hr = pStm->Write(&IsGlobal, sizeof(IsGlobal), &written);
    if (hr < 0 || written != sizeof(IsGlobal)) return E_FAIL;

    int32 ownerType = static_cast<int32>(OwnerHouseType);
    hr = pStm->Write(&ownerType, sizeof(ownerType), &written);
    if (hr < 0 || written != sizeof(ownerType)) return E_FAIL;

    // Write packed bool flags
    uint32 flags = 0;
    if (IsEnabled)        flags |= 0x01;
    if (IsForSkirmish)    flags |= 0x02;
    if (IsForBaseDefense) flags |= 0x04;
    if (Enabled_Easy)     flags |= 0x08;
    if (Enabled_Normal)   flags |= 0x10;
    if (Enabled_Hard)     flags |= 0x20;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&HouseIndex, sizeof(HouseIndex), &written);
    if (hr < 0 || written != sizeof(HouseIndex)) return E_FAIL;
    hr = pStm->Write(&SideIndex, sizeof(SideIndex), &written);
    if (hr < 0 || written != sizeof(SideIndex)) return E_FAIL;
    hr = pStm->Write(&TechLevel, sizeof(TechLevel), &written);
    if (hr < 0 || written != sizeof(TechLevel)) return E_FAIL;
    hr = pStm->Write(&unknown_B4, sizeof(unknown_B4), &written);
    if (hr < 0 || written != sizeof(unknown_B4)) return E_FAIL;

    hr = pStm->Write(&Weight_Current, sizeof(Weight_Current), &written);
    if (hr < 0 || written != sizeof(Weight_Current)) return E_FAIL;
    hr = pStm->Write(&Weight_Minimum, sizeof(Weight_Minimum), &written);
    if (hr < 0 || written != sizeof(Weight_Minimum)) return E_FAIL;
    hr = pStm->Write(&Weight_Maximum, sizeof(Weight_Maximum), &written);
    if (hr < 0 || written != sizeof(Weight_Maximum)) return E_FAIL;

    // Write condition object ID.
    char condObjID[0x18];
    std::memset(condObjID, 0, sizeof(condObjID));
    if (ConditionObject && ConditionObject->ID) {
        int32 j = 0;
        while (ConditionObject->ID[j] && j < static_cast<int32>(sizeof(condObjID)) - 1) {
            condObjID[j] = ConditionObject->ID[j]; ++j;
        }
    }
    hr = pStm->Write(condObjID, sizeof(condObjID), &written);
    if (hr < 0 || written != sizeof(condObjID)) return E_FAIL;

    // Write team1 ID.
    char team1ID[0x18];
    std::memset(team1ID, 0, sizeof(team1ID));
    if (Team1 && Team1->ID) {
        int32 j = 0;
        while (Team1->ID[j] && j < static_cast<int32>(sizeof(team1ID)) - 1) {
            team1ID[j] = Team1->ID[j]; ++j;
        }
    }
    hr = pStm->Write(team1ID, sizeof(team1ID), &written);
    if (hr < 0 || written != sizeof(team1ID)) return E_FAIL;

    // Write team2 ID.
    char team2ID[0x18];
    std::memset(team2ID, 0, sizeof(team2ID));
    if (Team2 && Team2->ID) {
        int32 j = 0;
        while (Team2->ID[j] && j < static_cast<int32>(sizeof(team2ID)) - 1) {
            team2ID[j] = Team2->ID[j]; ++j;
        }
    }
    hr = pStm->Write(team2ID, sizeof(team2ID), &written);
    if (hr < 0 || written != sizeof(team2ID)) return E_FAIL;

    // Write condition comparators (fixed array of 4).
    for (int32 i = 0; i < 4; ++i) {
        hr = pStm->Write(&Conditions[i].ComparatorType, sizeof(Conditions[i].ComparatorType), &written);
        if (hr < 0 || written != sizeof(Conditions[i].ComparatorType)) return E_FAIL;
        hr = pStm->Write(&Conditions[i].ComparatorOperand, sizeof(Conditions[i].ComparatorOperand), &written);
        if (hr < 0 || written != sizeof(Conditions[i].ComparatorOperand)) return E_FAIL;
    }

    hr = pStm->Write(&TimesExecuted, sizeof(TimesExecuted), &written);
    if (hr < 0 || written != sizeof(TimesExecuted)) return E_FAIL;
    hr = pStm->Write(&TimesCompleted, sizeof(TimesCompleted), &written);
    if (hr < 0 || written != sizeof(TimesCompleted)) return E_FAIL;
    hr = pStm->Write(&unknown_10C, sizeof(unknown_10C), &written);
    if (hr < 0 || written != sizeof(unknown_10C)) return E_FAIL;

    return S_OK;
}

AbstractType AITriggerTypeClass::WhatAmI() const {
    return AbstractType::AITriggerType;
}

int32 AITriggerTypeClass::Size() const {
    return sizeof(AITriggerTypeClass);
}

bool AITriggerTypeClass::LoadFromINIList(CCINIClass* pINI) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    if (!pINI->SectionExists(sectionName)) return false;

    Team1 = nullptr;
    Team2 = nullptr;
    ConditionObject = nullptr;

    char buffer[256];

    pINI->ReadString(sectionName, "Name", "", buffer, sizeof(buffer));
    if (buffer[0]) {
        int32 idx = 0;
        while (buffer[idx] && idx < static_cast<int32>(sizeof(this->Name)) - 1) {
            this->Name[idx] = buffer[idx];
            ++idx;
        }
        this->Name[idx] = '\0';
    }

    int32 condType = 0;
    pINI->GetInteger(sectionName, "ConditionType", condType);
    ConditionType = static_cast<AITriggerCondition>(condType);

    pINI->GetInteger(sectionName, "IsGlobal", IsGlobal);

    int32 ownerType = 0;
    pINI->GetInteger(sectionName, "OwnerHouseType", ownerType);
    OwnerHouseType = static_cast<AITriggerHouseType>(ownerType);

    pINI->GetInteger(sectionName, "HouseIndex", HouseIndex);
    pINI->GetInteger(sectionName, "SideIndex", SideIndex);
    pINI->GetInteger(sectionName, "TechLevel", TechLevel);

    pINI->GetDouble(sectionName, "Weight_Current", Weight_Current);
    pINI->GetDouble(sectionName, "Weight_Minimum", Weight_Minimum);
    pINI->GetDouble(sectionName, "Weight_Maximum", Weight_Maximum);

    IsForSkirmish = pINI->ReadBool(sectionName, "IsForSkirmish", IsForSkirmish);
    IsForBaseDefense = pINI->ReadBool(sectionName, "IsForBaseDefense", IsForBaseDefense);
    Enabled_Easy = pINI->ReadBool(sectionName, "Enabled_Easy", Enabled_Easy);
    Enabled_Normal = pINI->ReadBool(sectionName, "Enabled_Normal", Enabled_Normal);
    Enabled_Hard = pINI->ReadBool(sectionName, "Enabled_Hard", Enabled_Hard);

    char team1Name[0x18];
    pINI->ReadString(sectionName, "Team1", "", team1Name, sizeof(team1Name));
    if (team1Name[0] && _strcmpi(team1Name, "<none>") != 0) {
        Team1 = static_cast<TeamTypeClass*>(TeamTypeClass::Find(team1Name));
    }

    char team2Name[0x18];
    pINI->ReadString(sectionName, "Team2", "", team2Name, sizeof(team2Name));
    if (team2Name[0] && _strcmpi(team2Name, "<none>") != 0) {
        Team2 = static_cast<TeamTypeClass*>(TeamTypeClass::Find(team2Name));
    }

    char condObjName[0x18];
    pINI->ReadString(sectionName, "ConditionObject", "", condObjName, sizeof(condObjName));
    if (condObjName[0] && _strcmpi(condObjName, "<none>") != 0) {
        ConditionObject = static_cast<TechnoTypeClass*>(TechnoTypeClass::Find(condObjName));
    }

    for (int32 i = 0; i < 4; ++i) {
        char keyName[32];
        snprintf(keyName, sizeof(keyName), "Condition%d", i);
        char condStr[64];
        pINI->ReadString(sectionName, keyName, "0,0", condStr, sizeof(condStr));
        int32 cmpType = 0, cmpOperand = 0;
        char* comma = strchr(condStr, ',');
        if (comma) {
            *comma = '\0';
            cmpType = atoi(condStr);
            cmpOperand = atoi(comma + 1);
        } else {
            cmpType = atoi(condStr);
        }
        Conditions[i].ComparatorType = cmpType;
        Conditions[i].ComparatorOperand = cmpOperand;
    }

    return true;
}

bool AITriggerTypeClass::SaveToINIList(CCINIClass* pINI, bool Global) {
    if (!pINI) return false;

    char sectionName[64];
    snprintf(sectionName, sizeof(sectionName), "%s", this->ID);

    pINI->WriteString(sectionName, "Name", this->Name);
    pINI->WriteInteger(sectionName, "ConditionType", static_cast<int32>(ConditionType));
    pINI->WriteInteger(sectionName, "IsGlobal", IsGlobal);
    pINI->WriteInteger(sectionName, "OwnerHouseType", static_cast<int32>(OwnerHouseType));
    pINI->WriteInteger(sectionName, "HouseIndex", HouseIndex);
    pINI->WriteInteger(sectionName, "SideIndex", SideIndex);
    pINI->WriteInteger(sectionName, "TechLevel", TechLevel);
    pINI->WriteDouble(sectionName, "Weight_Current", Weight_Current);
    pINI->WriteDouble(sectionName, "Weight_Minimum", Weight_Minimum);
    pINI->WriteDouble(sectionName, "Weight_Maximum", Weight_Maximum);
    pINI->WriteBool(sectionName, "IsForSkirmish", IsForSkirmish);
    pINI->WriteBool(sectionName, "IsForBaseDefense", IsForBaseDefense);
    pINI->WriteBool(sectionName, "Enabled_Easy", Enabled_Easy);
    pINI->WriteBool(sectionName, "Enabled_Normal", Enabled_Normal);
    pINI->WriteBool(sectionName, "Enabled_Hard", Enabled_Hard);

    if (Team1) pINI->WriteString(sectionName, "Team1", Team1->get_ID());
    else pINI->WriteString(sectionName, "Team1", "<none>");

    if (Team2) pINI->WriteString(sectionName, "Team2", Team2->get_ID());
    else pINI->WriteString(sectionName, "Team2", "<none>");

    if (ConditionObject) pINI->WriteString(sectionName, "ConditionObject", ConditionObject->get_ID());
    else pINI->WriteString(sectionName, "ConditionObject", "<none>");

    for (int32 i = 0; i < 4; ++i) {
        char keyName[32], valueStr[64];
        snprintf(keyName, sizeof(keyName), "Condition%d", i);
        snprintf(valueStr, sizeof(valueStr), "%d,%d",
            Conditions[i].ComparatorType, Conditions[i].ComparatorOperand);
        pINI->WriteString(sectionName, keyName, valueStr);
    }

    return true;
}

bool AITriggerTypeClass::ConditionMet(HouseClass* CallingHouse, HouseClass* TargetHouse, bool EnoughBaseDefense) const {
    if (!IsEnabled) return false;
    if (!CallingHouse || !TargetHouse) return false;

    int32 difficulty = Game::Difficulty;
    if (difficulty == 0 && !Enabled_Easy) return false;
    if (difficulty == 1 && !Enabled_Normal) return false;
    if (difficulty == 2 && !Enabled_Hard) return false;

    if (IsForBaseDefense && !EnoughBaseDefense) return false;

    if (TechLevel > CallingHouse->TechLevel) return false;

    switch (ConditionType) {
        case AITriggerCondition::HouseOwns:
            return OwnerHouseOwns(CallingHouse, TargetHouse);
        case AITriggerCondition::EnemyOwns:
            return EnemyHouseOwns(CallingHouse, TargetHouse);
        case AITriggerCondition::CivilianOwns:
            return CivilianHouseOwns(CallingHouse, TargetHouse);
        case AITriggerCondition::HouseCredits:
            return HouseCredits(CallingHouse, TargetHouse);
        case AITriggerCondition::IronCurtainReady:
            return IronCurtainCharged(CallingHouse, TargetHouse);
        case AITriggerCondition::ChronosphereReady:
            return ChronoSphereCharged(CallingHouse, TargetHouse);
        case AITriggerCondition::BuildingCount:
            return EvaluateBuildingCount(CallingHouse);
        case AITriggerCondition::UnitCount:
            return EvaluateUnitCount(CallingHouse);
        case AITriggerCondition::InfantryCount:
            return EvaluateInfantryCount(CallingHouse);
        case AITriggerCondition::AircraftCount:
            return EvaluateAircraftCount(CallingHouse);
        case AITriggerCondition::PowerOutput:
            return EvaluatePowerOutput(CallingHouse);
        case AITriggerCondition::HasSuperWeapon:
            return EvaluateHasSuperWeapon(CallingHouse);
        case AITriggerCondition::TechLevel:
            return EvaluateTechLevel(CallingHouse);
        case AITriggerCondition::Weight:
            return EvaluateWeight();
        default:
            break;
    }

    if (ConditionObject) {
        int32 count = CallingHouse->CountOwnedNow(ConditionObject);
        for (int32 i = 0; i < 4; ++i) {
            const auto& cmp = Conditions[i];
            if (cmp.ComparatorType == 0) break;
            if (EvaluateComparator(count, cmp.ComparatorType, cmp.ComparatorOperand)) {
                return true;
            }
        }
        return false;
    }

    return false;
}

bool AITriggerTypeClass::EvaluateComparator(int32 currentValue, int32 comparatorType, int32 comparatorOperand) const {
    switch (comparatorType) {
        case 0: return false;
        case 1: return currentValue < comparatorOperand;
        case 2: return currentValue <= comparatorOperand;
        case 3: return currentValue == comparatorOperand;
        case 4: return currentValue >= comparatorOperand;
        case 5: return currentValue > comparatorOperand;
        case 6: return currentValue != comparatorOperand;
        default: return false;
    }
}

bool AITriggerTypeClass::EvaluateBuildingCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 count = pHouse->BuildingCount;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(count, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluateUnitCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 count = pHouse->UnitCount;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(count, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluateInfantryCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 count = pHouse->InfantryCount;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(count, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluateAircraftCount(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 count = pHouse->AircraftCount;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(count, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluatePowerOutput(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 availablePower = pHouse->PowerOutput - pHouse->PowerDrain;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(availablePower, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluateHasSuperWeapon(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 superWeaponCount = 0;
    for (int32 i = 0; i < 64; ++i) {
        if (pHouse->SuperWeaponTimers[i].HasTimeLeft()) {
            ++superWeaponCount;
        }
    }
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(superWeaponCount, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluateTechLevel(HouseClass* pHouse) const {
    if (!pHouse) return false;
    int32 tl = pHouse->TechLevel;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(tl, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::EvaluateWeight() const {
    return Weight_Current >= Weight_Minimum;
}

void AITriggerTypeClass::RegisterSuccess() {
    ++TimesCompleted;
    ++TimesExecuted;
    double delta = RulesClass::Instance ? RulesClass::Instance->AITriggerSuccessWeightDelta : 0.1;
    double coeff = RulesClass::Instance ? RulesClass::Instance->AITriggerTrackRecordCoefficient : 0.9;
    Weight_Current = Weight_Current * coeff + Weight_Minimum * delta;
    if (Weight_Current < Weight_Minimum) Weight_Current = Weight_Minimum;
}

void AITriggerTypeClass::RegisterFailure() {
    ++TimesExecuted;
    double delta = RulesClass::Instance ? RulesClass::Instance->AITriggerFailureWeightDelta : 0.1;
    Weight_Current = Weight_Current * (1.0 + delta);
    if (Weight_Current > Weight_Maximum) Weight_Current = Weight_Maximum;
}

double AITriggerTypeClass::GetSelectionProbability() const {
    if (!IsEnabled) return 0.0;
    double range = Weight_Maximum - Weight_Minimum;
    if (range <= 0.0) return 1.0;
    double normalized = (Weight_Current - Weight_Minimum) / range;
    return 1.0 - normalized;
}

bool AITriggerTypeClass::OwnerHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse) const {
    if (!CallingHouse || !TargetHouse) return false;
    if (OwnerHouseType == AITriggerHouseType::Single) {
        if (HouseIndex >= 0) {
            return OwnerHouseIndexMatches(CallingHouse);
        }
        return CallingHouse == TargetHouse;
    }
    if (OwnerHouseType == AITriggerHouseType::Any) {
        return true;
    }
    if (OwnerHouseType == AITriggerHouseType::Team) {
        return CallingHouse == TargetHouse;
    }
    return CallingHouse == TargetHouse;
}

bool AITriggerTypeClass::CivilianHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse) const {
    if (!CallingHouse || !TargetHouse) return false;
    return !CallingHouse->IsHumanPlayer && !CallingHouse->PlayerControl;
}

bool AITriggerTypeClass::EnemyHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse) const {
    if (!CallingHouse || !TargetHouse) return false;
    if (CallingHouse == TargetHouse) return false;
    return CallingHouse->IsHostileTo(TargetHouse);
}

bool AITriggerTypeClass::IronCurtainCharged(HouseClass* CallingHouse, HouseClass* TargetHouse) const {
    if (!CallingHouse || !TargetHouse) return false;
    int32 swIdx = CallingHouse->FindSuperWeapon(SuperWeaponType::IronCurtain);
    if (swIdx < 0) return false;
    return !CallingHouse->SuperWeaponTimers[swIdx].InProgress();
}

bool AITriggerTypeClass::ChronoSphereCharged(HouseClass* CallingHouse, HouseClass* TargetHouse) const {
    if (!CallingHouse || !TargetHouse) return false;
    int32 swIdx = CallingHouse->FindSuperWeapon(SuperWeaponType::ChronoSphere);
    if (swIdx < 0) return false;
    return !CallingHouse->SuperWeaponTimers[swIdx].InProgress();
}

bool AITriggerTypeClass::HouseCredits(HouseClass* CallingHouse, HouseClass* TargetHouse) const {
    if (!CallingHouse || !TargetHouse) return false;
    int32 credits = CallingHouse->Credits;
    for (int32 i = 0; i < 4; ++i) {
        const auto& cmp = Conditions[i];
        if (cmp.ComparatorType == 0) break;
        if (EvaluateComparator(credits, cmp.ComparatorType, cmp.ComparatorOperand)) return true;
    }
    return false;
}

bool AITriggerTypeClass::OwnerHouseIndexMatches(HouseClass* pHouse) const {
    return pHouse && pHouse->ArrayIndex == HouseIndex;
}

void AITriggerTypeClass::FormatForSaving(char* buffer, size_t size) const {
    const char* Team1Name = "<none>";
    const char* Team2Name = "<none>";
    const char* HouseName = "<none>";
    const char* ConditionName = "<none>";

    if (Team1) Team1Name = Team1->get_ID();
    if (Team2) Team2Name = Team2->get_ID();

    if (OwnerHouseType == AITriggerHouseType::Single) {
        if (HouseIndex != -1) {
            HouseName = "House";
        }
    } else if (OwnerHouseType == AITriggerHouseType::Any) {
        HouseName = "<all>";
    }

    if (ConditionObject) {
        ConditionName = ConditionObject->get_ID();
    }

    snprintf(buffer, size, "%s,%s,%s,%s,%d,%d,%s,%lf,%lf,%lf,%u,%d,%d,%u,%s,%u,%u,%u\n",
        this->ID, this->Name, Team1Name, HouseName, this->TechLevel,
        static_cast<int32>(this->ConditionType), ConditionName,
        this->Weight_Current, this->Weight_Minimum, this->Weight_Maximum,
        static_cast<uint32>(this->IsForSkirmish), 0, this->SideIndex,
        static_cast<uint32>(this->IsForBaseDefense), Team2Name,
        static_cast<uint32>(this->Enabled_Easy),
        static_cast<uint32>(this->Enabled_Normal),
        static_cast<uint32>(this->Enabled_Hard));
}