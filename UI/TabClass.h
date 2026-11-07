#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class GScreenClass;
class ShapeButtonClass;

// ============================================================================
// TabClass - the sidebar tab strip (flash + cameo presence query)
//
//  Original layout notes recovered from the asm:
//    ShapeButtonsTabs  -- 3 ShapeButtonClass objects in a row, each stride
//                         0x60 (index * 3 << 5); Flash_Tabs / SetTab index
//                         them the same way.
//    +0x54             -- cameo count used by HasCameos.
//    +0x539C           -- currently selected tab index (SidebarClass field,
//                         surfaced here because SetTab owns it).
// ============================================================================

class TabClass {
public:
    TabClass() noexcept : CameoCount(0), CurrentTab(0), RepaintPending(false) {}

    // ------------------------------------------------------------------------
    // asm: TabClass_Flash_Tabs6A76F0 (0x6A76F0)
    //
    //   esi = Frame                              ; 全局帧计数，不是入参
    //   eax = Frame; ecx = 0Ah; cdq; idiv ecx     ; edx = Frame % 10
    //   sub ecx,edx                              ; ecx = 10 - Frame%10
    //   lea edx,[esi+ecx] / magic div by 10      ; 四舍五入到整 10 帧
    //   and edx,80000001h ...                    ; 取奇偶
    //   setz dl                                  ; dl = (奇偶 == 0)
    //   eax = arg_0                              ; 入参是"第几个 tab"
    //   push dl (a4) / push ecx (a3) / push 0Ah (a2)
    //   ecx = &ShapeButtonsTabs[arg_0 * 3 << 5]
    //   call ShapeButtonClass_F2
    //   byte_884B8F = 1 ; Map.FlagToRedraw(0)
    // ------------------------------------------------------------------------
    static void Flash_Tabs(int32 index);

    // asm: TabClass_6A7760 (0x6A7760) —— 对整个 tab 按钮调 F1（取消闪烁）
    static void Stop_Flash(int32 index);

    // asm: TabClass_SetTab (0x6A7690 区域)
    //   切换选中页：旧页取消按下、新页按下、重建 60 个 cameo 槽并请求重绘。
    void SetTab(int32 tab);

    // ------------------------------------------------------------------------
    // asm: TabClass_HasCameos (0x6A7840)
    //   mov edx, [ecx+54h] / xor eax,eax / test edx,edx / setnle al / retn
    // ------------------------------------------------------------------------
    bool Has_Cameos() const { return CameoCount > 0; }   // asm: setnle al

    int32 CameoCount;      // asm +0x54
    int32 CurrentTab;      // asm +0x539Ch（SidebarClass 内）
    bool  RepaintPending;  // asm byte_884B8F
};

// ----------------------------------------------------------------------------
// asm globals surfaced by TabClass:
//   Frame             -- global frame counter used by Flash_Tabs
//   ShapeButtonsTabs  -- the three tab ShapeButtonClass instances
//   byte_884B8F       -- "tabs need repaint" latch
// ----------------------------------------------------------------------------
constexpr int32 TAB_COUNT = 3;

GScreenClass* Tab_Get_Map_Screen();          // asm: offset Map
void          Tab_Set_Repaint_Latch();       // asm: mov byte_884B8F,1
void          Tab_Bind_Map_Screen(GScreenClass* pScreen);

// asm: lea ecx,[eax+eax*2] / shl ecx,5 / add ecx,offset ShapeButtonsTabs
ShapeButtonClass* Tab_Get_Shape_Button(int32 index);

// asm: Frame —— 全局帧计数（原版由主循环递增）
extern int32 Frame;
