#include "WalkLocomotionClass.h"

#include <cstdio>
#include <cstdlib>
#include "../Map/MapClass.h"
#include "../AI/TagClass.h"
#include "../Map/CellClass.h"
#include "../Rules/RulesClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Abstract/FootClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// WalkLocomotionClass - 步兵行走移位器。
//
// 权威来源：
//   asm: WalkLocomotionClass_CTOR                        (asm:23)
//   asm: WalkLocomotionClass_ILocomotion_Destination      (asm:168)
//   asm: WalkLocomotionClass_ILocomotion_HeadToCoord      (asm:232)
//   asm: WalkLocomotionClass_ILocomotion_Process          (asm:310)
//   asm: WalkLocomotionClass_ILocomotion_MoveTo           (asm:348)
//   asm: WalkLocomotionClass_ILocomotion_StopMoving       (asm:454)
//   asm: WalkLocomotionClass_ILocomotion_DoTurn           (asm:514)
//   asm: WalkLocomotionClass_ILocomotion_ForceImmediateDestination (asm:549)
//   asm: WalkLocomotionClass_ILocomotion_MarkAllOccupationBits (asm:1256)
//   asm: WalkLocomotionClass_ILocomotion_IsMovingHere     (asm:1319)
//   asm: WalkLocomotionClass_ILocomotion_IsReallyMovingNow(asm:1418)
//   asm: WalkLocomotionClass_ILocomotion_Lock             (asm:1446)
//   asm: sub_75AEC0  —— 每帧行走仿真（asm 主库行 1747664..1749553）
//   asm: sub_75C240  —— Force_Immediate_Destination 的落点助手（asm:1749562）
//
// 对象布局（相对 WalkLocomotionClass 基址）：
//   +0x18 Destination      朝往目的地
//   +0x24 HeadTo           本帧真正推进到的坐标
//   +0x30 IsMoving         移动旗标
//   +0x31 处理中临时旗标
//   +0x32 IsReallyMovingNow
//   +0x34/0x35/0x36 逐帧布尔（坡道 / 落地 / 挤压）
//   +0x38 Piggyback        IPiggyback 的 ILocomotion* 槽
// ============================================================================

namespace {
// asm: WalkLocomotionClass_Default_Pos —— 全零哨兵坐标，代表"无目标"。
inline bool IsDefaultPos(const CoordStruct& c)
{
    return c.X == 0 && c.Y == 0 && c.Z == 0;
}

// asm: WalkLocomotionClass_ILocomotion_IsMovingNow 里 dword_B45C2C(=128) 的
// 高度容差：同格判定时 Z 之差不超过它即视为"就在此地"。
constexpr int32 kCellZTolerance = 128;
constexpr int32 kLeptonPerCell = 256;
} // namespace

WalkLocomotionClass::WalkLocomotionClass()
    : LocomotionClass()
    , CurrentCell(0, 0)
    , NextCell(0, 0)
    , PathIndex(0)
    , PathLength(0)
    , IsFalling(false)
    , IsJumping(false)
    , IsCrawling(false)
    , MoveStepTimer(0)
    , StepSize(8)
    , HeadTo(0, 0, 0)
    , IsReallyMovingNow(false)
    , OnRamp(false)
    , JustLanded(false)
    , WasCrushed(false)
    , AltBlockedCell(0, 0)
    , AltResultCell(0, 0)
    , AltCooldown(0)
{
    Speed = 64;
}

WalkLocomotionClass::~WalkLocomotionClass()
{
    Path.Clear();
}

HRESULT WalkLocomotionClass::GetClassID(CLSID* pClassID)
{
    // asm: 判空失败返回 80004003h (E_POINTER)，否则把 16 字节 GUID 原样拷出。
    if (pClassID == nullptr)
        return E_POINTER;

    *pClassID = CLSIDs::Get(LocoID);
    return S_OK;
}

int32 WalkLocomotionClass::Size()
{
    return sizeof(WalkLocomotionClass);
}

// ============================================================================
// Move_To - 登记朝往点并（按需）生成逐格路径。
// asm: WalkLocomotionClass_ILocomotion_MoveTo   (asm:348)
//   * 载体正处于"不可下达移动命令"的三种状态时直接返回（asm 里三次虚表
//     调用 +0x37C/+0x1D4/+0x1D8）。
//   * 目标非零：写 Destination，取目标格；若目标格带桥梁位（cell+0x140 的
//     第 9 位），给目的地 Z 加上 ScenarioClass::Random(0x3C) 的抖动。
//     最后置 IsMoving。
//   * 目标为零：清 IsMoving；若原先在动，则通知载体停止移动（虚表+0x54C）。
// ============================================================================

