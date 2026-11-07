#pragma once

#include "../Core/Definitions.h"

#include <cstddef>

// ============================================================================
// Networking/MultiplaySync.h
//
// 多人对局「同步日志」（sync log）子系统。
//
// asm 权威：Multiplay_LogToSync_MPDEBUG (0x651720, __fastcall，ecx = 槽位号，
//   edx = 出错的网络事件包指针，可为空)。逐句对照见 .cpp。
//
// 原版把三块模块私有状态散在 .data 里：
//   0xAC6660  8 个同步槽（步长 0x33C），由 sub_650A90 每帧填充；
//   0xA8B5C4  8 组连接统计（步长 0x68，0xA8B904 为末尾）；
//   0xB04474  256 项帧校验和（0xB04874 为末尾）。
// 这三张表都由「多人数同步」模块独占写入，故本工程把它们收进
// SyncLoggerClass 这一唯一所有者，不再另立影子全局。
// ============================================================================

// ----------------------------------------------------------------------------
// 拆出来的地址块：asm sub_53EDA0 从对象 +4 处拷 4 字节 IP、+8 处拷 2 字节端口，
// 从 +0 处拷 4 字节句柄。这里用具名字段表达同一份数据。
// ----------------------------------------------------------------------------
struct SyncAddressClass {
    uint32 IP;      // asm: [obj+4]（打印时按小端拆成 a.b.c.d）
    uint16 Port;    // asm: word [obj+8]
};

// ----------------------------------------------------------------------------
// 单个玩家槽（asm Slot+0Ch + i*5Ch，8 个，步长 0x5C）
//
//   sub_650A90 写入证据：
//     strncpy(base+0x00, 玩家名, 0x14)
//     [base+0x14] = IsHuman
//     strncpy(base+0x15, 颜色方案名, 0x1F)
//     [base+0x34] = 0
//     [base+0x38] = 房子下标（-1 = 空槽，本函数据此跳过）
//     strncpy(base+0x3C, 房型名, 0x1F)
// ----------------------------------------------------------------------------
struct SyncHouseSlotClass {
    char    Name[0x14];             // +0x00
    uint8   IsHuman;                // +0x14
    char    ColorName[0x1F];        // +0x15
    uint8   Unknown34;              // +0x34
    uint8   Reserved35[3];          // +0x35
    int32   HouseID;                // +0x38  -1 = 空
    char    HouseTypeName[0x20];    // +0x3C
};

// ----------------------------------------------------------------------------
// 对象记录（asm 记录池 [Slot+300h] 中的一项，步长 0x68）
//
// 四类对象共用同一头部布局（sub_650A90 逐类写入）：
//   +00 Coord、+04 类型下标、+08 类型名（0x17 字节）、+20 归属房子下标、
//   +24 目标值、+28 目标类名、+40 副目标值、+44 副目标类名、
//   +5C 速度累加（仅车辆）、+60/+62 磁道号/磁道索引（仅车辆）、
//   +64 朝向、+65 第二朝向（仅车辆）、+66 任务、+67 血量。
// ----------------------------------------------------------------------------
struct SyncObjectRecordClass {
    int32   Coord;                  // +0x00  asm "COORD:%x"
    int32   TypeIndex;              // +0x04  asm "Type:%d"
    char    TypeName[0x18];         // +0x08  asm "Type:%d(%s)"
    uint8   Owner;                  // +0x20  归属房子下标
    uint8   Reserved21[3];          // +0x21
    int32   TargetValue;            // +0x24  asm "Tgt:%s(%d)"
    char    TargetName[0x18];       // +0x28  asm "Tgt:%s(%d)"
    int32   NavComValue;            // +0x40  asm "NavCom:%s(%d)"
    char    NavComName[0x18];       // +0x44  asm "NavCom:%s(%d)"
    int32   SpeedAcc;               // +0x5C  asm "SpdAc:%d"（仅车辆）
    int16   TrackNumber;            // +0x60  asm "TrkNum:%d"（仅车辆）
    int16   TrackIndex;             // +0x62  asm "TrkInd:%d"（仅车辆）
    uint8   Facing;                 // +0x64
    uint8   Facing2;                // +0x65  asm "Facing2:%d"（仅车辆）
    uint8   Mission;                // +0x66
    uint8   Health;                 // +0x67  asm "Speed:%d"（步兵血量）
};

