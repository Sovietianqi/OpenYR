#pragma once

#include "../COM/IUnknown.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../IO/CRC.h"

class AbstractClass : public IPersistStream, public IRTTITypeInfo, public INoticeSink, public INoticeSource {
public:
    static const AbstractType AbsID = AbstractType::Abstract;

    static DynamicVectorClass<AbstractClass*>* Array;

    static const char* GetClassName(AbstractType abs) {
        static const char* names[] = {
            "None", "Unit", "Aircraft", "AircraftType", "Anim", "AnimType",
            "Building", "BuildingType", "Bullet", "BulletType", "Campaign",
            "Cell", "Factory", "House", "HouseType", "Infantry", "InfantryType",
            "Isotile", "IsotileType", "BuildingLight", "Overlay", "OverlayType",
            "Particle", "ParticleType", "ParticleSystem", "ParticleSystemType",
            "Script", "ScriptType", "Side", "Smudge", "SmudgeType", "Special",
            "SuperWeaponType", "TaskForce", "Team", "TeamType", "Terrain",
            "TerrainType", "Trigger", "TriggerType", "UnitType", "VoxelAnim",
            "VoxelAnimType", "Wave", "Tag", "TagType", "Tiberium", "Action",
            "Event", "WeaponType", "WarheadType", "Waypoint", "Abstract",
            "Tube", "LightSource", "EMPulse", "TacticalMap", "Super",
            "AITrigger", "AITriggerType", "Neuron", "FoggedObject",
            "AlphaShape", "VeinholeMonster", "NavyType", "SpawnManager",
            "CaptureManager", "Parasite", "Bomb", "RadSite", "Temporal",
            "Airstrike", "SlaveManager", "DiskLaser"
        };
        uint32 idx = static_cast<uint32>(abs);
        if (idx < 74) return names[idx];
        return nullptr;
    }

    const char* GetClassName() const {
        return AbstractClass::GetClassName(this->WhatAmI());
    }

    virtual HRESULT QueryInterface(REFIID iid, void** ppvObject) override {
        if (ppvObject) *ppvObject = nullptr;
        return E_FAIL;
    }
    virtual ULONG AddRef() override {
        ++RefCount;
        return RefCount;
    }
    virtual ULONG Release() override {
        if (RefCount > 0) --RefCount;
        return RefCount;
    }

    virtual HRESULT GetClassID(CLSID* pClassID) override = 0;

    virtual HRESULT IsDirty() override { return 0; }
    virtual HRESULT Load(IStream* pStm) override = 0;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override = 0;
    virtual HRESULT GetSizeMax(uint64* pcbSize) override { return 0; }

    virtual AbstractType What_Am_I() const override { return AbstractType::Abstract; }
    virtual int32 Fetch_ID() const override { return static_cast<int32>(UniqueID); }
    virtual void Create_ID() override {}

    virtual bool INoticeSink_Unknown(DWORD dwUnknown) override { return false; }
    virtual void INoticeSource_Unknown() override {}

    virtual ~AbstractClass() {}

    virtual void Init() {}
    virtual void PointerExpired(AbstractClass* pAbstract, bool removed) {}
    virtual AbstractType WhatAmI() const = 0;
    virtual int32 Size() const = 0;
    virtual void ComputeCRC(CRCEngine& crc) const {}
    virtual int32 GetOwningHouseIndex() const { return -1; }
    virtual HouseClass* GetOwningHouse() const { return nullptr; }
    virtual int32 GetArrayIndex() const { return -1; }
    virtual bool IsDead() const { return false; }
    virtual CoordStruct* GetCoords(CoordStruct* pCrd) const {
        *pCrd = CoordStruct(0, 0, 0);
        return pCrd;
    }
    virtual CoordStruct* GetDestination(CoordStruct* pCrd, TechnoClass* pDocker = nullptr) const {
        *pCrd = CoordStruct(0, 0, 0);
        return pCrd;
    }
    virtual bool IsOnFloor() const { return true; }
    virtual bool IsInAir() const { return false; }
    virtual CoordStruct* GetCenterCoords(CoordStruct* pCrd) const {
        return GetCoords(pCrd);
    }
    virtual void Update() {}

    CoordStruct GetCoords() const {
        CoordStruct ret;
        this->GetCoords(&ret);
        return ret;
    }

    CoordStruct GetDestination(TechnoClass* pDocker = nullptr) const {
        CoordStruct ret;
        this->GetDestination(&ret, pDocker);
        return ret;
    }

    CoordStruct GetCenterCoords() const {
        CoordStruct ret;
        this->GetCenterCoords(&ret);
        return ret;
    }

    bool operator<(const AbstractClass& rhs) const {
        return this->UniqueID < rhs.UniqueID;
    }

    AbstractClass() noexcept : UniqueID(0), RefCount(0), Dirty(false) {}

protected:
    explicit AbstractClass(noinit_t) noexcept : UniqueID(0), RefCount(0), Dirty(false) {}

public:
    DWORD UniqueID;
    uint32 Flags;
    DWORD unknown_18;
    ULONG RefCount;
    bool Dirty;
    uint8 padding_21[3];

    // ========================================================================
    // Static array management (declared here so the .cpp can define them)
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(AbstractClass* pInstance);
    static bool Remove_From_Array(AbstractClass* pInstance);
    static int32 Get_Total_Count();
    static AbstractClass* Get_Instance(int32 index);
    static int32 Find_Index(AbstractClass* pInstance);
    static void Delete_All_Instances();
    static const DynamicVectorClass<AbstractClass*>* Get_Array_Ptr();
    static void For_Each_Instance(bool (*pCallback)(AbstractClass*, void*),
                                  void* pUser);

    // ========================================================================
    // INI hooks - AbstractClass has no INI presence but the hooks let
    // polymorphic callers dispatch without RTTI checks.
    // ========================================================================
    virtual bool Read_INI(CCINIClass* pINI);
    virtual bool Write_INI(CCINIClass* pINI) const;

    // ========================================================================
    // CRC helper - feeds common bookkeeping fields into the stream.
    // Concrete subclasses chain this from their ComputeCRC override.
    // ========================================================================
    void Compute_CRC_Abstract(CRCEngine& crc) const;

    // ========================================================================
    // UniqueID generator - monotonic counter for standalone builds.
    // ========================================================================
    void Create_ID_Internal();
};