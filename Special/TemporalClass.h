#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class TechnoClass;

class TemporalClass {
public:
    TemporalClass() noexcept;
    ~TemporalClass();

    // 根据游戏行为，可知时间固定链的松脱分两档：完整松脱要把链接
    // 关系一并归还给后继节点，而即刻松脱只处理自身。
    void LetGo();
    void JustLetGo();
    bool CanWarpTarget(TechnoClass* pTarget);
    int32 GetHelperDamagePerStep(int32 percent) const;

    TechnoClass* LinkedTechno;
    TechnoClass* Owner;
    int32 WarpDistance;
    int32 ChargeCount;
    TemporalClass* Next;
    TemporalClass* Prev;
};
