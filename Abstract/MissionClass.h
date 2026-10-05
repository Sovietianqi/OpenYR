#pragma once

#include <Abstract/ObjectClass.h>
#include <Containers/DynamicVectorClass.h>
#include <Math/Timer.h>

// ============================================================================
// MissionControlClass - mission settings from INI
// ============================================================================
class MissionControlClass {
public:
    static DynamicVectorClass<MissionControlClass> Array;
    static const char* FindName(const Mission& index);
    static Mission FindIndex(const char* pName);
    static void LoadAllFromINI(CCINIClass* pINI);
    static MissionControlClass* Find(const Mission& index);

    MissionControlClass();
    const char* GetName();
    void LoadFromINI(CCINIClass* pINI);

    int32  ArrayIndex;
    bool   NoThreat;
    bool   Zombie;
    bool   Recruitable;
    bool   Paralyzed;
    bool   Retaliate;
    bool   Scatter;
    double Rate;
    double AARate;
};

// ============================================================================
// MissionClass - base for all objects with mission AI
// Inherits ObjectClass
// Original offset: data starts at 0xB8 after ObjectClass
// ============================================================================
class NOVTABLE MissionClass : public ObjectClass {
public:
    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~MissionClass();

    // ========================================================================
    // MissionClass virtuals
    // ========================================================================
    virtual bool QueueMission(Mission mission, bool start_mission);
    virtual bool NextMission();
    virtual void ForceMission(Mission mission);
    virtual void Override_Mission(Mission mission, AbstractClass* target, AbstractClass* destination);
    virtual bool Mission_Revert();
    virtual bool MissionIsOverriden() const;
    virtual bool ReadyToNextMission() const;

    // Mission state handlers (31 missions)
    virtual int32 Mission_Sleep();
    virtual int32 Mission_Harmless();
    virtual int32 Mission_Ambush();
    virtual int32 Mission_Attack();
    virtual int32 Mission_Capture();
    virtual int32 Mission_Eaten();
    virtual int32 Mission_Guard();
    virtual int32 Mission_AreaGuard();
    virtual int32 Mission_Harvest();
    virtual int32 Mission_Hunt();
    virtual int32 Mission_Move();
    virtual int32 Mission_Retreat();
    virtual int32 Mission_Return();
    virtual int32 Mission_Stop();
    virtual int32 Mission_Unload();
    virtual int32 Mission_Enter();
    virtual int32 Mission_Construction();
    virtual int32 Mission_Selling();
    virtual int32 Mission_Repair();
    virtual int32 Mission_Missile();
    virtual int32 Mission_Open();
    virtual int32 Mission_Rescue();
    virtual int32 Mission_Patrol();
    virtual int32 Mission_ParaDropApproach();
    virtual int32 Mission_ParaDropOverfly();
    virtual int32 Mission_Wait();
    virtual int32 Mission_SpyPlaneApproach();
    virtual int32 Mission_SpyPlaneOverfly();

    // ========================================================================
    // Non-virtual helpers
    // ========================================================================
    int32 ExecuteMission(Mission mission);
    void SuspendMission(Mission mission);

    // ========================================================================
    // Mission-layer queries (asm: all read the fields below)
    // ========================================================================
 // MissionClass_FindNameByIdx: the INI section name of the
    //   *current* mission, or "<none>" when none is set (-1).
    const char* FindNameByIdx() const;
 // MissionClass_IsRecruitable: whether the current mission
    //   lets the owner recruit this object into an AI team.  An unset mission
    //   (-1) is recruitable by default; otherwise the MissionControlClass
    //   table supplies the answer.
    bool IsRecruitable() const;
 // MissionClass_ResetMission: promotes a queued mission into
    //   the current slot, clearing the queue and the step counter.  Returns
    //   false when nothing was queued.
    bool ResetMission();
 // MissionClass_LoadMissionControlFromINI: loads the whole
    //   MissionControlClass table out of the [MissionControl] sections.
    static void LoadMissionControlFromINI(CCINIClass* pINI);

    // ========================================================================
    // Constructor
    // ========================================================================
    MissionClass() noexcept;

protected:
    explicit __forceinline MissionClass(noinit_t) noexcept : ObjectClass(noinit) {}

    // ========================================================================
    // Properties
    //
    //  MissionClass struc (asm sizeof = 0xD0, ObjectClass = 0xA4):
    //      +0xA4 LineTrailer
    //      +0xA8 currentMission        (enum eMission)
    //      +0xAC what                  (pending mission promoted by ResetMission)
    //      +0xB0 QueuedMission         (enum eMission)
    //      +0xB4 field_B4              (byte)
    //      +0xB8 MissionStatus         (int32)
    //      +0xBC CurrentMissionStartTime
    //      +0xC0 field_C0
    //      +0xC4 MissionTimer          (TimerStruct)
    // ========================================================================
public:
    // LineTrailer (+0xA4): scratch pointer the mission line parser uses while
    // stepping through a mission script; null between scripts.
    void*         LineTrailer;
    // currentMission (+0xA8): the mission being executed right now.
    Mission       CurrentMission;
    // what (+0xAC): the mission promoted from the queue by ResetMission.
    Mission       PendingMission;
    // QueuedMission (+0xB0): the mission QueueMission stored, awaiting
    // promotion by ResetMission.
    Mission       QueuedMission;
    // SuspendedMission: the mission saved by Mission_Revert / Override_Mission
    // so it can be restored later (reconstruction-only; the original keeps the
    // previous mission in the object's own mission field).
    Mission       SuspendedMission;
    bool          unknown_bool_B4;
    int32         MissionStatus;
    int32         CurrentMissionStartTime;
    DWORD         unknown_C0;
    CDTimerClass  UpdateTimer;
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Mi_ 系列与 Mission_ 系列状态处理器一一对应，
    // 为原版命名形态的转发入口。
    // ------------------------------------------------------------------------
    virtual int32 Mi_Sleep();
    virtual int32 Mi_Harmless();
    virtual int32 Mi_Ambush();
    virtual int32 Mi_Harvest();
    virtual int32 Mi_Return();
    virtual int32 Mi_Stop();
    virtual int32 Mi_Wait();
    virtual int32 Mi_Open();
    virtual int32 Mi_Construction();
    virtual int32 Mi_Selling();

};