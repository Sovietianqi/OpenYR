#include <Abstract/MissionClass.h>
#include <INI/INIClass.h>

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<MissionControlClass> MissionControlClass::Array;

// ============================================================================
// MissionControlClass
// ============================================================================
MissionControlClass::MissionControlClass()
    : ArrayIndex(-1)
    , NoThreat(false)
    , Zombie(false)
    , Recruitable(false)
    , Paralyzed(false)
    , Retaliate(false)
    , Scatter(false)
    , Rate(0.016)
    , AARate(0.016)
{
}

const char* MissionControlClass::GetName()
{
    return MissionControlClass::FindName(static_cast<Mission>(ArrayIndex));
}

const char* MissionControlClass::FindName(const Mission& index)
{
    static const char* names[] = {
        "Sleep", "Attack", "Move", "QMove", "Retreat", "Guard", "Sticky",
        "Enter", "Capture", "Eaten", "Harvest", "Area Guard", "Return",
        "Stop", "Ambush", "Hunt", "Unload", "Sabotage", "Construction",
        "Selling", "Repair", "Rescue", "Missile", "Harmless", "Open",
        "Patrol", "Paradrop Approach", "Paradrop Overfly", "Wait",
        "Attack Move", "Spyplane Approach", "Spyplane Overfly"
    };
    int32 idx = static_cast<int32>(index);
    if (idx >= 0 && idx < static_cast<int32>(Mission::Count)) {
        return names[idx];
    }
    return nullptr;
}

Mission MissionControlClass::FindIndex(const char* pName)
{
    if (!pName) return Mission::Sleep;
    for (int32 i = 0; i < static_cast<int32>(Mission::Count); ++i) {
        const char* name = FindName(static_cast<Mission>(i));
        if (name && _strcmpi(name, pName) == 0) {
            return static_cast<Mission>(i);
        }
    }
    return Mission::Sleep;
}

void MissionControlClass::LoadFromINI(CCINIClass* pINI)
{
    if (!pINI) return;

    const char* section = MissionControlClass::FindName(static_cast<Mission>(ArrayIndex));
    if (section == nullptr)
        section = "<none>";

    if (pINI->GetSection(section) == nullptr)
        return;

    NoThreat    = pINI->ReadBool(section, "NoThreat", NoThreat);
    Zombie      = pINI->ReadBool(section, "Zombie", Zombie);
    Recruitable = pINI->ReadBool(section, "Recruitable", Recruitable);
    Paralyzed   = pINI->ReadBool(section, "Paralyzed", Paralyzed);
    Retaliate   = pINI->ReadBool(section, "Retaliate", Retaliate);
    Scatter     = pINI->ReadBool(section, "Scatter", Scatter);
    Rate        = pINI->ReadFixed(section, "Rate", Rate);
    AARate      = pINI->ReadFixed(section, "AARate", AARate);
}

MissionControlClass* MissionControlClass::Find(const Mission& index)
{
    int32 idx = static_cast<int32>(index);
    if (idx < 0 || idx >= MissionControlClass::Array.Count)
        return nullptr;

    return &MissionControlClass::Array.Items[idx];
}

void MissionControlClass::LoadAllFromINI(CCINIClass* pINI)
{
    if (!pINI) return;

    if (MissionControlClass::Array.Count <= 0)
    {
        for (int32 i = 0; i < static_cast<int32>(Mission::Count); ++i)
        {
            if (MissionControlClass::FindName(static_cast<Mission>(i)) == nullptr)
                continue;

            MissionControlClass entry;
            entry.ArrayIndex = i;
            MissionControlClass::Array.Add(entry);
        }
    }

    for (int32 i = 0; i < MissionControlClass::Array.Count; ++i)
        MissionControlClass::Array.Items[i].LoadFromINI(pINI);
}

// ============================================================================
// MissionClass
// ============================================================================

// MissionClass_CTOR (asm 0x5B3680).
//
//  ObjectClass_CTOR, then every mission slot is set to -1 (no mission) and
//  the two counters / the timer are zeroed; MissionTimer.TimeStart is left 0
//  and its second word is stamped with the current frame.
MissionClass::MissionClass() noexcept
    : ObjectClass()
    , LineTrailer(nullptr)
    , CurrentMission(Mission::None)
    , PendingMission(Mission::None)
    , QueuedMission(Mission::None)
    , SuspendedMission(Mission::None)
    , unknown_bool_B4(false)
    , MissionStatus(0)
    , CurrentMissionStartTime(0)
    , unknown_C0(0)
    , UpdateTimer()
{
}

