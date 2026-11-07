#include "RadSiteClass.h"
#include "LightSourceClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Rules/RulesClass.h"
#include "../Game/Externs.h"
#include "../Game/Game.h"
#include "../Abstract/AbstractClass.h"

#include <cmath>

// =============================================================================
// Special/RadSiteClass.cpp - 辐射区对象实现
//
// 权威来源：/workspace/asm_export/RadSiteClass.asm.txt 及主 asm 中
// `RadSiteClass struc (sizeof=0x74)` 的结构定义。逐 proc 与 asm 一一对应。
// =============================================================================

// asm: vec_RadSites —— 全局辐射区池（定义在 Game/Globals.cpp）。
// static DynamicVectorClass<RadSiteClass*>* RadSiteClass::Array = nullptr;

namespace {

// asm: Float_To_Int_Floor —— 截断取整（向负无穷）。这里用 std::floor 等价表达。
inline int32 FloorToInt(double v)
{
    return static_cast<int32>(std::floor(v));
}

// asm 全局 Frame —— 本项目以 Game::CurrentFrame 承接。
inline int32 FrameNow()
{
    return Game::CurrentFrame;
}

// asm 中的 0x80808081 magic 序列表达 `c * 1000 / 255`（带符号，向负无穷取整，
// sar/shr 修正即为 floor 语义）。
inline int32 ByteToScaled(int32 c)
{
    return FloorToInt(static_cast<double>(c) * 1000.0 / 255.0);
}

} // anonymous namespace

// =============================================================================
// asm: RadSiteClass_CTOR —— 默认构造
//
//   AbstractClass_CTOR；
//   [24h]=0 (LightSource) ; [28h]=Frame (RadLevelStartTime) ; [30h]=0 ;
//   [34h]=Frame (RadLightStartTime) ; [3Ch]=0 ; [40h..70h]=0 ;
//   然后 Push 进 vec_RadSites。
// =============================================================================
RadSiteClass::RadSiteClass() noexcept
    : LightSource(nullptr)
    , RadLevelStartTime(0)
    , field_2C(0)
    , RadLevelDelay(0)
    , RadLightStartTime(0)
    , field_38(0)
    , RadLightDelay(0)
    , pos(0, 0)
    , Spread(0)
    , SpreadInLeptons(0)
    , RadLevel(0)
    , CurrentLevelDecrement(0)
    , CurrentIntensity(0)
    , CurrentLightDecrement(0)
    , CurrentLightStage(0)
    , StartRadLevel(0)
    , CurrentRadLevel(0)
{
    // asm: 两处起始帧都取当前 Frame。
    RadLevelStartTime = FrameNow();
    RadLightStartTime = FrameNow();

    CurrentTint.R = 0;
    CurrentTint.G = 0;
    CurrentTint.B = 0;

    // asm: CTOR 末尾把自己加入 vec_RadSites（length >= increment 时扩容）。
    if (!Array)
        Array = new DynamicVectorClass<RadSiteClass*>();
    Array->Add(this);
}

// =============================================================================
// asm: RadSiteClass::~RadSiteClass
//
//   1) 把中心格的 RadSite 指针清 0（Map->Coord_Cell(this+40h)->SetRadSite(null)）；
//   2) Detach（本项目抽象对象无 proximity 链，等价为空操作）；
//   3) 释放 LightSource（vtable +0x20 = 析构，本项目 delete）；
//   4) 从 vec_RadSites 摘除本对象（尾部前移）；
//   5) AbstractClass_DTOR。
// =============================================================================
RadSiteClass::~RadSiteClass()
{
    // 1) 清中心格的 RadSite 反向指针。
    if (MapClass::Instance) {
        CellClass* pCell = MapClass::Instance->GetCellAt(CoordStruct(pos.X * LeptonsPerCell,
                                                                     pos.Y * LeptonsPerCell, 0));
        if (pCell)
            pCell->Set_Rad_Site(nullptr);
    }

    // 2) asm: call Detach —— 抽象层解除引用注册。本项目无对应链条，逐语义留空。

    // 3) asm: [24h] != 0 时 push 1 ; call [vt+20h] 销毁光源。
    if (LightSource) {
        delete static_cast<LightSourceClass*>(LightSource);
        LightSource = nullptr;
    }

    // 4) asm: vec_RadSites 尾部前移覆盖（等价于 erase 一次）。
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }

    // 5) asm: AbstractClass_DTOR —— C++ 基类析构自动完成。
}

// =============================================================================
// asm: RadSiteClass_GetCRC —— 转调 AbstractClass_GetCRC（retn 4）。
// =============================================================================
void RadSiteClass::ComputeCRC(CRCEngine& crc) const
{
    Compute_CRC_Abstract(crc);
}

