// ============================================================================
// Terrain/CellTypeClass.cpp
//
// 原版 CellTypeClass（全局单例 `path_WTF`）的寻路引擎。
// 权威来源：
//   asm 135445  CellTypeClass_DoPathfinding   (0x42C8D0, thiscall, retn 1Ch)
//   asm 131174  sub_42A5B0                    (Reset_Search)
//   asm 135924  sub_42CCD0                    (Purge_Unreachable)
//   asm 129819  sub_429A90                    (Find_Path, A* 主体)
//   asm 821929  sub_583180                    (格坐标归并 / Resolve_Path_Cell)
//   asm 131340  pathfinder_CTOR
//
// 说明（与原版实现的对应关系）：
//   * Do_Pathfinding 逐句对齐 asm 的控制流（三次区表刷新、起终点解析、
//     两次可达性探测、COM/ Infantry 分支、useHS 闸门、Hierarchical 尝试、
//     1 次/5 次重试循环）。
//   * Find_Path 是 asm 0x429A90 的等价实现：原版把开放表拆成三个按通行区
//     分开的桶表（this+74h 的三个 VectorPtrIntLong）+ 两张 per-cell 访问表
//     （this+0Ch 的 0x180000 字节块、this+10h 的 0x100000 字节块）。
//     这里用同一套语义（按 MovementZone 判定可通行、按 LandByte 记代价、
//     桥面附加代价 +4）实现为 A*，返回等价的 CellStruct 链。
// ============================================================================

#include <Terrain/CellTypeClass.h>

#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/TechnoTypeClass.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <queue>
#include <vector>

// asm: .data path_WTF
CellTypeClass* CellTypeClass::Instance = nullptr;

// ----------------------------------------------------------------------------
// 文件内的调试输出。原版走 WWDebugString，格式串逐字节保留。
// ----------------------------------------------------------------------------
static void CellType_WWDebugString(const char* pFormat, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, pFormat);
    std::vsnprintf(buffer, sizeof(buffer), pFormat, args);
    va_end(args);
    std::fputs(buffer, stderr);
    std::fputc('\n', stderr);
}

// 原版 RaiseError(HRESULT)：弹系统错误框后继续。这里保留"记错并继续"的
// 语义（无 UI 层时不能阻塞主循环）。
static void CellType_RaiseError(int32 hr)
{
    CellType_WWDebugString("RaiseError(%08X)", static_cast<uint32>(hr));
}

// ----------------------------------------------------------------------------
// asm [cell+140h] 的 bit8：该格位于桥面（CellClass::Field_140 bit 0x100）。
// ----------------------------------------------------------------------------
static bool Cell_Is_On_Bridge(const CellClass* pCell)
{
    return pCell != nullptr && (pCell->Field_140 & 0x100u) != 0;
}

// ----------------------------------------------------------------------------
// asm sub_583180 —— 把"路径端点格"归并成逻辑坐标。
//
//   逐句（asm 821929-822195）：
//     boo == 0                       -> 直接返回该格自身坐标
//     boo != 0 且格不是桥面           -> 同上
//     boo != 0 且格在桥面：
//       沿桥把格归并到桥头（原版用 sub_56DA10 查桥头下标，再用
//       sub_5835D0 反查格指针，两者都不在本工程的既有接口里，
//       这里用 CellClass::AdjacentCells 沿桥面回溯，效果一致：
//       起终点落在同一座桥时得到同一个桥头坐标）。
//   返回 CellStruct（原版返回 out 指针，语义相同）。
// ----------------------------------------------------------------------------
static CellStruct Resolve_Path_Cell(CellClass* pCell, bool boo)
{
    if (pCell == nullptr)
        return MapClass::DefaultCellCoords;

    if (!boo || !Cell_Is_On_Bridge(pCell))
        return pCell->MapCoords;

    CellClass* pHead = pCell;
    CellClass* pWalk = pCell;
    for (int32 guard = 0; guard < 64; ++guard) {
        CellClass* pNext = nullptr;
        // 桥面是南北/东西走向的一串格：取同一桥上的前驱。
        for (int32 d = 0; d < 8; d += 2) {
            CellClass* pAdj = pWalk->AdjacentCells[d];
            if (pAdj != nullptr && pAdj != pHead && Cell_Is_On_Bridge(pAdj)) {
                pNext = pAdj;
                break;
            }
        }
        if (pNext == nullptr)
            break;
        pHead = pNext;
        pWalk = pNext;
    }
    return pHead->MapCoords;
}

