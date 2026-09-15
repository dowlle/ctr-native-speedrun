// Host harness for the speedrun state machine. Standalone: no SDL, no game.
// Builds against platform/native_speedrun.c and include/native_speedrun.h only.

#include <platform/native_speedrun.h>

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                                                                                        \
	do                                                                                                                                     \
	{                                                                                                                                      \
		g_checks++;                                                                                                                        \
		if (!(cond))                                                                                                                       \
		{                                                                                                                                  \
			g_failures++;                                                                                                                  \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                                 \
		}                                                                                                                                  \
	} while (0)

#define CHECK_INT(actual, expected)                                                                                                        \
	do                                                                                                                                     \
	{                                                                                                                                      \
		g_checks++;                                                                                                                        \
		long a_ = (long)(actual);                                                                                                          \
		long e_ = (long)(expected);                                                                                                        \
		if (a_ != e_)                                                                                                                      \
		{                                                                                                                                  \
			g_failures++;                                                                                                                  \
			fprintf(stderr, "FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, #actual, a_, e_);                                \
		}                                                                                                                                  \
	} while (0)

#define ADVENTURE NATIVE_SPEEDRUN_GM_ADVENTURE
#define BOSS      NATIVE_SPEEDRUN_GM_ADVENTURE_BOSS
#define START     NATIVE_SPEEDRUN_GM_START_OF_RACE
#define LOADING   NATIVE_SPEEDRUN_GM_LOADING
#define HUB       NATIVE_SPEEDRUN_HUB_N_SANITY_BEACH
#define MENU      0x27
#define IDLE      NATIVE_SPEEDRUN_LOAD_IDLE

static struct NativeSpeedrunState g_state;
static u64 g_wall;

static const struct NativeSpeedrunRoute g_route = {
    .count = 3,
    .splits =
        {
            {3, NATIVE_SPEEDRUN_SPLIT_NORMAL, "Crash Cove"},
            {13, NATIVE_SPEEDRUN_SPLIT_BOSS, "Ripper Roo"},
            {14, NATIVE_SPEEDRUN_SPLIT_BOSS, "N. Oxide"},
        },
};

static void BeginTestRoute(const struct NativeSpeedrunRoute *route)
{
	g_wall = 100000;
	memset(&g_state, 0, sizeof(g_state));
	NativeSpeedrun_Reset(&g_state, route);
}

static void BeginTest(void)
{
	BeginTestRoute(&g_route);
}

// One engine frame. wallDt advances the host clock; elapsed is the engine's own
// per-frame time (zero while the engine is frozen by pause).
static void Frame(s32 level, u32 mode, s32 loadStage, u32 elapsed, u32 wallDt, s32 finished, s32 paused)
{
	struct NativeSpeedrunFrame frame;

	memset(&frame, 0, sizeof(frame));
	frame.mainGameState = 3;
	frame.loadStage = loadStage;
	frame.gameMode1 = mode | (paused ? NATIVE_SPEEDRUN_GM_PAUSE_ALL : 0u);
	frame.levelID = level;
	frame.elapsedTimeMS = elapsed;
	frame.wallTimeMS = g_wall;
	frame.numPlayers = 1;
	frame.demoMode = 0;
	frame.playerFinished = finished;

	NativeSpeedrun_Update(&g_state, &frame);
	g_wall += wallDt;
}

static void StartRun(void)
{
	Frame(MENU, 0, IDLE, 32, 32, 0, 0);
	Frame(HUB, ADVENTURE, IDLE, 32, 32, 0, 0);
}

static void TestStartsOnHubControl(void)
{
	BeginTest();

	Frame(MENU, 0, IDLE, 32, 32, 0, 0);
	CHECK_INT(g_state.active, 0);
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_NONE);

	Frame(HUB, ADVENTURE | START, IDLE, 32, 32, 0, 0);
	CHECK_INT(g_state.active, 0); // fly-in not finished yet

	Frame(HUB, ADVENTURE, IDLE, 32, 32, 0, 0);
	CHECK_INT(g_state.active, 1);
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_RUN_START);
	CHECK_INT(g_state.lastEvent.totalTimeMS, 0);
	CHECK_INT(g_state.lastEvent.segmentTimeMS, 0);
}

static void TestLoadsExcludedRtaIncluded(void)
{
	BeginTest();
	StartRun();

	const u32 loadlessAfterStart = g_state.loadlessMS;
	const u32 rtaAfterStart = g_state.rtaMS;

	for (int i = 0; i < 10; i++)
	{
		Frame(HUB, ADVENTURE, 0, 32, 32, 0, 0); // loading
	}

	CHECK_INT(g_state.loadlessMS, loadlessAfterStart);
	CHECK_INT(g_state.rtaMS, rtaAfterStart + 320);
}

static void TestCutsceneCounted(void)
{
	BeginTest();
	StartRun();

	const u32 before = g_state.loadlessMS;
	Frame(HUB, ADVENTURE | START, IDLE, 32, 32, 0, 0); // cutscene, not a load
	CHECK_INT(g_state.loadlessMS, before + 32);
}

static void TestPauseCountedViaWall(void)
{
	BeginTest();
	StartRun();

	const u32 before = g_state.loadlessMS;
	const u32 rtaBefore = g_state.rtaMS;

	for (int i = 0; i < 5; i++)
	{
		Frame(HUB, ADVENTURE, IDLE, 0, 32, 0, 1); // engine frozen, wall advances
	}

	CHECK_INT(g_state.loadlessMS, before + 160);
	CHECK_INT(g_state.rtaMS, rtaBefore + 160);
	CHECK_INT(g_state.paused, 1);
}

static void TestSplitsFireInOrder(void)
{
	BeginTest();
	StartRun();

	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0); // P1 finishes Crash Cove
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_SPLIT);
	CHECK_INT(g_state.lastEvent.segmentIndex, 0);
	CHECK_INT(g_state.segmentIndex, 1);
	CHECK(strcmp(g_state.route.splits[0].name, "Crash Cove") == 0);

	Frame(4, ADVENTURE, IDLE, 32, 32, 0, 0); // racing again, finish edge reset

	Frame(13, ADVENTURE | BOSS, IDLE, 32, 32, 1, 0); // Ripper Roo
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_SPLIT);
	CHECK_INT(g_state.lastEvent.segmentIndex, 1);

	Frame(4, ADVENTURE, IDLE, 32, 32, 0, 0);

	Frame(14, ADVENTURE | BOSS, IDLE, 32, 32, 1, 0); // N. Oxide
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_RUN_END);
	CHECK_INT(g_state.lastEvent.segmentIndex, 2);
	CHECK_INT(g_state.active, 0);
	CHECK_INT(g_state.finished, 1);
}

