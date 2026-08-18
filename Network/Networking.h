#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/VectorClass.h"
#include "ConnectionClass.h"

static constexpr int32 MAX_NET_PLAYERS = 8;
static constexpr int32 MAX_CHAT_MESSAGES = 256;
static constexpr int32 MAX_EVENTS = 2048;

struct ChatMessage {
    int32 SenderID;
    char Message[128];
    int32 Frame;
    bool Delivered;
};

class NetworkingClass {
public:
    NetworkingClass();
    ~NetworkingClass();

    static NetworkingClass* GetInstance();

    void Init();
    void Shutdown();
    void Update();

    // Event management
    bool AddEvent(NetworkEventType type, int32 playerID, int32 v1, int32 v2, int32 v3, int32 v4);
    void RespondToEvent(const NetworkEvent& evt);
    void FillEvent(NetworkEvent& evt, NetworkEventType type, int32 playerID, int32 v1, int32 v2, int32 v3, int32 v4);
    void ProcessEvents();
    void ClearEvents();
    void ExecuteEvent(const NetworkEvent& evt);

    // Serialization
    bool SerializeEvent(const NetworkEvent& evt, PacketData& pkt);
    bool DeserializeEvent(NetworkEvent& evt, PacketData& pkt);
    uint32 CalculateEventCRC(const NetworkEvent& evt);

    // Frame synchronization
    void SetFrameLock(int32 frame);
    int32 GetFrameLock() const;
    void SetLatencyCompensation(int32 ms);
    int32 GetLatencyCompensation() const;
    bool IsSynced() const;
    void ForceSync();

    // Network send/receive
    void SendEvent(const NetworkEvent& evt);
    void BroadcastEvent(const NetworkEvent& evt);
    void SendEventToPlayer(const NetworkEvent& evt, int32 playerID);

    // Connection management
    void RegisterConnection(ConnectionClass* conn);
    void UnregisterConnection(ConnectionClass* conn);

    // Frame sync
    void ProcessSync();
    void SendSyncFrame();
    void AdvanceFrame();
    uint32 CalculateGameStateCRC();
    void RetransmitPendingEvents();

    // Player management
    void RegisterPlayer(int32 playerID, int32 houseType);
    void UnregisterPlayer(int32 playerID);
    void SetPlayerFrame(int32 playerID, int32 frame);
    void SetPlayerLatency(int32 playerID, int32 ms);
    class HouseClass* GetPlayerHouse(int32 playerID);
    int32 GetPlayerIDFromHouse(int32 houseIndex);

    // Chat
    void SendChatMessage(int32 senderID, const char* message);
    void ReceiveChatMessage(int32 senderID, int32 messageIndex);
    const ChatMessage* GetChatMessage(int32 index) const;
    void ConnectEventToEvent(NetworkEvent& evt, NetworkEventType type, int32 playerID, int32 v1, int32 v2, int32 v3, int32 v4);

    // Game lifecycle
    void StartGame();
    void EndGame();
    void SetHost(bool host);
    bool IsGameRunning() const;
    int32 GetMaxAheadFrames() const;
    void SetMaxAheadFrames(int32 frames);

    // Sync stats
    int32 GetCRCErrors() const;
    void ResetCRCErrors();
    bool IsDesyncDetected() const;

    // Event handlers
    void HandlePlaceEvent(int32 playerID, int32 objectType, int32 cellX, int32 cellY, int32 facing);
    void HandleAnimationEvent(int32 playerID, int32 animType, int32 cellX, int32 cellY);
    void HandleWaypointsEvent(int32 playerID, int32 unitID, int32 waypointX, int32 waypointY);
    void HandleSWPlaceEvent(int32 playerID, int32 swType, int32 cellX, int32 cellY, int32 targetID);
    void HandleProduceEvent(int32 playerID, int32 factoryID, int32 objectType, int32 count);
    void HandleAbandonEvent(int32 playerID, int32 unitID, int32 reason);
    void HandleSuspendEvent(int32 playerID, int32 factoryID, int32 unitID);
    void HandleSellEvent(int32 playerID, int32 buildingID, int32 cellX);
    void HandleRepairEvent(int32 playerID, int32 buildingID, int32 amount);
    void HandlePowerEvent(int32 playerID, int32 buildingID, int32 powerDelta);
    void HandleChronoEvent(int32 playerID, int32 unitID, int32 destX, int32 destY, int32 destZ);
    void HandleIronCurtainEvent(int32 playerID, int32 unitID, int32 duration, int32 cellX);
    void HandleSuperWeaponEvent(int32 playerID, int32 swType, int32 targetX, int32 targetY);
    void HandleSpeechEvent(int32 playerID, int32 speechType, int32 volume);
    void HandleRadarEvent(int32 playerID, int32 eventType, int32 cellX, int32 cellY);
    void HandleSpyEvent(int32 playerID, int32 spyID, int32 targetID, int32 action);
    void HandleGarrisonEvent(int32 playerID, int32 buildingID, int32 infantryType, int32 count);
    void HandleFireEvent(int32 playerID, int32 attackerID, int32 targetID, int32 weaponSlot);
    void HandleDetonateEvent(int32 playerID, int32 objectID, int32 damage);
    void HandleDamageEvent(int32 playerID, int32 targetID, int32 damage, int32 warhead);
    void HandleDestroyEvent(int32 playerID, int32 objectID, int32 killerID);
    void HandleTogglePowerEvent(int32 playerID, int32 buildingID, int32 state);
    void HandleDeployEvent(int32 playerID, int32 unitID, int32 destX);
    void HandleUndeployEvent(int32 playerID, int32 unitID, int32 param);
    void HandleChronoWarpEvent(int32 playerID, int32 unitID, int32 destX, int32 destY, int32 destZ);
    void HandleDropPodEvent(int32 playerID, int32 infantryType, int32 cellX, int32 cellY);
    void HandleTunnelEvent(int32 playerID, int32 unitID, int32 entranceX, int32 entranceY);
    void HandleEnterEvent(int32 playerID, int32 unitID, int32 buildingID, int32 transportID);
    void HandleExitEvent(int32 playerID, int32 unitID, int32 buildingID, int32 transportID);
    void HandleSellBuildingEvent(int32 playerID, int32 buildingID, int32 cellX);
    void HandleRepairBuildingEvent(int32 playerID, int32 buildingID, int32 amount);
    void HandlePowerToggleEvent(int32 playerID, int32 buildingID, int32 state);

