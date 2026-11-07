#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// TEventKind —— 触发条件种类。
//
// 数值来自 asm 的 `EventClass.eventKind`（+0x2C）。EventClass_HasOccured 的
// 三个跳转表（off_71F218/off_71F27C/off_71F328）覆盖 0..61：kind 0 是
// "无事件"，kind 1..61 是可判定事件。下面按 asm 的 kind 数值表给出语义名，
// 不做臆造（表驱动的种类保持结构性命名）。
//
// 注意：这**不是** Definitions.h 里的 TriggerEventType（那是事件广播类型），
// 二者数值体系不同，勿混用。
// ============================================================================
enum class TEventKind : int32 {
    None                      = 0,
    // 1..12 —— 对象交互类事件（kind==广播类型时才会命中）
    EnteredBy                 = 1,
    SpiedBy                   = 2,
    ThievedBy                 = 3,
    DiscoveredBy              = 4,
    AttackedBy                = 5,   // 不要求 kind 匹配
    DestroyedBy               = 6,
    AnyEvent                  = 7,   // 不要求 kind 匹配
    HouseDiscovered           = 8,   // 不要求 kind 匹配
    AttackedOrDestroyed       = 9,
    C4Planted                 = 10,
    TeamCreated               = 11,  // Team != 0 分支
    AnyHouseEvent             = 12,
    // 13..48 —— 世界/方势力状态类事件
    TimeElapsed               = 13,  // SecondsHaveElapsed 首落点
    MissionTimerExpired       = 14,
    BuildingExists            = 15,
    LowPower                  = 16,
    AllBuildingsDestroyed     = 17,
    AllUnitsDestroyed         = 18,
    AllInfantryDestroyed      = 19,
    NoFactoriesLeft           = 20,
    EvacTrigger               = 21,
    CivEvacTrigger            = 22,
    AllBuildingsOfTypeGone    = 23,
    AllUnitsOfTypeGone        = 24,
    AllInfantryOfTypeGone     = 25,
    GlobalSet                 = 26,
    GlobalClear               = 27,
    ElapsedTimeExceeded       = 28,
    MissionTimerExpiredAlt    = 29,
    GlobalSetAlt              = 30,
    GlobalClearAlt            = 31,
    ElapsedTimeAlt            = 32,
    LocalSet                  = 36,
    LocalClear                = 37,
    AmbientLightBelow         = 49,
    AmbientLightNotBelow      = 50,
    ScenarioTimeElapsed       = 51,
    TechnoTypeOnMap           = 59,
    TechnoTypeNotOnMap        = 60,
    SecondsHaveElapsed        = 61
};

class AbstractClass;
class CCINIClass;
class HouseClass;
class TechnoClass;
class TechnoTypeClass;
class TeamTypeClass;
class TriggerClass;
class CRCEngine;
class IStream;

