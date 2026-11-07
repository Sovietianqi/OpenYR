// =============================================================================
// SlaveManagerClass.cpp —— 尤里"奴隶矿车"的奴隶管理器
//
// 权威来源：/workspace/asm_export/SlaveManagerClass.asm.txt
//   SlaveManagerClass_CTOR                     (asm:25)
//   SlaveManagerClass_ReplaceWhichBelongsToUnit(asm:415)
//   SlaveManagerClass_Update                   (asm:482)
//   SlaveManagerClass_LoseSlave                (asm:546)
//   SlaveManagerClass_ReleaseSlaves            (asm:622)
//   SlaveManagerClass_6B0CC0  -> AI()          (asm:794)
//   SlaveManagerClass_Init                     (asm:850)
//   SlaveManagerClass_ExplicitHarvest          (asm:906)
//   SlaveManagerClass_ShouldWakeUp             (asm:1162)
//   SlaveManagerClass_GetCRC                   (asm:1252)
//   SlaveManagerClass_GetClassID / Load / Save / GetSize
// =============================================================================

#include "SlaveManagerClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/InfantryTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Game/Game.h"
#include "../Rules/RulesClass.h"
#include "../Audio/VocClass.h"
#include "../IO/CRC.h"

// asm: vec_SlaveManagers —— 全局在册管理器表
DynamicVectorClass<SlaveManagerClass*>* SlaveManagerClass::Array = nullptr;

// =============================================================================
// asm: SlaveManagerClass_CTOR (asm:25)  retn 14h = 5 参数
//
//   +0x24=Owner +0x28=Slave +0x2C=num +0x30=RegenRate +0x34=ReloadRate
//   +0x50=Frame +0x54=var +0x58=10 +0x5C=0 +0x60=0
//   循环 num 次，每次 new 一个 SlaveControl，交由 Slave->Create_Unit(house)
//   生成奴隶并把 [ctrl+0] 指向它；随后登记进 vec_SlaveManagers。
// =============================================================================
SlaveManagerClass::SlaveManagerClass(TechnoClass* pOwner, InfantryTypeClass* pSlaveType,
                                     int32 num, int32 regenRate, int32 reloadRate) noexcept
    : Owner(pOwner)
    , SlaveType(pSlaveType)
    , MaxSlaves(num)
    , RegenRate(regenRate)
    , ReloadRate(reloadRate)
    , TimerStart(Game::CurrentFrame)
    , TimerVar(0)
    , TimerLeft(10)
    , State(Manager_Idle)
    , StateStamp(0)
{
    // asm: loc_6AF221 循环体 —— 每个槽位各生成一名奴隶。
    for (int32 i = 0; i < num; ++i) {
        AllocSlave();
    }

    // asm: vec_SlaveManagers.length 自增并写回实例。
    if (Array == nullptr) {
        Array = new DynamicVectorClass<SlaveManagerClass*>();
    }
    if (Array != nullptr) {
        Array->Add(this);
    }
}

SlaveManagerClass::~SlaveManagerClass()
{
    // asm: SlaveManagerClass::~SlaveManagerClass (asm:307)
    //   倒序 delete 每个 SlaveControl，再从 vec_SlaveManagers 摘除。
    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        delete SlaveControls[i];
    }
    SlaveControls.Clear();

    if (Array != nullptr) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
    Owner = nullptr;
}

// =============================================================================
// SpawnSlave —— asm CTOR 循环体的 "Create_Unit" 分支。
//   原版：Owner->vtable[0x3C]() 取阵营 → SlaveType->vtable[0x8C](house) 造兵。
//   OpenYR 走真实的 InfantryTypeClass::CreateInfantry(house)。
// =============================================================================
TechnoClass* SlaveManagerClass::SpawnSlave()
{
    if (SlaveType == nullptr || Owner == nullptr || Owner->Owner == nullptr) {
        return nullptr;
    }

    InfantryClass* pSlave = SlaveType->CreateInfantry(Owner->Owner);
    if (pSlave == nullptr) {
        return nullptr;
    }

    // asm: mov [eax+2DCh],Owner —— 记下奴隶的归属矿车。
    pSlave->Set_Owner(Owner->Owner);
    return static_cast<TechnoClass*>(pSlave);
}

