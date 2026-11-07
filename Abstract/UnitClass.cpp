// =============================================================================
// UnitClass.cpp - Vehicle unit implementation
//
// Implements all vehicle unit behavior including harvesting, turret rotation,
// deploy/undeploy transitions, carryall transport, iron curtain immunity,
// and combat-related state management.
// =============================================================================

#include <Abstract/UnitClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Game/SaveGameClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Abstract/InfantryTypeClass.h>
#include <Abstract/TechnoTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Combat/BulletClass.h>
#include <Combat/BulletTypeClass.h>
#include <Animations/AnimClass.h>
#include <Animations/AnimTypeClass.h>
#include <Locomotion/LocomotionClass.h>
#include <Particles/ParticleSystemClass.h>
#include <Scenario/ScenarioClass.h>
#include <Special/TemporalClass.h>
#include <AI/TagClass.h>
#include <Houses/HouseClass.h>
#include <Rules/RulesClass.h>
#include <Map/MapClass.h>
#include "Rendering/ObjectUI.h"
#include <Map/CellClass.h>
#include <Game/Game.h>
#include <Game/EngineGlobals.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>
#include <FileFormats/VXL.h>
#include <FileFormats/HVA.h>
#include "Rendering/ConvertClass.h"
#include "Rendering/VoxelRaster.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <AI/TagClass.h>
#include <Game/Externs.h>
#include <Rendering/Surface.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/TacticalClass.h>
#include <Rendering/UnitArt.h>

#include <cmath>
#include <queue>
#include <utility>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include <cstdlib>

// =============================================================================
// Constants
// =============================================================================
static const int32 HARVEST_CAPACITY_DEFAULT = 500;
static const int32 HARVEST_RATE_DEFAULT = 25;
static const int32 TURRET_ROT_SPEED_DEFAULT = 10;
static const int32 IRON_CURTAIN_DURATION = 750;
static const int32 FORCE_SHIELD_DURATION = 500;
static const int32 DEPLOY_ANIMATION_FRAMES = 30;
static const int32 CARRYALL_PICKUP_HEIGHT = 1000;
static const int32 CARRYALL_CRUISE_HEIGHT = 2000;

// =============================================================================
// Static member definitions
// =============================================================================
DynamicVectorClass<UnitClass*>* UnitClass::Array = nullptr;

// =============================================================================
// Constructor
// =============================================================================
UnitClass::UnitClass(HouseClass* pOwner) noexcept
    : FootClass()
    , Type(nullptr)
    , TurretDir()
    , BarrelDir()
    , TurretRotation(0)
    , BarrelRotation(0)
    , IsHarvestingTiberium(false)
    , IsDumpingTiberium(false)
    , IsShowingCrate(false)
    , IsMCV(false)
    , IsHarvester(false)
    , IsAPC(false)
    , IsDeployer(false)
    , IsChrono(false)
    , IsBombing(false)
    , IsUnderground(false)
    , IsSubterranean(false)
    , IsButtMissile(false)
    , IsSpace(false)
    , IsTank(false)
    , IsWalker(false)
    , IsAntiAir(false)
    , IsOpenTopped(false)
    , IsCarryall(false)
    , IsCarryAllFly(false)
    , IsJumpJet(false)
    , HarvestAmount(0)
    , HarvestRate(HARVEST_RATE_DEFAULT)
    , TotalTiberiumValue(0)
    , FlagHouseIndex(-1)
    , Deployed(false)
    , Deploying(false)
    , Undeploying(false)
    , Unloading(false)
    , DeathFrameCounter(0)
    , NonPassengerCount(0)
    , HasFollowerCar(false), Gunner(nullptr), GunnerWeaponIndex(-1)
    , CarriesFlagOfHouse(-1)
    , DisguiseActive(false), DisguiseType(nullptr), DisguiseHouse(nullptr)
    , FollowerCar(nullptr)
    , unknown_7E0(0), unknown_7E4(0), unknown_7E8(0), unknown_7EC(0)
    , unknown_7F0(0), unknown_7F4(0), unknown_7F8(0), unknown_7FC(0)
    , unknown_800(0), unknown_804(0), unknown_808(0), unknown_80C(0)
    , unknown_810(0), unknown_814(0), unknown_818(0), unknown_81C(0)
    , unknown_820(0), unknown_824(0), unknown_828(0), unknown_82C(0)
    , unknown_830(0), unknown_834(0), unknown_838(0), unknown_83C(0)
    , unknown_840(0), unknown_844(0), unknown_848(0), unknown_84C(0)
    , unknown_850(0), unknown_854(0), unknown_858(0), unknown_85C(0)
    , unknown_860(0), unknown_864(0), unknown_868(0), unknown_86C(0)
    , unknown_870(0), unknown_874(0), unknown_878(0), unknown_87C(0)
    , unknown_880(0), unknown_884(0), unknown_888(0), unknown_88C(0)
    , unknown_890(0), unknown_894(0), unknown_898(0), unknown_89C(0)
    , unknown_8A0(0), unknown_8A4(0), unknown_8A8(0), unknown_8AC(0)
    , unknown_8B0(0), unknown_8B4(0), unknown_8B8(0), unknown_8BC(0)
    , unknown_8C0(0), unknown_8C4(0), unknown_8C8(0), unknown_8CC(0)
    , unknown_8D0(0), unknown_8D4(0), unknown_8D8(0), unknown_8DC(0)
    , unknown_8E0(0), unknown_8E4(0), unknown_8E8(0), unknown_8EC(0)
{
    Owner = pOwner;

    // Apply type-derived flags if a type is assigned before construction.
    if (Type) {
        IsHarvester = Type->Harvester;
        IsMCV = Type->IsSimpleDeployer;
        IsAPC = (Type->Passengers > 0);
        IsDeployer = Type->HasDeployer;
        IsChrono = Type->IsChrono;
        IsCarryall = Type->IsCarryall;
        IsTank = !Type->IsBalloonHover;
        IsJumpJet = Type->IsBalloonHover;
        IsSubterranean = Type->IsTeleporter;
    }

    if (Array) {
        Array->Add(this);
    }
}

// =============================================================================
// Destructor
// =============================================================================
UnitClass::~UnitClass()
{
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// =============================================================================
// IPersistStream
// =============================================================================
HRESULT __stdcall UnitClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;
    return FootClass::Load(pStm);
}

HRESULT __stdcall UnitClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;
    return FootClass::Save(pStm, fClearDirty);
}

// =============================================================================
// TechnoClass overrides
// =============================================================================
bool UnitClass::IsVoxel() const
{
    if (!Type) return false;
    return Type->IsVoxel;
}

void UnitClass::Destroyed(ObjectClass* Killer)
{
    IsHarvestingTiberium = false;
    IsDumpingTiberium = false;

    // Clear turret and barrel state.
    TurretRotation = 0;
    BarrelRotation = 0;

    // Remove iron curtain / force shield effects.
    IronCurtainTimer = 0;
    ForceShieldTimer = 0;

    // If this is a carryall with passengers, release them.
    if (IsCarryall) {
        IsCarryAllFly = false;
    }
}

bool UnitClass::CanScatter() const
{
    // Harvesters and MCVs cannot scatter while performing their tasks.
    if (IsHarvester && IsHarvestingTiberium) return false;
    if (IsMCV && IsDeployer) return false;
    return true;
}

int32 UnitClass::GetDefaultSpeed() const
{
    if (!Type) return 0;
    return Type->Speed;
}

bool UnitClass::HasTurret() const
{
    if (!Type) return false;
    return Type->HasTurret;
}

bool UnitClass::CanDeploySlashUnload() const
{
    if (!Type) return false;
    return Type->HasDeployer || Type->HasUndeployer;
}

bool UnitClass::IsUnitFactory() const
{
    if (!Type) return false;
    return Type->Factory != AbstractType::None;
}

// =============================================================================
// Harvesting subsystem
// =============================================================================
bool UnitClass::CanHarvest() const
{
    if (!Type) return false;
    return Type->Harvester || Type->ResourceGatherer;
}

bool UnitClass::IsHarvesting() const
{
    return IsHarvestingTiberium;
}

int32 UnitClass::GetTiberiumLoad() const
{
    return HarvestAmount;
}

float UnitClass::GetTiberiumValue() const
{
    return static_cast<float>(TotalTiberiumValue);
}

 // TechnoClass_GetTiberium reads four consecutive floats from
// the techno's storage block and sums their floored values.  A harvester
// stores the same cargo figure in all four slots, so the load is the value.
double UnitClass::Get_Tiberium() const
{
    return static_cast<double>(TotalTiberiumValue);
}

bool UnitClass::IsHarvestingTooMuch() const
{
    // The harvester is full when it reaches its capacity.
    int32 capacity = HARVEST_CAPACITY_DEFAULT;
    if (Type && Type->Passengers > 0) {
        capacity = Type->Passengers * 100;
    }
    return HarvestAmount >= capacity;
}

void UnitClass::StartHarvesting()
{
    if (!CanHarvest()) return;
    if (IsHarvestingTooMuch()) return;

    IsHarvestingTiberium = true;
    IsDumpingTiberium = false;
}

void UnitClass::StopHarvesting()
{
    IsHarvestingTiberium = false;
    IsDumpingTiberium = false;
}

void UnitClass::HarvestTiberium()
{
    if (!IsHarvestingTiberium) return;
    if (!CanHarvest()) return;

    // Check capacity before harvesting more.
    if (IsHarvestingTooMuch()) {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
        return;
    }

    // Extract tiberium from the current cell.
    CoordStruct currentPos = GetCoords();
    CellStruct cellPos = Math::CoordToCell(currentPos);

    // The harvest rate determines how much tiberium is gathered per cycle.
    int32 amount = HarvestRate;
    if (amount <= 0) amount = HARVEST_RATE_DEFAULT;

    HarvestAmount += amount;
    TotalTiberiumValue += amount;

    // When full, stop harvesting and start heading to a refinery.
    if (IsHarvestingTooMuch()) {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
    }
}

void UnitClass::EnterTiberiumField()
{
    if (!CanHarvest()) return;

    // Only start harvesting if we have capacity remaining.
    if (!IsHarvestingTooMuch()) {
        StartHarvesting();
    }
}

// =============================================================================
// Drawing subsystem
//
//   asm 的绘制顺序（AircraftClass_Draw / FootClass_DrawVXLShadow 为证）：
//       阴影 -> 本体 -> 命中闪白 -> 选中框 -> 血条/pips
//   本函数按这个顺序逐段落像素，任何一段都不能是空的。
//
//   两条路径：
//     * Type 里挂到了真实 SHP（ObjectTypeClass::ImageSHP）-> CC_Draw_Shape
//       走素材 blit；
//     * 没有素材（本仓库不带任何原版美术资源）-> UnitArt 程序化绘制。
// =============================================================================

namespace
{
    // 降级渲染提示只打一次，避免每帧刷屏。
    bool s_LoggedUnitArt = false;

    // ------------------------------------------------------------------------
    // 体素模型缓存
    //
    // asm 里 BodyVXL/TurretVXL/BarrelVXL 是 ObjectTypeClass 上的 VoxLib*，
    // 在 Resolve_VXL_References 阶段一次性载入。本树的类型只保存了
    // VoxelName/HVAName 字符串（见 UnitTypeClass::Resolve_VXL_References），
    // 因此这里按名字懒加载并缓存，失败者记为 Loaded=false 不再重试
    // ——与原版“载入失败即没有体素、退回 SHP”一致。
    // ------------------------------------------------------------------------
    struct VoxelModelEntry
    {
        char      VoxelName[0x20];
        char      HVAName[0x20];
        VXLClass  VXL;
        HVAClass  HVA;
        bool      HasHVA;
        bool      Loaded;
    };

    std::vector<VoxelModelEntry*>* s_pVoxelModels = nullptr;

    // ------------------------------------------------------------------------
    // Voxel_TryLoadFile - 依次尝试 name、name.vxl / name.hva（含大写扩展名）
    // ------------------------------------------------------------------------
    static bool Voxel_TryLoadVXL(VXLClass& vxl, const char* pName)
    {
        if (pName == nullptr || pName[0] == '\0')
            return false;

        if (std::strchr(pName, '.') != nullptr)
            return vxl.LoadFromFile(pName);

        char path[0x60];
        std::snprintf(path, sizeof(path), "%s.vxl", pName);
        if (vxl.LoadFromFile(path))
            return true;

        std::snprintf(path, sizeof(path), "%s.VXL", pName);
        return vxl.LoadFromFile(path);
    }

    static bool Voxel_TryLoadHVA(HVAClass& hva, const char* pName)
    {
        if (pName == nullptr || pName[0] == '\0')
            return false;

        if (std::strchr(pName, '.') != nullptr)
            return hva.LoadFromFile(pName);

        char path[0x60];
        std::snprintf(path, sizeof(path), "%s.hva", pName);
        if (hva.LoadFromFile(path))
            return true;

        std::snprintf(path, sizeof(path), "%s.HVA", pName);
        return hva.LoadFromFile(path);
    }

    // ------------------------------------------------------------------------
    // Voxel_GetModel - 按 VXL/HVA 名字取（或建立）缓存条目
    // ------------------------------------------------------------------------
    static VoxelModelEntry* Voxel_GetModel(const char* pVoxelName, const char* pHVAName)
    {
        if (pVoxelName == nullptr || pVoxelName[0] == '\0')
            return nullptr;

        if (s_pVoxelModels == nullptr)
            s_pVoxelModels = new std::vector<VoxelModelEntry*>();

        // 名字相同即同一条目（HVA 名字参与比较，炮塔 HVA 可能不同）
        const char* hvaName = (pHVAName != nullptr) ? pHVAName : "";
        for (size_t i = 0; i < s_pVoxelModels->size(); ++i)
        {
            VoxelModelEntry* e = (*s_pVoxelModels)[i];
            if (std::strncmp(e->VoxelName, pVoxelName, sizeof(e->VoxelName) - 1) == 0 &&
                std::strncmp(e->HVAName, hvaName, sizeof(e->HVAName) - 1) == 0)
                return e;
        }

        VoxelModelEntry* entry = new VoxelModelEntry();

        std::strncpy(entry->VoxelName, pVoxelName, sizeof(entry->VoxelName) - 1);
        std::strncpy(entry->HVAName, hvaName, sizeof(entry->HVAName) - 1);

        entry->Loaded = Voxel_TryLoadVXL(entry->VXL, pVoxelName) &&
                        entry->VXL.GetSectionCount() > 0;
        entry->HasHVA = entry->Loaded && Voxel_TryLoadHVA(entry->HVA, hvaName);

        s_pVoxelModels->push_back(entry);
        return entry;
    }
}

void UnitClass::DrawAsVXL(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint)
{
    if (!Type)
        return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr)
        return;

    // 调色板：优先用类型自己的 Palette 转换器，缺失时退回标准调色板。
    ConvertClass* pConvert = Type->PaletteConvert;
    if (pConvert == nullptr)
        pConvert = StandardPalette;

    VoxelModelEntry* pModel = Voxel_GetModel(Type->VoxelName, Type->HVAName);

    // 体素或调色板缺失 -> 原版同样退回 SHP（有 SHP 就画 SHP，都没有就不画）。
    if (pModel == nullptr || !pModel->Loaded || pConvert == nullptr)
    {
        DrawAsSHP(Coords, BoundingRect, Brightness, Tint);
        return;
    }

    // ------------------------------------------------------------------
    // 帧号（asm UnitClass_DrawVXL 0x73B4DA）：
    //     bodyFrame   = CurrentWalkingFrame % BodyHVA->FrameCount
    //     turretFrame = 0；仅当 bodyFrame == 0 时
    //                   turretFrame = TurretWalkFrame % TurretHVA->FrameCount
    // 本树没有 field_144（炮塔行走计数）这个独立字段，用 WalkFrameCounter
    // 顶上；bodyFrame != 0 时炮塔帧固定为 0，与原版一致。
    // ------------------------------------------------------------------
    int32 walkingFrame = WalkFrameCounter;
    if (walkingFrame < 0)
        walkingFrame = -walkingFrame;

    const int32 bodyFrameCount = (pModel->HasHVA && pModel->HVA.GetFrameCount() > 0)
                               ? pModel->HVA.GetFrameCount() : 1;
    const int32 bodyFrame = (bodyFrameCount > 0) ? (walkingFrame % bodyFrameCount) : 0;

    int32 turretFrame = 0;
    if (bodyFrame == 0 && pModel->HasHVA && pModel->HVA.GetFrameCount() > 0)
        turretFrame = walkingFrame % pModel->HVA.GetFrameCount();

    // ------------------------------------------------------------------
    // 裁剪矩形
    // ------------------------------------------------------------------
    Rectangle clip(BoundingRect.X, BoundingRect.Y,
                   BoundingRect.Width, BoundingRect.Height);
    if (clip.Width <= 0 || clip.Height <= 0)
        clip = Rectangle(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

    // 光照：隐身/黑夜由亮度承载（与 DrawAsSHP 同一套系数）。
    int32 adjustedBrightness = Brightness;
    if (IsCloaked())
        adjustedBrightness /= 2;
    if (adjustedBrightness < 0)    adjustedBrightness = 0;
    if (adjustedBrightness > 1000) adjustedBrightness = 1000;

    // ------------------------------------------------------------------
    // 本体（section 0）与炮塔（section 1）。
    //
    // asm 的变换矩阵来自 Locomotion->Get_Transform（vtbl+0x24），本树
    // Locomotion 没有该接口，用朝向角构造绕竖直轴旋转的等价矩阵：
    // 面朝 0 = 北（-Z 方向），facing 每 256 分之一个圆周。
    // ------------------------------------------------------------------
    const float facingRad = static_cast<float>(PrimaryFacing.Value) * 6.28318530718f / 256.0f;
    Matrix4 bodyMatrix = Matrix4::RotationY(-facingRad);

    HVAClass* pHVA = pModel->HasHVA ? &pModel->HVA : nullptr;
    const int32 sectionCount = pModel->VXL.GetSectionCount();

    bool drew = Voxel_Rasterize(pSurface, pConvert, &pModel->VXL, 0,
                                pHVA, bodyFrame, bodyMatrix,
                                Coords, adjustedBrightness, Tint, &clip);

    if (Type->HasTurret && sectionCount > 1)
    {
        const float turretRad = static_cast<float>(TurretFacing.Value) * 6.28318530718f / 256.0f;
        Matrix4 turretMatrix = Matrix4::RotationY(-turretRad);

        if (Voxel_Rasterize(pSurface, pConvert, &pModel->VXL, 1,
                            pHVA, turretFrame, turretMatrix,
                            Coords, adjustedBrightness, Tint, &clip))
            drew = true;
    }

    // 体素存在但一个面都没画出来（空模型）时仍退回 SHP，这是原版行为。
    if (!drew)
    {
        DrawAsSHP(Coords, BoundingRect, Brightness, Tint);
        return;
    }

    // 供 Draw() 定位血条/选中框。
    const Point2D size = Type->Get_Image_Size();
    LastRenderRect = RectangleStruct(Coords.X - size.X / 2, Coords.Y - size.Y / 2,
                                     size.X, size.Y);
}

