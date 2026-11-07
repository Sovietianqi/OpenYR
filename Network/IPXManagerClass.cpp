// ============================================================================
// Network/IPXManagerClass.cpp
//
// 逐句对照 asm：
//   sub_543080  Stop_Listening  —— mov eax,[ecx+48h]; mov byte [eax+0C8h],0
//   sub_543090  Start_Listening —— mov eax,[ecx+48h]; mov byte [eax+0C8h],1
//   sub_540C60  Set_Timing      —— 存 [+58h]/[+5Ch]/[+60h]，打印 RetryDelta /
//                                  MaxAhead，apply 时下发到传输与每个连接
//   sub_541260  Send            —— 按名字找到节点后交给传输层
//   sub_541370  Receive         —— 两道开关都开时才收包
//   sub_5424C0  Get_Send_Queue_Length —— [48h]->[4] 的队列长度
//   sub_5422D0  Get_Receive_Count     —— [48h]->[4]->[28h]
//   sub_53EF30  Match_Node_Address    —— repe cmpsb 比较 4/6 字节地址
//   sub_7931A0  First_Active_Name     —— 第一个 +0 位为奇数的连接项
// ============================================================================

#include "Network/IPXManagerClass.h"

#include "Network/ConnectionClass.h"
#include "Network/MPModes.h"
#include "Game/MPTeams.h"

#include <cstdio>
#include <cstring>
#include <cwctype>

// asm: 全局对象 IPXManager
IPXManagerClass IPXManager;
IPXManagerClass* IPXManagerClass::Instance = nullptr;

namespace {

// ---------------------------------------------------------------------------
// 传输帧布局（本树 IPXConnectionClass 之上的一层薄封装）。
//
//   +0x00  int16  Magic      0xAA04（YR 包标记，对应 word_A8D810）
//   +0x02  int16  包体长度
//   +0x04  uint8  Sender[12] 发送方地址（对应 dword_A8D804 指向的地址块）
//   +0x10  char   Name[40h]  发送方玩家名（对应 byte_A8D63C）
//   +0x50  uint8  Payload[]  包体
// ---------------------------------------------------------------------------
const int16  k_FrameMagic     = 0xAA04;
const int32  k_FrameAddrOff   = 0x04;
const int32  k_FrameNameOff   = 0x10;
const int32  k_FramePayloadOff = 0x50;
const int32  k_FrameNameLen   = 0x40;
const int32  k_FrameMax       = 0x50 + 0x1C7;

inline bool Wide_Icmp(const wchar_t* pA, const wchar_t* pB)
{
    if (pA == nullptr || pB == nullptr)
        return false;
    while (*pA != 0 && *pB != 0) {
        if (std::towlower(static_cast<wint_t>(*pA)) !=
            std::towlower(static_cast<wint_t>(*pB)))
            return false;
        ++pA;
        ++pB;
    }
    return *pA == *pB;
}

inline bool Narrow_Matches_Wide(const char* pNarrow, const wchar_t* pWide)
{
    if (pNarrow == nullptr || pWide == nullptr)
        return false;
    while (*pNarrow != 0 && *pWide != 0) {
        if (std::towlower(static_cast<wint_t>(
                static_cast<unsigned char>(*pNarrow))) !=
            std::towlower(static_cast<wint_t>(*pWide)))
            return false;
        ++pNarrow;
        ++pWide;
    }
    return *pNarrow == 0 && *pWide == 0;
}

void Copy_Wide_To_Narrow(const wchar_t* pSrc, char* pDst, int32 dstSize)
{
    if (pDst == nullptr || dstSize <= 0)
        return;
    int32 i = 0;
    if (pSrc != nullptr) {
        for (; i < dstSize - 1 && pSrc[i] != 0; ++i)
            pDst[i] = static_cast<char>(pSrc[i] & 0xFF);
    }
    pDst[i] = '\0';
}

// 在连接表里按玩家名找对端连接（asm: sub_541260 先按 __wcsicmp 找节点下标）
ConnectionClass* Find_Connection(const wchar_t* pNodeName)
{
    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    if (pMgr == nullptr)
        return nullptr;

    const int32 count = pMgr->GetConnectionCount();
    for (int32 i = 0; i < count; ++i) {
        ConnectionClass* pConn = pMgr->GetConnection(i);
        if (pConn != nullptr && Narrow_Matches_Wide(pConn->PeerName, pNodeName))
            return pConn;
    }
    return nullptr;
}

}  // namespace

