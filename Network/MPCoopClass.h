#pragma once

#include "MPGameModeClass.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// MPCooperative (本工程 MPCoopClass) —— 合作战役多人模式
//
//   原版 vftable 见 asm:2045463（.rdata:MPCooperative::`vftable'），全部槽位
//   的实体落在 asm:939973..947127。本文件按槽位逐条还原。
//
//   对象布局（只保留 asm 直接寻址过的字段，偏移取自 asm 本身）：
//     +0x040  CoopGameInfoRecord* Info      —— 当前进度记录（sub_49B610 构造）
//     +0x324  DynamicVectorClass<Record*>   —— 每个战役一条记录（func10 清空、
//                                              func18 取用 [this+0x328][i]）
//     +0x33C  int32 CampaignIndex           —— 当前战役下标（sub_49E3B0 的参数）
//     +0x340  bool  Dirty                   —— 面板需要重画（func24 置 1）
//     +0x048  bool  LoadPending             —— func18 读完存档后置 1
//     +0x04C  int32 SelectedSlot            —— func18 换选战役时写回
//     +0x1A4  8 组 (地图号表, AI 号表)，stride 0x18 —— func64/func74
//
//   注意：本工程 MPGameModeClass 的虚表顺序是重构过的，槽位名按原版
//   funcXX 保留以便逐条对照 asm，不追求与 MPGameModeClass 的字节对齐。
// ============================================================================

class CoopGameInfoRecord;

// ---------------------------------------------------------------------------
// CoopScorePacket —— func5C/sub_49CC50 传进来的「一份成绩包」。
//
//   asm sub_49CC50 一次收两份 0x70 字节的包，逐字段累加到记录的 +0x48..+0x64。
//   从取值偏移反推，每份包的有效字段是：
//     +0x38 Score ；+0x48 Kills ；+0x58 Lost ；+0x68 Time
//   （asm: mov edx,[esp+arg_48] 即 p1+0x48，arg_B8 即 p2+0x48，依此类推）
// ---------------------------------------------------------------------------
struct CoopScorePacket
{
    int32 Score;    // +0x38
    int32 Kills;    // +0x48
    int32 Lost;     // +0x58
    int32 Time;     // +0x68
};

// ---------------------------------------------------------------------------
// CoopGameInfoRecord —— 原版 0x70 字节的进度记录（GameInfo）
//
//   asm: sub_49B610 构造 / sub_49B720 析构 / sub_49B660 写名字 /
//        sub_49B760 由两个名字下标重填 / sub_49BC80 取战役 / sub_49BCA0 取
//        地图数 / sub_49BCC0 取当前地图 / sub_49CAF0 选战役 /
//        sub_49CC50 累加成绩 / sub_49CCF0 重置 / sub_49D070 取地图名
//        （以上均在 asm 0x49B610..0x49D120 段内）
//
//   字段顺序与 asm 偏移一一对应，不做任何重排：
//     +00  Name1[0x14]   +14 House1   +18 Color1
//     +1C  Name2[0x14]   +30 House2   +34 Color2
//     +38  CurrentMap    +3C MapNames +40 MapCount  +44 CampaignIndex
//     +48  Kills1  +4C Kills2  +50 Lost1  +54 Lost2
//     +58  Score1  +5C Score2  +60 Time1  +64 Time2  +68 Time
//     +6C  Loaded
// ---------------------------------------------------------------------------
class CoopGameInfoRecord
{
public:
    CoopGameInfoRecord();                          // asm: sub_49B610
    ~CoopGameInfoRecord();                         // asm: sub_49B720

    void Reset();                                  // asm: sub_49B610
    bool SetNames(int32 n1, int32 n2);             // asm: sub_49B660
    void SetNames(const char* p1, const char* p2); // asm: sub_49B760
    bool GetCurrentMap(int32* out) const;          // asm: sub_49BCC0
    bool GetCampaignIndex(int32* out) const;       // asm: sub_49BC80
    bool GetMapCount(int32* out) const;            // asm: sub_49BCA0
    bool SelectCampaign(int32 idx);                // asm: sub_49CAF0
    bool ResetForCampaign(int32 idx, int32 mapCount);  // asm: sub_49CCF0
    bool ApplyStats(int32 mapIndex, const CoopScorePacket& a,
                    const CoopScorePacket& b, int32 time);  // asm: sub_49CC50
    const char* GetMapName(int32 map) const;       // asm: sub_49D070
    bool Load();                                   // asm: sub_49C0D0
    bool Save() const;                             // asm: sub_49D680

