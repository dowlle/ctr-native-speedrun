// Game-side speedrun integration. Only compiled into the unity build when
// CTR_SPEEDRUN is defined; it is not part of the host test target.

#include <platform/native_speedrun.h>
#include <platform/native_speedrun_runtime.h>

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIVE_SPEEDRUN_ROUTE_FILE    "config/any-nmg.cfg"
#define NATIVE_SPEEDRUN_LOG_FILE      "speedrun-events.log"
#define NATIVE_SPEEDRUN_SURFACE_FILE  "speedrun-surface.bin"
#define NATIVE_SPEEDRUN_BRIDGE_SCRIPT "speedrun-bridge.py"
#define NATIVE_SPEEDRUN_BRIDGE_LOCK   16835
#define NATIVE_SPEEDRUN_ROUTE_MAX     8192

// The read-only surface lives in its own named section so an external tool can
// find it by section rather than by a hardcoded offset. The static initializer
// keeps the magic and ABI version present even before the first frame.
__attribute__((section(".ctrsr"), used)) global_variable struct NativeSpeedrunSurface g_ctr_speedrun_surface = {
    .magic = NATIVE_SPEEDRUN_SURFACE_MAGIC,
    .abiVersion = NATIVE_SPEEDRUN_ABI_VERSION,
};

global_variable struct NativeSpeedrunState s_speedrunState;
global_variable FILE *s_speedrunLog;
global_variable FILE *s_surfaceFile;
global_variable u32 s_speedrunReady;

// Writes the surface to a file so a companion can poll it without reading the
// game's memory. That is required on Linux, where yama ptrace_scope blocks
// cross-process memory reads; the named section still serves Windows and
// same-process consumers.
internal void NativeSpeedrunRuntime_PublishSurface(void)
{
	if (s_surfaceFile == NULL)
	{
		return;
	}

	fseek(s_surfaceFile, 0, SEEK_SET);
	fwrite(&g_ctr_speedrun_surface, sizeof(g_ctr_speedrun_surface), 1, s_surfaceFile);
}

internal void NativeSpeedrunRuntime_LoadRoute(struct NativeSpeedrunRoute *route)
{
	memset(route, 0, sizeof(*route));

	const char *path = getenv("CTR_SPEEDRUN_ROUTE");
	if ((path == NULL) || (path[0] == '\0'))
	{
		path = NATIVE_SPEEDRUN_ROUTE_FILE;
	}

	FILE *file = fopen(path, "rb");
	if (file == NULL)
	{
		Platform_Log("[CTR Speedrun] no route config (%s), running clocks only\n", path);
		return;
	}

	char text[NATIVE_SPEEDRUN_ROUTE_MAX];
	const size_t count = fread(text, 1, sizeof(text) - 1, file);
	fclose(file);
	text[count] = '\0';

	char err[128] = {0};
	const int splits = NativeSpeedrun_ParseRoute(text, route, err, sizeof(err));
	if (splits < 0)
	{
		Platform_Log("[CTR Speedrun] route config rejected: %s\n", err);
		memset(route, 0, sizeof(*route));
		return;
	}

	Platform_Log("[CTR Speedrun] route config loaded: %d splits\n", splits);
}

// Starts the timer bridge next to the executable, if it is present and python
// is available. The bridge takes a lock so a manual start cannot double up. Set
// CTR_SPEEDRUN_NO_BRIDGE=1 to skip it, or CTR_SPEEDRUN_PYTHON to pick an
// interpreter. The handle is intentionally left open so the child outlives this
// call; the bridge exits on its own after the surface goes idle.
internal void NativeSpeedrunRuntime_StartBridge(void)
{
	if (getenv("CTR_SPEEDRUN_NO_BRIDGE") != NULL)
	{
		return;
	}

	FILE *probe = fopen(NATIVE_SPEEDRUN_BRIDGE_SCRIPT, "rb");
	if (probe == NULL)
	{
		return;
	}
	fclose(probe);

	const char *python = getenv("CTR_SPEEDRUN_PYTHON");
	if ((python == NULL) || (python[0] == '\0'))
	{
#if defined(_WIN32)
		python = "python";
#else
		python = "python3";
#endif
	}

	char lockPort[16];
	snprintf(lockPort, sizeof(lockPort), "%d", NATIVE_SPEEDRUN_BRIDGE_LOCK);

	const char *args[] = {python,
	                      NATIVE_SPEEDRUN_BRIDGE_SCRIPT,
	                      "--source",
	                      "surface",
	                      "--surface-file",
	                      NATIVE_SPEEDRUN_SURFACE_FILE,
	                      "--events",
	                      NATIVE_SPEEDRUN_LOG_FILE,
	                      "--idle-timeout",
	                      "300",
	                      "--lock-port",
	                      lockPort,
	                      NULL};

	if (SDL_CreateProcess(args, false) == NULL)
	{
		Platform_Log("[CTR Speedrun] bridge could not start: %s\n", SDL_GetError());
		return;
	}

	Platform_Log("[CTR Speedrun] bridge started\n");
}

void NativeSpeedrunRuntime_Init(void)
{
	struct NativeSpeedrunRoute route;

	NativeSpeedrunRuntime_LoadRoute(&route);
	NativeSpeedrun_Reset(&s_speedrunState, &route);
	NativeSpeedrun_WriteSurface(&s_speedrunState, &g_ctr_speedrun_surface);

	s_surfaceFile = fopen(NATIVE_SPEEDRUN_SURFACE_FILE, "wb");
	if (s_surfaceFile != NULL)
	{
		setvbuf(s_surfaceFile, NULL, _IONBF, 0);
		NativeSpeedrunRuntime_PublishSurface();
	}

	s_speedrunReady = 1;

	NativeSpeedrunRuntime_StartBridge();
}

void NativeSpeedrunRuntime_Update(struct GameTracker *gGT)
{
	if (s_speedrunReady == 0)
	{
		return;
	}

	struct NativeSpeedrunFrame frame;

	memset(&frame, 0, sizeof(frame));
	frame.mainGameState = sdata->mainGameState;
	frame.loadStage = sdata->Loading.stage;
	frame.gameMode1 = (u32)gGT->gameMode1;
	frame.levelID = gGT->levelID;
	frame.elapsedTimeMS = (u32)gGT->elapsedTimeMS;
	frame.wallTimeMS = (u64)SDL_GetTicks();
	frame.numPlayers = gGT->numPlyrCurrGame;
	frame.demoMode = gGT->boolDemoMode;

	if (gGT->drivers[0] != NULL)
	{
		frame.playerFinished = (gGT->drivers[0]->actionsFlagSet & ACTION_RACE_FINISHED) != 0;
		if (frame.playerFinished != 0)
		{
			frame.finishPosition = gGT->drivers[0]->driverRank + 1;
		}
	}

	NativeSpeedrun_Update(&s_speedrunState, &frame);
	NativeSpeedrun_WriteSurface(&s_speedrunState, &g_ctr_speedrun_surface);
	NativeSpeedrunRuntime_PublishSurface();

	for (u32 i = 0; i < s_speedrunState.eventCount; i++)
	{
		char line[256];

		if (NativeSpeedrun_FormatEvent(&s_speedrunState, &s_speedrunState.events[i], line, sizeof(line)) <= 0)
		{
			continue;
		}

		if (s_speedrunLog == NULL)
		{
			s_speedrunLog = fopen(NATIVE_SPEEDRUN_LOG_FILE, "ab");
		}

		if (s_speedrunLog != NULL)
		{
			fputs(line, s_speedrunLog);
			fflush(s_speedrunLog);
		}
	}
}
