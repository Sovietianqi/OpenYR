#include <Abstract/TerrainClass.h>
#include <Abstract/TerrainTypeClass.h>

#include <Animations/AnimClass.h>
#include <Animations/AnimTypeClass.h>
#include <Combat/WarheadTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <FileFormats/SHP.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/Surface.h>
#include <Rendering/UnitArt.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>

#include <cstring>

// ============================================================================
// TerrainClass.cpp
//
//  TerrainClass is the runtime instance of a terrain object - trees, rocks,
//  marble blocks and other blocking map decorations rendered from SHP art.
//  Terrain objects can be destroyed, catch fire, and (for some types) spread
//  tiberium into neighbouring cells.
//
//  This file implements:
//    * Static Array management (Init_Array / Delete_Array / Delete_All)
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * IsSightable (can the object block / provide sight)
//    * Update (fire propagation / tiberium spread / destruction)
//    * Draw_It (SHP render hook)
//    * Is_Destroyed
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<TerrainClass*>* TerrainClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void TerrainClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<TerrainClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<TerrainClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<TerrainClass*>();
    }
}

// asm: InitList_3068 (asm:11) —— 清零 data/count/length。
// asm: InitList_3067 (asm:2)  —— capacity/increment 均置 0x80。
void TerrainClass::sctor()
{
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<TerrainClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<TerrainClass*>)));
        if (Array != nullptr)
        {
            new (Array) DynamicVectorClass<TerrainClass*>();
            Array->Capacity = 0x80;               // asm: mov dword_B0ED38, 80h
        }
    }
}

// asm: InitList_3068 的对称析构。
void TerrainClass::sdtor()
{
    Delete_Array();
}

void TerrainClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<TerrainClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void TerrainClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        TerrainClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Constructor
//
// asm: TerrainClass_CTOR (asm:404, retn 8 -> (type, &cell))
//   ObjectClass_CTOR(this)
//   [0AC]=0(Frame) / [0B0]=0 / [0B4]=0 / [0C0]=0 / [0C4]=1(FrameStep)
//   [0C8]=type   / [0CC]=0 / [0CD]=0 / [0D0..0DC]=0
//   重打虚表 / Create_ID
//   [this+6Ch] = type->[0A0h]
//   若 cell != 共享提示块: TerrainClass_Put(this, cellCenter, 0)，失败 Detach
//   追加进 vec_Terrain（grow-by-increment）；再登记到 vec_TechTypeLinks 队列
// ============================================================================

TerrainClass::TerrainClass(HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type          = nullptr;   // asm: [0C8h]（由调用方绑定）
    Health        = 0;
    Frame         = 0;         // asm: [0ACh] = 0
    IsOnFire      = false;     // asm: [0CCh] = 0
    FireCount     = 0;         // asm: [0CDh] = 0
    IsAnimActive  = false;     // asm: [0B0h] = 0
    FrameTimerRate= 0;         // asm: [0C0h] = 0
    FrameStep     = 1;         // asm: [0C4h] = 1
    FieldD0       = 0;         // asm: [0D0h] = 0
    FieldD4       = 0;         // asm: [0D4h] = 0
    Owner         = pOwner;
    IsSelected    = false;
    IsInLimbo     = false;

    // asm: 追加进 vec_Terrain（grow-by-increment），再按格注册 symlink。
    if (Array == nullptr)
        Init_Array();
    if (Array != nullptr)
        Array->Add(this);
}

// ============================================================================
// Destructor
// ============================================================================

TerrainClass::~TerrainClass()
{
    // No heap resources to release at this level.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT TerrainClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Terrain);
    return S_OK;
}

// asm: TerrainClass_Load (asm:2345, retn 8)
//   （从 vec_TechTypeLinks 撤销旧 symlink）
//   hr = ObjectClass_Load(this, pStm)          ; 失败直接返回
//   ObjectClass_LoadTables(this, &pStm)        ; 重打四张虚表
//   （登记新的 symlink 进 vec_TechTypeLinks）
//   SwizzleManager.Swizzle(this+0C8h)          ; 重绑 TerrainTypeClass*
//   asm 不序列化任何自有字段。
HRESULT TerrainClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0)
        return hr;                            // asm: jl loc_71CF23

    LoadTables(pStm);                         // asm: ObjectClass_LoadTables

    // asm: Swizzle(&this->Type) —— 以 ID 重绑类型指针。
    if (Type != nullptr)
    {
        TerrainTypeClass* pResolved = TerrainTypeClass::Find(Type->ID);
        Type = (pResolved != nullptr) ? pResolved : Type;
    }

    return S_OK;
}