// ============================================================================
// 构造 / 析构
// ============================================================================
IPXManagerClass::IPXManagerClass()
    : Connection(new IPXConnectionClass())
    , Is_Open(true)
    , Is_Service(true)
    , ConnectionCount(0)
    , RetryDelta(0)
    , RetryTimeout(0)
    , MaxAhead(0)
    , SendOverflow(0)
    , AddressLength(4)          // asm: dword_828140 —— TCP/IP 形态为 4 字节地址
    , PacketAddress{}
    , PacketName{}
    , PacketPayload{}
    , PacketLength(0)
{
    Instance = this;
    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    if (pMgr != nullptr)
        ConnectionCount = pMgr->GetConnectionCount();
}

IPXManagerClass::~IPXManagerClass()
{
    if (Instance == this)
        Instance = nullptr;
    delete Connection;
    Connection = nullptr;
}

// ============================================================================
// asm: sub_543080
// ============================================================================
void IPXManagerClass::Stop_Listening()
{
    if (Connection != nullptr)
        Connection->Listening = false;              // asm: [eax+0C8h] = 0
}

// ============================================================================
// asm: sub_543090
// ============================================================================
void IPXManagerClass::Start_Listening()
{
    if (Connection != nullptr)
        Connection->Listening = true;               // asm: [eax+0C8h] = 1
}

// ============================================================================
// asm: sub_540C60
// ============================================================================
void IPXManagerClass::Set_Timing(int32 retryDelta, int32 timeout, int32 maxAhead,
                                 int32 apply)
{
    RetryDelta   = retryDelta;                      // asm: [esi+58h] = arg_0
    RetryTimeout = timeout;                         // asm: [esi+5Ch] = arg_4
    MaxAhead     = maxAhead;                        // asm: [esi+60h] = arg_8

    std::printf("IPX Manager: RetryDelta = %d\n", retryDelta);
    // asm 这里打印的是全局 Factory_BuildSpeedMult；本树没有该全局，
    // 直接打印刚刚写入的 MaxAhead（同一语义，避免影子变量）。
    std::printf("MaxAhead is %d\n", maxAhead);

    // asm: 只有 apply 非 0 才下发；（[this+48h]->vtable[+38h] 的配置回调
    // 在本树由连接对象的 SetTimeout 承载）
    if (apply != 0 && Connection != nullptr) {
        Connection->SetTimeout(RetryTimeout);
    }

    // asm: [esi+44h] 个连接逐个写 +28h/+2Ch/+30h
    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    if (pMgr != nullptr) {
        const int32 count = pMgr->GetConnectionCount();
        ConnectionCount = count;
        for (int32 i = 0; i < count; ++i) {
            ConnectionClass* pConn = pMgr->GetConnection(i);
            if (pConn == nullptr)
                continue;
            pConn->RetransmitTimeout = RetryDelta;      // +28h
            pConn->SetTimeout(RetryTimeout);            // +2Ch
            pConn->MaxPacketSize = MaxAhead;            // +30h
        }
    }
}

// ============================================================================
// asm: sub_541260
// ============================================================================
int32 IPXManagerClass::Send(const void* pPacket, int32 socket, int32 flags,
                            const wchar_t* pNodeName, int32 length)
{
    (void)socket;
    (void)flags;

    // asm: 先在 NameNodeTypes 里按 __wcsicmp 找名字；找不到返回 0
    if (pNodeName == nullptr || pPacket == nullptr || length <= 0)
        return 0;

    ConnectionClass* pTarget = Find_Connection(pNodeName);
    if (pTarget == nullptr)
        return 0;

    // asm: 两道开关都开才真正下发
    if (!Is_Open || !Is_Service)
        return 0;

    if (length > k_FrameMax - k_FramePayloadOff)
        length = k_FrameMax - k_FramePayloadOff;

    uint8 frame[k_FrameMax];
    std::memset(frame, 0, sizeof(frame));
    const int16 magic = k_FrameMagic;
    std::memcpy(frame + 0, &magic, sizeof(magic));
    const int16 payloadLen = static_cast<int16>(length);
    std::memcpy(frame + 2, &payloadLen, sizeof(payloadLen));

    // 发送方地址：取本管理器的主连接地址（4 字节）
    if (Connection != nullptr) {
        const uint32 addr = Connection->Address;
        std::memcpy(frame + k_FrameAddrOff, &addr, sizeof(addr));
    }

    // 发送方名字：本地玩家名（asm: MP::PlayerNames[0]）
    char localName[k_FrameNameLen];
    Copy_Wide_To_Narrow(MP::PlayerNames[0], localName, k_FrameNameLen);
    std::memcpy(frame + k_FrameNameOff, localName,
                static_cast<size_t>(k_FrameNameLen - 1));

    std::memcpy(frame + k_FramePayloadOff, pPacket,
                static_cast<size_t>(length));

    const int32 total = k_FramePayloadOff + length;
    int32 result = pTarget->SendPacket(frame, total) ? 1 : 0;

    if (result == 0) {
        // asm: 发包失败 -> [edi+0A0h]++ 并打印 "Send overflow %d\n"
        ++SendOverflow;
        std::printf("Send overflow %d\n", SendOverflow);
    }
    return result;
}

