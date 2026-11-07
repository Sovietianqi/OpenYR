#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

class CRCEngine;

// ============================================================================
// ScriptTypeClass
//
// AI script TYPE -- the [ScriptTypes] definition of an ordered list of script
// actions.  The original name is AIScriptTypeClass (sizeof 0x234):
//
//   +0x00  AbstractTypeClass (0x94) -- vtable + ID/Name at asm +0x24/+0x64
//   +0x94  ... AbstractTypeClass's trailing fields (OpenYR layout)
//   +0x98  idxScript  (int32)       -- cached Array index (asm GetArrayIndex)
//   +0x9C  isGlobal   (int32)       -- owned by the map (0) vs global (1)
//   +0xA0  cntActions (int32)       -- number of populated action slots
//   +0xA4  actions    (50 x 8 bytes, {action, argument})
//
// The action stream is read from the [ScriptTypes] section: each key "0".."49"
// holds a "action,argument" pair parsed by AIScriptAction_Read.
// ============================================================================
class ScriptTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::ScriptType;

    static DynamicVectorClass<ScriptTypeClass*>* Array;

    // ------------------------------------------------------------------
    // asm: ScriptTypeClass_CreateFromINIList
    //   Walks the [ScriptTypes] section and builds/loads one script per
    //   listed name.  Sentinels "<none>"/"none" are skipped.
    // ------------------------------------------------------------------
    static void CreateFromINIList(CCINIClass* pINI);

    // Lookup by ID (asm +0x24); convenience wrapper used by ScriptClass /
    // TeamTypeClass / AITeamTypeClass deserialization paths.
    static ScriptTypeClass* Find(const char* pID);

    // asm: ScriptTypeClass_FindByName_IDandName -- matches ID (asm +0x24)
    // first, then Name (asm +0x64).
    static ScriptTypeClass* FindByName_IDandName(const char* pName);

    // asm: ScriptTypeClass_FindByName_NameOnly -- matches Name (asm +0x64).
    static ScriptTypeClass* FindByName_NameOnly(const char* pName);

    // asm: ScriptTypeClass_FindOrAllocate -- returns null for the
    // "<none>"/"none" sentinels, otherwise finds or news up an entry.
    static ScriptTypeClass* FindOrAllocate(const char* pName);

    virtual ~ScriptTypeClass();

    // ------------------------------------------------------------------
    // IPersistStream
    // ------------------------------------------------------------------
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    // ------------------------------------------------------------------
    // RTTI / identity
    // ------------------------------------------------------------------
    virtual AbstractType WhatAmI() const override;
    // asm: ScriptTypeClass_GetAbstractDerivationID -- constant 0x1B.
    virtual AbstractType GetAbstractDerivationID() const;
    // asm: ScriptTypeClass_GetSize -- constant 0x234.
    virtual int32 Size() const override;
    // asm: ScriptTypeClass_GetArrayIndex -- reads asm +0x98.
    virtual int32 GetArrayIndex() const override;
    // asm: ScriptTypeClass_GetCRC -- AbstractTypeClass CRC + cntActions.
    virtual int32 GetCRC() const;

    // asm: ScriptTypeClass_PointerGotInvalid (retn 8)
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed);

    // ------------------------------------------------------------------
    // INI
    // ------------------------------------------------------------------
    // asm: ScriptTypeClass_LoadFromINI (retn 4)
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    // asm: ScriptTypeClass_SaveIntoINI (retn 4)
    bool SaveIntoINI(CCINIClass* pINI);

    // ------------------------------------------------------------------
    // Action stream accessors (convenience wrappers over the asm fields)
    // ------------------------------------------------------------------
    int32 GetCommandCount() const { return ActionsCount; }
    int32 GetTotalActionCount() const { return ActionsCount; }
    bool IsEmpty() const { return ActionsCount == 0; }

    void AddCommand(int32 action, int32 argument);
    bool GetCommand(int32 index, int32* pAction, int32* pArgument) const;
    bool HasCommand(int32 action) const;

    // ------------------------------------------------------------------
    // Action-stream helpers used by the script engine and editors.
    // ------------------------------------------------------------------
    void AddAction(int32 action, int32 argument) { AddCommand(action, argument); }
    int32 GetActionArgument(int32 index) const {
        return (index >= 0 && index < ActionsCount) ? ScriptActions[index].Argument : 0;
    }
    bool HasAction(int32 action) const { return HasCommand(action); }
    static const char* GetActionName(int32 action);

    ScriptTypeClass(const char* pID) noexcept;

protected:
    explicit ScriptTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    int32 ArrayIndex;
    int32 IsGlobal;
    int32 ActionsCount;
    ScriptActionNode ScriptActions[MAX_SCRIPT_ACTIONS_COUNT];
};
