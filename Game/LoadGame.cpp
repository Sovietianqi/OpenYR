// ============================================================================
// Game/LoadGame.cpp - 存档读入（LoadGame 主流程的 slave1 段）
//
// 逐条对应 gamemd.exe（尤里的复仇 1.001, x86, MSVC6）：
//   LoadGame_slave1   @ 0x67F9F0
//   sub_539AE0        @ 0x539AE0   （slave1 中间调用的特效全局读入）
// 写入侧是 ExtraData0_Save @ 0x67F8B0，两边的字段顺序逐字节一致。
// ============================================================================
#include "LoadGame.h"

#include "SaveGameClass.h"
#include "Externs.h"

#include "Core/SwizzleManagerClass.h"
#include "Math/Timer.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/MissionClass.h"
#include "AI/TagClass.h"
#include "Houses/HouseClass.h"
#include "Rules/RulesClass.h"
#include "Particles/ParticleSystemClass.h"
#include "Rendering/Surface.h"
#include "SW/SuperClass.h"

#include <cstring>

// ---- LoadGame_slave3（地图 + 显示层）段所需的额外头文件 ----------------------
#include "Abstract/BuildingTypeClass.h"
#include "Abstract/OverlayTypeClass.h"
#include "Abstract/SmudgeTypeClass.h"
#include "Abstract/TerrainTypeClass.h"
#include "Animations/AnimTypeClass.h"
#include "COM/IUnknown.h"
#include "Map/MapClass.h"
#include "Rendering/RadarClass.h"
#include "Rendering/SidebarClass.h"
#include "Scenario/ScenarioClass.h"

#include <cstddef>

// ----------------------------------------------------------------------------
// asm: powerup_Weights / powerup_Anims / powerupData / powerupNaval
//   （.data 里四张连续的箱子表，尺寸 4Ch / 4Ch / 98h / 13h）
// ----------------------------------------------------------------------------
int32  Powerup_Weights[POWERUP_TABLE_COUNT] = { 0 };
int32  Powerup_Anims[POWERUP_TABLE_COUNT]   = { 0 };
double Powerup_Data[POWERUP_TABLE_COUNT]    = { 0.0 };
bool   Powerup_Naval[POWERUP_TABLE_COUNT]   = { false };

// asm: dword_B07784 —— 主循环节流用的"下次可执行帧"
int32 dword_B07784 = -1;

namespace {

// ---------------------------------------------------------------------------
// slave1 / sub_539AE0 里每个 `call [eax+0Ch]` 都是 IStream::Read(pv, cb, 0)，
// 返回 HRESULT：负值即失败，后续整段直接返回该负值。本工程的读流是
// LoadGameClass（Read 返回 bool），这里把失败折成 -1，保持"负数即失败"的
// 判定形态与返回值语义。
// ---------------------------------------------------------------------------
constexpr int32 READ_OK     = 0;
constexpr int32 READ_FAILED = -1;

int32 ReadBlock(LoadGameClass& Loader, void* pData, int32 size)
{
    return Loader.Read(pData, size) ? READ_OK : READ_FAILED;
}

// asm: 整个存档里贯穿的 SwizzleManager 实例（.data: SwizzleManager）
SwizzleManagerClass SwizzleManager;

// asm: sub_539AE0 里 `push offset vec_Anims_xxx / push offset SwizzleManager`
template <typename T>
void SwizzleVector(DynamicVectorClass<T*>& Vec)
{
    for (int32 i = 0; i < Vec.Count; ++i)
        SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&Vec.Items[i]));
}

// asm: loc_67FA4B / loc_67FB2F / loc_67FBF2 / loc_539C78 / loc_539D32 /
//   loc_539DEE 六处完全相同的 "读 count → count 次读 4 字节 → 追加进表"。
//   追加走 VectorClass::Add，内部正是原版那段扩容闸门（容量不足 → 看
//   IsAllocated → 看 incrementAmount → vt+8 Resize(cap + inc)）。
template <typename T>
int32 ReadPointerVector(LoadGameClass& Loader, DynamicVectorClass<T*>& Vec)
{
    int32 count = 0;
    int32 rc = ReadBlock(Loader, &count, 4);
    if (rc < 0)
        return rc;

    for (int32 i = 0; i < count; ++i)
    {
        T* pItem = nullptr;
        rc = ReadBlock(Loader, &pItem, 4);
        if (rc < 0)
            return rc;
        Vec.Add(pItem);
    }
    return READ_OK;
}

// 原版这些表都是 .data 里的静态对象，本工程用堆对象承载，首次用到时建出来。
template <typename T>
DynamicVectorClass<T*>& EnsureVector(DynamicVectorClass<T*>*& pVec)
{
    if (pVec == nullptr)
        pVec = new DynamicVectorClass<T*>();
    return *pVec;
}

}  // namespace