static void TestFinishEdgeOnlyFiresOnce(void)
{
	BeginTest();
	StartRun();

	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0);
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_SPLIT);

	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0); // still finished, no edge
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_NONE);
	CHECK_INT(g_state.segmentIndex, 1);
}

static void TestNonRouteFinishIgnored(void)
{
	BeginTest();
	StartRun();

	Frame(9, ADVENTURE, IDLE, 32, 32, 1, 0); // unrelated race
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_NONE);
	CHECK_INT(g_state.segmentIndex, 0);
	CHECK_INT(g_state.active, 1);
}

static void TestBossKindMustMatch(void)
{
	BeginTest();
	StartRun();

	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0); // Crash Cove, advances to split 1
	CHECK_INT(g_state.segmentIndex, 1);

	Frame(4, ADVENTURE, IDLE, 32, 32, 0, 0); // finish edge reset

	Frame(13, ADVENTURE, IDLE, 32, 32, 1, 0); // level matches but not boss mode
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_NONE);
	CHECK_INT(g_state.segmentIndex, 1);
}

static void TestNoCountingAfterRunEnd(void)
{
	BeginTest();
	StartRun();
	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0);
	Frame(4, ADVENTURE, IDLE, 32, 32, 0, 0);
	Frame(13, ADVENTURE | BOSS, IDLE, 32, 32, 1, 0);
	Frame(4, ADVENTURE, IDLE, 32, 32, 0, 0);
	Frame(14, ADVENTURE | BOSS, IDLE, 32, 32, 1, 0);

	const u32 loadless = g_state.loadlessMS;
	const u32 rta = g_state.rtaMS;

	for (int i = 0; i < 10; i++)
	{
		Frame(14, ADVENTURE, IDLE, 32, 32, 0, 0);
	}

	CHECK_INT(g_state.loadlessMS, loadless);
	CHECK_INT(g_state.rtaMS, rta);
}

