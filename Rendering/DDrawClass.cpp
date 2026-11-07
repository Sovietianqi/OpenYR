#include "DDrawClass.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// DDrawClass.cpp —— 引擎级 DirectDraw 全局过程实现
//
// 对应 asm 里的 lpDD_VerticalBlank / lpDD_GetDDCaps / DirectDraw_Prepare 的
// 公共状态与失败尾巴。逐句对应见头文件注释与下方行内注释。
// ============================================================================

namespace {
uint8   g_DDrawVBLReady  = 0;          // asm: byte_81FC6C
void*   g_pDDraw         = nullptr;    // asm: lpDD.vt
void*   g_hWndParent     = nullptr;    // asm: hWndParent
uint8   g_WindowedMode   = 0;          // asm: WindowedMode

int32 (*s_DDrawWait)(void*, int32, void*)                             = nullptr;
int32 (*s_DDrawGetCaps)(void*, void*)                                 = nullptr;
void  (*s_DDrawErrorHandler)(int32)                                   = nullptr;
void  (*s_DDrawMsgBox)(void*, const char*, const char*, uint32)       = nullptr;
int32 (*s_DirectDrawCreate)(void*, void**, void*)                     = nullptr;
int32 (*s_DDrawSetCoop)(void*, void*, int32)                          = nullptr;
void  (*s_DDrawDebugString)(const char*)                              = nullptr;
}  // namespace

// ---- 全局状态 --------------------------------------------------------------
void  DDrawClass::Set_Instance(void* pDD)     { g_pDDraw = pDD; }
void* DDrawClass::Get_Instance()              { return g_pDDraw; }
void  DDrawClass::Set_hWnd_Parent(void* hWnd) { g_hWndParent = hWnd; }
void* DDrawClass::Get_hWnd_Parent()           { return g_hWndParent; }
void  DDrawClass::Set_VBL_Ready(uint8 ready)  { g_DDrawVBLReady = ready; }
uint8 DDrawClass::Get_VBL_Ready()             { return g_DDrawVBLReady; }

void DDrawClass::Bind_Wait(int32 (*fn)(void*, int32, void*))
{
    s_DDrawWait = fn;
}

void DDrawClass::Bind_Get_Caps(int32 (*fn)(void*, void*))
{
    s_DDrawGetCaps = fn;
}

void DDrawClass::Bind_Error_Handler(void (*fn)(int32))
{
    s_DDrawErrorHandler = fn;
}

void DDrawClass::Bind_Message_Box(void (*fn)(void*, const char*, const char*,
                                             uint32))
{
    s_DDrawMsgBox = fn;
}

// ============================================================================
// 共享失败尾巴
//
// asm: mov edx,dword_8A00B0 / test edx,edx / jz -> MessageBox 路径
//      mov ecx,eax / call edx
//      push eax / push offset aDdraw_dllError / call _sprintf
//      push 30h / push offset aDirectX / push eax / push hWndParent
//      call ds:MessageBoxA
// ============================================================================
void DDrawClass::Report_Error(int32 hr)
{
    // asm: mov edx,dword_8A00B0 / test edx,edx / jz -> MessageBox 路径
    if (s_DDrawErrorHandler != nullptr) {
        s_DDrawErrorHandler(hr);                  // asm: mov ecx,eax / call edx
        return;
    }

    // asm: push eax / push offset aDdraw_dllError / call _sprintf
    char text[0x50];                              // asm: Text = byte ptr -50h
    std::sprintf(text, "DDRAW.DLL Error code = %08X",
                 static_cast<unsigned>(hr));

    // asm: push 30h / push offset aDirectX / push eax / push hWndParent
    //      call ds:MessageBoxA
    if (s_DDrawMsgBox != nullptr)
        s_DDrawMsgBox(g_hWndParent, text, "Direct X", 0x30);
}

