#pragma once

#include <cstdint>

// ============================================================================
// GUID - standard COM-style globally unique identifier
//
// The uint32/uint16/uint8 aliases below mirror those declared in
// Core/Definitions.h. Redeclaring an identical alias is permitted by C++, so
// this header remains self-contained and also composes correctly when
// Core/Definitions.h (which includes this file) is processed first.
// ============================================================================
using uint32 = uint32_t;
using uint16 = uint16_t;
using uint8  = uint8_t;

struct GUID {
    uint32 Data1;
    uint16 Data2;
    uint16 Data3;
    uint8  Data4[8];

    bool operator==(const GUID& other) const {
        if (Data1 != other.Data1) return false;
        if (Data2 != other.Data2) return false;
        if (Data3 != other.Data3) return false;
        for (int i = 0; i < 8; ++i) {
            if (Data4[i] != other.Data4[i]) return false;
        }
        return true;
    }

    bool operator!=(const GUID& other) const {
        return !(*this == other);
    }
};
