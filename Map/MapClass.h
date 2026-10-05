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
};
// The single global map instance, mirroring `Map` in the original binary.
extern MapClass* TheMap;
