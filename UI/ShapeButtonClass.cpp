#include "ShapeButtonClass.h"

#include <cstring>

namespace
{
    ShapeButtonClass::ShapeBlitProc g_ShapeBlit = nullptr;
    ShapeButtonClass::TooltipSink*  g_TooltipSink = nullptr;
}

ShapeButtonClass::ShapeButtonClass(int32 id, GScreenClass* pScreen, int32 shapeFrame)
    : ID(id)
    , X(0)
    , Y(0)
    , Width(0)
    , Height(0)
    , ShapeFrame(shapeFrame)
    , TooltipText(nullptr)
    , IsHovered(false)
    , Disabled(false)
    , Pressed(false)
    , Selected(false)
    , UseSidebarSurface(false)
    , Shadow(false)
    , OffsetX(0)
    , OffsetY(0)
    , FlashPhase(0)
    , FlashDuration(0)
    , FlashRemaining(0)
    , LastFrame(0)
    , ShapeDrawn(false)
    , NeedsRedraw(true)
    , Shape(nullptr)
    , OwnsShape(false)
{
    (void)pScreen;
}

ShapeButtonClass::~ShapeButtonClass()
{
    Erase();                     // asm: DTOR 释放自有 SHP
}

void ShapeButtonClass::Set_Shape_Blit(ShapeBlitProc pProc)
{
    g_ShapeBlit = pProc;
}

void ShapeButtonClass::Set_Tooltip_Sink(TooltipSink* pSink)
{
    g_TooltipSink = pSink;
}

// ----------------------------------------------------------------------------
// asm: Gadget_SetTopLeft (asm:71)  (retn 8)
//   mov [ecx+0Ch],a2 / mov [ecx+10h],a3
// ----------------------------------------------------------------------------
void ShapeButtonClass::SetTopLeft(int32 left, int32 top)
{
    X = left;
    Y = top;
}