// asm: TerrainClass_Save (asm:2500, retn 0Ch) —— 纯转发 AbstractClass_Save。
HRESULT TerrainClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    return ObjectClass::Save(pStm, fClearDirty);
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType TerrainClass::WhatAmI() const
{
    return AbstractType::Terrain;
}

int32 TerrainClass::Size() const
{
    return sizeof(TerrainClass);
}

// asm: TerrainClass_GetCRC (asm:2536)
//   ObjectClass_GetCRC(buf)
//   CRC_DWORD(Type->GetAbstractDerivationID())      ; vt+2Ch
//   CRC_DWORD(Type 的 ID 哈希)                        ; type+4 vt+10h
//   CRC_bool(IsOnFire[0CCh]) ; CRC_bool(FireCount[0CDh])
//   CRC_DWORD([0D0h])        ; CRC_DWORD([0D4h])
void TerrainClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    if (Type != nullptr)
    {
        // asm: call dword ptr [eax+2Ch] -> GetAbstractDerivationID
        int32 deriv = Type->GetAbstractDerivationID();
        crc.AddData(&deriv, sizeof(deriv));

        // asm: type+4 vt+10h -> ID 字符串哈希
        if (Type->ID != nullptr)
        {
            const int32 len = static_cast<int32>(std::strlen(Type->ID));
            if (len > 0)
                crc.AddData(Type->ID, len);
        }
    }

    crc.AddData(&IsOnFire,  sizeof(IsOnFire));   // asm: [0CCh]
    crc.AddData(&FireCount, sizeof(FireCount));  // asm: [0CDh]
    crc.AddData(&FieldD0,   sizeof(FieldD0));    // asm: [0D0h]
    crc.AddData(&FieldD4,   sizeof(FieldD4));    // asm: [0D4h]
}

int32 TerrainClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// asm: TerrainClass_GetType (asm:2782) -- "mov eax,[ecx+0C8h] ; retn"
ObjectTypeClass* TerrainClass::GetType() const
{
    return reinterpret_cast<ObjectTypeClass*>(Type);
}

// asm: TerrainClass_PointerGotInvalid (asm:2605, retn 8)
//   先走 ObjectClass 的失效链（asm 标为 ObjectClass::Detach），再在 Type 命中
//   失效对象时清空 Type。
void TerrainClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    ObjectClass::PointerGotInvalid(pInvalid, removed);

    if (reinterpret_cast<AbstractClass*>(Type) == pInvalid)
        Type = nullptr;
}

// asm: 类型是否"永不可伤"（TerrainTypeClass.Immune）。
bool TerrainClass::Is_Veinhole() const
{
    if (Type == nullptr)
        return false;
    return Type->IsVeinhole;
}