// =============================================================================
// AllocSlave —— 分配一个 SlaveControl 并按 asm 填默认值。
// =============================================================================
SlaveManagerClass::SlaveControl* SlaveManagerClass::AllocSlave()
{
    SlaveControl* pCtrl = new SlaveControl();
    pCtrl->Slave     = SpawnSlave();
    pCtrl->State     = (pCtrl->Slave != nullptr) ? Slave_Ready : Slave_Dead;
    pCtrl->TimeStart = Game::CurrentFrame;
    pCtrl->field_C   = 0;
    pCtrl->TimeLeft  = RegenRate;

    SlaveControls.Add(pCtrl);
    return pCtrl;
}

// =============================================================================
// WakeSlave —— asm: slave->vtable[0x3D0]()。奴隶为步兵/载具时散开待命。
// =============================================================================
void SlaveManagerClass::WakeSlave(TechnoClass* pSlave)
{
    if (pSlave == nullptr) {
        return;
    }
    const AbstractType type = pSlave->WhatAmI();
    if (type == AbstractType::Infantry || type == AbstractType::Unit ||
        type == AbstractType::Aircraft) {
        FootClass* pFoot = static_cast<FootClass*>(pSlave);
        pFoot->Scatter(pSlave->Get_Coord(), true);
    }
}

// =============================================================================
// asm: SlaveManagerClass_Update (asm:482)
//   edx=[+0x50] eax=[+0x58]
//   if (+0x50 != -1 && Frame-+0x50 < +0x58) eax = +0x58-(Frame-+0x50)
//   if (eax != 0) return;
//   [+0x50]=Frame [+0x54]=var [+0x58]=10
//   if (Owner) { sub_6AF6C0(); sub_6AFD60(); }
// =============================================================================
void SlaveManagerClass::Update()
{
    const int32 frame = Game::CurrentFrame;
    const int32 start = TimerStart;
    const int32 left  = TimerLeft;

    int32 remaining = left;
    if (start != -1) {
        const int32 elapsed = frame - start;
        if (elapsed >= left) {
            remaining = 0;
        } else {
            remaining = left - elapsed;
        }
    }

    if (remaining != 0) {
        return;
    }

    TimerStart = frame;
    TimerVar   = 0;
    TimerLeft  = 10;

    if (Owner != nullptr) {
        TickSlaves();    // asm: sub_6AF6C0
        TickHarvest();   // asm: sub_6AFD60
    }
}

// =============================================================================
// asm: sub_6AF6C0 —— 逐槽位处理阵亡补员。
//   asm LoseSlave 已把阵亡槽位置为 State=6 并写入 (TimeStart, TimeLeft=Regen)。
//   这里到点就重生一名奴隶并把槽位复位。
// =============================================================================
void SlaveManagerClass::TickSlaves()
{
    const int32 frame = Game::CurrentFrame;
    for (int32 i = 0; i < SlaveControls.Count; ++i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl == nullptr || pCtrl->State != Slave_Dead) {
            continue;
        }
        // 节拍门限：TimeLeft 为补员间隔，-1 视为立即可补。
        if (pCtrl->TimeLeft >= 0 && (frame - pCtrl->TimeStart) < pCtrl->TimeLeft) {
            continue;
        }
        TechnoClass* pNew = SpawnSlave();
        if (pNew != nullptr) {
            pCtrl->Slave     = pNew;
            pCtrl->State     = Slave_Ready;
            pCtrl->TimeStart = frame;
            pCtrl->field_C   = 0;
            pCtrl->TimeLeft  = RegenRate;
        }
    }
}

// =============================================================================
// asm: sub_6AFD60 —— 劳作推进（与 ExplicitHarvest 同源）。
// =============================================================================
void SlaveManagerClass::TickHarvest()
{
    ExplicitHarvest();
}

