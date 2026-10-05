#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;

// ============================================================================
// RadBeamClass - 雷达波束参数包
//
// 根据游戏行为，可知雷达波束以"参数包"形态存在：起点、终点、所属方、
// 颜色与束腰由一组 Set* 入口逐项填入，随后由 DrawAll 统一提交渲染。
// ============================================================================
class RadBeamClass
{
public:
    static DynamicVectorClass<RadBeamClass*>* Array;

    RadBeamClass() noexcept;

    // 参数填充入口（原版命名形态，逐项写入束体描述）。
    void SetCoordsTarget(const CoordStruct& from, const CoordStruct& to);
    void SetYetAnotherArg(int32 arg);
    void SetSomeMoreArg(int32 arg);
    void SetAnotherArg(int32 arg);
    void SetStillAnotherArg(int32 arg);
    void SetEvenMoreArg(int32 arg);

    // 把所有已登记波束提交渲染（每帧一次）。
    static void DrawAll();

    CoordStruct From;
    CoordStruct To;
    HouseClass* Owner;
    int32 ColorIndex;
    int32 Width;
    int32 Phase;
    int32 Lifetime;
};
