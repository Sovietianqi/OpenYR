#include "TeleportLocomotionClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Animations/AnimClass.h"
#include "../Rules/RulesClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/DamageArea.h"
#include "../Audio/VocClass.h"
#include "../Abstract/FootClass.h"

#include <cstdlib>
#include <cmath>

// ============================================================================
// TeleportLocomotionClass - 超时空/传送移位器。
//
// 权威来源：
//   asm: TeleportLocomotionClass_CTOR                      (asm:22)
//   asm: TeleportLocomotionClass_ILocomotion_718080        (asm:88)   Is_Moving
//   asm: TeleportLocomotionClass_ILocomotion_7180A0        (asm:116)  Destination
//   asm: TeleportLocomotionClass_ILocomotion_718100        (asm:183)  Move_To
//   asm: TeleportLocomotionClass_ILocomotion_718230        (asm:309)  Stop_Moving
//   asm: TeleportLocomotionClass_ILocomotion_7192C0        (asm:353)  Do_Turn
//   asm: TeleportLocomotionClass_ILocomotion_Process       (asm:390)  逐帧状态机
//   asm: TeleportLocomotionClass_ILocomotion_719E20        (asm:1468)
//   asm: TeleportLocomotionClass_ILocomotion_71A090        (asm:1571)
//
// 相位表（asm Process 的 switch，[esi+0x34]）：
//   0   起始/待命；由 IsBeingWarpedOut 或 Move_To 推入 1
//   1   蓄能：置 IsBeingWarpedOut，起 0x3C 帧计时
//   2   消失：WarpOut 动画 + ChronoOutSound，本体挪到目标点
//   3   冷却：写入 ChronoLock（此处落到 WarpOutTimer）
//   4   落位：把坐标写实到目标点
//   5   出现：ChronoInSound + WarpIn 动画 + 占用位复活
//   6   收尾等待
//   7   复位：清 IsWarpingOut / 相位 / 移动旗标
// ============================================================================

namespace {
// asm 里反复出现的 0x3C 帧蓄能/收尾时长。
constexpr int32 kChargeFrames = 0x3C;
} // namespace

TeleportLocomotionClass::TeleportLocomotionClass()
    : LocomotionClass()
    , IsTeleportingNow(false)
    , HasArrived(false)
    , WarpOutAnim(nullptr)
    , WarpInAnim(nullptr)
    , TargetCell(0, 0, 0)
    , WarpPhase(0)
    , WarpDelay(30)
    , TargetOnBridge(false)
{
    Speed = 0;
}

TeleportLocomotionClass::~TeleportLocomotionClass()
{
    WarpOutAnim = nullptr;
    WarpInAnim = nullptr;
}

HRESULT TeleportLocomotionClass::GetClassID(CLSID* pClassID)
{
    if (pClassID) {
        pClassID->Data1 = static_cast<uint32>(LocoID);
        pClassID->Data2 = 0;
        pClassID->Data3 = 0;
        for (int32 i = 0; i < 8; ++i) {
            pClassID->Data4[i] = 0;
        }
        return S_OK;
    }
    return E_FAIL;
}

int32 TeleportLocomotionClass::Size()
{
    return sizeof(TeleportLocomotionClass);
}

// ============================================================================
// Is_Moving - 传送中即为在动。
// asm: TeleportLocomotionClass_ILocomotion_718080   (asm:88)
//   `[+0x30] == 1` —— IsTeleportingNow 为真时返回真。
// ============================================================================

bool TeleportLocomotionClass::Is_Moving()
{
    return IsTeleportingNow;
}

// ============================================================================
// Destination - 传送中给目标点，否则给载体当前点。
// asm: TeleportLocomotionClass_ILocomotion_7180A0   (asm:116)
// ============================================================================

CoordStruct TeleportLocomotionClass::Destination()
{
    if (Is_Moving()) {
        return TargetCell;
    }
    return Owner ? Owner->CachedLocation : CoordStruct(0, 0, 0);
}

