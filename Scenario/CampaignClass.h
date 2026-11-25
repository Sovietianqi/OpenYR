#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

class CCINIClass;
class IStream;
class CRCEngine;

// ============================================================================
// CampaignClass - one entry of the [Battles] list in a campaign INI
//
//   asm: CampaignClass_CTOR begins with `call AbstractTypeClass_CTOR` and the
//   object carries the CampaignClass / IRTTITypeInfo / INoticeSink /
//   INoticeSource vtables; it is therefore an AbstractTypeClass derivative.
//
//   Layout (sizeof = 0x3A0, from the `CampaignClass struc`):
//     +00  VTable
//     +04  AbstractTypeClass a        (size 0x94; the INI section name lives
//                                      in the inherited nameINI slot at
//                                      +0x24, reached as `[this+0x24]`)
//     +98  int32 CD                   "CD"          (default -1)
//     +9C  char Scenario[0x200]       "Scenario"    upper-cased on load
//     +29C int32 FinalMovie           "FinalMovie"  movie index, default -1
//     +2A0 wchar_t Description[0x80]  "Description" (0x100 bytes)
//
//   CreateFromINIList walks [Battles]; each key names a campaign section, the
//   value names the campaign ID.  An existing campaign with that ID is reused
//   (looked up through FindByName), otherwise a new one is created.  Both
//   paths finish by invoking the LoadFromINI virtual (asm vtable slot +0x64).
// ============================================================================

class CampaignClass : public AbstractTypeClass
{
public:
    static const AbstractType AbsID = AbstractType::Campaign;

    static DynamicVectorClass<CampaignClass*>* Array;

    CampaignClass() noexcept;
    explicit CampaignClass(const char* pID) noexcept;
    explicit CampaignClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}
    virtual ~CampaignClass();

    // ------------------------------------------------------------------
    // IPersistStream / vtable contract (asm vftable slots).
    // ------------------------------------------------------------------
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override { return AbstractType::Campaign; }
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // asm: CampaignClass_GetAbstractDerivationID: mov eax,0Ah ; retn.
    // AbstractClass has no such virtual, so (as with AircraftClass etc.) it is
    // supplied here as a fresh virtual whose value matches the asm.
    virtual AbstractType GetAbstractDerivationID() const;

    // ------------------------------------------------------------------
    // [Battles] INI parser.  asm takes a single CCINIClass* and derives the
    // section from the object's inherited ID (set by the constructor).
    // ------------------------------------------------------------------
    virtual bool LoadFromINI(CCINIClass* pINI) override;

    // Walks [Battles] and instantiates one campaign per key.
    // asm: CampaignClass_CreateFromINIList.
    static bool CreateFromINIList(CCINIClass* pINI);

    // asm: sub_46CC90 - case-insensitive ID lookup, returns the array index or
    // -1 when not found.
    static int32 FindByName(const char* pName);

    static CampaignClass* Get(int32 index);
    static int32 GetCount();
    static void Clear();

    // ------------------------------------------------------------------
    // Members (offsets from the asm `CampaignClass struc`).
    // ------------------------------------------------------------------
    int32       CD;                  // +0x98
    char        Scenario[0x200];     // +0x9C
    int32       FinalMovie;          // +0x29C
    wchar_t     Description[0x80];   // +0x2A0
};
