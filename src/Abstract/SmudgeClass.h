#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class SmudgeTypeClass;
class HouseClass;

// ============================================================================
// SmudgeClass - instance of a smudge (scorch marks, craters, etc.)
// Smudges are non-blocking ground decorations that persist on the map after
// explosions. They are stored per-cell and serialized with the scenario.
// ============================================================================
class NOVTABLE SmudgeClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Smudge;

    static DynamicVectorClass<SmudgeClass*>* Array;

    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    SmudgeClass(HouseClass* pOwner = nullptr) noexcept;
    explicit SmudgeClass(noinit_t) noexcept : ObjectClass(noinit) {}
    virtual ~SmudgeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual void Update() override;

    // Smudge-specific helpers
    int32  Get_CRC() const;
    void   Draw_It(int32 originX, int32 originY) const;
    bool   Is_Visible() const;

    SmudgeTypeClass* Type;
    int32 Frame;
    int32 Health;
    bool IsActive;
};
