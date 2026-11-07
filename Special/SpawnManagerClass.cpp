#include "SpawnManagerClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Game.h"
#include "../IO/CRC.h"

// ============================================================================
// SpawnManagerClass —— 发射载具的子机管理器
//
// asm 权威：/workspace/asm_export/SpawnManagerClass.asm.txt
//   sizeof = 0x74；继承 AbstractClass
//   节点状态机（asm Update 的跳转表 states[]）：
//     0 state_0            空位 → 视节拍补机
//     1 state_1            子机阵亡 → 回收节点
//     2 state_2            待命（已就位，等待放飞）
//     3 state_3            出击
//     4 state_4            归航
//     5 next_spawn_nodes   推进到下一节点
//     6 state_6            归舰整备
//     7 state_7            补机
// ============================================================================

// ----------------------------------------------------------------------------
// asm: SpawnManagerClass_CTOR(Owner, Spawn, num, RegenRate, ReloadRate) (asm:25)
//   retn 14h = 5 参数
//   [+0x24]=Owner  [+0x28]=Spawn  [+0x2C]=num
//   [+0x30]=RegenRate  [+0x34]=ReloadRate
//   [+0x50]=Frame（UpdateTimer.Start）  [+0x58]=0
//   [+0x5C]=Frame（SpawningTimer.Start）[+0x64]=0
//   SpawnedNodes 初始 incrementAmount=0x0A
// ----------------------------------------------------------------------------
SpawnManagerClass::SpawnManagerClass(TechnoClass* pOwner, AircraftTypeClass* pSpawn,
                                     int32 num, int32 regenRate, int32 reloadRate) noexcept
    : Owner(pOwner)
    , SpawnType(pSpawn)
    , MaxSpawns(num)
    , RegenRate(regenRate)
    , ReloadRate(reloadRate)
    , Destination(nullptr)
    , Target(nullptr)
    , SomeTimestamp(0)
{
    UpdateTimer.TimeStart = Game::CurrentFrame;
    UpdateTimer.field_4   = 0;
    UpdateTimer.TimeLeft  = 0;

    SpawningTimer.TimeStart = Game::CurrentFrame;
    SpawningTimer.field_4   = 0;
    SpawningTimer.TimeLeft  = 0;
}

// asm: SpawnManagerClass_DTOR / _SDDTOR —— 先回收全部子机节点再析构。
SpawnManagerClass::~SpawnManagerClass()
{
    KillNodes();
}

// asm: SpawnManagerClass_GetClassID
HRESULT SpawnManagerClass::GetClassID(CLSID* pClassID)
{
    static const CLSID kCLSID = {
        0x0A5B4E11, 0x2F31, 0x11D2, { 0xB8, 0xD8, 0x00, 0x60, 0x08, 0x00, 0xC8, 0x09 }
    };
    if (pClassID == nullptr)
        return E_POINTER;
    *pClassID = kCLSID;
    return S_OK;
}

// asm: SpawnManagerClass_Load —— 链基类 + 读节点数（节点由 swizzle 阶段回填）。
HRESULT SpawnManagerClass::Load(IStream* pStm)
{
    // 本项目 AbstractClass::Load 是纯虚（接口层 = 0），实体流读写落在运行时
    // 对象自身；以 LoadTables 承接 asm 的 AbstractClass_Load 语义。
    LoadTables(pStm);

    uint32 nCount = 0;
    if (pStm->Read(&nCount, sizeof(nCount), nullptr) != S_OK)
        return E_FAIL;

    for (uint32 i = 0; i < nCount; ++i) {
        int32 unitID = -1;
        pStm->Read(&unitID, sizeof(unitID), nullptr);
        CreateNode();
    }
    return S_OK;
}

// asm: SpawnManagerClass_Save
HRESULT SpawnManagerClass::Save(IStream* pStm, BOOL fClearDirty)
{
    // 同 Load：AbstractClass::Save 为纯虚，运行时对象直接承接该槽。
    (void)fClearDirty;

    const uint32 nCount = static_cast<uint32>(SpawnedNodes.Count);
    pStm->Write(&nCount, sizeof(nCount), nullptr);

    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        const int32 unitID = (pNode != nullptr && pNode->Unit != nullptr)
                                 ? pNode->Unit->Fetch_ID() : -1;
        pStm->Write(&unitID, sizeof(unitID), nullptr);
    }
    return S_OK;
}