void WalkLocomotionClass::Move_To(CoordStruct to)
{
    if (Owner) {
        if (Owner->IsInLimbo || Owner->InOpenTopped || Owner->IsDead()) {
            return;
        }
    }

    // ★ 同一目标不重复寻路。
    //   任务层是每帧重下 Set_Destination 的（Mission_Attack / Mission_Hunt
    //   每个任务步都会调一次）。A* 的代价是几十到几千个节点，若每次 Move_To
    //   都重算，几十个步兵就能把一帧拖到几十毫秒（实测 2400 帧跑了 6 分钟
    //   还没结束）。目标未变且路径还在就什么都不做 —— 语义与"重复下达同一
    //   移动命令"一致。
    //   ★ 判据用"目标**格**"而不是"目标坐标"。
    //   追击类任务（Mission_Attack / Mission_Hunt）每帧拿目标的实时坐标重下
    //   命令，坐标每个像素都在变，逐字节比较永远不等 → 每帧重跑一次 A*。
    //   而路径本身就是逐格的，同一格内的坐标漂移不改变寻路结果：按格比较
    //   既保持语义，又把"每帧一次 A*"降成"每换一格一次"。
    if (IsMoving && !IsDefaultPos(to) && PathLength > 0
        && PathIndex < PathLength
        && CoordMath::CoordToCell(Dest).X == CoordMath::CoordToCell(to).X
        && CoordMath::CoordToCell(Dest).Y == CoordMath::CoordToCell(to).Y) {
        Dest = to;                 // 同步最新坐标，供到达判定用
        return;
    }

    Dest = to;

    if (IsDefaultPos(to)) {
        // 停机分支：目标归零。
        const bool wasMoving = IsMoving;
        IsMoving = false;
        HeadTo = CoordStruct(0, 0, 0);
        Path.Clear();
        PathIndex = 0;
        PathLength = 0;
        if (wasMoving) {
            Stop_Movement_Animation();
        }
        return;
    }

    HeadTo = to;

    // ★ 目标格不可入、且单位已经贴着它：不再重复寻路，直接进收脚/让位分支。
    //
    //   任务层每帧都会重下 Set_Destination。收脚后 IsMoving 为 false，上面
    //   那条"同一目标不重复寻路"的短路不再命中，于是每帧重跑一次
    //   FootClass::Build_Path —— 终点不可达时那是 8000 节点的 A*（实测
    //   900 帧从 31 秒涨到 59 秒）。这里把"贴着不可入的目标格"当作等价的
    //   到达状态：只登记朝往点、清空路径，让 Process 走收脚 / 让位分支。
    //   目标格一旦空出来，Can_Enter_Cell 变真，本短路自动失效。
    if (Owner != nullptr) {
        const CellStruct hereCell = CoordMath::CoordToCell(CurrentCoord);
        const CellStruct destCell = CoordMath::CoordToCell(to);
        if ((destCell.X != hereCell.X || destCell.Y != hereCell.Y)
            && CoordMath::CellDistance(hereCell, destCell) <= 1
            && !Owner->Can_Enter_Cell(destCell))
        {
            IsMoving = false;
            IsReallyMovingNow = false;
            Path.Clear();
            PathIndex = 0;
            PathLength = 0;
            return;
        }
    }

    // 生成逐格路径。
    //   ★ 优先用占用感知的 A*（FootClass::Build_Path）：它会绕开水面/建筑/
    //   已站满的格子，走到"终点周围最近的可站格"。此前这里是"起点到终点的
    //   直线补全"，完全不查通行性 —— 6 名步兵会一路走进敌方单位所在的格子
    //   叠成同一个坐标点，同时能开火的只有最前面一个。A* 失败（不可达）时
    //   再退回直线补全，保证不会因为寻路失败而完全不移动。
    Path.Clear();
    PathIndex = 0;
    PathLength = 0;

    CellStruct startCell = CoordMath::CoordToCell(CurrentCoord);
    CellStruct destCell = CoordMath::CoordToCell(to);

    CurrentCell = startCell;
    NextCell = destCell;

    bool routed = false;
    if (Owner != nullptr)
    {
        DynamicVectorClass<CellStruct> cells;
        const bool ok = Owner->Build_Path(destCell, cells);
        if (ok && cells.GetCount() > 0)
        {
            // 跳过起点格（步兵已经在那一格上）。
            for (int32 i = 1; i < cells.GetCount(); ++i)
                Path.Add(cells[i]);
            PathLength = Path.GetCount();
            if (PathLength > 0)
            {
                NextCell = Path[0];
                routed = true;
            }
        }
    }

    if (!routed)
    {
        int32 dx = static_cast<int32>(destCell.X) - static_cast<int32>(startCell.X);
        int32 dy = static_cast<int32>(destCell.Y) - static_cast<int32>(startCell.Y);
        const int32 adx = std::abs(dx);
        const int32 ady = std::abs(dy);
        const int32 steps = (adx > ady) ? adx : ady;

        if (steps > 0) {
            for (int32 i = 1; i <= steps; ++i) {
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(startCell.X)
                    + (dx * i) / steps);
                cs.Y = static_cast<int16>(static_cast<int32>(startCell.Y)
                    + (dy * i) / steps);
                Path.Add(cs);
            }
            PathLength = Path.GetCount();
            if (PathLength > 0) {
                NextCell = Path[0];
            }
        }
    }

    // asm: MapClass_Get_Target_Cell -> cell+0x140 的桥位（bit8）会触发
    //   `Random(0x3C)` 的高度抖动，避免多名步兵在同一桥格完全重叠。
    CellClass* pCell = MapClass::Instance->GetCellAt(destCell);
    if (pCell && pCell->IsBridge()) {
        int32 jitter = ScenarioClass::Instance->Random.Next(0, 0x3C);
        Dest.Z += jitter;
        HeadTo.Z += jitter;
    }

    IsMoving = true;
    IsFalling = false;
    SpeedAccum = 0;
}

void WalkLocomotionClass::Move_To(AbstractClass* target)
{
    if (!target) {
        return;
    }
    Move_To(target->GetCoords());
}

// ============================================================================
// Stop_Moving - 清空目的地与朝往点。
// asm: WalkLocomotionClass_ILocomotion_StopMoving   (asm:454)
//   先写 Destination = 0；然后仅当 HeadTo 也归零时，才清 IsMoving /
//   IsReallyMovingNow 并调用载体的虚表 +0x54C（停止移动动画）。
// ============================================================================

void WalkLocomotionClass::Stop_Moving()
{
    Dest = CoordStruct(0, 0, 0);

    if (!IsDefaultPos(HeadTo)) {
        return;
    }

    IsMoving = false;
    IsReallyMovingNow = false;
    IsFalling = false;
    IsJumping = false;
    Path.Clear();
    PathIndex = 0;
    PathLength = 0;
    MoveStepTimer = 0;
    Stop_Movement_Animation();
}

// ============================================================================
// Do_Turn - 直接把朝向写入载体的 FacingStruct。
// asm: WalkLocomotionClass_ILocomotion_DoTurn   (asm:514)
//   步兵不做限速转身：一次调用即到位（FacingStruct_SetFacing 语义）。
// ============================================================================

void WalkLocomotionClass::Do_Turn(DirStruct coord)
{
    if (!Owner) {
        return;
    }

    if (Owner->GetFacing().Value != coord.Value) {
        Owner->SetFacing(coord);
    }
}

// ============================================================================
// Head_To_Coord - 朝往点。
// asm: WalkLocomotionClass_ILocomotion_HeadToCoord   (asm:232)
//   HeadTo 非零 → 返回 HeadTo；否则返回载体的缓存绘制点（+0x9C）。
// ============================================================================

CoordStruct WalkLocomotionClass::Head_To_Coord() const
{
    if (!IsDefaultPos(HeadTo)) {
        return HeadTo;
    }
    return Owner ? Owner->CachedLocation : CoordStruct(0, 0, 0);
}

