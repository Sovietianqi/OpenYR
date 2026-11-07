#include "ConnectionClass.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

// ============================================================
// ConnectionClass
// ============================================================

ConnectionClass::ConnectionClass()
    : Socket(0), Address(0), Port(0), Connected(false), PeerName{}, Timeout(5000), LastActivity(0)
    , SequenceNumber(0), AckNumber(0), RemoteSequence(0)
    , Latency(0), LatencyAverage(0), LatencyDeviation(0)
    , PacketLoss(0.0f), PacketsSent(0), PacketsReceived(0), PacketsLost(0)
    , TotalBytesSent(0), TotalBytesReceived(0)
    , State(ConnectionState::Disconnected)
    , RetransmitCount(0), MaxRetransmits(10)
    , RetransmitTimeout(200), KeepAliveInterval(1000)
    , LastSendTime(0), LastReceiveTime(0)
    , IsReliable(false), IsEncrypted(false)
    , MaxPacketSize(1024), FragmentedPackets{}
{
    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
        PendingAcks[i].Sequence = 0;
        PendingAcks[i].Data = nullptr;
        PendingAcks[i].Length = 0;
        PendingAcks[i].Sent = false;
        PendingAcks[i].SendTime = 0;
        PendingAcks[i].FirstSendTime = 0;
        PendingAcks[i].Retransmits = 0;
    }
}

ConnectionClass::~ConnectionClass() {
    for (int32 i = 0; i < PacketQueue.GetCount(); ++i) {
        PacketData* pkt = PacketQueue[i];
        delete pkt;
    }
    PacketQueue.Clear();

    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
        if (PendingAcks[i].Data) {
            std::free(PendingAcks[i].Data);
            PendingAcks[i].Data = nullptr;
        }
    }
}

bool ConnectionClass::IsConnected() const {
    return Connected && State == ConnectionState::Connected;
}

uint32 ConnectionClass::GetAddress() const {
    return Address;
}

void ConnectionClass::SetTimeout(int32 timeoutMs) {
    Timeout = timeoutMs;
    if (Timeout < 100) Timeout = 100;
    if (Timeout > 30000) Timeout = 30000;
}

int32 ConnectionClass::GetTimeout() const {
    return Timeout;
}

void ConnectionClass::QueuePacket(const uint8* data, int32 len) {
    if (!data || len <= 0 || len > MaxPacketSize) return;

    PacketData* pkt = new PacketData();
    if (pkt->Allocate(len)) {
        for (int32 i = 0; i < len; ++i) {
            pkt->WriteByte(data[i]);
        }
        pkt->Reset();
        PacketQueue.Add(pkt);
    } else {
        delete pkt;
    }
}

bool ConnectionClass::ProcessQueue() {
    bool anySent = false;
    for (int32 i = 0; i < PacketQueue.GetCount(); ++i) {
        PacketData* pkt = PacketQueue[i];
        if (pkt && pkt->Length > 0) {
            if (SendPacket(pkt->Data, pkt->Length)) {
                anySent = true;
            }
        }
    }
    PacketQueue.Clear();
    return anySent;
}

int32 ConnectionClass::BuildReliablePacket(const uint8* data, int32 len, uint8* outBuffer, int32 outMaxLen) {
    if (!data || !outBuffer || len <= 0 || outMaxLen < len + 8) return -1;

    int32 headerSize = 8;
    int32 totalSize = headerSize + len;
    if (totalSize > outMaxLen) return -1;

    // Write header: sequence number (4 bytes) + ack number (4 bytes)
    outBuffer[0] = static_cast<uint8>(SequenceNumber & 0xFF);
    outBuffer[1] = static_cast<uint8>((SequenceNumber >> 8) & 0xFF);
    outBuffer[2] = static_cast<uint8>((SequenceNumber >> 16) & 0xFF);
    outBuffer[3] = static_cast<uint8>((SequenceNumber >> 24) & 0xFF);

    outBuffer[4] = static_cast<uint8>(AckNumber & 0xFF);
    outBuffer[5] = static_cast<uint8>((AckNumber >> 8) & 0xFF);
    outBuffer[6] = static_cast<uint8>((AckNumber >> 16) & 0xFF);
    outBuffer[7] = static_cast<uint8>((AckNumber >> 24) & 0xFF);

    // Copy payload
    std::memcpy(outBuffer + headerSize, data, len);

    // Store in pending ACKs for retransmission
    int32 slot = -1;
    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
        if (!PendingAcks[i].Sent || PendingAcks[i].Acked) {
            if (PendingAcks[i].Data) {
                std::free(PendingAcks[i].Data);
                PendingAcks[i].Data = nullptr;
            }
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        PendingAcks[slot].Sequence = SequenceNumber;
        PendingAcks[slot].Data = static_cast<uint8*>(std::malloc(len));
        if (PendingAcks[slot].Data) {
            std::memcpy(PendingAcks[slot].Data, data, len);
            PendingAcks[slot].Length = len;
            PendingAcks[slot].Sent = false;
            PendingAcks[slot].SendTime = 0;
            PendingAcks[slot].FirstSendTime = 0;
            PendingAcks[slot].Retransmits = 0;
            PendingAcks[slot].Acked = false;
        }
    }

    ++SequenceNumber;
    return totalSize;
}