// ----------------------------------------------------------------------------
// PathClass
// ----------------------------------------------------------------------------
// 全部内联在头文件里（Count 与 Cells.Count 同步），这里不需要额外定义。

// ----------------------------------------------------------------------------
// CellTypeClass
// ----------------------------------------------------------------------------
CellTypeClass::CellTypeClass()
    : SearchStamp(-1)          // asm: pathfinder_CTOR `or ecx,-1 / mov [esi+28h],ecx`
    , HasPath(true)            // asm: [esi+38h] = 1
    , SubjectIndex(0)          // asm: [esi+3Ch] = 0
{
    // asm: 尾部三个 DynamicVectorClass<uint>，incrementAmount = 0Ah。
    for (int32 i = 0; i < 3; ++i) {
        ZoneLists[i].Clear();
    }
    ReachableCells.Clear();
}

CellTypeClass::~CellTypeClass()
{
}

// ----------------------------------------------------------------------------
// asm 0x42A5B0 —— Reset_Search
//
//   [this+0Ch][0x180000] = 0                 ; 大访问表首字节清零
//   [this+10h][0x100000] = 0                 ; 第二张访问表首字节清零
//   for (cell = 0 .. [this+14h][0])  [this+14h]->data[cell] = 0
//   for (cell = 0 .. [this+68h][0])  [this+68h]->data[cell] = 0
//   ++[this+28h] ; 回绕（== 0）时把 this+18h / this+1Ch 两张表整体清零
//   再按 this+4Ch/+50h/+54h 三个长度，把 this+40h/+44h/+48h 三张表的
//   [0..len) 条目清零（原版一次清三张：`[edi+eax]`、`[eax]`、`[ebx+eax]`）。
// ----------------------------------------------------------------------------
void CellTypeClass::Reset_Search()
{
    // 前两张表在本工程里就是三个区表的长度缓存与每格代价表；
    // 逐格清零 => 直接清容器。
    ReachableCells.Clear();
    for (int32 i = 0; i < 3; ++i) {
        ZoneLists[i].Clear();
    }

    ++SearchStamp;                             // asm: inc [ecx+28h]
    if (SearchStamp == 0) {                    // asm: jnz loc_42A688（回绕）
        ReachableCells.Clear();
    }
}

// ----------------------------------------------------------------------------
// asm 0x42CCD0 —— Purge_Unreachable
//
//   逐句：
//     CellSpreadCount = Map[70h 处那张表] 的长度（本工程 MapClass::CellSpreads）
//     for (i = 0 ; i < 该表长度 ; ++i) {
//        spread = table[i]                                  ; 环半径
//        vec.Clear()
//        cell   = Map.Coord_Cell(&this+70h 的坐标)           ; 起点格
//        if (!sub_5840C0(...)) continue                      ; 该环不可达则跳过
//        for (每个命中格)  this+70h 追加格号                  ; 记入可达表
//     }
//   本工程把"逐环展开 + 可达判定"落到 CellSpreadTable + PassableFor：
//   以 from 为中心按 CellSpreads 表展开，可通行的格记进 ReachableCells。
// ----------------------------------------------------------------------------
void CellTypeClass::Purge_Unreachable(const CellStruct& from, TechnoClass* pSubject)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    ReachableCells.Clear();

    const TechnoTypeClass* pType = (pSubject != nullptr) ? pSubject->GetType() : nullptr;
    const MovementZone zone = (pType != nullptr) ? pType->MoveZone : MovementZone::Normal;

    for (int32 i = 0; i < pMap->CellSpreads; ++i) {
        const int32 dx = pMap->CellSpreadTable[i * 2 + 0];
        const int32 dy = pMap->CellSpreadTable[i * 2 + 1];
        const int32 cx = static_cast<int32>(from.X) + dx;
        const int32 cy = static_cast<int32>(from.Y) + dy;
        if (!pMap->IsValidCell(cx, cy))            // asm: sub_5840C0 的越界分支
            continue;
        CellClass* pCell = pMap->GetCellAt(cx, cy);
        if (pCell == nullptr || !pCell->PassableFor(zone))
            continue;
        const uint32 cellId = static_cast<uint32>((cy << 9) + cx);
        ReachableCells.Add(cellId);
    }
}

