#pragma once

//========================================================================
// InitHooks - 模块级全局初始化器集合
//
// 根据游戏行为，可知原版在启动路径上有一批"把全局对象搬进就绪状态"
// 的初始化器：规则表、随机数引擎、战役表、界面存储槽、各移动器的
// 默认出发点常量、全局向量等。这里把它们集中为一组静态入口，启动
// 流程按需调用。
//========================================================================

#include "../Math/CoordStruct.h"

class InitHooks
{
public:
    static void Rules();
    static void Randomizer();
    static void Campaigns();
    static void GadgetSidebar();
    static void ShowStartScreen();
    static void BulkData();
    static void SecondaryMixFiles();
    static void TheaterPAL_Visc_SHP_BSurfaceX();
    static void UI_INI_storage();
    static void RA2MD_INI_storage();
    static void ART_INI_storage();
    static void AI_INI_storage();
    static void vec_Kamikazes();
    static void vec_ColorSchemes();
    static void gfv();

    // 根据游戏行为，可知每种移动器都有一个默认出发点常量，序列化与
    // 出生落位都从这里取初值。
    static void DriveLoco_Default_Pos();
    static void FlyLoco_Default_Pos();
    static void WalkLoco_Default_Pos();
    static void ShipLoco_Default_Pos();
    static void HoverLoco_Default_Pos();
    static void TunnelLoco_Default_Pos();
    static void TeleportLoco_Default_Pos();
    static void JumpjetLoco_Default_Pos();
    static void RocketLoco_Default_Pos();
    static void DpodLoco_Default_Pos();
    static void MechLoco_Default_Pos_1();

    // 默认出发点查询：按移动器类别号取坐标。
    static CoordStruct GetLocoDefaultPos(int32 clsid);

private:
    // 各类别默认出发点存储（与 LocomotionClass::CLSIDs 的类别号对应）。
    static CoordStruct LocoDefaultPos[11];

    InitHooks() = delete;
    ~InitHooks() = delete;
};
