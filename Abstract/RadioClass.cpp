// =============================================================================
// RadioClass.cpp - Radio communication and linking system
//
// Manages radio links between techno objects. Used for docking, unloading,
// movement coordination, parking, and bug-out commands between units and
// structures (e.g. harvesters docking at refineries, aircraft at airfields).
// =============================================================================

#include <Abstract/RadioClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/MissionClass.h>
#include <Core/Memory.h>
#include <Game/Game.h>

// =============================================================================
// Constructor
// =============================================================================
RadioClass::RadioClass() noexcept
    : MissionClass()
    , RadioLinks()
{
    LastCommands[0] = RadioCommand::None;
    LastCommands[1] = RadioCommand::None;
    LastCommands[2] = RadioCommand::None;
}

// =============================================================================
// Destructor - Unlink all radio connections cleanly
// =============================================================================
RadioClass::~RadioClass()
{
    // Unlink all radio links in reverse order to avoid index shifting.
    while (RadioLinks.Count > 0) {
        TechnoClass* pLink = RadioLinks[0];
        Unlink(pLink);
    }
}

// =============================================================================
// IPersistStream - Load radio state from a stream
// =============================================================================
HRESULT __stdcall RadioClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;

    // Call parent class Load.
    HRESULT hr = MissionClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    // Read LastCommands
    hr = pStm->Read(LastCommands, sizeof(LastCommands), &read);
    if (hr < 0 || read != sizeof(LastCommands)) return E_FAIL;

    // Read radio link count
    int32 linkCount = 0;
    hr = pStm->Read(&linkCount, sizeof(linkCount), &read);
    if (hr < 0 || read != sizeof(linkCount)) return E_FAIL;
    if (linkCount < 0) linkCount = 0;

    // Read and resolve radio links
    RadioLinks.Clear();
    for (int32 i = 0; i < linkCount; ++i) {
        int32 linkIndex = -1;
        hr = pStm->Read(&linkIndex, sizeof(linkIndex), &read);
        if (hr < 0 || read != sizeof(linkIndex)) return E_FAIL;
        TechnoClass* pLink = nullptr;
        if (linkIndex >= 0) {
            pLink = static_cast<TechnoClass*>(AbstractClass::Get_Instance(linkIndex));
        }
        RadioLinks.Add(pLink);
    }

    return S_OK;
}

// =============================================================================
// IPersistStream - Save radio state to a stream
// =============================================================================
HRESULT __stdcall RadioClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;

    // Call parent class Save.
    HRESULT hr = MissionClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    // Write LastCommands
    hr = pStm->Write(LastCommands, sizeof(LastCommands), &written);
    if (hr < 0 || written != sizeof(LastCommands)) return E_FAIL;

    // Write radio link count
    int32 linkCount = RadioLinks.Count;
    hr = pStm->Write(&linkCount, sizeof(linkCount), &written);
    if (hr < 0 || written != sizeof(linkCount)) return E_FAIL;

    // Write each radio link as an object index
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        int32 linkIndex = -1;
        TechnoClass* pLink = RadioLinks[i];
        if (pLink) {
            linkIndex = AbstractClass::Find_Index(pLink);
        }
        hr = pStm->Write(&linkIndex, sizeof(linkIndex), &written);
        if (hr < 0 || written != sizeof(linkIndex)) return E_FAIL;
    }

    return S_OK;
}

