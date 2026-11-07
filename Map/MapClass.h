#pragma once

#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Abstract/AbstractClass.h>
#include <Math/CoordStruct.h>
#include <Math/Rectangle.h>
#include <Map/CellClass.h>
#include <Map/CrateClass.h>

class CRCEngine;
class IStream;
class TechnoTypeClass;
class BuildingTypeClass;
class CCINIClass;
class TechnoClass;
class WarheadTypeClass;
class HouseClass;
class ObjectClass;

template <typename T>
class DynamicVectorClass;

// ============================================================================
// 存档载入子对象（asm: LoadGame_slave3 / 0x5BE490，见 Game/LoadGame.cpp）
//
// 原版 Map 对象里这些子对象直接躺在 +0x014…+0x11BC 区间，靠"整块对象镜像"
// （+0x000 起 0x556C 字节）一次性恢复，随后按固定顺序重建。本工程 MapClass
// 按字段布局，因此把它们收进 MapLoadState，由 LoadGame_slave3 逐字段维护。
// ============================================================================

// 原版 VectorClass / DynamicVectorClass 的落位（32 位下 0x18 字节）：
//   +0x00 vftable  +0x04 Items  +0x08 Capacity  +0x0C IsAlloc
//   +0x0D GrowFlag +0x0E 对齐   +0x10 Count     +0x14 GrowStep
template <typename T>
struct MapLoadVector
{
    void* Vft       = nullptr;
    T*    Items     = nullptr;
    int32 Capacity  = 0;
    uint8 IsAlloc   = 1;
    uint8 GrowFlag  = 0;
    int16 Reserved  = 0;
    int32 Count     = 0;
    int32 GrowStep  = 0;

    // asm loc_5BE447 / loc_5BE569 的 CTOR 形态：只把计数归零。
    void Clear() { Count = 0; }

    // 按 asm loc_5BE3E5 起的门限追加：Count < Capacity 直接追加；否则只有
    // (GrowFlag 已置位 或 容量为 0) 且 GrowStep > 0 时才扩容，扩容失败或门限
    // 不满足时该条目被静默丢弃（原版就是这样丢的）。
    bool Append(const T& value)
    {
        if (Count >= Capacity && !Grow(GrowStep))
            return false;
        Items[Count++] = value;
        return true;
    }

    // GrowStep 由存档镜像/重建步骤给出时按 GrowStep 扩容；未给出（<=0）时
    // 退回本工程 DynamicVectorClass 的增长策略（0 → 8，之后翻倍）。
    bool Grow(int32 step)
    {
        if (!(GrowFlag != 0 || Capacity == 0))
            return false;
        const int32 newCapacity =
            (step > 0) ? Capacity + step : (Capacity == 0 ? 8 : Capacity * 2);
        if (newCapacity <= Capacity)
            return false;
        T* const newItems = new T[newCapacity];
        if (newItems == nullptr)
            return false;
        for (int32 i = 0; i < Count; ++i)
            newItems[i] = Items[i];
        delete[] Items;
        Items = newItems;
        Capacity = newCapacity;
        return true;
    }
};

// 0x1804 字节哈希块里的桶（原版 VectorClass<HashObject<uint,uint>>）。
struct MapLoadHashEntry
{
    uint32 Key;
    int32  Value;
};

// 0x1804 字节哈希块里的桶（原版
// VectorClass<HashObject<uint,SubzoneConnectionStruct>>）。
struct MapLoadSubzoneEntry
{
    uint32 Key;
    int32  Zone;
    int32  Subzone;
};

// 原版哈希包（Map 对象 +0x014 / +0x080 / +0x084 / +0x088，共 0x10 字节）：
//   +0x00 数据块  +0x04 哈希函数(sub_56CB80)  +0x08 容量(0x100)  +0x0C 步长(0x14)
struct MapLoadHashTable
{
    void*  Data      = nullptr;
    uint32 (*HashFn)(const uint32* pKey) = nullptr;
    int32  Capacity  = 0;
    int32  GrowStep  = 0;
};

// asm 15416 起逐条读出的 16 字节记录（4 个 word + 标志字节 + 索引）。
struct MapLoadCellRecord
{
    uint16 Field00 = 0;
    uint16 Field02 = 0;
    uint16 Field04 = 0;
    uint16 Field06 = 0;
    uint8  Field08 = 0;
    uint8  Reserved[3] = {0, 0, 0};   // 原版这三字节取自栈上残留，此处保持 0
    int32  Field0C = 0;
};

// ----------------------------------------------------------------------------
// MapLoadState —— 上面那些子对象在本工程里的具名落位。
// 注释里的偏移是原版 Map 对象内的偏移，仅供与 asm 对照；本工程按名取用。
// ----------------------------------------------------------------------------
struct MapLoadState
{
    MapLoadHashTable                 TileHash;          // asm +0x014
    void*                            LineBuffers[13] = {nullptr};  // asm +0x018..+0x04B
    int32                            LineBufferWords = 0;         // asm +0x04C
    MapLoadVector<MapLoadCellRecord> CellRecords;       // asm +0x050
    void*                            MinimapFlags   = nullptr;    // asm +0x068
    int32                            MinimapCells   = 0;          // asm +0x06C
    void*                            MinimapColors  = nullptr;    // asm +0x070
    MapLoadHashTable                 ZoneHash[3];       // asm +0x080 / +0x084 / +0x088
    MapLoadVector<uint32>            RadarCellList[3];  // asm +0x08C / +0x0A4 / +0x0BC
    MapLoadVector<int32>             DwordsD4;          // asm +0x0D4
    MapLoadVector<int32>             Dwords115C;        // asm +0x115C
    void*                            Swizzle11A0 = nullptr;       // asm +0x11A0
    void*                            Swizzle11A4 = nullptr;       // asm +0x11A4
    void*                            Swizzle11A8 = nullptr;       // asm +0x11A8
    bool                             LoadComplete = false;        // asm +0x11BC
    Rectangle                        RadarRect;                   // asm +0x0EC
};

// ============================================================================
// MapClass - The game map manager, singleton
// ============================================================================
class MapClass : public AbstractClass {
public:
    static MapClass* Instance;

    MapClass();
    virtual ~MapClass() noexcept {}

    // AbstractClass overrides
    virtual AbstractType WhatAmI() const { return AbstractType::Map; }
    virtual int32 Size() const { return sizeof(MapClass); }
    virtual int32 GetArrayIndex() const { return 0; }
    virtual bool IsDead() const { return false; }
    virtual HRESULT GetClassID(CLSID* pClassID) override { return 0; }

