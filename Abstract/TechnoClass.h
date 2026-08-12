#pragma once

#include "ObjectClass.h"
#include "TechnoTypeClass.h"

// ============================================================================
// CloakStateEnum - tracks the cloak fade animation
// ============================================================================
enum class CloakStateEnum : int32 {
    Idle        = 0,
    Cloaking    = 1,
    Cloaked     = 2,
    Uncloaking  = 3
};

class TechnoClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Techno;

    static DynamicVectorClass<TechnoClass*>* Array;

    TechnoClass() noexcept
        : ObjectClass()
        , TechnoType(nullptr)
        , Health(0)
        , MaxHealth(0)
        , VeterancyLevel(0)
        , Experience(0)
        , CloakState(CloakStateEnum::Idle)
        , CloakAlpha(255)
        , FireRechargeTimer(0)
        , CloakTimer(0)
        , RepairActive(false)
        , RepairRate(0)
        , IronCurtainTimer(0)
        , ForceShieldTimer(0)
        , LastFireFrame(-0x7FFFFFFF)
        , FireDamageTimer(0)
        , SparkyCounter(0)
        , IsParasited(false)
        , TemporalTimer(0)
        , GasTimer(0)
        , RadiationTimer(0)
    {}
    virtual ~TechnoClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::Techno; }
    virtual int32 Size() const override { return sizeof(TechnoClass); }

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    HouseClass* GetOwningHouse() const { return Owner; }
    int32 GetOwningHouseIndex() const { return 0; }

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(TechnoClass* pInstance);
    static bool Remove_From_Array(TechnoClass* pInstance);
    static int32 Get_Total_Count();
    static TechnoClass* Get_Instance(int32 index);
    static int32 Find_Index(TechnoClass* pInstance);

    // ========================================================================
    // Update loop (AI, combat, cloaking)
    // ========================================================================
    virtual void Update() override;
    void Update_AI();
    virtual void Update_Combat();
    void Update_Cloak();
    void Update_Repair();
    void Update_Veterancy();

    // ========================================================================
    // Fire weapon implementation
    // ========================================================================
    BulletClass* Fire_Impl(AbstractClass* pTarget, int32 nWeaponIndex);

    // ========================================================================
    // TakeDamage implementation
    // ========================================================================
    bool TakeDamage_Impl(int32 damage, ObjectClass* pSource,
                         WarheadTypeClass* pWarhead);

    // ========================================================================
    // Repair logic
    // ========================================================================
    void Repair_Start(int32 rate);
    void Repair_Stop();

    // ========================================================================
    // Cloak / Uncloak
    // ========================================================================
    virtual void Cloak(bool bPlaySound);
    virtual void Uncloak(bool bPlaySound);
    bool Is_Cloaked() const;
    bool Is_Cloaking() const;

    // ========================================================================
    // Veteran / Promote
    // ========================================================================
    void Promote(int32 experience);
    virtual int32 GetVeterancy() const;
    int32 Get_Experience() const;

    // ========================================================================
    // Is_Ally / Is_Enemy
    // ========================================================================
    bool Is_Ally(HouseClass* pHouse) const;
    bool Is_Enemy(HouseClass* pHouse) const;
    bool Is_Ally(TechnoClass* pTechno) const;
    bool Is_Enemy(TechnoClass* pTechno) const;

    // ========================================================================
    // Threat position
    // ========================================================================
    CoordStruct Get_Threat_Pos() const;

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ========================================================================
    // TechnoClass virtuals (preserved from original header)
    // ========================================================================
    virtual bool IsVoxel() const { return false; }
    virtual void Destroyed(ObjectClass* Killer) {}
    virtual bool CanScatter() const { return false; }
    virtual int32 GetDefaultSpeed() const { return 0; }
    virtual bool HasTurret() const { return false; }
    virtual bool CanDeploySlashUnload() const { return false; }
    virtual bool IsUnitFactory() const { return false; }
    virtual void TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead) {}
    virtual bool IsEngineer() const { return false; }
    virtual bool IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const { return false; }
    virtual bool IsCloseEnoughToAttack(AbstractClass* pTarget) const { return false; }
    virtual bool IsInAir() const { return false; }
    virtual bool IsOnFloor() const { return false; }
    virtual void Panic() {}
    virtual void Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination) {}
    virtual void UpdateIdleAction() {}
    virtual bool IsPowerOnline() const { return false; }
    virtual bool IsArmed() const { return false; }
    virtual bool IsBeingRepaired() const { return false; }
    virtual bool IsCurrentlyBeingSold() const { return false; }
    virtual bool IsPowered() const { return false; }
    virtual bool IsSelling() const { return false; }
    virtual bool IsFiring() const { return false; }
    virtual bool IsDeploying() const { return false; }
    virtual bool IsBeingDrained() const { return false; }
    virtual bool IsSensorsOnline() const { return false; }
    virtual bool IsPowerDrain() const { return false; }
    virtual bool IsCharged() const { return false; }
    virtual bool IsFactoryActive() const { return false; }
    virtual bool IsLaserFencePost() const { return false; }
    virtual bool IsCapturable() const { return false; }
    virtual bool IsOccupiable() const { return false; }
    virtual bool IsRubble() const { return false; }
    virtual bool IsBridge() const { return false; }
    virtual bool IsWall() const { return false; }
    virtual bool IsGate() const { return false; }
    virtual bool IsOverlay() const { return false; }
    virtual bool IsLight() const { return false; }
    virtual bool IsVehicle() const { return false; }
    virtual bool IsTiberium() const { return false; }
    virtual bool CanOccupyFire() const { return false; }
    virtual int32 GetOccupantCount() const { return 0; }
    virtual double GetStoragePercentage() const { return 0.0; }
    virtual int32 GetRefund() const { return 0; }
    virtual BulletClass* Fire(AbstractClass* pTarget, int32 nWeaponIndex) { return nullptr; }
    virtual bool IsClearlyVisibleTo(HouseClass* House) const { return true; }
    virtual bool IsControllable() const { return false; }
    virtual bool IsActive() const { return true; }
    virtual bool IsSelectable() const { return true; }
    virtual bool CanBeSelected() const { return true; }
    virtual bool CanBeSelectedNow() const { return true; }

    // ========================================================================
    // Iron Curtain / Force Shield / damage-effect state
    //
    // The Iron Curtain and Force Shield super weapons grant absolute
    // invulnerability for a bounded number of frames. The damage-effect
    // timers track the secondary warhead effects (fire, sparks, parasite,
    // temporal freeze, gas, radiation) that ApplyToTechno attaches to a
    // target after the primary damage has been resolved.
    // ========================================================================
    bool IsIronCurtained() const { return IronCurtainTimer > 0; }
    bool IsForceShielded() const { return ForceShieldTimer > 0; }
    bool IsShielded() const { return IronCurtainTimer > 0 || ForceShieldTimer > 0; }

    void ApplyIronCurtain(int32 frames) { if (frames > IronCurtainTimer) IronCurtainTimer = frames; }
    void ApplyForceShield(int32 frames) { if (frames > ForceShieldTimer) ForceShieldTimer = frames; }

    bool IsOnFire() const { return FireDamageTimer > 0; }
    void SetOnFire(int32 frames) { if (frames > FireDamageTimer) FireDamageTimer = frames; }

    bool IsSparky() const { return SparkyCounter > 0; }
    void SetSparky(int32 count) { SparkyCounter += count; }

    bool IsParasiteAttached() const { return IsParasited; }
    void SetParasite() { IsParasited = true; }

    bool IsTemporalized() const { return TemporalTimer > 0; }
    void SetTemporal(int32 frames) { if (frames > TemporalTimer) TemporalTimer = frames; }

    bool IsGassed() const { return GasTimer > 0; }
    void SetGas(int32 frames) { if (frames > GasTimer) GasTimer = frames; }

    bool IsIrradiated() const { return RadiationTimer > 0; }
    void SetRadiation(int32 frames) { if (frames > RadiationTimer) RadiationTimer = frames; }

    int32 GetLastFireFrame() const { return LastFireFrame; }
    void SetLastFireFrame(int32 frame) { LastFireFrame = frame; }

    // ========================================================================
    // State (TechnoClass-specific)
    // ========================================================================
    TechnoTypeClass* TechnoType;        // back-pointer to this techno's type definition
    int32         Health;
    int32         MaxHealth;
    int32         VeterancyLevel;     // 0=Rookie, 1=Veteran, 2=Elite
    int32         Experience;
    CloakStateEnum CloakState;
    uint8         CloakAlpha;         // 255 = fully visible, 0 = fully cloaked
    int32         FireRechargeTimer;  // frames remaining before next shot
    int32         CloakTimer;         // frames remaining in current cloak state
    bool          RepairActive;       // true while a service depot is healing us
    int32         RepairRate;         // HP per frame while being repaired

    // Iron Curtain / Force Shield invulnerability timers (frames remaining).
    int32         IronCurtainTimer;
    int32         ForceShieldTimer;

    // Frame stamp of the last successful weapon discharge (Game::CurrentFrame).
    // Used by Fire_Impl to gate firing on the weapon's rate of fire.
    int32         LastFireFrame;

    // Secondary warhead-effect state. These timers/counters are decremented
    // by Update_AI each frame and consulted by the damage / rendering code.
    int32         FireDamageTimer;    // frames remaining while burning
    int32         SparkyCounter;      // pending spark particle spawns
    bool          IsParasited;        // a parasite is attached to this techno
    int32         TemporalTimer;      // frames frozen by the chronosphere weapon
    int32         GasTimer;           // frames affected by gas
    int32         RadiationTimer;     // frames irradiated by a rad warhead

protected:
    explicit TechnoClass(noinit_t) noexcept : ObjectClass(noinit) {}
};
