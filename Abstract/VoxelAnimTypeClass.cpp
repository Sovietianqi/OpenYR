#include <Abstract/VoxelAnimTypeClass.h>
#include <Abstract/AbstractClass.h>
#include <Audio/VocClass.h>
#include <Particles/ParticleSystemTypeClass.h>
#include <Animations/AnimTypeClass.h>
#include <Combat/WarheadTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Helpers/AsmMath.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <IO/CCFileClass.h>
#include <Rules/RulesClass.h>

#include <cstring>
#include <cstdio>

// ============================================================================
// VoxelAnimTypeClass.cpp
//
//  asm: VoxelAnimTypeClass_CTOR              0x74AE58
//  asm: VoxelAnimTypeClass_LoadFromINI       0x74B010
//  asm: VoxelAnimTypeClass_GetCRC            0x74B5E0
//  asm: VoxelAnimTypeClass_GetClassID        0x74B6C0
//  asm: VoxelAnimTypeClass_Load              0x74B790
//  asm: VoxelAnimTypeClass_Save              0x74B8D0
//  asm: VoxelAnimTypeClass_PointerGotInvalid 0x74B8F0
//  asm: VoxelAnimTypeClass_FindOrAllocate    0x74B980
//  asm: VoxelAnimTypeClass_GetAbstractDerivationID 0x74B9F0
//  asm: VoxelAnimTypeClass_GetSize           0x74BA00
//  asm: sub_74B620 (GetNameByIndex)          0x74B620
//  asm: sub_74B642 (FindIndex)               0x74B642
//  asm: VoxelAnimTypeClass_SDDTOR            0x74BA80
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<VoxelAnimTypeClass*>* VoxelAnimTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void VoxelAnimTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<VoxelAnimTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<VoxelAnimTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<VoxelAnimTypeClass*>();
    }
}

void VoxelAnimTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<VoxelAnimTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void VoxelAnimTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        VoxelAnimTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// asm: VoxelAnimTypeClass_FindOrAllocate+0x1A..0x64 -- 顺序扫描 vec_VoxelAnimTypes，
// 用 __strcmpi 比较 this+0x24（ID）。
VoxelAnimTypeClass* VoxelAnimTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        VoxelAnimTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

VoxelAnimTypeClass* VoxelAnimTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

// asm: sub_74B642 (0x74B642)
//   push esi/edi ; edi = name ; 空指针返回 -1 ;
//   顺序扫描 vec_VoxelAnimTypes，__strcmpi(name, item+0x24)，命中返回下标，
//   否则 -1。
int32 VoxelAnimTypeClass::FindIndex(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return -1;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        VoxelAnimTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return i;
    }
    return -1;
}

// asm: sub_74B620 (0x74B620)
//   cmp ecx, vec_VoxelAnimTypes.length / jbe ; 越界 -> mov eax, offset Null
//   否则 eax = vec_VoxelAnimTypes.data[ecx] + 0x24 (即 ID 字符串)
const char* VoxelAnimTypeClass::GetNameByIndex(int32 index)
{
    if (Array == nullptr)
        return "Null";
    if (static_cast<uint32>(index) > static_cast<uint32>(Array->Count - 1))
        return "Null";
    if (index < 0 || index >= Array->Count)
        return "Null";

    VoxelAnimTypeClass* item = Array->Items[index];
    return (item != nullptr) ? item->ID : "Null";
}