// ============================================================================
// Move_To - 发起一次传送。
// asm: TeleportLocomotionClass_ILocomotion_718100   (asm:183)
//   * 载体的四种"忙"状态任一命中 → 清 Owner 的传送相位槽并放弃；
//   * 若载体类型具备超时空能力（+0x1F8 与类型 0xF），目标格先做
//     ScatterContent（把占位内容强行赶开）；
//   * 交给 sub_718B70 登记传送；
//   * 若朝往点仍为零 → 通知载体中止任务；否则置 IsMoving 并把朝往点
//     抄进目标点。
// ============================================================================

void TeleportLocomotionClass::Move_To(CoordStruct to)
{
    if (!Owner) {
        return;
    }

    if (Owner->IsInLimbo || Owner->IsDead() || Owner->InOpenTopped) {
        Owner->WarpInTimer = 0;
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(to);

    // 目标格被占用：原版会对具备超时空能力的载具驱散占位内容；
    // 重构模型里 Occupied 表示不可落点，能驱散时先清掉占用标记。
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell && pCell->IsOccupied()) {
        MapClass::Instance->MarkCellOccupied(cell, false);
    }

    HeadTo = to;
    Dest = to;
    IsTeleportingNow = true;
    HasArrived = false;
    WarpPhase = 0;
    TeleportTimer.Start(60);
}

void TeleportLocomotionClass::TeleportTo(CoordStruct coord)
{
    if (IsTeleportingNow) {
        return;
    }

    if (!IsValidDestinationForCoord(coord)) {
        CoordStruct nearest = GetClosestOkCell(coord);
        if (nearest == CoordStruct(0, 0, 0)) {
            return;
        }
        coord = nearest;
    }

    Move_To(coord);
}

void TeleportLocomotionClass::ChronoWarpTo(CoordStruct coord)
{
    TeleportTo(coord);
}

// ============================================================================
// Stop_Moving - 取消传送。
// asm: TeleportLocomotionClass_ILocomotion_718230   (asm:309)
//   目标点归零、IsTeleportingNow 与 HasArrived 清掉。
// ============================================================================

void TeleportLocomotionClass::Stop_Moving()
{
    TargetCell = CoordStruct(0, 0, 0);
    HeadTo = CoordStruct(0, 0, 0);
    Dest = CoordStruct(0, 0, 0);
    IsTeleportingNow = false;
    HasArrived = false;
    TeleportTimer.Stop();
}

// ============================================================================
// Do_Turn - 传送移位把朝向交给载体的 FacingClass（Set_Desired 语义）。
// asm: TeleportLocomotionClass_ILocomotion_7192C0   (asm:353)
// ============================================================================

void TeleportLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        Owner->SetFacing(coord);
    }
}

// ============================================================================
// Process - 传送状态机主体。
// asm: TeleportLocomotionClass_ILocomotion_Process   (asm:390)
// ============================================================================

bool TeleportLocomotionClass::Process()
{
    if (!Owner) {
        return false;
    }

    // (1)(2) 外部传送相位同步：被传送仪接管时当帧不推进。
    if (HandleOuterWarp()) {
        return false;
    }

    // (3) 被时间武器抹除（+0x27C）：武装并进入蓄能相位。
    if (Owner->IsBeingWarpedOut()) {
        if (WarpPhase == 0) {
            BeginCharge();
        } else {
            ProcessPhase();
        }
        return false;
    }

    // (4) 已在相位推进中。
    if (WarpPhase > 0) {
        return ProcessPhase();
    }

    // (5) 起始段：必须有移动指令。
    if (!Is_Moving()) {
        return false;
    }

    const CoordStruct head = Head_To_Coord();

    // 朝往点为零或与当前点重合 → 收脚。
    if (head == CoordStruct(0, 0, 0) || head == Owner->CachedLocation) {
        Mark_All_Occupation_Bits(MarkType::Down);
        Stop_Moving();
        return false;
    }

    BeginWarpOut();
    return false;
}

// ============================================================================
// HandleOuterWarp - 外部传送相位同步。
// asm: TeleportLocomotionClass_ILocomotion_Process 开头 (asm:390 段)
//   * 载体 IsWarpingOut（+0x74）为真且本机无相位、外部相位槽（+0x280）为空
//     → 走影子矩阵分支，本机不推进；
//   * 本机无相位但外部相位槽非空 → 把本机相位同步为外部相位槽并当帧不推进。
// ============================================================================

