#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// EngineGlobals - engine-wide singleton accessors, thread starters, palette
//                 resets
//
//  根据游戏行为，可知引擎把若干全局资源（光驱枚举、灰度映射、观战
//  出生点、单色屏、音频/流送/鼠标线程）以单例访问器与启动器形态
//  暴露；调色板与编码键在场景重载时整体清零。
// ============================================================================

class EngineGlobals {
public:
    // ---- Get 族：全局资源访问器 ----
    static int32 CD_Index(int32 index);
    static void* File_From_Host(const char* pHost);
    static void* Get_Grey_Remap_Ptr(int32 houseIndex);
    static void* Starting_Locations();
    static const char* Get_String(const char* pKey);
    static void* Mono_ScreenPtr_5BC450();
    static bool Audio_Thread_State();
    static void* House_PCX_Icon(int32 houseIndex);

    // ---- Thread 族：工作线程启动器 ----
    static void* Audio_Create();
    static bool Audio_Start();
    static void* Streamer_Create();
    static bool Streamer_Start();
    static void* Streamer2_Create();
    static bool Streamer2_Start();
    static void* Mouse_Create();
    static bool Mouse_Start();

    // ---- ZeroOut 族：重载清零 ----
    static void ZeroOut_temperate_pal();
    static void ZeroOut_waypoint_pal();
    static void ZeroOut_unitsno_pal();
    static void ZeroOut_EncodingKeys();
    // 根据游戏行为，可知清零入口以裸资源名暴露：调色板与编码
    // 密钥表都按各自的字节宽度整体归零。
    static void temperate_pal();
    static void waypoint_pal();
    static void unitsno_pal();
    static void EncodingKeys();
};