// ============================================================================
// asm: sub_539AE0 (0x539AE0, thiscall, IStream* pStm) -> HRESULT
//
//   读闪电风暴 / 心灵控制器两套特效的全局槽位 -> Swizzle 其中的指针槽 ->
//   读三张特效动画表（每张都是 count + 指针数组，读完整表逐项 Swizzle）->
//   最后把超时空特效中转面清空。
//
//   任一读取失败把 IStream::Read 的负值原样带回（asm 的 jl loc_539E95 出口
//   在那种情况下 eax 仍是负数，只有走到末尾才被 mov eax, edi 覆盖成 0）。
// ============================================================================
static int32 Load_SpecialEffects_539AE0(LoadGameClass& Loader)
{
    // asm: LightningStorm_Active（1 字节）
    int32 rc = ReadBlock(Loader, &SuperClass::LightningStorm_Active, 1);
    if (rc < 0)
        return rc;

    // asm: LightningStorm_StartTime / LightningStorm_Duration /
    //      LightningStorm_LightningDeferment（各 4 字节）
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_StartTime, 4);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_Duration, 4);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_LightningDeferment, 4);
    if (rc < 0)
        return rc;

    // asm: LightningStorm_TimeToEnd（1 字节）
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_TimeToEnd, 1);
    if (rc < 0)
        return rc;

    // asm: dword_827FC8 / dword_827FCC（各 4 字节）
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_Field8, 4);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_FieldC, 4);
    if (rc < 0)
        return rc;

    // asm: LightningStorm_Status / PsyDom_Status（各 4 字节）
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_Status, 4);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, &SuperClass::PsyDom_Status, 4);
    if (rc < 0)
        return rc;

    // asm: PsyDom_Anim（4 字节）+ SwizzleManagerClass_Swizzle(&PsyDom_Anim)
    rc = ReadBlock(Loader, &SuperClass::PsyDom_Anim, 4);
    if (rc < 0)
        return rc;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&SuperClass::PsyDom_Anim));

    // asm: LightningStorm_CellCoords / PsyDom_CellCoords（各 4 字节 = 两个 int16）
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_CellCoords, 4);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, &SuperClass::PsyDom_CellCoords, 4);
    if (rc < 0)
        return rc;

    // asm: LightningStorm_Owner（4 字节）+ Swizzle
    rc = ReadBlock(Loader, &SuperClass::LightningStorm_Owner, 4);
    if (rc < 0)
        return rc;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&SuperClass::LightningStorm_Owner));

    // asm: PsyDom_Owner（4 字节）+ Swizzle
    rc = ReadBlock(Loader, &SuperClass::PsyDom_Owner, 4);
    if (rc < 0)
        return rc;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&SuperClass::PsyDom_Owner));

    // asm: vec_Anims_const1 —— count + 指针数组，读完整表逐项 Swizzle
    rc = ReadPointerVector(Loader,
                           EnsureVector(SuperClass::vec_Anims_const1));
    if (rc < 0)
        return rc;
    SwizzleVector(EnsureVector(SuperClass::vec_Anims_const1));

    // asm: vec_Anims_const2
    rc = ReadPointerVector(Loader,
                           EnsureVector(SuperClass::vec_Anims_const2));
    if (rc < 0)
        return rc;
    SwizzleVector(EnsureVector(SuperClass::vec_Anims_const2));

    // asm: vec_Anims_const
    rc = ReadPointerVector(Loader,
                           EnsureVector(SuperClass::vec_Anims_const));
    if (rc < 0)
        return rc;
    SwizzleVector(EnsureVector(SuperClass::vec_Anims_const));

    // asm: mov ChronoScreenEffectSurf, 0（无条件清空，不读流）
    ChronoScreenEffectSurf = nullptr;

    return READ_OK;                      // asm: mov eax, edi / loc_539E95
}

