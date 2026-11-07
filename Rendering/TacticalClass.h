#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Abstract/AbstractClass.h"
#include "Rendering/Surface.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/ConvertClass.h"
#include "Math/CoordStruct.h"
#include "Math/Rectangle.h"
#include "Math/Matrix3D.h"
#include "Math/Facing.h"
#include "Math/Timer.h"
#include "Containers/DynamicVectorClass.h"

// Forward declarations
class DSurface;
class CellClass;
class ObjectClass;
class TechnoClass;
class HouseClass;
class IsometricTileClass;

// ============================================================================
// TacticalSelectableStruct - Selectable object info
// ============================================================================
struct TacticalSelectableStruct
{
    TechnoClass* Techno;
    int32 X;
    int32 Y;
};

// ============================================================================
// TacticalRenderMode - Rendering mode for tactical map
// ============================================================================
enum class TacticalRenderMode : int32
{
    All = 0,
    Terrain = 1,
    MovingAnimating = 2,
    AllAlt = 3,
    StopDrawing = 4,
    Mode5 = 5,
};

// ============================================================================
// RenderDebugStats - 渲染侧每帧计数器（冒烟诊断用）
//
//   只在 g_bDebugMode（OPENYR_DEBUG=1）为真时维护；MainLoop 每秒随
//   [FRAME] 一起把它打成一行 [RENDER]，用来证明"视口→可见对象→派发绘制"
//   这条链真的在跑，而不是空转。字段含义：
//     FrameCount      DisplayClass::Draw 被调用的次数（渲染帧计数）
//     UpdateTicks     TacticalClass::Update 真正执行过（未被帧戳门挡住）的次数
//     VisibleObjects  显示层（vec_VisibleObjects）里未 limbo 的对象数
//     DrawnObjects    通过视口剔除后真正派发 Draw 的对象数
//     DrawCalls       真正进入具体 Draw() 并正常返回的次数（与 DrawnObjects
//                     同时递增，单独列出是为了和"登记"类计数区分开）
//     CulledObjects   因 limbo / 视口外 / 未知类型而未派发的对象数
//     ShroudedCells   本帧判定为全遮罩的可见格数
//     VisibleCells    本帧登记进可见格表（TacticalClass::VisibleCells）的格数
//     ViewRect        本帧视口矩形（ScreenX, ScreenY, Width, Height）
// ============================================================================
struct RenderDebugStats
{
    uint32 FrameCount;
    uint32 UpdateTicks;
    int32  VisibleObjects;
    int32  DrawnObjects;
    int32  DrawCalls;
    int32  CulledObjects;
    int32  ShroudedCells;
    int32  VisibleCells;
    int32  ViewX;
    int32  ViewY;
    int32  ViewW;
    int32  ViewH;
};

extern RenderDebugStats g_RenderDebugStats;

// ============================================================================
// TacticalClass - Tactical map rendering (isometric tile engine)
//
// The core isometric rendering engine for C&C. Handles terrain tile
// rendering, object layer sorting (YSort), shroud/fog of war, grid
// overlays, and isometric coordinate conversion.
// ============================================================================
class NOVTABLE TacticalClass : public AbstractClass
{
public:
    static TacticalClass* Instance;

    TacticalClass();
    virtual ~TacticalClass();

    // Virtual methods
    virtual bool sub_6DBB60(const CoordStruct& a2, const CoordStruct& a3, DWORD a4, DWORD dwUnk);