// =============================================================================
// asm: RadSiteClass_GetAbstractDerivationID —— 返回 0x45 (69)。
// =============================================================================

// =============================================================================
// asm: RadSiteClass_GetClassID —— pClassID == null 返回 0x80004003，
//   否则把 CLSID_RadSiteClass 四个字段写进去并返回 0。
// =============================================================================
HRESULT RadSiteClass::GetClassID(CLSID* pClassID)
{
    if (!pClassID)
        return static_cast<HRESULT>(0x80004003);

    pClassID->Data1 = static_cast<uint32>(AbstractType::RadSite);
    pClassID->Data2 = 0;
    pClassID->Data3 = 0;
    pClassID->Data4[0] = 0; pClassID->Data4[1] = 0;
    pClassID->Data4[2] = 0; pClassID->Data4[3] = 0;
    pClassID->Data4[4] = 0; pClassID->Data4[5] = 0;
    pClassID->Data4[6] = 0; pClassID->Data4[7] = 0;
    return S_OK;
}

// =============================================================================
// asm: RadSiteClass_Load（retn 8）
//
//   AbstractClass_Load(object, pStm) -> edi；
//   edi < 0 直接返回；
//   object 非空时：AbstractClass_LoadTables ; [+30h]=0 ; [+28h]=Frame ;
//     [+34h]=Frame ; [+3Ch]=0 ; 重设 vtable 四指针；
//   最后 SwizzleManager_Swizzle(&SwizzleManager, object+24h) 修复光源指针。
//
// 注：本项目 AbstractClass::Load 为纯虚（接口层），实体流读写落在
//   AbstractTypeClass。辐射区是运行时对象（直接派生 AbstractClass），
//   与 ObjectClass/TechnoClass 同族，后者以 `{ return S_OK; }` 承接该槽，
//   故此处保持同构：抽象头部的读取由流层统一处理，这里只做 asm 的字段复位。
// =============================================================================
HRESULT RadSiteClass::Load(IStream* pStm)
{
    (void)pStm;

    // asm: AbstractClass_LoadTables —— 重设抽象接口身份。
    LoadTables(pStm);

    // asm: [+30h]=0 ; [+28h]=Frame ; [+34h]=Frame ; [+3Ch]=0
    RadLevelDelay = 0;
    RadLevelStartTime = FrameNow();
    RadLightStartTime = FrameNow();
    RadLightDelay = 0;

    // asm: SwizzleManager_Swizzle(&SwizzleManager, &this->LightSource) ——
    //   反序列化后把 +0x24 槽内保存的存档 id 解析为活指针。本项目暂无
    //   swizzle 设施，语义上等价于保持字段不变。
    return S_OK;
}

// =============================================================================
// asm: RadSiteClass_Save —— 直接转调 AbstractClass_Save（retn 0Ch）。
//   同 Load：运行时对象的实体实现落在对象自身，基类槽以 S_OK 承接。
// =============================================================================
HRESULT RadSiteClass::Save(IStream* pStm, BOOL fClearDirty)
{
    (void)pStm;
    (void)fClearDirty;
    return S_OK;
}

// =============================================================================
// asm: RadSite_SetBaseCell —— [this+40h] = 传入格子坐标（wXY）。
// =============================================================================
void RadSiteClass::SetBaseCell(const CellStruct& cell)
{
    pos = cell;
}

// =============================================================================
// asm: RadSite_SetCellSpread
//   [44h] = spread ; eax = spread << 8 ; eax += 80h ; [48h] = eax
// =============================================================================
void RadSiteClass::SetCellSpread(int32 spread)
{
    Spread = spread;                              // asm: mov [ecx+44h],eax
    SpreadInLeptons = (spread << 8) + 0x80;       // asm: shl eax,8 ; add eax,80h
}

// =============================================================================
// asm: RadSite_SetRadLevel
//   [4Ch] = level ; eax = RulesData[RadDurationMultiple] * level ;
//   [6Ch] = eax ; [70h] = eax
// =============================================================================
void RadSiteClass::SetRadLevel(int32 level)
{
    RadLevel = level;                             // asm: mov [ecx+4Ch], edx
    const int32 dur = TheRules ? TheRules->RadDurationMultiple : 0;
    const int32 total = dur * level;              // asm: imul eax, edx
    StartRadLevel   = total;                      // asm: mov [ecx+6Ch], eax
    CurrentRadLevel = total;                      // asm: mov [ecx+70h], eax
}