int32 VoxelAnimTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// ============================================================================
// Constructor
//
// asm: VoxelAnimTypeClass_CTOR (0x74AE58, retn 4)
//   1. call ObjectTypeClass_CTOR(name)
//   2. 初始化派生字段（初值见下，double 由 8 字节立即数对拼出）
//   3. 依次写 4 个 vftable、调用 IRTTITypeInfo::Create_ID
//   4. 写基类的 8 个标志字节（0x22F..0x235）
//   5. 把 this 追加进 vec_Abstracts_1 与 vec_VoxelAnimTypes
//
//   初值（按 asm 的字面量还原）：
//     0x294 = 0            0x295 = 0            0x296 = 0
//     0x298 = 0            0x29C = 0x1E         0x2A0 = 0.8 (double)
//     0x2A8 = 0            0x2B0 = 0.174... (double)
//     0x2B8 = 3.5 (double) 0x2C0 = 5.0 (double) 0x2C8 = 15.0 (double)
//     0x2D0 = 0            0x2D4 = 0            0x2D8 = 0
//     0x2DC = -1           0x2E0 = -1           0x2E4 = 0  0x2E8 = 0
//     0x2EC = 0            0x2F0 = 0            0x2F4 = 0
//     0x2F8 = 0            0x2FC = 0            0x300 = 0
// ============================================================================
VoxelAnimTypeClass::VoxelAnimTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    // 0x2DC / 0x2E0 -> -1
    StartSound     = -1;
    StopSound      = -1;

    // 0x294 / 0x295 / 0x296 -> 0
    Normalized     = false;
    Translucent    = false;
    ShareSource    = false;

    // 0x29C -> 0x1E
    Duration       = 0x1E;
    // 0x298 -> 0
    VoxelIndex     = 0;

    // 0x2A0 -> 0x3FE99999A0000000 = 0.8
    Elasticity          = 0.8;
    // 0x2A8 -> 0
    MinAngularVelocity  = 0.0;
    // 0x2B0 -> 0x3FC656ED1CE64946 = 0.174532925...  (10 度/帧)
    MaxAngularVelocity  = 0.17453292519943295;
    // 0x2B8 -> 0x400C000000000000 = 3.5
    MinZVel             = 3.5;
    // 0x2C0 -> 0x4014000000000000 = 5.0
    MaxZVel             = 5.0;
    // 0x2C8 -> 0x402E000000000000 = 15.0
    MaxXYVel            = 15.0;

    // 0x2D0 -> 0
    IsMeteor       = false;

    // 0x2D4 / 0x2D8 -> 0
    Spawns         = nullptr;
    SpawnCount     = 0;

    // 0x2E4 / 0x2E8 / 0x2EC -> 0
    BounceAnim     = nullptr;
    ExpireAnim     = nullptr;
    TrailerAnim    = nullptr;

    // 0x2F0 / 0x2F4 -> 0
    Damage         = 0;
    DamageRadius   = 0;

    // 0x2F8 / 0x2FC -> 0
    Warhead        = nullptr;
    AttachedSystem = nullptr;

    // 0x300 -> 0
    IsTiberium     = false;

    // ShareSource 共享来源（asm 语义为源 +0xB0..+0xC4 槽对）
    ShareSourceType = nullptr;

    // asm: 把 this 追加进 vec_Abstracts_1 与 vec_VoxelAnimTypes
    if (Array == nullptr)
        Init_Array();
    if (Array != nullptr)
        Array->Add(this);
}

// ============================================================================
// Destructor
//
// asm: VoxelAnimTypeClass_SDDTOR (0x74BA80, retn 4)
//   1. 写 4 个 vftable、call Detach
//   2. 从 vec_Abstracts_1 / vec_VoxelAnimTypes 中按值删除 this
//   3. 若 0x296 (ShareSource) 非零，把基类 0xB0/0xB4 置 0
//   4. call ObjectTypeClass_DTOR；若 arg_0 & 1 则 delete
// ============================================================================
VoxelAnimTypeClass::~VoxelAnimTypeClass()
{
    // asm: 从注册表移除
    if (Array != nullptr)
    {
        int32 idx = -1;
        for (int32 i = 0; i < Array->Count; ++i)
        {
            if (Array->Items[i] == this)
            {
                idx = i;
                break;
            }
        }
        if (idx >= 0)
            Array->Remove(idx);
    }

    // asm: 若 ShareSource 非零，清空基类的 VXL/HVA 装载槽（0xB0/0xB4）。
    //      本项目中基类对应的模型装载指针由 ObjectTypeClass 持有，
    //      语义等价于"共享来源时不释放模型数据"。
    // ObjectTypeClass_DTOR 负责其余基类清理（析构链）。
}

// ============================================================================
// IPersistStream
// ============================================================================

// asm: VoxelAnimTypeClass_GetClassID (0x74B6C0, retn 8)
//   arg_4 == 0 -> 返回 0x80004003 (E_POINTER)
//   否则把 CLSID_VoxelAnimTypeClass 的 16 字节拷进 *pClassID，返回 0。
HRESULT VoxelAnimTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    // 本项目尚未登记逐类型 CLSID 常量表，沿用其它类型类的约定：
    // 以 AbstractType 作为 CLSID.Data1 标识本类。
    std::memset(pClassID, 0, sizeof(CLSID));
    pClassID->Data1 = static_cast<uint32>(AbstractType::VoxelAnimType);
    return S_OK;
}

// asm: VoxelAnimTypeClass_Load (0x74B790, retn 8)
//   call ObjectTypeClass_Load(this, pStm)；失败(<0)直接返回；
//   重贴 4 个接口 vftable；
//   用 SwizzleManager 依次重绑 6 个指针槽：2F8(Warhead) / 2E4(BounceAnim) /
//     2E8(ExpireAnim) / 2EC(TrailerAnim) / 2D4(Spawns) / 2FC(AttachedSystem)；
//   最后 call ObjectTypeClass_Load3DArt(this)；返回 0。
HRESULT VoxelAnimTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    // asm: push pStm / push this / call ObjectTypeClass_Load
    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0)
        return hr;

    // asm: 6 次 SwizzleManagerClass_Swizzle
    //   本项目尚未实现存档指针重绑表；指针槽在 ObjectTypeClass::Load 中
    //   已按序号重建，这里保持槽位归一（失效指针置空）。
    if (Warhead != nullptr && Warhead->IsDead())
        Warhead = nullptr;             // asm: [+0x2F8] swizzle
    if (BounceAnim != nullptr && BounceAnim->IsDead())
        BounceAnim = nullptr;          // asm: [+0x2E4] swizzle
    if (ExpireAnim != nullptr && ExpireAnim->IsDead())
        ExpireAnim = nullptr;          // asm: [+0x2E8] swizzle
    if (TrailerAnim != nullptr && TrailerAnim->IsDead())
        TrailerAnim = nullptr;         // asm: [+0x2EC] swizzle
    if (Spawns != nullptr && Spawns->IsDead())
        Spawns = nullptr;              // asm: [+0x2D4] swizzle
    if (AttachedSystem != nullptr && AttachedSystem->IsDead())
        AttachedSystem = nullptr;      // asm: [+0x2FC] swizzle

    // asm: call ObjectTypeClass_Load3DArt
    ObjectTypeClass::Load3DArt(0);

    return S_OK;   // asm: xor eax,eax
}

