#include <cstdio>
#include <cstdlib>
#include <Abstract/MissionClass.h>
#include <INI/INIClass.h>
#include <IO/CRC.h>
#include <Game/ChainDebug.h>   // 逐帧调用链探针（#3 验证）

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<MissionControlClass> MissionControlClass::Array;

// ============================================================================
// MissionControlClass
// ============================================================================
// MissionControlClass_CTOR (asm:912386):
//   [this+0] = -1 (ArrayIndex), [+4] NoThreat=0, [+5] Zombie=0,
//   [+6] Recruitable=1, [+7] Paralyzed=0, [+8] Retaliate=1, [+9] Scatter=1,
//   [+10h]/[+18h] = double 0.016 (0x3F90624DD2F1A9FC).
MissionControlClass::MissionControlClass()
    : ArrayIndex(-1)
    , NoThreat(false)
    , Zombie(false)
    , Recruitable(true)
    , Paralyzed(false)
    , Retaliate(true)
    , Scatter(true)
    , Rate(0.016)
    , AARate(0.016)
{
}

const char* MissionControlClass::GetName()
{
    return MissionControlClass::FindName(static_cast<Mission>(ArrayIndex));
}

// strlist_MissionControlSections (asm:912319 indexes this table with the bare
// mission index).  The order is the eMission order.
const char* MissionControlClass::FindName(const Mission& index)
{
    static const char* names[] = {
        "Sleep", "Attack", "Move", "QMove", "Retreat", "Guard", "Sticky",
        "Enter", "Capture", "Eaten", "Harvest", "Area Guard", "Return",
        "Stop", "Ambush", "Hunt", "Unload", "Sabotage", "Construction",
        "Selling", "Repair", "Rescue", "Missile", "Harmless", "Open",
        "Patrol", "Paradrop Approach", "Paradrop Overfly", "Wait",
        "Attack Move", "Spyplane Approach", "Spyplane Overfly"
    };
    int32 idx = static_cast<int32>(index);
    if (idx >= 0 && idx < static_cast<int32>(Mission::Count)) {
        return names[idx];
    }
    return nullptr;
}

Mission MissionControlClass::FindIndex(const char* pName)
{
    if (!pName) return Mission::Sleep;
    for (int32 i = 0; i < static_cast<int32>(Mission::Count); ++i) {
        const char* name = FindName(static_cast<Mission>(i));
        if (name && _strcmpi(name, pName) == 0) {
            return static_cast<Mission>(i);
        }
    }
    return Mission::Sleep;
}

// MissionClass_LoadMissionControlFromINI (asm:912342, thiscall, retn 4).
//
//   Gate: the section must exist, otherwise the whole body is skipped and the
//   function reports false.  The five flags are read in asm order
//   (NoThreat / Zombie / Recruitable / Paralyzed / Retaliate / Scatter) with
//   the *current* value as the fallback, then Rate (fixed point) and AARate.
//   asm +190..+1A0: AARate is compared against the constant double_0 and, when
//   it equals zero, both dwords are copied from Rate (+0x10/+0x14 -> +0x18/+0x1C).
bool MissionControlClass::LoadFromINI(CCINIClass* pINI)
{
    if (!pINI) return false;

    const char* section = MissionControlClass::FindName(static_cast<Mission>(ArrayIndex));
    if (section == nullptr)
        section = "<none>";

    // asm: INIClass::Find_Section(section) -> test eax,eax -> jz loc_5B3905 (fail)
    if (pINI->GetSection(section) == nullptr)
        return false;

    NoThreat    = pINI->ReadBool(section, "NoThreat", NoThreat);
    Zombie      = pINI->ReadBool(section, "Zombie", Zombie);
    Recruitable = pINI->ReadBool(section, "Recruitable", Recruitable);
    Paralyzed   = pINI->ReadBool(section, "Paralyzed", Paralyzed);
    Retaliate   = pINI->ReadBool(section, "Retaliate", Retaliate);
    Scatter     = pINI->ReadBool(section, "Scatter", Scatter);

    // asm +150: GetFixed("Rate", &Rate)
    Rate        = pINI->ReadFixed(section, "Rate", Rate);

    // asm +168: GetFixed("AARate", 0.0) then, if it came back exactly 0.0,
    // AARate = Rate (both dwords copied, hence the bit copy in asm).
    AARate      = pINI->ReadFixed(section, "AARate", 0.0);
    if (AARate == 0.0)
        AARate = Rate;

    return true;
}

MissionControlClass* MissionControlClass::Find(const Mission& index)
{
    int32 idx = static_cast<int32>(index);
    if (idx < 0 || idx >= MissionControlClass::Array.Count)
        return nullptr;

    return &MissionControlClass::Array.Items[idx];
}

// RulesData_LoadTypeData +0x29D (asm:1300145) drives this table:
//
//     ebx = 0
//     edi = offset MissionControls
//   loc_679C99:
//     [edi] = ebx                      ; ArrayIndex = i
//     ecx = edi ; call MissionClass_LoadMissionControlFromINI(INI)
//     edi += 20h ; ++ebx
//     cmp edi, offset dword_A8E7A8     ; 32 records * 0x20 bytes
//     jl  loc_679C99
//
// So the table is a fixed 32-entry array and the *index is stamped every time*
// (not only when the table is first built), while a missing section is not an
// error: the record simply keeps its ctor defaults.
void MissionControlClass::LoadAllFromINI(CCINIClass* pINI)
{
    if (!pINI) return;

    if (MissionControlClass::Array.Count < static_cast<int32>(Mission::Count))
    {
        // Build the fixed table once; the asm array is 32 records of 0x20
        // bytes and is fully populated, not sparse.
        for (int32 i = MissionControlClass::Array.Count;
             i < static_cast<int32>(Mission::Count); ++i)
        {
            MissionControlClass entry;
            entry.ArrayIndex = i;
            MissionControlClass::Array.Add(entry);
        }
    }

    for (int32 i = 0; i < static_cast<int32>(Mission::Count); ++i)
    {
        MissionControlClass& rec = MissionControlClass::Array.Items[i];
        rec.ArrayIndex = i;               // asm: mov [edi], ebx
        rec.LoadFromINI(pINI);            // missing section keeps the defaults
    }
}