// =============================================================================
// asm: RadSiteClass_GetCurrentLevel
//   esi = [6Ch] (StartRadLevel) ; esi <= 0 -> return 0 ;
//   eax = [70h] (CurrentRadLevel) * [4Ch] (RadLevel) ; idiv esi
// =============================================================================
int32 RadSiteClass::GetCurrentLevel() const
{
    const int32 start = StartRadLevel;
    if (start <= 0)
        return 0;

    return (CurrentRadLevel * RadLevel) / start;
}

// =============================================================================
// asm: RadSiteClass_Add(amount)
//
//   edi = floor(RadLevel * CurrentRadLevel / StartRadLevel)；
//   DecreaseAppliedCells()；           ; asm: call sub_65BB50
//   RadLevel = edi + amount；
//   StartRadLevel = CurrentRadLevel = RulesData[RadDurationMultiple] * RadLevel；
//   Radiate()。
// =============================================================================
void RadSiteClass::Add(int32 amount)
{
    const int32 scaled = FloorToInt(
        static_cast<double>(RadLevel) * static_cast<double>(CurrentRadLevel)
        / static_cast<double>(StartRadLevel));

    DecreaseAppliedCells();

    RadLevel = scaled + amount;                   // asm: lea ecx,[edi+eax]

    const int32 dur = TheRules ? TheRules->RadDurationMultiple : 0;
    const int32 total = dur * RadLevel;           // asm: imul eax, ecx
    StartRadLevel   = total;
    CurrentRadLevel = total;

    Radiate();
}

// =============================================================================
// asm: RadSiteClass_Radiate
//
//   1) RadLevelStartTime = Frame ; [2Ch] = 保留 ;
//      RadLevelDelay = RulesData[RadApplicationDelay]；
//      RadLightStartTime = Frame ; [38h] = 保留 ;
//      RadLightDelay = RulesData[RadLightDelay]；
//   2) tint：tR/tG/tB = RadColor.{R,G,B} 归一到 0..1000 后乘 RadTintFactor；
//      Theatre==1(Snow) 时 R/B 强制 -0x80；
//      intensity = clamp(RadLevel * RadLevelFactor, max 2000)；
//   3) CurrentIntensity=intensity ; CurrentTint=R/G/B ;
//      CurrentLevelDecrement = StartRadLevel / RadApplicationDelay；
//      CurrentLightDecrement = StartRadLevel / RadLightDelay；
//      CurrentLightStage = intensity / CurrentLightDecrement；
//   4) 无 LightSource 则 new 一个（初值 Visibility=SpreadInLeptons、Intensity、
//      tint、中心格坐标）并 Start；否则 ChangeLevels；
//   5) IncreaseLevel()。
// =============================================================================
void RadSiteClass::Radiate()
{
    const int32 radApplicationDelay = TheRules ? TheRules->RadApplicationDelay : 0;
    const int32 radLightDelay        = TheRules ? TheRules->RadLightDelay : 0;

    // asm: 计时字段刷新（+28h/+30h 归辐射节拍，+34h/+3Ch 归光源节拍）。
    RadLevelStartTime = FrameNow();                    // asm: mov [edi], edx
    field_2C          = 0;
    RadLevelDelay     = radApplicationDelay;      // asm: mov [edi+8], ecx
    RadLightStartTime = FrameNow();
    field_38          = 0;
    RadLightDelay     = radLightDelay;            // asm: mov [edx+8], eax

    // asm: 取 RadColor 三字节；edx=G, bl=B, ebp=G, eax=R。
    int32 colR = TheRules ? static_cast<int32>(TheRules->RadColor.R) : 0;
    int32 colG = TheRules ? static_cast<int32>(TheRules->RadColor.G) : 0;
    int32 colB = TheRules ? static_cast<int32>(TheRules->RadColor.B) : 0;

    // asm: `cmp [Scenario+Theatre], 1` / jnz —— 雪地场景 R/B 强制 -0x80。
    if (TheScenario && static_cast<int32>(TheScenario->Theater) == 1) {
        colB = -0x80;
        colR = -0x80;
    }

    // asm: 每个分量先 `c*1000/255`，之后再乘 RadTintFactor 取整。
    const int32 normR = ByteToScaled(colR);       // asm: tintR（R 分量）
    const int32 normG = ByteToScaled(colG);       // asm: var_2C
    const int32 normB = ByteToScaled(colB);       // asm: var_28

    const double radLevelFactor = TheRules ? TheRules->RadLevelFactor : 0.0;
    const double radTintFactor  = TheRules ? TheRules->RadTintFactor  : 0.0;

    // asm: intensity = floor(clamp(RadLevel * RadLevelFactor, <= 2000))。
    double intensityF = static_cast<double>(RadLevel) * radLevelFactor;
    if (intensityF > 2000.0)
        intensityF = 2000.0;
    const int32 intensity = FloorToInt(intensityF);
    CurrentIntensity = intensity;                 // asm: mov [esi+54h], ebx

    // asm: tintR/G/B = floor(norm * RadTintFactor)。
    int32 tintR = FloorToInt(static_cast<double>(normR) * radTintFactor);
    int32 tintG = FloorToInt(static_cast<double>(normG) * radTintFactor);
    int32 tintB = FloorToInt(static_cast<double>(normB) * radTintFactor);

    CurrentTint.R = tintR;                        // asm: mov [esi+58h], edx
    CurrentTint.G = tintG;                        // asm: mov [esi+5Ch], ebp
    CurrentTint.B = tintB;                        // asm: mov [esi+60h], edi

    // asm: CurrentLevelDecrement = StartRadLevel / RadApplicationDelay
    CurrentLevelDecrement = radApplicationDelay != 0
        ? (StartRadLevel / radApplicationDelay)
        : 0;

    // asm: CurrentLightDecrement = StartRadLevel / RadLightDelay；
    //      CurrentLightStage     = intensity / CurrentLightDecrement。
    CurrentLightDecrement = radLightDelay != 0
        ? (StartRadLevel / radLightDelay)
        : 0;
    CurrentLightStage = CurrentLightDecrement != 0
        ? (intensity / CurrentLightDecrement)
        : 0;

    // asm: 中心格的世界坐标。
    const CoordStruct center(static_cast<int32>(pos.X) * LeptonsPerCell,
                             static_cast<int32>(pos.Y) * LeptonsPerCell,
                             0);

    if (!LightSource) {
        // asm: new LightSourceClass(Visibility=SpreadInLeptons, Intensity,
        //        tintR, tintG, tintB, coords) 然后 Start(0)。
        LightSourceClass* pLight = new LightSourceClass();
        pLight->Initialize(center, SpreadInLeptons, intensity,
                           ColorStruct(static_cast<uint8>(tintR & 0xFF),
                                       static_cast<uint8>(tintG & 0xFF),
                                       static_cast<uint8>(tintB & 0xFF)));
        LightSource = pLight;

        // asm: Start(mode=0) —— 置使能位并刷新格灯光。
        pLight->SetActive(true);
        pLight->UpdateCellLighting();
    } else {
        // asm: LightSourceClass_ChangeLevels(this, intensity, tintR, tintG, tintB, 0)
        static_cast<LightSourceClass*>(LightSource)
            ->ChangeLevels(intensity, tintR, tintG, tintB, 0);
    }

    // asm: call RadSiteClass_IncreaseLevel
    IncreaseLevel();
}

