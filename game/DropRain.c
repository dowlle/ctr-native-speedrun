#include <common.h>


void DropRain_MakeSound(struct GameTracker *gGT)
{
	int i, lev;
	u32 rained;

	rained = 0;
	// NOTE(aalhendi): Retail gates the effect using the active tracker; the rain buffers still come from the passed tracker.
	lev = GAME_TRACKER->levelID;

	// if you are not in
	if ((lev != TIGER_TEMPLE) && (lev != CORTEX_CASTLE))
	{
		return;
	}

	for (i = 0; i < gGT->numPlyrCurrGame; i++)
	{
		rained |= gGT->rainBuffer[i].numParticles_curr;
	}

	// if someone is rained on
	if (rained != 0)
	{
		// if there is no rain
		if (gGT->rainSoundID == 0)
		{
			gGT->rainSoundID = OtherFX_Play(0x82, 0);
		}
	}

	// if nobody is rained on
	else
	{
		if (gGT->rainSoundID != 0)
		{
			OtherFX_Stop1(gGT->rainSoundID);
			gGT->rainSoundID = 0;
		}
	}
	return;
}


void DropRain_Reset(struct GameTracker *gGT)
{
	gGT->rainSoundID = 0;
	return;
}
