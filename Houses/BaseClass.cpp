#include "BaseClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/BuildingTypeClass.h"
#include "../INI/INIClass.h"
#include "../IO/CRC.h"
#include "../Map/MapClass.h"
#include "../Map/CellClass.h"
#include "../Network/SessionClass.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// BaseClass - pre-placed starting base (rulesmd.ini [<House>] node list)
// ============================================================================

BaseClass::BaseClass()
    : Nodes(nullptr)
    , Capacity(0)
    , IsAllocated(false)
    , GrowthStep(1)
    , PercentBuilt(0)
    , NodeCount(0)
    , OwnerHouseIndex(-1)
{
}

BaseClass::~BaseClass()
{
    delete[] Nodes;
}

void BaseClass::Clear()
{
    delete[] Nodes;
    Nodes = nullptr;
    Capacity = 0;
    NodeCount = 0;
}

// ============================================================================
 // BaseClass_LoadFromINI -
//
//   "PercentBuilt" keeps its current value when the key is absent.  The
//   "NodeCount" entries are then read from the "%03d" keys; each one is a
//   comma separated list whose first field is a building name (or a leading
//   '-' followed by a raw index), and the next two fields are the cell.
//   An entry is appended when the capacity check passes.
// ============================================================================
void BaseClass::LoadFromINI(CCINIClass* pINI, const char* pSection)
{
    if (pINI == nullptr || pSection == nullptr) {
        return;
    }

    PercentBuilt = pINI->ReadInteger(pSection, "PercentBuilt", PercentBuilt);

    const int32 cntNodes = pINI->ReadInteger(pSection, "NodeCount", 0);
    if (cntNodes <= 0) {
        return;
    }

    for (int32 idxNode = 0; idxNode < cntNodes; ++idxNode)
    {
        char key[0x10];
        std::sprintf(key, "%03d", idxNode);

        char str[0x80];
        str[0] = '\0';
        pINI->ReadString(pSection, key, "", str, sizeof(str));

        int32 firstField = 0;
        if (str[0] == '-')
        {
            // A leading '-' means the field is a raw building index.
            char* pToken = std::strtok(str, ",");
            firstField = std::atoi(pToken);
        }
        else
        {
            char* pToken = std::strtok(str, ",");
            firstField = BuildingTypeClass::FindIndex(pToken);
        }

        char* pX = std::strtok(nullptr, ",");
        const int32 x = std::atoi(pX);

        char* pY = std::strtok(nullptr, ",");
        const int32 y = std::atoi(pY);

        // 追加节点（原版走基类内嵌 DynamicVectorClass<BaseNodeClass> 的
        //   Add；这里交给 Add_Node，内部按 GrowthStep 扩容）。
        Add_Node(reinterpret_cast<BuildingTypeClass*>(
                     static_cast<intptr_t>(firstField)),
                 CellStruct(static_cast<int16>(x), static_cast<int16>(y)));
    }
}

// ============================================================================
// asm: BaseClass_SaveToINI (retn 8)
//
//   bool __thiscall BaseClass_SaveToINI(BaseClass *this, CCINIClass *INI,
//                                       char *section)
//
//   逐字段写回 PercentBuilt / NodeCount，再把每个节点按
//     "%s,%d,%d"（类型下标 >= 0，取 BuildingTypeClass::Vector[idx] 的名字）
//   或 "%d,%d,%d"（下标 < 0，写裸索引）写成 "%03d" 键。
// ============================================================================
bool BaseClass::SaveToINI(CCINIClass* pINI, const char* pSection) const
{
    if (pINI == nullptr || pSection == nullptr) {
        return false;
    }

    // asm: mov eax,[esi+1Ch] / push 0 / push eax / push "PercentBuilt"
    pINI->WriteInteger(pSection, "PercentBuilt", PercentBuilt, false);
    // asm: mov ecx,[esi+14h] / push 0 / push ecx / push "NodeCount"
    pINI->WriteInteger(pSection, "NodeCount", NodeCount, false);

    for (int32 i = 0; i < NodeCount; ++i)   // asm: loc_42EDAB
    {
        char key[0x10];
        std::sprintf(key, "%03d", i);      // asm: _sprintf("%03d")

        const BaseNodeClass& node = Nodes[i];
        const intptr_t raw = reinterpret_cast<intptr_t>(node.pType);

        char value[0x80];
        if (raw >= 0)                      // asm: test ecx,ecx / jl
        {
            // asm: mov edx,BuildingTypeClass.Vector / mov eax,[edx+ecx*4]
            //      / add eax,24h（取类型名）/ _sprintf("%s,%d,%d")
            const char* pName = "";
            if (BuildingTypeClass::Array != nullptr
                && static_cast<int32>(raw) < BuildingTypeClass::Array->Count)
            {
                BuildingTypeClass* pType = BuildingTypeClass::Array->GetItem(static_cast<int32>(raw));
                if (pType != nullptr) {
                    pName = pType->get_Name();
                }
            }
            std::sprintf(value, "%s,%d,%d", pName,
                         static_cast<int>(node.Cell.X),
                         static_cast<int>(node.Cell.Y));
        }
        else                               // asm: loc_42EDE7 push ecx / "%d,%d,%d"
        {
            std::sprintf(value, "%d,%d,%d", static_cast<int>(raw),
                         static_cast<int>(node.Cell.X),
                         static_cast<int>(node.Cell.Y));
        }

        // asm: INIClass_WriteString(INI, section, key, value)
        pINI->WriteString(pSection, key, value);
    }

    return true;
}

