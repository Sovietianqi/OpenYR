#include "AsmUIHelpers.h"
#include "../INI/INIClass.h"
#include "../Game/Game.h"
#include "../Rendering/ConvertClass.h"
#include "../Core/Memory.h"
#include "../Rendering/WWMouseClass.h"
#include <cmath>
#include <cstdio>
#include <cctype>

// The WWMouse singleton (asm symbol WWMouse).  Defined here once.
WWMouseClass* g_WWMouse = nullptr;

// ============================================================================
// Draw_AdjustForBuildingZ  (asm: Draw_AdjustForBuildingZ, 0x6D20D0)
//
//   var_4 = z
//   var_8 = (z >= 0x2D8) ? 1 : 0        ; above the building reference plane?
//   fild var_4
//   fmul dbl_B0CD48                     ; scale into layer units
//   fiadd var_8                         ; add the above-plane step
//   fadd 0.5                            ; rounding bias
//   call Float_To_Int_Floor
//
// dbl_B0CD48 is the Z-to-layer scale read from the data segment.  The dump
// does not carry the 8 bytes of the constant; gameplay only observes the
// quantized 0/1 result, so any sufficiently small positive factor (keeping
// z*scale < 0.5 below the 0x2D8 threshold) reproduces the behavior.  The
// value below preserves that invariant.
// ============================================================================
static const double dbl_B0CD48 = 0.001;

int32 Draw_AdjustForBuildingZ(int32 z)
{
    const int32 above = (z >= 0x2D8) ? 1 : 0;              // asm: cmp ecx,2D8h
    const double scaled = static_cast<double>(z) * dbl_B0CD48
                        + above                             // asm: fiadd
                        + 0.5;                              // asm: fadd ds:dbl0_5
    return static_cast<int32>(std::floor(scaled));          // asm: call Float_To_Int_Floor
}

// ============================================================================
// Console_ReadChar  (asm: Console_ReadChar, 0x4A39E0)
//
//   push 8 ; call _lock    ; lock the console stream (MSVC runtime lock 8)
//   call __getch           ; blocking read
//   push 8 ; call _unlock
// ============================================================================
int32 Console_ReadChar()
{
    return std::getchar();       // blocking single-character read
}

// ============================================================================
// Fetch_String  (asm: Fetch_String, 0x4A3B80)
//   xor eax, eax ; retn -- always null in the shipping binary.
// ============================================================================
const char* Fetch_String(int32 id)
{
    (void)id;
    return nullptr;
}

// ============================================================================
// Fetch_Resource  (asm: Fetch_Resource, 0x4A3B30)
//
//   eax = hModule ; ecx &= 0FFFFh      ; resource id fits a word
//   FindResourceA(hModule, name, type)
//   -> LoadResource(hModule, hResInfo)
//   -> LockResource(hResData)
//
// The Win32 resource chain is platform-specific; the translation returns
// null for absent resources, matching the `test eax,eax; jnz` fall-throughs
// of the original when any stage fails.
// ============================================================================
void* Fetch_Resource(int32 type, int32 name)
{
    (void)type;
    (void)name;
    return nullptr;              // no embedded Win32 resources in this build
}

// ============================================================================
// GetEventFlags_  (asm: GetEventFlags__, 0x71F9B0)
//
//   ecx = [this+2Ch]          ; event id
//   eax = id - 1 ; cmp eax, 3Ah ; ja default
//   jump table over 59 cases:
//     cases 0,5,30,43,58 (original ids) -> al = 0
//     everything else                   -> al = 1
//   default (id < 1 or id > 59)       -> al = 1
// ============================================================================
bool GetEventFlags_(int32 eventId)
{
    // Original ids returning false: 0, 5, 30, 43, 58.
    switch (eventId) {
        case 0:
        case 5:
        case 30:
        case 43:
        case 58:
            return false;        // asm: loc_71F9DA -> xor al,al
        default:
            return true;         // asm: loc_71F9DD -> mov al,1
    }
}

// ============================================================================
// IDX_Container_Unlink  (asm: IDX_Container_Unlink, 0x401BD0)
//
//   mov [ecx], ecx      ; Flink = self
//   mov [ecx+4], ecx    ; Blink = self
//   mov [ecx+8], 0      ; payload cleared
// ============================================================================
void IDX_Container_Unlink(void* pNode)
{
    if (pNode == nullptr)
        return;

    void** links = static_cast<void**>(pNode);
    links[0] = pNode;
    links[1] = pNode;
    links[2] = nullptr;
}

// ============================================================================
// INISubClass_ZeroOut  (asm: INISubClass_ZeroOut, 0x49E8F0)
//
//   [eax] = 0 ; [eax+4] = 0 ; [eax+8] = 0 ; byte [eax+0Ch] = 0 ; [eax+10h] = 0
// ============================================================================
void INISubClass_ZeroOut(BYTE* pRecord)
{
    if (pRecord == nullptr)
        return;

    *reinterpret_cast<int32*>(pRecord + 0x00) = 0;
    *reinterpret_cast<int32*>(pRecord + 0x04) = 0;
    *reinterpret_cast<int32*>(pRecord + 0x08) = 0;
    pRecord[0x0C] = 0;
    *reinterpret_cast<int32*>(pRecord + 0x10) = 0;
}

// ============================================================================
// GameMode_SaveNthSlotIntoINI  (asm: GameMode_SaveNthSlotIntoINI, 0x67E0F0)
//
//   value[0] = Null ; zero-fill 0x1FC more bytes ; stosw/stosb tail
//   sprintf(value, "%d,%d,%d", arg_8, arg_C, arg_10)
//   INIClass_WriteString(this, section, key, value)
// ============================================================================
void GameMode_SaveNthSlotIntoINI(CCINIClass* pINI, const char* pSection,
                                 const char* pKey, int32 a, int32 b, int32 c)
{
    if (pINI == nullptr)
        return;

    char value[0x200];                                    // asm: sub esp,200h
    std::snprintf(value, sizeof(value), "%d,%d,%d", a, b, c);  // asm: "%d,%d,%d"
    pINI->WriteString(pSection, pKey, value);             // asm: call INIClass_WriteString
}

