// ============================================================================
// FactoryClass.cpp - production queue attached to a producing structure
// ============================================================================

#include "FactoryClass.h"
#include "../Houses/HouseClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/UnitTypeClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/InfantryTypeClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Abstract/AircraftTypeClass.h"
#include "../Core/Memory.h"
#include "../IO/CRC.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"

// ============================================================================
// Global registry
// ============================================================================

DynamicVectorClass<FactoryClass*>* FactoryClass::Array = nullptr;

void FactoryClass::Init_Array()
{
    if (Array == nullptr)
        Array = new DynamicVectorClass<FactoryClass*>();
}

void FactoryClass::Delete_Array()
{
    if (Array != nullptr)
    {
        for (int32 i = 0; i < Array->Count; ++i)
        {
            delete (*Array)[i];
        }
        delete Array;
        Array = nullptr;
    }
}

// ============================================================================
// Construction / destruction
// ============================================================================

FactoryClass::FactoryClass(BuildingClass* pFactory, HouseClass* pOwner)
    : Factory(pFactory)
    , Owner(pOwner)
    , CurrentType(nullptr)
    , Progress(0)
    , ProductionType(0)
    , Suspended(false)
    , HasCompleted(false)
{
    if (Array == nullptr)
        Init_Array();
    if (Array != nullptr)
        Array->Add(this);

    if (Owner != nullptr)
        ++Owner->FactoryCount;
}

FactoryClass::~FactoryClass()
{
    if (Array != nullptr)
    {
        for (int32 i = 0; i < Array->Count; ++i)
        {
            if ((*Array)[i] == this)
            {
                Array->Remove(i);
                break;
            }
        }
    }

    if (Owner != nullptr && Owner->FactoryCount > 0)
        --Owner->FactoryCount;

    Queue.Clear();
}

// ============================================================================
// Queue / order management
// ============================================================================

bool FactoryClass::QueueProduction(TechnoTypeClass* pType)
{
    if (pType == nullptr)
        return false;

    // Start the order immediately when the factory is idle.
    if (CurrentType == nullptr)
    {
        StartProduction(pType);
        return true;
    }

    Queue.Add(pType);
    return true;
}

void FactoryClass::StartProduction(TechnoTypeClass* pType)
{
    if (pType == nullptr)
        return;

    CurrentType = pType;
    Progress = 0;
    HasCompleted = false;

    // Distinguish building orders from unit orders.
    ProductionType = (pType->WhatAmI() == AbstractType::BuildingType) ? 1 : 0;
}

bool FactoryClass::RemoveFromQueue(TechnoTypeClass* pType)
{
    if (pType == nullptr)
        return false;

    if (CurrentType == pType)
    {
        AbandonProduction();
        return true;
    }

    for (int32 i = 0; i < Queue.Count; ++i)
    {
        if (Queue[i] == pType)
        {
            Queue.Remove(i);
            return true;
        }
    }
    return false;
}

bool FactoryClass::IsXQueued(TechnoTypeClass* pType) const
{
    if (pType == nullptr)
        return false;
    if (CurrentType == pType)
        return true;
    for (int32 i = 0; i < Queue.Count; ++i)
    {
        if (Queue[i] == pType)
            return true;
    }
    return false;
}

int32 FactoryClass::CountThisInQueue(TechnoTypeClass* pType) const
{
    if (pType == nullptr)
        return 0;

    int32 count = 0;
    if (CurrentType == pType)
        ++count;
    for (int32 i = 0; i < Queue.Count; ++i)
    {
        if (Queue[i] == pType)
            ++count;
    }
    return count;
}

void FactoryClass::Suspend()
{
    Suspended = true;
}

void FactoryClass::Unsuspend()
{
    Suspended = false;
}

void FactoryClass::AbandonProduction()
{
    CurrentType = nullptr;
    Progress = 0;
    HasCompleted = false;
    Queue.Clear();
}

// ============================================================================
// Progress
// ============================================================================

int32 FactoryClass::GetBuildTimeFrames(TechnoTypeClass* pType) const
{
    if (pType == nullptr)
        return 0;

    int32 frames = pType->BuildTime;
    if (frames <= 0)
        frames = (pType->Cost > 0) ? (pType->Cost / 8) : 60;
    if (frames < 1)
        frames = 1;
    return frames;
}

