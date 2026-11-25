#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"

class HouseClass;
class TechnoClass;
class TriggerClass;
class TActionClass;
class AnimClass;
class CCINIClass;

// ------------------------------------------------------------------------
// asm: dword_B0E6D4 —— 体素动画落在地面特殊格（bit 8 / bit 10）时叠加的
// Z 偏移量。原版直接读全局 dword；这里给出读写入口，装载侧负责赋值。
// ------------------------------------------------------------------------
int32  Voxel_Anim_Z_Offset();
void   Set_Voxel_Anim_Z_Offset(int32 value);

// ------------------------------------------------------------------------
// ActionClass —— 触发器动作面（原版 TActionClass / ActionClass_* 家族）。
//
// 原版把每一个动作处理器都写成独立的 __thiscall 过程，由 TActionClass::
// Execute 按动作号分派；ECX 里始终是动作记录自身。因此这些过程在语义上
// 都是本类的成员函数，参数从 [esp+4] 起算，返回 AL=1 表示动作生效。
//
// 字段偏移（原版内嵌布局，本类按语义重建，不要求逐字节一致）：
//   +0x28  TriggerAction（指向 TTrigger 的指针，Swizzle 用）
//   +0x2C  ActionIndex 之类的数值
//   +0x30  TeamType / 队伍类型指针
//   +0x34  TagClass 指针（在本类未直接使用，见 ObjectClass）
//   +0x44  Waypoint     路点下标
//   +0x48  P4_Value     通用参数 4（SW 充能值等）
//   +0x4C  Linked/Tag 指针
//   +0x50  TriggerClass* 目标触发器指针
//   +0x54  内嵌 0x18 字节的对象名/ID 字符串（原版 +0x54..+0x6C）
//   +0x6D  内嵌 0x20 字节的文本/电影名（原版 +0x6D..+0x8C）
//   +0x90  field_90      动作类型 / 参数1（也作路点或类型下标）
// ------------------------------------------------------------------------
class ActionClass {
public:
    ActionClass() noexcept;

    // ========================================================================
    // 类身份 / 序列化（原版 vftable 槽）
    // ========================================================================
    static int32  GetSize();
    static int32  GetAbstractDerivationID();
    static int32  GetArrayIndex();

    // asm: ActionClass_GetActionInternalType (0x6E3B70)
    //   把外部动作号折算成内部编辑分类编号。逐值查表（asm 行 4908-5115）。
    static int32  GetActionInternalType(int32 actionIndex);

    // asm: getActionFlags (0x6E3EE0) —— 只有“允许胜利”类动作返回 2，其余 0。
    static int32  GetActionFlags(int32 actionIndex);

    // asm: sub_6E3F70 —— 把该动作的路点结算成世界坐标（返回 3 个 int32）。
    void          GetWaypointCoord(int32* pOut);

    // asm: sub_6E3FB0 (0x6E3FB0) —— 把传入字符串去掉前导控制字后抄进 +0x6D。
    void          SetName(const char* pSource);

    // asm: sub_6E4010 —— 返回 +0x6D 的内嵌名（为空则返回 null）。
    const char*   GetName() const;

    // asm: ActionClass_FindHouseByIdx (0x6E45D7)
    //   0x2325 = 触发器关联房屋；-1 = 无；其余走房屋索引（MP 变体）。
    static HouseClass* FindHouseByIdx(TriggerClass* pTrigger, int32 idx);

    // asm: ActionClass_SaveIntoINI
    bool SaveIntoINI(class CCINIClass* pINI, const char* pSection);

    // ========================================================================
    // 目标房屋解析（内部小工具，非 asm 独立过程，供成员复用）
    // ========================================================================
    HouseClass* ResolveHouse(TriggerClass* pTrigger) const;

    // ========================================================================
    // 全局天气 / 环境动作
    // ========================================================================
    // asm: ActionClass_DoMeteorShower (0x6E2C00..0x6E2E0C)
    //   在路点周围随机抛洒 METLARGE / METSMALL 动画，个数取 MeteorCounts 表。
    bool DoMeteorShower(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);