// ============================================================================
// MissionClass
// ============================================================================

// MissionClass_CTOR (asm:910917):
//   ObjectClass_CTOR, then `what`, `QueuedMission` and `field_B4` are set to -1,
//   MissionStatus / CurrentMissionStartTime / field_C0 / MissionTimer.TimeStart
//   are zeroed and MissionTimer.field_4 is stamped with the current frame.
//   currentMission itself is left as ObjectClass_CTOR produced it; OpenYR
//   initialises it to None because Mission::None is the only value the mission
//   layer treats as "no mission".
MissionClass::MissionClass() noexcept
    : ObjectClass()
    , LineTrailer(nullptr)
    , CurrentMission(Mission::None)
    , what(Mission::None)
    , QueuedMission(Mission::None)
    , field_B4(-1)
    , MissionStatus(0)
    , CurrentMissionStartTime(0)
    , field_C0(0)
    , field_C4(0)
    , MissionTimer()
    , field_CC(0)
{
    MissionTimer.StartTime = 0;                       // asm: TimeStart = 0
    MissionTimer.TimeLeft  = FrameTimer::CurrentFrame; // asm: field_4 = Frame
}

MissionClass::~MissionClass()
{
    // Drop any pending mission bookkeeping.  The asm DTOR (/MissionClass_SDDTOR/)
    // only tears the base down; the mission slots are plain scalars.
    what = Mission::None;
    QueuedMission = Mission::None;
    field_B4 = -1;
}

// ============================================================================
// Mission state machine driver
// ============================================================================

