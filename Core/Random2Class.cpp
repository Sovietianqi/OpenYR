#include "Random2Class.h"

// ============================================================================
// Random2Class.cpp - lagged XOR generator used by the whole game
//
//   The generator the binary actually runs is a 250-slot lagged combination:
//   operator() advances IndexA and IndexB, folds the two table slots together
//   with XOR and stores the result back, so the table is continuously
//   re-mixed rather than merely cycled.  That makes the stream depend on every
//   draw, which is why gameplay-critical code and cosmetic code use separate
//   instances - a purely visual draw must never perturb the synchronised
//   stream.
// ============================================================================

// The non-critical generator is a plain global object, mirroring the
// binary's "NonCriticalRandomNumber" instance at 0x87F2A0.
Random2Class NonCriticalRandomNumber;

// ----------------------------------------------------------------------------
// Random2Class::Seed
//
//   Fills the table with a linear congruential walk keyed off the seed, then
//   resets both cursors.  Allowing a zero seed keeps the generator usable for
//   the "unseeded" replay case, where the draw sequence then depends purely on
//   the table contents.
// ----------------------------------------------------------------------------
void Random2Class::Seed(int32 seed)
{
    uint32 state = static_cast<uint32>(seed);

    for (int32 i = 0; i < RANDOM2_TABLE_SIZE; ++i) {
        state = state * 1103515245u + 12345u;
        Table[i] = static_cast<int32>(state);
    }

    IndexA = 0;
    IndexB = 0;
    Initialized = true;
}

// ----------------------------------------------------------------------------
// Random2Class::operator()
//
//   The binary bails out immediately when the byte at +0 is zero, leaving the
//   raw value 0 in eax.  Otherwise it performs the two-slot XOR and returns
//   the table slot that was advanced first.
// ----------------------------------------------------------------------------
int32 Random2Class::operator()()
{
    if (!Initialized) {
        return 0;
    }

    int32 a = IndexA;
    int32 b = IndexB;

    const int32 combined = Table[a] ^ Table[b];
    Table[a] = combined;

    ++a;
    ++b;
    if (a >= RANDOM2_TABLE_SIZE) {
        a = 0;
    }
    if (b >= RANDOM2_TABLE_SIZE) {
        b = 0;
    }

    IndexA = a;
    IndexB = b;

    return combined;
}

// ----------------------------------------------------------------------------
// Random2Class::operator()(min, max)
//
//   The binary draws one raw value and maps it onto the inclusive range.  A
//   degenerate range collapses to min, matching the original's behaviour when
//   max <= min.
// ----------------------------------------------------------------------------
int32 Random2Class::operator()(int32 min, int32 max)
{
    if (max <= min) {
        return min;
    }

    const int32 span = max - min + 1;
    const uint32 draw = static_cast<uint32>((*this)());
    const int32 offset = static_cast<int32>(draw % static_cast<uint32>(span));

    return min + offset;
}