    // Serialization
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL bSave) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Init
    void Init(int32 maxX, int32 maxY);
    void Init_Clear();
    void Init_Theater(TheaterType theater);
    void Init_Cells();
    void Init_Waypoints();
    void Init_Shroud();

 // MapClass::Shroud_The_Map.  Pushes every cell back into
    // the shrouded state for the given house, then re-reveals whatever that
    // house is legitimately allowed to see.
    void Shroud_The_Map(HouseClass* pHouse);

 // MapClass_Clear_Smudges.  Wipes every smudge / crater
    // decal placed on the map and asks the display to repaint.
    void Clear_Smudges();
    void BuildingToWall(const struct CellStruct& cell);
    void BuildingToOverlay(const struct CellStruct& cell);
    void ClearVectors();

 // MapClass_Sight_From.  Reveals a square of `radius` cells
    // around `coords` for `pHouse`.
    void Sight_From(const CoordStruct& coords, int32 radius, HouseClass* pHouse);

 // MapClass_CanLocationBeReached.  True when the given world
    // position can be reached with the supplied movement zone.
    bool Can_Location_Be_Reached(const CoordStruct& coords, bool a3, int32 zone);

    // ------------------------------------------------------------------------
    // asm: MapClass_wut_0 (真实入口 0x56D10C，retn 18h)
    //
    //   ; char __thiscall MapClass_wut_0(MapClass *this, wXY *where,
    //                                    wXY *position, int MovementZone,
    //                                    char a5, char boo, char a7)
    //
    //   原版符号名就是 MapClass_wut_0（未命名槽），用途是路径寻找里的
    //   "两点是否等价可达"判定：先排除地形边缘、再看两侧的行走区域是否
    //   给出同一个可达结论，从而判断单位能否从 where 走到 position。
    //
    //   逐句（asm 行 781344-781520）：
    //     if (MovementZone == -1)  return 1
    //     radarW = In_Radar(where, 1)                ; skipRange = 1
    //     edgeW  = (where.X + where.Y >  dword_87F8DC)
    //           && (where.X - where.Y <  dword_87F8DC)
    //           && (where.Y - where.X <  dword_87F8DC)
    //           && (where.X + where.Y <= dword_87F8DC + 2*dword_87F8E0)
    //     if (!radarW && edgeW)  return 1
    //     radarP = In_Radar(position, 1)
    //     edgeP  = 同上的 edgeW，只是换成 position 的两个分量
    //     if (a7 && radarW && !radarP && edgeP)  return 1
    //     reachP = CanLocationBeReached(position, MovementZone, boo)
    //     reachW = CanLocationBeReached(where,    MovementZone, a5)
    //     return reachW == reachP
    //
    //   注 1：dword_87F8DC / dword_87F8E0 是一对等距地界常量（地图的
    //         菱形边界），本项目尚未落地；见 .cpp 里的留白说明。
    //   注 2：AsmComplex6 批注把参数写成 (void* pMap, void* pWhere,
    //         void* pPosition, int32 mz, int32 a5, int32 boo, int32 a7)
    //         并把 In_Radar 的参数顺序也抄串了；以 asm 原文为准。
    // ------------------------------------------------------------------------
    char wut_0(const CellStruct& where, const CellStruct& position,
               int32 movementZone, bool a5, bool boo, bool a7);

 // MapClass_FlashCameo (xx0).  Lights up the sidebar cameo for
    // `pType` so the player notices a newly available build option.
    void Flash_Cameo(TechnoTypeClass* pType);

 // MapClass_Init_CellSpread.  Builds the flat (dx, dy) offset
    // table that every area-of-effect action walks: `CellSpreads` holds the
    // number of live entries and CellSpreadTable[2*i] / [2*i+1] the X / Y cell
    // delta of entry i, ordered so that expanding the walk grows a diamond.
    void Init_CellSpread();

 // MapClass_SetTab (xx0).  Switches the sidebar's active tab.
    void Set_Tab(int32 tabIndex);

 // MapClass_Reveal_The_Map (xxx): the inverse of Shroud_The_Map -
    // marks every cell as revealed for the given house (null = all houses).
    void Reveal_The_Map(HouseClass* pHouse);
    bool Allocate_Cells(int32 maxX, int32 maxY);
    void Free_Cells();

    // Cell access
    CellClass* GetCellAt(const CoordStruct& coord);
    CellClass* GetCellAt(const CellStruct& cell);
    CellClass* GetCellAt(int32 x, int32 y);
    CellClass* GetCellAt(int32 cellIndex);
    CellClass* TryGetCellAt(int32 x, int32 y);
    bool IsValidCell(int32 x, int32 y) const;
    bool IsValidCell(int32 cellIndex) const;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Sight_From_3 负责按侦察半径揭开一片区域，并遵守
    //  一个上下限：半径过小按最小值算、过大按最大值算，避免侦察范围失控。
    //  与 Sight_From 不同，它还额外接受一个高度参数，用于把高处俯视的范围
    //  一并纳入（高处看得更远）。
    // ------------------------------------------------------------------------
    void Sight_From_3(const CoordStruct& coords, int32 height, int32 radius, int32 a5);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Init_CellCoords / Init_RoomCoords 是启动期的静态
    //  重置例程：把地图的两个"默认坐标"（格子级默认点与房间级默认点）清零，
    //  保证一局开始时没有任何残留的默认位置。
    // ------------------------------------------------------------------------
    static void Init_CellCoords();
    static void Init_RoomCoords();

    // 根据游戏行为，可知这两个默认坐标是地图在启动期自己保留的"缺省参考
    //  点"：一个以格为单位，一个以世界坐标为单位；重置例程负责把它们清零。
    static CellStruct DefaultCellCoords;
    static CoordStruct DefaultRoomCoords;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 GetCellFloorHeight 负责给出某个世界坐标处的地面
    //  高度：先把坐标限制在地图范围内，换算成所在格，再取该格的地面高度。
    //  坐标越界时按边界格处理。
    // ------------------------------------------------------------------------
    int32 GetCellFloorHeight(const CoordStruct& loc) const;

    // ========================================================================
    // 原版层高常量（dword_89E870 / dword_89E864）
    //   由 MapClass_Init_CellSpread 链按原版公式算出：104 / 416。
    //   供 Damage_At_Cell 的高度门限使用。
    // ========================================================================
    static int32 CellHeightLeptons;   // dword_89E870
    static int32 HalfCellHeight;      // dword_89E864

    // 原版 InitList_0587..0596 链的等价实现：算出一层高度与半层偏置。
    static void InitCellHeightConstants();

    // ========================================================================
    // Damage_At_Cell  —  原版 Damage_At_Cell_Update_489280 (0x489280)
    //
    //   这是"在某点引爆"的统一入口：以世界坐标 loc 为中心，按弹头
    //   CellSpread 逐环收集受影响对象、结算伤害，并处理油桶连锁、
    //   钢/木桥破坏、震屏与粒子。控制流与算术均按原版 1:1 搬运。
    //
    //   参数：
    //     loc          引爆点世界坐标
    //     damage       基础伤害
    //     pSender      伤害来源单位（可为 null，例如地图脚本引爆）
    //     pWarhead     弹头类型（null 时立即返回 1）
    //     a5           是否扣减 tiberium
    //     pOwnerHouse  归属方，用于粒子生成
    //
    //   返回：1 = 空转/未造成伤害；2 = noShake 短路；0 = 造成了伤害。
    // ========================================================================
    int32 Damage_At_Cell(const CoordStruct& loc, int32 damage,
                         TechnoClass* pSender, WarheadTypeClass* pWarhead,
                         bool a5, HouseClass* pOwnerHouse);

    // 原版 GlobalUnitsForSomething：范围收集用的全局对象暂存区。
    static DynamicVectorClass<ObjectClass*> GlobalUnitsForSomething;
    static int32 CountNearbyUnits(CellClass* pCell, int32 spread);

    // ========================================================================
    // asm: gfv_FlushAll (0x4128F0)
    //
    //   清空 400 个 0x18 字节槽的 GlobalUnitsForSomething 表，以及叠在表尾
    //   的 TechnoClass* 向量（dword_889E08 块）。asm 逐段：
    //
    //     push ebx/ebp/esi/edi
    //     mov esi,offset GlobalUnitsForSomething / mov ebp,190h / xor ebx,ebx
    //   loc_412900:                            ; 活跃期：400 槽
    //     mov eax,[esi] / mov edi,[esi+8] / mov ecx,esi
    //     call dword ptr [eax+0Ch]             ; 虚表 +0x0C（清空）
    //     mov edx,[esi] / push ebx / push edi / mov ecx,esi
    //     call dword ptr [edx+8]               ; 虚表 +0x08（释放 data）
    //     add esi,18h / dec ebp / jnz loc_412900
    //     ; -- TechnoClass* 向量（dword_889E08 块）--
    //     eax = dword_889E08 / esi = dword_889E10 / ecx = &dword_889E08
    //     call [eax+0Ch] / edx = dword_889E08 / push ebx / push esi
    //     ecx = &dword_889E08 / call [edx+8]
    //     eax = dword_889E0C / ebp = VectorClass<TechnoClass*>::vftable
    //     cmp eax,ebx / mov dword_889E08,ebp / jz loc_412967
    //     cmp byte_889E15,bl / jz loc_412967
    //     push eax / call delete / add esp,4 / mov dword_889E0C,ebx
    //   loc_412967:
    //     mov byte_889E15,bl / mov dword_889E10,ebx
    //     mov esi,offset dword_889E0C / mov edi,190h
    //   loc_41297D:                            ; 清扫期：399 -> 0
    //     mov eax,[esi-18h] / sub esi,18h / cmp eax,ebx
    //     mov [esi-4],ebp / jz loc_41299A
    //     cmp [esi+9],bl / jz loc_41299A
    //     push eax / call delete / mov [esi],ebx
    //   loc_41299A:
    //     dec edi / mov [esi+9],bl / mov [esi+4],ebx / jnz loc_41297D
    //
    //   OpenYR 侧 GlobalUnitsForSomething 是 DynamicVectorClass<ObjectClass*>，
    //   本方法只做"清空 + 复位标志"这一步（原版的逐槽虚调用是 COM 宿主层的
    //   释放协议，本项目的容器不携带该协议）。铁律①：只做能确证的部分。
    // ========================================================================
    static void Flush_Global_Units();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 IsUnshrouded 负责判断某个世界坐标所在的格子是否
    //  已经不再被黑幕盖着（即已经被任何一方探明）：换算成格号后查该格的
    //  探明标记；格子无效时按"仍然被盖着"处理。
    // ------------------------------------------------------------------------
    bool IsUnshrouded(const CoordStruct& coords) const;

 // 根据游戏行为，可知 Get_Target_Cell 负责下面这段逻辑。
    //
    //  Maps a world coordinate to its cell, returning the scratch fallback
    //  cell when the coordinate lies outside the flat cell table.
    CellClass* GetTargetCell(const CoordStruct& coord);

    // Coordinate conversion
    int32 CoordToCell(const CoordStruct& coord) const;
    CoordStruct CellToCoord(int32 cellIndex) const;
    int32 GetCellX(int32 cellIndex) const;
    int32 GetCellY(int32 cellIndex) const;
    int32 XYToCell(int32 x, int32 y) const;
    CellStruct CellToCellStruct(int32 cellIndex) const;

    // Bounds
    bool IsWithinUsableArea(int32 x, int32 y) const;
    bool IsWithinUsableArea(int32 cellIndex) const;
    bool IsWithinUsableArea(const CoordStruct& coord) const;

    // Waypoints
    CoordStruct GetWaypoint(int32 idx) const;
    void SetWaypoint(int32 idx, const CoordStruct& coord);
    int32 ClosestWaypoint(const CoordStruct& coord) const;

    // Utility
    int32 GetRandomValidCell() const;
    CoordStruct Center_Coord() const;
    bool Is_Placement_Allowed(const CoordStruct& coord) const;
    bool Is_Placement_Allowed(const CellStruct& cell) const;

    // Cell terrain
    LandType GetLandType(const CellStruct& cell) const;
    int32 GetCellSlope(const CellStruct& cell) const;
    int32 GetGroundHeight(const CoordStruct& coord) const;
    void MarkCellOccupied(const CellStruct& cell, bool occupied);
    bool IsCellOccupied(const CellStruct& cell) const;
    ObjectClass* GetCellOccupier(const CellStruct& cell);

    // Bridge
    bool IsBridgeCell(const CellStruct& cell) const;
    bool IsBridgeDestroyed(const CellStruct& cell) const;

    // Damage
    void ApplyDamageArea(const DamageArea& area);
    void CreateCrater(const CellStruct& cell, int32 size);

    // ========================================================================
    // 三个由 TActionClass 触发的"在地图坐标处播放效果"入口。
    //
    // 原版把这三件事写在 TActionClass_Execute 的 case 87 / 88 / 98 里内联，
    // 并没有独立的 MapClass 成员函数；OpenYR 把它们收拢到地图对象上以复用
    // （见 AI/TActionClass.cpp 的调用点）。
    //
    //   Create_Particle_Anim(pos, typeName): 以 pos 为落点，按名字解析出的
    //     ParticleSystemType 新建一个粒子系统（原版 new ParticleSystemClass +
    //     ParticleSystemClass_CTOR(psType, loc, ...)）。
    //   Remove_Particle_Anim(pos): 遍历存活粒子系统表，把落点与 pos 相同的
    //     系统全部撤下（原版遍历 vec_ParticleSystems 后调用虚表 +0xF8）。
    //   Play_Sound_At(pos, index): 在 world 坐标 pos 处播放索引为 index 的
    //     3D 音效（原版 VocClass_PlayIndexAtPos(index, &pos, 1)）。
    // ========================================================================
    void Create_Particle_Anim(const CoordStruct& pos, const char* typeName);
    void Remove_Particle_Anim(const CoordStruct& pos);
    void Play_Sound_At(const CoordStruct& pos, int32 soundIndex);

    // Base zone
    bool Base_Is_Area_Occupied(int32 cellIndex, int32 radius) const;

    // DisplayClass::Read_INI / MapClass_SaveMapToINI cell-tag stage.
    // "CellTags" maps a packed cell index to the name of the tag that owns
    // it.  NewINIFormat >= 4 splits the index as (index / 1000, index % 1000);
    // older maps split it as (index / 128, index % 128).
    void ReadCellTags(CCINIClass* pINI, const char* pSection, int32 newINIFormat);

    // Wall
    void Place_Wall(int32 x, int32 y, int32 overlayIndex);
    void Remove_Wall(int32 x, int32 y);

    // Tiberium
    void Update_Tiberium_Spread();

 // Logic - MapClass::Logic.  The map's per-frame tick; the
    // only work it does is expiry-driven crate respawn, gated on a live
    // session and on crates being enabled.
    void Logic();

    // Crate
    void Update_Crate_Respawn();

    // ========================================================================
    // MapClass::Create_Crate  —  原版 sub_56BEC0 (0x56BEC0)
    //
    //   TActionClass case 107 (lct_TriggerAction_CreateCrate) 的落点：在给定
    //   格生成一只补给箱。与 CrateClass::Create_Crate（真正放置叠加物的那个
    //   成员函数）是两层不同的调用——本方法负责选格、挑空槽位并转发。
    //   逐句（asm 行 779056）：
    //     cell = CellFromIndex((Y<<9)+X)，越界则用 tmpCell
    //     if (cell[+0xEC] == 2)  Nearby_Location(..., a14)   ; 特殊格的搜索参数
    //     else                   Nearby_Location(coords, Foot, zone=-1, ...)
    //     在 0x100 个 0x10 步长的空槽里找 CellCoords==DefaultCellCoords 的槽
    //     if (槽满) return false
    //     if (CrateClass::Create_Crate(slot, coords)) {
    //         if (value != 0x14) cell[+0x11E] = value          ; 记录箱内容值
    //         return true
    //     }
    //     return false
    //
    //   value 为箱子内容取值（P3_Value），0x14 = 随机缺省，记录时跳过。
    // ========================================================================
    bool Create_Crate(const CellStruct& coords, int32 value);

 // Remove_Crate - MapClass::Remove_Crate.  Finds the crate
    // occupying a cell and harvests it.  With a live session the crate table
    // is searched for a matching slot; without one the cell's overlay is
    // checked directly.  The cell must carry a crate overlay, and the overlay
    // must be marked as one the map owns (its Crushable-equivalent "crate"
    // byte).
    bool Remove_Crate(const CellStruct& coords);

 // Place_Random_Crate - MapClass::Place_Random_Crate: pick a
    // random valid cell within the map's crate radius and spawn a crate there.
    bool Place_Random_Crate();

 // Nearby_Location - MapClass::Nearby_Location: the general
    // "find a free cell around this position" search used by crate spawning,
    // unit placement and start-location scans.
    //
    //   position      - the centre of the search.
    //   SpeedType     - the mover's speed class (SpeedType::Foot ..).
    //   a5            - the expected zone index, or -1 to ignore.
    //   MovementZone  - the mover's movement zone (default Normal).
    //   InAir         - the mover is airborne, so only the X/Y ring is checked.
    //   a8, a9        - extra flags forwarded to the cell test.
    //   a10           - forwarded to the cell test.
    //   a11 / a12 / a13 - cell-test options (see CellClass::Is_Clear_To_Move).
    //   a14           - optional output list (up to 24 candidates).
    //   a15           - restrict the search to a single axis.
    //   a16           - require the candidate to be on screen.
    //
    // Points are returned in cell coordinates.
    CellStruct Nearby_Location(const CellStruct& position, int32 SpeedType,
                               int32 a5, MovementZone zone, bool InAir,
                               int32 a8, int32 a9, int32 a10,
                               bool a11, bool a12, bool a13, bool a15, bool a16);

    // Convenience overload mirroring the binary's most common call shape.
    CellStruct Nearby_Location(const CellStruct& position, int32 SpeedType,
                               MovementZone zone);

 // Pick_Random_Location - MapClass::Pick_Random_Location:
    // draw a cell uniformly from the published local rect.
    CellStruct Pick_Random_Location();

 // CellInVisibleArea - MapClass_CellInVisibleArea: isometric
    // visible-region test.  A cell is on screen when it falls inside the
    // diamond formed by the viewport's horizontal and vertical extents:
    //
    //     x + y  <= Right        and   |x - y| < Right
    //     x + y  <= Right + 2*Bottom
    bool IsCellInVisibleArea(int32 cellX, int32 cellY) const;

 // Cell_Region - MapClass::Cell_Region.  Maps a cell to the
    // coarse 4x4 "region" identifier used by the threat grid.  Cells are
    // grouped in blocks of four; the identifier is
    //
    //     region = (y / 4) * 66 + (x / 4) * 2 + 0x83
    //
    // where the flooring divide is the arithmetic shift the binary uses.
    static int32 Cell_Region(const CellStruct& cell);

    // ========================================================================
    // 根据游戏行为，可知 ClearShroud 负责把整张地图在黑幕层面"清空重来"：
    //  遍历所有格，按当前是否有任何一方探明该格，重写每格快照
    //  （FoggedCells / VisibleCells），并递增黑幕版本号，让显示层丢弃旧的
    //  黑幕贴图重新生成。已在所有格上重建完毕时返回 true。
    // ========================================================================
    bool ClearShroud();

    // ========================================================================
    // 根据游戏行为，可知 CreateFog 负责在世界被改动（新建筑、被摧毁的墙体
    //  等）之后重建黑幕数据：它遍历每一格，就"该格是否被黑幕盖住、是否对
    //  本机可见、以及该格是否有单位活动"重新计算快照，并把结果同步进
    //  FoggedCells / VisibleCells 中，最后递增黑幕版本号通知显示层。
    //  与 ClearShroud 的区别在于它同时考虑格子内容（原地形/建筑遮挡）。
    // ========================================================================
    void CreateFog(HouseClass* pHouse);

    // ========================================================================
    // 根据游戏行为，可知 ReshroudAgain 负责在黑幕数据被改动之后把地图"再盖
    //  一遍"：它逐格比较本轮计算出的遮罩状态与上一轮快照，对发生变化的格
    //  向显示层登记一块脏矩形请求重绘，并刷新快照。这样每帧只有真正发生
    //  可见性变化的区域会被重画，避免整图重绘。
    // ========================================================================
    void ReshroudAgain();

    // ========================================================================
    // 根据游戏行为，可知 UpateGap 负责处理"地图上某处出现了一块没被任何
    //  单位探明的空白区域"这一情形：它按临时格记录的位置向四周扩散，把
    //  属于同一片空白、且当前仍然无人探明的格找出来，重新盖回黑幕，并把
    //  该区域的边界交给 ReshroudAgain 去重绘。
    // ========================================================================
    void UpateGap(CellStruct cell);

 // In_Radar - MapClass::In_Radar ( / 0x56BC40 family).  Tests
    // whether a cell falls inside the radar diamond.  With the local extents
    // R = +0xF4 and B = +0xF8 and cell (x, y):
    //
    //     x + y <= R   and   x - y < R   and   y - x < R
    //     x + y <= R + 2*B
    //
    // `skipRange` is accepted and ignored by this variant (the binary's
    // parameter is unused), matching the original signature.
    bool In_Radar(const CellStruct& cell, bool skipRange) const;

 // Cell_Threat - MapClass::Cell_Threat.  Reads the threat
    // value a given house holds for a cell, out of that house's threat grid.
    // The grid is indexed by the same region math as Cell_Region.
    int32 Cell_Threat(const CellStruct& cell, HouseClass* who) const;

    // ========================================================================
    // Visibility / radar / planning probes
    // ========================================================================

 // MapClass_CellExists (xx): true when the cell slot the packed
    // coordinate addresses holds a live CellClass pointer.
    bool CellExists(const CellStruct& cell) const;
 // MapClass_CellInVisibleArea (x): true when the world position
    // lies inside the current visible rectangle.
    bool CellInVisibleArea(const CoordStruct& xyz) const;
 // MapClass_IsCellUsable (xx): the radar-visibility + passability
    // test the cursor code performs before it accepts a click.  `skipRange`
    // short-circuits the range part of MapClass::In_Radar.
    bool IsCellUsable(const CoordStruct& where) const;
 // MapClass_IsCellShrouded (xx): the base map never reports a
    // shrouded cell; scenario/multiplayer overrides refine this.
    bool IsCellShrouded(const CoordStruct& loc) const;
 // MapClass_IsCellTainted (xx): true when the cell the coordinate
    // falls in carries the "tainted" (revealed-by-something) marker.
    bool IsCellTainted(const CoordStruct& loc, bool a3) const;
 // MapClass_IsVisible (0x5656EE, retn 8): reads the cell-visible bit — bit 3
    // of the shroud flag word (+0x12C) — of the cell the coordinate addresses.
    // Out-of-range coordinates resolve to the scratch cell, which reports not
    // visible.
    bool IsVisible(const CellStruct& cell) const;
 // MapClass_GetArea: (MapWidth + 4) * MapHeight * 2 - the
    // size of the per-cell threat grids, in int32 slots.
    int32 GetArea() const;

 // MapClass_IsRadarAvailable (xx): the cached radar-ready byte at
    // +0x14D8.
    bool IsRadarAvailable() const;

    // MapClass_IsPlanningModeActive / NoCanDoInPlanningMode / the cursor
 // predicate ( / 0x63A11E / 0x637DB0): the planning-mode state
    // byte the waypoint planner toggles.
    bool IsPlanningModeActive() const;
    bool Cursor_IsNotPlanningDeploy(int32 cursorType) const;
    void NoCanDoInPlanningMode();

    // ========================================================================
    // Cell iterator
    //
    // 根据游戏行为，可知 CellIterator_Reset 负责把整张地图的"之字形"遍历
    // 指针复位：它从地图最后一行首格开始，随后由 CellIterator_NextCell 一步步
    // 沿 Z 字折线走完所有格子。NextCell 在两条轴上交替推进，走满当前方向后
    // 换到另一条轴，直到两个方向的余量都耗尽为止；每次调用返回当前格的
    // CellClass*。
    // ========================================================================
    void       CellIterator_Reset();
    CellClass* CellIterator_NextCell();

    // ========================================================================
    // 根据游戏行为，可知 LoopOverCells 负责把地图上所有"已成型的矿脉"格
    // 重新铺设一遍：它先扫全图，把所有叠加物为矿脉、且生长阶段达标的格子
    // 收集起来并顺手抹掉叠加物；随后按收集的逆序逐格调用 IsVeins 判定，
    // 命中者重新执行 SetupVeins 让矿脉恢复成完整形态。
    // ========================================================================
    void LoopOverCells();

    // ========================================================================
    // asm: Setup_Veins (0x74DE90) —— 矿脉（Veinhole）生长账簿的建/拆总入口，
    // 由 INIClass_ReadScenario 末尾调用（ecx 为全局 Map，故本函数是 static）。
    //
    //   flag != 0：先对每个矿脉怪物调 sub_74E8A0（拆账簿）、释放全局
    //     VeinholeGrowthData、再对每个怪物按 RulesData.MaxVeinholeGrowth
    //     重建三块账簿；
    //   随后：VeinholeGrowthData 为空则 new(areaSize)；
    //     第一遍全图遍历清掉 CellFlags::Veinhole（bit 8000h）；
    //     把 VeinholeGrowthData 整块清零；
    //     逐个怪物按 byte[+100h] 选路调 Initialize1 / Initialize2；
    //     第二遍全图遍历：Overlay 仍是 0x7E 且该格的生长标记为 0 的，
    //     把 Overlay 改回 -1，并同样清掉 Veinhole 位。
    // ========================================================================
    static void Setup_Veins(int32 flag);

    // asm: VeinholeGrowthData —— 全图一张 areaSize 字节的"该格矿脉已长成"
    //   标记表（全局量，0x74DE90 段与 Remove_Matching 共用）。下标用
    //   VeinholeMonsterClass::Growth_Index（asm sub_42B1C0）换算。
    static uint8* VeinholeGrowthData;

    // ========================================================================
 // Follow-camera state
    // ========================================================================
    ObjectClass* FollowingWhat() const;
    bool FollowThis(ObjectClass* what);

    // ========================================================================
 // Mission timer (xxx)
    // ========================================================================
    // MapClass_TimerPinged: marks the mission timer as "pinged" so the UI
    // stops flashing it, without touching the countdown itself.
    void TimerPinged();
    // MapClass_StopTimerWQ: records the "stopped" flag, clears the pinged
    // state and snapshots the current frame into the timer's stop slot.
    void StopTimerWQ();

    // ========================================================================
    // 根据游戏行为，可知 UpdateRadarStatus 负责在雷达/战术地图状态切换时
    // 更新地图侧的"雷达可用状态"标记（1=激活中、2=关闭中、3=已激活），
    // 并在需要出声时播放激活/关闭音效，同时向调试日志打印一行状态。
    // ========================================================================
    void UpdateRadarStatus(uint8 active, uint8 playSound);

    // ========================================================================
    // 根据游戏行为，可知 SetRadarActivity 负责把外部传来的雷达可用性变化
    // 落到地图上：仅当状态真的发生变化时才改写标记，按新状态打印 on/off，
    // 然后根据当前是否处于战术地图视图，转交给 UpdateRadarStatus 或全局
    // 雷达对象的激活接口。
    // ========================================================================
    void SetRadarActivity(int32 activity);

    // ========================================================================
    // 根据游戏行为，可知 SetAircraftTab 负责在"机场被间谍渗透"等事件触发
    // 时点亮侧边栏的飞机标签页：先比对传入的房屋编号是否就是本机控制的
    // 房屋，命中后置上"已被渗透"标记、把该房屋对应的标签页槽位置位、请求
    // 界面重绘，并在闪烁计时器已过期时把过期帧顺延两帧。
    // ========================================================================
    void SetAircraftTab(int32 houseIndex);

    // ========================================================================
    // 根据游戏行为，可知 PlaceBeacon 负责在指定格插下一支信标：它会先确认
    //  该格处于可用范围内、没有别的单位挡住，然后在格上放置一个"信标"类型的
    //  叠加物，把信标的主人登记到叠加物上，并向显示层登记脏矩形以便立刻画出
    //  信标动画。返回值表示信标是否成功插下。
    // ========================================================================
    bool PlaceBeacon(const CellStruct& cell, HouseClass* pHouse);

    // ========================================================================
    // 根据游戏行为，可知 DestroyCliff 负责"炸掉岩壁"：它检查目标格上是否
    //  真的压着一段可摧毁的岩壁（悬崖）叠加物，是的话把该叠加物抹掉、把
    //  该格的地形恢复成普通地面，并通知附近单位与显示层更新（地面通行性
    //  变了，原先被岩壁挡住的路线可能就此打通）。
    // ========================================================================
    bool DestroyCliff(const CellStruct& cell);

    // ========================================================================
    // 根据游戏行为，可知 RepairBridge_DirA / RepairBridge_DirB 是修桥的两个
    //  半程：游戏把一座桥的修复拆成"从一端铺到中点"和"从中点铺到另一端"
    //  两个方向分别推进。每一步只处理当前方向上尚未铺好的那一格，把它改成
    //  桥面并登记重绘，直到该方向铺完为止。返回 true 表示该方向的这一格
    //  处理完毕、可以继续推进。
    // ========================================================================
    bool RepairBridge_DirA(const CellStruct& cell, HouseClass* pHouse);
    bool RepairBridge_DirB(const CellStruct& cell, HouseClass* pHouse);

    // ========================================================================
    // 根据游戏行为，可知 GetTip 负责给出"鼠标停在这格时该显示哪条提示"：
    //  它按格子当前内容（可建造/不可建造、被占、是墙还是地面、能不能通行）
    //  逐级判定，把对应的提示编号返回给光标层去显示，供玩家判断这一步能
    //  不能操作。没有可用提示时返回 -1。
    // ========================================================================
    int32 GetTip(const CellStruct& cell) const;

    // ========================================================================
    // 根据游戏行为，可知 Init_CellRevealRelations 负责在开局建立一个"格与格
    //  之间探明关系"的查找表：对每个格，预先把它与周围邻居的相互关系（谁
    //  先被探明、谁和谁连通成同一片可见区）算出来并存进表里，供后续的
    //  黑幕扩散/回收直接查表，而不用每帧重新推导。
    // ========================================================================
    void Init_CellRevealRelations();

    // ========================================================================
    // 根据游戏行为，可知 Init_TempCell 负责准备地图的"临时格"：有些查询
    //  （例如给越界坐标找一个落脚格）需要一个不真正属于地图、只用于承载
    //  中间结果的格子对象；这里把它初始化成一张干净的空地，并把坐标摆在
    //  地图之外。
    // ========================================================================
    void Init_TempCell();

    // ========================================================================
    // 根据游戏行为，可知 GetBattlefieldBoundingRectAsswards 负责给出"整个
    //  战场在地图坐标里的外接矩形"：它从所有已知有内容的格（建筑、单位、
    //  地形）里算出最小/最大的 X、Y 范围，返回给调用方用于自动框选、小地图
    //  缩放或摄像机初始定位等用途。
    // ========================================================================
    void GetBattlefieldBoundingRectAsswards(Rectangle* pRect) const;

    // ========================================================================
    // 根据游戏行为，可知 AddObjectToALayer / AddObjectToALayerX 负责把一个
    //  对象登记进它所在格的图层链表：前者按对象自身的坐标算出所在格再登记，
    //  后者直接使用外部已经算好的格号，避免重复换算。登记后该对象才会在地面
    //  绘制、命中判定与遮挡排序里被看到。
    // ========================================================================
    void AddObjectToALayer(ObjectClass* pObject);
    void AddObjectToALayerX(ObjectClass* pObject, int32 cellIndex);

    // ========================================================================
    // 根据游戏行为，可知 CanBuildingTypeBePlacedHere2 是放置判定的"第二步"：
    //  它在初步合法性检查通过之后，进一步验证该建筑放在这里是否真的可行
    //  ——地基覆盖范围内不能压到别的建筑、必须是可建造地面、且满足该建筑
    //  自身对地形/水岸的要求。返回 true 表示这一步也通过了。
    // ========================================================================
    bool CanBuildingTypeBePlacedHere2(BuildingTypeClass* pType, const CellStruct& cell,
                                      HouseClass* pHouse, bool a5) const;

    // ========================================================================
    // 根据游戏行为，可知 SaveMapToINI 负责把当前地图状态写回 INI：包括地图
    //  尺寸与局部坐标、每格的地形/叠加物/资源，以及格上的标记。这样存档或
    //  地图编辑器保存出来的文件才能完整还原这张地图。
    // ========================================================================
    bool SaveMapToINI(CCINIClass* pINI, const char* pSection, bool a3);

    // ========================================================================
    // 根据游戏行为，可知 SetGUIElementPositions 负责把地图相关的界面元素
    //  （侧边栏、雷达、底部信息条的锚点/尺寸）按当前地图形状与分辨率重新
    //  排布一次，保证切换分辨率或加载不同尺寸地图后界面仍然对齐。
    // ========================================================================
    void SetGUIElementPositions();

    // ========================================================================
    // 根据游戏行为，可知 Sight_From_2 是 Sight_From 的变体：同样以给定坐标
    //  为中心揭开一片区域，但它额外接受一个"只对某一方生效"的房屋参数，并
    //  在揭开时只更新该方自己的可见性数据，适合侦察机飞越等"只让自己看见"
    //  的场景。
    // ========================================================================
    void Sight_From_2(const CoordStruct& coords, int32 radius, HouseClass* pHouse);

    // ========================================================================
    // 根据游戏行为，可知 CellSmth0/2/3 是地图在被查询时用到的几个格的
    //  "空壳"占位：wut_0 是给越界坐标兜底的临时格，IsClearCell? 判断某格
    //  是否属于空地（没有地形阻挡、没有建筑）。
    // ========================================================================
    CellClass* CellSmth0(const CellStruct& cell);
    CellClass* CellSmth2(const CellStruct& cell) const;
    int32      CellSmth3(const CellStruct& cell) const;

    // ========================================================================
    // MapClass_IsClearCell? (0x56E7D0)
    //
    //   Walks an h x w block of cells anchored at pStart and reports whether
    //   every one of them is passable.  The anchor is a pair of int16 map
    //   coordinates (not a CellStruct pointer): pStart is dereferenced as
    //   [X, Y] and each step adds the loop counters with 16-bit wraparound,
    //   exactly as the original does.  Out-of-range or unmapped coordinates
    //   fall back to the shared tmpCell scratch cell.
    //
    //   checkOccupant mirrors arg_20: when set, a cell whose occupant slot
    //   (+0x44) is not -1 aborts the walk.
    // ========================================================================
    bool Is_Clear_Cell(const CellStruct& start, int32 h, int32 w,
                       int32 a2, int32 a5, int32 a6, int32 a7,
                       bool a8, bool checkOccupant);

    // Members
    int32       MapWidth;
    int32       MapHeight;
    int32       MapSize;
    int32       CellCount;

    // ── CellSpread table (asm CellSpreads / CellSpread_Table) ─────────────
    // CellSpreads is the number of live cells in the spread; the table is a
    // flat run of int16 (dx, dy) pairs, 369 entries in the original.
    int32       CellSpreads;
    int16       CellSpreadTable[369 * 2];
    CellClass*  CellArray;

    // ── Cell iterator state ───────────────────────────────────────────────
    // 根据游戏行为，可知重置时会把"行内/列内游标"置为 1、把"横向余量"置为
    // 地图宽度、把"纵向余量"置为宽度减一，并把当前格指针摆到最后一行首格。
    // 随后的每一步都在两个轴上交替推进，因此这组变量必须成对存在：
    //   CellIterWidth   一次纵轴推进覆盖的行数（即地图宽度）
    //   CellIterCursorX 纵轴游标（1 基）
    //   CellIterCursorY 横轴游标（1 基）
    //   CellIterRemX    本纵轴步剩余的横向格数
    //   CellIterRemY    本横轴步剩余的纵向格数
    //   CellIterPtr     当前格的原始字节指针（带 4 字节前缀）
    int32       CellIterWidth;
    int32       CellIterCursorX;
    int32       CellIterCursorY;
    int32       CellIterRemX;
    int32       CellIterRemY;
    uint8*      CellIterPtr;
    int32       MaxWaypoints;
    CoordStruct Waypoints[702];
    int32       CrateCount;
    CrateClass  Crate;
    int32       TotalValue;
    int32       VisibleRectX, VisibleRectY, VisibleRectWidth, VisibleRectHeight;
    TheaterType CurrentTheater;

    // The "follow camera" target and its active flag (asm +0x.../anonymous_56
    // and +FollowSomething).
    ObjectClass* FollowSomething;
    bool         FollowingFlag;

    // Cached radar readiness (asm +0x14D8).
    bool         RadarReady;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 +0x14AC 记录雷达/战术地图的当前状态机取值
    // （1=激活中、2=关闭中、3=已激活），SetRadarActivity/UpdateRadarStatus
    // 围绕它做状态迁移；+0x14B0 是一个"是否为战术地图视图"的判别值。
    // ------------------------------------------------------------------------
    int32        RadarStatus;
    int32        RadarMode;   // TacticalMap 视图判别值

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 FlashExpiryFrame 是侧边栏"新物品"闪烁的到期帧，
    // 由 SetAircraftTab 在渗透事件命中本机房屋时顺延。
    // ------------------------------------------------------------------------
    int32        FlashExpiryFrame;

    // ========================================================================
    // 根据游戏行为，可知 FoggedCells / VisibleCells 是本机视角在相邻两帧里
    //  各格"是否被黑幕盖住 / 是否可见"的整图快照：整图刷新时用 CreateFog
    //  重建这两张表，比较时用 ReshroudAgain 生成差异并通知显示层重绘。
    // ========================================================================
    void*        FoggedCells;      // 每格一字节：已被黑幕盖住
    void*        VisibleCells;     // 每格一字节：当前可见

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 shroud 的版本号/脏标记：ClearShroud 与 CreateFog
    //  重建黑幕数据后递增它，让显示层知道要重新生成黑幕贴图。
    // ------------------------------------------------------------------------
    int32        ShroudVersion;

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知这组标记记录"本帧黑幕是否需要整体重建"以及
    //  ReshroudAgain 自身的一次性锁存，避免同一帧重复整图刷新。
    // ------------------------------------------------------------------------
    bool         ShroudDirty;
    bool         ReshroudLatched;

    // Mission-timer state: the "pinged" latch and the frame the timer was
    // stopped at (asm +MissionTimerIsSomething and +TimerWQ).
    bool         MissionTimerPinged;
    int32        MissionTimerStopFrame;

    // Planning (waypoint) mode latch shared with the cursor code, plus the
    // one-shot warning latch used by NoCanDoInPlanningMode (asm +0xAC4C08).
    bool         PlanningModeActive;
    bool         PlanningNoCanDoLatched;

    uint8*      Tilesets;
    int32       TilesetCount;
    int32       unknown_0x1EF8;
    int32       unknown_0x1EFC;
    int32       unknown_0x1F00;
    int32       unknown_0x1F04;
    int32       unknown_0x1F08;
    int32       unknown_0x1F0C;
    int32       unknown_0x1F10;
    int32       unknown_0x1F14;
    int32       unknown_0x1F18;
    int32       unknown_0x1F1C;
    int32       unknown_0x1F20;
    int32       unknown_0x1F24;
    int32       unknown_0x1F28;
    int32       unknown_0x1F2C;
    int32       unknown_0x1F30;
    int32       unknown_0x1F34;
    int32       unknown_0x1F38;
    int32       unknown_0x1F3C;
    int32       unknown_0x1F40;
    int32       unknown_0x1F44;
    int32       unknown_0x1F48;
    int32       unknown_0x1F4C;
    int32       unknown_0x1F50;
    int32       unknown_0x1F54;
    int32       unknown_0x1F58;
    int32       unknown_0x1F5C;
    int32       unknown_0x1F60;
    int32       unknown_0x1F64;
    int32       unknown_0x1F68;
    int32       unknown_0x1F6C;
    int32       unknown_0x1F70;
    int32       unknown_0x1F74;
    int32       unknown_0x1F78;
    int32       unknown_0x1F7C;
    int32       unknown_0x1F80;
    int32       unknown_0x1F84;
    int32       unknown_0x1F88;
    int32       unknown_0x1F8C;
    int32       unknown_0x1F90;
    int32       unknown_0x1F94;
    int32       unknown_0x1F98;
    int32       unknown_0x1F9C;
    int32       unknown_0x1FA0;
    int32       unknown_0x1FA4;
    int32       unknown_0x1FA8;
    int32       unknown_0x1FAC;
    int32       unknown_0x1FB0;
    int32       unknown_0x1FB4;
    int32       unknown_0x1FB8;
    int32       unknown_0x1FBC;
    int32       unknown_0x1FC0;
    int32       unknown_0x1FC4;
    int32       unknown_0x1FC8;
    int32       unknown_0x1FCC;
    int32       unknown_0x1FD0;
    int32       unknown_0x1FD4;
    int32       unknown_0x1FD8;
    int32       unknown_0x1FDC;
    int32       unknown_0x1FE0;
    int32       unknown_0x1FE4;
    int32       unknown_0x1FE8;
    int32       unknown_0x1FEC;
    int32       unknown_0x1FF0;
    int32       unknown_0x1FF4;
    int32       unknown_0x1FF8;
    int32       unknown_0x1FFC;
    int32       unknown_0x2000;
    int32       unknown_0x2004;
    int32       unknown_0x2008;
    int32       unknown_0x200C;
    int32       unknown_0x2010;
    int32       unknown_0x2014;
    int32       unknown_0x2018;
    int32       unknown_0x201C;
    int32       unknown_0x2020;
    int32       unknown_0x2024;
    int32       unknown_0x2028;
    int32       unknown_0x202C;
    int32       unknown_0x2030;
    int32       unknown_0x2034;
    int32       unknown_0x2038;
    int32       unknown_0x203C;
    int32       unknown_0x2040;
    int32       unknown_0x2044;
    int32       unknown_0x2048;
    int32       unknown_0x204C;
    int32       unknown_0x2050;
    int32       unknown_0x2054;
    int32       unknown_0x2058;
    int32       unknown_0x205C;
    int32       unknown_0x2060;
    int32       unknown_0x2064;
    int32       unknown_0x2068;
    int32       unknown_0x206C;
    int32       unknown_0x2070;
    int32       unknown_0x2074;
    int32       unknown_0x2078;
    int32       unknown_0x207C;
    int32       unknown_0x2080;
    int32       unknown_0x2084;
    int32       unknown_0x2088;
    int32       unknown_0x208C;
    int32       unknown_0x2090;
    int32       unknown_0x2094;
    int32       unknown_0x2098;
    int32       unknown_0x209C;
    int32       unknown_0x20A0;
    int32       unknown_0x20A4;
    int32       unknown_0x20A8;
    int32       unknown_0x20AC;
    int32       unknown_0x20B0;
    int32       unknown_0x20B4;
    int32       unknown_0x20B8;
    int32       unknown_0x20BC;
    int32       unknown_0x20C0;
    int32       unknown_0x20C4;
    int32       unknown_0x20C8;
    int32       unknown_0x20CC;
    int32       unknown_0x20D0;
    int32       unknown_0x20D4;
    int32       unknown_0x20D8;
    int32       unknown_0x20DC;
    int32       unknown_0x20E0;
    int32       unknown_0x20E4;
    int32       unknown_0x20E8;
    int32       unknown_0x20EC;
    int32       unknown_0x20F0;
    int32       unknown_0x20F4;
    int32       unknown_0x20F8;
    int32       unknown_0x20FC;
    int32       unknown_0x2100;
    int32       unknown_0x2104;
    int32       unknown_0x2108;
    int32       unknown_0x210C;
    int32       unknown_0x2110;
    int32       unknown_0x2114;
    int32       unknown_0x2118;
    int32       unknown_0x211C;
    int32       unknown_0x2120;
    int32       unknown_0x2124;
    int32       unknown_0x2128;
    int32       unknown_0x212C;
    int32       unknown_0x2130;
    int32       unknown_0x2134;
    int32       unknown_0x2138;
    int32       unknown_0x213C;
    int32       unknown_0x2140;
    int32       unknown_0x2144;
    int32       unknown_0x2148;
    int32       unknown_0x214C;
    int32       unknown_0x2150;
    int32       unknown_0x2154;
    int32       unknown_0x2158;
    int32       unknown_0x215C;
    int32       unknown_0x2160;
    int32       unknown_0x2164;
    int32       unknown_0x2168;
    int32       unknown_0x216C;
    int32       unknown_0x2170;
    int32       unknown_0x2174;
    int32       unknown_0x2178;
    int32       unknown_0x217C;
    int32       unknown_0x2180;
    int32       unknown_0x2184;
    int32       unknown_0x2188;
    int32       unknown_0x218C;
    int32       unknown_0x2190;
    int32       unknown_0x2194;
    int32       unknown_0x2198;
    int32       unknown_0x219C;

    // Padding to match original binary layout
    // The original MapClass has a large gap of unknown members
    uint8       _unused_padding[0x456C - 0x21A0];

    // 存档载入期重建的地图子对象（asm: LoadGame_slave3）。
    // 维护入口：Game/LoadGame.cpp 的 LoadGame_slave3_5BE490。
    MapLoadState LoadState;
};
// The single global map instance, mirroring `Map` in the original binary.
extern MapClass* TheMap;
