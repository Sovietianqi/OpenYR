#include "StartSpotClass.h"

#include "../Core/Memory.h"

#include <cstring>

// ============================================================================
// StartSpotClass implementations
// ============================================================================

// asm: CSF_* sentinel slots -- the original compares the raw entry pointer
// against seven well-known reserved addresses.  Distinguishable static
// stand-ins preserve the comparison semantics.
static StartSpotClass g_CSF_OpenSlot;
static StartSpotClass g_CSF_ClosedSlot;
static StartSpotClass g_CSF_Waiting;
static StartSpotClass g_CSF_Observer;
static StartSpotClass g_CSF_EasyAI;
static StartSpotClass g_CSF_NormAI;
static StartSpotClass g_CSF_HardAI;

StartSpotClass* Get_CSF_OpenSlot()   { return &g_CSF_OpenSlot; }
StartSpotClass* Get_CSF_ClosedSlot() { return &g_CSF_ClosedSlot; }
StartSpotClass* Get_CSF_Waiting()    { return &g_CSF_Waiting; }
StartSpotClass* Get_CSF_Observer()   { return &g_CSF_Observer; }
StartSpotClass* Get_CSF_EasyAI()     { return &g_CSF_EasyAI; }
StartSpotClass* Get_CSF_NormAI()     { return &g_CSF_NormAI; }
StartSpotClass* Get_CSF_HardAI()     { return &g_CSF_HardAI; }

// ----------------------------------------------------------------------------
// asm: StartSpotClass_IsNotDummy (0x5E9420)
// ----------------------------------------------------------------------------
bool StartSpotClass::Is_Not_Dummy(const StartSpotClass* pEntry)
{
    // asm: test ecx,ecx / jz false
    if (pEntry == nullptr)
        return false;

    // asm: seven-way sentinel rejection chain
    if (pEntry == &g_CSF_OpenSlot)       // asm: cmp ecx,CSF_OpenSlot_
        return false;
    if (pEntry == &g_CSF_ClosedSlot)     // asm: cmp ecx,CSF_ClosedSlot_
        return false;
    if (pEntry == &g_CSF_Waiting)        // asm: cmp ecx,CSF_Waiting_
        return false;
    if (pEntry == &g_CSF_Observer)       // asm: cmp ecx,CSF_Observer_
        return false;
    if (pEntry == &g_CSF_EasyAI)         // asm: cmp ecx,CSF_EasyAI_
        return false;
    if (pEntry == &g_CSF_NormAI)         // asm: cmp ecx,CSF_NormAI_
        return false;
    if (pEntry == &g_CSF_HardAI)         // asm: cmp ecx,CSF_HardAI_
        return false;

    // asm: mov eax,1 / retn
    return true;
}

