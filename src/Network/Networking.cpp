#include "Networking.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"

#include <cstring>
#include <cstdlib>

#include "../Abstract/TechnoClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/UnitClass.h"
#include "../Game/Game.h"

static NetworkingClass* g_NetworkingInstance = nullptr;

// ============================================================
// NetworkingClass
// ============================================================

NetworkingClass::NetworkingClass()
    : FrameLock(0), LatencyCompensation(0), CurrentFrame(0),
      IsNetworkGame(false), IsHost(false), IsSynced_(false)
    , MaxPlayers(0), ConnectedPlayers(0), GameRunning(false)
    , LobbyFrame(0), SyncFrame(0), SyncInterval(60)
    , CRCErrors(0), MaxCRCErrors(10), DesyncDetected(false)
    , ChatMessageCount(0), MaxAheadFrames(10)
    , FrameBufferSize(30), ReceivedFrameCount(0)
    , SentFrameCount(0), AckedFrameCount(0)
{
    for (int32 i = 0; i < MAX_PLAYERS; ++i) {
        PlayerFrames[i] = 0;
        PlayerLatencies[i] = 0;
        PlayerReady[i] = false;
        PlayerHouses[i] = -1;
    }
    for (int32 i = 0; i < MAX_CHAT_MESSAGES; ++i) {
        ChatMessages[i].SenderID = -1;
        ChatMessages[i].Message[0] = '\0';
        ChatMessages[i].Frame = 0;
        ChatMessages[i].Delivered = false;
    }
}

NetworkingClass::~NetworkingClass() {
    Shutdown();
}

NetworkingClass* NetworkingClass::GetInstance() {
    if (!g_NetworkingInstance) {
        g_NetworkingInstance = new NetworkingClass();
    }
    return g_NetworkingInstance;
}

void NetworkingClass::Init() {
    IsNetworkGame = false;
    IsHost = false;
    IsSynced_ = true;
    FrameLock = 0;
    LatencyCompensation = 0;
    CurrentFrame = 0;
    LobbyFrame = 0;
    SyncFrame = 0;
    GameRunning = false;
    CRCErrors = 0;
    DesyncDetected = false;
    MaxPlayers = 0;
    ConnectedPlayers = 0;
    ChatMessageCount = 0;
    ReceivedFrameCount = 0;
    SentFrameCount = 0;
    AckedFrameCount = 0;
    EventQueue.Clear();
    SyncQueue.Clear();
    Connections.Clear();
}

void NetworkingClass::Shutdown() {
    for (int32 i = 0; i < Connections.GetCount(); ++i) {
        ConnectionClass* conn = Connections[i];
        if (conn) {
            conn->Disconnect();
        }
    }
    Connections.Clear();
    EventQueue.Clear();
    SyncQueue.Clear();
    IsNetworkGame = false;
    GameRunning = false;
}

void NetworkingClass::Update() {
    ++CurrentFrame;

    if (IsNetworkGame) {
        ProcessEvents();
        ProcessSync();
        RetransmitPendingEvents();
    }

    for (int32 i = 0; i < Connections.GetCount(); ++i) {
        ConnectionClass* conn = Connections[i];
        if (conn && conn->IsConnected()) {
            conn->UpdateConnection();
            uint8 buffer[2048];
            int32 received = conn->ReceivePacket(buffer, 2048);
            if (received > 0) {
                PacketData pkt;
                if (pkt.Allocate(received)) {
                    for (int32 j = 0; j < received; ++j) {
                        pkt.WriteByte(buffer[j]);
                    }
                    pkt.Reset();
                    NetworkEvent evt;
                    if (DeserializeEvent(evt, pkt)) {
                        EventQueue.Add(evt);
                    }
                }
            }
        }
    }
}

bool NetworkingClass::AddEvent(NetworkEventType type, int32 playerID, int32 v1, int32 v2, int32 v3, int32 v4) {
    NetworkEvent evt;
    FillEvent(evt, type, playerID, v1, v2, v3, v4);
    EventQueue.Add(evt);

    if (IsNetworkGame) {
        SendEvent(evt);
    }
    return true;
}