void UnitClass::DrawAsSHP(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint)
{
    if (!Type) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // 序列只影响帧号，不影响坐标。
    Sequence seq = GetSequence();
    const int32 facing8 = UnitArt::FacingToDir8(PrimaryFacing.Value);

    int32 adjustedBrightness = Brightness;
    if (IsCloaked()) adjustedBrightness = adjustedBrightness / 2;
    if (adjustedBrightness < 0)    adjustedBrightness = 0;
    if (adjustedBrightness > 1000) adjustedBrightness = 1000;

    // ---- ① 真实素材路径 ---------------------------------------------------
    SHPStruct* pShape = Type->GetImageData();
    if (pShape == nullptr) pShape = Type->GetImage();

    const int32 cx = Coords.X;
    const int32 cy = Coords.Y - 8;

    if (pShape != nullptr)
    {
        Rectangle clip(BoundingRect.X, BoundingRect.Y,
                       BoundingRect.Width, BoundingRect.Height);
        if (clip.Width <= 0 || clip.Height <= 0)
            clip = Rectangle(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

        // 帧号 = 序列 * 8 方向 + 当前朝向（单位 SHP 的排布惯例）。
        const int32 frameIndex = static_cast<int32>(seq) * 8 + facing8;

        Point2D pos(cx, cy);
        CC_Draw_Shape(pSurface, Type->PaletteConvert, pShape, frameIndex,
                      &pos, &clip,
                      BlitterFlags::Centered | BlitterFlags::Transparent,
                      0, 0, 0,
                      adjustedBrightness, Tint,
                      nullptr, 0, 0, 0);

        // 供 Draw() 定位血条/选中框：与 CC_Draw_Shape 同一套锚点换算
        //   （Centered 用画布宽高，再补帧原点）。
        {
            const Rectangle fr = pShape->GetFrameBounds(frameIndex);
            const int32 canvasW = (pShape->Width  > 0) ? pShape->Width  : fr.Width;
            const int32 canvasH = (pShape->Height > 0) ? pShape->Height : fr.Height;
            LastRenderRect = RectangleStruct(cx - canvasW / 2 + fr.X,
                                             cy - canvasH / 2 + fr.Y,
                                             fr.Width, fr.Height);
        }
        return;
    }

    // 没有素材：不画。原版同样不会用几何图形顶替 VXL/SHP。
}

void UnitClass::DrawObject(Surface* pSurface, Point2D Coords, RectangleStruct CacheRect, int32 Brightness, int32 Tint)
{
    if (!pSurface || !Type) return;

    // Delegate to VXL or SHP based on the type's rendering mode.
    if (IsVoxel()) {
        DrawAsVXL(Coords, CacheRect, Brightness, Tint);
    } else {
        DrawAsSHP(Coords, CacheRect, Brightness, Tint);
    }
}

void UnitClass::Draw(Point2D& point, RectangleStruct& rect)
{
    if (!Type) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // 逻辑序列（只改帧号，不改坐标）。
    if (Deployed) {
        SetSequence(Sequence::DeployedIdle);
    } else if (IsMoving()) {
        SetSequence(Sequence::Walk);
    } else {
        SetSequence(Sequence::Ready);
    }

    // 光照：隐身单位半亮，夜间/雾用整帧亮度系数承载。
    int32 brightness = 1000;
    int32 tint = 0;
    if (IsCloaked()) brightness = 480;

    const int32 screenX = point.X;
    const int32 screenY = point.Y;

    // ---- ① 阴影 ----------------------------------------------------------
    Point2D shadowPoint(screenX, screenY);
    DrawShadow(shadowPoint);

    // ---- ② 本体 ----------------------------------------------------------
    DrawObject(pSurface, point, rect, brightness, tint);

    // ---- ③ 选中框 / 血条 -------------------------------------------------
    //   包围盒取上一次本体绘制的实际 dst（SHP 帧 / 体素投影）；
    //   没有素材时 DrawAs* 不会写它，这里再兜一个默认框。
    RectangleStruct box = LastRenderRect;
    if (box.Width <= 0 || box.Height <= 0)
        box = RectangleStruct(screenX - 14, screenY - 18, 28, 22);

    if (IsSelected)
        ObjectUI::DrawSelectionBrackets(pSurface, box, 0x07E0);

    if (MaxHealth > 0 && Health > 0 && Health < MaxHealth)
        ObjectUI::DrawHealthBar(pSurface, box, Health * 100 / MaxHealth);

    if (g_bDebugMode)
        g_RenderDebugStats.DrawCalls++;
}

void UnitClass::DrawShadow(Point2D& point)
{
    if (!Type) return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr) return;

    // 原版 ObjectClass::Draw_Shadow：用 ImageSHP 的 ShadowIndex 帧再画一遍，
    //   由 Darken 位压成暗色。载具/飞机离地越高，投影往屏幕下方偏得越多。
    SHPStruct* pShape = Type->GetImageData();
    if (pShape == nullptr) pShape = Type->GetImage();
    if (pShape == nullptr) return;

    if (IsCarryAllFly || IsJumpJet)
        point.Y += 15;

    Point2D pos(point.X, point.Y);
    Rectangle clip(0, 0, pSurface->GetWidth(), pSurface->GetHeight());

    CC_Draw_Shape(pSurface, Type->PaletteConvert, pShape,
                  Type->ShadowIndex, &pos, &clip,
                  BlitterFlags::Centered | BlitterFlags::Transparent | BlitterFlags::Darken,
                  0, 0, 0, 1000, 0, nullptr, 0, 0, 0);
}

int32 UnitClass::GetZBias() const
{
    // Ground units have a Z-bias of 0; air units are elevated.
    if (IsCarryAllFly || IsJumpJet) return 1000;
    if (IsUnderground) return -500;
    return 0;
}

// =============================================================================
// State / deactivation
// =============================================================================
bool UnitClass::IsDeactivated() const
{
    if (IsInLimbo) return true;
    if (Health <= 0) return true;
    if (unknown_7FC > 0) return true; // DisableCount
    return false;
}

// =============================================================================
// Update subsystems
// =============================================================================
void UnitClass::UpdateTube()
{
    if (!IsSubterranean) return;

    // Subterranean units update their underground tunnel position.
    if (IsUnderground) {
        DeathFrameCounter++; // reuse as underground frame timer
        if (DeathFrameCounter > 60) {
            // Tunnel collapse or surface after timeout.
            IsUnderground = false;
            DeathFrameCounter = 0;
        }
    }
}

void UnitClass::UpdateRotation()
{
    if (!HasTurret()) return;

    // Rotate the turret toward the primary facing direction.
    DirStruct currentFacing = TurretDir;
    DirStruct targetFacing = PrimaryFacing;

    int32 current = static_cast<int32>(currentFacing.Value);
    int32 target = static_cast<int32>(targetFacing.Value);
    int32 diff = target - current;

    // Normalize to [-128, 127] for shortest rotation.
    if (diff > 128) diff -= 256;
    if (diff < -128) diff += 256;

    int32 rotSpeed = TURRET_ROT_SPEED_DEFAULT;
    if (Type && Type->ROT > 0) {
        rotSpeed = Type->ROT;
    }

    if (diff > 0) {
        if (diff > rotSpeed) diff = rotSpeed;
        TurretRotation += diff;
    } else if (diff < 0) {
        if (diff < -rotSpeed) diff = -rotSpeed;
        TurretRotation += diff;
    }

    TurretDir = targetFacing;

    // Also update barrel elevation to follow turret.
    int32 targetPitch = Pitch;
    int32 barrelDiff = targetPitch - BarrelRotation;
    int32 barrelSpeed = TURRET_ROT_SPEED_DEFAULT / 2;
    if (barrelDiff > barrelSpeed) barrelDiff = barrelSpeed;
    if (barrelDiff < -barrelSpeed) barrelDiff = -barrelSpeed;
    BarrelRotation += barrelDiff;
    BarrelDir = TurretDir;
}

void UnitClass::UpdateEdgeOfWorld()
{
    // Check if the unit has wandered off the map edge.
    CoordStruct pos = GetCoords();
    if (pos.X < 0 || pos.Y < 0) {
        // Unit is outside the map; turn it around.
        Stop();
        return;
    }

    // Check map bounds through the map instance.
    if (MapClass::Instance) {
        if (!MapClass::Instance->IsWithinUsableArea(pos)) {
            Stop();
        }
    }
}

void UnitClass::UpdateFiring()
{
    // Decrement fire recharge timer each frame.
    if (FireRechargeTimer > 0) {
        FireRechargeTimer--;
    }

    // Update spark/fire effects if active.
    if (FireDamageTimer > 0) {
        FireDamageTimer--;
    }
    if (SparkyCounter > 0) {
        SparkyCounter--;
    }

    // Update muzzle flash state.
    if (unknown_810 > 0) {
        unknown_810--;
    }
}

void UnitClass::UpdateVisceroid()
{
    // Visceroids are special units that spawn from tiberium.
    // This updates their movement and splitting behavior.
    if (!IsButtMissile) return;

    // Visceroids wander randomly and split when damaged enough.
    if (Health < MaxHealth / 2) {
        // Mark for potential splitting on next AI cycle.
        unknown_814 = 1;
    }
}

void UnitClass::UpdateDisguise()
{
    if (!IsDisguised()) return;

    // Update disguise state: if the unit fires or takes damage, reveal.
    if (FireRechargeTimer > 0 || FireDamageTimer > 0) {
        UnDisguise();
    }
}

void UnitClass::Explode()
{
    // Trigger the death explosion for this unit.
    DeathFrameCounter = 0;

    // Set the death sequence based on unit type.
    if (IsTank || IsWalker) {
        SetSequence(Sequence::Die1);
    } else if (IsCarryall || IsJumpJet) {
        SetSequence(Sequence::DieFly);
    } else {
        SetSequence(Sequence::Die2);
    }

    // If the unit has a death weapon, arm it.
    if (Type && Type->DeathWeaponIndex >= 0) {
        unknown_818 = static_cast<DWORD>(Type->DeathWeaponIndex);
    }
}

// =============================================================================
// Deploy / harvest / flag
// =============================================================================
bool UnitClass::GotoClearSpot()
{
    if (!Type) return false;

    // Find a nearby cell that is unoccupied for deployment or unloading.
    CoordStruct currentPos = GetCoords();
    CellStruct currentCell = Math::CoordToCell(currentPos);

    // Search in a spiral around the current position.
    for (int32 radius = 1; radius <= 5; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (abs(dx) != radius && abs(dy) != radius) continue;
                CellStruct testCell(static_cast<int32>(currentCell.X + dx), static_cast<int32>(currentCell.Y + dy));
                if (MapClass::Instance && MapClass::Instance->IsWithinUsableArea(testCell.X, testCell.Y)) {
                    if (!MapClass::Instance->IsCellOccupied(testCell)) {
                        CoordStruct dest = Math::CellToCoord(testCell);
                        SetDestination(dest);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool UnitClass::TryToDeploy()
{
    if (!CanDeploy()) return false;
    if (Deploying) return false;
    if (Deployed) return false;

    Deploying = true;
    Undeploying = false;
    unknown_7E0 = 0; // reset deploy animation frame counter
    SetSequence(Sequence::Deploy);
    return true;
}

void UnitClass::Deploy()
{
    Deploying = false;
    Deployed = true;
    Undeploying = false;
    SetSequence(Sequence::DeployedIdle);
    unknown_7E0 = 0;
}

void UnitClass::Undeploy()
{
    if (!Deployed) return;

    Undeploying = true;
    Deploying = false;
    Deployed = false;
    SetSequence(Sequence::Undeploy);
    unknown_7E0 = 0;
}

bool UnitClass::Harvesting()
{
    if (!IsHarvestingTiberium) return false;
    if (!CanHarvest()) return false;

    // Check if at a tiberium field and harvest.
    CoordStruct currentPos = GetCoords();
    CellStruct cellPos = Math::CoordToCell(currentPos);

    // Verify the cell has tiberium.
    if (MapClass::Instance) {
        LandType land = MapClass::Instance->GetLandType(cellPos);
        if (land != LandType::Tiberium && land != LandType::Weeds) {
            // No tiberium here; stop harvesting.
            IsHarvestingTiberium = false;
            return false;
        }
    }

    // Perform the harvest extraction.
    int32 amount = HarvestRate;
    if (amount <= 0) amount = HARVEST_RATE_DEFAULT;

    HarvestAmount += amount;
    TotalTiberiumValue += amount;

    if (IsHarvestingTooMuch()) {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
    }

    return true;
}

bool UnitClass::FlagAttach(int32 nHouseIdx)
{
    if (FlagHouseIndex >= 0) return false;
    if (nHouseIdx < 0) return false;
    FlagHouseIndex = nHouseIdx;
    return true;
}

bool UnitClass::FlagRemove()
{
    if (FlagHouseIndex < 0) return false;
    FlagHouseIndex = -1;
    return true;
}

// =============================================================================
// APC door
// =============================================================================
void UnitClass::APCCloseDoor()
{
    if (!IsAPC) return;
    // Close the APC door for passenger loading/unloading.
    SetSequence(Sequence::Ready);
    Unloading = false;
}

void UnitClass::APCOpenDoor()
{
    if (!IsAPC) return;
    // Open the APC door to allow passengers to exit.
    SetSequence(Sequence::Unload);
    Unloading = true;
}

// =============================================================================
// Combat decisions
// =============================================================================
bool UnitClass::ShouldCrashIt(TechnoClass* pTarget)
{
    if (!pTarget) return false;
    if (!Type) return false;

    // Air units should be shot down by anti-air weapons.
    if (pTarget->IsInAir()) {
        return IsAntiAir;
    }

    // Ground units can always be targeted unless they are allies.
    if (Is_Ally(pTarget)) return false;

    return true;
}

AbstractClass* UnitClass::AssignDestination(AbstractClass* pTarget)
{
    if (!pTarget) return nullptr;

    // Convert the target into a destination coordinate for movement.
    CoordStruct destCoord;
    pTarget->GetCoords(&destCoord);
    SetDestination(destCoord);

    return pTarget;
}

bool UnitClass::AStarAttempt(const CellStruct& cell1, const CellStruct& cell2)
{
    // Attempt A* pathfinding between two cells.
    if (!MapClass::Instance) return false;

    // Check if both cells are valid and passable.
    if (!MapClass::Instance->IsWithinUsableArea(cell1.X, cell1.Y)) return false;
    if (!MapClass::Instance->IsWithinUsableArea(cell2.X, cell2.Y)) return false;

    // Simple heuristic: check if a straight-line path is clear.
    int32 dx = cell2.X - cell1.X;
    int32 dy = cell2.Y - cell1.Y;
    int32 steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

    for (int32 i = 0; i <= steps; ++i) {
        int16 x = static_cast<int16>(cell1.X + (dx * i) / (steps > 0 ? steps : 1));
        int16 y = static_cast<int16>(cell1.Y + (dy * i) / (steps > 0 ? steps : 1));
        CellStruct stepCell(x, y);
        if (MapClass::Instance->IsCellOccupied(stepCell)) {
            return false; // Path blocked
        }
    }

    return true; // Path is clear
}

bool UnitClass::Find_Path(const CellStruct& start, const CellStruct& dest,
                          DynamicVectorClass<CellStruct>& outPath)
{
    outPath.Clear();
    if (MapClass::Instance == nullptr || Type == nullptr)
        return false;

    MapClass* pMap = MapClass::Instance;
    if (!pMap->IsWithinUsableArea(start.X, start.Y) ||
        !pMap->IsWithinUsableArea(dest.X, dest.Y))
    {
        return false;
    }

    const int32 width  = pMap->MapWidth;
    const int32 height = pMap->MapHeight;
    const int32 total  = width * height;
    if (total <= 0)
        return false;

    // A* state tables (reused per-call, allocated once).
    std::vector<int32>  gScore(total, 0x7FFFFFFF);
    std::vector<int32>  fScore(total, 0x7FFFFFFF);
    std::vector<int32>  cameFrom(total, -1);
    std::vector<bool>   inClosed(total, false);

    // Prefer the hierarchical route when the abstraction layer can find
    // one; the per-cell search below remains as the regular fallback.
    DynamicVectorClass<CellStruct> hierPath;
    if (CellClass::Pathfinding_Hierarchical(start, dest, hierPath, Type->MoveZone))
    {
        for (int32 i = 0; i < hierPath.Count; ++i)
            outPath.Add(hierPath[i]);
        return true;
    }

    const int32 startIdx = start.Y * width + start.X;
    const int32 destIdx  = dest.Y  * width + dest.X;

    auto idx = [width](int32 x, int32 y) { return y * width + x; };
    auto heuristic = [](int32 ax, int32 ay, int32 bx, int32 by) {
        int32 dx = abs(bx - ax);
        int32 dy = abs(by - ay);
        // Octile distance: diagonal moves cost ~1.414, straight moves 1.0.
        int32 diag = dx < dy ? dx : dy;
        int32 straight = dx + dy - 2 * diag;
        return straight * 256 + diag * 362;   // 256 = 1.0 cell, 362 = sqrt(2)*256
    };

    MovementZone zone = Type->MoveZone;

    // Open list: min-heap ordered by fScore.
    std::priority_queue<std::pair<int32, int32>,
                        std::vector<std::pair<int32, int32>>,
                        std::greater<std::pair<int32, int32>>> openList;
    gScore[startIdx] = 0;
    fScore[startIdx] = heuristic(start.X, start.Y, dest.X, dest.Y);
    openList.push({ fScore[startIdx], startIdx });

    // Direction offsets in cell space: N, NE, E, SE, S, SW, W, NW
    static const int8 dirDx[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };
    static const int8 dirDy[8] = { -1, -1,  0,  1,  1,  1,  0, -1 };

    while (!openList.empty())
    {
        auto [curF, cur] = openList.top();
        openList.pop();
        if (inClosed[cur])
            continue;
        if (cur == destIdx)
            break;
        inClosed[cur] = true;

        const int32 cx = cur % width;
        const int32 cy = cur / width;

        for (int32 d = 0; d < 8; ++d)
        {
            int32 nx = cx + dirDx[d];
            int32 ny = cy + dirDy[d];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                continue;

            int32 nIdx = idx(nx, ny);
            if (inClosed[nIdx])
                continue;

            CellClass* pCell = &pMap->CellArray[nIdx];
            if (pCell == nullptr)
                continue;

            // Diagonal corner-cut check: both orthogonal neighbours must be
            // passable, otherwise the unit clips through walls.
            if (d & 1)
            {
                CellClass* pA = &pMap->CellArray[idx(cx + dirDx[d - 1], cy + dirDy[d - 1])];
                CellClass* pB = &pMap->CellArray[idx(cx + dirDx[(d + 1) & 7], cy + dirDy[(d + 1) & 7])];
                if (pA == nullptr || pB == nullptr)
                    continue;
                if (!pA->PassableFor(zone) || !pB->PassableFor(zone))
                    continue;
            }

            if (!pCell->PassableFor(zone))
                continue;

            // Move cost: straight 256, diagonal 362 (in 1/256 cell units),
            // plus a terrain penalty for rough ground.
            int32 moveCost = (d & 1) ? 362 : 256;
            if (pCell->Get_Land_Type() == LandType::Rough ||
                pCell->Get_Land_Type() == LandType::Weeds)
            {
                moveCost += 128;
            }

            int32 tentative = gScore[cur] + moveCost;
            if (tentative < gScore[nIdx])
            {
                gScore[nIdx] = tentative;
                int32 h = heuristic(nx, ny, dest.X, dest.Y);
                fScore[nIdx] = tentative + h;
                cameFrom[nIdx] = cur;
                openList.push({ fScore[nIdx], nIdx });
            }
        }
    }

    if (gScore[destIdx] == 0x7FFFFFFF)
        return false;   // unreachable

    // Reconstruct the path (dest -> start), then reverse.
    std::vector<int32> rev;
    for (int32 c = destIdx; c != -1; c = cameFrom[c])
    {
        rev.push_back(c);
        if (c == startIdx)
            break;
    }
    if (rev.empty() || rev.back() != startIdx)
        return false;

    for (auto it = rev.rbegin(); it != rev.rend(); ++it)
    {
        int32 c = *it;
        outPath.Add(CellStruct(static_cast<int16>(c % width), static_cast<int16>(c / width)));
    }
    return true;
}


// =============================================================================
// Mouse interaction
// =============================================================================
Action UnitClass::MouseOverCell(CellStruct const* pCell, bool checkFog, bool ignoreForce) const
{
    if (!pCell) return Action::None;

    // Determine what action the player can perform on this cell.
    if (!Type) return Action::None;

    // Check if the cell is valid.
    if (MapClass::Instance && !MapClass::Instance->IsWithinUsableArea(pCell->X, pCell->Y)) {
        return Action::NoMove;
    }

    // If checking fog, verify the cell is visible.
    if (checkFog && MapClass::Instance) {
        CellClass* pCellObj = MapClass::Instance->GetCellAt(*pCell);
        if (pCellObj && !pCellObj->Is_Visible()) {
            return Action::None;
        }
    }

    (void)ignoreForce;

    // Default: the unit can move to this cell.
    if (CanDeploy() && Deployed) {
        return Action::NoDeploy; // Already deployed, cannot move
    }

    return Action::Move;
}

Action UnitClass::MouseOverObject(ObjectClass const* pObject, bool ignoreForce) const
{
    if (!pObject) return Action::None;
    if (!Type) return Action::None;

    // Determine action based on the target object type.
    if (pObject == this) {
        return Action::Select;
    }

    // Check if the target is an enemy.
    auto pTechno = const_cast<ObjectClass*>(pObject);
    if (Is_Enemy(pTechno->Get_Owner())) {
        return Action::Attack;
    }

    // Check if the target is a friendly structure the unit can enter.
    if (Is_Ally(pTechno->Get_Owner())) {
        return Action::Enter;
    }

    (void)ignoreForce;

    return Action::None;
}

// =============================================================================
// Occupation bits
// =============================================================================
void UnitClass::MarkAllOccupationBits(const CoordStruct& coords)
{
    // Mark all cells occupied by this unit at the given coordinates.
    CellStruct cell = Math::CoordToCell(coords);
    if (MapClass::Instance) {
        MapClass::Instance->MarkCellOccupied(cell, true);

        // For large units, mark adjacent cells too.
        if (Type && Type->SizeLimit > 1) {
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y)), true);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X), static_cast<int32>(cell.Y + 1)), true);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y + 1)), true);
        }
    }
}