// ============================================================================
// asm: sub_541370
// ============================================================================
int32 IPXManagerClass::Receive(int32* pType, int32* pUnknown, void** ppData,
                               int16* pWord)
{
    // asm: mov al,[ecx+4] / test / jz; mov al,[ecx+5] / test / jz
    if (!Is_Open || !Is_Service || Connection == nullptr)
        return 0;

    uint8 frame[k_FrameMax];
    const int32 n = Connection->ReceivePacket(frame, k_FrameMax);
    if (n <= 0)
        return 0;

    int16 magic = 0;
    std::memcpy(&magic, frame + 0, sizeof(magic));
    if (magic != k_FrameMagic)
        return 0;

    int16 payloadLen = 0;
    std::memcpy(&payloadLen, frame + 2, sizeof(payloadLen));
    if (payloadLen <= 0 || payloadLen > n - k_FramePayloadOff)
        return 0;

    if (Connection->PendingReceive > 0)
        --Connection->PendingReceive;

    // asm: dword_A8D804 指向发送方地址块；word_A8D810 = 0AA04h
    std::memcpy(PacketAddress, frame + k_FrameAddrOff, sizeof(PacketAddress));
    std::memcpy(PacketName, frame + k_FrameNameOff,
                sizeof(PacketName) - 1);
    PacketName[sizeof(PacketName) - 1] = '\0';

    if (pType != nullptr) {
        int32 type = 0;
        std::memcpy(&type, frame + k_FramePayloadOff, sizeof(type));
        *pType = type;
    }
    // 包体：供按块文件传输取用
    if (payloadLen > static_cast<int16>(sizeof(PacketPayload)))
        payloadLen = static_cast<int16>(sizeof(PacketPayload));
    std::memcpy(PacketPayload, frame + k_FramePayloadOff,
                static_cast<size_t>(payloadLen));
    PacketLength = payloadLen;

    if (pUnknown != nullptr)
        *pUnknown = 0;
    if (ppData != nullptr)
        *ppData = PacketAddress;
    if (pWord != nullptr)
        *pWord = magic;

    return 1;
}

// ============================================================================
// asm: sub_5424C0 —— [ecx+48h]->[4] 的队列长度（sub_48BA80）
// ============================================================================
int32 IPXManagerClass::Get_Send_Queue_Length()
{
    if (Connection == nullptr)
        return 0;
    return Connection->PacketQueue.GetCount();
}

// ============================================================================
// asm: sub_5422D0 —— [ecx+48h]->[4]->[28h]
// ============================================================================
int32 IPXManagerClass::Get_Receive_Count()
{
    if (!Is_Open || !Is_Service || Connection == nullptr)
        return 0;
    return Connection->PendingReceive;
}

// ============================================================================
// asm: sub_53EF30 —— repe cmpsb(node+4, arg+4)，长度 4 或 6
// ============================================================================
bool IPXManagerClass::Match_Node_Address(int32 nodeIndex, const void* pAddress)
{
    if (pAddress == nullptr)
        return false;

    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    if (pMgr == nullptr)
        return false;

    ConnectionClass* pConn = pMgr->GetConnection(nodeIndex);
    if (pConn == nullptr)
        return false;

    // asm: ecx = dword_828140 - 2 / neg / sbb / and 2 / add 4 —— 4 或 6 字节
    int32 len = (AddressLength == 2) ? 4 : 6;
    if (len > static_cast<int32>(sizeof(uint32)))
        len = static_cast<int32>(sizeof(uint32));

    uint8 local[12];
    std::memset(local, 0, sizeof(local));
    std::memcpy(local, &pConn->Address, sizeof(uint32));

    return std::memcmp(local, pAddress, static_cast<size_t>(len)) == 0;
}

// ============================================================================
// asm: sub_7931A0 —— 抄出第一个 +0 位为奇数的连接项，返回其名字（+0x24）
// ============================================================================
const char* IPXManagerClass::First_Active_Name()
{
    ConnectionManager* pMgr = ConnectionManager::GetInstance();
    if (pMgr == nullptr)
        return nullptr;                                // asm: xor eax,eax

    const int32 count = pMgr->GetConnectionCount();
    for (int32 i = 0; i < count; ++i) {
        ConnectionClass* pConn = pMgr->GetConnection(i);
        if (pConn != nullptr && pConn->Connected)      // asm: test byte [entry],1
            return pConn->PeerName;                    // asm: entry + 24h
    }
    return nullptr;
}
