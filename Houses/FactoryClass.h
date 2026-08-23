#pragma once

#include "../Core/Definitions.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;
class TechnoTypeClass;
class TechnoClass;
class BuildingClass;
class IStream;

// ============================================================================
// FactoryClass - production queue attached to a producing structure
//
// One FactoryClass exists per production structure (war factory, barracks,
// shipyard, etc.).  It owns the queue of pending production orders and
// advances the build progress each frame.  When a build completes the
// completed unit is placed at the factory's exit cell and the house is
// notified.
// ============================================================================
class FactoryClass
{
public:
    FactoryClass(BuildingClass* pFactory, HouseClass* pOwner);
    ~FactoryClass();

    // Queue / order management
    bool QueueProduction(TechnoTypeClass* pType);
    void StartProduction(TechnoTypeClass* pType);
    bool RemoveFromQueue(TechnoTypeClass* pType);
    bool IsXQueued(TechnoTypeClass* pType) const;
    int32 CountThisInQueue(TechnoTypeClass* pType) const;
    void Suspend();
    void Unsuspend();
    void CompletedProduction();
    void AbandonProduction();

    // Progress
    void Update();
    int32 GetBuildTimeFrames(TechnoTypeClass* pType) const;
    int32 GetProgress() const { return Progress; }
    int32 GetRemainingFrames() const;
    bool IsCompleted() const;
    bool IsSuspended() const { return Suspended; }
    bool IsWorking() const;

    // Accessors
    BuildingClass*   GetFactoryBuilding() const { return Factory; }
    HouseClass*      GetOwner() const { return Owner; }
    TechnoTypeClass* GetCurrentOrder() const { return CurrentType; }

    // Serialization / CRC
    bool Save(IStream* pStm) const;
    bool Load(IStream* pStm);
    void ComputeCRC(CRCEngine& crc) const;

    // Global registry
    static DynamicVectorClass<FactoryClass*>* Array;
    static void Init_Array();
    static void Delete_Array();

private:
    void PlaceProducedUnit(TechnoTypeClass* pType);

    BuildingClass*   Factory;
    HouseClass*      Owner;
    TechnoTypeClass* CurrentType;
    DynamicVectorClass<TechnoTypeClass*> Queue;
    int32            Progress;          // frames completed for the current order
    int32            ProductionType;    // 0 = unit, 1 = building (ABuildingType)
    bool             Suspended;
    bool             HasCompleted;
};
