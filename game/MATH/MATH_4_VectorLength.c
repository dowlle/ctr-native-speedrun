#include <common.h>

s32 MATH_VectorLength(SVec3 *vector)
{
	register u32 xy CTR_PSX_REGISTER("$12");
	register s32 z CTR_PSX_REGISTER("$13");

	xy = CTR_ReadU32LE(&vector->x);
	z = vector->z;

	CTC2(xy, 0);
	CTC2((u32)z, 1);

	// NOTE(aalhendi): Retail reloads the vector before writing the GTE data registers.
	CTR_PSX_MEMORY_BARRIER();
	xy = CTR_ReadU32LE(&vector->x);
	z = vector->z;
	MTC2(xy, 0);
	MTC2(z, 1);
	CTR_GteLoadDelay();
	gte_mvmva(0, 0, 0, 3, 0);

	return SquareRoot0_stub((s32)MFC2(25));
}