void UnitClass::UnmarkAllOccupationBits(const CoordStruct& coords)
{
    // Unmark all cells previously occupied by this unit.
    CellStruct cell = Math::CoordToCell(coords);
    if (MapClass::Instance) {
        MapClass::Instance->MarkCellOccupied(cell, false);

        if (Type && Type->SizeLimit > 1) {
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y)), false);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X), static_cast<int32>(cell.Y + 1)), false);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y + 1)), false);
        }
    }
}

// =============================================================================
// Target management
// =============================================================================
void UnitClass::SetTarget(AbstractClass* pTarget)
{
    // Store the target pointer across two adjacent DWORD fields.
    uintptr_t ptrVal = reinterpret_cast<uintptr_t>(pTarget);
    unknown_7E8 = static_cast<DWORD>(ptrVal);
    unknown_7EC = static_cast<DWORD>(ptrVal >> 32);
}

AbstractClass* UnitClass::GetTarget() const
{
    uintptr_t ptrVal = static_cast<uintptr_t>(unknown_7E8) |
                       (static_cast<uintptr_t>(unknown_7EC) << 32);
    return reinterpret_cast<AbstractClass*>(ptrVal);
}

void UnitClass::ClearTarget()
{
    unknown_7E8 = 0;
    unknown_7EC = 0;
}

bool UnitClass::HasTarget() const
{
    return unknown_7E8 != 0 || unknown_7EC != 0;
}

// =============================================================================
// Mission management
void UnitClass::SetMission(Mission mission)
{
    unknown_7F0 = static_cast<DWORD>(mission);

    // ★ 任务层同步（asm +0xA8 currentMission）：
    //   MissionClass_Update 按 currentMission 走 off_5B34E8 的 32 路分派，
    //   因此载具把任务同时写进任务层，任务状态机才看得到。
    MissionClass::CurrentMission = mission;

    // Apply mission-specific sequence changes.
    switch (mission) {
        case Mission::Sleep:
            SetSequence(Sequence::Ready);
            break;
        case Mission::Move:
            SetSequence(Sequence::Walk);
            break;
        case Mission::Guard:
        case Mission::AreaGuard:
            SetSequence(Sequence::Guard);
            break;
        case Mission::Harvest:
            SetSequence(Sequence::Harvest);
            break;
        case Mission::Unload:
            SetSequence(Sequence::Unload);
            break;
        default:
            break;
    }
}

Mission UnitClass::GetMission() const
{
    return static_cast<Mission>(unknown_7F0);
}

void UnitClass::QueueMission(Mission mission)
{
    unknown_7F4 = static_cast<DWORD>(mission);

    // 同步排队槽（asm +0xB0 QueuedMission）：MissionClass::ResetMission /
    // Override_Mission 都要读它。
    MissionClass::QueuedMission = mission;
}

// =============================================================================
// Mission implementations
// =============================================================================
void UnitClass::MissionAttack()
{
    SetMission(Mission::Attack);
    AbstractClass* pTarget = GetTarget();
    if (pTarget == nullptr) {
        MissionHunt();
        return;
    }

    // Select the best weapon and verify range before opening fire.
    int32 weaponIdx = SelectWeapon(pTarget);
    if (weaponIdx < 0) {
        MissionMove();
        return;
    }

    if (!IsCloseEnoughToTarget(pTarget, weaponIdx)) {
        // Approach while keeping the target locked.
        CoordStruct targetPos;
        pTarget->GetCoords(&targetPos);
        SetDestination(targetPos);
        return;
    }

    // Face the target and fire.
    CoordStruct targetPos;
    pTarget->GetCoords(&targetPos);
    CoordStruct myPos = GetCoords();

    double ang = std::atan2(static_cast<double>(targetPos.Y - myPos.Y),
                            static_cast<double>(targetPos.X - myPos.X));
    int32 oct = static_cast<int32>(std::round(ang / (3.14159265358979323846 / 4.0)));
    oct = (oct + 8) % 8;
    static const DirType octDir[8] = {
        DirType::E, DirType::SE, DirType::S, DirType::SW,
        DirType::W, DirType::NW, DirType::N, DirType::NE
    };
    SetFacing(Facing::FromDirType(octDir[oct]));

    // asm UnitClass_Mission_Attack 尾段 → UnitClass_Fire → TechnoClass_Fire。
    //   直接走 Fire() 才能完成"扣弹 + 后座 + 波武器 + 建弹丸"的完整记账。
    Fire(pTarget, weaponIdx);
}

void UnitClass::MissionMove()
{
    SetMission(Mission::Move);
    SetSequence(Sequence::Walk);
}

void UnitClass::MissionGuard()
{
    SetMission(Mission::Guard);
    SetSequence(Sequence::Guard);
}

void UnitClass::MissionSleep()
{
    SetMission(Mission::Sleep);
    SetSequence(Sequence::Ready);
}

void UnitClass::MissionHunt()
{
    SetMission(Mission::Hunt);
    // Hunting units actively seek out enemies.
    SetSequence(Sequence::Walk);
}

void UnitClass::MissionReturn()
{
    SetMission(Mission::Return);
    // Return to a refinery (harvester) or base.
    if (IsHarvester && IsDumpingTiberium) {
        SetSequence(Sequence::Walk);
    }
}

void UnitClass::MissionStop()
{
    SetMission(Mission::Stop);
    Stop();
    SetSequence(Sequence::Ready);
}

void UnitClass::MissionHarvest()
{
    SetMission(Mission::Harvest);
    if (CanHarvest() && !IsHarvestingTooMuch()) {
        StartHarvesting();
        SetSequence(Sequence::Harvest);
    }
}

void UnitClass::MissionUnload()
{
    SetMission(Mission::Unload);
    if (IsAPC && NonPassengerCount > 0) {
        APCOpenDoor();
        SetSequence(Sequence::Unload);
    }
    if (IsHarvester && HarvestAmount > 0) {
        IsDumpingTiberium = true;
        HarvestAmount = 0;
        TotalTiberiumValue = 0;
    }
}

void UnitClass::MissionEnter()
{
    SetMission(Mission::Enter);
    SetSequence(Sequence::Enter);
}

void UnitClass::MissionPatrol()
{
    SetMission(Mission::Patrol);
    SetSequence(Sequence::Walk);
}

void UnitClass::MissionAreaGuard()
{
    SetMission(Mission::AreaGuard);
    SetSequence(Sequence::Guard);
}

void UnitClass::UpdateMission()
{
    Mission currentMission = GetMission();

    // Process the queued mission if current is complete.
    if (unknown_7F4 != 0) {
        Mission queued = static_cast<Mission>(unknown_7F4);
        if (currentMission != queued) {
            SetMission(queued);
            unknown_7F4 = 0;
        }
    }

    // Update deploy animation timer.
    if (Deploying) {
        if (unknown_7E0 < static_cast<DWORD>(DEPLOY_ANIMATION_FRAMES)) {
            unknown_7E0++;
        } else {
            Deploy();
        }
    } else if (Undeploying) {
        if (unknown_7E0 < static_cast<DWORD>(DEPLOY_ANIMATION_FRAMES)) {
            unknown_7E0++;
        } else {
            Undeploying = false;
            SetSequence(Sequence::Ready);
            unknown_7E0 = 0;
        }
    }
}

// =============================================================================
// AI updates
// =============================================================================
void UnitClass::AI_Update()
{
    if (IsInLimbo) return;
    if (Health <= 0) return;

    // Update all subsystems.
    UpdateRotation();
    UpdateFiring();
    UpdateMission();
    UpdateDisguise();

    // Update iron curtain / force shield timers.
    if (IronCurtainTimer > 0) {
        IronCurtainTimer--;
    }
    if (ForceShieldTimer > 0) {
        ForceShieldTimer--;
    }

    // Update EMP timer.
    if (unknown_7E4 > 0) {
        unknown_7E4--;
    }

    // Update temporal timer.
    if (TemporalTimer > 0) {
        TemporalTimer--;
    }

    // Update carryall flight.
    if (IsCarryall) {
        if (IsCarryAllFly) {
            CoordStruct currentPos = GetCoords();
            if (currentPos.Z < CARRYALL_PICKUP_HEIGHT) {
                currentPos.Z += 20;
                SetCoords(currentPos);
            }
        } else {
            CoordStruct currentPos = GetCoords();
            if (currentPos.Z < CARRYALL_CRUISE_HEIGHT) {
                currentPos.Z += 15;
                SetCoords(currentPos);
            } else if (currentPos.Z > CARRYALL_CRUISE_HEIGHT) {
                currentPos.Z -= 10;
                SetCoords(currentPos);
            }
        }
    }
}

void UnitClass::Combat_AI()
{
    if (IsInLimbo) return;
    if (Health <= 0) return;
    if (unknown_7E4 > 0) return; // EMPed, cannot fight

    // Check if we have a target and can fire.
    if (!HasTarget()) return;

    AbstractClass* pTarget = GetTarget();
    if (!pTarget) return;

    // asm: 武器选择 → 射程判定 → TechnoClass_Fire。
    const int32 weaponIdx = SelectWeapon(pTarget);
    if (weaponIdx < 0) return;

    // 射程外由 Mission_Move 负责接近，本帧不开火。
    if (!IsCloseEnoughToTarget(pTarget, weaponIdx)) return;

    // ★ 走 TechnoClass_Fire（建弹丸），不要只做 MuzzleFlash。
    Fire(pTarget, weaponIdx);
}

void UnitClass::Movement_AI()
{
    if (IsInLimbo) return;
    if (Health <= 0) return;
    if (unknown_7E4 > 0) return; // EMPed, cannot move

    // Update pathfinding and movement.
    if (Is_Moving()) {
        if (Has_Path()) {
            CoordStruct nextStep = Peek_Next_Path();
            SetDestination(nextStep);
        }
    }

    // Update subterranean tunnel state.
    if (IsSubterranean) {
        UpdateTube();
    }

    // Check map edges.
    UpdateEdgeOfWorld();
}

// =============================================================================
// Combat - firing and weapons
//
// asm: UnitClass_Fire (0x741340, retn 8；调用点 UnitClass_Update 的武器分支)
//   void __thiscall UnitClass_Fire(UnitClass *this, WeaponTypeClass *a3,
//                                  int idxWeapon)
//     weap = [vt+GetWeapon](idx)            ; 取槽
//     if (!weap) return
//     ; FiringSync —— CurrentSpawnIndex % Burst < 2 时按
//     ;   FiringSyncFrame0/1 决定本发是否落在同步帧上
//     ; 前置门：call [vt+GetObjectFiringState](target, idx, 1)
//     ;   != 0（打不了）→ 直接返回
//     bullet = TechnoClass_Fire(target, idx) ; ★ 真正的建弹丸入口
//     if (!bullet) return
//     ; Ammo / ManualReload → Techno_Update_Reloading
//     ; FiringFrames -> field_6C0（开火动画计时）
//     ; bullet->Type->Scalable -> BulletClass_InitScalable
//     ; DisguiseWhenStill → 记录开火帧
//
//   原实现只做了 muzzle flash + ROF，**不产生弹丸**，整条开火链因此断在
//   这里；本批把它接回 TechnoClass::Fire（= asm 的 TechnoClass_Fire）。
// =============================================================================
void UnitClass::Fire_At(TargetClass* pTarget, int32 weaponIndex)
{
    if (!Type) return;
    if (weaponIndex < 0 || weaponIndex >= GetWeaponCount()) return;

    // asm: cmp [esi+FireRechargeTimer],0 / jnz —— 冷却中不开火。
    if (FireRechargeTimer > 0) return;

    // asm: TargetClass* → AbstractClass*
    AbstractClass* pAbsTarget =
        reinterpret_cast<AbstractClass*>(static_cast<void*>(pTarget));

    // asm UnitClass_Fire: GetObjectFiringState(target, slot, rangeMatters=1)
    //   非 0 表示"当前不能打"（无武器 / 射程不足 / 目标不可攻击）。
    //   OpenYR 以武器槽 + 射程判定等价表达（Can_Fire 的 +2ACh 分支另见存疑项）。
    if (pAbsTarget != nullptr && !IsCloseEnoughToTarget(pAbsTarget, weaponIndex))
        return;

    // asm: 开火动画起始（muzzle flash / FiringSync）。
    MuzzleFlash(weaponIndex);

    // asm: call TechnoClass_Fire —— ★ 建弹丸。
    BulletClass* pBullet = Fire(pAbsTarget, weaponIndex);

    // asm: test eax,eax / jz loc_74147F
    if (pBullet == nullptr)
        return;

    // asm: mov eax,[edi+BulletClass.Type] / test Scalable /
    //      call BulletClass_InitScalable
    if (pBullet->Class != nullptr && pBullet->Class->Scalable)
        pBullet->InitScalable();

    // asm: FiringFrames -> lea edx,[ecx+ecx-1] / mov [esi+field_6C0],edx
    if (Type->FiringFrames > 0)
        unknown_810 = Type->FiringFrames * 2 - 1;

    // asm: ROF 记账（Fire_Impl 已按 LastFireFrame 门控，这里补齐类内计时器）
    const int32 rof = GetROF();
    if (rof > 0)
        FireRechargeTimer = rof;

    LastFireFrame = Game::CurrentFrame;
    OnFired(weaponIndex);
}

bool UnitClass::Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const
{
    if (!Type) return false;
    if (weaponIndex < 0 || weaponIndex >= GetWeaponCount()) return false;
    if (FireRechargeTimer > 0) return false;
    if (unknown_7E4 > 0) return false; // EMPed

    // Check if the unit is in a state that allows firing.
    if (Deploying || Undeploying) return false;

    // Check target range if a target is provided.
    if (pTarget) {
        CoordStruct myPos = GetCoords();
        CoordStruct targetPos;
        pTarget->GetCoords(&targetPos);
        int32 distance = myPos.DistanceFrom(targetPos);
        int32 range = GetWeaponRange(weaponIndex);
        if (distance > range) return false;
    }

    return true;
}