// asm: VoxelAnimTypeClass_Save (0x74B8D0, retn 0Ch)
//   push a4 / push pStm / push this / call ObjectTypeClass_Save / retn 0Ch
//   纯转发基类存档例程，自身不写任何字段。
HRESULT VoxelAnimTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0)
        return hr;

    return S_OK;   // asm: xor eax,eax
}

// ============================================================================
// PointerGotInvalid — 失效指针清理
//
// asm: VoxelAnimTypeClass_PointerGotInvalid (0x74B8F0, retn 8)
//   形参：pAbstract（失效对象）、removed（是否被移除，此处 asm 未使用）。
//   逐个比较并清空指向该对象的槽：
//     2F8(Warhead) 2E4(BounceAnim) 2E8(ExpireAnim) 2EC(TrailerAnim)
//     2D4(Spawns)  2FC(AttachedSystem)
// ============================================================================
void VoxelAnimTypeClass::PointerGotInvalid(AbstractClass* pInvalid, bool /*removed*/)
{
    // asm: cmp [ecx+2F8h], eax / jnz -> mov [ecx+2F8h], 0
    if (reinterpret_cast<AbstractClass*>(Warhead) == pInvalid)
        Warhead = nullptr;

    // asm: cmp [ecx+2E4h], eax
    if (reinterpret_cast<AbstractClass*>(BounceAnim) == pInvalid)
        BounceAnim = nullptr;

    // asm: cmp [ecx+2E8h], eax
    if (reinterpret_cast<AbstractClass*>(ExpireAnim) == pInvalid)
        ExpireAnim = nullptr;

    // asm: cmp [ecx+2ECh], eax
    if (reinterpret_cast<AbstractClass*>(TrailerAnim) == pInvalid)
        TrailerAnim = nullptr;

    // asm: cmp [ecx+2D4h], eax
    if (reinterpret_cast<AbstractClass*>(Spawns) == pInvalid)
        Spawns = nullptr;

    // asm: mov esi,[ecx+2FCh] / cmp esi, eax / jnz -> mov [ecx+2FCh], 0
    if (reinterpret_cast<AbstractClass*>(AttachedSystem) == pInvalid)
        AttachedSystem = nullptr;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType VoxelAnimTypeClass::WhatAmI() const
{
    return AbstractType::VoxelAnimType;
}

// asm: VoxelAnimTypeClass_GetSize (0x74BA00) -- mov eax, 308h
int32 VoxelAnimTypeClass::Size() const
{
    return 0x308;
}

// asm: VoxelAnimTypeClass_GetAbstractDerivationID (0x74B9F0) -- mov eax, 2Ah
AbstractType VoxelAnimTypeClass::GetAbstractDerivationID() const
{
    return AbstractType::VoxelAnimType;   // 0x2A == 42
}

// ============================================================================
// Art name accessors
// ============================================================================

// asm: 体素模型的 .VXL 路径由基类 Image 名（ImageTag）拼出：
//   __makepath(dst, "", ImageTag, ".VXL", "") -> CCFileClass -> VoxLib_CTOR
// 本类的字段集里没有独立的 VoxelName，这里返回基类的 Image 名。
const char* VoxelAnimTypeClass::Get_VXL_Name() const
{
    return Image;
}

const char* VoxelAnimTypeClass::Get_HVA_Name() const
{
    // 体素动画不携带独立 HVA 名；沿用 VXL 名。
    return Image;
}

// ============================================================================
// LoadFromINI
//
// asm: VoxelAnimTypeClass_LoadFromINI (0x74B010, retn 4)
//   arg = CCINIClass* ; this = VoxelAnimTypeClass
//   1. INIClass_Reset(pINI)
//   2. call ObjectTypeClass_LoadFromINI(this, pINI)；失败(!al) -> 返回 false
//   3. section = this->ID (esi+0x24)
//   4. 读以下键（键名与 asm 逐字一致，fallback 为字段当前值）：
//      Normalized Translucent IsTiberium IsMeteor
//      Elasticity(Fixed) MinAngularVelocity(Fixed) MaxAngularVelocity(Fixed)
//         —— 角度两键读到的值若 !=0 则 ×(π/180)=dbl_7F65E8 转弧度后存回
//      Duration(Integer) MinZVel(Fixed) MaxZVel(Fixed) MaxXYVel(Fixed)
//      Spawns(String)->FindOrAllocate
//      SpawnCount(Integer) ShareBodyData ShareTurretData ShareBarrelData
//      VoxelIndex(Integer) StartSound(String)->VocClass::FindIndexOfName
//      StopSound(String)->VocClass::FindIndexOfName
//      BounceAnim/ExpireAnim/TrailerAnim(String)->AnimTypeClass::FindOrAllocate
//      Damage(Integer) DamageRadius(Integer)
//      Warhead(String)->WarheadTypeClass::FindOrAllocate
//      AttachedSystem(String)->ParticleSystemTypeClass::FindOrAllocate
//   5. ShareSource = (ShareBodyData|ShareTurretData|ShareBarrelData) != 0
//   6. 若 ShareSource：
//        TechnoType* s = INIClass_FindTechnoTypeByName(section, "ShareSource")
//        若命中：按 ShareBodyData/ShareBarrelData/ShareTurretData 分别把 s 的
//          +0xB0/+0xB4 (body) 、+0xB8/+0xBC (barrel) 、+0xC0/+0xC4 (turret)
//          拷到 this 的 +0xB0/+0xB4；否则清空 this 的 +0xB0/+0xB4
//   7. 否则（非 ShareSource）：拼 Image+".VXL" 路径、释放旧 VoxLib、new VoxLib
//   8. 返回 true
// ============================================================================
bool VoxelAnimTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // asm: call INIClass_Reset —— 原版传入的 INI 由调用方复用时先清空。
    //      本项目的 ReadXxx 直接按 section/key 取值，无需 Reset；
    //      为避免破坏调用方数据，此处不调用 Reset。

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // asm: push ebx / mov ecx,esi / call ObjectTypeClass_LoadFromINI
    if (!ObjectTypeClass::LoadFromINI(pINI))
        return false;

    // ------------------------------------------------------------------
    // 分类标志
    // asm: GetBool(section, "Normalized", fallback=0x294)
    // ------------------------------------------------------------------
    Normalized  = pINI->ReadBool(section, "Normalized",  Normalized);
    // asm: GetBool(section, "Translucent", fallback=0x295)
    Translucent = pINI->ReadBool(section, "Translucent", Translucent);
    // asm: GetBool(section, "IsTiberium", fallback=0x300)
    IsTiberium  = pINI->ReadBool(section, "IsTiberium",  IsTiberium);
    // asm: GetBool(section, "IsMeteor", fallback=0x2D0)
    IsMeteor    = pINI->ReadBool(section, "IsMeteor",    IsMeteor);

    // ------------------------------------------------------------------
    // 物理参数
    // asm: GetFixed(section, "Elasticity", fallback=0x2A0)
    // ------------------------------------------------------------------
    Elasticity = pINI->ReadFixed(section, "Elasticity", Elasticity);

    // asm: GetFixed "MinAngularVelocity" fallback 0.0, init push 0xBFF00000(= -1.0 high dword)
    //   fcom 0 ; 若 != 0 -> fmul dbl_7F65E8 (π/180) ; 存入 0x2A8
    {
        double v = pINI->ReadFixed(section, "MinAngularVelocity", 0.0);
        if (v != 0.0)
            MinAngularVelocity = v * (3.14159265358979323846 / 180.0);
    }
    // asm: 同上，键 "MaxAngularVelocity" -> 0x2B0
    {
        double v = pINI->ReadFixed(section, "MaxAngularVelocity", 0.0);
        if (v != 0.0)
            MaxAngularVelocity = v * (3.14159265358979323846 / 180.0);
    }

    // asm: GetInteger(section, "Duration", fallback=0x29C)
    Duration = pINI->ReadInteger(section, "Duration", Duration);
    // asm: GetFixed "MinZVel" fallback 0x2B8
    MinZVel  = pINI->ReadFixed(section, "MinZVel",  MinZVel);
    // asm: GetFixed "MaxZVel" fallback 0x2C0
    MaxZVel  = pINI->ReadFixed(section, "MaxZVel",  MaxZVel);
    // asm: GetFixed "MaxXYVel" fallback 0x2C8
    MaxXYVel = pINI->ReadFixed(section, "MaxXYVel", MaxXYVel);

    // asm: GetString(section, "Spawns", "", buf, 0x80)
    //   命中 -> sub_74BB20(buf, &vec_VoxelAnimTypes)（= FindOrAllocate），
    //   否则保持旧值。
    {
        char buf[0x80];
        if (pINI->ReadString(section, "Spawns", "", buf, sizeof(buf)) > 0)
        {
            VoxelAnimTypeClass* p = VoxelAnimTypeClass::FindOrAllocate(buf);
            if (p != nullptr)
                Spawns = p;
        }
    }

    // asm: GetInteger(section, "SpawnCount", fallback=0x2D8)
    SpawnCount = pINI->ReadInteger(section, "SpawnCount", SpawnCount);

    // asm: GetBool(section, "ShareBodyData",   0)
    const bool shareBody   = pINI->ReadBool(section, "ShareBodyData",   false);
    // asm: GetBool(section, "ShareTurretData", 0)
    const bool shareTurret = pINI->ReadBool(section, "ShareTurretData", false);
    // asm: GetBool(section, "ShareBarrelData", 0)
    const bool shareBarrel = pINI->ReadBool(section, "ShareBarrelData", false);

    // asm: GetInteger(section, "VoxelIndex", fallback=0x298)
    VoxelIndex = pINI->ReadInteger(section, "VoxelIndex", VoxelIndex);

    // asm: GetString(section, "StartSound", "", buf, 0x80)
    //   命中 -> VocClass_FindIndexOfName(buf) ; == -1 则保持旧值
    {
        char buf[0x80];
        if (pINI->ReadString(section, "StartSound", "", buf, sizeof(buf)) > 0)
        {
            int32 idx = VocClass::FindIndexOfName(buf);
            if (idx != -1)
                StartSound = idx;
        }
    }
    // asm: GetString(section, "StopSound", "", buf, 0x80)
    {
        char buf[0x80];
        if (pINI->ReadString(section, "StopSound", "", buf, sizeof(buf)) > 0)
        {
            int32 idx = VocClass::FindIndexOfName(buf);
            if (idx != -1)
                StopSound = idx;
        }
    }

    // asm: GetString(section, "BounceAnim", "", buf, 0x80)
    //   命中 -> AnimTypeClass_FindOrAllocate(buf)
    {
        char buf[0x80];
        if (pINI->ReadString(section, "BounceAnim", "", buf, sizeof(buf)) > 0)
        {
            AnimTypeClass* p = AnimTypeClass::FindOrAllocate(buf);
            if (p != nullptr)
                BounceAnim = p;
        }
    }
    // asm: GetString(section, "ExpireAnim", "", buf, 0x80)
    {
        char buf[0x80];
        if (pINI->ReadString(section, "ExpireAnim", "", buf, sizeof(buf)) > 0)
        {
            AnimTypeClass* p = AnimTypeClass::FindOrAllocate(buf);
            if (p != nullptr)
                ExpireAnim = p;
        }
    }
    // asm: GetString(section, "TrailerAnim", "", buf, 0x80)
    {
        char buf[0x80];
        if (pINI->ReadString(section, "TrailerAnim", "", buf, sizeof(buf)) > 0)
        {
            AnimTypeClass* p = AnimTypeClass::FindOrAllocate(buf);
            if (p != nullptr)
                TrailerAnim = p;
        }
    }

    // asm: GetInteger(section, "Damage", fallback=0x2F0)
    Damage = pINI->ReadInteger(section, "Damage", Damage);
    // asm: GetInteger(section, "DamageRadius", fallback=0x2F4)
    DamageRadius = pINI->ReadInteger(section, "DamageRadius", DamageRadius);

    // asm: GetString(section, "Warhead", "", buf, 0x80)
    //   命中 -> WarheadTypeClass_FindOrAllocate(buf)
    {
        char buf[0x80];
        if (pINI->ReadString(section, "Warhead", "", buf, sizeof(buf)) > 0)
        {
            WarheadTypeClass* p = WarheadTypeClass::FindOrAllocate(buf);
            if (p != nullptr)
                Warhead = p;
        }
    }
    // asm: GetString(section, "AttachedSystem", "", buf, 0x80)
    //   命中 -> ParticleSystemTypeClass_FindOrAllocate(buf)
    {
        char buf[0x80];
        if (pINI->ReadString(section, "AttachedSystem", "", buf, sizeof(buf)) > 0)
        {
            ParticleSystemTypeClass* p = ParticleSystemTypeClass::FindOrAllocate(buf);
            if (p != nullptr)
                AttachedSystem = p;
        }
    }

    // asm: ShareSource(0x296) = (shareTurret | shareBarrel | shareBody) ? 1 : 0
    //      注意 asm 判定顺序：先 ShareTurretData(var_281)、再 ShareBarrelData
    //      (var_283)、再 ShareBodyData(var_282)，任一为真即置 1。
    ShareSource = (shareTurret || shareBarrel || shareBody);

    if (ShareSource)
    {
        // asm: INIClass_FindTechnoTypeByName(section, "ShareSource")
        //   把命中的 TechnoType 作为模型数据来源。asm 逐字复制源类型
        //   +0xB0/+0xB4（body）、+0xB8/+0xBC（barrel）、+0xC0/+0xC4（turret）
        //   三对槽中的一对到 this 的 +0xB0/+0xB4。本项目以"记录共享来源类型"
        //   等价承载该语义（源类型随即提供 VXL/HVA 模型数据）。
        TechnoTypeClass* pSource =
            pINI->FindTechnoTypeByName(section, "ShareSource");

        if (pSource != nullptr)
        {
            // asm 的 Share*Data 择优顺序：
            //   ShareBodyData -> body   (源 +0xB0/+0xB4)
            //   否则 ShareBarrelData -> barrel (源 +0xB8/+0xBC)
            //   否则 ShareTurretData -> turret (源 +0xC0/+0xC4)
            if (shareBody || shareBarrel || shareTurret)
                ShareSourceType = pSource;
        }
        else
        {
            // asm: loc_74B555 —— 未命中，把 this+0xB0/+0xB4 清 0
            ShareSourceType = nullptr;
        }
    }
    else
    {
        // asm: loc_74B56C —— 拼 "Image.VXL" 路径，释放旧 VoxLib，new VoxLib。
        //   本项目体素模型的装载统一由基类 ObjectTypeClass::Load3DArt 承担，
        //   这里清除共享来源，回到自持模型路径。
        ShareSourceType = nullptr;
    }

    return true;
}

