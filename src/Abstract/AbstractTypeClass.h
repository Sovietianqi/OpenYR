#pragma once

#include "AbstractClass.h"

class CCINIClass;
class HouseClass;
class CRCEngine;

class AbstractTypeClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::Abstract;

    static DynamicVectorClass<AbstractTypeClass*>* Array;

    // ------------------------------------------------------------------
    // Static array management
    // ------------------------------------------------------------------
    static void Init_Array();
    static void Delete_Array();
    static int32 Get_Count();
    static AbstractTypeClass* Find(const char* pID);
    static AbstractTypeClass* Find_By_Index(int32 index);
    static AbstractTypeClass* Find_Or_Allocate(const char* pID);
    static void Delete_All();

    // ------------------------------------------------------------------
    // Construction / destruction
    // ------------------------------------------------------------------
    AbstractTypeClass(const char* pID) noexcept;
    explicit AbstractTypeClass(noinit_t) noexcept;
    virtual ~AbstractTypeClass();

    // ------------------------------------------------------------------
    // IPersistStream
    // ------------------------------------------------------------------
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    // ------------------------------------------------------------------
    // RTTI / size
    // ------------------------------------------------------------------
    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    // ------------------------------------------------------------------
    // CRC
    // ------------------------------------------------------------------
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // ------------------------------------------------------------------
    // Theater / INI hooks (subclasses override)
    // ------------------------------------------------------------------
    virtual void LoadTheaterSpecificArt(int32 th_type);
    virtual bool LoadFromINI(CCINIClass* pINI);
    virtual bool SaveToINI(CCINIClass* pINI);

    // ------------------------------------------------------------------
    // INI presence (delegates to LoadFromINI / SaveToINI)
    // ------------------------------------------------------------------
    virtual bool Read_INI(CCINIClass* pINI) override;
    virtual bool Write_INI(CCINIClass* pINI) const override;

    // ------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------
    const char* get_ID() const;
    const char* get_Name() const;
    int32 Get_Cost() const { return Cost; }
    int32 Get_TechLevel() const { return TechLevel; }
    int32 Get_ArrayIndex() const { return ArrayIndex; }
    int32 Get_Build_Limit() const;
    int32 Get_Owners_Count() const;
    bool Is_Allowed_For_House(HouseClass* pHouse) const;

    // ------------------------------------------------------------------
    // Coordinate helper - parses an X,Y,Z triple from an INI key
    // ------------------------------------------------------------------
    static CoordStruct Coordinate_From_INI(CCINIClass* pINI,
                                           const char* pSection,
                                           const char* pKey,
                                           const CoordStruct& defaultCoord);

protected:
    void Set_ArrayIndex(int32 index) { ArrayIndex = index; }

public:
    char ID[0x18];
    uint8 zero_3C;
    char UINameLabel[0x20];
    wchar_t UIName[64];
    char Name[0x31];
    int32 ArrayIndex;
    int32 TechLevel;
    int32 Cost;
};