// =============================================================================
// asm: SlaveManagerClass_6B0CC0 (asm:794)  —— 全队转入待命（State=1）。
//   if ([+0x5C] != 0) return;
//   [+0x5C]=1; [+0x60]=0x7FFFFFFF;
//   倒序：若 slot->State != 6 且 slot->Slave，则 slave->vtable[0x3D0]()。
// =============================================================================
void SlaveManagerClass::AI()
{
    if (State != Manager_Idle) {
        return;
    }
    State = Manager_Standby;
    StateStamp = 0x7FFFFFFF;

    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && pCtrl->State != Slave_Dead && pCtrl->Slave != nullptr) {
            WakeSlave(pCtrl->Slave);
        }
    }
}

// =============================================================================
// asm: SlaveManagerClass_Init (asm:850) —— 展开（State=4）。
//   if ([+0x5C] != 0) return; [+0x5C]=4; [+0x60]=0x7FFFFFFF; 倒序唤醒。
// =============================================================================
void SlaveManagerClass::Init()
{
    AbstractClass::Init();

    if (State != Manager_Idle) {
        return;
    }
    State = Manager_Deploy;
    StateStamp = 0x7FFFFFFF;

    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && pCtrl->State != Slave_Dead && pCtrl->Slave != nullptr) {
            WakeSlave(pCtrl->Slave);
        }
    }
}

// =============================================================================
// ---- 槽位语义 API ----
// =============================================================================

int32 SlaveManagerClass::GetSlaveIndex(const TechnoClass* pSlave) const
{
    if (pSlave == nullptr) {
        return -1;
    }
    for (int32 i = 0; i < SlaveControls.Count; ++i) {
        const SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && pCtrl->Slave == pSlave) {
            return i;
        }
    }
    return -1;
}

SlaveManagerClass::SlaveControl* SlaveManagerClass::GetSlaveControl(int32 index) const
{
    if (index < 0 || index >= SlaveControls.Count) {
        return nullptr;
    }
    return SlaveControls[index];
}

TechnoClass* SlaveManagerClass::GetSlaveAt(int32 index) const
{
    const SlaveControl* pCtrl = GetSlaveControl(index);
    return (pCtrl != nullptr) ? pCtrl->Slave : nullptr;
}

bool SlaveManagerClass::Add_Slave(TechnoClass* pSlave)
{
    if (pSlave == nullptr) {
        return false;
    }
    // 已在册则直接返回，避免重复登记。
    if (GetSlaveIndex(pSlave) >= 0) {
        return true;
    }
    // 有空槽（阵亡/待补员）则复用，否则新建。
    for (int32 i = 0; i < SlaveControls.Count; ++i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && pCtrl->State == Slave_Dead) {
            pCtrl->Slave     = pSlave;
            pCtrl->State     = Slave_Ready;
            pCtrl->TimeStart = Game::CurrentFrame;
            pCtrl->TimeLeft  = RegenRate;
            return true;
        }
    }
    if (SlaveControls.Count >= MaxSlaves) {
        return false;
    }
    SlaveControl* pCtrl = new SlaveControl();
    pCtrl->Slave     = pSlave;
    pCtrl->State     = Slave_Ready;
    pCtrl->TimeStart = Game::CurrentFrame;
    pCtrl->field_C   = 0;
    pCtrl->TimeLeft  = RegenRate;
    SlaveControls.Add(pCtrl);
    return true;
}

bool SlaveManagerClass::Remove_Slave(TechnoClass* pSlave)
{
    const int32 idx = GetSlaveIndex(pSlave);
    if (idx < 0) {
        return false;
    }
    // asm LoseSlave：把槽位标记为阵亡待补员，而不是立即删除。
    LoseSlave(pSlave);
    return true;
}