// MissionClass_Update (asm:911392..912027, shared tail at loc_5B34E0):
//
//   +0   call ObjectClass_Update
//   +9   mov al,[edi+Alive] / test / jz tail
//   +14  edx = MissionTimer.TimeStart ; eax = MissionTimer.TimeLeft
//   +1F  cmp edx,-1
//        jz  loc_5B309F               ; gate disabled -> run iff TimeLeft == 0
//        ecx = Frame - TimeStart
//        cmp ecx,eax / jge loc_5B30A7  ; elapsed >= TimeLeft -> run now
//        eax -= ecx                   ; else the remaining delay
//   loc_5B309F: test eax,eax / jnz tail
//   +56  eax = [edi+_ObjectClass.o.Health] / test / jle tail
//   +61  mov eax,[currentMission] / cmp eax,1Fh / ja mis_QMove
//        jmp ds:off_5B34E8[eax*4]
//   +A8  epilogue: TimeStart = Frame ; TimeLeft = saved slot ; field_8 = result
//
//   Three gates in asm order (alive, timer, health), then the 32-way dispatch.
void MissionClass::Update()
{
    // 调用链探针：证明 TechnoClass::Update 的 1561947 真的落到了任务层（#3）。
    ++ChainDebug::MissionUpdateCalls;

    ObjectClass::Update();

    // asm +9
    if (!Alive)
        return;

    // ── 任务采用段（原版 TechnoClass_Mission_Update 的 adopt 半）────────
    //   QueueMission → Assign_Mission 只把目标写进 field_B4，NextMission 再把它
    //   提升到 `what`（覆写槽），当前任务(+0xA8)始终不动 —— 原版由这一层
    //   在合适的时机把挂起的任务正式装上。
    {
        // ★ 待办任务的唯一来源是 field_B4（+0xB4）。
        //   QueueMission → Assign_Mission 只写 field_B4，原版靠 NextMission
        //   （ReadyToNextMission 为真时）把它提上来。本树的 ReadyToNextMission
        //   在多数任务下恒假，于是"排队的任务"永远停在 field_B4：矿车排了
        //   Unload 不去卸、单位排了 Harvest 不去采 —— 所有通过 QueueMission
        //   下达的指令都石沉大海。
        //
        //   ⚠ 不能把 `what`（+0xAC）当请求源：它是"返回到哪个任务"的过继槽
        //   （Override_Mission / SuspendMission 写它），由 Start_Default_Mission
        //   塞进去的 Guard 会一直留在那里。先前把它和 field_B4 并列当来源，
        //   结果矿车满舱排了 Unload，下一帧 `what` 里的 Guard 又把请求顶掉 ——
        //   DumpTiberium 整局零调用、AI 资金永远 0。
        //
        //   Mission::Sleep == 0 且 ForceMission 用"写 0"表示清空，所以待办
        //   判定用 `> Sleep` 而不是 `>= 0`，避免把"已清空"误读成"排了 Sleep"。
        //   三个槽都可能装着"下一个任务"，按优先级取：+0xAC（what，NextMission
        //   从 +0xB4 提上来的正式请求，也是 Override_Mission 的返回任务）>
        //   +0xB4（尚未提升的请求）> +0xB0（QueuedMission 直写路径）。
        //   任何一处留着请求都必须被采用 —— 只认其中一个槽会让"已经提到 what
        //   的请求"永远等不到采用（实测 what=16 而 CurrentMission=10 卡死）。
        Mission next = Mission::None;
        if (what != Mission::None)
            next = what;
        else if (field_B4 > static_cast<int32>(Mission::Sleep))
            next = static_cast<Mission>(field_B4);
        else if (QueuedMission != Mission::None)
            next = QueuedMission;

        if (next != Mission::None)
        {
            if (next == CurrentMission)
            {
                // 已经在跑同一个任务：把过继槽清掉，避免每帧重复进入。
                what          = Mission::None;
                QueuedMission = Mission::None;
                field_B4      = -1;
            }
            else
            {
                // 只有当前任务处于"可被打断的空闲态"时才转正；正在移动 /
                //   攻击 / 采集的对象不会被一个排队命令立刻拽走。
                const bool interruptible =
                    CurrentMission == Mission::None ||
                    CurrentMission == Mission::Guard ||
                    CurrentMission == Mission::AreaGuard ||
                    CurrentMission == Mission::Sleep ||
                    CurrentMission == Mission::Stop ||
                    CurrentMission == Mission::Hunt ||
                    // 采集不是"不可打断"的：舱满了必须能立刻转去卸矿，
                    //   否则矿车会带着满舱在原地反复空采。
                    CurrentMission == Mission::Harvest ||
                    // 卸矿同理：卸空之后要能被"回矿"接走，否则矿车卸完就
                    //   永久停在 Unload 上（货舱已空，任务永远返回 1）。
                    CurrentMission == Mission::Unload;

                // ★ Sleep / Guard / AreaGuard 都是"空闲归位"请求，不是任务指令。
                //   原版把它们放在 what 里只是让单位有个默认态；只要当前已经
                //   在做实事（Hunt/Attack/Harvest/Move/Unload…），它就不该把
                //   任务顶掉。此前缺这道门，AI 队伍刚被强制装上 Hunt，下一帧
                //   就被残留的 what=Guard 顶回 5 / what=Sleep 顶回 0，整队站桩。
                const bool nextIsIdle =
                    (next == Mission::Sleep || next == Mission::Guard
                     || next == Mission::AreaGuard);
                const bool curIsIdle =
                    (CurrentMission == Mission::None
                     || CurrentMission == Mission::Sleep
                     || CurrentMission == Mission::Guard
                     || CurrentMission == Mission::AreaGuard
                     || CurrentMission == Mission::Stop);
                if (nextIsIdle && !curIsIdle)
                {
                    what          = Mission::None;
                    QueuedMission = Mission::None;
                    field_B4      = -1;
                    // ★ 只是"拒绝这条空闲归位请求"，不是"本帧不再干活"。
                    //   原实现这里直接 return，把下面的计时器门与 AI_Mission()
                    //   一起跳过了 —— 单位任务没被改回 Sleep，却整帧不执行
                    //   任务步进，表现为"任务是对的但人不动"。清完槽继续往下走。
                    next = Mission::None;
                }

                if (next != Mission::None && interruptible)
                {
                    CurrentMission = next;
                    what           = Mission::None;
                    QueuedMission  = Mission::None;
                    field_B4       = -1;
                    Commence();          // 立刻跑第一步，并把计时器复位
                    return;
                }
            }
        }
    }

    // asm +14..+4C
    if (!MissionTimerExpired())
        return;

    // asm +56: original reads _ObjectClass.o.Health; the hook lets TechnoClass
    // supply its Health field, which OpenYR keeps one level lower.
    if (GetMissionHealth() <= 0)
        return;

    // asm +61
    AI_Mission();
}

// asm +1C..+4C of MissionClass_Update, recomputed verbatim in
// MissionClass_GetCRC at +55:
//   if (TimeStart == -1) -> the gate is off, run only when TimeLeft == 0
//   elapsed = Frame - TimeStart
//   elapsed >= TimeLeft  -> run            (jge loc_5B30A7)
//   else remaining = TimeLeft - elapsed ; run iff remaining == 0
bool MissionClass::MissionTimerExpired() const
{
    const int32 start = MissionTimer.StartTime;
    const int32 left  = MissionTimer.TimeLeft;

    if (start == -1)
        return left == 0;

    const int32 elapsed = FrameTimer::CurrentFrame - start;
    if (elapsed >= left)
        return true;

    return (left - elapsed) == 0;
}

// Same math as MissionTimerExpired, but keeps the remaining count.  The asm
// duplicates it in MissionClass_Update (+3F) and MissionClass_GetCRC (+55):
//   ecx = Frame - TimeStart ; cmp ecx,eax ; jge -> 0 ; sub eax,ecx
int32 MissionClass::RemainingMissionTime() const
{
    const int32 start = MissionTimer.StartTime;
    int32 remaining   = MissionTimer.TimeLeft;

    if (start == -1)
        return remaining;

    const int32 elapsed = FrameTimer::CurrentFrame - start;
    if (elapsed >= remaining)
        return 0;

    remaining -= elapsed;
    return remaining;
}

// asm +A8..+B8, byte-identical on all 32 jump-table targets:
//   mov ecx,Frame       -> MissionTimer.TimeStart
//   mov ecx,[esp+var_8] -> MissionTimer.TimeLeft
//   mov [esi+8],eax     -> MissionTimer.field_8 (last handler result)
//
// var_8 is never initialised in the shipped build (dead local), so the effective
// behaviour is "re-run next frame".  OpenYR writes 0 into TimeLeft and keeps the
// handler result in field_CC, which is the slot the rest of the engine reads.
void MissionClass::StampMissionTimer(int32 result)
{
    MissionTimer.StartTime = FrameTimer::CurrentFrame;
    MissionTimer.TimeLeft  = 0;
    field_CC               = result;
}