// =============================================================================
// asm: RadSiteClass_Update
//
//   --CurrentRadLevel；
//   if (RadLevelStartTime != -1) {
//     elapsed = FrameNow() - RadLevelStartTime;
//     rem = RadLevelDelay - elapsed;   // elapsed < delay 时
//   }
//   if (rem == 0) { DecreaseLevel(); RadLevelStartTime=Frame; RadLevelDelay=
//        RulesData[RadApplicationDelay]; }
//   … 光源节拍同构：到点则按 CurrentLevelDecrement 比例刷新光源
//        ChangeLevels(intensity', tintR', tintG', tintB', 0)；
//   if (CurrentRadLevel <= 0) 自毁（vtable +0x20）。
// =============================================================================
void RadSiteClass::Update()
{
    // asm: mov edx,[esi+70h] ; dec edx ; mov [esi+70h],edx
    --CurrentRadLevel;

    // ── 辐射等级节拍 ────────────────────────────────────────────────────
    {
        int32 rem = RadLevelDelay;                // asm: eax = [esi+30h]
        if (RadLevelStartTime != -1) {
            const int32 elapsed = FrameNow() - RadLevelStartTime;
            if (elapsed < rem)
                rem -= elapsed;                   // asm: sub eax, ecx
            else
                rem = 0;                          // asm: jge loc_65B82D
        }

        if (rem == 0) {
            // asm: call RadSiteClass_DecreaseLevel
            DecreaseLevel();

            RadLevelStartTime = FrameNow();
            RadLevelDelay = TheRules ? TheRules->RadApplicationDelay : 0;
        }
    }

    // ── 光源节拍 ────────────────────────────────────────────────────────
    {
        int32 rem = RadLightDelay;                // asm: eax = [esi+3Ch]
        if (RadLightStartTime != -1) {
            const int32 elapsed = FrameNow() - RadLightStartTime;
            if (elapsed < rem)
                rem -= elapsed;
            else
                rem = 0;
        }

        if (rem == 0) {
            // asm: 按 CurrentRadLevel/StartRadLevel 比例刷新 tint 与 intensity，
            //   其中 intensity' = light->Intensity - CurrentLightStage。
            if (LightSource) {
                const int32 cur = CurrentRadLevel;
                const int32 denom = StartRadLevel;

                int32 tB = 0, tG = 0, tR = 0, inten = 0;
                if (denom != 0) {
                    tB = (CurrentTint.B * cur) / denom;   // asm: [+60h]
                    tG = (CurrentTint.G * cur) / denom;   // asm: [+5Ch]
                    tR = (CurrentTint.R * cur) / denom;   // asm: [+58h]
                }

                LightSourceClass* pLight = static_cast<LightSourceClass*>(LightSource);
                // asm: intensity = light->intensity([ecx+24h]) - CurrentLightStage
                inten = pLight->Intensity - CurrentLightStage;

                pLight->ChangeLevels(inten, tR, tG, tB, 0);
            }

            RadLightStartTime = FrameNow();
            RadLightDelay = TheRules ? TheRules->RadLightDelay : 0;
        }
    }

    // asm: if (CurrentRadLevel <= 0) call vtable[+0x20] （自毁）。
    //   本项目由驱动循环经 IsDead() 清出 Array（见 MainLoop Update_Combat）。
}