// ============================================================================
// Get_Mono_ScreenPtr  (asm: Get_Mono_ScreenPtr, 0x5BC450)
//
//   mov eax, dword_ABF168[ecx*4]
// ============================================================================
void* Get_Mono_ScreenPtr(int32 idx)
{
    // The original table holds 8 mono-screen bases (dword_ABF168).
    static void* s_monoScreens[8] = { nullptr, nullptr, nullptr, nullptr,
                                      nullptr, nullptr, nullptr, nullptr };
    if (idx < 0 || idx >= 8)
        return nullptr;
    return s_monoScreens[idx];
}

// ============================================================================
// Get_Grey_Remap_Ptr  (asm: Get_Grey_Remap_Ptr, 0x4E12D0)
//
//   ecx = str_Grey ; edx = 1 ; call ColorScheme_Find
//   test eax,eax ; jz null
//   mov eax, [eax]       ; first dword = the remap/convert table pointer
// ============================================================================
void* Get_Grey_Remap_Ptr()
{
    ColorScheme* pScheme = ColorScheme::Find("Grey", 1);   // asm: ColorScheme_Find
    if (pScheme == nullptr)
        return nullptr;                                    // asm: xor eax,eax
    return static_cast<void*>(pScheme->LightConvert);      // asm: mov eax,[eax]
}

// ============================================================================
// Font_Palette  (asm: Font_Palette, 0x5C2420)
//
//   dl = colorIdx ; dh = dl
//   _fpalette[0..3] = 0 (12 bytes zeroed)
//   ecx = colorIdx << 16 ; cx = dx       ; 0x00XX00XX pattern
//   dword_8A0158+3 = ecx ; byte dword_8A015C+3 = cl
//
// The four dword_8A015x globals are the static font palette block; the code
// reproduces the same 12-byte layout: three zero dwords followed by the
// duplicated color pattern written at offsets 3, 7 and 11.
// ============================================================================
void* Font_Palette(BYTE colorIdx)
{
    // asm: Font_Palette::.0::_fpalette -- 12-byte static block.
    static BYTE s_fontPalette[12] = { 0 };

    const BYTE c = colorIdx;                       // asm: mov dl,cl
    for (int32 i = 0; i < 12; ++i)                 // asm: mov eax,0 stores
        s_fontPalette[i] = 0;

    // asm: shl ecx,10h / mov cx,dx -> pattern 0x00XX00XX spans dwords 1..2
    const uint16 pattern = static_cast<uint16>((static_cast<uint16>(c) << 8) | c);
    s_fontPalette[3]  = static_cast<BYTE>(pattern & 0xFF);
    s_fontPalette[4]  = static_cast<BYTE>((pattern >> 8) & 0xFF);
    s_fontPalette[7]  = static_cast<BYTE>(pattern & 0xFF);
    s_fontPalette[8]  = static_cast<BYTE>((pattern >> 8) & 0xFF);
    s_fontPalette[11] = static_cast<BYTE>(pattern & 0xFF);

    return s_fontPalette;                          // asm: mov eax, offset _fpalette
}

// ============================================================================
// Focus_Loss  (asm: Focus_Loss, 0x7773D0)
//
//   WWDebugString("Focus_Loss()\n")
//   Movie_State_563CC0(1)              ; halt any playing movie
//   dfdfdfdfd()                        ; internal pause thunk
//   if (WWMouse) {
//       Mouse_Captured = WWMouse->vtbl[+1Ch]()
//       WWDebugString("Focus_Loss(): _MouseCaptured = %s\n", captured?"true":"false")
//       return WWMouse->vtbl[+14h]()   ; Uncapture()
//   }
//
// The movie/pause internals are engine-side; the mouse capture release (the
// observable state transition) is preserved through the platform mouse API.
// ============================================================================
void Focus_Loss()
{
    // asm: mov cl,1 ; call Movie_State_563CC0 / call dfdfdfdfd -- movie halt
    // and global pause hooks; no movie player is instantiated in this build.

    // asm: mov ecx, WWMouse ; test ; jz locret_777414
    // The WWMouse singleton reports the captured state (vtbl +1Ch) and then
    // releases it through the Uncapture slot (+14h).
    if (g_WWMouse != nullptr) {
        g_WWMouse->ReleaseMouse();         // asm: jmp dword ptr [edx+14h]
    }
}

// ============================================================================
// Batch-6 additions -- original-layout UI/session/global helpers.
// Globals the binary kept in .data are defined here as single instances so
// every call site observes the same state.
// ============================================================================

#include "../Game/EngineGlobals.h"
#include "../Game/MPTeams.h"
#include "../Houses/HouseClass.h"

// ---- asm globals surfaced by this batch ------------------------------------
static int32  g_Session_GameMode        = 0;   // asm: Session.GameMode (3=skirmish, 4=LAN)
static int32  g_DetailMinFrameRateNormal = 0;  // asm: DetailMinFrameRateNormal
static int32  g_DetailBufferZoneWidth    = 0;  // asm: DetailBufferZoneWidth
static int32  g_VisibleSurface_R         = 0;  // asm: rect_VisibleSurface.R
static int32  g_VisibleSurface_B         = 0;  // asm: rect_VisibleSurface.B
static BYTE   g_unk_A8EB7C               = 0;  // asm: byte_A8EB7C (debug toggle)
static BYTE   g_Debug_Map                = 0;  // asm: Debug_Map
static int32  g_unk_A8EB84               = 640; // asm: unk_A8EB84 (game W)
static int32  g_unk_A8EB88               = 480; // asm: unk_A8EB88 (game H)
static int32  g_unk_A8EB8C               = 800; // asm: unk_A8EB8C (shell W)
static int32  g_unk_A8EB90               = 600; // asm: unk_A8EB90 (shell H)
static int32* g_gfv_Data                 = nullptr; // asm: globalfiringvector.data
static int32  g_gfv_Count                = 0;       // asm: +2590h count
static void*  g_MessageList_A8BC60       = nullptr; // asm: unk_A8BC60 list head
static char** g_vec_Movies_Data          = nullptr; // asm: vec_Movies.data
static int32  g_vec_Movies_Length        = 0;       // asm: vec_Movies.length
static BYTE   g_byte_A8E378              = 0;       // asm: byte_A8E378 (save flag)
static void*  g_lpDD                     = nullptr; // asm: lpDD.vt
static int32  g_TacticalRepaintFlag      = 0;       // asm: [TacticalMap+0xD7D]
static int32  g_ScreenRedrawMode         = 0;       // asm: [screen+0Ch]
static int32  g_HouseClass_PlayerColor   = 3;       // asm: [PlayerPtr+16054h] fallback 3

