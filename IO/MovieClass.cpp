#include "MovieClass.h"

#include <cstring>

// ============================================================================
// MovieClass - implementation
// ============================================================================

DynamicVectorClass<MovieClass*>* MovieClass::Array = nullptr;

MovieClass::MovieClass()
    : ArrayIndex(-1)
{
    Name[0] = '\0';
}

MovieClass::~MovieClass()
{
}

int32 MovieClass::GetCount()
{
    return Array != nullptr ? Array->Count : 0;
}

MovieClass* MovieClass::Get(int32 index)
{
    if (Array == nullptr || index < 0 || index >= Array->Count)
        return nullptr;
    return (*Array)[index];
}

MovieClass* MovieClass::Find(const char* pName)
{
    const int32 index = FindIndexByName(pName);
    return index >= 0 ? Get(index) : nullptr;
}

// ============================================================================
// FindIndexByName - MovieClass_FindIndexByName
//
//   "<none>" is reserved and always reports "not found", so a mission that
//   stores it does not accidentally reference slot zero.
// ============================================================================
int32 MovieClass::FindIndexByName(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0')
        return -1;

    if (_strcmpi(pName, "<none>") == 0)
        return -1;

    if (Array == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        MovieClass* pItem = (*Array)[i];
        if (pItem == nullptr)
            continue;
        if (_strcmpi(pItem->Name, pName) == 0)
            return i;
    }

    return -1;
}

int32 MovieClass::Register(const char* pName)
{
    if (pName == nullptr || pName[0] == '\0')
        return -1;

    const int32 existing = FindIndexByName(pName);
    if (existing >= 0)
        return existing;

    if (Array == nullptr)
        Array = new DynamicVectorClass<MovieClass*>();

    MovieClass* pMovie = new MovieClass();
    std::strncpy(pMovie->Name, pName, sizeof(pMovie->Name) - 1);
    pMovie->Name[sizeof(pMovie->Name) - 1] = '\0';
    pMovie->ArrayIndex = Array->Count;

    Array->Add(pMovie);
    return pMovie->ArrayIndex;
}

void MovieClass::Clear()
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
        delete (*Array)[i];

    Array->Clear();
}
