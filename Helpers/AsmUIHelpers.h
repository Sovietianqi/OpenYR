#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// Helpers/AsmUIHelpers.h - UI / platform / session helpers recovered from the
// binary (proc names kept as the IDA labels they came from).
// ============================================================================

// asm: Draw_AdjustForBuildingZ (0x6D20D0)
// AircraftClass_Draw Z quantizer: returns floor(z*dbl_B0CD48 +
// (z >= 0x2D8 ? 1 : 0) + 0.5) -- a 0/1 "above the building plane" selector.
int32 Draw_AdjustForBuildingZ(int32 z);

// asm: Console_ReadChar (0x4A39E0)
// Blocking single-character read used by the exception dialog.
int32 Console_ReadChar();

// asm: Fetch_String (0x4A3B80) -- resource string fetch; returns null here.
const char* Fetch_String(int32 id);

// asm: Fetch_Resource (0x4A3B30) -- FindResourceA/LoadResource/LockResource
// chain against the module handle; returns the locked resource or null.
void* Fetch_Resource(int32 type, int32 name);

// asm: GetEventFlags__ (0x71F9B0)
// TriggerClass event-predicate table: events 0,5,30,43,58 (and everything
// outside 1..59) return false; all other triggers return true.
bool GetEventFlags_(int32 eventId);

// asm: IDX_Container_Unlink (0x401BD0)
// Detaches a doubly-linked container node: both link slots point at self
// and the payload slot is zeroed.
void IDX_Container_Unlink(void* pNode);

// asm: INISubClass_ZeroOut (0x49E8F0)
// Clears the 0x11-byte INI sub-record (three dwords, a byte, a dword).
void INISubClass_ZeroOut(BYTE* pRecord);

// asm: GameMode_SaveNthSlotIntoINI (0x67E0F0)
// Writes "%d,%d,%d" of the three values into section/key via INIClass_WriteString.
void GameMode_SaveNthSlotIntoINI(class CCINIClass* pINI, const char* pSection,
                                 const char* pKey, int32 a, int32 b, int32 c);

// asm: Get_Mono_ScreenPtr (0x5BC450)
// Returns the mono-screen base for index idx from the global table.
void* Get_Mono_ScreenPtr(int32 idx);

// asm: Get_Grey_Remap_Ptr (0x4E12D0)
// Resolves the "Grey" color scheme and returns its first dword (the remap
// table pointer); null when the scheme is absent.
void* Get_Grey_Remap_Ptr();

// asm: Font_Palette (0x5C2420)
// Builds the 4-entry single-tone font palette from an 8-bit color index and
// returns a pointer to the 12-byte palette block.
void* Font_Palette(BYTE colorIdx);

// asm: Focus_Loss (0x7773D0)
// Application focus-lost handler: pauses the movie state, releases the mouse
// capture when a WWMouse exists, and reports the capture transition.
void Focus_Loss();

// ============================================================================
// Batch-6 additions -- original-layout UI/session/global helpers
// ============================================================================

// asm: Drawing_GetColorOfPlayer (0x642BA0)
// Resolves the color scheme entry for a player slot:
//   GameMode 3/4 and NameNodeTypes[idx]->Country == -2 -> color type of the
//   session's own slot (-2); otherwise slot idx within 0..8 resolves that
//   entry's starting-spot colour and then the colour type.  Returns
//   ColorScheme.data[colorType] (the scheme entry pointer).
void* Drawing_GetColorOfPlayer(int32 idx);

// asm: GScreen_FlagToRedraw (0x4F42E0)
// Marks the tactical map (+0xD7D = 1) and, when a2 is nonzero and the
// screen's redraw mode (+0xC) is not already 2, promotes it and calls the
// map refresh (sub_578AC0).
void GScreen_FlagToRedraw(class GScreenClass* pScreen, int32 a2);

// asm: GraphicMenuAnimItem_Update_Controls (0x4F2F80)
// Hit-test for a menu animation item: item enabled ([ecx+9]), then
// x in [+10h, +10h+[+18h]) and y in [+14h, +14h+[+1Ch]).
bool GraphicMenuAnimItem_Update_Controls(const int32* pItem, const int32* pPos);

// asm: GraphicMenuItem_On_Rollover (0x4F30E0) / Frame_Update (0x4F30F0)
// Both are vtable no-ops with stdcall 4-byte cleanup.
void GraphicMenuItem_On_Rollover(void* pItem);
void GraphicMenuItem_Frame_Update(void* pItem);

