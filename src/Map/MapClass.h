#pragma once

#include <Core/Definitions.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Abstract/AbstractClass.h>
#include <Math/CoordStruct.h>
#include <Map/CellClass.h>

class CRCEngine;
class IStream;

// ============================================================================
// MapClass - The game map manager, singleton
// ============================================================================
class MapClass : public AbstractClass {
public:
    static MapClass* Instance;

    MapClass();
    virtual ~MapClass() noexcept {}

    // AbstractClass overrides
    virtual AbstractType WhatAmI() const { return AbstractType::Map; }
    virtual int32 Size() const { return sizeof(MapClass); }
    virtual int32 GetArrayIndex() const { return 0; }
    virtual bool IsDead() const { return false; }
    virtual HRESULT GetClassID(CLSID* pClassID) override { return 0; }

    // Serialization
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL bSave) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // Init
    void Init(int32 maxX, int32 maxY);
    void Init_Clear();
    void Init_Theater(TheaterType theater);
    void Init_Cells();
    void Init_Waypoints();
    void Init_Shroud();
    bool Allocate_Cells(int32 maxX, int32 maxY);
    void Free_Cells();

    // Cell access
    CellClass* GetCellAt(const CoordStruct& coord);
    CellClass* GetCellAt(const CellStruct& cell);
    CellClass* GetCellAt(int32 x, int32 y);
    CellClass* GetCellAt(int32 cellIndex);
    CellClass* TryGetCellAt(int32 x, int32 y);
    bool IsValidCell(int32 x, int32 y) const;
    bool IsValidCell(int32 cellIndex) const;

    // Coordinate conversion
    int32 CoordToCell(const CoordStruct& coord) const;
    CoordStruct CellToCoord(int32 cellIndex) const;
    int32 GetCellX(int32 cellIndex) const;
    int32 GetCellY(int32 cellIndex) const;
    int32 XYToCell(int32 x, int32 y) const;
    CellStruct CellToCellStruct(int32 cellIndex) const;

    // Bounds
    bool IsWithinUsableArea(int32 x, int32 y) const;
    bool IsWithinUsableArea(int32 cellIndex) const;
    bool IsWithinUsableArea(const CoordStruct& coord) const;

    // Waypoints
    CoordStruct GetWaypoint(int32 idx) const;
    void SetWaypoint(int32 idx, const CoordStruct& coord);
    int32 ClosestWaypoint(const CoordStruct& coord) const;

    // Utility
    int32 GetRandomValidCell() const;
    CoordStruct Center_Coord() const;
    bool Is_Placement_Allowed(const CoordStruct& coord) const;
    bool Is_Placement_Allowed(const CellStruct& cell) const;

    // Cell terrain
    LandType GetLandType(const CellStruct& cell) const;
    int32 GetCellSlope(const CellStruct& cell) const;
    int32 GetGroundHeight(const CoordStruct& coord) const;
    void MarkCellOccupied(const CellStruct& cell, bool occupied);
    bool IsCellOccupied(const CellStruct& cell) const;
    ObjectClass* GetCellOccupier(const CellStruct& cell);

    // Bridge
    bool IsBridgeCell(const CellStruct& cell) const;
    bool IsBridgeDestroyed(const CellStruct& cell) const;

    // Damage
    void ApplyDamageArea(const DamageArea& area);
    void CreateCrater(const CellStruct& cell, int32 size);

    // Base zone
    bool Base_Is_Area_Occupied(int32 cellIndex, int32 radius) const;

    // Wall
    void Place_Wall(int32 x, int32 y, int32 overlayIndex);
    void Remove_Wall(int32 x, int32 y);

    // Tiberium
    void Update_Tiberium_Spread();

    // Crate
    void Update_Crate_Respawn();

    // Members
    int32       MapWidth;
    int32       MapHeight;
    int32       MapSize;
    int32       CellCount;
    CellClass*  CellArray;
    int32       MaxWaypoints;
    CoordStruct Waypoints[702];
    int32       CrateCount;
    int32       TotalValue;
    int32       VisibleRectX, VisibleRectY, VisibleRectWidth, VisibleRectHeight;
    TheaterType CurrentTheater;
    uint8*      Tilesets;
    int32       TilesetCount;
    int32       unknown_0x1EF8;
    int32       unknown_0x1EFC;
    int32       unknown_0x1F00;
    int32       unknown_0x1F04;
    int32       unknown_0x1F08;
    int32       unknown_0x1F0C;
    int32       unknown_0x1F10;
    int32       unknown_0x1F14;
    int32       unknown_0x1F18;
    int32       unknown_0x1F1C;
    int32       unknown_0x1F20;
    int32       unknown_0x1F24;
    int32       unknown_0x1F28;
    int32       unknown_0x1F2C;
    int32       unknown_0x1F30;
    int32       unknown_0x1F34;
    int32       unknown_0x1F38;
    int32       unknown_0x1F3C;
    int32       unknown_0x1F40;
    int32       unknown_0x1F44;
    int32       unknown_0x1F48;
    int32       unknown_0x1F4C;
    int32       unknown_0x1F50;
    int32       unknown_0x1F54;
    int32       unknown_0x1F58;
    int32       unknown_0x1F5C;
    int32       unknown_0x1F60;
    int32       unknown_0x1F64;
    int32       unknown_0x1F68;
    int32       unknown_0x1F6C;
    int32       unknown_0x1F70;
    int32       unknown_0x1F74;
    int32       unknown_0x1F78;
    int32       unknown_0x1F7C;
    int32       unknown_0x1F80;
    int32       unknown_0x1F84;
    int32       unknown_0x1F88;
    int32       unknown_0x1F8C;
    int32       unknown_0x1F90;
    int32       unknown_0x1F94;
    int32       unknown_0x1F98;
    int32       unknown_0x1F9C;
    int32       unknown_0x1FA0;
    int32       unknown_0x1FA4;
    int32       unknown_0x1FA8;
    int32       unknown_0x1FAC;
    int32       unknown_0x1FB0;
    int32       unknown_0x1FB4;
    int32       unknown_0x1FB8;
    int32       unknown_0x1FBC;
    int32       unknown_0x1FC0;
    int32       unknown_0x1FC4;
    int32       unknown_0x1FC8;
    int32       unknown_0x1FCC;
    int32       unknown_0x1FD0;
    int32       unknown_0x1FD4;
    int32       unknown_0x1FD8;
    int32       unknown_0x1FDC;
    int32       unknown_0x1FE0;
    int32       unknown_0x1FE4;
    int32       unknown_0x1FE8;
    int32       unknown_0x1FEC;
    int32       unknown_0x1FF0;
    int32       unknown_0x1FF4;
    int32       unknown_0x1FF8;
    int32       unknown_0x1FFC;
    int32       unknown_0x2000;
    int32       unknown_0x2004;
    int32       unknown_0x2008;
    int32       unknown_0x200C;
    int32       unknown_0x2010;
    int32       unknown_0x2014;
    int32       unknown_0x2018;
    int32       unknown_0x201C;
    int32       unknown_0x2020;
    int32       unknown_0x2024;
    int32       unknown_0x2028;
    int32       unknown_0x202C;
    int32       unknown_0x2030;
    int32       unknown_0x2034;
    int32       unknown_0x2038;
    int32       unknown_0x203C;
    int32       unknown_0x2040;
    int32       unknown_0x2044;
    int32       unknown_0x2048;
    int32       unknown_0x204C;
    int32       unknown_0x2050;
    int32       unknown_0x2054;
    int32       unknown_0x2058;
    int32       unknown_0x205C;
    int32       unknown_0x2060;
    int32       unknown_0x2064;
    int32       unknown_0x2068;
    int32       unknown_0x206C;
    int32       unknown_0x2070;
    int32       unknown_0x2074;
    int32       unknown_0x2078;
    int32       unknown_0x207C;
    int32       unknown_0x2080;
    int32       unknown_0x2084;
    int32       unknown_0x2088;
    int32       unknown_0x208C;
    int32       unknown_0x2090;
    int32       unknown_0x2094;
    int32       unknown_0x2098;
    int32       unknown_0x209C;
    int32       unknown_0x20A0;
    int32       unknown_0x20A4;
    int32       unknown_0x20A8;
    int32       unknown_0x20AC;
    int32       unknown_0x20B0;
    int32       unknown_0x20B4;
    int32       unknown_0x20B8;
    int32       unknown_0x20BC;
    int32       unknown_0x20C0;
    int32       unknown_0x20C4;
    int32       unknown_0x20C8;
    int32       unknown_0x20CC;
    int32       unknown_0x20D0;
    int32       unknown_0x20D4;
    int32       unknown_0x20D8;
    int32       unknown_0x20DC;
    int32       unknown_0x20E0;
    int32       unknown_0x20E4;
    int32       unknown_0x20E8;
    int32       unknown_0x20EC;
    int32       unknown_0x20F0;
    int32       unknown_0x20F4;
    int32       unknown_0x20F8;
    int32       unknown_0x20FC;
    int32       unknown_0x2100;
    int32       unknown_0x2104;
    int32       unknown_0x2108;
    int32       unknown_0x210C;
    int32       unknown_0x2110;
    int32       unknown_0x2114;
    int32       unknown_0x2118;
    int32       unknown_0x211C;
    int32       unknown_0x2120;
    int32       unknown_0x2124;
    int32       unknown_0x2128;
    int32       unknown_0x212C;
    int32       unknown_0x2130;
    int32       unknown_0x2134;
    int32       unknown_0x2138;
    int32       unknown_0x213C;
    int32       unknown_0x2140;
    int32       unknown_0x2144;
    int32       unknown_0x2148;
    int32       unknown_0x214C;
    int32       unknown_0x2150;
    int32       unknown_0x2154;
    int32       unknown_0x2158;
    int32       unknown_0x215C;
    int32       unknown_0x2160;
    int32       unknown_0x2164;
    int32       unknown_0x2168;
    int32       unknown_0x216C;
    int32       unknown_0x2170;
    int32       unknown_0x2174;
    int32       unknown_0x2178;
    int32       unknown_0x217C;
    int32       unknown_0x2180;
    int32       unknown_0x2184;
    int32       unknown_0x2188;
    int32       unknown_0x218C;
    int32       unknown_0x2190;
    int32       unknown_0x2194;
    int32       unknown_0x2198;
    int32       unknown_0x219C;

    // Padding to match original binary layout
    // The original MapClass has a large gap of unknown members
    uint8       _unused_padding[0x456C - 0x21A0];
};