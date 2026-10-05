#pragma once

#include "../Core/Definitions.h"

// ============================================================================
// CountClass - 网络记分用的计数表
//
// 根据游戏行为，可知原版以一张可增容的计数表登记各玩家的累计量（击
// 毁/损失等），在收发包时按网络字节序整表序列化。
// ============================================================================
class CountClass {
public:
    CountClass() noexcept;

    int32 GetTotal() const;
    void  IncValue(int32 idx);
    int32 IncValue_OrAlloc();
    void  Rem(int32 idx);
    void  ResetLimited();
    void  ResetWithGivenCount(int32 count);
    void  ResizeTo(int32 count);
    void  htonl();
    void  ntohl();

    int32* Values;       // 计数槽位
    int32  Capacity;     // 已分配容量
    int32  Count;        // 在用槽位数
    int32  Limit;        // 复位时保留的上限
};