MissionClass::~MissionClass()
{
    // Clean up any pending mission state
    CurrentMission = Mission::Sleep;
    SuspendedMission = Mission::Sleep;
    QueuedMission = Mission::Sleep;
}

// ============================================================================
// Mission queue management
// ============================================================================

bool MissionClass::QueueMission(Mission mission, bool start_mission)
{
    // Queue a mission for later execution
    if (mission == Mission::Sleep || mission == Mission::Count) {
        return false;
    }

    QueuedMission = mission;

    if (start_mission) {
        return NextMission();
    }

    return true;
}

bool MissionClass::NextMission()
{
    // Pop the next mission from the queue
    if (QueuedMission == Mission::Sleep) {
        return false;
    }

    SuspendedMission = CurrentMission;
    CurrentMission = QueuedMission;
    QueuedMission = Mission::Sleep;
    MissionStatus = 0;
    CurrentMissionStartTime = FrameTimer::CurrentFrame;

    return true;
}

void MissionClass::ForceMission(Mission mission)
{
    // Force override the current mission
    if (mission == Mission::Sleep || mission == Mission::Count) {
        return;
    }

    SuspendedMission = CurrentMission;
    CurrentMission = mission;
    QueuedMission = Mission::Sleep;
    MissionStatus = 0;
    CurrentMissionStartTime = FrameTimer::CurrentFrame;
}

void MissionClass::Override_Mission(Mission mission, AbstractClass* target, AbstractClass* destination)
{
    // Temporary override - used for things like entering transports
    if (mission == Mission::Sleep || mission == Mission::Count) {
        return;
    }

    SuspendedMission = CurrentMission;
    CurrentMission = mission;
    QueuedMission = Mission::Sleep;
    MissionStatus = 0;
    CurrentMissionStartTime = FrameTimer::CurrentFrame;
}

bool MissionClass::Mission_Revert()
{
    // Revert to the previously suspended mission
    if (SuspendedMission == Mission::Sleep) {
        return false;
    }

    CurrentMission = SuspendedMission;
    SuspendedMission = Mission::Sleep;
    MissionStatus = 0;
    CurrentMissionStartTime = FrameTimer::CurrentFrame;

    return true;
}

bool MissionClass::MissionIsOverriden() const
{
    return SuspendedMission != Mission::Sleep;
}

bool MissionClass::ReadyToNextMission() const
{
    return QueuedMission != Mission::Sleep;
}

// ============================================================================
// Mission state handlers (28 missions)
//
//  Every one of these is the *base* stub of a mission-step virtual.  The
//  original compiles each of them down to a bare constant return:
//      MissionClass_Mi_<Name>  ->  mov eax, <const> ; retn
//  so the whole point of the base class is to give every mission a default
//  "keep running" answer.  MissionClass_Mi_Return and MissionClass_Mi_Stop are
//  the two exceptions, both returning 0x450 instead of 0x1C2.
//
//      const 0x1C2 (= 450)  - step not finished, stay on this mission
//      const 0x450 (= 1104) - step finished, the mission chain may advance
//
//  FootClass / TechnoClass override the missions they actually implement.
// ============================================================================

// The default answer for every mission step (asm mov eax, 1C2h).
static const int32 MISSION_NOT_DONE = 0x1C2;
// The two missions that complete in their base form (asm mov eax, 450h).
static const int32 MISSION_DONE     = 0x450;

// MissionClass_Mi_Sleep (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Sleep()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Harmless (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Harmless()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Ambush (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Ambush()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Attack (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Attack()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Capture (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Capture()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Eaten (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Eaten()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Guard (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Guard()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_AreaGuard (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_AreaGuard()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Harvest (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Harvest()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Hunt (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Hunt()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Move (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Move()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Retreat (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Retreat()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Return (asm: mov eax, 450h ; retn).
int32 MissionClass::Mission_Return()
{
    return MISSION_DONE;
}

// MissionClass_Mi_Stop (asm: mov eax, 450h ; retn).
int32 MissionClass::Mission_Stop()
{
    return MISSION_DONE;
}

// MissionClass_Mi_Unload (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Unload()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Enter (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Enter()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Construction (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Construction()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Selling (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Selling()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Repair (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Repair()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Missile (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Missile()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Open (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Open()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Rescue (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Rescue()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Patrol (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Patrol()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_ParaDropApproach (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_ParaDropApproach()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_ParaDropOverfly (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_ParaDropOverfly()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Wait (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_Wait()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_SpyPlaneApproach (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_SpyPlaneApproach()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_SpyPlaneOverfly (asm: mov eax, 1C2h ; retn).
int32 MissionClass::Mission_SpyPlaneOverfly()
{
    return MISSION_NOT_DONE;
}

