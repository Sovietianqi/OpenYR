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
    // Constructor
    // ========================================================================
    MissionClass() noexcept;

protected:
    explicit __forceinline MissionClass(noinit_t) noexcept : ObjectClass(noinit) {}

    // ========================================================================
    // Properties (offset 0xB8 from MissionClass start)
    // ========================================================================
public:
    Mission       CurrentMission;
    Mission       SuspendedMission;
    Mission       QueuedMission;
    bool          unknown_bool_B8;
    int32         MissionStatus;
    int32         CurrentMissionStartTime;
    DWORD         unknown_C4;
    CDTimerClass  UpdateTimer;
};