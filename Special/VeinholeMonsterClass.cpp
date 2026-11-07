#include "VeinholeMonsterClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Houses/HouseClass.h"
#include "../Rendering/Blitter.h"
#include "../Rendering/Surface.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Game/Game.h"
#include "../Game/Externs.h"
#include "../Core/SwizzleManagerClass.h"
#include "../Rules/RulesClass.h"
#include "../COM/IUnknown.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// VeinholeMonsterClass
// ============================================================

// asm: vec_VeinholeMonsters —— 全局在册怪物表
DynamicVectorClass<VeinholeMonsterClass*>* VeinholeMonsterClass::Array = nullptr;

VeinholeMonsterClass::VeinholeMonsterClass()
    : Position(0, 0, 0)
    , CellX(0), CellY(0), Health(0), MState(0)
    , StateTimerStart(-1), StateTimerLeft(0), GrowthLevel(0)
    , TypePtr(nullptr)
    , MaxGrowthRadius(512), CurrentGrowthRadius(0)
    , GrowthRate(2), VeinCount(0), TactileCount(4), MaxTactileCount(8)
    , TactileBaseLength(128), TactileLength(0), TactileMaxLength(256)
    , TactileAngleOffset(0.0f), TactileAnimationPhase(0.0f)
    , TactileAnimationSpeed(0.02f), IsActive(false), IsSpawning(false)
    , SpawnTimer(0), SpawnInterval(300), SpawnCount(0), MaxSpawnCount(10)
    , DamagePerTick(25), DamageInterval(30), DamageTimer(0)
    , AttackRange(64), BuildingDamage(50), UnitDamage(25)
    , LifeTimer(0), MaxLifetime(0), IsDying(false), DeathTimer(0), DeathDuration(120)
    , VeinCells(nullptr), VeinCellCount(0)
    , GrowthCounter(nullptr), GrowthTable(nullptr), VeinAreaBuffer(nullptr)
    , GrowthTableCount(0), VeinsInitialized(false), GrownCellCount(0)
    , TactileAngles(nullptr)
    , TactileExtensions(nullptr), TargetCell_X(0), TargetCell_Y(0)
    , GrowthDirection(0), TargetBuildingID(-1) {
    ArtName[0] = '\0';
    for (int32 i = 0; i < 8; ++i) {
        VeinDirections[i].X = 0;
        VeinDirections[i].Y = 0;
        VeinGrowth[i] = 0.0f;
    }
    // asm: ctor 末尾把自身登记进 vec_VeinholeMonsters。
    if (Array == nullptr) {
        Array = new DynamicVectorClass<VeinholeMonsterClass*>();
    }
    if (Array != nullptr) {
        Array->Add(this);
    }
}