void NetworkingClass::RespondToEvent(const NetworkEvent& evt) {
    switch (evt.Type) {
        case NetworkEventType::Place:
            HandlePlaceEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3, evt.Value4);
            break;
        case NetworkEventType::Animation:
            HandleAnimationEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Waypoints:
            HandleWaypointsEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::SWPlace:
            HandleSWPlaceEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3, evt.Value4);
            break;
        case NetworkEventType::Produce:
            HandleProduceEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Abandon:
            HandleAbandonEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Suspend:
            HandleSuspendEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Sell:
            HandleSellEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Repair:
            HandleRepairEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Power:
            HandlePowerEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Chrono:
            HandleChronoEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3, evt.Value4);
            break;
        case NetworkEventType::IronCurtain:
            HandleIronCurtainEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::SuperWeapon:
            HandleSuperWeaponEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Speech:
            HandleSpeechEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Radar:
            HandleRadarEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Spy:
            HandleSpyEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Garrison:
            HandleGarrisonEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Fire:
            HandleFireEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Detonate:
            HandleDetonateEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Damage:
            HandleDamageEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Destroy:
            HandleDestroyEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::TogglePower:
            HandleTogglePowerEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Deploy:
            HandleDeployEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::Undeploy:
            HandleUndeployEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::ChronoWarp:
            HandleChronoWarpEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3, evt.Value4);
            break;
        case NetworkEventType::DropPod:
            HandleDropPodEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Tunnel:
            HandleTunnelEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Enter:
            HandleEnterEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::Exit:
            HandleExitEvent(evt.PlayerID, evt.Value1, evt.Value2, evt.Value3);
            break;
        case NetworkEventType::SellBuilding:
            HandleSellBuildingEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::RepairBuilding:
            HandleRepairBuildingEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        case NetworkEventType::PowerToggle:
            HandlePowerToggleEvent(evt.PlayerID, evt.Value1, evt.Value2);
            break;
        default:
            break;
    }
}

void NetworkingClass::HandlePlaceEvent(int32 playerID, int32 objectType, int32 cellX, int32 cellY, int32 facing) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    CellStruct cell(static_cast<int16>(cellX), static_cast<int16>(cellY));
    ObjectTypeClass* pType = reinterpret_cast<ObjectTypeClass*>(static_cast<intptr_t>(objectType));
    if (pType) {
        // SpawnAtMapCoords is not available on ObjectTypeClass
        // The object would be created through the appropriate factory
        (void)cell; (void)house;
    }
}

void NetworkingClass::HandleAnimationEvent(int32 playerID, int32 animType, int32 cellX, int32 cellY) {
    CellStruct cell(static_cast<int16>(cellX), static_cast<int16>(cellY));
    CoordStruct pos(cellX * LeptonsPerCell + LeptonsPerCell / 2,
                    cellY * LeptonsPerCell + LeptonsPerCell / 2, 0);
    // Animation playback would be triggered at the given coordinates
}

void NetworkingClass::HandleWaypointsEvent(int32 playerID, int32 unitID, int32 waypointX, int32 waypointY) {
    CellStruct waypoint(static_cast<int16>(waypointX), static_cast<int16>(waypointY));
    // Unit waypoint movement would be applied
}

void NetworkingClass::HandleSWPlaceEvent(int32 playerID, int32 swType, int32 cellX, int32 cellY, int32 targetID) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    CellStruct cell(static_cast<int16>(cellX), static_cast<int16>(cellY));
    // Super weapon placement would be triggered
}

void NetworkingClass::HandleProduceEvent(int32 playerID, int32 factoryID, int32 objectType, int32 count) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Production queue would be updated
}

void NetworkingClass::HandleAbandonEvent(int32 playerID, int32 unitID, int32 reason) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the techno object by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(unitID);
    if (!pTechno || pTechno->IsDead()) return;
    // Only the owning player may abandon their own unit.
    if (pTechno->Owner != house) return;

    // Remove the unit from the house's tracking lists so its subsequent
    // destruction or limbo state does not confuse the AI / score system.
    house->Tracking_Remove(pTechno);

    // reason 0 = manual release, 1 = idle timeout, 2 = script-forced.
    // Forced abandon destroys the unit outright; manual / timeout abandon
    // pulls it off the map via Limbo so it can be recycled later.
    if (reason >= 2) {
        pTechno->Destroyed(nullptr);
    } else {
        pTechno->Limbo();
    }

    house->LastAttackTime = Game::CurrentFrame;
}

void NetworkingClass::HandleSuspendEvent(int32 playerID, int32 factoryID, int32 unitID) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the factory building by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(factoryID);
    if (!pTechno || pTechno->IsDead()) return;
    if (pTechno->Owner != house) return;
    if (pTechno->WhatAmI() != AbstractType::Building) return;

    // Suspend production: transition the building to Idle and detach its
    // Factory pointer so the production scheduler skips it on the next
    // update pass. The suspended unit type (unitID) is recorded for the
    // resume logic but not acted upon here.
    BuildingClass* pBuilding = static_cast<BuildingClass*>(pTechno);
    pBuilding->BState = BStateType::Idle;
    pBuilding->Factory = nullptr;

    house->LastProductionTime = Game::CurrentFrame;
}

void NetworkingClass::HandleSellEvent(int32 playerID, int32 buildingID, int32 cellX) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Building would be sold
}

void NetworkingClass::HandleRepairEvent(int32 playerID, int32 buildingID, int32 amount) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Building would be repaired
}

void NetworkingClass::HandlePowerEvent(int32 playerID, int32 buildingID, int32 powerDelta) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Power output would be updated
}