// MissionClass_Update +0x61 (asm:911457):
//   The 32-entry jump table off_5B34E8 maps currentMission to its handler.  Four
//   slots alias another handler (QMove/AttackMove -> mis_QMove i.e. Mission_Sleep,
//   Sticky -> mis_Sticky i.e. Mission_Guard, Sabotage -> mis_Sabotage i.e.
//   Mission_Capture) and any value outside 0..31 falls to mis_QMove, which is
//   also the `ja` target of the `cmp eax,1Fh` bound check.  The epilogue
//   restamps the timer with the handler's result.
int32 MissionClass::AI_Mission()
{
    // 调用链探针：证明任务计时器/血量门已过，32 路分派真的发生（#3 续）。
    ++ChainDebug::MissionDispatchCalls;

    const int32 result = ExecuteMission(CurrentMission);
    StampMissionTimer(result);
    return result;
}

// Commence: the "start the mission I am on" prologue.  The asm has no standalone
// proc for it — every entry that installs a mission inlines the same five stores
// (see TechnoClass_ForceMission +0x1B..+0x33 and TechnoClass_NextMission
// +0xE..+0x2B).  Keeping it in one place is what lets ForceMission / NextMission
// / Override_Mission stay one-liners.
void MissionClass::Commence()
{
    MissionStatus = 0;                                   // asm: [0xB8] = 0
    CurrentMissionStartTime = FrameTimer::CurrentFrame;   // asm: [0xBC] = Frame
    field_C0 = 0;                                         // asm: [0xC0] = 0
    field_C4 = 0;                                         // asm: [0xC4] = 0
    MissionTimer.StartTime = FrameTimer::CurrentFrame;    // asm: Timer.TimeStart
    MissionTimer.TimeLeft  = 0;                           // asm: Timer.field_4
    field_CC = 0;                                         // asm: Timer.field_8

    // Run the first step immediately instead of waiting for the next Update().
    const int32 result = ExecuteMission(CurrentMission);
    StampMissionTimer(result);
}

// ============================================================================
// Mission queue management
// ============================================================================

// MissionClass::Assign_Mission (asm:912116, retn 8 = 2 args, returns void).
//
//   A Wait unit is never re-tasked to plain Guard, and a selling unit refuses
//   every new mission.  Otherwise the request is parked in field_B4 (unless the
//   running mission and field_B4 already agree) and MissionStatus resets.
//   With `start` the request is promoted at once: the vtable gate at +0x200 is
//   MissionClass_200 and always answers yes, then slot +0x1EC (NextMission) runs.
void MissionClass::Assign_Mission(Mission mission, bool start_mission)
{
    const Mission current = what;



    // asm +8:  what == Wait && mission == Guard -> reject
    if (current == Mission::Wait && mission == Mission::Guard)
        return;
    // asm +15: what == Selling -> reject everything
    if (current == Mission::Selling)
        return;

    if (mission != Mission::None) {
        const bool alreadyAgreed =
            (current == mission) &&
            (field_B4 == static_cast<int32>(mission) || field_B4 == -1);

        if (!alreadyAgreed) {
            field_B4 = static_cast<int32>(mission);  // asm: [+0xB4] = mission
            MissionStatus = 0;                       // asm: byte [+0xB8] = 0
        }
    }

    if (!start_mission)
        return;

    if (!ReadyToNextMission())   // asm: call [eax+200h]
        return;

    NextMission();               // asm: call [edx+1ECh]
}

// MissionClass_QueueMission (vtable slot +0x1E8).
bool MissionClass::QueueMission(Mission mission, bool start_mission)
{
    Assign_Mission(mission, start_mission);
    return mission != Mission::None;
}

// Non-virtual "queue without starting" convenience used by construction logic.
bool MissionClass::Queue_Mission(Mission mission)
{
    Assign_Mission(mission, false);
    return mission != Mission::None;
}

// TechnoClass_NextMission (asm:912028-9120D5, retn 0 = no arguments).
//
//   if (field_B4 == -1) return false;          ; asm +4 / jz lct_Out
//   what            = field_B4                 ; +0xAC
//   field_B4        = -1                       ; +0xB4
//   CurrentMissionStartTime = 0                ; +0xBC
//   MissionTimer.TimeStart  = Frame            ; lea esi,[ecx+0C8h] ; [esi] = Frame
//   MissionTimer.field_4    = var_8 (dead)     ; [esi+4]
//   MissionTimer.field_8    = 0                ; [esi+8]
//   field_C0        = Frame                    ; +0xC0
//   field_C4        = 0                        ; +0xC4
//   MissionStatus   = 0                        ; +0xB8 (byte store)
//   return true
//
//   NOTE the running mission (+0xA8) is deliberately NOT touched: NextMission
//   only promotes the *stash*, TechnoClass decides when to adopt it.
bool MissionClass::NextMission()
{
    // asm 只判 `== -1`。本树必须再加一道"<= Sleep"的界：
    //   Mission::Sleep == 0 是合法任务号，任何 0 值的待办槽都是"空"而不是
    //   "排了 Sleep"（ForceMission 历史上就留过 0，见那里的注释）。漏掉这道
    //   界会把空槽提升成 what=Sleep，再把当前任务顶成 Sleep。
    if (field_B4 == -1 || field_B4 <= static_cast<int32>(Mission::Sleep))
        return false;

    what = static_cast<Mission>(field_B4);  // what = field_B4
    field_B4 = -1;                          // field_B4 = -1

    CurrentMissionStartTime = 0;            // asm: [+0xBC] = 0
    MissionTimer.StartTime = FrameTimer::CurrentFrame; // asm: Timer.TimeStart = Frame
    MissionTimer.TimeLeft  = 0;             // asm: [esi+4] = dead stack slot
    field_CC = 0;                           // asm: [esi+8] = 0
    field_C0 = FrameTimer::CurrentFrame;    // asm: [+0xC0] = Frame
    field_C4 = 0;                           // asm: [+0xC4] = 0
    MissionStatus = 0;                      // asm: byte [+0xB8] = 0

    return true;
}

