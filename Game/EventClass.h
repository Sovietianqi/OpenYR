#pragma once

// ============================================================================
// EventClass - Game event system for single-player missions
//
// Manages scripted mission events that fire based on in-game conditions
// such as elapsed time, unit location, unit creation/destruction, and
// building state.  Each event has a type, trigger parameters, an optional
// callback, and a state (pending / fired / disabled).  The EventClass
// evaluates all pending events each frame and dispatches those whose
// conditions are satisfied.
//
// This is distinct from the NetworkEventQueueClass (network sync events)
// and from the TriggerClass / TEventClass system (map-attached triggers).
// EventClass provides a lightweight, scriptable event queue for mission
// logic that does not require map placement.
// ============================================================================

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;
class IStream;

// ----------------------------------------------------------------------------
// GameEventType - the kinds of conditions that can trigger a game event.
// ----------------------------------------------------------------------------
enum class GameEventType : int32
{
    None              = 0,
    TimeElapsed       = 1,   // Fire after N frames have elapsed
    UnitEnteredCell   = 2,   // Fire when any unit enters a specific cell
    UnitCreated       = 3,   // Fire when a unit of a specific type is created
    UnitDestroyed     = 4,   // Fire when a unit of a specific type is destroyed
    BuildingCreated   = 5,   // Fire when a building of a specific type is built
    BuildingDestroyed = 6,   // Fire when a building of a specific type is destroyed
    CreditsReached    = 7,   // Fire when player credits reach a threshold
    PowerLow          = 8,   // Fire when power output drops below requirement
    HouseDefeated     = 9,   // Fire when a house is eliminated
    ObjectiveComplete = 10,  // Fire when a mission objective is completed
    Custom            = 11,  // Fire via explicit Trigger() call
    Count             = 12
};

// ----------------------------------------------------------------------------
// EventState - the lifecycle state of a game event.
// ----------------------------------------------------------------------------
enum class EventState : int32
{
    Inactive = 0,   // Disarmed; will not be evaluated
    Pending  = 1,   // Armed and waiting for condition
    Fired    = 2,   // Condition met and callback dispatched
    Disabled = 3    // Permanently disabled; will not fire
};

// ----------------------------------------------------------------------------
// EventCallback - function pointer invoked when an event fires.
//   pUserData - caller-supplied context pointer
//   eventID   - the numeric ID of the event that fired
// ----------------------------------------------------------------------------
typedef void (*EventCallback)(void* pUserData, int32 eventID);

// ----------------------------------------------------------------------------
// GameEvent - a single scripted event in the event queue.
// ----------------------------------------------------------------------------
struct GameEvent
{
    int32          ID;             // Unique identifier for this event
    GameEventType  Type;           // What condition triggers this event
    EventState     State;          // Current lifecycle state
    int32          TriggerFrame;   // For TimeElapsed: the frame to fire on
    int32          TriggerCellX;   // For location-based events
    int32          TriggerCellY;
    int32          TriggerObjectType;// For unit/building events: object type index
    int32          TriggerHouseIndex;  // House index for house-specific events
    int32          TriggerValue;   // For CreditsReached: the credit threshold
    bool           Repeatable;     // Can this event fire more than once?
    int32          FireCount;      // How many times has this event fired?
    int32          LastFireFrame;  // Frame on which it last fired
    EventCallback  Callback;       // Function to call when the event fires
    void*          UserData;       // Context pointer passed to the callback
    char           Name[32];       // Human-readable name for debugging

    GameEvent()
        : ID(0)
        , Type(GameEventType::None)
        , State(EventState::Inactive)
        , TriggerFrame(0)
        , TriggerCellX(0)
        , TriggerCellY(0)
        , TriggerObjectType(-1)
        , TriggerHouseIndex(-1)
        , TriggerValue(0)
        , Repeatable(false)
        , FireCount(0)
        , LastFireFrame(-1)
        , Callback(nullptr)
        , UserData(nullptr)
    {
        Name[0] = '\0';
    }
};

// ============================================================================
// EventClass - the game event manager
// ============================================================================
class EventClass
{
public:
    // ----------------------------------------------------------------------
    // Construction / destruction
    // ----------------------------------------------------------------------
    EventClass() noexcept;
    ~EventClass();

    // ----------------------------------------------------------------------
    // Event queue management
    // ----------------------------------------------------------------------

    // Add a new event to the queue.  Returns the assigned event ID, or -1
    // on failure.  The event starts in the Pending state.
    int32 Add_Event(const GameEvent& event);