bool TeleportLocomotionClass::HandleOuterWarp()
{
    if (!Owner) {
        return false;
    }

    if (Owner->IsWarpingOut() && WarpPhase == 0 && Owner->WarpInTimer == 0) {
        return true;
    }

    if (WarpPhase == 0 && Owner->WarpInTimer != 0) {
        WarpPhase = Owner->WarpInTimer;
        return true;
    }

    return false;
}

// ============================================================================
// BeginCharge - 蓄能相位入口（被抹除分支）。
// asm: Process 的 loc_7197CC：置载体的抹除旗标、起 0x3C 帧计时、相位 +1。
// ============================================================================

void TeleportLocomotionClass::BeginCharge()
{
    if (!Owner) {
        return;
    }

    Owner->IsBeingWarpedOutFlag = true;
    TeleportTimer.Start(kChargeFrames);
    ++WarpPhase;
}

// ============================================================================
// TargetCellOnBridge - 相位点所在格是否桥面。
// asm: Process 里对相位点格取 +0x140 的桥位（bit8）。
// ============================================================================

bool TeleportLocomotionClass::TargetCellOnBridge() const
{
    CellStruct cell = CoordMath::CoordToCell(TargetCell);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }
    return MapClass::Instance->IsBridgeCell(cell);
}

// ============================================================================
// BeginWarpOut - 起始段：登记目标点、起 WarpOut 动画、置传送旗标。
// asm: TeleportLocomotionClass_ILocomotion_Process 的 449..818 段
//   （含目标点距离换算出的计时长度、ChronoOutSound、抛物线滞空用的
//    WarpOut 动画，以及 IsWarpingOut 的置位）。
// ============================================================================

void TeleportLocomotionClass::BeginWarpOut()
{
    TargetCell = Head_To_Coord();
    if (TargetCell == CoordStruct(0, 0, 0)) {
        TargetCell = Dest;
    }

    // 计时长度按传送距离与 Rules->ChronoDistanceFactor 折算。
    const int32 distance = CurrentCoord.DistanceFrom(TargetCell);
    int32 frames = distance / 8;
    if (frames < kChargeFrames) {
        frames = kChargeFrames;
    }
    if (RulesClass::Instance->ChronoDistanceFactor > 0) {
        frames = distance / RulesClass::Instance->ChronoDistanceFactor;
        if (frames < kChargeFrames) {
            frames = kChargeFrames;
        }
    }
    TeleportTimer.Start(frames);

    // 立刻播放消失动画和音效；本体随后在相位 2 挪位。
    PlayWarpOutAnimation();
    PlayChronoOutSound();

    // 相位点若在桥面，记录桥位标记（asm 写回载体的 +0x8C）。
    TargetOnBridge = TargetCellOnBridge();

    Owner->IsWarpingOutFlag = true;
    Owner->IsBeingWarpedOutFlag = false;
    IsTeleportingNow = true;
    HasArrived = false;

    // 相位由 0 直接进入 2（1 是"武装/抹除"专用入口）。
    WarpPhase = 2;
}

// ============================================================================
// ProcessPhase - 相位 1..7 的分派。
// asm: TeleportLocomotionClass_ILocomotion_Process 的 719803..719BE2 段。
// ============================================================================

bool TeleportLocomotionClass::ProcessPhase()
{
    switch (WarpPhase) {
        case 1:
            return ProcessWarpOut();

        case 2:
            return ProcessWarpOut();

        case 3:
            return ProcessWarpTransition();

        case 4:
            return ProcessWarpIn();

        case 5:
            return ProcessWarpIn();

        case 6:
            return ProcessWarpIn();

        case 7:
            return FinishWarp();

        default:
            return false;
    }
}

// ============================================================================
// ProcessWarpOut - 相位 1..2：蓄能结束后把本体挪到目标点并放消失动画。
// asm: TeleportLocomotionClass_ILocomotion_Process 的 case 1/case 2。
// ============================================================================

