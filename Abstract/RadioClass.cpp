// =============================================================================
// RadioClass.cpp - Radio communication and linking system
//
// Manages radio links between techno objects. Used for docking, unloading,
// movement coordination, parking, and bug-out commands between units and
// structures (e.g. harvesters docking at refineries, aircraft at airfields).
//
// asm anchors (binary named the receive side RadioClass_ReceivedRadioCommand):
//   RadioClass_CTOR                     (asm:1244166)
//   RadioClass_ReceivedRadioCommand     (asm:1244232)  -> Receive_Radio
//   Radio_LinkWith                      (asm:1244464)  -> SendCommandWithData
//   RadioClass::Limbo                   (asm:1244610)
//   Radio_SendSmth                      (asm:1244641)  -> SendCommand
//   RadioClass_PointerGotInvalid        (asm:1244666)
//   RadioClass_GetCRC                   (asm:1244723)
//   RadioClass_Load / _Save             (asm:1244790 / 1244870)
//   Radio_SendToFirst                   (asm:1244983)  -> SendToFirstLink
//   Radio_SendToEach                    (asm:1245016)  -> SendToEachLink
//   RadioClass_GetNthEntry/_2           (asm:1245080 / 1245092)
//   RadioClass_ContainsEntry            (asm:1245104)  -> ContainsLink
//   RadioClass_GetIdxOfEntry2           (asm:1245136)  -> FindLinkIndex
//   RadioClass_IsArrayEmpty             (asm:1245160)
//   RadioClass_IsObjectInArray          (asm:1245232)
//   RadioClass_ArrayHasAtLeastOneItem   (asm:1245285)
// =============================================================================

#include <Abstract/RadioClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/MissionClass.h>
#include <Houses/HouseClass.h>
#include <Core/Memory.h>
#include <Game/Game.h>
#include <IO/CRC.h>

// =============================================================================
// Compatibility translation between the legacy RadioCommand docking enum and
// the full eRadioCommands table the binary actually puts on the wire.
//
// Request*/response pairs: the request code asks, the plain code answers.
// =============================================================================
static RadioMessageType RadioCommandToMsg(RadioCommand command)
{
    switch (command) {
        case RadioCommand::RequestDock:   return RadioMessageType::QueryCanEnterAsPassenger; // 15
        case RadioCommand::Dock:          return RadioMessageType::Roger;                   // 1
        case RadioCommand::RequestUnload: return RadioMessageType::Msg18;                   // 18
        case RadioCommand::Unload:        return RadioMessageType::Roger;                   // 1
        case RadioCommand::RequestMove:   return RadioMessageType::NeedToMove;              // 19
        case RadioCommand::Move:          return RadioMessageType::Roger;                   // 1
        case RadioCommand::RequestLink:   return RadioMessageType::Hello;                   // 2
        case RadioCommand::Link:          return RadioMessageType::Roger;                   // 1
        case RadioCommand::RequestBugOut: return RadioMessageType::Exit;                    // 3
        case RadioCommand::BugOut:        return RadioMessageType::Exit;                    // 3
        case RadioCommand::RequestPark:   return RadioMessageType::QueryCanEnterAsPassenger; // 15
        case RadioCommand::Park:          return RadioMessageType::Roger;                   // 1
        case RadioCommand::None:
        default:                          return RadioMessageType::ReplyCannotEnterUnload;  // 0
    }
}

// =============================================================================
// UnknownRadioControl
//
// asm hands every send slot a pointer to the global `UnknownRadioControl` blob
// (see Radio_SendSmth / Radio_LinkWith / Radio_SendToFirst / Radio_SendToEach:
// `push offset UnknownRadioControl`).  It is a payload scratch the receivers may
// inspect; none of the radio handlers in the binary dereference it as an object,
// so a stable dummy is enough.  Keeping one address also keeps CRC/save state
// deterministic.
// =============================================================================
static int32 g_UnknownRadioControl = 0;

static AbstractClass* UnknownRadioControl()
{
    return reinterpret_cast<AbstractClass*>(&g_UnknownRadioControl);
}