// ============================================================================
// CRC
//
// asm: VoxelAnimTypeClass_GetCRC (0x74B5E0, retn 4)
//   先 AbstractTypeClass_GetCRC(buf)，再按顺序追加：
//     bool 294 / bool 295 / bool 296 / dword 298 / dword 29C /
//     double 2A0 / 2A8 / 2B0 / 2B8 / 2C0 / 2C8 /
//     bool 2D0 / dword 2D8 / dword 2DC / dword 2E0 /
//     dword 2F0 / dword 2F4 / bool 300
//   注意：指针字段（2D4/2E4/2E8/2EC/2F8/2FC）不参与 CRC。
// ============================================================================
void VoxelAnimTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    // bool ×3
    crc.AddData(&Normalized,  sizeof(Normalized));   // 294
    crc.AddData(&Translucent, sizeof(Translucent));  // 295
    crc.AddData(&ShareSource, sizeof(ShareSource));  // 296

    // dword ×2
    crc.AddData(&VoxelIndex,  sizeof(VoxelIndex));   // 298
    crc.AddData(&Duration,    sizeof(Duration));     // 29C

    // double ×6
    crc.AddData(&Elasticity,          sizeof(Elasticity));          // 2A0
    crc.AddData(&MinAngularVelocity,  sizeof(MinAngularVelocity));  // 2A8
    crc.AddData(&MaxAngularVelocity,  sizeof(MaxAngularVelocity));  // 2B0
    crc.AddData(&MinZVel,             sizeof(MinZVel));             // 2B8
    crc.AddData(&MaxZVel,             sizeof(MaxZVel));             // 2C0
    crc.AddData(&MaxXYVel,            sizeof(MaxXYVel));            // 2C8

    // bool + dword ×3
    crc.AddData(&IsMeteor,    sizeof(IsMeteor));     // 2D0
    crc.AddData(&SpawnCount,  sizeof(SpawnCount));   // 2D8
    crc.AddData(&StartSound,  sizeof(StartSound));   // 2DC
    crc.AddData(&StopSound,   sizeof(StopSound));    // 2E0

    // dword ×2 + bool
    crc.AddData(&Damage,       sizeof(Damage));       // 2F0
    crc.AddData(&DamageRadius, sizeof(DamageRadius)); // 2F4
    crc.AddData(&IsTiberium,   sizeof(IsTiberium));   // 300
}

