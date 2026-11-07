#pragma once

// =============================================================================
// Special/WaveClass.h - 波束特效类（激光 / 磁波 / 声波族）
//
// 权威来源：asm 片段 /workspace/agent_asm/WaveClass.asm 及主 asm 中
// `WaveClass struc (sizeof=0x240, mappedto_3245)` 的结构定义。
//
// 这是原版真实存在的 WaveClass：一个继承 TechnoClass 的"两点连一线"武器
// 特效对象。它不模拟水面高度，而是承载：
//   * 从枪口(FLH)到目标格的一段光束的几何插值；
//   * 以 Pythagoras / SineCurve 查找表驱动的逐像素颜色混合；
//   * 声波束的逐格伤害节拍；
//   * 由 field_130 倒计时驱动的寿命管理。
//
// Type 分派（switch 4 例）：
//   Type == 0  SonicBeam     —— 青色声波，逐格结算伤害
//   Type == 1  NodLaser      —— 红色细激光（与 2 同类）
//   Type == 2  NodLaser      —— 同上
//   Type == 3  MagneticBeam  —— 紫色磁能波 / 磁电拖曳样
//
// asm 中进程符号与本类成员的对应：
//   WaveClass_CTOR                    <-> WaveClass::WaveClass(FLH,...)
//   WaveClass_DCTOR                   <-> WaveClass::WaveClass()
//   WaveClass_SDDTOR                  <-> WaveClass::~WaveClass
//   WaveClass_Update                  <-> WaveClass::Update
//   WaveClass_Update_Beam             <-> WaveClass::Update_Beam
//   WaveClass_DamageCell              <-> WaveClass::DamageCell
//   WaveClass_CalculateLUTs           <-> WaveClass::CalculateLUTs
//   WaveClass_DrawOldSchool           <-> WaveClass::DrawOldSchool
//   WaveClass_Draw_Green              <-> WaveClass::Draw_Green
//   WaveClass_Draw_Purple             <-> WaveClass::Draw_Purple
//   WaveClass_Draw_NodLaser           <-> WaveClass::Draw_NodLaser
//   WaveClass_Draw_MagneticBeam       <-> WaveClass::Draw_MagneticBeam
//   WaveClass_Draw_MagnetronStyle     <-> WaveClass::DrawMagnetronStyle
//   WaveClass_Draw_SonicBeam          <-> WaveClass::Draw_SonicBeam
//   WaveClass_GetType                 <-> WaveClass::GetType
//   WaveClass_GetSize                 <-> WaveClass::Size
//   WaveClass_GetClassID              <-> WaveClass::GetClassID
//   WaveClass_GetAbstractDerivationID <-> WaveClass::GetAbstractDerivationID
//   WaveClass_PointerGotInvalid       <-> WaveClass::PointerGotInvalid
// =============================================================================

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Math/Facing.h"
#include "../Math/FacingStruct.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Map/CellClass.h"

class HouseClass;
class WeaponTypeClass;
class ObjectClass;

// ============================================================================
// WaveType - 真 WaveClass.Type 的四个 switch 档位
//
// asm: switch_Wave_Draw_Type / switch_Wave_Update_Type 各 4 例。
//   Draw:   [0]=SonicBeam, [1]=NodLaser, [2]=NodLaser, [3]=MagneticBeam
//   Update: 同左；case 1 与 case 2 合并走 NodLaser 分支。
// ============================================================================
enum class WaveType : int32 {
    SonicBeam    = 0,   // 青色声波束（逐格伤害）
    NodLaserA    = 1,   // 方尖碑红色激光
    NodLaserB    = 2,   // 方尖碑红色激光（同上）
    MagneticBeam = 3    // 紫色磁能波 / 磁电拖曳
};

// ============================================================================
// WaveClass - 波束特效对象
// ============================================================================
class WaveClass : public TechnoClass {
public:
    static const AbstractType AbsID = AbstractType::Wave;

