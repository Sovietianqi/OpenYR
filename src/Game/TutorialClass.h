#pragma once

// ============================================================================
// TutorialClass - In-game tutorial / help system
//
// Manages context-sensitive tutorial messages that are shown to the player
// during single-player missions.  Tutorial entries are loaded from an INI
// file; each entry binds an event trigger (e.g. "first power plant built")
// to a text string and an optional voice clip.  The class tracks which
// tutorials have already been shown so that repeating the same action does
// not re-trigger the same message.
// ============================================================================

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/Rectangle.h"
#include "Rendering/Surface.h"

class CCINIClass;
class VocClass;

// ----------------------------------------------------------------------------
// TutorialEventType - the kinds of in-game events that can trigger a tutorial
// message.  These mirror the common first-time actions a player performs.
// ----------------------------------------------------------------------------
enum class TutorialEventType : int32
{
    None              = 0,
    GameStart         = 1,    // Shown when the mission begins
    FirstPowerPlant   = 2,    // When the first power plant is built
    FirstRefinery     = 3,    // When the first refinery is built
    FirstBarracks     = 4,    // When the first barracks is built
    FirstWarFactory   = 5,    // When the first war factory is built
    FirstUnit         = 6,    // When the first unit is produced
    FirstInfantry     = 7,    // When the first infantry is produced
    FirstHarvester    = 8,    // When the first harvester deploys
    FirstCombat       = 9,    // When the player first attacks an enemy
    FirstDamage       = 10,   // When the player's unit is first damaged
    LowPower          = 11,   // When power drops below requirement
    InsufficientFunds = 12,   // When the player tries to build but lacks funds
    UnitSelected      = 13,   // When a unit is first selected
    MoveOrder         = 14,   // When a move order is first given
    AttackOrder       = 15,   // When an attack order is first given
    WaypointSet       = 16,   // When a waypoint is first set
    SuperWeapon       = 17,   // When a super weapon is first used
    RadarActivated    = 18,   // When the radar is first activated
    ConstructionMenu  = 19,   // When the construction menu is first opened
    PauseGame         = 20,   // When the game is first paused
    TimeElapsed       = 21,   // Shown after a certain time has elapsed
    ObjectiveComplete = 22,   // When a primary objective is completed
    ObjectiveFailed   = 23,   // When a primary objective fails
    Count             = 24
};

// ----------------------------------------------------------------------------
// TutorialEntry - a single tutorial message definition loaded from INI.
// ----------------------------------------------------------------------------
struct TutorialEntry
{
    int32              ID;           // Numeric identifier (INI key index)
    TutorialEventType  EventType;    // What triggers this tutorial
    char               Text[512];    // Display text (may contain line breaks)
    char               VoiceFile[64];// Optional voice/speech filename
    int32              DisplayFrames;// How many frames to display the message
    bool               ShowOnce;     // Only show this tutorial once per mission
    bool               PlayVoice;    // Whether to play the voice clip
    bool               HasPlayed;    // Runtime: has this been shown already

    TutorialEntry()
        : ID(0)
        , EventType(TutorialEventType::None)
        , DisplayFrames(180)  // ~3 seconds at 60 FPS
        , ShowOnce(true)
        , PlayVoice(false)
        , HasPlayed(false)
    {
        Text[0] = '\0';
        VoiceFile[0] = '\0';
    }
};

// ============================================================================
// TutorialClass
// ============================================================================
class TutorialClass
{
public:
    // ----------------------------------------------------------------------
    // Construction / destruction
    // ----------------------------------------------------------------------
    TutorialClass() noexcept;
    ~TutorialClass();

    // ----------------------------------------------------------------------
    // Initialization
    // ----------------------------------------------------------------------

    // Load all tutorial entries from the given INI file's [Tutorial] section.
    // Each key is a numeric ID; the value is the text.  Additional properties
    // are read from sub-sections [TutorialEvents], [TutorialVoice], etc.
    bool Load_From_INI(CCINIClass* pINI);

    // Clear all loaded tutorial entries and reset state.
    void Clear();

    // Reset the "has played" flags so tutorials can be re-triggered.
    void Reset_Played();

    // ----------------------------------------------------------------------
    // Tutorial triggering
    // ----------------------------------------------------------------------

    // Trigger the tutorial message associated with the given event type.
    // Returns true if a tutorial was found and displayed; false if no
    // tutorial is registered for this event or it has already been shown.
    bool Trigger(TutorialEventType eventType);

    // Trigger the tutorial by numeric ID.
    bool Trigger_By_ID(int32 id);

    // Update the active tutorial display.  Called every frame; decrements
    // the remaining display timer and removes the message when it expires.
    void Update();

    // ----------------------------------------------------------------------
    // Display
    // ----------------------------------------------------------------------

    // Draw the currently active tutorial message onto the given surface.
    // The message is rendered in a banner at the top of the screen.
    void Draw(Surface* pSurface) const;

    // Dismiss the currently active tutorial immediately.
    void Dismiss();

    // Check whether a tutorial message is currently being displayed.
    bool Is_Active() const { return ActiveEntry != nullptr; }

    // Get the text of the currently active tutorial.
    const char* Get_Active_Text() const;

    // ----------------------------------------------------------------------
    // Tutorial progression tracking
    // ----------------------------------------------------------------------

    // Check whether a tutorial for the given event has already been played.
    bool Has_Played(TutorialEventType eventType) const;

    // Mark a tutorial event as played without displaying it.
    void Mark_Played(TutorialEventType eventType);

    // Get the total number of tutorial entries loaded.
    int32 Get_Entry_Count() const { return Entries.Count; }

    // Get a tutorial entry by index.
    const TutorialEntry* Get_Entry(int32 index) const;

    // Find a tutorial entry by event type (returns nullptr if none).
    const TutorialEntry* Find_By_Event(TutorialEventType eventType) const;

    // ----------------------------------------------------------------------
    // Voice playback
    // ----------------------------------------------------------------------

    // Play the voice clip associated with the given entry.
    void Play_Voice(const TutorialEntry* pEntry);

    // Stop any currently playing tutorial voice clip.
    void Stop_Voice();

    // Set whether voice playback is enabled.
    void Set_Voice_Enabled(bool enabled) { VoiceEnabled = enabled; }
    bool Is_Voice_Enabled() const { return VoiceEnabled; }

    // ----------------------------------------------------------------------
    // Save / Load state
    // ----------------------------------------------------------------------

    // Save the tutorial "played" state to an INI section.
    bool Save_State(CCINIClass* pINI) const;

    // Load the tutorial "played" state from an INI section.
    bool Load_State(CCINIClass* pINI);

private:
    // ----------------------------------------------------------------------
    // Internal helpers
    // ----------------------------------------------------------------------

    // Find an entry by event type (non-const).
    TutorialEntry* Find_Entry_By_Event(TutorialEventType eventType);

    // Activate a tutorial entry for display.
    void Activate_Entry(TutorialEntry* pEntry);

    // ----------------------------------------------------------------------
    // Data
    // ----------------------------------------------------------------------
    DynamicVectorClass<TutorialEntry*> Entries;  // Owned entries
    TutorialEntry*                     ActiveEntry;   // Currently displayed
    int32                              DisplayTimer;  // Frames remaining
    VocClass*                          ActiveVoice;   // Currently playing voice
    bool                               VoiceEnabled;  // Global voice toggle
    int32                              BannerY;       // Y position of the banner

    // Disable copy
    TutorialClass(const TutorialClass&) = delete;
    TutorialClass& operator=(const TutorialClass&) = delete;
};