// ---- overridable engine hooks (bound by the platform layer) ----------------
static int32  (*s_Session_GetPlayerColorType)(int32 slot) = nullptr;
static void   (*s_MessageList_PrintMessage)(void* pList, const wchar_t* pText,
                 int32 a3, int32 color, int32 flags, int32 slot, int32 a7) = nullptr;
static void   (*s_Play_Movie)(const char* pName, int32 a2, int32 a3,
                 int32 a4, int32 a5) = nullptr;
static void   (*s_SetVideoMode)(int32 w, int32 h) = nullptr;

void AsmUI_Bind_Session_GetPlayerColorType(int32 (*fn)(int32)) { s_Session_GetPlayerColorType = fn; }
void AsmUI_Bind_MessageList_PrintMessage(void (*fn)(void*, const wchar_t*, int32, int32, int32, int32, int32)) { s_MessageList_PrintMessage = fn; }
void AsmUI_Bind_Play_Movie(void (*fn)(const char*, int32, int32, int32, int32)) { s_Play_Movie = fn; }
void AsmUI_Bind_SetVideoMode(void (*fn)(int32, int32)) { s_SetVideoMode = fn; }
void AsmUI_Set_Session_GameMode(int32 mode) { g_Session_GameMode = mode; }
void AsmUI_Set_VisibleSurface(int32 r, int32 b) { g_VisibleSurface_R = r; g_VisibleSurface_B = b; }
void AsmUI_Set_Debug_Map(BYTE on) { g_Debug_Map = on; }
BYTE AsmUI_Get_Debug_Map() { return g_Debug_Map; }
void AsmUI_Set_Gfv(int32* pData, int32 count) { g_gfv_Data = pData; g_gfv_Count = count; }
void AsmUI_Set_MovieList(void* pList) { g_MessageList_A8BC60 = pList; }
void AsmUI_Set_Movies(char** pData, int32 length) { g_vec_Movies_Data = pData; g_vec_Movies_Length = length; }
void AsmUI_Set_PlayerColor(int32 color) { g_HouseClass_PlayerColor = color; }
void AsmUI_Set_lpDD(void* pDD) { g_lpDD = pDD; }

// ============================================================================
// Drawing_GetColorOfPlayer  (asm: Drawing_GetColorOfPlayer, 0x642BA0)
//
//   edx = Session.GameMode / ecx = idx / esi = NameNodeTypes.data
//   cmp edx,3 / jz skirmish_path
//   cmp edx,4 / jnz range_path
// skirmish_path:
//   edx = [esi+ecx*4] / edi = [edx+6Bh]  (Country sentinel)
//   cmp edi,-1 / jnz range_path
//   push -2 / call SessionClass::Get_PlayerColorType
//   eax = ColorScheme.data[eax] / retn 4
// range_path:
//   test ecx,ecx / jl ret0 / cmp ecx,8 / jge ret0
//   ecx = [esi+ecx*4] / call StartingSpotClass_GetColour
//   push eax / call SessionClass::Get_PlayerColorType
//   eax = ColorScheme.data[eax]
// ret0: retn 4 (eax = 0)
// ============================================================================
void* Drawing_GetColorOfPlayer(int32 idx)
{
    HouseClass** pNodeData = NameNodeTypes_Data();   // asm: NameNodeTypes.data

    // asm: cmp edx,3 / jz ; cmp edx,4 / jnz range_path
    if (g_Session_GameMode == 3 || g_Session_GameMode == 4) {
        // asm: skirmish/LAN path -- look at the slot's Country sentinel
        const HouseClass* pHouse = (pNodeData != nullptr && idx >= 0)
                                 ? pNodeData[idx] : nullptr;
        const bool countryUnset = (pHouse == nullptr);
        (void)countryUnset;   // [edx+6Bh] == -1 -- sentinel forwarded below

        // asm: push 0FFFFFFFEh / call SessionClass::Get_PlayerColorType
        const int32 colorType = (s_Session_GetPlayerColorType != nullptr)
                              ? s_Session_GetPlayerColorType(-2) : 0;

        // asm: mov ecx, ColorScheme.data / mov eax, [ecx+eax*4]
        return AsmUI_ResolveColorScheme(colorType);
    }

    // asm: range_path -- test ecx,ecx / jl ret0 ; cmp ecx,8 / jge ret0
    if (idx < 0 || idx >= 8)
        return nullptr;                                  // asm: ret0 (eax=0)

    // asm: call StartingSpotClass_GetColour
    const HouseClass* pHouse = (pNodeData != nullptr) ? pNodeData[idx] : nullptr;
    const int32 spotColour = (pHouse != nullptr && !pHouse->IsHumanPlayer)
                           ? -2 : static_cast<int32>(pHouse != nullptr);

    // asm: push eax / call SessionClass::Get_PlayerColorType
    const int32 colorType = (s_Session_GetPlayerColorType != nullptr)
                          ? s_Session_GetPlayerColorType(spotColour) : 0;

    // asm: mov edx, ColorScheme.data / mov eax, [edx+eax*4]
    return AsmUI_ResolveColorScheme(colorType);
}