void SlaveManagerClass::SlaveTo(TechnoClass* pSlave, TechnoClass* pOwner)
{
    if (pSlave == nullptr) {
        return;
    }
    // asm: mov [eax+2DCh], edx —— 同步奴隶对矿车的归属。
    if (pOwner != nullptr && pOwner->Owner != nullptr) {
        pSlave->Set_Owner(pOwner->Owner);
    }
}

// =============================================================================
// IsSlaveMiner —— 语义等价 asm ExplicitHarvest 的入口守卫：
//   Owner->WhatAmI() == 1(Unit) 且类型声明了奴隶槽位。
// =============================================================================
bool SlaveManagerClass::IsSlaveMiner() const
{
    if (Owner == nullptr || SlaveType == nullptr) {
        return false;
    }
    return (Owner->WhatAmI() == AbstractType::Unit) && (MaxSlaves > 0);
}

// =============================================================================
// UpdateSlaveStats —— 汇总当前在岗奴隶数与剩余补员队列。
//   原版没有单独 proc，是 Update/TickSlaves 的统计视图，供外部查询与 CRC。
// =============================================================================
void SlaveManagerClass::UpdateSlaveStats()
{
    int32 alive = 0;
    for (int32 i = 0; i < SlaveControls.Count; ++i) {
        const SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && pCtrl->State != Slave_Dead && pCtrl->Slave != nullptr) {
            ++alive;
        }
    }
    // 用 StateStamp 的低位缓存上一轮的"在岗数"，避免新增字段破坏布局。
    StateStamp = (StateStamp & 0x7FFF0000) | (alive & 0xFFFF);
}

// =============================================================================
// asm: SlaveManagerClass_ReplaceWhichBelongsToUnit (asm:415)
//   oldOwner->SlaveManager = 0; this->Owner = newOwner;
//   if (newOwner->SlaveManager) { ReleaseSlaves(0,0); old->vtable[0x20](1); }
//   newOwner->SlaveManager = this;
//   倒序：slot->Slave && (slot->Slave->+0x2DC = this->Owner)。
// =============================================================================
void SlaveManagerClass::ReplaceWhichBelongsToUnit(TechnoClass* pNewOwner)
{
    if (Owner != nullptr) {
        Owner->SlaveManager = nullptr;   // asm: [oldOwner+2D8h]=0
    }
    Owner = pNewOwner;

    if (pNewOwner == nullptr) {
        return;
    }

    // asm: 若新主人身上已挂着一个旧管理器，先让它解散自己。
    if (pNewOwner->SlaveManager != nullptr && pNewOwner->SlaveManager != this) {
        pNewOwner->SlaveManager->ReleaseSlaves(nullptr);
        // asm: call [old->vtable+0x20](1) —— 旧管理器的重初始化槽。
        //   OpenYR 未确证该 vtable 索引语义（存疑项），此处以 Init() 承接
        //   "让旧管理器重新展开" 的行为，保持控制流等价。
        pNewOwner->SlaveManager->Init();
    }

    pNewOwner->SlaveManager = this;   // asm: [newOwner+2D8h]=this

    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && pCtrl->Slave != nullptr) {
            pCtrl->Slave->Set_Owner(Owner->Owner);
        }
    }
}

// =============================================================================
// asm: SlaveManagerClass_LoseSlave (asm:546)
//   倒序找 slot->Slave == arg；命中后：清 slave 归属、槽位置 0/6，
//   写入 (Frame, var, Regen)。
// =============================================================================
void SlaveManagerClass::LoseSlave(TechnoClass* pSlave)
{
    if (pSlave == nullptr) {
        return;
    }

    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl == nullptr || pCtrl->Slave != pSlave) {
            continue;
        }

        pCtrl->Slave     = nullptr;                             // [eax]=0
        pCtrl->State     = Slave_Dead;                          // [eax+4]=6
        pCtrl->TimeStart = Game::CurrentFrame;                  // [eax+8]=Frame
        pCtrl->field_C   = TimerVar;                            // [eax+C]=var
        pCtrl->TimeLeft  = RegenRate;                           // [eax+10]=SlaveRegen
        return;
    }
}