    // Add a simple time-based event that fires after 'delayFrames' frames.
    int32 Add_Timer_Event(int32 delayFrames, EventCallback callback = nullptr,
                          void* pUserData = nullptr);

    // Add a location-based event that fires when a unit enters the given cell.
    int32 Add_Location_Event(int32 cellX, int32 cellY,
                             EventCallback callback = nullptr,
                             void* pUserData = nullptr);

    // Add a unit-creation event that fires when a unit of the given type
    // is created by the specified house (-1 for any house).
    int32 Add_Unit_Created_Event(int32 objectType, int32 houseIndex,
                                 EventCallback callback = nullptr,
                                 void* pUserData = nullptr);

    // Add a building-destroyed event.
    int32 Add_Building_Destroyed_Event(int32 objectType, int32 houseIndex,
                                       EventCallback callback = nullptr,
                                       void* pUserData = nullptr);

    // Remove an event by ID.
    bool Remove_Event(int32 eventID);

    // Remove all events.
    void Clear();

    // Get the number of events in the queue.
    int32 Get_Event_Count() const { return Events.Count; }

    // Get an event by index.
    const GameEvent* Get_Event(int32 index) const;
    GameEvent* Get_Event_Mutable(int32 index);

    // Find an event by ID.
    const GameEvent* Find_Event(int32 eventID) const;
    GameEvent* Find_Event_Mutable(int32 eventID);

    // ----------------------------------------------------------------------
    // Event state control
    // ----------------------------------------------------------------------

    // Enable (arm) an event so it will be evaluated.
    bool Enable_Event(int32 eventID);

    // Disable an event so it will not fire.
    bool Disable_Event(int32 eventID);

    // Reset a fired event back to pending (for repeatable events).
    bool Reset_Event(int32 eventID);

    // ----------------------------------------------------------------------
    // Event dispatch
    // ----------------------------------------------------------------------

    // Evaluate all pending events and dispatch those whose conditions are
    // met.  This should be called once per frame from the game loop.
    void Update();

    // Manually trigger a Custom-type event by ID.
    bool Trigger_Event(int32 eventID);

    // Notify the event system that a unit entered a cell.  This evaluates
    // all UnitEnteredCell events against the given coordinates.
    void Notify_Unit_Entered_Cell(int32 cellX, int32 cellY, int32 unitType,
                                  int32 houseIndex);

    // Notify the event system that a unit was created.
    void Notify_Unit_Created(int32 unitType, int32 houseIndex);

    // Notify the event system that a unit was destroyed.
    void Notify_Unit_Destroyed(int32 unitType, int32 houseIndex);

    // Notify the event system that a building was created.
    void Notify_Building_Created(int32 buildingType, int32 houseIndex);

    // Notify the event system that a building was destroyed.
    void Notify_Building_Destroyed(int32 buildingType, int32 houseIndex);

    // Notify the event system that credits changed.
    void Notify_Credits_Changed(int32 houseIndex, int32 currentCredits);

    // Notify the event system that a house was defeated.
    void Notify_House_Defeated(int32 houseIndex);

    // Notify the event system that an objective was completed.
    void Notify_Objective_Complete(int32 objectiveID);

    // ----------------------------------------------------------------------
    // Event callbacks
    // ----------------------------------------------------------------------

    // Set the callback for an existing event.
    bool Set_Callback(int32 eventID, EventCallback callback, void* pUserData);

    // ----------------------------------------------------------------------
    // Save / Load event state
    // ----------------------------------------------------------------------

    // Save the event state (which events have fired, etc.) to a stream.
    bool Save(IStream* pStm) const;

    // Load the event state from a stream.
    bool Load(IStream* pStm);

    // Save event state to an INI section [GameEvents].
    bool Save_To_INI(CCINIClass* pINI) const;

    // Load event state from an INI section [GameEvents].
    bool Load_From_INI(CCINIClass* pINI);

private:
    // ----------------------------------------------------------------------
    // Internal: dispatch a single event (invoke callback, update state).
    // ----------------------------------------------------------------------
    void Dispatch_Event(GameEvent* pEvent);

    // Check whether a specific event's condition is satisfied.
    bool Check_Condition(const GameEvent* pEvent) const;

    // Generate the next unique event ID.
    int32 Next_Event_ID();

    DynamicVectorClass<GameEvent*> Events;
    int32                          NextID;

    // Disable copy
    EventClass(const EventClass&) = delete;
    EventClass& operator=(const EventClass&) = delete;
};