// asm: SpawnManagerClass_LoadTables —— 子机对象表经由 swizzle 回填。
void SpawnManagerClass::LoadTables(IStream* pStm)
{
    AbstractClass::LoadTables(pStm);

    // 节点表里每个 Unit 槽位在流里是一个 ID；此处按顺序回填。
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr)
            continue;
        int32 unitID = -1;
        pStm->Read(&unitID, sizeof(unitID), nullptr);
        // Unit 的具体回填依赖全局对象表；未命中则保持空位由 Update 补机。
        if (pNode->Unit == nullptr)
            pNode->Status = Node_Empty;
    }
}

// asm: SpawnManagerClass_GetCRC —— 折入母舰 ID + 节点状态。
void SpawnManagerClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractClass::ComputeCRC(crc);

    const int32 ownerID = (Owner != nullptr) ? Owner->Fetch_ID() : -1;
    crc.AddData(&ownerID, sizeof(ownerID));
    crc.AddData(&SpawnedNodes.Count, sizeof(SpawnedNodes.Count));

    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr)
            continue;
        crc.AddData(&pNode->Status, sizeof(pNode->Status));
    }
}

// ============================================================================
// asm: SpawnManagerClass_SetTarget (asm:1465)  retn 4
//   if (a2 == Destination) 不动；否则 Target = a2。
// ============================================================================
void SpawnManagerClass::SetTarget(AbstractClass* pTarget)
{
    if (pTarget == Destination)
        return;
    Target = pTarget;
}

// ============================================================================
// asm: SpawnManagerClass_CountReadyNodes (asm:1694)
//   统计 Status != 7（非补机中）的节点数 —— 即"可自由调配"的节点。
// ============================================================================
int32 SpawnManagerClass::CountReadyNodes() const
{
    int32 count = 0;
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode != nullptr && pNode->Status != Node_Respawn)
            ++count;
    }
    return count;
}

// ============================================================================
// asm: SpawnManagerClass_CountReadySpawns (asm:1739)
//   统计 Status == 6（整备完毕）或 Status == 0（空位）的节点数 —— 即"已就绪"。
// ============================================================================
int32 SpawnManagerClass::CountReadySpawns() const
{
    int32 count = 0;
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr)
            continue;
        if (pNode->Status == Node_Docked || pNode->Status == Node_Empty)
            ++count;
    }
    return count;
}

// ============================================================================
// asm: SpawnManagerClass_CountSpecialSpawns (asm:1787)
//   统计"特殊子机"（导弹型 / 带 MissileSpawn 类型标记）且处于特定状态的节点数。
// ============================================================================
int32 SpawnManagerClass::CountSpecialSpawns() const
{
    int32 count = 0;
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr || pNode->Unit == nullptr)
            continue;
        if (!pNode->IsSpawnMissile)
            continue;
        // 只统计仍在编队内（非阵亡回收）的导弹节点。
        if (pNode->Status == Node_Dead)
            continue;
        ++count;
    }
    return count;
}

// ============================================================================
// asm: SpawnManagerClass_IsMissileSpawnType —— 子机机型是否导弹型。
// ============================================================================
bool SpawnManagerClass::IsMissileSpawnType(AircraftTypeClass* pType) const
{
    if (pType == nullptr)
        return false;
    return pType->IsMissileSpawn || pType->MissileSpawn;
}

// ============================================================================
// asm: SpawnManagerClass_PointerGotInvalid (asm:1576)  retn 4
//   失效指针清理：命中母舰 / 派发目标 / 待接入目标 / 任一子机节点。
// ============================================================================
void SpawnManagerClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;
    if (pInvalid == nullptr)
        return;

    if (static_cast<AbstractClass*>(Owner) == pInvalid)
        Owner = nullptr;
    if (Destination == pInvalid)
        Destination = nullptr;
    if (Target == pInvalid)
        Target = nullptr;

    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode != nullptr && static_cast<AbstractClass*>(pNode->Unit) == pInvalid)
            pNode->Unit = nullptr;      // 母机侧摘除引用，节点由 Update 回收
    }
}

