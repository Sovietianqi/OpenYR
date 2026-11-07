#pragma once

#include <Abstract/ObjectClass.h>
#include <Containers/DynamicVectorClass.h>
#include <Math/Timer.h>

// ============================================================================
// MissionControlClass - mission settings from INI
//
//  MissionControlClass struc (asm sizeof = 0x20):
//      +0x00 ArrayIndex   (int32, -1 when unset)
//      +0x04 NoThreat     (bool)
//      +0x05 Zombie       (bool)
//      +0x06 Recruitable  (bool)
//      +0x07 Paralyzed    (bool)
//      +0x08 Retaliate    (bool)
//      +0x09 Scatter      (bool)
//      +0x10 Rate   (double, 0.016)
//      +0x18 AARate (double, 0.016)
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
    // MissionClass_LoadMissionControlFromINI (asm:912342): thiscall, retn 4.
    // Returns false when the [<mission name>] section is missing (asm: the
    // Find_Section result gates the whole body and feeds the `al` return).
    bool LoadFromINI(CCINIClass* pINI);

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
//
//  MissionClass struc (asm sizeof = 0xD4, ObjectClass = 0xA4):
//      +0xA4 LineTrailer              (void*, mission-script cursor)
//      +0xA8 currentMission           (enum eMission, -1 = none)
//      +0xAC what                     (mission stashed for Mission_Revert)
//      +0xB0 QueuedMission            (enum eMission)
//      +0xB4 field_B4   (db + 3 pad)  (= 4-byte slot; the requested next mission)
//      +0xB8 MissionStatus            (byte in asm; int32 here; reset on every fresh start)
//      +0xBC CurrentMissionStartTime  (int32, frame stamp)
//      +0xC0 field_C0                 (int32, frame stamp / per-mission tick counter)
//      +0xC4 field_C4                 (int32, zeroed by NextMission)
//      +0xC8 MissionTimer             (TimerStruct, 0xC bytes)
//          +0xC8 TimeStart / +0xCC TimeLeft / +0xD0 field_8 (last step result)
//
//  Field name mapping to the legacy OpenYR names used before this rewrite:
//      old SuspendedMission          -> field_B4
//      old unknown_bool_B4           -> MissionStatus
//      old MissionStatus             -> CurrentMissionStartTime
//      old CurrentMissionStartTime   -> field_C0
//      old unknown_C0 + UpdateTimer  -> MissionTimer + field_CC
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
    // MissionClass_Update (asm:911390): per-frame mission state machine.  The
    // ObjectClass vtable owns the slot; TechnoClass_Update tail-calls it at
    // +805.  It chains ObjectClass::Update, gates on the liveness flag and the
    // mission timer and the health hook, then runs AI_Mission().
    virtual void Update() override;
    // MissionClass_Update +0x61 (asm:911457): the 32-entry jump table
    // (off_5B34E8) dispatch.  Runs the handler for the running mission and
    // re-stamps the mission timer with the handler's result.
    //
    //   ★ 命名说明：asm 里这一段是 MissionClass_Update 的内联跳转表，没有独立
    //   过程名。OpenYR 曾把它叫 AI()，但 FootClass::AI 是另一个东西（足类逐帧
    //   主体，void 返回），两者同名会造成虚函数返回类型冲突。故改名为
    //   AI_Mission()，语义不变（仍是任务状态机的一步）。
    virtual int32 AI_Mission();
    // Commence (asm: shared prologue of ForceMission / NextMission /
    // Override_Mission): stamp the mission-start bookkeeping and run the
    // current mission's first step.
    virtual void Commence();

    virtual bool QueueMission(Mission mission, bool start_mission);
    virtual bool NextMission();
    virtual void ForceMission(Mission mission);
    virtual void Override_Mission(Mission mission, AbstractClass* target, AbstractClass* destination);
    virtual bool Mission_Revert();
    virtual bool MissionIsOverriden() const;
    virtual bool ReadyToNextMission() const;
    // MissionClass_GetCRC (asm:912663): folds the mission bookkeeping into the
    // save/multiplayer checksum.
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ------------------------------------------------------------------------
    // MissionClass_Update (asm:911392) health gate.
    //
    // asm +56: `mov eax,[edi+_ObjectClass.o.Health] / test eax,eax / jle out`.
    // The original ObjectClass carries Health at +0x64; OpenYR keeps Health in
    // TechnoClass, so the gate is expressed as a virtual hook.  TechnoClass is
    // expected to override it with `return Health;`.  The base answer keeps
    // non-techno mission owners (the abstract layer itself) running.
    // ------------------------------------------------------------------------
    virtual int32 GetMissionHealth() const { return 1; }

    // ------------------------------------------------------------------------
    // Mission timer helpers (asm:911392 gate + asm:912663 CRC share the same
    // TimeStart/TimeLeft math, and the four "restart the mission" prologues all
    // write Frame into TimeStart).
    // ------------------------------------------------------------------------
    // True when the mission step is allowed to run this frame (asm +1C..+4C).
    bool MissionTimerExpired() const;
    // Frames still owed before the next step; starts at TimeLeft and is drained
    // by Frame - TimeStart.  Never negative.
    int32 RemainingMissionTime() const;
    // asm +A8..+B8 epilogue: TimeStart = Frame, TimeLeft = 0, field_8 = result.
    void StampMissionTimer(int32 result);

    // Mission state handlers (28 of the 32 jump-table targets)
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
    // Executes one step of `mission` through the jump-table dispatch.
    int32 ExecuteMission(Mission mission);
    // Parks `mission` as the running mission (no queue arbitration).
    void SuspendMission(Mission mission);
    // MissionClass::Assign_Mission (asm:912116, retn 8 = 2 args): the asm-level
    // "remember and optionally start the next mission" entry.  Kept non-virtual
    // so the derived vtable layout is untouched; QueueMission() wraps it.
    void Assign_Mission(Mission mission, bool start_mission);
    bool Queue_Mission(Mission mission);

    // ========================================================================
    // Mission-layer queries
    // ========================================================================
    // MissionClass::FindNameByIdx (asm:912319): the INI section name of the
    //   *current* mission, or "<none>" when none is set (-1).
    const char* FindNameByIdx() const;
    // MissionClass_IsRecruitable (asm:912267): whether the current mission lets
    //   the owner recruit this object into an AI team.  An unset mission (-1)
    //   is recruitable by default; otherwise the MissionControlClass table
    //   supplies the answer.
    bool IsRecruitable() const;
    // MissionClass_ResetMission (asm:912236): promotes a queued mission into the
    //   `what` slot, clearing the queue and the status byte.  Returns false when
    //   nothing was queued.
    bool ResetMission();
    // Base half of TechnoClass::Restore_Mission (asm:1574901): ResetMission and,
    //   when it succeeded, restart the step machine on the restored mission.
    //   TechnoClass chains its own destination restore on top.
    bool RestoreMission();
    // TechnoClass_184 (asm:911369): the mission the object is *about* to run,
    //   i.e. currentMission when set, otherwise the queued mission.
    Mission GetMission() const;
    // The mission parked for the next NextMission() promotion (-1 when none).
    // asm: plain load of +0xB0, used by Assign_Mission / Override_Mission /
    // ResetMission; exposed so the team layer can inspect the pending order
    // without touching the raw field.
    Mission GetQueuedMission() const;
    // The override slot (`what`, +0xAC): the mission Mission_Revert returns to.
    Mission GetOverriddenMission() const;
    // currentMission == mission (asm: no dedicated proc; inline comparison used
    //   by the mission handlers/team logic).
    bool IsMission(Mission mission) const;
    // MissionClass::Get_Mission_Control_Data (asm:912737): the
    //   MissionControlClass record for the running mission.
    MissionControlClass* GetMissionControlData() const;
    // MissionClass_LoadMissionControlFromINI (asm:912342): loads the whole
    //   MissionControlClass table out of the [MissionControl] sections.
    static void LoadMissionControlFromINI(CCINIClass* pINI);

    // ------------------------------------------------------------------------
    // MissionControlClass flag accessors (inline reads of the 0x20-byte record
    // the asm reaches through Get_Mission_Control_Data at asm:912737).
    // All of them answer "no"/1.0 when currentMission is -1, so the AI layer can
    // call them unconditionally.
    // ------------------------------------------------------------------------
    bool   IsNoThreat() const;      // +0x04
    bool   IsZombie() const;        // +0x05
    bool   IsParalyzed() const;     // +0x07
    bool   CanRetaliate() const;    // +0x08
    bool   CanScatter() const;      // +0x09
    double GetMissionRate() const;  // +0x10
    double GetMissionAARate() const;// +0x18

    // ========================================================================
    // Constructor
    // ========================================================================
    MissionClass() noexcept;

protected:
    explicit __forceinline MissionClass(noinit_t) noexcept : ObjectClass(noinit) {}

public:
    // ------------------------------------------------------------------------
    // Properties (see the struc dump in the class banner above)
    // ------------------------------------------------------------------------
    void*         LineTrailer;             // +0xA4
    Mission       CurrentMission;          // +0xA8  currentMission
    Mission       what;                    // +0xAC
    Mission       QueuedMission;           // +0xB0
    int32         field_B4;                // +0xB4  requested next mission
    int32         MissionStatus;           // +0xB8
    int32         CurrentMissionStartTime; // +0xBC
    int32         field_C0;                // +0xC0
    int32         field_C4;                // +0xC4  asm: zeroed by NextMission
    CDTimerClass  MissionTimer;            // +0xC8  TimerStruct {TimeStart, TimeLeft, field_8}
    int32         field_CC;                // +0xD0  TimerStruct.field_8 (last step result)

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