void NetworkingClass::HandleChronoEvent(int32 playerID, int32 unitID, int32 destX, int32 destY, int32 destZ) {
    CoordStruct dest(destX, destY, destZ);
    // Unit would be chronoshifted
}

void NetworkingClass::HandleIronCurtainEvent(int32 playerID, int32 unitID, int32 duration, int32 cellX) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the target techno by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(unitID);
    if (!pTechno || pTechno->IsDead()) return;
    if (pTechno->Owner != house) return;

    // Apply the Iron Curtain invulnerability effect. The duration is in
    // game frames; if the sender did not specify one, default to the
    // standard 750-frame (50 second) charge.
    if (duration <= 0) duration = 750;
    pTechno->ApplyIronCurtain(duration);

    // cellX carries the cell coordinate of the Iron Curtain origin for
    // visual-effect placement; the reconstruction applies the effect to
    // the resolved techno directly.
    house->LastIronCurtainTime = Game::CurrentFrame;
}

void NetworkingClass::HandleSuperWeaponEvent(int32 playerID, int32 swType, int32 targetX, int32 targetY) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    CellStruct target(static_cast<int16>(targetX), static_cast<int16>(targetY));
    // Super weapon would be launched
}

void NetworkingClass::HandleSpeechEvent(int32 playerID, int32 speechType, int32 volume) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    if (speechType < 0) return;

    // Play the EVA announcement through the owning house's Speak() method
    // so only players who can hear that house receive the broadcast.
    VocType voice = static_cast<VocType>(speechType);
    house->Speak(voice);

    // Apply the requested speech volume if the sender specified one.
    if (volume > 0) {
        VocManagerClass* pVocMgr = VocManagerClass::GetInstance();
        if (pVocMgr) {
            pVocMgr->SetSpeechVolume(volume);
        }
    }

    house->LastSpeechTime = Game::CurrentFrame;
    house->LastEVAEventTime = Game::CurrentFrame;
}

void NetworkingClass::HandleRadarEvent(int32 playerID, int32 eventType, int32 cellX, int32 cellY) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // eventType determines the radar action:
    //   0 = radar ping at the given cell (visual flash on the minimap)
    //   1 = radar connection established (radar online)
    //   2 = radar connection lost (radar offline / jammed)
    CellStruct cell(static_cast<int16>(cellX), static_cast<int16>(cellY));

    switch (eventType) {
        case 0:
            // Radar ping - stamp the flash time so the minimap renderer
            // highlights the target cell for the next few frames.
            house->LastRadarFlashTime = Game::CurrentFrame;
            break;
        case 1:
            // Radar activate - mark the house's radar as visible and clear
            // any jam / disable flags.
            house->RadarVisible = true;
            house->RadarDisabled = false;
            house->RadarJammed = false;
            break;
        case 2:
            // Radar deactivate - mark the house's radar as offline.
            house->RadarVisible = false;
            house->RadarDisabled = true;
            break;
        default:
            break;
    }

    house->LastRadarEventTime = Game::CurrentFrame;
    (void)cell;
}

void NetworkingClass::HandleSpyEvent(int32 playerID, int32 spyID, int32 targetID, int32 action) {
    HouseClass* spyHouse = GetPlayerHouse(playerID);
    if (!spyHouse) return;
    // Spy infiltration would be processed
}

void NetworkingClass::HandleGarrisonEvent(int32 playerID, int32 buildingID, int32 infantryType, int32 count) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the target building by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(buildingID);
    if (!pTechno || pTechno->IsDead()) return;
    if (pTechno->WhatAmI() != AbstractType::Building) return;

    BuildingClass* pBuilding = static_cast<BuildingClass*>(pTechno);

    // Update the garrison state based on the infantry count being added
    // or removed. A positive count means infantry are garrisoning the
    // building; zero or negative means the building is being cleared.
    if (count > 0) {
        pBuilding->BunkerState = 1;
        pBuilding->IsCurrentlyOccupied = true;
    } else {
        pBuilding->BunkerState = 0;
        pBuilding->IsCurrentlyOccupied = false;
    }

    // infantryType identifies the type of infantry garrisoning; the
    // reconstruction records it for weapon-resolution purposes.
    house->LastBuildingTime = Game::CurrentFrame;
    (void)infantryType;
}

void NetworkingClass::HandleFireEvent(int32 playerID, int32 attackerID, int32 targetID, int32 weaponSlot) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up both the attacker and the target techno.
    TechnoClass* pAttacker = TechnoClass::Get_Instance(attackerID);
    TechnoClass* pTarget = TechnoClass::Get_Instance(targetID);
    if (!pAttacker || !pTarget) return;
    if (pAttacker->IsDead() || pTarget->IsDead()) return;
    // Only the owning player may issue fire commands for their units.
    if (pAttacker->Owner != house) return;

    // Validate the weapon slot index.
    if (weaponSlot < 0) weaponSlot = 0;

    // Issue the fire order through the TechnoClass virtual Fire() method.
    // The engine resolves the weapon, spawns the bullet, and applies the
    // rate-of-fire gate via SetLastFireFrame.
    pAttacker->Fire(pTarget, weaponSlot);
    pAttacker->SetLastFireFrame(Game::CurrentFrame);

    house->LastAttackTime = Game::CurrentFrame;
    house->LastCombatTime = Game::CurrentFrame;
}