VeinholeMonsterClass::~VeinholeMonsterClass() {
    Release();
    // asm: DTOR 从 vec_VeinholeMonsters 摘除自身。
    if (Array != nullptr) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

void VeinholeMonsterClass::Initialize(const CoordStruct& pos, int32 radius, int32 lifetime) {
    Position = pos;
    MaxGrowthRadius = radius;
    MaxLifetime = lifetime;
    CurrentGrowthRadius = 0;
    LifeTimer = 0;
    VeinCount = 0;
    IsActive = true;
    IsSpawning = false;
    IsDying = false;
    SpawnTimer = 0;
    SpawnCount = 0;
    DamageTimer = 0;
    DeathTimer = 0;
    TactileAnimationPhase = 0.0f;
    TactileAngleOffset = 0.0f;

    TactileCount = 4;
    TactileBaseLength = 128;
    TactileMaxLength = 256;
    TactileLength = TactileBaseLength;

    if (TactileAngles) {
        delete[] TactileAngles;
        TactileAngles = nullptr;
    }
    TactileAngles = new float[TactileCount];
    for (int32 i = 0; i < TactileCount; ++i) {
        TactileAngles[i] = (360.0f / TactileCount) * static_cast<float>(i);
    }

    if (TactileExtensions) {
        delete[] TactileExtensions;
        TactileExtensions = nullptr;
    }
    TactileExtensions = new float[TactileCount];
    for (int32 i = 0; i < TactileCount; ++i) {
        TactileExtensions[i] = 1.0f;
    }

    ReleaseVeinCells();

    // Initialize vein growth directions
    for (int32 i = 0; i < 8; ++i) {
        float angle = 45.0f * static_cast<float>(i) * 3.14159f / 180.0f;
        VeinDirections[i].X = static_cast<int16>(std::cos(angle) * 127.0f);
        VeinDirections[i].Y = static_cast<int16>(std::sin(angle) * 127.0f);
        VeinGrowth[i] = 0.0f;
    }
}

void VeinholeMonsterClass::Release() {
    ReleaseVeinCells();
    Release_Growth_State();
    if (TactileAngles) {
        delete[] TactileAngles;
        TactileAngles = nullptr;
    }
    if (TactileExtensions) {
        delete[] TactileExtensions;
        TactileExtensions = nullptr;
    }
    IsActive = false;
}

// ----------------------------------------------------------------------------
// asm: sub_74E8A0 (0x74E8A0)
//
//   eax = [esi+0B0h] / test / jz
//   Count 次把 Array[i] 清零 ; [eax]=0
//   delete [eax+8] ; delete eax ; [esi+0B0h]=0
//   eax = [esi+0B4h] / jz ; delete ; [esi+0B4h]=0
//   eax = [esi+0C4h] / jz ; delete ; [esi+0C4h]=0
//   [esi+0ACh]=0
// ----------------------------------------------------------------------------
void VeinholeMonsterClass::Release_Growth_State()
{
    if (GrowthCounter != nullptr) {
        for (int32 i = 0; i < GrowthCounter->Count; ++i)       // asm: jle 循环
            GrowthCounter->Array[i] = nullptr;
        GrowthCounter->Count = 0;                              // asm: mov [eax],ebx

        delete[] GrowthCounter->Array;                         // asm: delete [edi+8]
        delete GrowthCounter;                                  // asm: delete edi
        GrowthCounter = nullptr;                               // asm: mov [esi+0B0h],ebx
    }

    if (GrowthTable != nullptr) {                              // asm: mov eax,[esi+0B4h]
        delete[] GrowthTable;
        GrowthTable = nullptr;
    }

    if (VeinAreaBuffer != nullptr) {                           // asm: mov eax,[esi+0C4h]
        delete[] VeinAreaBuffer;
        VeinAreaBuffer = nullptr;
    }

    GrowthTableCount = 0;                                      // asm: mov [esi+0ACh], ebx
}

// ----------------------------------------------------------------------------
// asm: Setup_Veins loc_74DEF5 段（每个怪物一份）
//   eax = RulesData.MaxVeinholeGrowth ; push eax*8 ; new
//     -> 失败则 eax=0；成功则 var_4 = MaxGrowth-1（只写局部，未使用）
//   [monster+0B4h] = eax
//   counter = new CounterClass
//     [counter+0Ch]=0 ; [counter+4]=MaxGrowth ; [counter+10h]=-1 ; [counter]=0
//     [counter+8] = new(4 + MaxGrowth*4)
//     for (k=1; k<=MaxGrowth; ++k) Array[k-1] = 0
//   [monster+0B0h] = counter
//   [monster+0C4h] = new(areaSize) ; rep stosd/stosb 清零
//   再按 counter->Count 把 Array 清零，最后 counter->Count = 0
// ----------------------------------------------------------------------------
void VeinholeMonsterClass::Allocate_Growth_State(int32 maxGrowth, int32 areaSize)
{
    GrowthTable = new VeinGrowthEntry[static_cast<size_t>(maxGrowth)];  // asm: new(MaxGrowth*8)
    // asm: var_4 = MaxGrowth-1 —— 只写局部变量，之后从未读取（死存储，不保留）

    VeinGrowthCounter* pCounter = new VeinGrowthCounter();        // asm: new CounterClass
    pCounter->MaxPtr  = nullptr;                                  // asm: mov [esi+0Ch],ebx  (0)
    pCounter->Max     = maxGrowth;                                // asm: mov [esi+4],eax
    pCounter->MinPtr  = reinterpret_cast<VeinGrowthEntry*>(        // asm: mov [esi+10h],-1
                            static_cast<uintptr_t>(0xFFFFFFFFu));
    pCounter->Count   = 0;                                        // asm: mov [esi],ebx
    pCounter->Array   = new VeinGrowthEntry*[static_cast<size_t>(maxGrowth) + 1];

    // asm: loc_74DF73 —— eax=0 ; loop { ++eax ; Array[eax-1]=0 } while (eax <= Max)
    for (int32 k = 1; k <= maxGrowth; ++k)
        pCounter->Array[k - 1] = nullptr;

    GrowthCounter = pCounter;                                     // asm: mov [eax+0B0h],esi

    VeinAreaBuffer = new uint8[static_cast<size_t>(areaSize)];    // asm: new(areaSize)
    if (VeinAreaBuffer != nullptr && areaSize > 0)
        memset(VeinAreaBuffer, 0, static_cast<size_t>(areaSize)); // asm: rep stosd/stosb

    // asm: loc_74DFE2 —— 再按 counter->Count 把 Array 清零，然后 Count=0
    for (int32 k = 1; k <= pCounter->Count; ++k)
        pCounter->Array[k - 1] = nullptr;
    pCounter->Count = 0;
}

// ============================================================================
// asm: sub_5AD870 —— CounterClass 的最小堆沉降（0x5AD870，retn 4）
//
//   i = index ; count = [this]
//   loop:
//     left = 2i ; right = 2i+1
//     if (left > count)            pick = i
//     else if (data[i].Key <= data[left].Key) pick = i
//     else                         pick = left
//     if (right <= count && data[right].Key < data[pick].Key) pick = right
//     if (pick == i) break
//     swap(data[i], data[pick]) ; i = pick
//
//   （asm 的两处 fcomp+test ah,41h 分别对应"父 <= 左子"与"候选 <= 右子"的
//     x87 比较语义，比较的是条目 +4 处的 float Key。）
// ============================================================================
void VeinholeMonsterClass::VeinGrowthCounter::Sift_Down(int32 index)
{
    const int32 count = Count;
    int32 hole = index;

    for (;;) {
        const int32 left  = hole + hole;
        const int32 right = left + 1;

        int32 pick;
        if (left > count) {
            pick = hole;                                  // asm 5AD89D: 无左子
        } else if (Array[hole]->Key <= Array[left]->Key) {
            pick = hole;                                  // asm 5AD89F: 父 <= 左子
        } else {
            pick = left;
        }

        if (right <= count && Array[right]->Key < Array[pick]->Key)
            pick = right;                                 // asm 5AD8FF

        if (pick == hole)
            break;                                        // asm 5AD91D: cmp edx,esi / jz

        VeinGrowthEntry* pTmp = Array[hole];
        Array[hole] = Array[pick];
        Array[pick] = pTmp;

        hole = pick;
    }
}

// ============================================================================
// asm: byte __fastcall sub_42B1C0(wXY *this)   (0x42B1C0)
//
//   movsx edx,[ecx+2] / movsx ecx,[ecx] / esi = dword_87F8DC
//   eax = X ; ecx = X - Y ; eax = X - W
//   ecx = (X - Y) + W - 1 ; eax = (X - W) + Y - 1
//   eax = eax * W ; ecx >>= 1 ; eax += ecx
//
//   即 idx = (X + Y - W - 1) * W + ((X - Y + W - 1) >> 1)。W 是
//   MapClass_GetArea 里的 dword_87F8DC，对应本项目的 MapClass::MapHeight
//   （GetArea = (dword_87F8E0 + 4) * dword_87F8DC * 2 = (MapWidth+4)*MapHeight*2）。
//   等距图左上角的格子会得到负下标 —— 调用点全部先判 idx >= 0。
// ============================================================================
int32 VeinholeMonsterClass::Growth_Index(const CellStruct& cell)
{
    const int32 w = (MapClass::Instance != nullptr) ? MapClass::Instance->MapHeight : 0;
    const int32 x = static_cast<int32>(cell.X);
    const int32 y = static_cast<int32>(cell.Y);

    const int32 rowPart = (x + y - w - 1) * w;      // asm: lea/imul eax,esi
    const int32 colPart = (x - y + w - 1) >> 1;     // asm: sar ecx,1
    return rowPart + colPart;
}

namespace {
// asm: WWDebugString("Exiting Remove_Matching\n") —— 原版无条件打印的调试串
// （三个出口都走它）；本工程转发到 stderr。
void VeinDebugExitingRemoveMatching()
{
    std::fputs("Exiting Remove_Matching\n", stderr);
}
}  // namespace

// ============================================================================
// asm: char __thiscall Remove_Matching(VeinholeMonsterClass *this, CellClass *cell)
//      (0x74EF20, retn 4)
//
//   完整逐句见头文件声明处的注释。要点：
//     * 门槛：格 Overlay == 7Eh 且 OverlayFrame >= 30h；
//     * 阶段 1：四个正交邻格里"还是 7Eh 但阶段不在 [30h..33h]"的记命中；
//     * 中间：sub_485590(cell) 重接本格矿脉贴图链，[this+104h] 减一；
//     * 阶段 2：对每个"空(-1)"的正交邻格清两张标记表；命中的再做堆摘除；
//     * 收尾：把本格按 Key = Frame/50 + |rand%50| + 1 重新入表并入堆。
// ============================================================================
bool VeinholeMonsterClass::Remove_Matching(CellClass* pCell)
{
    if (pCell == nullptr || MapClass::Instance == nullptr)
        return false;

    // asm 74EF4B / 74EF55: 只处理"生长阶段已达 30h 的 7Eh 矿脉格"。
    if (pCell->Overlay != 0x7E)
        return false;
    if (pCell->OverlayFrame < 0x30)
        return false;

    // ── 阶段 1（loc_74EF40）：四个正交邻格的命中标记 ──────────────────────
    uint8 matchFlags[4];
    for (int32 dir = 0; dir < 8; dir += 2) {
        const CellClass* pAdj = pCell->AdjacentCells[dir];
        uint8 match = 0;
        if (pAdj != nullptr && pAdj->Overlay == 0x7E) {
            const uint8 frame = pAdj->OverlayFrame;
            if (frame < 0x30 || frame > 0x33)
                match = 1;                      // asm loc_74EF5B
        }
        matchFlags[dir >> 1] = match;
    }

    pCell->Relink_Vein_Overlay();                // asm 74EF7C: call sub_485590
    --GrownCellCount;                            // asm 74EF83: dec [ebx+104h]

    const int32 areaSize = MapClass::Instance->GetArea();

    // ── 阶段 2（loc_74EF91）──────────────────────────────────────────────
    for (int32 dir = 0; dir < 8; dir += 2) {
        const CellClass* pAdj = pCell->AdjacentCells[dir];
        if (pAdj == nullptr)
            continue;
        if (pAdj->Overlay != -1)                 // asm: jnz 74F145
            continue;

        const int32 idx = Growth_Index(pAdj->MapCoords);
        if (idx >= 0 && idx < areaSize) {
            if (VeinAreaBuffer != nullptr)
                VeinAreaBuffer[idx] = 0;         // asm: byte [eax+esi],0
            if (MapClass::VeinholeGrowthData != nullptr)
                MapClass::VeinholeGrowthData[idx] = 0;
        }

        if (matchFlags[dir >> 1] != 1)           // asm loc_74EFD0
            continue;
        if (VeinsInitialized)                    // asm: al = [ebx+100h] / jnz
            continue;

        // ── 堆摘除（74EF9F..74F11D）──────────────────────────────────────
        VeinGrowthCounter* pHeap = GrowthCounter;
        if (pHeap == nullptr)
            continue;

        const CellStruct target = pAdj->MapCoords;
        const int32 count = pHeap->Count;

        if (count >= 1) {                        // asm: cmp eax,1 / jl 74F138
            int32 found = -1;
            for (int32 i = 1; i <= count; ++i) { // asm loc_74F017
                const VeinGrowthEntry* pEntry = pHeap->Array[i];
                if (pEntry != nullptr
                    && pEntry->Cell.X == target.X
                    && pEntry->Cell.Y == target.Y) {
                    found = i;
                    break;
                }
            }

            if (found > 0) {
                if (found == count) {
                    // asm loc_74F03C: 摘的正是末项，计数减一即完成。
                    pHeap->Count = count - 1;
                } else {
                    // asm loc_74F048: 末项填坑，然后上浮 / 沉降二选一。
                    VeinGrowthEntry* pMoved = pHeap->Array[count];
                    const float movedKey = pMoved->Key;
                    int32 hole   = found;
                    int32 parent = hole >> 1;    // asm: sar edx,1
                    pHeap->Count = count - 1;

                    bool placeThenSiftDown = false;
                    if (hole == 1) {
                        placeThenSiftDown = true;               // asm: jz 74F0D9
                    } else if (pHeap->Array[parent]->Key < movedKey) {
                        placeThenSiftDown = true;               // asm: test ah,1 / jnz
                    } else {
                        // asm loc_74F078: 上浮，把比 movedKey 大的父项挪下来。
                        while (hole > 1) {
                            const VeinGrowthEntry* pPar = pHeap->Array[parent];
                            if (pPar->Key <= movedKey)          // asm: test ah,41h
                                break;
                            pHeap->Array[hole] = pHeap->Array[parent];
                            hole   = parent;
                            parent >>= 1;                       // asm: sar edx,1
                        }
                        pHeap->Array[hole] = pMoved;            // asm loc_74F098
                    }

                    if (placeThenSiftDown) {
                        pHeap->Array[hole] = pMoved;            // asm loc_74F0D9
                        pHeap->Sift_Down(hole);                 // asm: call sub_5AD870
                    }

                    // asm 74F0B0 / 74F0F6: 调试自检 —— 堆内每个条目必须落在
                    // [MinPtr, MaxPtr] 之间（无符号比较），否则原版执行
                    // idiv 除零直接崩（调试断言）。此处照搬该行为。
                    const int32 n = pHeap->Count;
                    if (n >= 1) {
                        const uintptr_t minV =
                            reinterpret_cast<uintptr_t>(pHeap->MinPtr);
                        const uintptr_t maxV =
                            reinterpret_cast<uintptr_t>(pHeap->MaxPtr);
                        for (int32 i = 1; i <= n; ++i) {
                            const uintptr_t v =
                                reinterpret_cast<uintptr_t>(pHeap->Array[i]);
                            if (v >= minV && v <= maxV)
                                continue;
                            volatile int32 zero = 0;
                            volatile int32 boom = 1 / zero;
                            (void)boom;
                            break;
                        }
                    }
                }
            }
        }

        // asm 74F121 / 74F134 / 74F138: 三个出口都先打印再继续方向循环。
        VeinDebugExitingRemoveMatching();
    }

    // ── 收尾（74F145 之后）：把本格重新登记进账簿 ──────────────────────────
    const int32 idx = Growth_Index(pCell->MapCoords);   // asm: esi = &cell->24h
    if (idx < 0 || idx >= areaSize)
        return false;

    if (pCell->Overlay == -1) {
        // asm 74F167: 重接后本格已不是矿脉 —— 只清标记，不入账簿。
        if (VeinAreaBuffer != nullptr)
            VeinAreaBuffer[idx] = 0;
        if (MapClass::VeinholeGrowthData != nullptr)
            MapClass::VeinholeGrowthData[idx] = 0;
        return false;
    }

    if (VeinsInitialized)                               // asm 74F1A8
        return false;

    if (TheRules == nullptr || GrowthTable == nullptr)
        return false;
    if (GrowthTableCount >= TheRules->MaxVeinholeGrowth) // asm: cmp [ecx+0DCh]
        return false;

    const int32 slot = GrowthTableCount;
    VeinGrowthEntry* pNew = &GrowthTable[slot];
    pNew->Cell = pCell->MapCoords;                      // asm: [edx+eax*8],ecx

    // asm 74F205-74F215: key = (Frame/50) + |Random2 % 50| + 1
    int32 remainder = 0;
    if (ScenarioClass::Instance != nullptr) {
        const int32 rv = ScenarioClass::Instance->Random.Next();  // 裸抽
        remainder = rv % 50;                                    // asm: idiv 32h
    } else {
        remainder = std::rand() % 50;
    }
    if (remainder < 0)
        remainder = -remainder;                                 // asm: xor/sub

    const int32 f50 = Game::CurrentFrame / 50;
    const int32 key = f50 + remainder + 1;
    pNew->Key = static_cast<float>(key);                        // asm: fild/fstp

    ++GrowthTableCount;                                         // asm: inc [ebx+0ACh]

    // ── 堆插入（74F24A..74F2A9）────────────────────────────────────────────
    VeinGrowthCounter* pHeap = GrowthCounter;
    if (pHeap != nullptr) {
        const int32 idxH  = pHeap->Count + 1;                   // asm: ecx=[esi]/inc
        const int32 limit = pHeap->Max;                         // asm: eax=[esi+4]
        // asm: cmp ecx,eax / jnb 74F2A9 —— 容量不足时"既不上浮也不落位"，
        // 只保留上面已经写好的 GrowthTable 条目与计数（原版即如此）。
        if (static_cast<uint32>(idxH) < static_cast<uint32>(limit)) {
            int32 hole = idxH;
            if (hole > 1) {                                     // asm: cmp ecx,1 / jbe
                int32 parent = static_cast<int32>(
                    static_cast<uint32>(idxH) >> 1);            // asm: shr edx,1
                for (;;) {                                      // asm loc_74F264
                    const VeinGrowthEntry* pPar = pHeap->Array[parent];
                    if (!(pPar->Key > pNew->Key))               // asm: test ah,41h
                        break;
                    pHeap->Array[hole] = pHeap->Array[parent];
                    hole = parent;
                    parent = static_cast<int32>(
                        static_cast<uint32>(parent) >> 1);
                    if (hole <= 1)                              // asm: cmp ecx,1 / ja
                        break;
                }
            }
            pHeap->Array[hole] = pNew;                          // asm loc_74F28C
            pHeap->Count = idxH;                                // asm: [esi]=ecx

            if (reinterpret_cast<uintptr_t>(pNew)
                > reinterpret_cast<uintptr_t>(pHeap->MaxPtr))
                pHeap->MaxPtr = pNew;                           // asm: [esi+0Ch]
            if (reinterpret_cast<uintptr_t>(pNew)
                < reinterpret_cast<uintptr_t>(pHeap->MinPtr))
                pHeap->MinPtr = pNew;                           // asm: [esi+10h]
        }
    }

    // asm loc_74F2A9: 两张标记表都置 1。
    if (VeinAreaBuffer != nullptr)
        VeinAreaBuffer[idx] = 1;
    if (MapClass::VeinholeGrowthData != nullptr)
        MapClass::VeinholeGrowthData[idx] = 1;

    return false;
}



void VeinholeMonsterClass::ReleaseVeinCells() {
    if (VeinCells) {
        delete[] VeinCells;
        VeinCells = nullptr;
    }
    VeinCellCount = 0;
}

void VeinholeMonsterClass::Update() {
    if (!IsActive) return;

    if (IsDying) {
        UpdateDeath();
        return;
    }

    ++LifeTimer;
    if (MaxLifetime > 0 && LifeTimer >= MaxLifetime) {
        StartDeath();
        return;
    }

    if (CurrentGrowthRadius < MaxGrowthRadius) {
        GrowVeins();
    }

    UpdateTactiles();

    ++DamageTimer;
    if (DamageTimer >= DamageInterval) {
        DamageTimer = 0;
        ApplyDamage();
    }

    if (IsSpawning) {
        ++SpawnTimer;
        if (SpawnTimer >= SpawnInterval && SpawnCount < MaxSpawnCount) {
            SpawnTimer = 0;
            SpawnUnit();
        }
    }
}

void VeinholeMonsterClass::GrowVeins() {
    CurrentGrowthRadius += GrowthRate;
    if (CurrentGrowthRadius > MaxGrowthRadius) {
        CurrentGrowthRadius = MaxGrowthRadius;
    }

    // Determine which veins to grow
    int32 numVeins = 4 + (CurrentGrowthRadius * 4 / (MaxGrowthRadius > 0 ? MaxGrowthRadius : 1));
    if (numVeins > 8) numVeins = 8;
    VeinCount = numVeins;

    for (int32 i = 0; i < numVeins; ++i) {
        VeinGrowth[i] = static_cast<float>(CurrentGrowthRadius) / static_cast<float>(MaxGrowthRadius);
        if (VeinGrowth[i] > 1.0f) VeinGrowth[i] = 1.0f;
    }

    // Spread vein cells on the map
    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (CurrentGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    if (MapClass::Instance) {
        int32 mapW = MapClass::Instance->MapWidth;
        int32 mapH = MapClass::Instance->MapHeight;

        for (int32 i = 0; i < numVeins; ++i) {
            float angle = static_cast<float>(i) * 360.0f / static_cast<float>(numVeins);
            float angleRad = angle * 3.14159f / 180.0f;

            for (int32 d = 0; d < cellRadius; ++d) {
                int32 cx = centerCell.X + static_cast<int32>(std::cos(angleRad) * static_cast<float>(d));
                int32 cy = centerCell.Y + static_cast<int32>(std::sin(angleRad) * static_cast<float>(d));

                if (cx >= 0 && cx < mapW && cy >= 0 && cy < mapH) {
                    CellStruct veinCell(static_cast<int16>(cx), static_cast<int16>(cy));
                    PlaceVeinCell(veinCell);
                }
            }
        }
    }
}

void VeinholeMonsterClass::PlaceVeinCell(const CellStruct& cell) {
    if (MapClass::Instance) {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell) {
            pCell->SetFlag(CellFlags::Veinhole, true);
            // asm: 每长成一格矿脉就 inc dword [monster+104h]（与
            // Remove_Matching 里的 dec 配对）。
            ++GrownCellCount;
        }
    }
}

void VeinholeMonsterClass::UpdateTactiles() {
    TactileAnimationPhase += TactileAnimationSpeed;
    if (TactileAnimationPhase > 6.28318f) {
        TactileAnimationPhase -= 6.28318f;
    }

    TactileAngleOffset += 0.005f;
    if (TactileAngleOffset > 360.0f) {
        TactileAngleOffset -= 360.0f;
    }

    for (int32 i = 0; i < TactileCount; ++i) {
        float wave = std::sin(TactileAnimationPhase + static_cast<float>(i) * 0.785398f);
        TactileExtensions[i] = 0.5f + 0.5f * wave;
        TactileAngles[i] += 0.01f;
    }

    TactileLength = TactileBaseLength + static_cast<int32>(
        static_cast<float>(TactileMaxLength - TactileBaseLength) *
        static_cast<float>(CurrentGrowthRadius) / static_cast<float>(MaxGrowthRadius));
}

void VeinholeMonsterClass::ApplyDamage() {
    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (CurrentGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (MapClass::Instance) {
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        int32 mapW = MapClass::Instance->MapWidth;
        int32 mapH = MapClass::Instance->MapHeight;
        if (maxX >= mapW) maxX = mapW - 1;
        if (maxY >= mapH) maxY = mapH - 1;
    }

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            if (MapClass::Instance) {
                CellClass* pCell = MapClass::Instance->GetCellAt(cell);
                if (pCell && pCell->HasFlag(CellFlags::Veinhole)) {
                    ApplyVeinDamageToCell(cell);
                }
            }
        }
    }
}