// ============================================================================
// asm: BaseClass_GetCRC (retn 4)
//
//   先喂 NodeCount，再逐节点喂 { pType(4B), Cell.X(2B), Cell.Y(2B) }。
//   field_08 / field_0C 不参与校验（asm 只读 [ebx] / [ebx+4] / [ebx+6]）。
// ============================================================================
void BaseClass::ComputeCRC(CRCEngine& crc) const
{
    // asm: mov eax,[esi+14h] / push eax / call CRC_DWORD
    crc.AddData(&NodeCount, sizeof(NodeCount));

    for (int32 i = 0; i < NodeCount; ++i)   // asm: loc_42F1A0
    {
        const BaseNodeClass& node = Nodes[i];

        const int32 encoded = static_cast<int32>(
            reinterpret_cast<intptr_t>(node.pType));
        crc.AddData(&encoded, sizeof(encoded));          // asm: CRC_DWORD [ebx]

        int16 cx = node.Cell.X;
        crc.AddData(&cx, sizeof(cx));                    // asm: CRC_WORD [ebx+4]
        int16 cy = node.Cell.Y;
        crc.AddData(&cy, sizeof(cy));                    // asm: CRC_WORD [ebx+6]
    }
}

// ============================================================================
// 节点管理（原版内嵌 DynamicVectorClass<BaseNodeClass> 的 Find/Add/Remove）
// ============================================================================

// 首个 {pType, cell} 完全匹配的节点下标；找不到 -1。
int32 BaseClass::Find_Node(BuildingTypeClass* pType, const CellStruct& cell) const
{
    for (int32 i = 0; i < NodeCount; ++i)
    {
        const BaseNodeClass& node = Nodes[i];
        if (node.pType == pType
            && node.Cell.X == cell.X
            && node.Cell.Y == cell.Y)
        {
            return i;
        }
    }
    return -1;
}

// 追加节点；必要时按 GrowthStep 扩容。返回新下标，失败 -1。
int32 BaseClass::Add_Node(BuildingTypeClass* pType, const CellStruct& cell)
{
    if (NodeCount >= Capacity)
    {
        // asm: 容量不足时通过向量虚表 +8 扩张；步长 GrowthStep。
        const int32 newCap = (Capacity == 0) ? GrowthStep : (Capacity + GrowthStep);
        if (newCap <= NodeCount) {
            return -1;
        }

        BaseNodeClass* pNew = new BaseNodeClass[newCap]();
        if (Nodes != nullptr)
        {
            for (int32 i = 0; i < NodeCount; ++i) {
                pNew[i] = Nodes[i];
            }
            delete[] Nodes;
        }
        Nodes = pNew;
        Capacity = newCap;
    }

    BaseNodeClass& node = Nodes[NodeCount];
    node.pType = pType;
    node.Cell = cell;
    node.field_08 = 0;
    node.field_0C = 0;
    const int32 index = NodeCount;
    ++NodeCount;
    return index;
}

// 删除指定下标的节点并左移后继。越界返回 false。
bool BaseClass::Remove_Node(int32 index)
{
    if (index < 0 || index >= NodeCount) {
        return false;
    }

    // asm: 向量 Remove —— 后继前移，长度 -1。
    for (int32 i = index; i + 1 < NodeCount; ++i) {
        Nodes[i] = Nodes[i + 1];
    }
    --NodeCount;
    return true;
}

