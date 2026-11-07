#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// TAction —— 触发器动作种类的权威数值表。
//
// 数值直接取自 asm TActionClass_Execute 的 switch 跳转表
// （switch_TriggerAction, asm:0x6DD880），下标 = kindAction - 1，共 145 个
// 入口（kind 1..145）。TriggerTypeClass 侧的判定（如 InvolvesAllowWin 比较
// 0x0F、getActionFlags 以 kind-14 索引）同样以这些原始数值为准，所以枚举
// 值必须与 asm case 号逐一对应，不可自定义。
// ============================================================================
enum class TAction {
    WinGame = 0,                     // lct_TriggerAction_WinnerIs
    LoseGame = 1,                    // lct_TriggerAction_LoserIs
    Production = 2,                  // lct_TriggerAction_ProductionBegins
    CreateTeam = 3,                  // lct_TriggerAction_CreateTeam
    DestroyTeam = 4,                 // lct_TriggerAction_DestroyTeam
    AllToHunt = 5,                   // lct_TriggerAction_AllToHunt
    Reinforcement = 6,               // lct_TriggerAction_Reinforcement
    DropZoneFlare = 7,               // lct_TriggerAction_DropZoneFlare
    FireSale = 8,                    // lct_TriggerAction_FireSale
    PlayMovie = 9,                   // lct_TriggerAction_PlayMovie
    TextTrigger = 10,                // lct_TriggerAction_TextTrigger
    DestroyTrigger = 11,             // lct_TriggerAction_DestroyTrigger
    AutocreateBegins = 12,           // lct_TriggerAction_AutocreateBegins
    ChangeHouse = 13,                // lct_TriggerAction_ChangeHouse
    AllowWin = 14,                   // lct_TriggerAction_AllowWin
    RevealAllMap = 15,               // lct_TriggerAction_RevealAllMap
    RevealAroundWaypoint = 16,       // lct_TriggerAction_RevealAroundWaypoint
    RevealZoneOfWaypoint = 17,       // lct_TriggerAction_RevealZoneOfWaypoint
    PlaySoundEffect = 18,            // lct_TriggerAction_PlaySoundEffect
    PlayMusicTheme = 19,             // lct_TriggerAction_PlayMusicTheme
    PlaySpeech = 20,                 // lct_TriggerAction_PlaySpeech
    ForceTrigger = 21,               // lct_TriggerAction_ForceTrigger
    TimerStart = 22,                 // lct_TriggerAction_TimerStart
    TimerStop = 23,                  // lct_TriggerAction_TimerStop
    TimerExtend = 24,                // lct_TriggerAction_TimerExtend
    TimerShorten = 25,               // lct_TriggerAction_TimerShorten
    TimerSet = 26,                   // lct_TriggerAction_TimerSet
    GlobalSet = 27,                  // lct_TriggerAction_GlobalSet
    GlobalClear = 28,                // lct_TriggerAction_GlobalClear
    AutoBaseBuilding = 29,           // lct_TriggerAction_AutoBaseBuilding
    GrowShroudOneStep = 30,          // lct_TriggerAction_GrowShroudOneStep
    DestroyAttachedBuilding = 31,    // lct_TriggerAction_DestroyAttachedBuilding
    Add1TimeSpecialWeapon = 32,      // lct_TriggerAction_Add1TimeSpecialWeapon
    AddRepeatingSpecialWeapon = 33,  // lct_TriggerAction_AddRepeatingSpecialWeapon
    PreferredTarget = 34,            // lct_TriggerAction_PreferredTarget
    AllChangeHouse = 35,             // lct_TriggerAction_AllChangeHouse
    MakeAlly = 36,                   // lct_TriggerAction_MakeAlly
    MakeEnemy = 37,                  // lct_TriggerAction_MakeEnemy
    ChangeZoomLevel = 38,            // lct_TriggerAction_ChangeZoomLevel
    ResizePlayerView = 39,           // lct_TriggerAction_ResizePlayerView
    PlayAnimAt = 40,                 // lct_TriggerAction_PlayAnimAt
    DoExplosionAt = 41,              // lct_TriggerAction_DoExplosionAt
    MeteorImpactAt = 42,             // lct_TriggerAction_MeteorImpactAt
    IonStormStart = 43,              // lct_TriggerAction_IonStormStart
    IonStormStop = 44,               // lct_TriggerAction_IonStormStop
    LockInput = 45,                  // lct_TriggerAction_LockInput
    UnlockInput = 46,                // lct_TriggerAction_UnlockInput
    CenterCameraAtWaypoint = 47,     // lct_TriggerAction_CenterCameraAtWaypoint
    ZoomIn = 48,                     // lct_TriggerAction_ZoomIn
    ZoomOut = 49,                    // lct_TriggerAction_ZoomOut
    ReshroudMap = 50,                // lct_TriggerAction_ReshroudMap
    ChangeLightBehaviour = 51,       // lct_TriggerAction_ChangeLightBehaviour
    EnableTrigger = 52,              // lct_TriggerAction_EnableTrigger
    DisableTrigger = 53,             // lct_TriggerAction_DisableTrigger
    CreateRadarEvent = 54,           // lct_TriggerAction_CreateRadarEvent
    LocalSet = 55,                   // lct_TriggerAction_LocalSet
    LocalClear = 56,                 // lct_TriggerAction_LocalClear
    MeteorShowAt = 57,               // lct_TriggerAction_MeteorShowAt
    ReduceTiberiumAt = 58,           // lct_TriggerAction_ReduceTiberiumAt
    SellBuilding = 59,               // lct_TriggerAction_SellBuilding
    TurnOffBuilding = 60,            // lct_TriggerAction_TurnOffBuilding
    TurnOnBuilding = 61,             // lct_TriggerAction_TurnOnBuilding
    Apply100DamageAt = 62,           // lct_TriggerAction_Apply100DamageAt
    LightFlashSmallAt = 63,          // lct_TriggerAction_LightFlashSmallAt
    LightFlashMediumAt = 64,         // lct_TriggerAction_LightFlashMediumAt
    LightFlashLargeAt = 65,          // lct_TriggerAction_LightFlashLargeAt
    AnnounceWin = 66,                // lct_TriggerAction_AnnounceWin
    AnnounceLose = 67,               // lct_TriggerAction_AnnounceLose
    ForceEnd = 68,                   // lct_TriggerAction_ForceEnd
    DestroyTag = 69,                 // lct_TriggerAction_DestroyTag
    SetAmbientStep = 70,             // lct_TriggerAction_SetAmbientStep
    SetAmbientRate = 71,             // lct_TriggerAction_SetAmbientRate
    SetAmbientLight = 72,            // lct_TriggerAction_SetAmbientLight
    AITriggersBegin = 73,            // lct_TriggerAction_AITriggersBegin
    AITriggersStop = 74,             // lct_TriggerAction_AITriggersStop
    RatioOfAITriggerTeams = 75,      // lct_TriggerAction_RatioOfAITriggerTeams
    RatioOfTeamAircraft = 76,        // lct_TriggerAction_RatioOfTeamAircraft
    RatioOfTeamInfantry = 77,        // lct_TriggerAction_RatioOfTeamInfantry
    RatioOfTeamUnits = 78,           // lct_TriggerAction_RatioOfTeamUnits
    ReinforcementTeam = 79,          // lct_TriggerAction_ReinforcementTeam
    WakeupSelf = 80,                 // lct_TriggerAction_WakeupSelf
    WakeupAllSleepers = 81,          // lct_TriggerAction_WakeupAllSleepers
    WakeupAllHarmless = 82,          // lct_TriggerAction_WakeupAllHarmless
    WakeupGroup = 83,                // lct_TriggerAction_WakeupGroup
    VeinGrowth = 84,                 // lct_TriggerAction_VeinGrowth
    TiberiumGrowth = 85,             // lct_TriggerAction_TiberiumGrowth
    IceGrowth = 86,                  // lct_TriggerAction_IceGrowth
    ParticleAnimAt = 87,             // lct_TriggerAction_ParticleAnimAt
    RemoveParticleAnimAt = 88,       // lct_TriggerAction_RemoveParticleAnimAt
    LightningStrikeAt = 89,          // lct_TriggerAction_LightningStrikeAt
    GoBerzerk = 90,                  // lct_TriggerAction_GoBerzerk
    // case 91/92 复用 ChangeZoomLevel（asm 跳转表三重指向同一标签）。
    ChangeZoomLevel2 = 91,           // lct_TriggerAction_ChangeZoomLevel
    ChangeZoomLevel3 = 92,           // lct_TriggerAction_ChangeZoomLevel
    IonCannonStrike = 93,            // lct_TriggerAction_IonCannonStrike
    NukeStrike = 94,                 // lct_TriggerAction_NukeStrike
    ChemMissileStrike = 95,          // lct_TriggerAction_ChemMissileStrike
    ToggleTrainCargo = 96,           // lct_TriggerAction_ToggleTrainCargo
    PlaySoundEffectRandom = 97,      // lct_TriggerAction_PlaySoundEffectRandom
    PlaySoundEffectAt = 98,          // lct_TriggerAction_PlaySoundEffectAt
    PlayIngameMovie = 99,            // lct_TriggerAction_PlayIngameMovie
    ReshroudMapAt = 100,             // lct_TriggerAction_ReshroudMapAt
    LightningStormStrike = 101,      // lct_TriggerAction_LightningStormStrike
    TimerText = 102,                 // lct_TriggerAction_TimerText
    FlashTeam = 103,                 // lct_TriggerAction_FlashTeam
    TalkBubble = 104,                // lct_TriggerAction_TalkBubble
    SetTechLevel = 105,              // lct_TriggerAction_SetTechLevel
    ReinforcementByChrono = 106,     // lct_TriggerAction_ReinforcementByChrono
    CreateCrate = 107,               // lct_TriggerAction_CreateCrate
    IronCurtainAt = 108,             // lct_TriggerAction_IronCurtainAt
    PauseGameForSeconds = 109,       // lct_TriggerAction_PauseGameForSeconds
    EvictOccupiers = 110,            // lct_TriggerAction_EvictOccupiers
    JumpCameraToWaypoint = 111,      // lct_TriggerAction_JumpCameraToWaypoint
    MakeSideCheer = 112,             // lct_TriggerAction_MakeSideCheer
    SetTabTo = 113,                  // lct_TriggerAction_SetTabTo
    FlashCameo = 114,                // lct_TriggerAction_FlashCameo
    StopSoundsAt = 115,              // lct_TriggerAction_StopSoundsAt
    PlayIngameMoviePause = 116,      // lct_TriggerAction_PlayIngameMoviePause
    ClearAllSmudges = 117,           // lct_TriggerAction_ClearAllSmudges
    DestroyAllOf = 118,              // lct_TriggerAction_DestroyAllOf
    DestroyAllBuildingsOf = 119,     // lct_TriggerAction_DestroyAllBuildingsOf
    DestroyAllLandUnitsOf = 120,     // lct_TriggerAction_DestroyAllLandUnitsOf
    DestroyAllNavalUnitsOf = 121,    // lct_TriggerAction_DestroyAllNavalUnitsOf
    MindControlBaseOf = 122,         // lct_TriggerAction_MindControlBaseOf
    RestoreMindControlledBaseTo = 123, // lct_TriggerAction_RestoreMindControlledBaseTo
    CreateBuildingAt = 124,          // lct_TriggerAction_CreateBuildingAt
    RestoreStartingTechnosOf = 125,  // lct_TriggerAction_RestoreStartingTechnosOf
    ChronoScreenEffectFor = 126,     // lct_TriggerAction_ChronoScreenEffectFor
    TeleportAllTo = 127,             // lct_TriggerAction_TeleportAllTo
    SetSuperWeaponCharge = 128,      // lct_TriggerAction_SetSuperWeaponCharge
    RestoreStartingBuildingsOf = 129, // lct_TriggerAction_RestoreStartingBuildingsOf
    FlashBuildingsOfType = 130,      // lct_TriggerAction_FlashBuildingsOfType
    SuperWeaponSetRechargeTime = 131,   // lct_TriggerAction_SuperWeaponSetRechargeTime
    SuperWeaponResetRechargeTime = 132, // lct_TriggerAction_SuperWeaponResetRechargeTime
    SuperWeaponReset = 133,          // lct_TriggerAction_SuperWeaponReset
    PreferredTargetCellSet = 134,    // lct_TriggerAction_PreferredTargetCellSet
    PreferredTargetCellClear = 135,  // lct_TriggerAction_PreferredTargetCellClear
    CenterBaseCellSet = 136,         // lct_TriggerAction_CenterBaseCellSet
    CenterBaseCellClear = 137,       // lct_TriggerAction_CenterBaseCellClear
    RadarBlackout = 138,             // lct_TriggerAction_RadarBlackout
    DefensiveTargetCellSet = 139,    // lct_TriggerAction_DefensiveTargetCellSet
    DefensiveTargetCellClear = 140,  // lct_TriggerAction_DefensiveTargetCellClear
    RetintRed = 141,                 // lct_TriggerAction_RetintRed
    RetintGreen = 142,               // lct_TriggerAction_RetintGreen
    RetintBlue = 143,                // lct_TriggerAction_RetintBlue
    JumpCameraHome = 144             // lct_TriggerAction_JumpCameraHome
};

