#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class VersionClass {
public:
    VersionClass() noexcept;

    // 根据游戏行为，可知版本号以定点小数维护，读写都落在两枚
    // 版本槽位上。
    int32 GetV1() const;
    int32 GetV2() const;
    void SetV();
    int32 GetVer3(int32 a2, int32 a3) const;
    bool ReadVer1();

    char VersionString[16];
    bool VersionStringLoaded;
    int32 VersionLow;
    int32 VersionHigh;
};