// =============================================================================
// Constructor
// =============================================================================
// RadioClass_CTOR (asm:1244166-1244218):
//   MissionClass_CTOR
//   RadioLinks.count        = 0                 ; +0xE8
//   RadioLinks.IsInitialized= 1                 ; +0xEC (byte)
//   [esi+size RadioClass+..iRTTIInfo] = 1       ; another init byte
//   [esi+...qX+0B5h]        = 0                 ; the "selected/active" byte
//   [esi+0E0h] = VectorClass<TechnoClass*>::vftable
//   data = new(4) ; data[0] = 0
//   [esi+0EDh] = 1
//   LastCommands[0..2] = 0                      ; +0xD4/+0xD8/+0xDC
//
//   The count starts at 0 while one zeroed pointer is allocated, i.e. a fresh
//   object has no *legal* link slot: the table is grown by the docking code
//   (VectorClass slot +8, see RadioClass_Load) before any Hello can land.
//   OpenYR keeps one usable slot instead, because the docking code that grows
//   the table is not ported yet; without it no handshake could ever succeed.
RadioClass::RadioClass() noexcept
    : MissionClass()
    , LastCommands()
    , RadioLinks()
{
    LastCommands[0] = RadioCommand::None;
    LastCommands[1] = RadioCommand::None;
    LastCommands[2] = RadioCommand::None;

    // asm: one pre-cleared slot; see the note above about the count.
    RadioLinks.Add(static_cast<TechnoClass*>(nullptr));
}

// =============================================================================
// Destructor - Unlink all radio connections cleanly
// =============================================================================
RadioClass::~RadioClass()
{
    // Drop every link without calling back into the partner: the asm DTOR/
    // SDDTOR keeps the vector teardown local (no reciprocal commands).
    RadioLinks.Clear();
}

// =============================================================================
// IPersistStream - Load radio state from a stream
//
// asm RadioClass_Load (asm:1244790-1244872, retn 8):
//   ObjectClass_Load(this, pStm)                 ; NOTE: skips MissionClass
//   RadioLinks.count = 0                         ; +4
//   RadioLinks.Capacity = 0                      ; +8
//   RadioLinks.IsInitialized = 1                 ; +0xC (byte)
//   RadioLinks.??? = 0                           ; +0xD (byte)
//   RadioLinks.vt = VectorClass<TechnoClass*>
//   Read(&count, 4)
//   RadioLinks.vtable[8](count)                  ; grow to the saved count
//   loop: Read(&entry, 4) ; data[i] = entry
//   loop: SwizzleManagerClass_Swizzle(&data[i])  ; resolve the saved pointer
//
//   LastCommands is NOT part of the stream: the three rolling codes are rebuilt
//   from live traffic.  OpenYR stores object *ids* instead of raw pointers and
//   resolves them with AbstractClass::Get_Instance (the swizzle step).
// =============================================================================
HRESULT __stdcall RadioClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;

    HRESULT hr = MissionClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    // asm +14: the vector is re-initialised before the count is read.
    RadioLinks.Clear();

    int32 linkCount = 0;
    hr = pStm->Read(&linkCount, sizeof(linkCount), &read);
    if (hr < 0 || read != sizeof(linkCount)) return E_FAIL;

    // The original grows the vector to whatever the stream claims; clamp to the
    // engine-wide maximum so a corrupt save cannot allocate unbounded memory.
    if (linkCount < 0) linkCount = 0;
    if (linkCount > MaxRadioLinks) linkCount = MaxRadioLinks;

    for (int32 i = 0; i < linkCount; ++i) {
        int32 linkIndex = -1;
        hr = pStm->Read(&linkIndex, sizeof(linkIndex), &read);
        if (hr < 0 || read != sizeof(linkIndex)) return E_FAIL;

        TechnoClass* pLink = nullptr;
        if (linkIndex >= 0)
            pLink = static_cast<TechnoClass*>(AbstractClass::Get_Instance(linkIndex));

        RadioLinks.Add(pLink);
    }

    return S_OK;
}