// =============================================================================
// asm: RadSiteClass_IncreaseLevel / RadSiteClass_DecreaseLevel / sub_65BB50
//
//   三段共用同一逐格扫描：以 pos 为中心，(Spread, Spread) 为半宽的正方形内
//   逐格遍历；对每格求它与中心格的原点距离 dist，
//     若 dist > SpreadInLeptons：剂量贡献 0；
//     否则 amount = (SpreadInLeptons - dist) / SpreadInLeptons * RadLevel；
//   Increase 分支把 amount 累加进格子（CellClass_RadLevel_Increase）；
//   Decrease 分支（含 sub_65BB50）额外 `fidiv [this+50h]`（CurrentLevelDecrement）
//   后从格子扣除（CellClass_RadLevel_Decrease）。
// =============================================================================
void RadSiteClass::ApplyToCells(int32 op)
{
    if (!MapClass::Instance)
        return;

    const int32 half = Spread;
    const int32 centerX = pos.X;
    const int32 centerY = pos.Y;

    const CoordStruct centerCoord = CellClass::Cell2Coord(centerX, centerY);

    // asm: 外层遍历 Y（var_44..），内层遍历 X（var_48..）；
    //   范围 [center-Spread, center+Spread]。
    for (int32 y = centerY - half; y <= centerY + half; ++y) {
        for (int32 x = centerX - half; x <= centerX + half; ++x) {
            const CoordStruct cellCoord = CellClass::Cell2Coord(x, y);
            const int32 dx = cellCoord.X - centerCoord.X;
            const int32 dy = cellCoord.Y - centerCoord.Y;
            const int32 dist = FloorToInt(std::sqrt(static_cast<double>(dx * dx + dy * dy)));

            // asm: amount = 0（默认），仅在 dist <= SpreadInLeptons 时覆盖。
            double amount = 0.0;
            if (dist <= SpreadInLeptons) {
                amount = static_cast<double>(SpreadInLeptons - dist)
                       / static_cast<double>(SpreadInLeptons)
                       * static_cast<double>(RadLevel);

                if (op < 0) {
                    // asm: fidiv [edx+50h] —— 除以 CurrentLevelDecrement。
                    if (CurrentLevelDecrement != 0)
                        amount /= static_cast<double>(CurrentLevelDecrement);
                    else
                        amount = 0.0;
                }
            }

            CellClass* pCell = MapClass::Instance->GetCellAt(x, y);
            if (!pCell)
                continue;

            if (op < 0)
                pCell->RadLevel_Decrease(amount);   // asm: CellClass_RadLevel_Decrease
            else
                pCell->RadLevel_Increase(amount);   // asm: CellClass_RadLevel_Increase
        }
    }
}

void RadSiteClass::IncreaseLevel()
{
    ApplyToCells(+1);
}

void RadSiteClass::DecreaseLevel()
{
    ApplyToCells(-1);
}

void RadSiteClass::DecreaseAppliedCells()
{
    // asm: sub_65BB50 —— 与 DecreaseLevel 同体（Add 内部先清扫既有覆盖格）。
    ApplyToCells(-1);
}
