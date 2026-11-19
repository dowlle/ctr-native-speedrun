#include <common.h>

void LOAD_OvrLOD(u32 numPlyrCurrGame)
{
	register u32 playerCount CTR_PSX_REGISTER("$6");
	// change {1-4} -> {0-3}
	u32 overlayIndex;
	register struct GameTracker *gGT CTR_PSX_REGISTER("$2");
	register struct GameTracker *threadsTracker CTR_PSX_REGISTER("$3");
#ifndef CTR_NATIVE
	register int loadType CTR_PSX_REGISTER("$5");
	int packIndex;
	register struct BigHeader *bigfile CTR_PSX_REGISTER("$4");
	register void *destination CTR_PSX_REGISTER("$7");
	void (*callback)(struct LoadQueueSlot *);
#endif

	// NOTE(aalhendi): Keep the resident tracker reloads and queue arguments in retail instruction order.
	CTR_PSX_LOAD_SYMBOL_PAGE(gGT, RETAIL_GAME_TRACKER_ASM_NAME);
	CTR_PSX_LOAD_WORD_FROM_PAGE(gGT, gGT, RETAIL_GAME_TRACKER_ASM_NAME, GAME_TRACKER);
	playerCount = numPlyrCurrGame;
	overlayIndex = playerCount - 1;
	// if new LOD overlay needs to load
	if ((u32)gGT->overlayIndex_LOD != overlayIndex)
	{
#ifndef CTR_NATIVE
		// LOD overlay 226-229
		sdata->load_inProgress = 1;
		callback = LOAD_Callback_Overlay_Generic;
		CTR_PSX_KEEP_VALUE(playerCount);
		CTR_PSX_LOAD_IMMEDIATE(loadType, LT_SETADDR);
		CTR_PSX_DEPEND_VALUE(playerCount, loadType);
		packIndex = BI_OVERLAYSECT2 + playerCount - 1;
		CTR_PSX_LOAD_WORD_AFTER(bigfile, sdata->ptrBigfileCdPos_2, packIndex);
		CTR_PSX_LOAD_SYMBOL_PAGE_AFTER(destination, "OVR_Region2", bigfile);
		CTR_PSX_ADD_SYMBOL_LOW_IN_PLACE(destination, "OVR_Region2", &OVR_Region2);
		LOAD_AppendQueue(bigfile, loadType, packIndex, destination, callback);
#endif

		// save ID, and reload next overlay (sector read invalidation)
		CTR_PSX_LOAD_SYMBOL_PAGE(gGT, RETAIL_GAME_TRACKER_ASM_NAME);
		CTR_PSX_LOAD_WORD_FROM_PAGE(gGT, gGT, RETAIL_GAME_TRACKER_ASM_NAME, GAME_TRACKER);
		gGT->overlayIndex_LOD = overlayIndex;
		CTR_PSX_MEMORY_BARRIER();
		CTR_PSX_LOAD_SYMBOL_PAGE(threadsTracker, RETAIL_GAME_TRACKER_ASM_NAME);
		CTR_PSX_LOAD_WORD_FROM_PAGE(threadsTracker, threadsTracker, RETAIL_GAME_TRACKER_ASM_NAME, GAME_TRACKER);
		threadsTracker->overlayIndex_Threads = OVERLAY_INDEX_NONE;
	}
	return;
}

void LOAD_OvrEndRace(u32 overlayIndex)
{
	register u32 endRaceIndex CTR_PSX_REGISTER("$16");
	register struct GameTracker *gGT CTR_PSX_REGISTER("$2");
	register struct GameTracker *lodTracker CTR_PSX_REGISTER("$3");
#ifndef CTR_NATIVE
	register int loadType CTR_PSX_REGISTER("$5");
	int packIndex;
	register struct BigHeader *bigfile CTR_PSX_REGISTER("$4");
	register void *destination CTR_PSX_REGISTER("$7");
	void (*callback)(struct LoadQueueSlot *);
#endif

	gGT = GAME_TRACKER;
	endRaceIndex = overlayIndex;
	// if new EndOfRace overlay needs to load
	if ((u32)gGT->overlayIndex_EndOfRace != endRaceIndex)
	{
#ifndef CTR_NATIVE
		// EndOfRace overlay 221-225
		sdata->load_inProgress = 1;
		callback = LOAD_Callback_Overlay_Generic;
		CTR_PSX_KEEP_VALUE(endRaceIndex);
		CTR_PSX_LOAD_IMMEDIATE(loadType, LT_SETADDR);
		CTR_PSX_DEPEND_VALUE(endRaceIndex, loadType);
		packIndex = BI_OVERLAYSECT1 + endRaceIndex;
		CTR_PSX_LOAD_WORD_AFTER(bigfile, sdata->ptrBigfileCdPos_2, packIndex);
		CTR_PSX_LOAD_SYMBOL_PAGE_AFTER(destination, "OVR_Region1", bigfile);
		CTR_PSX_ADD_SYMBOL_LOW_IN_PLACE(destination, "OVR_Region1", &OVR_Region1);
		LOAD_AppendQueue(bigfile, loadType, packIndex, destination, callback);
#endif

		// NOTE(aalhendi): Retail rereads the tracker after the queue call before resetting both overlay IDs.
		CTR_PSX_LOAD_SYMBOL_PAGE(gGT, RETAIL_GAME_TRACKER_ASM_NAME);
		CTR_PSX_LOAD_WORD_FROM_PAGE(gGT, gGT, RETAIL_GAME_TRACKER_ASM_NAME, GAME_TRACKER);
		gGT->overlayIndex_EndOfRace = endRaceIndex;
		CTR_PSX_MEMORY_BARRIER();
		CTR_PSX_LOAD_SYMBOL_PAGE(lodTracker, RETAIL_GAME_TRACKER_ASM_NAME);
		CTR_PSX_LOAD_WORD_FROM_PAGE(lodTracker, lodTracker, RETAIL_GAME_TRACKER_ASM_NAME, GAME_TRACKER);
		lodTracker->overlayIndex_LOD = OVERLAY_INDEX_NONE;
	}
	return;
}