int32 UnitClass::GetWeaponRange(int32 weaponIndex) const
{
    if (!Type) return 0;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return 0;

    // Return the range of the specified weapon.
    WeaponStruct& ws = Type->Weapons[weaponIndex];
    if (!ws.WeaponType) return 0;

    // Range is stored in leptons; use the weapon type's range.
    return static_cast<int32>(Type->GuardRange);
}

int32 UnitClass::GetWeaponDamage(int32 weaponIndex) const
{
    if (!Type) return 0;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return 0;

    WeaponStruct& ws = Type->Weapons[weaponIndex];
    if (!ws.WeaponType) return 0;

    // Return the weapon's damage value.
    return ws.dummy;
}

void UnitClass::MuzzleFlash(int32 weaponIndex)
{
    if (!Type) return;
    if (weaponIndex < 0 || weaponIndex >= GetWeaponCount()) return;

    // Activate the muzzle flash animation for the specified weapon.
    unknown_810 = 3; // muzzle flash lasts 3 frames

    // Set the firing sequence.
    if (HasTurret()) {
        SetSequence(Sequence::FireUp);
    }
}

void UnitClass::OnFired(int32 weaponIndex)
{
    // Post-fire processing.
    (void)weaponIndex;

    // If the unit is disguised, reveal the disguise after firing.
    if (IsDisguised()) {
        UnDisguise();
    }
}

int32 UnitClass::GetWeaponCount() const
{
    if (!Type) return 0;
    return Type->WeaponCount;
}

// =============================================================================
// Damage and death
// =============================================================================
void UnitClass::TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead)
{
    if (damage <= 0) return;

    // Iron curtained / force shielded units take no damage.
    if (IronCurtainTimer > 0 || ForceShieldTimer > 0) return;

    // Apply armor reduction.
    int32 armor = GetArmor();
    int32 actualDamage = damage;
    if (armor > 0) {
        actualDamage = damage * 100 / (100 + armor);
    }

    Health -= actualDamage;
    if (Health < 0) Health = 0;

    // If the unit is on fire, take additional damage.
    if (FireDamageTimer > 0 && pWarhead) {
        FireDamageTimer += actualDamage / 10;
    }

    // If health reaches zero, destroy the unit.
    if (Health <= 0) {
        OnDestroyed();
    }

    (void)pSource; (void)pWarhead;
}

void UnitClass::OnDestroyed()
{
    Explode();

    // Drop a crate if the unit type allows it.
    if (CanCrate()) {
        CreateCrate();
    }

    // Remove from the unit array.
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

void UnitClass::OnCaptured(HouseClass* pNewOwner)
{
    if (!pNewOwner) return;

    // Change ownership of the unit.
    Owner = pNewOwner;

    // Reset combat state.
    ClearTarget();
    Stop();

    // If the unit was harvesting, stop.
    StopHarvesting();

    // If the unit was deployed, undeploy.
    if (Deployed) {
        Undeploy();
    }
}

void UnitClass::OnVeterancyUp()
{
    VeterancyLevel++;
    if (VeterancyLevel > 2) VeterancyLevel = 2;

    // Heal the unit partially on promotion.
    int32 healAmount = MaxHealth / 4;
    Repair(healAmount);
}

// =============================================================================
// Visibility and sight
// =============================================================================
bool UnitClass::IsVisibleTo(HouseClass* pHouse) const
{
    if (!pHouse) return false;
    if (Is_Ally(pHouse)) return true;

    // Check if the unit is within sight range of any of the house's units.
    // Cloaked units are not visible to enemies.
    if (IsCloaked()) {
        return false;
    }

    // Check map visibility at the unit's position.
    CoordStruct pos = GetCoords();
    if (MapClass::Instance) {
        CellStruct cell = Math::CoordToCell(pos);
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell && pCell->Is_Visible()) {
            return true;
        }
    }

    return false;
}

void UnitClass::RevealTo(HouseClass* pHouse)
{
    if (!pHouse) return;
    // Reveal the area around this unit to the specified house.
    // This is used when a unit exits fog of war.
    (void)pHouse;
}

int32 UnitClass::GetSightRange() const
{
    if (!Type) return 0;
    return Type->SightRange;
}

// =============================================================================
// Health and armor
// =============================================================================
int32 UnitClass::GetArmor() const
{
    if (!Type) return 0;
    return static_cast<int32>(Type->ArmorType);
}

int32 UnitClass::GetMaxHealth() const
{
    if (!Type) return 0;
    return Type->Strength;
}

int32 UnitClass::GetHealth() const
{
    return Health;
}

void UnitClass::SetHealth(int32 hp)
{
    Health = hp;
    if (Health < 0) Health = 0;
    int32 maxHp = GetMaxHealth();
    if (maxHp > 0 && Health > maxHp) Health = maxHp;
}

bool UnitClass::IsAlive() const
{
    return Health > 0;
}

bool UnitClass::IsDead() const
{
    return Health <= 0;
}

bool UnitClass::IsDamaged() const
{
    return Health < GetMaxHealth();
}

bool UnitClass::IsGreenHP() const
{
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return false;
    return GetHealthRatio() > 0.66f;
}

bool UnitClass::IsYellowHP() const
{
    float ratio = GetHealthRatio();
    return ratio > 0.33f && ratio <= 0.66f;
}

bool UnitClass::IsRedHP() const
{
    return GetHealthRatio() <= 0.33f;
}

float UnitClass::GetHealthRatio() const
{
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return 0.0f;
    return static_cast<float>(Health) / static_cast<float>(maxHp);
}

void UnitClass::Repair(int32 amount)
{
    if (amount <= 0) return;
    if (Health <= 0) return;

    Health += amount;
    int32 maxHp = GetMaxHealth();
    if (Health > maxHp) Health = maxHp;

    RepairActive = true;
}

void UnitClass::Kill()
{
    Health = 0;
    OnDestroyed();
}

// =============================================================================
// Capabilities
// =============================================================================
bool UnitClass::CanDeploy() const
{
    if (!Type) return false;
    if (!Type->HasDeployer) return false;
    if (Deploying || Deployed) return false;
    return true;
}

bool UnitClass::CanEnter() const
{
    if (!Type) return false;
    // Units can enter structures if they are not deployed.
    if (Deployed) return false;
    return true;
}

bool UnitClass::CanBeEntered() const
{
    // Check if passengers can enter this unit (APC, transport).
    if (!Type) return false;
    if (!IsAPC) return false;
    if (NonPassengerCount >= Type->Passengers) return false;
    return true;
}

bool UnitClass::CanCrate() const
{
    if (!Type) return false;
    return Type->IsCanBeDestroyed;
}

void UnitClass::CreateCrate()
{
    if (!CanCrate()) return;
    IsShowingCrate = true;
    unknown_80C = static_cast<DWORD>(GetValue());
}

void UnitClass::PickUpCrate()
{
    if (!IsShowingCrate) return;
    IsShowingCrate = false;
    unknown_80C = 0;
}

// =============================================================================
// Value and economy
// =============================================================================
int32 UnitClass::GetValue() const
{
    if (!Type) return 0;
    return Type->ThreatPosedValue > 0 ? static_cast<int32>(Type->ThreatPosedValue) : 1;
}

int32 UnitClass::GetCost() const
{
    if (!Type) return 0;
    return Type->BuildCost;
}

int32 UnitClass::GetBuildTime() const
{
    if (!Type) return 0;
    return Type->BuildTime;
}

int32 UnitClass::GetSpeed() const
{
    if (!Type) return 0;
    return Type->Speed;
}

int32 UnitClass::GetROF() const
{
    if (!Type) return 0;

    // Rate of fire is derived from the primary weapon's ROF value.
    WeaponStruct* pWeapons = Type->Weapons;
    if (pWeapons && pWeapons[0].WeaponType) {
        int32 rof = pWeapons[0].WeaponType->ROF;
        if (rof > 0) return rof;
    }

    // Fall back to the weapon charge timer if set.
    if (Type->WeaponCharge > 0) return Type->WeaponCharge;

    return 15; // default ROF in frames
}

// =============================================================================
// Direction
// =============================================================================
DirStruct UnitClass::GetDirection() const
{
    return PrimaryFacing;
}

void UnitClass::SetDirection(DirStruct dir)
{
    PrimaryFacing = dir;
    SetFacing(dir);
}

DirStruct UnitClass::GetTurretDir() const
{
    return TurretDir;
}

void UnitClass::SetTurretDir(DirStruct dir)
{
    TurretDir = dir;
    SetTurretFacing(dir);
}

// =============================================================================
// Coordinates
// =============================================================================
CoordStruct UnitClass::GetCoords() const
{
    return Location;
}

void UnitClass::SetCoords(CoordStruct coords)
{
    // Unmark old occupation, move, then mark new.
    UnmarkAllOccupationBits(Location);
    Location = coords;
    MarkAllOccupationBits(coords);
}

CoordStruct UnitClass::GetDestination() const
{
    return Get_Destination();
}

void UnitClass::SetDestination(CoordStruct dest)
{
    Move_To(dest);
}

// =============================================================================
// Movement control
// =============================================================================
void UnitClass::Stop()
{
    Stop_Moving();
    Clear_Path();
    ClearTarget();
    SetSequence(Sequence::Ready);
}

void UnitClass::Scatter()
{
    if (!CanScatter()) return;
    // Move to a nearby random position to avoid danger.
    CoordStruct currentPos = GetCoords();
    int32 offsetX = (rand() % 200) - 100;
    int32 offsetY = (rand() % 200) - 100;
    CoordStruct scatterPos(currentPos.X + offsetX, currentPos.Y + offsetY, currentPos.Z);
    SetDestination(scatterPos);
}

void UnitClass::Hold()
{
    Stop_Moving();
    SetSequence(Sequence::Guard);
}

bool UnitClass::IsMoving() const
{
    return Is_Moving();
}

bool UnitClass::IsFiring() const
{
    return unknown_810 > 0 || FireRechargeTimer > 0;
}

bool UnitClass::IsIdle() const
{
    if (IsMoving()) return false;
    if (IsFiring()) return false;
    if (Deploying || Undeploying) return false;
    if (IsHarvestingTiberium) return false;
    return GetSequence() == Sequence::Ready || GetSequence() == Sequence::Guard;
}

void UnitClass::SetIdle()
{
    Stop_Moving();
    SetSequence(Sequence::Ready);
}

// =============================================================================
// Freeze / limbo
// =============================================================================
void UnitClass::Freeze()
{
    // Stop all activity.
    Stop_Moving();
    StopHarvesting();
    unknown_7F8 = 1; // frozen flag
}

void UnitClass::Unfreeze()
{
    unknown_7F8 = 0; // clear frozen flag
}

bool UnitClass::Limbo()
{
    if (IsInLimbo) return false;
    IsInLimbo = true;
    UnmarkAllOccupationBits(Location);
    Stop_Moving();
    return true;
}

bool UnitClass::Unlimbo()
{
    if (!IsInLimbo) return false;
    IsInLimbo = false;
    MarkAllOccupationBits(Location);
    // asm: UnitClass_Unlimbo -> ObjectClass::Unlimbo 尾部 vec_Logics.Add
    //   （L1049805）。覆写版必须补上这一步，否则单位不进逐帧主表。
    Register_Logic();
    return true;
}

bool UnitClass::InLimbo() const
{
    return IsInLimbo;
}

// =============================================================================
// Put  (asm: vt_TechnoClass.r.m.o.Put 的 UnitClass 覆写)
//
//   把单位落到给定 lepton 坐标：Z 贴该格地面高度，写下 Location 后走 Unlimbo
//   （清 limbo 位、登记所在格的占用位、把对象挂进 vec_Logics）。
//   与原版一致：返回是否落位成功。
// =============================================================================
bool UnitClass::Put(const CoordStruct& coords, int32 a2)
{
    (void)a2;

    if (MapClass::Instance == nullptr)
        return false;

    CoordStruct place = coords;
    place.Z = MapClass::Instance->GetCellFloorHeight(place);

    Location  = place;
    IsInLimbo = true;          // 原版 Put 先确保对象处于 limbo 再落位
    return Unlimbo();
}


// =============================================================================
// Mark / sync
// =============================================================================
void UnitClass::Mark(MarkType mark)
{
    if (mark == MarkType::Down) {
        MarkAllOccupationBits(Location);
    } else {
        UnmarkAllOccupationBits(Location);
    }
}

void UnitClass::Unmark()
{
    UnmarkAllOccupationBits(Location);
}

void UnitClass::Sync()
{
    // Synchronize state for network play.
    unknown_820 = static_cast<DWORD>(Health);
    unknown_824 = static_cast<DWORD>(unknown_7F0); // mission
    unknown_828 = static_cast<DWORD>(PrimaryFacing.Value);
}

void UnitClass::Unsync()
{
    // Restore synchronized state.
    Health = static_cast<int32>(unknown_820);
    unknown_7F0 = unknown_824;
    PrimaryFacing = DirStruct(static_cast<uint8>(unknown_828));
}

// =============================================================================
// Lock / disable
// =============================================================================
void UnitClass::Lock()
{
    unknown_7F8++;
}

void UnitClass::Unlock()
{
    if (unknown_7F8 > 0) {
        unknown_7F8--;
    }
}

bool UnitClass::IsLocked() const
{
    return unknown_7F8 > 0;
}

void UnitClass::Disable()
{
    unknown_7FC++;
    Stop_Moving();
}

void UnitClass::Enable()
{
    if (unknown_7FC > 0) {
        unknown_7FC--;
    }
}

bool UnitClass::IsDisabled() const
{
    return unknown_7FC > 0;
}

void UnitClass::Activate()
{
    if (Health > 0) {
        unknown_7FC = 0;
    }
}

void UnitClass::Deactivate()
{
    unknown_7FC++;
    Stop_Moving();
    StopHarvesting();
}

bool UnitClass::IsActive() const
{
    if (IsInLimbo) return false;
    if (Health <= 0) return false;
    if (unknown_7FC > 0) return false;
    return true;
}

// =============================================================================
// Cloak
// =============================================================================
void UnitClass::Cloak()
{
    if (!Type || !Type->CanCloak) return;
    CloakState = CloakStateEnum::Cloaking;
    CloakTimer = 30; // cloaking animation duration
}

void UnitClass::Decloak()
{
    CloakState = CloakStateEnum::Uncloaking;
    CloakTimer = 30;
}

bool UnitClass::IsCloaked() const
{
    return CloakState == CloakStateEnum::Cloaked;
}

// =============================================================================
// EMP
// =============================================================================
void UnitClass::EMPulse()
{
    if (Type && Type->IsImmuneToEMP) return;
    unknown_7E4 = 150; // EMP duration in frames
    Stop_Moving();
    StopHarvesting();
}

void UnitClass::UnEMP()
{
    unknown_7E4 = 0;
}

bool UnitClass::IsEMPed() const
{
    return unknown_7E4 > 0;
}

// =============================================================================
// Iron Curtain / Force Shield
// =============================================================================
void UnitClass::IronCurtain()
{
    IronCurtainTimer = IRON_CURTAIN_DURATION;
    ForceShieldTimer = 0;
}

void UnitClass::UnIronCurtain()
{
    IronCurtainTimer = 0;
}

bool UnitClass::IsIronCurtained() const
{
    return IronCurtainTimer > 0;
}

void UnitClass::ForceShield()
{
    ForceShieldTimer = FORCE_SHIELD_DURATION;
    IronCurtainTimer = 0;
}

void UnitClass::UnForceShield()
{
    ForceShieldTimer = 0;
}

bool UnitClass::IsForceShielded() const
{
    return ForceShieldTimer > 0;
}

// =============================================================================
// Chrono / temporal
// =============================================================================
void UnitClass::ChronoShift()
{
    if (!IsChrono) return;
    // Chrono shift teleports the unit to a new location.
    // The destination must have been set before calling this.
    CoordStruct dest = GetDestination();
    UnmarkAllOccupationBits(Location);
    Location = dest;
    MarkAllOccupationBits(dest);
}

void UnitClass::TemporalWarp()
{
    TemporalTimer = 300; // freeze for 300 frames
}

void UnitClass::UnTemporal()
{
    TemporalTimer = 0;
}

bool UnitClass::IsTemporalWarped() const
{
    return TemporalTimer > 0;
}

// =============================================================================
// Mind control
// =============================================================================
void UnitClass::MindControl(TechnoClass* pTarget)
{
    if (!pTarget) return;
    if (Type && Type->IsImmuneToPsionics) return;

    // Store the mind control target.
    uintptr_t ptrVal = reinterpret_cast<uintptr_t>(pTarget);
    unknown_804 = static_cast<DWORD>(ptrVal);
    unknown_808 = static_cast<DWORD>(ptrVal >> 32);
}

void UnitClass::UnMindControl()
{
    unknown_804 = 0;
    unknown_808 = 0;
}

bool UnitClass::IsMindControlled() const
{
    return unknown_804 != 0 || unknown_808 != 0;
}

// =============================================================================
// Disguise
// =============================================================================
void UnitClass::Disguise()
{
    if (!Type) return;
    unknown_800 = 1; // disguise active
}

void UnitClass::UnDisguise()
{
    unknown_800 = 0;
}

bool UnitClass::IsDisguised() const
{
    return unknown_800 != 0;
}

// =============================================================================
// Identity
// =============================================================================
bool UnitClass::IsUnit() const
{
    return true;
}

AbstractType UnitClass::WhatAmI() const
{
    return AbstractType::Unit;
}

int32 UnitClass::Size() const
{
    return sizeof(UnitClass);
}

void UnitClass::DumpTiberium()
{
    // Unload the harvested tiberium at a refinery and convert it to
    // credits.  The unload completes when the harvester is back at the
    // refinery's bay; the value is credited once per full dump.
    if (HarvestAmount <= 0)
    {
        IsDumpingTiberium = false;
        return;
    }

    if (Owner == nullptr)
    {
        IsDumpingTiberium = false;
        HarvestAmount = 0;
        return;
    }

    // Credit the refined value.  The per-cell tiberium value is looked up
    // from the map (the harvest weight of one unit of the deposit); each
    // point of load is worth that many credits.
    int32 cellValue = 10;   // fallback: one unit of tiberium ≈ $10
    if (MapClass::Instance != nullptr)
    {
        CoordStruct pos = GetCoords();
        CellStruct cellPos = Math::CoordToCell(pos);
        CellClass* pCell = MapClass::Instance->GetCellAt(cellPos);
        if (pCell != nullptr && pCell->TiberiumValue > 0)
            cellValue = pCell->TiberiumValue;
    }
    int32 credits = HarvestAmount * cellValue;
    Owner->GiveMoney(credits);

    HarvestAmount = 0;
    TotalTiberiumValue = 0;
    IsDumpingTiberium = false;

    // Resume harvesting on the next AI tick.
    IsHarvestingTiberium = true;
}

