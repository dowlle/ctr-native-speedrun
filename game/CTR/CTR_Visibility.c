#include <common.h>

// NOTE(aalhendi): These decoders use signed run lengths: negative repeats the
// next byte, positive copies bytes. The unsigned reload followed by a signed
// cast preserves retail's separate byte test and sign extension.
void CTR_unknownMaybeThunk1(void *dst, void *src)
{
	u8 *out = (u8 *)dst;
	s8 *rle = (s8 *)src;
	int count;
	int repeat;
	u8 value;

	while (*rle != 0)
	{
		count = (s8)(u8)*rle;

		if (count < 0)
		{
			repeat = -count + 1;
			rle++;
			value = (u8)*rle++;

			while (repeat != 0)
			{
				*out++ = value;
				repeat--;
			}
		}

		else
		{
			repeat = count;
			rle++;

			while (repeat != 0)
			{
				*out++ = (u8)*rle++;
				repeat--;
			}
		}
	}
}

void CTR_unknownMaybeThunk2(void *dst, void *src)
{
	u8 *out = (u8 *)dst;
	s8 *rle = (s8 *)src;
	int count;
	int repeat;
	u8 value;

	while (*rle != 0)
	{
		count = (s8)(u8)*rle;

		if (count < 0)
		{
			repeat = -count + 1;
			rle++;
			value = (u8)*rle++;

			while (repeat != 0)
			{
				*out++ |= value;
				repeat--;
			}
		}

		else
		{
			repeat = count;
			rle++;

			while (repeat != 0)
			{
				*out++ |= (u8)*rle++;
				repeat--;
			}
		}
	}
}

void CTR_unknownMaybeThunk3(void *dst, void *src, int byteCount)
{
	u32 *out = (u32 *)dst;
	u32 *in = (u32 *)src;
	u32 wordCount = (u32)(byteCount >> 2);

	while (wordCount != 0)
	{
		*out++ |= *in++;
		wordCount--;
	}
}