// ============================================================================
// asm: lpDD_VerticalBlank (0x4A4700)
// ============================================================================
void DDrawClass::Vertical_Blank()
{
    // asm: mov al,byte_81FC6C / test al,al / jz done
    if (g_DDrawVBLReady == 0)
        return;

    if (g_pDDraw == nullptr)                      // asm: mov eax,lpDD.vt / jz
        return;

    // asm: push 0 / push 1 / push eax / mov ecx,[eax] / call [ecx+58h]
    // IDirectDraw::WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL)
    const int32 hr = (s_DDrawWait != nullptr)
        ? s_DDrawWait(g_pDDraw, 1, nullptr)
        : 0;

    // asm: cmp eax,80004001h / jnz loc_4A472D
    if (hr == static_cast<int32>(0x80004001)) {   // DDERR_UNSUPPORTED
        g_DDrawVBLReady = 0;                      // asm: mov byte_81FC6C,0
        return;
    }

    // asm: test eax,eax / jz done          ; DD_OK -> 静默返回
    if (hr == 0)
        return;

    Report_Error(hr);
    // asm: add esp,50h / retn
}

// ============================================================================
// asm: lpDD_GetDDCaps (0x4A4610)
// ============================================================================
int32 DDrawClass::Get_DD_Caps()
{
    // asm: mov edx,lpDD.vt / test edx,edx / jz -> xor eax,eax / retn
    if (g_pDDraw == nullptr)
        return 0;

    if (s_DDrawGetCaps == nullptr)
        return 0;

    AsmDDCaps caps;
    std::memset(&caps, 0, sizeof(caps));          // asm: rep stosd (ecx=4Fh)
    caps.dwSize = 0x13C;                          // asm: mov [caps.dwSize],13Ch

    // asm: mov eax,[edx] / push 0 / push ecx / push edx / call [eax+2Ch]
    // IDirectDraw::GetCaps(&caps, NULL)
    const int32 hr = s_DDrawGetCaps(g_pDDraw, &caps);

    // asm: test eax,eax / jz loc_4A46AD
    if (hr != 0) {
        Report_Error(hr);
        return 0;                                 // asm: xor eax,eax / retn
    }

    // asm: mov ecx,[caps.dwCaps] / xor eax,eax
    const uint32 c = caps.dwCaps;
    uint32 result = 0;

    if (c & 0x40u)        result |= 1u;           // asm: test cl,40h
    if (c & 0x80u)        result |= 2u;           // asm: test cl,80h
    if (c & 0x10000u)     result |= 4u;           // asm: test ecx,10000h
    if (c & 0x8000000u)   result |= 8u;           // asm: test ecx,8000000h
    if (c & 0x4000000u)   result |= 0x10u;        // asm: test ecx,4000000h
    if (c & 0x2000000u)   result |= 0x20u;        // asm: test ecx,2000000h
    if (c & 0x200u)       result |= 0x40u;        // asm: test ch,2  (bit 9)

    return static_cast<int32>(result);            // asm: retn
}

void DDrawClass::Bind_DirectDrawCreate(int32 (*fn)(void*, void**, void*))
{
    s_DirectDrawCreate = fn;
}

void DDrawClass::Bind_Set_Coop(int32 (*fn)(void*, void*, int32))
{
    s_DDrawSetCoop = fn;
}

void DDrawClass::Bind_Debug_String(void (*fn)(const char*))
{
    s_DDrawDebugString = fn;
}

void DDrawClass::Set_Windowed_Mode(uint8 windowed) { g_WindowedMode = windowed; }
uint8 DDrawClass::Get_Windowed_Mode()              { return g_WindowedMode; }