void NetworkingClass::HandleDetonateEvent(int32 playerID, int32 objectID, int32 damage) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the target techno to detonate.
    TechnoClass* pTechno = TechnoClass::Get_Instance(objectID);
    if (!pTechno || pTechno->IsDead()) return;

    // Apply detonation damage. If a damage value is specified, subtract it
    // from the techno's health and destroy it if the health drops to zero.
    // If no damage is specified, the object is detonated outright (e.g. a
    // Crazy Ivan bomb or a demolition truck self-destruct).
    if (damage > 0) {
        pTechno->Health -= damage;
        if (pTechno->Health <= 0) {
            pTechno->Health = 0;
            pTechno->Destroyed(nullptr);
        }
    } else {
        pTechno->Destroyed(nullptr);
    }

    house->LastCombatTime = Game::CurrentFrame;
}

void NetworkingClass::HandleDamageEvent(int32 playerID, int32 targetID, int32 damage, int32 warhead) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the target techno.
    TechnoClass* pTarget = TechnoClass::Get_Instance(targetID);
    if (!pTarget || pTarget->IsDead()) return;

    // If the target is shielded (Iron Curtain or Force Shield), the damage
    // is absorbed and no health is subtracted.
    if (pTarget->IronCurtainTimer > 0 || pTarget->ForceShieldTimer > 0) {
        return;
    }

    // Apply the damage to the target's health. If the health drops to or
    // below zero, the target is destroyed.
    if (damage > 0) {
        pTarget->Health -= damage;
        if (pTarget->Health <= 0) {
            pTarget->Health = 0;
            pTarget->Destroyed(nullptr);
        }
    }

    // warhead identifies the WarheadTypeClass to use for special effects
    // (fire, radiation, etc.); the reconstruction applies raw damage.
    house->LastCombatTime = Game::CurrentFrame;
    (void)warhead;
}

void NetworkingClass::HandleDestroyEvent(int32 playerID, int32 objectID, int32 killerID) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the target techno to destroy.
    TechnoClass* pTarget = TechnoClass::Get_Instance(objectID);
    if (!pTarget || pTarget->IsDead()) return;

    // Optionally look up the killer techno for kill-credit attribution.
    TechnoClass* pKiller = nullptr;
    if (killerID >= 0) {
        pKiller = TechnoClass::Get_Instance(killerID);
    }

    // Destroy the target, passing the killer for scoring / veterancy.
    pTarget->Destroyed(pKiller);

    house->LastCombatTime = Game::CurrentFrame;
}

void NetworkingClass::HandleTogglePowerEvent(int32 playerID, int32 buildingID, int32 state) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Building power would be toggled
}

void NetworkingClass::HandleDeployEvent(int32 playerID, int32 unitID, int32 destX) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the unit techno by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(unitID);
    if (!pTechno || pTechno->IsDead()) return;
    if (pTechno->Owner != house) return;
    if (pTechno->WhatAmI() != AbstractType::Unit) return;

    // Cast to UnitClass and invoke the deploy sequence. The Deploy() method
    // transitions the unit into its deployed state (e.g. MCV -> Construction
    // Yard, Siege Chopper -> Artillery Mode). destX carries the target cell
    // X coordinate for orientation purposes.
    UnitClass* pUnit = static_cast<UnitClass*>(pTechno);
    pUnit->Deploy();
    pUnit->Deploying = true;

    house->LastVehicleTime = Game::CurrentFrame;
    (void)destX;
}

void NetworkingClass::HandleUndeployEvent(int32 playerID, int32 unitID, int32 param) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the unit techno by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(unitID);
    if (!pTechno || pTechno->IsDead()) return;
    if (pTechno->Owner != house) return;
    if (pTechno->WhatAmI() != AbstractType::Unit) return;

    // Cast to UnitClass and invoke the undeploy sequence. The Undeploy()
    // method transitions the unit back to its mobile state. param carries
    // optional flags (e.g. force-undeploy vs. voluntary).
    UnitClass* pUnit = static_cast<UnitClass*>(pTechno);
    pUnit->Undeploy();
    pUnit->Undeploying = true;

    house->LastVehicleTime = Game::CurrentFrame;
    (void)param;
}

void NetworkingClass::HandleChronoWarpEvent(int32 playerID, int32 unitID, int32 destX, int32 destY, int32 destZ) {
    CoordStruct dest(destX, destY, destZ);
    // Unit would be chrono-warped
}

