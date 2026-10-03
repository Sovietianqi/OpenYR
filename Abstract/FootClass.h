#pragma once

#include "TechnoClass.h"
#include "../Math/Facing.h"
#include "../Containers/DynamicVectorClass.h"

class LocomotionClass;
class TeamClass;

class FootClass : public TechnoClass {
public:
    static const AbstractType AbsID = AbstractType::Foot;

    static DynamicVectorClass<FootClass*>* Array;

    FootClass() noexcept : TechnoClass(), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr),
        Team(nullptr), NextTeamMember(nullptr), IsTeamLeader(false), Recruitable(false), Group(-1) {}
    virtual ~FootClass() {}

    virtual AbstractType WhatAmI() const override { return AbstractType::Foot; }
    virtual int32 Size() const override { return sizeof(FootClass); }

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(FootClass* pInstance);
    static bool Remove_From_Array(FootClass* pInstance);
    static int32 Get_Total_Count();
    static FootClass* Get_Instance(int32 index);
    static int32 Find_Index(FootClass* pInstance);

    // ========================================================================
    // Path management
    // ========================================================================
    bool Has_Path() const;
    int32 Get_Path_Length() const;
    CoordStruct Get_Path_At(int32 index) const;
    void Set_Path(const CoordStruct* pCoords, int32 count);
    void Append_Path(const CoordStruct& coord);
    void Clear_Path();
    CoordStruct Peek_Next_Path() const;
    CoordStruct Pop_Next_Path();

    // ========================================================================
    // Facing management
    // ========================================================================
    void SetFacing(DirStruct facing);
    DirStruct GetFacing() const;
    void SetTurretFacing(DirStruct facing);
    DirStruct GetTurretFacing() const;
    void SetPitch(int32 pitch) { Pitch = pitch; }
    int32 GetPitch() const { return Pitch; }

    // ========================================================================
    // Sequence
    // ========================================================================
    void SetSequence(Sequence seq) { CurrentSequence = seq; }
    Sequence GetSequence() const { return CurrentSequence; }

    // ========================================================================
    // Coordinate management
    // ========================================================================
    void SetCoords_Impl(const CoordStruct& coord);
    void SetCoords(const CoordStruct& coord);
    CoordStruct GetCoords_Impl() const;

    // ========================================================================
    // Locomotion interface delegation
    // ========================================================================
    void Set_Locomotion(LocomotionClass* pLoco);
    LocomotionClass* Get_Locomotion() const;
    bool Is_Moving() const;
    void Stop_Moving();
    void Move_To(const CoordStruct& coord);
    CoordStruct Get_Destination() const;

    // ========================================================================
    // Update loop for movement
    // ========================================================================
    virtual void Update() override;
    void Update_Movement();

    // ========================================================================
    // Misc inline compatibility methods preserved from the original header
    // ========================================================================
    void SetAlpha(uint8 /*alpha*/) {}
    void PlaySoundEffect(int32 /*soundId*/) {}
    void TakeDamage(int32 /*damage*/, ObjectClass* /*source*/, WarheadTypeClass* /*warhead*/) {}

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ==========================================================================
    // Mission-controller overrides (asm 0x41B5xx block of the FootClass vtable)
    //
    //  FootClass supplies the neutral bodies for the movement / morale slots
    //  so that vehicles, infantry and aircraft that do not implement a given
    //  behaviour still occupy the correct vtable entry.
    // ==========================================================================
    // FootClass_Panic (asm 0x5F3Dxx): bare retn at this layer.
    virtual void Panic();
    // FootClass_Unpanic: bare retn.
    virtual void Unpanic();
    // FootClass_PlayIdleAnim (asm 0x41B60x): retn 4 - derived types play their
    // own idle sequence.
    virtual void PlayIdleAnim(int32 a2);
    // FootClass_Draw (asm 0x41B6xx): retn 8.
    virtual void Draw(int32 a2, int32 a3, int32 a4);

    // ========================================================================
    // Team / layer probes
    // ========================================================================
    // FootClass_PartOfTeam (asm 0x4D4A30): true when this unit belongs to a
    // team (FootClass::Team != null).
    bool PartOfTeam() const;
    // FootClass_InAir (asm 0x4D4A40): thunk to the TechnoClass air-layer test.
    bool InAirLayer() const;
    // FootClass_CanAttack (asm 0x4D4A48): thunk to the type's CanMobileAttack
    // slot (TechnoClass_4C0 -> GetTechnoType()->CanMobileAttack()).
    bool CanAttack() const;

    // ========================================================================
    // Movement helpers
    // ========================================================================
    bool Set_Destination(const CoordStruct& dest);
    bool Can_Enter_Cell(const CellStruct& cell) const;
    void Scatter(const CoordStruct& from, bool ignoreMission = true);

    DirStruct PrimaryFacing;
    DirStruct TurretFacing;
    int32 Pitch;
    Sequence CurrentSequence;
    DynamicVectorClass<CoordStruct> Path;
    LocomotionClass* Locomotion;

    // ========================================================================
    // Team membership links
    //
    //  The AI team system threads every member of a team together.  The
    //  original stores the owning team at FootClass+0x5D0 ("PartOfTeam",
    //  reached as _FootClass+0x5D4), the next member in the chain at +0x5D4
    //  ("NextUnitInTeam", _FootClass+0x5D8) and the leader flag at +0x685
    //  ("Team_Leader", _FootClass+0x689).  TeamClass::RecruitUnit fills these
    //  in; TeamClass::Remove clears them.
    // ========================================================================
    TeamClass* Team;            // +0x5D0  owning team (null when unattached)
    FootClass* NextTeamMember;  // +0x5D4  next unit on the team's member chain
    bool       IsTeamLeader;    // +0x685  true when this unit leads its team

    // The per-unit "recruitable" byte at +0x421 and the current group id the
    // team hands out at +0x214.
    bool       Recruitable;
    int32      Group;

protected:
    explicit __forceinline FootClass(noinit_t) noexcept : TechnoClass(noinit), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr),
        Team(nullptr), NextTeamMember(nullptr), IsTeamLeader(false), Recruitable(false), Group(-1) {}
};
