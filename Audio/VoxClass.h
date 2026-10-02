#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

class CCINIClass;

// ============================================================================
// VoxClass - one EVA / speech line.
//
//   The original keeps a fixed pool of VoxClass objects, one per entry of the
//   [DialogList] section in ra2md.ini.  Each object is 0x54 bytes:
//
//     +00  ID[0x28]        the INI key name (the dialog identifier)
//     +28  Volume          float, default 1.0
//     +2C  Yuri[9]         the Yuri-side sample name
//     +35  Russian[9]      the Soviet-side sample name
//     +3E  Allied[9]       the Allied-side sample name
//     +47  (pad)
//     +48  Priority        int, default 1 (0 LOW, 1 NORMAL, 2 IMPORTANT, 3 CRITICAL)
//     +4C  Type            int, default 0 (0 STANDARD, 1 QUEUE,
//                                          2 INTERRUPT, 3 QUEUED_INTERRUPT)
//     +50  field_50        int, default 2
//
//   The three sample names hold at most eight characters plus a terminator,
//   which the original writes with a plain strncpy(..., 9).
// ============================================================================

class VoxClass
{
public:
    // Priority tokens the "Priority" key accepts.
    enum PriorityType : int32 {
        Priority_Low       = 0,
        Priority_Normal    = 1,
        Priority_Important = 2,
        Priority_Critical  = 3
    };

    // Type tokens the "Type" key accepts.
    enum VoxType : int32 {
        VoxType_Standard         = 0,
        VoxType_Queue            = 1,
        VoxType_Interrupt        = 2,
        VoxType_QueuedInterrupt  = 3
    };

    VoxClass();
    ~VoxClass();

    // VoxClass_LoadFromINI - reads the section named after this object's ID.
    bool LoadFromINI(CCINIClass* pINI);

    // Reads a single EVA line.  Kept as a free-standing helper so the list
    // walker can reuse it for both the reserved and the allocated objects.
    void Read(CCINIClass* pINI);

    // VoxClass_CreateFromINIList - walks [DialogList], allocating one object
    // per dialog identifier that is not already registered.
    static void CreateFromINIList(CCINIClass* pINI);

    // ── +00 ──────────────────────────────────────────────────────────────
    char    ID[0x28];

    // ── +28 ──────────────────────────────────────────────────────────────
    float   Volume;

    // ── +2C ──────────────────────────────────────────────────────────────
    char    Yuri[9];        // +2C
    char    Russian[9];     // +35
    char    Allied[9];      // +3E

    // ── +48 ──────────────────────────────────────────────────────────────
    int32   Priority;
    int32   Type;
    int32   field_50;       // default 2

    // ── Global dialog list ───────────────────────────────────────────────
    static DynamicVectorClass<VoxClass*>* Array;
    static VoxClass* Find(const char* pID);
    static void Clear();
};