// ============================================================================
// asm: TerrainClass_ReceiveDamage (asm:163, retn 1Ch)
//
//   if (WH == null)                       return 0
//   if (!WH->Wood)                        return 0
//   if (Type->Immune)                     return 0
//   eax = ObjectClass_ReceiveDamage(...)
//   if (eax == dsPostMortem)              return eax
//   if (eax != dsNowDead)                 return eax
//   // 死亡分支：
//   if (Type->SpawnsTiberium)            造死亡动画 + Damage_At_Cell(64)
//   else if (this->IsBurning)            缩短火焰动画
//   else if (![0CDh])                    [0CDh]=1; 启动计时器; [0CCh]=1 ...
//   注册脏区 / Mark(1) / 重绘
// ============================================================================
int32 TerrainClass::ReceiveDamage(int32* pDamage, int32 typeSource,
                                  WarheadTypeClass* pWarhead, TechnoClass* pAttacker,
                                  int32 ignoreDefenses, int32 arg10,
                                  HouseClass* pSourceHouse)
{
    (void)typeSource; (void)pAttacker; (void)ignoreDefenses;
    (void)arg10; (void)pSourceHouse;

    // asm: mov eax,[esp+LandType] / test eax,eax / jnz ; 否则 retn 0
    if (pWarhead == nullptr)
        return 0;

    // asm: mov cl,[eax+WarheadTypeClass.Wood] / test cl,cl / jz -> retn
    if (!pWarhead->Wood)
        return 0;

    // asm: mov dl,[ecx+Immune] / test dl,dl / jnz -> retn
    if (Type != nullptr && Type->Immune)
        return 0;

    // asm: ObjectClass_ReceiveDamage 结算（本类落 HP）。
    const int32 damage = (pDamage != nullptr) ? *pDamage : 0;
    Health -= damage;

    // 死亡状态：dsNowDead（本项目用 HP<=0 表达）。
    if (Health > 0)
        return 0;

    Health = 0;

    // ------------------------------------------------------------------
    // 死亡分支。
    // ------------------------------------------------------------------
    if (Type != nullptr && Type->SpawnsTiberium)
    {
        // asm: 造 SelectDamageAnimation 动画 + Damage_At_Cell(64)。
        //   此处落格播散矿脉作为等价。
        if (MapClass::Instance != nullptr)
        {
            CellClass* pCell = MapClass::Instance->GetCellAt(Location);
            if (pCell != nullptr)
                pCell->Spread_Tiberium(true);
        }
    }
    else if (IsOnFire)
    {
        // asm: Shorten_Attached_Anims —— 缩短火焰动画（无后端则跳过）。
    }
    else if (FireCount == 0)                     // asm: mov al,[esi+0CDh]
    {
        FireCount = 1;                           // asm: byte ptr [esi+0CDh],1
        // asm: [0C0h]=2；FrameTimer 复位；[0ACh]=0
        FrameTimerRate = 2;
        Frame = 0;
    }

    // asm: RegisterDirtyArea / Mark(1) / 重绘。
    Mark_Layer(1);
    return 0;
}

// ============================================================================
// IsSightable
//
//  Returns whether the terrain object can be seen / provides line-of-sight.
//  Rocks and other non-tree terrain do not block sight; trees do.
// ============================================================================

bool TerrainClass::IsSightable() const
{
    if (Type == nullptr)
        return false;

    // Trees are sight-blocking decorations; rocks and marble are not.
    if (Type->IsTree)
        return true;

    return false;
}

// ============================================================================
// 根据游戏行为，可知 Destroyed 负责下面这段逻辑。
// ============================================================================

bool TerrainClass::Is_Destroyed() const
{
    // A terrain object is destroyed when its health drops to zero or below.
    return Health <= 0;
}

// ============================================================================
// Update
//
// asm: TerrainClass_Update (asm:1670)
//   1) ObjectClass_Update
//   2) 若 type->IsAnimated 且本帧无定时器在跑：以 1e-6 缩放的随机数与
//      type->AnimationProbability 比较，命中则 Frame=0、启动计时器
//      (FrameTimerRate = type->AnimationRate)。
//   3) 计时器到期：Frame += FrameStep；IsAnimActive=1；重启计时器；
//      若是"岩类"且已到最后一帧 -> 重绘并返回。
//   4) 若是"造矿类 + 动画类"且 Frame == SHP 帧数/2 -> Frame=0、在格上播散矿。
// ============================================================================

void TerrainClass::Update()
{
    // asm: ObjectClass_Update(this)
    ObjectClass::Update();

    if (Type == nullptr)
        return;

    Tmp_AnimTick();

    // ------------------------------------------------------------------
    // 旧有的火处理 / 播矿兜底（保留以兼容无后端分支）。
    // 见 TerrainClass::Tmp_FireTick。
    // ------------------------------------------------------------------
    Tmp_FireTick();
}