void ShapeButtonClass::SetDrawOffset(int32 dx, int32 dy)
{
    OffsetX = dx;
    OffsetY = dy;
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_SetDimensions (asm:82)  (retn 8)
//   mov eax,[esp+a2] / mov edx,[esp+a3]
//   mov [ecx+14h],eax / mov [ecx+18h],edx
// 注意：签名只有两个参数（宽/高），位置由 Gadget_SetTopLeft 写入。
// ----------------------------------------------------------------------------
void ShapeButtonClass::SetDimensions(int32 width, int32 height)
{
    Width  = width;
    Height = height;
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_Enabled (asm:55)
//   mov dl,[ecx+1Eh] / xor eax,eax / test dl,dl / setz al
// ----------------------------------------------------------------------------
bool ShapeButtonClass::Enabled() const
{
    return !Disabled;
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_Erase (asm:384)
//   if ([esi+5Ch] && [esi+58h]) {
//       delete [esi+58h]; [esi+58h] = 0; [esi+5Ch] = 0;
//       [esi+14h] = 0; [esi+18h] = 0;      ; 宽高一并清零
//   }
// ----------------------------------------------------------------------------
void ShapeButtonClass::Erase()
{
    if (OwnsShape && Shape != nullptr)
    {
        delete static_cast<uint8*>(Shape);   // asm: call delete
        Shape = nullptr;                     // asm: mov [esi+58h],0
        OwnsShape = false;                   // asm: mov [esi+5Ch],0
        Width  = 0;                          // asm: mov [esi+14h],0
        Height = 0;                          // asm: mov [esi+18h],0
    }
}

// ----------------------------------------------------------------------------
// asm: sub_69DEA0 (asm:420)  mov [ecx+5Ch],al
// ----------------------------------------------------------------------------
void ShapeButtonClass::SetOwnsShape(bool owns)
{
    OwnsShape = owns;
}

void ShapeButtonClass::SetShape(void* pShape, bool owns)
{
    if (OwnsShape && Shape != nullptr && Shape != pShape)
        delete static_cast<uint8*>(Shape);

    Shape = pShape;              // asm: mov [esi+58h],eax
    OwnsShape = owns;            // asm: mov [esi+5Ch],al
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_SetTooltip (asm:772)
//   if (!this) return;
//   var_1C = [ecx+24h]            ; 提示串槽
//   rect = { [ecx+0Ch], [ecx+10h], [ecx+14h], [ecx+18h] }
//   sub_724730(CCToolTip, [ecx+24h])   ; 设标题
//   sub_724580(CCToolTip, &rect)       ; 设区域
// 提示条后端未建模，落到 TooltipSink 回调；未绑定时只保留文本。
// ----------------------------------------------------------------------------
void ShapeButtonClass::SetTooltip(const char* pText)
{
    TooltipText = pText;                       // asm: mov [esp+var_8],edx

    if (g_TooltipSink == nullptr)
        return;

    const int32 rect[4] = { X, Y, Width, Height };   // asm: [ecx+0Ch..18h]
    g_TooltipSink->Set_Tooltip(rect, pText);
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_F2 (asm:571)  (retn 0Ch)  -> char
//   if ([ecx+38h]) return 0;          ; 已在闪烁，不重入
//   [ecx+38h] = a2;                   ; 总时长
//   [ecx+3Ch] = a2 + a3;              ; 剩余 = 总时长 + 相位
//   [ecx+34h] = a4;                   ; 相位标志
//   return 1;
// ----------------------------------------------------------------------------
bool ShapeButtonClass::Flash(int32 duration, int32 phase, int32 flag)
{
    if (FlashDuration != 0)          // asm: mov eax,[ecx+38h] / test / jnz
        return false;

    FlashDuration  = duration;       // asm: mov [ecx+38h],eax
    FlashRemaining = duration + phase; // asm: add eax,edx / mov [ecx+3Ch],eax
    FlashPhase     = flag;           // asm: mov [ecx+34h],al
    return true;                     // asm: mov al,1
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_F1 (asm:615)
//   if (![ecx+38h]) return 0;
//   [ecx+34h] = 0; [ecx+3Ch] = 0; [ecx+38h] = 0; return 1;
// ----------------------------------------------------------------------------
bool ShapeButtonClass::StopFlash()
{
    if (FlashDuration == 0)          // asm: mov edx,[ecx+38h] / cmp edx,0 / jz
        return false;

    FlashPhase     = 0;              // asm: mov [ecx+34h],al
    FlashRemaining = 0;              // asm: mov [ecx+3Ch],eax
    FlashDuration  = 0;              // asm: mov [ecx+38h],eax
    return true;
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass_69E010 (asm:651)  —— 每帧闪烁推进
//
//   if (Enabled) {                        ; [ecx+1Eh] == 0
//       if (![ecx+38h]) return 0;
//       [ecx+34h]=0; [ecx+3Ch]=0; [ecx+38h]=0; return 1;
//   }
//   eax = [ecx+3Ch]; if (!eax) return 0;
//   --eax; [ecx+3Ch] = eax;
//   if (eax != 0) return 0;
//   al = [ecx+34h]; [ecx+34h] = (al == 0);   ; 相位翻转
//   [ecx+3Ch] = [ecx+38h];                   ; 重置剩余
//   return 1;
// ----------------------------------------------------------------------------
bool ShapeButtonClass::AI()
{
    if (Enabled())
    {
        if (FlashDuration == 0)      // asm: mov edx,[ecx+38h] / cmp edx,0 / jz
            return false;
        FlashPhase     = 0;
        FlashRemaining = 0;
        FlashDuration  = 0;
        return true;
    }

    if (FlashRemaining == 0)         // asm: mov eax,[ecx+3Ch] / cmp eax,0 / jz
        return false;

    --FlashRemaining;                // asm: dec eax
    if (FlashRemaining != 0)         // asm: cmp eax,0 / jnz
        return false;

    FlashPhase = (FlashPhase == 0) ? 1 : 0;   // asm: cmp al,dl / setz al
    FlashRemaining = FlashDuration;           // asm: mov edx,[ecx+38h] / mov [ecx+3Ch],edx
    return true;
}

bool ShapeButtonClass::IsFlashing() const
{
    return FlashDuration != 0;       // asm: test edx,edx / setnz al
}

bool ShapeButtonClass::GetFlashFlag() const
{
    return FlashPhase != 0;          // asm: mov al,[ecx+34h]
}

// ----------------------------------------------------------------------------
// asm: Gadget_40 / Gadget_3C
//   Gadget_40: [ecx+1Eh]=0 ; [ecx+1Ch]=1 ; jmp vtable+54h（请求重绘）
//   Gadget_3C: [ecx+1Eh]=1 ; [ecx+1Ch]=1 ; jmp vtable+54h
// 这里以 SetDisabled 统一表达，并置 NeedsRedraw（+0x1C）。
// ----------------------------------------------------------------------------
void ShapeButtonClass::SetDisabled(bool disabled)
{
    Disabled = disabled;             // asm: mov [ecx+1Eh],al
    NeedsRedraw = true;              // asm: mov [ecx+1Ch],1
}

void ShapeButtonClass::SetSelected(bool on)  { Selected = on; }
void ShapeButtonClass::SetHovered(bool on)   { IsHovered = on; }
void ShapeButtonClass::SetPressed(bool on)   { Pressed = on; }

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass::Draw_Me (asm:427) 的帧号选择
//
//   if ([esi+1Eh]) frame = 2;                       ; 禁用
//   else if ([esi+40h]) {                           ; 选中
//       if ([esi+34h]) frame = 3 + ([esi+2Dh] != 0);
//       else           frame = ([esi+2Dh] != 0);
//   } else             frame = ([esi+2Ch] != 0);    ; 悬停
// ----------------------------------------------------------------------------
int32 ShapeButtonClass::SelectFrame() const
{
    if (Disabled)
        return 2;

    if (Selected)
    {
        if (FlashPhase != 0)
            return 3 + (Pressed ? 1 : 0);
        return Pressed ? 1 : 0;
    }

    return IsHovered ? 1 : 0;
}

// ----------------------------------------------------------------------------
// asm: ShapeButtonClass::Draw_Me (asm:427)
//   if (!ControlClass::Draw_Me(forced)) return 0;
//   if (![esi+58h]) return 0;
//   frame = 上面的分支
//   surface = [esi+4Ch] ? DSurface_Sidebar : TempSurface
//   rect = surface->Get_Rect()
//   pos = { [esi+0Ch] + [esi+44h], [esi+10h] + [esi+48h] }
//   flags = [esi+55h] ? 0x800 : 0
//   CC_Draw_Shape(Surface, SHP, frame, pos, &rect, Palette, flags, ...)
//   [esi+ShapeDrawn] = 1; return 1;
//
// ControlClass::Draw_Me 只决定"是否可见"，这里用 NeedsRedraw/forced 等价。
// CC_Draw_Shape 由 blit 回调承接（见头文件）。
// ----------------------------------------------------------------------------
bool ShapeButtonClass::Draw_Me(bool forced)
{
    if (!forced && !NeedsRedraw)     // asm: ControlClass::Draw_Me
        return false;
    if (Shape == nullptr)            // asm: mov eax,[esi+58h] / test / jz
        return false;

    LastFrame = SelectFrame();       // asm: ebx = 帧号

    const int32 posX = X + OffsetX;  // asm: [esi+0Ch] + [esi+44h]
    const int32 posY = Y + OffsetY;  // asm: [esi+10h] + [esi+48h]
    const uint32 flags = Shadow ? 0x800u : 0u;   // asm: and eax,800h

    if (g_ShapeBlit != nullptr)
        g_ShapeBlit(Shape, LastFrame, posX, posY, Width, Height, flags);

    ShapeDrawn = true;               // asm: mov [esi+ShapeDrawn],al
    NeedsRedraw = false;
    return true;                     // asm: mov eax,1
}
