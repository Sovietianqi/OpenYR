#pragma once

#include <Abstract/TechnoTypeClass.h>

// ============================================================================
// InfantryTypeClass - type definition for infantry units
// ============================================================================
class NOVTABLE InfantryTypeClass : public TechnoTypeClass {
public:
    static const AbstractType AbsID = AbstractType::InfantryType;
    static DynamicVectorClass<InfantryTypeClass*>* Array;

    // Static lookup / array management
    static InfantryTypeClass* Find(const char* pID);
    static InfantryTypeClass* FindOrAllocate(const char* pID);
    static InfantryTypeClass* FindByIndex(int32 index);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~InfantryTypeClass();
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) const override;
    virtual AbstractType GetAbstractDerivationID() const override;
    virtual bool HasThisID(const char* pID) const override;
    virtual int32 GetCRC() const override;
    virtual int32 Size() const override;
    virtual AbstractType GetClassID() const override;
    virtual const char* get_ID() const override;
    virtual const wchar_t* GetUIName() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual bool IsEngineer() const;
    virtual bool IsThief() const;
    virtual bool IsCow() const;
    virtual bool IsDog() const;
    virtual bool IsBoris() const;
    virtual bool IsArmed() const;
    virtual bool IsMissileSpawn() const;
    virtual bool IsFake() const;
    virtual bool IsDisableable() const;
    virtual bool IsCanBeSuppressed() const;
    virtual bool IsCanBeOccupied() const;
    virtual bool IsCanBeDriven() const;
    virtual bool IsCanBeCaptured() const;
    virtual bool IsCanBeRepaired() const;
    virtual bool IsCanBeSold() const;
    virtual bool IsCanBePowered() const;
    virtual bool IsCanBeDestroyed() const;
    virtual bool IsCanBeDamaged() const;
    virtual bool IsCanBeInfiltrated() const;
    virtual bool IsCanBeSpied() const;
    virtual bool IsCanBeSabotaged() const;
    virtual bool IsCanBeStolen() const;
    virtual bool IsCanBeHijacked() const;

    // Extended infantry accessors
    bool   Is_Civilian() const;
    bool   Is_Spy() const;
    int32  Get_Sequence_Count() const;
    virtual void Resolve_SHP_References() override;
    virtual SHPStruct* Get_Cameo_Data() const override;
    virtual Point2D Get_Image_Size() const override;
    virtual AbstractType Get_Build_Queue_Type() const override;

    InfantryTypeClass() noexcept;
    explicit InfantryTypeClass(noinit_t) noexcept : TechnoTypeClass(noinit) {}

public:
    bool        Engineer;
    bool        Thief;
    bool        Cow;
    bool        Dog;
    bool        Boris;
    bool        IsArmed_;
    bool        IsMissileSpawn_;
    bool        IsFake_;
    bool        IsDisableable_;
    bool        IsCanBeSuppressed_;
    bool        IsCanBeOccupied_;
    bool        IsCanBeDriven_;
    bool        IsCanBeCaptured_;
    bool        IsCanBeRepaired_;
    bool        IsCanBeSold_;
    bool        IsCanBePowered_;
    bool        IsCanBeDestroyed_;
    bool        IsCanBeDamaged_;
    bool        IsCanBeInfiltrated_;
    bool        IsCanBeSpied_;
    bool        IsCanBeSabotaged_;
    bool        IsCanBeStolen_;
    bool        IsCanBeHijacked_;
    bool        IsCrushable;
    bool        IsCrushable2;
    bool        IsTeleporter;
    bool        IsChrono;
    bool        IsBomb;
    bool        IsCow_;
    bool        IsDog_;
    bool        IsBoris_;
    bool        IsTilter;
    bool        IsToProtect;
    bool        IsNominal;
    bool        IsRadarInvisible;
    bool        IsDontScore;
    bool        IsNoThreat;
    bool        IsSensorsSight;
    bool        IsHunterSeeker;
    bool        IsIvan;
    bool        IsLeader;
    bool        IsCarryall;
    bool        IsTrain;
    bool        IsConsideredAircraft;
    bool        IsConsideredVehicle;
    bool        IsSimpleDeployer;
    bool        IsFirebase;
    bool        IsSonic;
    bool        IsVan;
    bool        IsBalloonHover;
    bool        IsCyborg;
    bool        IsNotHuman;
    bool        IsImmuneToPsionics;
    bool        IsImmuneToPoison;
    bool        IsImmuneToRadiation;
    bool        IsImmuneToBerserk;
    bool        IsImmuneToEMP;
    float       ThreatPosedValue;
    int32       DeathWeaponIndex;
    int32       WeaponCharge;
    int32       WeaponCount;
    WeaponStruct Weapons[2];
    int32       EliteWeaponCount;
    WeaponStruct EliteWeapons[2];
    bool        HasTurret;
    bool        CanCloak;
    bool        IsVoxel;
    bool        HasDeployer;
    bool        HasUndeployer;
    bool        HasFirewall;
    Sequence    DeployFireSequence;

    // InfantryTypeClass_ParseSequence: for every entry of the Sequences
    // table, reads "<SequenceName>" out of artmd.ini's section for this
    // infantry and unpacks the "%d,%d,%d,%s" frame triple into the six-slot
    // SequenceData record that starts at SequenceData[0].
    void ParseSequence();
    static const char* GetSequenceName(int32 index);
    static int32 GetSequenceNameCount();

    // One 0x24 byte record per entry of the Sequences table.  The original
    // keeps the block at +SequenceData and straddles it with a 0x24 stride.
    struct SequenceFrame { int32 StartFrame; int32 FrameCount; };
    struct SequenceSound { int32 Frame; int32 SoundIndex; };
    struct SequenceDataRec
    {
        int32           Facing;      // +00
        int32           FrameCount;  // +04
        SequenceFrame   Frames[2];   // +08, +10
        SequenceSound   Sounds[2];   // +18, +20
    };

    static constexpr int32 MaxSequences = 42;
    SequenceDataRec SequenceData[MaxSequences];

    SequenceDataRec& SequenceDataAt(int32 index) {
        return SequenceData[index];
    }
    BYTE        padding_InfantryType[4];
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    int32        Pip;
    int32        OccupyPip;
    WeaponTypeClass* OccupyWeapon;
    WeaponTypeClass* EliteOccupyWeapon;
    char        VoiceComment[0x100];
    AnimTypeClass* DeadBodies;
    AnimTypeClass* DeathAnims;
    bool         Cyborg;
    bool         NotHuman;
    int32 EnterWaterSound;
    int32 LeaveWaterSound;
    bool         Fearless;
    bool         Fraidycat;
    bool         Infiltrate;
    bool         Ivan;
    bool         Occupier;
    bool         Assaulter;
    int32        DetectionDistance;
    int32        HarvestRate;
    bool         C4;
    bool         Civilian;
    bool         TiberiumProof;
    bool         Agent;
    bool         VehicleThief;
    bool         Doggie;
    bool         Deployer;
    bool         DeployedCrushable;
    bool         UseOwnName;
    bool         JumpJetTurn;
    bool         Crawls;
    int32        FireUp;
    int32        FireProne;
    int32        SecondaryFire;
    int32        SecondaryProne;
};