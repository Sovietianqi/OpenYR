#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <climits>

// 32-bit integer types consistent with x86 binary
using int8 = int8_t;
using uint8 = uint8_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;

using DWORD = uint32_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using LONG = int32_t;
using ULONG = uint32_t;
using BOOL = int32_t;

struct GUID;

#include "GUID.h"

using CLSID = GUID;
using REFIID = const GUID&;
using IID = GUID;

#define MAX_PATH_LEN 260
#define MAX_PLAYERS 8
#define MAX_HOUSES 32
#define MAX_TEAMS 60
#define MAX_TRIGGERS 256
#define MAX_ACTIONS 32
#define MAX_SCRIPT_ACTIONS 50
#define MAX_WEAPONS 128
#define MAX_WARHEADS 64

enum class AbstractType : uint32 {
    None = 0, Unit = 1, Aircraft = 2, AircraftType = 3, Anim = 4, AnimType = 5,
    Building = 6, BuildingType = 7, Bullet = 8, BulletType = 9, Campaign = 10,
    Cell = 11, Factory = 12, House = 13, HouseType = 14, Infantry = 15,
    InfantryType = 16, Isotile = 17, IsotileType = 18, BuildingLight = 19,
    Overlay = 20, OverlayType = 21, Particle = 22, ParticleType = 23,
    ParticleSystem = 24, ParticleSystemType = 25, Script = 26, ScriptType = 27,
    Side = 28, Smudge = 29, SmudgeType = 30, Special = 31, SuperWeaponType = 32,
    TaskForce = 33, Team = 34, TeamType = 35, Terrain = 36, TerrainType = 37,
    Trigger = 38, TriggerType = 39, UnitType = 40, VoxelAnim = 41,
    VoxelAnimType = 42, Wave = 43, Tag = 44, TagType = 45, Tiberium = 46,
    Action = 47, Event = 48, WeaponType = 49, WarheadType = 50, Waypoint = 51,
    Abstract = 52, Object = 53, Techno = 54, TechnoType = 77, Foot = 55,
    Tube = 56, LightSource = 57, EMPulse = 58, TacticalMap = 59,
    Super = 60, AITrigger = 61, AITriggerType = 62, Neuron = 63,
    FoggedObject = 64, AlphaShape = 65, VeinholeMonster = 66, NavyType = 67,
    SpawnManager = 68, CaptureManager = 69, Parasite = 70, Bomb = 71,
    RadSite = 72, Temporal = 73, Airstrike = 74, SlaveManager = 75, DiskLaser = 76,
    Map = 78
};

enum class AbstractFlags : uint32 {
    None = 0x0, Techno = 0x1, Object = 0x2, Foot = 0x4
};