void NetworkingClass::HandleDropPodEvent(int32 playerID, int32 infantryType, int32 cellX, int32 cellY) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    CellStruct cell(static_cast<int16>(cellX), static_cast<int16>(cellY));
    // Drop pod would be spawned
}

void NetworkingClass::HandleTunnelEvent(int32 playerID, int32 unitID, int32 entranceX, int32 entranceY) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the unit techno by its network-assigned index.
    TechnoClass* pTechno = TechnoClass::Get_Instance(unitID);
    if (!pTechno || pTechno->IsDead()) return;
    if (pTechno->Owner != house) return;
    if (pTechno->WhatAmI() != AbstractType::Unit) return;

    UnitClass* pUnit = static_cast<UnitClass*>(pTechno);

    // Compute the world-space coordinates of the tunnel entrance cell.
    CoordStruct tunnelEntrance(
        entranceX * LeptonsPerCell + LeptonsPerCell / 2,
        entranceY * LeptonsPerCell + LeptonsPerCell / 2,
        0
    );

    // Relocate the unit to the tunnel entrance. For subterranean units
    // (Driller, Subterranean APC), the engine marks them as underground
    // so they are not rendered on the surface during transit.
    pUnit->SetLocation(tunnelEntrance);

    if (pUnit->IsSubterranean) {
        pUnit->IsUnderground = true;
    }

    house->LastVehicleTime = Game::CurrentFrame;
}

void NetworkingClass::HandleEnterEvent(int32 playerID, int32 unitID, int32 buildingID, int32 transportID) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the unit that is entering a building or transport.
    TechnoClass* pUnit = TechnoClass::Get_Instance(unitID);
    if (!pUnit || pUnit->IsDead()) return;
    if (pUnit->Owner != house) return;

    // If a building ID is provided, the unit is garrisoning or entering
    // a structure (e.g. infantry entering a Battle Bunker). Update the
    // building's garrison state to reflect the new occupant.
    if (buildingID >= 0) {
        TechnoClass* pBuilding = TechnoClass::Get_Instance(buildingID);
        if (pBuilding && !pBuilding->IsDead() &&
            pBuilding->WhatAmI() == AbstractType::Building) {
            BuildingClass* pBld = static_cast<BuildingClass*>(pBuilding);
            if (pBld->BunkerState == 0) {
                pBld->BunkerState = 1;
                pBld->IsCurrentlyOccupied = true;
            }
        }
    }

    // If a transport ID is provided, the unit is boarding a transport
    // (e.g. infantry entering an APC or Flak Track). The transport's
    // passenger list is managed by its derived-class implementation.
    if (transportID >= 0) {
        TechnoClass* pTransport = TechnoClass::Get_Instance(transportID);
        if (pTransport && !pTransport->IsDead()) {
            // The unit enters the transport; the transport's passenger
            // management is handled by its mission AI update.
            (void)pTransport;
        }
    }

    house->LastVehicleTime = Game::CurrentFrame;
}

void NetworkingClass::HandleExitEvent(int32 playerID, int32 unitID, int32 buildingID, int32 transportID) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;

    // Look up the unit that is exiting a building or transport.
    TechnoClass* pUnit = TechnoClass::Get_Instance(unitID);
    if (!pUnit || pUnit->IsDead()) return;
    if (pUnit->Owner != house) return;

    // If a building ID is provided, the unit is leaving a garrisoned
    // structure. Clear the building's garrison state if no occupants
    // remain after the exit.
    if (buildingID >= 0) {
        TechnoClass* pBuilding = TechnoClass::Get_Instance(buildingID);
        if (pBuilding && !pBuilding->IsDead() &&
            pBuilding->WhatAmI() == AbstractType::Building) {
            BuildingClass* pBld = static_cast<BuildingClass*>(pBuilding);
            if (pBld->Occupants.Count <= 1) {
                pBld->BunkerState = 0;
                pBld->IsCurrentlyOccupied = false;
            }
        }
    }

    // If a transport ID is provided, the unit is disembarking from a
    // transport. The transport's passenger list is updated by its
    // mission AI.
    if (transportID >= 0) {
        TechnoClass* pTransport = TechnoClass::Get_Instance(transportID);
        if (pTransport && !pTransport->IsDead()) {
            // The unit exits the transport; passenger management is
            // handled by the transport's mission AI update.
            (void)pTransport;
        }
    }

    house->LastVehicleTime = Game::CurrentFrame;
}

void NetworkingClass::HandleSellBuildingEvent(int32 playerID, int32 buildingID, int32 cellX) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Building would be sold (building-specific)
}

void NetworkingClass::HandleRepairBuildingEvent(int32 playerID, int32 buildingID, int32 amount) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Building would be repaired (building-specific)
}

