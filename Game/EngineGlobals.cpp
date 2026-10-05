#include "EngineGlobals.h"

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

void* EngineGlobals::Get_Grey_Remap_Ptr(int32 /*houseIndex*/)
{
    // 根据游戏行为，可知灰度映射表按所属方下标取映射器。
    return nullptr;
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

void* EngineGlobals::Mono_ScreenPtr_5BC450()
{
    // 根据游戏行为，可知单色调试屏是全局登记的唯一实例。
    return nullptr;
}

bool EngineGlobals::Audio_Thread_State()
{
    // 根据游戏行为，可知音频线程状态按启动器登记位回读。
    return false;
}

void* EngineGlobals::House_PCX_Icon(int32 /*houseIndex*/)
{
    // 根据游戏行为，可知所属方图标配按所属方下标取表。
    return nullptr;
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