// =============================================================================
// IPersistStream - Save radio state to a stream
//
// asm RadioClass_Save (asm:1244872-1244960, retn 0Ch):
//   AbstractClass_Save(this, pStm, fClearDirty)
//   Write(&RadioLinks.count, 4)
//   for (i < count) Write(&data[i], 4)            ; raw pointers, relocated
//
//   Same as Load: LastCommands is not serialised.
// =============================================================================
HRESULT __stdcall RadioClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;

    HRESULT hr = MissionClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    int32 linkCount = RadioLinks.Count;
    hr = pStm->Write(&linkCount, sizeof(linkCount), &written);
    if (hr < 0 || written != sizeof(linkCount)) return E_FAIL;

    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        int32 linkIndex = -1;
        TechnoClass* pLink = RadioLinks[i];
        if (pLink != nullptr)
            linkIndex = AbstractClass::Find_Index(pLink);

        hr = pStm->Write(&linkIndex, sizeof(linkIndex), &written);
        if (hr < 0 || written != sizeof(linkIndex)) return E_FAIL;
    }

    return S_OK;
}

// =============================================================================
// Receive_Radio (asm: RadioClass_ReceivedRadioCommand, asm:1244232-1244460)
//
//   The receive side of the radio protocol and the vtable slot every sender
//   hits.  It first rolls the three-slot last-command history, then dispatches
//   on the wire code:
//
//   +11  mov eax,[esi+lastRadios.rc+4] / cmp cmd,eax / jz skip
//        mov ecx,[...+8] / [...+8] = eax / [rc+0] = ecx / [rc+4] = cmd
//        i.e. rc[0] = old rc[2]; rc[2] = old rc[1]; rc[1] = cmd.
//   +32  cmp ebx,3 (rcExit)
//          scan RadioLinks for the source
//            found: ObjectClass_ReceivedRadioCommand(src,3,a4) then slot = 0,
//                   return 1
//            not found: forward and return the base answer
//   +75  cmp ebx,2 (rcHello)
//          [esi+6Ch] == 0 (no owner house) -> plain forward
//          source->Owner->Is_Ally(this) == 0 -> return 0Ah
//          this->Alive && this->Owner->Is_Ally(source) == 0 -> return 0Ah
//          source already linked -> return 1
//          first null slot <- source -> return 1
//          full -> return 0Ah
//   +F1  default: forward verbatim, return the base answer
//
//   Forwarding always passes (source, cmd, a4) to ObjectClass's handler, whose
//   answer (0 when unhandled) is what the sender sees.
// =============================================================================
int32 RadioClass::Receive_Radio(TechnoClass* pSource, RadioMessageType cmd, int32 a4)
{
    // asm +5..+1F: rolling history.  Only a new command shifts the window.
    if (static_cast<int32>(cmd) != static_cast<int32>(LastCommands[1])) {
        const RadioCommand previousLast = LastCommands[2];
        LastCommands[2] = LastCommands[1];              // rc[2] = old rc[1]
        LastCommands[0] = previousLast;                 // rc[0] = old rc[2]
        LastCommands[1] = static_cast<RadioCommand>(cmd);
    }

    // The asm passes the source as the 32-bit "object" argument of the
    // ObjectClass handler; the port keeps the same slot convention.
    const int32 arg0 = static_cast<int32>(reinterpret_cast<intptr_t>(pSource));
    const int32 wireCmd = static_cast<int32>(cmd);

    switch (cmd) {
    case RadioMessageType::Exit:
    {
        // asm +32..+70: only a *linked* source gets the "cleared slot + Roger"
        // treatment; anyone else is just forwarded.
        if (pSource != nullptr) {
            const int32 idx = FindLinkIndex(pSource);
            if (idx >= 0) {
                ObjectClass::ReceivedRadioCommand(wireCmd, arg0, a4);
                AssignLinkSlot(idx, nullptr);           // asm: [data+edi*4] = 0
                return static_cast<int32>(RadioMessageType::Roger);
            }
        }
        return ObjectClass::ReceivedRadioCommand(wireCmd, arg0, a4);
    }

    case RadioMessageType::Hello:
    {
        HouseClass* myHouse = GetRadioHouse();
        if (myHouse == nullptr)
            break;                                      // asm +75: plain forward

        // asm +8D: the *source's* owner must consider us friendly.
        if (pSource == nullptr || !pSource->Is_Ally(myHouse))
            return static_cast<int32>(RadioMessageType::ReplyNoCantEnterAsPassenger);

        // asm +B5: and we must consider the source friendly, but only while we
        // are alive (asm tests bit0 of the flags byte at +0x14).
        TechnoClass* pSelf = GetRadioSelf();
        if (Alive != 0 && pSelf != nullptr && !pSelf->Is_Ally(pSource))
            return static_cast<int32>(RadioMessageType::ReplyNoCantEnterAsPassenger);

        // asm +DF: already linked -> Roger, no second entry.
        if (FindLinkIndex(pSource) >= 0)
            return static_cast<int32>(RadioMessageType::Roger);

        // asm +11D: first null slot wins; a full table refuses.
        const int32 slot = FindFreeSlot();
        if (slot >= 0) {
            AssignLinkSlot(slot, pSource);
            return static_cast<int32>(RadioMessageType::Roger);
        }
        return static_cast<int32>(RadioMessageType::ReplyNoCantEnterAsPassenger);
    }

    default:
        break;
    }

    // asm loc_65A872: plain forward, base answer returned.
    return ObjectClass::ReceivedRadioCommand(wireCmd, arg0, a4);
}

