#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

#include "FileSystem.h"

class CCFileClass;
class CCINIClass;

class CRC {
public:
    static uint32 Calculate(const void* data, int32 size) {
        uint32 crc = 0xFFFFFFFF;
        const uint8* bytes = static_cast<const uint8*>(data);
        for (int32 i = 0; i < size; ++i) {
            crc ^= bytes[i];
            for (int32 j = 0; j < 8; ++j) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc ^ 0xFFFFFFFF;
    }
};

class CRCEngine {
public:
    uint32 Value;

    CRCEngine() : Value(0xFFFFFFFF) {}

    void AddByte(uint8 byte) {
        Value ^= byte;
        for (int32 j = 0; j < 8; ++j) {
            if (Value & 1) {
                Value = (Value >> 1) ^ 0xEDB88320;
            } else {
                Value >>= 1;
            }
        }
    }

    void AddData(const void* data, int32 size) {
        const uint8* bytes = static_cast<const uint8*>(data);
        for (int32 i = 0; i < size; ++i) {
            AddByte(bytes[i]);
        }
    }

    uint32 GetCRC() const {
        return Value ^ 0xFFFFFFFF;
    }
};