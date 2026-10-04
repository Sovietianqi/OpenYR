#pragma once

#include "ObjectClass.h"
#include "TechnoTypeClass.h"
#include <Math/Timer.h>

class BuildingClass;

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
        , Tunnel(false)
        , ActiveTurretIndex(0)
        , CurrentWeaponNumber(-1)
        , OrigOwner(nullptr)
        , Captured(false)
        , IsDisguisedFlag(false)
        , WarpInTimer(0)
        , WarpOutTimer(0)
        , IsWarpingOutFlag(false)
        , DisguiseCreationFrame(-1)
        , DisguiseBlinkTimer()
        , PrimaryFacing()
        , GroundHeight(0)
        , DrainTimer(0)
        , PlanningToken(-1)
        , WeaponStage(0)
        , FocusOnUnit(nullptr)
        , TemporalImUsing(nullptr)
        , QueuedVoiceIndex(-1)
        , CurrentAmmo(0)
        , ReloadTimer()
        , AirstrikeTimeStart(0)
        , AirstrikeTimeLeft(0)
        , AirstrikeTimeGen(0)
    {}
    virtual ~TechnoClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::Techno; }
    virtual int32 Size() const override { return sizeof(TechnoClass); }

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    HouseClass* GetOwningHouse() const { return Owner; }
    int32 GetOwningHouseIndex() const { return 0; }

    // TechnoClass_GetThreatPosed (asm 0x708B50).  The threat this object
    // presents to the enemy, counted into the owning house's threat grid.
    //
    //   * Without a type the answer is zero.
    //   * A building carrying occupants (WhatAmI == 6) multiplies the occupant
    //     count by RulesClass::ThreatPerOccupant.
    //   * Otherwise the type's ThreatPosed value is returned.
    virtual int32 GetThreatValue() const;

    // The cell the object currently stands in.
    CellStruct Get_Cell_Ptr_Coord() const;

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

    // Per-instance serialization for the save-game stream.
    void Save(class SaveGameClass& saver) const;
    void Load(class LoadGameClass& loader);

    // ========================================================================
    // Combat core (mirrors TechnoClass_* in the original binary)
    // ========================================================================
    int32  SelectWeapon(AbstractClass* pTarget);
    bool   IsCloseEnoughToTarget(AbstractClass* pTarget, int32 idxWeapon);
    int32  EvalThreatRating(TechnoClass* pThreat, int32 idxWeapon);
    int32  EstimateDamage(AbstractClass* pTarget, int32 idxWeapon);
    bool   ShouldRetaliate(TechnoClass* pAttacker);
    void   RegisterDestruction();
    void   RegisterLoss();
    CoordStruct GetFLH(int32 nWeaponIndex, bool muzzle);
    bool   IsRadarVisible(HouseClass* pHouse) const;
    int32  Get_ZAdjustment() const;
    VisualType VisualCharacter(bool raw);
    void   CreateGap();
    void   DeleteGap();
    // TechnoClass_UpdatePowered (asm 0x70ED20): re-evaluates whether this
    // techno still receives the power feed it depends on.  Called by an
    // owning structure's BuildingClass_InitMore for each powered unit slot.
    void   UpdatePowered();
    void   UpdateSight();
    void   DrawExtras(Point2D* pCoord, RectangleStruct* pRect);
    void   DrawHidden(Point2D* pCoord, RectangleStruct* pRect);
    void   DealParticleDamage(TechnoClass* pVictim, WarheadTypeClass* pWarhead,
                              int32 damage, int32 distanceFromEpicenter);
    void   PointerGotInvalid(AbstractClass* pInvalid);
    int32  GetSightRange() const;
    bool   GapActive;

    // ========================================================================
    // TechnoClass virtuals (preserved from original header)
    // ========================================================================
    virtual bool IsVoxel() const { return false; }
    virtual void Destroyed(ObjectClass* Killer) {}
    virtual bool CanScatter() const { return false; }
    virtual int32 GetDefaultSpeed() const { return 0; }
    virtual bool HasTurret() const;
    virtual bool CanDeploySlashUnload() const { return false; }
    virtual bool IsUnitFactory() const { return false; }
    virtual void TakeDamage(int32 damage, ObjectClass* source, WarheadTypeClass* warhead) {}
    virtual bool IsEngineer() const { return false; }
    virtual bool IsCloseEnough(AbstractClass* pTarget, int32 idxWeapon) const { return false; }
    virtual bool IsCloseEnoughToAttack(AbstractClass* pTarget) const { return false; }
    virtual bool IsInAir() const;
    virtual bool IsOnFloor() const;
    virtual bool OnBridge() const;

    // ── TechnoClass state probes / accessors ──────────────────────────────
    // TechnoClass_IsCrewed (asm 0x6F3B30): mirrors the type's Crewed flag.
    virtual bool IsCrewed() const;
    // TechnoClass_IsFactory (asm 0x102414): base-class answer is false.
    virtual bool IsFactory() const;
    // TechnoClass_HasMultipleTurrets (asm 0x70DC90).
    virtual bool HasMultipleTurrets() const;
    // TechnoClass_GetZ (asm 0x5F3C70).
    virtual int32 GetZ() const;
    // TechnoClass_GetActiveTurretIndex (asm 0x70DCB0).
    virtual int32 GetActiveTurretIndex() const;
    // TechnoClass_GetTurretIndex (asm 0x70DD30).
    virtual int32 GetTurretIndex() const;
    // TechnoClass_CurrentWeaponSelected (asm 0x70DCA0).
    virtual bool CurrentWeaponSelected() const;
    // TechnoClass_GetOwner (asm 0x70F810).
    HouseClass* Get_Owner() const;
    // ── Disguise ──────────────────────────────────────────────────────────
    // TechnoClass_IsDisguised (asm 0x1905A8): reads the type/instance disguise
    // flag.  The two-argument form adds a range parameter the original accepts
    // and ignores.
    virtual bool IsDisguised() const;
    virtual bool IsDisguised2(int32 a2) const;
    // TechnoClass_ClearDisguise (asm 0x1905C0): drops the disguise.
    virtual void ClearDisguise();

    // ── Cloak / warp / temporal state ─────────────────────────────────────
    // TechnoClass_IsCloakable (asm 0x708C40).
    virtual bool IsCloakable() const;
    // TechnoClass_IsBeingWarpedOut (asm 0x708C50).
    virtual bool IsBeingWarpedOut() const;
    // TechnoClass_IsWarpingOut (asm 0x708C5E).
    virtual bool IsWarpingOut() const;
    // TechnoClass_IsNotTemporalLocked (asm 0x708C8E).
    virtual bool IsNotTemporalLocked() const;
    // TechnoClass_IsNotWarpingIn (asm 0x5F3E31).
    virtual bool IsNotWarpingIn() const;
    // TechnoClass_IsDraining (asm 0x70F5B5).
    virtual bool IsDraining() const;

    // ── Temporal / weapon-legal probes ────────────────────────────────────
    // TechnoClass_IsTemporalSource (asm 0x70C5D0): true when this techno is
    // currently the source of a temporal weapon that has a victim.
    bool IsTemporalSource() const;
    // TechnoClass_IsLegalWeapon (asm 0x70E245): true when the weapon container
    // is non-null and holds a weapon id.
    bool IsLegalWeapon(const void* pWeapon) const;
    // TechnoClass_GetNonSprayWeapon (asm 0x70DDC8-adjacent): returns the weapon
    // in the slot that IsNoSprayAttack selects.
    void* GetNonSprayWeapon() const;
    // TechnoClass_CanAreaFire (asm 0x70DD40): true when the current weapon is
    // flagged as an area-effect weapon.
    bool CanAreaFire() const;

    // ── Weapon selection helpers ──────────────────────────────────────────
    // TechnoClass_CanPassiveAquire (asm 0x70917A).
    virtual bool CanPassiveAquire() const;
    // TechnoClass_CanTraverse (asm 0x802612).
    virtual bool CanTraverse() const;
    // TechnoClass_CanSetWaypoint (asm 0x700C40).
    virtual bool CanSetWaypoint() const;

    // ── Miscellaneous probes ──────────────────────────────────────────────
    // TechnoClass_NeedsToSelfHeal (asm 0x70BE80).
    virtual bool NeedsToSelfHeal() const;
    // TechnoClass_GetHealthState (asm 0x5F5DF0): 0 = healthy, 1 = damaged,
    // 2 = critical, matching the rules-side health thresholds.
    virtual int32 GetHealthState() const;
    // TechnoClass_GetXYDistanceFrom (asm 0x5F6500): planar distance to another
    // object, in leptons.
    virtual double GetXYDistanceFrom(const AbstractClass* pOther) const;
    // ── Panic / idle / power probes ───────────────────────────────────────
    // TechnoClass_Panic (asm 0x41B3xx): the zero-argument form does nothing at
    // this layer; the mission-controller entry point is FootClass::Panic.
    virtual void Panic();
    // TechnoClass_Unpanic: the counterpart that returns the object to normal
    // morale; the base class does nothing.
    virtual void Unpanic();
    virtual void Scatter(const CoordStruct& crd, bool ignoreMission, bool ignoreDestination) {}
    // TechnoClass_IdleAction (asm 0x41B5A4): returns false - derived missions
    // override it to report that they have finished idling.
    virtual bool IdleAction();
    virtual void UpdateIdleAction() {}
    // TechnoClass_IsPowerOnline (asm 0x41B57x): true only for powered
    // structures; the base class answers false.
    virtual bool IsPowerOnline() const;
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
    // TechnoClass_GetTiberium (asm 0x6C9640).  Sums the four tiberium storage
    // floats carried by a harvesting techno and floors the running total.  The
    // base implementation carries no storage and therefore reports zero.
    virtual double Get_Tiberium() const { return 0.0; }

    // IsBeingMindControlled - the vtable slot the original queries at +0x160.
    // True while this object is already under an external mind controller.
    virtual bool IsBeingMindControlled() const { return false; }

    // TechnoClass_CanBePermaMC (asm 0x53C445).  Whether this object may be
    // permanently mind controlled by a psychic dominator:
    //
    //   * buildings never qualify (WhatAmI() == Building);
    //   * the techno type must not be ImmuneToPsionics;
    //   * the object must not already be mind controlled;
    //   * the techno type must not be a balloon-hover type;
    //   * the object must still be alive.
    virtual bool Can_Be_PermaMC() const
    {
        if (WhatAmI() == AbstractType::Building)
            return false;

        const TechnoTypeClass* pType = TechnoType;
        if (pType != nullptr && pType->IsImmuneToPsionics)
            return false;

        if (IsBeingMindControlled())
            return false;

        if (pType != nullptr && pType->IsBalloonHover)
            return false;

        return !IsDead();
    }
    // TechnoClass_CanOccupyFire (asm 0x41B534): only buildings and the infantry
    // that garrison them answer true; the base class returns false.
    virtual bool CanOccupyFire() const;
    // TechnoClass_GetOccupantCount (asm 0x41B53C): number of occupants garrisoned.
    virtual int32 GetOccupantCount() const;

    // ── Layer / cell helpers ──────────────────────────────────────────────
    // TechnoClass_InWhichLayer (asm 0x41ADCB): asks the locomotor which draw
    // layer this techno currently occupies.
    virtual int32 InWhichLayer() const;
    // TechnoClass_GetCellCoords (asm 0x41BEBE): converts the world coordinates
    // into the owning cell's X/Y, dividing by 0x100 (one leptons-per-cell unit).
    virtual CellStruct GetCellCoords() const;

    // ── Threat / value ratings (asm 0x41B547 / 0x41B54F / 0x41B557) ──────
    virtual int32 GetAntiAirValue() const;
    virtual int32 GetAntiArmorValue() const;
    virtual int32 GetAntiInfantryValue() const;

    // TechnoClass_UpdateRefinerySmokeSystems (asm 0x41B5C0): no-op at this layer.
    virtual void UpdateRefinerySmokeSystems() {}

    // ========================================================================
    // Planning-token and type-flag probes
    // ========================================================================
    // TechnoClass_GetPlanningToken (asm 0x70DDC0): the waypoint token slot
    // (+0x514).
    int32 GetPlanningToken() const;
    // TechnoClass_AttachPlanningToken (asm 0x70DDC8): stores token into the
    // waypoint token slot (+0x514).
    void AttachPlanningToken(int32 token);
    // TechnoClass_Assign_Destination_Cell (asm 0x70DDD5): stores the target
    // building into the "focus on unit" slot.
    void Assign_Destination_Cell(BuildingClass* pTarget);
    // TechnoClass_NotSubmerged (asm 0x70DDE0): true when the object's height is
    // above the submarine threshold (-20).
    bool NotSubmerged() const;
    // TechnoClass_IsNotSprayAttack (asm 0x70DD00).
    bool IsNotSprayAttack() const;
    // TechnoClass_IsNotSprayAttack2 (asm 0x70DD20).
    bool IsNotSprayAttack2() const;
    // TechnoClass_SetCurrentWeaponStage (asm 0x70DDD4): stores idx into the
    // multi-stage weapon counter (+0x140) when it is non-negative.
    void SetCurrentWeaponStage(int32 idx);
    // TechnoClass_HasTurretTooltips (asm 0x70DDA6): the type's turret-tooltip
    // flag.
    bool HasTurretTooltips() const;

    // TechnoClass::Greatest_Threat (asm 0x6F8DA0).  The engine's universal
    // target-acquisition entry point.  `projFlags` is the projectile-
    // capability bitmask (ProjectileTypeFlags); `curThreat` seeds the best
    // threat so a caller can require a strictly better candidate; `a4`
    // enumerates the caller class (0 = building, 1 = vehicle, 2 = infantry).
    // Returns the best target found, or null.
    virtual ObjectClass* Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4);

    // TechnoClass_Combat_Damage (asm 0x6F8CB0).  Weapon-slot damage query used
    // by Greatest_Threat to decide whether a special movement class (engineer
    // / terrorist) should ignore military targets.
    int32 Combat_Damage(int32 idxWeapon) const;

    // TechnoClass_Techno_31C (asm 0x7087D0).  Vtable +0x31C - resolves the
    // techno's current target object honouring the requested slot.
    ObjectClass* Techno_31C(int32 which) const;

    // ========================================================================
    // Position / altitude probes
    // ========================================================================

    // TechnoClass_GetCellCoords1 (asm 0x5F6A50).  Writes the techno's owning
    // map cell (floored to the cell grid) into `pOut` and returns it.
    CellStruct* GetCellCoords1(CellStruct* pOut) const;

    // TechnoClass_GetCell1 (asm 0x5F6A90).  The CellClass the techno is
    // standing on, or null off-map.
    CellClass* GetCell1() const;

    // TechnoClass_OnFloor (asm 0x5F6B60) / _InAir (asm 0x5F6B90).  True when
    // the techno's Z puts it on the ground / in the air.  Both test the
    // "has height" flag at +0x74 first, then compare the current Z against
    // twice the ObjectClass::HeightAtSpawn offset (the parked-on-ground
    // threshold).
    bool OnFloor() const;
    bool InAir() const;

    // TechnoClass_GetZFudgeCliff (asm 0x704270) / _Column (asm 0x703E60) /
    // _Tunnel (asm 0x703F00).  FootClass::Get_ZAdjustment consults these to
    // snap a unit's draw height to the terrain in front of it.  Each returns
    // a fudge value in pixels.
    int32 GetZFudgeCliff() const;
    int32 GetZFudgeColumn() const;
    int32 GetZFudgeTunnel() const;

    // TechnoClass_GetElevationRangeBonus (asm 0x6F6FA0) /
    // _GetElevationBonusNoSqrt (asm 0x6F7090).  The extra weapon range a
    // height advantage confers.  The NoSqrt variant skips the square root and
    // is used by the cheaper proximity test.
    double GetElevationRangeBonus(ObjectClass* pTarget) const;
    double GetElevationBonusNoSqrt(ObjectClass* pTarget) const;

    // TechnoClass_TimeForCellInset (asm 0x6F7690).  True when the distance to
    // the target exceeds (warhead CellSpread - CellInset), i.e. the shot has
    // already cleared the minimum arming distance.
    bool TimeForCellInset(TechnoClass* pTarget) const;

    // ========================================================================
    // Combat / role classifiers
    // ========================================================================

    // TechnoClass_CanLobber (asm 0x6F9CB0).  True when the techno's current
    // weapon is flagged as a lobber (arcing artillery).
    bool CanLobber() const;

    // TechnoClass_HasAbility (asm 0x6F9BE0).  True when the type carries the
    // requested special ability flag.
    bool HasAbility(int32 ability) const;

    // TechnoClass_CanBeBunkered (asm 0x6FB5E0).  True when this techno may be
    // loaded into a battle bunker / tank bunker.
    bool CanBeBunkered() const;

    // TechnoClass_CanBePermaMC (asm 0x5B1080).  True when this techno may be
    // permanently mind-controlled (Yuri Prime's capture).
    bool CanBePermaMC() const;

    // TechnoClass_BelongsToPlayer (asm 0x6FBF40) /
    // _PlayerOwnedAliveAndNamed (asm 0x6FBFF0).  Ownership probes used by the
    // damage text / EVA paths.
    bool BelongsToPlayer() const;
    bool PlayerOwnedAliveAndNamed() const;

    // TechnoClass_GetPointsValue (asm 0x707DC0).  The score value this techno
    // contributes when destroyed: the type's PointValue, plus the value of
    // everything it carries, plus the locomotor's contributed value.
    int32 GetPointsValue() const;

    // TechnoClass_GetTiberiumPercentage (asm 0x708B90).  Ore storage fill
    // fraction (0.0..1.0).  Zero when the type has no storage.
    double GetTiberiumPercentage() const;

    // TechnoClass_GetFacingAgain (asm 0x70ED90).  Writes the current facing
    // through the out-pointer and returns it.
    DirStruct* GetFacingAgain(DirStruct* pOut) const;

    // TechnoClass_GetDisguiseFlags (asm 0x70ED60) /
    // _IsDisguisedAgainst (asm 0x70EE40).  Disguise-blinking state helpers.
    int32 GetDisguiseFlags(int32 flags) const;
    bool IsDisguisedAgainst(HouseClass* pHouse) const;


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

    // TechnoClass_Reload (asm 0x6FB000).
    //
    //  Ticks the ammo counter up by one once the reload timer has run out and
    //  the magazine is not yet full.  A type with Ammo == -1 never reloads
    //  (infinite magazine); a type with a finite magazine whose timer is
    //  still running is left alone.  On a successful reload the techno is
    //  Mark()ed (ground layer) and Techno_Update_Reloading restarts the timer
    //  for the next round.
    void Reload();

    // Techno_Update_Reloading (asm 0x6FB0D0): recomputes and restarts the
    //  reload delay after a round has been loaded.
    void Update_Reloading();

    // TechnoClass_StartAirstrikeTimer (asm 0x6FC930) / _StopAirstrikeTimer
    //  (asm 0x6FC950): arm / disarm the frame window during which a follow-up
    //  airstrike may be requested.  Start also zeroes the generation counter.
    void StartAirstrikeTimer(int32 duration);
    void StopAirstrikeTimer();

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

    // Layer flag at +0x74: true while this object occupies a tunnel, bridge or
    // similar non-ground layer.  Consulted by IsOnFloor / IsInAir.
    bool          Tunnel;
    uint8         pad_Tunnel[3];

    // Turret-slot bookkeeping.  ActiveTurretIndex addresses +0x124 (the slot
    // the unit is currently drawn with) and CurrentWeaponNumber addresses
    // +0x134; -1 means "no weapon selected".
    int32         ActiveTurretIndex;
    int32         CurrentWeaponNumber;

    // House that owned this object before it was captured.  Only meaningful
    // while Captured is set.  (asm +0x2E0)
    HouseClass*   OrigOwner;
    bool          Captured;

    // ── Disguise / warp / drain state ─────────────────────────────────────
    bool          IsDisguisedFlag;    // +0x81 style disguise flag
    int32         WarpInTimer;        // remaining frames of a chrono-warp-in
    int32         WarpOutTimer;       // remaining frames of a chrono-warp-out
    bool          IsWarpingOutFlag;   // warp-out has completed
    int32         DrainTimer;         // remaining frames of a drain effect

    // Disguise blinking.  `DisguiseCreationFrame` (+0x1DC) remembers when the
    // disguise was taken; `DisguiseBlinkTimer` (+0x1EC / +0x1F4) drives the
    // periodic flicker that tips the player off.  GetDisguiseFlags and
    // IsDisguisedAgainst read both.
    int32         DisguiseCreationFrame;
    CDTimerClass  DisguiseBlinkTimer;

    // Planning-token slot: -1 when the object has no waypoint assigned.
    int32         PlanningToken;

    // Multi-stage weapon counter at +0x140 (gattling / prism style weapons).
    int32         WeaponStage;

    // The building this techno is currently focused on (asm FocusOnUnit).
    BuildingClass* FocusOnUnit;

    // Primary facing (+0x388).  Where the object points.  FootClass keeps its
    // own copy for the locomotor; this is the location the binary's
    // Desired_Facing256 reads for structures and turreted units.
    DirStruct     PrimaryFacing;

    // Height used by the on-floor / in-air tests (+0x68 in the original's
    // type block, mirrored here).  Zero for ground vehicles and structures.
    int32         GroundHeight;

    // Index of a voice line queued for playback, or -1.
    int32         QueuedVoiceIndex;

    // The locomotor COM object driving this techno's every-frame movement
    // (asm +0x674).  May be null for statics such as buildings.
    ILocomotion*  Locomotor;

    // Secondary warhead-effect state. These timers/counters are decremented
    // by Update_AI each frame and consulted by the damage / rendering code.
    int32         FireDamageTimer;    // frames remaining while burning
    int32         SparkyCounter;      // pending spark particle spawns
    bool          IsParasited;        // a parasite is attached to this techno
    int32         TemporalTimer;      // frames frozen by the chronosphere weapon

    // The temporal weapon object this techno is currently applying (asm
    // TemporalImUsing), or null.  IsTemporalSource tests both this and its
    // victim slot.
    void*         TemporalImUsing;
    int32         GasTimer;           // frames affected by gas
    int32         RadiationTimer;     // frames irradiated by a rad warhead

    // ========================================================================
    // Ammunition & reloading (asm currentAmmo / ReloadTimer)
    // ========================================================================
    // CurrentAmmo (+0x[+], asm TechnoClass.currentAmmo): rounds remaining.
    //   Reload adds one round once ReloadTimer has elapsed, up to the type's
    //   Ammo ceiling (-1 = infinite, in which case Reload does nothing).
    int32         CurrentAmmo;
    CDTimerClass  ReloadTimer;

    // ========================================================================
    // Airstrike timer (asm TechnoClass.StartAirstrikeTimer / StopAirstrikeTimer)
    // ========================================================================
    // The airstrike window is a plain frame stamp plus a generation counter,
    // touched by the two helpers below rather than by a real countdown.
    int32         AirstrikeTimeStart;
    int32         AirstrikeTimeLeft;
    int32         AirstrikeTimeGen;

protected:
    explicit TechnoClass(noinit_t) noexcept : ObjectClass(noinit) {}
};
