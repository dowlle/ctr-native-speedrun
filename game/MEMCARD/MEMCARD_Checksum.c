#include <common.h>

u32 MEMCARD_CRC16(u32 crc, int nextByte)
{
	int i;

	for (i = 7; i >= 0; i--)
	{
		crc <<= 1;
		crc |= (nextByte >> i) & 1;

		if ((crc & 0x10000) != 0)
		{
			crc = crc ^ 0x11021;
		}
	}

	return crc;
}

void MEMCARD_ChecksumSave(u8 *saveBytes, int len)
{
	int i;
	int crc;
	int end;
	// NOTE(aalhendi): Retail recomputes each checksum-byte address in v0.
	register u8 *checksumByte CTR_PSX_REGISTER("$2");

	crc = 0;
	// NOTE(aalhendi): Seed the index from the zero CRC register as retail does.
	CTR_PSX_COPY_VALUE(i, crc);
	len -= 2;

	if (len > 0)
	{
		CTR_PSX_COPY_VALUE(end, len);
		do
		{
			crc = MEMCARD_CRC16(crc, saveBytes[i]);
			i++;
		} while (i < end);
	}

	sdata->crc16_checkpoint_status = crc;
	// NOTE(aalhendi): Commit the checkpoint before the two finishing CRC calls.
	CTR_PSX_MEMORY_BARRIER();

	// finishing check
	crc = MEMCARD_CRC16(crc, 0);
	crc = MEMCARD_CRC16(crc, 0);

	// write checksum to data (last 2 bytes),
	// swap endians to throw off hackers,
	// which didn't really throw anyone off at all
	CTR_PSX_ADD_POINTER_OFFSET(checksumByte, saveBytes, i);
	checksumByte[0] = (u8)(crc >> 8);
	CTR_PSX_ADD_POINTER_OFFSET_OFFSET_FIRST(checksumByte, saveBytes, i);
	checksumByte[1] = (u8)crc;
}