static void TestResetOnMainMenu(void)
{
	BeginTest();
	StartRun();

	Frame(MENU, 0, IDLE, 32, 32, 0, 0);
	CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_RESET);
	CHECK_INT(g_state.active, 0);
	CHECK_INT(g_state.finished, 0);
}

static void TestSegmentTime(void)
{
	BeginTest();
	StartRun();

	// Three non-load frames of 32 ms each, then finish.
	Frame(HUB, ADVENTURE, IDLE, 32, 32, 0, 0);
	Frame(HUB, ADVENTURE, IDLE, 32, 32, 0, 0);
	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0);

	// Start frame counted 32, then two more, then the finish frame: 4 frames.
	CHECK_INT(g_state.lastEvent.segmentTimeMS, 4u * 32u);
	CHECK_INT(g_state.lastEvent.totalTimeMS, 4u * 32u);
}

static void TestParserValid(void)
{
	const char *text = "# Any% NMG\n"
	                   "3 normal Crash Cove\n"
	                   "13 boss Ripper Roo\n"
	                   "\n"
	                   "14 boss N. Oxide\n";

	struct NativeSpeedrunRoute route;
	char err[128] = {0};

	const int count = NativeSpeedrun_ParseRoute(text, &route, err, sizeof(err));

	CHECK_INT(count, 3);
	CHECK(strcmp(err, "") == 0);
	CHECK_INT(route.splits[0].levelID, 3);
	CHECK_INT(route.splits[0].kind, NATIVE_SPEEDRUN_SPLIT_NORMAL);
	CHECK(strcmp(route.splits[0].name, "Crash Cove") == 0);
	CHECK_INT(route.splits[1].kind, NATIVE_SPEEDRUN_SPLIT_BOSS);
	CHECK(strcmp(route.splits[2].name, "N. Oxide") == 0);
}

static void TestParserRejectsBadKind(void)
{
	struct NativeSpeedrunRoute route;
	char err[128] = {0};
	CHECK_INT(NativeSpeedrun_ParseRoute("3 sideways Crash Cove\n", &route, err, sizeof(err)), -1);
	CHECK(strstr(err, "kind") != NULL);
}

static void TestParserRejectsLongName(void)
{
	struct NativeSpeedrunRoute route;
	char err[128] = {0};
	const char *text = "3 normal ThisNameIsDefinitelyFarTooLongForTheNameBuffer\n";
	CHECK_INT(NativeSpeedrun_ParseRoute(text, &route, err, sizeof(err)), -1);
	CHECK(strstr(err, "too long") != NULL);
}

static void TestSurface(void)
{
	BeginTest();
	StartRun();
	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0);

	struct NativeSpeedrunSurface surface;
	NativeSpeedrun_WriteSurface(&g_state, &surface);

	CHECK_INT(surface.magic, NATIVE_SPEEDRUN_SURFACE_MAGIC);
	CHECK_INT(surface.abiVersion, NATIVE_SPEEDRUN_ABI_VERSION);
	CHECK_INT(surface.flags, NATIVE_SPEEDRUN_SURFACE_ACTIVE);
	CHECK_INT(surface.segmentIndex, 1);
	CHECK_INT(surface.lastEventType, NATIVE_SPEEDRUN_EVENT_SPLIT);
	CHECK_INT(surface.lastEventSegmentIndex, 0);
	CHECK(surface.lastEventSequence == g_state.sequence);
	CHECK(surface.lastEventTotalTimeMS == g_state.loadlessMS);
}

