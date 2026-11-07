#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"
#include "Map/MapClass.h"
#include "Math/CoordStruct.h"
#include "Math/Rectangle.h"
#include "Containers/DynamicVectorClass.h"

// Forward declarations
class CCINIClass;
class ObjectTypeClass;
class ObjectClass;
class HouseClass;
class TechnoClass;
class CellClass;
class MapClass;

// MapClass is defined in Map/MapClass.h

// ============================================================================
// ScrollDirType - Scroll direction enum
// ============================================================================
enum class ScrollDirType : int32
{
    None = -1, North = 0, NorthEast = 1, East = 2, SouthEast = 3,
    South = 4, SouthWest = 5, West = 6, NorthWest = 7, Count = 8
};

// ============================================================================
// DisplayClass - Main display manager
//
// Manages the tactical view, map scrolling, mouse interaction, and
// building placement. Inherits from MapClass.
// ============================================================================
class NOVTABLE DisplayClass : public MapClass
{
public:
    static DisplayClass* Instance;

    DisplayClass();
    virtual ~DisplayClass();

    // MapClass overrides
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual void LoadFromINI(CCINIClass* pINI);
    virtual const wchar_t* GetToolTip(uint32 nDlgID);
    virtual void CloseWindow();

    // ------------------------------------------------------------------------
    // asm: vec_ObjectsInLayers[layer] —— 三张绘制层表（DisplayClass.cpp 内的
    //   文件级容器）。同步日志的 MAP LAYER 段按层逐项转储，故在此暴露
    //   只读访问器，避免调用方再去猜裸地址。
    // ------------------------------------------------------------------------
    static int32 GetLayerCount();                                  // 层表张数
    static int32 GetLayerObjectCount(int32 layer);                 // vec_ObjectsInLayers[layer].length
    static ObjectClass* GetLayerObject(int32 layer, int32 index);  // vec_ObjectsInLayers[layer].data[index]


    // DisplayClass virtual methods
    virtual bool MapCell(CellStruct* pMapCoord, HouseClass* pHouse);
    virtual bool RevealFogShroud(CellStruct* pMapCoord, HouseClass* pHouse, bool bIncreaseShroudCounter);
    virtual bool MapCellFoggedness(CellStruct* pMapCoord, HouseClass* pHouse);
    virtual bool MapCellVisibility(CellStruct* pMapCoord, HouseClass* pHouse);
    virtual MouseCursorType GetLastMouseCursor() = 0;
    virtual bool ScrollMap(DWORD dwUnk1, DWORD dwUnk2, DWORD dwUnk3);
    virtual void Set_View_Dimensions(const Rectangle& rect);
    virtual void vt_entry_AC(DWORD dwUnk);
    virtual void vt_entry_B0(DWORD dwUnk);
    virtual void vt_entry_B4(Point2D* pPoint);

    // Mouse interaction
    virtual bool ConvertAction(
        const CellStruct& cell, bool bShrouded, ObjectClass* pObject,
        Action action, bool dwUnk);
    virtual void LeftMouseButtonDown(const Point2D& point);
    virtual void LeftMouseButtonUp(
        const CoordStruct& coords, const CellStruct& cell,
        ObjectClass* pObject, Action action, DWORD dwUnk2);
    virtual void RightMouseButtonUp(DWORD dwUnk);
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知显示层补全原版命名形态：守卫/攻击移动两种
    // 光标码、选择集可攻判定与攻击动作换算、右键按下与重绘恢复。
    // ------------------------------------------------------------------------
    virtual MouseCursorType Cursor_Guard() const;
    virtual MouseCursorType Cursor_AttackMove() const;
    virtual bool CanAllSelectedAttack() const;
    virtual Action ConvertAttack(Action action);
    virtual void RightMouseDown(const Point2D& point);
    virtual void Refresh();
    virtual void Restore();

    // Non-virtual methods
    void Init();
    void Draw(bool forced);

    // ------------------------------------------------------------------------
    // asm: void_RecreateSurfaces (0x533F70, __usercall: ecx = 第一张矩形，
    //      edx = 第二张，栈参 a3/a4；retn 0Ch，恒返回 1)
    //
    // 销毁并重建五张全局离屏表面（Alternate / Hidden / Composite / Tile /
    // Sidebar）。原版是独立函数、无 this，唯一调用点在 SetVideoMode 尾部
    // （+41Eh），四个参数都是屏幕矩形：
    //   visRect      -> Hidden 两趟 + Alternate   （ecx，asm ebp）
    //   altRect      -> Composite                 （edx，asm edi）
    //   tileRect     -> Tile                      （a3）
    //   sidebarRect  -> Sidebar；其首参地址低字节另作 Hidden 的首/次趟门控
    //                  （a4，asm 直读 [esp+arg_8] 的低字节）
    //
    // 尺寸一律取 .Right/.Bottom：调用点的 L/T 恒为 0，与 asm 读 +8/+0Ch
    // 逐字节一致。
    // ------------------------------------------------------------------------
    static int32 RecreateSurfaces(const LTRBStruct& visRect,
                                  const LTRBStruct& altRect,
                                  const LTRBStruct& tileRect,
                                  const LTRBStruct& sidebarRect);