// ============================================================================
// Per-instance serialization (base fields + unit-specific fields).
// ============================================================================
void UnitClass::Save(SaveGameClass& saver) const
{
    TechnoClass::Save(saver);

    saver.Write(static_cast<int32>(TurretDir.Value));
    saver.Write(static_cast<int32>(BarrelDir.Value));
    saver.Write(static_cast<int8>(IsHarvestingTiberium ? 1 : 0));
    saver.Write(static_cast<int8>(IsDumpingTiberium ? 1 : 0));
    saver.Write(HarvestAmount);
    saver.Write(static_cast<int8>(IsMCV ? 1 : 0));
    saver.Write(static_cast<int8>(IsHarvester ? 1 : 0));
    saver.Write(static_cast<int8>(IsAPC ? 1 : 0));
    saver.Write(static_cast<int8>(IsDeployer ? 1 : 0));
    saver.Write(static_cast<int8>(IsChrono ? 1 : 0));
    saver.Write(static_cast<int8>(IsUnderground ? 1 : 0));
    saver.Write(static_cast<int8>(IsSubterranean ? 1 : 0));
    saver.Write(static_cast<int8>(IsCarryall ? 1 : 0));
    saver.Write(static_cast<int8>(IsJumpJet ? 1 : 0));
}

void UnitClass::Load(LoadGameClass& loader)
{
    TechnoClass::Load(loader);

    int32 turretDir = 0;
    int32 barrelDir = 0;
    loader.Read(turretDir);
    loader.Read(barrelDir);
    TurretDir.Value = turretDir;
    BarrelDir.Value = barrelDir;

    int8 flag = 0;
    loader.Read(flag); IsHarvestingTiberium = (flag != 0);
    loader.Read(flag); IsDumpingTiberium = (flag != 0);
    loader.Read(HarvestAmount);
    loader.Read(flag); IsMCV = (flag != 0);
    loader.Read(flag); IsHarvester = (flag != 0);
    loader.Read(flag); IsAPC = (flag != 0);
    loader.Read(flag); IsDeployer = (flag != 0);
    loader.Read(flag); IsChrono = (flag != 0);
    loader.Read(flag); IsUnderground = (flag != 0);
    loader.Read(flag); IsSubterranean = (flag != 0);
    loader.Read(flag); IsCarryall = (flag != 0);
    loader.Read(flag); IsJumpJet = (flag != 0);
}

// ============================================================================
// UnitClass - type-flag probes
// ============================================================================

 // asm: char __fastcall UnitClass_IsARealVehicle(UnitClass *this)   (asm:1706796)
//
//  True when the unit type is NOT flagged IsConsideredVehicle: the engine uses
//  this to exclude infantry-like or pseudo-vehicle units from real vehicle
//  handling (crushing, repair pads, ...).
bool UnitClass::IsARealVehicle() const
{
    return (Type != nullptr) && !Type->IsConsideredVehicle;
}

 // 根据游戏行为，可知 IsDeployer 负责下面这段逻辑。
//
//  Reads the unit type's IsSimpleDeployer byte - a vehicle that turns into a
//  structure (MCV, ...).
bool UnitClass::IsDeployable() const
{
    return (Type != nullptr) && Type->IsSimpleDeployer;
}

 // UnitClass_GetCrewCount: thunk to TechnoClass::Crew_Type.
//
//  Returns the infantry type this unit leaves behind on destruction.
InfantryTypeClass* UnitClass::GetCrewCount() const
{
    if (TechnoType != nullptr)
        return TechnoType->Crew;

    return nullptr;
}

 // asm: UnitClass_CalcPipPercentage
//
//  The passenger/ammo pips fill proportionally to how many passengers are
//  aboard, capped at the type's passenger limit.
int32 UnitClass::CalcPipPercentage() const
{
    if (TechnoType == nullptr || TechnoType->Passengers <= 0)
        return 0;

    const int32 current = GetOccupantCount();
    int32 percent = (current * 100) / TechnoType->Passengers;

    if (percent > 100)
        percent = 100;
    if (percent < 0)
        percent = 0;

    return percent;
}

 // asm: UnitClass_ClearSomeVec
//
//  Clears the object's attached abstract vector (the FootClass vec_Abs slot).
//  The reconstruction models that vector separately, so this resets the
//  object's intermediate destination state instead.
void UnitClass::ClearSomeVec()
{
    Path.Clear();
}

// ============================================================================
// UnitClass - 身份 / 运行时类型 / CRC
// ============================================================================

// asm: int __fastcall UnitClass_GetClassID(UnitClass *this, CLSID *pClassID)
//
//  返回本类的 CLSID：指针为空时报参数错误（0x80004003 = E_POINTER），否则把
//  UnitClass 的类 GUID 抄进调用方缓冲并报告成功。
HRESULT UnitClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    // UnitClass 的类标识（'Unit'）。
    pClassID->Data1 = 0x74696E55;
    pClassID->Data2 = 0;
    pClassID->Data3 = 0;
    for (int32 i = 0; i < 8; ++i)
        pClassID->Data4[i] = 0;

    return S_OK;
}

// asm: int __fastcall UnitClass_GetAbstractDerivationID(UnitClass *this)
//
//  抽象派生链上的编号：载具恒为 1。
int32 UnitClass::GetAbstractDerivationID() const
{
    return 1;
}

// asm: void __thiscall UnitClass_GetCRC(UnitClass *this, CRCEngine *crc)
//
//  把本载具参与同步校验的字段逐个喂给 CRC 引擎：先走基类，再喂入类型索引、
//  拖挂从车索引、载旗阵营索引，以及两个布尔标志（展开中/收起中）。
void UnitClass::GetCRC(CRCEngine& crc) const
{
    TechnoClass::ComputeCRC(crc);

    // 类型索引。
    int32 typeIndex = (Type != nullptr) ? Type->Get_ArrayIndex() : -1;
    crc.AddData(&typeIndex, sizeof(typeIndex));

    // 拖挂从车索引。
    int32 followerIndex = -1;
    if (FollowerCar != nullptr)
        followerIndex = FollowerCar->GetArrayIndex();
    crc.AddData(&followerIndex, sizeof(followerIndex));

    // 载旗阵营索引。
    crc.AddData(&CarriesFlagOfHouse, sizeof(CarriesFlagOfHouse));

    // 部署状态布尔。
    uint8 deploying   = Deploying ? 1 : 0;
    uint8 undeploying = Undeploying ? 1 : 0;
    crc.AddData(&deploying, sizeof(deploying));
    crc.AddData(&undeploying, sizeof(undeploying));
}

// asm: UnitClass_GetSize   (asm:12591)  mov eax, 8E8h; retn
//
//  返回本单位实例序列化所需的字节数（原版固定 0x8E8）。
int32 UnitClass::GetSize() const
{
    return 0x8E8;
}

// asm: char __thiscall UnitClass_GetType(UnitClass *this)
//
//  返回本对象的运行时类型定义（UnitTypeClass）。
UnitTypeClass* UnitClass::GetUnitType() const
{
    return Type;
}

// asm: wchar_t *__thiscall UnitClass_GetUIName(UnitClass *this)
//
//  显示名：伪装状态下优先用伪装身份的名字；多炮塔且类型带炮塔提示时，按当前
//  炮塔把显示名换成 "Tip:Rocket"/"Tip:MachineGun"/"Tip:Repair" 之类；其余
//  用类型自带的 UI 名。
const wchar_t* UnitClass::GetUIName() const
{
    // 伪装状态下优先显示伪装身份的名字。
    if (IsDisguised() && IsDisguisedAs(1)) {
        TechnoTypeClass* pDisguise = GetDisguise(1);
        if (pDisguise != nullptr)
            return pDisguise->GetUIName();
    }

    if (Type == nullptr)
        return L"";

    // 多炮塔载具（战斗要塞）按当前炮塔给出提示名。
    if (HasMultipleTurrets() && Type->HasTurretTooltips) {
        TechnoClass* pOccupant = Attached_Object();
        if (pOccupant != nullptr && pOccupant->WhatAmI() == AbstractType::Infantry) {
            const int32 turret = GetActiveTurretIndex();
            const char* pTipKey = "Tip:Rocket";
            switch (turret) {
                case 1: pTipKey = "Tip:MachineGun"; break;
                case 2: pTipKey = "Tip:Repair";     break;
                default: break;
            }

            const char* pTip = EngineGlobals::Get_String(pTipKey);
            if (pTip != nullptr && pTip[0] != '\0') {
                // 组装 "<载具名> <炮塔提示>"，与原版一致。
                swprintf(m_UINameCache, sizeof(m_UINameCache) / sizeof(wchar_t),
                         L"%s %s", Type->UIName, pTip);
                return m_UINameCache;
            }
        }
    }

    return Type->GetUIName();
}

// ============================================================================
// UnitClass - 伪装
// ============================================================================

// asm: void __thiscall UnitClass_DisguiseAs(_TechnoClass *this, _TechnoClass *tgt)
//
//  把本单位伪装成目标对象：目标是空则什么也不做；否则按目标类别取下伪装用
//  的类型与所属阵营，并唤起外观刷新。目标异常（既非步兵也非载具）时忽略。
void UnitClass::DisguiseAs(AbstractClass* pTarget)
{
    if (pTarget == nullptr || pTarget == this)
        return;

    const AbstractType what = pTarget->WhatAmI();

    // 只有可以伪装成的对象才有伪装身份（步兵/载具/建筑/飞机）。
    if (what != AbstractType::Unit && what != AbstractType::Infantry
        && what != AbstractType::Building && what != AbstractType::Aircraft)
        return;

    TechnoClass* pTechno = static_cast<TechnoClass*>(pTarget);
    TechnoTypeClass* pDisguiseType = pTechno->GetTechnoType();
    if (pDisguiseType == nullptr)
        return;

    // 记下伪装身份并让外观刷新。
    DisguiseType   = pDisguiseType;
    DisguiseHouse  = pTechno->Get_Owner();
    DisguiseActive = true;

    // 与既有的伪装标志位保持同步。
    unknown_800 = 1;
}

// asm: void __fastcall UnitClass_ClearDisguise(UnitClass *this)
//
//  解除伪装：清掉伪装标志、伪装类型与伪装所属，并通知外观刷新。
void UnitClass::ClearDisguise()
{
    DisguiseActive = false;
    DisguiseType   = nullptr;
    DisguiseHouse  = nullptr;
    unknown_800    = 0;
}

// asm: char __thiscall UnitClass_IsDisguisedAs(UnitClass *this, HouseClass *h)
//
//  当本单位处于伪装状态、且没有被指定阵营的探测手段看穿时，报告"对 h 而言
//  显示为伪装身份"。
bool UnitClass::IsDisguisedAs(int32 a2) const
{
    // 未伪装则无从谈起。
    if (!IsDisguised())
        return false;

    HouseClass* pHouse = HouseClass::GetHouseByIndex(a2);
    if (pHouse == nullptr)
        return false;

    // 自己的盟友看得到真身。
    if (Is_Ally(pHouse))
        return false;

    // 对方有探测手段（伪装探测器）时被看穿。
    const CoordStruct pos = GetCoords();
    const CellStruct  cell = Math::CoordToCell(pos);
    if (MapClass::Instance != nullptr) {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell != nullptr) {
            ObjectClass* pOccupier = pCell->Get_Occupier();
            if (pOccupier != nullptr && Is_Enemy(pOccupier->Get_Owner()))
                return false;
        }
    }

    return true;
}

// asm: _TechnoTypeClass *__thiscall UnitClass_GetDisguise(UnitClass *this, char a2)
//
//  返回伪装用的类型：自己是玩家盟友且 a2 非零时直接返回真实类型，否则返回
//  伪装类型。
TechnoTypeClass* UnitClass::GetDisguise(int32 a2) const
{
    HouseClass* pPlayer = HouseClass::Player;

    if (Is_Ally(pPlayer) && a2 != 0)
        return TechnoType;

    return DisguiseType;
}

// asm: HouseClass *__thiscall UnitClass_GetDisguiseHouse(UnitClass *this, char a2)
//
//  返回伪装身份的所属阵营：自己是玩家盟友且 a2 非零时返回真实阵营，否则返回
//  伪装阵营。
HouseClass* UnitClass::GetDisguiseHouse(int32 a2) const
{
    HouseClass* pPlayer = HouseClass::Player;

    if (Is_Ally(pPlayer) && a2 != 0)
        return Owner;

    return DisguiseHouse;
}

// ============================================================================
// UnitClass - 部署 / 面向
// ============================================================================

// asm: char __thiscall UnitClass_IsDeployed(UnitClass *this)
//
//  本单位是否处于展开状态。
bool UnitClass::IsDeployed() const
{
    return Deployed;
}

// asm: int __thiscall UnitClass_GetFacing(UnitClass *this, int *a2)
//
//  计算朝向目标点 a2 的 256 分度朝向写回 a2：多炮塔载具用炮塔朝向起算，
//  其余用车身朝向。
void UnitClass::GetFacing(CoordStruct* pCoord)
{
    if (pCoord == nullptr)
        return;

    const CoordStruct here = GetCoords();
    const int32 dx = pCoord->X - here.X;
    const int32 dy = pCoord->Y - here.Y;

    // 由两点向量得到 0..255 的朝向分度。
    int32 facing = 0;
    if (dx != 0 || dy != 0) {
        const double ang = std::atan2(static_cast<double>(dy),
                                      static_cast<double>(dx));
        facing = static_cast<int32>(std::round(ang * 128.0 / 3.14159265358979323846));
        facing &= 0xFF;
    }

    // 多炮塔载具以炮塔当前朝向为基准起算。
    if (HasMultipleTurrets()) {
        const int32 base = static_cast<int32>(TurretDir.Value);
        facing = (facing - base) & 0xFF;
    }

    pCoord->X = facing;
}

// ============================================================================
// UnitClass - 点击 / 指令 / 自动索敌
// ============================================================================

// asm: char __thiscall UnitClass_ClickedMission(UnitClass *this, eActions act, _ObjectClass *target, int a4)
//
//  玩家点击本单位时把光标动作归一到任务动作，再交给基类处理；已经展开或
//  正在忙的单位不接受新指令。
bool UnitClass::ClickedMission(int32 act, ObjectClass* pTarget, int32 a4) const
{
    // 由目标推出光标动作。
    const Action cursorAct = MouseOverObject(pTarget, false);

    int32 mapped = act;

    if (static_cast<int32>(cursorAct) != act) {
        const int32 cur = static_cast<int32>(cursorAct);
        if (cur == static_cast<int32>(Action::Enter)) {
            mapped = 1;
        } else if (cur == static_cast<int32>(Action::Capture)
                   || cur == static_cast<int32>(Action::Sabotage)) {
            mapped = 5;
        } else if (static_cast<int32>(act) == static_cast<int32>(Action::GuardArea)
                   || static_cast<int32>(act) == static_cast<int32>(Action::Repair)) {
            mapped = 5;
        }
    }

    // 点击自己并且动作是移动/进入一类时不接受。
    if (pTarget == this
        && (mapped == 2 || mapped == 5)) {
        return false;
    }

    // 已经展开的单位（如 MCV 展开后）不接受新指令。
    if (Deployed)
        return false;

    // 交给基类处理（a3 以整数槽位承载目标指针，按 asm 惯例截断）。
    return ObjectClass::ClickedMission(
        mapped, static_cast<int32>(reinterpret_cast<intptr_t>(pTarget)), a4);
}

// asm: int __thiscall UnitClass_ReceivedRadioCommand(UnitClass *this, int val, uint cmd, int arg)
//
//  处理无线电报文：少数与载具状态相关的报文就地处理，其余转交基类。
int32 UnitClass::ReceivedRadioCommand(int32 val, int32 cmd, int32 arg)
{
    switch (cmd) {
        // 报文 3：询问能否接收乘客——已满或不经乘客链时报告不能。
        case 3: {
            if (CanBeEntered())
                return 1;
            return 0;
        }
        default:
            break;
    }

    // asm loc_7376AD / loc_737AFC：本层未处理的报文一律上交父层
    //   `FootClass_ReceivedRadioCommand(this, Techno, RC, unknown)`。
    // 本项目参数顺序是 (源对象, 报文, 载荷)，父层要的是 (报文, 源对象, 载荷)。
    return FootClass::ReceivedRadioCommand(cmd, val, arg);
}

// asm: _ObjectClass *__thiscall UnitClass_SelectAutoTarget(UnitClass *this, int TargetFlags, int a5, int a6)   (asm:9530)
//
//  自动索敌：玩家持有的"无威胁"载具不自动索敌；否则把本单位武器的反地/反空
//  标志折进 TargetFlags（多炮塔单位取当前炮塔武器，其余取两档常规武器），再
//  交给通用索敌 Greatest_Threat。原版掩码已带位时不重复折入。
ObjectClass* UnitClass::SelectAutoTarget(int32 projFlags, int32 curThreat, int32 a4)
{
    // 玩家持有的无威胁载具（IsNoThreat）不做自动索敌。
    if (Owner != nullptr && Owner->Is_Human() && Type != nullptr && Type->IsNoThreat)
        return nullptr;

    int32 mask = projFlags;

    if ((mask & 0x1B978) == 0) {
        if (HasMultipleTurrets()) {
            // 多炮塔单位用当前炮塔所挂武器的标志。
            WeaponStruct* pTurret = GetTurretChangingWeapon();
            if (pTurret != nullptr && pTurret->WeaponType != nullptr)
                mask |= WeaponTypeClass::GetProjectileAGAA_Flags(pTurret->WeaponType);
        } else if (Type != nullptr) {
            for (int32 i = 0; i < 2; ++i) {
                WeaponStruct& ws = Type->Weapons[i];
                if (ws.WeaponType != nullptr)
                    mask |= WeaponTypeClass::GetProjectileAGAA_Flags(ws.WeaponType);
            }
        }
    }

    return Greatest_Threat(mask, curThreat, a4);
}

