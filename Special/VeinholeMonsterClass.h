#pragma once

#include "../COM/IUnknown.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class BlitterClass;
class DSurface;
class CellClass;
struct IStream;

static constexpr int32 MAX_VEINHOLES = 16;

class VeinholeMonsterClass {
public:
    VeinholeMonsterClass();
    ~VeinholeMonsterClass();

    // 根据游戏行为，可知巨噬 monsters 的初始化分两段：先立骨架
    // 再铺触须；细胞查找沿所在格与邻格两路进行。
    void Initialize1(const CoordStruct& pos);
    void Initialize2(int32 radius, int32 lifetime);

    // asm: VeinholeMonsterClass_FindInCell (asm:1719272) —— 返回落在该格的怪物。
    static VeinholeMonsterClass* FindInCell(const CellStruct& cell);
    // asm: VeinholeMonsterClass_FindWhat (asm:906) —— 按生长数据占据判定。
    static VeinholeMonsterClass* FindWhat(const CellStruct& cell);

    // asm: VeinholeMonsterClass_DeleteAll (asm:1583) —— 销毁全部怪物并清表。
    static void DeleteAll();

    // asm: VeinholeMonsterClass_Draw_All (asm:1399) —— 逐个体绘制。
    void Draw_All(BlitterClass* blitter, DSurface* surface);
    // asm: VeinholeMonsterClass_LoadArt (asm:1439) —— 按剧场加载 "VEINHOLE.%s"。
    void LoadArt(int32 theatre);
    // asm: VeinholeMonsterClass_ReceiveDamage (asm:1489) —— 受伤状态迁移。
    int32 ReceiveDamage(int32 damage);
    // asm: VeinholeMonsterClass_GetType (asm:2305) —— 类型/美术指针。
    void* GetType() const { return TypePtr; }
    // asm: VeinholeMonsterClass_GetSize (asm:2405) —— 0x108
    static int32 Get_Size() { return 0x108; }
    // asm: VeinholeMonsterClass_GetAbstractDerivationID (asm:2378)
    static int32 Get_Abstract_Derivation_ID() { return 0x42; }
    // asm: VeinholeMonsterClass_GetClassID (asm:2336)
    HRESULT GetClassID(CLSID* pClassID);
    // asm: VeinholeMonsterClass_Save (asm:2269)
    HRESULT Save(IStream* pStm, BOOL fClearDirty);

    // ------------------------------------------------------------------------
    // asm: LoadGame_LoadVeinholes (0x74EA80 区, thiscall, returns bool)
    //   先删掉全部在场怪物并清空 vec_VeinholeMonsters，再按存档块流式重建：
    //   区域格数、VeinholeGrowthData 原始块，然后每个 id 一只 0x108 字节的
    //   怪物（Swizzle 重绑 + LoadTables + 0xB0/0xB4/0xC4 的计数/数组字段
    //   回填，逐项 base+offset 修正），最后追加进 vec_TechTypeLinks。
    // ------------------------------------------------------------------------
    static bool Load_All(IStream* pStm);

    void Initialize(const CoordStruct& pos, int32 radius, int32 lifetime);
    void Release();
    void ReleaseVeinCells();

    // ------------------------------------------------------------------------
    // 生长账簿 —— Setup_Veins / sub_74E8A0 直接按偏移访问的三块
    //   +0xB0  VeinGrowthCounter*  每格一条记录的二叉最小堆（原版 CounterClass，0x14）
    //   +0xB4  VeinGrowthEntry*    MaxGrowth 条 8 字节的生长区间表
    //   +0xC4  uint8*              areaSize 字节的"已长成"标记缓冲
    // asm CounterClass 布局：+0 Count / +4 Max / +8 Entry** Array /
    //   +0xC 已登记条目地址上界 / +0x10 已登记条目地址下界（初值 -1）
    // ------------------------------------------------------------------------
    struct VeinGrowthEntry {
        CellStruct Cell;   // +0x00 条目格坐标（asm 比较的是这个 dword 的两个 word）
        float      Key;    // +0x04 排序键 = Frame/50 + |rand%50| + 1
    };

