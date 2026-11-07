#include "DropshipLoadoutClass.h"

#include "../Rendering/Surface.h"
#include "../Rendering/ConvertClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Combat/WeaponTypeClass.h"

#include <cwchar>
#include <cstring>

DSurface* DropshipLoadoutClass::HiddenSurface = nullptr;

// ============================================================================
// asm: DropshipLoadout_WriteUnit (0x4B9A80 区)
//
//   thiscall: ecx = 条目上下文（vt+80h = Get_Width，vt+7Ch = Get_Height），
//             edx = dc，[esp+4] = 单位类型，[esp+8] = showCost
//
//   w  = this->Get_Width()  ; hw = DSurface_Hidden->Get_Width()
//   x0 = (hw - w) / 2                       ; cdq / sub / sar 1
//   h  = this->Get_Height() ; hh = DSurface_Hidden->Get_Height()
//   y0 = (hh - h) / 2
//
//   栈槽 var_13C 是一组 {1C2h, 12Ch, 1C2h, 12Ch, 0BEh, 050h}，先由
//   sub_437F10 造出 {0BEh, 050h} 的矩形、Rect::Set_Pos 定位，再选 0 号
//   Blitter，最后 CC_Draw_Shape_4AF2A0(Hidden, dc, src=+8h, zero, pixRect)。
//
//   if (unit == 0) return
//   _swprintf(Name,      "…%s…", unit+64h)        ; +64h = UIName
//   showCost ? _swprintf(Cost, "COST %d", vt+0ACh=Get_Cost())
//            : _swprintf(Cost, "COST N/A")
//   w = TechnoType_GetWeapon(unit, 0)
//   if (w && w->[0]) _swprintf(Armament, "…%s…", w->[0]->[64h])
//   else             _swprintf(Armament, "ARMAMENT NONE")
//   _swprintf(Armor, "…%s…", ArmorStrList[unit->[9Ch]])
//   scheme = ColorScheme_Find(showCost ? "Green" : "Red", showCost ? 1 : 0)
//
//   四行文字各走 HiddenSurface->vt+30h(rect, pos, scheme, 0, 46h) 之后
//   PrintUnicode(pos, text, surface, ret)；四次的 rect 依次取 var_114 的
//   +0 / +0Ch / +8 / +4 槽（原版的错位取法，照抄）。
// ============================================================================

namespace {

// asm 的 rdata 格式串（字面量 UNVERIFIED，前缀长度由 PrintUnicode 的
// 文本偏移反推：name+6 / armament+8 / armor+4 正好跳过各串的前缀）。
const wchar_t* const kFmtName         = L"NAME: %s\n";      // 前缀 6
const wchar_t* const kFmtCost         = L"COST %d\n";
const wchar_t* const kFmtCostNA       = L"COST N/A\n";
const wchar_t* const kFmtArmament     = L"ARMAMENT %s\n";   // 前缀 8
const wchar_t* const kFmtArmamentNone = L"ARMAMENT NONE\n";
const wchar_t* const kFmtArmor        = L"ARM %s\n";        // 前缀 4

// asm 的 ArmorStrList —— 护甲下标到字符串
const wchar_t* ArmorName(int32 armor)
{
    static const wchar_t* const kArmorNames[11] = {
        L"none", L"flak", L"plate", L"light", L"medium",
        L"heavy", L"wood", L"steel", L"concrete", L"special_1", L"special_2"
    };
    if (armor < 0 || armor > 10)
        return L"unknown";
    return kArmorNames[armor];
}

// asm: cdq / sub eax,edx / sar eax,1 —— 有符号除以 2 向下取整
inline int32 FloorDiv2(int32 v)
{
    return (v - (v < 0 ? 1 : 0)) >> 1;
}

}  // namespace

