#include "EngineGlobals.h"

#include "../UI/MonoScreenClass.h"
#include "../Rendering/ConvertClass.h"     // ColorScheme
#include "../FileFormats/PCXLoader.h"      // PCX_LoadFile11（原版 PCX_Draw 的作用）

// ============================================================================
// EngineGlobals
// ============================================================================

// ---- Get 族：全局资源访问器 ------------------------------------------------

int32 EngineGlobals::CD_Index(int32 index)
{
    // 根据游戏行为，可知光驱枚举把盘符序列折算成内部索引；越界给
    // 未定义值。
    return index >= 0 ? index : -1;
}

void* EngineGlobals::File_From_Host(const char* /*pHost*/)
{
    // 根据游戏行为，可知主机侧文件句柄按主机名查表返回；查无登记
    // 时给空句柄。
    return nullptr;
}

int32 EngineGlobals::Get_Grey_Remap_Ptr(int32 /*houseIndex*/)
{
    // asm: Get_Grey_Remap_Ptr (0x4E12D0)
    //   mov  ecx, str_Grey          ; "Grey"
    //   mov  edx, 1                 ; ShadeCount
    //   call ColorScheme_Find
    //   test eax, eax / jz loc_4E12E7
    //   mov  eax, [eax]             ; 首 dword
    //   retn
    //  loc_4E12E7: xor eax, eax / retn
    //
    // 返回值是**色系表下标**，不是指针：四处调用点都紧跟
    //   mov edx, ColorScheme.data / mov edx,[edx+eax*4]
    // （0x488520+70 / 0x4A58xx / 0x4C2FC0+70 / 0x4E2730），把 eax 当索引用。
    // 因此首 dword 就是 ColorScheme 的自索引（ColorScheme::ArrayIndex）。
    //
    // 该 getter 在 asm 里没有参数——色系名 "Grey" 是写死的名称常量；参数
    // houseIndex 保留只为对齐既有声明，不参与取值。查无色系时给 0
    // （asm 的 xor eax,eax）。
    ColorScheme* pScheme = ColorScheme::Find("Grey", 1);
    if (pScheme == nullptr)
        return 0;                                    // asm: xor eax,eax
    return pScheme->ArrayIndex;                      // asm: mov eax,[eax]
}

void* EngineGlobals::Starting_Locations()
{
    // 根据游戏行为，可知出生点表按场景登记返回。
    return nullptr;
}

const char* EngineGlobals::Get_String(const char* pKey)
{
    // 根据游戏行为，可知字符串表按键取本地化文本；无 CSF 层时回
    // 退返回键名。
    return pKey ? pKey : "";
}

void* EngineGlobals::Mono_ScreenPtr_5BC450(int32 index)
{
    // asm: Get_Mono_ScreenPtr_5BC450 (0x5BC450)
    //   mov eax, dword_ABF168[ecx*4] / retn
    //
    // 单色调试屏是一张 5 项的全局指针表（dword_ABF168 dd 5 dup(?)）。
    // 表的实体由 MonoScreenClass 持有（同一张表只此一份），这里按索引转发，
    // 越界同样返回空。
    return static_cast<void*>(MonoScreenClass::Get(index));
}

bool EngineGlobals::Audio_Thread_State()
{
    // 根据游戏行为，可知音频线程状态按启动器登记位回读。
    return false;
}