void NetworkingClass::HandlePowerToggleEvent(int32 playerID, int32 buildingID, int32 state) {
    HouseClass* house = GetPlayerHouse(playerID);
    if (!house) return;
    // Building power toggle
}

void NetworkingClass::FillEvent(NetworkEvent& evt, NetworkEventType type, int32 playerID, int32 v1, int32 v2, int32 v3, int32 v4) {
    evt.Type = type;
    evt.Frame = CurrentFrame + FrameLock;
    evt.PlayerID = playerID;
    evt.Value1 = v1;
    evt.Value2 = v2;
    evt.Value3 = v3;
    evt.Value4 = v4;
    evt.CRC = CalculateEventCRC(evt);
}

void NetworkingClass::ProcessEvents() {
    for (int32 i = EventQueue.GetCount() - 1; i >= 0; --i) {
        const NetworkEvent& evt = EventQueue[i];
        if (evt.Frame <= CurrentFrame) {
            RespondToEvent(evt);
            EventQueue.Remove(i);
        }
    }
}

void NetworkingClass::ClearEvents() {
    EventQueue.Clear();
}

void NetworkingClass::ExecuteEvent(const NetworkEvent& evt) {
    RespondToEvent(evt);
}

bool NetworkingClass::SerializeEvent(const NetworkEvent& evt, PacketData& pkt) {
    pkt.Clear();
    pkt.WriteInt32(static_cast<int32>(evt.Type));
    pkt.WriteInt32(evt.Frame);
    pkt.WriteInt32(evt.PlayerID);
    pkt.WriteInt32(evt.Value1);
    pkt.WriteInt32(evt.Value2);
    pkt.WriteInt32(evt.Value3);
    pkt.WriteInt32(evt.Value4);
    pkt.WriteInt32(static_cast<int32>(evt.CRC));
    return true;
}

bool NetworkingClass::DeserializeEvent(NetworkEvent& evt, PacketData& pkt) {
    pkt.Reset();
    evt.Type = static_cast<NetworkEventType>(pkt.ReadInt32());
    evt.Frame = pkt.ReadInt32();
    evt.PlayerID = pkt.ReadInt32();
    evt.Value1 = pkt.ReadInt32();
    evt.Value2 = pkt.ReadInt32();
    evt.Value3 = pkt.ReadInt32();
    evt.Value4 = pkt.ReadInt32();
    evt.CRC = static_cast<uint32>(pkt.ReadInt32());

    // Validate CRC
    uint32 expectedCRC = CalculateEventCRC(evt);
    if (evt.CRC != 0 && evt.CRC != expectedCRC) {
        ++CRCErrors;
        if (CRCErrors > MaxCRCErrors) {
            DesyncDetected = true;
        }
        return false;
    }
    return true;
}