// ============================================================================
// Non-virtual helpers
// ============================================================================

int32 MissionClass::ExecuteMission(Mission mission)
{
    // Execute a specific mission by dispatching to the appropriate handler
    switch (mission) {
        case Mission::Sleep:              return Mission_Sleep();
        case Mission::Harmless:           return Mission_Harmless();
        case Mission::Ambush:             return Mission_Ambush();
        case Mission::Attack:             return Mission_Attack();
        case Mission::Capture:            return Mission_Capture();
        case Mission::Eaten:              return Mission_Eaten();
        case Mission::Guard:              return Mission_Guard();
        case Mission::AreaGuard:          return Mission_AreaGuard();
        case Mission::Harvest:            return Mission_Harvest();
        case Mission::Hunt:               return Mission_Hunt();
        case Mission::Move:               return Mission_Move();
        case Mission::Retreat:            return Mission_Retreat();
        case Mission::Return:             return Mission_Return();
        case Mission::Stop:               return Mission_Stop();
        case Mission::Unload:             return Mission_Unload();
        case Mission::Enter:              return Mission_Enter();
        case Mission::Construction:       return Mission_Construction();
        case Mission::Selling:            return Mission_Selling();
        case Mission::Repair:             return Mission_Repair();
        case Mission::Missile:            return Mission_Missile();
        case Mission::Open:               return Mission_Open();
        case Mission::Rescue:             return Mission_Rescue();
        case Mission::Patrol:             return Mission_Patrol();
        case Mission::ParaDropApproach:   return Mission_ParaDropApproach();
        case Mission::ParaDropOverfly:    return Mission_ParaDropOverfly();
        case Mission::Wait:               return Mission_Wait();
        case Mission::SpyPlaneApproach:   return Mission_SpyPlaneApproach();
        case Mission::SpyPlaneOverfly:    return Mission_SpyPlaneOverfly();
        default:                          return -1;
    }
}

void MissionClass::SuspendMission(Mission mission)
{
    // Suspend the current mission and queue the given one
    SuspendedMission = CurrentMission;
    CurrentMission = mission;
    MissionStatus = 0;
}
// ============================================================================
// Mission-layer queries
// ============================================================================

// MissionClass_FindNameByIdx (asm 0x5B3730).
//
//  Returns the INI section name of the *current* mission.  A mission of -1
//  (no mission assigned) resolves to the literal "<none>" instead of a table
//  entry, exactly as the original does before indexing the name table.
const char* MissionClass::FindNameByIdx() const
{
    if (CurrentMission == Mission::None)
        return "<none>";

    return MissionControlClass::FindName(CurrentMission);
}

// MissionClass_IsRecruitable (asm 0x5B36D0).
//
//  The AI team recruiter asks the object whether its current mission permits
//  being pulled into a team.  An unassigned mission (-1) always answers yes;
//  otherwise the answer comes from the MissionControlClass table, which the
//  rules INI fills in.  The original indexes by `mission << 5`, i.e. by the
//  32-byte record stride.
bool MissionClass::IsRecruitable() const
{
    if (CurrentMission == Mission::None)
        return true;

    const int32 idx = static_cast<int32>(CurrentMission);
    if (idx < 0 || idx >= MissionControlClass::Array.Count)
        return false;

    return MissionControlClass::Array.Items[idx].Recruitable;
}

// MissionClass_ResetMission (asm 0x5B36C0).
//
//  Promotes a queued mission into the live slot.  With nothing queued (-1)
//  the call is a no-op that reports failure; otherwise the queued value moves
//  into `what`, the queue is cleared back to -1 and the step counter resets so
//  the fresh mission starts at its first step.
bool MissionClass::ResetMission()
{
    if (QueuedMission == Mission::None)
        return false;

    PendingMission = QueuedMission;
    QueuedMission = Mission::None;
    MissionStatus = 0;

    return true;
}

// MissionClass_LoadMissionControlFromINI (asm 0x5B3760).
//
//  RulesData_LoadTypeData reaches this to fill the whole [MissionControl]
//  block of the rules INI into the MissionControlClass table.  Each table
//  entry reads its own section, and the section name comes from the same
//  string table FindNameByIdx indexes.
void MissionClass::LoadMissionControlFromINI(CCINIClass* pINI)
{
    MissionControlClass::LoadAllFromINI(pINI);
}
