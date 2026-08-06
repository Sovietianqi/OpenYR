#include <Abstract/MissionClass.h>

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
        "Sleep", "Harmless", "Ambush", "Attack", "Capture", "Eaten",
        "Guard", "AreaGuard", "Harvest", "Hunt", "Move", "Retreat",
        "Return", "Stop", "Unload", "Enter", "Construction", "Selling",
        "Repair", "Missile", "Open", "Rescue", "Patrol",
        "ParaDropApproach", "ParaDropOverfly", "Wait",
        "SpyPlaneApproach", "SpyPlaneOverfly",
        // Yuri's Revenge extended missions.
        "Deploy", "Follow", "Spy", "EnterTunnel", "ChronoWarp",
        "ChronoSphere", "IronCurtain", "SelfDestruct", "Circle",
        "Recycle", "Sticky", "Emergency", "TakeCover", "Gibber"
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
    // Load mission control settings from INI
    // Example: NoThreat, Zombie, Recruitable, etc.
    // These would be read from rulesmd.ini [MissionControl] section
    NoThreat = false;
    Zombie = false;
    Recruitable = false;
    Paralyzed = false;
    Retaliate = false;
    Scatter = false;
    Rate = 0.016;
    AARate = 0.016;
}

// ============================================================================
// MissionClass
// ============================================================================

MissionClass::MissionClass() noexcept
    : ObjectClass()
    , CurrentMission(Mission::Guard)
    , SuspendedMission(Mission::Sleep)
    , QueuedMission(Mission::Sleep)
    , unknown_bool_B8(false)
    , MissionStatus(0)
    , CurrentMissionStartTime(0)
    , unknown_C4(0)
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
// Mission state handlers (31 missions)
// Each returns a status code: 0 = running, >0 = complete, <0 = error
// ============================================================================

int32 MissionClass::Mission_Sleep()
{
    // Do nothing - unit is sleeping
    return 1;
}

int32 MissionClass::Mission_Harmless()
{
    // Do nothing - harmless mode
    return 1;
}

int32 MissionClass::Mission_Ambush()
{
    // Wait for enemy to come in range, then attack
    return 1;
}

int32 MissionClass::Mission_Attack()
{
    // Move toward target and attack
    return 1;
}

int32 MissionClass::Mission_Capture()
{
    // Move to target building and capture it
    return 1;
}

int32 MissionClass::Mission_Eaten()
{
    // Being eaten by something
    return 1;
}

int32 MissionClass::Mission_Guard()
{
    // Guard current area - attack nearby enemies
    return 1;
}

int32 MissionClass::Mission_AreaGuard()
{
    // Guard a specific area
    return 1;
}

int32 MissionClass::Mission_Harvest()
{
    // Move to tiberium field, harvest, return to refinery
    return 1;
}

int32 MissionClass::Mission_Hunt()
{
    // Hunt for enemy units
    return 1;
}

int32 MissionClass::Mission_Move()
{
    // Move to destination
    return 1;
}

int32 MissionClass::Mission_Retreat()
{
    // Retreat from current position
    return 1;
}

int32 MissionClass::Mission_Return()
{
    // Return to base
    return 1;
}

int32 MissionClass::Mission_Stop()
{
    // Stop all movement
    return 1;
}

int32 MissionClass::Mission_Unload()
{
    // Unload passengers
    return 1;
}

int32 MissionClass::Mission_Enter()
{
    // Enter a transport or building
    return 1;
}

int32 MissionClass::Mission_Construction()
{
    // Building construction in progress
    return 1;
}

int32 MissionClass::Mission_Selling()
{
    // Building being sold
    return 1;
}

int32 MissionClass::Mission_Repair()
{
    // Repairing at a repair bay
    return 1;
}

int32 MissionClass::Mission_Missile()
{
    // Missile tracking its target
    return 1;
}

int32 MissionClass::Mission_Open()
{
    // Gate opening
    return 1;
}

int32 MissionClass::Mission_Rescue()
{
    // Rescue mission
    return 1;
}

int32 MissionClass::Mission_Patrol()
{
    // Patrol between waypoints
    return 1;
}

int32 MissionClass::Mission_ParaDropApproach()
{
    // Paratrooper plane approaching drop zone
    return 1;
}

int32 MissionClass::Mission_ParaDropOverfly()
{
    // Paratrooper plane flying over drop zone
    return 1;
}

int32 MissionClass::Mission_Wait()
{
    // Wait for a specified duration
    return 1;
}

int32 MissionClass::Mission_SpyPlaneApproach()
{
    // Spy plane approaching target area
    return 1;
}

int32 MissionClass::Mission_SpyPlaneOverfly()
{
    // Spy plane flying over target area
    return 1;
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