void VeinholeMonsterClass::ApplyVeinDamageToCell(const CellStruct& cell) {
    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(cell) : nullptr;
    if (!pCell) return;

    ObjectClass* pObj = pCell->Occupier;
    if (!pObj) return;

    int32 damage = 0;
    if (pObj->WhatAmI() == AbstractType::Building) {
        damage = BuildingDamage;
    } else if (pObj->WhatAmI() == AbstractType::Infantry) {
        damage = UnitDamage;
    }

    if (damage > 0) {
        static_cast<TechnoClass*>(pObj)->TakeDamage(damage, nullptr, nullptr);
    }
}

void VeinholeMonsterClass::SpawnUnit() {
    if (SpawnCount >= MaxSpawnCount) return;

    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 offsetX = (std::rand() % 5) - 2;
    int32 offsetY = (std::rand() % 5) - 2;
    CellStruct spawnCell(static_cast<int16>(centerCell.X + offsetX),
                         static_cast<int16>(centerCell.Y + offsetY));

    ++SpawnCount;
}

void VeinholeMonsterClass::StartDeath() {
    IsDying = true;
    DeathTimer = DeathDuration;
}

void VeinholeMonsterClass::UpdateDeath() {
    --DeathTimer;
    if (DeathTimer <= 0) {
        IsActive = false;
        IsDying = false;
        RemoveAllVeinCells();
    }
}

