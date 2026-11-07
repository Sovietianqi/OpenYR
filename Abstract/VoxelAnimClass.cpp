#include <Abstract/VoxelAnimClass.h>
#include <Abstract/VoxelAnimTypeClass.h>
#include <Abstract/ObjectTypeClass.h>
#include <Abstract/TechnoClass.h>
#include <Animations/AnimClass.h>
#include <Animations/AnimTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Houses/HouseClass.h>
#include <Map/CellClass.h>
#include <Math/Timer.h>
#include <Math/Rectangle.h>
#include <Rendering/TacticalClass.h>
#include <Rendering/Surface.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/UnitArt.h>
#include <Rendering/VoxelRaster.h>
#include <FileFormats/VXL.h>
#include <FileFormats/HVA.h>
#include <Helpers/AsmUIHelpers.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>
#include <Audio/VocClass.h>

#include <cstring>
#include <cstdio>
#include <vector>
#include <cstdlib>

// ============================================================================
// VoxelAnimClass.cpp
//
//  VoxelAnimClass is the runtime instance of a voxel-based animation - falling
//  debris, meteor chunks, and other tumbling voxel projectiles that bounce
//  across the map.  It inherits ObjectClass and is updated every frame by the
//  main loop, applying gravity, elasticity and lifetime decay.
//
//  This file implements:
//    * Static Array management (Init_Array / Delete_Array / Delete_All)
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * Update (physics integration, bouncing, lifetime, damage application)
//    * Draw_It (voxel render hook)
//    * Get_Bounce_Physics (whether the anim bounces on impact)
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<VoxelAnimClass*>* VoxelAnimClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void VoxelAnimClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<VoxelAnimClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<VoxelAnimClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<VoxelAnimClass*>();
    }
}

void VoxelAnimClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<VoxelAnimClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void VoxelAnimClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        VoxelAnimClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// asm: VoxelAnimClass_SCTOR (0x40EE40, 静态全局构造)
//
//   把 v_VoxelAnim（DynamicVectorClass<VoxelAnimClass*>）初始化：
//     vftable = DynamicVectorClass 表；Capacity(dword_887390)=0；
//     IsAllocated(byte_887394)=1；incrementAmount(dword_88739C)=0Ah(10)；
//     count(v_VoxelAnim_count)=0。随后 SDTOR 经 _atexit 登记。
//   OpenYR 以"建立带 10 步进容量的全局数组"等价表达。
// ============================================================================
void VoxelAnimClass::SCTOR()
{
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<VoxelAnimClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<VoxelAnimClass*>)));
        if (Array != nullptr)
        {
            // asm: incrementAmount = 0Ah(10)；IsAllocated = 1。
            new (Array) DynamicVectorClass<VoxelAnimClass*>();
            Array->Grow(10);   // asm: dword_88739C = 0Ah
        }
    }
}

// ============================================================================
// asm: VoxelAnimClass_SDTOR (0x40EE8C, _atexit 回调)
//
//   把 vftable 还原为 VectorClass 表；若 IsAllocated(byte_887395) 置位则
//   delete 数据区并把 v_VoxelAnim_ptr 清零；最后清 IsAllocated 与 Capacity。
//   OpenYR 对应 Delete_Array 的析构 + 释放。
// ============================================================================
void VoxelAnimClass::SDTOR()
{
    Delete_Array();
}

// ============================================================================
// Constructor
// ============================================================================

VoxelAnimClass::VoxelAnimClass(VoxelAnimTypeClass* pType, HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type       = pType;
    Velocity   = CoordStruct(0, 0, 0);
    Health     = 1;
    Lifetime   = 0;
    VoxelFlags = 0;
    IsActive   = true;
    Owner      = pOwner;
    IsSelected = false;
    IsInLimbo  = false;

    // Initialise the lifetime from the type when available.  The full binary
    // uses a per-type lifetime; the standalone build defaults to a fixed span.
    if (pType != nullptr)
    {
        Lifetime = 600;
    }
}

// ============================================================================
// Destructor
// ============================================================================

VoxelAnimClass::~VoxelAnimClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT VoxelAnimClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::VoxelAnim);
    return S_OK;
}

