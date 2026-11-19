#include <common.h>

// packID will always be 3-GAME_TRACKER->activeMempackIndex
void LOAD_Hub_ReadFile(struct BigHeader *bigfile, int levID, int packID)
{
	register int levelToLoad CTR_PSX_REGISTER("$17") = levID;
#if !defined(CTR_NATIVE)
	register int subfileIndex CTR_PSX_REGISTER("$2");
#endif

	// if level is already loaded, quit
	if (GAME_TRACKER->levID_in_each_mempack[packID] == levelToLoad)
	{
		return;
	}

	sdata->modelMaskHints3D = 0;
	CTR_PSX_MEMORY_BARRIER();

	// Swap to pack of hub you're NOT on,
	// wipe the pack to reload the new hub
	MEMPACK_SwapPacks(packID);
	MEMPACK_ClearLowMem();

#if defined(CTR_NATIVE)
	sdata->load_inProgress = 1;
	GAME_TRACKER->level2 = 0;
	GAME_TRACKER->levID_in_each_mempack[packID] = levelToLoad;
#else
	{
		register struct GameTracker *gGT CTR_PSX_REGISTER("$2") = GAME_TRACKER;
		register int levelID CTR_PSX_REGISTER("$4") = levelToLoad;
		register int loadingFlag CTR_PSX_REGISTER("$5") = 1;
		register int fileType CTR_PSX_REGISTER("$6") = LVI_VRAM;
		// NOTE(aalhendi): Retail stores from a1 after an otherwise dead copy to v1.
		__asm__ volatile("move $3,$5\n\tsw $5,%0" : "=m"(sdata->load_inProgress) : "r"(gGT), "r"(levelID), "r"(loadingFlag), "r"(fileType) : "$3");
		gGT->level2 = 0;
		gGT->levID_in_each_mempack[packID] = levelToLoad;
	}
#endif

	LOAD_AppendQueue(bigfile, LT_VRAM, LOAD_GetBigfileIndex(levelToLoad, LOAD_LEVEL_LOD_1P, LVI_VRAM), NULL, NULL);
	LOAD_AppendQueue(bigfile, LT_GETADDR, LOAD_GetBigfileIndex(levelToLoad, LOAD_LEVEL_LOD_1P, LVI_LEV), NULL, LOAD_Callback_LEV);
#if defined(CTR_NATIVE)
	LOAD_AppendQueue(bigfile, LT_SETADDR, LOAD_GetBigfileIndex(levelToLoad, LOAD_LEVEL_LOD_1P, LVI_PTR), sdata->PatchMem_Ptr, LOAD_HubCallback);
#else
	// NOTE(aalhendi): Retail stores the fifth argument in the call delay slot;
	// GCC 2.8.1 otherwise stores it early and schedules the index move there.
	subfileIndex = LOAD_GetBigfileIndex(levelToLoad, LOAD_LEVEL_LOD_1P, LVI_PTR);
	__asm__ volatile(".set\tnoreorder\n\t"
	                 "move $4,%1\n\t"
	                 "li $5,%3\n\t"
	                 "move $6,%0\n\t"
	                 "lui $2,%%hi(LOAD_HubCallback)\n\t"
	                 "lw $7,%2\n\t"
	                 "addiu $2,$2,%%lo(LOAD_HubCallback)\n\t"
	                 "jal LOAD_AppendQueue\n\t"
	                 "sw $2,16($sp)\n\t"
	                 ".set\treorder"
	                 : "+r"(subfileIndex)
	                 : "r"(bigfile), "m"(sdata->PatchMem_Ptr), "i"(LT_SETADDR)
	                 : "$3", "$4", "$5", "$6", "$7", "$31", "memory");
#endif
}