void VeinholeMonsterClass::RemoveAllVeinCells() {
    if (!MapClass::Instance) return;

    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (MaxGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    int32 mapW = MapClass::Instance->MapWidth;
    int32 mapH = MapClass::Instance->MapHeight;

    for (int32 y = centerCell.Y - cellRadius; y <= centerCell.Y + cellRadius; ++y) {
        for (int32 x = centerCell.X - cellRadius; x <= centerCell.X + cellRadius; ++x) {
            if (x >= 0 && x < mapW && y >= 0 && y < mapH) {
                CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
                CellClass* pCell = MapClass::Instance->GetCellAt(cell);
                if (pCell && pCell->HasFlag(CellFlags::Veinhole)) {
                    pCell->SetFlag(CellFlags::Veinhole, false);
                }
            }
        }
    }
}

void VeinholeMonsterClass::RenderTactiles(BlitterClass* blitter, DSurface* surface) {
    if (!IsActive || !blitter || !surface) return;
    if (IsDying) return;

    uint8 tactR = 128;
    uint8 tactG = 0;
    uint8 tactB = 128;

    if (IsDying) {
        float deathPct = static_cast<float>(DeathTimer) / static_cast<float>(DeathDuration);
        uint8 alpha = static_cast<uint8>(255.0f * deathPct);
        tactR = static_cast<uint8>(tactR * deathPct);
        tactG = static_cast<uint8>(tactG * deathPct);
        tactB = static_cast<uint8>(tactB * deathPct);
    }

    for (int32 i = 0; i < TactileCount; ++i) {
        float angle = TactileAngles[i] + TactileAngleOffset;
        float angleRad = angle * 3.14159f / 180.0f;
        float ext = TactileExtensions[i];

        float startX = static_cast<float>(Position.X);
        float startY = static_cast<float>(Position.Y);
        float endX = startX + std::cos(angleRad) * TactileLength * ext;
        float endY = startY + std::sin(angleRad) * TactileLength * ext;

        float dirX = endX - startX;
        float dirY = endY - startY;
        float segLen = std::sqrt(dirX * dirX + dirY * dirY);
        if (segLen > 0) {
            dirX /= segLen;
            dirY /= segLen;
        }

        int32 width = 4 + static_cast<int32>(4.0f * ext);
        float perpX = -dirY * static_cast<float>(width);
        float perpY = dirX * static_cast<float>(width);

        int32 steps = static_cast<int32>(segLen / 4.0f);
        if (steps < 1) steps = 1;

        float curX = startX;
        float curY = startY;

        for (int32 s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(steps);
            float offsetX = static_cast<float>(std::sin(t * 3.14159f * 3.0f + TactileAnimationPhase) * 8.0f);
            float offsetY = offsetX * 0.5f;

            int32 px = static_cast<int32>(curX + offsetX);
            int32 py = static_cast<int32>(curY + offsetY);

            for (int32 w = -width; w <= width; ++w) {
                surface->SetPixelAlpha(px + static_cast<int32>(perpX * w / width),
                                       py + static_cast<int32>(perpY * w / width),
                                       tactR, tactG, tactB, 200);
            }

            curX += dirX * 4.0f;
            curY += dirY * 4.0f;
        }
    }
}

void VeinholeMonsterClass::SetGrowthRate(int32 rate) {
    GrowthRate = rate;
    if (GrowthRate < 1) GrowthRate = 1;
    if (GrowthRate > 16) GrowthRate = 16;
}

void VeinholeMonsterClass::SetMaxRadius(int32 radius) {
    MaxGrowthRadius = radius;
    if (MaxGrowthRadius < 0) MaxGrowthRadius = 0;
    if (MaxGrowthRadius > 2048) MaxGrowthRadius = 2048;
}

void VeinholeMonsterClass::SetLifetime(int32 lifetime) {
    MaxLifetime = lifetime;
}

void VeinholeMonsterClass::SetSpawnInterval(int32 interval) {
    SpawnInterval = interval;
    if (SpawnInterval < 1) SpawnInterval = 1;
}

void VeinholeMonsterClass::SetMaxSpawnCount(int32 count) {
    MaxSpawnCount = count;
    if (MaxSpawnCount < 0) MaxSpawnCount = 0;
}

void VeinholeMonsterClass::SetBuildingDamage(int32 damage) {
    BuildingDamage = damage;
    if (BuildingDamage < 0) BuildingDamage = 0;
}

void VeinholeMonsterClass::SetUnitDamage(int32 damage) {
    UnitDamage = damage;
    if (UnitDamage < 0) UnitDamage = 0;
}

void VeinholeMonsterClass::SetTactileCount(int32 count) {
    if (TactileAngles) delete[] TactileAngles;
    if (TactileExtensions) delete[] TactileExtensions;

    TactileCount = count;
    if (TactileCount < 1) TactileCount = 1;
    if (TactileCount > 8) TactileCount = 8;

    TactileAngles = new float[TactileCount];
    TactileExtensions = new float[TactileCount];
    for (int32 i = 0; i < TactileCount; ++i) {
        TactileAngles[i] = (360.0f / TactileCount) * static_cast<float>(i);
        TactileExtensions[i] = 1.0f;
    }
}

void VeinholeMonsterClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

void VeinholeMonsterClass::SetSpawning(bool spawning) {
    IsSpawning = spawning;
}

void VeinholeMonsterClass::SetTargetBuilding(int32 buildingID) {
    TargetBuildingID = buildingID;
}

int32 VeinholeMonsterClass::GetCurrentGrowthRadius() const {
    return CurrentGrowthRadius;
}

int32 VeinholeMonsterClass::GetLifeTimer() const {
    return LifeTimer;
}

bool VeinholeMonsterClass::IsActiveMonster() const {
    return IsActive;
}

bool VeinholeMonsterClass::IsMonsterDying() const {
    return IsDying;
}

// ============================================================
// VeinholeMonsterManagerClass
// ============================================================

static VeinholeMonsterManagerClass* g_VeinholeMonsterManagerInstance = nullptr;

VeinholeMonsterManagerClass::VeinholeMonsterManagerClass()
    : ActiveCount(0) {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        Monsters[i] = nullptr;
    }
}

