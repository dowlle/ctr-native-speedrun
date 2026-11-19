#include <common.h>

void MEMCARD_SetIcon(int icon)
{
	register int *src CTR_PSX_REGISTER("$2");
	register int *dst CTR_PSX_REGISTER("$3");
	register int *srcEnd CTR_PSX_REGISTER("$4");
	register u32 dstPage CTR_PSX_REGISTER("$2");
	int word0;
	int word1;
	int word2;
	int word3;

	// NOTE(aalhendi): Retail forms the hand-icon address from its 0x8008 page.
	CTR_PSX_LOAD_SYMBOL_PAGE(dstPage, "data+19968");
	dst = &CTR_PSX_PAGE_LVALUE(int, dstPage, 0x57a0, GAME_MEMCARD_ICON_HAND[0]);
	if (((u32)icon << 16) != 0)
	{
		src = &GAME_MEMCARD_ICON_CRASH[0];
	}
	else
	{
		src = &GAME_MEMCARD_ICON_GHOST[0];
	}

	srcEnd = src + 0x40;

	do
	{
		word0 = src[0];
		word1 = src[1];
		word2 = src[2];
		word3 = src[3];
		dst[0] = word0;
		dst[1] = word1;
		dst[2] = word2;
		dst[3] = word3;
		// NOTE(aalhendi): Keep all four stores ahead of the source-pointer increment.
		CTR_PSX_MEMORY_BARRIER();
		src += 4;
		dst += 4;
	} while (src != srcEnd);
}
