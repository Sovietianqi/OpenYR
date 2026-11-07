#pragma once

#include <Abstract/MissionClass.h>
#include <Containers/VectorClass.h>

// ============================================================================
// eRadioCommands (asm mappedto_3739): the wire protocol spoken between two
// linked objects.  OpenYR historically modelled only the docking subset as
// RadioCommand; this is the full table the binary actually uses, so the new
// Receive_Radio / SendCommand implementations can speak the original codes.
// ============================================================================
enum class RadioMessageType : int32 {
    ReplyCannotEnterUnload     = 0,   // rcReplyCannotEnterUnload
    Roger                      = 1,   // rcRoger
    Hello                      = 2,   // rcHello   - "I want to link with you"
    Exit                       = 3,   // rcExit    - "I am leaving the link"
    Msg4                       = 4,
    Msg5                       = 5,
    Msg6                       = 6,
    Msg7                       = 7,
    Msg8                       = 8,
    Msg9                       = 9,
    ReplyNoCantEnterAsPassenger= 10,  // rcReplyNoCantEnterAsPassenger
    Msg11                      = 11,
    Msg12                      = 12,
    Msg13                      = 13,
    Msg14                      = 14,
    QueryCanEnterAsPassenger   = 15,  // rcQueryCanEnterAsPassenger
    Msg16                      = 16,
    Msg17                      = 17,
    Msg18                      = 18,
    NeedToMove                 = 19,  // rcNeedToMove
    Msg20                      = 20,
    Msg21                      = 21,
    Msg22                      = 22,
    Msg23                      = 23,
    TryAcceptDocker            = 24,  // rcTryAcceptDocker
    Msg25                      = 25,
    Msg26                      = 26,
    Msg27                      = 27,
    Msg28                      = 28,
    Msg29                      = 29,
    Msg30                      = 30,
    WantRearm                  = 31,  // rcWantRearm
    CantRepairLowCash          = 32,  // rcCantRepairLowCash
    Msg33                      = 33,
    QueryWantRepairs           = 34,  // rcQueryWantRepairs
};

// ============================================================================
// RadioClass - base for objects with radio communication (docking/linking)
// Inherits MissionClass
//
//  RadioClass struc (asm sizeof = 0xEC, MissionClass = 0xD0):
//      +0xD0 lastRadios.rc[0..2]  (enum eRadioCommands[3], 0xC bytes)
//      +0xE0 RadioLinks           (VectorPtrTechnoClass_short)
//  In OpenYR the vector is VectorClass<TechnoClass*> with data pointer, count and
//  capacity laid out by the container; the three rolling last-command slots are
//  modelled as LastCommands[3].
// ============================================================================
class NOVTABLE RadioClass : public MissionClass {
public:
    static constexpr int32 MaxRadioLinks = 10;

    // ========================================================================
    // IPersistStream
    // ========================================================================
    virtual HRESULT __stdcall Load(IStream* pStm) override;
    virtual HRESULT __stdcall Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // ObjectClass overrides
    // ========================================================================
    // RadioClass::Limbo (asm:1244610): broadcast rcExit to the links, then chain.
    virtual bool Limbo() override;
    // RadioClass_PointerGotInvalid (asm:1244666): detach and clear dead links.
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;
    // RadioClass_GetCRC (asm:1244723): folds the link table into the checksum.
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ========================================================================
    // Destructor
    // ========================================================================
    virtual ~RadioClass();

    // ========================================================================
    // RadioClass virtuals (vtable slots after the mission handlers:
    //   Radio_CommandFirstDocked  -> SendToFirstLink   (asm:1244983)
    //   Radio_SendCommand         -> SendCommand       (asm:1244641)
    //   Radio_CommandToDocked     -> SendCommandWithData (asm:1244464)
    //   Radio_CommandEachDocked   -> SendToEachLink    (asm:1245016) )
    //
    // All of them return the peer's *wire answer* (1 = Roger, 10 = cannot,
    // 0 = nothing sent), exactly like the asm's `eax`.
    // ========================================================================
    virtual int32 SendToFirstLink(RadioCommand command);
    virtual int32 SendCommand(RadioCommand command, TechnoClass* pRecipient);
    virtual int32 SendCommandWithData(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient);
    virtual void  SendToEachLink(RadioCommand command);