    struct VeinGrowthCounter {
        int32             Count;   // +0x00 堆内条目数（1 基，Array[0] 不用）
        int32             Max;     // +0x04 容量 = RulesData.MaxVeinholeGrowth
        VeinGrowthEntry** Array;   // +0x08 new(4 + Max*4) 的 1 基指针数组
        VeinGrowthEntry*  MaxPtr;  // +0x0C 已登记条目的地址上界
        VeinGrowthEntry*  MinPtr;  // +0x10 已登记条目的地址下界

        // asm: sub_5AD870 —— 从 index 处向下沉降（最小堆，比较条目 Key）。
        void Sift_Down(int32 index);
    };

    // asm: sub_74E8A0 —— 拆掉三块生长账簿（+0xB0 连同其 Array、+0xB4、+0xC4），
    //   并把 +0xAC 清零。
    void Release_Growth_State();

    // asm: Setup_Veins loc_74DEF5 段 —— 按 maxGrowth 重建三块生长账簿：
    //   +0xB4 = new(maxGrowth*8)            （成功后 MaxGrowth-1 只写局部变量）
    //   +0xB0 = new CounterClass{Count=0, Max=maxGrowth, Field10=-1,
    //                            Array=new(4+maxGrowth*4)}，Array 逐项清零
    //   +0xC4 = new(areaSize)，整块清零；再按 Count 把 Array 清零并把 Count 归零
    void Allocate_Growth_State(int32 maxGrowth, int32 areaSize);

    // ------------------------------------------------------------------------
    // asm: char __thiscall Remove_Matching(VeinholeMonsterClass *this,
    //                                      CellClass *cell)    (0x74EF20, retn 4)
    //
    //   某格矿脉叠加物（7Eh，生长阶段 >= 30h）消失时的账簿修正：
    //     1) 先按四个正交邻格算出"待摘除"标记（邻格仍是 7Eh 但阶段不在
    //        [30h..33h] 的算命中）；
    //     2) 重接本格矿脉贴图链（CellClass::Relink_Vein_Overlay），
    //        并把本怪物的已长成格计数 +0x104 减一；
    //     3) 对每个"空（-1）"的正交邻格清掉生长标记表的两位，若它在
    //        第 1 步命中且本怪物尚未长成（+0x100 == 0），再从 +0xB0 的
    //        最小堆里按格坐标摘掉该条目（末项填坑 + 上浮 + 沉降 + 自检）；
    //     4) 收尾把本格的坐标按 Key = Frame/50 + |rand%50| + 1 重新入堆、
    //        入表，并给两张标记表置 1。
    //   asm 的返回值恒为 eax 残留（调用点全部忽略），此处统一返回 false。
    // ------------------------------------------------------------------------
    bool Remove_Matching(CellClass* pCell);

    // ------------------------------------------------------------------------
    // asm: byte __fastcall sub_42B1C0(wXY *this)（0x42B1C0）
    //
    //   等距格坐标 → 全图生长标记表（VeinholeGrowthData / +0xC4 缓冲）的线性
    //   下标：idx = (X + Y - 1) * MapHeight + (X - Y + MapHeight - 1) / 2。
    //   （asm 里读的 dword_87F8DC 就是本项目 MapClass::MapHeight，
    //     见 MapClass_GetArea = (87F8E0 + 4) * 87F8DC * 2 的对应关系。）
    // ------------------------------------------------------------------------
    static int32 Growth_Index(const CellStruct& cell);

    void Update();
    void GrowVeins();
    void PlaceVeinCell(const CellStruct& cell);
    void UpdateTactiles();
    void ApplyDamage();
    void ApplyVeinDamageToCell(const CellStruct& cell);
    void SpawnUnit();
    void StartDeath();
    void UpdateDeath();
    void RemoveAllVeinCells();
    void RenderTactiles(BlitterClass* blitter, DSurface* surface);