    // asm: vec_Waves —— 全局波束池。CTOR 把自己挂进去，SDDTOR 摘除。
    static DynamicVectorClass<WaveClass*>* Array;

    // ========================================================================
    // 构造 / 析构
    // ========================================================================
    //
    // asm: WaveClass_DCTOR —— 默认构造初始化序列：
    //   field_12C=1（存活）、field_130=100（寿命）、field_138/field_140=0、
    //   Type/Target/Owner=0、LaserIntensity=0、Facing_Default，
    //   并把 vec_Cells 建成 capacity 10 的 DynamicVectorClass<CellClass*>。
    WaveClass() noexcept;

    // asm: WaveClass_CTOR —— 主构造：
    //   (FLH, CellCoords, Source, type, Target)。
    //   距离 <= 240 才真正建盘（否则立即 Detach 返回）；
    //   建好后加入 vec_Waves；首次调用触发 CalculateLUTs；
    //   Type==3 走 DrawMagnetronStyle，否则走 DrawOldSchool；
    //   用 sub_75F230(出发像素, 目标像素) 算出 beam 朝向存进 field_1CC；
    //   Unlimbo 后 Type 1/2 置 LaserIntensity=0xA0 并灌初始 4 段坐标。
    WaveClass(const CoordStruct& FLH, const CoordStruct& CellCoords,
              TechnoClass* Source, int32 type, TechnoClass* Target) noexcept;

    // asm: WaveClass_SDDTOR —— 从 vec_Waves 摘除、释放 vec_Cells 存储、
    //   Detach 后销毁。
    virtual ~WaveClass() override;

    // ========================================================================
    // 身份
    // ========================================================================
    // asm: WaveClass_GetType —— 返回 0（分类标识）。
    // 注：TechnoClass 已有非虚的 GetType()(返回 TechnoTypeClass*)，
    //     故此处以 WaveKind() 表达 asm 中该 slot 的"返回 0"语义。
    virtual AbstractType WhatAmI() const override { return AbstractType::Wave; }
    int32 WaveKind() const { return 0; }

    // asm: WaveClass_GetSize —— 返回 0x240。
    virtual int32 Size() const override { return 0x240; }

    // ------------------------------------------------------------------------
    // IsDead / GetLUTs —— 供调用方查询存活与亮度表的便捷访问器
    //
    // asm 里"存活"由 field_12C（激活标记）与 field_130（寿命倒计时）共同
    // 表达：Update 每帧递减 field_130，减到负数即 UnInit。这里把两者合起
    // 来看，供 TechnoClass / WeaponWaveClass 判断波是否已经散掉。
    // ------------------------------------------------------------------------
    bool IsDead() const { return field_12C == 0 || field_130 < 0; }

    // asm: Wave_LUT_* 系列组合出的 32 档亮度表（供绘制层查询）。
    static const int32* GetLUTs();

    // asm: WaveClass_GetClassID —— 返回固定的类 ID。
    virtual HRESULT GetClassID(CLSID* pClassID) override;

    // asm: WaveClass_GetAbstractDerivationID —— 返回 0x2B (43)。
    int32 GetAbstractDerivationID() const { return 0x2B; }

    // ========================================================================
    // 主循环
    // ========================================================================
    // asm: WaveClass_Update —— 按 Type switch 分派：
    //   case 0: Update_Beam；遍历 vec_Cells 逐格 ConvertCoords+DamageCell；
    //           --field_130 <0 -> UnInit 否则 ObjectClass::Update。
    //   case 3: Update_Beam；--field_130 <0 -> UnInit 否则 ObjectClass::Update。
    //   case 1/2: LaserIntensity -= 6；<32 则 UnInit。
    virtual void Update() override;

    // asm: WaveClass_Update_Beam —— 每帧推进光束扩散：
    //   头部进度 field_138 以 0.05 递增，超过 1.0 回绕；
    //   尾部进度 field_140 在 field_12D（到达标记置起后）同理推进；
    //   两者达标后刷新 4 段像素坐标并逐格标记伤害格。
    void Update_Beam();