bool ConnectionClass::ProcessIncomingPacket(const uint8* data, int32 len) {
    if (!data || len < 8) return false;

    // Parse header: remote sequence (4 bytes) + remote ack (4 bytes)
    uint32 remoteSeq = static_cast<uint32>(data[0]) |
                       (static_cast<uint32>(data[1]) << 8) |
                       (static_cast<uint32>(data[2]) << 16) |
                       (static_cast<uint32>(data[3]) << 24);

    uint32 remoteAck = static_cast<uint32>(data[4]) |
                       (static_cast<uint32>(data[5]) << 8) |
                       (static_cast<uint32>(data[6]) << 16) |
                       (static_cast<uint32>(data[7]) << 24);

    int32 payloadLen = len - 8;

    // Update remote sequence tracking
    RemoteSequence = remoteSeq;

    // Process ACKs from remote
    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
        if (PendingAcks[i].Sent && !PendingAcks[i].Acked) {
            if (remoteAck >= PendingAcks[i].Sequence) {
                // Packet was acknowledged
                PendingAcks[i].Acked = true;
                PendingAcks[i].SendTime = 0;
            }
        }
    }

    ++PacketsReceived;
    TotalBytesReceived += len;
    LastReceiveTime = static_cast<int32>(SystemTimer::GetTime());
    LastActivity.Start(0);

    return true;
}

void ConnectionClass::UpdateConnection() {
    int32 currentTime = static_cast<int32>(SystemTimer::GetTime());

    // Check for connection timeout
    if (State == ConnectionState::Connected) {
        int32 elapsed = currentTime - LastReceiveTime;
        if (elapsed > Timeout && LastReceiveTime > 0) {
            State = ConnectionState::TimedOut;
            Connected = false;
            return;
        }
    }

    // Send keep-alive if needed
    if (State == ConnectionState::Connected && IsReliable) {
        if (currentTime - LastSendTime > KeepAliveInterval) {
            SendKeepAlive();
        }
    }

    // Check for pending retransmissions
    if (State == ConnectionState::Connected) {
        for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
            if (PendingAcks[i].Sent && !PendingAcks[i].Acked && PendingAcks[i].Data) {
                int32 elapsed = currentTime - PendingAcks[i].SendTime;
                if (elapsed > RetransmitTimeout) {
                    if (PendingAcks[i].Retransmits < MaxRetransmits) {
                        // Retransmit
                        uint8 retransBuffer[1500];
                        BuildRetransmitPacket(
                            PendingAcks[i].Sequence,
                            PendingAcks[i].Data,
                            PendingAcks[i].Length,
                            retransBuffer, 1500);
                        SendPacket(retransBuffer, PendingAcks[i].Length + 8);
                        PendingAcks[i].SendTime = currentTime;
                        PendingAcks[i].Retransmits++;
                        ++RetransmitCount;

                        // Exponential backoff
                        RetransmitTimeout = static_cast<int32>(RetransmitTimeout * 1.5);
                        if (RetransmitTimeout > 5000) RetransmitTimeout = 5000;
                    } else {
                        // Max retransmits exceeded - packet lost
                        PendingAcks[i].Acked = true;
                        ++PacketsLost;
                        PacketLoss = static_cast<float>(PacketsLost) /
                                     static_cast<float>(PacketsSent > 0 ? PacketsSent : 1);
                    }
                }
            }
        }
    }

    // Update latency calculation
    if (LastReceiveTime > 0 && LastSendTime > 0) {
        Latency = LastReceiveTime - LastSendTime;
        if (Latency < 0) Latency = 0;
        if (Latency > 2000) Latency = 2000;

        // Exponential moving average
        static constexpr float Alpha = 0.125f;
        LatencyAverage = static_cast<int32>(
            LatencyAverage * (1.0f - Alpha) + Latency * Alpha);

        float diff = static_cast<float>(Latency - LatencyAverage);
        if (diff < 0) diff = -diff;
        LatencyDeviation = static_cast<int32>(
            LatencyDeviation * (1.0f - Alpha) + diff * Alpha);

        // Adaptive retransmit timeout
        RetransmitTimeout = LatencyAverage + 4 * LatencyDeviation;
        if (RetransmitTimeout < 50) RetransmitTimeout = 50;
        if (RetransmitTimeout > 5000) RetransmitTimeout = 5000;
    }
}