int32 VoxelAnimTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// FindOrAllocate
//
// asm: VoxelAnimTypeClass_FindOrAllocate (0x74B980, __fastcall, name=ecx)
//   1. __strcmpi(name, "<none>") == 0 -> 返回 0
//   2. __strcmpi(name, "none")   == 0 -> 返回 0
//   3. 顺序扫描 vec_VoxelAnimTypes，命中返回该项
//   4. 未命中：new 0x308 字节，call CTOR(name)，返回新项
// ============================================================================
VoxelAnimTypeClass* VoxelAnimTypeClass::FindOrAllocate(const char* pID)
{
    // asm: __strcmpi(name, "<none>") / __strcmpi(name, "none")
    if (pID == nullptr)
        return nullptr;
    if (!_strcmpi(pID, "<none>") || !_strcmpi(pID, "none"))
        return nullptr;

    // asm: 顺序扫描
    if (Array != nullptr)
    {
        VoxelAnimTypeClass* found = Find(pID);
        if (found != nullptr)
            return found;
    }

    // asm: push 308h / call new / push name / call VoxelAnimTypeClass_CTOR
    //   构造器内部已把新项追加进 vec_VoxelAnimTypes，此处不再重复 Add。
    if (Array == nullptr)
        Init_Array();

    return GameCreate<VoxelAnimTypeClass>(pID);
}