// TechnoClass_ForceMission (asm:911324-911372, retn 4 = 1 arg).
//
//   if (currentMission == Wait && mission == Guard) return;
//   currentMission = mission                   ; +0xA8
//   QueuedMission  = -1                        ; +0xB0
//   field_B4       = 0                         ; +0xB4 (byte store of 0)
//   MissionStatus  = 0                         ; +0xB8
//   CurrentMissionStartTime = Frame            ; +0xBC
//   field_C0       = 0                         ; +0xC0
//   MissionTimer   = { Frame, var_8, 0 }       ; +0xC8
//
//   Unlike NextMission this *does* overwrite the running mission and clears the
//   queue; it never touches `what` (+0xAC), so an active override survives.
void MissionClass::ForceMission(Mission mission)
{
    if (CurrentMission == Mission::Wait && mission == Mission::Guard)
        return;

    CurrentMission = mission;                       // asm: [+0xA8] = mission
    QueuedMission  = Mission::None;                 // asm: [+0xB0] = -1
    // ★ 过继槽必须一起清。
    //   MissionClass::Update 的采用段把 `what`（+0xAC）当作最高优先级的
    //   待装任务；ForceMission 若留着旧值（典型是 Start_Default_Mission 塞进
    //   去的 Sleep/Guard），下一帧就会把刚强制装上的 Hunt 顶回 Sleep ——
    //   于是 AI 队伍成员任务恒为 0，一步都不走。
    what           = Mission::None;
    // ★ 待办槽写 -1（空），不能照抄汇编的 `byte [+0xB4] = 0`。
    //   本树用 -1 表示"没有排队任务"，而 Mission::Sleep == 0 是一个**合法
    //   任务**。按汇编写 0 会留下一个"排了 Sleep"的幻影：只要之后有任何
    //   一次 NextMission（Start_Default_Mission / InfantryClass / UnitClass /
    //   TechnoClass 都会调），它就把 0 提升进 what，采用段再把它装成
    //   CurrentMission —— 单位刚被强制装上 Hunt、打一轮，回头就变成 Sleep
    //   站着不动（实测 AI 步兵队 m0 由 15 → 1 → 0）。
    field_B4       = -1;                            // asm: [+0xB4] = 0 (byte)
    MissionStatus  = 0;                             // asm: [+0xB8] = 0
    CurrentMissionStartTime = FrameTimer::CurrentFrame; // asm: [+0xBC] = Frame
    field_C0       = 0;                             // asm: [+0xC0] = 0
    MissionTimer.StartTime = FrameTimer::CurrentFrame;  // asm: MissionTimer.TimeStart
    MissionTimer.TimeLeft  = 0;                     // asm: dead stack slot
    field_CC       = 0;                             // asm: MissionTimer.field_8
}

// MissionClass::Override_Mission (asm:912187, retn 0Ch = 3 args).
//
//   Temporarily swaps in `mission`, stashing the mission to return to in `what`:
//   the queued mission if one is pending, otherwise the running mission.  The
//   target/destination arguments are ignored by the base implementation (the
//   TechnoClass override uses them to retarget).  Wait->Guard and Selling are
//   both refused.
void MissionClass::Override_Mission(Mission mission, AbstractClass* target, AbstractClass* destination)
{
    (void)target;
    (void)destination;

    const Mission current = CurrentMission;

    if (current == Mission::Wait && mission == Mission::Guard)
        return;
    if (current == Mission::Selling)
        return;

    const Mission queued = QueuedMission;
    if (queued != Mission::None) {
        CurrentMission = mission;   // asm: [+0xA8] = newMission
        what = queued;              // asm: [+0xAC] = QueuedMission
    } else {
        what = current;             // asm: [+0xAC] = previous currentMission
        CurrentMission = mission;   // asm: [+0xA8] = newMission
    }

    field_B4 = 0;                   // asm: [+0xB4] = 0
}

// Mission_Revert: undoes an Override_Mission by restoring `what`.  The asm has
// no dedicated proc; FootClass_ResetMission goes through MissionClass_ResetMission
// for the *queue* slot, while `what` is the override slot, restored here.
bool MissionClass::Mission_Revert()
{
    if (what == Mission::None)
        return false;

    // The override slot holds the mission that was running before the override.
    CurrentMission = what;
    what = Mission::None;
    MissionStatus = 0;
    CurrentMissionStartTime = FrameTimer::CurrentFrame;

    // Re-arm the step timer so the restored mission executes on the next
    // Update() instead of inheriting the override's countdown.
    MissionTimer.StartTime = FrameTimer::CurrentFrame;
    MissionTimer.TimeLeft  = 0;
    field_CC = 0;

    return true;
}

// The override is pending while `what` holds a mission to return to.
bool MissionClass::MissionIsOverriden() const
{
    return what != Mission::None;
}

// MissionClass_200 (asm: the vtable gate at +0x200): `mov al,1 ; retn`.
// TechnoClass overrides it with the real "is the current step complete" test.
bool MissionClass::ReadyToNextMission() const
{
    return true;
}