    char    Name1[0x14];
    int32   House1;
    int32   Color1;
    char    Name2[0x14];
    int32   House2;
    int32   Color2;
    int32   CurrentMap;
    char**  MapNames;
    int32   MapCount;
    int32   CampaignIndex;
    int32   Kills1;
    int32   Kills2;
    int32   Lost1;
    int32   Lost2;
    int32   Score1;
    int32   Score2;
    int32   Time1;
    int32   Time2;
    int32   Time;
    bool    Loaded;
};

// ---------------------------------------------------------------------------
// 合作战役表的地图清单由 Scenario/CoopCampaignClass 负责
// （CoopCampaignClass::Get / GetCount / NumberOfCampaignMaps / Maps）。
// 这里只补它没有解析的两张逐地图名单：
//   [<section>] CampaignPlayer%d = "A,B,C"   玩家可用国家
//   [<section>] CampaignEnemy%d  = "A,B,C"   AI 可用国家
// 键名与读取顺序见 asm CoopCampaignClass_LoadFromINIList (0x49DB19) 里
// "CampaignPlayer"/"CampaignEnemy" + sprintf("%s%d") 两段。
// 原版把它们存在战役条目 +0x10 / +0x28 的两个 DynamicVectorClass 里，
// 每格是 DynamicVectorClass<char*>*（元素 +4 = 名字数组，+0x10 = 个数）。
// ---------------------------------------------------------------------------
namespace CoopMapHouses
{
    int32       Count(int32 campaignIndex, int32 mapIndex);          // [vec+0x10]
    const char* Name(int32 campaignIndex, int32 mapIndex, int32 i);  // [vec+4][i]
    int32       AICount(int32 campaignIndex, int32 mapIndex);
    const char* AIName(int32 campaignIndex, int32 mapIndex, int32 i);
}

// ---------------------------------------------------------------------------
// 参战名册（原版 NameNodeTypes）
//
//   原版是 NameNodeClass* 的全局向量：每个连接席位一项，带国家号
//   （sub_696F30 读 / sub_696F90 写）与席位号（[node+0x6B]）。
//   本工程这些信息落在 SessionClass::Players 上，这里做一层转发。
// ---------------------------------------------------------------------------
namespace CoopNameNodes
{
    int32       Count();                       // asm: NameNodeTypes.length
    const char* Name(int32 i);
    int32       Country(int32 i);              // asm: sub_696F30
    void        SetCountry(int32 i, int32 c);  // asm: sub_696F90
    int32       Slot(int32 i);                 // asm: [node+0x6B]
    void        SetSlot(int32 i, int32 s);
}

class MPCoopClass : public MPGameModeClass {
public:
    MPCoopClass() noexcept;
    virtual ~MPCoopClass();

