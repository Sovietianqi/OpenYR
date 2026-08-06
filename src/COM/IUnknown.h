#pragma once

#include <cstddef>
#include <utility>

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Math/Matrix3D.h"

typedef void* LPVOID;
typedef void* PVOID;

class IUnknown {
public:
    virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppvObject) {
        if (ppvObject) *ppvObject = nullptr;
        return E_FAIL;
    }
    virtual ULONG AddRef() {
        RefCount++;
        return RefCount;
    }
    virtual ULONG Release() {
        if (RefCount > 0) RefCount--;
        return RefCount;
    }

    ULONG RefCount;
    IUnknown() : RefCount(1) {}
    virtual ~IUnknown() = default;
};

// ============================================================================
// IStream - minimal COM stream interface for serialization.
//
//  The original game uses the real COM IStream for save/load.  The
//  standalone build provides this minimal definition with the Read/Write
//  methods that the engine's serialization code calls.
// ============================================================================

class IStream : public IUnknown {
public:
    virtual HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead) { return E_NOTIMPL; }
    virtual HRESULT Write(const void* pv, ULONG cb, ULONG* pcbWritten) { return E_NOTIMPL; }
    virtual HRESULT Seek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition) { return E_NOTIMPL; }
    virtual HRESULT SetSize(uint64 libNewSize) { return E_NOTIMPL; }
    virtual HRESULT Commit(DWORD grfCommitFlags) { return E_NOTIMPL; }
    virtual HRESULT Revert() { return E_NOTIMPL; }
};

class IPersist : public IUnknown {
public:
    virtual HRESULT GetClassID(CLSID* pClassID) = 0;
};

class IPersistStream : public IPersist {
public:
    virtual HRESULT IsDirty() { return 0; }
    virtual HRESULT Load(IStream* pStm) = 0;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) = 0;
    virtual HRESULT GetSizeMax(uint64* pcbSize) { return 0; }
};

class INoticeSink {
public:
    virtual bool INoticeSink_Unknown(DWORD dwUnknown) { return false; }
};

class INoticeSource {
public:
    virtual void INoticeSource_Unknown() {}
};

class IRTTITypeInfo : public IUnknown {
public:
    virtual AbstractType What_Am_I() const = 0;
    virtual int32 Fetch_ID() const { return 0; }
    virtual void Create_ID() {}
};

class ILocomotion : public IUnknown {
public:
    virtual HRESULT Link_To_Object(void* pointer) { return S_OK; }
    virtual bool Is_Moving() { return false; }
    virtual CoordStruct Destination() { return CoordStruct(); }
    virtual CoordStruct Head_To_Coord() { return CoordStruct(); }
    virtual Move Can_Enter_Cell(CellStruct cell) { return Move::OK; }
    virtual bool Is_To_Have_Shadow() { return true; }
    virtual Matrix3D Draw_Matrix(VoxelIndexKey* pIndex) { return Matrix3D(); }
    virtual Matrix3D Shadow_Matrix(VoxelIndexKey* pIndex) { return Matrix3D(); }
    virtual Point2D Draw_Point() { return Point2D(); }
    virtual Point2D Shadow_Point() { return Point2D(); }
    virtual VisualType Visual_Character(bool raw) { return VisualType::Normal; }
    virtual int32 Z_Adjust() { return 0; }
    virtual ZGradient Z_Gradient() { return ZGradient::None; }
    virtual bool Process() { return false; }
    virtual void Move_To(CoordStruct to) {}
    virtual void Stop_Moving() {}
    virtual void Do_Turn(DirStruct coord) {}
    virtual void Unlimbo() {}
    virtual void Tilt_Pitch_AI() {}
    virtual bool Power_On() { return true; }
    virtual bool Power_Off() { return true; }
    virtual bool Is_Powered() { return true; }
    virtual bool Is_Ion_Sensitive() { return false; }
    virtual bool Push(DirStruct dir) { return false; }
    virtual bool Shove(DirStruct dir) { return false; }
    virtual void Force_Track(int32 track, CoordStruct coord) {}
    virtual Layer In_Which_Layer() = 0;
    virtual void Force_Immediate_Destination(CoordStruct coord) {}
    virtual void Force_New_Slope(int32 ramp) {}
    virtual bool Is_Moving_Now() { return false; }
    virtual int32 Apparent_Speed() { return 0; }
    virtual int32 Drawing_Code() { return 0; }
    virtual FireError Can_Fire() { return FireError::OK; }
    virtual int32 Get_Status() { return 0; }
    virtual void Acquire_Hunter_Seeker_Target() {}
    virtual bool Is_Surfacing() { return false; }
    virtual void Mark_All_Occupation_Bits(MarkType mark) {}
    virtual bool Is_Moving_Here(CoordStruct to) { return false; }
    virtual bool Will_Jump_Tracks() { return false; }
    virtual bool Is_Really_Moving_Now() { return false; }
    virtual void Stop_Movement_Animation() {}
    virtual void Limbo() {}
    virtual void Lock() {}
    virtual void Unlock() {}
    virtual int32 Get_Track_Number() { return 0; }
    virtual int32 Get_Track_Index() { return 0; }
    virtual int32 Get_Speed_Accum() { return 0; }
};

class IPiggyback : public IUnknown {
public:
    virtual HRESULT Begin_Piggyback(ILocomotion* pointer) { return S_OK; }
    virtual HRESULT End_Piggyback(ILocomotion** pointer) { return S_OK; }
    virtual bool Is_Ok_To_End() { return true; }
    virtual HRESULT Piggyback_CLSID(GUID* classid) { return S_OK; }
    virtual bool Is_Piggybacking() { return false; }
};

class IFlyControl : public IUnknown {
public:
    virtual int32 __stdcall Landing_Altitude() { return 0; }
    virtual int32 __stdcall Landing_Direction() { return 0; }
    virtual LONG __stdcall Is_Loaded() { return 0; }
    virtual LONG __stdcall Is_Strafe() { return 0; }
    virtual LONG __stdcall Is_Fighter() { return 0; }
    virtual LONG __stdcall Is_Locked() { return 0; }
};