// MissionClass_ResetMission (asm:912236):
//   With nothing queued (-1) the call is a no-op that reports failure; otherwise
//   the queued value moves into `what`, the queue is cleared back to -1 and the
//   status byte resets.
bool MissionClass::ResetMission()
{
    if (QueuedMission == Mission::None)
        return false;

    what = QueuedMission;           // asm: [+0xAC] = QueuedMission
    QueuedMission = Mission::None;  // asm: [+0xB0] = -1
    MissionStatus = 0;              // asm: byte [+0xB8] = 0

    return true;
}

// MissionClass::RestoreMission (asm: the base half of TechnoClass::Restore_Mission,
// asm:1574901-1574945).
//
//   push esi / mov esi,ecx / call MissionClass_ResetMission
//   test al,al / jz loc_701401 (return 0)
//   mov ecx,[esi+2B8h] / mov eax,[esi] / push ecx
//   call [eax+3C8h]                     ; TechnoClass-level: restore destination
//   return 1
//
//   The +0x3C8 virtual is TechnoClass state (the saved destination), so only the
//   mission-layer half lives here: promote the queued mission into `what`, and
//   restart the step machine.  TechnoClass::Restore_Mission is expected to call
//   this and then re-apply its destination.
bool MissionClass::RestoreMission()
{
    if (!ResetMission())               // asm: MissionClass_ResetMission
        return false;

    Commence();                        // re-arm the step timer for `what`

    return true;
}

// ============================================================================
// Mission state handlers (32 jump-table targets)
//  Every base handler compiles down to a bare constant return:
//      MissionClass_Mi_<Name>  ->  mov eax, <const> ; retn
//  Mission_Return and Mission_Stop are the two exceptions, both returning 0x450
//  instead of 0x1C2.
//
//      const 0x1C2 (= 450)  - step not finished, stay on this mission
//      const 0x450 (= 1104) - step finished, the mission chain may advance
//
//  FootClass / TechnoClass override the missions they actually implement; the
//  base class guarantees every mission has a "keep running" default.
// ============================================================================

// The default answer for every mission step (asm mov eax, 1C2h).
static const int32 MISSION_NOT_DONE = 0x1C2;
// The two missions that complete in their base form (asm mov eax, 450h).
static const int32 MISSION_DONE     = 0x450;

// MissionClass_Mi_Sleep (asm:910950, jump table slot 0) -> mov eax, 1C2h.
int32 MissionClass::Mission_Sleep()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Harmless (asm:910963, jump table slot 23) -> mov eax, 1C2h.
int32 MissionClass::Mission_Harmless()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Ambush (asm:910987, jump table slot 14) -> mov eax, 1C2h.
int32 MissionClass::Mission_Ambush()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Attack (asm:911000, jump table slot 1) -> mov eax, 1C2h.
int32 MissionClass::Mission_Attack()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Capture (asm:911013, jump table slots 8 and 17/Sabotage).
int32 MissionClass::Mission_Capture()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Eaten (asm:911025, jump table slot 9) -> mov eax, 1C2h.
int32 MissionClass::Mission_Eaten()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Guard (asm:911038, jump table slots 5 and 6/Sticky).
int32 MissionClass::Mission_Guard()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_AreaGuard (asm:911051, jump table slot 11) -> mov eax, 1C2h.
int32 MissionClass::Mission_AreaGuard()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Harvest (asm:911064, jump table slot 10) -> mov eax, 1C2h.
int32 MissionClass::Mission_Harvest()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Hunt (asm:911075, jump table slot 15) -> mov eax, 1C2h.
int32 MissionClass::Mission_Hunt()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Move (asm:911088, jump table slot 2) -> mov eax, 1C2h.
int32 MissionClass::Mission_Move()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Retreat (asm:911101, jump table slot 4) -> mov eax, 1C2h.
int32 MissionClass::Mission_Retreat()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Return (asm:911114, jump table slot 12) -> mov eax, 450h.
int32 MissionClass::Mission_Return()
{
    return MISSION_DONE;
}

// MissionClass_Mi_Stop (asm:911127, jump table slot 13) -> mov eax, 450h.
int32 MissionClass::Mission_Stop()
{
    return MISSION_DONE;
}

// MissionClass_Mi_Unload (asm:911140, jump table slot 16) -> mov eax, 1C2h.
int32 MissionClass::Mission_Unload()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Enter (asm:911153, jump table slot 7) -> mov eax, 1C2h.
int32 MissionClass::Mission_Enter()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Construction (asm:911166, jump table slot 18) -> 1C2h.
int32 MissionClass::Mission_Construction()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Selling (asm:911179, jump table slot 19) -> mov eax, 1C2h.
int32 MissionClass::Mission_Selling()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Repair (asm:911192, jump table slot 20) -> mov eax, 1C2h.
int32 MissionClass::Mission_Repair()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Missile (asm:911205, jump table slot 22) -> mov eax, 1C2h.
int32 MissionClass::Mission_Missile()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Open (asm:911218, jump table slot 24) -> mov eax, 1C2h.
int32 MissionClass::Mission_Open()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Rescue (asm:911227, jump table slot 21) -> mov eax, 1C2h.
int32 MissionClass::Mission_Rescue()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Patrol (asm:911240, jump table slot 25) -> mov eax, 1C2h.
int32 MissionClass::Mission_Patrol()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_ParadropApproach (asm:911254, jump table slot 26) -> 1C2h.
int32 MissionClass::Mission_ParaDropApproach()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_ParadropOverfly (asm:911268, jump table slot 27) -> 1C2h.
int32 MissionClass::Mission_ParaDropOverfly()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_Wait (asm:911281, jump table slot 28) -> mov eax, 1C2h.
int32 MissionClass::Mission_Wait()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_SpyplaneApproach (asm:911295, jump table slot 30) -> 1C2h.
int32 MissionClass::Mission_SpyPlaneApproach()
{
    return MISSION_NOT_DONE;
}