HRESULT VoxelAnimClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0) return hr;

    ULONG read = 0;

    // Read type ID
    char typeId[0x18];
    hr = pStm->Read(typeId, sizeof(typeId), &read);
    if (hr < 0 || read != sizeof(typeId)) return E_FAIL;
    typeId[sizeof(typeId) - 1] = '\0';
    Type = typeId[0] ? VoxelAnimTypeClass::Find(typeId) : nullptr;

    hr = pStm->Read(&Velocity, sizeof(Velocity), &read);
    if (hr < 0 || read != sizeof(Velocity)) return E_FAIL;

    hr = pStm->Read(&Health, sizeof(Health), &read);
    if (hr < 0 || read != sizeof(Health)) return E_FAIL;

    hr = pStm->Read(&Lifetime, sizeof(Lifetime), &read);
    if (hr < 0 || read != sizeof(Lifetime)) return E_FAIL;

    hr = pStm->Read(&VoxelFlags, sizeof(VoxelFlags), &read);
    if (hr < 0 || read != sizeof(VoxelFlags)) return E_FAIL;

    // Read bool flags as a bitmask
    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    IsActive = (flags & 0x0001) != 0;

    return S_OK;
}

HRESULT VoxelAnimClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    ULONG written = 0;

    // Write type ID
    char typeId[0x18];
    std::memset(typeId, 0, sizeof(typeId));
    if (Type && Type->ID) {
        int32 j = 0;
        while (Type->ID[j] && j < static_cast<int32>(sizeof(typeId)) - 1) {
            typeId[j] = Type->ID[j]; ++j;
        }
    }
    hr = pStm->Write(typeId, sizeof(typeId), &written);
    if (hr < 0 || written != sizeof(typeId)) return E_FAIL;

    hr = pStm->Write(&Velocity, sizeof(Velocity), &written);
    if (hr < 0 || written != sizeof(Velocity)) return E_FAIL;

    hr = pStm->Write(&Health, sizeof(Health), &written);
    if (hr < 0 || written != sizeof(Health)) return E_FAIL;

    hr = pStm->Write(&Lifetime, sizeof(Lifetime), &written);
    if (hr < 0 || written != sizeof(Lifetime)) return E_FAIL;

    hr = pStm->Write(&VoxelFlags, sizeof(VoxelFlags), &written);
    if (hr < 0 || written != sizeof(VoxelFlags)) return E_FAIL;

    // Write bool flags as a bitmask
    uint32 flags = 0;
    if (IsActive) flags |= 0x0001;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    return S_OK;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType VoxelAnimClass::WhatAmI() const
{
    return AbstractType::VoxelAnim;
}

int32 VoxelAnimClass::Size() const
{
    return sizeof(VoxelAnimClass);
}

// ============================================================================
// CRC
// ============================================================================

void VoxelAnimClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&Velocity,   sizeof(Velocity));
    crc.AddData(&Health,     sizeof(Health));
    crc.AddData(&Lifetime,   sizeof(Lifetime));
    crc.AddData(&VoxelFlags, sizeof(VoxelFlags));
    crc.AddData(&IsActive,   sizeof(IsActive));
}

int32 VoxelAnimClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// 根据游戏行为，可知 Bounce_Physics 负责下面这段逻辑。
//
//  Returns whether this voxel anim bounces when it hits the ground.  This is
//  determined by the type's WillBounce flag.
// ============================================================================