// ============================================================================
// BaseClass_Placed (0x42F270)
//
//   Stamps the start-node that corresponds to a freshly placed building, so
//   the base builder knows that slot is already occupied.  BuildingClass::
//   Unlimbo calls it as
//
//     ecx = &house->Base      (asm: add ecx, HouseClass.Base)
//     push building           (arg_0)
//     call BaseClass_Placed   (retn 4 -> exactly one stack argument)
//
//   Disassembly (arg_0 == ebp == the BuildingClass):
//
//     mov eax,[ebp+21Ch]            ; building's OwnerHouseIndex
//     cmp eax,[esi+74h]             ; vs. the house this base belongs to
//     jnz loc_42F2DE                ; different house -> arm 2
//
//     mov eax,[esi+14h]             ; vec_Nodes.length
//     xor edi,edi / test eax,eax / jle loc_42F2DE
//     xor ebx,ebx                   ; byte offset into vec_Nodes.data
//   loc_42F285:
//     mov ecx,[esi+8]               ; vec_Nodes.data
//     mov edx,[ebp+520h]            ; building's Type
//     add ebx,ecx
//     mov eax,[edx+NumberInArray]   ; Type->ArrayIndex
//     cmp eax,[ebx]                 ; node->pType index
//     jnz loc_42F2BD
//     mov edx,[ebp] / lea eax,[var_4] / push eax / mov ecx,ebp
//     call [edx+1B8h]               ; virtual -> this building's cell
//     mov cx,[eax] / cmp cx,[ebx+4]                 ; node->Cell.X
//     jnz loc_42F2BD
//     mov dx,[eax+2] / cmp dx,[ebx+6]               ; node->Cell.Y
//     jz  loc_42F2D2
//   loc_42F2BD:
//     mov ebx,[arg_0] / mov eax,[esi+14h] / inc edi
//     add ebx,10h                   ; stride = sizeof(BaseNodeClass)
//     cmp edi,eax / mov [arg_0],ebx / jge loc_42F2DE
//     jmp loc_42F285
//   loc_42F2D2:
//     mov ecx,[esi+8] / shl edi,4 / add edi,ecx
//     test edi,edi / jnz loc_42F321
//   loc_42F2DE:                     ; arm 2
//     mov ebp,[ebp+520h]            ; Type again (ebp is reused as the type)
//     mov eax,[ebp+408h]            ; Type->"is base" flag
//     test eax,eax / jz  loc_42F32C
//     mov ecx,[esi+14h] / mov ebp,[ebp+0DF8h] / xor eax,eax
//     test ecx,ecx / jle loc_42F32C
//     mov esi,[esi+8]               ; vec_Nodes.data
//   loc_42F300:
//     cmp [esi],ebp                 ; node->pType == that Type pointer
//     jnz loc_42F30B
//     mov dl,[esi+8] / test dl,dl / jz loc_42F31B     ; already placed?
//   loc_42F30B:
//     inc eax / add esi,10h / cmp eax,ecx / jl loc_42F300
//     ... retn 4                    ; not found -> no write
//   loc_42F31B:
//     test esi,esi / mov edi,esi / jz loc_42F32C
//   loc_42F321:
//     mov byte [edi+8],1            ; node->field_08 = 1
//     mov dword [edi+0Ch],0         ; node->field_0C = 0
//   loc_42F32C:
//     retn 4
//
//   Two subtle points preserved faithfully:
//
//   * arm 2's outer guard is on Type+408h (nonzero) but the loop itself
//     compares node->pType against Type+0DF8h -- two different fields of the
//     same BuildingTypeClass.  They are not interchangeable.
//
//   * arm 1 falls through to arm 2 when the scan misses (loc_42F2D2 -> test
//     edi,edi -> jz? no: on a miss it jumps to loc_42F2DE).  The hit path
//     stamps the node and returns through loc_42F321; the miss path continues
//     into arm 2, which is why a same-house building can still be matched by
//     pointer in the second pass.
// ============================================================================
bool BaseClass::Placed(BuildingClass* pBuilding)
{
    if (pBuilding == nullptr)
        return false;

    // asm: mov eax,[ebp+21Ch] / cmp eax,[esi+74h]
    //
    //   OwnerHouseIndex 在本树有几条建房路径（Scenario 大厅、MakeHouses）
    //   未必都被赋值；留 -1 时用"调用本函数的这个建筑自己的房索引"兜底 ——
    //   Placed 本来就是由该房自己的 Unlimbo 调的，两者必然同一房。
    const int32 baseOwnerIndex = (this->OwnerHouseIndex >= 0)
        ? this->OwnerHouseIndex : pBuilding->GetOwningHouseIndex();
    if (pBuilding->GetOwningHouseIndex() == baseOwnerIndex) {
        // asm: mov eax,[esi+14h] / xor edi,edi / test / jle loc_42F2DE
        BuildingTypeClass* pBuildingType = pBuilding->Type;
        const int32 wantIndex =
            (pBuildingType != nullptr) ? pBuildingType->Get_ArrayIndex() : -1;

        for (int32 i = 0; i < this->NodeCount; ++i) {      // asm: loc_42F285
            BaseNodeClass& node = this->Nodes[i];

            // asm: mov eax,[edx+NumberInArray] / cmp eax,[ebx]
            //
            //   ★ 本工程的节点里存的是 BuildingTypeClass*（GenerateAIBuildList
            //     ⑦ 段写指针），原版存的是数组下标。两种编码都认：先比指针，
            //     再比下标，否则"类型匹配"这一支永远走不通，节点打不上标记。
            const intptr_t nodeRaw = reinterpret_cast<intptr_t>(node.pType);
            bool typeMatch =
                (node.pType == pBuildingType)
                || (wantIndex >= 0 && nodeRaw == static_cast<intptr_t>(wantIndex));

            // 指针/下标都不等时按 ID 认：本树的类型注册走 FindOrAllocate，
            //   同名类型在"列表登记"与"DeploysInto / FreeUnit 引用"两条路径上
            //   有可能拿到不同实例，指针比对随之失效，节点永远打不上标记 ——
            //   AI 就会反复造同一座建筑（实测 bldTo 卡在 GAWEAP、n08 停在 3）。
            if (!typeMatch && node.pType != nullptr && pBuildingType != nullptr
                && node.pType->ID[0] != '\0' && pBuildingType->ID[0] != '\0')
            {
                typeMatch = (std::strcmp(node.pType->ID, pBuildingType->ID) == 0);
            }
            if (!typeMatch)
                continue;

            // asm: call [edx+1B8h] -- the building's own cell
            const CellStruct cell = pBuilding->Get_Cell_Ptr_Coord();

            // 节点格还是初始的 (0,0)：首次落位就把节点钉到真实格子上。
            //   AI 清单生成时并不选点（原版由 Base 规划器在放置前填格），
            //   若这一步不写回，arm1 的"格子相等"永远不成立。
            if (node.Cell.X == 0 && node.Cell.Y == 0) {
                node.Cell = cell;
                node.field_08 = 1;
                node.field_0C = 0;
                return true;
            }

            if (cell.X != node.Cell.X || cell.Y != node.Cell.Y)   // asm: cmp cx/[ebx+4]
                continue;

            // asm: loc_42F2D2 .. loc_42F321 -- matched: stamp and return.
            node.field_08 = 1;
            node.field_0C = 0;
            return true;
        }
        // asm: fell through the whole scan -> loc_42F2DE (arm 2)
    }

    // --- arm 2 ---------------------------------------------------------------
    // asm: mov ebp,[ebp+520h] / mov eax,[ebp+408h] / test / jz loc_42F32C
    const BuildingTypeClass* pType = pBuilding->Type;
    if (pType == nullptr || pType->field_408 == 0)
        return false;

    // asm: mov ecx,[esi+14h] / mov ebp,[ebp+0DF8h] / xor eax,eax
    BuildingTypeClass* const wantPtr = pType->field_DF8;
    for (int32 i = 0; i < this->NodeCount; ++i) {          // asm: loc_42F300
        BaseNodeClass& node = this->Nodes[i];

        // asm: cmp [esi],ebp / jnz loc_42F30B
        if (node.pType != wantPtr)
            continue;

        // asm: mov dl,[esi+8] / test dl,dl / jz loc_42F31B
        if (node.field_08 != 0)
            continue;                                      // already stamped

        // asm: loc_42F31B / loc_42F321
        node.field_08 = 1;
        node.field_0C = 0;
        return true;
    }

    // asm: loop exhausted -> retn 4 (no write)
    return false;
}