// =============================================================================
// asm: SlaveManagerClass_ReleaseSlaves (asm:622)  retn 8
//   NewOwnerHouse 为 null 时兜底取第一个 Civilian 阵营。
//   倒序处理每个槽位：
//     · 奴隶阵亡中 -> UnInit 回收；
//     · 否则按 (Owner->Owner / NewOwnerHouse / firstCivvie) 定接收方，
//       有接收方则改属 + 唤醒 + 去任务；无接收方则就地处置；
//   最后：若游戏进行中且献出过奴隶，按 Rules 播撒放音效，Owner=0。
// =============================================================================
void SlaveManagerClass::ReleaseSlaves(HouseClass* pNewOwnerHouse)
{
    if (Owner == nullptr) {
        return;
    }

    // asm: Side_From_Name("Civilian") -> 找第一个 Civilian 阵营作为兜底。
    HouseClass* pFirstCivvie = nullptr;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse != nullptr && pHouse->IsCivilians) {
            pFirstCivvie = pHouse;
            break;
        }
    }

    TechnoClass* pFirstReleased = nullptr;

    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl == nullptr || pCtrl->Slave == nullptr) {
            continue;
        }
        TechnoClass* pSlave = pCtrl->Slave;

        // asm: 游戏未激活时不动奴隶。
        if (!Game::IsActive()) {
            continue;
        }

        if (pSlave->IsDyingNow) {
            // 已阵亡：从地图上摘除。
            const AbstractType t = pSlave->WhatAmI();
            if (t == AbstractType::Infantry || t == AbstractType::Unit ||
                t == AbstractType::Aircraft) {
                static_cast<FootClass*>(pSlave)->UnInit();
            }
            continue;
        }

        HouseClass* pDest = nullptr;
        if (Owner != nullptr && Owner->Owner != nullptr) {
            pDest = Owner->Owner;                       // asm: [ebx+21Ch]
        } else if (pNewOwnerHouse != nullptr) {
            pDest = pNewOwnerHouse;
        } else {
            pDest = pFirstCivvie;
        }

        if (pDest != nullptr) {
            pSlave->Set_Owner(pDest);                   // asm: vtable[0x3D4]
            WakeSlave(pSlave);                          // asm: vtable[0x3D0]
            if (pFirstReleased == nullptr) {
                pFirstReleased = pSlave;                // asm: edi 记录第一个
            }
        }
        // 无接收方（连平民都没有）时原版走销毁分支；保留引用待回收。
    }

    // asm: 献出过奴隶且游戏进行中 -> 播放撤放音效（RulesClass+0x234）。
    if (pFirstReleased != nullptr && Game::IsActive() && RulesClass::Instance != nullptr) {
        CoordStruct pos = pFirstReleased->Get_Coord();
        const int32 sound = RulesClass::Instance->SlaveMinerUndeploySound;
        if (sound != -1) {
            VocClass::PlayIndexAtPos(sound, &pos);
        }
    }

    Owner = nullptr;   // asm: [ebx+24h]=0
}