// ============================================================================
// asm: LoadGame_slave1 (0x67F9F0, thiscall, IStream* pStm) -> HRESULT
//
//   1) ParticleSystem_Gas / HouseClass_PlayerPtr / Frame 三个全局槽位
//      （前两个读完整指针后交给 SwizzleManager 回填，Frame 不回填）
//   2) v_CurrentObjects：count + 指针数组，读完逐项 Swizzle
//   3) LandCharacteristics 整表 1B0h 字节
//   4) sub_539AE0（特效全局）
//   5) vec_Tags_0x10：count + 指针数组 + 逐项 Swizzle
//   6) vec_Tags_0x4 ：count + 指针数组 + 逐项 Swizzle
//   7) 箱子四表 4Ch / 4Ch / 98h / 13h
//   8) MissionControls 整表 400h 字节（32 条 × 20h）
//   9) dword_B07784
//
//   返回值：asm 末尾是
//     xor edx,edx / test eax,eax / setnl dl / dec edx / and eax,edx
//   —— 成功（>=0）折成 0，失败保留负的 HRESULT。
// ============================================================================
int32 LoadGame_Slave1(LoadGameClass& Loader)
{
    int32 rc = READ_OK;

    // ------------------------------------------------------------------
    // asm: Read(&ParticleSystem_Gas, 4) ; Swizzle(&ParticleSystem_Gas)
    // ------------------------------------------------------------------
    rc = ReadBlock(Loader, &ParticleSystem_Gas, 4);
    if (rc < 0)
        return rc;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&ParticleSystem_Gas));

    // ------------------------------------------------------------------
    // asm: Read(&HouseClass_PlayerPtr, 4) ; Swizzle(&HouseClass_PlayerPtr)
    // ------------------------------------------------------------------
    rc = ReadBlock(Loader, &HouseClass::Player, 4);
    if (rc < 0)
        return rc;
    SwizzleManager.Swizzle(0, reinterpret_cast<void**>(&HouseClass::Player));

    // ------------------------------------------------------------------
    // asm: Read(&Frame, 4)（不回填）
    // ------------------------------------------------------------------
    rc = ReadBlock(Loader, &FrameTimer::CurrentFrame, 4);
    if (rc < 0)
        return rc;

    // ------------------------------------------------------------------
    // asm: loc_67FA4B —— v_CurrentObjects 追加 count 个对象指针，
    //   loc_67FAC5 再逐项 Swizzle 整张表。
    // ------------------------------------------------------------------
    rc = ReadPointerVector(Loader, EnsureVector(ObjectClass::Array));
    if (rc < 0)
        return rc;
    SwizzleVector(EnsureVector(ObjectClass::Array));

    // ------------------------------------------------------------------
    // asm: Read(&LandCharacteristics, 1B0h)
    // ------------------------------------------------------------------
    {
        static_assert(sizeof(RulesClass::LandTypeCharacteristics)
                              * RulesClass::LAND_TYPE_COUNT == 0x1B0,
                      "LandCharacteristics 整表必须与二进制的 1B0h 一致");
        const int32 landBytes = static_cast<int32>(
            sizeof(RulesClass::LandTypeCharacteristics)
            * RulesClass::LAND_TYPE_COUNT);          // 0x24 * 12 == 0x1B0
        if (TheRules != nullptr)
        {
            rc = ReadBlock(Loader, TheRules->LandCharacteristics, landBytes);
        }
        else
        {
            // 规则库还没建（正常流程不会走到）：仍把 1B0h 字节读掉，保证
            // 后续字段的流位置与 asm 一致。
            uint8 scratch[0x1B0];
            rc = ReadBlock(Loader, scratch, landBytes);
        }
        if (rc < 0)
            return rc;
    }

    // ------------------------------------------------------------------
    // asm: call sub_539AE0（this = 同一个 stream）
    // ------------------------------------------------------------------
    rc = Load_SpecialEffects_539AE0(Loader);
    if (rc < 0)
        return rc;

    // ------------------------------------------------------------------
    // asm: loc_67FB2F —— vec_Tags_0x10 追加 + loc_67FBB4 逐项 Swizzle
    // ------------------------------------------------------------------
    rc = ReadPointerVector(Loader, EnsureVector(TagClass::Array));
    if (rc < 0)
        return rc;
    SwizzleVector(EnsureVector(TagClass::Array));

    // ------------------------------------------------------------------
    // asm: loc_67FBF2 —— vec_Tags_0x4 追加 + loc_67FC76 逐项 Swizzle
    // ------------------------------------------------------------------
    rc = ReadPointerVector(Loader, EnsureVector(TagClass::Array4));
    if (rc < 0)
        return rc;
    SwizzleVector(EnsureVector(TagClass::Array4));

    // ------------------------------------------------------------------
    // asm: 箱子四表整块读入
    // ------------------------------------------------------------------
    rc = ReadBlock(Loader, Powerup_Weights, 0x4C);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, Powerup_Anims, 0x4C);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, Powerup_Data, 0x98);
    if (rc < 0)
        return rc;
    rc = ReadBlock(Loader, Powerup_Naval, 0x13);
    if (rc < 0)
        return rc;

    // ------------------------------------------------------------------
    // asm: Read(&MissionControls, 400h) —— 32 条 × 20h 的任务控制记录。
    //   本工程的任务表是 MissionControlClass::Array（DynamicVectorClass），
    //   记录按 20h 步长连续存放；表已按 Mission::Count 建好时直接整块落进去，
    //   否则先读进临时缓冲、再逐条覆盖已存在的记录（流位置始终与 asm 一致）。
    // ------------------------------------------------------------------
    {
        static_assert(sizeof(MissionControlClass) == 0x20,
                      "MissionControlClass 必须与二进制的 20h 步长一致");
        const int32 missionCount = static_cast<int32>(Mission::Count);
        const int32 missionBytes =
            static_cast<int32>(sizeof(MissionControlClass)) * missionCount;   // 0x400

        if (MissionControlClass::Array.Count >= missionCount
            && MissionControlClass::Array.Items != nullptr)
        {
            rc = ReadBlock(Loader, MissionControlClass::Array.Items, missionBytes);
        }
        else
        {
            uint8 scratch[0x400];
            rc = ReadBlock(Loader, scratch, missionBytes);
            if (rc >= 0)
            {
                for (int32 i = 0; i < MissionControlClass::Array.Count; ++i)
                {
                    std::memcpy(&MissionControlClass::Array.Items[i],
                                scratch + i * static_cast<int32>(sizeof(MissionControlClass)),
                                sizeof(MissionControlClass));
                }
            }
        }
        if (rc < 0)
            return rc;
    }

    // ------------------------------------------------------------------
    // asm: Read(&dword_B07784, 4)
    // ------------------------------------------------------------------
    rc = ReadBlock(Loader, &dword_B07784, 4);

    // asm: xor edx,edx / test eax,eax / setnl dl / dec edx / and eax,edx
    return (rc >= 0) ? 0 : rc;
}