// ============================================================================
// TEventClass —— 触发条件（原版 EventClass）。
//
// 与 TActionClass / TagTypeClass 同族：AbstractTypeClass 派生，走 INI 的
// 数组型类型。asm 结构（sizeof = 0x58）：
//   vt            +0x00   (+0x04/+0x08/+0x0C 是 IRTTITypeInfo/INoticeSink/
//                          INoticeSource 三个接口 vtable)
//   a             +0x04   AbstractClass 子对象
//   idxInArray    +0x24   在 vec_Events 里的下标缓存
//   chainedEvent  +0x28   EventClass* 链（Swizzle 槽）
//   eventKind     +0x2C   事件种类
//   Team          +0x30   TeamTypeClass*（事件参数为队伍类型时使用）
//   param         +0x34   通用整数参数（下标 / 阈值 / 路点号 / 队伍号 …）
//   TechnoININame +0x38   char[28] 内嵌对象名（事件参数为建筑/单位类型时）
//   House         +0x54   HouseClass* 运行时解析出的关联方（HasOccured 写回）
//
// 外部沿用旧名：EventKind = eventKind，Data = param，EventIndex = 保存/加载
// 串里的“参数种类 tag”(0=数值/1=队伍/2=对象名)，ID 来自基类。
// ============================================================================
class TEventClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::Event;

    static DynamicVectorClass<TEventClass*>* Array;

    static TEventClass* Find(const char* pID);
    static TEventClass* FindOrAllocate(const char* pID);
    static TEventClass* FindByName(const char* pName);

    TEventClass(const char* pID) noexcept;
    explicit TEventClass(noinit_t) noexcept;
    virtual ~TEventClass();

    // ------------------------------------------------------------------------
    // IPersistStream / RTTI / CRC（原版 vftable 槽）
    // ------------------------------------------------------------------------
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual AbstractType GetAbstractDerivationID() const;   // asm: TEventClass_GetAbstractDerivationID
    virtual int32 Size() const override;                    // asm: TEventClass_GetSize
    virtual int32 GetArrayIndex() const override;           // asm: TEventClass_GetArrayIndex
    virtual void ComputeCRC(CRCEngine& crc) const override;
    virtual int32 GetCRC() const;                           // asm: EventClass_GetCRC

    // asm: EventClass_PointerGotInvalid —— 被销毁实例若正是 chainedEvent 链的
    // 一环或 Team 槽所指，则修正对应指针。
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // asm: TEventClass_LoadFromINI / TEventClass_SaveToINI
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) override;

    // asm: EventClass_HasOccured —— 判定本事件在当前世界状态下是否成立。
    //   eventKind    本次广播的事件类型（用于和自身 kind 比对）
    //   pHouse       触发者的关联方
    //   pObject      触发对象
    //   pTrigger     触发器实例（提供 TimeElapsed 的倒计时）
    //   isRepeating  [out] true 表示该事件一次性且应被记入已发生位
    //
    // 实现统一收口到 EventClass::HasOccurred（Game/EventClass.cpp，逐条对应
    // asm 的三个跳转表），本函数只做旧签名的转发。
    bool Evaluate(int32 eventKind, HouseClass* pHouse, AbstractClass* pObject,
                  TriggerClass* pTrigger, bool* isRepeating);

    // 兼容旧调用：TriggerClass::CheckConditions / Spring 走这里。
    bool IsSatisfied(TriggerClass* pTrigger) const;
    bool MatchEvent(TriggerEventType eventType, AbstractClass* pObject,
                    CellStruct cell, TriggerClass* pTrigger) const;

    void GetEventName(char* buffer, int32 bufferSize) const;

    // asm TEventClass_SaveToINI 的串写出目标（TriggerTypeClass 侧调用）。
    void FormatSaveString(char* buffer, int32 bufferSize) const;
    // asm TEventClass_LoadFromINI 的串解析（field0=kind,field1=tag,field2=value[,name]）。
    bool ParseSaveString(const char* pString);
    // asm: TEventClass_LoadFromINI —— 从 token 数组的 *pIndex 处解析一个事件元素
    // （kind,tag,value[,name]），推进 *pIndex 并返回消费的 token 数；<0 表示失败。
    // 供 TriggerTypeClass::LoadFromINI 对 [Events] 串逐元素解析。
    int32 ParseTokens(const char* const* tokens, int32 count, int32* pIndex);

private:
    // ------------------------------------------------------------------------
    // 事件参数种类 tag（保存串的第二字段）：0=纯数值, 1=队伍名, 2=对象名。
    // ------------------------------------------------------------------------
    static constexpr int32 ParamType_Number = 0;
    static constexpr int32 ParamType_Team   = 1;
    static constexpr int32 ParamType_Techno = 2;

public:
    // asm 语义字段（+0x24 .. +0x54）。EventKind/EventIndex/Data 为外部沿用名。
    int32 EventKind;                 // +0x2C  eventKind

    TeamTypeClass* Team;             // +0x30  Team
    int32 Data;                      // +0x34  param（通用整数参数）
    char TechnoININame[0x1C];        // +0x38  内嵌对象名 (28 字节)
    HouseClass* House;               // +0x54  运行时关联方

    // 保存串第二字段的参数种类 tag（0/1/2），与 ASM 的 SaveToINI/LoadFromINI 对齐。
    int32 EventIndex;                // 逻辑上映射 asm 保存串的 tag 字段

    // +0x28 链指针（Swizzle 槽）。
    TEventClass* chainedEvent;
    // +0x24 数组下标缓存。
    int32 idxInArray;
};
