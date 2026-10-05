#pragma once

#include "MPGameModeClass.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// MPCoopClass - cooperative multiplayer mode
//
//  根据游戏行为，可知合作模式在通用多人模式面板上扩展：参战各方同
//  阵营共闯、AI 队友按关卡表补位、战役完成后落持久战绩码。以下槽位
//  以原版字面槽位名保留可溯源性。
// ============================================================================

class MPCoopClass : public MPGameModeClass {
public:
    MPCoopClass() noexcept;
    virtual ~MPCoopClass();

    bool func04() const;
    bool func08() const;
    void func14();
    bool func18();
    void func1C();
    void func20();
    void func24();
    void func28();
    void func38();
    void func44();
    void func4C();
    void func58();
    void func5C();
    void func60();
    void func64();
    void func68();
    void func6C();
    void func70();
    void func74(const char* pList);
    void func78();
    bool func7C();
    void func90();
    void func94();
    int32 func98() const;
    void func9C();
    void funcA0();
    void funcA4();
    void funcA8();
    void funcAC();
    void funcB0();
    void funcB4();
    void funcB8();
    bool funcBC() const;

    // 观察者槽：0..7 为玩家席，8 为观战席。
    int32 ObserverSlot = 8;
    // 战役关卡号：完成战役后落持久战绩码。
    int32 CampaignMission = 0;
    uint32 AccomplishmentCode = 0;
};