// ============================================================================
// LoadGame_slave3（asm 0x5BE490，925 行）—— 存档载入的第 3 段：地图 + 显示层
//
//   asm 调用点：LoadGame+11A  `push esi ; mov ecx, offset Map ; call ...`
//   即 this = 全局地图对象，第一个栈参是存档流。
//
// 从 Helpers/AsmComplex14.cpp 的绑定+裸偏移版本搬迁而来：引擎触点全部改成
// 直调真实类方法，裸偏移全部换成 MapClass::LoadState 的具名成员（缺的字段
// 已补进 Map/MapClass.h）。逐句对照用 asm 的 loc_5BExxx 标签做锚点。
// ============================================================================

// asm: int_CurrentTheatre —— 当前已装载的剧场号，-1 表示不与任何剧场匹配。
int32 int_CurrentTheatre = -1;

namespace {

// ---- 常量：与 asm 逐字节一致 ------------------------------------------------
constexpr int32 kMapImageSize      = 0x556C;  // asm: push 556Ch
constexpr int32 kHashCapacity      = 0x100;   // asm: mov ebx,100h
constexpr int32 kBucketStep        = 0x14;    // asm: mov dword ptr [esi+0Ch],14h
constexpr int32 kBucketInitStep    = 0xA;     // asm: mov dword ptr [esi+14h],0Ah
constexpr int32 kLineBufferCount   = 0x0D;    // asm: cmp ebx,0Dh
constexpr int32 kRadarCellFlag     = 7;       // asm: mov byte ptr [ecx],7
constexpr int32 kMinimapColBytes   = 10;      // asm: lea edx,[eax+eax*4] / shl edx,1
constexpr int32 kMinimapFlagStride = 4;       // asm: add ecx,4
constexpr int32 kUnsetTheatre      = -1;      // asm: mov int_CurrentTheatre,-1

// asm: sub_56CB80 —— 桶号 = (key & 0xF) | ((key >> 12) & 0xF)，落在 0x100 桶内。
uint32 MapLoadHashIndex(const uint32* pKey)
{
    const uint32 key = *pKey;
    return (key & 0x0F) | ((key >> 12) & 0x0F);
}

// asm: IStream::Read（每个读取点都 `push 0` 作为 pcbRead）。
HRESULT MapLoadStreamRead(IStream* pStm, void* pv, int32 cb)
{
    return pStm->Read(pv, static_cast<ULONG>(cb), nullptr);
}

// 0x1804 字节哈希块：块头是桶数（0x100），后面 0x100 个 0x18 字节的桶。
// 桶的初值与原版 sub_5BEA30(0,0) / sub_58B070(0,0) 完全一致（+14h = 0xA）。
template <typename Bucket>
struct MapLoadHashBlock
{
    int32  Count;
    Bucket Buckets[kHashCapacity];
};

template <typename Bucket>
void* CreateHashBlock(int32 bucketStep)
{
    MapLoadHashBlock<Bucket>* pBlock = new MapLoadHashBlock<Bucket>();
    if (pBlock == nullptr)
        return nullptr;
    pBlock->Count = kHashCapacity;                        // asm: mov [eax],ebx
    for (int32 i = 0; i < kHashCapacity; ++i) {
        Bucket& bucket = pBlock->Buckets[i];
        bucket.Vft = nullptr;                             // 原版写 VectorClass vftable
        bucket.Items = nullptr;                           // asm: [esi+4] = 0
        bucket.Capacity = 0;                              // asm: [esi+8] = 0
        bucket.IsAlloc = 1;                               // asm: byte [esi+0Ch] = 1
        bucket.GrowFlag = 0;                              // asm: byte [esi+0Dh] = 0
        bucket.Count = 0;                                 // asm: [esi+10h] = 0
        bucket.GrowStep = bucketStep;                     // asm: [esi+14h] = 0Ah
    }
    return pBlock;
}

// asm loc_5BE22C / loc_5BE31B：建完块之后按块头计数（0x100）逐桶把 +14h
// 统一改写成 0x14。
template <typename Bucket>
void StampBucketStep(void* pData, int32 step)
{
    MapLoadHashBlock<Bucket>* const pBlock =
        static_cast<MapLoadHashBlock<Bucket>*>(pData);
    if (pBlock == nullptr)
        return;
    for (int32 i = 0; i < pBlock->Count; ++i)
        pBlock->Buckets[i].GrowStep = step;
}

// 释放哈希包：原版先对包内对象走 vtable[0](3)（虚析构），再 delete 外层包
// （asm loc_5BE003 / loc_5BE05F）；包内每个桶的条目缓冲也在此一并回收。
template <typename Bucket>
void ReleaseHashTable(MapLoadHashTable& table)
{
    MapLoadHashBlock<Bucket>* pBlock =
        static_cast<MapLoadHashBlock<Bucket>*>(table.Data);
    if (pBlock != nullptr) {
        for (int32 i = 0; i < kHashCapacity; ++i)
            delete[] pBlock->Buckets[i].Items;
        delete pBlock;
    }
    table.Data = nullptr;
    table.HashFn = nullptr;
    table.Capacity = 0;
    table.GrowStep = 0;
}

// asm 15523-15553：存档侧（MapClass_Save loc_5BE855）是"遍历所有有效格，
// 逐格 OleSaveToStream(cell, pStm)"，所以流里每一段都是一个持久化的
// CellClass。ole32 的 OleLoadFromStream 读的是 [CLSID][IPersistStream 负载]；
// 本工程没有 ole32 的类注册表，因此按同一形态自己解帧：先吃掉这一段 CLSID
// （4+2+2+8 = 16 字节，与 GUID 同宽），再把负载交给真实的 CellClass::Load。
CellClass* LoadPersistedCell(IStream* pStm)
{
    CLSID objectClass;
    std::memset(&objectClass, 0, sizeof(objectClass));
    if (MapLoadStreamRead(pStm, &objectClass,
                          static_cast<int32>(sizeof(objectClass))) < 0)
        return nullptr;

    CellClass* pCell = new CellClass();
    if (pCell == nullptr)
        return nullptr;

    if (!pCell->Load(pStm)) {
        delete pCell;
        return nullptr;
    }
    return pCell;
}

}  // namespace