CoordStruct WalkLocomotionClass::Head_To_Coord()
{
    return static_cast<const WalkLocomotionClass*>(this)->Head_To_Coord();
}

// ============================================================================
// Destination - 只有正在移动时才回传目的地。
// asm: WalkLocomotionClass_ILocomotion_Destination   (asm:168)
//   Is_Moving() 为假 → 回传全零哨兵（而非上一次的残留目的地）。
// ============================================================================

CoordStruct WalkLocomotionClass::Destination()
{
    if (Is_Moving()) {
        return Dest;
    }
    return CoordStruct(0, 0, 0);
}

// ============================================================================
// Lock - 把目的地与朝往点一起归零。
// asm: WalkLocomotionClass_ILocomotion_Lock   (asm:1446)
// ============================================================================

void WalkLocomotionClass::Lock()
{
    Dest = CoordStruct(0, 0, 0);
    HeadTo = CoordStruct(0, 0, 0);
    IsMoving = false;
    IsReallyMovingNow = false;
    Path.Clear();
    PathIndex = 0;
    PathLength = 0;
}

// ============================================================================
// Force_Immediate_Destination - 立即落点。
// asm: WalkLocomotionClass_ILocomotion_ForceImmediateDestination   (asm:549)
//   先把传入点交给 sub_75C240（行进落点助手）登记；随后若 HeadTo 与
//   Destination 同时归零，则清 IsMoving 且在原先移动时通知载体停手。
// ============================================================================

void WalkLocomotionClass::Force_Immediate_Destination(CoordStruct coord)
{
    const bool wasMoving = IsMoving;

    HeadTo = coord;
    Dest = coord;

    if (IsDefaultPos(HeadTo) && IsDefaultPos(Dest)) {
        IsMoving = false;
        IsReallyMovingNow = false;
        if (wasMoving) {
            Stop_Movement_Animation();
        }
    }
}

// ============================================================================
// Is_Moving_Here - 目标是否与给定坐标同格且高度接近。
// asm: WalkLocomotionClass_ILocomotion_IsMovingHere   (asm:1319)
//   取朝往点，比较 (X>>8, Y>>8) 与目标同格，且 |Z 差| <= 容差。
// ============================================================================

bool WalkLocomotionClass::Is_Moving_Here(CoordStruct to)
{
    CoordStruct head = Head_To_Coord();

    const int16 hx = static_cast<int16>(head.X >> 8);
    const int16 hy = static_cast<int16>(head.Y >> 8);
    const int16 tx = static_cast<int16>(to.X >> 8);
    const int16 ty = static_cast<int16>(to.Y >> 8);

    if (hx != tx || hy != ty) {
        return false;
    }

    return std::abs(head.Z - to.Z) <= kCellZTolerance;
}

// ============================================================================
// Stop_Movement_Animation - 停止行走动画。
// asm: WalkLocomotionClass_ILocomotion_StopMovementAction   (asm:1577)
// ============================================================================

void WalkLocomotionClass::Stop_Movement_Animation()
{
    if (Owner) {
        Owner->SetSequence(Sequence::Ready);
    }
}

// ============================================================================
// Process - 每帧行走推进入口。
// asm: WalkLocomotionClass_ILocomotion_Process   (asm:310)
//   置"处理中"旗标（+0x31 = 1），调用 sub_75AEC0 做一帧仿真，清旗标后再
//   回传 Is_Moving()。sub_75AEC0 的控制流按三块组织：
//     1) 瘫痪/速度闸门：载体 SpeedPercentage(+0x578) 为 0 则整帧冻结；
//     2) 到达判定：到朝往点距离小于 Rules->CloseEnough 即收脚；
//     3) 逐格推进：按速度累积满 256 个单位推进一格，写回坐标/朝向，
//        坡道做 Z 插值，按节拍播放脚步音，并刷新占用位。
// ============================================================================