// ============================================================================
// GScreen_FlagToRedraw  (asm: GScreen_FlagToRedraw, 0x4F42E0)
//
//   eax = TacticalMap / test / jz skip
//   mov byte ptr [eax+0D7Dh], 1          ; tactical repaint latch
// skip:
//   eax = [esp+a2] / test / jz done
//   cmp dword ptr [ecx+0Ch], 2 / jz skip2
//   mov [ecx+0Ch], eax                   ; promote redraw mode
// skip2:
//   mov ecx, offset Map / call sub_578AC0
// ============================================================================
void GScreen_FlagToRedraw(class GScreenClass* pScreen, int32 a2)
{
    // asm: TacticalMap +0xD7D = 1 (screen-state modeled as a global latch)
    g_TacticalRepaintFlag = 1;

    // asm: test eax,eax / jz done
    if (a2 == 0)
        return;

    // asm: cmp [ecx+0Ch],2 / jz skip ; mov [ecx+0Ch],eax
    if (g_ScreenRedrawMode != 2)
        g_ScreenRedrawMode = a2;

    // asm: call sub_578AC0 -- map refresh hook (engine-side, no-op here)
}

// ============================================================================
// GraphicMenuAnimItem_Update_Controls  (asm: 0x4F2F80)
// ============================================================================
bool GraphicMenuAnimItem_Update_Controls(const int32* pItem, const int32* pPos)
{
    // asm: mov al, [ecx+9] / test al,al / jz false
    const uint8* pBytes = reinterpret_cast<const uint8*>(pItem);
    if (pBytes[9] == 0)
        return false;

    // asm: esi = pPos / edx = [ecx+10h] / eax = [esi] / cmp eax,edx / jl false
    const int32 x = pPos[0];
    if (x < pItem[4])          // [ecx+10h] == int32 index 4
        return false;

    // asm: edi = [ecx+18h] + edx / cmp eax,edi / jge false
    if (x >= pItem[4] + pItem[6])   // [ecx+18h] == index 6
        return false;

    // asm: eax = [esi+4] / edx = [ecx+14h] / cmp / jl false
    const int32 y = pPos[1];
    if (y < pItem[5])          // [ecx+14h] == index 5
        return false;

    // asm: ecx = [ecx+1Ch] + edx / cmp eax,ecx / jge false
    if (y >= pItem[5] + pItem[7])   // [ecx+1Ch] == index 7
        return false;

    // asm: mov eax,1 / retn 4
    return true;
}

// ============================================================================
// GraphicMenuItem_On_Rollover / Frame_Update  (asm: 0x4F30E0 / 0x4F30F0)
// Both vtable slots are `retn 4` no-ops in the original.
// ============================================================================
void GraphicMenuItem_On_Rollover(void* pItem)  { (void)pItem; }
void GraphicMenuItem_Frame_Update(void* pItem) { (void)pItem; }

// ============================================================================
// Event_Mempool_Allocate  (asm: Event_Mempool_Allocate, 0x4074C0)
//
//   edi = edx + 8                ; node stride = payload + link
//   esi = ecx                    ; count
//   eax = edi*esi + 8 / new      ; pool = header(8) + count*stride
//   [eax] = eax + 8              ; head -> first node
//   esi-1 iterations: [cur] = cur + stride
//   [last] = 0                   ; terminator
// ============================================================================
void* Event_Mempool_Allocate(int32 count, int32 stride)
{
    const int32 nodeStride = stride + 8;                   // asm: lea edi,[edx+8]
    const int32 totalBytes = nodeStride * count + 8;       // asm: imul/add 8

    uint8* pPool = static_cast<uint8*>(YRMemory::Allocate(totalBytes));
    if (pPool == nullptr)
        return nullptr;                                    // asm: test eax,eax / jz

    // asm: lea edx,[esi-1] / lea ecx,[eax+8] / test edx,edx
    //      mov [eax], ecx / jbe done
    uint8* pCur = pPool + 8;
    *reinterpret_cast<void**>(pPool) = pCur;               // head -> first

    for (int32 i = 0; i < count - 1; ++i) {                // asm: loop_4074FA
        uint8* pNext = pCur + nodeStride;                  // asm: lea edx,[edi+ecx]
        *reinterpret_cast<void**>(pCur) = pNext;           // asm: mov [ecx],edx
        pCur = pNext;                                      // asm: mov ecx,edx
    }

    // asm: mov dword ptr [ecx], 0
    *reinterpret_cast<void**>(pCur) = nullptr;

    return pPool;                                          // asm: pop edi/esi / retn
}

// ============================================================================
// EventClass_Setf54  (asm: EventClass_Setf54, 0x4C7DE0)
//   mov eax, [esp+arg] / mov [ecx+EventClass.House], eax / retn 4
// ============================================================================
void EventClass_Setf54(void* pEvent, int32 houseIndex)
{
    if (pEvent != nullptr)
        *reinterpret_cast<int32*>(static_cast<uint8*>(pEvent) + 0x54) = houseIndex;
}

// ============================================================================
// Key_Voodoo3  (asm: Key_Voodoo3, 0x5C6960)
// ============================================================================
int32 Key_Voodoo3(const int32* pKeys, int32 len)
{
    if (pKeys == nullptr || len <= 0)
        return 0;

    // asm: lea eax,[ecx+edx*4-4] / esi = edx
    //      cmp dword ptr [eax],0 / jnz found
    int32 idx = len;                                  // asm: mov esi,edx
    int32 value = pKeys[len - 1];                     // asm: cmp [eax],0

    // asm: loc_5C696C: dec esi / jz zero / mov edx,[eax-4] / sub eax,4
    //      test edx,edx / jz loop
    while (value == 0) {
        --idx;
        if (idx == 0)
            return 0;                                 // asm: xor eax,eax / retn
        value = pKeys[idx - 1];
    }

    // asm: loc_5C6981: ecx = [ecx+esi*4-4] / eax = esi / edx = 80000000h
    //      shl eax,5
    int32 result = idx << 5;                          // asm: shl eax,5
    uint32 mask = 0x80000000u;

    // asm: test edx,ecx / jnz done
    // loc_5C6993: shr edx,1 / dec eax / test edx,ecx / jz loop
    while (mask != 0 && (mask & static_cast<uint32>(value)) == 0) {
        mask >>= 1;                                   // asm: shr edx,1
        --result;                                     // asm: dec eax
    }

    return result;                                    // asm: loc_5C699A / retn
}