// ============================================================================
// HRESULT LoadGame_slave3_5BE490(MapClass* pMap, IStream* pStm)
//
//   asm: ecx = this（全局 Map），[esp+4] = pStm；每个读取失败点都 `jl
//   loc_5BE6B8`，把负 HRESULT 原样返回。
// ============================================================================
HRESULT LoadGame_slave3_5BE490(MapClass* pMap, IStream* pStm)
{
    if (pMap == nullptr || pStm == nullptr)
        return E_POINTER;

    // 原版这一整段都挂在一个函数级 SwizzleManager 全局上。
    static SwizzleManagerClass SwizzleManager;

    HRESULT hr = S_OK;

    // ── 阶段 1：清空两个整型向量（+115Ch / +0D4h，vt+0Ch = Clear）──────────
    pMap->LoadState.Dwords115C.Clear();
    pMap->LoadState.DwordsD4.Clear();

    // ── 阶段 2（asm 14899）：地图核心流载入 ───────────────────────────────
    // asm: push pStm ; mov ecx, ebp ; call sub_6AC5D0
    // sub_6AC5D0 是 vtable +78h 的槽，转发到 sub_6568A0 —— 本类自己的流式
    // 载入入口，即 MapClass::Load。
    hr = pMap->Load(pStm);
    if (hr < 0)
        return hr;

    // ── 阶段 3（asm 14908）：读 4 字节保留槽 ──────────────────────────────
    // 原版读完就丢（var_14 之后再没被引用），但流必须前进 4 字节。
    int32 reservedSlot = 0;
    hr = MapLoadStreamRead(pStm, &reservedSlot, 4);
    if (hr < 0)
        return hr;

    // ── 阶段 4（asm 14919）：剧场置 -1，释放格子表 ────────────────────────
    int_CurrentTheatre = kUnsetTheatre;      // asm: mov int_CurrentTheatre,-1
    pMap->Free_Cells();                      // asm: vt+5Ch —— MapClass::Free_Cells

    // ── 阶段 5（asm 14924）：释放雷达两张贴图 ────────────────────────────
    ::operator delete(pMap->LoadState.MinimapColors);   // asm: delete [ebp+70h]
    pMap->LoadState.MinimapColors = nullptr;
    ::operator delete(pMap->LoadState.MinimapFlags);    // asm: delete [ebp+68h]
    pMap->LoadState.MinimapFlags = nullptr;

    // ── 阶段 6（asm 14933-15018）：释放载入期容器 ─────────────────────────
    // 6a: +14h 处的哈希包（先 vtable[0](3)，再 delete）。
    ReleaseHashTable<MapLoadVector<MapLoadHashEntry>>(pMap->LoadState.TileHash);

    // 6b: +8Ch 起的三个 0x18 字节对象（vt+0Ch = Clear）。
    for (int32 i = 0; i < 3; ++i)
        pMap->LoadState.RadarCellList[i].Clear();

    // 6c: 13 个行缓冲（每个 2 字节/字）。
    for (int32 i = 0; i < kLineBufferCount; ++i) {
        ::operator delete(pMap->LoadState.LineBuffers[i]);
        pMap->LoadState.LineBuffers[i] = nullptr;
    }

    // 6d: +80h/+84h/+88h 三张哈希包。
    for (int32 i = 0; i < 3; ++i)
        ReleaseHashTable<MapLoadVector<MapLoadSubzoneEntry>>(
            pMap->LoadState.ZoneHash[i]);

    // 6e: +138h 的格子向量（vt+0Ch = Clear）。本工程的格子表由
    // Free_Cells/Allocate_Cells 承接，这里重走一次释放以保证槽位为空。
    pMap->Free_Cells();

    // ── 阶段 7（asm 15020）：读 0x556C 字节的对象镜像 ─────────────────────
    // 原版把整块镜像直接读进对象本体（push 556Ch ; push ebp），随后由镜像
    // 里的标量驱动后面的重建。本工程 MapClass 已按字段重排，因此只从镜像里
    // 取出本段真正要用的标量，其余字节按原样的长度消费掉。
    {
#pragma pack(push, 1)
        struct MapImage
        {
            uint8  Prefix[0x4C];        // +0x000 vftable / +0x014 哈希 / +0x018 行缓冲
            int32  LineBufferWords;     // +0x04C
            uint8  RecordHeader[0x14];  // +0x050 记录向量的表头
            int32  RecordGrowStep;      // +0x064
            uint8  MinimapFlagsPtr[4];  // +0x068
            int32  MinimapCells;        // +0x06C
            uint8  MinimapColorsPtr[4]; // +0x070
            uint8  ToDwordsStep[0x74];  // +0x074..+0x0E7
            int32  DwordsD4Step;        // +0x0E8
            int32  RectX;               // +0x0EC
            int32  RectY;               // +0x0F0
            int32  RectW;               // +0x0F4
            int32  RectH;               // +0x0F8
            uint8  ToDwords115C[0x1060];   // +0x0FC..+0x115B
            uint8  Dwords115CHeader[0x14]; // +0x115C
            int32  Dwords115CStep;      // +0x1170
            uint8  Rest[0x556C - 0x1174];  // +0x1174..+0x556B
        };
#pragma pack(pop)

        static_assert(offsetof(MapImage, LineBufferWords) == 0x04C, "asm +0x04C");
        static_assert(offsetof(MapImage, RecordGrowStep) == 0x064, "asm +0x064");
        static_assert(offsetof(MapImage, MinimapCells) == 0x06C, "asm +0x06C");
        static_assert(offsetof(MapImage, DwordsD4Step) == 0x0E8, "asm +0x0E8");
        static_assert(offsetof(MapImage, RectX) == 0x0EC, "asm +0x0EC");
        static_assert(offsetof(MapImage, RectH) == 0x0F8, "asm +0x0F8");
        static_assert(offsetof(MapImage, Dwords115CStep) == 0x1170, "asm +0x1170");
        static_assert(sizeof(MapImage) == 0x556C, "asm push 556Ch");

        MapImage* pImage = static_cast<MapImage*>(::operator new(kMapImageSize));
        if (pImage == nullptr)
            return E_POINTER;

        hr = MapLoadStreamRead(pStm, pImage, kMapImageSize);
        if (hr < 0) {
            ::operator delete(pImage);
            return hr;
        }

        pMap->LoadState.LineBufferWords = pImage->LineBufferWords;
        pMap->LoadState.MinimapCells = pImage->MinimapCells;
        pMap->LoadState.RadarRect = Rectangle(pImage->RectX, pImage->RectY,
                                             pImage->RectW, pImage->RectH);
        pMap->LoadState.CellRecords.GrowStep = pImage->RecordGrowStep;
        pMap->LoadState.DwordsD4.GrowStep = pImage->DwordsD4Step;
        pMap->LoadState.Dwords115C.GrowStep = pImage->Dwords115CStep;

        ::operator delete(pImage);
    }

    // ── 阶段 8（asm 15031）：复位 Radar / Power / Sidebar 三个子对象 ───────
    // asm: lea edx,[esp+13h] ; call sub_5BE9B0 —— sub_5BE9B0 的实体是
    // sub_6A4F20（复位雷达/电力/侧边栏运行态）外加一次 vftable 重盖：
    //   [this+0]    = TabClass::`vftable'{for `TabClass'}
    //   [this+5518] = TabClass::`vftable'{for `INoticeSink'}
    // 后两步纯粹是"整块镜像覆盖了对象头"的 ABI 修复；本工程不做整块覆盖，
    // 因此只保留真正会改状态的复位动作。
    if (TheRadar != nullptr)
        TheRadar->Init_Clear();
    if (TheSidebar != nullptr)
        TheSidebar->Init_Clear();

    // ── 阶段 9（asm 15044）：指针回填 ─────────────────────────────────────
    // 9a: 侧边栏 4 个标签页各 75 条假目标记录（原版 4 组 × 0x4B，组内步长
    //     0x34、组间步长 0xF94），逐条交给 SwizzleManager。
    if (TheSidebar != nullptr) {
        for (int32 tab = 0; tab < SidebarClass::MaxTabs; ++tab) {
            for (int32 i = 0; i < SidebarClass::MaxCameosPerTab; ++i) {
                SwizzleManager.Swizzle(
                    0, reinterpret_cast<void**>(
                           &TheSidebar->Tabs[tab].Cameos[i]));  // asm: loc_5BE0D3
            }
        }
    }
    // 9b: +11A4h / +11A8h / +11A0h 三个单槽（顺序就是 asm 的顺序）。
    SwizzleManager.Swizzle(0, &pMap->LoadState.Swizzle11A4);
    SwizzleManager.Swizzle(0, &pMap->LoadState.Swizzle11A8);
    SwizzleManager.Swizzle(0, &pMap->LoadState.Swizzle11A0);

    // ── 阶段 10（asm 15083）：重建格子表 ─────────────────────────────────
    if (!pMap->Allocate_Cells(pMap->MapWidth, pMap->MapHeight))  // asm: vt+58h
        return E_FAIL;
    pMap->Init_Cells();                                          // asm: vt+60h

    // ── 阶段 11（asm 15091）：重建雷达图 ─────────────────────────────────
    // asm: this->vt+70h(&rect@0xEC, 1, 0, 0) —— 即 RadarClass_CreateMap，
    // 本工程落在 RadarClass::CreateEmptyMap(rect, reuse, level, unk)。
    pMap->LoadState.MinimapColors = nullptr;
    pMap->LoadState.MinimapFlags = nullptr;
    if (TheRadar != nullptr)
        TheRadar->CreateEmptyMap(pMap->LoadState.RadarRect, true, 0, false);

    // asm 15103-15123：清掉 vt+70h 留下的东西。
    if (pMap->LoadState.MinimapColors != nullptr) {
        ::operator delete(pMap->LoadState.MinimapColors);
        pMap->LoadState.MinimapColors = nullptr;
    }
    if (pMap->LoadState.MinimapFlags != nullptr) {
        ::operator delete(pMap->LoadState.MinimapFlags);
        pMap->LoadState.MinimapFlags = nullptr;
    }

    // ── 阶段 12（asm 15127）：重建最小地图双数组 ──────────────────────────
    // asm: new(count*10) / new(count*4)，后者逐项写 (7, 0)。
    {
        const int32 cells = pMap->LoadState.MinimapCells;
        pMap->LoadState.MinimapColors = ::operator new(cells * kMinimapColBytes);

        uint8* const pFlags =
            static_cast<uint8*>(::operator new(cells * kMinimapFlagStride));
        if (pFlags != nullptr) {
            for (int32 i = 0; i < cells; ++i) {
                pFlags[i * kMinimapFlagStride] = kRadarCellFlag;
                pFlags[i * kMinimapFlagStride + 1] = 0;
            }
        }
        pMap->LoadState.MinimapFlags = pFlags;
    }

    // ── 阶段 13（asm 15167）：重建 +14h 哈希包 ────────────────────────────
    // asm: 0x10 包(+4 = sub_56CB80, +8 = 0x100, +0Ch = 0x14) 套 0x1804 块，
    // 块内 0x100 个桶由 sub_5BEA30(0,0) 建出，末轮把每个桶的 +14h 改 0x14。
    {
        MapLoadHashTable& table = pMap->LoadState.TileHash;
        table.Data = CreateHashBlock<MapLoadVector<MapLoadHashEntry>>(
            kBucketInitStep);                  // asm: sub_5BEA30 盖 0Ah
        StampBucketStep<MapLoadVector<MapLoadHashEntry>>(table.Data, kBucketStep);
        table.HashFn = &MapLoadHashIndex;      // asm: [esi+4] = sub_56CB80
        table.Capacity = kHashCapacity;        // asm: [esi+8] = 100h
        table.GrowStep = kBucketStep;          // asm: [esi+0Ch] = 14h
    }

    // ── 阶段 14（asm 15240）：三档梯度重建 ────────────────────────────────
    // asm loc_5BE25A：idx = 1..3，每轮
    //   Clear(桶@0x8C + 0x18*(idx-1))；
    //   桶.GrowStep = 雷达矩形高 * 宽 * 4 / (1<<idx)^2  ← 注意是 (1<<idx) 的平方；
    //   新建 0x1804 哈希包（桶由 sub_58B070(0,0) 建，先 0xA 再统一改 0x14）；
    //   包存入 +80h + 4*(idx-1)。
    for (int32 idx = 1; idx <= 3; ++idx) {
        MapLoadVector<uint32>& bucketList =
            pMap->LoadState.RadarCellList[idx - 1];
        bucketList.Clear();                          // asm: vt+0Ch

        // asm: mov eax,[ebp+0F8h] ; imul eax,[ebp+0F4h] ; shl eax,2
        //      mov ecx,esi(1<<idx) ; imul ecx,esi ; idiv ecx
        const int32 area = pMap->LoadState.RadarRect.Width *
                           pMap->LoadState.RadarRect.Height;
        const int32 scale = 1 << idx;
        bucketList.GrowStep = (area << 2) / (scale * scale);

        MapLoadHashTable& table = pMap->LoadState.ZoneHash[idx - 1];
        table.Data = CreateHashBlock<MapLoadVector<MapLoadSubzoneEntry>>(
            kBucketInitStep);
        StampBucketStep<MapLoadVector<MapLoadSubzoneEntry>>(table.Data,
                                                           kBucketStep);
        table.HashFn = &MapLoadHashIndex;
        table.Capacity = kHashCapacity;
        table.GrowStep = kBucketStep;
    }

    // ── 阶段 15（asm 15355）：读最小地图标记数组（count*4）────────────────
    {
        const int32 cells = pMap->LoadState.MinimapCells;
        hr = MapLoadStreamRead(pStm, pMap->LoadState.MinimapFlags,
                               cells * kMinimapFlagStride);
        if (hr < 0)
            return hr;
    }

    // ── 阶段 16（asm 15369）：13 个行缓冲 ─────────────────────────────────
    for (int32 i = 0; i < kLineBufferCount; ++i) {
        const int32 words = pMap->LoadState.LineBufferWords;  // asm: [ebp+4Ch]
        void* const pBuffer = ::operator new(words * 2);
        pMap->LoadState.LineBuffers[i] = pBuffer;
        hr = MapLoadStreamRead(pStm, pBuffer, words * 2);
        if (hr < 0)
            return hr;
    }

    // ── 阶段 17（asm 15398）：16 字节记录表 ───────────────────────────────
    int32 recordCount = 0;
    hr = MapLoadStreamRead(pStm, &recordCount, 4);
    if (hr < 0)
        return hr;

    pMap->LoadState.CellRecords.Clear();          // asm: vt+0Ch on +50h
    for (int32 i = 0; i < recordCount; ++i) {
        MapLoadCellRecord record;                 // asm: 4 个 word 置 0、
        record.Field00 = 0;                       //      标志字节置 0、
        record.Field02 = 0;                       //      末 dword 置 -1
        record.Field04 = 0;
        record.Field06 = 0;
        record.Field08 = 0;
        record.Field0C = -1;
        hr = MapLoadStreamRead(pStm, &record,
                               static_cast<int32>(sizeof(record)));
        if (hr < 0)
            return hr;
        pMap->LoadState.CellRecords.Append(record);
    }

    // ── 阶段 18（asm 15495）：释放单元表 ─────────────────────────────────
    // asm loc_5BE494：遍历 +13Ch 表的 +140h 项，非空项走 vtable+20h(1) 释放该
    // 格持有的对象引用后把槽位置空。本工程的格子表是连续数组，槽位本身就是
    // 单元格实例，因此对应动作是把三处"瞬态对象引用"清掉：
    //   Occupier（格子占用者）、FirstObject / AltObject（两层占用链头）。
    for (int32 i = 0; i < pMap->CellCount; ++i) {
        CellClass& cell = pMap->CellArray[i];
        cell.Occupier = nullptr;
        cell.FirstObject = nullptr;
        cell.AltObject = nullptr;
    }

    // ── 阶段 19（asm 15523）：OleLoadFromStream 循环 ─────────────────────
    // 原版把取回的对象指针反复覆盖在同一个栈槽里，既不使用也不 Release
    // （等于泄漏）；这里读回对象后立即释放。
    int32 oleCount = 0;
    hr = MapLoadStreamRead(pStm, &oleCount, 4);
    if (hr < 0)
        return hr;
    for (int32 i = 0; i < oleCount; ++i) {
        CellClass* const pCell = LoadPersistedCell(pStm);
        delete pCell;
    }

    // ── 阶段 20a（asm 15558）：读 +115Ch 的整型流 ────────────────────────
    int32 count = 0;
    hr = MapLoadStreamRead(pStm, &count, 4);
    if (hr < 0)
        return hr;
    for (int32 i = 0; i < count; ++i) {
        int32 value = 0;
        hr = MapLoadStreamRead(pStm, &value, 4);
        if (hr < 0)
            return hr;
        pMap->LoadState.Dwords115C.Append(value);
    }

    // ── 阶段 20b（asm 15636）：读 +0D4h 的整型流 ─────────────────────────
    count = 0;
    hr = MapLoadStreamRead(pStm, &count, 4);
    if (hr < 0)
        return hr;
    for (int32 i = 0; i < count; ++i) {
        int32 value = 0;
        hr = MapLoadStreamRead(pStm, &value, 4);
        if (hr < 0)
            return hr;
        pMap->LoadState.DwordsD4.Append(value);
    }

    // ── 阶段 21（asm 15714）：按剧场重建全部类型表现层 ────────────────────
    {
        const int32 theatre = (TheScenario != nullptr)
                                  ? static_cast<int32>(TheScenario->Theater)
                                  : 0;

        TerrainTypeClass::CreateFromINIList(theatre);   // asm: TerrainTypeClass
        // asm loc_5BE63A：剧场变了走 IsometricTileTypeClass_CreateFromINIList
        // (idx, 1) 重建瓦片集表，没变走 sub_547110 清掉各瓦片集的派生缓存。
        // 两条分支在本工程都收敛到"按该剧场重算瓦片表现层"这一个真实入口。
        pMap->Init_Theater(static_cast<TheaterType>(theatre));
        // asm loc_5BE653: sub_546DA0(0, 0) —— 全图单元格按瓦片重算。
        pMap->Init_Cells();

        OverlayTypeClass::CreateFromINIList();           // asm: 无栈参（ecx = 剧场）
        if (BuildingTypeClass::Array != nullptr) {       // asm: BuildingTypeClass_LoadArt
            for (int32 i = 0; i < BuildingTypeClass::Array->Count; ++i) {
                BuildingTypeClass* const pType = (*BuildingTypeClass::Array)[i];
                if (pType != nullptr)
                    pType->LoadArt();
            }
        }
        AnimTypeClass::CreateFromINIList(theatre);       // asm: AnimTypeClass_
        SmudgeTypeClass::LoadFromINIList(theatre);       // asm: SmudgeTypeClass_

        pMap->LoadState.LoadComplete = false;            // asm: mov [ebp+11BCh],ebx
        int_CurrentTheatre = theatre;                    // asm: mov int_CurrentTheatre,eax
    }

    // asm loc_5BE6B8 的成功出口：eax = 0。
    return S_OK;
}
