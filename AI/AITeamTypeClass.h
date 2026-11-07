#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

// ============================================================================
// 前置声明
// ============================================================================
class CCINIClass;
class HouseClass;
class TagTypeClass;
class ScriptTypeClass;
class TaskForceClass;
class AITeamClass;
class TeamClass;
class TechnoClass;
class TechnoTypeClass;

// ============================================================================
// AITeamTypeClass —— AI 队伍模板（[AITeamTypes] / [TeamTypes] 段里的每个名字）
//
// 原版把这一类命名为 TeamTypeClass（见 asm struc AITeamTypeClass，
// sizeof=0xF8）。OpenYR 中已存在一个独立的 TeamTypeClass（人类可用队伍
// 模板），故本类保留 AITeamTypeClass 之名。字段顺序与偏移严格对齐 asm
// struc 的定义，便于按 +0xNN 交叉核对。
// ============================================================================
class NOVTABLE AITeamTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TeamType;

    static DynamicVectorClass<AITeamTypeClass*>* Array;

    static AITeamTypeClass* Find(const char* pID);
    // asm: TeamTypeClass_FindByString (0x6F1010) —— 依次比对 ID(nameINI) 与
    // Name(nameFlag) 两个字符串。
    static AITeamTypeClass* FindByString(const char* pID);
    static AITeamTypeClass* FindOrAllocate(const char* pID);

    // asm: TeamTypes_CreateFromINIList —— 遍历 [TeamTypes] 逐名建型。
    static void CreateFromINIList(CCINIClass* pINI, bool isGlobal);

    AITeamTypeClass(const char* pID) noexcept;
    explicit AITeamTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}
    virtual ~AITeamTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    // asm: vt_AITeamTypeClass.TTC_GetAbstractDerivationID -> 常量 0x23
    virtual AbstractType GetAbstractDerivationID() const;
    // asm: TeamTypeClass_GetArrayIndex (+0x98 下标缓存)
    virtual int32 GetArrayIndex() const;
    // asm: TeamTypeClass_PointerGotInvalid (0x6F1010) retn 8
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed);
    // asm: TeamTypeClass_GetCRC (0x6F1BE0) retn 4
    virtual int32 GetCRC() const;

    // asm: TeamTypeClass_LoadFromINI (0x6F10A0) retn 4
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    // asm: TeamTypeClass_SaveToINI (0x6F11B0) retn 4
    virtual bool SaveToINI(CCINIClass* pINI) override;

    // asm: TeamTypeClass_SaveToINIList (0x6F1AB0) —— [TeamTypes] 段整体回写。
    void SaveToINIList(CCINIClass* pINI, int32 owner);

    // asm: TeamTypeClass_GetHouse (0x6F2070) —— 显式阵营或多人 idxHouse。
    HouseClass* GetHouse() const;

    // asm: TeamTypeClass_GetGroup (0x6F1880) —— Group 未设时回落到 TaskForce。
    int32 GetGroup() const;
    // asm: TeamTypeClass_FindWaypoint (0x6F18A0) retn 4
    CellStruct* GetWaypoint(CellStruct* buffer) const;
    // asm: TeamTypeClass_FindTransportWaypoint (0x6F18E0) retn 4
    CellStruct* GetTransportWaypoint(CellStruct* buffer) const;

    // asm: TeamTypeClass_CreateTeam (0x6F09D0) retn 4
    // 原版返回 AITeamClass*（new AITeamClass + TeamClass_CTOR）。
    AITeamClass* CreateTeam(HouseClass* pHouse);
    // asm: TeamTypeClass_DestroyAllInstances (0x6F0A70)
    void DestroyAllInstances();
    // asm: TeamTypeClass_FindFirstInstance (0x6F1F70) —— 返回 AITeamClass*。
    AITeamClass* FindFirstInstance() const;
    // asm: TeamTypeClass_SetAllFlashing (0x6F1F30) retn 4
    void SetAllFlashing(int32 duration);
    // asm: TeamTypeClass_CanRecruitUnit (0x6F1E20) retn 8
    bool CanRecruitUnit(TechnoClass* pUnit, HouseClass* pOwner) const;

    // asm: AITeamTypeClass_ProcessTaskforce (0x6F1F80)
    void ProcessTaskforce();
    // asm: AITeamTypeClass_ProcessAllTaskforces (0x6F2030)
    static void ProcessAllTaskforces();

    // ------------------------------------------------------------------------
    // 字段（偏移严格对齐 asm struc AITeamTypeClass）
    // ------------------------------------------------------------------------
    // asm: idxTeam (+0x98)，本类型在 Array 中的下标缓存。
    int32 ArrayIndex;
    int32 Group;                 // +0x9C
    int32 VeteranLevel;          // +0xA0
    bool Loadable;               // +0xA4
    bool Full;                   // +0xA5
    bool Annoyance;              // +0xA6
    bool GuardSlower;            // +0xA7
    bool Recruiter;              // +0xA8
    bool Autocreate;             // +0xA9
    bool Prebuild;               // +0xAA
    bool Reinforce;              // +0xAB
    bool Whiner;                 // +0xAC
    bool Aggressive;             // +0xAD
    bool LooseRecruit;           // +0xAE
    bool Suicide;                // +0xAF
    bool Droppod;                // +0xB0
    bool UseTransportOrigin;     // +0xB1
    bool field_B2;               // +0xB2（构造清零）
    bool OnTransOnly;            // +0xB3
    int32 Priority;              // +0xB4
    int32 Max;                   // +0xB8
    int32 field_BC;              // +0xBC（构造清零）
    int32 MindControlDecision;   // +0xC0
    HouseClass* House;           // +0xC4
    int32 idxHouse;              // +0xC8
    int32 TechLevel;             // +0xCC
    TagTypeClass* Tag;           // +0xD0
    int32 Waypoint;              // +0xD4
    int32 TransportWaypoint;     // +0xD8
    int32 cntInstances;          // +0xDC
    ScriptTypeClass* ScriptType; // +0xE0
    TaskForceClass* TaskForce;   // +0xE4
    int32 isGlobal;              // +0xE8
    int32 field_EC;              // +0xEC（构造赋 9）
    bool field_F0;               // +0xF0（构造置 1）
    bool field_F1;               // +0xF1（构造清零）
    bool AvoidThreats;           // +0xF2
    bool IonImmune;              // +0xF3
    bool TransportsReturnOnUnload;    // +0xF4
    bool AreTeamMembersRecruitable;   // +0xF5（构造置 1）
    bool IsBaseDefense;          // +0xF6
    bool OnlyTargetHouseEnemy;   // +0xF7
};