bool TeleportLocomotionClass::ProcessWarpOut()
{
    TeleportTimer.Update();

    if (WarpPhase == 1) {
        if (!TeleportTimer.Expired()) {
            return true;
        }
        ++WarpPhase;
        return true;
    }

    // 相位 2：消失。
    PlayWarpOutAnimation();
    PlayChronoOutSound();

    Owner->IsWarpingOutFlag = true;
    Owner->IsBeingWarpedOutFlag = false;

    // 暗中把本体挪到目标点（原版在"隐形"期间完成位移）。
    CurrentCoord = TargetCell;
    Owner->SetCoords(TargetCell);

    Mark_All_Occupation_Bits(MarkType::Down);

    TeleportTimer.Start(kChargeFrames);
    ++WarpPhase;
    return true;
}

// ============================================================================
// ProcessWarpTransition - 相位 3..4：冷却写入 + 落位。
// asm: case 3（写 ChronoLockRemaining = Rules[+0xBEC]）、case 4
//   （载体设坐标到朝往点）。
// ============================================================================

bool TeleportLocomotionClass::ProcessWarpTransition()
{
    TeleportTimer.Update();

    if (WarpPhase == 3) {
        // 冷却：原版写 ChronoLockRemaining，这里落到载体的 WarpOutTimer。
        int32 lock = RulesClass::Instance->ChronoDistanceFactor;
        Owner->WarpOutTimer = (lock > 0) ? lock : kChargeFrames;

        if (TeleportTimer.Expired()) {
            ++WarpPhase;
        }
        return true;
    }

    // 相位 4：落位。
    CurrentCoord = TargetCell;
    Owner->SetCoords(TargetCell);
    ++WarpPhase;
    return true;
}

// ============================================================================
// ProcessWarpIn - 相位 5..6：出现动画 + 音效 + 占用位复活。
// asm: case 5（ChronoInSound / WarpIn 动画 / 占用位）与 case 6 收尾等待。
// ============================================================================

bool TeleportLocomotionClass::ProcessWarpIn()
{
    TeleportTimer.Update();

    if (WarpPhase == 5) {
        CurrentCoord = TargetCell;
        Owner->SetCoords(TargetCell);

        PlayChronoInSound();
        PlayWarpInAnimation();
        Mark_All_Occupation_Bits(MarkType::Up);

        TeleportTimer.Start(kChargeFrames);
        ++WarpPhase;
        return true;
    }

    // 相位 6：等动画收束。
    if (TeleportTimer.Expired()) {
        ++WarpPhase;
    }
    return true;
}

// ============================================================================
// FinishWarp - 相位 7：复位所有传送状态。
// asm: case 7（清 IsWarpingOut、清相位槽、清 IsTeleportingNow、清相位）。
// ============================================================================

bool TeleportLocomotionClass::FinishWarp()
{
    Owner->IsWarpingOutFlag = false;
    Owner->WarpInTimer = 0;

    IsTeleportingNow = false;
    HasArrived = true;
    WarpPhase = 0;
    TargetCell = CoordStruct(0, 0, 0);
    HeadTo = CoordStruct(0, 0, 0);
    Dest = CoordStruct(0, 0, 0);
    return false;
}

bool TeleportLocomotionClass::IsTeleporting() const
{
    return IsTeleportingNow;
}

int32 TeleportLocomotionClass::GetWarpPhase() const
{
    return WarpPhase;
}

// ============================================================================
// IsValidDestination - 目的地点是否可落（地形 + 占用）。
// ============================================================================

bool TeleportLocomotionClass::IsValidDestination() const
{
    return IsValidDestinationForCoord(TargetCell);
}

bool TeleportLocomotionClass::IsValidDestinationForCoord(CoordStruct coord) const
{
    CellStruct cell = CoordMath::CoordToCell(coord);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return false;
    }

    return true;
}

// ============================================================================
// RevertTeleport - 传送失败回退到出发前位置。
// ============================================================================

void TeleportLocomotionClass::RevertTeleport()
{
    IsTeleportingNow = false;
    HasArrived = false;
    WarpPhase = 0;
    TeleportTimer.Stop();

    if (Owner) {
        Owner->IsWarpingOutFlag = false;
        Owner->IsBeingWarpedOutFlag = false;
        CurrentCoord = Owner->CachedLocation;
        Owner->SetCoords(CurrentCoord);
        Mark_All_Occupation_Bits(MarkType::Up);
    }
}