// ============================================================================
// Voxel 法线表（asm 全局 NormalTables / NormalTableLengths /
// Voxel_NormalizedNodes）。原版直接寻址引擎 .data；此处用表装载接口注入，
// 量化逻辑与 asm 保持逐句一致。
//
// asm: Voxel_ProcessNormalsTables (0x758680)
//
//   mov eax,NormalTables[edx*4] / mov ebp,NormalTableLengths[edx*4]
//   xor esi,esi / test ebp,ebp / jle done
//   lea edi,[eax+4]
// loc_75868D:
//   fld [edi-4] / fmul [ebx]          ; x * s0
//   fld [edi+4] / fmul [ebx+8]        ; z * s2
//   faddp                             ; x*s0 + z*s2
//   fld [ebx+4] / fmul [edi]          ; y * s1
//   faddp                             ; + y*s1
//   fcom ds:double_0 / fnstsw ax / test ah,1
//   jz loc_7586B9                     ; sum >= 0 -> 量化
//   fstp st                           ; 丢弃
//   mov Voxel_NormalizedNodes[esi],0
//   jmp loc_7586CA
// loc_7586B9:
//   fmul ds:num_16 / call Float_To_Int_Floor
//   mov Voxel_NormalizedNodes[esi],al
// loc_7586CA:
//   inc esi / add edi,0Ch / cmp esi,ebp / jl loc_75868D
// done (loc_7586D3):
//   mov al,10h / mov Voxel_NormalizedNodes+0FFh,al
//              / mov Voxel_NormalizedNodes+0FEh,al
//              / mov Voxel_NormalizedNodes+0FDh,al
// ============================================================================
namespace {
constexpr int32 kVoxelNormalTableMax = 8;
const float* g_VoxelNormalTables[kVoxelNormalTableMax] = {};
int32        g_VoxelNormalLengths[kVoxelNormalTableMax] = {};
uint8        g_VoxelNormalizedNodes[0x100] = {};
}  // namespace