// asm: the vtable slot itself.  TechnoClass callers push the source object in
// the integer argument, so decode it and run the wire state machine.
int32 RadioClass::ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4)
{
    TechnoClass* pSource = (arg0 != 0)
        ? reinterpret_cast<TechnoClass*>(static_cast<intptr_t>(arg0))
        : nullptr;

    return Receive_Radio(pSource, static_cast<RadioMessageType>(cmd), a4);
}

// =============================================================================
// SendCommandWithData (asm: Radio_LinkWith, asm:1244464-1244705)
//
//   The transmit/link-manager slot (`Radio_CommandToDocked`).  Returns the peer's
//   wire answer, 0 when nothing was sent.
//
//   +C   edi = dockBld(recipient); if (edi == 0) { edi = RadioLinks.data[0];
//        if (edi == 0) return 0; }
//   +2A  if (activity == rcExit):
//          for (i < RadioLinks.count) if (data[i] == recipient) data[i] = 0;
//          return recipient->ReceivedRadioCommand(this, rcExit, unknown)
//   +68  if (activity != rcHello) goto done_radiolinks (send + return answer)
//   +75  ebx = -1; scan for the first null slot (ebx) while ALSO testing
//        data[i] == recipient -> loc_65AA5D: return 1 (already linked, the
//        recipient is *not* re-sent a Hello)
//   +AD  when no blank slot exists:
//          SendCommand(rcExit, data[0])      ; vtable Radio_SendCommand
//          ebx = 0                            ; reuse slot 0
//   +BE  answer = recipient->ReceivedRadioCommand(this, rcHello, unknown)
//        answer == 1 -> data[ebx] = recipient ; return 1
//        else        -> return 0Ah
// =============================================================================
int32 RadioClass::SendCommandWithData(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient)
{
    TechnoClass* pLink = pRecipient;

    // asm +C: fall back to the first link; with none, give up silently.
    if (pLink == nullptr) {
        pLink = GetFirstLink();
        if (pLink == nullptr)
            return 0;
    }

    const RadioMessageType msg = RadioCommandToMsg(command);

    // asm: the peer is always told *who* is talking — `push Is_Techno(this)`
    // feeds the first argument of the recipient's handler.  The payload pointer
    // travels in the third slot.
    const int32 sourceArg = static_cast<int32>(
        reinterpret_cast<intptr_t>(GetRadioSelf()));
    const int32 payloadArg = static_cast<int32>(
        reinterpret_cast<intptr_t>(pInOut));

    if (msg == RadioMessageType::Exit) {
        // asm +3A..+60: clear every matching entry on our side, then push Exit.
        ClearLinkReferences(pLink);
        return pLink->ReceivedRadioCommand(static_cast<int32>(msg), sourceArg, payloadArg);
    }

    if (msg == RadioMessageType::Hello) {
        // asm +A3: single pass that records the first blank slot (ebx) and
        // short-circuits when the recipient is already in the table.
        int32 slot = -1;
        for (int32 i = 0; i < RadioLinks.Count; ++i) {
            if (slot == -1 && RadioLinks[i] == nullptr)
                slot = i;
            if (RadioLinks[i] == pLink)
                return static_cast<int32>(RadioMessageType::Roger);   // loc_65AA5D
        }

        if (slot == -1) {
            // asm +B5..+BA: no room.  Command the first link to bug out and
            // then reuse slot 0 (the asm's `xor ebx,ebx`).
            TechnoClass* pFirst = GetFirstLink();
            if (pFirst != nullptr)
                SendCommand(RadioCommand::BugOut, pFirst);
            slot = 0;
        }

        const int32 answer = pLink->ReceivedRadioCommand(
            static_cast<int32>(msg), sourceArg, payloadArg);
        if (answer == static_cast<int32>(RadioMessageType::Roger)) {
            AssignLinkSlot(slot, pLink);                             // +D9
            return static_cast<int32>(RadioMessageType::Roger);
        }
        return static_cast<int32>(RadioMessageType::ReplyNoCantEnterAsPassenger);
    }

    // asm loc_65A872 / done_radiolinks: fire-and-forward, return the answer.
    return pLink->ReceivedRadioCommand(static_cast<int32>(msg), sourceArg, payloadArg);
}

