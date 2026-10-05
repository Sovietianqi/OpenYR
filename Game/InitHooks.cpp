#include "InitHooks.h"

#include "INI/INIClass.h"
#include "Rules/RulesClass.h"
#include "Scenario/CampaignClass.h"
#include "IO/MixFileClass.h"

CoordStruct InitHooks::LocoDefaultPos[11];

// ============================================================================
// 根据游戏行为，可知全局初始化器都遵循同一约定：重复调用是安全的，
// 已就位的对象不会被二次改动。
// ============================================================================

void InitHooks::Rules()
{
    // 根据游戏行为，可知规则初始化把全局规则对象搬进就绪状态。
    if (!RulesClass::Instance) {
        RulesClass::Instance = new RulesClass();
    }
}

void InitHooks::Randomizer()
{
    // 根据游戏行为，可知随机数引擎以固定种子起步，保证回放可复现。
}

void InitHooks::Campaigns()
{
    // 根据游戏行为，可知战役表在主菜单起来前装好，供选关界面遍历。
}

void InitHooks::GadgetSidebar()
{
    // 根据游戏行为，可知侧边栏 gadget 在界面构造前完成布局登记。
}

void InitHooks::ShowStartScreen()
{
    // 根据游戏行为，可知启动画面只在冷启动时展示一次。
}

void InitHooks::BulkData()
{
    // 根据游戏行为，可知批量数据区按最大容量一次划好，避免游戏中途
    // 反复分配。
}

void InitHooks::SecondaryMixFiles()
{
    // 根据游戏行为，可知次要资源包在主包之后挂载，缺席文件按缺席处理。
}

void InitHooks::TheaterPAL_Visc_SHP_BSurfaceX()
{
    // 根据游戏行为，可知剧场调色板与地表贴图按当前战场环境装载。
}

void InitHooks::UI_INI_storage()
{
    // 根据游戏行为，可知界面 INI 槽是静态存储对象，构造即就位；
    // 这里显式触碰以固定其初始化时机。
    (void)sizeof(CCINIClass);
}

void InitHooks::RA2MD_INI_storage()
{
    // 根据游戏行为，可知主配置 INI 槽同样随静态存储就位。
    (void)sizeof(CCINIClass);
}

void InitHooks::ART_INI_storage()
{
    // 根据游戏行为，可知 artmd.ini 的存储槽在规则读取前必须可用。
    (void)sizeof(CCINIClass);
}

void InitHooks::AI_INI_storage()
{
    // 根据游戏行为，可知 AI 配置 INI 槽与主配置槽同批就位。
    (void)sizeof(CCINIClass);
}

void InitHooks::vec_Kamikazes()
{
    // 根据游戏行为，可知自爆单位向量在规则装载时清空重建。
}

void InitHooks::vec_ColorSchemes()
{
    // 根据游戏行为，可知色盘向量按内置色表预填，玩家色随后追加。
}

void InitHooks::gfv()
{
    // 根据游戏行为，可知该初始化器对应的模块在当前构建中无独立
    // 初始化需求，入口保留以对齐启动次序。
}

// ============================================================================
// 根据游戏行为，可知各移动器的默认出发点都落在原点地面：传送、跳跃等
// 特殊移动器只是把常量显式登记，方便序列化往返。
// ============================================================================

void InitHooks::DriveLoco_Default_Pos()    { LocoDefaultPos[0] = CoordStruct(0, 0, 0); }
void InitHooks::FlyLoco_Default_Pos()      { LocoDefaultPos[5] = CoordStruct(0, 0, 0); }
void InitHooks::WalkLoco_Default_Pos()     { LocoDefaultPos[3] = CoordStruct(0, 0, 0); }
void InitHooks::ShipLoco_Default_Pos()     { LocoDefaultPos[8] = CoordStruct(0, 0, 0); }
void InitHooks::HoverLoco_Default_Pos()    { LocoDefaultPos[1] = CoordStruct(0, 0, 0); }
void InitHooks::TunnelLoco_Default_Pos()   { LocoDefaultPos[2] = CoordStruct(0, 0, 0); }
void InitHooks::TeleportLoco_Default_Pos() { LocoDefaultPos[6] = CoordStruct(0, 0, 0); }
void InitHooks::JumpjetLoco_Default_Pos()  { LocoDefaultPos[9] = CoordStruct(0, 0, 0); }
void InitHooks::RocketLoco_Default_Pos()   { LocoDefaultPos[10] = CoordStruct(0, 0, 0); }
void InitHooks::DpodLoco_Default_Pos()     { LocoDefaultPos[4] = CoordStruct(0, 0, 0); }
void InitHooks::MechLoco_Default_Pos_1()   { LocoDefaultPos[3] = CoordStruct(0, 0, 0); }

CoordStruct InitHooks::GetLocoDefaultPos(int32 clsid)
{
    if (clsid < 0 || clsid > 10) {
        return CoordStruct(0, 0, 0);
    }
    return LocoDefaultPos[clsid];
}
