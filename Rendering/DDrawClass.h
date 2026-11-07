#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// DDrawClass —— 引擎级 DirectDraw 全局对象（asm: lpDD / hWndParent / byte_81FC6C）
//
// 原版这几个过程操作的都是**全局** DirectDraw 实例，而不是某个渲染对象的成员：
//   lpDD_VerticalBlank (0x4A4700)  —— 等待垂直回扫
//   lpDD_GetDDCaps     (0x4A4610)  —— 查询 DDCAPS 并折算成能力位
//   DirectDraw_Prepare (0x4A3FF0)  —— 初始化（同族的失败尾巴）
//
// 它们共享同一套全局状态：
//   lpDD            IDirectDraw*   主 DirectDraw 实例
//   hWndParent      窗口句柄（MessageBox 的父窗口）
//   byte_81FC6C     "VBL 可用" 标志；DDERR_UNSUPPORTED 后置 0 不再重试
//   dword_8A00B0    自定义错误回调；置空时回落到 MessageBoxA
//
// 本项目把 COM 调用面抽成可注入的挂钩（铁律①：只写能确证的部分——dump 里
// 能看到 vtable 偏移 +0x58 = WaitForVerticalBlank、+0x2C = GetCaps，
// 但 DLL 实现不在仓库内）。
// ============================================================================

class DDrawClass
{
public:
    // ---- 全局状态访问 ------------------------------------------------------
    static void  Set_Instance(void* pDD);           // asm: lpDD
    static void* Get_Instance();
    static void  Set_hWnd_Parent(void* hWnd);       // asm: hWndParent
    static void* Get_hWnd_Parent();
    static void  Set_VBL_Ready(uint8 ready);        // asm: byte_81FC6C
    static uint8 Get_VBL_Ready();

    // ---- 可注入的 COM / 平台面 --------------------------------------------
    // asm: IDirectDraw::WaitForVerticalBlank @ vtable +0x58
    static void Bind_Wait(int32 (*fn)(void* pDD, int32 flags, void* hWnd));
    // asm: IDirectDraw::GetCaps @ vtable +0x2C
    static void Bind_Get_Caps(int32 (*fn)(void* pDD, void* pCaps));
    // asm: dword_8A00B0 自定义错误处理器
    static void Bind_Error_Handler(void (*fn)(int32 hr));
    // asm: MessageBoxA(hWnd, text, "Direct X", 0x30)
    static void Bind_Message_Box(void (*fn)(void* hWnd, const char* text,
                                            const char* caption, uint32 type));
    // asm: DirectDrawCreate(0, &lpDD, 0)
    static void Bind_DirectDrawCreate(int32 (*fn)(void* pGUID, void** ppDD,
                                                  void* pUnkOuter));
    // asm: IDirectDraw::SetCooperativeLevel @ vtable +0x50
    static void Bind_Set_Coop(int32 (*fn)(void* pDD, void* hWnd, int32 flags));
    // asm: WWDebugString(const char*)
    static void Bind_Debug_String(void (*fn)(const char* pText));
    // asm: WindowedMode (全局窗口化标志)
    static void Set_Windowed_Mode(uint8 windowed);
    static uint8 Get_Windowed_Mode();

    // ========================================================================
    // asm: DirectDraw_Prepare (0x4A3FF0)   —— 初始化 DirectDraw
    //
    //   DirectDraw_Prepare proc near  ; CODE XREF: WinMain+2034
    //
    //   push offset aPrepDirectDraw / call WWDebugString
    //   mov eax,lpDD / test eax,eax / jnz loc_4A4016     ; 已初始化 -> 直接返回
    //   push 0 / push offset lpDD / push 0 / call DirectDrawCreate
    //   test eax,eax / jz loc_4A4057
    //   ... dword_8A00B0 处理器 / "DirectDrawCreate failed!" 尾巴
    // loc_4A4057:
    //   mov [lpDD],eax
    //   mov al,WindowedMode / test / jz -> push 11h (全屏) ; else push 8 (窗口)
    //   mov edx,hWndParent / push edx / push eax
    //   mov ecx,[eax] / call [ecx+50h]                   ; SetCooperativeLevel
    //   test eax,eax / jz loc_4A4016                     ; DD_OK -> 返回
    //   ... 失败尾巴
    // ========================================================================
    static void Prepare();

