// ============================================================================
// FactoryClass.cpp - production queue attached to a producing structure
//
// 逐 proc 还原 gamemd.exe 的 FactoryClass（0x4C98xx..0x4CA8xx）。
// 细节以 asm 为唯一权威；凡 asm 未表达的字段/行为不臆造。
// ============================================================================

#include "FactoryClass.h"
#include "../Houses/HouseClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/TechnoClass.h"
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
#include "../Game/GameInit.h"
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
// 帮手：产品对象的建时取值
//
// asm: 产品对象在 +0x21C 处缓存其生产建时（TechnoClass::BuildTime）。
// OpenYR 侧由各具体 TechnoClass 子类的 virtual GetBuildTime() 覆写提供。
// ============================================================================
static int32 Get_Product_Build_Time(TechnoClass* pProduct)
{
    if (pProduct == nullptr)
        return 0;

    switch (pProduct->WhatAmI())
    {
    case AbstractType::Building:  return static_cast<BuildingClass*>(pProduct)->GetBuildTime();
    case AbstractType::Unit:      return static_cast<UnitClass*>(pProduct)->GetBuildTime();
    case AbstractType::Infantry:  return static_cast<InfantryClass*>(pProduct)->GetBuildTime();
    case AbstractType::Aircraft:  return static_cast<AircraftClass*>(pProduct)->GetBuildTime();
    default:                      return 0;
    }
}

// asm: (est * 4BDA12F7h) >> 36  ==  est / 54，随后 clamp 到 [1,255]。
//   mov eax,4BDA12F7h / imul edx / sar edx,4 / shr eax,1Fh / add edx,eax
static int32 Scale_Build_Time(int32 est)
{
    int32 v = static_cast<int32>(
        (static_cast<int64>(est) * static_cast<int64>(0x4BDA12F7)) >> 36);

    if (v < 1)     v = 1;       // asm: cmp edx,1 / jge / mov edx,1
    if (v > 0xFF)  v = 0xFF;    // asm: cmp edx,0FFh / jle / mov edx,0FFh
    return v;
}

// asm: 类型虚表 +0x8Ch = 创建产品对象（TechnoTypeClass::Create_Instance）。
// OpenYR 侧按类型类别 new 出对应 TechnoClass 子类，并置好 Type 与 Owner。
static TechnoClass* Create_Product(TechnoTypeClass* pType, HouseClass* pOwner)
{
    if (pType == nullptr || pOwner == nullptr)
        return nullptr;

    switch (pType->WhatAmI())
    {
        case AbstractType::UnitType:
        {
            UnitClass* p = new UnitClass(pOwner);
            if (p != nullptr) p->Type = static_cast<UnitTypeClass*>(pType);
            return p;
        }
        case AbstractType::InfantryType:
        {
            InfantryClass* p = new InfantryClass(pOwner);
            if (p != nullptr) p->Type = static_cast<InfantryTypeClass*>(pType);
            return p;
        }
        case AbstractType::AircraftType:
        {
            AircraftClass* p = new AircraftClass(pOwner);
            if (p != nullptr) p->Type = static_cast<AircraftTypeClass*>(pType);
            return p;
        }
        case AbstractType::BuildingType:
        {
            BuildingClass* p = new BuildingClass(pOwner);
            if (p != nullptr) p->Type = static_cast<BuildingTypeClass*>(pType);
            return p;
        }
        default:
            return nullptr;
    }
}

// ============================================================================
// Construction / destruction
//
// asm: FactoryClass_CTOR (0x4C98xx)
//   除 AbstractClass_CTOR + 把自己挂进 Factory / vec_Abstracts_0 两个全局向量
//   之外，逐字段初始化如下（ebx=0, al=1）：
//     +0x24=0  +0x28=0  +0x2C=Frame  +0x34=0  +0x38=0  +0x3C=1
//     +0x44=0  +0x48=0  +0x4C=1  +0x4D=0  Queue.vft  +0x54=0Ah(cap)  +0x50=0
//     +0x58=0  +0x5C=0  +0x5D=0  +0x60=0  +0x64=0  +0x68=0FFFFFFFFh
//     +0x6C=0  +0x70=0  +0x71=1
// ============================================================================