// ============================================================================
// StartSpot_Create_Potato / _PotatoToo / _OlCruddy
//
// asm 真实地址（经核实均存在，与 AsmComplex3 标注一致）：
//   StartSpot_Create_OlCruddy  0x796DC0
//   StartSpot_Create_Potato    0x796EC0
//   StartSpot_Create_PotatoToo 0x796FA0
// 三者真实签名均为 void(void)，调用者 sub_7970D0（开发者主机名彩蛋）。
//
// 下面是 asm 缺失依赖的留白声明（铁律①：宁可留白，不写错）：
//   sub_53ECB0  名字节点 init      sub_735060  赋显示名
//   sub_53ED20  赋坐标            NameNodeTypes / dword_B7790C / stru_A8BB1C
// 这些例程与全局数据尚未落地，此处只保留 asm 调用点、参数与原偏移，
// 不臆造实现；待相应模块补齐后再回填（与 LoadProgressMgr 处理方式一致）。
// ============================================================================
namespace {

// asm: dword_B7790C[gPlayerCount] = 0x64 ; ++gPlayerCount
// 全局玩家开始位表 + 计数。asm 里的两个全局对象，此处保留接入点。
int32* g_pPlayerSpotTable = nullptr;      // asm: dword_B7790C
int32* g_pPlayerCount     = nullptr;      // asm: gPlayerCount

// asm: stru_A8BB1C -- PotatoToo 额外拷贝的 12 字节坐标节点镜像
int32 g_StruA8BB1C[3] = { 0, 0, 0 };

// asm: NameNodeTypes 追加（DynamicVectorClass 增长语义）
void NameNode_Append(StartSpotClass* /*pNode*/)
{
    // asm: cmp length,eax(count) / jl loc_796F7F
    //      cmp IsAllocated,bl / jnz grow ; mov ecx,incrementAmount ...
    //      add ecx,eax / push ebx / push ecx / call [vt+8]
    //      test al,al / jz skip ; data[length] = node ; length++
    // 依赖全局 NameNodeTypes 向量，尚未落地 -> 留白。
}

// asm: loc_796F97 block
void Register_Player_Spot()
{
    // asm: mov eax,gPlayerCount / mov dword_B7790C[eax*4],64h / inc eax
    if (g_pPlayerSpotTable != nullptr && g_pPlayerCount != nullptr)
        g_pPlayerSpotTable[*g_pPlayerCount] = 0x64;
    if (g_pPlayerCount != nullptr)
        ++(*g_pPlayerCount);
}

// asm: 公共骨架 -- 分配 0x85 字节记录、初始化 +0x28 名字节点、赋显示名
StartSpotClass* Alloc_Start_Spot(const char* /*pName*/)
{
    // asm: push 85h / call new / mov esi,eax / cmp esi,ebx / jz -> esi = 0
    StartSpotClass* pObj =
        static_cast<StartSpotClass*>(YRMemory::Allocate(0x85));
    if (pObj == nullptr)
        return nullptr;

    // asm: lea ecx,[esi+28h] / call sub_53ECB0      ; 名字节点 init
    // asm: push -1 / mov edx,<name> / mov ecx,esi / call sub_735060
    //      两个引擎例程尚未落地 -> 留白（保留调用点与参数形状）。

    // asm: mov dword ptr [esi+7Bh],0FFFFFFFFh
    *reinterpret_cast<int32*>(reinterpret_cast<uintptr_t>(pObj) + 0x7B) = -1;

    return pObj;
}

// asm: 公共坐标负载 -- a2 = 0, a3 = {0Ah,02h,X,Y,0,0} -> sub_53ED20(obj+28h)
void Assign_Coords(StartSpotClass* pObj, uint8 x, uint8 y, bool mirrorGlobal)
{
    if (pObj == nullptr)
        return;

    int32 a2 = 0;
    uint8 a3[6] = { 0x0A, 0x02, x, y, 0x00, 0x00 };

    // asm: lea ecx,[esi+28h] / push 0 / push &a3 / push &a2 / call sub_53ED20
    //      依赖 sub_53ED20，尚未落地 -> 留白。

    if (mirrorGlobal) {
        // asm: mov edx,[edi] / mov stru_A8BB1C.field_0,edx ...
        //      拷贝 12 字节坐标节点到全局镜像。
        std::memcpy(g_StruA8BB1C,
                    reinterpret_cast<const void*>(
                        reinterpret_cast<uintptr_t>(pObj) + 0x28),
                    sizeof(g_StruA8BB1C));
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// asm: StartSpot_Create_Potato (0x796EC0)
// ----------------------------------------------------------------------------
StartSpotClass* StartSpot_Create_Potato()
{
    // asm: xor ebx,ebx / push 85h / call new ...
    StartSpotClass* pObj = Alloc_Start_Spot("Potato");
    if (pObj == nullptr)
        return nullptr;

    // asm: mov [esi+4Bh],ebx          (country 0)
    // asm: mov [esi+53h],ebx          (colour 0)
    pObj->CountrySlot = 0;
    pObj->ColourSlot  = 0;

    // a3 = { 0Ah, 02h, 14h, 61h, 00, 00 }
    Assign_Coords(pObj, 0x14, 0x61, false);

    NameNode_Append(pObj);       // asm: loc_796F7F block
    Register_Player_Spot();      // asm: loc_796F97 block
    return pObj;
}

// ----------------------------------------------------------------------------
// asm: StartSpot_Create_PotatoToo (0x796FA0)
// ----------------------------------------------------------------------------
StartSpotClass* StartSpot_Create_PotatoToo()
{
    StartSpotClass* pObj = Alloc_Start_Spot("PotatoToo");
    if (pObj == nullptr)
        return nullptr;

    // asm: mov dword ptr [esi+4Bh],1   (country 1)
    // asm: mov eax,2 / mov [esi+53h],eax  (colour 2)
    pObj->CountrySlot = 1;
    pObj->ColourSlot  = 2;

    // a3 = { 0Ah, 02h, 50h, FAh, 00, 00 }
    Assign_Coords(pObj, 0x50, 0xFA, true);   // asm: 额外写 stru_A8BB1C

    NameNode_Append(pObj);
    Register_Player_Spot();
    return pObj;
}

// ----------------------------------------------------------------------------
// asm: StartSpot_Create_OlCruddy (0x796DC0)
// ----------------------------------------------------------------------------
StartSpotClass* StartSpot_Create_OlCruddy()
{
    StartSpotClass* pObj = Alloc_Start_Spot("Ol Cruddy");
    if (pObj == nullptr)
        return nullptr;

    // asm: mov [esi+4Bh],ebx           (country 0)
    // asm: mov dword ptr [esi+53h],3   (colour 3)
    pObj->CountrySlot = 0;
    pObj->ColourSlot  = 3;

    // a3 = { 0Ah, 02h, 51h, 52h, 00, 00 }
    Assign_Coords(pObj, 0x51, 0x52, false);

    NameNode_Append(pObj);
    Register_Player_Spot();
    return pObj;
}

// ----------------------------------------------------------------------------
// asm: sub_7970D0 -- GetComputerNameA / "STEVE-T3" | "STEVE-T2" dispatcher
// ----------------------------------------------------------------------------
void StartSpot_Create_DevBots()
{
    // asm: GetComputerNameA(name, &size)
    //      __strcmpi(name,"STEVE-T3") -> Create_Potato / Create_PotatoToo
    //      __strcmpi(name,"STEVE-T2") -> Create_OlCruddy
    // 依赖 GetComputerNameA（Win32）与三个硬编码主机名常量；
    // 铁律①：不臆造，保留 asm 调用点形状。
}
