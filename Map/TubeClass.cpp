// ============================================================================
// Map/TubeClass.cpp
//
// asm 权威：vec_Tubes 的登记表与 TubeClass vt+34h 的 CRC 取值。
// ============================================================================

#include "TubeClass.h"

DynamicVectorClass<TubeClass*>* TubeClass::Array = nullptr;

TubeClass::TubeClass() noexcept
    : EnterCell()
    , ExitCell()
    , Direction(0)
{
}

// asm: TubeClass::ComputeCRC —— 入口格、出口格、通行方向依次入 CRC。
void TubeClass::ComputeCRC(CRCEngine& crc) const
{
    crc.AddData(&EnterCell.X, sizeof(EnterCell.X));
    crc.AddData(&EnterCell.Y, sizeof(EnterCell.Y));
    crc.AddData(&ExitCell.X, sizeof(ExitCell.X));
    crc.AddData(&ExitCell.Y, sizeof(ExitCell.Y));
    crc.AddData(&Direction, sizeof(Direction));
}
