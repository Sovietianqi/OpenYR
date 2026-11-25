#include <Game/LogicClass.h>

#include <Game/Game.h>
#include <Game/Externs.h>
#include <Game/MainLoop.h>

#include <AI/TagClass.h>
#include <AI/TriggerClass.h>
#include <AI/TeamClass.h>
#include <AI/TeamTypeClass.h>
#include <AI/AITeamClass.h>
#include <Combat/BombClass.h>
#include <Houses/FactoryClass.h>
#include <Houses/HouseClass.h>
#include <Map/MapClass.h>
#include <Rendering/TacticalClass.h>
#include <Special/DiskLaserClass.h>
#include <Special/RadSiteClass.h>

// asm: func_55B8F0_obsolete 逐表扫描所需的 11 个对象类
#include <Animations/AnimClass.h>
#include <Abstract/BuildingLightClass.h>
#include <Combat/BulletClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/AircraftClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/UnitClass.h>
#include <Particles/ParticleSystemClass.h>
#include <Abstract/TerrainClass.h>
#include <Abstract/VoxelAnimClass.h>
#include <Special/WaveClass.h>

#include <Core/Memory.h>

#include <cstdio>
#include <cstdlib>

// ============================================================================
// LogicClass.cpp
//
//  逐帧逻辑总入口 LogicClass_Update 与 vec_Logics 维护。
//  唯一权威：gamemd.exe.asm.txt 的 LogicClass_Update（proc@751139-751988）
//  与 Main_Loop 的调用点（L756687）。每一段都标注了对应的 asm 行号。
// ============================================================================

// ============================================================================
// 全局表
// ============================================================================

// asm: L28156 `vec_Logics` 定义；L69914-69920 用 VectorClass<ObjectClass*>
//  的 vftable 初始化。定义在 Game/Globals.cpp（extern 见 Game/Externs.h）。

// ============================================================================
// LogicClass_Init / LogicClass_Shutdown
// ============================================================================

void LogicClass_Init()
{
    // asm: L69914-69920 —— 启动时建表，长度 0。
    if (vec_Logics == nullptr)
        vec_Logics = new DynamicVectorClass<LogicClass*>();

    // vec_VisibleObjects 与 vec_Logics 同属“全局对象表”，在同一个初始化块里
    // 建立（ObjectClass::Init_Array 自身也保证幂等）。
    ObjectClass::Init_Array();
}

void LogicClass_Shutdown()
{
    // asm: Map_ClearVectors (proc@673178) 对全部 vec_* 逐一清空后释放。
    if (vec_Logics != nullptr)
    {
        vec_Logics->Clear();
        delete vec_Logics;
        vec_Logics = nullptr;
    }

    ObjectClass::Delete_Array();
}

// ============================================================================
// LogicClass_Register / _Unregister / _IsRegistered
//
// asm: sub_55BAA0(VectorClass::Add, 参数 a1 = 对象, a2 = 0)
//      sub_55BAE0(VectorClass::Remove, 参数 = 对象)
// 表本身不持有引用，只搬运裸指针。
// ============================================================================

void LogicClass_Register(LogicClass* pLogic)
{
    if (pLogic == nullptr)
        return;

    if (vec_Logics == nullptr)
        vec_Logics = new DynamicVectorClass<LogicClass*>();

    // 原版 Add 不去重；重复项会让同一对象每帧被驱动两次（移动/开火都会翻倍），
    // 故这里先做一次线性查重。表长量级为数百，Unlimbo 又不是热路径。
    for (int32 i = 0; i < vec_Logics->Count; ++i)
    {
        if (vec_Logics->Items[i] == pLogic)
            return;
    }

    vec_Logics->Add(pLogic);
}

void LogicClass_Unregister(LogicClass* pLogic)
{
    if (vec_Logics == nullptr || pLogic == nullptr)
        return;

    // asm sub_55BAE0 按对象指针定位后再尾部前移摘除，重复项一并清掉。
    for (int32 i = vec_Logics->Count - 1; i >= 0; --i)
    {
        if (vec_Logics->Items[i] == pLogic)
            vec_Logics->Remove(i);
    }
}

bool LogicClass_IsRegistered(const LogicClass* pLogic)
{
    if (vec_Logics == nullptr || pLogic == nullptr)
        return false;

    for (int32 i = 0; i < vec_Logics->Count; ++i)
    {
        if (vec_Logics->Items[i] == pLogic)
            return true;
    }
    return false;
}