    // asm: WaveClass_DamageCell(coords) —— 以一个世界坐标定位所属格子：
    //   取 Owner 的 slot-0 武器，拿 Warhead / AmbientDamage；
    //   遍历该格地面的对象链，对非 Owner、Alive、非 InLimbo、Health>0 的
    //   目标调用 ReceiveDamage；随后处理 Overlay（ChainReaction/Wall），
    //   最后按随机数决定是否 MapClass::DestroyCliff。
    void DamageCell(const CoordStruct& coords);

    // asm: WaveClass_CalculateLUTs —— 构建四张全局查找表：
    //   Wave_LUT_Pythagoras  sqrt(i^2+j^2)
    //   Wave_LUT_SineCurve1  12*sin(i/8)+0.49
    //   Wave_LUT_SineCurve2  12*sin(i*0.1)+0.49
    //   Wave_LUT_Linear1     从 0x6E 起步进 8 直到 0xD6
    // 只在 Wave_LUTs_Calculated==0 时执行。
    static void CalculateLUTs();

    // ========================================================================
    // 各风格绘制入口
    // ========================================================================
    //
    // asm: WaveClass_DrawOldSchool —— 经典两段式光束：构建 16 组单位立方体
    //   顶点表（±31/±100/±30/±27/±34/±44/±50），按 from->to 插值出端点，
    //   经 Matrix3D 旋转用 arccos 求张角，变换后写出 4 个像素坐标
    //   (field_DC..field_F4) 与 4 个世界坐标 (field_FC/108/114/120)，
    //   并把 Owner 的 TurretFacing 复制到 Facing。
    void DrawOldSchool(const CoordStruct& from, const CoordStruct& to);

    // asm: WaveClass_Draw_Green —— 声波的单像素着色器：
    //   idx = Pythagoras[a4*12 + (a2 - field_CC - a3)]，再取
    //   SineCurve1[idx + field_130] 做色系索引，从 Colouring[] 拿 565 基色，
    //   用 Linear1[] 做亮度混合后写回目标像素 [a5]。
    void Draw_Green(int32 a2, int32 a3, int32 a4, uint16* a5);

    // asm: WaveClass_Draw_Purple —— 紫色磁波的单像素着色器：
    //   与 Green 同构，改用 From.X / SineCurve2，索引钳到 9。
    void Draw_Purple(int32 a2, int32 a3, int32 a4, uint16* a5);

    // asm: WaveClass_Draw_NodLaser —— 方尖碑激光：整行扫描，
    //   以 LaserIntensity 作亮度缩放（<<6 / *3<<5），按屏幕行逐像素混合。
    void Draw_NodLaser(const RectangleStruct* a2, const RectangleStruct* a3);

    // asm: WaveClass_Draw_MagneticBeam —— 磁能波：几何与 SonicBeam 几乎相同
    //   （sub_63F080 求屏幕矩形），但逐像素调 Draw_Purple 上紫色。
    void Draw_MagneticBeam(const RectangleStruct* a2, const RectangleStruct* a3);

    // asm: WaveClass_Draw_MagnetronStyle —— 磁电拖曳样：
    //   用 from/to 与 16 组顶点表算出 4 个角点世界坐标，
    //   经 Matrix3D + 5AFB80 变换，写 field_FC/108/114/120，
    //   再 Coord_To_Pixel 出 field_DC..F4，并把 Owner->TurretFacing 拷进 Facing。
    void DrawMagnetronStyle(const CoordStruct& from, const CoordStruct& to);

    // asm: WaveClass_Draw_SonicBeam —— 声波束：按屏幕矩形做两次扫描，
    //   区内逐像素调 Draw_Green（青色），并维护左右裁剪边界。
    void Draw_SonicBeam(const RectangleStruct* a2, const RectangleStruct* a3);