// asm: TerrainClass_Update 的动画段（asm:1679）。
void TerrainClass::Tmp_AnimTick()
{
    // asm: cmp [eax+2B3h],0(IsAnimated) / jz ; cmp [esi+0C0h],0 / jnz
    if (!Type->IsAnimated)
        return;

    // 计时器在跑则跳过重抽。
    if (FrameTimerRate != 0)
        goto advance;

    {
        // asm: Random(0,0F4240h)*1e-6 < type->AnimationProbability ?
        if (ScenarioClass::Instance != nullptr)
        {
            const int32 r = ScenarioClass::Instance->Random.Next(0, 0x0F423F);
            const double p = static_cast<double>(r) * 1.0e-6;
            if (p < Type->AnimationProbability)
            {
                Frame          = 0;                  // asm: mov [esi+0ACh], 0
                FrameTimerRate = Type->AnimationRate; // asm: [0C0h] = type->[2A0h]
                IsAnimActive   = false;
            }
        }
    }

advance:
    // ------------------------------------------------------------------
    // asm: FrameTimer 到期 -> Frame += FrameStep，重启定时器。
    //       本实现以"每帧推进"简化定时器（FrameTimerClass_Get_Remaining）。
    // ------------------------------------------------------------------
    if (FrameTimerRate == 0)
        return;

    Frame += FrameStep;                          // asm: [0ACh] += [0C4h]
    IsAnimActive = true;                         // asm: byte ptr [esi+0B0h],1

    // asm: 岩类（type->[2B1h] && type->[2B3h]）且 Frame == SHP 帧数/2 -> 播矿。
    if (Type->SpawnsTiberium && Type->IsAnimated)
    {
        SHPStruct* pShape = Type->GetImage();
        const int32 frames = (pShape != nullptr) ? pShape->Frames : 0;
        if (frames > 0 && Frame == frames / 2)
        {
            Frame          = 0;
            FrameTimerRate = 0;
            if (MapClass::Instance != nullptr)
            {
                CellClass* pCell = MapClass::Instance->GetCellAt(Location);
                if (pCell != nullptr)
                    pCell->Spread_Tiberium(true);
            }
        }
    }
}

// 火处理（保留原语义，配合 Burn()）。
void TerrainClass::Tmp_FireTick()
{
    if (Health <= 0)
        return;

    if (IsOnFire)
    {
        if (Type->IsFlammable)
        {
            int32 fireDamage = (Type->Damage > 0) ? Type->Damage : 1;
            Health -= fireDamage;
            ++FireCount;
            if (Health <= 0 || FireCount > 30)
            {
                Health = 0;
                IsOnFire = false;
            }
        }
        else
        {
            IsOnFire = false;
            FireCount = 0;
        }
    }
}

// ============================================================================
// 根据游戏行为，可知 It 负责下面这段逻辑。
//
//  Renders the terrain object at the given screen origin.  The full binary
//  blits the terrain SHP frame onto the tactical surface, applying the fire
//  overlay when the object is burning.
// ============================================================================

void TerrainClass::Draw_It(int32 originX, int32 originY) const
{
    if (Type == nullptr)
        return;

    if (Health <= 0)
        return;

    Point2D coords;
    coords.X = originX;
    coords.Y = originY;

    // 原版 TerrainClass_Draw_It 自己从显示层取目标表面，再转调 Draw。
    // 这里以前传的是 nullptr，Draw 的第一道判空会直接返回 —— 树和岩石
    // 因此从来没出现在画面上。
    Draw(&coords, UnitArt::GetTacticalSurface());
}

