#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class CRCEngine;

// ============================================================================
// SmudgeTypeClass - type definition for smudges
// Inherits ObjectTypeClass. Describes a kind of smudge (scorch, crater, etc.)
// including its dimensions and behavior on the terrain.
// ============================================================================
class NOVTABLE SmudgeTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::SmudgeType;

    static DynamicVectorClass<SmudgeTypeClass*>* Array;

    static SmudgeTypeClass* Find(const char* pID);
    static SmudgeTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    SmudgeTypeClass(const char* pID) noexcept;
    explicit SmudgeTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~SmudgeTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI);
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // Smudge-specific helpers
    CellStruct Get_Size() const;
    bool       Is_Crater() const;
    bool       Is_Scorch() const;

public:
    CellStruct CellSize;
    int32      Frames;
    bool       IsCrater;
    bool       IsScorch;
    bool       IsBib;
    bool       IsAnimated;
    bool       IsFlat;
    int32      ChainCount;
    int32      ChainSteps;
    char       ArtName[0x20];
};