// ----------------------------------------------------------------------------
// 图层项（asm 5 个 MAP LAYER 与 LOGIC LAYER 的条目，步长 0x28）
//
//   +00 Coord、+04 类型下标（0x2B/0x13 时写 -1）、+08 类名、
//   +20 归属房子下标（0xFF = 无主）、+24 派生 ID（int16）。
// ----------------------------------------------------------------------------
struct SyncLayerEntryClass {
    int32   Coord;                  // +0x00
    int32   TypeIndex;              // +0x04
    char    TypeName[0x18];         // +0x08
    uint8   Owner;                  // +0x20
    uint8   Reserved21[3];          // +0x21
    int16   DerivationID;           // +0x24
    uint8   Reserved26[2];          // +0x26
};

// ----------------------------------------------------------------------------
// 一个同步槽（asm 0xAC6660 + slot*33Ch）
//
//   sub_650A90 写入证据：
//     [Slot+00] = dword_AC51FC（本帧校验和）
//     [Slot+04] = 场景随机数
//     [Slot+08] = Frame
//     [Slot+0C] = 8 个玩家槽（0x2E0 字节）
//     [Slot+2EC] = 记录总数
//     [Slot+2F0..2FC] = 步兵/车辆/飞机/建筑 计数
//     [Slot+300] = 记录池指针
//     [Slot+304+4i] = 第 i 层计数，[Slot+318+4i] = 第 i 层条目指针
//     [Slot+32C]/[Slot+330] = LOGIC 层计数/条目指针
//     [Slot+334]/[Slot+338] = 日志缓冲指针/容量
// ----------------------------------------------------------------------------
struct SyncSlotClass {
    int32                   Checksum;           // +0x00
    int32                   RandomNumber;       // +0x04
    int32                   Frame;              // +0x08
    SyncHouseSlotClass      Houses[8];          // +0x0C  (0x2E0)
    int32                   RecordCount;        // +0x2EC
    int32                   InfantryCount;      // +0x2F0
    int32                   UnitCount;          // +0x2F4
    int32                   AircraftCount;      // +0x2F8
    int32                   BuildingCount;      // +0x2FC
    SyncObjectRecordClass*  Records;            // +0x300
    int32                   LayerCount[5];      // +0x304
    SyncLayerEntryClass*    LayerEntries[5];    // +0x318
    int32                   LogicCount;         // +0x32C
    SyncLayerEntryClass*    LogicEntries;       // +0x330
    void*                   Buffer;             // +0x334
    int32                   BufferCapacity;     // +0x338
};

// ----------------------------------------------------------------------------
// 一组连接统计（asm unk_A8B5C4 + i*68h，8 组）
//
//   由 sub_53EDA0 的调用点与 sub_48E0B0 的打印点共同确定：
//     +00 名字（0x40 字节，首字节 0 = 空组）
//     +40 最大往返、+44 重发次数、+48 丢包、+4C 丢包百分比、
//     +50 最大平均往返、+54 帧同步停顿、+58 命令计数停顿、
//     +5C 地址句柄、+60 IP、+64 端口。
// ----------------------------------------------------------------------------
struct SyncConnectionStatsClass {
    char        Name[0x40];         // +0x00
    int32       MaxRoundTrip;       // +0x40
    int32       Resends;            // +0x44
    int32       Lost;               // +0x48
    int32       PercentLost;        // +0x4C
    int32       MaxAvgRoundTrip;    // +0x50
    int32       FrameSyncStalls;    // +0x54
    int32       CommandCountStalls; // +0x58
    uint32      AddressHandle;      // +0x5C
    SyncAddressClass Address;       // +0x60 (IP) / +0x64 (Port)

    // asm sub_53EDA0(ecx = this+0x5C, arg0 = &handle, arg1 = &address)：
    //   把 this+0x5C 处的 4 字节拷给 arg0，把 this+0x60 处的 IP 与
    //   this+0x64 处的端口拷给 arg1。
    void GetAddressComponents(uint32& handle, SyncAddressClass& addr) const;
};