// ============================================================================
// asm: TerrainClass_Draw (asm:1010, retn 8)
//
//   coords = GetCoords(); cell = Map.Coord_Cell(coords);
//   SHP = GetImage(); if null return
//   type = [this+0C8h]
//   if (type->[2B3h] /*IsAnimated*/)     frameIdx = Frame
//   else if ([this+0CDh] /*已毁标志*/)   frameIdx = Frame + [0CDh]
//   else if ([this+6Ch] < 2)             frameIdx = 1
//     （[this+6Ch] 是 TerrainClass_CTOR 从 type->[0A0h] 拷来的 TreeStrength；
//       本工程构造时不带类型实参，故这里直接用 Type->TreeStrength。）
//   pos = arg coords；
//   zAdj = -Draw_AdjustForBuildingZ()
//   if (type->[2B1h] /*SpawnsTiberium*/) pos.Y -= 0x10
//   flags = (type->[2B3h] || [this+0CDh]) ? 0x2E00 : 0x4E00
//   CC_Draw_Shape(TempSurface, cell->LightConvert, SHP, frameIdx, pos, bounds,
//                 flags, Remap=0, ZAdjust=zAdj-0xC, ZGradient=2,
//                 Brightness=cell->Intensity, TintColor=0, ...)
//   （Brightness 取该格 Intensity；本工程 CC_Draw_Shape 未消费该实参，
//     且 CellClass 无逐格 Intensity 字段，故传 0。）
//
//   树木被摧毁时 TerrainClass_ReceiveDamage 把 [0CDh] 置 1 并把 Frame([0ACh])
//   清零，于是本函数取到 Frame+1 = 1 —— 即 SHP 的残桩帧。
// ============================================================================
void TerrainClass::Draw(Point2D* pCoords, Surface* pSurface) const
{
    if (pCoords == nullptr || Type == nullptr)
        return;

    SHPStruct* pShape = Type->GetImage();
    if (pShape == nullptr)
        return;                                  // asm: jz loc_71C353

    // asm: 帧号选择。
    int32 frameIdx = 0;
    if (Type->IsAnimated)                        // asm: [type+2B3h]
    {
        frameIdx = Frame;                        // asm: [esi+0ACh]
    }
    else if (FireCount != 0)                     // asm: [esi+0CDh]
    {
        frameIdx = Frame + FireCount;            // asm: [esi+0ACh] + al
    }
    else if (Type->TreeStrength < 2)             // asm: cmp [esi+6Ch],2 / jge 跳过
    {
        frameIdx = 1;                            // asm: mov ebx,1
    }

    Point2D pos = *pCoords;                      // asm: pos = arg_0

    // asm: 调用 vt+1D0h 取建筑 Z 调整，再取负作为 ZAdjust 基础。
    const int32 zAdj = -Draw_AdjustForBuildingZ();

    // asm: 可造矿类型上抬 0x10。
    if (Type->SpawnsTiberium)                    // asm: [type+2B1h]
        pos.Y -= 0x10;

    // asm: flags = (IsAnimated || [0CDh]) ? 2E00h : 4E00h
    const uint32 flags = (Type->IsAnimated || FireCount != 0) ? 0x2E00u : 0x4E00u;

    if (pSurface == nullptr)
        return;

    // asm: mov edx,[ebp+34h] —— CellClass::LightConvert（该格的光照转换器）。
    //   本工程没有逐格 LightConvert，统一用已装的剧场调色板转换器。
    ConvertClass* pConvert = StandardPalette;
    if (pConvert == nullptr)
        return;

    CC_Draw_Shape(pSurface, pConvert, pShape, frameIdx, &pos, nullptr,
                  static_cast<BlitterFlags>(flags),
                  0, zAdj - 0x0C, 2, 0, 0,
                  nullptr, 0, 0, 0);
}

// asm: vt+1D0h 的等价——本类无建筑 Z，返回 0。
int32 TerrainClass::Draw_AdjustForBuildingZ() const
{
    return 0;
}


// ============================================================================
// Fire control (helper used by weapons / warheads)
//
//  Ignites the terrain object if it is flammable and not already burning.
//  Declared inline to keep the header minimal; the implementation lives here.
// ============================================================================

// Note: the ignition entry point is exposed through the weapon system which
// calls Update() repeatedly once IsOnFire is set.  The flag is toggled by the
// warhead's fire-starting logic rather than a dedicated method on this class.