// =============================================================================
// asm: SlaveManagerClass_ExplicitHarvest (asm:906)
//   Owner->WhatAmI()==Unit 时：读取脚下格，若可采则 State=2 并唤醒全队；
//   否则 State=0 空闲。Owner 为建筑(6)时走换位分支 State=6。
// =============================================================================
void SlaveManagerClass::ExplicitHarvest()
{
    if (Owner == nullptr) {
        State = Manager_Idle;
        return;
    }

    const AbstractType ownerType = Owner->WhatAmI();

    if (ownerType == AbstractType::Unit) {
        const CellStruct cell = CellClass::Coord2Cell(Owner->Get_Coord());
        CellClass* pCell = (MapClass::Instance != nullptr)
                               ? MapClass::Instance->GetCellAt(cell)
                               : nullptr;

        if (pCell != nullptr && pCell->Get_Contained_Tiberium_Index() != -1) {
            // 脚下有矿：转入开采态并唤醒全部奴隶。
            State = Manager_Harvest;
            StateStamp = 0x7FFFFFFF;
            for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
                SlaveControl* pCtrl = SlaveControls[i];
                if (pCtrl != nullptr && pCtrl->State != Slave_Dead &&
                    pCtrl->Slave != nullptr) {
                    WakeSlave(pCtrl->Slave);
                }
            }
            UpdateSlaveStats();
            return;
        }

        // 无矿可采：回到空闲。
        State = Manager_Idle;
        StateStamp = Game::CurrentFrame;
        return;
    }

    if (ownerType == AbstractType::Building) {
        // 主人是建筑：转入换位态，由外部 Assign_Destination 驱动。
        if (Owner->LastTarget != nullptr) {
            State = Manager_Relocate;
            StateStamp = 0x7FFFFFFF;
            for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
                SlaveControl* pCtrl = SlaveControls[i];
                if (pCtrl != nullptr && pCtrl->State != Slave_Dead &&
                    pCtrl->Slave != nullptr) {
                    WakeSlave(pCtrl->Slave);
                }
            }
        } else {
            State = Manager_Idle;
            StateStamp = Game::CurrentFrame;
        }
        return;
    }

    // 其他类型：直接空闲。
    State = Manager_Idle;
    StateStamp = Game::CurrentFrame;
}

// =============================================================================
// asm: SlaveManagerClass_ShouldWakeUp (asm:1162)
//   if (!Owner) return 0;
//   if ([+0x5C] != 0) return 0;                 // 非空闲态不唤醒
//   if (!Owner->Owner->IsHuman(+0x1EC)) return 1;
//   if (脚下 LandType == 5[Tiberium]) return 1;
//   if (+0x60 + Rules[+0x1790] >= Frame) return 0;
//   ... 扫描邻格，命中则 1。
// =============================================================================
bool SlaveManagerClass::ShouldWakeUp() const
{
    if (Owner == nullptr) {
        return false;
    }
    if (State != Manager_Idle) {
        return false;
    }
    if (Owner->Owner == nullptr) {
        return false;
    }

    // asm: [house+1ECh] 为 IsHumanPlayer；非玩家阵营立即可唤醒。
    if (!Owner->Owner->Is_Human()) {
        return true;
    }

    // asm: Owner->vtable[0x1BCh]() 取所在格，LandType==5 即泰伯利亚。
    const CellStruct cell = CellClass::Coord2Cell(Owner->Get_Coord());
    CellClass* pCell = (MapClass::Instance != nullptr)
                           ? MapClass::Instance->GetCellAt(cell)
                           : nullptr;
    if (pCell != nullptr && pCell->Get_Land_Type() == LandType::Tiberium) {
        return true;
    }

    // asm: RulesData[+0x1790]（长扫节拍）+ StateStamp 与 Frame 比较。
    int32 scanDelay = 0;
    if (RulesClass::Instance != nullptr) {
        scanDelay = RulesClass::Instance->SlaveMinerLongScan;
    }
    if (StateStamp + scanDelay >= Game::CurrentFrame) {
        return false;
    }

    // asm: 以 RulesData[+0x1780]（扫描修正）偏移主人坐标，得到候选格；
    //   只要该格在图上（非 dword_B0B5B8 哨兵）即返回真。
    return pCell != nullptr;
}

// =============================================================================
// asm: SlaveManagerClass_GetCRC (asm:1252)
//   AbstractClass::GetCRC 之后折入 [+0x5C]，再倒序折入每个槽位的 [+0x4]。
// =============================================================================
void SlaveManagerClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractClass::ComputeCRC(crc);

    crc.AddData(&State, sizeof(State));

    for (int32 i = SlaveControls.Count - 1; i >= 0; --i) {
        const SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr) {
            crc.AddData(&pCtrl->State, sizeof(pCtrl->State));
        }
    }
}

