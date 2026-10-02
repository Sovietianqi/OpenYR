#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Containers/DynamicVectorClass.h"

// ============================================================================
// MovieClass - the global movie name table
//
//  Movie names are registered once (from the [Movies] section of the rules
//  INI) and afterwards referenced by index, which is how the campaign and
//  mission files store their Intro / Brief / Win / Lose selections.
//  A name of "<none>" always resolves to "not found".
// ============================================================================

class MovieClass
{
public:
    MovieClass();
    ~MovieClass();

    // Global registry
    static DynamicVectorClass<MovieClass*>* Array;

    static int32  GetCount();
    static MovieClass* Get(int32 index);
    static MovieClass* Find(const char* pName);

    // Case-insensitive lookup returning the slot index, or -1.  "<none>"
    // and the empty string never match.
    static int32  FindIndexByName(const char* pName);

    // Adds a name to the table, returning its index.  An existing entry is
    // re-used so repeated loads stay idempotent.
    static int32  Register(const char* pName);

    static void   Clear();

    const char* GetName() const { return Name; }

    char  Name[0x100];
    int32 ArrayIndex;
};