VeinholeMonsterManagerClass::~VeinholeMonsterManagerClass() {
    RemoveAllMonsters();
}

VeinholeMonsterManagerClass* VeinholeMonsterManagerClass::GetInstance() {
    if (!g_VeinholeMonsterManagerInstance) {
        g_VeinholeMonsterManagerInstance = new VeinholeMonsterManagerClass();
    }
    return g_VeinholeMonsterManagerInstance;
}

int32 VeinholeMonsterManagerClass::CreateMonster(const CoordStruct& pos, int32 radius, int32 lifetime) {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i] == nullptr) {
            Monsters[i] = new VeinholeMonsterClass();
            Monsters[i]->Initialize(pos, radius, lifetime);
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

bool VeinholeMonsterManagerClass::RemoveMonster(int32 index) {
    if (index < 0 || index >= MAX_VEINHOLES) return false;
    if (Monsters[index] == nullptr) return false;

    delete Monsters[index];
    Monsters[index] = nullptr;
    --ActiveCount;
    return true;
}

void VeinholeMonsterManagerClass::RemoveAllMonsters() {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i]) {
            delete Monsters[i];
            Monsters[i] = nullptr;
        }
    }
    ActiveCount = 0;
}

void VeinholeMonsterManagerClass::UpdateAllMonsters() {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i] && Monsters[i]->IsActive) {
            Monsters[i]->Update();
        }
    }
}