CoordStruct TeleportLocomotionClass::GetOriginalPosition() const
{
    return CurrentCoord;
}

// ============================================================================
// PlayWarpOutAnimation - 在当前位置放消失动画（Rules->WarpOut）。
// asm: TeleportLocomotionClass_ILocomotion_Process 里的 AnimClass 构造
//   （WarpOut / WarpAway 两种动画源）。
// ============================================================================

void TeleportLocomotionClass::PlayWarpOutAnimation()
{
    AnimTypeClass* pAnim = WarpOutAnim;
    if (!pAnim) {
        pAnim = RulesClass::Instance->WarpOut;
    }
    if (!pAnim) {
        pAnim = RulesClass::Instance->WarpAway;
    }
    if (!pAnim || !Owner) {
        return;
    }

    AnimClass* anim = GameCreate<AnimClass>();
    if (anim) {
        anim->Type = pAnim;
        anim->SetCoords(Owner->CachedLocation);
    }
}

// ============================================================================
// PlayWarpInAnimation - 在目标点放出现动画。
// ============================================================================

void TeleportLocomotionClass::PlayWarpInAnimation()
{
    AnimTypeClass* pAnim = WarpInAnim;
    if (!pAnim) {
        pAnim = RulesClass::Instance->WarpAway;
    }
    if (!pAnim || !Owner) {
        return;
    }

    AnimClass* anim = GameCreate<AnimClass>();
    if (anim) {
        anim->Type = pAnim;
        anim->SetCoords(TargetCell);
    }
}

// ============================================================================
// PlayChronoOutSound / PlayChronoInSound - 消失 / 出现音效。
// asm: Process 中 Sound_Effect(ChronoOutSound) / Sound_Effect(ChronoInSound)，
//   音效编号优先取类型字段，回落到 Rules。
// ============================================================================

void TeleportLocomotionClass::PlayChronoOutSound()
{
    if (!Owner) {
        return;
    }
    int32 index = RulesClass::Instance->ChronoOutSound;
    if (index < 0) {
        return;
    }
    CoordStruct pos = Owner->CachedLocation;
    VocClass::PlayIndexAtPos(index, &pos);
}

void TeleportLocomotionClass::PlayChronoInSound()
{
    if (!Owner) {
        return;
    }
    int32 index = RulesClass::Instance->ChronoInSound;
    if (index < 0) {
        return;
    }
    CoordStruct pos = TargetCell;
    VocClass::PlayIndexAtPos(index, &pos);
}

// ============================================================================
// CheckChronoVortex / SpawnChronoVortex - 传送涡流（原版按 ChronoTrigger 规则
//   概率生成，可摧毁单位与附近目标）。
// ============================================================================

void TeleportLocomotionClass::CheckChronoVortex()
{
    if (!Owner) {
        return;
    }

    int32 vortexChance = RulesClass::Instance->GetChronoVortexChance();
    if (vortexChance <= 0) {
        return;
    }

    int32 roll = rand() % 100;
    if (roll < vortexChance) {
        SpawnChronoVortex();
    }
}

void TeleportLocomotionClass::SpawnChronoVortex()
{
    if (!Owner) {
        return;
    }

    CoordStruct vortexPos = TargetCell;

    int32 damage = RulesClass::Instance->GetChronoVortexDamage();
    int32 radius = RulesClass::Instance->GetChronoVortexRadius();

    DamageArea damageArea;
    damageArea.X = vortexPos.X;
    damageArea.Y = vortexPos.Y;
    damageArea.Z = vortexPos.Z;
    damageArea.Damage = damage;
    damageArea.Range = radius;
    damageArea.Warhead = WarheadTypeClass::GetDefault();

    MapClass::Instance->ApplyDamageArea(damageArea);

    Owner->TakeDamage(damage, nullptr, WarheadTypeClass::GetDefault());

    IsTeleportingNow = false;
    IsMoving = false;
    HasArrived = false;
    WarpPhase = 0;
}

