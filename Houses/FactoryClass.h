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

    // FactoryClass_IsOnHold (asm 0x4C9xxx): `mov al, [ecx+OnHold]`.
    //   True while the factory's owner has put the queue on hold.
    bool IsOnHold() const { return OnHold; }
    // FactoryClass::Has_Changed (asm 0x4C9xxx).
    //   Reads the "the queue changed since the sidebar last looked" byte and
    //   clears it, so the caller learns about an update exactly once.
    bool Has_Changed() { const bool c = IsDifferent; IsDifferent = false; return c; }
    // FactoryClass::Get_Product (asm 0x4C9xxx): the type currently on the line.
    TechnoTypeClass* Get_Product() const { return CurrentProduction; }
    // FactoryClass_GetSpecialItem (asm 0x4C9xxx): the side-specific "special"
    //   item (a free bonus unit the house can build), cached at +0x68.
    TechnoTypeClass* GetSpecialItem() const { return SpecialItem; }

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

    // ── Fields the vtable probes read directly ──────────────────────────
    bool             OnHold;             // factory queue suspended by the owner
    bool             IsDifferent;        // "queue changed" flag, cleared on read
    TechnoTypeClass* CurrentProduction;  // the type on the line right now
    TechnoTypeClass* SpecialItem;        // side-specific bonus buildable
};
