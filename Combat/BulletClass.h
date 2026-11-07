#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/ObjectClass.h"

class BulletTypeClass;

// ============================================================================
// BulletMotion - the four flight models selected by BulletTypeClass.
//
//   The original engine never stores an explicit "motion" enumerator; it
//   dispatches per frame on the type flags (Dropping / Vertical / Arcing /
//   ROT).  OpenYR derives the same value once at launch so the per-frame
//   Update can switch on a single int -- this is semantically identical and
//   makes the four asm branches (Update loc_4670C3 / 4671CA / 467402 /
//   the ROT>0 homing head) explicit.
// ============================================================================
enum class BulletMotion : int32 {
    Straight = 0,   // ROT > 0 : guided line / heat seeker
    Arcing   = 1,   // type->Arcing : mortar arc
    Vertical = 2,   // type->Vertical : straight up, then down
    Dropping = 3,   // type->Dropping : gravity bomb
};

// ============================================================================
// BulletClass - one in-flight projectile (bullet / missile / shell)
//
// Field layout mirrors the original binary.  The asm offsets are noted next to
// each semantic field so the per-frame logic can be read against the listing.
// (Fields do not need a byte-exact layout in this port -- only semantics.)
// ============================================================================
class BulletClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Bullet;

    static DynamicVectorClass<BulletClass*>* Array;
    BulletClass(BulletTypeClass* pType) noexcept;
    virtual ~BulletClass() override;

    virtual AbstractType WhatAmI() const override { return AbstractType::Bullet; }
    virtual int32 Size() const override;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual HRESULT GetClassID(CLSID* pClassID) override;

    // asm: BulletClass_Load (0x46AE80) / BulletClass_Save (0x46B000)
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    // asm: BulletClass_AddRef (0x46B060) / BulletClass_Release (0x46B090)
    virtual ULONG AddRef() override;
    virtual ULONG Release() override;

    // asm: BulletClass_SetLayer (0x4666C0)
    virtual bool Mark_Layer(int32 idxLayer) override;

    // asm: BulletClass_InWhichLayer (0x468C90)
    virtual Layer InWhichLayer() const;

    virtual ObjectTypeClass* GetType() const override;

    // asm: BulletClass_GetSize (0x46B6B0) -- sizeof == 0x160
    int32 Get_Bullet_Size() const;

    // asm: BulletClass_PointerGotInvalid (0x4684E0) -- retn 8 -> 2 args
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed) override;

    // ------------------------------------------------------------------------
    // asm: BulletClass_Update (0x466700) -- the per-frame "AI".
    //
    //   Reconstructed control flow (asm 243943..246416):
    //     1. ObjectClass::Update + "alive" gate ([+0x90]).
    //     2. forced-detonate when the pending trailer anim finished ([+0x158]
    //        MustSpawnNextAnim set and NextAnim == 0).
    //     3. rotation frame counter advanced from type AnimLow/High/Rate
    //        ([+0x2F4/2F5/2F6] into [+0x12C]/[+0x12D]).
    //     4. periodic trailer anim spawn (type +0x2D8, rate +0x2E8/+0x2E4).
    //     5. dispatch on type->ROT:
    //          ROT > 0  -> homing / guided head (aim, arm, steer, collide).
    //          ROT <= 0 -> ballistic head (Straight/Arcing/Vertical/Dropping).
    //     6. cell / floor / bridge collision + occupant techno test.
    //     7. nuke "NUKEBALL" spawn when the warhead is a nuke; otherwise
    //        Detonate(flag) + Destroy().
    //     8. refresh the packed cell coords ([+0x14C]).
    // ------------------------------------------------------------------------
    void Update() override;

    // ---- Update sub-routines (asm inline sections, split for readability) ----
    // asm 246.. (Update loc_4667B2..466808): advance [+0x12C]/[+0x12D].
    void Advance_Rotation();
    // asm (Update loc_466808..4668BD): spawn the +0x2D8 trailer animation.
    void Spawn_Trailer();
    // asm (Update loc_4670C3..): the ROT<=0 ballistic integration head.
    //   Returns the hit flag: 0 = still flying, 1 = hit, 2 = left the map.
    int32 Simulate_Ballistic(bool& preciseHit, bool& mustDetonate);
    // asm (Update loc_4668BD..4670C3): the ROT>0 homing head.
    int32 Simulate_Homing(bool& preciseHit, bool& mustDetonate);
    // asm (Update loc_467A2B..467B7A): floor / cell / occupant collision.
    //   Returns the hit flag (0/1/2); `exactHit` marks a precise collision.
    int32 Resolve_Ballistic_Hit(const CoordStruct& prev, const CoordStruct& next,
                                bool& exactHit);
    // asm 247553..247785 (sub_468BB0): ground / cliff / wall grounding test;
    //   snaps `pos` onto the struck object/floor when it returns true.
    bool Check_Terrain_Impact(CoordStruct& pos);
    // asm 244809.. (Update loc_46695B..loc_4669B9): homing step + fuse arming.
    int32 Compute_Homing_Step();
    // asm 245520.. (Update loc_467B7A..loc_467F9B): post-step decision ladder.
    void Resolve_Step_End(int32 hitFlag, bool& preciseHit, bool& mustDetonate);
    // asm 245936.. (Update loc_467E53..loc_467F36): the nuke "NUKEBALL" spawn.
    bool Spawn_NukeBall();
    // asm (Update loc_467FBA..): refresh the packed cell coords.
    void Commit_Position(const CoordStruct& next);
    // asm: the cell flag word (cell+0x140) & 0x100 wall / bridge-adjacent test.
    bool Cell_Is_Bridged(const CoordStruct& loc) const;

    // ---- detonation chain ---------------------------------------------------
    // asm: BulletClass_Detonate (0x468D50), retn 4 -> one bool arg.
    void Detonate();
    void Detonate(bool a2);
    // asm: BulletClass_Detonate loc_468DC1..468FF4 -- pick the true impact
    //   point (snap onto the target when close, honour the fuse otherwise).
    CoordStruct Resolve_Impact_Point(bool a2) const;
    // asm: BulletClass_Fire (0x4690A0) -- the warhead "explode" work-horse.
    void Explode(const CoordStruct& loc);
    // asm: BulletClass_Fire loc_469130..lct_RadiationEnd -- RadSiteClass spawn.
    void Apply_Radiation(const CoordStruct& loc);
    // asm: BulletClass_Fire lct_NoMindControl..loc_469AA4 -- the warhead's
    //   special-effect ladder (mind control / Ivan / electric / parasite /
    //   temporal / locomotor / airstrike / direct-rocker).
    void Apply_Warhead_Special(const CoordStruct& loc);
    // asm: BulletClass_Fire loc_4698xx.. -- cell-spread splash animation.
    void Spawn_Splash_Anim(const CoordStruct& loc);
    // asm: BulletClass_Detonate loc_468FF4..4690A6 -- cluster sub-munitions.
    void Spawn_Cluster_munitions(const CoordStruct& loc);

    // asm: BulletClass_Shrapnel (0x46A340)
    void Shrapnel();
    // asm: Shrapnel loc_46A34D..loc_46A3D2 -- resolve type->ShrapnelCount
    //   (+0x2B8), turning a negative "ring radius" into a distance-based count.
    int32 GetShrapnel_Count() const;
    // asm: Shrapnel loc_46A..: create + Initialize + Place one sub-munition.
    void Spawn_Shrapnel_Bullet(const CoordStruct& spawn, const CoordStruct& vel,
                               WeaponTypeClass* pWeapon);

    // ------------------------------------------------------------------------
    // asm: BulletClass_SetMovement (0x468AF0) -- bool retn 8 (2 args).
    //   Places the projectile (ObjectClass::Unlimbo), seeds the velocity vector
    //   ([+0xE8]) and, for the flat-tracer/inaccurate types, scatters the
    //   launch point and normalises the direction.  Returns false if Unlimbo
    //   fails.
    // ------------------------------------------------------------------------
    bool SetMovement(CoordStruct xyz, CoordStruct velocity, int32 speed);

    // asm: BulletClass_NukeMaker (0x46B590)
    void NukeMaker();

    // asm: BulletClass_TargetWentAway (0x4683F0)
    void TargetWentAway();
    // asm: PointerGotInvalid / TargetWentAway shared retarget-to-cell tail.
    void Retarget_To_Cell(bool keepAsCell);

    // asm: BulletClass_Draw (0x467F60) -- __thiscall(this, Point2D* pos,
    //   Rectangle* bounds), retn 8。pRect 是 CC_Draw_Shape 的裁剪矩形。
    void Draw(Point2D* pCoord, RectangleStruct* pRect);

    // asm: BulletClass_DrawVXL (0x46B090) -- 把类型的体素模型（BodyVXL/HVA）
    //   按本帧朝向光栅化到战术面。落点由 Draw 里算好传入，pClip 为裁剪矩形。
    void DrawVXL(Point2D* pScreenPos, const RectangleStruct* pClip);

    // asm: BulletClass_GetAnimRate (0x467D20)
    int32 GetAnimRate() const;
    // helper: pitch-driven sprite frame from the velocity vector (asm 466CD..).
    int32 Compute_Pitch_Frame() const;

    // asm: BulletClass_InitScalable (0x46B250)
    void InitScalable();

    // asm: BulletClass_IsHoming_obsolete (0x46B070) -- type->ROT > 0
    bool IsHoming_obsolete() const;

    // asm: BulletClass_Initialize (0x466400)
    //   Fills the bullet's combat/drawing state from a type + launch params.
    void Initialize(BulletTypeClass* pType, CoordStruct pos, TechnoClass* pOwner,
                    int32 damage, WarheadTypeClass* pWarhead, int32 speed,
                    bool bright);

    // Convenience reset overload kept for the rest of the tree.
    void Initialize(BulletTypeClass* pType);

    // Retained convenience initialiser used by GlobalFiring / TechnoClass.
    void Init(BulletTypeClass* pType, CoordStruct source, CoordStruct target,
              TechnoClass* pOwner, int32 damage, WarheadTypeClass* pWarhead,
              WeaponTypeClass* pWeapon);

    // asm: BulletClass_SubCTOR (invoked from BulletClass_CTOR+0x23)
    void SubCTOR();

    void Destroy();

    // Accessors kept for the rest of the tree.
    // asm: BulletClass_SetTarget (0x46B7A0) -- [this+0x10C] = TechnoClass*
    void SetTarget(TechnoClass* pTarget);
    TechnoClass* GetTarget() const;
    void SetOwner(TechnoClass* pOwner);
    TechnoClass* GetOwner() const;
    CoordStruct GetLocation() const;
    // asm: BulletClass vtable +0x48 -- returns a copy of the in-flight
    //   position ([+0x9C]); the projectile's own coordinate getter.
    virtual CoordStruct* GetCoords(CoordStruct* pCrd) const override;
    // asm: BulletClass_GetZ -- the Z of the in-flight position.
    int32 GetZ() const;
    bool IsActive() const override;
    bool IsDetonated() const;
    int32 GetDamage() const;
    int32 GetDamageMultiplier() const;
    bool IsBulletGravity() const;
    bool IsBulletInaccurate() const;
    bool IsTargetingAA() const;
    bool IsTargetingAG() const;
    BulletTypeClass* GetBulletType() const;
    BulletMotion GetMotion() const;
    FacingType GetFacing() const;
    void SetFacing(FacingType facing);
    int32 GetSpeed() const;
    void SetSpeed(int32 speed);
    void MoveTo(CoordStruct location);
    void SetRange(int32 range);
    void SetWeaponType(WeaponTypeClass* pWeapon);
    WeaponTypeClass* GetWeaponType() const;

    // asm: BulletType_CreateBullet path -- factory used by weapons.
    static BulletClass* Fire(BulletTypeClass* pType, WeaponTypeClass* pWeapon,
                             CoordStruct source, CoordStruct target,
                             TechnoClass* pOwner, int32 damage,
                             WarheadTypeClass* pWarhead);

