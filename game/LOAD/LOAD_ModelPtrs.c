#include <common.h>

void LOAD_GlobalModelPtrs_MPK()
{
	struct GameTracker *gGT = GAME_TRACKER;
	int i;

	for (i = 0; i < LOAD_DRIVER_MODEL_EXTRA_COUNT; i++)
	{
		struct Model *m = GAME_DRIVER_MODEL_EXTRAS[i].model;

		if (m == NULL)
		{
			continue;
		}

		if (m->id == -1)
		{
			continue;
		}

		gGT->modelPtr[m->id] = m;
	}

	if (GAME_PLAYER_OBJECT_LIST != 0)
	{
		// NOTE(aalhendi): Retail reloads the tracker for this call after the model loop.
		LibraryOfModels_Store(GAME_TRACKER_RELOAD(), -1, GAME_PLAYER_OBJECT_LIST);
	}
}

void LOAD_HubSwapPtrs(struct GameTracker *gGT, int unused)
{
	struct Level *oldLev1;
	struct VisMem *oldVisMem1;
	struct VisMem *oldVisMem2;
	// NOTE(aalhendi): Retail passes a second argument that this callee does not use.
	(void)unused;

	// if no secondary lev exists, quit
	if (gGT->level2 == 0)
	{
		return;
	}

	oldLev1 = gGT->level1;
	oldVisMem1 = gGT->visMem1;
	oldVisMem2 = gGT->visMem2;

	gGT->level1 = gGT->level2;
	gGT->boolHubSwapped = 1;

	gGT->level2 = oldLev1;
	gGT->visMem1 = oldVisMem2;
	gGT->visMem2 = oldVisMem1;
}
