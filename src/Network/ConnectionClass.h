#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Containers/ListClass.h"
#include "../Math/Timer.h"

struct PacketData {
    uint8* Data;
    int32 Length;
    int32 MaxLength;
    int32 Position;

    PacketData() : Data(nullptr), Length(0), MaxLength(0), Position(0) {}
    ~PacketData() {
        if (Data) {
            std::free(Data);
            Data = nullptr;
        }
    }

    bool Allocate(int32 size) {
        Data = static_cast<uint8*>(std::malloc(size));
        if (!Data) return false;
        MaxLength = size;
        Length = 0;
        Position = 0;
        return true;
    }

    void WriteByte(uint8 val) {
        if (Position < MaxLength) {
            Data[Position++] = val;
            if (Position > Length) Length = Position;
        }
    }

    void WriteInt32(int32 val) {
        if (Position + 4 <= MaxLength) {
            Data[Position++] = static_cast<uint8>(val & 0xFF);
            Data[Position++] = static_cast<uint8>((val >> 8) & 0xFF);
            Data[Position++] = static_cast<uint8>((val >> 16) & 0xFF);
            Data[Position++] = static_cast<uint8>((val >> 24) & 0xFF);
            if (Position > Length) Length = Position;
        }
    }

    void WriteInt16(int16 val) {
        if (Position + 2 <= MaxLength) {
            Data[Position++] = static_cast<uint8>(val & 0xFF);
            Data[Position++] = static_cast<uint8>((val >> 8) & 0xFF);
            if (Position > Length) Length = Position;
        }
    }

    uint8 ReadByte() {
        if (Position < Length) return Data[Position++];
        return 0;
    }

    int32 ReadInt32() {
        if (Position + 4 <= Length) {
            int32 val = Data[Position] | (Data[Position + 1] << 8) |
                       (Data[Position + 2] << 16) | (Data[Position + 3] << 24);
            Position += 4;
            return val;
        }
        return 0;
    }

    int16 ReadInt16() {
        if (Position + 2 <= Length) {
            int16 val = static_cast<int16>(Data[Position] | (Data[Position + 1] << 8));
            Position += 2;
            return val;
        }
        return 0;
    }

    void Reset() {
        Position = 0;
    }

    void Clear() {
        Length = 0;
        Position = 0;
    }
};

enum class ConnectionState : int32 {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    TimedOut = 3,
    Error = 4
};

static constexpr int32 MAX_PENDING_ACKS = 256;
static constexpr int32 MAX_SOCKETS = 32;
static constexpr int32 MAX_CONNECTIONS = 16;

struct PendingAck {
    uint32 Sequence;
    uint8* Data;
    int32 Length;
    bool Sent;
    int32 SendTime;
    int32 Retransmits;
    bool Acked;
};

class ConnectionClass {
public:
    ConnectionClass();
    virtual ~ConnectionClass();

    virtual bool SendPacket(void* data, int32 len) = 0;
    virtual int32 ReceivePacket(void* buffer, int32 maxLen) = 0;
    virtual bool Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const;
    virtual uint32 GetAddress() const;

    void SetTimeout(int32 timeoutMs);
    int32 GetTimeout() const;

    void QueuePacket(const uint8* data, int32 len);
    bool ProcessQueue();

    // Reliable transmission
    int32 BuildReliablePacket(const uint8* data, int32 len, uint8* outBuffer, int32 outMaxLen);
    bool ProcessIncomingPacket(const uint8* data, int32 len);
    void UpdateConnection();
    void SendKeepAlive();
    void BuildRetransmitPacket(uint32 seq, const uint8* data, int32 len, uint8* outBuffer, int32 outMaxLen);

    // Statistics
    int32 GetLatency() const;
    int32 GetAverageLatency() const;
    float GetPacketLoss() const;
    ConnectionState GetState() const;
    void SetState(ConnectionState newState);
    void SetReliable(bool reliable);
    void SetMaxPacketSize(int32 maxSize);
    void ResetStatistics();
    static uint32 CalculateCRC(const uint8* data, int32 len);