    // ========================================================================
    // Receive side (asm: RadioClass_ReceivedRadioCommand, asm:1244232)
    // ========================================================================
    // This *is* the vtable slot the transmitter calls (the asm reaches it as
    // `[vt_TechnoClass.r.m.o.Techno_ReceivedRadioCommand]`, i.e. the ObjectClass
    // slot as inherited through Techno -> Radio -> Mission -> Object).  arg0 is
    // the source object, carried in an integer slot exactly like the rest of the
    // port's radio API; 0 means "no source".
    virtual int32 ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4) override;

    // Runs the rolling last-command update, then:
    //   cmd == Exit(3)  : drop pSource from RadioLinks and forward the command
    //   cmd == Hello(2) : ally-gate, find/allocate a link slot, forward
    //   otherwise       : forward straight to ObjectClass::ReceivedRadioCommand
    // Returns Roger(1) on success, ReplyNoCantEnterAsPassenger(10) on refusal,
    // or whatever the base returns.
    int32 Receive_Radio(TechnoClass* pSource, RadioMessageType cmd, int32 a4);

    // ========================================================================
    // Non-virtual link management
    // ========================================================================
    TechnoClass* const& GetNthLink(int32 idx = 0) const;
    bool ContainsLink(TechnoClass const* pLink) const;
    int32 FindLinkIndex(TechnoClass const* pLink) const;
    bool HasFreeLink() const;
    bool HasFreeLink(TechnoClass const* pIgnore) const;
    bool HasAnyLink() const;
    void SetLinkCount(int32 count);

    bool LinkTo(TechnoClass* pLink);
    bool Unlink(TechnoClass* pLink);
    bool IsLinked(TechnoClass* pLink) const;

    // First occupied slot, or nullptr (asm: `mov eax,[RadioLinks.data] ; mov edi,[eax]`).
    TechnoClass* GetFirstLink() const;
    // Index of the first null slot, -1 when the table is full (the asm scan at
    // Radio_LinkWith +0xA3 that feeds `ebx` and later `[data + ebx*4]`).
    int32 FindFreeSlot() const;
    // Direct slot write, bounds checked.
    bool AssignLinkSlot(int32 idx, TechnoClass* pLink);
    // Drop every slot without notifying the partners (asm RadioClass::Limbo /
    // the Exit sweep).  Keeps the slot count, it only nulls the entries.
    void ClearLinks();
    // Null every slot that references pLink (asm Radio_LinkWith +0x3A loop and
    // RadioClass_PointerGotInvalid +0x2E).  Returns how many slots were cleared.
    int32 ClearLinkReferences(TechnoClass const* pLink);

    // Convenience/alias surface (asm-backed, original naming)
    bool IsRadioContact(TechnoClass* pTech) const;
    bool CanCommunicate(TechnoClass* pTech) const;
    TechnoClass* GetRadioContact(int32 idx = 0) const;

    // ========================================================================
    // Constructor
    // ========================================================================
    RadioClass() noexcept;

protected:
    explicit __forceinline RadioClass(noinit_t) noexcept : MissionClass(noinit) {}

    // ========================================================================
    // Hookup points for the derived TechnoClass
    //
    //  RadioClass_ReceivedRadioCommand reaches into TechnoClass state that the
    //  base cannot own (`[this+0x21C]` = Owner house).  Until MissionClass ->
    //  RadioClass -> TechnoClass is wired up these answer "no house" / "no self",
    //  which makes the ally gate fail-closed exactly like the original's null
    //  check.  TechnoClass is expected to override them with:
    //      HouseClass* GetRadioHouse() const override { return Owner; }
    //      TechnoClass* GetRadioSelf() const override { return const_cast<TechnoClass*>(this); }
    // ========================================================================
    virtual HouseClass* GetRadioHouse() const { return nullptr; }
    // "this" as the TechnoClass the wire protocol speaks to.  Needed because the
    // ally gate is mutual: source->Is_Ally(this) requires a TechnoClass*.
    virtual TechnoClass* GetRadioSelf() const { return nullptr; }

public:
    // ========================================================================
    // Properties (offset 0xD0 from RadioClass start)
    // ========================================================================
    RadioCommand LastCommands[3];
    VectorClass<TechnoClass*> RadioLinks;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知链路表查询以原版命名成对暴露（2 号形态为出参版）。
    // ------------------------------------------------------------------------
    bool IsArrayEmpty() const;
    bool ArrayHasAtLeastOneItem() const;
    bool ContainsEntry(TechnoClass* pTech) const;
    bool IsObjectInArray(TechnoClass* pTech) const;
    TechnoClass* GetNthEntry(int32 idx) const;
    bool GetNthEntry2(int32 idx, TechnoClass** pOut) const;
    int32 GetIdxOfEntry2(TechnoClass* pTech) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知链路指令与停靠的原版命名形态：SendToFirst 对首
    // 链路下发，SendToEach 向所有链路广播，SendSmth 携带数据载荷下发，
    // LinkWith 建立双向链路。
    // ------------------------------------------------------------------------
    int32 SendToFirst(RadioCommand command);
    void SendToEach(RadioCommand command);
    int32 SendSmth(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient);
    bool LinkWith(TechnoClass* pLink);
};
