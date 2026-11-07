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
class TagTypeClass;
class TechnoClass;

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

    // ------------------------------------------------------------------------
    // asm: TeamTypeClass_CreateInstance (0x65D8E0)
    // 把本类型"AITeamClass_ProcessTaskforce"-式的投产请求落成一个真实的
    // 队伍：校验 TaskForce/Script 就绪后建队、把队伍推进到任务力脚本首行，
    // 再按空降/机降（PDPLANE）或地面两种投送方式把队员送到目标航点；成功
    // 至少投出一个成员时返回真。
    // ------------------------------------------------------------------------
    bool CreateInstance(int32 idxWaypoint);

    // ------------------------------------------------------------------------
    // asm: TeamTypeClass_CreateTeam (0x6F09D0)
    // 按拥有者与投产上限决定是否新开一支队伍；返回新建的 TeamClass 或空。
    // ------------------------------------------------------------------------
    TeamClass* CreateTeam(HouseClass* pHouse);

    // ------------------------------------------------------------------------
    // asm: TeamTypeClass_DestroyAllInstances (0x6F0A70)
    // 让本类型名下的所有活动队立即消失。
    // ------------------------------------------------------------------------
    void DestroyAllInstances();

    // asm: TeamTypeClass_CTOR (0x6F07D0)
    TeamTypeClass(const char* pID) noexcept;
    explicit TeamTypeClass(noinit_t) noexcept
        : AbstractTypeClass(noinit), Waypoint(-1), TransportWaypoint(-1),
          ScriptType(nullptr), ReplayScriptType(nullptr),
          TaskForce(nullptr), ArrayIndex(-1), Max(-1), Grouping(-1),
          OriginHouseIndex(-1), House(nullptr), Annoyance(0), Priority(7),
          VeteransLevel(1), Tag(nullptr), TechLevel(0), CountInstances(0),
          MindControlDecision(0), IsGlobal(0), field_BC(0), field_EC(9),
          Autocreate(false), Full(false), AreMembersRecruitable(true),
          Suicide(false), Loadable(false), Prebuild(false),
          TransportsReturn(false), Aggressive(false), UseSibling(false),
          Easy(false), Normal(false), Hard(false), GuardSlower(false),
          Reinforce(false), Whiner(false), LooseRecruit(false), Droppod(false),
          UseTransportOrigin(false), OnTransOnly(false), AvoidThreats(false),
          IonImmune(false), TransportsReturnOnUnload(false), IsBaseDefense(false),
          OnlyTargetHouseEnemy(false) {}
    virtual ~TeamTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    // asm: vt_AITeamTypeClass.TTC_GetAbstractDerivationID
    virtual AbstractType GetAbstractDerivationID() const;
    // asm: TeamTypeClass_GetSize (0x6F21B0) - 固定 0xF8
    virtual int32 Size() const override;
    // asm: TeamTypeClass_GetArrayIndex (0x6F21C0)
    virtual int32 GetArrayIndex() const override;
    // asm: TeamTypeClass_PointerGotInvalid (0x6F1010)
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed);

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    // asm: TeamTypeClass_SaveToINI (0x6F11B0)
    virtual bool SaveToINI(CCINIClass* pINI) override;
    virtual int32 GetCRC() const;

    // ------------------------------------------------------------------------
    // asm: TeamTypeClass_SaveToINIList (0x6F1AB0)
    // 场景 [TeamTypes] 段的整体回写：先规范化既有键，再把归属给定拥有者
    // （isGlobal 匹配）的类型逐条编号写出并调用各自的 SaveToINI。
    // ------------------------------------------------------------------------
    void SaveToINIList(CCINIClass* pINI);

    // asm: TeamTypeClass_GetHouse (0x6F2070)
    HouseClass* GetHouse() const;

    // asm: TeamTypeClass_FindFirstInstance (0x6F1F70)
    TeamClass* FindFirstInstance() const;

    // asm: TeamTypeClass_CanRecruitUnit (0x6F1E20)
    bool CanRecruitUnit(TechnoClass* pUnit, HouseClass* pOwner) const;

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
    int32 Waypoint;
    int32 TransportWaypoint;

    ScriptTypeClass* ScriptType;
    ScriptTypeClass* ReplayScriptType;
    TaskForceClass*  TaskForce;
    // asm: idxTeam     (+0x98)，本类型在 TeamTypeClass::Array 中的下标缓存。
    int32  ArrayIndex;
    int32  Max;
    int32  Grouping;
    // asm: idxHouse   (+0xC8)，来源阵营的国别下标；越界/未设时为 -1。
    int32  OriginHouseIndex;
    // asm: House      (+0xC4)，被显式指定的归属阵营指针（可空）。
    HouseClass* House;
    int32  Annoyance;
    int32  Priority;
    int32  VeteransLevel;
    // asm: Tag        (+0xD0)，本类型绑定的标签（可空）。
    TagTypeClass* Tag;
    // asm: techLevel  (+0xCC)。
    int32  TechLevel;
    // asm: cntInstances (+0xDC)，当前活动队计数。
    int32  CountInstances;
    // asm: MindControlDecision (+0xC0)。
    int32  MindControlDecision;
    // asm: isGlobal   (+0xE8)，写入 [TeamTypes] 时按拥有者归属分组。
    int32  IsGlobal;
    // asm: field_BC   (+0xBC)。
    int32  field_BC;
    // asm: field_EC   (+0xEC)，构造值 9。
    int32  field_EC;
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
    // asm: GuardSlower  (+0xA7)。
    bool   GuardSlower;
    // asm: Reinforce    (+0xAB)。
    bool   Reinforce;
    // asm: Whiner       (+0xAC)。
    bool   Whiner;
    // asm: LooseRecruit (+0xAE)。
    bool   LooseRecruit;
    // asm: Droppod      (+0xB0)，投产时改用空降（PDPLANE）投送。
    bool   Droppod;
    // asm: UseTransportOrigin (+0xB1)，空投时按运输航点定位。
    bool   UseTransportOrigin;
    // asm: OnTransOnly  (+0xB3)。
    bool   OnTransOnly;
    // asm: AvoidThreats (+0xF2)。
    bool   AvoidThreats;
    // asm: IonImmune    (+0xF3)。
    bool   IonImmune;
    // asm: TransportsReturnOnUnload (+0xF4)。
    bool   TransportsReturnOnUnload;
    // asm: IsBaseDefense (+0xF6)。
    bool   IsBaseDefense;
    // asm: OnlyTargetHouseEnemy (+0xF7)。
    bool   OnlyTargetHouseEnemy;
};
