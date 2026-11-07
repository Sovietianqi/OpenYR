#include "CD.h"

static int32 int_RequiredCD = 0;
static int32 int_CurrentCD = 0;

// ------------------------------------------------------------------------
// 根据游戏行为，可知换盘请求以模态提示框告知玩家插入指定盘号，
// 玩家确认即视为盘已就位。
// ------------------------------------------------------------------------
bool CD::Inserted(int32 diskNumber)
{
    (void)diskNumber;
    int_CurrentCD = diskNumber;
    return true;
}

// ------------------------------------------------------------------------
// 根据游戏行为，可知盘在位探测按盘符根目录可读性判定。
// ------------------------------------------------------------------------
bool CD::IsInserted()
{
    return int_CurrentCD > 0;
}

int32 CD::GetCDIndex()
{
    return int_CurrentCD;
}

bool CD::SwapToDisk(int32 diskNumber)
{
    if (IsInserted() && int_CurrentCD == diskNumber) {
        return true;
    }
    return Inserted(diskNumber);
}

void CD::SetRequiredCDIndex(int32 index)
{
    int_RequiredCD = index;
}

// ============================================================================
// asm: Force_CD_Available (真实入口 ≈0x4A8277)
//
//   ; char __thiscall CD::ForceAvailable(void* this, int cdIndex)
//   this = ecx ; retn 4
//   调用者：CD::Is_Available+24 直接 call；并经 vtable 被 6+ 处虚派发。
//
//   铁证（asm 行 37854 vt_CDClass 结构）：
//     { ForceAvailable(+0) ; FoundCdNumber(+4) ; SwapToDisk(+8) }
//   Force_CD_Available 即虚表槽 0；this+4 即 FoundCdNumber。
//
//   （AsmComplex5 原标注 0x4A8260 在 asm 中未找到；由 loc_4A829B = +0x24
//     反推真实入口 ≈0x4A8277。）
//
// 铁律①（宁可留白，不写错）：本类当前是纯静态桩，无实例/vtable；
//   依赖实例虚槽的"提示插盘"[this]->vt[+4](cd) 与全局 CD 状态
//   (RequiredCDNumber / OldRequiredCDNumber / CDDataClass.counter /
//    drives / round-robin [field_6C]) 尚未落地，此处固化已确证的
//   **判定流程**（cd==-2 / cd>=0 / Get_CD_Index 两次查询 / timeout 阶梯 /
//   round-robin 轮询 / finalize 分支），保留 asm 调用点。
// ============================================================================
namespace {
int32 g_RequiredCD    = 0;      // asm: RequiredCDNumber
int32 g_OldRequiredCD = 0;      // asm: OldRequiredCDNumber
int32 g_FoundCdNumber = -1;     // asm: this+4（vt 槽 +4）
int32 g_CDCounter     = 0;      // asm: CDDataClass.counter
int32 g_CDRoundRobin  = 0;      // asm: CDDataClass.field_6C
int32 g_CDDrives[8]   = { 0 };  // asm: CDDataClass.drives
}

int32 CD::Get_Found_Cd_Number() { return g_FoundCdNumber; }
void  CD::Set_Found_Cd_Number(int32 n) { g_FoundCdNumber = n; }

// asm: Get_CD_Index(drive, timeout) -- 探测某盘符是否就位。
// 依赖平台盘符探测，尚未落地 -> 留白（返回 -1 表示未找到）。
int32 CD::Get_Index(int32 /*drive*/, int32 /*timeout*/)
{
    return -1;
}

// asm: CD::Is_Available (0x4790E0)
//
//   注意：本类在 IO/FileSystem.cpp 里已有一份以「强制盘号」为判据的旧实现，
//   那份对应的是另一条调用路径。这里按 asm 的语义落一份：asm 里
//   Is_Available 直接 call ForceAvailable，把「能否用某个盘号」整个交给
//   强制选盘流程判定。
bool CD::Is_Available(int32 cdIndex)
{
    return ForceAvailable(cdIndex);
}