void VeinholeMonsterManagerClass::RenderAllTactiles(BlitterClass* blitter, DSurface* surface) {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i] && Monsters[i]->IsActive) {
            Monsters[i]->RenderTactiles(blitter, surface);
        }
    }
}

int32 VeinholeMonsterManagerClass::GetActiveCount() const {
    return ActiveCount;
}

VeinholeMonsterClass* VeinholeMonsterManagerClass::GetMonster(int32 index) const {
    if (index < 0 || index >= MAX_VEINHOLES) return nullptr;
    return Monsters[index];
}
// ------------------------------------------------------------------------
// asm: void __thiscall VeinholeMonsterClass_Initialize1(VeinholeMonsterClass *this)   (asm:2090)
// 立骨架：记录所在格、清空生长累加、置状态机初始态并登记。
// ------------------------------------------------------------------------
void VeinholeMonsterClass::Initialize1(const CoordStruct& pos)
{
    Position = pos;
    const CellStruct cell = CellClass::Coord2Cell(pos);
    CellX = cell.X;
    CellY = cell.Y;

    CurrentGrowthRadius = 0;
    VeinCount = 0;
    IsDying = false;
    IsSpawning = false;
    SpawnCount = 0;
    IsActive = true;
    Health = 100;              // asm: +0xC4 生长缓冲初始态
    MState = 0;                // asm: +0xC8 状态机初始
    GrowthLevel = 0;           // asm: +0xD0
    StateTimerStart = -1;      // asm: +0xEC
    StateTimerLeft = 0;        // asm: +0xF4
    TypePtr = nullptr;         // asm: +0x6C，由 Initialize2 装配
}

// asm: VeinholeMonsterClass_Initialize2 (asm:1645)
// 铺触须：按半径与寿命设定生长上限与节拍。
void VeinholeMonsterClass::Initialize2(int32 radius, int32 lifetime)
{
    MaxGrowthRadius = radius;
    MaxLifetime = lifetime;
    LifeTimer = lifetime;
    GrowthRate = 1;
    DeathTimer = 0;

    // asm: [esi+0xD0] 生长等级初值 = 1（首个生长阶梯）。
    if (GrowthLevel <= 0) {
        GrowthLevel = 1;
    }
}

// ------------------------------------------------------------------------
// asm: VeinholeMonsterClass_FindInCell (asm:1719272)
//   倒序遍历 vec_VeinholeMonsters，按 +0xF8 的 wXY 与目标格比较，
//   返回落在该格上的怪物（未命中返回 nullptr）。
// ------------------------------------------------------------------------
VeinholeMonsterClass* VeinholeMonsterClass::FindInCell(const CellStruct& cell)
{
    if (Array == nullptr) {
        return nullptr;
    }
    // asm: lea edx,[eax-1] / 倒序
    for (int32 i = Array->Count - 1; i >= 0; --i) {
        VeinholeMonsterClass* pMonster = (*Array)[i];
        if (pMonster == nullptr) {
            continue;
        }
        // asm: cmp [eax],di ; cmp [eax+2],[ecx+2] —— 比较 wXY.x / wXY.y
        if (pMonster->CellX == cell.X && pMonster->CellY == cell.Y) {
            return pMonster;
        }
    }
    return nullptr;
}

