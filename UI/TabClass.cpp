#include "TabClass.h"
#include "ShapeButtonClass.h"

// ============================================================================
// TabClass implementations.  Globals that the original binary kept in .data
// are defined here so every translation unit agrees on one instance.
// ============================================================================

int32 Frame = 0;                               // asm: global Frame

// asm: byte_884B8F -- repaint latch for the tab strip
static int32 g_TabRepaintLatch = 0;

// asm: offset Map -- the global screen/map composite
static GScreenClass* g_TabMapScreen = nullptr;

// asm: ShapeButtonsTabs —— 三个连排的 ShapeButtonClass，步长 0x60
static ShapeButtonClass g_ShapeButtonsTabs[TAB_COUNT] = {
    ShapeButtonClass(0, nullptr, 0),
    ShapeButtonClass(1, nullptr, 0),
    ShapeButtonClass(2, nullptr, 0)
};

GScreenClass* Tab_Get_Map_Screen()
{
    return g_TabMapScreen;                 // asm: mov ecx, offset Map
}

void Tab_Set_Repaint_Latch()
{
    g_TabRepaintLatch = 1;                 // asm: mov byte_884B8F, 1
}

void Tab_Bind_Map_Screen(GScreenClass* pScreen)
{
    g_TabMapScreen = pScreen;
}

// asm: lea ecx,[eax+eax*2] / shl ecx,5 / add ecx,offset ShapeButtonsTabs
// 数组是本文件里的连续对象，[index * 3 << 5] 即 index * 0x60；这里直接
// 用下标表达同一寻址。
ShapeButtonClass* Tab_Get_Shape_Button(int32 index)
{
    if (index < 0 || index >= TAB_COUNT)
        return nullptr;
    return &g_ShapeButtonsTabs[index];
}

// ============================================================================
// TabClass_Flash_Tabs  (asm: TabClass_Flash_Tabs6A76F0, 0x6A76F0)
//
// 帧相位来自全局 Frame，入参是 tab 下标：
//   rem   = Frame % 10
//   phase = ((Frame + (10 - rem)) / 10) & 1     ; 整 10 帧取奇偶
//   a4    = (phase == 0)                        ; setz dl
//   a3    = 10 - rem                            ; sub ecx,edx 后的 ecx
//   a2    = 10                                  ; 闪烁周期
// ============================================================================
void TabClass::Flash_Tabs(int32 index)
{
    const int32 frame = Frame;                 // asm: mov esi, Frame

    // asm: mov eax,esi / mov ecx,0Ah / cdq / idiv ecx  -> edx = Frame % 10
    const int32 remainder = frame % 10;

    // asm: sub ecx,edx / lea edx,[esi+ecx] / imul 66666667h / sar 2 / shr 1Fh
    //      / add edx,eax  -> 四舍五入到最近的 10 帧再取商
    const int32 phaseStep = 10 - remainder;    // asm: sub ecx,edx
    const int32 quotient  = (frame + phaseStep) / 10;

    // asm: and edx,80000001h / jns / dec / or 0FFFFFFFEh / inc -> 取奇偶
    int32 parity = quotient & 1;
    if (parity < 0)                            // asm: 负数分支的补齐
        parity = (parity == 0) ? 0 : 1;

    // asm: setz dl —— 奇偶为 0 时点亮（a4）
    const int32 flashOn = (parity == 0) ? 1 : 0;

    ShapeButtonClass* pButton = Tab_Get_Shape_Button(index);
    if (pButton != nullptr)
    {
        // asm: push a4 / push a3(=10-rem) / push 0Ah / call ShapeButtonClass_F2
        pButton->Flash(10, phaseStep, flashOn);
    }

    // asm: mov byte_884B8F,1 / mov ecx,offset Map / push 0 / GScreen_FlagToRedraw
    Tab_Set_Repaint_Latch();
}

// ============================================================================
// asm: TabClass_6A7760 (asm:281)
//   eax = arg_0 ; ecx = &ShapeButtonsTabs[arg*3<<5] ; call ShapeButtonClass_F1
// ============================================================================
void TabClass::Stop_Flash(int32 index)
{
    ShapeButtonClass* pButton = Tab_Get_Shape_Button(index);
    if (pButton != nullptr)
        pButton->StopFlash();                  // asm: call ShapeButtonClass_F1
}

// ============================================================================
// asm: TabClass_SetTab (asm:83)
//
//   if (tab == this->[539Ch]) return;                  ; 同页直接返回
//   old = this->[539Ch];
//   ShapeButtonsTabs[old] -> sub_723EB0()              ; 抬起
//   cameoSlot[old].[1Ch] = 0;                          ; 该页不再绘制
//   for (i = 0; i < 60; ++i) Map->sub_4F4450(&cameo[old][i]);   ; 摘除 60 个槽
//   this->[539Ch] = tab;
//   ShapeButtonsTabs[tab] -> sub_723EA0()              ; 按下
//   cameoSlot[tab].[1Ch] = 1;
//   count = Scenario->[34B8h] ? 0x12 : 0x1A;           ; 每页槽数
//   for (i = 0; i < count; ++i) { Gadget_20(&cameo[tab][i]); sub_4F4410(...); }
//   byte_884B8F = 1; sub_6A6610(this); this->[53A7h] = 1;
//
// cameo 槽数组（SidebarClass::StripClass）与 Map 的 4F4450/4F4410 在本工程
// 未建模，故这里只复刻"选中页切换 + 按钮按下状态 + 重绘请求"这三步，
// 槽位重建留给 SidebarClass 集成时补。
// ============================================================================
void TabClass::SetTab(int32 tab)
{
    if (tab == CurrentTab)                     // asm: cmp ebx,eax / jz
        return;
    if (tab < 0 || tab >= TAB_COUNT)
        return;

    // asm: ShapeButtonsTabs[old] -> sub_723EB0（抬起）
    ShapeButtonClass* pOld = Tab_Get_Shape_Button(CurrentTab);
    if (pOld != nullptr)
        pOld->SetPressed(false);

    // asm: mov [ebp+539Ch], ebx
    CurrentTab = tab;

    // asm: ShapeButtonsTabs[tab] -> sub_723EA0（按下）
    ShapeButtonClass* pNew = Tab_Get_Shape_Button(tab);
    if (pNew != nullptr)
    {
        pNew->SetPressed(true);
        pNew->NeedsRedraw = true;              // asm: mov byte ptr [edi+1Ch],1
    }

    // asm: mov eax,1Ah / jz ; mov eax,12h  -> 每页 cameo 槽数
    CameoCount = 0x1A;                          // asm: mov eax,1Ah
    // （Scenario->[34B8h] 非零时为 0x12；战役/遭遇战分支，见上注释）

    // asm: mov byte_884B8F,1 / sub_6A6610 / [53A7h] = 1
    Tab_Set_Repaint_Latch();
    RepaintPending = true;
}