void LOAD_Hub_SwapNow()
{
	// stall until load is done
	while (GAME_TRACKER->level2 == 0)
	{
		LOAD_NextQueuedFile();
		VSync(0);
	}

	LevInstDef_RePack(GAME_TRACKER->level1->ptr_mesh_info, 1);

	LOAD_HubSwapPtrs(GAME_TRACKER, 1);

	// 0,1,2
	GAME_TRACKER->activeMempackIndex = LOAD_HUB_MEMPACK_PAIR_INDEX_SUM - GAME_TRACKER->activeMempackIndex;

	GAME_TRACKER->prevLEV = GAME_TRACKER->levelID;
	GAME_TRACKER->levelID = GAME_TRACKER->levID_in_each_mempack[GAME_TRACKER->activeMempackIndex];

	Audio_AdvHub_SwapSong(GAME_TRACKER->levelID);

	LibraryOfModels_Clear(GAME_TRACKER);

	if (GAME_PLAYER_OBJECT_LIST != 0)
	{
		LOAD_GlobalModelPtrs_MPK();
	}

	if (GAME_TRACKER->level1 != 0)
	{
		LibraryOfModels_Store(GAME_TRACKER, GAME_TRACKER->level1->numModels, GAME_TRACKER->level1->ptrModelsPtrArray);

		INSTANCE_LevInitAll(GAME_TRACKER->level1->ptrInstDefs, GAME_TRACKER->level1->numInstances);

		LevInstDef_UnPack(GAME_TRACKER->level1->ptr_mesh_info);

		DecalGlobal_Store(GAME_TRACKER, GAME_TRACKER->level1->levTexLookup);
	}

	MEMPACK_SwapPacks(GAME_TRACKER->activeMempackIndex);
	MainInit_VisMem(GAME_TRACKER);

	GAME_TRACKER->cameraDC[0].ptrQuadBlock = 0;
	GAME_TRACKER->cameraDC[0].visInstSrc = 0;
	GAME_TRACKER->cameraDC[0].visLeafSrc = 0;
	GAME_TRACKER->cameraDC[0].visFaceSrc = 0;
	GAME_TRACKER->cameraDC[0].visOVertSrc = 0;
	GAME_TRACKER->cameraDC[0].visSCVertSrc = 0;

	GAME_TRACKER->visMem1->visLeafSrc[0] = 0;
	GAME_TRACKER->visMem1->visFaceSrc[0] = 0;
	GAME_TRACKER->visMem1->visOVertSrc[0] = 0;
	GAME_TRACKER->visMem1->visSCVertSrc[0] = 0;

	GAME_TRACKER->drivers[0]->underDriver = 0;

	GAME_TRACKER->framesInThisLEV = 0;
	GAME_TRACKER->msInThisLEV = 0;
}

void LOAD_Hub_Main(struct BigHeader *bigfilePtr)
{
	// NOTE(aalhendi): Retail copies this table from rdata to the stack before scanning players.
	int connectedLevID[LOAD_ADV_HUB_COUNT][LOAD_ADV_HUB_CONNECTION_COUNT] = {
	    {N_SANITY_BEACH, THE_LOST_RUINS, -1},
	    {GEM_STONE_VALLEY, GLACIER_PARK, -1},
	    {GEM_STONE_VALLEY, GLACIER_PARK, -1},
	    {N_SANITY_BEACH, THE_LOST_RUINS, CITADEL_CITY},
	    {GLACIER_PARK, -1, -1},
	};
	int i;

	for (i = 0; i < GAME_TRACKER->numPlyrCurrGame; i++)
	{
		s32 stepFlagSet;
		int nextLevelID;
		int needSwapNow;

		if (GAME_LOADING_STAGE != LOAD_IDLE)
		{
			continue;
		}

		stepFlagSet = GAME_TRACKER->drivers[i]->stepFlagSet;
		nextLevelID = (stepFlagSet & COLL_STEP_TRIGGER_HUB_LEVEL_ID_MASK) >> COLL_STEP_TRIGGER_HUB_LEVEL_ID_SHIFT;
		needSwapNow = (stepFlagSet & COLL_STEP_TRIGGER_HUB_SWAP_NOW_MASK) >> COLL_STEP_TRIGGER_HUB_SWAP_NOW_SHIFT;

		if (nextLevelID != LOAD_HUB_TRIGGER_NONE)
		{
			u32 currLevelID = GAME_TRACKER->levelID - GEM_STONE_VALLEY;

			// Only AdvHub levels 0-4 have connections; other modes can reach this path.
			if (currLevelID < LOAD_ADV_HUB_COUNT)
			{
				LOAD_Hub_ReadFile(bigfilePtr, connectedLevID[currLevelID][nextLevelID - LOAD_HUB_TRIGGER_ID_BIAS],
				                  LOAD_HUB_MEMPACK_PAIR_INDEX_SUM - GAME_TRACKER->activeMempackIndex);
			}
		}
		else
		{
			if ((needSwapNow != 0) || (GAME_TRACKER->bool_AdvHub_NeedToSwapLEV != 0))
			{
				GAME_TRACKER->bool_AdvHub_NeedToSwapLEV = 0;
				LOAD_Hub_SwapNow();
			}
		}
	}
}