// ============================================================================
// Mark_All_Occupation_Bits - 传送期间不占格；否则按脚印标记当前格。
// asm: Process 里换格时的占用位切换。
// ============================================================================

void TeleportLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    bool occupy = (mark == MarkType::Up);
    MapClass::Instance->MarkCellOccupied(cell, occupy);
}

void TeleportLocomotionClass::Limbo()
{
    IsTeleportingNow = false;
    IsMoving = false;
    HasArrived = false;
    WarpPhase = 0;
    TeleportTimer.Stop();
    Mark_All_Occupation_Bits(MarkType::Down);
}

int32 TeleportLocomotionClass::Get_Status() const
{
    if (IsTeleportingNow) {
        if (WarpPhase < 3) {
            return static_cast<int32>(Sequence::Deploy);
        }
        if (WarpPhase >= 5) {
            return static_cast<int32>(Sequence::Undeploy);
        }
        return static_cast<int32>(Sequence::Ready);
    }
    return static_cast<int32>(Sequence::Ready);
}

FireError TeleportLocomotionClass::Can_Fire() const
{
    if (IsTeleportingNow) {
        return FireError::Movement;
    }
    return FireError::OK;
}

bool TeleportLocomotionClass::Is_Really_Moving_Now() const
{
    return IsTeleportingNow;
}

// 传送面板的默认登记点（复位槽位写回该值）。
static CoordStruct TeleportLocomotionClass_Default_Pos(0, 0, 0);

bool TeleportLocomotionClass::ILocomotion_718080() const
{
    // asm: TeleportLocomotionClass_ILocomotion_718080   (asm:88)
    //   只认 `[+0x30] == 1`。
    return IsTeleportingNow;
}

void TeleportLocomotionClass::ILocomotion_7180A0(CoordStruct* pOut) const
{
    // asm: TeleportLocomotionClass_ILocomotion_7180A0   (asm:116)
    //   传送中给目标点，否则给载体现行坐标。
    if (!pOut)
        return;
    if (IsTeleportingNow)
        *pOut = TargetCell;
    else if (LinkedTo)
        LinkedTo->GetCoords(pOut);
    else
        *pOut = TeleportLocomotionClass_Default_Pos;
}

void TeleportLocomotionClass::ILocomotion_718100(const CoordStruct& to)
{
    // asm: TeleportLocomotionClass_ILocomotion_718100   (asm:183)
    //   载体忙/封闭则放弃；否则登记目标点并归零相位。
    if (!LinkedTo)
        return;
    if (LinkedTo->IsInLimbo || LinkedTo->IsDead() || LinkedTo->InOpenTopped)
        return;

    TargetCell = to;
    HeadTo = to;
    IsTeleportingNow = true;
    WarpPhase = 0;
}

void TeleportLocomotionClass::ILocomotion_718230()
{
    // asm: TeleportLocomotionClass_ILocomotion_718230   (asm:309)
    TargetCell = TeleportLocomotionClass_Default_Pos;
    HeadTo = TeleportLocomotionClass_Default_Pos;
    IsTeleportingNow = false;
    HasArrived = false;
}

void TeleportLocomotionClass::ILocomotion_7192C0(DirStruct dir)
{
    // asm: TeleportLocomotionClass_ILocomotion_7192C0   (asm:353)
    if (LinkedTo)
        LinkedTo->SetFacing(dir);
}

int32 TeleportLocomotionClass::ILocomotion_719E20() const
{
    // asm: TeleportLocomotionClass_ILocomotion_719E20   (asm:1468)
    return 2;
}

void TeleportLocomotionClass::ILocomotion_71A090(CoordStruct* pCoord)
{
    // asm: TeleportLocomotionClass_ILocomotion_71A090   (asm:1571)
    if (pCoord)
        return;
    if (LinkedTo)
        LinkedTo->SetLocation(CurrentCoord);
}

HRESULT TeleportLocomotionClass::ILocomotion_71A160(REFIID iid, void** ppvObject)
{
    return QueryInterface(iid, ppvObject);
}

ULONG TeleportLocomotionClass::ILocomotion_71A170()
{
    return AddRef();
}

ULONG TeleportLocomotionClass::ILocomotion_71A180()
{
    return Release();
}
