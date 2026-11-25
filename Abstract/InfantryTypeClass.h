#pragma once

#include <Abstract/TechnoTypeClass.h>

class HouseClass;

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
    static int32 FindIndex(const char* pID);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    virtual ~InfantryTypeClass();
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual AbstractType WhatAmI() const override { return AbstractType::InfantryType; }
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

    // asm: InfantryTypeClass_GetRepairStep / GetRepairStepCost
    virtual int32 GetRepairStep() const override;
    virtual int32 GetRepairStepCost() const override;

    // asm: InfantryTypeClass_SpawnAtCoords (InfantryTypeClass vftable slot)
    virtual InfantryClass* SpawnAtCoords(const CoordStruct& coord, HouseClass* pOwner);

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

    // Factory: construct an InfantryClass of this type owned by pOwner and wire
    // up its Type back-pointer.  Mirrors InfantryTypeClass_CreateInfantry.
    InfantryClass* CreateInfantry(HouseClass* pOwner) const;

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
    // 注意：Weapons / EliteWeapons / WeaponCount 全部继承自 TechnoTypeClass
    //   （原版只有一份 18 槽武器表）。这里曾经重复声明过一份 2 槽的表，
    //   结果 Read_INI 写基类那份、战斗代码读子类这份，两边永远对不上：
    //   所有步兵/载具的 GetWeaponRange 返回 0、自动索敌半径为 1 格、
    //   有武器的单位也永远打不出子弹。
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

    // ------------------------------------------------------------------------
    // 项目 Sequence 枚举 → 原版 Sequences 表记录索引
    //
    //   artmd.ini 的序列记录按原版表顺序保存（见 g_SequenceNames），而项目
    //   的 Sequence 枚举是"步兵 + 载具"合并的自定义顺序，两者从第 6 项起
    //   就分叉（原版 6 = Crawl，项目 6 = FireProne）。任何按记录索引取
    //   StartFrame / FrameCount 的地方都必须先过这一层映射。
    //
    //   asm Sequences 表（.data:0x8B7xxx）：
    //     0 Ready   1 Guard   2 Prone   3 Walk    4 FireUp  5 Down
    //     6 Crawl   7 Up      8 FireProne 9 Idle1  10 Idle2 11..15 Die1..Die5
    //     16 Tread  17 Swim   18 WetIdle1 19 WetIdle2 20 WetDie1 21 WetDie2
    //     22 WetAttack 23 Hover 24 Fly 25 Tumble 26 FireFly
    //     27 Deploy 28 Deployed 29 DeployedFire 30 DeployedIdle 31 Undeploy
    //     32 Cheer 33 Paradrop 34 AirDeathStart 35 AirDeathFalling
    //     36 AirDeathFinish 37 Panic 38 Shovel 39 Carry 40 SecondaryFire
    //     41 SecondaryProne
    // ------------------------------------------------------------------------
    static int32 GetSequenceRecordIndex(Sequence seq);

    // One 0x24 byte record per entry of the Sequences table.  The original
    // keeps the block at +SequenceData and straddles it with a 0x24 stride.
    //
    // asm InfantryTypeClass_ParseSequence:
    //   sscanf("…", "%d,%d,%d,%s", &rec[+00], &rec[+04], &rec[+08], facing)
    //   "N".."NW" 比较链写 rec[+0C] = 0..7
    //   音效循环写 rec[+10] = 槽数, rec[+14/+1C] = 帧号, rec[+18/+20] = Voc 索引
    //   （mov ebx,-12 / sub ebx,esi 之后 lea [ebx+ebp] 得到记录基址，
    //     slot*8 分别落在 +14/+18 与 +1C/+20）
    // 合计 9 个 dword = 0x24。
    struct SequenceDataRec
    {
        int32 StartFrame;          // +00  第 1 个 %d
        int32 FrameCount;          // +04  第 2 个 %d
        int32 FramesPerDirection;  // +08  第 3 个 %d
        int32 Facing;              // +0C  "N".."NW" -> 0..7
        int32 SoundCount;          // +10  已填入的音效槽数（上限 2）
        int32 SoundFrame[2];       // +14 / +1C
        int32 SoundIndex[2];       // +18 / +20
    };
    static_assert(sizeof(SequenceDataRec) == 0x24,
                  "SequenceDataRec 必须与 asm 的 0x24 步长一致");

    static constexpr int32 MaxSequences = 42;
    SequenceDataRec SequenceData[MaxSequences];
    // asm: InfantryTypeClass_Save 直接把 [this+0E3Ch] 起 0x5E8 字节整块写入；
    // 本实现的 SequenceData 是内联数组，sizeof 必须与之一致。
    static_assert(sizeof(SequenceData) == 0x5E8,
                  "SequenceData 必须整块 0x5E8 字节（42 条 × 0x24）");

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
    // asm: [this+0E88h] TypeList<int>，由 INIClass_Get_Vector_Voc_525430 从
    // "VoiceComment" 键按 ',' 切分、逐个 VocClass_Find + VocClass_FindIndex
    // 填入。InfantryTypeClass_Save 写 [0E98h] 计数后跟 [0E8Ch] 的 dword 数组。
    DynamicVectorClass<int32> VoiceCommentSounds;
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