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
    // ObjectTypeClass vtable probes (asm one-liners)
    // ------------------------------------------------------------------
    // ObjectTypeClass_GetImage (asm 0x718xxx): the loaded SHP the type draws
    //   itself with, cached at +0x10 by Resolve_SHP_References.
    virtual SHPStruct* GetImage() const;
    // ObjectTypeClass_GetPipMax (asm 0x716xxx): base answers 0 - only the
    //   types that actually render pips (buildings, harvesters) override it.
    virtual int32 GetPipMax() const;
    // ObjectTypeClass_GetActualCost (asm 0x716xxx): the cost after the
    //   owner's modifiers; at type level nothing is applied, so 0.
    virtual int32 GetActualCost(HouseClass* pOwner) const;
    // ObjectTypeClass_GetBuildSpeed (asm 0x716xxx): base build-speed factor.
    int32 GetBuildSpeed() const;
    // ObjectTypeClass_GetCameo (asm 0x716xxx): the sidebar cameo index; base
    //   has no cameo of its own.
    virtual int32 GetCameo() const;
    // ObjectTypeClass_Generic (asm 0x716xxx): stamps the 'G' marker byte that
    //   the sidebar uses to flag a generic (non-faction) cameo.
    void Generic();

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
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    // ImageSHP (+0x10): the loaded SHP handle for Image, resolved after the
    // art INI has been read.  Null until Resolve_SHP_References runs.
    SHPStruct*  ImageSHP;
    char        Image[0x20];
    char        AlphaImage[0x20];
    int32 CrushSound;
    int32 AmbientSound;
    bool         Crushable;
    bool         Bombable;
    bool         NoSpawnAlt;
    bool         AlternateArcticArt;
    bool         RadarInvisible;
    Armor        ArmorType;
    int32        Strength;
    bool         HasRadialIndicator;
    uint8        RadialColor[3];
    bool         IgnoresFirestorm;
    bool         UseLineTrail;
    uint8        LineTrailColor[3];
    int32        LineTrailColorDecrement;
    bool         Theater;
    bool         NewTheater;
    bool         Voxel;
};