inline AbstractFlags operator|(AbstractFlags a, AbstractFlags b) {
    return static_cast<AbstractFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline AbstractFlags operator&(AbstractFlags a, AbstractFlags b) {
    return static_cast<AbstractFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

enum class ArmorType : int32 {
    None = 0, Flak = 1, Plate = 2, Light = 3, Medium = 4,
    Heavy = 5, Wood = 6, Steel = 7, Concrete = 8, Drone = 9, Special_1 = 10
};

enum class HouseType : int32 {
    None = 0, GDI = 1, Nod = 2, Neutral = 3, Special = 4,
    Soviet = 5, Allies = 6, Yuri = 7, Civilian = 8
};

enum class GameMode : int32 {
    Campaign = 0, Skirmish = 1, LAN = 2, Internet = 3
};

enum class TriggerEvent : int32 {
    None = 0, EnteredBy = 1, SpiedBy = 2, ThievedBy = 3, AttackedBy = 4,
    DestroyedBy = 5, AnyEvent = 6, HouseDiscovered = 7, TimeElapsed = 8,
    CreditsExceed = 9, LowPower = 10, AllDestroyed = 11, BuildingExists = 12,
    ElapsedTime = 13, BridgeDestroyed = 14, RandomDelay = 15,
    IronCurtainReady = 16, ChronosphereReady = 17, SuperWeaponReady = 18,
    GlobalSet = 19, GlobalCleared = 20, LocalSet = 21, LocalCleared = 22,
    CrossesHorizontalLine = 23, CrossesVerticalLine = 24, ZoneEntry = 25
};

enum class TriggerAction : int32 {
    None = 0, Win = 1, Lose = 2, EndGame = 3, CreateBuilding = 4,
    DestroyAllOf = 5, FireIronCurtain = 6, LightningStrike = 7,
    PlayAnim = 8, TeleportAll = 9, MindControlHouse = 10,
    ReshroudMap = 11, FlashCameo = 12, RevealWaypoint = 13,
    SetBaseCenter = 14, ClearDefensiveCell = 15, DoExplosion = 16,
    Reinforcement = 17, SpeakText = 18, PlaySound = 19, PlayMovie = 20,
    TriggerOther = 21, TimerSet = 22, TimerAdd = 23, TimerSubtract = 24,
    TimerGo = 25, TimerStop = 26, GlobalSet = 27, GlobalClear = 28,
    LocalSet = 29, LocalClear = 30, AutoCreate = 31, DestroyTrigger = 32,
    DestroyTeam = 33, ChangeHouse = 34, AllowWin = 35, RevealAll = 36,
    RevealAround = 37, Reshroud = 38, DisableTrigger = 39, EnableTrigger = 40,
    FlashTeam = 41, FlashTeamWithDelay = 42, WakeTeam = 43,
    LightningStrikeAt = 44, RemoveParticleSystems = 45, WakeupAttached = 46,
    SetVeinGrowth = 47, SetTiberiumGrowth = 48, SetIceGrowth = 49,
    WakeupAllIdle = 50, WakeupAllHarmless = 51, WakeupGroup = 52,
    PlayerWin = 53, PlayerLose = 54, EndScenario = 55,
    Apply100Damage = 56, SmallLightFlash = 57, MediumLightFlash = 58,
    LargeLightFlash = 59, SellAttached = 60, BerserkAttached = 61,
    SendAttachedOffline = 62, SendAttachedOnline = 63,
    SwitchAttached = 64, SwitchAll = 65, MindControlBuildings = 66,
    RelinquishMindControl = 67, ChronoWarp = 68, IonStorm = 69,
    MeteorShower = 70, LightningStorm = 71, ParaDrop = 72,
    SpyPlane = 73
};

enum class AITriggerCondition : int32 {
    HouseOwns = 0, EnemyOwns = 1, CivilianOwns = 2, HouseCredits = 3,
    IronCurtainReady = 4, ChronosphereReady = 5, BuildingCount = 6,
    UnitCount = 7, InfantryCount = 8, AircraftCount = 9, PowerOutput = 10,
    HasSuperWeapon = 11, TechLevel = 12, Weight = 13
};

enum class AITriggerHouseType : int32 {
    None = 0, Single = 1, Any = 2, Team = 3
};

enum class ScriptAction : int32 {
    Attack = 0, AttackWaypoint = 1, MoveToWaypoint = 2, MoveToCell = 3,
    GuardArea = 4, JumpToLine = 5, PlayerCheck = 6, Wait = 7, Unload = 8,
    Deploy = 9, Follow = 10, LoadIntoTransport = 11, Spy = 12,
    Patrol = 13, EnterTunnel = 14, ChronoWarp = 15, ChronoSphere = 16,
    IronCurtain = 17, Sell = 18, Repair = 19, SelfDestruct = 20,
    ChangeTeam = 21, ChangeScript = 22, ChangeMission = 23,
    Fear = 24, Retreat = 25, Scatter = 26, Stop = 27, Sleep = 28,
    Group = 29, Recruit = 30, Flash = 31, LoadOntoTransports = 32,
    Chronominimum = 33, ChronoMaximum = 34, ForceMove = 35,
    Circle = 36, SearchAndDestroy = 37, Harmless = 38, Suicide = 39,
    Recycle = 40, Repeat = 41, Protect = 42, Sticky = 43,
    Emergency = 44, TakeCover = 45, Gibber = 46, IronCurtainMe = 47,
    ChronoSphereMe = 48, Win = 49, Lose = 50
};

struct ScriptActionNode {
    int32 Action;
    int32 Argument;
};

// NetworkEventType — 事件编号严格对齐原版 gamemd.exe
// 依据汇编 Networking_RespondToEvent 跳转表 off_4C8114（46 分支）
enum class NetworkEventType : int32 {
    // 0x00 - 0x10：常规玩家指令
    POWERON      = 0x00,
    POWEROFF     = 0x01,
    ALLY         = 0x02,
    MEGAMISSION_F= 0x03,
    MEGAMISSION_G= 0x04,
    IDLE         = 0x05,
    SCATTER      = 0x06,
    DESTRUCT     = 0x07,
    DEPLOY       = 0x08,
    DETONATE     = 0x09,
    PLACE        = 0x0A,
    OPTIONS      = 0x0B,
    GAMESPEED    = 0x0C,
    PRODUCE      = 0x0D,
    SUSPEND      = 0x0E,
    ABANDON      = 0x0F,
    PRIMARY      = 0x10,

    // 0x11 - 0x1F
    SPECIAL_PLACE= 0x11,
    EXIT         = 0x12,
    ANIMATION    = 0x13,
    REPAIR       = 0x14,
    SELL         = 0x15,
    SELLCELL     = 0x16,
    SPECIAL      = 0x17,
    PACKETTIMING = 0x18,   // 0x18-0x1B 复用
    RESPONSE_TIME= 0x1A,
    SAVEGAME     = 0x1C,
    ARCHIVE      = 0x1D,
    ADDPLAYER    = 0x1E,
    TIMING       = 0x1F,

    // 0x20 - 0x2D
    PROCESS_TIME = 0x20,
    PAGEUSER     = 0x21,
    REMOVEPLAYER = 0x22,
    LATENCYFUDGE = 0x23,
    ABOUTTOEXIT  = 0x26,
    FALLBACKHOST = 0x27,
    ADDRESSCHANGE= 0x28,
    PLANNODEDELETE= 0x29,  // 0x29-0x2B 复用
    ALLCHEER     = 0x2C,
    ABANDON_ALL  = 0x2D,

    Count        = 0x2E    // 46 个事件
};

struct NetworkEvent {
    NetworkEventType Type;
    int32 Frame;
    int32 PlayerID;
    int32 Value1;
    int32 Value2;
    int32 Value3;
    int32 Value4;
    uint32 CRC;
};

constexpr int32 LeptonsPerCell = 256;
constexpr int32 CellHeight = 208;
constexpr int32 LevelHeight = 104;
constexpr int32 CellWidthInPixels = 60;
constexpr int32 CellHeightInPixels = 30;

struct Point2D {
    int32 X;
    int32 Y;
    Point2D() : X(0), Y(0) {}
    Point2D(int32 x, int32 y) : X(x), Y(y) {}
    Point2D operator+(const Point2D& other) const {
        return Point2D(X + other.X, Y + other.Y);
    }
    Point2D operator-(const Point2D& other) const {
        return Point2D(X - other.X, Y - other.Y);
    }
    Point2D& operator+=(const Point2D& other) {
        X += other.X; Y += other.Y; return *this;
    }
    Point2D& operator-=(const Point2D& other) {
        X -= other.X; Y -= other.Y; return *this;
    }
    bool operator==(const Point2D& other) const {
        return X == other.X && Y == other.Y;
    }
    bool operator!=(const Point2D& other) const {
        return !(*this == other);
    }
};

struct CellStruct {
    int16 X;
    int16 Y;
    CellStruct() : X(0), Y(0) {}
    CellStruct(int16 x, int16 y) : X(x), Y(y) {}
    CellStruct(int32 x, int32 y) : X(static_cast<int16>(x)), Y(static_cast<int16>(y)) {}
    static const CellStruct Empty;
    bool operator==(const CellStruct& other) const {
        return X == other.X && Y == other.Y;
    }
    bool operator!=(const CellStruct& other) const {
        return !(*this == other);
    }
};

struct CoordStruct {
    int32 X;
    int32 Y;
    int32 Z;
    CoordStruct() : X(0), Y(0), Z(0) {}
    CoordStruct(int32 x, int32 y, int32 z) : X(x), Y(y), Z(z) {}

    bool operator==(const CoordStruct& other) const {
        return X == other.X && Y == other.Y && Z == other.Z;
    }
    bool operator!=(const CoordStruct& other) const {
        return !(*this == other);
    }
    CoordStruct operator+(const CoordStruct& other) const {
        return CoordStruct(X + other.X, Y + other.Y, Z + other.Z);
    }
    CoordStruct operator-(const CoordStruct& other) const {
        return CoordStruct(X - other.X, Y - other.Y, Z - other.Z);
    }
    int32 DistanceFrom(const CoordStruct& other) const {
        int32 dx = X - other.X;
        int32 dy = Y - other.Y;
        int32 dz = Z - other.Z;
        return static_cast<int32>(std::sqrt(static_cast<double>(dx*dx + dy*dy + dz*dz)));
    }
    int64 DistanceSquaredFrom(const CoordStruct& other) const {
        int64 dx = static_cast<int64>(X - other.X);
        int64 dy = static_cast<int64>(Y - other.Y);
        int64 dz = static_cast<int64>(Z - other.Z);
        return dx * dx + dy * dy + dz * dz;
    }
};

struct ColorStruct {
    uint8 R;
    uint8 G;
    uint8 B;
    uint8 A;
    ColorStruct() : R(0), G(0), B(0), A(255) {}
    ColorStruct(uint8 r, uint8 g, uint8 b) : R(r), G(g), B(b), A(255) {}
    ColorStruct(uint8 r, uint8 g, uint8 b, uint8 a) : R(r), G(g), B(b), A(a) {}
    bool operator==(const ColorStruct& other) const {
        return R == other.R && G == other.G && B == other.B && A == other.A;
    }
    bool operator!=(const ColorStruct& other) const {
        return !(*this == other);
    }
};

struct RectangleStruct {
    int32 X;
    int32 Y;
    int32 Width;
    int32 Height;
    RectangleStruct() : X(0), Y(0), Width(0), Height(0) {}
    RectangleStruct(int32 x, int32 y, int32 w, int32 h) : X(x), Y(y), Width(w), Height(h) {}
};

struct RandomStruct {
    int32 Min;
    int32 Max;
    RandomStruct() : Min(0), Max(0) {}
    RandomStruct(int32 min, int32 max) : Min(min), Max(max) {}
    int32 GetRandom() const {
        if (Max <= Min) return Min;
        return Min + (rand() % (Max - Min + 1));
    }
};

class NOVTABLE {
};

struct noinit_t {};

constexpr noinit_t noinit{};

struct DirStruct {
    uint8 Value;
    DirStruct() : Value(0) {}
    explicit DirStruct(uint8 v) : Value(v) {}
};

enum class DirType : uint8 {
    N = 0, NE = 16, E = 32, SE = 48, S = 64, SW = 80, W = 96, NW = 112, COUNT = 128
};

using FacingType = DirType;

enum class TriggerEventType : int32 {
    None = 0, EnteredBy = 1, SpiedBy = 2, ThievedBy = 3, AttackedBy = 4,
    DestroyedBy = 5, AnyEvent = 6, HouseDiscovered = 7, TimeElapsed = 8,
    CreditsExceed = 9, LowPower = 10, AllDestroyed = 11, BuildingExists = 12,
    ElapsedTime = 13, BridgeDestroyed = 14, RandomDelay = 15,
    IronCurtainReady = 16, ChronosphereReady = 17, SuperWeaponReady = 18,
    GlobalSet = 19, GlobalCleared = 20, LocalSet = 21, LocalCleared = 22,
    CrossesHorizontalLine = 23, CrossesVerticalLine = 24, ZoneEntry = 25
};

enum class DamageType : int32 {
    Normal = 0,
    Fire = 1,
    Electric = 2,
    Radiation = 3,
    Sonic = 4,
    Psychic = 5,
    Special = 6
};

enum class Mission : int32 {
    Sleep = 0, Harmless, Ambush, Attack, Capture, Eaten, Guard, AreaGuard,
    Harvest, Hunt, Move, Retreat, Return, Stop, Unload, Enter, Construction,
    Selling, Repair, Missile, Open, Rescue, Patrol, ParaDropApproach,
    ParaDropOverfly, Wait, SpyPlaneApproach, SpyPlaneOverfly,
    // Yuri's Revenge extended missions (added to support script actions that
    // previously used fallback missions because these were missing).
    Deploy, Follow, Spy, EnterTunnel, ChronoWarp, ChronoSphere,
    IronCurtain, SelfDestruct, Circle, Recycle, Sticky,
    Emergency, TakeCover, Gibber,
    Count
};

enum class Sequence : int32 {
    Ready = 0, Guard = 1, Prone = 2, Walk = 3, FireUp = 4, Down = 5,
    FireProne = 6, Idle1 = 7, Idle2 = 8, Die1 = 9, Die2 = 10, Die3 = 11,
    Die4 = 12, Die5 = 13, Swim = 14, WetIdle1 = 15, WetIdle2 = 16,
    WetDie1 = 17, WetDie2 = 18, Crawl = 19, Fly = 20, FireFly = 21,
    IdleFly = 22, DieFly = 23, Tumble = 24, Deploy = 25, Deployed = 26,
    DeployedFire = 27, DeployedIdle = 28, Undeploy = 29, Paradrop = 30,
    Enter = 31, Unload = 32, Deploy2 = 33, Harvest = 34, Count = 35
};

enum class RadioCommand : int32 {
    None = 0, RequestDock = 1, Dock = 2, RequestUnload = 3, Unload = 4,
    RequestMove = 5, Move = 6, RequestLink = 7, Link = 8,
    RequestBugOut = 9, BugOut = 10, RequestPark = 11, Park = 12
};

enum class Action : int32 {
    None = 0, Move = 1, Select = 2, Attack = 3, Guard = 4, Deploy = 5,
    Repair = 6, Sell = 7, Enter = 8, Capture = 9, Harvest = 10,
    NoMove = 11, NoSelect = 12, NoAttack = 13, NoDeploy = 14,
    Scatter = 15, TogglePrimary = 16, Scroll = 17, Waypoint = 18,
    Nuke = 19, IronCurtain = 20, ChronoSphere = 21, LightningStorm = 22,
    Dominator = 23, ParaDrop = 24, ForceShield = 25, IonCannon = 26,
    HunterSeeker = 27, SpyPlane = 28, GeneticMutator = 29,
    PsychicReveal = 30, ChronoWarp = 31, DropPod = 32
};

enum class BuildCat : int32 {
    Tech = 0, Resource = 1, Power = 2, Infrastructure = 3, Combat = 4
};

enum class VisualType : int32 {
    Normal = 0, Hidden = 1, Cloaked = 2, Shadow = 3
};

enum class Layer : int32 {
    Ground = 0, Surface = 1, Air = 2, Top = 3
};

enum class Move : int32 {
    OK = 0, No = 1, Temp = 2, Destructible = 3
};

enum class MarkType : int32 {
    Up = 0, Down = 1
};

enum class ZGradient : int32 {
    None = 0, Ground = 1, Degrade = 2
};

enum class FireError : int32 {
    OK = 0, No = 1, Ammo = 2, ROT = 3, Ill = 4, Movement = 5,
    Recharge = 6, Range = 7, Area = 8, NoAmmo = 9, NotReady = 10
};

constexpr int32 MAX_SCRIPT_ACTIONS_COUNT = 50;
constexpr int32 MAX_VERSES = 11;

// Additional types needed by game engine
using COLORREF = uint32;

// Movement zone types
enum class MovementZone : int32 {
    Normal = 0, Crusher = 1, Destroyer = 2, Water = 3,
    WaterBeach = 4, Amphibious = 5, AmphibiousCrusher = 6,
    AmphibiousDestroyer = 7, Fly = 8
};

// Land types
enum class LandType : int32 {
    Clear = 0, Rough = 1, Road = 2, Water = 3, Rock = 4,
    Wall = 5, Tiberium = 6, Beach = 7, Tunnel = 8,
    Railroad = 9, Weeds = 10, Ice = 11
};

// Speed types
enum class SpeedType : int32 {
    Slow = 0, Medium = 1, Fast = 2, VeryFast = 3
};

// Damage area result
enum class DamageAreaResult : int32 {
    None = 0, Hit = 1, Miss = 2
};

// Theater types
enum class TheaterType : int32 {
    Temperate = 0, Snow = 1, Urban = 2, Desert = 3,
    Lunar = 4, NewUrban = 5
};

// Armor types
enum class Armor : int32 {
    None = 0, Flak = 1, Plate = 2, Light = 3, Medium = 4,
    Heavy = 5, Wood = 6, Steel = 7, Concrete = 8, Drone = 9,
    Special_1 = 10
};

// Add DistanceSquaredFrom to CoordStruct
inline int64 DistanceSquaredFrom(const CoordStruct& a, const CoordStruct& b) {
    int64 dx = static_cast<int64>(a.X - b.X);
    int64 dy = static_cast<int64>(a.Y - b.Y);
    int64 dz = static_cast<int64>(a.Z - b.Z);
    return dx * dx + dy * dy + dz * dz;
}

struct VoxelIndexKey { int32 dummy; };
class WeaponTypeClass;
struct WeaponStruct {
    WeaponTypeClass* WeaponType;  // resolved WeaponTypeClass for this slot (null if unset)
    int32 dummy;
};
class IStream;
class IPersistStream;
class IPersist;
class INoticeSink;
class INoticeSource;
class IRTTITypeInfo;
class ILocomotion;
class IPiggyback;
class ObjectTypeClass;
class TechnoTypeClass;
class HouseClass;
class HouseTypeClass;
class TechnoClass;
class FootClass;
class ObjectClass;
class AbstractClass;
class AnimClass;
class AnimTypeClass;
class BuildingClass;
class BuildingTypeClass;
class CellClass;
class CCINIClass;
class CRCEngine;
class BulletTypeClass;
class ParticleTypeClass;
class ParticleSystemTypeClass;
class VoxelAnimTypeClass;
class WarheadTypeClass;
class WeaponTypeClass;
class SuperClass;
class TriggerTypeClass;
class TriggerClass;
class TActionClass;
class TEventClass;
class TeamTypeClass;
class TeamClass;
class ScriptTypeClass;
class ScriptClass;
class TaskForceClass;
class TagClass;
class TagTypeClass;
class AITriggerTypeClass;
class AITeamTypeClass;
class AITeamClass;
class InfantryTypeClass;
class UnitTypeClass;
class AircraftTypeClass;
class AircraftClass;
class InfantryClass;
class UnitClass;
class SuperWeaponTypeClass;
class MPGameModeClass;
class SessionClass;
class IPXManagerClass;
class ShapeButtonClass;
class LineTrail;
class BombClass;
class VoxelAnimClass;
class SHPStruct;
class OverlayClass;
class OverlayTypeClass;
class SmudgeClass;
class SmudgeTypeClass;
class TerrainClass;
class TerrainTypeClass;
class TiberiumClass;
class WaveClass;
class RadarClass;
class RadarEventClass;
class RadioClass;
class SpawnManagerClass;
class CaptureManagerClass;
class ParasiteClass;
class EMPulseClass;
class RadSiteClass;
class TemporalClass;
class AirstrikeClass;
class SlaveManagerClass;
class DiskLaserClass;
class ParticleClass;
class ParticleSystemClass;
class AlphaShapeClass;
class VeinholeMonsterClass;
class BuildingLightClass;
class FactoryClass;
class SideClass;
class CampaignClass;
class ScenarioClass;
class RulesClass;
class MapClass;
class DisplayClass;
class TacticalClass;
class GScreenClass;
class MouseClass;
class MessageListClass;
class EventClass;
class SidebarClass;
class TabClass;
class ControlClass;
class NetworkEvent;
struct NodeNameType;
struct SessionOptionsClass;
class WinsockInterfaceClass;
class ConnectionClass;
class IPXConnectionClass;
class UDPConnectionClass;
class UDPInterfaceClass;
class NetworkingClass;
class LocomotionClass;
class WalkLocomotionClass;
class DriveLocomotionClass;
class ShipLocomotionClass;
class FlyLocomotionClass;
class JumpjetLocomotionClass;
class RocketLocomotionClass;
class TeleportLocomotionClass;
class TunnelLocomotionClass;
class DropPodLocomotionClass;
class BulletClass;
class DamageArea;

class GlobalFiring;
class CCFileClass;
class FileSystem;
class IUnknown;
class TimerStruct;
class CDTimerClass;
class RateTimer;
class ColorScheme;
class VoxClass;
class VocClass;
class ThemeClass;
class Theater;
class StringTable;
class Randomizer;
class Audio;
class Matrix3D;
class Quaternion;
class AITriggerTypeClass;
class AITeamTypeClass;
class AITeamClass;