// =============================================================================
// SendToFirstLink - Send a command to the primary (first) linked object
// =============================================================================
RadioCommand RadioClass::SendToFirstLink(RadioCommand command)
{
    if (RadioLinks.Count <= 0) {
        return RadioCommand::None;
    }

    TechnoClass* pFirstLink = RadioLinks[0];
    if (!pFirstLink) {
        return RadioCommand::None;
    }

    // Record the last command sent through this channel.
    LastCommands[0] = command;

    // Process the command based on its type.
    switch (command) {
        case RadioCommand::RequestDock:
            // Request the linked object to prepare for docking.
            // The linked object (e.g. a refinery) acknowledges by
            // sending back a Dock command.
            break;

        case RadioCommand::Dock:
            // Confirm docking. The linked object is now docked.
            break;

        case RadioCommand::RequestUnload:
            // Request the linked object to unload its contents.
            break;

        case RadioCommand::Unload:
            // The linked object has unloaded.
            break;

        case RadioCommand::RequestMove:
            // Request the linked object to move out of the way.
            break;

        case RadioCommand::Move:
            // The linked object has moved as requested.
            break;

        case RadioCommand::RequestLink:
            // Request to establish a radio link with the first linked object.
            break;

        case RadioCommand::Link:
            // The radio link has been established.
            break;

        case RadioCommand::RequestBugOut:
            // Request the linked object to leave immediately (emergency).
            break;

        case RadioCommand::BugOut:
            // The linked object is bugging out.
            break;

        case RadioCommand::RequestPark:
            // Request the linked object to park at its designated location.
            break;

        case RadioCommand::Park:
            // The linked object has parked.
            break;

        default:
            break;
    }

    return command;
}

// =============================================================================
// SendCommand - Send a command to a specific linked recipient
// =============================================================================
RadioCommand RadioClass::SendCommand(RadioCommand command, TechnoClass* pRecipient)
{
    if (!pRecipient) {
        return RadioCommand::None;
    }

    // The recipient must be linked to us.
    if (!IsLinked(pRecipient)) {
        return RadioCommand::None;
    }

    // Record the last command sent through this channel.
    LastCommands[1] = command;

    // Handle bidirectional link commands that affect the radio state.
    if (command == RadioCommand::RequestLink) {
        // The recipient should acknowledge with a Link command.
        // If the recipient already has us linked, confirm immediately.
        if (pRecipient->IsActive()) {
            LastCommands[1] = RadioCommand::Link;
        }
    }

    if (command == RadioCommand::RequestBugOut) {
        // Emergency departure: the recipient should break its link
        // and move away immediately.
        Unlink(pRecipient);
    }

    if (command == RadioCommand::Unload) {
        // The recipient has completed unloading; we can release the link
        // if no further commands are pending.
    }

    return command;
}

// =============================================================================
// SendCommandWithData - Send a command with an associated data object
// =============================================================================
RadioCommand RadioClass::SendCommandWithData(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient)
{
    if (!pRecipient) {
        return RadioCommand::None;
    }

    // The recipient must be linked to us.
    if (!IsLinked(pRecipient)) {
        return RadioCommand::None;
    }

    // Record the last command sent through this channel.
    LastCommands[2] = command;

    // The pInOut parameter carries additional context for the command.
    // For example, a RequestMove command might carry the destination
    // coordinates as an AbstractClass pointer.
    switch (command) {
        case RadioCommand::RequestMove:
            // The pInOut object specifies where the recipient should move.
            if (pInOut) {
                // The recipient reads the destination from pInOut.
            }
            break;

        case RadioCommand::RequestDock:
            // The pInOut object specifies the docking bay or position.
            if (pInOut) {
                // The recipient navigates to the specified dock.
            }
            break;

        case RadioCommand::RequestUnload:
            // The pInOut object specifies what to unload or where.
            if (pInOut) {
                // The recipient unloads at the specified location.
            }
            break;

        case RadioCommand::RequestPark:
            // The pInOut object specifies the parking slot.
            if (pInOut) {
                // The recipient parks at the specified slot.
            }
            break;

        default:
            break;
    }

    return command;
}

// =============================================================================
// SendToEachLink - Broadcast a command to all linked objects
// =============================================================================
void RadioClass::SendToEachLink(RadioCommand command)
{
    // Iterate through all links and send the command to each.
    // We use a snapshot of the count since links may be removed
    // during iteration (e.g. BugOut commands break links).
    int32 initialCount = RadioLinks.Count;
    for (int32 i = 0; i < initialCount && i < RadioLinks.Count; ++i) {
        TechnoClass* pLink = RadioLinks[i];
        if (pLink) {
            SendCommand(command, pLink);
        }
    }
}

// =============================================================================
// GetNthLink - Return a reference to the Nth linked object
// =============================================================================
TechnoClass* const& RadioClass::GetNthLink(int32 idx) const
{
    if (idx < 0 || idx >= RadioLinks.Count) {
        static TechnoClass* nullLink = nullptr;
        return nullLink;
    }
    return RadioLinks[idx];
}

