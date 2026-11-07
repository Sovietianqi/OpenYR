// ============================================================================
// Networking/MultiplaySync.cpp
//
// asm 权威：Multiplay_LogToSync_MPDEBUG (0x651720, __fastcall，
//   ecx = 槽位号，edx = 出错事件包指针)。起始标签与本文的段落一一对应：
//     loc_651782  文件头（版本 / 构建 / CPU / 帧 / FPS / MaxAhead / Latency /
//                 游戏速度 / FrameSendRate / 本槽帧号 / 槽位）
//     loc_651903  本机 UDP 连接地址
//     loc_651955  8 组连接统计（unk_A8B5C4 .. unk_A8B904）
//     loc_651A3B  256 项 CRC（dword_B04474 .. dword_B04874）
//     loc_651A7A  8 个玩家槽详情
//     loc_651AD1  步兵段 / loc_651BA3 车辆段 / loc_651C88 飞机段 /
//     loc_651D55  建筑段（均按房子下标重扫一遍记录池）
//     loc_651DF8  5 个 MAP LAYER / loc_651F04 LOGIC LAYER
//     loc_651FBD  随机数 + 出错事件 + _fclose
//
// 本文件不再经过任何 bind / 函数指针表：文件、版本、CPU、地址与类名全部
// 直调本工程的真实实现（CRT、Globals、ConnectionManager、
// AbstractClass::GetClassName）。
// ============================================================================

#include "Networking/MultiplaySync.h"

#include "Abstract/AircraftClass.h"
#include "Abstract/AircraftTypeClass.h"
#include "Abstract/AbstractClass.h"
#include "Abstract/BuildingClass.h"
#include "Abstract/BuildingLightClass.h"
#include "Abstract/BuildingTypeClass.h"
#include "Abstract/FootClass.h"
#include "Abstract/InfantryClass.h"
#include "Abstract/InfantryTypeClass.h"
#include "Abstract/OverlayTypeClass.h"
#include "Abstract/SmudgeTypeClass.h"
#include "Abstract/TechnoClass.h"
#include "Abstract/TechnoTypeClass.h"
#include "Abstract/UnitClass.h"
#include "Abstract/UnitTypeClass.h"
#include "Abstract/VoxelAnimClass.h"
#include "Abstract/VoxelAnimTypeClass.h"
#include "AI/ScriptClass.h"
#include "AI/ScriptTypeClass.h"
#include "AI/TActionClass.h"
#include "AI/TEventClass.h"
#include "AI/TagClass.h"
#include "AI/TagTypeClass.h"
#include "AI/TaskForceClass.h"
#include "AI/TeamClass.h"
#include "AI/TeamTypeClass.h"
#include "AI/TriggerClass.h"
#include "AI/TriggerTypeClass.h"
#include "Animations/AnimClass.h"
#include "Animations/AnimTypeClass.h"
#include "Combat/BulletClass.h"
#include "Combat/BulletTypeClass.h"
#include "Combat/WarheadTypeClass.h"
#include "Combat/WeaponTypeClass.h"
#include "Containers/DynamicVectorClass.h"
#include "Game/Externs.h"
#include "Game/Game.h"
#include "Game/LogicClass.h"
#include "Houses/FactoryClass.h"
#include "Houses/HouseClass.h"
#include "Houses/HouseTypeClass.h"
#include "Houses/SideClass.h"
#include "Map/TubeClass.h"
#include "Map/WaypointPathClass.h"
#include "Network/ConnectionClass.h"
#include "Network/MPModes.h"
#include "Network/Networking.h"
#include "Network/SessionClass.h"
#include "Particles/ParticleClass.h"
#include "Particles/ParticleSystemClass.h"
#include "Particles/ParticleSystemTypeClass.h"
#include "Particles/ParticleTypeClass.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/DisplayClass.h"
#include "Scenario/ScenarioClass.h"
#include "Special/EMPulseClass.h"
#include "Special/LightSourceClass.h"
#include "Special/TiberiumClass.h"
#include "UI/MonoScreenClass.h"

#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
    #include <cpuid.h>
#endif