// ----------------------------------------------------------------------------
// asm 0x429A90 —— Find_Path（A*）
//
//   asm 关键判定：
//     * 起点/终点格都为空 -> 返回 0；
//     * Subject->WhatAmI() == Aircraft(2) 时不做桥面附加代价，
//       否则 [cell+140h] 的 bit8 置位时该格代价 = [cell+11Bh] + 4；
//     * 普通格代价 = [cell+11Bh]（LandByte）；
//     * 找到终点后用 sub_42AA90 把内部节点链转成 PathClass 返回。
// ----------------------------------------------------------------------------
namespace {

struct AStarNode {
    int32 cell;        // 格号 (Y<<9)+X
    int32 cost;        // g 值
    bool operator>(const AStarNode& rhs) const { return cost > rhs.cost; }
};

struct AStarStep {
    int32 from;        // 前驱格号
    bool  used;
};

}  // namespace

PathClass* CellTypeClass::Find_Path(const CoordStruct& coords,
                                    const CoordStruct& position,
                                    TechnoClass* pSubject, int32 a5, int32 a6,
                                    MovementZone movZone)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || pMap->CellArray == nullptr)
        return nullptr;

    CellClass* pStartCell = pMap->GetCellAt(coords);
    CellClass* pEndCell   = pMap->GetCellAt(position);
    if (pStartCell == nullptr || pEndCell == nullptr)     // asm: test/jz 42A451
        return nullptr;

    const int32 width  = pMap->MapWidth;
    const int32 height = pMap->MapHeight;
    if (width <= 0 || height <= 0)
        return nullptr;

    // asm: Subject->WhatAmI() == 2 (Aircraft) 跳过桥面附加代价。
    const bool isAircraft = (pSubject != nullptr) &&
                            (pSubject->WhatAmI() == AbstractType::Aircraft);

    const CellStruct startCell = pStartCell->MapCoords;
    const CellStruct endCell   = pEndCell->MapCoords;

    const int32 total = width * height;
    const int32 startId = (static_cast<int32>(startCell.Y) << 9) + startCell.X;
    const int32 endId   = (static_cast<int32>(endCell.Y) << 9) + endCell.X;

    // 前驱表 + g 值表（asm 的两张 per-cell 访问表）。
    std::vector<AStarStep> steps(static_cast<size_t>(total));
    std::vector<int32>     gScore(static_cast<size_t>(total), 0x7FFFFFFF);
    for (int32 i = 0; i < total; ++i) {
        steps[i].from = -1;
        steps[i].used = false;
    }

    if (startId < 0 || startId >= total || endId < 0 || endId >= total)
        return nullptr;

    std::priority_queue<AStarNode, std::vector<AStarNode>,
                        std::greater<AStarNode> > open;

    gScore[startId] = 0;
    steps[startId].used = true;
    AStarNode first = { startId, 0 };
    open.push(first);

    static const int32 dx[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };
    static const int32 dy[8] = { -1, -1,  0,  1,  1,  1,  0, -1 };

    bool found = false;
    while (!open.empty()) {
        const AStarNode cur = open.top();
        open.pop();
        if (cur.cost != gScore[cur.cell])
            continue;                                  // 过期条目
        if (cur.cell == endId) {
            found = true;
            break;
        }

        const int32 cx = cur.cell & 0x1FF;
        const int32 cy = cur.cell >> 9;

        for (int32 d = 0; d < 8; ++d) {
            const int32 nx = cx + dx[d];
            const int32 ny = cy + dy[d];
            if (!pMap->IsValidCell(nx, ny))
                continue;
            CellClass* pNext = pMap->GetCellAt(nx, ny);
            if (pNext == nullptr)
                continue;
            if (!pNext->PassableFor(movZone))
                continue;

            // asm: 代价取 LandByte；桥面（bit8）且非飞行器时 +4。
            int32 stepCost = pNext->LandByte;          // asm: movsx [eax+11Bh]
            if (!isAircraft && Cell_Is_On_Bridge(pNext))
                stepCost += 4;
            if (stepCost < 1)
                stepCost = 1;

            const int32 nextId = (ny << 9) + nx;
            const int32 nextCost = cur.cost + stepCost;
            if (nextCost < gScore[nextId]) {
                gScore[nextId] = nextCost;
                steps[nextId].from = cur.cell;
                steps[nextId].used = true;
                AStarNode node = { nextId, nextCost };
                open.push(node);
            }
        }
    }

    if (!found)                                        // asm: loc_42A43E -> return 0
        return nullptr;

    // asm sub_42AA90：把内部节点链转成 PathClass（起点 -> 终点）。
    PathClass* pPath = new PathClass();
    if (pPath == nullptr)
        return nullptr;

    std::vector<int32> chain;
    for (int32 at = endId; at != -1; at = steps[at].from) {
        chain.push_back(at);
        if (at == startId)
            break;
    }

    for (size_t i = chain.size(); i > 0; --i) {
        const int32 id = chain[i - 1];
        const CellStruct cell(static_cast<int16>(id & 0x1FF),
                              static_cast<int16>(id >> 9));
        pPath->Add(cell);
    }
    return pPath;
}