// =============================================================================
// ContainsLink - Check if a specific object is in our link list
// =============================================================================
bool RadioClass::ContainsLink(TechnoClass const* pLink) const
{
    if (!pLink) return false;
    return FindLinkIndex(pLink) >= 0;
}

// =============================================================================
// FindLinkIndex - Return the index of a linked object, or -1 if not found
// =============================================================================
int32 RadioClass::FindLinkIndex(TechnoClass const* pLink) const
{
    if (!pLink) return -1;
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] == pLink) {
            return i;
        }
    }
    return -1;
}

// =============================================================================
// HasFreeLink - Check if there is a free link slot available
// =============================================================================
bool RadioClass::HasFreeLink() const
{
    return RadioLinks.Count < MaxRadioLinks;
}

// =============================================================================
// HasFreeLink - Check if there is a free link slot, ignoring one specific link
// =============================================================================
bool RadioClass::HasFreeLink(TechnoClass const* pIgnore) const
{
    if (RadioLinks.Count < MaxRadioLinks) return true;

    // If we're at capacity, but one of the links is pIgnore, we have room
    // because that link can be replaced.
    if (pIgnore && ContainsLink(pIgnore)) return true;

    return false;
}

// =============================================================================
// HasAnyLink - Check if there are any active radio links
// =============================================================================
bool RadioClass::HasAnyLink() const
{
    return RadioLinks.Count > 0;
}

// =============================================================================
// SetLinkCount - Resize the link vector to the specified count
// =============================================================================
void RadioClass::SetLinkCount(int32 count)
{
    if (count < 0) count = 0;
    if (count > MaxRadioLinks) count = MaxRadioLinks;

    // Grow the vector by adding null links.
    while (RadioLinks.Count < count) {
        RadioLinks.Add(static_cast<TechnoClass*>(nullptr));
    }

    // Shrink the vector by removing from the end.
    while (RadioLinks.Count > count) {
        RadioLinks.Remove(RadioLinks.Count - 1);
    }
}

// =============================================================================
// LinkTo - Establish a radio link with another techno object
// =============================================================================
bool RadioClass::LinkTo(TechnoClass* pLink)
{
    if (!pLink) return false;

    // Already linked: success (idempotent operation).
    if (ContainsLink(pLink)) return true;

    // No free slots available.
    if (!HasFreeLink()) return false;

    // Add the link to our list.
    return RadioLinks.Add(pLink);
}

// =============================================================================
// Unlink - Remove a radio link with another techno object
// =============================================================================
bool RadioClass::Unlink(TechnoClass* pLink)
{
    if (!pLink) return false;

    int32 idx = FindLinkIndex(pLink);
    if (idx < 0) return false;

    // Clear the last commands associated with this link.
    if (RadioLinks.Count == 1) {
        // This was the last link; clear all command history.
        LastCommands[0] = RadioCommand::None;
        LastCommands[1] = RadioCommand::None;
        LastCommands[2] = RadioCommand::None;
    }

    return RadioLinks.Remove(idx);
}

// =============================================================================
// IsLinked - Check if a specific object is linked to us
// =============================================================================
bool RadioClass::IsLinked(TechnoClass* pLink) const
{
    return ContainsLink(pLink);
}

// =============================================================================
// The radio system is used throughout the game for:
//
// - Harvester -> Refinery docking (RequestDock / Dock / Unload)
// - Aircraft -> Airfield/Helipad parking (RequestPark / Park)
// - Transport -> Infantry loading/unloading (RequestUnload / Unload)
// - Combat coordination between grouped units (RequestMove / Move)
// - Emergency evacuation (RequestBugOut / BugOut)
// - Base defense structure <-> power plant linking
//
// Each RadioClass instance can maintain up to MaxRadioLinks (10) concurrent
// radio connections. The link list is managed as a VectorClass of TechnoClass
// pointers. Commands are sent through three channels tracked by LastCommands:
//   [0] - Commands sent to the first link (primary dock/partner)
//   [1] - Commands sent to a specific recipient
//   [2] - Commands sent with additional data payload
// =============================================================================