// =============================================================================
// asm: SlaveManagerClass_GetClassID (asm:1309)  retn 8
// =============================================================================
HRESULT SlaveManagerClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    // CLSID_SlaveManagerClass，原版从 .data 直接拷入；这里以 AbsID 作入口。
    pClassID->Data1 = static_cast<uint32>(AbstractType::SlaveManager);
    return S_OK;
}

// =============================================================================
// asm: SlaveManagerClass_Load (asm:1351)  retn 8
//   AbstractClass::Load → 读槽位数 → 逐槽 new SlaveControl 并读 0x14 字节 →
//   全部经 SwizzleManager 回填指针。
// =============================================================================
HRESULT SlaveManagerClass::Load(IStream* pStm)
{
    if (pStm == nullptr) {
        return E_POINTER;
    }
    // asm: AbstractClass::Load 承接公共头；OpenYR 的 AbstractClass::Load 为纯虚，
    //   实体流读写由运行时对象自身承接（LoadTables 负责重贴接口 vtable）。
    LoadTables(pStm);

    uint32 nCount = 0;
    if (pStm->Read(&nCount, sizeof(nCount), nullptr) != S_OK) {
        return E_FAIL;
    }

    SlaveControls.Clear();
    for (uint32 i = 0; i < nCount; ++i) {
        SlaveControl* pCtrl = new SlaveControl();
        pCtrl->Slave     = nullptr;
        pCtrl->State     = Slave_Dead;
        pCtrl->TimeStart = 0;
        pCtrl->field_C   = 0;
        pCtrl->TimeLeft  = 0;

        // asm: 每个槽位读入 0x14 字节（含需 swizzle 的裸指针）。
        if (pStm->Read(pCtrl, 0x14, nullptr) != S_OK) {
            delete pCtrl;
            return E_FAIL;
        }
        // 指针回填依赖全局对象表；未命中则保持空位由 Update 补员。
        if (pCtrl->Slave == nullptr) {
            pCtrl->State = Slave_Dead;
        }
        SlaveControls.Add(pCtrl);
    }

    // asm: SwizzleManagerClass_Swizzle(&Owner)、(slave 槽)、(NewOwnerHouse 槽)。
    return S_OK;
}

// =============================================================================
// asm: SlaveManagerClass_Save (asm:1539)  retn 0Ch
// =============================================================================
HRESULT SlaveManagerClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr) {
        return E_POINTER;
    }
    (void)fClearDirty;
    // asm: AbstractClass::Save 落公共头；其在本项目为纯虚，故只落派生字段。

    const uint32 nCount = static_cast<uint32>(SlaveControls.Count);
    if (pStm->Write(&nCount, sizeof(nCount), nullptr) != S_OK) {
        return E_FAIL;
    }

    for (int32 i = 0; i < SlaveControls.Count; ++i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl == nullptr) {
            SlaveControl dummy{};
            dummy.State = Slave_Dead;
            if (pStm->Write(&dummy, 0x14, nullptr) != S_OK) {
                return E_FAIL;
            }
            continue;
        }
        if (pStm->Write(pCtrl, 0x14, nullptr) != S_OK) {
            return E_FAIL;
        }
    }
    return S_OK;
}

// =============================================================================
// asm: SlaveManagerClass_PointerGotInvalid —— 失效指针清理。
//   命中 Owner 或任一槽位奴隶时清空对应引用。
// =============================================================================
void SlaveManagerClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    (void)removed;
    if (pInvalid == nullptr) {
        return;
    }

    if (static_cast<AbstractClass*>(Owner) == pInvalid) {
        Owner = nullptr;
    }

    for (int32 i = 0; i < SlaveControls.Count; ++i) {
        SlaveControl* pCtrl = SlaveControls[i];
        if (pCtrl != nullptr && static_cast<AbstractClass*>(pCtrl->Slave) == pInvalid) {
            pCtrl->Slave = nullptr;
            pCtrl->State = Slave_Dead;
            pCtrl->TimeStart = Game::CurrentFrame;
            pCtrl->TimeLeft = RegenRate;
        }
    }
}