    // asm: MPCoop_func04 (0x5C4EA0) —— mov al,1
    bool func04() const;
    // asm: MPCoop_func08 (0x5C4EB0) —— mov al,1
    bool func08() const;
    // asm: MPCoop_func14 (0x5C1DB0) —— 名册两项时重填记录，再转基类 ret1_0
    int32 func14(int32 a1);
    // asm: MPCoop_func18 (0x5C1DD0) —— 战役选择 / 记录装载
    bool func18();
    // asm: MPCoop_func1C (0x5C17C0) —— 该国是否在当前地图的玩家名单里
    bool func1C(int32 houseOrdinal);
    // asm: MPCoop_func20 (0x5C1A20) —— 观察者席位（"O8"）与战役反查
    void func20();
    // asm: MPCoop_func24 (0x5C1CF0) —— mov byte [ecx+340h],1
    void func24();
    // asm: MPCoop_func28 (0x5C1FB0) —— 当前地图唯一国家号，否则 -2
    int32 func28();
    // asm: MPCoop_func38 (0x5C2640) —— 逐个席位重建地图选择控件
    void func38();
    // asm: MPCoop_func44 (0x5C1CE0) —— 战役首图 -> 场景下标
    int32 func44();
    // asm: MPCoop_func4C (0x5C3A40) —— 面板初始化：建记录、填存档列表
    void func4C();
    // asm: MPCoop_func58 (0x5C2040) —— 换选战役
    bool func58(int32 campaignIndex);
    // asm: MPCoop_func5C (0x5C2100) —— 结算两份成绩包
    bool func5C(const CoopScorePacket& a, const CoopScorePacket& b, int32 time);
    // asm: MPCoop_func60 (0x5C2680) —— 落盘（sub_49D680）
    bool func60();
    // asm: MPCoop_func64 (0x5C26D0) —— 拼 "C1,..." AI 设置串并发出去
    bool func64();
    // asm: MPCoop_func68 (0x5C30C0) —— 按剧本人数补 AI 席位
    bool func68();
    // asm: MPCoop_func6C (0x5C34E0) —— 从当前地图玩家名单随机取一国
    //   基类槽 func6C 是 void，原版的 eax 返回值改由 RandomHousePick 带出。
    void func6C();
    // asm: MPCoop_func70 (0x5C3610) —— 从当前地图敌方名单随机取一国
    int32 func70();
    // asm: MPCoop_func74 (0x5C2D70) —— 解析 "C1,..." 串回填 8 组槽位
    void func74(int32 mapCount, const char* pList);
    // asm: MPCoop_func78 (0x5C2190) —— 向 0x732 控件投递刷新消息
    void func78();
    // asm: MPCoop_func7C (0x5C2CF0) —— 面板刷新 + 恒定成功
    bool func7C();
    // asm: MPCoop_func90 (0x5C3F80) —— 把各席位状态回显成文本
    void func90();
    // asm: MPCoop_func94 (0x5C40A0) —— 下一关卡记录入列
    void func94();
    // asm: MPCoop_func98 (0x5C3FA0) —— mov eax,2
    int32 func98() const;
    // asm: MPCoop_func9C (0x5C3F30) —— 重置面板 + 名册席位编号
    void func9C();
    // asm: MPCoop_funcA0 (0x5C3760) —— 结算：判定人类存续并落战绩
    void funcA0();
    // asm: MPCoop_funcA4 (0x5C4510) —— 生成一条结算成绩项
    void funcA4(bool finished);
    // asm: MPCoop_funcA8 (0x5C4290) —— 结算统计入列
    void funcA8();
    // asm: MPCoop_funcAC (0x5C4440) —— 组开局广播包
    void funcAC();
    // asm: MPCoop_funcB0 (0x5C4CE0) —— 扫存档槽，找本战役续玩点
    void funcB0();
    // asm: MPCoop_funcB4 (0x5C4E90) —— 记录存在时释放缓冲并返回 true
    bool funcB4(void* memory, int32 a2, int32 a3, int32 a4, int32 a5, int32 a6);
    // asm: MPCoop_funcB8 (0x5C4660) —— 战役打完时 1 << 战役号
    int32 funcB8();
    // asm: MPCoop_funcBC (0x5C4ED0) —— xor al,al
    bool funcBC() const;

    // asm: sub_5C1980 (0x5C1980) —— 用 "场景名" 在当前战役表的三组地图名
    //   里反查战役下标；找不到返回 -1。
    int32 FindCampaignByMapName(const char* pName) const;

    // ── asm 直接寻址到的字段 ────────────────────────────────────────────
    CoopGameInfoRecord* Info = nullptr;            // [this+0x040]
    // asm: [this+0x324] —— 每个战役一条记录；func10 逐项释放
    DynamicVectorClass<CoopGameInfoRecord*> Records;
    int32 CampaignIndex = 0;                       // [this+0x33C]
    bool  Dirty = false;                           // [this+0x340]
    bool  LoadPending = false;                     // [this+0x048]
    int32 SelectedSlot = -1;                       // [this+0x04C]
    // asm: MPCoop_func6C 的 eax 返回值（基类槽是 void，结果落在这里）
    int32 RandomHousePick = 0;

    // asm: [this+0x1A4] —— 8 组（地图号表, AI 号表），stride 0x18；
    //   func64 组串、func74 解析都按这个顺序读写。
    static constexpr int32 AI_SLOT_COUNT = 8;

    struct AISlot
    {
        DynamicVectorClass<int32> Maps;   // 原版 VectorClass<int>
        DynamicVectorClass<int32> AIs;    // 原版 VectorClass<int>
    };
    AISlot AISlots[AI_SLOT_COUNT];

    // asm: dword_ABF460 —— func64/func74 用的 8 项「已占用 AI 号」表
    int32 AIHouseTaken[AI_SLOT_COUNT];
};