bool VoxelAnimClass::Get_Bounce_Physics() const
{
    if (Type == nullptr)
        return false;

    // asm: VoxelAnimTypeClass 无独立 WillBounce 字段；是否反弹由
    // 弹性系数 Elasticity (0x2A0) 决定（>0 即可反弹）。
    return Type->Elasticity > 0.0;
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  Integrates the voxel anim's motion:
//    * Applies gravity to the vertical velocity component.
//    * Moves the anim by its velocity.
//    * Handles ground collision - bouncing (with elasticity) or stopping.
//    * Applies damage to anything within the damage radius on impact.
//    * Decrements the lifetime; the anim is deactivated when it expires.
// ============================================================================

// ============================================================================
// asm: VoxelAnimClass_Update (0x7499xx, ~0x2C0 行)
//
//   体素碎片动画的逐帧推进。原版控制流：
//     [977]  Type->StartSound(0x2DC)!=-1 → VocClass_UpdateAtLocation(StartSound, coords)
//     loc_749F67: [ebx+110h](IsActive)==0 → vt+0xF8(UnInit) 后返回
//     loc_749F83: --Lifetime(0x140)；若 >0 → loc_74A70E 走物理段
//                否则落到 loc_74A13E 的"落地收束段"
//     loc_74A13E: 取所在地块；若 landType==2（水）→ BounceAnim 派生分支；
//                若 Type->BounceAnim(0x2E4)!=0 → 派生 AnimClass(BounceAnim)
//                （asm 74A7AB 内还会按帧奇偶派生 TrailerAnim(0x2EC)）
//                随后遍历地块占据者，曼哈顿距离 ≤ Type->DamageRadius(0x2F4)
//                时以 Type->Warhead(0x2F8) / Type->Damage(0x2F0) ReceiveDamage
//     loc_74A934: SetCoords(coords) 回写位置
//   OpenYR 把以上落到 AI()，Update() 只做门控后转译。
// ============================================================================
void VoxelAnimClass::Update()
{
    if (Type == nullptr)
    {
        IsActive = false;
        return;
    }

    AI();
}

int32 VoxelAnimClass::AI()
{
    // ------------------------------------------------------------------
    // 1) 起始音效（asm: Type->StartSound != -1 → VocClass_UpdateAtLocation）。
    // ------------------------------------------------------------------
    if (Type->StartSound != -1)
    {
        CoordStruct pos;
        ObjectClass::GetCoords(&pos);
        VocClass::Sound_Effect(Type->StartSound, &pos, nullptr);
    }

    // ------------------------------------------------------------------
    // 2) 未激活 → 从战场撤下（asm: [ebx+110h]==0 → UnInit）。
    // ------------------------------------------------------------------
    if (!IsActive)
    {
        return 1;
    }

    // ------------------------------------------------------------------
    // 3) 生命周期递减（asm: dec [ebx+140h]）。
    // ------------------------------------------------------------------
    if (Lifetime > 0)
    {
        --Lifetime;
    }

    if (Lifetime > 0)
    {
        // ==============================================================
        // 物理段（asm loc_74A70E..74A934）：
        //   a) TrailerAnim：帧号为偶时按速度落点派生拖尾动画。
        //   b) 陨石自旋：IsMeteor 时角速度积分。
        //   c) 速度积分位置 + 重力 + 地面反弹（MaxXYVel/Elasticity）。
        // ==============================================================
        if (Type->TrailerAnim != nullptr && (FrameTimer::GetTime() & 1) == 0)
        {
            new AnimClass(Type->TrailerAnim, Location, 0, 1, 0x600, 0, false);
        }

        // 重力：Z 方向速度持续衰减（per-type 重力以 ZVel 参数近似）。
        const int32 gravity = (Type->MinZVel < 0.0) ? static_cast<int32>(-Type->MinZVel) : 32;
        Velocity.Z -= gravity;

        // 速度上限（asm: MaxXYVel 0x2C8）。
        if (Type->MaxXYVel > 0.0)
        {
            const int32 maxVel = static_cast<int32>(Type->MaxXYVel);
            if (Velocity.X < -maxVel) Velocity.X = -maxVel;
            if (Velocity.X >  maxVel) Velocity.X =  maxVel;
            if (Velocity.Y < -maxVel) Velocity.Y = -maxVel;
            if (Velocity.Y >  maxVel) Velocity.Y =  maxVel;
            if (Velocity.Z >  maxVel) Velocity.Z =  maxVel;
        }

        // 位置积分。
        Location.X += Velocity.X;
        Location.Y += Velocity.Y;
        Location.Z += Velocity.Z;

        // 地面接触：按地块地面高度判定反弹/静止。
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        int32 floorZ = (pCell != nullptr) ? pCell->GetFloorHeight(Location) : 0;

        if (Location.Z <= floorZ)
        {
            Location.Z = floorZ;

            if (Type->Elasticity > 0.0)
            {
                int32 bounceVel = static_cast<int32>(
                    static_cast<double>(-Velocity.Z) * Type->Elasticity);
                if (bounceVel > 0)
                {
                    Velocity.Z = bounceVel;
                    // 落地摩擦：水平速度按弹性系数衰减。
                    Velocity.X = static_cast<int32>(static_cast<double>(Velocity.X) * Type->Elasticity);
                    Velocity.Y = static_cast<int32>(static_cast<double>(Velocity.Y) * Type->Elasticity);
                }
                else
                {
                    // 弹不动了 → 触地静止，进入收束。
                    Velocity = CoordStruct(0, 0, 0);
                    SettleOnImpact();
                    return 1;
                }
            }
            else
            {
                Velocity = CoordStruct(0, 0, 0);
                SettleOnImpact();
                return 1;
            }
        }

        return 0;
    }

    // ------------------------------------------------------------------
    // 4) 生命周期耗尽（asm loc_74A13E..74A934）：落地收束。
    //    若落在水面（landType==2）直接结束；否则派生 BounceAnim 并结算伤害。
    // ------------------------------------------------------------------
    {
        CellStruct cell = CoordMath::CoordToCell(Location);
        CellClass* pCell = (MapClass::Instance != nullptr)
            ? MapClass::Instance->GetCellAt(cell) : nullptr;
        const int32 landType = (pCell != nullptr)
            ? static_cast<int32>(pCell->Get_Land_Type()) : -1;

        if (landType == 2)
        {
            // 落水：Lifetime 归零、直接撤下，不派生反弹。
            Lifetime = 0;
            return 1;
        }

        if (Type->BounceAnim != nullptr)
        {
            new AnimClass(Type->BounceAnim, Location, 0, 1, 0x600, 0, false);
        }
        SettleOnImpact();
        return 1;
    }
}

// ============================================================================
// asm: VoxelAnimClass_Update 的落点伤害段（loc_74A884..loc_74A929）
//
//   取动画所在地块的占据者链表（cell+0xE4），对每个占据者：
//     曼哈顿距离 |dx|+|dy| ≤ Type->DamageRadius(0x2F4) 时，
//     以 Type->Warhead(0x2F8) 与 Type->Damage(0x2F0) 调用 ReceiveDamage。
// ============================================================================
void VoxelAnimClass::SettleOnImpact()
{
    if (Type->Damage <= 0 || Type->DamageRadius <= 0 || Type->Warhead == nullptr)
    {
        IsActive = false;
        return;
    }

    CoordStruct center;
    ObjectClass::GetCoords(&center);
    CellStruct cell = CoordMath::CoordToCell(center);
    CellClass* pCell = (MapClass::Instance != nullptr)
        ? MapClass::Instance->GetCellAt(cell) : nullptr;
    if (pCell != nullptr)
    {
        for (ObjectClass* pObj = pCell->Get_Occupier();
             pObj != nullptr; pObj = pObj->NextObject)
        {
            const AbstractType what = pObj->WhatAmI();
            if (what != AbstractType::Unit && what != AbstractType::Infantry
                && what != AbstractType::Building && what != AbstractType::Aircraft)
            {
                continue;
            }
            CoordStruct objCoord;
            pObj->GetCoords(&objCoord);
            const int32 dist = std::abs(objCoord.X - center.X)
                             + std::abs(objCoord.Y - center.Y);
            if (dist <= Type->DamageRadius)
            {
                int32 damage = Type->Damage;
                static_cast<TechnoClass*>(pObj)->ReceiveDamage(
                    &damage, 0, Type->Warhead, 0, 0, 0, Owner);
            }
        }
    }

    VoxelFlags |= 0x1;   // 记录已结算落地伤害
    IsActive = false;
}

// ============================================================================
// asm: VoxelAnimClass_Draw (0x749B90, retn 8 = 2 参数: pos, OtherSurface)
//
//   碎片体素模型的绘制。逐条（asm 原文）：
//
//   1) Scenario 的 shroud 位（test ch,10h）→ Map.IsCellShrouded(Location) 直接返回；
//   2) 落点 = TacticalMap->Coord_To_Pixel(Location)，若所在格带 +0x140 的
//      0x100（桥面）则先把 Z 抬 dword_B1D1BC 再算像素；
//   3) [this+0x13Ch] 非零（不可见/已停用）→ 返回；
//   4) VXL = Type->[+0x0B0]；为空返回；
//   5) 由 [this+0xB0] 的姿态（sub_4399E0 取出 12 dword 的 Matrix3D）建变换，
//      Voxel_753E00 重算法线明暗；
//   6) 阴影遍：sub_753F90(VXL, 0, &unk_887430, matrix, &dword_887420,
//      Type->VoxelIndex, 0,0,0) → 用 Convert_UNITSNO、0x2001 旗标、
//      ZAdjust = -4 - Draw_AdjustForBuildingZ(...) blit 一次；
//   7) 本体遍：hvaFrame = Type->VoxelIndex(+0x298)，sub_7540F0(VXL, frame, 0,
//      matrix)；旗标 = 0x2800，Type->Translucent(+0x295) 时 |= 0x04；
//      Convert = Owner(+0x10C) ? ColorScheme[Owner->Colour]->[0x30C]
//                             : (Type->[0x300] ? dword_87F6BC : Convert_UNITSNO)；
//      ZAdjust = -2 - Draw_AdjustForBuildingZ(GetZ())；亮度 0x3E8 = 1000。
//
//   本树的接线：模型按 Type->Get_VXL_Name() 懒加载（Rendering/VoxelRaster 的
//   Voxel_Rasterize 是同一套逐面画家算法 + 16 级明暗），section 0 的 HVA 帧取
//   Type->VoxelIndex，旗标/亮度/ZAdjust 按上面取值。两处依赖需要说明：
//     * Voxel_Rasterize 没有"阴影 remap"入口，第 6) 步阴影遍无法表达，
//       这里只做本体遍（阴影由该光栅器的接口决定，不在本类内补）；
//     * 碎片翻滚姿态（asm this+0xB0 的四元数/矩阵）本类未建模，
//       变换用单位矩阵，即模型按载入姿态绘制；
//     * 桥面格的整体抬升（dword_B1D1BC，loc_749BD0 的 +0x100 判定）
//       本树没有该全局，落点不做这段修正。
// ============================================================================
namespace
{
    struct VoxelAnimModelEntry
    {
        char     Name[0x20];
        VXLClass VXL;
        HVAClass HVA;
        bool     HasHVA;
        bool     Loaded;
    };

    std::vector<VoxelAnimModelEntry*>* s_pVoxelAnimModels = nullptr;

    VoxelAnimModelEntry* VoxelAnim_Get_Model(const char* pName)
    {
        if (pName == nullptr || pName[0] == '\0') return nullptr;

        if (s_pVoxelAnimModels == nullptr)
            s_pVoxelAnimModels = new std::vector<VoxelAnimModelEntry*>();

        for (size_t i = 0; i < s_pVoxelAnimModels->size(); ++i) {
            if (std::strncmp((*s_pVoxelAnimModels)[i]->Name, pName,
                             sizeof(VoxelAnimModelEntry::Name) - 1) == 0) {
                return (*s_pVoxelAnimModels)[i];
            }
        }

        VoxelAnimModelEntry* pEntry = new VoxelAnimModelEntry();
        pEntry->HasHVA = false;
        pEntry->Loaded = false;
        std::strncpy(pEntry->Name, pName, sizeof(VoxelAnimModelEntry::Name) - 1);

        char path[0x40];
        std::snprintf(path, sizeof(path), "%s.vxl", pName);
        pEntry->Loaded = pEntry->VXL.LoadFromFile(path) && pEntry->VXL.GetSectionCount() > 0;
        if (pEntry->Loaded) {
            std::snprintf(path, sizeof(path), "%s.hva", pName);
            pEntry->HasHVA = pEntry->HVA.LoadFromFile(path);
        }

        s_pVoxelAnimModels->push_back(pEntry);
        return pEntry;
    }
}

void VoxelAnimClass::Draw_It(int32 originX, int32 originY) const
{
    if (!IsActive)
        return;

    if (Type == nullptr)
        return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr)
        return;

    VoxelAnimModelEntry* pModel = VoxelAnim_Get_Model(Type->Get_VXL_Name());
    if (pModel == nullptr || !pModel->Loaded)
        return;                                     // asm: jz loc_749EEA

    // asm 第 7) 步：Convert 三选一（有 Owner 用房主色阶，否则标准盘）。
    ConvertClass* pConvert = StandardPalette;
    if (Owner != nullptr && ::ColorScheme::Array != nullptr
        && Owner->ColorSchemeIndex >= 0
        && Owner->ColorSchemeIndex < ::ColorScheme::Array->Size()) {
        ::ColorScheme* pScheme = (*::ColorScheme::Array)[Owner->ColorSchemeIndex];
        if (pScheme != nullptr && pScheme->LightConvert != nullptr) {
            pConvert = pScheme->LightConvert;
        }
    }
    if (pConvert == nullptr)
        return;

    // asm 第 7) 步的旗标是 0x2800（Alpha|ZGradientDeg45），
    //   Type->Translucent(+0x295) 为真时补 0x04（Translucent50）。
    //   Voxel_Rasterize 的入参里没有旗标槽（它直接写像素），半透明的
    //   表达依赖该接口，不在这里自造。

    // asm loc_749BBC: 落点 = TacticalMap->Coord_To_Pixel(Location) + arg_0
    //   （arg_0 是调用方给的屏幕偏移，本树由 originX/originY 承载）。
    Point2D screen(originX, originY);
    if (TacticalClass::Instance != nullptr) {
        CoordStruct world;
        ObjectClass::GetCoords(&world);
        const Point2D pixel = TacticalClass::Instance->CoordsToScreen(world);
        screen.X += pixel.X;
        screen.Y += pixel.Y;
    }

    // asm 第 7) 步：ZAdjust = -2 - Draw_AdjustForBuildingZ(this->GetZ())；
    //   GetZ 即 TechnoClass_GetZ（vt+0x1D0，取 [+0xA4] 的 Z）。
    //   ZAdjust 由 CC_Draw_Shape 层承载，本树的体素光栅器直接落像素，故只记录。

    // asm: hvaFrame = Type->VoxelIndex(+0x298)
    int32 hvaFrame = Type->VoxelIndex;
    if (hvaFrame < 0) hvaFrame = 0;
    HVAClass* pHVA = (pModel->HasHVA && pModel->HVA.GetFrameCount() > 0) ? &pModel->HVA : nullptr;
    if (pHVA != nullptr) hvaFrame %= pHVA->GetFrameCount();

    Rectangle clip(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

    // asm: 本体遍 —— 亮度 0x3E8(=1000)，逐 section 走 sub_7540F0。
    const Matrix4 transform = Matrix4::Identity();   // 姿态未建模，见文件头说明
    const int32 sectionCount = pModel->VXL.GetSectionCount();
    for (int32 section = 0; section < sectionCount; ++section) {
        Voxel_Rasterize(pSurface, pConvert, &pModel->VXL, section,
                        pHVA, hvaFrame, transform, screen,
                        1000, 0, &clip);
    }
}

// ============================================================================
// asm: VoxelAnimClass_CTOR（分配-构造两段序列）
//
// VoxelAnim_Spawn 里的模式是：
//   push size CellClass / call new / add esp,4 / test eax,eax / jz done
//   mov edx,[esi+90h] / ... / call VoxelAnimClass_CTOR
// 即"先按 CellClass 的尺寸申请存储，再跑构造函数"。本工厂把这两步合成
// 一个入口，字段初始化仍由本类构造函数完成。
// ============================================================================
VoxelAnimClass* VoxelAnimClass::Create(VoxelAnimTypeClass* pType,
                                       const CoordStruct& loc)
{
    if (pType == nullptr)
        return nullptr;                            // asm: test eax,eax / jz done

    VoxelAnimClass* pAnim = GameCreateArgs<VoxelAnimClass>(pType, nullptr);
    if (pAnim == nullptr)
        return nullptr;

    pAnim->Location = loc;                         // asm: ObjectClass 位置段
    return pAnim;
}