// asm: signed int __thiscall UnitClass_GetObjectActivityState(UnitClass *this, _TechnoClass *target, int idx, int what)
//
//  报告本单位"当前正在干什么"，供选中信息与 AI 判断：正在装填/开火/移动等
//  各返回对应状态码。返回码沿用原版：0 正常、6 已摧毁、7 移动中、5 无武器、
//  0x0A 正在生产、0x0B 受限等。
int32 UnitClass::GetObjectActivityState() const
{
    // 正在死亡/已死：报告"已摧毁"。
    if (Health <= 0)
        return 6;

    // 有残留的生成物（子机）在活动。
    if (IsCarryall && IsCarryAllFly)
        return 0x0A;

    // 正在开火装填：报告"无法中断"。
    if (FireRechargeTimer > 0)
        return 5;

    // 有目标且在移动中去接近它：报告"移动中"。
    if (HasTarget() && Is_Moving())
        return 7;

    // 正在移动。
    if (Is_Moving())
        return 7;

    // 正在采矿/卸矿。
    if (IsHarvestingTiberium || IsDumpingTiberium)
        return 6;

    // 其余算正常待命。
    return 0;
}

// ============================================================================
// UnitClass - 销毁登记 / 指针失效
// ============================================================================

// asm: void __thiscall UnitClass_RegisterDestruction(UnitClass *this, _TechnoClass *killer)   (asm:11288)
//
//  登记本载具被摧毁：若挂有且非"重复触发"的触发器，则依次抛出事件 7（被摧毁）、
//  0x30（被任何方式摧毁）、0x1D（被特定对象摧毁）——其中 7 号事件只在存在凶手
//  时抛出；随后走通用的摧毁登记。
void UnitClass::RegisterDestruction(TechnoClass* pKiller)
{
    if (Tag != nullptr && !Tag->IsTriggerRepeating()) {
        const CellStruct loc = CellClass::Coord2Cell(GetCoords());

        if (pKiller != nullptr)
            Tag->RaiseEvent(7, this, loc, false, nullptr);

        Tag->RaiseEvent(0x30, this, loc, false, nullptr);
        Tag->RaiseEvent(0x1D, this, loc, false, nullptr);
    }

    TechnoClass::RegisterDestruction();
}

// asm: void __thiscall UnitClass_PointerGotInvalid(UnitClass *this, _AbstractClass *arg_0, int arg_4)   (asm:11238)
//
//  某个被引用的对象失效时清理本载具持有的悬挂指针：先走 FootClass 基类（不是
//  TechnoClass），再把正好指向该对象的从车（+0x6C8）与类型（+0x6C4）清空。
void UnitClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    FootClass::PointerGotInvalid(pInvalid, removed);

    if (FollowerCar == pInvalid)
        FollowerCar = nullptr;

    if (Type == pInvalid)
        Type = nullptr;
}

// ============================================================================
// UnitClass - 地图 INI 存取
// ============================================================================