namespace {

// ----------------------------------------------------------------------------
// 引擎调试输出通道（asm WWDebugString）。沿用本树其它子系统（PKTNodeClass /
// ScoreScreenClass）的约定：落到 stderr。
// ----------------------------------------------------------------------------
void WWDebugString(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    std::vfprintf(stderr, pFormat, args);
    va_end(args);
}

// ----------------------------------------------------------------------------
// 构建信息（asm GetModuleVersion / GetModuleInternalVersion /
// GetModuleBuildTimestamp / GetModuleBuildVersion / GetModuleBuildAuthor）
//
// 原版从 PE 版本资源与模块内嵌的链接时间戳取值；本工程用 Globals 里的真实
// 版本常量与编译期 __DATE__/__TIME__ 表达同一份信息。
// ----------------------------------------------------------------------------
const char* ModuleVersionString()
{
    return g_GameVersionString;
}

// asm: sprintf(dest, "%d.%3.3d", 主版本, 次版本)
const char* ModuleInternalVersionString()
{
    static char s_buffer[16] = {0};
    if (s_buffer[0] == '\0') {
        std::sprintf(s_buffer, "%d.%3.3d",
                     static_cast<int>(g_GameVersionMajor),
                     static_cast<int>(g_GameVersionMinor));
    }
    return s_buffer;
}

// asm: sprintf(dest, "%d", BuildVersion)
const char* ModuleBuildVersionString()
{
    static char s_buffer[16] = {0};
    if (s_buffer[0] == '\0') {
        std::sprintf(s_buffer, "%d", static_cast<int>(g_GameVersionBuild));
    }
    return s_buffer;
}

// asm: strncpy(dest, "gsmallwoo", 20h)
const char* ModuleBuildAuthorString()
{
    return "gsmallwoo";
}

// asm GetModuleBuildTimestamp：FILETIME -> SYSTEMTIME ->
//   sprintf("%02d/%02d/%04d - %02d:%02d:%02d", 月, 日, 年, 时, 分, 秒)
const char* ModuleBuildTimestampString()
{
    static char s_buffer[0x40] = {0};
    if (s_buffer[0] != '\0') {
        return s_buffer;
    }

    // __DATE__ = "Mmm dd yyyy"（日可能带前导空格），__TIME__ = "hh:mm:ss"
    const char* const pDate = __DATE__;
    const char* const pTime = __TIME__;

    static const char* const kMonths[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    int32 month = 1;
    for (int32 i = 0; i < 12; ++i) {
        if (std::strncmp(pDate, kMonths[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }

    const int32 day  = std::atoi(pDate + 4);
    const int32 year = std::atoi(pDate + 7);

    int32 hour = 0;
    int32 minute = 0;
    int32 second = 0;
    std::sscanf(pTime, "%d:%d:%d", &hour, &minute, &second);

    std::sprintf(s_buffer, "%02d/%02d/%04d - %02d:%02d:%02d",
                 static_cast<int>(month), static_cast<int>(day),
                 static_cast<int>(year), static_cast<int>(hour),
                 static_cast<int>(minute), static_cast<int>(second));
    return s_buffer;
}

// ----------------------------------------------------------------------------
// CPU 探测（asm sub_7C5F3D/sub_7C5FFF 填厂牌、族、型号；
// GetCPUMMXSupport 取 MMX 位；GetCPUType 查 strlist_CPUTypes）
// ----------------------------------------------------------------------------
void CpuId(uint32 leaf, uint32 out[4])
{
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;

#if defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuid(regs, static_cast<int>(leaf));
    out[0] = static_cast<uint32>(regs[0]);
    out[1] = static_cast<uint32>(regs[1]);
    out[2] = static_cast<uint32>(regs[2]);
    out[3] = static_cast<uint32>(regs[3]);
#elif defined(__i386__) || defined(__x86_64__)
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;
    __cpuid_count(static_cast<unsigned int>(leaf), 0, a, b, c, d);
    out[0] = static_cast<uint32>(a);
    out[1] = static_cast<uint32>(b);
    out[2] = static_cast<uint32>(c);
    out[3] = static_cast<uint32>(d);
#else
    (void)leaf;
#endif
}

// asm GetCPUMMXSupport：MMX 支持位（CPUID.1:EDX[23]）写进调用方的标志字节
bool CpuSupportsMmx()
{
    uint32 regs[4];
    CpuId(1, regs);
    return (regs[3] & (1u << 23)) != 0;
}

bool StrEqualNoCase(const char* pLeft, const char* pRight)
{
    while (*pLeft != '\0' && *pRight != '\0') {
        char a = *pLeft++;
        char b = *pRight++;
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return *pLeft == *pRight;
}

// asm GetCPUType：厂牌 "AuthenticAMD"/"CyrixInstead" 先行，其余按族/型号查表。
const char* CpuTypeName()
{
    static const char* const kCpuTypes[9] = {
        "486",          // 0
        "Pentium",      // 1
        "Pentium Pro",  // 2
        "Pentium II",   // 3
        "Pentium III",  // 4
        "Celeron",      // 5
        "Pentium 4",    // 6
        "AMD",          // 7
        "Cyrix"         // 8
    };

    uint32 vendor[4];
    CpuId(0, vendor);

    char szVendor[13];
    std::memcpy(szVendor + 0, &vendor[1], 4);   // EBX
    std::memcpy(szVendor + 4, &vendor[3], 4);   // EDX
    std::memcpy(szVendor + 8, &vendor[2], 4);   // ECX
    szVendor[12] = '\0';

    if (StrEqualNoCase(szVendor, "AuthenticAMD")) {
        return kCpuTypes[7];
    }
    if (StrEqualNoCase(szVendor, "CyrixInstead")) {
        return kCpuTypes[8];
    }

    uint32 regs[4];
    CpuId(1, regs);
    const uint32 eax = regs[0];

    int32 family = static_cast<int32>((eax >> 8) & 0x0F);
    int32 model  = static_cast<int32>((eax >> 4) & 0x0F);
    if (family == 6 || family == 15) {
        family += static_cast<int32>((eax >> 20) & 0xFF);
        model  += static_cast<int32>((eax >> 16) & 0x0F) << 4;
    }

    // asm: 族为 0 或型号为 0 时返回 -1 -> "Unknown"
    if (family == 0 || model == 0) {
        return "Unknown";
    }

    switch (family) {
    case 4:
        return kCpuTypes[0];
    case 5:
        return kCpuTypes[1];
    case 6:
        switch (model) {
        case 3:
        case 5:
            return kCpuTypes[3];
        case 6:
            return kCpuTypes[5];
        case 7:
        case 8:
        case 10:
            return kCpuTypes[4];
        default:
            return kCpuTypes[2];
        }
    default:
        return kCpuTypes[6];
    }
}

// ----------------------------------------------------------------------------
// asm 各 fprintf 的格式串：逐字节取自 .data（含 "Command cound stalls" 的
// 原文拼写错误）。
// ----------------------------------------------------------------------------
const char kFmtSyncName[]       = "SYNC%01d_%03d.TXT";
const char kFmtWritingSync[]    = "Writing sync data to file %s\n";
const char kFmtOpenFail[] =
    "Error - failed to open sync file for writing. Error code %d - %s\n";
const char kFmtRa2Sync[]        = "RA2 Sync\n";
const char kFmtVersion[]        = "\nVersion %s\n";
const char kFmtInternalVersion[] = "Internal Version %s\n";
const char kFmtReleaseBuild[]   = "Release Build: %s by %s - %s\r\n";
const char kFmtCpu[]            = "CPU %s, MMX %s, Vendor: %s\r\n";
const char kFmtFrames[]         = "Frames: %d\n";
const char kFmtAverageFps[]     = "Average FPS: %d\n";
const char kFmtMaxMaxAhead[]    = "Max MaxAhead: %d\n";
const char kFmtLatency[]        = "Latency setting: %d\n";
const char kFmtGameSpeed[]      = "Game speed setting: %d\n";
const char kFmtFrameSendRate[]  = "FrameSendRate: %d\n";
const char kFmtSlotFrame[]      = "\nFrame: %d\n";
const char kFmtSlot[]           = "Slot: %d\n";
const char kFmtLocalAddress[]   = "Local address: %d.%d.%d.%d\n";
const char kFmtConnName[]       = "\nName: %s\n";
const char kFmtConnAddress[]    = "Address: %d.%d.%d.%d\n";
const char kFmtMaxAvgRoundTrip[] = "Max avg round trip: %d\n";
const char kFmtMaxRoundTrip[]   = "Max round trip: %d\n";
const char kFmtResends[]        = "Resends: %d\n";
const char kFmtFrameSyncStalls[] = "Frame sync stalls: %d\n";
const char kFmtCommandStalls[]  = "Command cound stalls: %d\n";
const char kFmtLost[]           = "Lost: %d\n";
const char kFmtPercentLost[]    = "Percent lost: %d\n";
const char kFmtCrc[]            = "CRC[%d]=%x\n";
const char kFmtCrcCurrent[]     = "CRC[%d]=%x  *****\n";
const char kFmtHouse[]          = "%s: IsHuman:%d  Color:%s  ID:%d"
                                  "  HouseType:%s\n";
const char kFmtHeadInfantry[] =
    "-------------------- %s Infantry -------------------\n";
const char kFmtHeadUnits[] =
    "-------------------- %s Units -------------------\n";
const char kFmtHeadAircraft[] =
    "-------------------- %s Aircraft -------------------\n";
const char kFmtHeadBuildings[] =
    "-------------------- %s Buildings -------------------\n";
const char kFmtCoordInfantry[] =
    "COORD:%x   Facing:%d   Mission:%d   Type:%d(%s)   Tgt:%s(%d)"
    " Speed:%d NavCom:%s(%d)\n";
const char kFmtCoordUnits[] =
    "COORD:%x   Facing:%d   Facing2:%d   Mission:%d   Type:%d(%s)"
    "   Tgt:%s(%d)  NavCom:%s(%d)   TrkNum:%d TrkInd:%d SpdAc:%d\n";
const char kFmtCoordAircraft[] =
    "COORD:%x   Facing:%d   Mission:%d   Type:%d(%s)   Tgt:%s(%d)"
    "  NavCom:%s(%d)\n";
const char kFmtCoordBuildings[] =
    "COORD:%x   Facing:%d   Mission:%d   Type:%d(%s)   Tgt:%s(%d)\n";
const char kFmtMapLayer[]       = ">>>> MAP LAYER %d <<<<\n";
const char kFmtLogicLayer[]     = ">>>> LOGIC LAYER <<<<\n";
const char kFmtObject[]         = "Object %d: %x ";
const char kFmtTypeNamed[]      = "%s\t(Type:%d(%s)) ";
const char kFmtTypePlain[]      = "%s\t(Type:%d) ";
const char kFmtOwner[]          = "Owner: %s\n";
const char kFmtOwnerNone[]      = "Owner: None\n";
const char kFmtRandomNumber[]   = "\nRandom Number:%d\n";
const char kFmtOffendingEvent[] = "Offending event:\n";
const char kFmtEventType[]      = "Type:         %d\n";
const char kFmtEventFrame[]     = "Frame:        %d\n";
const char kFmtEventId[]        = "ID:           %x\n";
const char kFmtEventCrc[]       = "CRC:          %x\n";
const char kFmtEventCmdCount[]  = "CommandCount: %d\n";
const char kFmtEventDelay[]     = "Delay:        %d\n";

// IP 按小端拆成 a.b.c.d（asm 逐字节 and 0FFh）
void PrintAddress(std::FILE* pFile, const char* pFormat, uint32 ip)
{
    std::fprintf(pFile, pFormat,
                 static_cast<int>(ip & 0xFF),
                 static_cast<int>((ip >> 8) & 0xFF),
                 static_cast<int>((ip >> 16) & 0xFF),
                 static_cast<int>((ip >> 24) & 0xFF));
}

// MAP / LOGIC 层共用的单个条目输出（asm loc_651E17 / loc_651F04 后的那段）
void PrintLayerEntry(std::FILE* pFile, const SyncSlotClass& slot, int32 index,
                     const SyncLayerEntryClass& entry)
{
    std::fprintf(pFile, kFmtObject, static_cast<int>(index),
                 static_cast<uint32>(entry.Coord));

    const int16 derivation = entry.DerivationID;
    if (derivation != 0x2B && derivation != 0x13) {
        // asm: push entry[+4]; push entry[+8] 作为 Type:%d(%s) 的两个实参
        std::fprintf(pFile, kFmtTypeNamed,
                     AbstractClass::GetClassName(
                         static_cast<AbstractType>(derivation)),
                     static_cast<int>(entry.TypeIndex),
                     entry.TypeName);
    } else {
        // asm: 该分支只 push 0，Type:%d 打印的是这个 0
        std::fprintf(pFile, kFmtTypePlain,
                     AbstractClass::GetClassName(
                         static_cast<AbstractType>(derivation)),
                     0);
    }

    const uint8 owner = entry.Owner;
    if (owner != 0xFF) {
        std::fprintf(pFile, kFmtOwner, slot.Houses[owner].Name);
    } else {
        std::fprintf(pFile, kFmtOwnerNone);
    }
}

}  // namespace

// ============================================================================
// SyncConnectionStatsClass
// ============================================================================

// asm sub_53EDA0 (0x53EDA0): 把 this+0x5C 的 4 字节拷给 arg0，
//   把 this+0x60 的 4 字节 IP 与 this+0x64 的 2 字节端口拷给 arg1。
void SyncConnectionStatsClass::GetAddressComponents(uint32& handle,
                                                    SyncAddressClass& addr) const
{
    handle = AddressHandle;
    addr.IP = Address.IP;
    addr.Port = Address.Port;
}

// ============================================================================
// SyncLoggerClass
// ============================================================================

static SyncLoggerClass* g_SyncLoggerInstance = nullptr;

SyncLoggerClass* SyncLoggerClass::GetInstance()
{
    if (g_SyncLoggerInstance == nullptr) {
        g_SyncLoggerInstance = new SyncLoggerClass();
    }
    return g_SyncLoggerInstance;
}

SyncLoggerClass::SyncLoggerClass() noexcept
{
    std::memset(Slots, 0, sizeof(Slots));
    std::memset(ConnectionStats, 0, sizeof(ConnectionStats));
    std::memset(ChecksumTable, 0, sizeof(ChecksumTable));

    // asm: sub_650A90 每次重建槽位前把玩家位一律置 -1（空槽）
    for (int32 i = 0; i < MaxSlots; ++i) {
        for (int32 h = 0; h < MaxHouses; ++h) {
            Slots[i].Houses[h].HouseID = -1;
        }
    }
}

SyncLoggerClass::~SyncLoggerClass()
{
    if (g_SyncLoggerInstance == this) {
        g_SyncLoggerInstance = nullptr;
    }
}

// ============================================================================
// asm 0x651720: Multiplay_LogToSync_MPDEBUG
// ============================================================================
void SyncLoggerClass::LogToSync_MPDEBUG(int32 slotIndex,
                                        const SyncEventPacketClass* pEvent)
{
    char filename[0x100];
    char buildTimestamp[0x80];
    char buildAuthor[0x80];
    char buildVersion[0x80];

    SyncSlotClass& slot = Slots[slotIndex];

    // ------------------------------------------------------------------------
    // asm: SYNC<玩家号>_<槽位>.TXT  —— 玩家号取 HouseClass_PlayerPtr+30h
    // ------------------------------------------------------------------------
    const int32 playerNumber = (HouseClass::Player != nullptr)
                                   ? HouseClass::Player->PlayerNumber
                                   : 0;
    std::sprintf(filename, kFmtSyncName, static_cast<int>(playerNumber),
                 static_cast<int>(slotIndex));

    WWDebugString(kFmtWritingSync, filename);

    // asm: _fopen(filename, "wt")
    std::FILE* pFile = std::fopen(filename, "wt");
    if (pFile == nullptr) {
        // asm 只把错误码压栈，格式串里的第二个 %s 没有对应实参 ——
        // 原版打印的就是栈上的野值，这里以空指针表达"无附加说明"。
        const char* const pDetail = nullptr;
        WWDebugString(kFmtOpenFail, static_cast<int>(errno), pDetail);
        return;
    }

    // ------------------------------------------------------------------------
    // loc_651782: 头部
    // ------------------------------------------------------------------------
    std::fprintf(pFile, kFmtRa2Sync);
    std::fprintf(pFile, kFmtVersion, ModuleVersionString());
    std::fprintf(pFile, kFmtInternalVersion, ModuleInternalVersionString());

    std::strncpy(buildTimestamp, ModuleBuildTimestampString(),
                 sizeof(buildTimestamp) - 1);
    buildTimestamp[sizeof(buildTimestamp) - 1] = '\0';
    std::strncpy(buildAuthor, ModuleBuildAuthorString(),
                 sizeof(buildAuthor) - 1);
    buildAuthor[sizeof(buildAuthor) - 1] = '\0';
    std::strncpy(buildVersion, ModuleBuildVersionString(),
                 sizeof(buildVersion) - 1);
    buildVersion[sizeof(buildVersion) - 1] = '\0';

    std::fprintf(pFile, kFmtReleaseBuild, buildVersion, buildAuthor,
                 buildTimestamp);

    // asm: GetCPUMMXSupport(&var_388, &flag, 0, 0) —— 只需标志字节
    const bool hasMmx = CpuSupportsMmx();
    std::fprintf(pFile, kFmtCpu, CpuTypeName(), hasMmx ? "Yes" : "No",
                 "Not available");

    // ------------------------------------------------------------------------
    // asm: 游戏状态头（Frame / FPS / MaxAhead / Latency / 游戏速度 / 发送率）
    //   原版读全局 Frame、dword_ABCD48/4C、Max_MaxAhead、Latency、Options、
    //   FrameSendRate；本工程对应 Game 的帧计数、Network 的最大前瞻与
    //   延迟补偿、Session 的游戏速度、Network 的同步帧发送间隔。
    // ------------------------------------------------------------------------
    std::fprintf(pFile, kFmtFrames, static_cast<int>(Game::CurrentFrame));
    std::fprintf(pFile, kFmtAverageFps, static_cast<int>(Game::FrameRate));
    std::fprintf(pFile, kFmtMaxMaxAhead,
                 (TheNetworking != nullptr)
                     ? static_cast<int>(TheNetworking->MaxAheadFrames)
                     : 0);
    std::fprintf(pFile, kFmtLatency,
                 (TheNetworking != nullptr)
                     ? static_cast<int>(TheNetworking->LatencyCompensation)
                     : 0);
    std::fprintf(pFile, kFmtGameSpeed,
                 (TheSession != nullptr)
                     ? static_cast<int>(TheSession->GameSpeed)
                     : 0);
    std::fprintf(pFile, kFmtFrameSendRate,
                 (TheNetworking != nullptr)
                     ? static_cast<int>(TheNetworking->FrameSendRate)
                     : 0);
    std::fprintf(pFile, kFmtSlotFrame, static_cast<int>(slot.Frame));
    std::fprintf(pFile, kFmtSlot, static_cast<int>(slotIndex));

    // ------------------------------------------------------------------------
    // loc_651903: 本机全部连接的地址（asm UDP->Count() / UDP->Connection(i)）
    // ------------------------------------------------------------------------
    ConnectionManager* const pManager = ConnectionManager::GetInstance();
    if (pManager != nullptr) {
        for (int32 i = 0; i < pManager->GetConnectionCount(); ++i) {
            ConnectionClass* const pConn = pManager->GetConnection(i);
            if (pConn == nullptr) {
                continue;
            }
            PrintAddress(pFile, kFmtLocalAddress, pConn->GetAddress());
        }
    }

    // ------------------------------------------------------------------------
    // loc_651955: 8 组连接统计
    // ------------------------------------------------------------------------
    for (int32 i = 0; i < MaxConnections; ++i) {
        const SyncConnectionStatsClass& stats = ConnectionStats[i];
        if (stats.Name[0] == '\0') {
            continue;
        }

        std::fprintf(pFile, kFmtConnName, stats.Name);

        uint32 addressHandle = 0;
        SyncAddressClass address = {0, 0};
        stats.GetAddressComponents(addressHandle, address);
        // asm 把句柄拷进 var_3A4 后随即被 Name 打印复用，从未输出
        (void)addressHandle;

        PrintAddress(pFile, kFmtConnAddress, address.IP);

        std::fprintf(pFile, kFmtMaxAvgRoundTrip,
                     static_cast<int>(stats.MaxAvgRoundTrip));
        std::fprintf(pFile, kFmtMaxRoundTrip,
                     static_cast<int>(stats.MaxRoundTrip));
        std::fprintf(pFile, kFmtResends,
                     static_cast<int>(stats.Resends));
        std::fprintf(pFile, kFmtFrameSyncStalls,
                     static_cast<int>(stats.FrameSyncStalls));
        std::fprintf(pFile, kFmtCommandStalls,
                     static_cast<int>(stats.CommandCountStalls));
        std::fprintf(pFile, kFmtLost,
                     static_cast<int>(stats.Lost));
        std::fprintf(pFile, kFmtPercentLost,
                     static_cast<int>(stats.PercentLost));
    }

    // ------------------------------------------------------------------------
    // loc_651A3B: 256 项帧校验和，本槽那项加 *****
    // ------------------------------------------------------------------------
    for (int32 i = 0; i < ChecksumCount; ++i) {
        if (i == slotIndex) {
            std::fprintf(pFile, kFmtCrcCurrent, static_cast<int>(i),
                         ChecksumTable[i]);
        } else {
            std::fprintf(pFile, kFmtCrc, static_cast<int>(i),
                         ChecksumTable[i]);
        }
    }

    // ------------------------------------------------------------------------
    // loc_651A7A: 8 个玩家槽详情；var_39C 记下最后一个有效槽的房子下标
    // ------------------------------------------------------------------------
    int32 lastHouseIndex = 0;
    for (int32 i = 0; i < MaxHouses; ++i) {
        const SyncHouseSlotClass& house = slot.Houses[i];
        if (house.HouseID == -1) {
            continue;
        }
        lastHouseIndex = house.HouseID;
        std::fprintf(pFile, kFmtHouse, house.Name,
                     static_cast<int>(house.IsHuman), house.ColorName,
                     static_cast<int>(house.HouseID), house.HouseTypeName);
    }

    // ------------------------------------------------------------------------
    // loc_651AD1: 步兵段
    // ------------------------------------------------------------------------
    for (int32 houseIndex = 0; houseIndex < lastHouseIndex; ++houseIndex) {
        const SyncHouseSlotClass& house = slot.Houses[houseIndex];
        if (house.HouseID == -1) {
            continue;
        }
        std::fprintf(pFile, kFmtHeadInfantry, house.Name);

        const int32 count = slot.InfantryCount;
        if (count <= 0) {
            continue;
        }

        const SyncObjectRecordClass* pRecord = slot.Records;
        for (int32 j = 0; j < count; ++j, ++pRecord) {
            if (pRecord->Owner != houseIndex) {
                continue;
            }
            std::fprintf(pFile, kFmtCoordInfantry,
                         static_cast<uint32>(pRecord->Coord),
                         static_cast<int>(pRecord->Facing),
                         static_cast<int>(pRecord->Mission),
                         static_cast<int>(pRecord->TypeIndex),
                         pRecord->TypeName,
                         pRecord->TargetName,
                         static_cast<int>(pRecord->TargetValue),
                         static_cast<int>(pRecord->Health),
                         pRecord->NavComName,
                         static_cast<int>(pRecord->NavComValue));
        }
    }

    // ------------------------------------------------------------------------
    // loc_651BA3: 车辆段（记录池跳过步兵块）
    // ------------------------------------------------------------------------
    for (int32 houseIndex = 0; houseIndex < lastHouseIndex; ++houseIndex) {
        const SyncHouseSlotClass& house = slot.Houses[houseIndex];
        if (house.HouseID == -1) {
            continue;
        }
        std::fprintf(pFile, kFmtHeadUnits, house.Name);

        const int32 count = slot.UnitCount;
        if (count <= 0) {
            continue;
        }

        const SyncObjectRecordClass* pRecord =
            slot.Records + slot.InfantryCount;
        for (int32 j = 0; j < count; ++j, ++pRecord) {
            if (pRecord->Owner != houseIndex) {
                continue;
            }
            std::fprintf(pFile, kFmtCoordUnits,
                         static_cast<uint32>(pRecord->Coord),
                         static_cast<int>(pRecord->Facing),
                         static_cast<int>(pRecord->Facing2),
                         static_cast<int>(pRecord->Mission),
                         static_cast<int>(pRecord->TypeIndex),
                         pRecord->TypeName,
                         pRecord->TargetName,
                         static_cast<int>(pRecord->TargetValue),
                         pRecord->NavComName,
                         static_cast<int>(pRecord->NavComValue),
                         static_cast<int>(pRecord->TrackNumber),
                         static_cast<int>(pRecord->TrackIndex),
                         static_cast<int>(pRecord->SpeedAcc));
        }
    }

    // ------------------------------------------------------------------------
    // loc_651C88: 飞机段
    // ------------------------------------------------------------------------
    for (int32 houseIndex = 0; houseIndex < lastHouseIndex; ++houseIndex) {
        const SyncHouseSlotClass& house = slot.Houses[houseIndex];
        if (house.HouseID == -1) {
            continue;
        }
        std::fprintf(pFile, kFmtHeadAircraft, house.Name);

        const int32 count = slot.AircraftCount;
        if (count <= 0) {
            continue;
        }

        const SyncObjectRecordClass* pRecord =
            slot.Records + slot.InfantryCount + slot.UnitCount;
        for (int32 j = 0; j < count; ++j, ++pRecord) {
            if (pRecord->Owner != houseIndex) {
                continue;
            }
            std::fprintf(pFile, kFmtCoordAircraft,
                         static_cast<uint32>(pRecord->Coord),
                         static_cast<int>(pRecord->Facing),
                         static_cast<int>(pRecord->Mission),
                         static_cast<int>(pRecord->TypeIndex),
                         pRecord->TypeName,
                         pRecord->TargetName,
                         static_cast<int>(pRecord->TargetValue),
                         pRecord->NavComName,
                         static_cast<int>(pRecord->NavComValue));
        }
    }

    // ------------------------------------------------------------------------
    // loc_651D55: 建筑段
    // ------------------------------------------------------------------------
    for (int32 houseIndex = 0; houseIndex < lastHouseIndex; ++houseIndex) {
        const SyncHouseSlotClass& house = slot.Houses[houseIndex];
        if (house.HouseID == -1) {
            continue;
        }
        std::fprintf(pFile, kFmtHeadBuildings, house.Name);

        const int32 count = slot.BuildingCount;
        if (count <= 0) {
            continue;
        }

        const SyncObjectRecordClass* pRecord =
            slot.Records + slot.InfantryCount + slot.UnitCount
            + slot.AircraftCount;
        for (int32 j = 0; j < count; ++j, ++pRecord) {
            if (pRecord->Owner != houseIndex) {
                continue;
            }
            std::fprintf(pFile, kFmtCoordBuildings,
                         static_cast<uint32>(pRecord->Coord),
                         static_cast<int>(pRecord->Facing),
                         static_cast<int>(pRecord->Mission),
                         static_cast<int>(pRecord->TypeIndex),
                         pRecord->TypeName,
                         pRecord->TargetName,
                         static_cast<int>(pRecord->TargetValue));
        }
    }

    // ------------------------------------------------------------------------
    // loc_651DF8: 5 个 MAP LAYER
    // ------------------------------------------------------------------------
    for (int32 layer = 0; layer < MapLayerCount; ++layer) {
        std::fprintf(pFile, kFmtMapLayer, static_cast<int>(layer));

        const int32 count = slot.LayerCount[layer];
        if (count <= 0) {
            continue;
        }

        const SyncLayerEntryClass* pEntry = slot.LayerEntries[layer];
        for (int32 j = 0; j < count; ++j, ++pEntry) {
            PrintLayerEntry(pFile, slot, j, *pEntry);
        }
    }

    // ------------------------------------------------------------------------
    // loc_651F04: LOGIC LAYER
    // ------------------------------------------------------------------------
    std::fprintf(pFile, kFmtLogicLayer);
    {
        const int32 count = slot.LogicCount;
        if (count > 0) {
            const SyncLayerEntryClass* pEntry = slot.LogicEntries;
            for (int32 j = 0; j < count; ++j, ++pEntry) {
                PrintLayerEntry(pFile, slot, j, *pEntry);
            }
        }
    }

    // ------------------------------------------------------------------------
    // loc_651FBD: 随机数
    // ------------------------------------------------------------------------
    std::fprintf(pFile, kFmtRandomNumber,
                 static_cast<int>(slot.RandomNumber));

    // ------------------------------------------------------------------------
    // asm: 出错事件（正常帧 pEvent 为空）
    // ------------------------------------------------------------------------
    if (pEvent != nullptr) {
        std::fprintf(pFile, kFmtOffendingEvent);
        std::fprintf(pFile, kFmtEventType, static_cast<int>(pEvent->Kind));
        std::fprintf(pFile, kFmtEventFrame,
                     static_cast<int>(pEvent->Frame));
        std::fprintf(pFile, kFmtEventId,
                     static_cast<uint32>(
                         static_cast<int32>(pEvent->PlayerID)));
        std::fprintf(pFile, kFmtEventCrc, pEvent->CRC);
        std::fprintf(pFile, kFmtEventCmdCount,
                     static_cast<int>(pEvent->CommandCount));
        std::fprintf(pFile, kFmtEventDelay, static_cast<int>(pEvent->Delay));
    }

    std::fclose(pFile);
}

// ============================================================================
// asm 0x64E1C4: void __cdecl Multiplay_LogToSYNC_NOMPDEBUG(Event*)
//
// MPDEBUG 变体（0x651720）的"无调试输出"编译版。两者共用同一套槽位/连接
// 统计/校验和表，差别在于本文这一版：
//
//   * 文件名只带玩家号（SYNC%01d.TXT），没有槽位号；
//   * CRC 环形表不做 "*****" 高亮；
//   * 不写 "Frame/Slot" 头，改写成 "My Frame"；
//   * 额外把实时对象表逐类转储（步兵/车辆/建筑/飞机 + MAP/LOGIC 图层 +
//     43 个类型的全量 CRC 段），并向 #4 单色屏镜像输出。
//
// 原版 __usercall 的 ecx = OffendingEvent 包指针（可为空）。
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
// 本变体专有的格式串：逐字节取自 .data（含 "Bullet Type's" 的原样拼写）
// ----------------------------------------------------------------------------
const char kNdFmtSyncName[]        = "SYNC%01d.TXT";
const char kNdFmtPlayerDetails[]   = "\n---Player Details---\n";
const char kNdFmtHouseDetail[] =
    "%s: IsHuman:%d  Color:%s  ID:%d  HouseType:%s  Edge:%d "
    " Startallies:%d  Startspot:%d Visionary:%d MapIsClear:%d\n";
const char kNdFmtMonoHouse[]       = "House %s:%x\n";
const char kNdHeadInfantry[] =
    "-------------------- %s (%d) Infantry -------------------\n";
const char kNdHeadUnits[] =
    "-------------------- %s (%d) Units -------------------\n";
const char kNdHeadBuildings[] =
    "-------------------- %s (%d) Buildings -------------------\n";
const char kNdHeadAircraft[] =
    "-------------------- %s (%d) Aircraft -------------------\n";
const char kNdFmtMonoInfantry[]    = "%s Infantry:%x\n";
const char kNdFmtMonoUnits[]       = "%s Units:%x\n";
const char kNdFmtMonoBuildings[]   = "%s Buildings:%x\n";
const char kNdFmtMonoAircraft[]    = "%s Aircraft:%x\n";
const char kNdFmtCoordInfantry[] =
    "COORD:%d,%d  Facing:%d  Mission:%d  Type:%d(%s)  TarCom:%s(%d,%d)"
    "  Speed:%d  NavCom:%s(%d,%d)\n";
const char kNdFmtCoordUnits[] =
    "COORD:%d,%d  Facing:%d  Facing2:%d  Mission:%d  Type:%d(%s)"
    "  TarCom:%s(%d,%d)  NavCom:%s(%d,%d)  TrkNum:%d  TrkInd:%d"
    "  SpdAcc:%d\n";
const char kNdFmtCoordBldAir[] =
    "COORD:%d,%d  Facing:%d  Mission:%d  Type:%d(%s)  TarCom:%s(%d,%d)\n";
const char kNdFmtOwnerSD[]         = "Owner: %s(%d)\n";
const char kNdFmtOwnerS[]          = "Owner: %s\n";
const char kNdFmtOwnerNone[]       = "Owner: NONE\n";
const char kNdFmtMapLayersX[]      = "Map Layers:%x  \n";
const char kNdFmtLogicX[]          = "Logic:%x  \n";
const char kNdFmtRandomX[]         = "Random Number:%x  \n";
const char kNdFmtMyFrameMono[]     = "My Frame:%d  \n";
const char kNdFmtMyFrame[]         = "My Frame:%d\n";
const char kNdFmtCrcIndex[]        = "Index     CRC\n";
const char kNdFmtCrcWaves[] =
    "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
const char kNdFmtCrcRow[]          = "%05d      %08x\n";
const char kNdStrNone[]            = "None";
const char kNdStrNotAvail[]        = "Not available";
// LOGIC 层的 9 路类型分派（顺序与 asm 的比较链一致）
struct NdLogicType {
    int32       What;
    const char* Format;
};

const NdLogicType kNdLogicTypes[] = {
    {2,    "Aircraft  (Type:%d(%s)) "},
    {4,    "Anim      (Type:%d(%s)) "},
    {6,    "Building  (Type:%d(%s)) "},
    {8,    "Bullet    (Type:%d(%s)) "},
    {0x0F, "Infantry  (Type:%d(%s)) "},
    {0x14, "Overlay   (Type:%d(%s)) "},
    {0x1D, "Smudge    (Type:%d(%s)) "},
    {0x24, "Terrain   (Type:%d(%s)) "},
    {1,    "Unit      (Type:%d(%s)) "},
};

// 43 个 CRC 段标题（顺序即 asm 的调用顺序）
const char kNdCrcTitleHouseType[] =
    "\n\n*************** House Type CRCs**************\n\n";
const char kNdCrcTitleHouse[] =
    "\n\n*************** House CRCs*******************\n\n";
const char kNdCrcTitleUnitType[] =
    "\n\n*************** Unit Type CRCs**************\n\n";
const char kNdCrcTitleUnit[] =
    "\n\n*************** Unit CRCs**************\n\n";
const char kNdCrcTitleInfantryType[] =
    "\n\n*************** Infantry Type CRCs**************\n\n";
const char kNdCrcTitleInfantry[] =
    "\n\n*************** Infantry CRCs**************\n\n";
const char kNdCrcTitleBuildingType[] =
    "\n\n*************** BuildingType CRCs**************\n\n";
const char kNdCrcTitleBuilding[] =
    "\n\n*************** Building CRCs**************\n\n";
const char kNdCrcTitleAircraftType[] =
    "\n\n*************** Aircraft Type CRCs**************\n\n";
const char kNdCrcTitleAircraft[] =
    "\n\n*************** Aircraft CRCs**************\n\n";
const char kNdCrcTitleAnim[] =
    "\n\n*************** Anim CRCs**************\n\n";
const char kNdCrcTitleAnimType[] =
    "\n\n*************** Anim Type CRCs**************\n\n";
const char kNdCrcTitleTaskForce[] =
    "\n\n*************** Task Force CRCs**************\n\n";
const char kNdCrcTitleTeamType[] =
    "\n\n*************** Team Type CRCs**************\n\n";
const char kNdCrcTitleTeam[] =
    "\n\n*************** Team CRCs**************\n\n";
const char kNdCrcTitleScriptType[] =
    "\n\n*************** Script Type CRCs**************\n\n";
const char kNdCrcTitleScript[] =
    "\n\n*************** Script CRCs**************\n\n";
const char kNdCrcTitleTagType[] =
    "\n\n*************** Tag Type CRCs**************\n\n";
const char kNdCrcTitleTag[] =
    "\n\n*************** Tag CRCs**************\n\n";
const char kNdCrcTitleTriggerType[] =
    "\n\n*************** Trigger Type CRCs**************\n\n";
const char kNdCrcTitleTrigger[] =
    "\n\n*************** Trigger CRCs**************\n\n";
const char kNdCrcTitleAction[] =
    "\n\n*************** Action CRCs**************\n\n";
const char kNdCrcTitleEvent[] =
    "\n\n*************** Event CRCs**************\n\n";
const char kNdCrcTitleFactory[] =
    "\n\n*************** Factory CRCs**************\n\n";
const char kNdCrcTitleVoxelAnimType[] =
    "\n\n*************** Voxel Anim Type CRCs**************\n\n";
const char kNdCrcTitleVoxelAnim[] =
    "\n\n*************** Voxel Anim CRCs**************\n\n";
const char kNdCrcTitleWarhead[] =
    "\n\n*************** Warhead CRCs**************\n\n";
const char kNdCrcTitleWeapon[] =
    "\n\n*************** Weapon CRCs**************\n\n";
const char kNdCrcTitleParticleType[] =
    "\n\n*************** Particle Type CRCs**************\n\n";
const char kNdCrcTitleParticle[] =
    "\n\n*************** Particle CRCs**************\n\n";
const char kNdCrcTitleParticleSystem[] =
    "\n\n*************** Particle System CRCs**************\n\n";
const char kNdCrcTitleParticleSystemType[] =
    "\n\n*************** Particle System Type CRCs**************\n\n";
const char kNdCrcTitleBulletType[] =
    "\n\n*************** Bullet Type's**************\n\n";
const char kNdCrcTitleBullet[] =
    "\n\n*************** Bullet CRCs**************\n\n";
const char kNdCrcTitleWaypointPath[] =
    "\n\n*************** Waypoint Path CRCs**************\n\n";
const char kNdCrcTitleSmudgeType[] =
    "\n\n*************** Smudge Type CRCs**************\n\n";
const char kNdCrcTitleOverlayType[] =
    "\n\n*************** Overlay Type CRCs**************\n\n";
const char kNdCrcTitleLightSource[] =
    "\n\n*************** Light Source CRCs**************\n\n";
const char kNdCrcTitleBuildingLight[] =
    "\n\n*************** Building Light CRCs**************\n\n";
const char kNdCrcTitleTube[] =
    "\n\n*************** Tube CRCs**************\n\n";
const char kNdCrcTitleSides[] =
    "\n\n*************** Sides CRCs**************\n\n";
const char kNdCrcTitleTiberium[] =
    "\n\n*************** Tiberium CRCs**************\n\n";
const char kNdCrcTitleEmPulse[] =
    "\n\n*************** Em Pulse CRCs**************\n\n";

// ----------------------------------------------------------------------------
// 单色屏输出（asm: MonoClass::Printf）。屏为空时整段丢弃 —— 原版在无
// 调试屏的构建里同样什么都不做。
// ----------------------------------------------------------------------------
void NdMono(MonoScreenClass* pMono, const char* pFormat, ...)
{
    if (pMono == nullptr) {
        return;
    }
    va_list args;
    va_start(args, pFormat);
    pMono->PrintfV(pFormat, args);
    va_end(args);
}

// asm: magic 0x66666667 除法（sar 2 + 符号补偿）—— 即向零截断的 /10。
int32 NdDiv10(int32 value)
{
    return value / 10;
}

// asm: dword_AC51FC 累积：acc = acc*2 + signbit(acc) + value
//   （shr ecx,1Fh / lea ecx,[ecx+edx*2] / add ecx,eax / 存回）
int32 NdHashAcc(int32 acc, int32 value)
{
    uint32 h = static_cast<uint32>(acc);
    h = h * 2u + (h >> 31u) + static_cast<uint32>(value);
    return static_cast<int32>(h);
}

// ----------------------------------------------------------------------------
// asm: Desired_Facing256(obj+388h) 取当前朝向状态，再按 (raw>>7 +1)>>1 归一
// 成 0..255 的字节。原版朝向是 16 位插值状态；本工程的 DirStruct 直接就是
// 0..255 的当前朝向，故取字节值即同一语义。
// ----------------------------------------------------------------------------
int32 NdFacingOf(TechnoClass* pObj)
{
    if (pObj == nullptr) {
        return 0;
    }
    // 足类（步兵/车辆/飞机）的朝向由 FootClass 自己那份 PrimaryFacing 承载，
    // 建筑等静态对象取 TechnoClass 的同一槽位。
    const AbstractType what = pObj->WhatAmI();
    if (what == AbstractType::Infantry || what == AbstractType::Unit ||
        what == AbstractType::Aircraft) {
        return static_cast<int32>(
            static_cast<FootClass*>(pObj)->PrimaryFacing.Value);
    }
    return static_cast<int32>(pObj->PrimaryFacing.Value);
}

// asm: Desired_Facing256(obj+3A0h) —— 车辆/飞机的炮塔朝向（FootClass::TurretFacing）
int32 NdFacing2Of(FootClass* pFoot)
{
    return (pFoot != nullptr) ? static_cast<int32>(pFoot->TurretFacing.Value) : 0;
}

// ----------------------------------------------------------------------------
// 共用子目标块（[obj+2B4h] TarCom / [obj+5A4h] NavCom）：
// 两次 vt+48h 取坐标，再 vt+2Ch 取抽象类型号 -> 类名。
// ----------------------------------------------------------------------------
void NdSubBlock(AbstractClass* pSub, int32& coordX, int32& coordY,
                const char*& pClassName)
{
    coordX = 0;
    coordY = 0;
    pClassName = kNdStrNone;
    if (pSub == nullptr) {
        return;
    }

    CoordStruct coord;
    pSub->GetCoords(&coord);          // asm: 第一次 vt+48h -> 取 +0
    coordX = coord.X;
    pSub->GetCoords(&coord);          // asm: 第二次 vt+48h -> 取 +4
    coordY = coord.Y;

    pClassName = AbstractClass::GetClassName(pSub->WhatAmI());
    if (pClassName == nullptr) {
        pClassName = kNdStrNone;
    }
}

// ----------------------------------------------------------------------------
// 对象 CRC 取值：asm 对每个条目调 vt+34h（ComputeCRC(CRCEngine&)）。
//
// 本工程的 43 个类型并非都露出同一入口：多数走 ComputeCRC，一部分只提供
// 零参 GetCRC()（内部就是 ComputeCRC 的封装），还有 TeamClass / ScriptClass
// 这类独立类完全不带 CRC 入口。这里按"GetCRC() 优先 -> ComputeCRC -> 整对象
// 字节"的顺序落到同一位置，保证每个条目都有确定值。
// ----------------------------------------------------------------------------
template <typename T, typename = void>
struct NdHasGetCRC : std::false_type {};

template <typename T>
struct NdHasGetCRC<
    T, std::void_t<decltype(std::declval<const T&>().GetCRC())>>
    : std::true_type {};

template <typename T, typename = void>
struct NdHasComputeCRC : std::false_type {};

template <typename T>
struct NdHasComputeCRC<
    T, std::void_t<decltype(std::declval<const T&>().ComputeCRC(
           std::declval<CRCEngine&>()))>> : std::true_type {};

template <typename T>
uint32 NdObjectCRC(const T* pObj)
{
    if constexpr (NdHasGetCRC<T>::value) {
        return static_cast<uint32>(pObj->GetCRC());
    } else if constexpr (NdHasComputeCRC<T>::value) {
        CRCEngine crc;
        pObj->ComputeCRC(crc);
        return crc.GetCRC();
    } else {
        // 独立类没有 CRC 入口：按对象字节求值（原版这类对象也没有
        // 虚表槽，出处见各自的 GetCRC 注释）。
        CRCEngine crc;
        crc.AddData(pObj, static_cast<int32>(sizeof(T)));
        return crc.GetCRC();
    }
}

// ----------------------------------------------------------------------------
// 一个标准 CRC 段：标题 + "Index     CRC" + 波浪线 + 逐个对象 vt+34h。
//
// asm 每次都会 new 一个 0Ch 的累积器并清零；本工程用 CRCEngine 承接同一位
// 置（四个字节的累积值即原版的 buf[0]）。
// ----------------------------------------------------------------------------
template <typename T>
void NdCRCSection(std::FILE* pFile, const char* pTitle,
                  DynamicVectorClass<T*>* pVec, bool skipUninitializedAnim)
{
    std::fprintf(pFile, "%s", pTitle);
    std::fprintf(pFile, "%s", kNdFmtCrcIndex);
    std::fprintf(pFile, "%s", kNdFmtCrcWaves);

    if (pVec == nullptr) {
        return;
    }

    for (int32 i = 0; i < pVec->Count; ++i) {
        T* const pObj = (*pVec)[i];
        if (pObj == nullptr) {
            continue;
        }

        if (skipUninitializedAnim) {
            // asm: [obj+4]->vt+10h(obj+4) == -2 (IRTTITypeInfo::Fetch_ID)
            if constexpr (std::is_base_of<AbstractClass, T>::value) {
                if (static_cast<AbstractClass*>(pObj)->Fetch_ID() == -2) {
                    continue;
                }
            }
        }

        std::fprintf(pFile, kNdFmtCrcRow, static_cast<int>(i),
                     NdObjectCRC(pObj));
    }
}

// 固定数组形态的两段（HouseType / House 用的不是 DynamicVectorClass）
template <typename T>
void NdCRCFixedSection(std::FILE* pFile, const char* pTitle,
                       T* const* pItems, int32 count)
{
    std::fprintf(pFile, "%s", pTitle);
    std::fprintf(pFile, "%s", kNdFmtCrcIndex);
    std::fprintf(pFile, "%s", kNdFmtCrcWaves);

    for (int32 i = 0; i < count; ++i) {
        T* const pObj = pItems[i];
        if (pObj == nullptr) {
            continue;
        }
        std::fprintf(pFile, kNdFmtCrcRow, static_cast<int>(i),
                     NdObjectCRC(pObj));
    }
}

}  // namespace

void SyncLoggerClass::LogToSync_NoDebug(const SyncEventPacketClass* pEvent)
{
    // ------------------------------------------------------------------------
    // asm: SYNC<玩家号>.TXT —— 玩家号取 HouseClass_PlayerPtr+30h
    // ------------------------------------------------------------------------
    char filename[0x100];
    const int32 playerNumber = (HouseClass::Player != nullptr)
                                   ? HouseClass::Player->PlayerNumber
                                   : 0;
    std::sprintf(filename, kNdFmtSyncName, static_cast<int>(playerNumber));
    WWDebugString(kFmtWritingSync, filename);

    // asm: Get_Mono_ScreenPtr_5BC450(4) -> sub_5BCC90(mono) -> sub_5BCC40(mono,0,0)
    MonoScreenClass* const pMono = MonoScreenClass::Get(4);
    if (pMono != nullptr) {
        pMono->Clear();
        pMono->SetCursor(0, 0);
    }

    std::FILE* const pFile = std::fopen(filename, "wt");
    if (pFile == nullptr) {
        // 原版同样的错：格式串尾部有 %s，调用点只压了 GetLastError()。
        WWDebugString(kFmtOpenFail, static_cast<int>(errno),
                      static_cast<const char*>(nullptr));
        return;
    }

    // ------------------------------------------------------------------------
    // 头部：版本 / 构建 / CPU / 帧 / FPS / MaxAhead / Latency / 速度 / 发送率
    // ------------------------------------------------------------------------
    std::fprintf(pFile, "%s", kFmtRa2Sync);
    std::fprintf(pFile, kFmtVersion, ModuleVersionString());
    std::fprintf(pFile, kFmtInternalVersion, ModuleInternalVersionString());

    char buildTimestamp[0x80];
    char buildAuthor[0x80];
    char buildVersion[0x80];
    std::strncpy(buildTimestamp, ModuleBuildTimestampString(),
                 sizeof(buildTimestamp) - 1);
    buildTimestamp[sizeof(buildTimestamp) - 1] = '\0';
    std::strncpy(buildAuthor, ModuleBuildAuthorString(),
                 sizeof(buildAuthor) - 1);
    buildAuthor[sizeof(buildAuthor) - 1] = '\0';
    std::strncpy(buildVersion, ModuleBuildVersionString(),
                 sizeof(buildVersion) - 1);
    buildVersion[sizeof(buildVersion) - 1] = '\0';

    // asm 压栈顺序 version, author, timestamp（"%s by %s - %s"）
    std::fprintf(pFile, kFmtReleaseBuild, buildVersion, buildAuthor,
                 buildTimestamp);

    // asm: GetCPUMMXSupport(&vendor, &flag, 0, 0) -> flag 决定 Yes/No；
    //   GetCPUType(Yes/No, "Not available") 的第二个实参就是 Vendor 一栏。
    const bool hasMmx = CpuSupportsMmx();
    std::fprintf(pFile, kFmtCpu, CpuTypeName(), hasMmx ? "Yes" : "No",
                 kNdStrNotAvail);

    std::fprintf(pFile, kFmtFrames, static_cast<int>(Game::CurrentFrame));
    std::fprintf(pFile, kFmtAverageFps, static_cast<int>(Game::FrameRate));
    std::fprintf(pFile, kFmtMaxMaxAhead,
                 (TheNetworking != nullptr)
                     ? static_cast<int>(TheNetworking->MaxAheadFrames)
                     : 0);
    std::fprintf(pFile, kFmtLatency,
                 (TheNetworking != nullptr)
                     ? static_cast<int>(TheNetworking->LatencyCompensation)
                     : 0);
    std::fprintf(pFile, kFmtGameSpeed,
                 (TheSession != nullptr)
                     ? static_cast<int>(TheSession->GameSpeed)
                     : 0);
    std::fprintf(pFile, kFmtFrameSendRate,
                 (TheNetworking != nullptr)
                     ? static_cast<int>(TheNetworking->FrameSendRate)
                     : 0);

    // ------------------------------------------------------------------------
    // asm: UDP->vt+48h() 计数 + vt+4Ch(i) 取地址，逐条打印本机连接地址
    // ------------------------------------------------------------------------
    ConnectionManager* const pManager = ConnectionManager::GetInstance();
    if (pManager != nullptr) {
        for (int32 i = 0; i < pManager->GetConnectionCount(); ++i) {
            ConnectionClass* const pConn = pManager->GetConnection(i);
            if (pConn == nullptr) {
                continue;
            }
            PrintAddress(pFile, kFmtLocalAddress, pConn->GetAddress());
        }
    }

    // ------------------------------------------------------------------------
    // asm: unk_A8B5C4 .. unk_A8B904（步长 68h）—— 8 组连接统计
    // ------------------------------------------------------------------------
    for (int32 i = 0; i < MaxConnections; ++i) {
        const SyncConnectionStatsClass& stats = ConnectionStats[i];
        if (stats.Name[0] == '\0') {
            continue;
        }

        std::fprintf(pFile, kFmtConnName, stats.Name);

        uint32 addressHandle = 0;
        SyncAddressClass address = {0, 0};
        stats.GetAddressComponents(addressHandle, address);
        (void)addressHandle;   // asm 拷进 a2 后从未使用

        PrintAddress(pFile, kFmtConnAddress, address.IP);

        std::fprintf(pFile, kFmtMaxAvgRoundTrip,
                     static_cast<int>(stats.MaxAvgRoundTrip));
        std::fprintf(pFile, kFmtMaxRoundTrip,
                     static_cast<int>(stats.MaxRoundTrip));
        std::fprintf(pFile, kFmtResends, static_cast<int>(stats.Resends));
        std::fprintf(pFile, kFmtFrameSyncStalls,
                     static_cast<int>(stats.FrameSyncStalls));
        std::fprintf(pFile, kFmtCommandStalls,
                     static_cast<int>(stats.CommandCountStalls));
        std::fprintf(pFile, kFmtLost, static_cast<int>(stats.Lost));
        std::fprintf(pFile, kFmtPercentLost,
                     static_cast<int>(stats.PercentLost));
    }

    // ------------------------------------------------------------------------
    // asm: dword_B04474 .. dword_B04874 —— 256 项帧校验和（无 ***** 高亮）
    // ------------------------------------------------------------------------
    for (int32 i = 0; i < ChecksumCount; ++i) {
        std::fprintf(pFile, kFmtCrc, static_cast<int>(i), ChecksumTable[i]);
    }

    // ------------------------------------------------------------------------
    // asm: "\n---Player Details---\n" —— 逐房详情（宽字符名 + 配色方案名）
    // ------------------------------------------------------------------------
    std::fprintf(pFile, "%s", kNdFmtPlayerDetails);
    for (int32 hi = 0; hi < HouseClass::ArrayCount; ++hi) {
        HouseClass* const pHouse = HouseClass::Array[hi];
        if (pHouse == nullptr) {
            continue;
        }

        // asm: MP_ReadNthPlayerName(&house+1602Ah) —— 宽字符玩家名
        const char* const pPlayerName =
            MP::ReadNthPlayerName(pHouse->CSFName);
        const char* const pName = (pPlayerName != nullptr) ? pPlayerName : "";

        const int32 colorIndex = pHouse->ColorSchemeIndex;
        const char* pSchemeName = "";
        if (ColorScheme::Array != nullptr && colorIndex >= 0 &&
            colorIndex < ColorScheme::Array->Count) {
            const ColorScheme* const pScheme =
                (*ColorScheme::Array)[colorIndex];
            if (pScheme != nullptr && pScheme->ID != nullptr) {
                pSchemeName = pScheme->ID;
            }
        }

        HouseTypeClass* const pHouseType = pHouse->Type;
        const char* const pHouseTypeName =
            (pHouseType != nullptr) ? pHouseType->get_Name() : "";
        const char* const pHouseTypeID =
            (pHouseType != nullptr) ? pHouseType->get_ID() : "";

        std::fprintf(pFile, kNdFmtHouseDetail,
                     pName,
                     static_cast<int>(pHouse->IsHumanPlayer),
                     pSchemeName,
                     static_cast<int>(pHouse->PlayerNumber),
                     pHouseTypeName,
                     static_cast<int>(pHouse->Edge),
                     static_cast<int>(pHouse->TeamIndex),
                     static_cast<int>(pHouse->StartingSlot),
                     static_cast<int>(pHouse->IsVisionary),
                     static_cast<int>(pHouse->MapIsClear));

        // asm: MonoClass::Printf("House %s:%x", [h+34h]+24h, dword_AC51FC)
        NdMono(pMono, kNdFmtMonoHouse, pHouseTypeID, 0);
    }

    // ------------------------------------------------------------------------
    // asm loc_64E2EA / loc_64E561 / loc_64E916 / loc_64EA9C ——
    //   逐房逐类扫描实时对象表。每段的骨架相同：
    //     本房累加器清零 → 段标题（房型 ID, 玩家号）→ 扫对象 →
    //     单色屏尾部 "%s <类>:%x"（房型 ID, 累加器）
    // ------------------------------------------------------------------------
    const int32 houseCount = HouseClass::ArrayCount;

    // ................ 步兵 ................
    if (InfantryClass::Array != nullptr) {
        DynamicVectorClass<InfantryClass*>* const pVec = InfantryClass::Array;
        for (int32 hi = 0; hi < houseCount; ++hi) {
            HouseClass* const pHouse = HouseClass::Array[hi];
            if (pHouse == nullptr) {
                continue;
            }

            int32 hashAcc = 0;
            const char* const pHouseTypeID =
                (pHouse->Type != nullptr) ? pHouse->Type->get_ID() : "";

            std::fprintf(pFile, kNdHeadInfantry, pHouseTypeID,
                         static_cast<int>(pHouse->PlayerNumber));

            for (int32 di = 0; di < pVec->Count; ++di) {
                InfantryClass* const pInf = (*pVec)[di];
                if (pInf == nullptr || pInf->Owner != pHouse) {
                    continue;
                }

                // asm: [obj+9Ch] 缓存坐标，X/Y 分别 /10 混进累加器
                const CoordStruct& cached = pInf->CachedLocation;
                int32 facing = NdFacingOf(pInf);
                hashAcc = NdHashAcc(hashAcc,
                                    NdDiv10(cached.X) + facing +
                                        (NdDiv10(cached.Y) << 16));

                int32 tarX = 0;
                int32 tarY = 0;
                int32 navX = 0;
                int32 navY = 0;
                const char* pTarName = kNdStrNone;
                const char* pNavName = kNdStrNone;
                NdSubBlock(pInf->GetTarget(), tarX, tarY, pTarName);
                NdSubBlock(static_cast<FootClass*>(pInf)->CurrentTarget,
                           navX, navY, pNavName);

                facing = NdFacingOf(pInf);

                TechnoTypeClass* const pType = pInf->Type;
                const int32 typeIndex =
                    (pType != nullptr) ? pType->Get_ArrayIndex() : 0;
                const char* const pTypeName =
                    (pType != nullptr) ? pType->get_ID() : "";

                // asm: fld [obj+578h] * 256.0 -> Float_To_Int_Floor
                const int32 speed = static_cast<int32>(std::floor(
                    static_cast<FootClass*>(pInf)->SpeedPercentage * 256.0));

                // asm: vt+184h(df8, typeName) —— 取当前任务号
                const int32 mission = static_cast<int32>(pInf->GetMission());

                // asm: 两次 vt+48h，第一次取 +4（Y），第二次取 +0（X）
                const CoordStruct coord = pInf->GetCoords();
                const int32 coordX = coord.X;
                const int32 coordY = coord.Y;

                std::fprintf(pFile, kNdFmtCoordInfantry,
                             static_cast<int>(coordX),
                             static_cast<int>(coordY),
                             static_cast<int>(facing),
                             static_cast<int>(mission),
                             static_cast<int>(typeIndex),
                             pTypeName,
                             pTarName,
                             static_cast<int>(tarX),
                             static_cast<int>(tarY),
                             static_cast<int>(speed),
                             pNavName,
                             static_cast<int>(navX),
                             static_cast<int>(navY));
            }

            NdMono(pMono, kNdFmtMonoInfantry, pHouseTypeID, hashAcc);
        }
    }

    // ................ 车辆 ................
    if (UnitClass::Array != nullptr) {
        DynamicVectorClass<UnitClass*>* const pVec = UnitClass::Array;
        for (int32 hi = 0; hi < houseCount; ++hi) {
            HouseClass* const pHouse = HouseClass::Array[hi];
            if (pHouse == nullptr) {
                continue;
            }

            int32 hashAcc = 0;
            const char* const pHouseTypeID =
                (pHouse->Type != nullptr) ? pHouse->Type->get_ID() : "";

            std::fprintf(pFile, kNdHeadUnits, pHouseTypeID,
                         static_cast<int>(pHouse->PlayerNumber));

            for (int32 di = 0; di < pVec->Count; ++di) {
                UnitClass* const pUnit = (*pVec)[di];
                if (pUnit == nullptr || pUnit->Owner != pHouse) {
                    continue;
                }

                FootClass* const pFoot = static_cast<FootClass*>(pUnit);

                const CoordStruct& cached = pUnit->CachedLocation;
                int32 facing = NdFacingOf(pUnit);
                hashAcc = NdHashAcc(hashAcc,
                                    NdDiv10(cached.X) + facing +
                                        (NdDiv10(cached.Y) << 16));

                int32 tarX = 0;
                int32 tarY = 0;
                int32 navX = 0;
                int32 navY = 0;
                const char* pTarName = kNdStrNone;
                const char* pNavName = kNdStrNone;
                NdSubBlock(pUnit->GetTarget(), tarX, tarY, pTarName);
                NdSubBlock(pFoot->CurrentTarget, navX, navY, pNavName);

                // asm: 驱动 [obj+674h] 的行走机构（IPiggyback 三连查询）：
                //   vt+0C4h -> SpdAcc / vt+0C0h -> TrkInd / vt+0BCh -> TrkNum
                ILocomotion* const pLoco = pUnit->Locomotor;
                int32 trackNumber = 0;
                int32 trackIndex = 0;
                int32 speedAcc = 0;
                if (pLoco != nullptr) {
                    trackNumber = pLoco->Get_Track_Number();
                    trackIndex = pLoco->Get_Track_Index();
                    speedAcc = pLoco->Get_Speed_Accum();
                }

                // asm: Facing2 取 +3A0h，Facing 再取一次 +388h
                const int32 facing2 = NdFacing2Of(pFoot);
                facing = NdFacingOf(pUnit);

                TechnoTypeClass* const pType = pUnit->Type;
                const int32 typeIndex =
                    (pType != nullptr) ? pType->Get_ArrayIndex() : 0;
                const char* const pTypeName =
                    (pType != nullptr) ? pType->get_ID() : "";

                const int32 mission = static_cast<int32>(pUnit->GetMission());

                const CoordStruct coord = pUnit->GetCoords();
                const int32 coordX = coord.X;
                const int32 coordY = coord.Y;

                std::fprintf(pFile, kNdFmtCoordUnits,
                             static_cast<int>(coordX),
                             static_cast<int>(coordY),
                             static_cast<int>(facing),
                             static_cast<int>(facing2),
                             static_cast<int>(mission),
                             static_cast<int>(typeIndex),
                             pTypeName,
                             pTarName,
                             static_cast<int>(tarX),
                             static_cast<int>(tarY),
                             pNavName,
                             static_cast<int>(navX),
                             static_cast<int>(navY),
                             static_cast<int>(trackNumber),
                             static_cast<int>(trackIndex),
                             static_cast<int>(speedAcc));
            }

            NdMono(pMono, kNdFmtMonoUnits, pHouseTypeID, hashAcc);
        }
    }

    // ................ 建筑（只有 TarCom）................
    if (BuildingClass::Array != nullptr) {
        DynamicVectorClass<BuildingClass*>* const pVec = BuildingClass::Array;
        for (int32 hi = 0; hi < houseCount; ++hi) {
            HouseClass* const pHouse = HouseClass::Array[hi];
            if (pHouse == nullptr) {
                continue;
            }

            int32 hashAcc = 0;
            const char* const pHouseTypeID =
                (pHouse->Type != nullptr) ? pHouse->Type->get_ID() : "";

            std::fprintf(pFile, kNdHeadBuildings, pHouseTypeID,
                         static_cast<int>(pHouse->PlayerNumber));

            for (int32 di = 0; di < pVec->Count; ++di) {
                BuildingClass* const pBuilding = (*pVec)[di];
                if (pBuilding == nullptr || pBuilding->Owner != pHouse) {
                    continue;
                }

                const CoordStruct& cached = pBuilding->CachedLocation;
                int32 facing = NdFacingOf(pBuilding);
                hashAcc = NdHashAcc(hashAcc,
                                    NdDiv10(cached.X) + facing +
                                        (NdDiv10(cached.Y) << 16));

                int32 tarX = 0;
                int32 tarY = 0;
                const char* pTarName = kNdStrNone;
                NdSubBlock(pBuilding->GetTarget(), tarX, tarY, pTarName);

                facing = NdFacingOf(pBuilding);

                TechnoTypeClass* const pType = pBuilding->Type;
                const int32 typeIndex =
                    (pType != nullptr) ? pType->Get_ArrayIndex() : 0;
                const char* const pTypeName =
                    (pType != nullptr) ? pType->get_ID() : "";

                const int32 mission =
                    static_cast<int32>(pBuilding->GetMission());

                const CoordStruct coord = pBuilding->GetCoords();
                const int32 coordX = coord.X;
                const int32 coordY = coord.Y;

                std::fprintf(pFile, kNdFmtCoordBldAir,
                             static_cast<int>(coordX),
                             static_cast<int>(coordY),
                             static_cast<int>(facing),
                             static_cast<int>(mission),
                             static_cast<int>(typeIndex),
                             pTypeName,
                             pTarName,
                             static_cast<int>(tarX),
                             static_cast<int>(tarY));
            }

            NdMono(pMono, kNdFmtMonoBuildings, pHouseTypeID, hashAcc);
        }
    }

    // ................ 飞机（只有 TarCom）................
    if (AircraftClass::Array != nullptr) {
        DynamicVectorClass<AircraftClass*>* const pVec = AircraftClass::Array;
        for (int32 hi = 0; hi < houseCount; ++hi) {
            HouseClass* const pHouse = HouseClass::Array[hi];
            if (pHouse == nullptr) {
                continue;
            }

            int32 hashAcc = 0;
            const char* const pHouseTypeID =
                (pHouse->Type != nullptr) ? pHouse->Type->get_ID() : "";

            std::fprintf(pFile, kNdHeadAircraft, pHouseTypeID,
                         static_cast<int>(pHouse->PlayerNumber));

            for (int32 di = 0; di < pVec->Count; ++di) {
                AircraftClass* const pAircraft = (*pVec)[di];
                if (pAircraft == nullptr || pAircraft->Owner != pHouse) {
                    continue;
                }

                const CoordStruct& cached = pAircraft->CachedLocation;
                int32 facing = NdFacingOf(pAircraft);
                hashAcc = NdHashAcc(hashAcc,
                                    NdDiv10(cached.X) + facing +
                                        (NdDiv10(cached.Y) << 16));

                int32 tarX = 0;
                int32 tarY = 0;
                const char* pTarName = kNdStrNone;
                NdSubBlock(pAircraft->GetTarget(), tarX, tarY, pTarName);

                facing = NdFacingOf(pAircraft);

                TechnoTypeClass* const pType = pAircraft->Type;
                const int32 typeIndex =
                    (pType != nullptr) ? pType->Get_ArrayIndex() : 0;
                const char* const pTypeName =
                    (pType != nullptr) ? pType->get_ID() : "";

                const int32 mission =
                    static_cast<int32>(pAircraft->GetMission());

                const CoordStruct coord = pAircraft->GetCoords();
                const int32 coordX = coord.X;
                const int32 coordY = coord.Y;

                std::fprintf(pFile, kNdFmtCoordBldAir,
                             static_cast<int>(coordX),
                             static_cast<int>(coordY),
                             static_cast<int>(facing),
                             static_cast<int>(mission),
                             static_cast<int>(typeIndex),
                             pTypeName,
                             pTarName,
                             static_cast<int>(tarX),
                             static_cast<int>(tarY));
            }

            NdMono(pMono, kNdFmtMonoAircraft, pHouseTypeID, hashAcc);
        }
    }

    // ------------------------------------------------------------------------
    // asm loc_64EC94: MAP LAYER 段（vec_ObjectsInLayers，原版 5 张、步长 18h）
    //   本工程的绘制层表由 DisplayClass 持有，实际张数是 3（见 DisplayClass.cpp
    //   的 kDisplayLayerCount），故按该张数转储，表内容与访问方式一致。
    // ------------------------------------------------------------------------
    {
        int32 layerAcc = 0;
        const int32 layerCount = DisplayClass::GetLayerCount();
        for (int32 layer = 0; layer < layerCount; ++layer) {
            std::fprintf(pFile, kFmtMapLayer, static_cast<int>(layer));

            const int32 count = DisplayClass::GetLayerObjectCount(layer);
            for (int32 oi = 0; oi < count; ++oi) {
                ObjectClass* const pObj = DisplayClass::GetLayerObject(layer, oi);
                if (pObj == nullptr) {
                    continue;
                }

                AbstractType what = pObj->WhatAmI();
                // asm: 类型号 4（Anim）且 Fetch_ID()==-2 的条目跳过
                if (what == AbstractType::Anim &&
                    pObj->Fetch_ID() == -2) {
                    continue;
                }

                const CoordStruct& cached = pObj->CachedLocation;
                layerAcc = NdHashAcc(layerAcc,
                                     NdDiv10(cached.X) + static_cast<int32>(what) +
                                         (NdDiv10(cached.Y) << 16));

                // asm: "Object %d: %x" 的哈希 = (Y/10 << 16) + X/10（不含类型号）
                std::fprintf(pFile, kFmtObject, static_cast<int>(oi),
                             static_cast<uint32>((NdDiv10(cached.Y) << 16) +
                                                 NdDiv10(cached.X)));

                what = pObj->WhatAmI();
                if (static_cast<int32>(what) == 0x2B ||
                    static_cast<int32>(what) == 0x13) {
                    // asm: 0x2B(Wave) / 0x13(BuildingLight) 走 "%s\t(Type:%d) "
                    //   分支，%d 槽取的是为 vt+2Ch 压栈的那个 0。
                    std::fprintf(pFile, kFmtTypePlain,
                                 AbstractClass::GetClassName(what), 0);
                } else {
                    ObjectTypeClass* const pType = pObj->GetType();
                    const int32 typeIndex =
                        (pType != nullptr) ? pType->Get_ArrayIndex() : 0;
                    const char* const pTypeName =
                        (pType != nullptr) ? pType->get_ID() : "";
                    std::fprintf(pFile, kFmtTypeNamed,
                                 AbstractClass::GetClassName(what),
                                 static_cast<int>(typeIndex),
                                 pTypeName);
                }

                HouseClass* const pOwner = pObj->GetOwningHouse();
                if (pOwner != nullptr) {
                    const char* const pOwnerTypeID =
                        (pOwner->Type != nullptr) ? pOwner->Type->get_ID() : "";
                    std::fprintf(pFile, kNdFmtOwnerSD, pOwnerTypeID,
                                 static_cast<int>(pOwner->PlayerNumber));
                } else {
                    std::fprintf(pFile, "%s", kNdFmtOwnerNone);
                }
            }
        }
        NdMono(pMono, kNdFmtMapLayersX, layerAcc);
    }

    // ------------------------------------------------------------------------
    // asm loc_64EEBD: LOGIC LAYER（vec_Logics，9 路类型分派）
    // ------------------------------------------------------------------------
    {
        int32 logicAcc = 0;
        std::fprintf(pFile, "%s", kFmtLogicLayer);

        if (vec_Logics != nullptr) {
            for (int32 di = 0; di < vec_Logics->Count; ++di) {
                LogicClass* const pLogic = (*vec_Logics)[di];
                if (pLogic == nullptr) {
                    continue;
                }

                // vec_Logics 里登记的实体都来自 ObjectClass 家族（ObjectClass
                // 以 LogicClass 为基类），故按对象面取坐标/类型/归属。
                ObjectClass* const pObj = static_cast<ObjectClass*>(pLogic);

                AbstractType what = pObj->WhatAmI();
                if (what == AbstractType::Anim && pObj->Fetch_ID() == -2) {
                    continue;
                }

                const CoordStruct& cached = pObj->CachedLocation;
                logicAcc = NdHashAcc(logicAcc,
                                     NdDiv10(cached.X) + static_cast<int32>(what) +
                                         (NdDiv10(cached.Y) << 16));

                std::fprintf(pFile, kFmtObject, static_cast<int>(di),
                             static_cast<uint32>((NdDiv10(cached.Y) << 16) +
                                                 NdDiv10(cached.X)));

                for (const NdLogicType& entry : kNdLogicTypes) {
                    if (static_cast<int32>(what) != entry.What) {
                        continue;
                    }
                    ObjectTypeClass* const pType = pObj->GetType();
                    const int32 typeIndex =
                        (pType != nullptr) ? pType->Get_ArrayIndex() : 0;
                    const char* const pTypeName =
                        (pType != nullptr) ? pType->get_ID() : "";
                    std::fprintf(pFile, entry.Format,
                                 static_cast<int>(typeIndex), pTypeName);
                    break;
                }

                HouseClass* const pOwner = pObj->GetOwningHouse();
                if (pOwner != nullptr) {
                    const char* const pOwnerTypeID =
                        (pOwner->Type != nullptr) ? pOwner->Type->get_ID() : "";
                    std::fprintf(pFile, kNdFmtOwnerS, pOwnerTypeID);
                } else {
                    std::fprintf(pFile, "%s", kNdFmtOwnerNone);
                }
            }
        }
        NdMono(pMono, kNdFmtLogicX, logicAcc);
    }

    // ------------------------------------------------------------------------
    // asm loc_64F1D0: 随机数 + 本帧号（先 mono，再文件）
    // ------------------------------------------------------------------------
    const int32 randomNumber =
        (ScenarioClass::Instance != nullptr)
            ? ScenarioClass::Instance->Random.Next()
            : 0;
    NdMono(pMono, kNdFmtRandomX, randomNumber);
    std::fprintf(pFile, kFmtRandomNumber, static_cast<int>(randomNumber));
    NdMono(pMono, kNdFmtMyFrameMono, static_cast<int>(Game::CurrentFrame));
    std::fprintf(pFile, kNdFmtMyFrame, static_cast<int>(Game::CurrentFrame));

    // ------------------------------------------------------------------------
    // asm loc_64F2B7: 出错事件块（正常帧为空）
    // ------------------------------------------------------------------------
    if (pEvent != nullptr) {
        std::fprintf(pFile, "%s", kFmtOffendingEvent);
        std::fprintf(pFile, kFmtEventType, static_cast<int>(pEvent->Kind));
        std::fprintf(pFile, kFmtEventFrame, static_cast<int>(pEvent->Frame));
        std::fprintf(pFile, kFmtEventId,
                     static_cast<uint32>(static_cast<int32>(pEvent->PlayerID)));
        std::fprintf(pFile, kFmtEventCrc, pEvent->CRC);
        std::fprintf(pFile, kFmtEventCmdCount,
                     static_cast<int>(pEvent->CommandCount));
        std::fprintf(pFile, kFmtEventDelay, static_cast<int>(pEvent->Delay));
    }

    // ------------------------------------------------------------------------
    // asm 0x64F2D3 .. 0x650A73: 43 个类型 CRC 段
    // ------------------------------------------------------------------------
    NdCRCFixedSection(pFile, kNdCrcTitleHouseType, HouseTypeClass::Array,
                      HouseTypeClass::ArrayCount);
    NdCRCFixedSection(pFile, kNdCrcTitleHouse, HouseClass::Array,
                      HouseClass::ArrayCount);
    NdCRCSection(pFile, kNdCrcTitleUnitType, UnitTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleUnit, UnitClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleInfantryType, InfantryTypeClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleInfantry, InfantryClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleBuildingType, BuildingTypeClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleBuilding, BuildingClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleAircraftType, AircraftTypeClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleAircraft, AircraftClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleAnim, AnimClass::Array, true);
    NdCRCSection(pFile, kNdCrcTitleAnimType, AnimTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleTaskForce, TaskForceClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleTeamType, TeamTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleTeam, TeamClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleScriptType, ScriptTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleScript, ScriptClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleTagType, TagTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleTag, TagClass::Array4, false);
    NdCRCSection(pFile, kNdCrcTitleTriggerType, TriggerTypeClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleTrigger, TriggerClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleAction, TActionClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleEvent, TEventClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleFactory, FactoryClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleVoxelAnimType, VoxelAnimTypeClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleVoxelAnim, VoxelAnimClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleWarhead, WarheadTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleWeapon, WeaponTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleParticleType, ParticleTypeClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleParticle, ParticleClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleParticleSystem, ParticleSystemClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleParticleSystemType,
                 ParticleSystemTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleBulletType, BulletTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleBullet, BulletClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleWaypointPath, WaypointPathClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleSmudgeType, SmudgeTypeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleOverlayType, OverlayTypeClass::Array,
                 false);
    // Light Source 段：asm 走 vec_LightSources，本工程由 LightSourcePoolClass
    // 持有同一批光源（IsActive 即"在册"）。
    {
        std::fprintf(pFile, "%s", kNdCrcTitleLightSource);
        std::fprintf(pFile, "%s", kNdFmtCrcIndex);
        std::fprintf(pFile, "%s", kNdFmtCrcWaves);

        LightSourcePoolClass* const pPool = LightSourcePoolClass::GetInstance();
        if (pPool != nullptr) {
            int32 row = 0;
            for (int32 i = 0; i < MAX_POOLED_LIGHTS; ++i) {
                const LightSourceClass& light = pPool->LightSources[i];
                if (!light.IsActive) {
                    continue;
                }
                std::fprintf(pFile, kNdFmtCrcRow, static_cast<int>(row++),
                             NdObjectCRC(&light));
            }
        }
    }
    NdCRCSection(pFile, kNdCrcTitleBuildingLight, BuildingLightClass::Array,
                 false);
    NdCRCSection(pFile, kNdCrcTitleTube, TubeClass::Array, false);
    NdCRCSection(pFile, kNdCrcTitleSides, SideClass::Sides, false);
    NdCRCSection(pFile, kNdCrcTitleTiberium, TiberiumClass::Array, false);

    // Em Pulse 段：asm 走 vec_EMPulses，本工程由 EMPulseManagerClass 持有。
    {
        std::fprintf(pFile, "%s", kNdCrcTitleEmPulse);
        std::fprintf(pFile, "%s", kNdFmtCrcIndex);
        std::fprintf(pFile, "%s", kNdFmtCrcWaves);

        EMPulseManagerClass* const pManager = EMPulseManagerClass::GetInstance();
        if (pManager != nullptr) {
            int32 row = 0;
            for (int32 i = 0; i < MAX_EMPULSES; ++i) {
                const EMPulseClass* const pPulse = pManager->GetEMPulse(i);
                if (pPulse == nullptr) {
                    continue;
                }
                std::fprintf(pFile, kNdFmtCrcRow, static_cast<int>(row++),
                             NdObjectCRC(pPulse));
            }
        }
    }

    std::fclose(pFile);
}