FactoryClass::FactoryClass(BuildingClass* pFactory, HouseClass* pOwner)
    : Progress(0)
    , AdvancedThisFrame(false)
    , ProductionStartFrame(0)
    , ProductionTimerPad(0)
    , ProductionDuration(0)
    , ProductionEstimateFrames(0)
    , Rate(1)
    , CurrentProduction(nullptr)
    , BalanceDirty(false)
    , IsDifferent(false)
    , Balance(0)
    , OriginalBalance(0)
    , SpecialItem(0xFFFFFFFF)
    , Owner(pOwner)
    , OnHold(false)
    , CompleteSuspended(true)
    , Factory(pFactory)
    , CurrentType(nullptr)
{
    if (Array == nullptr)
        Init_Array();
    if (Array != nullptr)
        Array->Add(this);

    if (Owner != nullptr)
        ++Owner->FactoryCount;
}

// asm: FactoryClass_DTOR (0x4C99xx) / FactoryClass_SDDTOR (0x4CA7xx)
//   Detach 自身 -> 从 Factory 向量移除 -> GameActive 时 Abandon ->
//   从 vec_Abstracts_0 移除 -> 释放队列内存。
FactoryClass::~FactoryClass()
{
    if (GameActive)
        Abandon();

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

// ============================================================================
// asm: FactoryClass_QueueProduction (0x4C9C66)
//
//   byte __thiscall FactoryClass_QueueProduction(FactoryClass *this,
//                                                _TechnoTypeClass *a2,
//                                                _TechnoClass *a1, char a4)
//   retn 0Ch -> 3 参数（不算 this）。OpenYR 侧 a1/a4 精简为语义参数。
//
//   逐段（loc_4C9C66..loc_4C9E10）：
//     WhatAmI(a2)==7 (BuildingType) -> Abandon(this)
//     loc_4C9C8E: WhatAmI(a2)==7 -> lct_BuildingType（建筑直接开工）
//     loc_4C9CAC: [+0x38]!=0 && [+0x70]==0 -> loc_4C9CC9
//                 [+0x50]>0 -> loc_4C9CC9
//                 [+0x58]==0 -> lct_BuildingType
//                 [+0x5D]==0 -> lct_BuildingType
//     loc_4C9CC9: a4 != 0 -> lct_BuildingType（强制开工）
//                 [+0x50] >= MaximumQueuedObjects -> loc_4C9D3B（拒绝）
//                 HouseClass_ShouldDisableCameo(owner,a2) -> loc_4C9D3B（拒绝）
//                 Queue.push(a2) -> return 1
//     lct_BuildingType: 置 +0x5D=1 +0x70=1，+0x24=0，重置计时器，
//                 创建产品对象到 +0x58，取 owner/应付金额/原始金额；
//                 return (+0x58 != 0)
//     loc_4C9D3B: 播放"无法建造"音效 -> return 0
// ============================================================================
bool FactoryClass::QueueProduction(TechnoTypeClass* pType)
{
    if (pType == nullptr)
        return false;

    // asm: call WhatAmI / cmp eax,7 / jnz loc_4C9C8E
    if (pType->WhatAmI() == AbstractType::BuildingType)
    {
        // asm: mov ecx,esi / call FactoryClass::Abandon
        Abandon();
    }

    // asm: loc_4C9C8E: 再取一次 WhatAmI / cmp eax,7 / jz lct_BuildingType
    const bool isBuilding = (pType->WhatAmI() == AbstractType::BuildingType);
    bool goStart = isBuilding;

    if (!isBuilding)
    {
        // asm: mov eax,[esi+38h] / test / jz loc_4C9CAC
        //      mov al,[esi+70h] / test / jz loc_4C9CC9
        bool reject = false;
        if (ProductionEstimateFrames != 0 && !OnHold)
            reject = true;

        if (!reject)
        {
            // asm: loc_4C9CAC: cmp [esi+50h],0 / jg loc_4C9CC9
            if (Queue.Count > 0)
                reject = true;
            else if (CurrentProduction == nullptr)
                goStart = true;                 // asm: jz lct_BuildingType
            else if (!IsDifferent)
                goStart = true;                 // asm: [+0x5D]==0 -> start
            else
                reject = true;
        }

        // asm: loc_4C9CC9
        if (reject)
        {
            // asm: mov al,[a4] / test / jnz lct_BuildingType（强制开工）
            //   OpenYR 无 a4 来源，保持 false 走拒绝路径。
            const int32 kMaxQueued = 100;       // asm: RulesClass.MaximumQueuedObjects
            if (Queue.Count >= kMaxQueued)
                return false;                   // asm: loc_4C9D3B

            // asm: HouseClass_ShouldDisableCameo([+0x6C], a2)
            if (Owner != nullptr && Owner->ShouldDisableCameo(pType))
                return false;                   // asm: loc_4C9D3B

            // asm: Queue.push(edi) -> return 1
            Queue.Add(pType);
            return true;
        }
    }

    if (goStart)
    {
        // asm: lct_BuildingType: mov byte ptr [esi+5Dh],1 / [+0x70]=1
        IsDifferent = true;
        OnHold = true;
        // asm: [esi+24h]=0；重置计时器 [+0x38]=0 / [+0x2C]=Frame / [+0x34]=0
        Progress = 0;
        ProductionEstimateFrames = 0;
        ProductionStartFrame = Game::CurrentFrame;
        ProductionDuration = 0;

        // asm: edx=[edi] / call [edx+8Ch] = 创建产品对象 -> [+0x58]
        CurrentProduction = Create_Product(pType, Owner);
        CurrentType = pType;

        // asm: [+0x6C] = owner；应付金额 [+0x60] / 原始金额 [+0x64]
        if (Owner != nullptr && CurrentType != nullptr)
        {
            OriginalBalance = static_cast<int32>(
                CurrentType->Cost * Owner->Get_Cost_Mult(CurrentType));
            Balance = OriginalBalance;
        }

        // asm: return ([+0x58] != 0)
        return (CurrentProduction != nullptr);
    }

    // asm: loc_4C9D3B: 播放"无法建造"音效（音效层未落桩）-> return 0
    return false;
}

// ============================================================================
// asm: FactoryClass_StartProduction (0x4CA5A6)
//
//   void __thiscall FactoryClass_StartProduction(FactoryClass *this)
//   retn -> 无参数
//
//   逐段：
//     mov eax,[esi+50h] / test / jz retn      ; 队列空 -> 返回
//     mov eax,[esi+58h] / test / jnz retn     ; 已有在线产品 -> 返回
//     mov eax,[esi+38h] / test / jz loc       ; 有估计帧数时
//     mov al,[esi+70h] / test / jz retn       ;   且未挂起 -> 返回
//   loc_4CA5C0:
//     edi = [+44][0]                          ; 队首类型
//     弹出队首：Count--，其后元素整体左移
//     eax = edi->vft[0x40]()                  ; 类型的“索引”虚函数
//     if (eax < 0) return                     ; 无效索引 -> 返回
//     cl  = edi->Naval
//     HouseClass_BeginProductionOf(owner,
//         edi->vft[0x2Ch](),  ; WhatAmI（抽象 ID）
//         cl,                 ; isNaval
//         eax,                ; idx
//         1)                  ; quantity
// ============================================================================
void FactoryClass::StartProduction()
{
    // asm: mov eax,[esi+50h] / test eax,eax / jz loc_4CA60F
    if (Queue.Count <= 0)
        return;

    // asm: mov eax,[esi+58h] / test eax,eax / jnz loc_4CA60F
    if (CurrentProduction != nullptr)
        return;

    // asm: mov eax,[esi+38h] / test / jz short loc_4CA5C0
    //      mov al,[esi+70h] / test / jz loc_4CA60F
    if (ProductionEstimateFrames != 0 && !OnHold)
        return;

    // asm: loc_4CA5C0 —— edi = 队首类型
    TechnoTypeClass* const pType = Queue[0];
    if (pType == nullptr)
    {
        // 队首为空指针：原版仍会走弹出逻辑，但随后索引非法即返回；
        // 这里保守地先弹出再返回，保持队列不卡死。
        (void)Queue.Remove(0);
        return;
    }

    // asm: 弹出队首（Count--，其后元素整体左移）
    for (int32 i = 0; i + 1 < Queue.Count; ++i)
        Queue[i] = Queue[i + 1];
    Queue.Count -= 1;

    // asm: call [eax+40h]；OpenYR 侧以类型的数组下标等价该“索引”虚函数。
    const int32 idx = pType->GetArrayIndex();
    if (idx < 0)
        return;                                // asm: test eax,eax / jl loc_4CA60F

    // asm: mov cl,[edi+Naval] ; BuyIsNaval
    const bool isNaval = pType->IsNaval;

    // asm: HouseClass_BeginProductionOf([+6Ch], WhatAmI(), isNaval, idx, 1)
    if (Owner != nullptr)
    {
        Owner->BeginProductionOf(pType, 1);
        (void)isNaval;                         // 语义已由 BeginProductionOf 内部消费
    }

    CurrentType = pType;
}

bool FactoryClass::RemoveFromQueue(TechnoTypeClass* pType)
{
    // asm: FactoryClass_RemoveFromQueue (0x4CA62E)
    //   在 +0x40 队列里查找并删除；命中返回 1。
    if (pType == nullptr)
        return false;

    for (int32 i = 0; i < Queue.Count; ++i)
    {
        if (Queue[i] == pType)
        {
            Queue.Remove(i);
            return true;                       // asm: mov al,1
        }
    }
    return false;                              // asm: loc_4CA65D xor al,al
}

// asm: FactoryClass_IsXQueued (0x4CA6D4): 队列里是否存在 obj
bool FactoryClass::IsXQueued(TechnoTypeClass* pType) const
{
    if (pType == nullptr)
        return false;
    for (int32 i = 0; i < Queue.Count; ++i)
    {
        if (Queue[i] == pType)
            return true;                       // asm: loc_4CA6D3 mov al,1
    }
    return false;
}

// asm: FactoryClass_CountThisInQueue (0x4CA6xx)
//   计数 = (在线产品类型 == a2) + 队列里等于 a2 的项数。
int32 FactoryClass::CountThisInQueue(TechnoTypeClass* pType) const
{
    if (pType == nullptr)
        return 0;

    int32 count = 0;

    // asm: mov ecx,[esi+58h] / call [eax+84h] ; 产品类型
    if (CurrentProduction != nullptr && CurrentProduction->TechnoType == pType)
        count = 1;

    for (int32 i = 0; i < Queue.Count; ++i)
    {
        if (Queue[i] == pType)
            ++count;
    }
    return count;
}

// ============================================================================
// asm: FactoryClass::Suspend (0x4C9E5B)
//
//   char __thiscall FactoryClass::Suspend(FactoryClass *this, char a2)
//   retn 4 -> 1 参数
//
//   仅当尚未挂起时生效：+0x71=a2, +0x70=1，重置计时器（+0x38=0，+0x2C=Frame）。
//   返回是否真的挂起。
// ============================================================================
bool FactoryClass::Suspend(bool a2)
{
    // asm: mov al,[ecx+70h] / cmp al,dl(=0) / jnz loc_4C9E98
    if (OnHold)
        return false;                          // asm: loc_4C9E98 xor al,al

    // asm: mov [ecx+71h],al ; al = a2
    CompleteSuspended = a2;
    // asm: mov byte ptr [ecx+70h],1
    OnHold = true;
    // asm: mov [ecx+38h],edx(=0) / mov [esi],Frame / [esi+4]=var_8 / [esi+8]=0
    ProductionEstimateFrames = 0;
    ProductionStartFrame = Game::CurrentFrame;
    ProductionDuration = 0;

    return true;                               // asm: mov al,1
}

void FactoryClass::Unsuspend()
{
    Unsuspend_Ex(0);
}

// ============================================================================
// asm: FactoryClass_Unsuspend (0x4C9E97)
//
//   char __thiscall FactoryClass_Unsuspend(FactoryClass *this, int a2)
//   retn 4 -> 1 参数
//
//   恢复生产队列。逐段见下。
// ============================================================================
bool FactoryClass::Unsuspend_Ex(int32 a2)
{
    // asm: mov ecx,[esi+58h] ; 产品对象
    TechnoClass* pProduct = CurrentProduction;

    // asm: test ecx,ecx / jnz loc_4C9EB9
    if (pProduct == nullptr)
    {
        // asm: mov eax,[esi+68h] / test / jz loc_4C9FA6 ; 无产品且无特殊项
        if (SpecialItem == 0xFFFFFFFF)
            return false;                      // asm: loc_4C9FA6 -> 0
    }

    // asm: mov al,[esi+70h] / test / jz loc_4C9FA6 ; 未挂起
    if (!OnHold)
        return false;

    // asm: test ecx,ecx / mov edi,36h
    const int32 kDone = 0x36;
    if (pProduct != nullptr)
    {
        // asm: cmp [esi+24h],edi / jz loc_4C9FA6 ; 已完成
        if (Progress == kDone)
            return false;
    }

    // asm: cmp dword ptr [esi+68h],0FFFFFFFFh / jz short loc_4C9EE5
    if (SpecialItem != 0xFFFFFFFF)
    {
        // asm: cmp [esi+24h],edi / jz loc_4C9FA6
        if (Progress == kDone)
            return false;
    }

    // asm: loc_4C9EE5: xor edx,edx / mov byte ptr [esi+70h],0
    OnHold = false;

    // asm: test ecx,ecx / jz short loc_4C9EF6
    //      call TechnoClass_GetBuildTime / mov edx,eax
    int32 est = 0;
    if (pProduct != nullptr)
        est = Get_Product_Build_Time(pProduct);

    // asm: edx = est / 54, clamp [1,255]
    const int32 estimate = Scale_Build_Time(est);

    // asm: mov [esi+38h],edx / mov [ecx],Frame / [ecx+4]=var_8 / [ecx+8]=edx
    ProductionEstimateFrames = estimate;
    ProductionStartFrame = Game::CurrentFrame;
    ProductionDuration = estimate;

    // asm: 进度跨度计算
    int32 span = 0;
    if (pProduct != nullptr)
    {
        // asm: mov eax,[esi+24h] / mov ecx,edi / sub ecx,eax / jz short loc_4C9F51
        const int32 remaining = kDone - Progress;
        if (remaining != 0)
            span = Balance / remaining;        // asm: [esi+60h] / ecx
        else
            span = Balance;                    // asm: loc_4C9F51
    }
    else
    {
        span = 0;                              // asm: loc_4C9F56 xor edi,edi
    }

    // asm: cmp eax,edi / jl loc_4C9FA6 ; eax = 产品进度
    const int32 progress = Progress;
    if (progress < span)
        return false;

    // asm: mov byte ptr [esi+71h],1
    CompleteSuspended = true;

    // asm: test al,al(=a2) / jz loc_4C9F9C
    if (a2 != 0)
    {
        // asm: mov al,[esi+70h] / test / jnz loc_4C9F9C
        if (!OnHold)
        {
            // asm: mov byte ptr [esi+71h],1 / mov byte ptr [esi+70h],1
            CompleteSuspended = true;
            OnHold = true;
            // asm: mov [edx],Frame / [esi+38h]=0 / [edx+8]=0
            ProductionStartFrame = Game::CurrentFrame;
            ProductionEstimateFrames = 0;
            ProductionDuration = 0;
        }
    }

    return true;                               // asm: loc_4C9F9C mov al,1
}

// ============================================================================
// asm: FactoryClass::Abandon (0x4CA0F8)
//
//   char __thiscall FactoryClass::Abandon(FactoryClass *this)
//
//   若无在线产品直接返回 0；否则：
//     - 退还应付款与已付款之差（HouseClass_GiveMoney）
//     - 清 +0x60/+0x68/+0x24，置 +0x70=1 / +0x5D=1
//     - 建筑类型（WhatAmI 0xF/1/2/6）按类清 House 的对应标志
//     - ScenarioInit++ ... 产品 Detach ... ScenarioInit--
//   返回 1。
// ============================================================================
bool FactoryClass::AbandonProduction()
{
    // asm: mov ecx,[esi+58h] / cmp ecx,edi(=0) / jz loc_4CA115
    if (CurrentProduction == nullptr)
    {
        CurrentType = nullptr;
        return false;                          // asm: loc_4CA115 xor al,al
    }

    // asm: 退款 = 类型应付 − 已付余额
    if (Owner != nullptr)
    {
        const int32 owed = CurrentType != nullptr
            ? static_cast<int32>(CurrentType->Cost * Owner->Get_Cost_Mult(CurrentType))
            : 0;
        const int32 refund = owed - Balance;
        if (refund > 0)
            Owner->GiveMoney(refund);
    }

    Balance = 0;
    if (SpecialItem != 0)
        SpecialItem = 0xFFFFFFFF;

    // asm: 重置计时器（+0x38=0 / +0x2C=Frame）
    ProductionEstimateFrames = 0;
    ProductionStartFrame = Game::CurrentFrame;
    ProductionDuration = 0;

    Progress = 0;
    OnHold = true;
    IsDifferent = true;

    // asm: 建筑/单位类型的房主标志清理（+0x5650/+0x5654/+0x5658/+0x564C）
    //   OpenYR 侧以分类清 0 近似；相关字段未落桩，按铁律①不臆造。

    // asm: ScenarioInit++ ... 产品 Detach(1) ... ScenarioInit-- ... +0x58=0
    ++ScenarioInit;
    CurrentProduction = nullptr;
    CurrentType = nullptr;
    --ScenarioInit;

    return true;                               // asm: mov al,1
}

bool FactoryClass::Abandon()
{
    return AbandonProduction();
}

// ============================================================================
// asm: FactoryClass::Has_Completed (0x4CA137)
//
//   char __thiscall FactoryClass::Has_Completed(FactoryClass *this)
//   在线产品完成，或特殊项完成 -> 1。
// ============================================================================
bool FactoryClass::IsCompleted() const
{
    const int32 kDone = 0x36;

    if (CurrentProduction != nullptr && Progress == kDone)
        return true;
    if (SpecialItem != 0xFFFFFFFF && Progress == kDone)
        return true;
    return false;
}

bool FactoryClass::IsWorking() const
{
    return (CurrentProduction != nullptr) && !OnHold && !IsCompleted();
}

// ============================================================================
// asm: FactoryClass_GetBuildTimeFrames (0x4C9FAB)
//
//   int __thiscall FactoryClass_GetBuildTimeFrames(FactoryClass *this)
//   注意：asm 无参数，取 [+0x58] 的建时；估 = 建时/54，clamp [1,255]。
// ============================================================================
int32 FactoryClass::GetBuildTimeFrames() const
{
    // asm: mov ecx,[ecx+58h] / test / jz short loc_4C9FC0
    int32 est = 0;
    if (CurrentProduction != nullptr)
        est = Get_Product_Build_Time(CurrentProduction);  // asm: call TechnoClass_GetBuildTime
    return Scale_Build_Time(est);                          // asm: /54 + clamp
}

int32 FactoryClass::GetRemainingFrames() const
{
    if (CurrentProduction == nullptr)
        return 0;
    const int32 total = GetBuildTimeFrames();
    const int32 remaining = total - Progress;
    return (remaining > 0) ? remaining : 0;
}

// ============================================================================
// asm: HouseClass_Update_Factories_Timing 内联体 (0x4CA6F0..0x4CA73F)
//
//   mov ecx,[esi+58h] / xor edx,edx / test ecx,ecx / jz short
//     call TechnoClass_GetBuildTime / mov edx,eax
//   mov eax,4BDA12F7h / imul edx / sar edx,4 ... -> est/54，clamp [1,255]
//   cmp edx,[esi+38h] / jz retn / mov [esi+38h],edx
// ============================================================================
void FactoryClass::Recalc_Build_Estimate()
{
    int32 est = 0;
    if (CurrentProduction != nullptr)
        est = Get_Product_Build_Time(CurrentProduction);   // asm: TechnoClass_GetBuildTime

    const int32 scaled = Scale_Build_Time(est);            // asm: /54 + clamp

    // asm: 值变化才写回估计帧数。
    if (scaled != ProductionEstimateFrames)
        ProductionEstimateFrames = scaled;
}

// ============================================================================
// asm: _sub_4CA180_unused (0x4CA180)
//
//   int __thiscall sub_4CA180(FactoryClass *this)
//   在线产品的“每剩余进度应付款”——Update 里用它决定本帧扣款额：
//     if ([+0x58] == 0) return 0;             ; 无在线产品
//     ecx = 0x36 - Progress                   ; 剩余进度步数
//     eax = [+0x60]                           ; Balance
//     if (ecx == 0) return eax;               ; 进度已满 -> 全款
//     return eax / ecx;                       ; 否则均摊
// ============================================================================
int32 FactoryClass::Get_Cost() const
{
    if (CurrentProduction == nullptr)
        return 0;                              // asm: loc_4CA19C xor eax,eax

    const int32 left = 0x36 - Progress;        // asm: mov esi,36h / sub esi,eax
    const int32 balance = Balance;             // asm: mov eax,[ecx+60h]
    if (left == 0)
        return balance;                        // asm: loc_4CA19E
    if (left < 0)
        return balance;                        // 超界保护（原版进度不会超 36h）
    return balance / left;                     // asm: cdq / idiv esi
}

// ============================================================================
// asm: FactoryClass_Update (0x4C9B03)
//
//   void __thiscall FactoryClass_Update(FactoryClass *this)
//
//   逐帧推进生产线。逐段见下。
// ============================================================================
void FactoryClass::Update()
{
    // asm: mov al,[esi+70h] / test / jnz loc_4C9C3B  ; 挂起 -> 直接返回
    if (OnHold)
        return;

    // asm: mov eax,[esi+58h] / test / jnz loc_4C9B49
    //      mov ecx,[esi+68h] / test / jz loc_4C9C3B   ; 无产品且无特殊项
    if (CurrentProduction == nullptr && SpecialItem == 0xFFFFFFFF)
        return;

    // asm: loc_4C9B49: cmp [esi+24h],36h / jz loc_4C9C3B  ; 已完成
    if (CurrentProduction != nullptr && Progress == 0x36)
        return;

    // asm: cmp [esi+68h],0FFFFFFFFh / jz loc_4C9B63
    //      cmp [esi+24h],36h / jz loc_4C9C3B
    if (SpecialItem != 0xFFFFFFFF && Progress == 0x36)
        return;

    // asm: FrameTimerClass_Get_Remaining(+0x2C) / test eax / jnz loc_4C9BBA
    const int32 remaining = (ProductionStartFrame == -1)
        ? ProductionDuration
        : ((Game::CurrentFrame - ProductionStartFrame) >= ProductionDuration
               ? 0
               : (ProductionDuration - (Game::CurrentFrame - ProductionStartFrame)));

    if (remaining == 0 || ProductionEstimateFrames == 0)
    {
        // asm: loc_4C9BBA: mov byte ptr [esi+28h],0 ; return
        AdvancedThisFrame = false;
        return;
    }

    // asm: edx = [esi+24h] + [esi+3Ch] ; [esi+24h]=edx ; [esi+28h]=1
    Progress += Rate;
    AdvancedThisFrame = true;

    // asm: mov ecx,Frame / mov [edi],ecx / [edi+4]=var_8 / [edi+8]=eax(=估计)
    ProductionStartFrame = Game::CurrentFrame;
    ProductionDuration = ProductionEstimateFrames;

    // asm: 计算跨度 edi
    int32 span = 0;
    if (CurrentProduction != nullptr)
    {
        // asm: mov eax,[esi+24h] / mov ecx,36h / sub ecx,eax / jz loc_4C9BC5
        const int32 left = 0x36 - Progress;
        if (left != 0)
            span = Balance / left;             // asm: [esi+60h] / ecx
        else
            span = Balance;                    // asm: loc_4C9BC5
    }
    else
    {
        span = 0;                              // asm: loc_4C9BCA xor edi,edi
    }

    // asm: cmp edi,eax(=+0x60) / jl / mov edi,eax
    if (span >= Balance)
        span = Balance;

    // asm: eax = [+0x6C] 上的进度查询
    const int32 productProgress = Progress;

    // asm: cmp edi,eax / jle short loc_4C9BF1
    if (span > productProgress)
    {
        // asm: mov eax,[esi+24h] / mov [esi+OnHold],bl(=1) / dec eax / [esi+24h]=eax
        OnHold = true;
        Progress -= 1;
    }
    else
    {
        // asm: loc_4C9BF1: HouseClass::Spend_Money([+0x6C], edi)
        if (Owner != nullptr && span > 0)
            Owner->SpendMoney(span);
        // asm: mov byte ptr [esi+5Ch],0 ; [esi+60h] -= edi
        BalanceDirty = false;
        Balance -= span;
    }

    // asm: cmp [esi+24h],36h / jnz loc_4C9C3B
    if (Progress == 0x36)
    {
        // asm: mov [esi+IsCompleteSuspended],bl(=1)
        CompleteSuspended = true;
        // asm: 重置计时器（+0x38=0 / +0x2C=Frame）
        ProductionEstimateFrames = 0;
        ProductionStartFrame = Game::CurrentFrame;
        ProductionDuration = 0;
        // asm: HouseClass::Spend_Money([+0x6C],[+0x60]) ; [+0x60]=0
        if (Owner != nullptr && Balance > 0)
            Owner->SpendMoney(Balance);
        Balance = 0;
    }
}

// ============================================================================
// asm: FactoryClass_CompletedProduction (0x4CA1B1)
//
//   char __thiscall FactoryClass_CompletedProduction(FactoryClass *this)
//
//   若在线产品已完成 -> 清空 +0x58，置 +0x70/+0x5D，重置进度与计时器，返回 1；
//   否则若特殊项已完成 -> SpecialItem=0xFFFFFFFF，同样收尾。
// ============================================================================
bool FactoryClass::CompletedProduction()
{
    // asm: mov esi,[ecx+58h] / cmp esi,edx(=0) / jz loc_4CA1E4
    //      cmp [ecx+24h],eax(=36h) / jnz loc_4CA1E4
    if (CurrentProduction != nullptr && Progress == 0x36)
    {
        // asm: mov al,1 / [ecx+58h]=0 / [ecx+70h]=1 / [ecx+5D]=1 / [ecx+24h]=0
        CurrentProduction = nullptr;
        CurrentType = nullptr;
        OnHold = true;
        IsDifferent = true;
        Progress = 0;
        // asm: [ecx+38h]=0 / [+0x2C]=Frame
        ProductionEstimateFrames = 0;
        ProductionStartFrame = Game::CurrentFrame;
        ProductionDuration = 0;
        return true;
    }

    // asm: loc_4CA1E4: cmp [ecx+68h],edx(=0) / jz loc_4CA21E
    //      cmp [ecx+24h],eax / jnz loc_4CA21E
    if (SpecialItem != 0 && Progress == 0x36)
    {
        // asm: mov dword ptr [ecx+68h],0FFFFFFFFh / [ecx+70h]=1 / [ecx+5D]=1 / [ecx+24h]=0
        SpecialItem = 0xFFFFFFFF;
        OnHold = true;
        IsDifferent = true;
        Progress = 0;
        ProductionEstimateFrames = 0;
        ProductionStartFrame = Game::CurrentFrame;
        ProductionDuration = 0;
        return true;
    }

    // asm: loc_4CA21E: xor al,al
    return false;
}

// ============================================================================
// 完成品的落地（工厂侧派出）
//
// asm: FactoryClass_CompletedProduction 只做状态收尾；成品的实际创建/派出在
//   BuildingClass_Update_Factory / HouseClass_UnitFromFactory 路径上完成。
//   此处保留 OpenYR 现有的落地实现，供工厂侧调用链使用。
// ============================================================================
bool FactoryClass::PlaceProducedUnit(TechnoTypeClass* pType)
{
    if (pType == nullptr || Factory == nullptr || Owner == nullptr)
        return false;

    CellStruct exitCell = Factory->FindExitCell(0, 0);
    CellClass* pCell = MapClass::Instance->GetCellAt(exitCell);
    if (pCell == nullptr)
        return false;

    CoordStruct exitCoord(static_cast<int32>(exitCell.X) * 256,
                          static_cast<int32>(exitCell.Y) * 256,
                          pCell->Get_Ground_Height());

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
                Owner->UnitFromFactory(pUnit, Factory, false);
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
                Owner->UnitFromFactory(pInf, Factory, false);
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
                Owner->UnitFromFactory(pAircraft, Factory, false);
            }
            break;
        }
        case AbstractType::BuildingType:
        {
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
    return true;
}

