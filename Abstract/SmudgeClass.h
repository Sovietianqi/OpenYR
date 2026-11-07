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

    // asm: InitList_2722 / InitList_2723 —— vec_Smudge 的静态构造/析构
    //   （capacity/increment 置 0x80，data/count 清零）。
    static void sctor();
    static void sdtor();

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

    // asm: SmudgeClass_GetType (mov eax,[ecx+0ACh] ; retn) -- the bound
    //   SmudgeTypeClass* lives at the same instance slot the OverlayClass uses.
    virtual ObjectTypeClass* GetType() const override;
    // asm: SmudgeClass_GetAbstractDerivationID (mov eax,1Dh ; retn)
    virtual int32 GetAbstractDerivationID() const;

    // asm: SmudgeClass_PointerGotInvalid (retn 8 -> 2 args). 先走 ObjectClass
    //   的失效链，再在本类的 Type 指针命中时清空。
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // asm: SmudgeClass_SetLayer (0x6B4BE0, __thiscall(this, idxLayer), retn 4).
    //   Re-runs the ObjectClass layer mark and, on the ground/air layers,
    //   re-stamps the smudge type into every cell the SHP footprint covers.
    bool SetLayer(int32 idxLayer);

    // Smudge-specific helpers
    int32  Get_CRC() const;
    void   Draw_It(int32 originX, int32 originY) const;
    bool   Is_Visible() const;

    SmudgeTypeClass* Type;
    int32 Frame;
    int32 Health;
    bool IsActive;
};