class CCINIClass;
class HouseClass;
class TechnoTypeClass;
class TeamTypeClass;
class TriggerClass;
class TriggerTypeClass;
class TagTypeClass;

// ============================================================================
// TActionClass —— 触发器动作（原版 ActionClass）。
//
// 与 TagTypeClass 同属「AbstractTypeClass 派生 + 走 INI 的数组型类型」：
// 标识符内嵌在基类的 ID[] 里，实例在构造时登记进静态 Array 并缓存下标。
// asm 结构（sizeof=0x94）：
//   vt            +0x00
//   a             AbstractTypeClass
//   idxInArray    +0x24   在 vec_Actions 里的下标缓存
//   chainedAction +0x28   Swizzle 指针（ActionClass 链）
//   kindAction    +0x2C   动作种类（数值）
//   TeamType      +0x30   TeamTypeClass*
//   Tag           +0x34   TagTypeClass*
//   Waypoint      +0x44   路点下标
//   field_48      +0x48   通用参数 4（超武参数等）
//   arg4/link     +0x4C   TriggerTypeClass*/Tag 指针
//   Trigger       +0x50   TriggerTypeClass* 目标触发器类型
//                            （asm: [cAction+Trigger] vs [cur+TriggerClass.triggerType]）
//   text          +0x54   内嵌 0x18 字节对象名
//   text__        +0x6D   内嵌 0x20 字节文本/电影名
//   field_90      +0x90   动作类型 / 参数 1
// 本实现按功能对齐，字段命名沿用语义名（不要求逐字节一致）。
// ============================================================================
class TActionClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::Action;

    static DynamicVectorClass<TActionClass*>* Array;

    static TActionClass* Find(const char* pID);
    static TActionClass* FindOrAllocate(const char* pID);

    TActionClass(const char* pID) noexcept;
    explicit TActionClass(noinit_t) noexcept;
    virtual ~TActionClass();

    // ========================================================================
    // IPersistStream / RTTI / CRC（原版 vftable 槽）
    // ========================================================================
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual int32 Size() const override;              // asm: TActionClass_GetSize
    virtual int32 GetArrayIndex() const override;     // asm: TActionClass_GetArrayIndex
    virtual void ComputeCRC(CRCEngine& crc) const override;
    virtual int32 GetCRC() const;                     // asm: TActionClass_GetCRC

    virtual bool LoadFromINI(CCINIClass* pINI) override;  // asm: TActionClass_LoadFromINI
    virtual bool SaveToINI(CCINIClass* pINI) override;    // asm: ActionClass_SaveIntoINI

    // asm: TActionClass_LoadFromINI —— 从 token 数组的 *pIndex 处解析一个动作元素
    // （kind,selector,name,arg3..arg6[,argN]），推进 *pIndex 并返回消费的 token 数。
    // 供 TriggerTypeClass::LoadFromINI 对 [Actions] 串逐元素解析；<0 表示字段不足。
    int32 ParseTokens(const char* const* tokens, int32 count, int32* pIndex);

    // asm: TActionClass_PointerGotInvalid —— 被销毁实例若正是某个 swizzle 槽所
    // 指的链头，则把该槽前进到链中下一个（此处仅清空相关指针）。
    void PointerGotInvalid(AbstractClass* gone);

    // [Actions] 段的整体读写：键名是触发器类型 ID，值是动作命令串。
    bool LoadFromINIList(CCINIClass* pINI);
    bool SaveToINIList(CCINIClass* pINI);

    void ExecuteAction(TriggerClass* pTrigger);

    // asm TActionClass_Execute 的 trigger 栈参数（TriggerClass* 实例）。每个
    // case 都用它配合 ActionClass_FindHouseByIdx / TagClass_IsAttachedToTrigger
    // 解析目标房子与目标对象。Execute 进入时记录，退出时清除。
    TriggerClass* ActiveTrigger;

    // 根据游戏行为，可知动作注册表会为“允许胜利”类动作打标，供触发器
    // 判定其动作链能否宣告胜利。
    bool IsVictoryAction() const;
    bool IsAllowWinFlag;
    void GetActionName(char* buffer, int32 bufferSize) const;
    void SetTrigger(TriggerTypeClass* pTrigger);
    TriggerTypeClass* GetTrigger() const;

    // ========================================================================
    // Scripted action helpers (asm 0x6E42C3..0x6E460E)
    //
    //  The original binary implements these as standalone ActionClass_*
    //  functions despatched from TActionClass::Execute with `this` already in
    //  ECX, so they behave as member functions.  Each returns true when it
    //  applied the action and false when a precondition (usually the target
    //  house or the waypoint) was not met.
    // ========================================================================
    bool SetTargetCell(TriggerClass* pTrigger);
    bool ClearTargetCell(TriggerClass* pTrigger);
    bool SetDefensiveCell(TriggerClass* pTrigger);
    bool ClearDefensiveCell(TriggerClass* pTrigger);
    bool SetBaseCenter(TriggerClass* pTrigger);
    bool ClearBaseCenter(TriggerClass* pTrigger);
    bool SetSWCharge(TriggerClass* pTrigger);
    bool SetSWRecharge(TriggerClass* pTrigger);
    bool ResetSWRecharge(TriggerClass* pTrigger);
    bool ResetSW(TriggerClass* pTrigger);
    HouseClass* FindHouseByIdx(TriggerClass* pTrigger, int32 idx);

    // asm TriggerClass_FireActions 传给 Execute 的 House 参数：由调用触发器的
    // 类型所属国家（TriggerTypeClass.House -> idxCountry）解析出的 HouseClass*。
    HouseClass* Resolve_Trigger_House(TriggerClass* pTrigger);

    // ========================================================================
    // Special strike actions (asm 0x6E35F0 / 0x6E38C0 / 0x6E33A0)
    // ========================================================================
    bool FireChemLauncher(HouseClass* pOwner, int32 a3, TriggerClass* pTrigger, int32 a5);
    bool TriggerNukeStrike(HouseClass* pOwner, int32 a3, TriggerClass* pTrigger, int32 a5);

    // ========================================================================
    // ActionClass_* handlers (asm 0x6E1xxx..0x6E4xxx).  The binary implements
    // these as free functions called with `this` already loaded into ECX, so
    // they are modelled as members here.
    // ========================================================================
    bool ClearSmudges();
    bool RetintRed();
    bool RetintGreen();
    bool RetintBlue();
    bool RadarBlackout(HouseClass* pHouse);
    bool TeleportAllTo(HouseClass* pHouse);
    bool ReshroudMap();
    bool DropFlare(HouseClass* pHouse, TriggerClass* pTrigger);
    bool StopSoundsAt(HouseClass* pHouse, TriggerClass* pTrigger);
    bool WakeupAttachedObjects(HouseClass* pHouse, int32 a3,
                               TriggerClass* pTrigger, int32 a5);
    bool MindControlHouseBuildings(HouseClass* pNewOwner, TriggerClass* pTrigger);
    bool ReturnControlHouseBuildings(HouseClass* pOwner, TriggerClass* pTrigger);
    bool ResizePlayerView();
    bool EnableTrigger(HouseClass* pHouse, TriggerClass* pTrigger);
    bool FlashCameo(HouseClass* pHouse, TriggerClass* pTrigger);
    bool CreateBuilding(int32 a1, int32 a3, int32 a4, int32 a5);
    bool FlashBuildingsOfType(HouseClass* pHouse);

    // The recurring house-resolution helper every ActionClass handler uses:
    // index 0x2325 means "the house related to the calling trigger", -1 means
    // "no house", and anything else is looked up in the house array - through
    // the MP-aware variant for the seven special multiplayer country slots.
    static HouseClass* Resolve_Action_House(int32 idx, TriggerClass* pTrigger);

    bool AttachedTagSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger);
    bool FireIronCurtain(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllLandUnitsOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool DestroyAllNavalOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool RestoreStartingTechnoOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool RestoreStartingBuildingsOf(HouseClass* pHouse, TriggerClass* pTrigger);
    bool SetTab(int32 tabIndex);
    bool SetHouseTargetCell(HouseClass* pHouse);
    bool ClearHouseTargetCell(HouseClass* pHouse);
    bool LightningStrikeAt();
    bool Apply100DamageAt();
    bool AllObjectsSwitchHouse(HouseClass* pHouse, TriggerClass* pTrigger);
    bool PlayAnimAt();
    bool RevealZoneOfWaypoint();
    bool DoExplosionAt();

private:
    // 每个 Action_* 对应 asm TActionClass_Execute 的一个（或一组）case。
    void Action_WinGame();
    void Action_LoseGame();
    void Action_Production();
    void Action_CreateTeam();
    void Action_DestroyTeam();
    void Action_AllToHunt();
    void Action_Reinforcement();
    void Action_DropZoneFlare();
    void Action_FireSale();
    void Action_PlayMovie();
    void Action_TextTrigger();
    void Action_DestroyTrigger();
    void Action_AutocreateBegins();
    void Action_ChangeHouse();
    void Action_AllowWin();
    void Action_RevealAllMap();
    void Action_RevealAroundWaypoint();
    void Action_RevealZoneOfWaypoint();
    void Action_PlaySoundEffect();
    void Action_PlayMusicTheme();
    void Action_PlaySpeech();
    void Action_ForceTrigger();
    void Action_TimerStart();
    void Action_TimerStop();
    void Action_TimerExtend();
    void Action_TimerShorten();
    void Action_TimerSet();
    void Action_GlobalSet();
    void Action_GlobalClear();
    void Action_AutoBaseBuilding();
    void Action_GrowShroudOneStep();
    void Action_DestroyAttachedBuilding();
    void Action_AddSpecialWeapon(bool repeating);
    void Action_PreferredTarget();
    void Action_AllChangeHouse();
    void Action_MakeAllyEnemy(bool makeAlly);
    void Action_ChangeZoomLevel();
    void Action_ResizePlayerView();
    void Action_PlayAnimAt();
    void Action_DoExplosionAt();
    void Action_MeteorImpactAt();
    void Action_IonStormStart();
    void Action_IonStormStop();
    void Action_LockInput();
    void Action_UnlockInput();
    void Action_CenterCameraAtWaypoint();
    void Action_ZoomIn();
    void Action_ZoomOut();
    void Action_ReshroudMap();
    void Action_ChangeLightBehaviour();
    void Action_EnableTrigger();
    void Action_DisableTrigger();
    void Action_CreateRadarEvent();
    void Action_LocalSet();
    void Action_LocalClear();
    void Action_MeteorShowAt();
    void Action_ReduceTiberiumAt();
    void Action_SellBuilding();
    void Action_TurnOffBuilding();
    void Action_TurnOnBuilding();
    void Action_Apply100DamageAt();
    void Action_LightFlash(int32 sizeClass);
    void Action_AnnounceWin();
    void Action_AnnounceLose();
    void Action_ForceEnd();
    void Action_DestroyTag();
    void Action_SetAmbientStep();
    void Action_SetAmbientRate();
    void Action_SetAmbientLight();
    void Action_AITriggers(bool begin);
    void Action_RatioOfTeam(int32 which);
    void Action_ReinforcementTeam();
    void Action_Wakeup(int32 which);
    void Action_VeinGrowth();
    void Action_TiberiumGrowth();
    void Action_IceGrowth();
    void Action_ParticleAnimAt();
    void Action_RemoveParticleAnimAt();
    void Action_LightningStrikeAt();
    void Action_GoBerzerk();
    void Action_IonCannonStrike();
    void Action_NukeStrike();
    void Action_ChemMissileStrike();
    void Action_ToggleTrainCargo();
    void Action_PlaySoundEffectRandom();
    void Action_PlaySoundEffectAt();
    void Action_PlayIngameMovie();
    void Action_ReshroudMapAt();
    void Action_LightningStormStrike();
    void Action_TimerText();
    void Action_FlashTeam();
    void Action_TalkBubble();
    void Action_SetTechLevel();
    void Action_ReinforcementByChrono();
    void Action_CreateCrate();
    void Action_IronCurtainAt();
    void Action_PauseGameForSeconds();
    void Action_EvictOccupiers();
    void Action_JumpCameraToWaypoint();
    void Action_MakeSideCheer();
    void Action_SetTabTo();
    void Action_FlashCameo();
    void Action_StopSoundsAt();
    void Action_PlayIngameMoviePause();
    void Action_ClearAllSmudges();
    void Action_DestroyAllOf();
    void Action_DestroyAllBuildingsOf();
    void Action_DestroyAllLandUnitsOf();
    void Action_DestroyAllNavalUnitsOf();
    void Action_MindControlBaseOf();
    void Action_RestoreMindControlledBaseTo();
    void Action_CreateBuildingAt();
    void Action_RestoreStartingTechnosOf();
    void Action_ChronoScreenEffectFor();
    void Action_TeleportAllTo();
    void Action_SetSuperWeaponCharge();
    void Action_RestoreStartingBuildingsOf();
    void Action_FlashBuildingsOfType();
    void Action_SWSetRechargeTime();
    void Action_SWResetRechargeTime();
    void Action_SWReset();
    void Action_PreferredTargetCellSet();
    void Action_PreferredTargetCellClear();
    void Action_CenterBaseCellSet();
    void Action_CenterBaseCellClear();
    void Action_RadarBlackout();
    void Action_DefensiveTargetCellSet();
    void Action_DefensiveTargetCellClear();
    void Action_RetintRed();
    void Action_RetintGreen();
    void Action_RetintBlue();
    void Action_JumpCameraHome();

public:
    // ID 来自基类 AbstractTypeClass（内嵌 char[0x18]）。
    TAction ActionKind;
    int32 ActionIndex;
    int32 Waypoint;
    HouseClass* P1_House;
    TechnoTypeClass* P2_Object;
    int32 P3_Value;
    int32 P4_Value;
    int32 P5_Value;
    int32 P6_Value;
    int32 P7_Value;
    int32 P8_Value;
    TriggerTypeClass* Trigger;
    bool IsGlobal;

    // asm 的 swizzle 槽（+0x30/+0x4C/+0x50 的语义指针）。
    TeamTypeClass*    TeamType;      // +0x30 队伍类型指针
    TriggerTypeClass* TriggerType;   // +0x50 目标触发器类型指针
    TagTypeClass*     TagPtr;        // +0x34 Tag 指针

    // asm 内嵌名/文本槽（+0x54 / +0x6D）。
    char InlineName[0x18];
    char Text[0x20];

    // 在 Array 里的下标缓存（asm +0x24）。
    int32 idxInArray;
};