#ifdef CTR_NATIVE
static void LOAD_NativeResetThreadsOverlay(enum OverlayIndex overlayIndex)
{
	switch (overlayIndex)
	{
	case OVERLAY_INDEX_NONE:
		break;
	case OVERLAY_INDEX_MAIN_MENU:
		OVR230_InitData();
		break;
	case OVERLAY_INDEX_RACING_OR_BATTLE:
		OVR231_InitData();
		break;
	case OVERLAY_INDEX_ADV_HUB:
		OVR232_InitData();
		break;
	case OVERLAY_INDEX_PODIUMS:
		OVR233_InitData();
		break;
	}
}
#endif

void LOAD_OvrThreads(u32 overlayIndex)
{
	// NOTE(aalhendi): The direct sData field address keeps retail's tracker load ahead of the stack frame.
	struct GameTracker *gGT = CTR_PSX_PAGE_LVALUE(struct GameTracker *, OFFSETOF_SDATA(gGT), 0, GAME_TRACKER);
	register u32 threadIndex CTR_PSX_REGISTER("$6");
#ifndef CTR_NATIVE
	register int loadType CTR_PSX_REGISTER("$5");
	int packIndex;
	register struct BigHeader *bigfile CTR_PSX_REGISTER("$4");
	register void *destination CTR_PSX_REGISTER("$7");
	register void **callbackBase CTR_PSX_REGISTER("$3");
	register u32 callbackOffset CTR_PSX_REGISTER("$2");
	register void **callbackSlot CTR_PSX_REGISTER("$2");
	void (*callback)(struct LoadQueueSlot *);
#endif

	threadIndex = overlayIndex;

	// if new Threads overlay needs to load
	if ((u32)gGT->overlayIndex_Threads != threadIndex)
	{
#ifndef CTR_NATIVE
		// Threads overlay 230-233
		sdata->load_inProgress = 1;
		gGT->overlayIndex_Threads = OVERLAY_INDEX_NONE;
		// NOTE(aalhendi): Keep callback lookup ahead of the queue arguments and load it from the table's zero offset.
		CTR_PSX_KEEP_VALUE(threadIndex);
		CTR_PSX_LOAD_SYMBOL_PAGE(callbackBase, RETAIL_OVERLAY_CALLBACKS_ASM_NAME);
		CTR_PSX_ADD_SYMBOL_LOW_IN_PLACE(callbackBase, RETAIL_OVERLAY_CALLBACKS_ASM_NAME, GAME_OVERLAY_CALLBACKS);
		callbackOffset = threadIndex * sizeof(*callbackSlot);
		CTR_PSX_ADD_POINTER_OFFSET_OFFSET_FIRST(callbackSlot, callbackBase, callbackOffset);
		CTR_PSX_LOAD_IMMEDIATE(loadType, LT_SETADDR);
		CTR_PSX_DEPEND_VALUE(threadIndex, loadType);
		packIndex = BI_OVERLAYSECT3 + threadIndex;
		CTR_PSX_LOAD_WORD_AFTER(callback, *callbackSlot, packIndex);
		CTR_PSX_LOAD_WORD_AFTER(bigfile, sdata->ptrBigfileCdPos_2, callback);
		CTR_PSX_LOAD_SYMBOL_PAGE_AFTER(destination, "OVR_Region3", bigfile);
		CTR_PSX_ADD_SYMBOL_LOW_IN_PLACE(destination, "OVR_Region3", &OVR_Region3);
		LOAD_AppendQueue(bigfile, loadType, packIndex, destination, callback);
#else
		// NOTE(aalhendi): Native overlays are already linked, so reset the
		// overlay-owned data that retail would refresh by streaming into OVR_Region3.
		gGT->overlayIndex_Threads = OVERLAY_INDEX_NONE;
		LOAD_NativeResetThreadsOverlay((enum OverlayIndex)overlayIndex);
		((void (*)())GAME_OVERLAY_CALLBACKS[threadIndex])();
#endif
	}
}

int LOAD_GetAdvPackIndex(void)
{
	int levelID = GAME_TRACKER->levelID;

	if ((levelID == GEM_STONE_VALLEY) || (levelID == GLACIER_PARK))
	{
		return 2;
	}

	return 1;
}