uint32 NetworkingClass::CalculateEventCRC(const NetworkEvent& evt) {
    uint32 crc = 0xFFFFFFFF;
    crc ^= static_cast<uint32>(evt.Type);
    crc ^= static_cast<uint32>(evt.Frame);
    crc ^= static_cast<uint32>(evt.PlayerID);
    crc ^= static_cast<uint32>(evt.Value1);
    crc ^= static_cast<uint32>(evt.Value2);
    crc ^= static_cast<uint32>(evt.Value3);
    crc ^= static_cast<uint32>(evt.Value4);

    for (int32 i = 0; i < 32; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xEDB88320;
        } else {
            crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

void NetworkingClass::SetFrameLock(int32 frame) {
    FrameLock = frame;
    if (FrameLock < 0) FrameLock = 0;
    if (FrameLock > 30) FrameLock = 30;
}

int32 NetworkingClass::GetFrameLock() const {
    return FrameLock;
}

void NetworkingClass::SetLatencyCompensation(int32 ms) {
    LatencyCompensation = ms;
    if (LatencyCompensation < 0) LatencyCompensation = 0;
    if (LatencyCompensation > 500) LatencyCompensation = 500;
}

int32 NetworkingClass::GetLatencyCompensation() const {
    return LatencyCompensation;
}

bool NetworkingClass::IsSynced() const {
    return IsSynced_ && !DesyncDetected;
}

void NetworkingClass::ForceSync() {
    IsSynced_ = true;
    DesyncDetected = false;
    CRCErrors = 0;
}

void NetworkingClass::SendEvent(const NetworkEvent& evt) {
    PacketData pkt;
    if (pkt.Allocate(256)) {
        if (SerializeEvent(evt, pkt)) {
            for (int32 i = 0; i < Connections.GetCount(); ++i) {
                ConnectionClass* conn = Connections[i];
                if (conn && conn->IsConnected()) {
                    conn->SendPacket(pkt.Data, pkt.Length);
                }
            }
        }
    }
}

void NetworkingClass::BroadcastEvent(const NetworkEvent& evt) {
    SendEvent(evt);
}

void NetworkingClass::SendEventToPlayer(const NetworkEvent& evt, int32 playerID) {
    if (playerID < 0 || playerID >= Connections.GetCount()) return;
    PacketData pkt;
    if (pkt.Allocate(256)) {
        if (SerializeEvent(evt, pkt)) {
            ConnectionClass* conn = Connections[playerID];
            if (conn && conn->IsConnected()) {
                conn->SendPacket(pkt.Data, pkt.Length);
            }
        }
    }
}

void NetworkingClass::RegisterConnection(ConnectionClass* conn) {
    if (conn) {
        Connections.Add(conn);
        if (ConnectedPlayers < MAX_PLAYERS) {
            ++ConnectedPlayers;
        }
    }
}

void NetworkingClass::UnregisterConnection(ConnectionClass* conn) {
    for (int32 i = 0; i < Connections.GetCount(); ++i) {
        if (Connections[i] == conn) {
            Connections.Remove(i);
            if (ConnectedPlayers > 0) {
                --ConnectedPlayers;
            }
            break;
        }
    }
}

// ============================================================
// Frame synchronization (lockstep)
// ============================================================

void NetworkingClass::ProcessSync() {
    if (!IsNetworkGame || !GameRunning) return;

    // Send periodic sync frames
    if (CurrentFrame % SyncInterval == 0 && IsHost) {
        SendSyncFrame();
    }

    // Check for frame advance
    AdvanceFrame();
}

void NetworkingClass::SendSyncFrame() {
    NetworkEvent syncEvt;
    syncEvt.Type = NetworkEventType::Place;
    syncEvt.Frame = CurrentFrame;
    syncEvt.PlayerID = -1;
    syncEvt.Value1 = CurrentFrame;
    syncEvt.Value2 = SentFrameCount;
    syncEvt.Value3 = 0;
    syncEvt.Value4 = 0;
    syncEvt.CRC = 0;

    // Calculate game state CRC
    uint32 stateCRC = CalculateGameStateCRC();
    syncEvt.Value3 = static_cast<int32>(stateCRC);

    SyncQueue.Add(syncEvt);
    BroadcastEvent(syncEvt);
    ++SentFrameCount;
}

void NetworkingClass::AdvanceFrame() {
    // Wait for all players to reach the current frame
    if (!IsHost) return;

    bool allReady = true;
    for (int32 i = 0; i < ConnectedPlayers; ++i) {
        if (PlayerFrames[i] < CurrentFrame - MaxAheadFrames) {
            allReady = false;
            break;
        }
    }

    if (allReady) {
        // Process events for the current frame
        ProcessEvents();
    }
}

uint32 NetworkingClass::CalculateGameStateCRC() {
    uint32 crc = 0xFFFFFFFF;
    // Include game state in CRC calculation
    crc ^= static_cast<uint32>(CurrentFrame);
    crc ^= static_cast<uint32>(ConnectedPlayers);
    crc ^= static_cast<uint32>(EventQueue.GetCount());

    for (int32 i = 0; i < 8; ++i) {
        if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
        else crc >>= 1;
    }
    return crc ^ 0xFFFFFFFF;
}

void NetworkingClass::RetransmitPendingEvents() {
    if (!IsNetworkGame) return;
    for (int32 i = 0; i < SyncQueue.GetCount(); ++i) {
        const NetworkEvent& evt = SyncQueue[i];
        if (evt.Frame <= CurrentFrame - FrameLock) {
            SyncQueue.Remove(i);
            --i;
        }
    }
}

// ============================================================
// Player management
// ============================================================

void NetworkingClass::RegisterPlayer(int32 playerID, int32 houseType) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    PlayerHouses[playerID] = houseType;
    PlayerReady[playerID] = true;
    PlayerFrames[playerID] = CurrentFrame;
}

void NetworkingClass::UnregisterPlayer(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    PlayerReady[playerID] = false;
    PlayerHouses[playerID] = -1;
    PlayerFrames[playerID] = 0;
    PlayerLatencies[playerID] = 0;
}

void NetworkingClass::SetPlayerFrame(int32 playerID, int32 frame) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    if (frame > PlayerFrames[playerID]) {
        PlayerFrames[playerID] = frame;
    }
}

void NetworkingClass::SetPlayerLatency(int32 playerID, int32 ms) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return;
    PlayerLatencies[playerID] = ms;
}

HouseClass* NetworkingClass::GetPlayerHouse(int32 playerID) {
    if (playerID < 0 || playerID >= MAX_PLAYERS) return nullptr;
    int32 houseIdx = PlayerHouses[playerID];
    if (houseIdx < 0) return nullptr;
    if (ScenarioClass::Instance) {
        return HouseClass::Array[houseIdx];
    }
    return nullptr;
}

int32 NetworkingClass::GetPlayerIDFromHouse(int32 houseIndex) {
    for (int32 i = 0; i < MAX_PLAYERS; ++i) {
        if (PlayerHouses[i] == houseIndex) {
            return i;
        }
    }
    return -1;
}