void VoxelAnimTypeClass::Set_Normal_Table(int32 idx, const float* pData,
                                          int32 length)
{
    if (idx < 0 || idx >= kVoxelNormalTableMax)
        return;
    g_VoxelNormalTables[idx]  = pData;
    g_VoxelNormalLengths[idx] = length;
}

int32 VoxelAnimTypeClass::Get_Normal_Table_Count()
{
    return kVoxelNormalTableMax;
}

uint8 VoxelAnimTypeClass::Get_Voxel_Normalized_Node(int32 idx)
{
    if (idx < 0 || idx >= 0x100)
        return 0;
    return g_VoxelNormalizedNodes[idx];
}

void VoxelAnimTypeClass::Process_Normals_Tables(const float* pScale,
                                                int32 tableIdx)
{
    if (tableIdx < 0 || tableIdx >= kVoxelNormalTableMax)
        return;

    // asm: mov eax,NormalTables[edx*4] / mov ebp,NormalTableLengths[edx*4]
    const float* pEntries = g_VoxelNormalTables[tableIdx];
    const int32  length   = g_VoxelNormalLengths[tableIdx];

    if (pScale == nullptr || pEntries == nullptr)
        return;

    // asm: xor esi,esi / test ebp,ebp / jle done
    for (int32 i = 0; i < length; ++i) {             // asm: loc_75868D
        // Entry base: edi walks [data+4] then +0Ch each round，所以三元组是
        // { [edi-4]=x, [edi]=y, [edi+4]=z }。
        const float* e = pEntries + i * 3;

        // asm FPU 求和顺序：x*s0 + z*s2 + y*s1
        const float sum = e[0] * pScale[0] + e[2] * pScale[2]
                        + e[1] * pScale[1];

        // asm: fcom ds:double_0 / fnstsw ax / test ah,1
        // C0 置位（sum < 0）-> 存 0，否则量化。
        if (sum < 0.0f) {
            g_VoxelNormalizedNodes[i] = 0;           // asm: mov [esi],0
            continue;
        }

        // asm: fmul ds:num_16 / call Float_To_Int_Floor
        const double scaled = static_cast<double>(sum) * 16.0;
        const int64  q      = Float_To_Int_Floor(scaled);

        g_VoxelNormalizedNodes[i] = static_cast<uint8>(q);   // asm: mov al
    }

    // asm: mov al,10h ; nodes[0xFF] = [0xFE] = [0xFD] = 16
    g_VoxelNormalizedNodes[0xFF] = 0x10;
    g_VoxelNormalizedNodes[0xFE] = 0x10;
    g_VoxelNormalizedNodes[0xFD] = 0x10;
}

