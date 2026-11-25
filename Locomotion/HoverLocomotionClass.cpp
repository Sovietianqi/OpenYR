// =============================================================================
// HoverLocomotionClass.cpp - 悬浮移位面板
// =============================================================================

#include "HoverLocomotionClass.h"
#include "../Abstract/FootClass.h"

CoordStruct HoverLocomotionClass::Default_Pos(0, 0, 0);

HoverLocomotionClass::HoverLocomotionClass()
    : LocomotionClass()
    , Position(0, 0, 0)
    , HoverHeight(0)
    , IsHovering(false)
{
}

HoverLocomotionClass::~HoverLocomotionClass()
{
}

// asm: HoverLocomotionClass_GetClassID (0x517070, retn 8)
//   判空失败返回 80004003h (E_POINTER)，否则拷出 CLSID_HoverLocomotionClass
//   = 4A582742-9839-11D1-B709-00A024DDAFD1。
HRESULT HoverLocomotionClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;

    *pClassID = CLSIDs::Get(LocoID);
    return S_OK;
}

int32 HoverLocomotionClass::Size()
{
    return static_cast<int32>(sizeof(HoverLocomotionClass));
}

bool HoverLocomotionClass::Is_Moving()
{
    return IsHovering;
}

CoordStruct HoverLocomotionClass::Destination()
{
    return Position;
}

bool HoverLocomotionClass::Process()
{
    // 根据游戏行为，可知悬浮移位没有逐帧位移：悬浮态直接到位。
    IsHovering = false;
    return true;
}

void HoverLocomotionClass::Move_To(CoordStruct to)
{
    Position = to;
    IsHovering = true;
}

void HoverLocomotionClass::Stop_Moving()
{
    IsHovering = false;
}

void HoverLocomotionClass::Do_Turn(DirStruct coord)
{
    // 根据游戏行为，可知悬浮体转向由载体的朝向机承担。
    if (LinkedTo)
        LinkedTo->SetFacing(coord);
}

void HoverLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    (void)mark;
}

void HoverLocomotionClass::Limbo()
{
    IsHovering = false;
    Position = Default_Pos;
}

int32 HoverLocomotionClass::Get_Status() const
{
    return IsHovering ? 1 : 0;
}

FireError HoverLocomotionClass::Can_Fire() const
{
    return FireError::OK;
}

bool HoverLocomotionClass::Is_Really_Moving_Now() const
{
    return IsHovering;
}

void HoverLocomotionClass::ILocomotion_GetCoords(CoordStruct* pOut) const
{
    if (!pOut)
        return;
    if (Position.X != Default_Pos.X || Position.Y != Default_Pos.Y || Position.Z != Default_Pos.Z)
        *pOut = Position;
    else if (LinkedTo)
        LinkedTo->GetCoords(pOut);
    else
        *pOut = CoordStruct(0, 0, 0);
}
