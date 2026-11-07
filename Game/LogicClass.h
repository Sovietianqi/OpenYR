#pragma once

#include <Abstract/AbstractClass.h>

// ============================================================================
// LogicClass — 逐帧逻辑驱动接口（asm: AbstractClass 的 vtable 槽 0x5C = Update）
//
// 原版：`vec_Logics` 是 `VectorClass<LogicClass*>`
//   （定义 L28156，初始化 L69914-69920），`Main_Loop@756687` 把它作为
//   `ecx` 传给 `LogicClass_Update`（proc@751139），函数体在 L751847-751856 做：
//
//     mov eax, [edi+4]          ; vec_Logics.data
//     mov ecx, [eax+esi*4]      ; this = vec_Logics[i]
//     mov edx, [ecx]
//     call [edx+vt_TechnoClass.r.m.o.a.Update]
//
// 即“每一个进入战场的东西，每帧被自己虚表 0x5C 槽驱动一次”。
//
// 原版的 Update 槽其实定义在 AbstractClass 上（见 §0.1 的 0x5C = Update），
// 所以凡能进 vec_Logics 的实体（ObjectClass 全家族、BuildingLightClass 等）
// 天然满足本接口。OpenYR 因此把 LogicClass 实现为**无数据成员的纯接口**，
// 让 ObjectClass 以它为基类——对象布局与“直接继承 AbstractClass”完全一致，
// 虚表槽位也不新增，等价于原版的 `vt_AbstractClass.a.Update`。
// ============================================================================
class LogicClass : public AbstractClass {
public:
    // asm: vt+0x5C —— 唯一的逐帧驱动入口。
    virtual void Update() = 0;
};

// ============================================================================
// vec_Logics 维护（asm: sub_55BAA0 = VectorClass::Add / sub_55BAE0 = Remove）
//
// 原版 ObjectClass::Unlimbo 在 L1049805 调 Add，ObjectClass_DTOR 在 L1047348、
// ObjectClass_Remove0 在 L1049520 调 Remove。OpenYR 的对应调用点见
// ObjectClass::Unlimbo / ~ObjectClass / ObjectClass::Remove0。
// ============================================================================

// 分配 vec_Logics 与 vec_VisibleObjects（等价 asm L69914-69920 的全局表初始化）。
// 可重复调用（幂等）。
void LogicClass_Init();

// 释放全局表（Map_ClearVectors 的收尾形态）。
void LogicClass_Shutdown();

// 登记 / 注销 / 查询。登记与注销都按 asm 的“表不持有引用”语义处理：
// 只搬运指针，不触碰 RefCount，重复登记会被忽略（原版 Add 前不做去重，
// 但重复项会让对象每帧被驱动两次，故这里做一次线性去重）。
void LogicClass_Register(LogicClass* pLogic);
void LogicClass_Unregister(LogicClass* pLogic);
bool LogicClass_IsRegistered(const LogicClass* pLogic);

// ============================================================================
// LogicClass_Update  (asm: proc@751139 .. 751988)
//
//  每帧逻辑总入口，由 Main_Loop@756687 调用（ecx = &vec_Logics）。
//  原版没有 Update_AI / Update_Objects / Update_Combat 之类的手写阶段，
//  只有这一个函数，内部按固定顺序驱动各子系统：
//
//    Tags(10 事件族) → Shroud → 空投表 → BombList → Kamikaze
//      → Teams → DiskLaser → RadSites → 光源 → EMP
//      → ★ vec_Logics(Objects) → 第二对象表 → IonBlast
//      → Map::Logic → Tactical → Factory → Houses → 跟随镜头
//
//  实现见 Game/LogicClass.cpp，逐段标注 asm 行号。
// ============================================================================
void LogicClass_Update();

// ============================================================================
// asm: func_55B8F0_obsolete (0x55B8F0)
//
//  原版 .text 里一段**没有调用点**的公共符号（IDA 便标成 _obsolete）。
//  它把 11 张对象表各自从尾部往前扫一遍，对每个 byte[obj+98h] 非零的对象
//  调一次 vt+5Ch（= LogicClass::Update）：
//
//    vec_Anims / v_BuildingLight / vec_Bullets / vec_Buildings /
//    vec_Aircrafts / inf_vec / vec_Units / vec_ParticleSystems /
//    vec_Terrain / v_VoxelAnim / vec_Waves
//
//  注意 asm 里的 +98h 是按"对象指针"+0x98 取的裸字节，各类的对象前置偏移
//  并不一致（例如 BuildingLightClass 内嵌 ObjectClass 于 +4）；该字节的
//  语义在原版里没有留下符号，故这里保留同一裸偏移判定，不做猜测性命名。
// ============================================================================
void Logic_Sweep_All_Objects_Obsolete();
