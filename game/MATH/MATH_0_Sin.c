#include <common.h>

int MATH_Sin(u32 angle)
{
	struct TrigTable *trigBase = data.trigApprox;
	u32 trig = CTR_ReadU32LE(&trigBase[ANG_MODULO_HALF_PI(angle)]);
	s32 sine;

	if ((angle & ANG_QUADRANT_BIT) == 0)
	{
		trig <<= 0x10;
	}

	sine = (s32)trig >> 0x10;
	if ((angle & ANG_SIGN_BIT) != 0)
	{
		sine = -sine;
	}

	return sine;
}
