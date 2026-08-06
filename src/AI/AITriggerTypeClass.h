#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

struct AITriggerConditionComparator {
    int32 ComparatorType;
    int32 ComparatorOperand;
};

class AITriggerTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::AITriggerType;

    static DynamicVectorClass<AITriggerTypeClass*>* Array;

    static AITriggerTypeClass* Find(const char* pID);
    static AITriggerTypeClass* FindOrAllocate(const char* pID);

    virtual ~AITriggerTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI, bool Global);

    bool ConditionMet(HouseClass* CallingHouse, HouseClass* TargetHouse, bool EnoughBaseDefense) const;
    void RegisterSuccess();
    void RegisterFailure();
    double GetSelectionProbability() const;

    bool OwnerHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool CivilianHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool EnemyHouseOwns(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool HouseCredits(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool IronCurtainCharged(HouseClass* CallingHouse, HouseClass* TargetHouse) const;
    bool ChronoSphereCharged(HouseClass* CallingHouse, HouseClass* TargetHouse) const;

    bool EvaluateBuildingCount(HouseClass* pHouse) const;
    bool EvaluateUnitCount(HouseClass* pHouse) const;
    bool EvaluateInfantryCount(HouseClass* pHouse) const;
    bool EvaluateAircraftCount(HouseClass* pHouse) const;
    bool EvaluatePowerOutput(HouseClass* pHouse) const;
    bool EvaluateHasSuperWeapon(HouseClass* pHouse) const;
    bool EvaluateTechLevel(HouseClass* pHouse) const;
    bool EvaluateWeight() const;
    bool EvaluateComparator(int32 currentValue, int32 comparatorType, int32 comparatorOperand) const;
    bool OwnerHouseIndexMatches(HouseClass* pHouse) const;

    void FormatForSaving(char* buffer, size_t size) const;

    AITriggerTypeClass(const char* pID) noexcept;

protected:
    explicit AITriggerTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    AITriggerCondition ConditionType;
    int32 IsGlobal;
    AITriggerHouseType OwnerHouseType;
    bool IsEnabled;
    int32 HouseIndex;
    int32 SideIndex;
    int32 TechLevel;
    int32 unknown_B4;
    double Weight_Current;
    double Weight_Minimum;
    double Weight_Maximum;
    bool IsForSkirmish;
    bool IsForBaseDefense;
    bool Enabled_Easy;
    bool Enabled_Normal;
    bool Enabled_Hard;
    TechnoTypeClass* ConditionObject;
    TeamTypeClass* Team1;
    TeamTypeClass* Team2;
    AITriggerConditionComparator Conditions[4];
    int32 TimesExecuted;
    int32 TimesCompleted;
    int32 unknown_10C;
};