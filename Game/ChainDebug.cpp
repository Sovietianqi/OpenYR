#include <Game/ChainDebug.h>

#include <cstdio>
#include <cstdlib>

// ============================================================================
// ChainDebug.cpp —— 逐帧调用链探针的实现
//
//  计数器只在对应函数入口自增一次，成本是一次内存自增（非原子，逻辑线程
//  单线程访问，无需同步）。默认不打印任何东西；排查时显式调用 Dump() /
//  CheckAllLinks()。
// ============================================================================

namespace ChainDebug
{
    int32 TechnoUpdateCalls      = 0;
    int32 FootUpdateCalls        = 0;
    int32 MissionUpdateCalls     = 0;
    int32 MissionDispatchCalls   = 0;
    int32 LocomotionProcessCalls = 0;

    void Reset()
    {
        TechnoUpdateCalls      = 0;
        FootUpdateCalls        = 0;
        MissionUpdateCalls     = 0;
        MissionDispatchCalls   = 0;
        LocomotionProcessCalls = 0;
    }

    void Dump(const char* pWhere)
    {
        std::fprintf(stderr,
            "[ChainDebug] %s\n"
            "  TechnoClass::Update      = %d\n"
            "  FootClass::Update        = %d\n"
            "  MissionClass::Update     = %d\n"
            "  MissionClass::AI_Mission = %d\n"
            "  ILocomotion::Process     = %d\n",
            (pWhere != nullptr) ? pWhere : "(no tag)",
            static_cast<int>(TechnoUpdateCalls),
            static_cast<int>(FootUpdateCalls),
            static_cast<int>(MissionUpdateCalls),
            static_cast<int>(MissionDispatchCalls),
            static_cast<int>(LocomotionProcessCalls));
        std::fflush(stderr);
    }

    bool CheckAllLinks(const char* pWhere)
    {
        const char* pTag = (pWhere != nullptr) ? pWhere : "(no tag)";
        bool ok = true;

        // #2：TechnoClass::Update 必须被上层（vec_Logics / 子类 Update）驱动到。
        if (TechnoUpdateCalls <= 0)
        {
            std::fprintf(stderr, "[ChainDebug] %s FAIL #2: TechnoClass::Update 未被调用\n", pTag);
            ok = false;
        }

        // #3：TechnoClass::Update 末尾必须落到 MissionClass::Update。
        if (MissionUpdateCalls <= 0)
        {
            std::fprintf(stderr, "[ChainDebug] %s FAIL #3: MissionClass::Update 未被调用\n", pTag);
            ok = false;
        }

        // #3 续：MissionClass::Update 必须真的走到 32 路分派（过血量/计时器门）。
        if (MissionDispatchCalls <= 0)
        {
            std::fprintf(stderr, "[ChainDebug] %s FAIL #3: Mission_* 分派未发生（血量或任务计时器门未过）\n", pTag);
            ok = false;
        }

        // #6：足类必须进入 FootClass::Update。
        if (FootUpdateCalls <= 0)
        {
            std::fprintf(stderr, "[ChainDebug] %s FAIL #6: FootClass::Update 未被调用\n", pTag);
            ok = false;
        }

        // #7：行走逻辑必须每帧被 Process 推进。
        if (LocomotionProcessCalls <= 0)
        {
            std::fprintf(stderr, "[ChainDebug] %s FAIL #7: ILocomotion::Process 未被调用\n", pTag);
            ok = false;
        }

        if (ok)
            std::fprintf(stderr, "[ChainDebug] %s PASS: #2/#3/#6/#7 全链路连通\n", pTag);

        std::fflush(stderr);
        return ok;
    }

    // ------------------------------------------------------------------
    // Tick —— Main_Loop 帧尾调用
    //
    //   原版没有这个函数；它纯粹是排查设施。默认关闭（未设环境变量时
    //   只做一次静态标志判断），因此不会进入发布行为的路径。
    // ------------------------------------------------------------------
    void Tick()
    {
        static int8  s_Enabled = -1;    // -1 未探测 / 0 关 / 1 开
        static int32 s_Frames  = 0;

        if (s_Enabled < 0)
        {
            const char* pEnv = std::getenv("OPENYR_CHAIN_DEBUG");
            s_Enabled = (pEnv != nullptr && pEnv[0] == '1') ? 1 : 0;
        }

        if (s_Enabled == 0)
            return;

        // 每 300 帧汇总一次，避免刷屏。
        if ((++s_Frames % 300) != 0)
            return;

        Dump("periodic");
        CheckAllLinks("periodic");

        // 清空后重新累计下一窗口，便于看出"这 300 帧里是否有对象被驱动"。
        Reset();
    }
}
