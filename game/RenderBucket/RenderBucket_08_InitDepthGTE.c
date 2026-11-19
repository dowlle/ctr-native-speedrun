#include <common.h>

#ifdef CTR_NATIVE
void RenderBucket_InitDepthGTE(void)
{
	CTC2(0, 27);
	CTC2(0, 28);
	CTC2(0x555, 29);
	CTC2(0x400, 30);
}
#else
// NOTE(aalhendi): Retail writes the final GTE register in the return delay slot.
__asm__(".section .RenderBucket_InitDepthGTE,\"ax\",@progbits\n"
        ".align 2\n"
        ".ent RenderBucket_InitDepthGTE\n"
        ".set noreorder\n"
        ".globl RenderBucket_InitDepthGTE\n"
        "RenderBucket_InitDepthGTE:\n"
        "ctc2 $zero,$27\n"
        "ctc2 $zero,$28\n"
        "li $v1,0x555\n"
        "ctc2 $v1,$29\n"
        "li $v1,0x400\n"
        "jr $ra\n"
        "ctc2 $v1,$30\n"
        ".end RenderBucket_InitDepthGTE\n");
#endif