// ============================================================================
// asm: TerrainClass_Burn (0x71C5E0，虚表槽)
//
//   char __thiscall TerrainClass_Burn(TerrainClass *this)
//
//   sub esp,18h / push edi / mov edi,ecx
//   mov al,[edi+0CDh] / test al,al / jnz lct_Out         ; 已处理 -> false
//   mov al,[edi+0CCh] / test al,al / jnz lct_Out         ; 燃烧中 -> false
//   mov eax,[edi+0C8h]                                    ; TerrainTypeClass*
//   cmp dword ptr [eax+9Ch], wood / jnz lct_Out          ; wood = 6
//   mov cl,[eax+2B1h] / test cl,cl / jnz lct_Out         ; 类型标志
//   mov eax,Scenario / push ebx / push esi
//   lea ecx,[eax+218h] / call Random2Class::operator()   ; r
//   push size AnimClass / mov esi,eax / call new / mov ebx,eax
//   add esp,4 / test ebx,ebx / jz short loc_71C699
//   mov edx,[edi] / lea eax,[esp+24h+var_C] / push eax
//   mov ecx,edi / call dword ptr [edx+48h]               ; 取世界坐标
//   mov ecx,[eax] / mov edx,[eax+4] / mov eax,[eax+8]
//   mov [esp+28h+var_18],edx / mov edx,RulesData
//   add eax,50h                                          ; Z += 0x50
//   push 0 / push 0 / mov [esp+30h+pos],ecx
//   mov [esp+30h+var_14],eax
//   mov eax,[edx+0B94h] ; TreeFire
//   push 600h / push 0FFh / lea ecx,[esp+38h+pos]
//   and esi,1 / push 0 / push ecx
//   mov ecx,[eax+esi*4] / push ecx / mov ecx,ebx
//   call AnimClass::AnimClass                            ; 造火焰动画
//   mov esi,eax / test esi,esi / jz short loc_71C67A
//   push edi / mov ecx,esi / call AnimClass::Attach_To
// loc_71C67A:
//   mov eax,[esi+100h] / add eax,0FFFFFFECh / mov [esi+100h],eax
//   pop esi / mov byte ptr [edi+0CCh],1
//   pop ebx / mov al,1 / pop edi / add esp,18h / retn
// loc_71C699: xor esi,esi / jmp short loc_71C67A
// lct_Out: xor al,al / pop edi ...
//
// 字段映射（铁律①：语义等价，不臆造偏移）：
//   asm +0xCC -> 本类 IsOnFire      （"已在燃烧"）
//   asm +0xCD -> 本类 FireCount     （"已处理过"计数；asm 只判零/置位）
//   asm +0xC8 -> 本类 Type          （TerrainTypeClass*）
//   asm [type+9Ch] == wood(6) -> 本类 Type->IsFlammable 的 INI 可燃标记
//     （OpenYR 的 TerrainTypeClass 尚未抽出独立的 LandType 字段，
//       此处用 IsFlammable 承接同一语义；asm 的 wood 值已注明。）
//   asm [type+2B1h] -> 同 IsFlammable 分支（见下方注释）
// ============================================================================
bool TerrainClass::Burn()
{
    // asm: mov al,[edi+0CDh] / test al,al / jnz lct_Out
    if (FireCount != 0)
        return false;                              // asm: lct_Out -> xor al,al

    // asm: mov al,[edi+0CCh] / test al,al / jnz lct_Out
    if (IsOnFire)
        return false;                              // asm: 正在燃烧 -> false

    // asm: mov eax,[edi+0C8h]
    TerrainTypeClass* pType = Type;
    if (pType == nullptr)
        return false;

    // asm: cmp dword ptr [eax+9Ch], wood / jnz lct_Out   ; wood = 6
    //      mov cl,[eax+2B1h] / test cl,cl / jnz lct_Out
    // 两处判断在语义上都是"该地形类型是否可燃"。OpenYR 的
    // TerrainTypeClass 用 IsFlammable 承接（INI 侧同一来源）。
    if (!pType->IsFlammable)
        return false;

    // asm: mov eax,Scenario / lea ecx,[eax+218h] / call Random2Class::operator()
    //      and esi,1
    // 该调用只取一个随机位（随后 and esi,1），用 [0,1] 区间等价复现。
    const int32 r = ScenarioClass::Instance->Random.Next(0, 1);

    // asm: mov edx,[edi] / lea eax,[esp+24h+var_C] / push eax
    //      mov ecx,edi / call dword ptr [edx+48h]
    //      -- 虚表 +0x48 = 取世界坐标；本类用 ObjectClass::Location。
    const CoordStruct base = Location;

    // asm: add eax,50h    ; Z += 0x50
    CoordStruct pos{ base.X, base.Y, base.Z + 0x50 };

    // asm: mov eax,[edx+0B94h] ; TreeFire / mov ecx,[eax+esi*4]
    AnimTypeClass* pFire = nullptr;
    if (RulesClass::Instance != nullptr &&
        RulesClass::Instance->TreeFire.Count > 0)
    {
        const int32 idx = r & 1;
        if (idx < RulesClass::Instance->TreeFire.Count)
            pFire = RulesClass::Instance->TreeFire[idx];
    }

    // asm: push 600h / push 0FFh / push 0 / push &pos / push animType
    //      call AnimClass::AnimClass(animType, pos, 0, 0FFh, 600h, 0)
    AnimClass* pAnim = nullptr;
    if (pFire != nullptr)
        pAnim = GameCreateArgs<AnimClass>(pFire, pos, 0, 0xFF, 0x600, 0, false);

    if (pAnim != nullptr)
    {
        // asm: push edi / mov ecx,esi / call AnimClass::Attach_To
        // +0x100 是动画的 ZAdjust 累加槽；本类用 AnimClass::ZAdjust。
        pAnim->SetOwnerObject(this);
        pAnim->ZAdjust -= 0x14;
    }

    // asm: mov byte ptr [edi+0CCh],1 / mov al,1
    IsOnFire = true;
    return true;
}