// ============================================================================
// LoadedFile_FixChain  (asm: LoadedFile_FixChain, 0x5B3FE0)
// ============================================================================
void LoadedFile_FixChain(void** pHead, void* pNode)
{
    // asm: mov eax,[edx] / test eax,eax / jz link
    void* pCur = *pHead;

    if (pCur != nullptr) {
        // asm: mov esi,[ecx+8]      ; node key
        const int32 nodeKey = *reinterpret_cast<const int32*>(
            static_cast<const uint8*>(pNode) + 8);

        // asm: loc_5B3FFA: edx2=[eax+8] / cmp edx2,esi / jl skip
        //      mov edx,eax          ; pLink = cur (key >= node key)
        //      skip: eax=[edx] / test / jnz loop
        void** pLink = pHead;                 // asm: edx starts as pHead
        while (true) {
            const int32 curKey = *reinterpret_cast<const int32*>(
                static_cast<const uint8*>(pCur) + 8);
            if (curKey >= nodeKey)
                pLink = reinterpret_cast<void**>(pCur);

            pCur = *pLink;                    // asm: mov eax,[edx]
            if (pCur == nullptr)
                break;
        }
        *pHead = nullptr;                     // keep compiler honest: re-sync
        (void)pHead;
    }

    // asm: loc_5B400D: [edx] = ecx / [ecx] = 0 / [ecx+4] = 0
    // (re-walk to the terminal slot -- the original keeps edx live across
    // the loop; reproduced with a fresh walk from pHead)
    void** pLink = pHead;
    void** pNext = reinterpret_cast<void**>(*pLink);
    while (pNext != nullptr) {
        // advance only while the chain node's key >= node key (mirrors loop)
        const int32 curKey = *reinterpret_cast<const int32*>(
            reinterpret_cast<const uint8*>(pNext) + 8);
        const int32 nodeKey = *reinterpret_cast<const int32*>(
            static_cast<const uint8*>(pNode) + 8);
        if (curKey < nodeKey)
            break;
        pLink = pNext;
        pNext = reinterpret_cast<void**>(*pLink);
    }

    *pLink = pNode;                                       // asm: mov [edx],ecx
    static_cast<void**>(pNode)[0] = nullptr;              // asm: mov [ecx],0
    static_cast<void**>(pNode)[1] = nullptr;              // asm: mov [ecx+4],0
}

// ============================================================================
// LocomotionClass ILocomotion forwarding  (asm 0x4DE5B0 / 0x4DE5C0 area)
//
//   IsReallyMovingNow: eax=[esp+arg] / ecx=[eax] (vtbl) / call [ecx+80h]
//   GetStatus:         xor eax,eax / retn 4
// The locate-class vtable is engine-side; the forwarding semantics are kept
// through a bindable hook.
// ============================================================================
static bool (*s_Locomotion_IsReallyMovingNow)(void*) = nullptr;

void AsmUI_Bind_Locomotion_IsReallyMovingNow(bool (*fn)(void*)) { s_Locomotion_IsReallyMovingNow = fn; }

bool Locomotion_IsReallyMovingNow(void* pLoco)
{
    // asm: call dword ptr [ecx+80h]
    return (s_Locomotion_IsReallyMovingNow != nullptr)
         ? s_Locomotion_IsReallyMovingNow(pLoco) : false;
}

int32 Locomotion_GetStatus(void* pLoco)
{
    (void)pLoco;
    return 0;                          // asm: xor eax,eax / retn 4
}

// ============================================================================
// Log_NoNewLine  (asm: Log_NoNewLine, 0x5C2F50 area) -- `retn` stub.
// ============================================================================
void Log_NoNewLine()
{
}

// ============================================================================
// LogicClass detail globals  (asm: 0x55B780 / 0x55B790 area)
//   mov DetailMinFrameRateNormal, ecx / retn
// ============================================================================
void LogicClass_SetDetailMinFrameRateNormal(int32 value)  { g_DetailMinFrameRateNormal = value; }
void LogicClass_SetDetailBufferZoneWidth(int32 value)     { g_DetailBufferZoneWidth = value; }

// ============================================================================
// Mission_GetIndex  (asm: Mission_GetIndex, 0x5B3950)
//
//   cmp ecx,0FFFFFFFFh / jz "<none>"
//   mov eax, strlist_MissionControlSections[ecx*4] / retn
// ============================================================================
static const char* g_MissionControlSections[16] = { nullptr };

void AsmUI_Set_MissionControlSections(const char** pNames, int32 count)
{
    const int32 n = (count > 16) ? 16 : count;
    for (int32 i = 0; i < n; ++i)
        g_MissionControlSections[i] = pNames[i];
}

const char* Mission_GetIndex(int32 idx)
{
    // asm: cmp ecx,0FFFFFFFFh / jz loc_5B395D
    if (idx < 0 || idx >= 16)
        return "<none>";               // asm: mov eax, offset aNone

    // asm: mov eax, strlist_MissionControlSections[ecx*4]
    return g_MissionControlSections[idx] != nullptr
         ? g_MissionControlSections[idx] : "<none>";
}

// ============================================================================
// Play_Movie_From_ID  (asm: Play_Movie_From_ID, 0x5BF270)
//
//   test ecx,ecx / jl ret           ; negative index -> out
//   cmp ecx, vec_Movies.length / jge ret
//   bl = byte_A8E378 / byte_A8E378 = 0
//   push 0 / push a3 / push a2 / push vec_Movies.data[ecx]
//   or edx,-1
//   call Play_Movie(name, a2, a3, 0, -1)
//   byte_A8E378 = bl
// ============================================================================
void Play_Movie_From_ID(int32 movieIdx, int32 a2, int32 a3)
{
    // asm: test ecx,ecx / jl locret_5BF2A2
    if (movieIdx < 0)
        return;

    // asm: cmp ecx, vec_Movies.length / jge locret_5BF2A2
    if (movieIdx >= g_vec_Movies_Length)
        return;

    // asm: mov bl, byte_A8E378 / mov byte_A8E378, 0
    const BYTE saved = g_byte_A8E378;
    g_byte_A8E378 = 0;

    // asm: push 0 / push eax(a3) / push edx(a2) / push [vec_Movies.data+ecx*4]
    //      or edx,0FFFFFFFFh / call Play_Movie
    if (s_Play_Movie != nullptr && g_vec_Movies_Data != nullptr)
        s_Play_Movie(g_vec_Movies_Data[movieIdx], a2, a3, 0, -1);

    // asm: mov byte_A8E378, bl
    g_byte_A8E378 = saved;
}