// =============================================================================
// SendToFirstLink (asm: Radio_SendToFirst, asm:1244983-1244999, retn 4)
//
//   mov eax,[ecx+0E4h]      ; RadioLinks.data
//   mov eax,[eax]           ; first slot
//   test eax,eax / jz loc_65ACD2 -> xor eax,eax
//   edx = [ecx]             ; vtable
//   push eax                ; recipient
//   push offset UnknownRadioControl
//   push activity
//   call [edx+Radio_CommandToDocked]
//
//   Note there is NO LastCommands bookkeeping here: the rolling history is only
//   written by the *receive* side (asm:1244232).  The first slot is used whether
//   or not it is null -- a null first link simply sends to nobody and the callee
//   returns 0.
// =============================================================================
int32 RadioClass::SendToFirstLink(RadioCommand command)
{
    TechnoClass* pFirst = GetFirstLink();
    if (pFirst == nullptr)
        return 0;                                  // asm: loc_65ACD2

    AbstractClass* payload = UnknownRadioControl();
    return SendCommandWithData(command, payload, pFirst);
}

// =============================================================================
// SendCommand (asm: Radio_SendSmth, asm:1244641-1244660, retn 8)
//
//   mov edx,[esp+a2]        ; recipient
//   mov eax,[ecx]           ; vtable
//   push edx / push offset UnknownRadioControl / push activity
//   call [eax+Radio_CommandToDocked]
//   retn 8
//
//   A pure tail-call into the link manager: no LastCommands write, no null check
//   (the manager's own first-link fallback handles a null recipient).
// =============================================================================
int32 RadioClass::SendCommand(RadioCommand command, TechnoClass* pRecipient)
{
    AbstractClass* payload = UnknownRadioControl();
    return SendCommandWithData(command, payload, pRecipient);
}

// =============================================================================
// SendToEachLink (asm: Radio_SendToEach, asm:1245016-1245049, retn 4)
//
//   for (edi = 0; edi < RadioLinks.count; )      ; re-reads the count each step
//       if (data[edi] != 0)
//           data[edi]->CommandToDocked(activity, &UnknownRadioControl, data[edi]);
//       ++edi;
//
//   The count is re-loaded from the object every iteration because a callback
//   can unlink (and therefore shrink or null) entries.
// =============================================================================
void RadioClass::SendToEachLink(RadioCommand command)
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        TechnoClass* pLink = RadioLinks[i];
        if (pLink != nullptr) {
            AbstractClass* payload = UnknownRadioControl();
            SendCommandWithData(command, payload, pLink);
        }
    }
}