// ============================================================================
// LogicClass_Update  (asm: 751139 .. 751988)
//
//   顺序严格照 asm。逐段说明见每段注释。
//   注意：原版第 15 步的 vec_Logics 循环是本函数的**主体**，所有单位、
//   子弹、动画、波束、建筑都在这一步被 `call [obj+0x5C]` 驱动。
// ============================================================================
void LogicClass_Update()
{
    // ── 1. Tag 事件（asm L751177 / 751198 / 751211 / 751224 / 751237 /
    //        751259 / 751272 / 751287 / 751300 / 751336）─────────────────
    // 对 vec_Tags 里每个标签上抛 10 类全局事件（50/27/28/36/37/45/46/13/51/14），
    // 命中后经 TriggerClass::HaveAllEventsOccured → FireActions → TActionClass。
    Update_Triggers();

    // ── 2. 幕布/黑幕再生（asm L751461 / 751486 / 751533）───────────────
    // 原版按 RulesData.VeinholeShrinkRate 节流调 DisplayClass::Encroach_Shadow /
    // MapClass_ReshroudAgain / DisplayClass::Encroach_Fog。OpenYR 的
    // DisplayClass 尚未暴露这两个节流入口，黑幕改由 TacticalClass 绘制期
    // 增量刷新（TacticalClass::Draw_Shroud）。此处留空但不影响逻辑面。

    // ── 3. 空投/超武活动表（asm L751570-751585）────────────────────────
    // Nuke_IsActive / sub_53A120 / ChronoEffect_Active / PsyDom_IsActive
    // 属于超武表现层，由 Update_SuperWeapons 每帧推进。

    // ── 5. 炸弹表（asm L751695 BombListClass_Update）────────────────────
    BombClass::AI();

    // ── 6. 神风管理器（asm L751698 sub_54E4D0）──────────────────────────
    // OpenYR 未建模 KamikazeContainer，随 BombClass::AI 一并处理。

    // ── 7. Team 循环（asm L751785）──────────────────────────────────────
    // mov eax,vec_Teams.data / mov ecx,[eax+esi*4] / call [edx+5Ch]
    if (TeamClass::Array != nullptr)
    {
        for (int32 i = 0; i < TeamClass::Array->Count; ++i)
        {
            TeamClass* pTeam = (*TeamClass::Array)[i];
            if (pTeam != nullptr)
                pTeam->Update();
        }

        // 临时诊断：每 300 帧打印一次队伍构成与工厂队列深度。
        {
            static const bool enableDump = (std::getenv("OPENYR_TEAMDBG") != nullptr);
            static int32 lastDump = -9999;
            if (enableDump && Game::CurrentFrame - lastDump >= 300) {
                lastDump = Game::CurrentFrame;
                std::fprintf(stderr, "[TEAMDBG] f=%d teams=%d\n",
                             (int)Game::CurrentFrame, (int)TeamClass::Array->Count);
                for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
                    TeamClass* pT = (*TeamClass::Array)[i];
                    if (pT == nullptr) continue;
                    std::fprintf(stderr,
                        "  [TEAMDBG]   t%d name=%s members=%d miss=%d m=%d counts=%d,%d,%d,%d,%d,%d\n",
                        (int)i,
                        (pT->Type ? pT->Type->ID : "?"),
                        (int)pT->Members.Count,
                        (int)pT->IsMissingMembers,
                        (int)pT->CurrentMission,
                        (int)pT->TeamCounts[0], (int)pT->TeamCounts[1],
                        (int)pT->TeamCounts[2], (int)pT->TeamCounts[3],
                        (int)pT->TeamCounts[4], (int)pT->TeamCounts[5]);
                }
                {
                    for (int32 h = 0; h < HouseClass::ArrayCount; ++h) {
                        HouseClass* pH = HouseClass::Array[h];
                        if (pH == nullptr || pH->Type == nullptr) continue;
                        std::fprintf(stderr, "  [HOUSEDBG] %s human=%d money=%d blds=%d units=%d inf=%d:",
                                     pH->Type->ID, (int)pH->Is_Human(),
                                     (int)pH->Credits,
                                     (int)pH->OwnedBuildings.Count,
                                     (int)pH->OwnedUnits.Count,
                                     (int)pH->OwnedInfantry.Count);
                        for (int32 k = 0; k < pH->OwnedBuildings.Count; ++k) {
                            BuildingClass* pB = pH->OwnedBuildings[k];
                            if (pB == nullptr || pB->Type == nullptr) continue;
                            std::fprintf(stderr, " %s@%p%s", pB->Type->ID,
                                         static_cast<void*>(pB),
                                         pB->IsConstructed ? "" : "(wip)");
                        }
                        std::fprintf(stderr, "\n");
                    }
                }
                if (FactoryClass::Array != nullptr) {
                    for (int32 i = 0; i < FactoryClass::Array->Count; ++i) {
                        FactoryClass* pF = (*FactoryClass::Array)[i];
                        if (pF == nullptr) continue;
                        std::fprintf(stderr,
                            "  [FACTDBG]  f%d owner=%s bld=%s q=%d cur=%s online=%d prog=%d est=%d hold=%d cs=%d bal=%d money=%d\n",
                            (int)i,
                            (pF->GetOwner() && pF->GetOwner()->Type ? pF->GetOwner()->Type->ID : "?"),
                            (pF->GetFactoryBuilding() && pF->GetFactoryBuilding()->Type ? pF->GetFactoryBuilding()->Type->ID : "orphan"),
                            (int)pF->GetQueueCount(),
                            (pF->GetCurrentOrder() ? pF->GetCurrentOrder()->ID : "none"),
                            (int)(pF->Get_Product() != nullptr),
                            (int)pF->GetProgress(),
                            (int)pF->Get_EstimateFrames(),
                            (int)pF->IsOnHold(),
                            (int)pF->IsCompleteSuspended(),
                            (int)pF->Get_Balance(),
                            (pF->GetOwner() ? (int)pF->GetOwner()->Credits : -1));
                    }
                }
            }
        }

        // ★ 回收已解散的队伍。
        //   DoDisappear 只把成员清空并把 JustDisappeared 置位，队伍对象本身
        //   仍留在 vec_Teams 里。不回收会有两个后果：① 每帧仍被 Update 走一遍
        //   （虽已短路，但表会无限增长）；② HouseClass::CountInstancesOfTeam
        //   把空壳算成"这种编队已经存在"，Max 名额永远不释放 —— 于是 AI
        //   造出来的兵再也组不成新队伍，全部停在 Guard 驻守。
        //   倒序删除：~TeamClass 会自己从 vec_Teams 摘除，倒序遍历保证下标安全。
        for (int32 i = TeamClass::Array->Count - 1; i >= 0; --i)
        {
            TeamClass* pTeam = (*TeamClass::Array)[i];
            if (pTeam != nullptr && pTeam->JustDisappeared)
                delete pTeam;
        }
    }

    // asm: vec_Teams 存的另一支是 AITeamClass（TeamTypeClass_CreateTeam 产出），
    //   与 TeamClass 同属第 7 步的这一轮驱动。
    AITeamClass::UpdateAll();

    // ── 8. 磁盘激光（asm L751804 v_DiskLaser[i] → [edx+vt_Abstract.Update]）
    if (DiskLaserManagerClass* pLasers = DiskLaserManagerClass::GetInstance())
        pLasers->UpdateAllLasers();

    // ── 9/10/11. 探照灯 / 激光绘制 / sub_53A6C0（asm L751811-751815）────
    // OpenYR 未建模 SpotlightClass / LaserDrawClass；探照灯在 Tactical 绘制期
    // 处理，故略过。

    // ── 12. RadSite 循环（asm L751827）倒序，call [edx+5Ch] ─────────────
    if (RadSiteClass::Array != nullptr)
    {
        for (int32 i = RadSiteClass::Array->Count - 1; i >= 0; --i)
        {
            RadSiteClass* pRad = (*RadSiteClass::Array)[i];
            if (pRad != nullptr)
                pRad->Update();
        }
    }

    // ── 13/14. 光源栅格 / EMP 脉冲（asm L751836 / L751838）──────────────
    // vecLightSources_UpdateCells(6, 0) 与 EMPulseClass_UpdateAll。
    // OpenYR 的光源按需在 LightSourceClass::Update 内自刷新，EMP 由超武推进。

    // ── 15. ★ 对象循环（asm L751847-751856）─────────────────────────────
    //   mov eax,[edi+4]        ; data
    //   mov ecx,[eax+esi*4]    ; this
    //   mov edx,[ecx] / call [edx+vt_TechnoClass.r.m.o.a.Update]
    //
    // 每一轮都重读 Count（asm 在循环尾重读 [edi+10h]），因为被驱动的对象
    // 可能在 Update 里登记新对象（子机/残骸动画/子弹）。
    if (vec_Logics != nullptr)
    {
        for (int32 i = 0; i < vec_Logics->Count; ++i)
        {
            LogicClass* pLogic = vec_Logics->Items[i];
            if (pLogic != nullptr)
                pLogic->Update();          // ← vt+0x5C
        }
    }

    // ── 16. 第二对象表（asm L751877，仅 Session != 0 且 != 5）───────────
    // stru_A83E04 是多人录像/观战用的镜像对象表，OpenYR 未建模。

    // ── 17/18. 离子风暴 / sub_420E90（asm L751886 / L751888）────────────
    // IonBlastClass_UpdateAll 由 Update_SuperWeapons 承担。

    // ── 19. 地图逻辑（asm L751891 MapClass::Logic）──────────────────────
    // 仅做随机木箱：CrateClass::Remove_It + MapClass::Place_Random_Crate。
    if (MapClass::Instance != nullptr)
        MapClass::Instance->Logic();

    // ── 20. 战术层（asm L751895 TacticalMap → [eax+0x5C]）───────────────
    // asm: mov ecx,TacticalMap / mov eax,[ecx] / call [eax+5Ch]
    //   → TacticalClass_Update（1483457）：视口平滑滚动插值、坐标夹取、
    //     鼠标帧计时、帧戳与重绘标记。地图/侧栏/雷达的实际像素绘制仍由
    //     Main_Loop 的 Render_Frame 承担。
    if (TheTactical != nullptr)
    {
        TheTactical->Update();          // ← vt+0x5C = TacticalClass_Update
        TheTactical->YSortObjects();    // 绘制前的对象 Y 排序
    }

    // ── 21. Factory 循环（asm L751897-751912）call [edx+5Ch] ─────────────
    if (FactoryClass::Array != nullptr)
    {
        for (int32 i = 0; i < FactoryClass::Array->Count; ++i)
        {
            FactoryClass* pFactory = (*FactoryClass::Array)[i];
            if (pFactory != nullptr)
                pFactory->Update();
        }
    }

    // ── 22. House 循环（asm L751915-751935）call [edx+5Ch] ──────────────
    // HouseClass::Update 内含 AI_Building/AI_Unit/AI_Infantry/AI_Aircraft 与
    // TeamTypeClass_CreateTeam（L552843 / L553467 / L553494 / L553574 / L553577）。
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i)
    {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse != nullptr)
            pHouse->Update();
    }

    // ── 23. 跟随镜头（asm MapClass_FollowingWhat → sub_6D6070）───────────
    // 镜头跟随属于显示层，随雷达/侧栏刷新一起在 UI 阶段处理。
}

