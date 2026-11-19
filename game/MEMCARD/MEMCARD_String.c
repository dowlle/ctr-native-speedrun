#include <common.h>

char *MEMCARD_StringInit(int slotIndex, char *dstString)
{
	u32 firstDigit;
	u32 secondDigit;
	// NOTE(aalhendi): Keep both halves of the retail fallback address in a1.
	register char *defaultString CTR_PSX_REGISTER("$5");

	if (!dstString)
	{
		defaultString = GAME_MEMCARD_DIR_HEADER;
		CTR_PSX_KEEP_VALUE_RELAXED(defaultString);
		dstString = defaultString;
	}
	// NOTE(aalhendi): Keep digit formation after the fallback branch as in retail.
	CTR_PSX_MEMORY_BARRIER();

	firstDigit = ('0' + ((slotIndex >> 4) & 1)) << 16;
	secondDigit = (('0' + (slotIndex & 3)) << 24) | 0x7562;
	CTR_WriteU32AlignedLE(dstString, firstDigit | secondDigit);
	// NOTE(aalhendi): Native char buffers need byte writes; retail uses one halfword store.
#ifdef CTR_NATIVE
	CTR_WriteU16LE(dstString + 4, 0x3a);
#else
	*(u16 *)(dstString + 4) = 0x3a;
#endif
	return dstString;
}

void MEMCARD_StringSet(char *dstString, int slotIdx, char *srcString)
{
	int i;
	int j;
	MEMCARD_StringInit(slotIdx, dstString);

	// fast strlen
	for (i = 0; dstString[i] != '\0'; i++)
	{
	}

	// copy string from src to dst
	for (j = 0; (srcString[j] != '\0' && i < 63); j++)
	{
		dstString[i] = srcString[j];
		i++;
	}

	// nullptr
	dstString[i] = '\0';
	return;
}