// ============================================================================
// asm: FactoryClass_PointerGotInvalid (0x4CA580)
//
//   _TechnoClass *__thiscall FactoryClass_PointerGotInvalid(
//       FactoryClass *this, _TechnoClass *a2, int a3)
//   retn 8 -> 2 参数
//
//   若失效指针正是在线产品（+0x58），清空它。
// ============================================================================
void FactoryClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;
    // asm: mov eax,[esp+a2] / mov edx,[ecx+58h] / cmp / jnz retn
    if (reinterpret_cast<void*>(pInvalid) == reinterpret_cast<void*>(CurrentProduction))
    {
        // asm: mov dword ptr [ecx+58h],0
        CurrentProduction = nullptr;
        CurrentType = nullptr;
    }
}

// ============================================================================
// 序列化 / CRC
//
// asm: FactoryClass_Save (0x4CA3C4) / FactoryClass_Load (0x4CA2A0)
//   写出 [+0x50] 队列计数，随后逐项写队列指针；Load 反向并走 Swizzle。
//   OpenYR 的 IStream 层尚未落桩逐字段读写，这里保持现有薄壳。
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

// asm: FactoryClass_GetCRC (0x4CA469)
//   逐字段喂 CRC：OnHold / IsDifferent / Balance / OriginalBalance /
//   在线产品 ID / SpecialItem / Owner 的 ID。
void FactoryClass::ComputeCRC(CRCEngine& crc) const
{
    const bool hold = OnHold;
    crc.AddData(&hold, sizeof(hold));

    const bool diff = IsDifferent;
    crc.AddData(&diff, sizeof(diff));

    crc.AddData(&Balance, sizeof(Balance));
    crc.AddData(&OriginalBalance, sizeof(OriginalBalance));

    int32 productID = 0;
    if (CurrentProduction != nullptr)
        productID = CurrentProduction->Fetch_ID();
    crc.AddData(&productID, sizeof(productID));

    crc.AddData(&SpecialItem, sizeof(SpecialItem));

    int32 ownerID = 0;
    if (Owner != nullptr)
        ownerID = Owner->Fetch_ID();
    crc.AddData(&ownerID, sizeof(ownerID));
}