// ----------------------------------------------------------------------------
// 出错事件包头部（asm 调用点把 NetworkPackets*.ev.EventKind 的地址传进来，
// 本函数读 +0/+2/+3/+7/+0Bh/+0Dh，故按 1 字节紧凑排布）。
// ----------------------------------------------------------------------------
#pragma pack(push, 1)
struct SyncEventPacketClass {
    uint8   Kind;           // +0x00  asm "Type:         %d"
    uint8   Unknown01;      // +0x01
    int8    PlayerID;       // +0x02  asm "ID:           %x"
    int32   Frame;          // +0x03  asm "Frame:        %d"
    uint32  CRC;            // +0x07  asm "CRC:          %x"
    uint16  CommandCount;   // +0x0B  asm "CommandCount: %d"
    uint8   Delay;          // +0x0D  asm "Delay:        %d"
};
#pragma pack(pop)

static_assert(sizeof(SyncEventPacketClass) == 0x0E,
              "SyncEventPacketClass 必须与网络包头部 14 字节一致");

// ============================================================================
// SyncLoggerClass —— 同步日志的唯一所有者
//
// 对应原版 0xAC6660 / 0xA8B5C4 / 0xB04474 三块模块私有的同步状态：
//   每帧先由同步记录组装（asm sub_650A90）填好槽位，再由本类的
//   LogToSync_MPDEBUG 把槽位内容与连接统计、校验和表一起落盘。
// ============================================================================
class SyncLoggerClass {
public:
    static constexpr int32 MaxSlots        = 8;    // asm 0xAC6660 表长
    static constexpr int32 MaxHouses       = 8;    // asm 每槽 8 个玩家位
    static constexpr int32 MaxConnections  = 8;    // asm 0xA8B5C4 表长
    static constexpr int32 ChecksumCount   = 256;  // asm 0xB04474 表长
    static constexpr int32 MapLayerCount   = 5;    // asm Slot+304h 的层数

    // 进程内唯一实例（原版是散落的全局数据，这里由本模块自己持有）
    static SyncLoggerClass* GetInstance();

    SyncLoggerClass() noexcept;
    ~SyncLoggerClass();

    SyncSlotClass& GetSlot(int32 index) { return Slots[index]; }
    SyncConnectionStatsClass& GetConnectionStats(int32 index)
    {
        return ConnectionStats[index];
    }

    // asm: loc_6472FC 的 dword_B04474[Frame & 0FFh] = dword_AC51FC
    //   —— 每帧把本帧校验和写进环形表，供下面的 CRC 段打印。
    void Store_Checksum(int32 frame, uint32 checksum)
    {
        ChecksumTable[frame & (ChecksumCount - 1)] = checksum;
    }

    // ------------------------------------------------------------------------
    // asm 0x651720: void __fastcall Multiplay_LogToSync_MPDEBUG(int slot, Event*)
    //
    //   slotIndex —— 本机槽位（同时决定文件名 SYNC<玩家号>_<槽位>.TXT）
    //   pEvent    —— 出错网络包，可为空（正常帧传 0）
    //
    // 输出顺序（与 asm 完全一致）：
    //   头部（版本/构建/CPU/帧/FPS/MaxAhead/Latency/游戏速度/FrameSendRate/
    //   本槽帧号/槽位） → 本机全部连接地址 → 8 组连接统计 →
    //   256 项 CRC（本槽标 *****）→ 8 个玩家槽详情 →
    //   按房子下标 0..最后有效下标，逐房输出 步兵/车辆/飞机/建筑 四段 →
    //   5 个 MAP LAYER → LOGIC LAYER → 随机数 → 出错事件 → 关闭文件
    // ------------------------------------------------------------------------
    void LogToSync_MPDEBUG(int32 slotIndex, const SyncEventPacketClass* pEvent);

    // ------------------------------------------------------------------------
    // asm 0x64E1C4: void __cdecl Multiplay_LogToSYNC_NOMPDEBUG(Event*)
    //
    //   MPDEBUG 变体的"无调试输出"编译版：文件名只有玩家号（SYNC<玩家号>.TXT），
    //   不写槽位号，也不做 CRC 行高亮；此外它把实时对象表（步兵/车辆/飞机/
    //   建筑/图层/vec_Logics/全类型 CRC）逐一转储，并向 #4 单色屏同步输出。
    //
    //   pEvent —— 出错网络包，可为空（正常帧传 0）。
    // ------------------------------------------------------------------------
    void LogToSync_NoDebug(const SyncEventPacketClass* pEvent);