static void TestEventLogLine(void)
{
	BeginTest();
	StartRun();
	Frame(3, ADVENTURE, IDLE, 32, 32, 1, 0);

	char line[256] = {0};
	const int written = NativeSpeedrun_FormatEvent(&g_state, line, sizeof(line));

	CHECK(written > 0);
	CHECK(strstr(line, "type=split") != NULL);
	CHECK(strstr(line, "level=3") != NULL);
	CHECK(strstr(line, "segment=0") != NULL);
	CHECK(strstr(line, "loadless_ms=") != NULL);
	CHECK(line[written - 1] == '\n');

	char small[8];
	CHECK_INT(NativeSpeedrun_FormatEvent(&g_state, small, sizeof(small)), -1);
}

#ifdef NATIVE_SPEEDRUN_REPO_CONFIG
static int LoadRepoRoute(struct NativeSpeedrunRoute *route)
{
	FILE *file = fopen(NATIVE_SPEEDRUN_REPO_CONFIG, "rb");
	if (file == NULL)
	{
		return 0;
	}

	char text[8192];
	const size_t count = fread(text, 1, sizeof(text) - 1, file);
	fclose(file);
	text[count] = '\0';

	char err[128] = {0};
	return NativeSpeedrun_ParseRoute(text, route, err, sizeof(err));
}

static void TestRepoRouteConfig(void)
{
	struct NativeSpeedrunRoute route;

	CHECK_INT(LoadRepoRoute(&route), 21);
	if (route.count != 21)
	{
		return;
	}

	CHECK_INT(route.splits[0].levelID, 3);
	CHECK_INT(route.splits[0].kind, NATIVE_SPEEDRUN_SPLIT_NORMAL);
	CHECK(strcmp(route.splits[0].name, "Crash Cove") == 0);

	CHECK_INT(route.splits[4].levelID, 6);
	CHECK_INT(route.splits[4].kind, NATIVE_SPEEDRUN_SPLIT_BOSS);
	CHECK(strcmp(route.splits[4].name, "Ripper Roo") == 0);

	CHECK_INT(route.splits[20].levelID, 13);
	CHECK_INT(route.splits[20].kind, NATIVE_SPEEDRUN_SPLIT_BOSS);
	CHECK(strcmp(route.splits[20].name, "N. Oxide") == 0);
}

static void TestFullRoute(void)
{
	struct NativeSpeedrunRoute route;

	if (LoadRepoRoute(&route) != 21)
	{
		CHECK(false);
		return;
	}

	BeginTestRoute(&route);
	StartRun();

	for (s32 i = 0; i < route.count; i++)
	{
		const u32 mode = (route.splits[i].kind == NATIVE_SPEEDRUN_SPLIT_BOSS) ? (ADVENTURE | BOSS) : ADVENTURE;

		Frame(route.splits[i].levelID, mode, IDLE, 32, 32, 0, 0);
		Frame(route.splits[i].levelID, mode, IDLE, 32, 32, 1, 0);

		if (i < (route.count - 1))
		{
			CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_SPLIT);
			CHECK_INT(g_state.lastEvent.segmentIndex, i);
		}
		else
		{
			CHECK_INT(g_state.lastEvent.type, NATIVE_SPEEDRUN_EVENT_RUN_END);
			CHECK_INT(g_state.lastEvent.segmentIndex, i);
		}
	}

	CHECK_INT(g_state.active, 0);
	CHECK_INT(g_state.finished, 1);
	CHECK_INT(g_state.segmentIndex, route.count);
}
#endif

int main(void)
{
	TestStartsOnHubControl();
	TestLoadsExcludedRtaIncluded();
	TestCutsceneCounted();
	TestPauseCountedViaWall();
	TestSplitsFireInOrder();
	TestFinishEdgeOnlyFiresOnce();
	TestNonRouteFinishIgnored();
	TestBossKindMustMatch();
	TestNoCountingAfterRunEnd();
	TestResetOnMainMenu();
	TestSegmentTime();
	TestParserValid();
	TestParserRejectsBadKind();
	TestParserRejectsLongName();
	TestSurface();
	TestEventLogLine();
#ifdef NATIVE_SPEEDRUN_REPO_CONFIG
	TestRepoRouteConfig();
	TestFullRoute();
#endif

	fprintf(stderr, "native_speedrun: %d checks, %d failures\n", g_checks, g_failures);
	return (g_failures == 0) ? 0 : 1;
}
