#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/VectorClass.h"

// ============================================================================
// SideClass
//
//   One entry of the [Sides] section, built by RulesClass_Addition_Sides
 //.  The object is an AbstractTypeClass whose ID is the side
//   name, plus the list of houses that belong to it.
//
//   Layout, matching the original:
//     +24  ID[0x48]      the side name (AbstractTypeClass's own string field)
//     +98  cVector<int>  the house ordinals parsed by INIClass_ParseSideHouses
//     +AC  int32         the vector's default growth step (0x0A)
// ============================================================================

class SideClass : public AbstractTypeClass
{
public:
    static const AbstractType AbsID = AbstractType::Abstract;

    // vec_Sides - every registered side, in creation order.
    static DynamicVectorClass<SideClass*>* Sides;

    SideClass(const char* pID) noexcept;
    virtual ~SideClass();

 // Side_From_Name: the ordinal of the side whose name
    // matches pName case-insensitively, or -1.  RulesClass_Addition_Sides
    // reuses an existing side through this before allocating a new one.
    static int32 From_Name(const char* pName);

    static SideClass* Find(const char* pID);
    static SideClass* FindOrAllocate(const char* pID);
    static int32 GetCount();

 // RulesClass_Addition_Sides: the whole [Sides] walk.
    static void ReadSides(CCINIClass* pINI);

 // INIClass_ParseSideHouses: reads the side's value string
    // out of pINI, splits it on ',' and turns every token into a house
    // ordinal, appending the hits to the vector.  An unset or empty value
    // leaves the vector empty.
    void ParseHouses(CCINIClass* pINI, const char* pSection, const char* pKey);

    // The house ordinals this side owns.  The original seeds the vector's
    // growth step with 0x0A at construction.
    VectorClass<int> Houses;
};
