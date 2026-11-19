#include <common.h>

void CTR_ErrorScreen(u8 r, u8 g, u8 b)
{
	TILE p;
	int i;

#ifdef CTR_NATIVE
	// NOTE(aalhendi): Native tag helpers read the prior length byte before setlen overwrites it.
	p.tag = 0;
#endif

	for (i = 0; i < 2; i++)
	{
		DrawSync(0);
		VSync(0);
		DISPLAY_Swap();

		p.code = 2;
		termPrim(&p);
		setlen(&p, 3);

		p.r0 = r;
		p.g0 = g;
		p.b0 = b;
		p.w = GAME_TRACKER->frontBuffer->drawEnv.clip.w;
		p.h = GAME_TRACKER->frontBuffer->drawEnv.clip.h;
		p.x0 = GAME_TRACKER->frontBuffer->drawEnv.clip.x;
		p.y0 = GAME_TRACKER->frontBuffer->drawEnv.clip.y;

		DrawOTag(&p);
	}
	DrawSync(0);
	VSync(0);
	DISPLAY_Swap();
}