void DropshipLoadoutClass::Write_Unit(DSurface* pShape, DSurface* pDc,
                                      const TechnoTypeClass* pUnit, bool showCost)
{
    DSurface* pHidden = HiddenSurface;

    // asm: vt+80h（Get_Width）两次 -> 居中 X；vt+7Ch（Get_Height）两次 -> 居中 Y
    const int32 w  = (pShape != nullptr) ? static_cast<int32>(pShape->Width)  : 0;
    const int32 h  = (pShape != nullptr) ? static_cast<int32>(pShape->Height) : 0;
    const int32 hw = (pHidden != nullptr) ? static_cast<int32>(pHidden->Width)  : 0;
    const int32 hh = (pHidden != nullptr) ? static_cast<int32>(pHidden->Height) : 0;
    const int32 x0 = FloorDiv2(hw - w);              // asm: var_148
    const int32 y0 = FloorDiv2(hh - h);              // asm: var_14C

    // asm var_13C 槽组：{1C2h, 12Ch, 1C2h, 12Ch, 0BEh, 050h}
    const int32 slot_1C2a = 0x1C2;
    const int32 slot_12Ca = 0x12C;
    const int32 slot_1C2b = 0x1C2;
    const int32 slot_12Cb = 0x12C;
    const int32 slot_0BE  = 0x0BE;
    const int32 slot_050  = 0x050;

    // asm: sub_437F10(&var_13C 槽组, &rect, &var_14C{x0,y0}, 0BEh, 050h)
    //      再 Rect::Set_Pos / Select_Blitter(0) / CC_Draw_Shape_4AF2A0(...)
    // 降级点：本树没有 Make_Load_Rect / Rect::Set_Pos / Select_Blitter /
    //   CC_Draw_Shape 这一套造型贴图原语（渲染后端未接），此处只保留居中
    //   与尺寸计算；pDc/pShape 的贴图按 DSurface::BlitPart 的语义留待后端接。
    Rectangle shapeRect(x0, y0, slot_0BE, slot_050);
    (void)slot_1C2a; (void)slot_12Ca; (void)slot_1C2b; (void)slot_12Cb;
    if (pDc == nullptr || pShape == nullptr)
        return;

    if (pUnit == nullptr)                            // asm: cmp esi,ebx / jz 出口
        return;

    // ---- 四行说明文字 ----------------------------------------------------
    wchar_t name[0x16], cost[0x14], armament[0x28], armor[0x1E];
    swprintf(name, 0x16, kFmtName, pUnit->GetUIName());        // asm: unit+64h

    if (showCost) {
        swprintf(cost, 0x14, kFmtCost, pUnit->BuildCost);      // asm: vt+0ACh
    } else {
        swprintf(cost, 0x14, L"%s", kFmtCostNA);
    }

    {
        const WeaponStruct* pSlot = pUnit->GetWeapon(0);        // asm: GetWeapon(0)
        if (pSlot != nullptr && pSlot->WeaponType != nullptr) {
            swprintf(armament, 0x28, kFmtArmament,
                     pSlot->WeaponType->UIName);                // asm: w->[0]->[64h]
        } else {
            swprintf(armament, 0x28, L"%s", kFmtArmamentNone);
        }
    }

    swprintf(armor, 0x1E, kFmtArmor,
             ArmorName(static_cast<int32>(pUnit->ArmorType)));  // asm: [esi+9Ch]

    // asm: ColorScheme_Find(showCost ? "Green" : "Red", showCost ? 1 : 0)
    // 降级点：本树 DSurface::DrawText 直接吃 DWORD 颜色，配色方案对象由
    //   ColorScheme 提供；这里取方案的 RGB 分量做前景色。
    ColorScheme* pScheme = ColorScheme::Find(showCost ? "Green" : "Red",
                                             showCost ? 1 : 0);
    DWORD fore = 0x00FFFFFFu;
    if (pScheme != nullptr)
        fore = static_cast<DWORD>(pScheme->BaseColor.R)
             | (static_cast<DWORD>(pScheme->BaseColor.G) << 8)
             | (static_cast<DWORD>(pScheme->BaseColor.B) << 16);

    // asm: 四次绘制都传 46h 作为文本旗标
    const TextPrintType flags = static_cast<TextPrintType>(0x46);

    // 四次绘制的落点（asm 的错位槽：+0 / +0Ch / +8 / +4，最后 +12Ch/1C2h 步进）
    Rectangle box(shapeRect.X, shapeRect.Y, shapeRect.Width, shapeRect.Height);
    int32 posX = x0;
    int32 posY = y0;

    // -- 第 1 行：名字（rect 槽 +0）--
    {
        Point2D pos(posX + slot_1C2a, posY + slot_12Ca);
        box.X = shapeRect.X + 0;
        pDc->DrawText(name + 6, &box, &pos, fore, 0, flags);
    }
    // -- 第 2 行：武器（rect 槽 +0Ch）--
    {
        Point2D pos(0x1C2 + 0x13B, 0x12C + 0x1C2);            // asm: var_144 / var_148
        box.X = shapeRect.X + 0x0C;                          // asm: &var_114[3]
        pDc->DrawText(armament + 8, &box, &pos, fore, 0, flags);
    }
    // -- 第 3 行：护甲（rect 槽 +8）--
    {
        Point2D pos(0x12C + 0x14A, 0x1C2 + 0x13B + 0x1C2);    // asm: var_148 / var_14C
        box.X = shapeRect.X + 0x08;                          // asm: &var_114[2]
        pDc->DrawText(armor + 4, &box, &pos, fore, 0, flags);
    }
    // -- 第 4 行：造价（rect 槽 +4）--
    {
        Point2D pos(0x12C + 0x1C2 + 0x159, 0x12C + 0x14A + 0x1C2);   // asm: var_150
        box.X = shapeRect.X + 0x04;                          // asm: &var_114[1]
        pDc->DrawText(cost, &box, &pos, fore, 0, flags);
    }
}
