#include <Objects/IsometricTileGlobals.h>

#include <cstring>

// ============================================================================
// The special tile-set ordinals
//
// IsometricTileTypeClass_CreateFromINIList holds the requested SetName for
// each of these in a local, reads every [TileSet%04d] in turn, and stores the
// ordinal into the matching global whenever the section's SetName is equal.
// The globals therefore carry the "which tile set is the water/bridge/... set"
// answer for the entire engine.
// ============================================================================

int32 tile_RampBase = -1;
int32 tile_RampSmooth = -1;
int32 tile_MMRampBase = -1;
int32 tile_ClearTile = -1;
int32 tile_RoughTile = -1;
int32 tile_SandTile = -1;
int32 tile_GreenTile = -1;
int32 tile_PaveTile = -1;
int32 tile_MiscPaveTile = -1;
int32 tile_ClearToRoughLat = -1;
int32 tile_ClearToSandLat = -1;
int32 tile_ClearToGreenLat = -1;
int32 tile_ClearToPaveLat = -1;
int32 tile_HeightBase = -1;
int32 tile_BlackTile = -1;
int32 tile_BridgeSet = -1;
int32 tile_WoodBridgeSet = -1;
int32 tile_CliffSet = -1;
int32 tile_ShorePieces = -1;
int32 tile_WaterSet = -1;
int32 tile_SlopeSetPieces = -1;
int32 tile_SlopeSetPieces2 = -1;
int32 tile_MonorailSlopes = -1;
int32 tile_Tunnels = -1;
int32 tile_TrackTunnels = -1;
int32 tile_DirtTunnels = -1;
int32 tile_DirtTrackTunnels = -1;
int32 tile_WaterfallEast = -1;
int32 tile_WaterfallWest = -1;
int32 tile_WaterfallNorth = -1;
int32 tile_WaterfallSouth = -1;
int32 tile_CliffRamps = -1;
int32 tile_PavedRoads = -1;
int32 tile_PavedRoadEnds = -1;
int32 tile_Medians = -1;
int32 tile_RoughGround = -1;
int32 tile_DirtRoadJunction = -1;
int32 tile_DirtRoadCurve = -1;
int32 tile_DirtRoadStraight = -1;
int32 tile_DestroyableCliffs = -2;
int32 tile_WaterCaves = -1;
int32 tile_WaterCliffs = -1;
int32 tile_PavedRoadSlopes = -1;
int32 tile_DirtRoadSlopes = -1;
int32 tile_Rocks = -1;
int32 tile_WaterBridge = -1;
int32 tile_BridgeTopLeft1 = -1;
int32 tile_BridgeTopLeft2 = -1;
int32 tile_BridgeBottomRight1 = -1;
int32 tile_BridgeBottomRight2 = -1;
int32 tile_BridgeTopRight1 = -1;
int32 tile_BridgeTopRight2 = -1;
int32 tile_BridgeBottomLeft1 = -1;
int32 tile_BridgeBottomLeft2 = -1;
int32 tile_BridgeMiddle1 = -1;
int32 tile_BridgeMiddle2 = -1;

void IsometricTileGlobals_Reset()
{
    tile_RampBase = -1;
    tile_RampSmooth = -1;
    tile_MMRampBase = -1;
    tile_ClearTile = -1;
    tile_RoughTile = -1;
    tile_SandTile = -1;
    tile_GreenTile = -1;
    tile_PaveTile = -1;
    tile_MiscPaveTile = -1;
    tile_ClearToRoughLat = -1;
    tile_ClearToSandLat = -1;
    tile_ClearToGreenLat = -1;
    tile_ClearToPaveLat = -1;
    tile_HeightBase = -1;
    tile_BlackTile = -1;
    tile_BridgeSet = -1;
    tile_WoodBridgeSet = -1;
    tile_CliffSet = -1;
    tile_ShorePieces = -1;
    tile_WaterSet = -1;
    tile_SlopeSetPieces = -1;
    tile_SlopeSetPieces2 = -1;
    tile_MonorailSlopes = -1;
    tile_Tunnels = -1;
    tile_TrackTunnels = -1;
    tile_DirtTunnels = -1;
    tile_DirtTrackTunnels = -1;
    tile_WaterfallEast = -1;
    tile_WaterfallWest = -1;
    tile_WaterfallNorth = -1;
    tile_WaterfallSouth = -1;
    tile_CliffRamps = -1;
    tile_PavedRoads = -1;
    tile_PavedRoadEnds = -1;
    tile_Medians = -1;
    tile_RoughGround = -1;
    tile_DirtRoadJunction = -1;
    tile_DirtRoadCurve = -1;
    tile_DirtRoadStraight = -1;
    tile_DestroyableCliffs = -2;
    tile_WaterCaves = -1;
    tile_WaterCliffs = -1;
    tile_PavedRoadSlopes = -1;
    tile_DirtRoadSlopes = -1;
    tile_Rocks = -1;
    tile_WaterBridge = -1;
    tile_BridgeTopLeft1 = -1;
    tile_BridgeTopLeft2 = -1;
    tile_BridgeBottomRight1 = -1;
    tile_BridgeBottomRight2 = -1;
    tile_BridgeTopRight1 = -1;
    tile_BridgeTopRight2 = -1;
    tile_BridgeBottomLeft1 = -1;
    tile_BridgeBottomLeft2 = -1;
    tile_BridgeMiddle1 = -1;
    tile_BridgeMiddle2 = -1;
}