// asm: Event_Mempool_Allocate (0x4074C0)
// Chained fixed-stride pool: allocates (stride+8)*count+8 bytes, links the
// nodes head-to-tail with a null terminator, and returns the pool header.
void* Event_Mempool_Allocate(int32 count, int32 stride);

// asm: EventClass_Setf54 (0x4C7DE0)
// EventClass.House (asm +0x54) write accessor.
void EventClass_Setf54(void* pEvent, int32 houseIndex);

// asm: Key_Voodoo3 (0x5C6960)
// Scans back over trailing zero dwords of a key table; then bit-scans the
// last nonzero dword from the top (0x80000000 downward).  Returns
// (foundIndex << 5) - bit, or 0 when the table is all zeros.
int32 Key_Voodoo3(const int32* pKeys, int32 len);

// asm: LoadedFile_FixChain (0x5B3FE0)
// Sorted-chain insert for the LoadedFile list: walks [edx] descending on
// [node+8] vs [ecx+8] keeping the last node with a greater key, then links
// ecx in and clears its forward/back slots.
void LoadedFile_FixChain(void** pHead, void* pNode);

// asm: LocomotionClass_ILocomotion_IsReallyMovingNow (0x4DE5B0 area)
// Forwards to the locate-class vtable slot +80h.
bool Locomotion_IsReallyMovingNow(void* pLoco);

// asm: LocomotionClass_ILocomotion_GetStatus (0x4DE5C0 area)
// Constant zero with stdcall cleanup.
int32 Locomotion_GetStatus(void* pLoco);

// asm: Log_NoNewLine (0x5C2F50 area) -- empty stub.
void Log_NoNewLine();

// asm: LogicClass_SetDetailMinFrameRateNormal (0x55B780 area)
// Stores ecx into the DetailMinFrameRateNormal global.
void LogicClass_SetDetailMinFrameRateNormal(int32 value);

// asm: LogicClass_SetDetailBufferZoneWidth (0x55B790 area)
void LogicClass_SetDetailBufferZoneWidth(int32 value);

// asm: Mission_GetIndex (0x5B3950)
// strlist_MissionControlSections[ecx] or "<none>" for -1.
const char* Mission_GetIndex(int32 idx);

// asm: Play_Movie_From_ID (0x5BF270)
// Bounds-checks ecx against vec_Movies, saves byte_A8E378, clears it, and
// calls Play_Movie(movieName, a2, a3, 0, -1); restores the byte after.
void Play_Movie_From_ID(int32 movieIdx, int32 a2, int32 a3);

// asm: Shake_The_Screen (0x55D850 area) -- empty stub in this build.
void Shake_The_Screen();

// asm: SidebarClass__Set_Cameo_Text_bool (0x6A8BD0 area) -- retn 4 stub.
void SidebarClass_Set_Cameo_Text_bool(void* pCameo, bool flag);

// asm: UI_ConfirmSurrender (0x4F34B0)
// MsgBox(TXT_CANCEL / TXT_OK strings, lParam) -> returns (ret != 0).
bool UI_ConfirmSurrender(int32 lParam);

// asm: VecSymlinks_Comparator (0x41C610)
// Compares two pointer entries' first dword; returns -1/0/1 with the
// original's sign materialization (setnl / dec / and / inc).
int32 VecSymlinks_Comparator(const void** pA, const void** pB);

// asm: ArmorClass_FindIndexOfString (0x772A50)
// Walks strlist_ArmorTypes (up to strlist_CrateTypes) with _strcmpi;
// returns the index or 0 when absent (note: original returns eax=0, not -1).
int32 ArmorClass_FindIndexOfString(const char* pName);

// asm: ScriptAction_FindAttackIndex (0x645B50)
// Walks strlist_AIScrAttackTypes (up to flt_7F0278) with _strcmpi; returns
// the index or 0.
int32 ScriptAction_FindAttackIndex(const char* pName);

// asm: ToggleVideoResol_Shell_Game (0x684310 area)
// Switches between the in-game resolution and the shell resolution via
// SetVideoMode, logging the transition.
void ToggleVideoResol_Shell_Game(int32 inGame);

// asm: Counter_ReturnSelf (0x4BF380 area)
// mov eax, ecx / retn -- identity helper.
const void* Counter_ReturnSelf(const void* pThis);