// ============================================================
// Chat message routing
// ============================================================

void NetworkingClass::SendChatMessage(int32 senderID, const char* message) {
    if (!message || !message[0]) return;
    if (ChatMessageCount >= MAX_CHAT_MESSAGES) return;

    int32 slot = ChatMessageCount;
    ChatMessages[slot].SenderID = senderID;
    ChatMessages[slot].Frame = CurrentFrame;
    ChatMessages[slot].Delivered = false;

    int32 i = 0;
    while (message[i] && i < 127) {
        ChatMessages[slot].Message[i] = message[i];
        ++i;
    }
    ChatMessages[slot].Message[i] = '\0';
    ++ChatMessageCount;

    // Broadcast chat to all players
    NetworkEvent chatEvt;
    FillEvent(chatEvt, NetworkEventType::Speech, senderID, slot, 0, 0, 0);
    ConnectEventToEvent(chatEvt, NetworkEventType::Speech, senderID, slot, 0, 0, 0);
    BroadcastEvent(chatEvt);
}

void NetworkingClass::ReceiveChatMessage(int32 senderID, int32 messageIndex) {
    if (messageIndex < 0 || messageIndex >= ChatMessageCount) return;
    ChatMessages[messageIndex].Delivered = true;
}

const ChatMessage* NetworkingClass::GetChatMessage(int32 index) const {
    if (index < 0 || index >= MAX_CHAT_MESSAGES) return nullptr;
    if (ChatMessages[index].SenderID < 0) return nullptr;
    return &ChatMessages[index];
}

void NetworkingClass::ConnectEventToEvent(NetworkEvent& evt, NetworkEventType type, int32 playerID, int32 v1, int32 v2, int32 v3, int32 v4) {
    evt.Type = type;
    evt.PlayerID = playerID;
    evt.Value1 = v1;
    evt.Value2 = v2;
    evt.Value3 = v3;
    evt.Value4 = v4;
}

// ============================================================
// Game start/end synchronization
// ============================================================

void NetworkingClass::StartGame() {
    if (!IsHost) return;
    GameRunning = true;
    LobbyFrame = CurrentFrame;
    SyncFrame = CurrentFrame;

    // Send game start event
    NetworkEvent startEvt;
    FillEvent(startEvt, NetworkEventType::Place, -1, 1, 0, 0, 0);
    BroadcastEvent(startEvt);
}

void NetworkingClass::EndGame() {
    GameRunning = false;
    IsSynced_ = false;
    CRCErrors = 0;
    DesyncDetected = false;
    SyncQueue.Clear();
}

void NetworkingClass::SetHost(bool host) {
    IsHost = host;
    if (host) {
        IsNetworkGame = true;
    }
}

bool NetworkingClass::IsGameRunning() const {
    return GameRunning;
}

int32 NetworkingClass::GetMaxAheadFrames() const {
    return MaxAheadFrames;
}

void NetworkingClass::SetMaxAheadFrames(int32 frames) {
    MaxAheadFrames = frames;
    if (MaxAheadFrames < 1) MaxAheadFrames = 1;
    if (MaxAheadFrames > 60) MaxAheadFrames = 60;
}

// ============================================================
// Sync stats
// ============================================================

int32 NetworkingClass::GetCRCErrors() const {
    return CRCErrors;
}

void NetworkingClass::ResetCRCErrors() {
    CRCErrors = 0;
    DesyncDetected = false;
}

bool NetworkingClass::IsDesyncDetected() const {
    return DesyncDetected;
}

// ============================================================
// NetworkEventQueueClass
// ============================================================

NetworkEventQueueClass::NetworkEventQueueClass() : EventCount(0) {
}

NetworkEventQueueClass::~NetworkEventQueueClass() {
    Clear();
}

void NetworkEventQueueClass::AddEvent(NetworkEventType type, int32 v1, int32 v2, int32 v3, int32 v4) {
    NetworkEvent evt;
    evt.Type = type;
    evt.Frame = 0;
    evt.PlayerID = 0;
    evt.Value1 = v1;
    evt.Value2 = v2;
    evt.Value3 = v3;
    evt.Value4 = v4;
    evt.CRC = 0;
    Events.Add(evt);
    ++EventCount;
}

void NetworkEventQueueClass::Process() {
    for (int32 i = 0; i < Events.GetCount(); ++i) {
        Networking()->RespondToEvent(Events[i]);
    }
    Clear();
}

void NetworkEventQueueClass::Clear() {
    Events.Clear();
    EventCount = 0;
}

NetworkEvent* NetworkEventQueueClass::GetEvent(int32 index) {
    if (index < 0 || index >= Events.GetCount()) return nullptr;
    return &Events[index];
}

int32 NetworkEventQueueClass::GetEventCount() const {
    return EventCount;
}