    // ========================================================================
    // 触发器 / 队伍控制
    // ========================================================================
    // asm: ActionClass_EnableTrigger (0x6E2AE0)
    bool EnableTrigger(HouseClass* pHouse, int32 a3,
                       TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2B70 —— 关闭 +0x50 指向的那批触发器。
    bool DisableTrigger(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2A50 —— 让 +0x4C 指向的那批 Tag 自行消失。
    bool MakeTagPoof(HouseClass* pHouse, int32 a3,
                     TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2BB0 —— 在路点处加一次雷达事件（类型 = +0x90）。
    bool AddRadarEvent(HouseClass* pHouse, int32 a3,
                       TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E4020 —— 让关联队伍类型整体闪烁（+0x30 指针，时长 = +0x90）。
    bool FlashTeam(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E4040 —— 关联队伍类型的首个实例冒话泡（文本 = +0x90）。
    bool TalkBubbleOfTeam(HouseClass* pHouse, int32 a3,
                          TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E4090 —— 让附加在触发器上的建筑撤走乘员。
    bool RemoveOccupantsOfAttachedBuildings(HouseClass* pHouse, int32 a3,
                                            TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2970 —— 让附加建筑灯光灯闪（行为 = +0x90）。
    bool FlashAttachedBuildingLights(HouseClass* pHouse, int32 a3,
                                     TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E1FF0 —— 让关联房屋走一次“投降/让位”虚调用。
    bool HouseYield(HouseClass* pHouse, int32 a3,
                    TriggerClass* pTrigger, int32 a5);

    // ========================================================================
    // 全局 / 本地变量与计时器
    // ========================================================================
    // asm: sub_6E0FA0 / sub_6E0FC0 —— 置 1 / 置 0 全局变量（下标 = +0x90）。
    bool SetGlobalValue(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    bool ClearGlobalValue(HouseClass* pHouse, int32 a3,
                          TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2BE0 / sub_6E2C00 —— 置 1 / 置 0 本地变量（下标 = +0x90）。
    bool SetLocalValue(HouseClass* pHouse, int32 a3,
                       TriggerClass* pTrigger, int32 a5);
    bool ClearLocalValue(HouseClass* pHouse, int32 a3,
                         TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E1440 / sub_6E14A0 / sub_6E1530 / sub_6E15F0 / sub_6E13A0 /
    //      sub_6E13E0 —— 任务计时器三元组（+0x11E8 start / +4 value / +8 dur）。
    bool TimerStart(HouseClass* pHouse, int32 a3,
                    TriggerClass* pTrigger, int32 a5);
    bool TimerStop(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);
    bool TimerSet(HouseClass* pHouse, int32 a3,
                  TriggerClass* pTrigger, int32 a5);
    bool TimerAdd(HouseClass* pHouse, int32 a3,
                  TriggerClass* pTrigger, int32 a5);
    bool TimerSubtract(HouseClass* pHouse, int32 a3,
                       TriggerClass* pTrigger, int32 a5);
    bool TimerExtend(HouseClass* pHouse, int32 a3,
                     TriggerClass* pTrigger, int32 a5);
    bool TimerPing(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E3320 / sub_6E3340 / sub_6E3360 / sub_6E3300 —— 把 +0x90
    //   写进房屋的四个“任务计时器显示值”字段（+0x565C/+0x5660/+0x5664/+0x5668）。
    bool SetHouseTimerA(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    bool SetHouseTimerB(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    bool SetHouseTimerC(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    bool SetHouseTimerD(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E0ED0 —— 把 +0x90 写进房屋 +0x54EC。
    bool SetHouseValue54EC(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E0EF0 —— 开/关“基地自动建造”并设置基地出兵格。
    bool AutoBase(HouseClass* pHouse, int32 a3,
                  TriggerClass* pTrigger, int32 a5);

    // ========================================================================
    // 房屋阵营 / 同盟 / 灯闪
    // ========================================================================
    // asm: sub_6E0DF0 / sub_6E0E60 —— 关联房屋与目标房屋互为盟友 / 互为敌人。
    bool MakeAllyWithResolved(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5);
    bool MakeEnemyWithResolved(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2FA0 / sub_6E3000 —— 置/清 房屋 +0x1F2（“启用/禁用”某状态）。
    bool SetHouseFlag1F2(HouseClass* pHouse, int32 a3,
                         TriggerClass* pTrigger, int32 a5);
    bool ClearHouseFlag1F2(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E3060 —— 关联房屋全体欢呼。
    bool AllCheer(HouseClass* pHouse, int32 a3,
                  TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E0F90 —— 让地图推进一次阴影边界。
    bool EncroachShadow(HouseClass* pHouse, int32 a3,
                        TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E0D60 —— 打印一条 EVA/消息列消息。
    bool PrintMessage(HouseClass* pHouse, int32 a3,
                      TriggerClass* pTrigger, int32 a5);

    // ========================================================================
    // 地图 / 黑幕 / 天气
    // ========================================================================
    // asm: sub_6E1330 —— 首次把整图对玩家“揭开阴影边界”。
    bool GrowShroudToPlayer(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E26C0 / sub_6E2850 / sub_6E2860 / sub_6E28DC / sub_6E2950 ——
    //   视图/雷达/相机的一族界面动作。
    bool ViewEnter(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);
    bool CameraHome(HouseClass* pHouse, int32 a3,
                    TriggerClass* pTrigger, int32 a5);
    bool ZoomEnter(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);
    bool ZoomLeave(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);
    bool ShroudMapOf(HouseClass* pHouse, int32 a3,
                     TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E3080? / sub_6E3B20 —— 翻转 Scenario+0x34A5（“允许空袭”开关）。
    bool ToggleAirStrike(HouseClass* pHouse, int32 a3,
                         TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2F4F —— 设定闪电风暴时长（写 Scenario+0x3528 / +0x3530）。
    bool SetLightningStormTime(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2F90 —— 触发时光屏幕特效。
    bool ChronoScreenEffect(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2600 / sub_6E2640 —— 启动 / 终止离子风暴。
    bool StartLightningStorm(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5);
    bool StopLightningStorm(HouseClass* pHouse, int32 a3,
                            TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E2660 —— 清空放置/修理/出售等光标模式。
    bool CancelPlacementMode(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E2950 (0x6E2950) —— 对指定房屋整图重盖黑幕。
    bool ShroudTheMap(HouseClass* pHouse, int32 a3,
                      TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E26D0 / sub_6E2790 —— 把视图居中到路点。
    bool CenterViewOnWaypoint(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5);
    bool CenterViewOnPoint(HouseClass* pHouse, int32 a3,
                           TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E1180 —— 减少路点处的矿物（泰伯利亚）。
    bool ReduceTiberiumAt(HouseClass* pHouse, int32 a3,
                          TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E3380 —— 在路点处创建一个离子冲击（IonBlastClass）。
    bool SpawnIonBlast(HouseClass* pHouse, int32 a3,
                       TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E35F0 —— 在路点处启动一次闪电风暴（时长取 Rules+179Ch）。
    bool StartLightningStormAtWaypoint(HouseClass* pHouse, int32 a3,
                                       TriggerClass* pTrigger, int32 a5);

    // asm: sub_6E2F90 变体：播放一段电影（ID = +0x90）。
    bool PlayMovie(HouseClass* pHouse, int32 a3,
                   TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E1720 / sub_6E1740 —— 切换视频分辨率（窗口/全屏）。
    bool ToggleVideoWindowed(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5);
    bool ToggleVideoFullscreen(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E1760 —— 播放一段音效（索引 = +0x90）。
    bool PlaySoundAt(HouseClass* pHouse, int32 a3,
                     TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E1780 —— 在随机有效路点处播放 3D 音效。
    bool PlaySoundAtRandomWaypoint(HouseClass* pHouse, int32 a3,
                                   TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E1830 —— 返回路点所在格的建筑或地形（0x6E1830 未列入成员表）。
    int32 GetBuildingOrTerrainAt(HouseClass* pHouse, int32 a3,
                                 TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E3890 —— 让关联队伍类型“时空传送进场”。
    bool ChronoInTeam(HouseClass* pHouse, int32 a3,
                      TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E38B0 —— 在路点处执行一次 sub_56BEC0（地图级效果）。
    bool MapEffectAtWaypoint(HouseClass* pHouse, int32 a3,
                             TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E37F0 —— 把 +0x90 写进匹配类型（+0x54 名字）的 +0x634，
    //   并把全体房屋的 recheckTechLevels 置位。
    bool SetTypeTechLevelFlag(HouseClass* pHouse, int32 a3,
                              TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E4080 (0x6E4080) —— 让场景暂停 +0x90 秒。
    bool PauseForSeconds(HouseClass* pHouse, int32 a3,
                         TriggerClass* pTrigger, int32 a5);
    // asm: sub_6E3FB0 的公共辅助（字符串拷贝已在头中声明为 SetName）。

    // ========================================================================
    // 战斗 / 毁伤动作
    // ========================================================================
    // asm: ActionClass_WakeupAttachedObjects (0x6E0190)
    bool WakeupAttachedObjects(int32 a3, int32 a4,
                               TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_Apply100DamageAt (0x6E0570)
    bool Apply100DamageAt(int32 a2, int32 a3,
                          TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_AttachedTagSwitchHouse (0x6E0A90)
    bool AttachedTagSwitchHouse(HouseClass* pHouse, int32 a3,
                                TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_AllObjectsSwitchHouse (0x6E0B60)
    bool AllObjectsSwitchHouse(HouseClass* pHouse, int32 a3,
                               TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_MindControlHouseBuildings (0x6E0C90)
    bool MindControlHouseBuildings(HouseClass* pNewOwner, int32 a3,
                                   TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ReturnControlHouseBuildings (0x6E0D10)
    bool ReturnControlHouseBuildings(HouseClass* pOwner, int32 a3,
                                     TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_RevealWaypoint (0x6E0FE0)
    bool RevealWaypoint(int32 idxHouse, int32 a3,
                        TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_RevealZoneOfWaypoint (0x6E11D0)
    bool RevealZoneOfWaypoint(int32 idxHouse, int32 a3,
                              TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_StopSoundsAt (0x6E1980)
    bool StopSoundsAt(int32 idxHouse, int32 a3,
                      TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_TeleportAllTo (0x6E1D50)
    bool TeleportAllTo(int32 idxHouse, int32 a3,
                       TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ReshroudMap (0x6E1D80)
    bool ReshroudMap(int32 idxHouse, int32 a3,
                     TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DropFlare (0x6E1E40)
    bool DropFlare(int32 idxHouse, int32 a3,
                   TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DestroyAttached (0x6E2020)
    bool DestroyAttached(int32 idxHouse, int32 a3,
                         TriggerClass* targetTrigger,
                         const struct CellStruct* a5);
    // asm: sub_6E21D0 —— 空操作（AL=1）。
    bool NoOp(int32 a3, int32 a4, TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ResizePlayerView (0x6E21D0)
    bool ResizePlayerView(int32 idxHouse, int32 a3,
                          TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_PlayAnimAt (0x6E2280)
    bool PlayAnimAt(int32 idxHouse, int32 a3,
                    TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DoExplosionAt (0x6E2660)
    bool DoExplosionAt(int32 idxHouse, int32 a3,
                       TriggerClass* targetTrigger, int32 a5);
    // asm: VoxelAnim_Spawn (0x6E2530)
    int32 Spawn_Voxel_Anim(int32 idxHouse, int32 a3,
                           TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ClearSmudges (0x6E1A60)
    bool ClearSmudges(int32 idxHouse, int32 a3,
                      TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_RetintRed / Green / Blue (0x6E2E80 / 0x6E2EE0 / 0x6E2F40)
    bool RetintRed(int32 idxHouse, int32 a3,
                   TriggerClass* targetTrigger, int32 a5);
    bool RetintGreen(int32 idxHouse, int32 a3,
                     TriggerClass* targetTrigger, int32 a5);
    bool RetintBlue(int32 idxHouse, int32 a3,
                    TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_RestoreStartingTechnoOf (0x6E30B0)
    bool RestoreStartingTechnoOf(HouseClass* pHouse, int32 a3,
                                 TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_RestoreStartingBuildingsOf (0x6E3110)
    bool RestoreStartingBuildingsOf(HouseClass* pHouse, int32 a3,
                                    TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DestroyAllOf (0x6E3190)
    bool DestroyAllOf(HouseClass* pHouse, int32 a3,
                      TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DestroyAllBuildingsOf (0x6E31F0)
    bool DestroyAllBuildingsOf(HouseClass* pHouse, int32 a3,
                               TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DestroyAllLandUnitsOf (0x6E3260)
    bool DestroyAllLandUnitsOf(HouseClass* pHouse, int32 a3,
                               TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_DestroyAllNavalOf (0x6E32D0)
    bool DestroyAllNavalOf(HouseClass* pHouse, int32 a3,
                           TriggerClass* targetTrigger, int32 a5);

    // ========================================================================
    // 超武 / 侧栏动作
    // ========================================================================
    // asm: ActionClass_FireIronCurtain (0x6E36E0)
    bool FireIronCurtain(int32 idxHouse, int32 a3,
                         TriggerClass* targetTrigger, int32 a5);
    // asm: TActionClass_TriggerNukeStrike (0x6E33A0)
    bool TriggerNukeStrike(int32 idxHouse, int32 a3,
                           TriggerClass* targetTrigger, int32 a5);
    // asm: TActionClass_FireChemLauncher (0x6E38C0)
    bool FireChemLauncher(int32 idxHouse, int32 a3,
                          TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_RadarBlackout (0x6E3B20)
    bool RadarBlackout(HouseClass* pHouse, int32 a3,
                       TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_SetTab (0x6E4130)
    bool SetTab(int32 idxHouse, int32 a3,
                TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_FlashCameo (0x6E4140)
    bool FlashCameo(int32 idxHouse, int32 a3,
                    TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_CreateBuilding (0x6E4200)
    bool CreateBuilding(int32 coords, int32 a3, int32 a4, int32 a5);
    // asm: ActionClass_SetSWCharge (0x6E42C3)
    bool SetSWCharge(HouseClass* pHouse, int32 a3,
                     TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_SetSWRecharge (0x6E432E)
    bool SetSWRecharge(HouseClass* pHouse, int32 a3,
                       TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ResetSWRecharge (0x6E4375)
    bool ResetSWRecharge(HouseClass* pHouse, int32 a3,
                         TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ResetSW (0x6E43B5)
    bool ResetSW(HouseClass* pHouse, int32 a3,
                 TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_SetTargetCell (0x6E43DE)
    bool SetTargetCell(HouseClass* pHouse, int32 a3,
                       TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ClearTargetCell (0x6E4438)
    bool ClearTargetCell(HouseClass* pHouse, int32 a3,
                         TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_SetDefensiveCell (0x6E445E)
    bool SetDefensiveCell(HouseClass* pHouse, int32 a3,
                          TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ClearDefensiveCell (0x6E44BC)
    bool ClearDefensiveCell(HouseClass* pHouse, int32 a3,
                            TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_SetBaseCenter (0x6E44DE)
    bool SetBaseCenter(HouseClass* pHouse, int32 a3,
                       TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_ClearBaseCenter (0x6E453C)
    bool ClearBaseCenter(HouseClass* pHouse, int32 a3,
                         TriggerClass* targetTrigger, int32 a5);
    // asm: ActionClass_FlashBuildingsOfType (0x6E4560)
    bool FlashBuildingsOfType(HouseClass* pHouse, int32 a3,
                              TriggerClass* targetTrigger, int32 a5);

    // ========================================================================
    // 静电场/粒子遗留接口（历史空壳，本任务按 asm 语义归位）
    // ========================================================================
    // asm: ActionClass_LightningStrikeAt_unused (0x6E0040) —— __thiscall，
    // 路点号取自 this+0x44，不是静态函数。
    void LightningStrikeAt_unused(const CoordStruct& pos);
    // asm: ActionClass_RemoveParticleSystemsAt_unused (0x6E00D0) —— 同上。
    void RemoveParticleSystemsAt_unused(const CoordStruct& pos);

    // ========================================================================
    // 内嵌字段（原版 ActionClass 布局）
    // ========================================================================
    void*         Value28;             // +0x28  Swizzle 槽
    int32         Value2C;             // +0x2C  数值
    void*         TeamType;            // +0x30  队伍类型指针
    void*         TagPtr;              // +0x34  Tag 指针
    int32         Waypoint;            // +0x44  路点下标
    int32         P4_Value;            // +0x48  通用参数 4
    void*         LinkedPtr;           // +0x4C  Linked/Tag 指针
    // asm: ActionClass.Trigger (+0x50) —— 目标触发器“类型”指针。原版把它与
    //   TriggerClass.triggerType（TActionClass*）作比较，故此处用 TActionClass*。
    TActionClass* Trigger;             // +0x50  目标触发器类型指针
    char          InlineName[0x20];    // +0x54  内嵌 0x18 字节对象名（含尾 0x6D 起文本）
    char          TextName[0x20];      // +0x6D  内嵌文本/电影名
    int32         ActionIndex;         // +0x90  动作类型 / 参数1
};

// ------------------------------------------------------------------------
// asm: CellSpreads / CellSpread_Table 的调用点在这里以 MapClass 表为准；
// ActionClass 不额外持有副本。MeteorCounts 表（原版 6E2E60 附近）以 rdata
// 常量形式给出，见 .cpp。
// ------------------------------------------------------------------------