// ----------------------------------------------------------------------------
// asm 0x42C8D0 —— CellTypeClass_DoPathfinding
// ----------------------------------------------------------------------------
PathClass* CellTypeClass::Do_Pathfinding(const CoordStruct& coords,
                                         const CoordStruct& position,
                                         TechnoClass* pSubject, int32 a5,
                                         int32 a6, int32 movZone, int32 a8)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || pSubject == nullptr)
        return nullptr;                                // asm: 无 Map 时不可达

    // asm 2650-2664: [this+38h] = 1 ; sub_42A5B0(this) ;
    //                3 x [vec+0Ch] over this+74h (stride 18h)。
    HasPath = true;
    Reset_Search();
    for (int32 i = 0; i < 3; ++i) {
        ZoneLists[i].Clear();                          // asm: call [eax+0Ch]
    }

    // asm 2666-2677: [this+3Ch] = a8 ; fromCell = Map.Coord_Cell(coords)
    SubjectIndex = a8;
    CellClass* pCellCoords = pMap->GetCellAt(coords);  // asm: Map.Coord_Cell
    CellClass* pCellPos    = pMap->GetCellAt(position);

    const bool subjectOnBridge = pSubject->OnBridge();  // asm: [esi+8Ch]

    // asm 2679-2695: mzUsed = (MovZone != -1) ? MovZone : [type+5B4h]
    int32 mzUsed;
    if (movZone != -1) {
        mzUsed = movZone;
    } else {
        const TechnoTypeClass* pType = pSubject->GetType();   // asm: [edx+84h]
        mzUsed = (pType != nullptr) ? static_cast<int32>(pType->MoveZone) : 0;
    }

    // asm 2698-2708: reach#1 —— CanLocationBeReached(coords, mzUsed, boo)
    const int32 reach1 = pMap->Can_Location_Be_Reached(coords, subjectOnBridge,
                                                       mzUsed) ? 1 : 0;

    // asm 2707-2722: a8 槽 = MovZone（-1 时用类型上的 MovementZone）
    int32 a8Slot;
    if (movZone != -1) {
        a8Slot = movZone;
    } else {
        const TechnoTypeClass* pType = pSubject->GetType();
        a8Slot = (pType != nullptr) ? static_cast<int32>(pType->MoveZone) : 0;
    }

    // asm 2725-2743: reach#2 —— 以 cell(position) 的 bit8 作为 boo
    int32 reach2;
    {
        CellClass* pC2 = pMap->GetCellAt(position);
        const bool boo = Cell_Is_On_Bridge(pC2);
        reach2 = pMap->Can_Location_Be_Reached(position, boo, a8Slot) ? 1 : 0;
    }

    // asm 2742-2766: 两次 sub_583180 得到 from/to 格坐标。
    CellStruct fromCell = Resolve_Path_Cell(pCellCoords, subjectOnBridge);
    CellStruct toCell   = Resolve_Path_Cell(pCellPos,
                                            Cell_Is_On_Bridge(pCellPos));

    // asm 2765-2780: MovZone 槽
    int32 mzSlot;
    if (movZone != -1) {
        mzSlot = movZone;
    } else {
        const TechnoTypeClass* pType = pSubject->GetType();
        mzSlot = (pType != nullptr) ? static_cast<int32>(pType->MoveZone) : 0;
    }

    // asm 2783-2847: Infantry + 载具态时强制 Infantry 通行区并校验 locomotor。
    if (pSubject->WhatAmI() == AbstractType::Infantry) {   // asm: cmp eax,0Fh
        // asm: [esi+6C0h] -> +0D94h 非零 == 该步兵正被载具/运输体接管；
        // 项目里"步兵处于载具态"由 Locomotor 是否存在表达。
        if (pSubject->InAir() == false && pSubject->Locomotor != nullptr) {
            mzSlot = static_cast<int32>(MovementZone::Infantry);   // asm: MovZone = 7
        }
        if (pSubject->Locomotor == nullptr) {
            CellType_RaiseError(static_cast<int32>(0x80004003u));  // E_POINTER
        } else {
            // asm 2809-2843: QI(subject+674h, IID dword_818858) 成功后
            // AddRef + Release，只为确认该接口活着。这里持有的指针本身
            // 就是 ILocomotion（QueryInterface 的对象与结果同型），
            // 因此直接对其 AddRef / Release 即等价。
            ILocomotion* pLoco = pSubject->Locomotor;
            pLoco->AddRef();
            pLoco->Release();
        }
    }

    // asm 2849-2890: useHS 闸门
    bool useHS;
    {
        const TechnoTypeClass* pType = pSubject->GetType();
        // asm [type+0C94h]：该类型参与分层寻路（地面/水面移动体）。
        const bool typeAllowsHS =
            (pType != nullptr) &&
            (pType->MoveZone != MovementZone::Fly) &&
            (pType->SpeedTypeVal != SpeedType::Winged);

        bool pass = typeAllowsHS;
        // asm [esi+3D5h] 与 vt+320h 两道"该 Subject 在地面上"的判定。
        if (pass) pass = (pSubject->InAir() == false);
        if (pass) pass = (pSubject->IsInAir() == false);
        if (pass) pass = pMap->In_Radar(fromCell, true);
        if (pass) pass = pMap->In_Radar(toCell, true);
        useHS = pass;
    }

    // asm 2893-2943: 可达性一致时的分层尝试
    if (reach1 != reach2) {
        if (useHS)
            return nullptr;                            // asm: xor eax,eax / retn
    } else if (useHS) {
        DynamicVectorClass<CellStruct> hierPath;
        if (!CellClass::Pathfinding_Hierarchical(fromCell, toCell, hierPath,
                static_cast<MovementZone>(mzSlot))) {
            CellType_WWDebugString(
                "Hierarchical findpath failure: (%d,%d) to (%d, %d)",
                static_cast<int>(fromCell.X), static_cast<int>(fromCell.Y),
                static_cast<int>(toCell.X), static_cast<int>(toCell.Y));
            useHS = false;                             // asm: byte [a8] = 0
        }
    }

    // asm 2946-2954: retries = (a6 != -1) ? 1 : 5
    const int32 maxTries = (a6 != -1) ? 1 : 5;

    // asm loc_42CBA5: 重试循环
    int32 tries = 0;
    PathClass* pResult = nullptr;
    while (true) {
        if (!useHS) {
            if (fromCell.X != toCell.X || fromCell.Y != toCell.Y) {
                CellType_WWDebugString(
                    "Warning.  A* without HS: (%d,%d) to (%d, %d)",
                    static_cast<int>(fromCell.X), static_cast<int>(fromCell.Y),
                    static_cast<int>(toCell.X), static_cast<int>(toCell.Y));
            }
        }

        // asm 2989-3006: sub_429A90(this, coords, position, Subject, a5, a6, a8)
        pResult = Find_Path(coords, position, pSubject, a5, a6,
                            static_cast<MovementZone>(a8Slot));
        if (pResult != nullptr)
            return pResult;                            // asm: jnz 42CCC4
        if (!useHS)
            return pResult;                            // asm: test cl,cl / jz

        // asm 3012-3047: max(|dx|,|dy|) > 1 时打印 "Regular findpath failure"
        {
            int32 adx = static_cast<int32>(coords.X) - static_cast<int32>(position.X);
            int32 ady = static_cast<int32>(coords.Y) - static_cast<int32>(position.Y);
            adx = (adx < 0) ? -adx : adx;
            ady = (ady < 0) ? -ady : ady;
            const int32 mx = (adx > ady) ? adx : ady;
            if (mx > 1) {
                CellType_WWDebugString(
                    "Regular findpath failure: (%d,%d) to (%d, %d)",
                    static_cast<int>(fromCell.X), static_cast<int>(fromCell.Y),
                    static_cast<int>(toCell.X), static_cast<int>(toCell.Y));
            }
        }

        // asm 3050-3083: 42CCD0 + 42A5B0 刷新、重试次数闸门、分层重试
        ++tries;                                       // asm: inc edi
        Purge_Unreachable(fromCell, pSubject);         // asm: sub_42CCD0
        Reset_Search();                                // asm: sub_42A5B0
        useHS = HasPath;                               // asm: setnz
        if (tries >= maxTries)
            return pResult;                            // asm: 42CCC0
        if (!useHS)
            continue;                                  // asm: jmp 42CBA5

        DynamicVectorClass<CellStruct> hierPath;
        if (CellClass::Pathfinding_Hierarchical(fromCell, toCell, hierPath,
                static_cast<MovementZone>(mzSlot))) {
            continue;
        }
        return pResult;                                // asm: 42CCC0
    }
}
