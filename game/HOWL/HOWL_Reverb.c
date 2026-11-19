#include <common.h>

void SetReverbMode(u16 newReverbMode)
{
	if (newReverbMode >= 5)
	{
		if (sdata->curReverb != 5)
		{
			// disable reverb and reset mode to 5
			SpuSetReverbModeDepth(0, 0);
			SpuSetReverb(0);
			sdata->curReverb = 5;
			// NOTE(aalhendi): Keep the mode write before the retail return jump.
			CTR_PSX_MEMORY_BARRIER();
		}
		return;
	}
	else if (sdata->curReverb != newReverbMode)
	{
		// update reverb setting if mode has changed
		SpuSetReverbModeDepth(0, 0);
		SpuSetReverb(1);
		SpuSetReverbModeParam(&GAME_HOWL_REVERB_PARAMS[newReverbMode]);
		SpuSetReverbModeDepth(GAME_HOWL_REVERB_PARAMS[newReverbMode].depth.left, GAME_HOWL_REVERB_PARAMS[newReverbMode].depth.right);
		sdata->curReverb = newReverbMode;
	}
}