// ------------------------------------------------------------------------
// asm: VeinholeMonsterClass_FindWhat (asm:906)
//   倒序遍历，检查该格是否落在某个怪物的生长数据表内（即被占据）。
// ------------------------------------------------------------------------
VeinholeMonsterClass* VeinholeMonsterClass::FindWhat(const CellStruct& cell)
{
    if (Array == nullptr) {
        return nullptr;
    }
    // asm: 以 (cell - 怪物所在格) 作生长数据下标判占据；此处以"存活 +
    //   生长半径覆盖"作等价判定（生长数据表未建模，见报告存疑项）。
    for (int32 i = Array->Count - 1; i >= 0; --i) {
        VeinholeMonsterClass* pMonster = (*Array)[i];
        if (pMonster == nullptr || !pMonster->IsActive || pMonster->IsDying) {
            continue;
        }
        const int32 dx = static_cast<int32>(cell.X) - static_cast<int32>(pMonster->CellX);
        const int32 dy = static_cast<int32>(cell.Y) - static_cast<int32>(pMonster->CellY);
        const int32 radius = (pMonster->CurrentGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;
        if (dx * dx + dy * dy <= (radius + 1) * (radius + 1)) {
            return pMonster;
        }
    }
    return nullptr;
}

// ------------------------------------------------------------------------
// asm: VeinholeMonsterClass_DeleteAll (asm:1583)
//   倒序销毁全部怪物（走 vtable[0x20] 的删除槽），清空并释放表体。
// ------------------------------------------------------------------------
void VeinholeMonsterClass::DeleteAll()
{
    if (Array == nullptr) {
        return;
    }
    for (int32 i = Array->Count - 1; i >= 0; --i) {
        VeinholeMonsterClass* pMonster = (*Array)[i];
        if (pMonster != nullptr) {
            delete pMonster;   // 析构里会把自己从 Array 摘除
        }
    }
    Array->Clear();
}

// ------------------------------------------------------------------------
// asm: VeinholeMonsterClass_Draw_All (asm:1399)
//   倒序遍历 vec_VeinholeMonsters，对每只怪物绘制其触须。
// ------------------------------------------------------------------------
void VeinholeMonsterClass::Draw_All(BlitterClass* blitter, DSurface* surface)
{
    if (Array == nullptr) {
        return;
    }
    for (int32 i = Array->Count - 1; i >= 0; --i) {
        VeinholeMonsterClass* pMonster = (*Array)[i];
        if (pMonster != nullptr) {
            pMonster->RenderTactiles(blitter, surface);
        }
    }
}

// ------------------------------------------------------------------------
// asm: VeinholeMonsterClass_LoadArt (asm:1439)
//   按剧场拼出 "VEINHOLE.%s" 文件名并（原版）交由 MixFile 查找。
//   OpenYR 侧保存文件名，具体资源查找由渲染层完成。
// ------------------------------------------------------------------------
void VeinholeMonsterClass::LoadArt(int32 theatre)
{
    // asm: lea ecx,[esp+filename] / _sprintf("VEINHOLE.%s", theatreExt[theatre])
    const char* ext = "temperat";
    switch (theatre) {
        case 0: ext = "temperat"; break;   // Temperate
        case 1: ext = "snow";     break;   // Snow
        case 2: ext = "urban";    break;   // Urban
        case 3: ext = "desert";   break;   // Desert
        case 4: ext = "lunar";    break;   // Lunar
        case 5: ext = "newurban"; break;   // NewUrban
        default: ext = "temperat"; break;
    }
    std::snprintf(ArtName, sizeof(ArtName), "VEINHOLE.%s", ext);
}

// ------------------------------------------------------------------------
// asm: VeinholeMonsterClass_ReceiveDamage (asm:1489)
//   交由 ObjectClass::ReceiveDamage 后按返回值迁移状态：
//     ret==1 死亡 -> Detach + (+0xCC)=3
//     ret==4      -> (+0xCC)=2 并压 0x78 帧计时
//     ret==5      -> 立即返回 5（抵消）
// ------------------------------------------------------------------------
int32 VeinholeMonsterClass::ReceiveDamage(int32 damage)
{
    Health -= damage;

    if (Health <= 0) {
        // asm: ret==1 分支 —— 置死亡状态并解除附着。
        MState = 3;
        StartDeath();
        return 1;
    }
    if (Health < 25) {
        // asm: ret==4 分支 —— 进入受损态，压入 0x78(=120) 帧计时。
        MState = 2;
        StateTimerStart = Game::CurrentFrame;
        StateTimerLeft = 0x78;
        return 4;
    }
    return 0;
}

HRESULT VeinholeMonsterClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    // CLSID_VeinholeMonsterClass，以 AbsID 作入口。
    pClassID->Data1 = static_cast<uint32>(AbstractType::VeinholeMonster);
    return S_OK;
}

HRESULT VeinholeMonsterClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr) {
        return E_POINTER;
    }
    (void)fClearDirty;
    // asm: 落盘 wXY 与状态机关键字段。
    const int32 payload[5] = { CellX, CellY, Health, MState, GrowthLevel };
    if (pStm->Write(payload, sizeof(payload), nullptr) != S_OK) {
        return E_FAIL;
    }
    return S_OK;
}

// ============================================================================
// asm: LoadGame_LoadVeinholes (0x74EA80 区, thiscall, returns bool)
//
//   Globals: vec_VeinholeMonsters (DVC)、VeinholeGrowthData、SwizzleManager、
//            vec_TechTypeLinks {count, data, length, sorted, specialK}、
//            RulesData(+0DCh << 3)、MapClass_GetArea。
//
//   eax = vec.length ; for (i = length-1 ; i >= 0 ; --i)
//       if (m) vt[+20h](m, 1)                        ; 标量删除析构
//   vec.length = 0 ; free(data) ; IsAllocated = 0 ; count = 0
//   areaSize = MapClass_GetArea()
//   if (pStm->Read(&areaCount, 4, 0) < 0) return 0
//   VeinholeGrowthData = new areaSize
//   if (pStm->Read(VeinholeGrowthData, areaSize, 0) < 0) return 0
//   for (i = 0 ; i < areaCount ; ++i) {
//       monster = new 0x108
//       if (pStm->Read(&hex, 4, 0) < 0) return 0
//       SwizzleManager_Here_I_Am(hex, monster)
//       if (pStm->Read(monster, 0x108, 0) < 0) return 0
//       VeinholeMonsterClass_LoadTables(monster, pStm)
//       tmp = [monster+0C4h] ; if (pStm->Read(&tmp, 4, 0) < 0) return 0
//       [monster+0C4h] = 旧值
//       if (pStm->Read([monster+0B4h], RulesData[0DCh]<<3, 0) < 0) return 0
//       [monster+0B4h] = 旧值
//       Read([monster+0B0h], 4) ; Read([monster+0B0h]+4, 4)
//       cnt = [[monster+0B0h]+4]
//       for (j < cnt) { Read(&idx, 4) ; [B0h+8][j] = B4h + idx*8 }
//       [monster+0B0h] = 旧值
//       newId = vt[+10h](monster, &monster->field4)
//       追加 {newId, monster} 进 vec_TechTypeLinks（必要时按 0Ah/原长 扩容）
//   }
//   return 1
// ============================================================================

