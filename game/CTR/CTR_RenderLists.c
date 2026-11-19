#include <common.h>

// NOTE(aalhendi): Keep the slot writes explicit. Retail reloads the player's rendered-block
// pointer after each slot, so caching it across the writes changes the MIPS code.
void CTR_ClearRenderLists_1P2P(struct GameTracker *gGT, int numPlyrCurrGame)
{
	int i;
	void **rendered;
	void *quadBlocksRendered;

	if (numPlyrCurrGame <= 0)
	{
		return;
	}

	i = 0;
	rendered = data.ptrRenderedQuadblockDestination_forEachPlayer;
	do
	{
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[0].bspListStart = 0;
		gGT->LevRenderLists[i].list[0].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[1].bspListStart = 0;
		gGT->LevRenderLists[i].list[1].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[2].bspListStart = 0;
		gGT->LevRenderLists[i].list[2].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[3].bspListStart = 0;
		gGT->LevRenderLists[i].list[3].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		rendered++;
		gGT->LevRenderLists[i].list[4].bspListStart = 0;
		gGT->LevRenderLists[i].list[4].ptrQuadBlocksRendered = quadBlocksRendered;

		gGT->LevRenderLists[i].bspListStart_FullDynamic = 0;
		gGT->LevRenderLists[i].ptrQuadBlocksRendered_FullDynamic = 0;
		i++;
	} while (i < numPlyrCurrGame);
}

void CTR_ClearRenderLists_3P4P(struct GameTracker *gGT, int numPlyrCurrGame)
{
	int i;
	void **rendered;
	void *quadBlocksRendered;

	if (numPlyrCurrGame <= 0)
	{
		return;
	}

	i = 0;
	rendered = data.ptrRenderedQuadblockDestination_again;
	do
	{
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[0].bspListStart = 0;
		gGT->LevRenderLists[i].list[0].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[1].bspListStart = 0;
		gGT->LevRenderLists[i].list[1].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		gGT->LevRenderLists[i].list[2].bspListStart = 0;
		gGT->LevRenderLists[i].list[2].ptrQuadBlocksRendered = quadBlocksRendered;
		quadBlocksRendered = *rendered;
		rendered++;
		gGT->LevRenderLists[i].list[3].bspListStart = 0;
		gGT->LevRenderLists[i].list[3].ptrQuadBlocksRendered = quadBlocksRendered;

		gGT->LevRenderLists[i].list[4].ptrQuadBlocksRendered = 0;
		i++;
	} while (i < numPlyrCurrGame);
}

void CTR_EmptyFunc_MainFrame_ResetDB(void)
{
}