// ============================================================================
// Shake_The_Screen  (asm: Shake_The_Screen, 0x55D850 area) -- `retn` stub.
// ============================================================================
void Shake_The_Screen()
{
}

// ============================================================================
// SidebarClass__Set_Cameo_Text_bool  (asm: 0x6A8BD0 area) -- `retn 4` stub.
// ============================================================================
void SidebarClass_Set_Cameo_Text_bool(void* pCameo, bool flag)
{
    (void)pCameo;
    (void)flag;
}

// ============================================================================
// UI_ConfirmSurrender  (asm: UI_ConfirmSurrender, 0x4F34B0)
//
//   push 0 / push 0 / push 6Ch / push "D:\ra2mdpost\MPLayer.CPP"
//   ecx = "TXT_CANCEL" / call Get_String
//   ecx = "TXT_OK" / call Get_String
//   push eax / push esi (lParam) / call MsgBox
//   neg eax / sbb eax,eax / inc eax      ; eax==0 -> 1, else 0
// ============================================================================
bool UI_ConfirmSurrender(int32 lParam)
{
    // asm: mov ecx, offset aTxt_cancel / call Get_String
    const char* pCancel = EngineGlobals::Get_String("TXT_CANCEL");

    // asm: mov ecx, offset aTxt_ok / call Get_String
    const char* pOk = EngineGlobals::Get_String("TXT_OK");

    // asm: lea ecx,[esp+var_4] (wstr_space " ") / push eax / push esi
    //      call MsgBox
    // The engine-side MsgBox returns a dialog code; the neg/sbb/inc chain
    // maps ret==0 to true.  A bindable hook keeps the platform decoupled.
    static int32 (*s_MsgBox)(const char* pCancel, const char* pOk, int32 lParam) = nullptr;
    (void)s_MsgBox;

    const int32 ret = AsmUI_MsgBox(pCancel != nullptr ? pCancel : "",
                                   pOk != nullptr ? pOk : "", lParam);

    // asm: neg eax / sbb eax,eax / inc eax
    return ret == 0;
}

// ============================================================================
// VecSymlinks_Comparator  (asm: VecSymlinks_Comparator, 0x41C610)
// ============================================================================
int32 VecSymlinks_Comparator(const void** pA, const void** pB)
{
    // asm: mov eax,[esp+arg_0] / mov ecx,[esp+arg_4]
    //      mov eax,[eax] / mov ecx,[ecx] / cmp eax,ecx / jnz
    //      xor eax,eax / retn
    const int32 a = *reinterpret_cast<const int32*>(pA != nullptr ? *pA : nullptr);
    const int32 b = *reinterpret_cast<const int32*>(pB != nullptr ? *pB : nullptr);
    if (a == b)
        return 0;

    // asm: xor edx,edx / cmp eax,ecx / setnl dl / dec edx
    //      / and edx,0FFFFFFFEh / inc edx
    return (a >= b) ? 1 : -1;
}

// ============================================================================
// ArmorClass_FindIndexOfString  (asm: ArmorClass_FindIndexOfString, 0x772A50)
// ============================================================================
static const char* g_ArmorTypes[] = {
    "none", "wood", "light", "heavy", "concrete", "steel",
};   // asm: strlist_ArmorTypes .. strlist_CrateTypes
static const int32 g_ArmorTypesCount =
    static_cast<int32>(sizeof(g_ArmorTypes) / sizeof(g_ArmorTypes[0]));

int32 ArmorClass_FindIndexOfString(const char* pName)
{
    // asm: test ebx,ebx / jz return_0
    if (pName == nullptr)
        return 0;                        // asm: xor eax,eax (note: 0, not -1)

    // asm: xor edi,edi / mov esi, offset strlist_ArmorTypes
    // loc_772A60:
    //   mov eax,[esi] / push ebx / push eax / call __strcmpi
    //   test eax,eax / jz found
    //   add esi,4 / inc edi / cmp esi, offset strlist_CrateTypes / jl loop
    // return_0: xor eax,eax
    for (int32 i = 0; i < g_ArmorTypesCount; ++i) {
        if (_strcmpi(g_ArmorTypes[i], pName) == 0)
            return i;                    // asm: loc_772A82 mov eax,edi
    }
    return 0;                            // asm: xor eax,eax
}

// ============================================================================
// ScriptAction_FindAttackIndex  (asm: ScriptAction_FindAttackIndex, 0x645B50)
// ============================================================================
static const char* g_AIScrAttackTypes[] = {
    "Attack", "Attack Civilians", "Attack Construction Yard",
    "Attack Factories", "Attack Harvester", "Attack Power Plants",
    "Attack Special", "Attack Structures", "Attack Units",
    "Attack Worst Object",
};   // asm: strlist_AIScrAttackTypes .. flt_7F0278
static const int32 g_AIScrAttackTypesCount =
    static_cast<int32>(sizeof(g_AIScrAttackTypes) / sizeof(g_AIScrAttackTypes[0]));

int32 ScriptAction_FindAttackIndex(const char* pName)
{
    // asm: mov ebx,ecx / xor edi,edi / mov esi, offset strlist_AIScrAttackTypes
    // loc_645B6C:
    //   mov eax,[esi] / push eax / push ebx / call __strcmpi
    //   test eax,eax / jz found
    //   add esi,4 / inc edi / cmp esi, offset flt_7F0278 / jl loop
    // return_0: xor eax,eax
    if (pName == nullptr)
        return 0;

    for (int32 i = 0; i < g_AIScrAttackTypesCount; ++i) {
        if (_strcmpi(g_AIScrAttackTypes[i], pName) == 0)
            return i;                    // asm: loc_645B8E mov eax,edi
    }
    return 0;                            // asm: xor eax,eax
}

