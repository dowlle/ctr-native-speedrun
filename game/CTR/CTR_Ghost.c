#include <common.h>

// NOTE(aalhendi): Keep the byte in a word-sized local for retail's OR operand
// order, but narrow each source read so native char signedness cannot change it.
void CTR_ScrambleGhostString(char *dst, const char *src)
{
	u32 inputByte = (u8)*src;
	u32 key;
	u16 *entry;
	u16 entryKey;
	u16 encoded;

	while (inputByte != '\0')
	{
		key = 0;

		if (inputByte < 4)
		{
			inputByte = (u8)*src++;
			key = (u32)inputByte << 8;
		}

		inputByte = (u8)*src++;

		entry = &data.ghostScrambleData[0];
		entryKey = entry[1];
		key |= inputByte;
		while (entryKey != 0xffff)
		{
			if (entryKey == (key & 0xffff))
			{
				encoded = entry[0];
				if ((encoded & 0xff00) != 0)
				{
					*dst++ = encoded >> 8;
					*dst++ = encoded;
					break;
				}
			}

			entry += 2;
			entryKey = entry[1];
		}

		inputByte = (u8)*src;
	}

	*dst = '\0';
}