// ============================================================================
// asm: Voxel_ProcessNormalsTables_Alt (真实入口 0x758724，retn 8)
//
//   ecx = a（源法线 float[3]）; edx = d（增量 float[3]）;
//   栈 arg_0 = k（偏振系数 float）; 栈 arg_4 = tableIdx（int32）
//
//   与 Process_Normals_Tables 共用同一张法线表与同一个输出缓冲，区别在于
//   它先把 a 与 d 相加并归一化成 v，再对每个法线项同时算两个点积：
//   dotA 用原始 a、dotV 用归一化的 v，最后把 dotA 的正面分量与 dotV 的
//   偏振比值相加后量化。
//
//   逐句对照见头文件；下面按 asm 的 FPU 语句逐条落位（含 fdivr/fsubr
//   这两个反向运算的换算）。
// ============================================================================
void VoxelAnimTypeClass::Process_Normals_Tables_Alt(const float* pNormal,
                                                    const float* pDelta,
                                                    float k, int32 tableIdx)
{
    if (tableIdx < 0 || tableIdx >= kVoxelNormalTableMax)
        return;
    if (pNormal == nullptr || pDelta == nullptr)
        return;

    // asm: fld [ebp+0] / fadd [edx] / fstp var_24 等三条 —— v = a + d
    float vx = pNormal[0] + pDelta[0];
    float vy = pNormal[1] + pDelta[1];
    float vz = pNormal[2] + pDelta[2];

    // asm: fld var_1C / fmul var_1C ... faddp ... fstp var_3C / call float_sqrt
    //      求和顺序：vx*vx + vy*vy + vz*vz
    const float lenSq = vx * vx + vy * vy + vz * vz;
    const float len   = std::sqrt(lenSq);

    // asm: fcom ds:double_0 / fnstsw ax / test ah,40h / jnz loc_758786
    //      ah&40h 是 C3（相等）位 —— 长度等于 0 时跳过归一化，直接用 v。
    if (len != 0.0f) {
        // asm: fld var_24 / fdiv st,st(1) / fstp var_C  —— 三条 fdiv，除以 len
        vx = vx / len;
        vy = vy / len;
        vz = vz / len;
    }

    // asm: mov esi,NormalTables[ebx*4] / mov ebx,NormalTableLengths[ebx*4]
    const float* pEntries = g_VoxelNormalTables[tableIdx];
    const int32  length   = g_VoxelNormalLengths[tableIdx];
    if (pEntries == nullptr) {
        return;
    }

    for (int32 i = 0; i < length; ++i) {   // asm: loc_7587B4，每次 esi += 0Ch
        const float* e = pEntries + i * 3;

        // asm: fld [esi+4]/fmul [ebp+8] ; fld [esi]/fmul [ebp+4] ; faddp ;
        //      fld [esi-4]/fmul [ebp+0] ; faddp
        //      —— 求和顺序：e.z*a.z + e.y*a.y + e.x*a.x
        const float dotA = e[2] * pNormal[2] + e[1] * pNormal[1]
                         + e[0] * pNormal[0];

        // asm: fld var_18(归一化 v.x)/fmul [esi-4] ;
        //      fld var_14(v.y)/fmul [esi] ; faddp ;
        //      fld var_10(v.z)/fmul [esi+4] ; faddp
        //      —— 求和顺序：v.x*e.x + v.y*e.y + v.z*e.z
        const float dotV = vx * e[0] + vy * e[1] + vz * e[2];

        // asm: fld st / fmul arg_0 / fsubr arg_0 / fadd st,st(1) / fdivr st,st(1)
        //      fsubr 是 "arg_0 - st"，fdivr 是 "st / 被除者"，逐条落位：
        //        t0 = k * dotV
        //        t1 = k - t0                 (fsubr: k - t0)
        //        t2 = t1 + t0
        //        q  = dotV / t2              (fdivr: 以 dotV 为分子)
        float q = dotV / ((k - k * dotV) + dotV);

        // asm: fcomp ds:num_0_0 / fnstsw ax / test ah,1 / jz ; mov arg_4,0
        if (q < 0.0f) {
            q = 0.0f;
        }

        // asm: fcom ds:num_0_0 / fnstsw ax / test ah,1 / jz ; fstp st / fld 0
        //      —— 取 dotA 的正面分量
        const float dotAPos = (dotA < 0.0f) ? 0.0f : dotA;

        // asm: fadd arg_4  —— val = max(dotA, 0) + q
        const float val = dotAPos + q;

        // asm: fcom ds:double_0 / fnstsw ax / test ah,1 / jz
        if (val < 0.0f) {
            // asm: fstp st / mov Voxel_NormalizedNodes[edi],0
            g_VoxelNormalizedNodes[i] = 0;
        } else {
            // asm: fmul ds:num_16 / call Float_To_Int_Floor / mov [edi],al
            const double scaled = static_cast<double>(val) * 16.0;
            const int64  q16    = Float_To_Int_Floor(scaled);
            g_VoxelNormalizedNodes[i] = static_cast<uint8>(q16);
        }
    }

    // asm: mov al,10h ; nodes[0xFF] = [0xFE] = [0xFD] = 16
    g_VoxelNormalizedNodes[0xFF] = 0x10;
    g_VoxelNormalizedNodes[0xFE] = 0x10;
    g_VoxelNormalizedNodes[0xFD] = 0x10;
}