    void SetGrowthRate(int32 rate);
    void SetMaxRadius(int32 radius);
    void SetLifetime(int32 lifetime);
    void SetSpawnInterval(int32 interval);
    void SetMaxSpawnCount(int32 count);
    void SetBuildingDamage(int32 damage);
    void SetUnitDamage(int32 damage);
    void SetTactileCount(int32 count);
    void SetPosition(const CoordStruct& pos);
    void SetSpawning(bool spawning);
    void SetTargetBuilding(int32 buildingID);

    int32 GetCurrentGrowthRadius() const;
    int32 GetLifeTimer() const;
    int32 GetHealth() const { return Health; }
    int32 GetState() const { return MState; }
    bool IsActiveMonster() const;
    bool IsMonsterDying() const;

    // vec_VeinholeMonsters —— 全局在册怪物表
    static DynamicVectorClass<VeinholeMonsterClass*>* Array;

    CoordStruct Position;
    int16 CellX;                  // asm +0xF8 wXY.x
    int16 CellY;                  // asm +0xFA wXY.y
    int32 Health;                 // asm 生命值（0 表示已死）
    int32 MState;                 // asm +0xC8 状态机 0..3
    int32 StateTimerStart;        // asm +0xECh
    int32 StateTimerLeft;         // asm +0xF4
    int32 GrowthLevel;            // asm +0xD0 生长等级
    void* TypePtr;                // asm +0x6C 类型/美术
    char  ArtName[64];            // asm LoadArt 拼出的 "VEINHOLE.%s"
    int32 MaxGrowthRadius;
    int32 CurrentGrowthRadius;
    int32 GrowthRate;
    int32 VeinCount;
    int32 TactileCount;
    int32 MaxTactileCount;
    int32 TactileBaseLength;
    int32 TactileLength;
    int32 TactileMaxLength;
    float TactileAngleOffset;
    float TactileAnimationPhase;
    float TactileAnimationSpeed;
    bool IsActive;
    bool IsSpawning;
    int32 SpawnTimer;
    int32 SpawnInterval;
    int32 SpawnCount;
    int32 MaxSpawnCount;
    int32 DamagePerTick;
    int32 DamageInterval;
    int32 DamageTimer;
    int32 AttackRange;
    int32 BuildingDamage;
    int32 UnitDamage;
    int32 LifeTimer;
    int32 MaxLifetime;
    bool IsDying;
    int32 DeathTimer;
    int32 DeathDuration;
    CellStruct* VeinCells;
    int32 VeinCellCount;
    // asm 生长账簿（见上面 Release_Growth_State / Allocate_Growth_State）
    VeinGrowthCounter* GrowthCounter;   // asm +0xB0
    VeinGrowthEntry*   GrowthTable;     // asm +0xB4  MaxGrowth 条 8 字节条目
    uint8*             VeinAreaBuffer;  // asm +0xC4
    int32 GrowthTableCount;             // asm +0xAC GrowthTable 的下一个空位下标
    bool  VeinsInitialized;             // asm +0x100 Setup_Veins 选 Initialize1/2 的标记
    int32 GrownCellCount;               // asm +0x104 已长成的矿脉格数
    float* TactileAngles;
    float* TactileExtensions;
    int16 TargetCell_X;
    int16 TargetCell_Y;
    int32 GrowthDirection;
    int32 TargetBuildingID;
    CellStruct VeinDirections[8];
    float VeinGrowth[8];
};

class VeinholeMonsterManagerClass {
public:
    VeinholeMonsterManagerClass();
    ~VeinholeMonsterManagerClass();

    static VeinholeMonsterManagerClass* GetInstance();

    int32 CreateMonster(const CoordStruct& pos, int32 radius, int32 lifetime);
    bool RemoveMonster(int32 index);
    void RemoveAllMonsters();
    void UpdateAllMonsters();
    void RenderAllTactiles(BlitterClass* blitter, DSurface* surface);
    int32 GetActiveCount() const;
    VeinholeMonsterClass* GetMonster(int32 index) const;

    VeinholeMonsterClass* Monsters[MAX_VEINHOLES];
    int32 ActiveCount;
};