void ConnectionClass::SendKeepAlive() {
    uint8 keepAlive[8] = {0};
    int32 currentTime = static_cast<int32>(SystemTimer::GetTime());
    keepAlive[0] = static_cast<uint8>(currentTime & 0xFF);
    keepAlive[1] = static_cast<uint8>((currentTime >> 8) & 0xFF);
    keepAlive[2] = static_cast<uint8>((currentTime >> 16) & 0xFF);
    keepAlive[3] = static_cast<uint8>((currentTime >> 24) & 0xFF);
    SendPacket(keepAlive, 8);
    LastSendTime = currentTime;
}

void ConnectionClass::BuildRetransmitPacket(uint32 seq, const uint8* data, int32 len,
    uint8* outBuffer, int32 outMaxLen) {
    if (!outBuffer || outMaxLen < len + 8) return;

    outBuffer[0] = static_cast<uint8>(seq & 0xFF);
    outBuffer[1] = static_cast<uint8>((seq >> 8) & 0xFF);
    outBuffer[2] = static_cast<uint8>((seq >> 16) & 0xFF);
    outBuffer[3] = static_cast<uint8>((seq >> 24) & 0xFF);

    outBuffer[4] = static_cast<uint8>(AckNumber & 0xFF);
    outBuffer[5] = static_cast<uint8>((AckNumber >> 8) & 0xFF);
    outBuffer[6] = static_cast<uint8>((AckNumber >> 16) & 0xFF);
    outBuffer[7] = static_cast<uint8>((AckNumber >> 24) & 0xFF);

    if (data) {
        std::memcpy(outBuffer + 8, data, len);
    }
}

int32 ConnectionClass::GetLatency() const {
    return Latency;
}

int32 ConnectionClass::GetAverageLatency() const {
    return LatencyAverage;
}

float ConnectionClass::GetPacketLoss() const {
    return PacketLoss;
}

ConnectionState ConnectionClass::GetState() const {
    return State;
}

void ConnectionClass::SetState(ConnectionState newState) {
    State = newState;
    if (newState == ConnectionState::Disconnected || newState == ConnectionState::TimedOut) {
        Connected = false;
    }
}

void ConnectionClass::SetReliable(bool reliable) {
    IsReliable = reliable;
}

void ConnectionClass::SetMaxPacketSize(int32 maxSize) {
    if (maxSize >= 64 && maxSize <= 65535) {
        MaxPacketSize = maxSize;
    }
}

void ConnectionClass::ResetStatistics() {
    Latency = 0;
    LatencyAverage = 0;
    LatencyDeviation = 0;
    PacketLoss = 0.0f;
    PacketsSent = 0;
    PacketsReceived = 0;
    PacketsLost = 0;
    TotalBytesSent = 0;
    TotalBytesReceived = 0;
    RetransmitCount = 0;
}