// ============================================================================
// asm: sub_42E780(BaseClass *this, int idx) —— 节点是否"已占用/不可再建"。
//
//   mov eax,[edi+8] / shl esi,4 / add eax,esi     ; &vec_Nodes[idx]
//   mov ecx,[eax+4]                               ; node.Cell（wXY）
//   ... MapClass::Coord_Cell + CellClass::Cell_Building
//   若该格上已有建筑，且建筑的类型索引 == 节点的类型索引、且建筑的格子 == 节点
//   的格子、且归属同一房 -> 视为占用。
//   随后再看该节点类型是否 Wall：是墙且落点已被墙占 -> 占用。
//   本项目按其可表达的判定重写：格上有建筑即视为占用（节点格本就是预置建筑位）。
// ============================================================================
bool BaseClass::Is_Node_Occupied(int32 index) const
{
    if (index < 0 || index >= NodeCount) {
        return false;
    }

    const BaseNodeClass& node = Nodes[index];

    // asm: 节点 pType 索引 < 0 -> 未占用。
    if (reinterpret_cast<intptr_t>(node.pType) < 0) {
        return false;
    }

    if (MapClass::Instance == nullptr) {
        return false;
    }

    CellClass* pCell = MapClass::Instance->GetCellAt(node.Cell);
    if (pCell == nullptr) {
        return false;
    }

    // asm: call CellClass::Cell_Building -> 非空即认为该起始位已被占。
    BuildingClass* pBuilding = pCell->Cell_Building();
    if (pBuilding != nullptr) {
        return true;
    }

    return false;
}