    uint32 Socket;
    uint32 Address;
    uint16 Port;
    bool Connected;
    int32 Timeout;
    VectorClass<PacketData*> PacketQueue;
    CDTimerClass LastActivity;

    // Sequence tracking
    uint32 SequenceNumber;
    uint32 AckNumber;
    uint32 RemoteSequence;

    // Latency and packet loss
    int32 Latency;
    int32 LatencyAverage;
    int32 LatencyDeviation;
    float PacketLoss;
    int32 PacketsSent;
    int32 PacketsReceived;
    int32 PacketsLost;
    int32 TotalBytesSent;
    int32 TotalBytesReceived;

    // Connection state
    ConnectionState State;
    int32 RetransmitCount;
    int32 MaxRetransmits;
    int32 RetransmitTimeout;
    int32 KeepAliveInterval;
    int32 LastSendTime;
    int32 LastReceiveTime;

    // Transmission control
    bool IsReliable;
    bool IsEncrypted;
    int32 MaxPacketSize;
    PendingAck PendingAcks[MAX_PENDING_ACKS];
    uint8 FragmentedPackets[256];
};

class WinsockInterfaceClass {
public:
    WinsockInterfaceClass();
    ~WinsockInterfaceClass();

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    uint32 CreateSocket(int32 type);
    bool BindSocket(uint32 socket, uint16 port);
    bool CloseSocket(uint32 socket);

    int32 SendTo(uint32 socket, const void* data, int32 len, uint32 address, uint16 port);
    int32 ReceiveFrom(uint32 socket, void* buffer, int32 maxLen, uint32* address, uint16* port);

    uint32 ResolveAddress(const char* hostname);
    static uint32 AddressToString(uint32 address, char* buffer, int32 bufferSize);

    static WinsockInterfaceClass* GetInstance();

    bool Initialized;
    uint32 NextSocketID;
    uint32 ActiveSockets[MAX_SOCKETS];
    uint16 BoundPorts[MAX_SOCKETS];
    int32 SocketCount;
};

class IPXConnectionClass : public ConnectionClass {
public:
    IPXConnectionClass();
    virtual ~IPXConnectionClass();

    virtual bool SendPacket(void* data, int32 len) override;
    virtual int32 ReceivePacket(void* buffer, int32 maxLen) override;
    virtual bool Connect() override;
    virtual void Disconnect() override;

    bool CreateIPXSocket();
    bool BindIPXAddress(uint32 network, uint32 node, uint16 socket);

    uint32 IPXNetwork;
    uint32 IPXNode;
    uint16 IPXSocket;
};

class UDPConnectionClass : public ConnectionClass {
public:
    UDPConnectionClass();
    virtual ~UDPConnectionClass();

    virtual bool SendPacket(void* data, int32 len) override;
    virtual int32 ReceivePacket(void* buffer, int32 maxLen) override;
    virtual bool Connect() override;
    virtual void Disconnect() override;

    bool CreateUDPSocket();
    bool BindUDPAddress(uint32 address, uint16 port);
    void SetRemoteAddress(uint32 address, uint16 port);
    void UpdateUDPConnection();
    bool HasTimedOut() const;

    uint32 RemoteAddress;
    uint16 RemotePort;
    uint16 LocalPort;
};

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    static ConnectionManager* GetInstance();

    int32 AddConnection(ConnectionClass* conn);
    bool RemoveConnection(int32 index);
    bool RemoveConnection(ConnectionClass* conn);
    void RemoveAllConnections();
    void UpdateAll();

    ConnectionClass* GetConnection(int32 index) const;
    int32 GetConnectionCount() const;
    ConnectionClass* FindConnectionByAddress(uint32 address) const;

private:
    ConnectionClass* Connections[MAX_CONNECTIONS];
    int32 ActiveConnectionCount;
};