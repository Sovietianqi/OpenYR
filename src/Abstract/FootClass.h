#pragma once

#include "TechnoClass.h"
#include "../Math/Facing.h"
#include "../Containers/DynamicVectorClass.h"

class LocomotionClass;

class FootClass : public TechnoClass {
public:
    static const AbstractType AbsID = AbstractType::Foot;

    static DynamicVectorClass<FootClass*>* Array;

    FootClass() noexcept : TechnoClass(), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr) {}
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

    DirStruct PrimaryFacing;
    DirStruct TurretFacing;
    int32 Pitch;
    Sequence CurrentSequence;
    DynamicVectorClass<CoordStruct> Path;
    LocomotionClass* Locomotion;

protected:
    explicit __forceinline FootClass(noinit_t) noexcept : TechnoClass(noinit), Pitch(0), CurrentSequence(Sequence::Ready), Locomotion(nullptr) {}
};