    void Update();
    void Pan(int32 dx, int32 dy);
    void Scroll(ScrollDirType dir);
    void CenterOn(const CoordStruct& coord);
    void ZoomIn();
    void ZoomOut();
    void TacticalToScreen(const CoordStruct& coord, Point2D* outPoint);
    void ScreenToTactical(const Point2D& point, CoordStruct* outCoord);
    bool IsInView(const CoordStruct& coord);
    void MarkToRedraw();
    void Read_INI();
    void Help_Text(const wchar_t* text);
    void Mouse_Left_Release();
    // 根据游戏行为，可知鼠标消息以左右键与按压状态分档进入
    // 处理面；裸名入口与 Mouse_ 前缀入口一一对应。
    void Left_Press();
    void Left_Release();
    void Left_Held();
    void Left_Up();
    void Right_Press();

    // ------------------------------------------------------------------------
    // asm: DisplayClass_Mouse_Left_Press / _Mouse_Left_Held /
    //      _Mouse_Right_Press / _Mouse_Left_Release / _MouseMove。
    // 鼠标消息按按键与按压相位分派；每个入口只处理自己那一档状态机。
    // ------------------------------------------------------------------------
    void Mouse_Move(const Point2D& point);
    void Mouse_Left_Press(const Point2D& point);
    void Mouse_Left_Held(const Point2D& point);
    void Mouse_Left_Up(const Point2D& point);
    void Mouse_Right_Press(const Point2D& point);

    // 逐帧输入总派发（asm: 主循环 Main_Loop 内的鼠标状态轮询段）。
    void Process_Input();
    // 逐帧显示层 AI/更新（asm: DisplayClass::AI）。
    void AI();

    // 放置预览：把当前 CurrentBuildingType 的脚印落到 cell 上，返回是否落位
    // 成功（asm: Mouse_Left_Release 的放置分支）。
    bool Place_Object(const CellStruct& cell);
    // 取消放置：清空 CurrentBuilding/CurrentBuildingType 与脚印数据。
    void Cancel_Placement();

    // Action decision
    Action DecideAction(const CellStruct& cell, ObjectClass* pObject, DWORD dwUnk);

    // Foundation management
    CellStruct* FoundationBoundsSize(
        CellStruct& outBuffer, const CellStruct* pFoundationData) const;
    CellStruct FoundationBoundsSize(const CellStruct* pFoundationData) const;
    void MarkFoundation(CellStruct* BaseCell, bool Mark);
    void SetActiveFoundation(CellStruct* Coords);

    // Layer management
    void Submit(ObjectClass* pObject);
    void Remove(ObjectClass* pObject);

    // Click processing
    bool ProcessClickCoords(Point2D* src, CellStruct* XYdst, CoordStruct* XYZdst,
                            ObjectClass** Target, BYTE* a5, BYTE* a6);

    // Properties
    Rectangle VisibleRect;
    Rectangle MapRect;
    int32 ScreenWidth;
    int32 ScreenHeight;
    float ZoomLevel;
    int32 ScrollAmount;
    bool IsScrolling;
    Point2D CursorPosition;
    Point2D ViewPosition;
    int32 ScrollTimer;

    // Foundation placement data
    CellStruct CurrentFoundation_CenterCell;
    CellStruct CurrentFoundation_TopLeftOffset;
    CellStruct* CurrentFoundation_Data;
    bool unknown_1180;
    bool unknown_1181;
    CellStruct CurrentFoundationCopy_CenterCell;
    CellStruct CurrentFoundationCopy_TopLeftOffset;
    CellStruct* CurrentFoundationCopy_Data;
    DWORD unknown_1190;
    DWORD unknown_1194;
    DWORD unknown_1198;
    bool FollowObject;
    ObjectClass* ObjectToFollow;
    ObjectClass* CurrentBuilding;
    ObjectTypeClass* CurrentBuildingType;
    DWORD unknown_11AC;
    bool RepairMode;
    bool SellMode;
    bool PowerToggleMode;
    bool PlanningMode;
    bool PlaceBeaconMode;
    int32 CurrentSWTypeIndex;
    DWORD unknown_11BC;
    DWORD unknown_11C0;
    DWORD unknown_11C4;
    DWORD unknown_11C8;
    bool unknown_bool_11CC;
    bool unknown_bool_11CD;
    bool unknown_bool_11CE;
    bool DraggingRectangle;
    bool unknown_bool_11D0;
    bool unknown_bool_11D1;
    DWORD unknown_11D4;
    DWORD unknown_11D8;
    DWORD unknown_11DC;
    DWORD unknown_11E0;
    DWORD padding_11E4;
};
// ============================================================================
// asm: unk_A8EB96 (db ?) —— 全库只有 void_RecreateSurfaces 读它一次：为 0 时
// Sidebar 面拿 a4 = 1（强制系统内存），与 bVideoBackBuffer 的反相一起决定
// 每张面的 DDSCAPS。原版没有符号名，按原地址名保留，定义在 DisplayClass.cpp。
// ============================================================================
extern uint8 g_UnkA8EB96;