bool WalkLocomotionClass::Process()
{
    if (std::getenv("OPENYR_CHAIN_DEBUG") != nullptr)
    {
        static int s_dumped = 0;
        if (s_dumped < 2)
        {
            ++s_dumped;
            const CoordStruct oc = (Owner != nullptr) ? Owner->GetCoords() : CoordStruct(0,0,0);
            std::fprintf(stderr,
                "[CHAIN] walk#%d moving=%d pathIdx=%d pathLen=%d cur=(%d,%d,%d) own=(%d,%d,%d) dest=(%d,%d) fall=%d jump=%d spdPct=%.3f\n",
                s_dumped, (int)IsMoving, (int)PathIndex, (int)PathLength,
                CurrentCoord.X, CurrentCoord.Y, CurrentCoord.Z,
                oc.X, oc.Y, oc.Z, Dest.X, Dest.Y,
                (int)IsFalling, (int)IsJumping,
                (Owner != nullptr) ? Owner->SpeedPercentage : -1.0);
        }
    }

    // asm: sub_75AEC0 开头 —— HeadTo 与 Destination 同时归零则直接返回。
    if (!IsMoving && IsDefaultPos(HeadTo) && IsDefaultPos(Dest)) {
        IsReallyMovingNow = false;
        return false;
    }

    if (!Owner) {
        IsReallyMovingNow = false;
        return false;
    }

    // 替代格缓存的退避窗口按帧递减（每个载体每帧一次）。
    if (AltCooldown > 0) {
        --AltCooldown;
    }

    // 先跑空格落逻辑（下坠 / 跳跃期间不走地面步进）。
    if (IsFalling) {
        return ProcessFall();
    }
    if (IsJumping) {
        return ProcessJump();
    }

    // asm: 载体 SpeedPercentage(+0x578) 为 0 → 被定身，本帧不推进。
    if (Owner->SpeedPercentage <= 0.0) {
        IsReallyMovingNow = false;
        return true;
    }

    CoordStruct target = !IsDefaultPos(HeadTo) ? HeadTo : Dest;
    if (IsDefaultPos(target)) {
        IsReallyMovingNow = false;
        IsMoving = false;
        return false;
    }

    // ---- 目标格站满：就地落位收脚 ----------------------------------------
    //   目标格已被 3 名步兵站满（或被建筑 / 载具占住）时，逐格步进永远走不
    //   进去：MoveOneStep 每帧找替代格 → 又回到本格，表现就是原地抖动、
    //   动画一直播走路。这里在"已经贴到目标格（切比雪夫距离 <= 1）"时就
    //   结束这次移动：先把 Owner 让到 BFS 找到的最近可站格并登记占用，再
    //   走正常收尾（IsMoving = false 等）。
    //
    //   与"已经在目标格内"不冲突：这里要求 targetCell != hereCell，已站在
    //   目标格里的情况仍由下面的 IsAtDestination 分支处理。
    const CellStruct hereCell   = CoordMath::CoordToCell(CurrentCoord);
    const CellStruct targetCell = CoordMath::CoordToCell(target);

    if ((targetCell.X != hereCell.X || targetCell.Y != hereCell.Y)
        && CoordMath::CellDistance(hereCell, targetCell) <= 1
        && !Owner->Can_Enter_Cell(targetCell))
    {
        const CellStruct altCell = FindAlternativeCell(targetCell);
        if ((altCell.X != 0 || altCell.Y != 0)
            && (altCell.X != hereCell.X || altCell.Y != hereCell.Y))
        {
            CoordStruct loc = CoordMath::CellToCoord(altCell);
            loc.Z = MapClass::Instance->GetGroundHeight(loc);

            CellClass* pOldCell = MapClass::Instance->GetCellAt(hereCell);
            CellClass* pNewCell = MapClass::Instance->GetCellAt(altCell);
            if (pOldCell != nullptr && pOldCell != pNewCell)
                pOldCell->Remove_Occupier(Owner);
            if (pNewCell != nullptr)
                pNewCell->Add_Occupier(Owner);

            CurrentCoord = loc;
            CurrentCell  = altCell;
            NextCell     = altCell;
            Owner->SetCoords(CurrentCoord);
        }

        IsMoving = false;
        IsReallyMovingNow = false;
        HeadTo = CoordStruct(0, 0, 0);
        Dest   = CoordStruct(0, 0, 0);
        Path.Clear();
        PathIndex = 0;
        PathLength = 0;
        Owner->SetSequence(Sequence::Ready);
        return false;
    }

    // ---- 到达判定（asm: RulesClass CloseEnough 比较） ----
    //   ★ 吸附到目标点之前必须先确认"那一格还能站"。
    //   IsAtDestination 的判据是距离 <= CloseEnough，可能落在下一格的中心；
    //   此前这里直接 `CurrentCoord = target` 跳过逐格步进，于是①绕过了
    //   CanMoveToCell 的占用检查（5 名步兵照样压进同一格）②换格了却不更新
    //   占用链（后来者看到的仍是空格）。目标格站满时改为继续走逐格步进，
    //   由 MoveOneStep 选择替代格。
    if (IsAtDestination()) {
        if ((targetCell.X != hereCell.X || targetCell.Y != hereCell.Y)
            && !Owner->Can_Enter_Cell(targetCell))
        {
            // 目标格已满/不可入：不吸附，落到下面的逐格步进。
        }
        else
        {
            if (targetCell.X != hereCell.X || targetCell.Y != hereCell.Y)
            {
                CellClass* pOldCell = MapClass::Instance->GetCellAt(hereCell);
                CellClass* pNewCell = MapClass::Instance->GetCellAt(targetCell);
                if (pOldCell != nullptr && pOldCell != pNewCell)
                    pOldCell->Remove_Occupier(Owner);
                if (pNewCell != nullptr)
                    pNewCell->Add_Occupier(Owner);
            }

            // 收脚：贴合目标点，停止移动。
            CurrentCoord = target;
            Owner->SetCoords(CurrentCoord);
            IsMoving = false;
            IsReallyMovingNow = false;
            HeadTo = CoordStruct(0, 0, 0);
            Dest = CoordStruct(0, 0, 0);
            Path.Clear();
            PathIndex = 0;
            PathLength = 0;
            Owner->SetSequence(Sequence::Ready);
            return false;
        }
    }

    // ---- 朝向朝往点（步兵转身不设限速） ----
    Do_Turn(CoordMath::DirectionTo(CurrentCoord, target));

    // ---- 逐格推进 ----
    const int32 effectiveSpeed = GetCurrentSpeed();
    SpeedAccum += (effectiveSpeed > 0) ? effectiveSpeed : 1;

    IsReallyMovingNow = false;
    while (SpeedAccum >= kLeptonPerCell) {
        SpeedAccum -= kLeptonPerCell;

        if (!MoveOneStep()) {
            IsMoving = false;
            IsReallyMovingNow = false;
            Owner->SetSequence(Sequence::Ready);
            return false;
        }
        IsReallyMovingNow = true;
    }

    UpdatePosition();

    if (IsMoving && IsReallyMovingNow) {
        Owner->SetSequence(IsCrawling ? Sequence::Crawl : Sequence::Walk);
        PlayStepSound();
    }

    return IsMoving;
}

// ============================================================================
// GetCurrentSpeed - 本帧生效速度。
//   载体侧把老兵加成与 SpeedPercentage 折进来；这里再乘本类的 Speed。
// ============================================================================

int32 WalkLocomotionClass::GetCurrentSpeed() const
{
    int32 base = Owner ? Owner->GetCurrentSpeed() : Speed;
    if (base <= 0) {
        base = Speed;
    }
    if (base <= 0) {
        base = 1;
    }
    return base;
}

// ============================================================================
// UpdatePosition - 沿当前格朝下一格做逐帧插值，坡道/楼梯做高度过渡。
// asm: sub_75AEC0 的坐标写回段（对 [esi+0x0C] 载体调用 +0x0F0 设坐标）。
// ============================================================================