// ============================================================================
// asm: DirectDraw_Prepare (0x4A3FF0)   —— 初始化 DirectDraw
//
//   push offset aPrepDirectDraw / call WWDebugString
//   mov eax,lpDD / test eax,eax / jnz loc_4A4016      ; 已初始化 -> 直接返回
//   push 0 / push offset lpDD / push 0 / call DirectDrawCreate
//   test eax,eax / jz loc_4A4057
//   ... dword_8A00B0 处理器 / "DirectDrawCreate failed!" 尾巴
// loc_4A4057:
//   mov [lpDD],eax
//   mov al,WindowedMode / test / jz -> push 11h ; else push 8
//   mov edx,hWndParent / push edx / push eax
//   mov ecx,[eax] / call [ecx+50h]                    ; SetCooperativeLevel
//   test eax,eax / jz loc_4A4016                      ; DD_OK -> 返回
//   ... 失败尾巴
//
// 调用者：WinMain。与 Vertical_Blank / Get_DD_Caps 共享同一失败尾巴
// dword_8A00B0（即 Report_Error）。
// ============================================================================
void DDrawClass::Prepare()
{
    // asm: push offset aPrepDirectDraw / call WWDebugString
    if (s_DDrawDebugString != nullptr)
        s_DDrawDebugString("Prep direct draw.\n");

    // asm: mov eax,lpDD / test eax,eax / jnz loc_4A4016
    void* pDD = g_pDDraw;
    if (pDD != nullptr)
        return;

    // asm: push 0 / push offset lpDD / push 0 / call DirectDrawCreate
    const int32 hr = (s_DirectDrawCreate != nullptr)
        ? s_DirectDrawCreate(nullptr, &pDD, nullptr)
        : -1;

    // asm: test eax,eax / jz loc_4A4057
    if (hr != 0) {
        Report_Error(hr);                       // asm: dword_8A00B0 尾巴
        if (s_DDrawDebugString != nullptr)
            s_DDrawDebugString("DirectDrawCreate failed!\n");
        return;                                 // asm: loc_4A4016
    }

    g_pDDraw = pDD;                             // asm: mov [lpDD],eax

    // asm: mov al,WindowedMode / test / jz -> push 11h (全屏) ; else push 8
    const int32 flags = (g_WindowedMode != 0) ? 8 : 0x11;

    // asm: mov edx,hWndParent / push edx / push eax / call [ecx+50h]
    const int32 hr2 = (s_DDrawSetCoop != nullptr)
        ? s_DDrawSetCoop(pDD, g_hWndParent, flags)
        : 0;

    // asm: test eax,eax / jz loc_4A4016
    if (hr2 != 0) {
        Report_Error(hr2);                      // asm: loc_4A408E 尾巴
        return;
    }
}

// ============================================================================
// asm: lpDD_GetDispInfo (真实入口 ≈0x4A4161)
//
//   lpDD_GetDispInfo proc near     ; 无参无 this（retn）
//   调用者：WinMain+1FD9。返回调整后的 VRAM 估算值。
//   （AsmComplex5 原标注 0x4A3FD0 在 asm 中未找到；由 loc_4A4179 = +0x18
//     反推真实入口。与 Get_DD_Caps(0x4A4610) 紧邻、同族。）
// ============================================================================
namespace {
int32 (*s_DispGetCaps)(void*, void*)              = nullptr;
int32 (*s_DispGetDisplayMode)(void*, void*)       = nullptr;
int32 (*s_DispRelease)(void*)                     = nullptr;
void  (*s_DispDebug2)(const char*, int32, int32)  = nullptr;
void  (*s_DispDebug4)(const char*, int32, int32, int32, int32) = nullptr;
}

void DDrawClass::Bind_Get_Display_Mode(int32 (*fn)(void*, void*))
{
    s_DispGetDisplayMode = fn;
}
void DDrawClass::Bind_Release(int32 (*fn)(void*))
{
    s_DispRelease = fn;
}
void DDrawClass::Bind_Debug_String_2(void (*fn)(const char*, int32, int32))
{
    s_DispDebug2 = fn;
}
void DDrawClass::Bind_Debug_String_4(void (*fn)(const char*, int32, int32,
                                               int32, int32))
{
    s_DispDebug4 = fn;
}