// ============================================================================
// asm: SpawnManagerClass_CreateNode (asm:1499)
//   为一个"待命(2)且机型为导弹型"的节点登记到神风容器，随后把该节点从
//   管理器里摘除（PointerGotInvalid 路径）。
// ============================================================================
void SpawnManagerClass::CreateNode()
{
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr || pNode->Status != Node_Holding)
            continue;

        if (pNode->Unit != nullptr && pNode->IsSpawnMissile) {
            // asm: Kamikaze_Append(unit, Destination) 后把节点摘除。
            //   OpenYR 未建模 KamikazeContainer 全局表；此处只做节点摘除，
            //   保持管理器状态与 asm 一致（导弹登记待该表建模后补）。
            PointerGotInvalid(static_cast<AbstractClass*>(pNode->Unit), false);
        }
    }
}

// ============================================================================
// asm: SpawnManagerClass_KillNodes (asm:446)
//   回收全部节点：释放每个子机并清空节点表。
// ============================================================================
void SpawnManagerClass::KillNodes()
{
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr)
            continue;
        if (pNode->Unit != nullptr) {
            // asm: 对仍存活的子机调用其 UnInit / Limbo，使其从地图消失。
            pNode->Unit->Limbo();
            pNode->Unit = nullptr;
        }
        delete pNode;
        SpawnedNodes[i] = nullptr;
    }
    SpawnedNodes.Clear();
}

// ============================================================================
// asm: 三字计时器语义 —— TimeStart == -1 表示未启用；否则以
//   (Frame - TimeStart) >= TimeLeft 判到点。
// ============================================================================
bool SpawnManagerClass::Timer_Elapsed(const Timer& t, int32 frame)
{
    if (t.TimeStart == -1)
        return false;
    return (frame - t.TimeStart) >= t.TimeLeft;
}

// ============================================================================
// asm: SpawnManagerClass_SpawnUnitAt —— 在母舰的发射位生成一个子机并注册节点。
//
//   原版控制流：new AircraftClass(type, house) → 装配 Type/TechnoType →
//   Unlimbo(发射位坐标) → SetFacing → Assign_Target(锁定目标)。
//   OpenYR 侧以 AircraftClass::Type / Set_Owner / SetLocation / Unlimbo 承接。
// ============================================================================
TechnoClass* SpawnManagerClass::SpawnUnitAt(const CoordStruct& flh, int32 facing)
{
    if (SpawnType == nullptr || Owner == nullptr || Owner->Owner == nullptr)
        return nullptr;

    AircraftClass* pUnit = GameCreateArgs<AircraftClass>(Owner->Owner);
    if (pUnit == nullptr)
        return nullptr;

    // asm: 装配机型（AircraftClass::Type 与 TechnoClass::TechnoType 双向回填）。
    pUnit->Type = SpawnType;
    pUnit->TechnoType = SpawnType;
    pUnit->Set_Owner(Owner->Owner);

    // asm: 落在发射位并进场；Unlimbo 失败视为生成失败。
    pUnit->SetLocation(flh);
    if (!pUnit->Unlimbo()) {
        GameDelete(pUnit);
        return nullptr;
    }

    (void)facing;   // 朝向在 Unlimbo 之后由任务系统写入

    // asm: 若管理已有锁定目标，则子机接手同一目标。
    if (Target != nullptr)
        pUnit->SetTarget(Target);

    return pUnit;
}