void WalkLocomotionClass::UpdatePosition()
{
    if (!Owner) {
        return;
    }

    CellStruct curCell = CoordMath::CoordToCell(CurrentCoord);
    CellStruct aimCell = (PathIndex < PathLength) ? Path[PathIndex] : NextCell;
    CoordStruct cellCenter = CoordMath::CellToCoord(aimCell);

    // 水平插值：按速度向前推进，不允许越过格中心。
    const int32 speed = GetCurrentSpeed();
    int32 dx = cellCenter.X - CurrentCoord.X;
    int32 dy = cellCenter.Y - CurrentCoord.Y;
    const int32 dist = static_cast<int32>(std::sqrt(
        static_cast<double>(dx) * dx + static_cast<double>(dy) * dy));

    if (dist > 0) {
        if (dist <= speed) {
            CurrentCoord.X = cellCenter.X;
            CurrentCoord.Y = cellCenter.Y;
        } else {
            CurrentCoord.X += (dx * speed) / dist;
            CurrentCoord.Y += (dy * speed) / dist;
        }
    }

    // 高度：目标格地面高度做逐帧逼近（上坡抬升 / 下坡下沉）。
    CellClass* pAim = MapClass::Instance->GetCellAt(aimCell);
    if (pAim) {
        OnRamp = InterpolateHeight(pAim->Get_Ground_Height());
    }

    Owner->SetCoords(CurrentCoord);

    // 进入新格 → [CellTags]（asm: FootClass_UpdatePosition 的格判定段）。
    const bool enteredNewCell =
        (curCell.X != CurrentCell.X || curCell.Y != CurrentCell.Y);
    CurrentCell = curCell;
    if (enteredNewCell) {
        TagClass::SpringCellTag(curCell, Owner);
    }
}

// ============================================================================
// InterpolateHeight - 坡道 / 楼梯高度插值。
// asm: sub_75AEC0 的高度段——每帧把 Z 朝目标格地面高度推进固定步长 8，
//   差值在一个步长内则直接贴合，并把"刚落地"旗标（+0x35）置上。
//   返回本帧是否存在高度差（即是否在坡道上）。
// ============================================================================

bool WalkLocomotionClass::InterpolateHeight(int32 targetZ)
{
    const int32 dz = targetZ - CurrentCoord.Z;
    if (dz == 0) {
        return false;
    }

    const int32 step = StepSize > 0 ? StepSize : 8;
    if (std::abs(dz) <= step) {
        CurrentCoord.Z = targetZ;
        JustLanded = true;
    } else {
        CurrentCoord.Z += (dz > 0) ? step : -step;
    }
    return true;
}

// ============================================================================
// IsAtDestination - 到达判定（距离 <= Rules->CloseEnough）。
// asm: sub_75AEC0 的收脚判定段。
// ============================================================================

bool WalkLocomotionClass::IsAtDestination() const
{
    CoordStruct target = !IsDefaultPos(HeadTo) ? HeadTo : Dest;
    if (IsDefaultPos(target)) {
        return false;
    }
    const int32 dist = CurrentCoord.DistanceFrom(target);
    int32 closeEnough = RulesClass::Instance->GetCloseEnoughSpeed();
    if (closeEnough <= 0) {
        closeEnough = 1;
    }
    return dist <= closeEnough;
}

// ============================================================================
// MoveOneStep - 推进到路径中的下一格；被占用时改走替代格。
// asm: sub_75AEC0 的逐格步进段（含 MapClass 的通行/占用判定）。
// ============================================================================

bool WalkLocomotionClass::MoveOneStep()
{
    if (PathIndex >= PathLength) {
        // 没有寻路结果（步兵的 Set_Destination 走的是"直线 Move_To"，
        // 不会填 Path）。这里退化成"朝目标格推进一格"，让 UpdatePosition
        // 每帧能插值过去；否则 MoveOneStep 恒 false，IsMoving 每帧被清，
        // 步兵接到移动/攻击命令后原地踏步。
        const CoordStruct target = !IsDefaultPos(HeadTo) ? HeadTo : Dest;
        if (IsDefaultPos(target)) {
            return false;
        }
        NextCell = CoordMath::CoordToCell(target);
        if (CurrentCell.X == NextCell.X && CurrentCell.Y == NextCell.Y) {
            // 已经在目标格里、但坐标没贴合落点：这里返回 true 会让
            // Process 每帧都把 IsMoving 置回真、循环播走路动画而坐标不动
            // （实测 5 名 E1 这样空转）。逐格移动的粒度本来就是"格"，
            // 进了目标格就算走到头 —— 返回 false 让 Process 收脚。
            return false;
        }
        // 退化步进也要过占用检查：目标格站满时找替代格，别硬挤进去。
        if (!CanMoveToCell(NextCell)) {
            const CellStruct altCell = FindAlternativeCell(NextCell);
            if (altCell.X == 0 && altCell.Y == 0) {
                return false;               // 半径 4 内没有能站的格子
            }
            if (altCell.X == CurrentCell.X && altCell.Y == CurrentCell.Y) {
                // 最近的可站格就是脚下这一格：再走也是原地，直接收脚。
                return false;
            }
            NextCell = altCell;
        }
        // 与寻路分支保持一致：换格时同步占用登记。
        if (Owner) {
            CellClass* pOldCell = MapClass::Instance->GetCellAt(CurrentCell);
            CellClass* pNewCell = MapClass::Instance->GetCellAt(NextCell);
            if (pOldCell != nullptr && pOldCell != pNewCell)
                pOldCell->Remove_Occupier(Owner);
            if (pNewCell != nullptr)
                pNewCell->Add_Occupier(Owner);
        }
        CurrentCell = NextCell;
        return true;
    }

    CellStruct nextCell = Path[PathIndex];
    if (!CanMoveToCell(nextCell)) {
        const CellStruct altCell = FindAlternativeCell(nextCell);
        if (altCell.X == 0 && altCell.Y == 0) {
            return false;
        }
        if (altCell.X == CurrentCell.X && altCell.Y == CurrentCell.Y) {
            // 最近的可站格就是脚下这一格：堵死在本格，不再空转。
            return false;
        }
        nextCell = altCell;
    }

    // ★ 换格时同步登记占用位。
    //   此前步兵走路只改坐标，从不更新格子的占用链；于是"格子上站着人"
    //   这件事在 CellClass 里根本不存在 —— Can_Enter_Cell / IsCellOccupied
    //   一律看到空格，整队步兵会走进同一格叠成同一个坐标点（实测 6 名 E1
    //   压在同一格，同时开火的只有最前面一个）。原版由
    //   ObjectClass::Mark_All_Occupation_Bits 在每格步进时完成这件事。
    CellClass* pOldCell = MapClass::Instance->GetCellAt(CurrentCell);

    CurrentCell = nextCell;
    NextCell = nextCell;
    CoordStruct cellCoord = CoordMath::CellToCoord(nextCell);
    cellCoord.Z = CurrentCoord.Z;

    CurrentCoord.X = cellCoord.X;
    CurrentCoord.Y = cellCoord.Y;
    CurrentCoord.Z = cellCoord.Z;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);

        CellClass* pNewCell = MapClass::Instance->GetCellAt(nextCell);
        if (pOldCell != nullptr && pOldCell != pNewCell)
            pOldCell->Remove_Occupier(Owner);
        if (pNewCell != nullptr)
            pNewCell->Add_Occupier(Owner);
    }

    ++PathIndex;
    return true;
}

