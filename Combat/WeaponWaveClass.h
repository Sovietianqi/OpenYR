#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Containers/DynamicVectorClass.h"

class TechnoClass;
class WeaponTypeClass;
class HouseClass;
class WarheadTypeClass;

// ============================================================================
// BeamSegment - 一条已提交给渲染层的波束段
//
// 各 Draw_* 风格入口把几何与风格参数折算成一个段提交到待渲染队列，由
// 战术层在帧末消费。
// ============================================================================
struct BeamSegment
{
    CoordStruct From;
    CoordStruct To;
    int32 ColorIndex;   // 色系：0 绿 / 1 紫 / 2 红激光 / 3 磁波 / 4 声波 / 5 经典
    int32 Width;        // 以像素计的束腰
    int32 Phase;        // 取亮度查找表的相位
};

// ============================================================================
// WeaponWaveClass - 波束武器特效（激光/磁波/声波族）
//
// 根据游戏行为，可知原版波束池承载所有"两点连一线"的武器特效：苏军
// 磁暴线圈与磁能波、Nod 方尖碑激光、声波炮、以及经典激光。各风格只
// 差颜色、束腰宽度与亮度衰减曲线；束体存在期由年龄推进，声波束在存续
// 期按节拍对目标格持续结算伤害。
// ============================================================================
class WeaponWaveClass
{
public:
    static DynamicVectorClass<WeaponWaveClass*>* Array;

    WeaponWaveClass(TechnoClass* pOwner, TechnoClass* pTarget,
                    WeaponTypeClass* pWeapon,
                    const CoordStruct& from, const CoordStruct& to) noexcept;
    ~WeaponWaveClass();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知亮度查找表在首次绘制前构建一次：32 档亮度从满
    // 亮度线性衰减到熄灭，供各风格按相位取值。
    // ------------------------------------------------------------------------
    static void CalculateLUTs();
    static const int32* GetLUTs();

    // 每帧推进束体年龄；到期打上终止标记并从对象表摘除。
    void Update_Beam();

    // 按武器伤害对目标坐标所在区域结算一次伤害（声波束的持续节拍入口）。
    void DamageCell();

    // 各风格绘制入口：折算风格参数后提交一条待渲染波束段。
    void Draw_Green();
    void Draw_Purple();
    void Draw_NodLaser();
    void Draw_MagneticBeam();
    void Draw_SonicBeam();
    void DrawMagnetronStyle();
    void DrawOldSchool();

    // 待渲染段队列：渲染层每帧取空。
    static DynamicVectorClass<BeamSegment>* GetSegmentQueue();

    TechnoClass*     Owner;
    TechnoClass*     Target;
    WeaponTypeClass* Weapon;
    CoordStruct      From;
    CoordStruct      To;
    int32            Age;
    int32            Duration;
    int32            Width;
    int32            DamageDealt;
    bool             IsDead;

protected:
    void SubmitSegment(int32 colorIndex, int32 width, int32 phase);
};
