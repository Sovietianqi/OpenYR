#pragma once

#include "../Core/Definitions.h"
#include "../Math/CoordStruct.h"

class DamageArea {
public:
    int32 X;
    int32 Y;
    int32 Z;
    int32 Damage;
    int32 Range;
    WarheadTypeClass* Warhead;

    DamageArea() : X(0), Y(0), Z(0), Damage(0), Range(0), Warhead(nullptr) {}

    static void ApplyCellDamage(CoordStruct center, int32 damage, TechnoClass* pSource,
                                WarheadTypeClass* pWarhead, bool affectsAllies,
                                HouseClass* pSourceHouse);
    static void ApplyAtLocation(CoordStruct location, int32 damage, TechnoClass* pSource,
                                WarheadTypeClass* pWarhead, bool affectsAllies,
                                HouseClass* pSourceHouse);
    static int32 CalculateDamageFalloff(int32 baseDamage, int32 distance, int32 maxDistance,
                                         float percentAtMax);
    static bool IsInRange(CoordStruct center, CoordStruct target, float cellSpread);
    static void ApplyRadiation(CoordStruct center, int32 damage, int32 duration,
                                TechnoClass* pSource, HouseClass* pSourceHouse);

private:
    static void ApplyToCell(CellClass* pCell, int32 damage, TechnoClass* pSource,
                            WarheadTypeClass* pWarhead, bool affectsAllies,
                            HouseClass* pSourceHouse);
    static void ApplyToTechno(TechnoClass* pTarget, int32 damage, TechnoClass* pSource,
                              WarheadTypeClass* pWarhead, bool affectsAllies,
                              HouseClass* pSourceHouse);
};