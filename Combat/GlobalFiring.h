#pragma once

#include "../Core/Definitions.h"
#include "../Containers/ListClass.h"
#include "../Math/CoordStruct.h"

class GlobalFiring {
public:
    static BulletClass* Fire(WeaponTypeClass* pWeapon, CoordStruct source,
                             CoordStruct target, TechnoClass* pOwner);
    static BulletClass* FireAt(WeaponTypeClass* pWeapon, TechnoClass* pOwner,
                               CoordStruct target);
    static BulletClass* FireBurst(WeaponTypeClass* pWeapon, TechnoClass* pOwner,
                                  CoordStruct target, int32 burstCount);
    static CoordStruct CalculateFireCoords(CoordStruct source, CoordStruct target,
                                            WeaponTypeClass* pWeapon);
    static CoordStruct ApplyScatter(CoordStruct coords, WeaponTypeClass* pWeapon,
                                     TechnoClass* pOwner);
    static void UpdateAll();
    static void RemoveBullet(BulletClass* pBullet);
    static void RemoveAllBullets();
    static int32 GetBulletCount();
    static BulletClass* GetFirstBullet();
    static BulletClass* GetNextBullet(BulletClass* pBullet);
    static void ProcessBulletImpacts();
    static void UpdateBulletAnimations();
    static void CleanupOrphanedBullets();
    static bool CanFireAt(WeaponTypeClass* pWeapon, TechnoClass* pOwner,
                          TechnoClass* pTarget);
    static bool IsValidTarget(TechnoClass* pTarget, TechnoClass* pOwner);

public:
    static List<BulletClass*> Bullets;
};