bool WalkLocomotionClass::MoveToNextCell()
{
    return MoveOneStep();
}

// ============================================================================
// CanMoveToCell - 单格通行性判定（步兵视角）。
// asm: MapClass 侧的通行判定 + 占用位。
// ============================================================================

bool WalkLocomotionClass::CanMoveToCell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return false;
    }

    // ★ 占用判据必须"按步兵的规则"，不能用 IsCellOccupied。
    //   IsCellOccupied 只要格子上有任何对象就返回 true —— 用它会让两名步兵
    //   永远无法相邻集结/并肩推进；而真正该拦的是"建筑、载具、或已有 3 名
    //   步兵"。这套规则就在 FootClass::Can_Enter_Cell 里。
    //   匍匐前进（IsCrawling）按原版可以钻过被占的格子，故跳过该判定。
    if (!IsCrawling && Owner != nullptr && !Owner->Can_Enter_Cell(cell)) {
        return false;
    }

    return true;
}

// ============================================================================
// FindAlternativeCell - 目标格不可入时，去最近的可站格。
//
// asm 依据：asm 导出里 WalkLocomotionClass 没有独立的"替代格"函数（只有
//   CTOR / Process / MoveTo / StopMoving 那一批），落点搜索是走
//   MapClass::Nearby_Location（asm:0x56DC50 起）—— 从中心格逐圈由内向外
//   扫，半径取 min(0x20, width+height)。这里按行为等价实现：以当前格为
//   圆心逐圈外扩，取第一个"地形可走 + 还能站人"的格子。
//
// 边界与判据：
//   * 半径上限 4 格（9x9 = 81 个候选）。徒步单位疏散一格就够用了，同时
//     把最坏代价钉死在 81 个节点以内 —— 不引入全局 A*，也不做每帧搜索。
//   * 扩展层只看地形（水 / 墙 / 越界不可穿过），斜走不切角（与
//     FootClass::Build_Path 的规则一致）；候选层才用 CanMoveToCell 判占用
//     （建筑 / 载具独占、步兵每格 3 人）。两者必须分开：站满 3 人的格子
//     不能站，但绕行时不该把它当墙，否则队伍一密集搜索立刻"四周全黑"。
//   * 起点自身不算候选（单位已经站在那儿）。
//   * 同一圈里若有多个可站格，取离 blocked 最近的那个。否则疏散会随机
//     落到"离目标更远"的一侧，下一帧再往回走，表现就是原地抖动。
//   * 找不到返回 (0,0)，沿用原有返回值约定。
// ============================================================================
CellStruct WalkLocomotionClass::FindAlternativeCell(CellStruct blocked)
{
    const CellStruct here = CoordMath::CoordToCell(CurrentCoord);

    // ── 缓存命中：同一目标格，退避窗口未过 ────────────────────────────────
    //   被围死时如果每帧都重跑一遍 81 格搜索，几十个步兵同帧搜索会把一帧
    //   拖长；没找到的情况下重搜也没意义（格子的占用不会在 1 帧内变空）。
    if (AltCooldown > 0
        && AltBlockedCell.X == blocked.X && AltBlockedCell.Y == blocked.Y)
    {
        if (AltResultCell.X == 0 && AltResultCell.Y == 0)
            return AltResultCell;               // 上次就没找到：窗口内不再重搜
        if (CoordMath::CellDistance(here, AltResultCell) <= 4
            && CanMoveToCell(AltResultCell))
            return AltResultCell;               // 结果格仍能站：直接复用
        // 结果格已被别人占满 / 单位已走远 → 往下重搜。
    }

    // 地形通行（不含占用）：用于 BFS 扩展。
    const auto terrainOk = [](CellStruct cs) -> bool
    {
        if (cs.X < 0 || cs.Y < 0 || cs.X >= 512 || cs.Y >= 512)
            return false;
        const LandType land = MapClass::Instance->GetLandType(cs);
        return land != LandType::Water && land != LandType::Wall;
    };

    // 能否落脚（含占用）：用于候选判定。
    //   自己脚下永远算能站 —— Can_Enter_Cell 在"本人已登记于该格、且该格
    //   正好满 3 人（含本人）"时会返回 false，不能拿它否掉自己的格子。
    const auto standable = [&](CellStruct cs) -> bool
    {
        if (cs.X == here.X && cs.Y == here.Y)
            return true;
        return CanMoveToCell(cs);
    };

    constexpr int32 kRadius = 4;
    constexpr int32 kSpan   = kRadius * 2 + 1;          // 9

    static const int8 kDX[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };
    static const int8 kDY[8] = { -1, -1,  0,  1,  1,  1,  0, -1 };

    bool  visited[kSpan * kSpan] = {};
    int8  curX[kSpan * kSpan];
    int8  curY[kSpan * kSpan];
    int8  nxtX[kSpan * kSpan];
    int8  nxtY[kSpan * kSpan];

    visited[kRadius * kSpan + kRadius] = true;          // 起点
    curX[0] = 0;
    curY[0] = 0;
    int32 curCount = 1;

    CellStruct result(0, 0);

    for (int32 ring = 1; ring <= kRadius; ++ring)
    {
        int32 nxtCount = 0;
        int32 bestDist = 0x7FFFFFFF;

        for (int32 i = 0; i < curCount; ++i)
        {
            for (int32 d = 0; d < 8; ++d)
            {
                const int32 ox = static_cast<int32>(curX[i]) + kDX[d];
                const int32 oy = static_cast<int32>(curY[i]) + kDY[d];
                if (ox < -kRadius || ox > kRadius || oy < -kRadius || oy > kRadius)
                    continue;

                const int32 vi = (oy + kRadius) * kSpan + (ox + kRadius);
                if (visited[vi])
                    continue;

                const CellStruct cs(static_cast<int16>(here.X + ox),
                                    static_cast<int16>(here.Y + oy));
                if (!terrainOk(cs))
                {
                    visited[vi] = true;             // 墙 / 水 / 越界：不可穿过
                    continue;
                }

                // 斜走不切角：两条正交边也必须能过。
                if (kDX[d] != 0 && kDY[d] != 0)
                {
                    const CellStruct sideX(static_cast<int16>(here.X + ox), here.Y);
                    const CellStruct sideY(here.X, static_cast<int16>(here.Y + oy));
                    if (!terrainOk(sideX) || !terrainOk(sideY))
                        continue;                   // 不置 visited：正交方向仍可到达
                }

                visited[vi] = true;
                nxtX[nxtCount] = static_cast<int8>(ox);
                nxtY[nxtCount] = static_cast<int8>(oy);
                ++nxtCount;

                // (0,0) 是"没找到"的哨兵值，不能当作合法候选返回。
                if (cs.X == 0 && cs.Y == 0)
                    continue;

                if (standable(cs))
                {
                    const int32 dist = CoordMath::CellDistance(cs, blocked);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        result = cs;
                    }
                }
            }
        }

        if (result.X != 0 || result.Y != 0)
            break;                                  // 本圈已命中：最近的一圈

        for (int32 i = 0; i < nxtCount; ++i)
        {
            curX[i] = nxtX[i];
            curY[i] = nxtY[i];
        }
        curCount = nxtCount;
        if (curCount == 0)
            break;                                  // 四周全是墙/水：无路可疏散
    }

    // 写回缓存：找到时 15 帧内复用，没找到时退避 60 帧（被围死期间不再重搜）。
    AltBlockedCell = blocked;
    AltResultCell  = result;
    AltCooldown    = (result.X == 0 && result.Y == 0) ? 60 : 15;

    return result;
}