// asm: void __thiscall UnitClass_CreateFromINIList(CCINIClass *pINI)
//
//  读 [Units] 段逐行重建载具：每行给出拥有者、类型、坐标、朝向、生命值与
//  任务，按类型与拥有者实例化后放到地图上。格式串（NewINIFormat >= 5）为
//  "owner,type,health,facing,x,y,mission,..."。
void UnitClass::CreateFromINIList(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return;

    if (!pINI->SectionExists("Units"))
        return;

    const int32 count = pINI->GetKeyCount("Units");
    for (int32 i = 0; i < count; ++i) {
        const char* pKey = pINI->GetKeyName("Units", i);
        if (pKey == nullptr)
            continue;

        char value[256];
        value[0] = '\0';
        pINI->ReadString("Units", pKey, "", value, sizeof(value));

        if (value[0] == '\0')
            continue;

        char buffer[256];
        std::strncpy(buffer, value, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        // 逐段切分。
        const char* pOwnerName = std::strtok(buffer, ",");
        const char* pTypeName  = std::strtok(nullptr, ",");
        if (pOwnerName == nullptr || pTypeName == nullptr)
            continue;

        const int32 ownerIndex = HouseClass::FindIndexByName(pOwnerName);
        if (ownerIndex < 0)
            continue;

        UnitTypeClass* pType = UnitTypeClass::Find(pTypeName);
        if (pType == nullptr)
            continue;

        HouseClass* pOwner = HouseClass::GetHouseByIndex(ownerIndex);
        if (pOwner == nullptr)
            continue;

        // 按原版顺序读取其余字段：生命值、朝向、坐标、任务。
        const char* pField = std::strtok(nullptr, ",");
        const int32 health = (pField != nullptr) ? std::atoi(pField) : 0;

        pField = std::strtok(nullptr, ",");
        int32 facing = (pField != nullptr) ? std::atoi(pField) : 0;

        pField = std::strtok(nullptr, ",");
        const int32 x = (pField != nullptr) ? std::atoi(pField) : 0;

        pField = std::strtok(nullptr, ",");
        const int32 y = (pField != nullptr) ? std::atoi(pField) : 0;

        // 实例化并放到地图上。
        UnitClass* pUnit = new UnitClass(pOwner);
        if (pUnit == nullptr)
            continue;

        // 朝向按 256 分度写回。
        pUnit->PrimaryFacing = DirStruct(static_cast<uint8>(facing & 0xFF));

        if (health > 0)
            pUnit->Health = health;

        CoordStruct pos(x, y, 0);
        if (MapClass::Instance != nullptr) {
            CellClass* pCell = MapClass::Instance->GetCellAt(
                CellStruct(static_cast<int32>(x), static_cast<int32>(y)));
            if (pCell != nullptr)
                pCell->ConvertCoords(&pos);
        }
        pUnit->SetCoords(pos);
    }
}

// asm: int __thiscall UnitClass_SaveToMapINIList(CCINIClass *INI)
//
//  把全地图上所有已放置的载具回写进 [Units] 段，每台一行。
void UnitClass::SaveToMapINIList(INIClass* pINI)
{
    if (pINI == nullptr || Array == nullptr)
        return;

    // 先写下段头。
    pINI->WriteString("Units", nullptr, nullptr);

    const int32 count = Array->Count;
    for (int32 i = 0; i < count; ++i) {
        UnitClass* pUnit = (*Array)[i];
        if (pUnit == nullptr)
            continue;

        // 不在 limbo 且类型要求写回时才写。
        if (pUnit->IsInLimbo)
            continue;
        if (pUnit->Type == nullptr)
            continue;

        pUnit->SaveToMapINI(pINI);
    }
}

// asm: void __thiscall UnitClass_SaveToMapINI(UnitClass *this, INIClass *INI)
//
//  回写单台载具的放置记录，格式（NewINIFormat >= 5）：
//    <index> = <owner>,<type>,<health>,<facing>,<x>,<y>,<mission>
void UnitClass::SaveToMapINI(INIClass* pINI) const
{
    if (pINI == nullptr || Type == nullptr)
        return;

    char key[32];
    std::snprintf(key, sizeof(key), "%d", GetArrayIndex());

    // 拥有者名字。
    const char* pOwnerName = "None";
    if (Owner != nullptr && Owner->UIName[0] != '\0')
        pOwnerName = Owner->UIName;

    // 类型名。
    const char* pTypeName = Type->ID;

    // 朝向量化到 256 分度的 INI 表示。
    const DirStruct facing = GetDirection();
    const int32 facing256 = (facing.Value >> 7) + 1 >> 1 & 0xFF;

    // 生命值量化。
    const int32 health = static_cast<int32>(GetHealthRatio() * 256.0f);

    // 坐标。
    const CoordStruct pos = GetCoords();

    // 当前任务。
    const int32 mission = static_cast<int32>(GetMission());

    char value[256];
    std::snprintf(value, sizeof(value), "%s,%s,%d,%d,%d,%d,%d",
                  pOwnerName, pTypeName, health, facing256,
                  static_cast<int>(pos.X), static_cast<int>(pos.Y), mission);

    pINI->WriteString("Units", key, value);
}

// ============================================================================
// UnitClass - deployment / flag handling
// ============================================================================

 // asm: byte __thiscall UnitClass_IsBusy(UnitClass *this)   (asm:1708073)
//
//  True while the unit is mid-deploy or mid-undeploy.  The engine refuses to
//  give such a unit new orders (missions, movement, flag pick-up) until the
//  transition animation has settled.
bool UnitClass::IsBusy() const
{
    if (Deploying)
        return true;

    if (Undeploying)
        return true;

    return false;
}

 // asm: char __thiscall UnitClass_DropFlag(UnitClass *this)   (asm:1696504)
//
//  Releases the carried flag identity.  When no flag is held (+0x6CC == -1)
//  nothing happens and the call reports failure; otherwise the slot is reset
//  and the ownership change is broadcast through vtable slot +0x124 with the
//  message id 2 (the engine's "flag changed" notification).
bool UnitClass::DropFlag()
{
    const int32 none = -1;

    if (CarriesFlagOfHouse == none)
        return false;

    CarriesFlagOfHouse = none;
    FlagHouseIndex = none;
    Mark_Layer(2);

    return true;
}

 // asm: UnitClass_PickUpFlag
//
//  Claims the flag for the given house.  An invalid house (-1) or an already
//  carried flag makes the call a no-op that reports failure; on success the
//  slot is stamped and the same +0x124 notification is re-broadcast.
bool UnitClass::PickUpFlag(int32 houseIndex)
{
    if (houseIndex == -1)
        return false;

    if (CarriesFlagOfHouse != -1)
        return false;

    CarriesFlagOfHouse = houseIndex;
    FlagHouseIndex = houseIndex;
    Mark_Layer(2);

    return true;
}

// ============================================================================
 // asm: UnitClass_GetTiberiumPipFullness
//
//  Fraction of the ore bay that is filled, used to size the harvest pip.  A
//  unit whose type is neither a Harvester nor a Weeder has no bay to speak
//  of and reports 0.0; a harvester divides its accumulated ore by the type's
//  Storage capacity.
// ============================================================================
double UnitClass::GetTiberiumPipFullness() const
{
    if (Type == nullptr)
        return 0.0;

    if (!Type->Harvester && !Type->Weeder)
        return 0.0;

    const int32 storage = Type->Storage;
    if (storage <= 0)
        return 0.0;

    return Get_Tiberium() / static_cast<double>(storage);
}

// ============================================================================
 // asm: char __thiscall UnitClass_CanCrush(UnitClass *this, _FootClass *target)   (asm:10234)
//
//  True when this unit may roll over `pTarget`.  Authority to crush comes from
//  the type's Crusher flag or the CRUSHER veteran ability; the target must
//  exist and be crushable; the straight-line distance must be within the
//  rules' crush reach; and the owner's human/AI gating must allow it.  The
//  final test rejects targets currently disguised as this unit's owner.
bool UnitClass::CanCrush(FootClass* pTarget) const
{
    // Crusher authority: the type flag, or the CRUSHER ability.
    if (!Type->Crusher && !HasAbility(2))
        return false;

    if (pTarget == nullptr)
        return false;

    if (!pTarget->CanGetCrushed(const_cast<UnitClass*>(this)))
        return false;

    // Straight-line distance against the rules crush reach (+0x155C).
    const CoordStruct mine   = GetCoords();
    const CoordStruct theirs = pTarget->GetCoords();
    const double dx = static_cast<double>(mine.X - theirs.X);
    const double dy = static_cast<double>(mine.Y - theirs.Y);
    const double dz = static_cast<double>(mine.Z - theirs.Z);
    const int32 dist = static_cast<int32>(std::sqrt(dx * dx + dy * dy + dz * dz));

    if (RulesClass::Instance != nullptr && dist > RulesClass::Instance->Crush)
        return false;

    // Human owners only auto-crush when the rules flag or the type flag allows.
    if (Owner != nullptr && Owner->Is_Human()
        && !(RulesClass::Instance != nullptr && RulesClass::Instance->PlayerAutoCrush)
        && !Type->AutoCrush)
        return false;

    // The AI is not allowed to crush when set to the hardest difficulty.
    if (Owner != nullptr && Owner->DifficultyLevel == 2)
        return false;

    // A disguised target that reads as this owner is left alone.
    if (Owner != nullptr && pTarget->IsDisguisedAs(Owner->GetArrayIndex()))
        return false;

    return true;
}

// ============================================================================
// 炮手 / 载员管理
// ============================================================================

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_ReceiveGunner(UnitClass *this, _FootClass *gunner)   (asm:1706814)
//
//  接收一名步兵上车充当炮手：先确认该步兵确实能担任炮手（类型允许、且
//  自己还不是别人的炮手），再把它挂到本单位的炮手位上，并按它的兵种切换
//  当前武器档位，从而让多功能车一类载具换装。
// ----------------------------------------------------------------------------
bool UnitClass::ReceiveGunner(InfantryClass* pGunner)
{
    if (pGunner == nullptr)
        return false;

    // 本单位已经有人占着炮手位就换不了。
    if (this->Gunner != nullptr)
        return false;

    // 步兵得先停下手上的事。
    pGunner->Unlimbo();

    // 记下炮手，并按它的兵种切换武器档位。
    this->Gunner = pGunner;
    this->GunnerWeaponIndex = pGunner->TechnoType != nullptr ? 0 : -1;

    // 把炮手挪到本车的位置上。
    pGunner->Set_Coord(this->Get_Coord());

    return true;
}

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_RemoveGunner(UnitClass *this, _FootClass *Gunner)   (asm:1706904)
//
//  让当前炮手下车：清掉炮手位与武器档位，并让步兵重新回到地图上自由
//  行动。传入的步兵与当前炮手不符时不处理。
// ----------------------------------------------------------------------------
bool UnitClass::RemoveGunner(InfantryClass* pGunner)
{
    if (this->Gunner == nullptr)
        return false;

    // 指定的不是当前的炮手就免动。
    if (pGunner != nullptr && pGunner != this->Gunner)
        return false;

    InfantryClass* pLeaving = this->Gunner;

    // 清掉炮手位与武器档位。
    this->Gunner            = nullptr;
    this->GunnerWeaponIndex = -1;

    // 步兵重新回到地图上。
    pLeaving->Unlimbo();

    return true;
}

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_SwitchGunner(UnitClass *this, int idx)   (asm:1597930)
//
//  按给定档位切换炮手武器：档位需要在合法范围内（共 18 档），越界则忽略；
//  合法就记下新档位，让载具改用该档对应的武器。
// ----------------------------------------------------------------------------
bool UnitClass::SwitchGunner(int32 idx)
{
    // 档位越界就免动。
    if (idx < 0 || idx >= 18)
        return false;

    // 本单位不靠炮手提供武器时无需切换。
    if (this->Gunner == nullptr)
        return false;

    this->GunnerWeaponIndex = idx;
    return true;
}

// ----------------------------------------------------------------------------
// asm: UnitClass_RemoveFirstPassenger
//
//  把首位乘客卸下：从乘客链上摘下第一个，让它回到地图上；若本载具的武器
//  要靠乘客提供、而卸完之后车厢已经空了，就把武器档位一并复位。
// ----------------------------------------------------------------------------
void UnitClass::RemoveFirstPassenger()
{
    // 车厢里没人就没得卸。
    TechnoClass* pFirst = this->Attached_Object();
    if (pFirst == nullptr)
        return;

    // 从乘客链上摘下第一个。
    this->PassengerHead = static_cast<TechnoClass*>(pFirst->NextObject);
    if (this->PassengerCount > 0)
        --this->PassengerCount;

    pFirst->NextObject = nullptr;

    // 让被卸下的单位回到地图上。
    pFirst->Unlimbo();

    // 武器靠乘客提供、而车厢已经空了：档位复位。
    if (this->Attached_Object() == nullptr)
        this->GunnerWeaponIndex = -1;
}

// ============================================================================
// 变形 / 展开
// ============================================================================

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass_ToggleDeployState(UnitClass *this)   (asm:1683943)
//
//  切换本单位的"变形"状态：只有类型上允许变形、且当前确实停稳时才会进行；
//  正在展开的再点一次就收回，正在收起的再点一次就展开；其余情况免动。
// ----------------------------------------------------------------------------
void UnitClass::ToggleDeployState()
{
    // 类型不允许变形就免动。
    if (!this->IsDeployable())
        return;

    // 正在移动时不能变形。
    if (this->IsMoving())
        return;

    if (this->Undeploying || this->Deployed)
    {
        // 已经展开或正在收起：收回。
        this->Undeploy();
        this->Deployed   = false;
        this->Undeploying = false;
    }
    else
    {
        // 尚未展开：展开。
        this->Deploy();
        this->Deployed  = true;
        this->Deploying = false;
    }
}

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass_ToggleSimpleDeploy(UnitClass *this)   (asm:1684144)
//
//  切换"简易展开"（例如采矿车就地卸矿这类不改变外形的展开）：按当前是
//  展开还是收起状态，走对应的展开/收起流程；正在移动时免动。
// ----------------------------------------------------------------------------
void UnitClass::ToggleSimpleDeploy()
{
    // 正在移动时不能展开。
    if (this->IsMoving())
        return;

    if (this->Deployed)
    {
        // 已经展开：收回。
        this->Undeploy();
        this->Deployed   = false;
        this->Undeploying = false;
    }
    else
    {
        // 尚未展开：展开。
        this->Deploy();
        this->Deployed  = true;
        this->Deploying = false;
    }
}

// ============================================================================
// asm: char __thiscall UnitClass_ChangeOwnership(UnitClass *this, HouseClass_old *newOwner, int a3)   (asm:1706740)
//
//  把本单位转交给新的拥有者：随车拖挂的从车一并转手；若本单位载着乘客，
//  乘客也随之易主；最后更新自己的归属并让界面重绘。
// ============================================================================
void UnitClass::ChangeOwnership(HouseClass* pNewOwner, bool a3)
{
    // 归属没变就免动。
    if (pNewOwner == this->Owner)
        return;

    // 随车拖挂的从车一并转手；从车转手后把自己标记为"有拖挂主车"。
    if (this->FollowerCar != nullptr)
    {
        this->HasFollowerCar = false;
        this->FollowerCar->ChangeOwnership(pNewOwner, true);
        this->FollowerCar->HasFollowerCar = true;
    }

    // 车上的乘客也随之易主。
    TechnoClass* pPassenger = this->Attached_Object();
    while (pPassenger != nullptr)
    {
        TechnoClass* pNext = static_cast<TechnoClass*>(pPassenger->NextObject);

        // 乘客一并易主（从车由载具本身处理，这里只改归属）。
        if (pPassenger->Owner != pNewOwner)
        {
            pPassenger->Owner = pNewOwner;
        }

        pPassenger = pNext;
    }

    // 更新自己的归属。
    this->Owner = pNewOwner;

    (void)a3;
}

// ============================================================================
// asm: UnitClass_ExitObject
//
//  让本载具里的第一名乘客下车：把乘客从载具的乘客链上摘下来、放到载具附近
//  的空地上、解除"谁运着谁"的关联，随后乘客回到地图上自由行动。载具里没有
//  乘客时直接返回空。
// ============================================================================
FootClass* UnitClass::ExitObject()
{
    // asm: char __thiscall UnitClass_Die(UnitClass *this)   (asm:1708014)
    if (this->PassengerCount <= 0 || this->PassengerHead == nullptr) {
        return nullptr;
    }

    // asm: char __thiscall UnitClass_Die(UnitClass *this)   (asm:1708014)
    TechnoClass* pPassenger = this->PassengerHead;
    this->PassengerHead = static_cast<TechnoClass*>(pPassenger->NextObject);
    --this->PassengerCount;

    FootClass* pFoot = reinterpret_cast<FootClass*>(pPassenger);

    // asm: char __thiscall UnitClass_Die(UnitClass *this)   (asm:1708014)
    //  载具旁边的空地。
    pFoot->LetGoOfUnit();
    pFoot->Unlimbo();

    return pFoot;
}

// ============================================================================
// asm: char __thiscall UnitClass_Die(UnitClass *this)   (asm:1708014)
//
//  载具死亡的统一入口：先把车上的乘客全部赶出去（车都炸了乘客不能再坐），
//  再走通用死亡流程处理残骸、经验与统计。已经死过的单位重复调用没有效果。
// ============================================================================
void UnitClass::Die(TechnoClass* pKiller)
{
    // asm: UnitClass_AutoCrushSomething
    if (this->IsDyingNow) {
        return;
    }

    // asm: UnitClass_AutoCrushSomething
    while (this->PassengerCount > 0 && this->PassengerHead != nullptr) {
        FootClass* pFoot = this->ExitObject();
        (void)pFoot;
    }

    // asm: UnitClass_AutoCrushSomething
    this->TechnoClass::Die(pKiller);
}

// ============================================================================
// asm: UnitClass_AutoCrushSomething
//
//  自动碾压：能碾的单位在移动途中贴到可碾压的目标时，不需要玩家下令就顺手
//  碾过去。这里在载具脚下找一圈相邻格，发现有可碾压的步兵就执行碾压并把
//  目标直接压死。返回真表示这一帧碾到了东西。
// ============================================================================
bool UnitClass::AutoCrushSomething()
{
    // asm: UnitClass_VisceroidWander
    if (this->Type == nullptr || !this->Type->Crusher) {
        return false;
    }

    CoordStruct here = this->GetCoords();
    const CellStruct cell = CellClass::Coord2Cell(here);

    // asm: UnitClass_VisceroidWander
    //  看到"隔空碾压"的怪相。
    CellClass* pCell = TheMap->GetCellAt(cell.X, cell.Y);
    if (pCell == nullptr) {
        return false;
    }

    ObjectClass* pObj = pCell->Get_Occupier();
    if (pObj == nullptr) {
        return false;
    }

    // asm: UnitClass_VisceroidWander
    if (pObj->WhatAmI() != AbstractType::Infantry) {
        return false;
    }

    FootClass* pFoot = reinterpret_cast<FootClass*>(pObj);
    if (!this->CanCrush(pFoot)) {
        return false;
    }

    // asm: UnitClass_VisceroidWander
    //  asm: TechnoClass_ReceiveDamage(pFoot, &health, 0, nullptr, 0, 0, 0, nullptr)
    int32 crushDamage = pFoot->Health;
    pFoot->ReceiveDamage(&crushDamage, 0, nullptr, 0, 0, 0, nullptr);
    return true;
}

// ============================================================================
// asm: UnitClass_VisceroidWander
//
//  病毒体的游荡步进：病毒体没有目标时在矿脉附近随机换方向挪一格；发现附近
//  有别的病毒体时朝它靠过去（同类相吸，会合体）。这一步只决定"往哪挪"，
//  真正的挪动交给移动系统。
// ============================================================================
void UnitClass::VisceroidWander()
{
    // asm: vt_UnitClass.DrawVXL   (asm:?)
    //  的都是病毒体，这里不再重复判定类型。
    if (this->Type == nullptr) {
        return;
    }

    CoordStruct here = this->GetCoords();
    const CellStruct cell = CellClass::Coord2Cell(here);

    // asm: vt_UnitClass.DrawVXL   (asm:?)
    //  矿脉地——病毒体离开矿脉会持续掉血，所以它们只在矿上打转。
    const int32 dir = Game::GetCurrentFrame() % 8;
    static const int32 dxTab[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int32 dyTab[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

    const int32 nx = cell.X + dxTab[dir];
    const int32 ny = cell.Y + dyTab[dir];

    if (!TheMap->IsValidCell(nx, ny)) {
        return;
    }

    CellClass* pNext = TheMap->GetCellAt(nx, ny);
    if (pNext == nullptr || !pNext->IsTiberium()) {
        return;
    }

    // asm: vt_UnitClass.DrawVXL   (asm:?)
    CoordStruct dest;
    pNext->ConvertCoords(&dest);
    this->Set_Destination(dest);
}

// ============================================================================
// asm: vt_UnitClass.DrawVXL   (asm:?)
//
//  载具的绘制入口分两层：DrawVXL 负责"把车体画出来"（按当前朝向挑帧、按
//  受损情况调色），DrawAt 负责"画在哪"（把世界坐标折算成屏幕坐标后交给
//  绘制层）。两层分开，让潜地、运输等特殊状态的绘制也能复用同一套画体逻辑。
// ============================================================================
void UnitClass::DrawVXL(const Point2D& coords, const RectangleStruct& rect)
{
    if (this->Type == nullptr) {
        return;
    }

    // 车体绘制：整条链（阴影 -> 本体 -> 闪白 -> 选中框 -> 血条）由 Draw()
    // 统一负责，这里只是把 const 入参抄成可写局部量后转进去，保证这个虚槽
    // 不会是一个空实现。
    Point2D point(coords.X, coords.Y);
    RectangleStruct viewRect = rect;
    this->Draw(point, viewRect);
}

void UnitClass::DrawAt(const Point2D& coords, const RectangleStruct& rect)
{
    // 根据游戏行为，可知 DrawAt 是"落地"的那一步：先算屏幕坐标，再调画体。
    this->DrawVXL(coords, rect);
}

// ============================================================================
// 逐帧入口 / 开火 / 任务处理器（asm: UnitClass_Update / _Fire / Mission_*）
//
//  这一组函数把载具"能动、能打、能采矿"的骨架补齐：Update 是 vec_Logics 每帧
//  驱动的唯一入口（虚表 +0x5C），Fire 是 vt+0x1E4，Mission_* 是任务跳表上的
//  载具特化分支。所有分支编号与控制流都对齐 asm，字段按功能等价映射。
// ============================================================================
namespace {

// 由两点求 0..255 的朝向分度（与 Desired_Facing256 的取值空间一致）。
DirStruct FacingToPoint(const CoordStruct& from, const CoordStruct& to)
{
    const int32 dx = to.X - from.X;
    const int32 dy = to.Y - from.Y;
    if (dx == 0 && dy == 0)
        return DirStruct(0);

    const double ang = std::atan2(static_cast<double>(dy),
                                  static_cast<double>(dx));
    int32 val = static_cast<int32>(std::round(ang * 128.0 / 3.14159265358979323846));
    val &= 0xFF;
    return DirStruct(static_cast<uint8>(val));
}

// 在 owner 的建筑里找一座符合谓词的建筑（离 pFrom 最近者）。
template <typename Pred>
BuildingClass* FindNearestBuilding(HouseClass* pOwner, const CoordStruct& from, Pred pred)
{
    if (pOwner == nullptr || BuildingClass::Array == nullptr)
        return nullptr;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
    {
        BuildingClass* pBldg = (*BuildingClass::Array)[i];
        if (pBldg == nullptr || pBldg->Type == nullptr)
            continue;
        if (pBldg->Owner != pOwner || pBldg->IsInLimbo)
            continue;
        if (!pred(pBldg))
            continue;

        const CoordStruct pos = pBldg->GetCoords();
        const int32 d = from.DistanceFrom(pos);
        if (d < bestDist) {
            bestDist = d;
            pBest = pBldg;
        }
    }

    return pBest;
}

} // namespace

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_Update(UnitClass *this)   (asm:277)
//
//  逐帧入口。asm 的分段与这里的段落一一对应：
//    A  (+10..+B0)   失能/垂死载具每 30 帧冒一次烟（粒子系统）。
//    B  (+F1..+131)  抽能状态失去来源 → TechnoClass_ExpireDrain。
//    C  (+14C)       TemporalTargettingMe（超时空武器）逐帧。
//    D  (+13F..+1A0) 展开/卸载中每 24 帧一次时空火花动画。
//    E  (+2A4..+2C1) 死亡计数推进，到 MaxDeathCounter 播爆炸。
//    F  (+32C..+3AD) AI 名下的"可展开单位"（MCV）自动排队展开任务。
//    G  (+3C2)       FootClass::AI（父类逐帧，含 TechnoClass_Update）。
//    H  (+3CE)       静止伪装单位的伪装刷新。
//    I  (+5DD..+637) 运兵车舱门开合；开火帧计数递减。
//    J  (+64A..+700) AI 受伤单位每 16 帧找维修厂回厂。
//    K  (+81D..+980) AI 矿车闲置 300 帧后自行回矿/回厂。
// ----------------------------------------------------------------------------
void UnitClass::Update()
{
    const int32 frame = Game::GetCurrentFrame();
    RulesClass* pRules = RulesClass::Instance;

    // ---- 段 A：失能载具的残烟 ------------------------------------------
    if (IsDeactivated() && (frame % 30) == 0)
    {
        AnimTypeClass* pSmoke = AnimTypeClass::Find("SGRYSMK1");
        if (pSmoke != nullptr)
        {
            CoordStruct pos = GetCoords();
            if (ScenarioClass::Instance != nullptr) {
                pos.X += ScenarioClass::Instance->Random.Next(-100, 100);
                pos.Y += ScenarioClass::Instance->Random.Next(-100, 100);
            }
            pos.Z += 10;
            new AnimClass(pSmoke, pos, 0, 1, 0x600, 0, false);
        }
    }

    // ---- 段 B：抽能状态失效 --------------------------------------------
    //  asm 1677708：抽能源所在的格子已换建筑/建筑消失 → 结束抽能。
    if (DrainTimer > 0)
    {
        CellClass* pCell = GetCell1();
        BuildingClass* pBuilding = (pCell != nullptr) ? pCell->Cell_Building() : nullptr;
        if (pBuilding == nullptr || (!IsDraining() && !IsBeingDrained()))
            ExpireDrain();
    }

    // ---- 段 C：超时空武器逐帧 ------------------------------------------
    if (TemporalTargettingMe != nullptr)
        TemporalTargettingMe->Update();

    // ---- 段 D：展开/卸载中的时空火花 -----------------------------------
    if ((Deployed || Deploying || Undeploying || Unloading)
        && (frame % 24) == 0
        && pRules != nullptr && pRules->ChronoSparkle1 != nullptr)
    {
        CoordStruct pos = GetCoords();
        pos.Z += 0x78;
        new AnimClass(pRules->ChronoSparkle1, pos, 0, 1, 0x600, 0, false);
    }

    // ---- 段 E：死亡计数 -------------------------------------------------
    //  原版把 DeathCounter 初值置 -1 表示"没在死"；本项目构造时置 0，
    //  因此用 Health <= 0 作为同一个前置门，避免活着的单位被推进。
    if (Health <= 0)
    {
        const int32 maxDeath = (Type != nullptr) ? Type->MaxDeathCounter : 1;
        if (DeathFrameCounter < maxDeath) {
            ++DeathFrameCounter;
        } else if (DeathFrameCounter == maxDeath) {
            // 只播一次：把计数推过阈值，避免每帧重复爆炸。
            ++DeathFrameCounter;
            if (!IsDyingNow) {
                Explode();
                Mark(MarkType::Up);
            }
        }
    }

    // ---- 段 F：AI 的 MCV 自动展开 ---------------------------------------
    if (Type != nullptr && Type->DeploysInto != nullptr
        && Owner != nullptr && !Owner->Is_Human() && pRules != nullptr)
    {
        bool isBaseConstant = false;
        for (int32 i = 0; i < pRules->BuildConst.Count; ++i)
        {
            if (pRules->BuildConst[i] == Type->DeploysInto) {
                isBaseConstant = true;
                break;
            }
        }

        // asm: 当前任务已经是 0Fh/10h（展开/卸载）时不再重复排队。
        if (isBaseConstant
            && GetMission() != Mission::Hunt
            && GetMission() != Mission::Unload)
        {
            QueueMission(Mission::Hunt);
        }
    }

    // ---- 段 G：父类逐帧 -------------------------------------------------
    // asm 1678024..1678031: `call [vt+200h]` 为真 → `call [vt+1ECh]`。
    //   ReadyToNextMission 是任务层的"本步已完成"门，NextMission 把
    //   排队中的任务提升为运行任务。
    if (ReadyToNextMission())
        NextMission();

    FootClass::Update();

    // ---- 段 H：静止伪装刷新 ---------------------------------------------
    if (Type != nullptr && Type->DisguiseWhenStill && IsDisguised())
        UpdateDisguise();

    // ---- 段 I：舱门与开火帧 ---------------------------------------------
    if (Type != nullptr && Type->Passengers > 0)
    {
        // asm: +5E0（载员数）> 0 时维持舱门状态：非卸载任务则关门。
        if (!Unloading && GetMission() != Mission::Unload)
            APCCloseDoor();
    }

    if (unknown_81C > 0)
        --unknown_81C;

    // ---- 段 J：AI 受伤单位回维修厂 ---------------------------------------
    if ((frame & 0x0F) == 0 && Owner != nullptr && !Owner->Is_Human()
        && Type != nullptr && !Type->Harvester && !Type->Weeder
        && Health > 0 && Health < GetMaxHealth()
        && (GetMission() == Mission::Guard || GetMission() == Mission::AreaGuard))
    {
        BuildingClass* pBay = Owner->FindRepairBay(this);
        if (pBay != nullptr)
        {
            SetDestination(pBay->GetCoords());
            SetTarget(nullptr);
            QueueMission(Mission::Enter);
        }
    }

    // ---- 段 K：AI 矿车闲置自动回矿 ---------------------------------------
    if (Type != nullptr && Type->Harvester && Owner != nullptr && !Owner->Is_Human()
        && (frame - CurrentMissionStartTime) > 300)
    {
        LocomotionClass* pLoco = Locomotion;
        const bool moving = (pLoco != nullptr) && pLoco->Is_Moving();

        if (!moving)
        {
            const CoordStruct here = GetCoords();
            BuildingClass* pRefinery = FindNearestBuilding(
                Owner, here,
                [](BuildingClass* pB) {
                    return pB->Type != nullptr && pB->Type->Refinery;
                });

            if (pRefinery != nullptr)
            {
                // 有空矿舱就回炼油厂卸，否则回矿脉。
                if (HarvestAmount > 0 || IsDumpingTiberium) {
                    SetDestination(pRefinery->GetCoords());
                    QueueMission(Mission::Unload);
                } else {
                    CellStruct ore;
                    if (Find_Ore(ore)) {
                        SetDestination(Math::CellToCoord(ore));
                        QueueMission(Mission::Harvest);
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// asm: BulletClass *__thiscall UnitClass_Fire(UnitClass *this,
//          AbstractClass *target, int idxWeapon)   (asm:1697189, retn 8)
//
//  1) GetWeapon(idx) 取不到武器 → 返回 0；
//  2) idx==0 时按"连发序号 % Burst"查类型的 FiringSyncFrame；若当前已经在
//     开火帧计数中且查到了同步帧，则本次不重设开火帧（bl = 1）；
//  3) GetObjectFiringState(1, idx, target) != 0（够不着）→ 返回 0；
//  4) 转交 TechnoClass_Fire，得到弹丸；
//  5) 弹匣未满且非 ManualReload → Techno_Update_Reloading；
//  6) 非"沿用"时把开火帧设为 2*FiringFrames-1；
//  7) 可缩放弹丸 → BulletClass_InitScalable；
//  8) 静止伪装类型 → 记下帧/武器/闪烁时长三元组。
// ----------------------------------------------------------------------------
BulletClass* UnitClass::Fire(AbstractClass* pTarget, int32 nWeaponIndex)
{
    if (TechnoType == nullptr)
        return nullptr;

    WeaponStruct* pSlot = TechnoType->GetWeapon(nWeaponIndex);
    if (pSlot == nullptr || pSlot->WeaponType == nullptr)
        return nullptr;

    WeaponTypeClass* pWeapon = pSlot->WeaponType;

    // ---- 连发同步帧 -----------------------------------------------------
    //  原版用 CurrentSpawnIndex % Burst 选同步帧；本项目没有连发游标，
    //  以"是否正处于开火帧计数中"表达 asm 的第二个判断（field_6C0 != -1）。
    bool keepFiringFrames = false;
    if (nWeaponIndex == 0 && unknown_81C != 0)
    {
        const int32 syncFrame = (Type != nullptr && Type->FiringSyncFrame[0] != 0)
                              ? Type->FiringSyncFrame[0] : -1;
        if (syncFrame != -1)
            keepFiringFrames = true;
    }
    (void)pWeapon->Burst;

    // ---- 够不够得着 -----------------------------------------------------
    //  asm 的 GetObjectFiringState(1=rangeMatters, idx, target)：射程内才允许
    //  继续。这里直接复用范围判定（含空中加成、高地加成、最小射程门）。
    if (pTarget != nullptr && !IsCloseEnoughToTarget(pTarget, nWeaponIndex))
        return nullptr;

    // ---- 真正开火（基类负责弹丸生成与音效）------------------------------
    BulletClass* pBullet = TechnoClass::Fire(pTarget, nWeaponIndex);
    if (pBullet == nullptr)
        return nullptr;

    // ---- 装填 -----------------------------------------------------------
    if (Type != nullptr && Type->Ammo > 0
        && CurrentAmmo < Type->Ammo && !Type->ManualReload)
    {
        Update_Reloading();
    }

    // ---- 开火帧 ---------------------------------------------------------
    if (!keepFiringFrames && Type != nullptr && Type->FiringFrames > 0)
        unknown_81C = static_cast<DWORD>(Type->FiringFrames * 2 - 1);

    // ---- 可缩放弹丸 -----------------------------------------------------
    if (pBullet->Class != nullptr && pBullet->Class->Scalable)
        pBullet->InitScalable();

    // ---- 静止伪装闪烁 ---------------------------------------------------
    if (Type != nullptr && Type->DisguiseWhenStill)
    {
        unknown_82C = static_cast<DWORD>(Game::GetCurrentFrame());
        unknown_830 = static_cast<DWORD>(nWeaponIndex);
        unknown_834 = static_cast<DWORD>(pWeapon->DisguiseFakeBlinkTime);
    }

    return pBullet;
}

// ----------------------------------------------------------------------------
// asm: signed int __thiscall UnitClass::Mission_Move(UnitClass *this)   (asm:8261)
//
//  IsHarvesting 先清 0；展开/收起/展开中三者任一成立 → 排队 Guard 并报告
//  任务已完成（返回 1）；否则先确认舱门关好，再走 FootClass::Mission_Move。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Move()
{
    IsHarvestingTiberium = false;

    if (Deployed || Deploying || Undeploying)
    {
        QueueMission(Mission::Guard);
        return 1;
    }

    // asm: Door_Open 返回 false → Close_Door(type->field_3C4, 3C8)。
    if (Type != nullptr && Type->Passengers > 0)
        APCCloseDoor();

    return FootClass::Mission_Move();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass::Mission_Guard(UnitClass *this)   (asm:8033)
//
//  1) 奴隶矿车：管理器到点唤醒 → 直接去唤醒分支（本项目无该字段，跳过）；
//  2) 矿车/除草车且非玩家所有：若名下任一 Dock 建筑还有空位，就转去采矿；
//  3) 可展开单位（DeploysInto）且类型在 Rules->BuildConst 里、业主是 AI
//     且开了自动建造 → 排队 10h（Unload）；
//  4) 其余退回 FootClass::Mission_Guard。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Guard()
{
    if (Type != nullptr && (Type->Harvester || Type->Weeder))
    {
        if (Owner != nullptr && !Owner->Is_Human())
        {
            // asm: 遍历 type->Dock，用业主的计数器查每种 Dock 的在场数量；
            //      有任意一种在场就说明"有地方卸矿"。
            bool hasDock = false;
            for (int32 i = 0; i < Type->Dock.Count; ++i)
            {
                BuildingTypeClass* pDockType = Type->Dock[i];
                if (pDockType == nullptr)
                    continue;

                if (FindNearestBuilding(Owner, GetCoords(),
                        [pDockType](BuildingClass* pB) {
                            return pB->Type == pDockType;
                        }) != nullptr)
                {
                    hasDock = true;
                    break;
                }
            }

            if (hasDock)
            {
                // asm: 满舱 / 正在卸 → 直接转 Harvest 由任务层接着跑。
                QueueMission(Mission::Harvest);
                return 1;
            }
        }
    }

    // ---- 可展开单位（MCV）的自动展开 ------------------------------------
    if (Type != nullptr && Type->DeploysInto != nullptr
        && Owner != nullptr && !Owner->Is_Human())
    {
        RulesClass* pRules = RulesClass::Instance;
        if (pRules != nullptr)
        {
            for (int32 i = 0; i < pRules->BuildConst.Count; ++i)
            {
                if (pRules->BuildConst[i] != Type->DeploysInto)
                    continue;

                QueueMission(Mission::Unload);
                return 1;
            }
        }
    }

    return FootClass::Mission_Guard();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass_Hunt(UnitClass *this)
//      （见 UnitClass::Goto_Clear_Spot+7B 的调用点）
//
//  猎杀前先确保自己站在一个可用的空位上：没有目标且原地被占 → 找空位。
//  之后一律交给 MissionClass::Mission_Hunt 做通用猎杀。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Hunt()
{
    if (!HasTarget())
    {
        // asm: 目标为空时先 Goto_Clear_Spot，把车挪出拥挤的格子。
        GotoClearSpot();
    }

    return MissionClass::Mission_Hunt();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass_Mission_Return(UnitClass *this)
//
//  矿车：满舱 → 回最近炼油厂（Unload）；空舱 → 回矿脉（Harvest）。
//  其余载具：退回基类的返航处理。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Return()
{
    if (Type != nullptr && (Type->Harvester || Type->Weeder) && Owner != nullptr)
    {
        const CoordStruct here = GetCoords();

        if (HarvestAmount > 0 || IsDumpingTiberium)
        {
            BuildingClass* pRefinery = FindNearestBuilding(
                Owner, here,
                [](BuildingClass* pB) {
                    return pB->Type != nullptr && pB->Type->Refinery;
                });

            if (pRefinery != nullptr)
            {
                SetDestination(pRefinery->GetCoords());
                if (!Is_Moving())
                    QueueMission(Mission::Unload);
                return 0;
            }
        }
        else
        {
            CellStruct ore;
            if (Find_Ore(ore))
            {
                SetDestination(Math::CellToCoord(ore));
                QueueMission(Mission::Harvest);
                return 0;
            }
        }
    }

    return MissionClass::Mission_Return();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass_Mission_Enter(UnitClass *this)
//
//  进入目标（建筑/运输载具）：目标还在但自己够不着 → 先把目标格设为目的地
//  继续靠近，返回 0（任务未完成）；已经贴上或没有目标 → 交给基类处理。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Enter()
{
    AbstractClass* pTarget = GetTarget();
    if (pTarget != nullptr && pTarget != this)
    {
        CoordStruct tgt;
        pTarget->GetCoords(&tgt);

        const int32 dist = GetCoords().DistanceFrom(tgt);
        // asm: 以 0x100（一格）为"贴近"阈值，远了先走过去。
        if (dist > 0x100)
        {
            SetDestination(tgt);
            return 0;
        }
    }

    return FootClass::Mission_Enter();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass_Unload(UnitClass *this)   (asm:1690071)
//
//  卸载三段：
//    1) 矿车停在自家炼油厂格里 → 直接把矿石舱结算成信用点（DumpTiberium），
//       卸完前停在 Unload（返回 0）；不在炼油厂则报告完成（返回 1）让上层
//       重新规划路线。
//    2) 有载员上限的运兵车 → 开门、每次放一名乘客下车；放空后关门。
//    3) 简易展开单位（如已就位需要收起）→ 走 ToggleSimpleDeploy。
//  其余情况退回 FootClass::Mission_Unload。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Unload()
{
    // ---- 1) 矿车卸矿 ----------------------------------------------------
    if (Type != nullptr && (Type->Harvester || Type->Weeder))
    {
        if (HarvestAmount <= 0 && !IsDumpingTiberium)
            return 1;

        BuildingClass* pRefinery = nullptr;
        if (MapClass::Instance != nullptr)
        {
            CellClass* pCell = MapClass::Instance->GetCellAt(GetCoords());
            if (pCell != nullptr)
                pRefinery = pCell->Cell_Building();
        }

        if (pRefinery != nullptr && pRefinery->Type != nullptr
            && pRefinery->Type->Refinery
            && (Owner == nullptr || pRefinery->Owner == Owner))
        {
            IsDumpingTiberium = true;
            DumpTiberium();
            return 0;
        }

        // 不在这里：交给 Return 去跑路。
        if (Owner != nullptr)
        {
            BuildingClass* pDock = FindNearestBuilding(
                Owner, GetCoords(),
                [](BuildingClass* pB) {
                    return pB->Type != nullptr && pB->Type->Refinery;
                });
            if (pDock != nullptr) {
                SetDestination(pDock->GetCoords());
                return 0;
            }
        }
        return 1;
    }

    // ---- 2) 运兵车放人 --------------------------------------------------
    if (Type != nullptr && Type->Passengers > 0)
    {
        if (PassengerHead != nullptr)
        {
            APCOpenDoor();
            ExitObject();
            return 0;
        }

        APCCloseDoor();
        Unloading = false;
        return 1;
    }

    // ---- 3) 简易展开单位收起 --------------------------------------------
    if (Type != nullptr && Type->IsSimpleDeployer && Deployed)
    {
        ToggleSimpleDeploy();
        return 1;
    }

    return FootClass::Mission_Unload();
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass_Mission_Capture(UnitClass *this)
//
//  载具本身没有占领能力，保留覆写只是为了把任务表槽位对齐；实体行为退回
//  MissionClass 的通用实现（工程师车一类由步兵侧处理）。
// ----------------------------------------------------------------------------
int32 UnitClass::Mission_Capture()
{
    return MissionClass::Mission_Capture();
}

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass_Harvest(UnitClass *this)   (asm:6482)
//
//  采一步矿：脚下不是矿就就近找一格；每步把矿格扣掉"装得下的量"，并把这
//  些量按该格矿石单价折算进 TotalTiberiumValue；装满后切到卸矿状态。
// ----------------------------------------------------------------------------
bool UnitClass::Harvest()
{
    if (Type == nullptr || (!Type->Harvester && !Type->Weeder))
        return false;

    if (MapClass::Instance == nullptr)
        return false;

    // 已经满了：不再采，转去卸。
    if (IsHarvestingTooMuch())
    {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
        return false;
    }

    CellStruct cell = Math::CoordToCell(GetCoords());
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);

    if (pCell == nullptr || !pCell->IsTiberium())
    {
        CellStruct oreCell;
        if (!Find_Ore(oreCell))
            return false;

        cell = oreCell;
        pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell == nullptr)
            return false;
    }

    const int32 available = pCell->Get_Tiberium_Value();
    if (available <= 0)
        return false;

    const int32 capacity = (Type->Storage > 0) ? Type->Storage : HARVEST_CAPACITY_DEFAULT;
    int32 take = capacity - HarvestAmount;
    if (take <= 0)
    {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
        return false;
    }
    if (take > available)
        take = available;

    // 扣矿。
    const int32 oreType = pCell->Get_Tiberium_Type();
    const int32 left = available - take;
    pCell->Set_Tiberium(oreType, left);

    // 入舱。
    HarvestAmount += take;
    TotalTiberiumValue += take * Get_Ore_Value(cell);
    IsHarvestingTiberium = true;

    SetSequence(Sequence::Harvest);

    if (HarvestAmount >= capacity)
    {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
    }

    return true;
}

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass::Find_Ore(UnitClass *this, CellStruct *out)
//
//  以自身所在格为中心做环状扩散搜索（半径 0..12），找到第一格含矿的地块
//  就写回 out 并报告成功。逐环搜索保证"最近优先"，与 asm 的螺旋扫描一致。
// ----------------------------------------------------------------------------
bool UnitClass::Find_Ore(CellStruct& outCell)
{
    if (MapClass::Instance == nullptr)
        return false;

    const CellStruct here = Math::CoordToCell(GetCoords());
    const int32 maxRadius = 12;

    for (int32 r = 0; r <= maxRadius; ++r)
    {
        for (int32 dx = -r; dx <= r; ++dx)
        {
            for (int32 dy = -r; dy <= r; ++dy)
            {
                // 只走当前环，内圈已经查过。
                if (r > 0 && std::abs(dx) != r && std::abs(dy) != r)
                    continue;

                const int32 x = static_cast<int32>(here.X) + dx;
                const int32 y = static_cast<int32>(here.Y) + dy;
                if (!MapClass::Instance->IsValidCell(x, y))
                    continue;

                CellClass* pCell = MapClass::Instance->GetCellAt(x, y);
                if (pCell == nullptr || !pCell->IsTiberium())
                    continue;

                outCell = CellStruct(static_cast<int16>(x), static_cast<int16>(y));
                return true;
            }
        }
    }

    return false;
}

// ----------------------------------------------------------------------------
// asm: int __thiscall UnitClass_GetOreValue(UnitClass *this, CellStruct *cell)
//
//  一格矿石的单价：优先取地图格子记录的 TiberiumValue；没有记录时退回
//  该格当前的矿量；两者都为零时给一个保底值，避免把矿石算成 0 收益。
// ----------------------------------------------------------------------------
int32 UnitClass::Get_Ore_Value(const CellStruct& cell) const
{
    if (MapClass::Instance != nullptr)
    {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell != nullptr)
        {
            if (pCell->TiberiumValue > 0)
                return pCell->TiberiumValue;

            const int32 value = pCell->Get_Tiberium_Value();
            if (value > 0)
                return value;
        }
    }

    // 原版矿石基础价（每单位 10 信用点）。
    return 10;
}

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass_Can_Enter(UnitClass *this, FootClass *p)
//
//  运兵车收人：必须有载员位、自己不在 limbo、还没满员，且不能收自己。
// ----------------------------------------------------------------------------
bool UnitClass::Can_Enter(FootClass* pPassenger) const
{
    if (pPassenger == nullptr)
        return false;
    if (Type == nullptr || Type->Passengers <= 0)
        return false;
    if (IsInLimbo)
        return false;
    if (pPassenger == this)
        return false;
    if (PassengerCount >= Type->Passengers)
        return false;

    return true;
}

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass_On_Enter(UnitClass *this, FootClass *p)
//
//  把乘客挂到乘客链尾（NextObject 单向链），计数加一，并让乘客进入 limbo
//  （不再参与地图占位与逐帧驱动）。
// ----------------------------------------------------------------------------
bool UnitClass::On_Enter(FootClass* pPassenger)
{
    if (!Can_Enter(pPassenger))
        return false;

    TechnoClass* pTechno = pPassenger;

    pTechno->NextObject = nullptr;
    if (PassengerHead == nullptr)
    {
        PassengerHead = pTechno;
    }
    else
    {
        TechnoClass* pTail = PassengerHead;
        while (pTail->NextObject != nullptr)
            pTail = static_cast<TechnoClass*>(pTail->NextObject);
        pTail->NextObject = pTechno;
    }

    ++PassengerCount;
    pPassenger->Limbo();

    return true;
}

// ----------------------------------------------------------------------------
// asm: char __thiscall UnitClass_On_Exit(UnitClass *this, FootClass *p)
//
//  从乘客链上摘掉指定乘客，放到车旁的空地并解除运载；找不到这名乘客时
//  什么都不做并报告失败。
// ----------------------------------------------------------------------------
bool UnitClass::On_Exit(FootClass* pPassenger)
{
    if (pPassenger == nullptr || PassengerHead == nullptr)
        return false;

    TechnoClass* pWanted = pPassenger;

    if (PassengerHead == pWanted)
    {
        PassengerHead = static_cast<TechnoClass*>(pWanted->NextObject);
    }
    else
    {
        TechnoClass* pPrev = PassengerHead;
        while (pPrev->NextObject != nullptr
               && static_cast<TechnoClass*>(pPrev->NextObject) != pWanted)
        {
            pPrev = static_cast<TechnoClass*>(pPrev->NextObject);
        }

        if (pPrev->NextObject == nullptr)
            return false;

        pPrev->NextObject = pWanted->NextObject;
    }

    pWanted->NextObject = nullptr;
    if (PassengerCount > 0)
        --PassengerCount;

    // 落在车旁一格，然后回到地图。
    const CoordStruct here = GetCoords();
    pPassenger->Move_To(CoordStruct(here.X + 128, here.Y + 128, here.Z));
    pPassenger->Unlimbo();

    return true;
}

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_Kill_Passengers(UnitClass *this,
//          TechnoClass *killer)
//
//  车毁时乘客不再被"放出来"，而是按同一凶手逐个结算死亡。逐个摘链后调用
//  ReceiveDamage 用乘客当前生命值打满，走正常的死亡流程（经验、残骸、
//  触发器都由它统一处理）。
// ----------------------------------------------------------------------------
void UnitClass::Kill_Passengers(TechnoClass* pKiller)
{
    while (PassengerCount > 0 && PassengerHead != nullptr)
    {
        TechnoClass* pVictim = PassengerHead;
        PassengerHead = static_cast<TechnoClass*>(pVictim->NextObject);
        pVictim->NextObject = nullptr;
        --PassengerCount;

        int32 damage = pVictim->Health;
        if (damage <= 0)
            damage = 1;

        (void)pKiller;  // ReceiveDamage 的凶手由弹头/战斗层补，这里只负责致死。
        pVictim->ReceiveDamage(&damage, 0, nullptr, 0, 0, 0, nullptr);
    }
}

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_UpdateTurret(UnitClass *this,
//          AbstractClass *target)
//
//  炮塔逐帧：求目标方向的期望朝向，按类型 ROT（每帧最多转多少分度）限制
//  步长向它靠拢，最后同步 BarrelDir。没有炮塔或没有目标时不动。
// ----------------------------------------------------------------------------
void UnitClass::Update_Turret(AbstractClass* pTarget)
{
    if (!HasTurret() || pTarget == nullptr)
        return;

    CoordStruct tgt;
    pTarget->GetCoords(&tgt);

    const DirStruct want = FacingToPoint(GetCoords(), tgt);

    // 每帧最大转角：优先用类型 ROT，缺省用一个保守值。
    int32 rate = (Type != nullptr && Type->ROT > 0)
               ? Type->ROT : TURRET_ROT_SPEED_DEFAULT;
    if (rate <= 0)
        rate = 1;

    const int32 diff = Facing::Difference(TurretDir, want);
    int32 step = diff;
    if (step >  rate) step =  rate;
    if (step < -rate) step = -rate;

    int32 val = static_cast<int32>(TurretDir.Value) + step;
    val &= 0xFF;
    TurretDir = DirStruct(static_cast<uint8>(val));

    // 炮管跟随炮塔。
    BarrelDir = TurretDir;

    // 已经对上目标就记下朝向（供绘制/射击命中判定使用）。
    if (step == 0)
        SetTurretFacing(TurretDir);
}

// ----------------------------------------------------------------------------
// asm: void __thiscall UnitClass_Crash(UnitClass *this)
//
//  失去升力的空中载具逐帧下沉；落地（Z <= 0）时按"无凶手"登记摧毁并停下。
//  地面单位调用本函数没有效果。
// ----------------------------------------------------------------------------
void UnitClass::Crash()
{
    if (!IsJumpJet && !IsCarryall && !IsCarryAllFly)
        return;

    CoordStruct pos = GetCoords();
    if (pos.Z > 0)
    {
        pos.Z -= 20;
        if (pos.Z < 0)
            pos.Z = 0;

        SetCoords(pos);
    }

    if (pos.Z <= 0)
    {
        IsCarryAllFly = false;
        if (!IsDyingNow)
            Die(nullptr);
    }
}