// MissionClass_Mi_SpyplaneOverfly (asm:911308, jump table slot 31) -> 1C2h.
int32 MissionClass::Mission_SpyPlaneOverfly()
{
    return MISSION_NOT_DONE;
}

// ============================================================================
// Non-virtual helpers
// ============================================================================

// The asm dispatcher is the 32-entry jump table off_5B34E8 (MissionClass_Update
// +61).  Rebuilt here as a member-function-pointer table: slot 3/QMove and
// 29/AttackMove alias mis_QMove (-> Mission_Sleep), slot 6/Sticky aliases
// mis_Sticky (-> Mission_Guard) and slot 17/Sabotage aliases mis_Sabotage
// (-> Mission_Capture).  Anything outside 0..31 falls to Mission_Sleep.
int32 MissionClass::ExecuteMission(Mission mission)
{
    typedef int32 (MissionClass::*Handler)();

    static const Handler handlers[32] = {
        &MissionClass::Mission_Sleep,            //  0 Sleep
        &MissionClass::Mission_Attack,           //  1 Attack
        &MissionClass::Mission_Move,             //  2 Move
        &MissionClass::Mission_Sleep,            //  3 QMove          -> mis_QMove
        &MissionClass::Mission_Retreat,          //  4 Retreat
        &MissionClass::Mission_Guard,            //  5 Guard
        &MissionClass::Mission_Guard,            //  6 Sticky         -> mis_Sticky
        &MissionClass::Mission_Enter,            //  7 Enter
        &MissionClass::Mission_Capture,          //  8 Capture
        &MissionClass::Mission_Eaten,            //  9 Eaten
        &MissionClass::Mission_Harvest,          // 10 Harvest
        &MissionClass::Mission_AreaGuard,        // 11 Area Guard
        &MissionClass::Mission_Return,           // 12 Return
        &MissionClass::Mission_Stop,             // 13 Stop
        &MissionClass::Mission_Ambush,           // 14 Ambush
        &MissionClass::Mission_Hunt,             // 15 Hunt
        &MissionClass::Mission_Unload,           // 16 Unload
        &MissionClass::Mission_Capture,          // 17 Sabotage       -> mis_Sabotage
        &MissionClass::Mission_Construction,     // 18 Construction
        &MissionClass::Mission_Selling,          // 19 Selling
        &MissionClass::Mission_Repair,           // 20 Repair
        &MissionClass::Mission_Rescue,           // 21 Rescue
        &MissionClass::Mission_Missile,          // 22 Missile
        &MissionClass::Mission_Harmless,         // 23 Harmless
        &MissionClass::Mission_Open,             // 24 Open
        &MissionClass::Mission_Patrol,           // 25 Patrol
        &MissionClass::Mission_ParaDropApproach, // 26 Paradrop Approach
        &MissionClass::Mission_ParaDropOverfly,  // 27 Paradrop Overfly
        &MissionClass::Mission_Wait,             // 28 Wait
        &MissionClass::Mission_Sleep,            // 29 Attack Move    -> mis_QMove
        &MissionClass::Mission_SpyPlaneApproach, // 30 Spyplane Approach
        &MissionClass::Mission_SpyPlaneOverfly,  // 31 Spyplane Overfly
    };

    const int32 idx = static_cast<int32>(mission);
    if (idx < 0 || idx >= 32)
        return Mission_Sleep();                  // asm: default -> mis_QMove

    return (this->*handlers[idx])();
}

// Suspend the running mission and switch to `mission` directly (no queue
// arbitration).  Used by the script/team layer when it wants to save the
// current mission before a forced one.
void MissionClass::SuspendMission(Mission mission)
{
    what = CurrentMission;
    CurrentMission = mission;
    MissionStatus = 0;
}

// ============================================================================
// Mission-layer queries
// ============================================================================

// MissionClass_FindNameByIdx (asm:912319): `if (idx == -1) return "<none>";
// else return strlist_MissionControlSections[idx];`
const char* MissionClass::FindNameByIdx() const
{
    if (CurrentMission == Mission::None)
        return "<none>";

    return MissionControlClass::FindName(CurrentMission);
}

// MissionClass_IsRecruitable (asm:912267, __fastcall idx):
//   `if (idx == -1) return 1; return MissionControls[idx*0x20].Recruitable;`
//   (the original indexes the 32-byte records with `idx << 5`).
bool MissionClass::IsRecruitable() const
{
    const int32 idx = static_cast<int32>(CurrentMission);
    if (idx == -1)
        return true;

    if (idx < 0 || idx >= MissionControlClass::Array.Count)
        return false;

    return MissionControlClass::Array.Items[idx].Recruitable;
}

// TechnoClass_184 (asm:911369):
//   `eax = currentMission; if (eax == -1) eax = QueuedMission; return eax;`
Mission MissionClass::GetMission() const
{
    if (CurrentMission == Mission::None)
        return QueuedMission;
    return CurrentMission;
}

bool MissionClass::IsMission(Mission mission) const
{
    return GetMission() == mission;
}