int32 FactoryClass::GetRemainingFrames() const
{
    if (CurrentType == nullptr)
        return 0;
    int32 total = GetBuildTimeFrames(CurrentType);
    int32 remaining = total - Progress;
    return (remaining > 0) ? remaining : 0;
}

bool FactoryClass::IsCompleted() const
{
    return HasCompleted;
}

bool FactoryClass::IsWorking() const
{
    return (CurrentType != nullptr) && !Suspended && !HasCompleted;
}

void FactoryClass::Update()
{
    if (CurrentType == nullptr || Suspended)
        return;

    ++Progress;
    if (Progress >= GetBuildTimeFrames(CurrentType))
    {
        CompletedProduction();
    }
}

// ============================================================================
// Completion
// ============================================================================

void FactoryClass::CompletedProduction()
{
    if (CurrentType == nullptr)
        return;

    PlaceProducedUnit(CurrentType);

    HasCompleted = true;
    CurrentType = nullptr;
    Progress = 0;

    // Pull the next order from the queue.
    if (Queue.Count > 0)
    {
        TechnoTypeClass* pNext = Queue[0];
        Queue.Remove(0);
        StartProduction(pNext);
    }
}

void FactoryClass::PlaceProducedUnit(TechnoTypeClass* pType)
{
    if (pType == nullptr || Factory == nullptr || Owner == nullptr)
        return;

    // Resolve the exit cell; bail out if the factory cannot release.
    CellStruct exitCell = Factory->FindExitCell(0, 0);
    CellClass* pCell = MapClass::Instance->GetCellAt(exitCell);
    if (pCell == nullptr)
        return;

    CoordStruct exitCoord(static_cast<int32>(exitCell.X) * 256,
                          static_cast<int32>(exitCell.Y) * 256,
                          pCell->Get_Ground_Height());

    // Create the unit at the exit point.  Buildings are placed by the
    // construction workflow rather than spawned here.
    switch (pType->WhatAmI())
    {
        case AbstractType::UnitType:
        {
            UnitClass* pUnit = new UnitClass(Owner);
            if (pUnit != nullptr)
            {
                pUnit->Type = static_cast<UnitTypeClass*>(pType);
                pUnit->Owner = Owner;
                pUnit->SetCoords(exitCoord);
            }
            break;
        }
        case AbstractType::InfantryType:
        {
            InfantryClass* pInf = new InfantryClass(Owner);
            if (pInf != nullptr)
            {
                pInf->Type = static_cast<InfantryTypeClass*>(pType);
                pInf->Owner = Owner;
                pInf->SetCoords(exitCoord);
            }
            break;
        }
        case AbstractType::AircraftType:
        {
            AircraftClass* pAircraft = new AircraftClass(Owner);
            if (pAircraft != nullptr)
            {
                pAircraft->Type = static_cast<AircraftTypeClass*>(pType);
                pAircraft->Owner = Owner;
                pAircraft->SetCoords(exitCoord);
            }
            break;
        }
        case AbstractType::BuildingType:
        {
            // Buildings are placed at the exit cell of the constructing
            // yard; the foundation is registered and the structure becomes
            // live immediately.  (Animated construction overlay is handled
            // by the animation layer once a construction anim is attached.)
            BuildingClass* pBuilding = new BuildingClass(Owner);
            if (pBuilding != nullptr)
            {
                pBuilding->Type = static_cast<BuildingTypeClass*>(pType);
                pBuilding->Owner = Owner;
                pBuilding->SetCoords(exitCoord);
                pBuilding->Place(true);
            }
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// Serialization / CRC
// ============================================================================

bool FactoryClass::Save(IStream* pStm) const
{
    if (pStm == nullptr)
        return false;
    return true;
}

bool FactoryClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return false;
    return true;
}

void FactoryClass::ComputeCRC(CRCEngine& crc) const
{
    crc.AddData(&Progress, sizeof(Progress));
    crc.AddData(&ProductionType, sizeof(ProductionType));
    crc.AddData(&Suspended, sizeof(Suspended));
    crc.AddData(&HasCompleted, sizeof(HasCompleted));
    crc.AddData(&Queue.Count, sizeof(Queue.Count));
}
