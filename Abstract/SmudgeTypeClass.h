#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "ObjectTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class CRCEngine;
class SmudgeClass;

// ============================================================================
// SmudgeTypeClass - type definition for smudges
// Inherits ObjectTypeClass. Describes a kind of smudge (scorch, crater, etc.)
// including its dimensions and behavior on the terrain.
// ============================================================================
class NOVTABLE SmudgeTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::SmudgeType;

    static DynamicVectorClass<SmudgeTypeClass*>* Array;

    static SmudgeTypeClass* Find(const char* pID);
    static SmudgeTypeClass* FindOrAllocate(const char* pID);
    static SmudgeTypeClass* FindByIndex(int32 index);
    // asm: SmudgeType_FindIndexByName (0x6B5440) -- case-insensitive lookup of
    //   the ordinal by the type's ID string; returns -1 when not present.
    static int32 FindIndexByName(const char* pID);
    // Convenience wrapper over FindIndexByName returning the type pointer.
    static SmudgeTypeClass* FindByName(const char* pID);
    static int32 GetCount();
    static void Init_Array();
    static void Delete_Array();
    static void Delete_All();

    // asm: InitList_2736 / InitList_2737 —— vec_SmudgeTypes 的静态构造/析构。
    static void sctor();
    static void sdtor();

    // asm: SmudgeTypeClass_LoadFromINIList (asm:204) —— 依剧场索引把每个
    //   剧场地形污渍的 "<ID>.<theater ext>" SHP 解析进 ImageSHP。
    static void LoadFromINIList(int32 idxTheater);

    SmudgeTypeClass(const char* pID) noexcept;
    explicit SmudgeTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~SmudgeTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    // asm: SmudgeTypeClass_GetArrayIndex (mov eax,[ecx+294h] ; retn)
    virtual int32 GetArrayIndex() const;
    // asm: SmudgeTypeClass_GetAbstractDerivationID (mov eax,1Eh ; retn)
    virtual int32 GetAbstractDerivationID() const;

    // Places a smudge instance of this type at the given cell.  The original
    // allocates a SmudgeClass (an OverlayClass-sized ObjectClass derivative)
    // and constructs it with a pixel coordinate of cell*256 + 0x80 (the cell
    // center) and owner -1.  Returns true when the instance was created.
    // Mirrors SmudgeTypeClass_SpawnSmudgeAt (vt slot, 0x6B5540).
    virtual bool SpawnSmudgeAt(CellStruct* pCell);

    // asm: SmudgeTypeClass_CreateSmudge (0x6B55C0, __thiscall(this), retn 4).
    //   Allocates a bare SmudgeClass of this type anchored at the origin cell
    //   (the shared {0,0,0} hint block) with owner -1.  Returns the instance or
    //   nullptr when allocation fails.
    virtual SmudgeClass* CreateSmudge();

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI);
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // asm: 类型层 SHP 句柄访问面（对应 ObjectTypeClass::GetImage / +0xA4）。
    SHPStruct* GetImageData() const;

    // Smudge-specific helpers
    CellStruct Get_Size() const;
    bool       Is_Crater() const;
    bool       Is_Scorch() const;

public:
    int32      HeapID;        // +0x294: registry ordinal (GetArrayIndex)
    CellStruct CellSize;
    int32      Frames;
    bool       IsCrater;
    bool       IsScorch;
    bool       IsBib;
    bool       IsAnimated;
    bool       IsFlat;
    bool       IsBurn;
    int32      Width;
    int32      Height;
    int32      ChainCount;
    int32      ChainSteps;
    char       ArtName[0x20];
};