// ============================================================================
// ProcessFall - 下坠：每帧下落，着地即转趴下姿态。
// asm: sub_75AEC0 的下坠分支（载体姿态切 Prone/Tumble）。
// ============================================================================

bool WalkLocomotionClass::ProcessFall()
{
    CurrentCoord.Z -= 8;

    int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);
    if (CurrentCoord.Z <= groundZ) {
        CurrentCoord.Z = groundZ;
        IsFalling = false;
        IsMoving = false;
        JustLanded = true;
        IsReallyMovingNow = false;

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Prone);
        }
        return false;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Tumble);
    }

    return true;
}

// ============================================================================
// ProcessJump - 跳跃：抛物线式升-降，落回地面高度。
// asm: sub_75AEC0 的跳跃分支。
// ============================================================================

bool WalkLocomotionClass::ProcessJump()
{
    static int32 jumpHeight = 0;
    static int32 jumpPhase = 0;
    const int32 jumpMax = 128;

    int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);

    if (jumpPhase == 0) {
        jumpHeight += 16;
        if (jumpHeight >= jumpMax) {
            jumpHeight = jumpMax;
            jumpPhase = 1;
        }
    } else {
        jumpHeight -= 16;
        if (jumpHeight <= 0) {
            jumpHeight = 0;
            jumpPhase = 0;
            IsJumping = false;
            CurrentCoord.Z = groundZ;
        }
    }

    CurrentCoord.Z = groundZ + jumpHeight;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    return IsJumping;
}

void WalkLocomotionClass::BeginCrawl()
{
    IsCrawling = true;
    StepSize = 4;

    if (Owner) {
        Owner->SetSequence(Sequence::Crawl);
    }
}

void WalkLocomotionClass::EndCrawl()
{
    IsCrawling = false;
    StepSize = 8;

    if (Owner) {
        Owner->SetSequence(Sequence::Ready);
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - 把占用位交给载体统一登记。
// asm: WalkLocomotionClass_ILocomotion_MarkAllOccupationBits   (asm:1256)
//   mark == 0 时取朝往点坐标并转交载体的 +0xF4 槽；mark != 0 时（清位）原版
//   直接返回，由载体的更新循环统一撤销。
// ============================================================================

void WalkLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (mark != MarkType::Up) {
        return;
    }
    if (!Owner) {
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    MapClass::Instance->MarkCellOccupied(cell, true);
}

void WalkLocomotionClass::Limbo()
{
    Mark_All_Occupation_Bits(MarkType::Down);
    IsMoving = false;
    IsFalling = false;
    IsJumping = false;
    IsCrawling = false;
    IsReallyMovingNow = false;
    MoveStepTimer = 0;
    HeadTo = CoordStruct(0, 0, 0);
    Path.Clear();
    PathIndex = 0;
    PathLength = 0;
}

// ============================================================================
// Can_Enter_Cell - 步兵通行判定。
//   水面/墙不可入；岩石只能临时停留（Move::Temp）；其余可入。
// ============================================================================

Move WalkLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return Move::No;
    }

    // ★ 占用判定交给载体（FootClass::Can_Enter_Cell）：建筑/载具独占一格、
    //   步兵每格最多 3 人。这段此前完全缺失，于是整队步兵会一路走进敌方
    //   单位所在的格子叠成一堆 —— 只查地形永远发现不了"这格站满了人"。
    if (Owner != nullptr && !Owner->Can_Enter_Cell(cell)) {
        // 岩石格允许"临时停留"（Move::Temp 会让上层继续找下一格）。
        return (land == LandType::Rock) ? Move::Temp : Move::No;
    }

    if (land == LandType::Rock) {
        return Move::Temp;
    }

    return Move::OK;
}

// ============================================================================
// GetStepSound - 脚步音编号按地表类型分档。
// asm: sub_75AEC0 里 VocClass_Play 的地表判定段。
// ============================================================================

int32 WalkLocomotionClass::GetStepSound() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(cell);

    switch (land) {
        case LandType::Road:     return 1;
        case LandType::Ice:      return 2;
        case LandType::Weeds:    return 3;
        case LandType::Tiberium: return 4;
        case LandType::Rough:    return 5;
        case LandType::Beach:    return 6;
        case LandType::Clear:
        default:                 return 0;
    }
}

// ============================================================================
// PlayStepSound - 脚步音按固定节拍触发。
// asm: sub_75AEC0 —— 载体 +0x68A 为步伐节拍旗标，到点后 VocClass_Play
//   播放一次并清旗标。
// ============================================================================

