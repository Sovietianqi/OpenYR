#pragma once

#include <Abstract/AbstractTypeClass.h>
#include <Containers/DynamicVectorClass.h>

class CCINIClass;
class CRCEngine;
class HouseClass;
class SHPStruct;

// ============================================================================
// ObjectTypeClass - base type class for all placeable map objects
//
//  Sits between AbstractTypeClass and the concrete TechnoTypeClass /
//  OverlayTypeClass / SmudgeTypeClass / TerrainTypeClass / VoxelAnimTypeClass
//  hierarchies.  Holds the fields common to every object type: sight range,
//  cost, tech level, strength, and other build/selection metadata parsed from
//  the rules INI.
//
//  Note: Cost, TechLevel, ID, UIName and Name are inherited from
//  AbstractTypeClass and are not re-declared here.
// ============================================================================
class NOVTABLE ObjectTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::Object;

    // Global registry of all object types.
    static DynamicVectorClass<ObjectTypeClass*>* Array;

    // Lookup helpers.
    static ObjectTypeClass* Find(const char* pID);
    static ObjectTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    // ------------------------------------------------------------------
    // Construction / destruction
    // ------------------------------------------------------------------
    ObjectTypeClass() noexcept;
    ObjectTypeClass(const char* pID) noexcept;
    explicit ObjectTypeClass(noinit_t) noexcept;
    virtual ~ObjectTypeClass();

    // ------------------------------------------------------------------
    // IPersistStream (stream Load/Save) - default no-op implementations.
    // Concrete subclasses override these.
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
    // INI parsing / CRC
    // ------------------------------------------------------------------
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI);
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // ------------------------------------------------------------------
    // INI helpers
    // ------------------------------------------------------------------
    virtual bool Read_INI(CCINIClass* pINI) override;
    virtual bool Write_INI(CCINIClass* pINI) const override;

    // ------------------------------------------------------------------
    // Placement / map presence
    // ------------------------------------------------------------------
    virtual bool Can_Place_On_Map(const CoordStruct& coord,
                                  HouseClass* pOwner = nullptr) const;
    virtual uint32 Get_Occupy_Bits() const;

    // ------------------------------------------------------------------
    // Type flags - default implementations, overridden where needed.
    // ------------------------------------------------------------------
    virtual bool Is_Temple_Of_NOD() const;
    virtual bool Is_Flak() const;
    virtual bool Is_Listed() const;
    virtual int32 Get_Max_Pips() const;

    // ------------------------------------------------------------------
    // Factory helpers
    // ------------------------------------------------------------------
    virtual ObjectClass* Create_One_Of(HouseClass* pOwner);
    virtual SHPStruct* Get_Cameo_Data() const;

    // ------------------------------------------------------------------
    // INI flag parser - reads Yes/No-style keys into a bitfield.
    // ------------------------------------------------------------------
    void Read_TypeFlags(CCINIClass* pINI, const char* pSection);

    // ------------------------------------------------------------------
    // SHP art resolution - called after the art INI has been loaded.
    // ------------------------------------------------------------------
    virtual void Resolve_SHP_References();

    // ------------------------------------------------------------------
    // Common object-type fields
    // ------------------------------------------------------------------
    int32   Sight;          // sight range in cells
    int32   Speed;          // base movement speed (shadowed by TechnoTypeClass)
    int32   MaxStrength;    // maximum health / HP for instances of this type
    int32   BuildLimit;     // maximum number concurrently buildable
    int32   Score;          // score awarded to the killer when destroyed
    int32   ROT;            // rate of turn (degrees per frame)
    bool    Selectable;     // can the player select instances of this type?
    bool    LegalTarget;    // can instances be targeted for attack?
    bool    Insignificant;  // does this type count toward victory/defeat?
    bool    Immune;         // are instances immune to all damage?
    bool    OnFire;         // is the type drawn with the "on fire" overlay?
    bool    Repairable;     // can instances be repaired?
    bool    Unsellable;     // can instances NOT be sold?
    bool    Cloakable;      // can instances cloak?
    bool    TurretEquipped; // does the type have a turret?
    bool    IsStealthy;     // is the type invisible on radar?
    bool    IsTrainable;    // can the type gain veteran/elite promotions?
    bool    IsNotHuman;     // is the type a non-human (vehicle/building)?
    bool    IsTheater;      // does the art vary by theater?
    Layer   IdleLayer;      // render layer when idle
    LandType Land;          // land type the object occupies
};