    // ========================================================================
    // 失效指针清理
    // ========================================================================
    // asm: WaveClass_PointerGotInvalid —— 若失效对象是 Target 或施放者
    //   （asm 的 +0x1D4 / +0xAC）就清零。
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed) override;

    // ========================================================================
    // 数据成员（语义对齐 asm，不要求字节偏移一致）
    // ========================================================================
    int32               field_A8;           // asm +0xA8 保留槽
    ObjectClass*        Target;             // asm +0xAC  目标对象
    int32               Type;               // asm +0xB0  WaveType 分派值

    // 光束两端的原始世界坐标（asm +0xB4 from / +0xC0 to）。
    CoordStruct         From;               // asm +0xB4
    CoordStruct         To;                 // asm +0xC0

    // 两端在屏幕空间的像素坐标（asm +0xCC from / +0xD4 to）。
    Point2D             PixelFrom;          // asm +0xCC
    Point2D             PixelTo;            // asm +0xD4

    // asm +0xDC .. +0xF8：DrawOldSchool/MagnetronStyle 写出的 4 组像素。
    Point2D             DrawPixels[4];      // asm +0xDC / +0xE4 / +0xEC / +0xF4

    // asm +0xFC / +0x108 / +0x114 / +0x120：4 个角点世界坐标。
    CoordStruct         DrawCoords[4];      // asm +0xFC / +0x108 / +0x114 / +0x120

    uint8               field_12C;          // asm +0x12C  存活/激活标记（db）
    uint8               field_12D;          // asm +0x12D  尾部推进标记（db）
    uint8               field_12E;          // asm +0x12E  （db）
    uint8               field_12F;          // asm +0x12F  （db）

    int32               field_130;          // asm +0x130  寿命倒计时
    int32               field_134;          // asm +0x134  回绕计次
    double              field_138;          // asm +0x138  头部推进进度（qword）
    double              field_140;          // asm +0x140  尾部推进进度（qword）
    int32               field_144;          // asm +0x144
    int32               field_148;          // asm +0x148  4 段项数
    CoordStruct*        field_14C;          // asm +0x14C  指向 DrawCoords 的指针

    int32               field_1A0;          // asm +0x1A0  屏幕矩形宽
    int32               field_1A4;          // asm +0x1A4  屏幕矩形高
    int32               field_1A8;          // asm +0x1A8  行缓冲基址
    int32               field_1C4;          // asm +0x1C4
    int32               field_1C8;          // asm +0x1C8
    int32               field_1CC;          // asm +0x1CC  光束取向（sub_75F230）

    int32               LaserIntensity;     // asm +0x1D0  NodLaser 亮度
    TechnoClass*        Owner;              // asm +0x1D4  施放者
    FacingStruct        Facing;             // asm +0x1D8  朝向块

    // asm +0x1F0  声道/伤害格队列：VectorPtrCellTypeClass
    DynamicVectorClass<CellClass*> vec_Cells;

    // asm +Colouring  14 个色系（每系一个 RGB565 基色）。
    int32               Colouring[14];

private:
    // asm: sub_7610F0 —— 沿光束轴线逐格推进，把命中格加入 vec_Cells。
    void MarkDamageCells();

    // 局部工具：CoordStruct 线性插值（DrawMagnetronStyle 使用）。
    static CoordStruct LerpCoord(const CoordStruct& a, const CoordStruct& b, double t);
};

// ============================================================================
// 全局查找表（asm 中的 Wave_LUT_* 系列，定义在 WaveClass.cpp）
// ============================================================================
extern uint16   Wave_LUT_Pythagoras[];   // sqrt(i^2+j^2)，半步表
extern int16    Wave_LUT_SineCurve1[];   // 12*sin(i/8)+0.49
extern int16    Wave_LUT_SineCurve2[];   // 12*sin(i*0.1)+0.49
extern int32    Wave_LUT_Linear1[];      // 0x6E 起，步进 8
extern int32    Wave_LUTs_Calculated;    // asm: Wave_LUTs_Calculated