void WalkLocomotionClass::PlayStepSound()
{
    // 每推进两格响一次（原版按载体动画帧触发，这里以步进计数近似同一节拍）。
    ++MoveStepTimer;
    if ((MoveStepTimer & 1) != 0) {
        return;
    }

    const int32 sound = GetStepSound();
    if (sound != 0 && Owner) {
        Owner->PlaySoundEffect(sound);
    }
}

// ============================================================================
// Is_Moving_Now - 是否此刻真正在动。
// asm: WalkLocomotionClass_ILocomotion_IsMovingNow   (asm:106)
//   Is_Moving() && 载体 SpeedPercentage(+0x578) > 0 && HeadTo != (0,0,0)。
// ============================================================================

bool WalkLocomotionClass::Is_Moving_Now()
{
    if (!IsMoving) {
        return false;
    }
    if (Owner && Owner->SpeedPercentage <= 0.0) {
        return false;
    }
    return !IsDefaultPos(HeadTo);
}

// ============================================================================
// Is_Really_Moving_Now - 本帧真的推进过。
// asm: WalkLocomotionClass_ILocomotion_IsReallyMovingNow   (asm:1418)
//   直接读 +0x32 的逐帧旗标。
// ============================================================================

bool WalkLocomotionClass::Is_Really_Moving_Now() const
{
    return IsReallyMovingNow && !IsFalling && !IsJumping;
}

// ============================================================================
// Get_Status - 步兵动作状态码。
// ============================================================================

int32 WalkLocomotionClass::Get_Status() const
{
    if (IsFalling) {
        return static_cast<int32>(Sequence::Tumble);
    }
    if (IsCrawling) {
        return static_cast<int32>(Sequence::Crawl);
    }
    if (IsMoving || IsJumping) {
        return static_cast<int32>(Sequence::Walk);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Movement_AI - 地形感知的速度修正。
// asm: WalkLocomotionClass_ILocomotion_Process 内的地表速度段。
// ============================================================================

void WalkLocomotionClass::Movement_AI()
{
    if (!IsMoving || !Powered) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(currentCell);

    float baseSpeedMod = 1.0f;
    switch (land) {
        case LandType::Road:     baseSpeedMod = 1.1f;  break;
        case LandType::Rough:    baseSpeedMod = 0.75f; break;
        case LandType::Ice:      baseSpeedMod = 0.8f;  break;
        case LandType::Weeds:    baseSpeedMod = 0.85f; break;
        case LandType::Tiberium: baseSpeedMod = 0.6f;  break;
        case LandType::Beach:    baseSpeedMod = 0.9f;  break;
        default:                 baseSpeedMod = 1.0f;  break;
    }

    if (IsCrawling) {
        baseSpeedMod *= 0.5f;
    }

    SpeedPercentage = baseSpeedMod;

    CellStruct destCell = CoordMath::CoordToCell(Dest);
    if (CoordMath::CellDistance(currentCell, destCell) <= 1) {
        SpeedPercentage *= 0.8f;
    }
}

FireError WalkLocomotionClass::Can_Fire() const
{
    if (IsFalling || IsJumping) {
        return FireError::Movement;
    }
    return FireError::OK;
}

bool WalkLocomotionClass::Is_To_Have_Moving_Anim() const
{
    return IsMoving && !IsFalling && !IsJumping;
}

// ============================================================================
// Apparent_Speed - 表观速度（爬行减半）。
// ============================================================================

int32 WalkLocomotionClass::Apparent_Speed() const
{
    const int32 base = GetCurrentSpeed();
    if (IsCrawling) {
        return base / 2;
    }
    return base;
}

// ============================================================================
// Force_New_Slope - 坡道处理。
//   陡坡（ramp > 2）直接转入下坠；缓坡按坡度降低速度。
// ============================================================================

void WalkLocomotionClass::Force_New_Slope(int32 ramp)
{
    if (ramp > 2) {
        IsFalling = true;
        CurrentCoord.Z = MapClass::Instance->GetGroundHeight(CurrentCoord);
    } else {
        SpeedPercentage = 1.0f - static_cast<float>(ramp) * 0.1f;
        if (SpeedPercentage < 0.5f) {
            SpeedPercentage = 0.5f;
        }
    }
}

// ------------------------------------------------------------------------
// asm: WalkLocomotionClass_IPiggyback_QueryInterface
// 状态由一个 ILocomotion* 槽承载——开始搭车登记、结束搭车交出并清槽、
// 是否搭车看槽位占用、能否结束要确认本体已经停下。
// ------------------------------------------------------------------------
HRESULT WalkLocomotionClass::IPiggyback_QueryInterface(REFIID iid, void** ppvObject)
{
    return QueryInterface(iid, ppvObject);
}

ULONG WalkLocomotionClass::IPiggyback_AddRef()
{
    return AddRef();
}

ULONG WalkLocomotionClass::IPiggyback_Release()
{
    return Release();
}

bool WalkLocomotionClass::IPiggyback_IsPiggybacking() const
{
    return Piggyback != nullptr;
}

HRESULT WalkLocomotionClass::IPiggyback_BeginPiggyback(ILocomotion* pLoco)
{
    if (!pLoco)
        return static_cast<HRESULT>(0x80004003); // E_POINTER
    if (Piggyback && Piggyback != pLoco)
        return static_cast<HRESULT>(0x80004003);
    Piggyback = pLoco;
    return S_OK;
}

HRESULT WalkLocomotionClass::IPiggyback_EndPiggyback(ILocomotion** ppOut)
{
    if (!ppOut)
        return static_cast<HRESULT>(0x80004003); // E_POINTER
    if (!Piggyback)
        return E_FAIL;
    *ppOut = Piggyback;
    Piggyback = nullptr;
    return S_OK;
}

bool WalkLocomotionClass::IPiggyback_IsOKToEnd()
{
    // 本体必须已经停下、且仍挂着搭车对象、且不在下坠中。
    if (IsMoving)
        return false;
    if (!Piggyback)
        return false;
    if (IsFalling)
        return false;
    return true;
}

HRESULT WalkLocomotionClass::IPiggyback_PiggybackCLSID(GUID* pGUID)
{
    if (!pGUID || !Piggyback)
        return static_cast<HRESULT>(0x80004003); // E_POINTER
    LocomotionClass* pLoco = static_cast<LocomotionClass*>(Piggyback);
    return pLoco->GetClassID(pGUID);
}
