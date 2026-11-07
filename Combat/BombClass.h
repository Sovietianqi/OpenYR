#pragma once

#include "../COM/IUnknown.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../IO/CRC.h"

class TechnoClass;
class HouseClass;
class AbstractClass;

// ============================================================================
// BombClass - 定时/死亡炸弹（疯狂伊文的 C4、恐怖分子死亡炸弹）
//
// 权威来源：/workspace/asm_export/BombClass.asm.txt
//
// 结构（原版 sizeof=0x5C，AbstractClass 之后）：
//   +0x24 SourceUnit     布雷者
//   +0x28 OwnerHouse     布雷方所属
//   +0x2C TargetUnit     被布雷单位
//   +0x30 State          0 = 计时中；1 = 引爆/已处置
//   +0x34 PlantingFrame  布雷帧（图标闪烁基准）
//   +0x38 WhenToBlow     引爆帧
//   +0x3C strX           附着名称（字符串）
//   +0x54 field_54       已从 BombList 摘除标志
//   +0x58 IsDeathBomb    已解除（SagedMyself 返回值，asm 的 mostlyHarmless1）
//
// 每个实例在构造时登记进全局 vec_BombList.bombs，由 BombClass::sctor
// 负责初始化该注册表（bombs / detectors 两张表 + arg）。
// ============================================================================
class BombClass {
public:
    BombClass() noexcept;
    ~BombClass();

    // asm: BombClass_SCTOR (asm:21) —— 初始化全局 vec_BombList 注册表
    static void sctor();

    // asm: BombClass_Detonate (asm:88)
    //   入口守卫（无目标 / 已解除 → 直接返回）；
    //   摘掉目标挂载 → 若目标已爆则仅置位；否则取目标坐标施加
    //   IvanWarhead/IvanDamage 的范围伤害、投放 C4 动画，并在目标为
    //   桥修 Hut 时扫描周边 ±2 格触发桥面损毁；最后清空全部引用。
    void Detonate();

    // asm: BombClass_Disarm (asm:328)
    //   拆除：解除目标身上的挂载，清空引用并把 IsDeathBomb 置位。
    void Disarm();

    // asm: BombClass_SagedMyself (asm:378) —— 直接返回 IsDeathBomb
    bool SagedMyself() const;

    // asm: BombClass_GetCurrentFrame (asm:405)
    //   返回炸弹图标的当前帧 0..11（按 IvanTimedDelay / IvanIconFlickerRate 计算）。
    int32 GetCurrentFrame() const;

    // asm: BombClass_TimeToBlow (asm:477)
    //   State==0 且当前帧已越过 WhenToBlow 且尚未解除时才返回 true。
    bool TimeToBlow() const;

    // asm: TechnoClass_Update+8B6 处的逐帧倒计时判定 —— 到点即引爆。
    void Update();
    // 遍历 vec_BombList.bombs 做一遍全局倒计时（等价原版每帧的炸弹扫描）。
    static void AI();

    // asm: vecBombClass_PointerGotInvalid (asm:163144) 的类内形态 ——
    //   失效指针清理：命中布雷者 / 被布雷单位 / 归属阵营时清空引用。
    void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // asm: BombClass_GetCRC (asm:516)
    void ComputeCRC(CRCEngine& crc) const;
    // asm: BombClass_GetClassID (asm:590)
    HRESULT GetClassID(CLSID* pClassID);
    // asm: BombClass_Load (asm:653) —— SwizzleManager 回填三个引用。
    HRESULT Load(IStream* pStm);
    // asm: BombClass_Save —— 落盘三个引用（ID 形式）。
    HRESULT Save(IStream* pStm, BOOL fClearDirty) const;

    // asm: BombClass_GetSize (asm:679) —— 返回 0x5C
    static int32 Get_Size() { return 0x5C; }
    // asm: BombClass_GetAbstractDerivationID (asm:704) —— 返回 0x44
    static int32 Get_Abstract_Derivation_ID() { return 0x44; }

    // 注册表维护（对应 vec_BombList.bombs 的 Add / Remove）
    void Add_To_BombList();
    void Remove_From_BombList();

    // ---- 字段（偏移见上方结构说明） ----
    TechnoClass* SourceUnit;    // +0x24
    HouseClass*  OwnerHouse;    // +0x28
    TechnoClass* TargetUnit;    // +0x2C
    int32        State;         // +0x30
    int32        PlantingFrame; // +0x34
    int32        WhenToBlow;    // +0x38
    char         AttachedName[32]; // +0x3C strX
    bool         field_54;      // +0x54
    bool         IsDeathBomb;   // +0x58

    // vec_BombList.bombs —— 全局在册炸弹表
    static DynamicVectorClass<BombClass*>* BombList;
    // vec_BombList.detectors —— 需要感知炸弹的观察者（BombSight 单位）
    static DynamicVectorClass<TechnoClass*>* Detectors;
    // vec_BombList.arg —— asm 里写死的 0x64
    static int32 BombListArg;
};
