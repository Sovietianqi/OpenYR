#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class ScriptTypeClass;
class TaskForceClass;
class HouseClass;
class TeamClass;

// ============================================================================
// TeamTypeClass - definition of an AI team type
// Inherits AbstractTypeClass. A team type binds a TaskForceClass (the unit
// composition) to a ScriptTypeClass (the behavior script) along with build
// limits and flags. Parsed from the [TeamTypes] INI block.
// ============================================================================
class NOVTABLE TeamTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TeamType;

    static DynamicVectorClass<TeamTypeClass*>* Array;

    static TeamTypeClass* Find(const char* pID);
    // TeamTypes_CreateFromINIList: walks [TeamTypes] and builds one team
    // type per listed name.
    static void CreateFromINIList(CCINIClass* pINI);

    static TeamTypeClass* FindOrAllocate(const char* pID);
    static int32 GetCount();

    TeamTypeClass(const char* pID) noexcept;
    explicit TeamTypeClass(noinit_t) noexcept
        : AbstractTypeClass(noinit), ScriptType(nullptr), TaskForce(nullptr),
          Max(1), Autocreate(false), Full(false), AreMembersRecruitable(false),
          Annoyance(0), Suicide(false), Loadable(false), Prebuild(false),
          Grouping(-1), TransportsReturn(false), VeteransLevel(0),
          Priority(0), OriginHouseIndex(-1), ReplayScriptType(nullptr) {}
    virtual ~TeamTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual int32 GetCRC() const;

    // Task-required accessors
    TaskForceClass* Get_TaskForce() const;
    ScriptTypeClass* Get_ScriptType() const;
    ScriptTypeClass* Get_ReplayScriptType() const;
    int32 Get_Initial_Threat() const;
    int32 Get_Group() const;
    int32 Get_Max_Count() const;
    int32 Get_Priority() const;
    int32 Get_Veterans_Level() const;
    bool Is_Valid() const;
    bool Is_Recruiter() const;
    bool Is_Autocreate() const;
    bool Is_Full() const;
    bool Is_Suicide() const;
    bool Is_Loadable() const;
    bool Is_Prebuild() const;
    bool Is_Aggressive() const;
    bool Is_Transports_Return() const;
    bool Is_Allowed_Difficulty(int32 difficulty) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知队伍类型补全原版命名形态：按字符串/航点号/
    // 运输航点号查找类型；把名下所有活动队闪标切换；成员生成与超时空
    // 进入是队伍投产的两个入口。
    // ------------------------------------------------------------------------
    static TeamTypeClass* FindByString(const char* pID);
    static TeamTypeClass* FindWaypoint(int32 waypoint);
    static TeamTypeClass* FindTransportWaypoint(int32 waypoint);
    void SetAllFlashing(bool flash);
    void CreateMembers();
    void ChronoIn();

    // 航点登记：-1 表示未与航点挂钩。
    int32 Waypoint = -1;
    int32 TransportWaypoint = -1;

    ScriptTypeClass* ScriptType;
    ScriptTypeClass* ReplayScriptType;
    TaskForceClass*  TaskForce;
    int32  Max;
    int32  Grouping;
    int32  OriginHouseIndex;
    int32  Annoyance;
    int32  Priority;
    int32  VeteransLevel;
    bool   Autocreate;
    bool   Full;
    bool   AreMembersRecruitable;
    bool   Suicide;
    bool   Loadable;
    bool   Prebuild;
    bool   TransportsReturn;
    bool   Aggressive;
    bool   UseSibling;
    bool   Easy;
    bool   Normal;
    bool   Hard;
};
