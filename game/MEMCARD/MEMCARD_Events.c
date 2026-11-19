#include <common.h>

s32 MEMCARD_GetNextSwEvent(void)
{
	// IOE = IO End, meaning "finished without error"
	if (TestEvent(GAME_MEMCARD_SW_IOE) == 1)
	{
		return MC_RETURN_IOE;
	}
	if (TestEvent(sdata->SwCARD_EvSpERROR) == 1)
	{
		return MC_RETURN_TIMEOUT;
	}
	if (TestEvent(sdata->SwCARD_EvSpTIMOUT) == 1)
	{
		return MC_RETURN_NOCARD;
	}
	if (TestEvent(sdata->SwCARD_EvSpNEW) == 1)
	{
		return MC_RETURN_NEWCARD;
	}

	return MC_RETURN_PENDING;
}

s32 MEMCARD_GetNextHwEvent(void)
{
	// IOE = IO End, meaning "finished without error"
	if (TestEvent(GAME_MEMCARD_HW_IOE) == 1)
	{
		return MC_RETURN_IOE;
	}
	if (TestEvent(sdata->HwCARD_EvSpERROR) == 1)
	{
		return MC_RETURN_TIMEOUT;
	}
	if (TestEvent(sdata->HwCARD_EvSpTIMOUT) == 1)
	{
		return MC_RETURN_NOCARD;
	}
	if (TestEvent(sdata->HwCARD_EvSpNEW) == 1)
	{
		return MC_RETURN_NEWCARD;
	}

	return MC_RETURN_PENDING;
}

s32 MEMCARD_WaitForHwEvent(void)
{
	while (1)
	{
		// IOE = IO End, meaning "finished without error"
		// NOTE(aalhendi): Retail recognizes the signaled state only when TestEvent returns 1.
		if (TestEvent(sdata->HwCARD_EvSpIOE) == 1)
		{
			return MC_RETURN_IOE;
		}
		if (TestEvent(sdata->HwCARD_EvSpERROR) == 1)
		{
			return MC_RETURN_TIMEOUT;
		}
		if (TestEvent(sdata->HwCARD_EvSpTIMOUT) == 1)
		{
			return MC_RETURN_NOCARD;
		}
		if (TestEvent(sdata->HwCARD_EvSpNEW) == 1)
		{
			return MC_RETURN_NEWCARD;
		}

		// Not allowed to return PENDING, the goal is to
		// wait until the memcard is not PENDING anymore
		// return MC_RETURN_PENDING;
	}
}

s32 MEMCARD_SkipEvents(void)
{
	// Flush all "previous" Events until everything shows PENDING
	while (MEMCARD_GetNextSwEvent() != MC_RETURN_PENDING)
	{
		;
	}
	while (MEMCARD_GetNextHwEvent() != MC_RETURN_PENDING)
	{
		;
	}
	return MC_RETURN_PENDING;
}
