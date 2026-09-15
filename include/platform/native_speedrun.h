#ifndef NATIVE_SPEEDRUN_H
#define NATIVE_SPEEDRUN_H

#include <macros.h>

// Pure speedrun state machine. It has no engine, platform or SDL dependency:
// callers decode one engine frame into struct NativeSpeedrunFrame and read back
// the clocks and emitted events. This is the single test seam for the feature.

#define NATIVE_SPEEDRUN_ABI_VERSION 1u

// gameMode1 bits. Values mirror include/namespace_Main.h; kept local so the
// module stays free of the game headers.
#define NATIVE_SPEEDRUN_GM_PAUSE_ALL      0x0000000fu
#define NATIVE_SPEEDRUN_GM_START_OF_RACE  0x00000040u
#define NATIVE_SPEEDRUN_GM_ADVENTURE      0x00080000u
#define NATIVE_SPEEDRUN_GM_END_OF_RACE    0x00200000u
#define NATIVE_SPEEDRUN_GM_LOADING        0x40000000u
#define NATIVE_SPEEDRUN_GM_ADVENTURE_BOSS 0x80000000u

// include/namespace_Main.h
#define NATIVE_SPEEDRUN_LOAD_IDLE (-1)

// include/namespace_Level.h, the starting adventure hub.
#define NATIVE_SPEEDRUN_HUB_N_SANITY_BEACH 0x1a

#define NATIVE_SPEEDRUN_MAX_SPLITS 64
#define NATIVE_SPEEDRUN_NAME_MAX   32

enum NativeSpeedrunEventType
{
	NATIVE_SPEEDRUN_EVENT_NONE = 0,
	NATIVE_SPEEDRUN_EVENT_RUN_START,
	NATIVE_SPEEDRUN_EVENT_SPLIT,
	NATIVE_SPEEDRUN_EVENT_RUN_END,
	NATIVE_SPEEDRUN_EVENT_RESET,
};

enum NativeSpeedrunSplitKind
{
	NATIVE_SPEEDRUN_SPLIT_NORMAL = 0,
	NATIVE_SPEEDRUN_SPLIT_BOSS,
};

struct NativeSpeedrunSplit
{
	s32 levelID;
	s32 kind; // enum NativeSpeedrunSplitKind
	char name[NATIVE_SPEEDRUN_NAME_MAX];
};

struct NativeSpeedrunRoute
{
	struct NativeSpeedrunSplit splits[NATIVE_SPEEDRUN_MAX_SPLITS];
	s32 count;
};

// One engine frame as observed by the integration layer.
struct NativeSpeedrunFrame
{
	s32 mainGameState;  // sdata->mainGameState
	s32 loadStage;      // sdata->Loading.stage
	u32 gameMode1;      // gGT->gameMode1
	s32 levelID;        // gGT->levelID
	u32 elapsedTimeMS;  // gGT->elapsedTimeMS
	u64 wallTimeMS;     // host monotonic clock in milliseconds
	s32 numPlayers;     // gGT->numPlyrCurrGame
	s32 demoMode;       // gGT->boolDemoMode
	s32 playerFinished; // P1 actionsFlagSet & ACTION_RACE_FINISHED
};

struct NativeSpeedrunEvent
{
	u32 sequence;
	u32 type; // enum NativeSpeedrunEventType
	s32 levelID;
	u32 gameMode1;
	s32 segmentIndex; // for SPLIT and RUN_END: the split that completed
	s32 finishPosition;
	u32 segmentTimeMS;
	u32 totalTimeMS;
};

// Identifies the surface block so a consumer can locate and version-check it.
#define NATIVE_SPEEDRUN_SURFACE_MAGIC 0x43545253u

enum NativeSpeedrunSurfaceFlag
{
	NATIVE_SPEEDRUN_SURFACE_ACTIVE = 1u << 0,
	NATIVE_SPEEDRUN_SURFACE_FINISHED = 1u << 1,
	NATIVE_SPEEDRUN_SURFACE_PAUSED = 1u << 2,
};

// Read-only snapshot a timer or verifier consumes. Fixed layout, no pointers.
struct NativeSpeedrunSurface
{
	u32 magic;
	u32 abiVersion;
	u32 sequence;
	u32 flags; // enum NativeSpeedrunSurfaceFlag
	s32 segmentIndex;
	u32 loadlessMS;
	u32 rtaMS;
	s32 lastEventType; // enum NativeSpeedrunEventType
	s32 lastEventLevelID;
	u32 lastEventGameMode1;
	s32 lastEventSegmentIndex;
	u32 lastEventSequence;
	u32 lastEventSegmentTimeMS;
	u32 lastEventTotalTimeMS;
};

struct NativeSpeedrunState
{
	u32 abiVersion;
	u32 sequence;
	u32 active;
	u32 finished;
	u32 paused;
	s32 segmentIndex;
	u32 loadlessMS;
	u32 rtaMS;
	struct NativeSpeedrunEvent lastEvent;

	// Route and integration bookkeeping. Do not read directly.
	struct NativeSpeedrunRoute route;
	u64 runStartWallMS;
	u64 prevWallMS;
	u32 havePrevWall;
	s32 prevPlayerFinished;
	u32 lastSplitMS;
};

// Prepares the state for a route. Copies the route by value.
void NativeSpeedrun_Reset(struct NativeSpeedrunState *state, const struct NativeSpeedrunRoute *route);

// Advances one frame. Emits at most one event into state->lastEvent per call.
void NativeSpeedrun_Update(struct NativeSpeedrunState *state, const struct NativeSpeedrunFrame *frame);

// Copies the externally visible state into the fixed-layout surface block.
void NativeSpeedrun_WriteSurface(const struct NativeSpeedrunState *state, struct NativeSpeedrunSurface *out);

// Formats state->lastEvent as one text line, including the loadless and RTA
// values. Returns the number of bytes written excluding the null terminator, or
// -1 if the buffer is too small.
int NativeSpeedrun_FormatEvent(const struct NativeSpeedrunState *state, char *buf, u32 size);

// Parses a line-based route config. Lines are "<levelID> <kind> <name>", where
// kind is "normal" or "boss"; blank lines and lines starting with '#' are
// ignored. Returns the split count, or -1 on error with a message in err.
int NativeSpeedrun_ParseRoute(const char *text, struct NativeSpeedrunRoute *out, char *err, u32 errSize);

#endif