    // ── AbstractClass 的纯虚槽 ──────────────────────────────────────────────
    // asm: TacticalClass 在 gamemd 的 vtable 里自带这几项（WhatAmI 返回
    //   0x3B = AbstractType::TacticalMap）。本树此前没落，TacticalClass 一直
    //   是抽象类、无法实例化，Render_Frame 因此永远拿不到 TacticalMap。
    //   这里按 MapClass 的同形写法补齐；序列化两槽本树未接存档系统，
    //   明确返回 E_NOTIMPL，不伪装成功。
    virtual AbstractType WhatAmI() const override { return AbstractType::TacticalMap; }
    virtual int32 Size() const override { return sizeof(TacticalClass); }
    virtual HRESULT GetClassID(CLSID* pClassID) override
    {
        if (pClassID == nullptr)
            return E_POINTER;
        *pClassID = CLSID();
        return S_OK;
    }
    virtual HRESULT Load(IStream* pStm) override { (void)pStm; return E_NOTIMPL; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override
    {
        (void)pStm; (void)fClearDirty;
        return E_NOTIMPL;
    }

    // Isometric coordinate conversion
    void SetTacticalPosition(CoordStruct* pCoord);
    CellStruct* CoordsToCell(CellStruct* pDest, CoordStruct* pSource);
    bool CoordsToClient(const CoordStruct* coords, Point2D* pOutClient) const;
    Point2D CoordsToScreen(const CoordStruct& coord) const;
    Point2D* CoordsToScreen(Point2D* pDest, const CoordStruct* pSource);
    CoordStruct* ClientToCoords(CoordStruct* pOutBuffer, const Point2D& client) const;
    CoordStruct ClientToCoords(const Point2D& client) const;

    // Cell operations
    CellStruct CellToScreen(const CellStruct& cell) const;
    CellStruct ScreenToCell(const Point2D& screen) const;
    static Point2D AdjustForZShapeMove(int32 x, int32 y);
    static int32 AdjustForZ(int32 Height);

    // Viewport testing
    bool Is_In_Viewport(const CoordStruct& coord) const;
    bool Is_In_Viewport(const Rectangle& rect) const;
    char GetOcclusion(const CellStruct& cell, bool fog) const;

    // 每帧 tick（asm: TacticalClass_Update @1483457，vtable 槽 0x5C，由
    //   LogicClass_Update 第 20 步 `mov ecx,TacticalMap / call [eax+5Ch]` 驱动）。
    void Update();

    // Rendering
    //
    // DrawSurface 是"本帧战术层落到哪张面"的载体。原版 TacticalClass_Draw
    // 从 .data 的 DSurface_Composite 取面（`mov ecx, DSurface_Composite`），
    // Render 的入参只是同一张面的显式副本；本树保留 Render(pSurface) 的
    // 形参形态（asm 调用点确实传面），由 Render 写进该成员，各 Draw_* 用
    // Target_Surface() 取面。这样既有单一的绘制目标，也不会让每个 Draw_*
    // 都改签名。
    void Set_Draw_Surface(Surface* pSurface) { DrawSurface = pSurface; }
    Surface* Target_Surface();

    void Draw();
    void Draw_Overlay();
    void Draw_Ground_Decals();
    void Draw_Objects();
    void Draw_Transient_Objects();
    void Draw_Shroud();
    void Draw_Fog();
    void Draw_Grid();
    void Draw_Waypoints();
    void Draw_Placement_Grid();
    void Render(DSurface* pSurface, bool flag, TacticalRenderMode eMode);
    void FocusOn(CoordStruct* pDest, int32 Velocity);

    // Layer sorting (YSort) for correct render order
    void YSortObjects();

    // Visible object management
    void Visible_Objects();
    DynamicVectorClass<ObjectClass*> GetVisibleObjects();

    // Dirty area management
    void RegisterDirtyArea(Rectangle Area, bool bUnk);
    void RegisterCellAsVisible(CellClass* pCell);

    // Selection
    void AddSelectable(TechnoClass* pTechno, int32 x, int32 y);
    void ClearSelectables();

    // Timer drawing
    static int32 DrawTimer(
        int32 index, ColorScheme* Scheme, int32 Time,
        wchar_t* Text, Point2D* someXY1, Point2D* someXY2);

    // Matrix operations
    Point2D* ApplyMatrix_Pixel(Point2D* coords, Point2D* offset);

    // Properties
    static constexpr int32 IsometricTileWidth = 60;
    static constexpr int32 IsometricTileHeight = 30;
    static constexpr int32 HalfTileWidth = 30;
    static constexpr int32 HalfTileHeight = 15;

    wchar_t ScreenText[64];
    int32 EndGameGraphicsFrame;
    int32 LastAIFrame;
    bool field_AC;
    bool field_AD;
    BYTE gap_AE[2];
    Point2D TacticalPos;
    Point2D LastTacticalPos;
    double ZoomInFactor;
    Point2D Point_C8;
    Point2D Point_D0;
    float field_D8;
    float field_DC;
    int32 VisibleCellCount;
    CellClass* VisibleCells[800];
    Point2D TacticalCoord1;
    DWORD field_D6C;
    DWORD field_D70;
    Point2D TacticalCoord2;
    bool field_D7C;
    bool Redrawing;
    BYTE gap_D7E[2];
    Rectangle ContainingMapCoords;
    LTRBStruct Band;
    DWORD MouseFrameIndex;
    CDTimerClass StartTime;
    int32 SelectableCount;
    Matrix3D Unused_Matrix3D;
    Matrix3D IsoTransformMatrix;
    DWORD field_E14;
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知战术视图层补全选择集管理与航点路径绘制。
    // ------------------------------------------------------------------------
    static int32 CountSelected();
    static ObjectClass* GetNthSelected(int32 idx);
    static void DeselectAll();
    static void DefineSelectionAsTeamXX(int32 team);
    static void EraseTeamXX(int32 team);
    static int32 CountMembersOfTeamXX(int32 team);
    static bool UnselectedTeamMembersExist(int32 team);
    static void CollectSelectedIDs(DynamicVectorClass<int32>* pOut);
    static void DrawWaypointPaths();
    static void Init_DefaultRoomCoords();

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知战术层补全原版命名形态的入口：反序列化复位、结算
    // 画面、主绘制例程、全量绘制、格子闪烁细节层、集结点连线、格子旗标、
    // 坐标换算与拖拽框选。
    // ------------------------------------------------------------------------
    void NoInit(IStream* pStm);
    void DrawGameOver(const wchar_t* pText);
    void Draw_It_6D5030();
    void Draw_All_6D8DB0();
    // asm: Tactical_Do_Cell_Twinkle_6D7840 (0x6D7840, retn 8)
    //   region = {x256, y256, w, h} 区域，clip = 裁剪矩形（屏幕坐标）
    void Do_Cell_Twinkle_6D7840(const Rectangle& region, const Rectangle& clip);
    // asm: Tactical_Draw_RallyPoint_Paths_6DA9D0 (0x6DA9D0, retn 4)
    //   arg0 原样转给线段绘制原语（本树无画笔后端，见实现里的降级点）
    void Draw_RallyPoint_Paths_6DA9D0(int32 arg0);

    // asm: DetailFrameRate —— 细节层帧率（与最低帧率比较决定是否闪）
    static int32 DetailFrameRate;
    // asm: dword_B0E640 —— 上一次闪烁的时间戳
    static int32 LastTwinkleTime;
    // asm: Get_Min_Frame_Rate（0x4A5C40 族）—— 当前允许的最低帧率
    static int32 Get_Min_Frame_Rate();
    int32 GetCellFlags(const CellStruct& coords, CellStruct* pWat);
    Point2D To_Pixel(const CoordStruct& coord) const;
    void Drag_Select();
    void Drag_Select_Dimensions();

    // 根据游戏行为，可知战术层持有默认坐标常量，初始化例程负责清零。
    static CoordStruct Default_RoomCoords;

    // 拖拽框选的两个角点（屏幕坐标）
    int32 DragX1 = 0;
    int32 DragY1 = 0;
    int32 DragX2 = 0;
    int32 DragY2 = 0;

    static DynamicVectorClass<ObjectClass*> SelectedObjects;  // 当前选择集
    static DynamicVectorClass<int32> SelectedTeamIds;         // 与选择集平行的队伍号

    // ------------------------------------------------------------------------
    // asm: TacticalClass_RegisterCellAsVisible / _GetCellFlags / _Draw_Timer /
    //      _DrawWaypointPaths。AddSelectable 把可点选对象连同其屏幕锚点登记
    //      进 Selectables（asm 的 `selections` 数组，每项 0Ch 字节），
    //      HitTest 用同一份数据做最近命中；下表即 asm 定长 100 项的形态。
    // ------------------------------------------------------------------------
    TacticalSelectableStruct Selectables[100];
    int32 ScreenWidth = 0;
    int32 ScreenHeight = 0;

    // 本帧绘制目标面（见 Set_Draw_Surface 注释）。
    Surface* DrawSurface = nullptr;

    // asm: rect_mouseclass_vp —— 战术视口矩形。z 缓冲（ZBufferClass）按它
    //   建，尺寸是"视口"而不是整张面；由 Set_View_Dimensions 写入。
    Rectangle ViewRect;

    // asm: ZBuffer 每帧清零（sub_6D2B60 按脏区 sub_7BCFB0 填 0FFFFh）。
    //   本树全量重绘，等价做法是每帧整片清一次；用帧号做门，避免同一帧
    //   的多次 Render（地形/对象/特效三趟）互相把 z 清掉。
    int32 ZBufferFrame = -1;

    // 命中测试：返回 Selectables 中离 (screenPt) 最近且在阈值内的对象。
    ObjectClass* HitTest(const Point2D& screenPt, int32 radius) const;
    // 点选：把命中对象加入/移出选择集，返回是否发生了选择变化。
    bool Select_At(const Point2D& screenPt, bool additive);

    // 选中框（拖拽带）绘制入口：把 Band 换算成世界屏幕坐标后登记脏区。
    void Draw_Selection_Rect();
    // ------------------------------------------------------------------------
    // asm: Draw_Radial (0x456970) —— 径向指示器：等距椭圆环 + 四道刻度射线。
    //   drawTicks      = asm 的 cl（[var_80+7]）：非零时画 chunk[] 的四道射线
    //   darken         = asm 的 dl（[var_80+6]）：非零时环色按 Adjust(0x32,黑)
    //   coords         = asm 的第 3~5 个整型参（CoordStruct 按值传入）
    //   color          = asm arg4（RGBb，R/G/B 三字节）
    //   radius         = asm arg5（float）
    //   integralRadius = asm arg6：非零直接用 radius，否则 (r+0.5)/√2*60
    //   truncateRadius = asm arg7：非零先把 radius 截断成整数再转回 double
    // ------------------------------------------------------------------------
    void Draw_Radial_Indicator(uint8 drawTicks, uint8 darken,
                               const CoordStruct& coords, const RGBClass& color,
                               float radius, bool integralRadius, bool truncateRadius);

    // 视口尺寸同步（由 DisplayClass::Set_View_Dimensions 驱动）。
    void Set_View_Dimensions(const Rectangle& view);

    // 当前视口在屏幕坐标系中的左上角基准（世界→屏幕的平移量）。
    Point2D GetTacticalOrigin() const;

    // asm: sub_6D8640 — 把视口左上角夹进"地图可滚动范围"。
    //   入参/出参都是 Point2D（世界坐标，leptons）；返回是否发生了夹取。
    //   TacticalClass_Update 两次调用它：插值出的新位置、以及外部直接改动的
    //   TacticalCoord2。
    bool Clamp_Tactical_Position(Point2D& pos) const;

    // 邻格遮挡位掩码（asm: TacticalClass_GetCellFlags 的 8 方向探查部分）。
    // altMode 为真时按 asm 的第二分支读"可通行"位，否则读"已探明"位。
    int32 ComputeNeighbourMask(const CellStruct& cell, bool altMode) const;

};