#pragma once

#include "../Abstract/AbstractTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class TriggerTypeClass;
class TagClass;
class CCINIClass;

// ------------------------------------------------------------------------
// 根据游戏行为，可知标签类型是触发器类型与附着对象的纽带：查找
// 沿标识符进行，允许胜利与区块穿越都有快捷判定。
//
// asm 结构 (sizeof=0xA4):
//   vt          +0x00
//   a           AbstractTypeClass
//   idxInArray  +0x98   在 Array 里的下标缓存
//   Repeating   +0x9C   是否可重复触发
//   Trigger     +0xA0   TriggerTypeClass*
// 本实现按功能对齐，字段命名沿用语义名。
// ------------------------------------------------------------------------
class TagTypeClass : public AbstractTypeClass {
public:
    static const AbstractType AbsID = AbstractType::TagType;

    static DynamicVectorClass<TagTypeClass*>* Array;

    static TagTypeClass* Find(const char* pID);
    static TagTypeClass* FindOrAllocate(const char* pID);
    static TagTypeClass* FindByNameOrID(const char* pName);

    // asm: TagTypeClass *__thiscall TagTypeClass_CreateInstance(TagTypeClass *source)
    // 在 vec_Tags_all 中查找已绑定到本类型（TagType）的实例，命中即返回，
    // 否则新建一个 TagClass 并挂到该触发器类型上。
    static TagClass* CreateInstance(TagTypeClass* pSource);

    // asm: TagTypeClass_CreateFromINIList / TagTypeClass_SaveToINIList
    // 场景 [Tags] 段的整体读写：每个键名是一条标签的 ID 或十六进制句柄，
    // 值是该标签的命令串。
    void CreateFromINIList(CCINIClass* pINI);
    void SaveToINIList(CCINIClass* pINI);

    TagTypeClass(const char* pID) noexcept;
    explicit TagTypeClass(noinit_t) noexcept;
    virtual ~TagTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual int32 Size() const override;          // asm: TagTypeClass_GetSize
    virtual bool LoadFromINI(CCINIClass* pINI) override;
    virtual bool SaveToINI(CCINIClass* pINI) override;
    virtual int32 GetArrayIndex() const override; // asm: TagTypeClass_GetArrayIndex
    virtual int32 GetCRC() const;                 // asm: TagTypeClass_GetCRC

    // asm: int __fastcall TagTypeClass_GetFlags(TagTypeClass *this)
    // 沿 Trigger 链依次取每个实例的标志位并做或累积。
    int32 GetFlags() const;

    // asm: byte __fastcall TagTypeClass_ContainsAllowWin(TagTypeClass *a1)
    bool ContainsAllowWin() const;
    // asm: char __fastcall TagTypeClass_CrossHorizontalZone(TagTypeClass *a1)
    bool CrossHorizontalZone() const;
    // asm: char __fastcall TagTypeClass_CrossVerticalZone(TagTypeClass *a1)
    bool CrossVerticalZone() const;
    // asm: TagTypeClass_IsWPZoneEntry
    bool IsWPZoneEntry() const;

    // asm: TagTypeClass_PointerGotInvalid(TagTypeClass* this, AbstractClass* gone)
    // 被销毁的实例若正是 Trigger 链头，则把链头前进一格。
    void PointerGotInvalid(AbstractClass* gone);

    // 语义字段（对应 asm Repeating / Trigger / idxInArray）。
    // Repeating 具备 bool 语义：非零即可重复触发。
    TriggerTypeClass* Trigger;
    int32 Repeating;
    int32 idxInArray;
};