    SyncSlotClass               Slots[MaxSlots];
    SyncConnectionStatsClass    ConnectionStats[MaxConnections];
    uint32                      ChecksumTable[ChecksumCount];
};

// 布局自检（与 asm 的步长/偏移逐一对应）
static_assert(sizeof(SyncHouseSlotClass) == 0x5C, "玩家槽步长必须是 0x5C");
static_assert(offsetof(SyncHouseSlotClass, IsHuman) == 0x14, "IsHuman 偏移");
static_assert(offsetof(SyncHouseSlotClass, ColorName) == 0x15, "Color 偏移");
static_assert(offsetof(SyncHouseSlotClass, HouseID) == 0x38, "HouseID 偏移");
static_assert(offsetof(SyncHouseSlotClass, HouseTypeName) == 0x3C, "HouseType 偏移");

static_assert(sizeof(SyncObjectRecordClass) == 0x68, "记录步长必须是 0x68");
static_assert(offsetof(SyncObjectRecordClass, TypeName) == 0x08, "类型名偏移");
static_assert(offsetof(SyncObjectRecordClass, Owner) == 0x20, "归属偏移");
static_assert(offsetof(SyncObjectRecordClass, TargetValue) == 0x24, "目标值偏移");
static_assert(offsetof(SyncObjectRecordClass, TargetName) == 0x28, "目标名偏移");
static_assert(offsetof(SyncObjectRecordClass, NavComValue) == 0x40, "副目标值偏移");
static_assert(offsetof(SyncObjectRecordClass, NavComName) == 0x44, "副目标名偏移");
static_assert(offsetof(SyncObjectRecordClass, SpeedAcc) == 0x5C, "速度偏移");
static_assert(offsetof(SyncObjectRecordClass, TrackNumber) == 0x60, "磁道号偏移");
static_assert(offsetof(SyncObjectRecordClass, Facing) == 0x64, "朝向偏移");
static_assert(offsetof(SyncObjectRecordClass, Mission) == 0x66, "任务偏移");
static_assert(offsetof(SyncObjectRecordClass, Health) == 0x67, "血量偏移");

static_assert(sizeof(SyncLayerEntryClass) == 0x28, "图层项步长必须是 0x28");
static_assert(offsetof(SyncLayerEntryClass, Owner) == 0x20, "图层归属偏移");
static_assert(offsetof(SyncLayerEntryClass, DerivationID) == 0x24, "派生 ID 偏移");

static_assert(offsetof(SyncSlotClass, Houses) == 0x0C, "玩家槽起点");
static_assert(offsetof(SyncSlotClass, InfantryCount) == 0x2F0, "步兵计数偏移");
static_assert(offsetof(SyncSlotClass, UnitCount) == 0x2F4, "车辆计数偏移");
static_assert(offsetof(SyncSlotClass, AircraftCount) == 0x2F8, "飞机计数偏移");
static_assert(offsetof(SyncSlotClass, BuildingCount) == 0x2FC, "建筑计数偏移");
static_assert(offsetof(SyncSlotClass, Records) == 0x300, "记录池偏移");
// 记录池之后的图层表（asm 0x304/0x318）、LOGIC 表（0x32C/0x330）与日志缓冲
// （0x334/0x338）在原版里是 32 位指针，本工程按目标平台的自然对齐排布，
// 故只校验到指针块起点为止；字段序号与 asm 完全一致。

static_assert(sizeof(SyncConnectionStatsClass) == 0x68, "连接统计步长必须是 0x68");
static_assert(offsetof(SyncConnectionStatsClass, MaxRoundTrip) == 0x40, "最大往返偏移");
static_assert(offsetof(SyncConnectionStatsClass, Resends) == 0x44, "重发偏移");
static_assert(offsetof(SyncConnectionStatsClass, Lost) == 0x48, "丢包偏移");
static_assert(offsetof(SyncConnectionStatsClass, PercentLost) == 0x4C, "丢包率偏移");
static_assert(offsetof(SyncConnectionStatsClass, MaxAvgRoundTrip) == 0x50, "往返偏移");
static_assert(offsetof(SyncConnectionStatsClass, AddressHandle) == 0x5C, "句柄偏移");
static_assert(offsetof(SyncConnectionStatsClass, Address) == 0x60, "地址偏移");