// asm: plain load of +0xB0 (`QueuedMission`).  No dedicated proc exists — every
// user inlines the load, e.g. Override_Mission +0x23, ResetMission +0x2 and
// TechnoClass_184 +0xA (the fallback when currentMission is None).
Mission MissionClass::GetQueuedMission() const
{
    return QueuedMission;
}

// asm: plain load of +0xAC (`what`), the slot Override_Mission stores the
// interrupted mission in and MissionIsOverriden tests.
Mission MissionClass::GetOverriddenMission() const
{
    return what;
}

// MissionClass::Get_Mission_Control_Data (asm:912737):
//     mov eax,[ecx+currentMission]
//     shl eax,5                      ; 32-byte records
//     add eax,offset MissionControls
//     retn
//   i.e. an unchecked &MissionControls[currentMission].  A -1 index would walk
//   one record before the table in the original, so the port refuses it instead
//   of returning a wild pointer (the only callers pass a live mission).
MissionControlClass* MissionClass::GetMissionControlData() const
{
    const int32 idx = static_cast<int32>(CurrentMission);
    if (idx < 0)
        return nullptr;

    return MissionControlClass::Find(CurrentMission);
}

// MissionClass_LoadMissionControlFromINI (asm:912342): RulesData_LoadTypeData
// reaches this to fill the whole [MissionControl] block of the rules INI.
void MissionClass::LoadMissionControlFromINI(CCINIClass* pINI)
{
    MissionControlClass::LoadAllFromINI(pINI);
}

// ----------------------------------------------------------------------------
// MissionControlClass flag accessors.
//
// The original reads these bytes inline wherever they are needed, always via
// `Get_Mission_Control_Data()` first (asm:912737) — e.g. AircraftClass_Mi_Hunt
// reads NoThreat after the call, and FootClass_CanBeRecruited reads Recruitable.
// They are collected here so the AI layer never has to know the record layout
// (and never dereferences a null record when currentMission is -1).
// ----------------------------------------------------------------------------

// asm: byte [MissionControls + idx*0x20 + 4]
bool MissionClass::IsNoThreat() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return pData != nullptr && pData->NoThreat;
}

// asm: byte [MissionControls + idx*0x20 + 5] — a "zombie" object neither
// retaliates nor scatters; it only walks its current mission.
bool MissionClass::IsZombie() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return pData != nullptr && pData->Zombie;
}

// asm: byte [MissionControls + idx*0x20 + 7] — the mission freezes the object
// (paradrop approach, selling, etc.).
bool MissionClass::IsParalyzed() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return pData != nullptr && pData->Paralyzed;
}

// asm: byte [MissionControls + idx*0x20 + 8]
bool MissionClass::CanRetaliate() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return pData != nullptr && pData->Retaliate;
}

// asm: byte [MissionControls + idx*0x20 + 9]
bool MissionClass::CanScatter() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return pData != nullptr && pData->Scatter;
}

// asm: double [MissionControls + idx*0x20 + 0x10]; 1.0 when unset so callers can
// multiply unconditionally.
double MissionClass::GetMissionRate() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return (pData != nullptr) ? pData->Rate : 1.0;
}

// asm: double [MissionControls + idx*0x20 + 0x18] (AARate).  LoadFromINI forces
// it to Rate when the INI leaves it at zero, so it is never 0 in practice.
double MissionClass::GetMissionAARate() const
{
    const MissionControlClass* pData = GetMissionControlData();
    return (pData != nullptr) ? pData->AARate : 1.0;
}

// MissionClass_GetCRC (asm:912663-912745, retn 4):
//   ObjectClass_GetCRC(crc)
//   CRC_DWORD(what)                   ; +0xAC
//   CRC_DWORD(QueuedMission)          ; +0xB0
//   CRC_DWORD(field_B4)               ; +0xB4
//   CRC_DWORD(CurrentMissionStartTime) ; +0xBC
//   CRC_DWORD(remaining mission time) ; TimeStart/TimeLeft math (+55)
//   CRC_DWORD(field_C0)               ; +0xC0
//   Note field_C4 and field_CC are NOT hashed by the original.
void MissionClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&what, sizeof(what));
    crc.AddData(&QueuedMission, sizeof(QueuedMission));
    crc.AddData(&field_B4, sizeof(field_B4));
    crc.AddData(&CurrentMissionStartTime, sizeof(CurrentMissionStartTime));

    // asm +4D..+67: TimeStart == -1 contributes TimeLeft verbatim, otherwise
    // max(0, TimeLeft - (Frame - TimeStart)).  Same arithmetic as the gate.
    int32 remaining = RemainingMissionTime();
    crc.AddData(&remaining, sizeof(remaining));

    crc.AddData(&field_C0, sizeof(field_C0));
}

// ------------------------------------------------------------------------
// asm: MissionClass_Mi_Sleep 等
// 形态，行为一一对应。
// ------------------------------------------------------------------------
int32 MissionClass::Mi_Sleep()        { return Mission_Sleep(); }
int32 MissionClass::Mi_Harmless()     { return Mission_Harmless(); }
int32 MissionClass::Mi_Ambush()       { return Mission_Ambush(); }
int32 MissionClass::Mi_Harvest()      { return Mission_Harvest(); }
int32 MissionClass::Mi_Return()       { return Mission_Return(); }
int32 MissionClass::Mi_Stop()         { return Mission_Stop(); }
int32 MissionClass::Mi_Wait()         { return Mission_Wait(); }
int32 MissionClass::Mi_Open()         { return Mission_Open(); }
int32 MissionClass::Mi_Construction() { return Mission_Construction(); }
int32 MissionClass::Mi_Selling()      { return Mission_Selling(); }