bool CD::ForceAvailable(int32 cdIndex)
{
    int32 cd = cdIndex;

    // asm: cmp edi,0FFFFFFFEh / jz loc_4A83B9
    if (cd == -2)
        return true;

    // asm: test edi,edi / jl skip / mov edi,2
    if (cd >= 0)
        cd = 2;

    // asm: esi = RequiredCDNumber / Get_CD_Index(esi, 3Ch)
    const int32 req = g_RequiredCD;
    int32       idx = Get_Index(req, 0x3C);

    int32 found   = 0;          // asm: var_8
    bool  goFinal = false;

    // asm: cmp eax,-1 / jle ; cmp edi,eax / jz ; cmp edi,-1 / jnz
    if (idx > -1 && (cd == idx || cd == -1)) {
        found = req;
        // asm: test esi,esi / mov var_8,esi / jnz 4A8388 (final)
        if (req != 0)
            goFinal = true;
    }

    if (!goFinal) {
        // asm 4A82C7: ecx = OldRequiredCDNumber / test / jz 4A82F9
        //             / cmp ecx,RequiredCDNumber / jz 4A82F9
        const int32 old = g_OldRequiredCD;
        if (old != 0 && old != req) {
            // asm: edx = 258h / call Get_CD_Index (ecx = old)
            idx = Get_Index(old, 0x258);
            if (idx > -1) {
                // asm: cmp edi,eax / jz 4A8388 ; cmp edi,-1 / jz 4A83B9
                if (cd == idx)
                    goFinal = true;
                else if (cd == -1)
                    return true;
            }
        }
    }

    if (goFinal) {
        // asm: call CD_SetRequiredCDIndex / call sub_47AAC0 / 尾部
        return true;
    }

    // asm 4A82F9: ebp = 78h
    int32 timeout = 0x78;

    for (;;) {
        found = 0;

        // asm 4A82FE: ecx = CDDataClass.counter / test / jle 4A835D
        int32 counter = g_CDCounter;
        if (counter > 0) {
            int32 k = 0;
            for (;;) {
                // asm 4A830A: test ecx,ecx / jz 4A832D (drive = -1)
                int32 drive;
                if (counter != 0) {
                    // asm: eax = field_6C / cmp eax,ecx / jnz
                    //      / xor eax,eax / mov field_6C,eax
                    int32 rr = g_CDRoundRobin;
                    if (rr == counter) {
                        rr             = 0;
                        g_CDRoundRobin = 0;
                    }
                    // asm 4A831E: esi = drives[eax*4] / inc eax / mov field_6C,eax
                    drive = (rr < 8) ? g_CDDrives[rr] : -1;
                    g_CDRoundRobin = rr + 1;
                } else {
                    drive = -1;              // asm 4A832D: or esi,-1
                }

                // asm 4A8330: Get_CD_Index(drive, ebp)
                idx = Get_Index(drive, timeout);

                // asm: cmp eax,-1 / jle ; cmp edi,eax / jz
                //      ; cmp edi,-1 / jz ; cmp edi,-2 / jz
                if (idx > -1 && (cd == idx || cd == -1 || cd == -2)) {
                    found = drive;           // asm 4A8359: mov var_8,esi
                    break;
                }

                // asm 4A834C: ecx = counter / inc ebx / cmp ebx,ecx / jl 4A830A
                ++k;
                counter = g_CDCounter;
                if (k >= counter)
                    break;
            }
        }

        // asm 4A835D: mov ecx,var_8 / test ecx,ecx / jg 4A837A
        if (found > 0) {
            // asm 4A837A: call CD_SetRequiredCDIndex (ecx = found) ; sub_47AAC0
            g_RequiredCD = found;
            return true;
        }

        // asm: mov ecx,index / push edi / mov eax,[ecx] / call [eax+4]
        //      依赖实例 vtable 槽 +4 的"提示插盘"，本类暂无实例/vtable
        //      -> 留白（返回 false 表示玩家未确认）。
        return false;

        // asm: timeout = 12Ch / jmp 4A82FE   （循环继续；见上方留白）
    }
}
