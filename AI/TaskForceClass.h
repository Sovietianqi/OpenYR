#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Abstract/AbstractTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================
class CCINIClass;
class TechnoTypeClass;

// ============================================================================
// TaskForceMember - a single entry in a task force (unit type + count)
// ============================================================================
struct TaskForceMember {
    TechnoTypeClass* Type;
    int32            Count;
    int32            MinCount;
    int32            MaxCount;

    TaskForceMember() : Type(nullptr), Count(0), MinCount(0), MaxCount(0) {}
};

// ============================================================================
// TaskForceClass - definition of an AI task force
// Inherits AbstractTypeClass. A task force lists the unit types and counts
// that make up a team's composition, parsed from the [TaskForces] INI block.
// ============================================================================
class NOVTABLE TaskForceClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TaskForce;

    static DynamicVectorClass<TaskForceClass*>* Array;

    // TaskForceClass_CreateFromINIList: walks [TaskForces] and builds
    // one task force per listed name.
    // 根据游戏行为，可知任务力补全原版命名形态：成员表与 INI 的
    // 双向同步、成员计数与一个弃用查找槽。
    bool SaveListToINI(class CCINIClass* pINI, const char* pSection);
    void LoadObjects(class CCINIClass* pINI, const char* pSection);
    int32 CountObjects() const;
    static TaskForceClass* Find_obsolete(const char* pID);

    static void CreateFromINIList(CCINIClass* pINI);

    static TaskForceClass* Find(const char* pID);
    static TaskForceClass* FindOrAllocate(const char* pID);
    static int32 GetCount();

    TaskForceClass(const char* pID) noexcept;
    explicit TaskForceClass(noinit_t) noexcept : AbstractTypeClass(noinit) {}
    virtual ~TaskForceClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual int32 GetCRC() const;

    int32 GetMemberCount() const;
    int32 GetTotalUnitCount() const;
    TaskForceMember* GetMember(int32 index);

    // Task-required member management API
    void Add_Member(TechnoTypeClass* pType, int32 count, int32 minCount = 0, int32 maxCount = 0);
    void Remove_Member(int32 index);
    void Remove_Member(TechnoTypeClass* pType);
    int32 Get_Member_Count() const;
    int32 Get_Total_Units() const;
    bool Has_Member_Type(TechnoTypeClass* pType) const;
    TaskForceMember* Get_Member_Entry(int32 index);
    const DynamicVectorClass<TaskForceMember>* Get_All_Members() const;
    bool Is_Valid() const;

    DynamicVectorClass<TaskForceMember> Members;
    int32 Grouping;
};
