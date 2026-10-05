#pragma once

#include <Abstract/MissionClass.h>
#include <Containers/VectorClass.h>

// ============================================================================
// RadioClass - base for objects with radio communication (docking/linking)
// Inherits MissionClass
// Original offset: data starts at 0xD0 after MissionClass
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
    // Destructor
    // ========================================================================
    virtual ~RadioClass();

    // ========================================================================
    // RadioClass virtuals
    // ========================================================================
    virtual RadioCommand SendToFirstLink(RadioCommand command);
    virtual RadioCommand SendCommand(RadioCommand command, TechnoClass* pRecipient);
    virtual RadioCommand SendCommandWithData(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient);
    virtual void SendToEachLink(RadioCommand command);

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

    // ========================================================================
    // Constructor
    // ========================================================================
    RadioClass() noexcept;

protected:
    explicit __forceinline RadioClass(noinit_t) noexcept : MissionClass(noinit) {}

    // ========================================================================
    // Properties (offset 0xD0 from RadioClass start)
    // ========================================================================
public:
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
    RadioCommand SendToFirst(RadioCommand command);
    void SendToEach(RadioCommand command);
    RadioCommand SendSmth(RadioCommand command, AbstractClass*& pInOut, TechnoClass* pRecipient);
    bool LinkWith(TechnoClass* pLink);

};