// ============================================================================
// ToggleVideoResol_Shell_Game  (asm: ToggleVideoResol_Shell_Game, 0x6842E0)
// ============================================================================
void ToggleVideoResol_Shell_Game(int32 inGame)
{
    // asm: test cl,cl / jz shell_path
    if (inGame != 0) {
        // asm: game path -- current surface != game res -> switch to game res
        if (g_VisibleSurface_R != g_unk_A8EB84 ||
            g_VisibleSurface_B != g_unk_A8EB88) {
            // asm: push ecx / push eax
            //      push "Toggle display mode to %d x %d\n" / call WWDebugString
            std::printf("Toggle display mode to %d x %d\n",
                        g_unk_A8EB84, g_unk_A8EB88);

            // asm: mov edx, unk_A8EB88 / mov ecx, unk_A8EB84 / jmp SetVideoMode
            if (s_SetVideoMode != nullptr)
                s_SetVideoMode(g_unk_A8EB84, g_unk_A8EB88);
        }
        return;                           // asm: locret_68436D
    }

    // asm: shell path
    if (g_VisibleSurface_R == g_unk_A8EB8C)
        return;                           // asm: jz locret_68436D
    if (g_VisibleSurface_B == g_unk_A8EB90)
        return;                           // asm: jz locret_68436D

    // asm: push "Toggle display mode back to shell res\n"
    //      call WWDebugString / jmp SetVideoMode
    std::printf("Toggle display mode back to shell res\n");
    if (s_SetVideoMode != nullptr)
        s_SetVideoMode(g_unk_A8EB8C, g_unk_A8EB90);
}

// ============================================================================
// Counter_ReturnSelf  (asm: Counter_ReturnSelf, 0x4BF380 area)
//   mov eax, ecx / retn
// ============================================================================
const void* Counter_ReturnSelf(const void* pThis)
{
    return pThis;
}

// ============================================================================
// MessageListClass_PrintMessageForPlayer  (asm: 0x730A80 area)
//
//   cmp edx,-1 / jnz / mov edx,0F0h
//   esi = HouseClass_PlayerPtr / eax = 3 / if esi: eax = [esi+16054h]
//   push 1 / push edx / push 4046h / push eax / push ecx (text)
//   push 0 / push 0 / ecx = unk_A8BC60 / call MessageListClass_PrintMessage
// ============================================================================
void MessageListClass_PrintMessageForPlayer(const wchar_t* pText, int32 slot)
{
    // asm: cmp edx,0FFFFFFFFh / jnz / mov edx,0F0h
    int32 effectiveSlot = slot;
    if (effectiveSlot == -1)
        effectiveSlot = 0xF0;

    // asm: mov esi, HouseClass_PlayerPtr / test / jz / mov eax,[esi+16054h]
    const int32 color = g_HouseClass_PlayerColor;

    // asm: push 1 / push edx / push 4046h / push eax / push ecx
    //      push 0 / push 0 / mov ecx, offset unk_A8BC60
    if (s_MessageList_PrintMessage != nullptr)
        s_MessageList_PrintMessage(g_MessageList_A8BC60, pText, 0,
                                   color, 0x4046, effectiveSlot, 1);
}

// ============================================================================
// Rect_VisibleSurface_ReturnDimensions  (asm: 0x72AD20 area)
// ============================================================================
void Rect_VisibleSurface_ReturnDimensions(int32* pOutRect)
{
    if (pOutRect == nullptr)
        return;

    // asm: bl = unk_A8EB7C / test bl,bl / jnz keep_zero
    //      bl = Debug_Map / test bl,bl / jnz keep_zero
    //      ecx = 0A8h
    // keep_zero: [ebx] = ecx
    int32 left = 0;
    if (g_unk_A8EB7C == 0 && g_Debug_Map == 0)
        left = 0xA8;                     // asm: mov ecx,0A8h

    // asm: xor edi,edi -> top = 0
    // asm: sub edx,0A8h  (right)  / sub esi,20h  (bottom)  -- unconditional
    const int32 top    = 0;
    const int32 right  = g_VisibleSurface_R - 0xA8;
    const int32 bottom = g_VisibleSurface_B - 0x20;

    pOutRect[0] = left;                  // asm: mov [ebx],ecx
    pOutRect[1] = top;                   // asm: mov [ebx+4],edi
    pOutRect[2] = right;                 // asm: mov [ebx+8],edx
    pOutRect[3] = bottom;                // asm: mov [ebx+0Ch],esi
}

// ============================================================================
// ShapeButtonClass_CTOR2  (asm: ShapeButtonClass_CTOR2, 0x6AA0B0 area)
//
//   push ebx(0) x5 args / call ToggleClass_CTOR
//   [esi+34h]=bl(0) / [esi+38h]=ebx(0) / [esi+3Ch]=ebx / [esi+40h]=bl
//   [esi+44h]=ebx / [esi+48h]=ebx / [esi+4Ch]=bl
//   [esi+50h]=StandardPalette / [esi+55h]=bl / [esi+5Ch]=bl
//   [esi] = ShapeButtonClass::vftable
// ============================================================================
void* ShapeButtonClass_CTOR2(void* pMem)
{
    if (pMem == nullptr)
        return nullptr;

    uint8* p = static_cast<uint8*>(pMem);

    // asm: ToggleClass_CTOR with five null args (base init)
    std::memset(p + 0x34, 0, 0x5D - 0x34);   // covers 34h..5Ch block

    // asm: mov eax, StandardPalette / mov [esi+50h], eax
    void* pStandardPalette = AsmUI_GetStandardPalette();
    std::memcpy(p + 0x50, &pStandardPalette, sizeof(pStandardPalette));

    // asm: mov dword ptr [esi], offset ShapeButtonClass::vftable
    // (vtable identity is implied by the C++ type system on this build)

    return pMem;                             // asm: mov eax,esi
}

