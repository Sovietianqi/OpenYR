#pragma once

#include "AbstractClass.h"
#include "AbstractTypeClass.h"
#include "ObjectTypeClass.h"

class ObjectClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::Object;

    static DynamicVectorClass<ObjectClass*>* Array;

    ObjectClass() noexcept : AbstractClass() {}
    virtual ~ObjectClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::Object; }
    virtual int32 Size() const override { return sizeof(ObjectClass); }

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    virtual CoordStruct* GetCoords(CoordStruct* pCrd) const override {
        *pCrd = Location;
        return pCrd;
    }

    virtual bool IsInAir() const override { return false; }
    virtual bool IsOnFloor() const override { return true; }

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(ObjectClass* pInstance);
    static bool Remove_From_Array(ObjectClass* pInstance);
    static int32 Get_Total_Count();
    static ObjectClass* Get_Instance(int32 index);
    static int32 Find_Index(ObjectClass* pInstance);

    // ========================================================================
    // Limbo / Unlimbo - remove from / re-attach to the map
    // ========================================================================
    virtual bool Limbo();
    virtual bool Unlimbo();

    // ========================================================================
    // Coordinate accessors
    // ========================================================================
    CoordStruct Get_Coord() const;
    void Set_Coord(const CoordStruct& coord);

    // ========================================================================
    // Map presence / validity
    // ========================================================================
    bool Is_On_Map() const;
    bool Is_Valid() const;

    // ========================================================================
    // Selection
    // ========================================================================
    virtual bool Select();
    virtual void Deselect();
    bool Is_Selected() const;
    virtual bool Is_Selectable() const;

    // ========================================================================
    // Engineer / spy steal check
    // ========================================================================
    virtual bool Is_Allowed_To_Steal() const;

    // ========================================================================
    // Owner accessors
    // ========================================================================
    void Set_Owner(HouseClass* pNewOwner);
    HouseClass* Get_Owner() const;
    virtual HouseClass* GetOwningHouse() const override;
    virtual int32 GetOwningHouseIndex() const override;

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

    CoordStruct GetCoords() const {
        CoordStruct ret;
        GetCoords(&ret);
        return ret;
    }

    void SetLocation(const CoordStruct& loc) { Location = loc; }

protected:
    explicit ObjectClass(noinit_t) noexcept : AbstractClass(noinit) {}

public:
    CoordStruct Location;
    HouseClass* Owner;
    bool IsSelected;
    bool IsInLimbo;
    uint8 align_34[2];
};
