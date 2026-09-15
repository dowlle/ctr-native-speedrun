// Test fixture: publishes a .ctrsr surface on a scripted timeline so the
// bridge's surface reader can be tested end to end without the game.

#include <platform/native_speedrun.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep((DWORD)(ms))
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((useconds_t)((ms) * 1000))
#endif

__attribute__((section(".ctrsr"), used)) struct NativeSpeedrunSurface g_fixture_surface;

static FILE *g_out;

static void publish(const struct NativeSpeedrunSurface *surface)
{
	g_fixture_surface = *surface;

	if (g_out != NULL)
	{
		fseek(g_out, 0, SEEK_SET);
		fwrite(surface, sizeof(*surface), 1, g_out);
	}
}

int main(int argc, char **argv)
{
	struct NativeSpeedrunSurface surface;

	if (argc > 1)
	{
		g_out = fopen(argv[1], "wb");
		if (g_out != NULL)
		{
			setvbuf(g_out, NULL, _IONBF, 0);
		}
	}

	memset(&surface, 0, sizeof(surface));
	surface.magic = NATIVE_SPEEDRUN_SURFACE_MAGIC;
	surface.abiVersion = NATIVE_SPEEDRUN_ABI_VERSION;
	publish(&surface);

#ifdef _WIN32
	printf("%lu\n", (unsigned long)GetCurrentProcessId());
#else
	printf("%d\n", (int)getpid());
#endif
	fflush(stdout);

	SLEEP_MS(500);

	surface.flags = NATIVE_SPEEDRUN_SURFACE_ACTIVE;
	surface.lastEventType = NATIVE_SPEEDRUN_EVENT_RUN_START;
	surface.lastEventSequence = 1;
	surface.loadlessMS = 0;
	publish(&surface);

	for (int i = 0; i < 10; i++)
	{
		SLEEP_MS(50);
		surface.loadlessMS += 32;
		publish(&surface);
	}

	surface.lastEventType = NATIVE_SPEEDRUN_EVENT_SPLIT;
	surface.lastEventSequence = 2;
	surface.segmentIndex = 1;
	surface.lastEventSegmentIndex = 0;
	surface.lastEventSegmentTimeMS = 320;
	surface.lastEventTotalTimeMS = 320;
	publish(&surface);

	for (int i = 0; i < 10; i++)
	{
		SLEEP_MS(50);
		surface.loadlessMS += 32;
		publish(&surface);
	}

	surface.lastEventType = NATIVE_SPEEDRUN_EVENT_RUN_END;
	surface.lastEventSequence = 3;
	surface.segmentIndex = 2;
	surface.lastEventSegmentIndex = 1;
	surface.lastEventSegmentTimeMS = 320;
	surface.lastEventTotalTimeMS = 640;
	surface.flags = NATIVE_SPEEDRUN_SURFACE_FINISHED;
	publish(&surface);

	SLEEP_MS(300);
	return 0;
}
