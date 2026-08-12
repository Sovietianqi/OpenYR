#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

class ScriptTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::ScriptType;

    static DynamicVectorClass<ScriptTypeClass*>* Array;

    static ScriptTypeClass* Find(const char* pID);
    static ScriptTypeClass* FindOrAllocate(const char* pID);

    virtual ~ScriptTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    bool LoadFromINIList(CCINIClass* pINI, bool IsGlobal);
    bool LoadFromINI(CCINIClass* pINI);
    bool SaveToINI(CCINIClass* pINI);

    bool IsActionValid(int32 actionIndex) const;
    static const char* GetActionName(int32 action);
    int32 GetActionArgument(int32 actionIndex) const;
    bool HasAction(int32 actionType) const;
    int32 FindActionIndex(int32 actionType, int32 startFrom = 0) const;
    bool IsEmpty() const;
    int32 GetTotalActionCount() const;
    void AddAction(int32 action, int32 argument);
    void RemoveAction(int32 actionIndex);
    void ClearActions();

    ScriptTypeClass(const char* pID) noexcept;

protected:
    explicit ScriptTypeClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}

public:
    int32 ArrayIndex;
    bool IsGlobal;
    ScriptActionNode ScriptActions[MAX_SCRIPT_ACTIONS_COUNT];
    int32 ActionsCount;
};