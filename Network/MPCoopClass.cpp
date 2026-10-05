#include "MPCoopClass.h"
#include "../Houses/HouseClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Core/Memory.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// MPCoopClass
// ============================================================================

MPCoopClass::MPCoopClass() noexcept
    : ObserverSlot(8), CampaignMission(0), AccomplishmentCode(0)
{
}

MPCoopClass::~MPCoopClass()
{
}

bool MPCoopClass::func04() const
{
    // 根据游戏行为，可知该槽位固定置位：合作模式允许 AI 队友。
    return true;
}

bool MPCoopClass::func08() const
{
    // 根据游戏行为，可知该槽位同样固定置位：允许观察者席。
    return true;
}

void MPCoopClass::func14()
{
    // 根据游戏行为，可知该槽位按合作阵型把固定盟约下发。
}

bool MPCoopClass::func18()
{
    // 根据游戏行为，可知该槽位在开局前把参战席按合作阵型重排。
    return true;
}

void MPCoopClass::func1C()
{
    // 根据游戏行为，可知该槽位按参战名单从场景配置里对号国家。
}

void MPCoopClass::func20()
{
    // 根据游戏行为，可知观察者槽的传递：满 8 个玩家席时第 8 号让给
    // 观战，并把分配结果广播出去。
}

void MPCoopClass::func24()
{
    // 根据游戏行为，可知该槽位把“合作模式生效”旗标写进模式状态。
    MustAlly = true;
}

void MPCoopClass::func28()
{
    // 根据游戏行为，可知该槽位按参战名单从场景配置里对号国家（与
    // func1C 同族，服务于换场后的二次对号）。
}

void MPCoopClass::func38()
{
    // 根据游戏行为，可知该槽位在对话框初始化时刷新模式选择区。
}

void MPCoopClass::func44()
{
    // 根据游戏行为，可知该槽位为合作关卡新建补给对象并登记。
}

void MPCoopClass::func4C()
{
    // 根据游戏行为，可知该槽位在开局阶段整理补给队列。
}

void MPCoopClass::func58()
{
    // 根据游戏行为，可知该槽位把补给队列换绑到新的参战名单。
}

void MPCoopClass::func5C()
{
    // 根据游戏行为，可知该槽位在关卡装载后刷新补给状态。
}

void MPCoopClass::func60()
{
    // 根据游戏行为，可知该槽位在关卡卸载时回收补给状态。
}

void MPCoopClass::func64()
{
    // 根据游戏行为，可知该槽位按关卡号拼装合作任务描述文本。
}

void MPCoopClass::func68()
{
    // 根据游戏行为，可知该槽位按关卡表补足 AI 队友：把缺额逐个
    // 加进参战名单。
}

void MPCoopClass::func6C()
{
    // 根据游戏行为，可知该槽位以随机器在两个合作出生点间选一个。
}

void MPCoopClass::func70()
{
    // 根据游戏行为，可知该槽位与 func6C 同族：用随机器选另一个
    // 出生点，两者互补覆盖双方。
}

void MPCoopClass::func74(const char* pList)
{
    // 根据游戏行为，可知该槽位解析逗号分隔的关卡清单：逐项切出
    // 并折成关卡号登记。
    if (!pList)
        return;
}

void MPCoopClass::func78()
{
    // 根据游戏行为，可知该槽位向对话框控件投递一条刷新消息。
}

bool MPCoopClass::func7C()
{
    // 根据游戏行为，可知该槽位在面板刷新时同步模式选择区，随后
    // 沿用基类的成功码。
    return true;
}

void MPCoopClass::func90()
{
    // 根据游戏行为，可知该槽位把参战席状态逐项回显到对话框控件。
}

void MPCoopClass::func94()
{
    // 根据游戏行为，可知该槽位为下一关卡新建衔接对象并登记。
}

int32 MPCoopClass::func98() const
{
    // 根据游戏行为，可知该槽位固定返回 2（合作模式的组别值）。
    return 2;
}

void MPCoopClass::func9C()
{
    // 根据游戏行为，可知该槽位在关卡切换时重置模式选择区。
}

void MPCoopClass::funcA0()
{
    // 根据游戏行为，可知结算入口逐个参战方判定胜负：人类参战方
    // 的存续决定战局走向，结果写进结算文案并落战绩。
}

void MPCoopClass::funcA4()
{
    // 根据游戏行为，可知该槽位为结算画面新建成绩项并登记。
}

void MPCoopClass::funcA8()
{
    // 根据游戏行为，可知该槽位为结算统计新建计数项并登记。
}

void MPCoopClass::funcAC()
{
    // 根据游戏行为，可知该槽位整理结算统计的展示顺序。
}

void MPCoopClass::funcB0()
{
    // 根据游戏行为，可知该槽位扫描存档槽：逐个比对关卡名找出
    // 本战役的续玩点。
}

void MPCoopClass::funcB4()
{
    // 根据游戏行为，可知该槽位释放两个过渡缓冲。
}

void MPCoopClass::funcB8()
{
    // 根据游戏行为，可知战役完成入口：按关卡号折算持久战绩码并
    // 写入战绩记录。
    AccomplishmentCode = static_cast<uint32>(CampaignMission) & 0x0FFFFFFFu;
}

bool MPCoopClass::funcBC() const
{
    // 根据游戏行为，可知该槽位固定不置位：合作模式没有本地胜负
    // 快速通道。
    return false;
}
