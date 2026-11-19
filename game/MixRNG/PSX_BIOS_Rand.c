#include <common.h>

#ifdef CTR_NATIVE
static u32 psxRandSeed = 1;

// NOTE(aalhendi): Native reproduces the PSX BIOS generator and exposes its
// seed to the checkpoint system. Retail calls BIOS A(2F) instead.
int rand(void)
{
	psxRandSeed = psxRandSeed * 0x41c64e6dU + 0x3039U;
	return (int)((psxRandSeed >> 16) & 0x7fffU);
}

void srand(unsigned int seed)
{
	psxRandSeed = (u32)seed;
}

u32 PSX_BIOS_GetRandSeed(void)
{
	return psxRandSeed;
}

void PSX_BIOS_SetRandSeed(u32 seed)
{
	psxRandSeed = seed;
}
#else
// NOTE(aalhendi): Retail's 16-byte rand entry is a BIOS A(2F) call stub.
// It jumps to the A0 vector with the function number in the delay slot.
__asm__(".section .rand,\"ax\",@progbits\n"
        ".align 2\n"
        ".ent rand\n"
        ".set noreorder\n"
        ".globl rand\n"
        "rand:\n"
        "li $t2,0xa0\n"
        "jr $t2\n"
        "li $t1,0x2f\n"
        "nop\n"
        ".end rand\n"
        ".set reorder\n"
        ".text\n");
#endif