    // ========================================================================
    // asm: lpDD_VerticalBlank (0x4A4700)
    //
    //   mov al,byte_81FC6C / test al,al / jz done      ; VBL 不可用直接返回
    //   mov eax,lpDD.vt / test eax,eax / jz done
    //   push 0 / push 1 / push eax
    //   mov ecx,[eax] / call [ecx+58h]                 ; WaitForVerticalBlank
    //   cmp eax,80004001h / jnz loc_4A472D
    //   mov byte_81FC6C,0                              ; DDERR_UNSUPPORTED
    //   retn
    // loc_4A472D:
    //   test eax,eax / jz done                         ; DD_OK -> 静默返回
    //   ... 失败尾巴（DDraw_Report_Error）
    // ========================================================================
    static void Vertical_Blank();

    // ========================================================================
    // asm: lpDD_GetDDCaps (0x4A4610)
    //
    //   mov edx,lpDD.vt / test edx,edx / jz -> return 0
    //   DDCAPS caps = {0}; caps.dwSize = 0x13C        ; rep stosd (ecx=4Fh)
    //   push 0 / push &caps / push edx
    //   mov eax,[edx] / call [eax+2Ch]                 ; GetCaps
    //   test eax,eax / jnz 失败尾巴 -> return 0
    //   mov ecx,[caps.dwCaps] / xor eax,eax            ; 折算能力位
    //     test cl,40h      -> |1
    //     test cl,80h      -> |2
    //     test ecx,10000h  -> |4
    //     test ecx,8000000h-> |8
    //     test ecx,4000000h-> |0x10
    //     test ecx,2000000h-> |0x20
    //     test ch,2        -> |0x40     (bit 9)
    // ========================================================================
    static int32 Get_DD_Caps();

    // ========================================================================
    // 共享失败尾巴：dword_8A00B0 有处理器就调用，否则弹
    // "DDRAW.DLL Error code = %08X" 的 MessageBoxA。
    // lpDD_VerticalBlank / lpDD_GetDDCaps / DirectDraw_Prepare 三处同源。
    // ========================================================================
    static void Report_Error(int32 hr);

    // ========================================================================
    // asm: lpDD_GetDispInfo (真实入口 ≈0x4A4161)
    //
    //   lpDD_GetDispInfo proc near
    //   无参无 this（retn）；返回调整后的 VRAM 估算值。
    //   调用者：WinMain+1FD9。
    //
    //   （AsmComplex5 原标注 0x4A3FD0 在 asm 中未找到；真实入口由
    //     loc_4A4179 = +0x18 反推。与 Get_DD_Caps(0x4A4610) 紧邻、同族。）
    //
    //   esi = 0 ; createdHere = false
    //   if (!lpDD):
    //     hr = DirectDrawCreate(0, &lpDD, 0)
    //     hr != 0 -> Report_Error ; lpDD = 0 ; return 0
    //     createdHere = true
    //   if (lpDD):
    //     caps{0x13C} ; caps.dwSize = 0x13C
    //     if (lpDD->GetCaps(&caps,0) == 0):            (vt+2Ch)
    //       vram = caps[+3Ch]
    //       DebugString("DDraw returnded actual VRAM of %d bytes (%dMb)")
    //       desc{0x6C} ; desc.dwSize = 0x6C
    //       if (lpDD->GetDisplayMode(&desc) == 0):     (vt+30h)
    //         DebugString("Current display size is %dx%dx%d. Pitch is %d")
    //         vram += (bpp/8) * width * height
    //       vram = (vram + 0x20000) & 0xFFF00000
    //       DebugString("Adjusted VRAM size is %d bytes (%dMb)\n")
    //   if (createdHere && lpDD):
    //     if (lpDD->[vt+8](lpDD) != 0) Report_Error    (Release 探测)
    //     lpDD = 0
    //   return vram
    // ========================================================================
    static void Bind_Get_Display_Mode(int32 (*fn)(void* pDD, void* pDesc));
    static void Bind_Release(int32 (*fn)(void* pDD));
    static void Bind_Debug_String_2(void (*fn)(const char* fmt, int32 a1,
                                              int32 a2));
    static void Bind_Debug_String_4(void (*fn)(const char* fmt, int32 a1,
                                              int32 a2, int32 a3, int32 a4));
    static int32 Get_Disp_Info();

private:
    // 0x13C 字节的 _DDCAPS 记录：dwSize@0, dwCaps@4，其余填充。
    struct AsmDDCaps
    {
        uint32 dwSize;      // +0x00
        uint32 dwCaps;      // +0x04
        uint32 raw[0x4D];   // 0x13C 记录的剩余部分
    };
};
