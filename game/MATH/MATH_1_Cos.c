#include <common.h>

int MATH_Cos(u32 angle)
{
	struct TrigTable *trigBase = data.trigApprox;
	u32 trig = CTR_ReadU32LE(&trigBase[ANG_MODULO_HALF_PI(angle)]);

	if ((angle & ANG_QUADRANT_BIT) != 0)
	{
		trig = (s32)(trig << 0x10) >> 0x10;
		if ((angle & ANG_SIGN_BIT) == 0)
		{
			trig = -(s32)trig;
		}
	}
	else
	{
		trig = (s32)trig >> 0x10;
		if ((angle & ANG_SIGN_BIT) != 0)
		{
			trig = -(s32)trig;
		}
	}

	return (s32)trig;
}
