#include <common.h>

// NOTE(aalhendi): packID is the other hub's pack, as returned by
// LOAD_GetAdvPackIndex(). The current hub uses 3 - that index.
void LOAD_TalkingMask(int packID, int maskID)
{
	register int maskOffset CTR_PSX_REGISTER("$17");
	register int packOffset CTR_PSX_REGISTER("$16");
	register int subfileIndex CTR_PSX_REGISTER("$6");
	register int requestType CTR_PSX_REGISTER("$5");
	register void *destination CTR_PSX_REGISTER("$7");
	register struct BigHeader *bigfile CTR_PSX_REGISTER("$4");

	sdata->modelMaskHints3D = 0;

	// invalidate alternative-hub, because
	// the mask will load in that level's RAM
	GAME_TRACKER->levID_in_each_mempack[packID] = -1;

	// Swap to pack of hub you're NOT on,
	// wipe the pack to reload the new MASK
	MEMPACK_SwapPacks(packID);
	MEMPACK_ClearLowMem();

	maskOffset = maskID;
	CTR_PSX_SHIFT_LEFT_IN_PLACE(maskOffset, 2);
	CTR_PSX_OBSERVE_VALUE(maskOffset);
	requestType = LT_VRAM;
	CTR_PSX_OBSERVE_VALUE(requestType);
	packOffset = packID - 1;
	CTR_PSX_SHIFT_LEFT_IN_PLACE(packOffset, 1);
#ifdef CTR_NATIVE
	subfileIndex = packOffset + BI_UKAHEAD;
#else
	// NOTE(aalhendi): GCC 2.8.1 otherwise reassociates the two offsets.
	__asm__("addiu %0,%1,%2" : "=r"(subfileIndex) : "r"(packOffset), "I"(BI_UKAHEAD));
#endif
	CTR_PSX_ADD_U32(subfileIndex, maskOffset, subfileIndex);
	CTR_PSX_OBSERVE_VALUE(subfileIndex);
	destination = NULL;
	CTR_PSX_OBSERVE_VALUE(destination);
	bigfile = sdata->ptrBigfileCdPos_2;
	CTR_PSX_OBSERVE_VALUE(bigfile);
	sdata->load_inProgress = 1;

	// NOTE(aalhendi): Retail queues legacy VRAM type 3 with no final callback.
	LOAD_AppendQueue(bigfile, requestType, subfileIndex, destination, NULL);

	requestType = LT_GETADDR;
	CTR_PSX_OBSERVE_VALUE(requestType);
	packOffset += BI_UKAHEAD + 1;
	subfileIndex = maskOffset + packOffset;
	destination = NULL;
	CTR_PSX_OBSERVE_VALUE(destination);
	bigfile = sdata->ptrBigfileCdPos_2;
	LOAD_AppendQueue(bigfile, requestType, subfileIndex, destination, LOAD_Callback_MaskHints3D);
}

void LOAD_LevelFile(int levelID)
{
	struct GameTracker *gGT = GAME_TRACKER;

	// A new level invalidates the previous mask-model hints.
	sdata->modelMaskHints3D = 0;

	gGT->hudFlags &= HUD_FLAG_CLEAR_RACE_HUD_MASK;
	// NOTE(aalhendi): Preserve the retail order of the HUD byte store and
	// following level-ID loads.
	CTR_PSX_MEMORY_BARRIER();

	gGT->prevLEV = gGT->levelID;
	gGT->levelID = levelID;

	// disable all rendering except checkeredFlag
	GAME_TRACKER_RELOAD()->renderFlags &= RENDER_FLAG_CHECKERED_FLAG;

	if (RaceFlag_IsFullyOffScreen() == 1)
	{
		RaceFlag_BeginTransition(1);
	}

	// start loading
	GAME_LOADING_STAGE = LOAD_TEN_STAGES_0;
}