// =============================================================================
// GetNthLink (asm: RadioClass_GetNthEntry, asm:1245080)
// =============================================================================
TechnoClass* const& RadioClass::GetNthLink(int32 idx) const
{
    static TechnoClass* nullLink = nullptr;
    if (idx < 0 || idx >= RadioLinks.Count)
        return nullLink;
    return RadioLinks[idx];
}

// =============================================================================
// ContainsLink (asm: RadioClass_ContainsEntry, asm:1245104)
//   `pLink == 0 -> -1 -> false`; otherwise the index probe answers.
// =============================================================================
bool RadioClass::ContainsLink(TechnoClass const* pLink) const
{
    if (!pLink) return false;
    return FindLinkIndex(pLink) != -1;
}

// =============================================================================
// FindLinkIndex (asm: RadioClass_GetIdxOfEntry2, asm:1245136)
// =============================================================================
int32 RadioClass::FindLinkIndex(TechnoClass const* pLink) const
{
    if (!pLink) return -1;
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] == pLink)
            return i;
    }
    return -1;
}

// =============================================================================
// HasFreeLink - is there a free slot (asm: the `IsArrayEmpty` hole scan)
// =============================================================================
bool RadioClass::HasFreeLink() const
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] == nullptr)
            return true;
    }
    return false;
}

bool RadioClass::HasFreeLink(TechnoClass const* pIgnore) const
{
    return HasFreeLink() || (pIgnore != nullptr && ContainsLink(pIgnore));
}

// =============================================================================
// HasAnyLink (asm: RadioClass_ArrayHasAtLeastOneItem, asm:1245285)
// =============================================================================
bool RadioClass::HasAnyLink() const
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] != nullptr)
            return true;
    }
    return false;
}

// =============================================================================
// GetFirstLink (asm: `mov eax,[ecx+0E4h] / mov eax,[eax]` in Radio_SendToFirst
// and Radio_LinkWith +0xC).  Returns slot 0 only -- the original never scans
// for "the first non-null link" in these paths.
// =============================================================================
TechnoClass* RadioClass::GetFirstLink() const
{
    if (RadioLinks.Count <= 0 || RadioLinks.Items == nullptr)
        return nullptr;
    return RadioLinks.Items[0];
}

// =============================================================================
// FindFreeSlot (asm: the `ebx` scan at Radio_LinkWith +0xA3):
//   for (i = 0; i < count; ++i) if (data[i] == 0) { ebx = i; break; }
// Returns -1 when the table has no hole.
// =============================================================================
int32 RadioClass::FindFreeSlot() const
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] == nullptr)
            return i;
    }
    return -1;
}

// =============================================================================
// AssignLinkSlot - bounds-checked `data[idx] = pLink` (asm: `[data+ebx*4] = edi`
// at Radio_LinkWith +0xD9 and `[data+eax*4] = edi` at +0x12B).
// The slot must already exist; the asm never grows the vector here.
// =============================================================================
bool RadioClass::AssignLinkSlot(int32 idx, TechnoClass* pLink)
{
    if (idx < 0 || idx >= RadioLinks.Count)
        return false;

    RadioLinks.Items[idx] = pLink;
    return true;
}

// =============================================================================
// ClearLinks - null every slot, keep the count (asm RadioClass::Limbo keeps the
// vector alive; only ObjectClass_Remove0 tears the object down).
// =============================================================================
void RadioClass::ClearLinks()
{
    for (int32 i = 0; i < RadioLinks.Count; ++i)
        RadioLinks.Items[i] = nullptr;
}