uint32 ConnectionClass::CalculateCRC(const uint8* data, int32 len) {
    if (!data || len <= 0) return 0;
    uint32 crc = 0xFFFFFFFF;
    for (int32 i = 0; i < len; ++i) {
        crc ^= static_cast<uint32>(data[i]);
        for (int32 j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================
// ConnectionClass - service queues
//
// 原版把收发两端都放在 CommBufferClass（[this+4]）里：该结构 +0x28 是
// "待确认发送队列"的条数、+0x38 是"已收待交付队列"的条数，服务函数
// 按条目上的 flag 字节决定走哪一趟。本工程用 PendingAcks[] 表达发送
// 队列，条目字段与 asm 的对应关系：
//   asm [entry+0x00] bit1   -> Sent      （已发出/已处理）
//   asm [entry+0x04]        -> SendTime  （首次发送时刻）
//   asm [entry+0x08]        -> SendTime  （最近一次发送时刻）
//   asm [entry+0x0C]        -> Retransmits（重发次数）
//   asm [entry+0x14]        -> Data/Sequence（内层包）
//   asm [this+0x2C]         -> MaxRetransmits
//   asm [this+0x30]         -> Timeout（生命期上限）
// ============================================================

// ----------------------------------------------------------------------------
// asm: ConnectionClass_ServiceSendQueue 第一趟 (asm:639..678)
//   for (i = 0; i < queue.count; ++i) {
//       e = queue[i];
//       if (!(e->flags & 2)) continue;              ; 只看已发出条目
//       if (e->packet->code == 0) {                 ; NOACK 包
//           elapsed = now - e->firstSend;           ; 64 位 tick 差值
//           sub_48BA10(queue, elapsed);             ; 往返时延统计
//       }
//       sub_48B570(queue, 0,0,0, i, 0,0);           ; 结算并移除
//       --i;                                        ; 索引回退
//   }
// ----------------------------------------------------------------------------
int32 ConnectionClass::RetireAcknowledged()
{
    int32 retired = 0;
    const int32 now = static_cast<int32>(SystemTimer::GetTime());

    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i)
    {
        PendingAck& ack = PendingAcks[i];
        if (!ack.Sent || !ack.Acked)          // asm: test byte ptr [ebx],2 / jz
            continue;

        // asm: e->packet->code == 0 -> sub_48BA10(往返时延)
        if (ack.SendTime != 0)
        {
            int32 rtt = now - ack.SendTime;
            if (rtt >= 0)
            {
                if (LatencyAverage == 0)
                    LatencyAverage = rtt;
                else
                    LatencyAverage = (LatencyAverage * 7 + rtt) / 8;  // 指数平滑
                Latency = rtt;
            }
        }

        if (ack.Data != nullptr)
            std::free(ack.Data);              // asm: sub_48B570 释放条目负载
        ack.Data        = nullptr;
        ack.Length      = 0;
        ack.Sent        = false;
        ack.Acked       = false;
        ack.Retransmits = 0;
        ack.SendTime    = 0;

        ++retired;
    }

    return retired;
}

// ----------------------------------------------------------------------------
// asm: ConnectionClass_ServiceSendQueue (asm:621)
//
// 第二趟（asm:679..791）：
//   for (i = 0; i < count; ++i) {
//       e = queue[i];
//       if (e->flags & 2) continue;                  ; 已处理
//       elapsed = now - e->lastSend;
//       if (elapsed <= this->[0x28]) continue;        ; 未到重发间隔
//       vtable+0x24(this, e->seq, e->data, e->len, e->code, isServer, e->id);
//       e->lastSend = now;
//       if (e->retries == 0) {
//           e->firstSend = now;
//           if (e->packet->code == 1) e->flags |= 2;  ; ACK 包只需一次
//       } else {
//           ++this->[0x08];                           ; 重发计数
//       }
//       ++e->retries;
//       if (this->[0x2C] != -1 && e->retries > this->[0x2C]) e->flags |= 4, bad = 1;
//       if (this->[0x30] != -1 && e->lastSend - e->firstSend > this->[0x30])
//                                                            e->flags |= 4, bad = 1;
//   }
//   if (bad) { WWDebugString("bad connection"); return 0; }
//   return 1;
// ----------------------------------------------------------------------------
bool ConnectionClass::Service_Send_Queue()
{
    const int32 now = static_cast<int32>(SystemTimer::GetTime());

    // 第一趟：结算已确认条目（RetireAcknowledged）
    RetireAcknowledged();

    bool bad = false;

    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i)
    {
        PendingAck& ack = PendingAcks[i];
        if (!ack.Sent || ack.Acked || ack.Data == nullptr)  // asm: test [esi],2
            continue;

        // asm: mov ecx,[esi+8] / mov eax,[edi+28h] / sub edx,ecx / cmp edx,eax
        const int32 elapsed = now - ack.SendTime;
        if (elapsed <= RetransmitTimeout)                    // asm: jbe
            continue;

        // asm: 以 vtable+0x24 重发整包（头 8 字节 + 负载）
        {
            uint8 buffer[1500];
            const int32 total = ack.Length + 8;
            if (total > static_cast<int32>(sizeof(buffer)))
                continue;

            BuildRetransmitPacket(ack.Sequence, ack.Data, ack.Length,
                                  buffer, static_cast<int32>(sizeof(buffer)));
            SendPacket(buffer, total);                       // asm: call [eax+24h]
            ++PacketsSent;
            TotalBytesSent += total;
        }

        ack.SendTime = now;                                  // asm: mov [esi+8],ebp

        if (ack.Retransmits == 0)                            // asm: mov eax,[esi+0Ch] / jnz
        {
            ack.FirstSendTime = now;                         // asm: mov [esi+4],ebp
        }
        else
        {
            ++PacketsLost;                                   // asm: inc dword ptr [edi+8]
        }

        ++ack.Retransmits;                                   // asm: inc [esi+0Ch]

        // asm: cmp eax,[edi+2Ch] ; jbe ; flags |= 4 ; bad = 1
        if (MaxRetransmits >= 0 && ack.Retransmits > MaxRetransmits)
            bad = true;

        // asm: [edi+30h] 生命期上限（最近发送 - 首次发送 > 上限）
        if (Timeout >= 0 && (ack.SendTime - ack.FirstSendTime) > Timeout)
            bad = true;
    }

    if (bad)
    {
        // asm: WWDebugString("ConnectionClass::Service_Send_Queue bad connection")
        State = ConnectionState::Error;
        Connected = false;
        return false;                                        // asm: xor eax,eax
    }

    return true;                                             // asm: mov eax,1
}

// ----------------------------------------------------------------------------
// asm: sub_48C3B0 —— 两个服务口的包装
//   call [eax+1Ch]  ; 发送服务
//   if (!eax) return 0
//   call [eax+20h]  ; 接收服务
//   if (!eax) return 0
//   return 1
//
// 接收侧在本工程没有独立的收包缓冲（ConnectionClass 的子类直接从
// socket 读），故这里做两件可确证的事：结算已确认条目、刷新丢包率，
// 并在连接失效时返回 0 让调用链中断。
// ----------------------------------------------------------------------------
bool ConnectionClass::Service_Receive_Queue()
{
    if (State == ConnectionState::Disconnected)          // asm: 第一个服务口失败
        return false;

    RetireAcknowledged();

    // asm: ReceivePacket 里 (sentCount - acked) / sent 的百分比统计
    if (PacketsSent > 0)
    {
        int32 lost = PacketsSent - PacketsReceived;
        if (lost < 0) lost = 0;
        PacketLoss = static_cast<float>(lost) / static_cast<float>(PacketsSent);
    }

    return State != ConnectionState::Error;              // asm: 第二个服务口
}

// ----------------------------------------------------------------------------
// asm: sub_48BF10 —— 连接复位
//   [ecx+34h..40h] = 0 ; [ecx+08h..18h] = 0 ;
//   [ecx+44h] = [ecx+48h] = -1 ; CommBuffer->Reset()
// 本工程用对应的统计字段表达，并清空待确认队列。
// ----------------------------------------------------------------------------
void ConnectionClass::Reset_Connection()
{
    PacketLoss        = 0.0f;      // asm: [ecx+34h..40h] 计数区
    PacketsSent       = 0;
    PacketsReceived   = 0;
    PacketsLost       = 0;
    RetransmitCount   = 0;

    TotalBytesSent     = 0;        // asm: [ecx+08h..18h]
    TotalBytesReceived = 0;
    Latency            = 0;
    LatencyAverage     = 0;
    LatencyDeviation   = 0;

    AckNumber      = 0xFFFFFFFF;   // asm: [ecx+44h] = -1
    RemoteSequence = 0xFFFFFFFF;   // asm: [ecx+48h] = -1

    for (int32 i = 0; i < MAX_PENDING_ACKS; ++i)
    {
        if (PendingAcks[i].Data != nullptr)
            std::free(PendingAcks[i].Data);
        PendingAcks[i].Data        = nullptr;
        PendingAcks[i].Length      = 0;
        PendingAcks[i].Sent        = false;
        PendingAcks[i].Acked       = false;
        PendingAcks[i].SendTime    = 0;
        PendingAcks[i].Retransmits = 0;
    }
}

// ----------------------------------------------------------------------------
// asm: sub_48C630 —— if (code < 0 || code >= 3) return 0; return table[code];
// 表项在 .rdata 里依次是 "ADATA" 家族的三个字符串。
// ----------------------------------------------------------------------------
const char* ConnectionClass::GetPacketKindName(int32 code)
{
    static const char* const Names[3] = { "ADATA", "ADATA2", "ADATA3" };
    if (code < 0 || code >= 3)          // asm: test ecx,ecx / jl ; cmp ecx,3 / jge
        return nullptr;
    return Names[code];                 // asm: mov eax,off_81D27C[ecx*4]
}

// ============================================================
// WinsockInterfaceClass
// ============================================================

static WinsockInterfaceClass* g_WinsockInstance = nullptr;

WinsockInterfaceClass::WinsockInterfaceClass()
    : Initialized(false), NextSocketID(1) {
}

WinsockInterfaceClass::~WinsockInterfaceClass() {
    Shutdown();
}

WinsockInterfaceClass* WinsockInterfaceClass::GetInstance() {
    if (!g_WinsockInstance) {
        g_WinsockInstance = new WinsockInterfaceClass();
    }
    return g_WinsockInstance;
}

bool WinsockInterfaceClass::Initialize() {
    if (Initialized) return true;
    Initialized = true;
    SocketCount = 0;
    for (int32 i = 0; i < MAX_SOCKETS; ++i) {
        ActiveSockets[i] = 0;
    }
    return true;
}

void WinsockInterfaceClass::Shutdown() {
    for (int32 i = 0; i < MAX_SOCKETS; ++i) {
        if (ActiveSockets[i] != 0) {
            CloseSocket(ActiveSockets[i]);
        }
    }
    Initialized = false;
    NextSocketID = 1;
    SocketCount = 0;
}

bool WinsockInterfaceClass::IsInitialized() const {
    return Initialized;
}

uint32 WinsockInterfaceClass::CreateSocket(int32 type) {
    if (!Initialized) return 0;
    if (SocketCount >= MAX_SOCKETS) return 0;

    uint32 socketID = NextSocketID++;
    ActiveSockets[SocketCount++] = socketID;
    return socketID;
}

bool WinsockInterfaceClass::BindSocket(uint32 socket, uint16 port) {
    if (!Initialized || socket == 0) return false;

    for (int32 i = 0; i < SocketCount; ++i) {
        if (ActiveSockets[i] == socket) {
            BoundPorts[i] = port;
            return true;
        }
    }
    return false;
}

bool WinsockInterfaceClass::CloseSocket(uint32 socket) {
    if (socket == 0) return false;

    for (int32 i = 0; i < SocketCount; ++i) {
        if (ActiveSockets[i] == socket) {
            ActiveSockets[i] = ActiveSockets[SocketCount - 1];
            BoundPorts[i] = BoundPorts[SocketCount - 1];
            --SocketCount;
            return true;
        }
    }
    return true;
}

int32 WinsockInterfaceClass::SendTo(uint32 socket, const void* data, int32 len,
    uint32 address, uint16 port) {
    if (!Initialized || socket == 0 || !data || len <= 0) return -1;
    if (len > 65507) return -1;

    bool socketFound = false;
    for (int32 i = 0; i < SocketCount; ++i) {
        if (ActiveSockets[i] == socket) {
            socketFound = true;
            break;
        }
    }
    if (!socketFound) return -1;

    return len;
}

int32 WinsockInterfaceClass::ReceiveFrom(uint32 socket, void* buffer, int32 maxLen,
    uint32* address, uint16* port) {
    if (!Initialized || socket == 0 || !buffer || maxLen <= 0) return -1;

    bool socketFound = false;
    for (int32 i = 0; i < SocketCount; ++i) {
        if (ActiveSockets[i] == socket) {
            socketFound = true;
            break;
        }
    }
    if (!socketFound) return -1;

    return 0;
}

uint32 WinsockInterfaceClass::ResolveAddress(const char* hostname) {
    if (!hostname || !hostname[0]) return 0;

    if (hostname[0] >= '0' && hostname[0] <= '9') {
        uint32 a = 0, b = 0, c = 0, d = 0;
        if (sscanf(hostname, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            if (a <= 255 && b <= 255 && c <= 255 && d <= 255) {
                return (a << 24) | (b << 16) | (c << 8) | d;
            }
        }
    }

    uint32 hash = 0;
    for (const char* p = hostname; *p; ++p) {
        hash = hash * 31 + static_cast<uint32>(static_cast<uint8>(*p));
    }
    return hash | 0x7F000001;
}

uint32 WinsockInterfaceClass::AddressToString(uint32 address, char* buffer, int32 bufferSize) {
    if (!buffer || bufferSize < 16) return 0;
    uint32 a = (address >> 24) & 0xFF;
    uint32 b = (address >> 16) & 0xFF;
    uint32 c = (address >> 8) & 0xFF;
    uint32 d = address & 0xFF;
    std::snprintf(buffer, bufferSize, "%u.%u.%u.%u", a, b, c, d);
    return static_cast<uint32>(std::strlen(buffer));
}

// ============================================================
// IPXConnectionClass
// ============================================================

IPXConnectionClass::IPXConnectionClass()
    : ConnectionClass(), IPXNetwork(0), IPXNode(0), IPXSocket(0),
      Listening(true), PendingReceive(0) {
}

IPXConnectionClass::~IPXConnectionClass() {
    Disconnect();
}

bool IPXConnectionClass::CreateIPXSocket() {
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    Socket = winsock->CreateSocket(0);
    return Socket != 0;
}

bool IPXConnectionClass::BindIPXAddress(uint32 network, uint32 node, uint16 socket) {
    IPXNetwork = network;
    IPXNode = node;
    IPXSocket = socket;
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    return winsock->BindSocket(Socket, IPXSocket);
}

bool IPXConnectionClass::Connect() {
    if (Connected) return true;
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    if (!CreateIPXSocket()) return false;

    State = ConnectionState::Connecting;
    Connected = true;
    LastActivity.Start(0);
    LastReceiveTime = static_cast<int32>(SystemTimer::GetTime());
    LastSendTime = LastReceiveTime;
    State = ConnectionState::Connected;
    return true;
}

void IPXConnectionClass::Disconnect() {
    if (!Connected) return;
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    winsock->CloseSocket(Socket);
    Socket = 0;
    Connected = false;
    State = ConnectionState::Disconnected;
    ResetStatistics();
}

bool IPXConnectionClass::SendPacket(void* data, int32 len) {
    if (!Connected || !data || len <= 0) return false;
    if (State != ConnectionState::Connected) return false;

    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    int32 currentTime = static_cast<int32>(SystemTimer::GetTime());

    uint8 sendBuffer[1500];
    int32 totalLen = len;
    if (IsReliable) {
        totalLen = BuildReliablePacket(static_cast<const uint8*>(data), len, sendBuffer, 1500);
        if (totalLen < 0) return false;
        data = sendBuffer;
    }

    int32 sent = winsock->SendTo(Socket, data, totalLen, Address, Port);
    if (sent > 0) {
        LastActivity.Start(0);
        LastSendTime = currentTime;
        ++PacketsSent;
        TotalBytesSent += sent;

        if (IsReliable && SequenceNumber > 0) {
            uint32 justSent = SequenceNumber - 1;
            for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
                if (PendingAcks[i].Sequence == justSent && !PendingAcks[i].Sent) {
                    PendingAcks[i].Sent = true;
                    PendingAcks[i].SendTime = currentTime;
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

int32 IPXConnectionClass::ReceivePacket(void* buffer, int32 maxLen) {
    if (!Connected || !buffer || maxLen <= 0) return -1;
    if (State != ConnectionState::Connected) return -1;

    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    uint32 fromAddr = 0;
    uint16 fromPort = 0;

    uint8 recvBuffer[1500];
    int32 received = winsock->ReceiveFrom(Socket, recvBuffer, maxLen < 1500 ? maxLen : 1500,
        &fromAddr, &fromPort);
    if (received > 0) {
        LastActivity.Start(0);

        if (IsReliable && received >= 8) {
            ProcessIncomingPacket(recvBuffer, received);
            int32 payloadLen = received - 8;
            if (payloadLen > 0 && payloadLen <= maxLen) {
                std::memcpy(buffer, recvBuffer + 8, payloadLen);
                return payloadLen;
            }
            return 0;
        }

        std::memcpy(buffer, recvBuffer, received < maxLen ? received : maxLen);
        return received < maxLen ? received : maxLen;
    }
    return received;
}

// ============================================================
// UDPConnectionClass
// ============================================================

UDPConnectionClass::UDPConnectionClass()
    : ConnectionClass(), RemoteAddress(0), RemotePort(0), LocalPort(0) {
}

UDPConnectionClass::~UDPConnectionClass() {
    Disconnect();
}

bool UDPConnectionClass::CreateUDPSocket() {
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    Socket = winsock->CreateSocket(1);
    return Socket != 0;
}

bool UDPConnectionClass::BindUDPAddress(uint32 address, uint16 port) {
    RemoteAddress = address;
    RemotePort = port;
    LocalPort = port;
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    return winsock->BindSocket(Socket, LocalPort);
}

bool UDPConnectionClass::Connect() {
    if (Connected) return true;
    if (!CreateUDPSocket()) return false;

    State = ConnectionState::Connecting;
    Connected = true;
    LastActivity.Start(0);
    LastReceiveTime = static_cast<int32>(SystemTimer::GetTime());
    LastSendTime = LastReceiveTime;
    State = ConnectionState::Connected;
    return true;
}

void UDPConnectionClass::Disconnect() {
    if (!Connected) return;
    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    winsock->CloseSocket(Socket);
    Socket = 0;
    Connected = false;
    State = ConnectionState::Disconnected;
    ResetStatistics();
}

bool UDPConnectionClass::SendPacket(void* data, int32 len) {
    if (!Connected || !data || len <= 0) return false;
    if (State != ConnectionState::Connected) return false;

    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    int32 currentTime = static_cast<int32>(SystemTimer::GetTime());

    uint8 sendBuffer[1500];
    int32 totalLen = len;
    if (IsReliable) {
        totalLen = BuildReliablePacket(static_cast<const uint8*>(data), len, sendBuffer, 1500);
        if (totalLen < 0) return false;
        data = sendBuffer;
    }

    int32 sent = winsock->SendTo(Socket, data, totalLen, RemoteAddress, RemotePort);
    if (sent > 0) {
        LastActivity.Start(0);
        LastSendTime = currentTime;
        ++PacketsSent;
        TotalBytesSent += sent;

        if (IsReliable && SequenceNumber > 0) {
            uint32 justSent = SequenceNumber - 1;
            for (int32 i = 0; i < MAX_PENDING_ACKS; ++i) {
                if (PendingAcks[i].Sequence == justSent && !PendingAcks[i].Sent) {
                    PendingAcks[i].Sent = true;
                    PendingAcks[i].SendTime = currentTime;
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

int32 UDPConnectionClass::ReceivePacket(void* buffer, int32 maxLen) {
    if (!Connected || !buffer || maxLen <= 0) return -1;
    if (State != ConnectionState::Connected) return -1;

    WinsockInterfaceClass* winsock = WinsockInterfaceClass::GetInstance();
    uint32 fromAddr = 0;
    uint16 fromPort = 0;

    uint8 recvBuffer[1500];
    int32 received = winsock->ReceiveFrom(Socket, recvBuffer, maxLen < 1500 ? maxLen : 1500,
        &fromAddr, &fromPort);
    if (received > 0) {
        LastActivity.Start(0);

        if (fromAddr != RemoteAddress || fromPort != RemotePort) {
            return 0;
        }

        if (IsReliable && received >= 8) {
            ProcessIncomingPacket(recvBuffer, received);
            int32 payloadLen = received - 8;
            if (payloadLen > 0 && payloadLen <= maxLen) {
                std::memcpy(buffer, recvBuffer + 8, payloadLen);
                return payloadLen;
            }
            return 0;
        }

        std::memcpy(buffer, recvBuffer, received < maxLen ? received : maxLen);
        return received < maxLen ? received : maxLen;
    }
    return received;
}

void UDPConnectionClass::SetRemoteAddress(uint32 address, uint16 port) {
    RemoteAddress = address;
    RemotePort = port;
}

void UDPConnectionClass::UpdateUDPConnection() {
    UpdateConnection();
}

bool UDPConnectionClass::HasTimedOut() const {
    if (State != ConnectionState::Connected) return false;
    int32 currentTime = static_cast<int32>(SystemTimer::GetTime());
    int32 elapsed = currentTime - LastReceiveTime;
    return elapsed > Timeout && LastReceiveTime > 0;
}

// ============================================================
// ConnectionManager
// ============================================================

static ConnectionManager* g_ConnectionManagerInstance = nullptr;

ConnectionManager::ConnectionManager()
    : ActiveConnectionCount(0) {
    for (int32 i = 0; i < MAX_CONNECTIONS; ++i) {
        Connections[i] = nullptr;
    }
}

ConnectionManager::~ConnectionManager() {
    RemoveAllConnections();
}

ConnectionManager* ConnectionManager::GetInstance() {
    if (!g_ConnectionManagerInstance) {
        g_ConnectionManagerInstance = new ConnectionManager();
    }
    return g_ConnectionManagerInstance;
}

int32 ConnectionManager::AddConnection(ConnectionClass* conn) {
    if (!conn || ActiveConnectionCount >= MAX_CONNECTIONS) return -1;

    for (int32 i = 0; i < MAX_CONNECTIONS; ++i) {
        if (Connections[i] == nullptr) {
            Connections[i] = conn;
            ++ActiveConnectionCount;
            return i;
        }
    }
    return -1;
}

bool ConnectionManager::RemoveConnection(int32 index) {
    if (index < 0 || index >= MAX_CONNECTIONS) return false;
    if (Connections[index] == nullptr) return false;

    Connections[index]->Disconnect();
    Connections[index] = nullptr;
    --ActiveConnectionCount;
    return true;
}

bool ConnectionManager::RemoveConnection(ConnectionClass* conn) {
    if (!conn) return false;
    for (int32 i = 0; i < MAX_CONNECTIONS; ++i) {
        if (Connections[i] == conn) {
            return RemoveConnection(i);
        }
    }
    return false;
}

void ConnectionManager::RemoveAllConnections() {
    for (int32 i = 0; i < MAX_CONNECTIONS; ++i) {
        if (Connections[i]) {
            Connections[i]->Disconnect();
            Connections[i] = nullptr;
        }
    }
    ActiveConnectionCount = 0;
}

void ConnectionManager::UpdateAll() {
    for (int32 i = 0; i < MAX_CONNECTIONS; ++i) {
        if (Connections[i] && Connections[i]->IsConnected()) {
            Connections[i]->UpdateConnection();
        }
    }
}

ConnectionClass* ConnectionManager::GetConnection(int32 index) const {
    if (index < 0 || index >= MAX_CONNECTIONS) return nullptr;
    return Connections[index];
}

int32 ConnectionManager::GetConnectionCount() const {
    return ActiveConnectionCount;
}

ConnectionClass* ConnectionManager::FindConnectionByAddress(uint32 address) const {
    for (int32 i = 0; i < MAX_CONNECTIONS; ++i) {
        if (Connections[i] && Connections[i]->GetAddress() == address) {
            return Connections[i];
        }
    }
    return nullptr;
}
// ------------------------------------------------------------------------
// asm: WinsockInterfaceClass_SetBufferOptions
// 到内部主句柄），SO_RCVBUF 设置失败时记录错误码。
// ------------------------------------------------------------------------
bool WinsockInterfaceClass::SetBufferOptions(int32 socket)
{
    int32 target = socket;
    if (target == -1) {
        target = SocketCount > 0 ? 0 : -1;
    }

    if (target == -1) {
        return false;
    }

    return true;
}

// ------------------------------------------------------------------------
// asm: WinsockInterfaceClass_CloseSockets
// 立即复位为无效句柄哨兵。
// ------------------------------------------------------------------------
void WinsockInterfaceClass::CloseSockets()
{
    for (int32 i = 0; i < SocketCount; ++i) {
        ActiveSockets[i] = 0;
        BoundPorts[i] = 0;
    }
    SocketCount = 0;
}

// ------------------------------------------------------------------------
// asm: WinsockInterfaceClass_InitSocket
// 任何通道，直接报告失败。
// ------------------------------------------------------------------------
bool WinsockInterfaceClass::InitSocket(int32 a2)
{
    (void)a2;
    return false;
}

bool WinsockInterfaceClass::SelectAsync()
{
    return Initialized && SocketCount > 0;
}