// ============================================================================
// asm: BaseClass::Next_Buildable (0x42EB3C, retn 4)
//
//   BaseNodeClass *__thiscall BaseClass::Next_Buildable(BaseClass *this, int a2)
//
//   sub_42EB50(this, a2) 返回下一个可建节点的下标（找不到 -1），
//   随后 `mov ecx,[esi+8] / shl eax,4 / add eax,ecx` 把它换算成节点指针。
//   a2 == -1：任意类型；否则限 pType 索引 == a2。
//   命中条件（sub_42EB50）：
//     a2 == -1:
//        sub_42E780(i) 为真（已占）-> 跳过；
//        Session == 0          -> 返回 i；
//        node.field_08 == 0    -> 返回 i；
//        否则（属多人为战且已标记）继续扫描。
//     a2 != -1:
//        node.pType 索引 != a2 -> 跳过；
//        sub_42E780(i) 为真     -> 返回 i。
// ============================================================================
BaseNodeClass* BaseClass::Next_Buildable(int32 a2)
{
    // asm: mov eax,[esi+14h] / test / jle -> 空表
    if (NodeCount <= 0) {
        return nullptr;
    }

    for (int32 i = 0; i < NodeCount; ++i) {           // asm: loc_42EB65
        BaseNodeClass& node = Nodes[i];

        if (a2 == -1) {
            // asm: call sub_42E780 -> 已占则跳过
            if (Is_Node_Occupied(i)) {
                continue;
            }
            // asm: Session == 0 -> 直接返回该节点 / 否则看 field_08 == 0。
            //
            //   ★ 单机分支不能无条件返回：原版单机任务的节点由场景脚本预置，
            //     节点自带格子，Is_Node_Occupied 足以判"建过没有"；本工程的
            //     节点是 GenerateAIBuildList 现生成的，落位前格子是 (0,0)，
            //     只靠 Is_Node_Occupied 判不出重复 —— AI 会拿着同一个节点
            //     无限重造同一座建筑（实测造出 21 座后才因缺钱停下）。
            //     Placed() 会把节点钉到真实格子并置 field_08=1，随后
            //     Is_Node_Occupied 与 field_08 两条都能判"已建"。
            if (SessionClass::GetInstance() == nullptr) {
                if (node.field_08 == 0) {
                    return &node;
                }
                continue;
            }
            // asm: mov cl,[eax+8] / test / jz -> field_08 == 0 则可用
            if (node.field_08 == 0) {
                return &node;
            }
            // asm: 已标记且需重建时才复用（依赖 Base_ShouldRebuildNow）。
            continue;
        }

        // asm: cmp [ecx+ebx], ebp / jnz -> 类型索引须匹配
        if (reinterpret_cast<intptr_t>(node.pType) != static_cast<intptr_t>(a2)) {
            continue;
        }
        if (Is_Node_Occupied(i)) {
            return &node;
        }
    }

    // asm: loc_42EBB9 or eax,0FFFFFFFFh -> 返回 nullptr
    return nullptr;
}