// =============================================================================
// ClearLinkReferences (asm Radio_LinkWith +0x3A..+0x60 and
// RadioClass_PointerGotInvalid +0x2E): null every slot equal to pLink.
// Returns the number of references dropped so callers can tell "was linked".
// =============================================================================
int32 RadioClass::ClearLinkReferences(TechnoClass const* pLink)
{
    if (pLink == nullptr)
        return 0;

    int32 cleared = 0;
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks.Items[i] == pLink) {
            RadioLinks.Items[i] = nullptr;
            ++cleared;
        }
    }
    return cleared;
}

// =============================================================================
// SetLinkCount - Resize the link vector to the specified count
// =============================================================================
void RadioClass::SetLinkCount(int32 count)
{
    if (count < 0) count = 0;
    if (count > MaxRadioLinks) count = MaxRadioLinks;

    while (RadioLinks.Count < count)
        RadioLinks.Add(static_cast<TechnoClass*>(nullptr));

    while (RadioLinks.Count > count)
        RadioLinks.Remove(RadioLinks.Count - 1);
}

// =============================================================================
// LinkTo - Establish a radio link with another techno object
// =============================================================================
bool RadioClass::LinkTo(TechnoClass* pLink)
{
    if (!pLink) return false;
    if (ContainsLink(pLink)) return true;
    if (!HasFreeLink()) return false;

    return RadioLinks.Add(pLink);
}

// =============================================================================
// Unlink (asm: the rcExit branch of Radio_LinkWith)
// =============================================================================
bool RadioClass::Unlink(TechnoClass* pLink)
{
    if (!pLink) return false;

    const int32 idx = FindLinkIndex(pLink);
    if (idx < 0) return false;

    RadioLinks[idx] = nullptr;                     // asm clears, never shifts
    return true;
}

// =============================================================================
// IsLinked
// =============================================================================
bool RadioClass::IsLinked(TechnoClass* pLink) const
{
    return ContainsLink(pLink);
}

// =============================================================================
// IsRadioContact / CanCommunicate / GetRadioContact - asm-backed aliases
// =============================================================================
bool RadioClass::IsRadioContact(TechnoClass* pTech) const
{
    return ContainsLink(pTech);
}

// CanCommunicate: the asm has no separate proc; the gate it enforces is the
// Hello-handler ally test.  A contact is communicable when both sides answer
// Is_Ally for our owner house.
bool RadioClass::CanCommunicate(TechnoClass* pTech) const
{
    if (pTech == nullptr)
        return false;
    HouseClass* myHouse = GetRadioHouse();
    if (myHouse == nullptr)
        return false;
    return pTech->Is_Ally(myHouse);
}

TechnoClass* RadioClass::GetRadioContact(int32 idx) const
{
    return GetNthEntry(idx);
}

// =============================================================================
// LinkWith (asm: the rcHello branch of Radio_LinkWith, asm:1244464)
//
//   The convenience form of "ask the peer, record it on Roger": it is exactly
//   SendCommandWithData(RequestLink) with an empty payload, i.e. the asm's
//   rcHello path.  Kept as a bool for the callers that only care whether the
//   handshake succeeded.
// =============================================================================
bool RadioClass::LinkWith(TechnoClass* pLink)
{
    if (pLink == nullptr)
        return false;

    AbstractClass* payload = UnknownRadioControl();
    const int32 answer = SendCommandWithData(RadioCommand::RequestLink, payload, pLink);

    return answer == static_cast<int32>(RadioMessageType::Roger);
}

// =============================================================================
// Limbo (asm: RadioClass::Limbo, asm:1244610-1244638)
//
//   mov al,[esi+81h] / test al,al / jnz skip   ; already in limbo -> no broadcast
//   mov eax,[esi] / push 3                     ; rcExit
//   call [eax+Radio_CommandEachDocked]
//   call ObjectClass_Remove0
// =============================================================================
bool RadioClass::Limbo()
{
    if (!IsInLimbo)
        SendToEachLink(RadioCommand::BugOut);   // BugOut -> rcExit(3)

    return ObjectClass::Limbo();
}