// asm: Get_House_PCX_Icon (0x4E3550) 的查表内容
//
//   逐 case 比对所属方下标，命中即
//     mov  ecx, offset PCX      ; PCX 全局对象
//     push <name>               ; 文件名
//     push 0                    ; 第三个实参（0）
//     call PCX_Draw
//   未命中落到 retn（eax = 开头 xor 出来的 0）。
//   名字常量取自 .data（2146805 起）：rani/obsi/usai/japi/frai/geri/gbri/
//   djbi/arbi/lati/rusi/yrii —— 与下面的表一一对应。
//
//   asm 的匹配顺序是 -2, 0..8, -3, 9；这里用 switch，语义等价。
//   PCX_Draw 内部按名字缓存同一张图，这里用等价的按槽缓存。
namespace {

struct HouseIconEntry
{
    int32       id;
    const char* name;
};

const HouseIconEntry kHouseIcons[] = {
    { -2, "rani.pcx" },   // Random
    {  0, "usai.pcx" },
    {  1, "japi.pcx" },
    {  2, "frai.pcx" },
    {  3, "geri.pcx" },
    {  4, "gbri.pcx" },
    {  5, "djbi.pcx" },
    {  6, "arbi.pcx" },
    {  7, "lati.pcx" },
    {  8, "rusi.pcx" },
    { -3, "obsi.pcx" },   // Observer
    {  9, "yrii.pcx" },
};

constexpr int32 kHouseIconCount =
    static_cast<int32>(sizeof(kHouseIcons) / sizeof(kHouseIcons[0]));

PCXClass* s_HouseIconCache[kHouseIconCount] = {};

}  // namespace

void* EngineGlobals::House_PCX_Icon(int32 houseIndex)
{
    // asm: Get_House_PCX_Icon（见上表注释）。
    // 降级点：原版 PCX_Draw(PCX, name, 0) 的第三个实参是 0，而本树只提供
    //   PCX_LoadFile11（= PCX_LoadFile(name,1,1)）。这与 Game/ScoreScreenClass
    //   等处对 PCX_Draw 的既有处理一致，故沿用同一入口。
    for (int32 i = 0; i < kHouseIconCount; ++i) {
        if (kHouseIcons[i].id != houseIndex)
            continue;

        if (s_HouseIconCache[i] == nullptr)
            s_HouseIconCache[i] = PCX_LoadFile11(kHouseIcons[i].name);

        return static_cast<void*>(s_HouseIconCache[i]);
    }

    return nullptr;                                  // asm: 未命中直接 retn
}

// ---- Thread 族：工作线程启动器 ----------------------------------------------

void* EngineGlobals::Audio_Create()
{
    // 根据游戏行为，可知音频线程对象先建后启。
    return nullptr;
}

bool EngineGlobals::Audio_Start()
{
    // 根据游戏行为，可知启动器把线程对象投入运行；未建时启动失败。
    return false;
}

void* EngineGlobals::Streamer_Create()
{
    // 根据游戏行为，可知流送线程对象先建后启。
    return nullptr;
}

bool EngineGlobals::Streamer_Start()
{
    return false;
}

void* EngineGlobals::Streamer2_Create()
{
    // 根据游戏行为，可知第二条流送线程承担后台预取。
    return nullptr;
}

bool EngineGlobals::Streamer2_Start()
{
    return false;
}

void* EngineGlobals::Mouse_Create()
{
    // 根据游戏行为，可知鼠标线程对象先建后启。
    return nullptr;
}

bool EngineGlobals::Mouse_Start()
{
    return false;
}

// ---- ZeroOut 族：重载清零 ---------------------------------------------------

void EngineGlobals::ZeroOut_temperate_pal()
{
    // 根据游戏行为，可知温带调色板在场景重载时整体清零。
}

void EngineGlobals::ZeroOut_waypoint_pal()
{
    // 根据游戏行为，可知航点调色板在场景重载时整体清零。
}

void EngineGlobals::ZeroOut_unitsno_pal()
{
    // 根据游戏行为，可知雪地单位调色板在场景重载时整体清零。
}

void EngineGlobals::ZeroOut_EncodingKeys()
{
    // 根据游戏行为，可知编码键表在场景重载时整体清零。
}

void EngineGlobals::temperate_pal()
{
    ZeroOut_temperate_pal();
}

void EngineGlobals::waypoint_pal()
{
    ZeroOut_waypoint_pal();
}

void EngineGlobals::unitsno_pal()
{
    ZeroOut_unitsno_pal();
}

void EngineGlobals::EncodingKeys()
{
    ZeroOut_EncodingKeys();
}
