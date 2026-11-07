#include "VersionClass.h"
#include "../IO/FileSystem.h"

static constexpr int32 VERSION_FIXED_POINT = 0x20000;

VersionClass::VersionClass() noexcept
    : VersionStringLoaded(false)
    , VersionLow(0)
    , VersionHigh(0)
{
    VersionString[0] = '\0';
}

int32 VersionClass::GetV1() const
{
    return VERSION_FIXED_POINT;
}

int32 VersionClass::GetV2() const
{
    return VERSION_FIXED_POINT;
}

void VersionClass::SetV()
{
    VersionLow = VERSION_FIXED_POINT;
    VersionHigh = VERSION_FIXED_POINT;
}

// ------------------------------------------------------------------------
// asm: signed int __thiscall VersionClass_GetVer3(VersionClass *this, unsigned int a2, unsigned int a3)   (asm:1725452)
// 截断返回。
// ------------------------------------------------------------------------
int32 VersionClass::GetVer3(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
    return VersionHigh;
}

// ------------------------------------------------------------------------
// asm: VersionClass_ReadVer1
// 且行尾的回车符会被剥离。
// ------------------------------------------------------------------------
bool VersionClass::ReadVer1()
{
    RawFileClass file("VERSION.TXT");
    if (!file.Exists()) {
        return false;
    }

    char buffer[16];
    const int32 read = file.ReadNextBytes(buffer, sizeof(buffer));
    if (read <= 0) {
        return false;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    for (int32 i = 0; i < sizeof(buffer); ++i) {
        if (buffer[i] == '\r' || buffer[i] == '\n') {
            buffer[i] = '\0';
            break;
        }
    }

    for (int32 i = 0; i < sizeof(VersionString); ++i) {
        VersionString[i] = buffer[i];
        if (buffer[i] == '\0') {
            break;
        }
    }

    VersionStringLoaded = true;
    return true;
}