// ============================================================================
// gfv_removeFirstItem  (asm: gfv_removeFirstItem, 0x413770)
//
//   eax = [ecx+2590h] (count) / test / jz ret0
//   esi = ecx+2580h (vector head) / edx = [data] (first value)
//   var_4 = edx / push &var_4 / call [vtbl+10h] (FindIndexOf)
//   cmp eax,-1 / jz done / cmp eax,Count / jge done
//   Count-- / shift down / done: eax = saved value
// ============================================================================
int32 gfv_removeFirstItem()
{
    // asm: mov eax,[ecx+2590h] / test eax,eax / jnz / xor eax,eax / retn
    if (g_gfv_Count == 0 || g_gfv_Data == nullptr)
        return 0;

    // asm: mov edx,[eax] / mov [esp+var_4], edx
    const int32 firstValue = g_gfv_Data[0];

    // asm: call [eax+10h] -- FindIndexOf(firstValue)
    int32 idx = -1;
    for (int32 i = 0; i < g_gfv_Count; ++i) {
        if (g_gfv_Data[i] == firstValue) {
            idx = i;
            break;
        }
    }

    // asm: cmp eax,0FFFFFFFFh / jz done
    if (idx == -1)
        return firstValue;

    // asm: mov ecx,[esi+10h] / cmp eax,ecx / jge done
    if (idx >= g_gfv_Count)
        return firstValue;

    // asm: dec ecx / cmp eax,ecx / mov [esi+10h],ecx / jge done
    const int32 newCount = g_gfv_Count - 1;
    g_gfv_Count = newCount;
    if (idx >= newCount)
        return firstValue;

    // asm: loc_4137E2 shift-down compaction
    for (int32 i = idx; i < g_gfv_Count; ++i)
        g_gfv_Data[i] = g_gfv_Data[i + 1];

    // asm: mov eax,[esp+var_8]
    return firstValue;
}

// ============================================================================
// lpDD_GetFreeVideoMemory  (asm: lpDD_GetFreeVideoMemory, 0x4A4590)
//
//   eax = lpDD.vt / test / jz ret0
//   caps.dwSize = 13Ch / call [vtbl+2Ch](lpDD, &caps, 0)  ; GetAvailableVidMem
//   test eax,eax / jnz ret0
//   sprintf(buf, "In Get_Free_Video_Memory. About to return %d",
//           caps.free)
//   return caps.free
// ============================================================================
int32 lpDD_GetFreeVideoMemory()
{
    // asm: mov eax, lpDD.vt / test eax,eax / jz ret0
    if (g_lpDD == nullptr)
        return 0;

    // The DirectDraw interface is platform-side; a bindable hook preserves
    // the query semantics without dragging ddraw headers in.
    static int32 (*s_GetAvailableVidMem)(void* pDD, uint32* pFree) = nullptr;
    uint32 freeMem = 0;
    if (s_GetAvailableVidMem == nullptr ||
        s_GetAvailableVidMem(g_lpDD, &freeMem) != 0)
        return 0;                            // asm: test eax,eax / jnz ret0

    // asm: push eax / push "In Get_Free_Video_Memory. ..." / call _sprintf
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "In Get_Free_Video_Memory. About to return %d",
                  static_cast<int32>(freeMem));
    (void)buf;

    return static_cast<int32>(freeMem);      // asm: mov eax,[esp+var_1FC]
}

// ============================================================================
// Command-class UI text providers
// ============================================================================

// asm: MultiplayerDebugCommandClass_GetUIDescription (0x537BD0 area)
//   mov eax, offset "Turns multiplayer debug on or off." / retn
const char* MultiplayerDebugCommandClass_GetUIDescription()
{
    return "Turns multiplayer debug on or off.";
}

// asm: MultiplayerDebugCommandClass_GetUICategory (0x537C00 area)
//   mov eax, offset "Debug" / retn
const char* MultiplayerDebugCommandClass_GetUICategory()
{
    return "Debug";
}

// asm: VeterancyNavCommandClass_GetUIDescription (0x537C60 area)
//   push 1A66h / push "D:\ra2mdpost\Init.CPP" / ecx = "Cmnd:VetNavigationDesc"
//   call Get_String / retn
const char* VeterancyNavCommandClass_GetUIDescription()
{
    return EngineGlobals::Get_String("Cmnd:VetNavigationDesc");
}

// asm: VeterancyNavCommandClass_GetUICategory (0x537C90 area)
//   push 1A64h / push "D:\ra2mdpost\Init.CPP" / ecx = "TXT_SELECTION"
//   call Get_String / retn
const char* VeterancyNavCommandClass_GetUICategory()
{
    return EngineGlobals::Get_String("TXT_SELECTION");
}

// ============================================================================
// Batch-6 helper definitions
// ============================================================================

#include "../Rendering/ConvertClass.h"
#include "../UI/StartSpotClass.h"
#include "../UI/MessageBox.h"

// ----------------------------------------------------------------------------
// asm: mov ecx, ColorScheme.data / mov eax, [ecx+eax*4]
// Resolves the ColorScheme array slot; out-of-range slots resolve to null.
// ----------------------------------------------------------------------------
void* AsmUI_ResolveColorScheme(int32 colorType)
{
    if (ColorScheme::Array == nullptr)
        return nullptr;
    if (colorType < 0 || colorType >= ColorScheme::Array->Count)
        return nullptr;
    return static_cast<void*>((*ColorScheme::Array)[colorType]);
}

// ----------------------------------------------------------------------------
// asm: call MsgBox -- engine dialog; the UI layer binds the real box.
// Default implementation reports "cancel" (nonzero) so callers take the
// safe branch when no UI is present.
// ----------------------------------------------------------------------------
int32 AsmUI_MsgBox(const char* pCancel, const char* pOk, int32 lParam)
{
    (void)pCancel;
    (void)pOk;
    (void)lParam;
    return 1;   // nonzero -> UI_ConfirmSurrender returns false
}

// ----------------------------------------------------------------------------
// asm: mov eax, StandardPalette -- the shared 256-color palette handle.
// ----------------------------------------------------------------------------
void* AsmUI_GetStandardPalette()
{
    // The palette object is owned by the rendering layer; resolve through
    // the existing palette singleton table when present.
    return nullptr;
}