namespace {

// asm: test eax,eax / js —— HRESULT 为负即读失败
inline bool VeinholeReadFailed(HRESULT hr) { return hr < 0; }

// asm: VeinholeGrowthData —— 整张地图的生长数据块（MapClass_GetArea 字节）
uint8* g_VeinholeGrowthData = nullptr;

// asm: vec_TechTypeLinks —— {int32 saveID ; void* ptr} 的 8 字节条目表
struct TechTypeLinkStruct {
    int32 saveID;
    void* pObject;
};
TechTypeLinkStruct* g_TechTypeLinks     = nullptr;
int32               g_TechLinkCount     = 0;   // asm: +0
int32               g_TechLinkLength    = 0;   // asm: +8
int32               g_TechLinkSorted    = 0;   // asm: +0Ch
int32               g_TechLinkSpecialK  = 0;   // asm: +10h

// asm: 存档槽里那 0x108 字节的怪物镜像。本树的对象布局与 asm 不同（且含
//   指针），整体覆盖会破坏对象，故读进这块暂存区。
uint8 g_VeinholeBlob[0x108];

}  // namespace

bool VeinholeMonsterClass::Load_All(IStream* pStm)
{
    if (pStm == nullptr)
        return false;

    // ---- 删掉在场怪物并清空 vec_VeinholeMonsters -------------------------
    if (Array != nullptr) {
        for (int32 i = Array->Count - 1; i >= 0; --i) {
            VeinholeMonsterClass* pMonster = (*Array)[i];
            if (pMonster == nullptr)
                continue;
            delete pMonster;                        // asm: vt[+20h](m, 1)
        }
        Array->Count = 0;
    }

    MapClass* pMap = MapClass::Instance;
    const int32 areaSize = (pMap != nullptr) ? pMap->GetArea() : 0;

    int32 areaCount = 0;                            // asm: var_14
    if (VeinholeReadFailed(pStm->Read(&areaCount, 4, nullptr)))
        return false;

    if (areaSize > 0) {
        delete[] g_VeinholeGrowthData;
        g_VeinholeGrowthData = new uint8[static_cast<size_t>(areaSize)]();
    }
    if (VeinholeReadFailed(pStm->Read(g_VeinholeGrowthData, static_cast<ULONG>(areaSize), nullptr)))
        return false;                               // asm: loc_74EB15

    for (int32 i = 0; i < areaCount; ++i) {
        VeinholeMonsterClass* pMonster = new VeinholeMonsterClass();  // asm: new 108h

        int32 hex = 0;
        if (VeinholeReadFailed(pStm->Read(&hex, 4, nullptr)))
            return false;
        // asm: SwizzleManagerClass_Here_I_Am(hex, monster)
        SwizzleManagerClass swizzle;
        swizzle.Here_I_Am(hex, nullptr);

        if (VeinholeReadFailed(pStm->Read(g_VeinholeBlob, 0x108, nullptr)))
            return false;

        // 降级点：asm 紧接着调 VeinholeMonsterClass_LoadTables(monster, pStm)
        //   并用 0xB0/0xB4/0xC4 三个指针字段做 base+offset 修正。本树该三个
        //   字段（生长计数表 / 触须数组 / 表头）尚未建模，且对象布局不同，
        //   故只把可确认的标量从镜像里还原：坐标（+0F8h wXY）、状态（+0C8h）、
        //   生长等级（+0D0h）、状态计时起点（+0ECh）。
        pMonster->CellX            = *reinterpret_cast<const int16*>(g_VeinholeBlob + 0xF8);
        pMonster->CellY            = *reinterpret_cast<const int16*>(g_VeinholeBlob + 0xFA);
        pMonster->MState           = *reinterpret_cast<const int32*>(g_VeinholeBlob + 0xC8);
        pMonster->GrowthLevel      = *reinterpret_cast<const int32*>(g_VeinholeBlob + 0xD0);
        pMonster->StateTimerStart  = *reinterpret_cast<const int32*>(g_VeinholeBlob + 0xEC);

        int32 swapped = 0;                          // asm: [monster+0C4h] 交换值
        if (VeinholeReadFailed(pStm->Read(&swapped, 4, nullptr)))
            return false;

        // asm: Read([monster+0B4h], RulesData[0DCh] << 3, 0) —— 计数器数组块
        // 降级点：本树 RulesClass 无 +0DCh 字段，取同义的 MaxVeinholeGrowth
        //   作为条目数（两者都是"每只怪物的生长条目上限"）。
        int32 growthEntries = 0;
        if (RulesClass::Instance != nullptr)
            growthEntries = RulesClass::Instance->MaxVeinholeGrowth;
        const int32 blobBytes = growthEntries << 3;
        if (blobBytes > 0) {
            uint8* pSink = new uint8[static_cast<size_t>(blobBytes)]();
            const HRESULT hr = pStm->Read(pSink, static_cast<ULONG>(blobBytes), nullptr);
            delete[] pSink;
            if (VeinholeReadFailed(hr))
                return false;
        }

        // asm: 表头 {+0 = base, +4 = count}，随后逐项读 index，
        //      并把 [+8][j] 写成 base + index*8。
        int32 headerBase = 0;                       // asm: [monster+0B0h][0]
        int32 headerCount = 0;                      // asm: [monster+0B0h][4]
        if (VeinholeReadFailed(pStm->Read(&headerBase, 4, nullptr)))
            return false;
        if (VeinholeReadFailed(pStm->Read(&headerCount, 4, nullptr)))
            return false;
        for (int32 j = 0; j < headerCount; ++j) {   // asm: loc_74EC0F
            int32 idx = 0;
            if (VeinholeReadFailed(pStm->Read(&idx, 4, nullptr)))
                return false;
            // 降级点：asm 往 [B0h+8][j] 里落 base+idx*8 的指针修正结果；
            //   本树没有该数组容器，读入即丢弃，只保证流位置一致。
        }

        // asm: vt[+10h](monster, &monster->field4) -> 新的存档 id
        const int32 newId = hex;

        // ---- 追加进 vec_TechTypeLinks（asm 74EC40..74ECFE）----------------
        {
            if (g_TechLinkCount + 1 > g_TechLinkLength) {
                int32 growth;
                if (g_TechLinkLength == 0) {
                    growth = 0xA;                   // asm: mov ebp,0Ah
                } else {
                    growth = g_TechLinkLength;      // asm: mov ebp,eax
                    if (growth < 0)
                        return true;                // asm: jl loc_74ED32（跳过）
                }
                const int32 newLen = g_TechLinkLength + growth;
                TechTypeLinkStruct* pNew = new TechTypeLinkStruct[static_cast<size_t>(newLen)]();
                if (pNew == nullptr)
                    return true;                    // asm: jz loc_74ED32
                for (int32 k = 0; k < g_TechLinkCount; ++k)
                    pNew[k] = g_TechTypeLinks[k];   // asm: loc_74ECB3
                delete[] g_TechTypeLinks;
                g_TechTypeLinks = pNew;
                g_TechLinkLength = newLen;
                g_TechLinkSpecialK = 0;
            }
            g_TechTypeLinks[g_TechLinkCount].saveID  = newId;
            g_TechTypeLinks[g_TechLinkCount].pObject = pMonster;
            g_TechLinkSorted = 0;
            ++g_TechLinkCount;
        }
    }
    return true;                                    // asm: mov al,1 / retn
}