// =============================================================================
// PointerGotInvalid (asm: RadioClass_PointerGotInvalid, asm:1244666-1244720)
//
//   ObjectClass::Detach(pInvalid, removed)
//   for (i = 0; i < RadioLinks.count; ++i)
//       if (data[i] == pInvalid && removed) data[i] = 0;
//
//   Note the asm compares the *link slot* against the raw pointer and only
//   clears when `removed` is true; the count is re-read every iteration.
// =============================================================================
void RadioClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    ObjectClass::PointerGotInvalid(pInvalid, removed);

    if (!removed)
        return;                                 // asm: test bl,bl ; jz out

    TechnoClass* pAsLink = static_cast<TechnoClass*>(pInvalid);
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks.Items[i] == pAsLink)
            RadioLinks.Items[i] = nullptr;       // asm: mov dword ptr [eax], 0
    }
}

// =============================================================================
// ComputeCRC (asm: RadioClass_GetCRC, asm:1244723)
//   Hashes the MissionClass state, the link count, and per link both its array
//   index (vtable +0x10) and its unique id (vtable +0x2C).
// =============================================================================
void RadioClass::ComputeCRC(CRCEngine& crc) const
{
    MissionClass::ComputeCRC(crc);

    const int32 count = RadioLinks.Count;
    crc.AddData(&count, sizeof(count));

    for (int32 i = 0; i < count; ++i) {
        TechnoClass* pLink = RadioLinks[i];
        if (pLink == nullptr)
            continue;

        int32 arrayIndex = pLink->GetArrayIndex();
        crc.AddData(&arrayIndex, sizeof(arrayIndex));

        int32 uniqueId = AbstractClass::Find_Index(pLink);
        crc.AddData(&uniqueId, sizeof(uniqueId));
    }
}

// =============================================================================
// Link-table probes (original naming, asm-faithful)
// =============================================================================

// RadioClass_IsArrayEmpty (asm:1245160): true when at least one slot is null
// (i.e. the table has room); a zero-length table reports false.
bool RadioClass::IsArrayEmpty() const
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] == nullptr)
            return true;
    }
    return false;
}

// RadioClass_ArrayHasAtLeastOneItem (asm:1245285): true when any slot is set.
bool RadioClass::ArrayHasAtLeastOneItem() const
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] != nullptr)
            return true;
    }
    return false;
}

bool RadioClass::ContainsEntry(TechnoClass* pTech) const
{
    return ContainsLink(pTech);
}

// RadioClass_IsObjectInArray (asm:1245232): true when pTech is present *or*
// when the scan meets a null slot first (the asm treats a hole as a hit).
bool RadioClass::IsObjectInArray(TechnoClass* pTech) const
{
    for (int32 i = 0; i < RadioLinks.Count; ++i) {
        if (RadioLinks[i] == nullptr)
            return true;
        if (RadioLinks[i] == pTech)
            return true;
    }
    return false;
}

TechnoClass* RadioClass::GetNthEntry(int32 idx) const
{
    if (idx < 0 || idx >= RadioLinks.Count)
        return nullptr;
    return RadioLinks.Items[idx];
}

bool RadioClass::GetNthEntry2(int32 idx, TechnoClass** pOut) const
{
    if (!pOut)
        return false;
    *pOut = GetNthEntry(idx);
    return *pOut != nullptr;
}

int32 RadioClass::GetIdxOfEntry2(TechnoClass* pTech) const
{
    return FindLinkIndex(pTech);
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知链路指令与停靠的原版命名形态是现有指令面的别名：
// SendToFirst 对首链路下发，SendToEach 向所有链路广播，SendSmth 携带
// 数据载荷下发，LinkWith 建立双向链路。
// ------------------------------------------------------------------------
int32 RadioClass::SendToFirst(RadioCommand command)
{
    return SendToFirstLink(command);
}

void RadioClass::SendToEach(RadioCommand command)
{
    SendToEachLink(command);
}

int32 RadioClass::SendSmth(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient)
{
    return SendCommandWithData(command, pInOut, pRecipient);
}
