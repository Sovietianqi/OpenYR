#pragma once

// ============================================================================
// IsometricTileGlobals.h - the special tile-set ordinals
//
//  IsometricTileTypeClass_CreateFromINIList reads a fixed block of keys from
//  the theater [General] section and remembers, for each one, the tile-set
//  ordinal whose SetName matches the key's value.  The rest of the engine then
//  asks "which tile set is the water set?" through these globals rather than
//  re-searching by name.
//
//  Every one of them defaults to -1 (meaning "no such set in this theater")
//  except DestroyableCliffs, which the assembly seeds with -2 so that the
//  negative test used by the cliff code stays true even before the tileset INI
//  is read.
// ============================================================================

#include <Core/Definitions.h>

extern int32 tile_RampBase;
extern int32 tile_RampSmooth;
extern int32 tile_MMRampBase;
extern int32 tile_ClearTile;
extern int32 tile_RoughTile;
extern int32 tile_SandTile;
extern int32 tile_GreenTile;
extern int32 tile_PaveTile;
extern int32 tile_MiscPaveTile;
extern int32 tile_ClearToRoughLat;
extern int32 tile_ClearToSandLat;
extern int32 tile_ClearToGreenLat;
extern int32 tile_ClearToPaveLat;
extern int32 tile_HeightBase;
extern int32 tile_BlackTile;
extern int32 tile_BridgeSet;
extern int32 tile_WoodBridgeSet;
extern int32 tile_CliffSet;
extern int32 tile_ShorePieces;
extern int32 tile_WaterSet;
extern int32 tile_SlopeSetPieces;
extern int32 tile_SlopeSetPieces2;
extern int32 tile_MonorailSlopes;
extern int32 tile_Tunnels;
extern int32 tile_TrackTunnels;
extern int32 tile_DirtTunnels;
extern int32 tile_DirtTrackTunnels;
extern int32 tile_WaterfallEast;
extern int32 tile_WaterfallWest;
extern int32 tile_WaterfallNorth;
extern int32 tile_WaterfallSouth;
extern int32 tile_CliffRamps;
extern int32 tile_PavedRoads;
extern int32 tile_PavedRoadEnds;
extern int32 tile_Medians;
extern int32 tile_RoughGround;
extern int32 tile_DirtRoadJunction;
extern int32 tile_DirtRoadCurve;
extern int32 tile_DirtRoadStraight;
extern int32 tile_DestroyableCliffs;
extern int32 tile_WaterCaves;
extern int32 tile_WaterCliffs;
extern int32 tile_PavedRoadSlopes;
extern int32 tile_DirtRoadSlopes;
extern int32 tile_Rocks;
extern int32 tile_WaterBridge;
extern int32 tile_BridgeTopLeft1;
extern int32 tile_BridgeTopLeft2;
extern int32 tile_BridgeBottomRight1;
extern int32 tile_BridgeBottomRight2;
extern int32 tile_BridgeTopRight1;
extern int32 tile_BridgeTopRight2;
extern int32 tile_BridgeBottomLeft1;
extern int32 tile_BridgeBottomLeft2;
extern int32 tile_BridgeMiddle1;
extern int32 tile_BridgeMiddle2;

// Resets every ordinal to its not-found default.  The assembly performs the
// same store sequence at the head of IsometricTileTypeClass_CreateFromINIList,
// before it reads the [General] block.
void IsometricTileGlobals_Reset();