    // Handlers completing the original 46-event dispatch table
    // (jump table off_4C8114 in Networking_RespondToEvent)
    void HandleAllyEvent(int32 playerID, int32 houseID, int32 allyFlag);
    void HandleIdleEvent(int32 playerID);
    void HandleScatterEvent(int32 playerID, int32 objectID);
    void HandleOptionsEvent(int32 playerID, int32 optionsFlags);
    void HandleGameSpeedEvent(int32 playerID, int32 speed);
    void HandlePrimaryEvent(int32 playerID, int32 factoryID);
    void HandleSellCellEvent(int32 playerID, int32 cellX, int32 cellY);
    void HandlePacketTimingEvent(int32 playerID, int32 timing);
    void HandleSaveGameEvent(int32 playerID, int32 saveSlot);
    void HandleArchiveEvent(int32 playerID, int32 archiveSlot);
    void HandleAddPlayerEvent(int32 playerID, int32 side, int32 color);
    void HandleTimingEvent(int32 playerID, int32 frame);
    void HandleProcessTimeEvent(int32 playerID, int32 processTime);
    void HandlePageUserEvent(int32 playerID, int32 pageIndex);
    void HandleRemovePlayerEvent(int32 playerID, int32 reason);
    void HandleLatencyFudgeEvent(int32 playerID, int32 fudge);
    void HandleAboutToExitEvent(int32 playerID);
    void HandleFallbackHostEvent(int32 playerID, int32 newHostID);
    void HandleAddressChangeEvent(int32 playerID, int32 newAddress);
    void HandlePlanNodeDeleteEvent(int32 playerID, int32 nodeID);
    void HandleAllCheerEvent(int32 playerID);
    void HandleAbandonAllEvent(int32 playerID);

    int32 FrameLock;
    int32 LatencyCompensation;
    int32 CurrentFrame;
    bool IsNetworkGame;
    bool IsHost;
    bool IsSynced_;
    int32 MaxPlayers;
    int32 ConnectedPlayers;
    bool GameRunning;
    int32 LobbyFrame;
    int32 SyncFrame;
    int32 SyncInterval;

    int32 PlayerFrames[MAX_NET_PLAYERS];
    int32 PlayerLatencies[MAX_NET_PLAYERS];
    bool PlayerReady[MAX_NET_PLAYERS];
    int32 PlayerHouses[MAX_NET_PLAYERS];

    VectorClass<NetworkEvent> EventQueue;
    VectorClass<NetworkEvent> SyncQueue;
    VectorClass<ConnectionClass*> Connections;

    ChatMessage ChatMessages[MAX_CHAT_MESSAGES];
    int32 ChatMessageCount;

    int32 CRCErrors;
    int32 MaxCRCErrors;
    bool DesyncDetected;

    int32 MaxAheadFrames;
    int32 FrameBufferSize;
    int32 ReceivedFrameCount;
    int32 SentFrameCount;
    int32 AckedFrameCount;
};

// NetworkEventQueueClass - renamed from EventClass to avoid collision with
// the single-player mission Game/EventClass.h game-event system.
class NetworkEventQueueClass {
public:
    NetworkEventQueueClass();
    ~NetworkEventQueueClass();

    void AddEvent(NetworkEventType type, int32 v1, int32 v2, int32 v3, int32 v4);
    void Process();
    void Clear();
    NetworkEvent* GetEvent(int32 index);
    int32 GetEventCount() const;

    VectorClass<NetworkEvent> Events;
    int32 EventCount;
};

// Global instance accessor
inline NetworkingClass* Networking() {
    return NetworkingClass::GetInstance();
}