private:
    bool CanHit(TechnoClass* pTarget) const;

public:
    // ---- scalar / combat state -------------------------------------------------
    int32 Damage;                    // asm: this+0x6C
    int32 Strength;                  // legacy alias of Damage (asm +0x6C)
    int32 Health;                    // arm / durability channel (asm type->Arm)
    int32 DamageMultiplier;          // asm: BulletClass.DamageMultiplier (DirectRocker)
    BulletTypeClass* Class;          // asm: this+0xAC
    TechnoClass* Owner;              // asm: this+0xB0
    TechnoClass* Target;             // asm: this+0x10C
    WarheadTypeClass* Warhead;       // asm: this+0x128
    WeaponTypeClass* WeaponType;     // asm: this+0x130

    // ---- lifecycle flags -------------------------------------------------------
    bool IsBulletActive;             // asm: this+0x90  ("alive")
    bool IsBulletDetonated;          // set by Detonate
    bool IsInAir;                    // asm: this+0x8C
    bool IsIncoming;                 // asm: this+0x8D
    bool IsAnimating;                // project-side anim enable (GlobalFiring)
    bool IsArmed;                    // asm: this+0x104
    bool IsHoming;                   // asm: this+0x105 -- homing/seek active
    bool MustSpawnNextAnim;          // asm: this+0x158 -- force detonate next frame
    bool Bright;                     // asm: BulletClass.Bright
    bool IsGravity;                  // derived from type->Dropping
    bool IsInaccurate;               // derived from type->Inaccurate
    bool IsAA;                       // derived from type->AA
    bool IsAG;                       // derived from type->AG
    bool IsAS;                       // derived from type->ASW
    bool IsFlat;                     // derived from type->Flat (draw layer)

    // ---- trajectory state ------------------------------------------------------
    double Velocity[3];              // asm: this+0xE8,+0xF0,+0xF8 (dir vector)
    int32  Speed;                    // asm: this+0x110
    int32  ArmCounter;               // asm: this+0x108
    int32  Unknown_100;              // asm: this+0x100
    int32  ColorScheme;              // asm: this+0x114 (-1 = none)
    int32  TrackFrames;              // asm: this+0x118
    double TrackAccum;               // asm: this+0x120
    uint32 Timer;                    // asm: this+0xB8 Timer (fuse/arm)
    uint32 Timer2;                   // asm: this+0xC4 Timer (trail)
    uint8  AnimFrame;                // asm: this+0x12C
    uint8  AnimDelay;                // asm: this+0x12D
    int32  Unknown_150;              // asm: this+0x150 (0x100 after Initialize)

    BulletMotion Motion;             // derived flight model (type flags)

    CoordStruct Location;            // asm: this+0x9C
    CoordStruct StartCoords;         // asm: 子弹发射起点快照（轨迹/绘制用）
    CoordStruct SourceCoords;        // asm: this+0x134
    CoordStruct FuseCoords;          // asm: this+0x140
    CoordStruct TargetCoords;        // project-side cached target position
    CoordStruct LastCoords;          // project-side previous position

    uint16 CellX;                    // asm: this+0x14C (packed cell X)
    uint16 CellY;                    // asm: this+0x14C+2 (packed cell Y)

    // asm: BulletClass.NextAnim (ptr, swizzled on Load)
    AbstractClass* NextAnim;

    int32 Range;                     // convenience range cap for the port
    int32 DistanceTraveled;          // convenience travelled-lepton counter
    int32 CreationFrame;             // project-side birth frame
    FacingType Facing;               // project-side cached facing

    BulletClass* Next;
    BulletClass* Prev;
};
