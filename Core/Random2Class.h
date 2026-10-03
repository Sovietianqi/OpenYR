#pragma once

#include "Definitions.h"

// ============================================================================
// Random2Class - the game's secondary pseudo-random generator
//
//   The original implements a 250-slot lagged Fibonacci / XOR combination
//   generator.  The object layout is:
//
//      +0x00  bool  Initialized    (when zero, operator() returns 0 and does
//                                   not advance the state)
//      +0x04  int   IndexA
//      +0x08  int   IndexB
//      +0x0C  int   Table[250]
//
//   operator() advances both indices, XORs Table[IndexA] into Table[IndexB],
//   then wraps each index at 250.  operator()(min, max) maps the raw draw onto
//   the inclusive range [min, max].
// ============================================================================

class Random2Class
{
public:
    static constexpr int32 RANDOM2_TABLE_SIZE = 250;  // 0xFA

    bool  Initialized;                  // +0x00
    int32 IndexA;                       // +0x04
    int32 IndexB;                       // +0x08
    int32 Table[RANDOM2_TABLE_SIZE];    // +0x0C

    Random2Class() noexcept
        : Initialized(false)
        , IndexA(0)
        , IndexB(0)
    {
        for (int32 i = 0; i < RANDOM2_TABLE_SIZE; ++i) {
            Table[i] = 0;
        }
    }

    // Seeds the generator and marks it live.  The original fills the table
    // from the supplied seed with a linear congruential walk before letting
    // the lagged combination take over.
    void Seed(int32 seed);

    // Raw draw.  Returns 0 (without touching the state) while unseeded.
    int32 operator()();

    // Inclusive-range draw - Random2Class::operator()(min, max).
    int32 operator()(int32 min, int32 max);
};

// The game keeps two independent generators: the synchronised one used by
// gameplay code (stored inside ScenarioClass at +0x218) and the so-called
// non-critical generator used by purely cosmetic code paths.
extern Random2Class NonCriticalRandomNumber;