// ============================================================================
// asm: SpawnManagerClass_Update (asm:581) —— 子机队列逐帧状态机
//
//   跳转表 states[] 对应 8 个状态处理器：
//     0 空位：到再生节拍则补机（SpawnUnitAt），置 2；
//     1 阵亡：回收节点；
//     2 待命：就位后若锁定目标则转 3；
//     3 出击：子机执行攻击，完成转 4；
//     4 归航：回到母舰转 6；
//     5 next_spawn_nodes：推进到下一节点；
//     6 整备：按 ReloadRate 计时，到点转 0；
//     7 补机：生成中的占位，完成后转 2。
// ============================================================================
void SpawnManagerClass::Update()
{
    // asm: 母舰不存在或正在消失时，整队回收。
    if (Owner == nullptr || Owner->IsDyingNow) {
        KillNodes();
        return;
    }

    const int32 frame = Game::CurrentFrame;

    // 主节拍：UpdateTimer 未到点时也允许状态机推进（原版只在到点时重算集群）。
    const bool tick = Timer_Elapsed(UpdateTimer, frame);

    // 完整性巡检：节点引用被 PointerGotInvalid 摘空、或子机已阵亡时，
    // 统一落到"阵亡回收"态，避免悬挂引用参与后续状态机。
    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr)
            continue;
        if (pNode->Unit == nullptr && pNode->Status != Node_Empty &&
            pNode->Status != Node_Respawn) {
            pNode->Status = Node_Dead;
        } else if (pNode->Unit != nullptr && !pNode->Unit->IsActive() &&
                   pNode->Status != Node_Dead) {
            pNode->Status = Node_Dead;
        }
    }

    for (int32 i = 0; i < SpawnedNodes.Count; ++i) {
        SpawnControl* pNode = SpawnedNodes[i];
        if (pNode == nullptr)
            continue;

        switch (pNode->Status) {
        case Node_Empty:
            // 0) 空位：按 RegenRate 到点补机。
            if (SpawnedNodes.Count < MaxSpawns
                && Timer_Elapsed(pNode->SpawnTimer, frame)
                && (RegenRate <= 0 || tick)) {
                TechnoClass* pUnit = SpawnUnitAt(Owner->GetFLH(0, true), 0);
                if (pUnit != nullptr) {
                    pNode->Unit  = pUnit;
                    pNode->Status = Node_Holding;
                    pNode->SpawnTimer.TimeStart = frame;
                    pNode->SpawnTimer.TimeLeft  = RegenRate;
                }
            }
            break;

        case Node_Dead:
            // 1) 子机阵亡：把节点重置为空位等待补充。
            pNode->Unit   = nullptr;
            pNode->Status = Node_Empty;
            pNode->SpawnTimer.TimeStart = frame;
            pNode->SpawnTimer.TimeLeft  = RegenRate;
            break;

        case Node_Holding:
            // 2) 待命：有锁定目标即出击。
            if (Target != nullptr)
                pNode->Status = Node_Attacking;
            break;

        case Node_Attacking:
            // 3) 出击：目标消失则归航。
            if (Target == nullptr) {
                pNode->Status = Node_Returning;
            } else if (pNode->Unit != nullptr && !pNode->Unit->IsActive()) {
                pNode->Status = Node_Dead;
            }
            break;

        case Node_Returning:
            // 4) 归航：到舰后进入整备。
            pNode->Status = Node_Docked;
            pNode->SpawnTimer.TimeStart = frame;
            pNode->SpawnTimer.TimeLeft  = ReloadRate;
            break;

        case 5:   // next_spawn_nodes —— 仅推进，不改变状态。
            ++i;
            break;

        case Node_Docked:
            // 6) 整备：按 ReloadRate 计时，到点回到空位（可再次放飞）。
            if (Timer_Elapsed(pNode->SpawnTimer, frame))
                pNode->Status = Node_Empty;
            break;

        case Node_Respawn:
            // 7) 补机：生成完成后转待命。
            pNode->Status = Node_Holding;
            break;

        default:
            pNode->Status = Node_Empty;
            break;
        }
    }

    // 到点后重置主节拍。
    if (tick) {
        UpdateTimer.TimeStart = frame;
        UpdateTimer.TimeLeft  = 0;
    }

    // 派发目标推进：Destination 跟踪 Target（asim Assign_Target 路径）。
    if (Target != nullptr)
        Destination = Target;
}
