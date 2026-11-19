#include <common.h>

// NOTE(aalhendi): Qualify selected callback stores to preserve retail's
// return delay slots without changing the shared state layout.
void LOAD_Callback_Overlay_Generic(struct LoadQueueSlot *lqs)
{
	(void)lqs;
	*(volatile int *)&sdata->load_inProgress = 0;
}

void LOAD_Callback_Overlay_230(void)
{
	sdata->load_inProgress = 0;
	GAME_TRACKER->overlayIndex_Threads = OVERLAY_INDEX_MAIN_MENU;
}

void LOAD_Callback_Overlay_231(void)
{
	sdata->load_inProgress = 0;
	GAME_TRACKER->overlayIndex_Threads = OVERLAY_INDEX_RACING_OR_BATTLE;
}

void LOAD_Callback_Overlay_232(void)
{
	sdata->load_inProgress = 0;
	GAME_TRACKER->overlayIndex_Threads = OVERLAY_INDEX_ADV_HUB;
}

void LOAD_Callback_Overlay_233(void)
{
	sdata->load_inProgress = 0;
	GAME_TRACKER->overlayIndex_Threads = OVERLAY_INDEX_PODIUMS;
}

void LOAD_Callback_MaskHints3D(struct LoadQueueSlot *lqs)
{
	struct Model *model = (struct Model *)lqs->ptrDestination;

	sdata->load_inProgress = 0;
	*(struct Model *volatile *)&sdata->modelMaskHints3D = model;
}

void LOAD_Callback_Podiums(struct LoadQueueSlot *lqs)
{
	struct Model *model = (struct Model *)lqs->ptrDestination;

	sdata->load_inProgress = 0;
	data.podiumModel_podiumStands = model;
}

void LOAD_Callback_LEV(struct LoadQueueSlot *lqs)
{
	if ((lqs->flags & LT_GETADDR) == 0)
	{
		sdata->load_inProgress = 0;
	}

	*(struct Level *volatile *)&sdata->ptrLevelFile = (struct Level *)lqs->ptrDestination;
}

void LOAD_Callback_PatchMem(struct LoadQueueSlot *lqs)
{
	// CTR doesn't load one lev DRAM for AdvHub,
	// it loads one ReadFile for LEV in a sub-mempack,
	// it loads one ReadFile for PtrMap with AllocHighMem

	// that's why the patch map is handled here
	struct DramPointerMap *patchMap = lqs->ptrDestination;
	int patchNum;

	sdata->load_inProgress = 0;
	// NOTE(aalhendi): Retail reads the patch count after clearing the load gate.
	patchNum = patchMap->numBytes >> DRAM_POINTER_MAP_WORD_SHIFT;

	LOAD_RunPtrMap((char *)sdata->ptrLevelFile, DRAM_GETOFFSETS(patchMap), patchNum);

	MEMPACK_SwapPacks(0);
	MEMPACK_ClearHighMem();
	MEMPACK_SwapPacks(GAME_TRACKER->activeMempackIndex);
}

void LOAD_Callback_DriverModels(struct LoadQueueSlot *lqs)
{
	int destination = (int)lqs->ptrDestination;

	sdata->load_inProgress = 0;
	*(volatile int *)&sdata->ptrMPK = destination;
}

void LOAD_HubCallback(struct LoadQueueSlot *lqs)
{
	struct GameTracker *gGT;

	*(volatile int *)&sdata->load_inProgress = 0;
	LOAD_Callback_PatchMem(lqs);

	gGT = GAME_TRACKER;
	gGT->level2 = sdata->ptrLevelFile;
	MEMPACK_SwapPacks(gGT->activeMempackIndex);
}