// ============================================================================
// asm: func_55B8F0_obsolete (0x55B8F0)
//
//  每张表都是同一个三段式：
//    mov eax, <vec>.length / lea esi,[eax-1] / test esi,esi / jl next_table
//  loop:
//    mov ecx, <vec>.data / mov ecx,[ecx+esi*4]
//    mov al,[ecx+98h] / test al,al / jz skip
//    mov edx,[ecx] / call dword ptr [edx+5Ch]     ; = LogicClass::Update
//  skip:
//    dec esi / jns loop
//
//  11 张表顺序：vec_Anims → v_BuildingLight → vec_Bullets → vec_Buildings →
//  vec_Aircrafts → inf_vec → vec_Units → vec_ParticleSystems → vec_Terrain →
//  v_VoxelAnim → vec_Waves。
//  原版对空槽/空表不做保护（会直接解引用），这里按本工程惯例补 null 守卫。
// ============================================================================
namespace {

// asm: mov al,[ecx+98h] / test al,al —— 裸偏移判定，语义未在原版留下符号。
inline bool Sweep_Mark_Set(const void* pObject)
{
    return *reinterpret_cast<const uint8*>(static_cast<const uint8*>(pObject) + 0x98) != 0;
}

template <typename T>
void Sweep_Object_Array(DynamicVectorClass<T*>* pArray)
{
    if (pArray == nullptr)
        return;

    for (int32 i = pArray->Count - 1; i >= 0; --i) {   // asm: lea esi,[eax-1] / dec / jns
        T* pObject = pArray->Items[i];                 // asm: mov ecx,[data+esi*4]
        if (pObject == nullptr)
            continue;
        if (Sweep_Mark_Set(pObject))                   // asm: mov al,[ecx+98h]
            pObject->Update();                         // asm: call [edx+5Ch]
    }
}

} // namespace

void Logic_Sweep_All_Objects_Obsolete()
{
    Sweep_Object_Array(AnimClass::Array);              // asm: vec_Anims
    Sweep_Object_Array(BuildingLightClass::Array);     // asm: v_BuildingLight
    Sweep_Object_Array(BulletClass::Array);            // asm: vec_Bullets
    Sweep_Object_Array(BuildingClass::Array);          // asm: vec_Buildings
    Sweep_Object_Array(AircraftClass::Array);          // asm: vec_Aircrafts
    Sweep_Object_Array(InfantryClass::Array);          // asm: inf_vec
    Sweep_Object_Array(UnitClass::Array);              // asm: vec_Units
    Sweep_Object_Array(ParticleSystemClass::Array);    // asm: vec_ParticleSystems
    Sweep_Object_Array(TerrainClass::Array);           // asm: vec_Terrain
    Sweep_Object_Array(VoxelAnimClass::Array);         // asm: v_VoxelAnim
    Sweep_Object_Array(WaveClass::Array);              // asm: vec_Waves
}