int32 DDrawClass::Get_Disp_Info()
{
    bool  createdHere = false;      // asm: bl
    int32 vram        = 0;          // asm: esi

    void* pDD = g_pDDraw;

    if (pDD == nullptr) {
        // asm: push 0 / push offset lpDD / push 0 / call DirectDrawCreate
        const int32 hr = (s_DirectDrawCreate != nullptr)
            ? s_DirectDrawCreate(nullptr, &pDD, nullptr)
            : -1;
        if (hr != 0) {
            // asm: dword_8A00B0 handler or MessageBox, then lpDD = 0 ; return 0
            Report_Error(hr);
            g_pDDraw = nullptr;
            return 0;
        }
        createdHere = true;                     // asm: bl = 1
        g_pDDraw    = pDD;
    }

    if (pDD != nullptr) {
        // asm: mov ecx,4Fh / xor eax,eax / rep stosd ; mov dwSize,13Ch
        uint8 caps[0x13C];
        std::memset(caps, 0, sizeof(caps));
        *reinterpret_cast<int32*>(caps + 0x00) = 0x13C;

        // asm: push 0 / push &caps / push eax / call [ecx+2Ch]
        const int32 hrCaps = (s_DispGetCaps != nullptr)
            ? s_DispGetCaps(pDD, caps)
            : -1;

        if (hrCaps == 0) {
            // asm: mov eax,[caps+3Ch]      ; VRAM 镜像
            vram = *reinterpret_cast<int32*>(caps + 0x3C);
            if (s_DispDebug2 != nullptr)
                s_DispDebug2("DDraw returnded actual VRAM of %d bytes (%dMb)",
                             vram, static_cast<uint32>(vram) >> 0x14);

            // asm: mov ecx,1Bh / xor eax,eax / rep stosd ; mov dwSize,6Ch
            uint8 desc[0x6C];
            std::memset(desc, 0, sizeof(desc));
            *reinterpret_cast<int32*>(desc + 0x00) = 0x6C;

            // asm: push &desc / push eax / call [edx+30h]
            const int32 hrMode = (s_DispGetDisplayMode != nullptr)
                ? s_DispGetDisplayMode(pDD, desc)
                : -1;

            if (hrMode == 0) {
                // 本游戏自有 DDSURFACEDESC 镜像布局：
                //   width@0x14  height@0x18  bpp@0x28 (ddpf+0Ch)  pitch@0x3C
                const int32 width  = *reinterpret_cast<int32*>(desc + 0x14);
                const int32 height = *reinterpret_cast<int32*>(desc + 0x18);
                const int32 bpp    = *reinterpret_cast<int32*>(desc + 0x28);
                const int32 pitch  = *reinterpret_cast<int32*>(desc + 0x3C);

                if (s_DispDebug4 != nullptr)
                    s_DispDebug4(
                        "Current display size is %dx%dx%d. Pitch is %d",
                        width, height, bpp, pitch);

                // asm: mov eax,[bpp] / shr eax,3 / imul eax,[width]
                //      / imul eax,[height] / add esi,eax
                const int32 bytesPx = static_cast<int32>(
                    static_cast<uint32>(bpp) >> 3);
                vram += bytesPx * width * height;
            }

            // asm: add esi,20000h / and esi,0FFF00000h
            vram = static_cast<int32>(
                (static_cast<uint32>(vram) + 0x20000u) & 0xFFF00000u);

            if (s_DispDebug2 != nullptr)
                s_DispDebug2("Adjusted VRAM size is %d bytes (%dMb)\n",
                             vram, static_cast<uint32>(vram) >> 0x14);
        }

        // asm: test bl,bl / jz done
        if (createdHere) {
            // asm: push eax / call [edx+8]         (Release 探测)
            const int32 hrRel = (s_DispRelease != nullptr)
                ? s_DispRelease(pDD)
                : 0;
            if (hrRel != 0)
                Report_Error(hrRel);
            g_pDDraw = nullptr;                  // asm: mov lpDD,0
        }
    }

    return vram;                                 // asm: mov eax,esi
}