// asm: MessageListClass_PrintMessageForPlayer (0x730A80 area)
// Routes a message to the local player's list: HouseClass_PlayerPtr's
// +16054h color (or 3 default), 0x4046 flags, edx slot (-1 -> 0xF0).
void MessageListClass_PrintMessageForPlayer(const wchar_t* pText, int32 slot);

// asm: Rect_VisibleSurface_ReturnDimensions (0x72AD20 area)
// Computes the drawable rect: when neither byte_A8EB7C nor Debug_Map is set
// the top/left become 0xA8/0; right -= 0xA8, bottom -= 0x20 always.
void Rect_VisibleSurface_ReturnDimensions(int32* pOutRect);

// asm: ShapeButtonClass_CTOR2 (0x6AA0B0 area)
// Constructs a shape button over ToggleClass with the standard 0x34..0x5C
// field zeroing and StandardPalette default.
void* ShapeButtonClass_CTOR2(void* pMem);

// asm: gfv_removeFirstItem (0x413770)
// Pops the first entry of the global firing vector (+2590h count, +2580h
// vector head) and returns the popped value; 0 when empty.
int32 gfv_removeFirstItem();

// asm: lpDD_GetFreeVideoMemory (0x4A45C0)
// Queries DirectDraw's GetAvailableVidMem (vtbl +2Ch) with a 0x13C-byte
// DDSCAPS2 and returns the free total; 0 when lpDD is absent or the call
// fails.  Logs via sprintf to the mono console.
int32 lpDD_GetFreeVideoMemory();

// asm: MultiplayerDebugCommandClass_GetUIDescription (0x537BD0 area)
const char* MultiplayerDebugCommandClass_GetUIDescription();

// asm: MultiplayerDebugCommandClass_GetUICategory (0x537C00 area)
const char* MultiplayerDebugCommandClass_GetUICategory();

// asm: VeterancyNavCommandClass_GetUIDescription (0x537C60 area)
// Get_String("Cmnd:VetNavigationDesc") with the Init.CPP 1A66h line tag.
const char* VeterancyNavCommandClass_GetUIDescription();

// asm: VeterancyNavCommandClass_GetUICategory (0x537C90 area)
// Get_String("TXT_SELECTION") with the Init.CPP 1A64h line tag.
const char* VeterancyNavCommandClass_GetUICategory();

// ============================================================================
// Batch-6 supporting hooks & globals
// ============================================================================

// ColorScheme.data[colorType] resolution (asm: mov ecx, ColorScheme.data /
// mov eax, [ecx+eax*4]).  Returns the scheme entry pointer as void*.
void* AsmUI_ResolveColorScheme(int32 colorType);

// MsgBox stand-in (asm: call MsgBox with TXT_CANCEL/TXT_OK strings).
int32 AsmUI_MsgBox(const char* pCancel, const char* pOk, int32 lParam);

// StandardPalette global (asm: mov eax, StandardPalette).
void* AsmUI_GetStandardPalette();

// ---- bindable engine hooks (platform layer installs these) -----------------
void AsmUI_Bind_Session_GetPlayerColorType(int32 (*fn)(int32 slot));
void AsmUI_Bind_MessageList_PrintMessage(void (*fn)(void*, const wchar_t*, int32, int32, int32, int32, int32));
void AsmUI_Bind_Play_Movie(void (*fn)(const char*, int32, int32, int32, int32));
void AsmUI_Bind_SetVideoMode(void (*fn)(int32, int32));
void AsmUI_Bind_Locomotion_IsReallyMovingNow(bool (*fn)(void*));

// ---- global state setters (mirror the .data writes the binary performs) ----
void AsmUI_Set_Session_GameMode(int32 mode);
void AsmUI_Set_VisibleSurface(int32 r, int32 b);
void AsmUI_Set_Debug_Map(BYTE on);
// asm: Debug_Map —— 读取当前调试地图开关（Fill_In_Data 用它跳过视图初始化）。
BYTE AsmUI_Get_Debug_Map();
void AsmUI_Set_Gfv(int32* pData, int32 count);
void AsmUI_Set_MovieList(void* pList);
void AsmUI_Set_Movies(char** pData, int32 length);
void AsmUI_Set_PlayerColor(int32 color);
void AsmUI_Set_lpDD(void* pDD);
void AsmUI_Set_MissionControlSections(const char** pNames, int32 count);
