#include <common.h>

void LOAD_AppendQueue(struct BigHeader *bigfile, int type, int fileIndex, void *destinationPtr, void (*callback)(struct LoadQueueSlot *))
{
	register struct LoadQueueSlot *lqs CTR_PSX_REGISTER("$2");
	register struct LoadQueueSlot *queueBase CTR_PSX_REGISTER("$3");
	s16 queueLength = sdata->queueLength;
	u16 nextQueueLength = (u16)sdata->queueLength;
	s32 slotOffset;

	// NOTE(aalhendi): Retail reads the length as signed for indexing and unsigned for the final increment.
	if (queueLength >= LOAD_QUEUE_SLOT_COUNT)
	{
		return;
	}

#ifndef CTR_NATIVE
	// NOTE(aalhendi): GCC places this page load in the branch delay slot; the low relocation follows it.
	queueBase = (struct LoadQueueSlot *)(OFFSETOF_SDATA(queueSlots) & 0xffff0000u);
#endif
	CTR_PSX_ADD_SYMBOL_LOW_IN_PLACE(queueBase, RETAIL_QUEUE_SLOTS_ASM_NAME, &sdata->queueSlots[0]);
	slotOffset = queueLength * sizeof(*lqs);
	CTR_PSX_ADD_POINTER_OFFSET_OFFSET_FIRST(lqs, queueBase, slotOffset);
	lqs->flags = 0;
	lqs->ptrBigfileCdPos_UNUSED = bigfile;
	lqs->type_UNUSED = type;
	lqs->subfileIndex = fileIndex;
	lqs->ptrDestination = destinationPtr;
	lqs->size_UNUSED = 0;
	lqs->callbackFuncPtr = callback;

	sdata->queueLength = nextQueueLength + 1;
}

#if defined(CTR_NATIVE)
void LOAD_CDRequestCallback(struct LoadQueueSlot *lqs)
{
	if (lqs->callbackFuncPtr != NULL)
	{
		lqs->callbackFuncPtr(lqs);
	}

	sdata->queueReady = 1;
}
#else
// NOTE(aalhendi): Retail restores ra before the queueReady store, filling the
// load-delay slot. GCC 2.8.1 places the store first and emits an extra nop.
// maspsx recognizes the tab-separated .set directives when handling delay slots.
__asm__(".section .LOAD_CDRequestCallback,\"ax\",@progbits\n"
        ".align 2\n"
        ".ent LOAD_CDRequestCallback\n"
        ".set\tnoreorder\n"
        ".globl LOAD_CDRequestCallback\n"
        "LOAD_CDRequestCallback:\n"
        "addiu $29,$29,-24\n"
        "sw $31,16($29)\n"
        "lw $2,20($4)\n"
        "nop\n"
        "beq $2,$0,.LLOAD_CDRequestCallback_ready\n"
        "nop\n"
        "jalr $2\n"
        "nop\n"
        ".LLOAD_CDRequestCallback_ready:\n"
        "lw $31,16($29)\n"
        "li $2,1\n"
        "sb $2,308($28)\n"
        "jr $31\n"
        "addiu $29,$29,24\n"
        ".end LOAD_CDRequestCallback\n"
        ".set\treorder\n"
        ".text\n");
#endif

#ifdef CTR_NATIVE
void LOAD_NextQueuedFile()
#else
// NOTE(aalhendi): Retail tests queueReady before the stack frame is opened.
// The entry load falls through to the C body in the same function section.
__asm__(".section .LOAD_NextQueuedFile,\"ax\",@progbits\n"
        ".align 2\n"
        ".globl LOAD_NextQueuedFile\n"
        ".type LOAD_NextQueuedFile,@function\n"
        "LOAD_NextQueuedFile:\n"
        "lbu $2,308($28)\n"
        ".text\n");
void LOAD_QueueNextBody() CTR_PSX_MATCH_SECTION(".LOAD_NextQueuedFile");
void LOAD_QueueNextBody()
#endif
{
	struct LoadQueueSlot *curr;
	register int i CTR_PSX_REGISTER("$5");
	register int ready CTR_PSX_REGISTER("$2");

#ifdef CTR_NATIVE
	ready = sdata->queueReady;
#else
	CTR_PSX_CAPTURE_REGISTER(ready, sdata->queueReady);
#endif

	if ((ready != 0) && (CTR_PSX_PAGE_LVALUE(XAState, OFFSETOF_SDATA(XA_State), 0, sdata->XA_State) == 0) && (sdata->queueLength != 0))
	{
		int retry = sdata->queueRetry;
		register u32 destPage CTR_PSX_REGISTER("$4");
		sdata->queueReady = 0;
		CTR_PSX_MEMORY_BARRIER();
		i = LOAD_QUEUE_FIRST_PENDING_SLOT;

		if (retry == 0)
		{
			int pendingLength;
			{
				register CtrPackedU32 *destWords CTR_PSX_REGISTER("$10");
				register CtrPackedU32 *sourceWords CTR_PSX_REGISTER("$11");
				register u32 word0 CTR_PSX_REGISTER("$8");
				register u32 word1 CTR_PSX_REGISTER("$9");
				register u32 sourcePage CTR_PSX_REGISTER("$3");
				destPage = OFFSETOF_DATA(currSlot) & 0xffff0000u;
				pendingLength = sdata->queueLength;
				CTR_PSX_LOAD_SYMBOL_PAGE_AFTER(sourcePage, RETAIL_QUEUE_SLOTS_ASM_NAME, pendingLength);
				CTR_PSX_ADD_SYMBOL_LOW(sourceWords, sourcePage, RETAIL_QUEUE_SLOTS_ASM_NAME, (CtrPackedU32 *)&sdata->queueSlots[0]);
				CTR_PSX_ADD_SYMBOL_LOW(destWords, destPage, RETAIL_CURR_SLOT_ASM_NAME, (CtrPackedU32 *)&data.currSlot);
				CTR_PSX_OBSERVE_VALUE(destWords);
				word0 = sourceWords[0];
				word1 = sourceWords[1];
				destWords[0] = word0;
				destWords[1] = word1;
				word0 = sourceWords[2];
				word1 = sourceWords[3];
				destWords[2] = word0;
				destWords[3] = word1;
				word0 = sourceWords[4];
				word1 = sourceWords[5];
#ifdef CTR_NATIVE
				destWords[4] = word0;
				destWords[5] = word1;
#else
				// NOTE(aalhendi): The final pair precedes the loop test in retail;
				// GCC otherwise moves the stores past its compare.
				__asm__("sw %1,16(%2)\n\tsw %3,20(%2)" : "+r"(pendingLength) : "r"(word0), "r"(destWords), "r"(word1));
#endif
			}

			if (i < pendingLength)
			{
				register struct LoadQueueSlot *copyDst CTR_PSX_REGISTER("$4");
				register struct LoadQueueSlot *copySrc CTR_PSX_REGISTER("$3");
				register char *copyBase CTR_PSX_REGISTER("$2");
#ifdef CTR_NATIVE
				copyDst = &sdata->queueSlots[0];
				copySrc = &sdata->queueSlots[1];
#else
				copyBase = (char *)(OFFSETOF_SDATA(queueSlots) & 0xffff0000u);
				CTR_PSX_ADD_SYMBOL_LOW_IN_PLACE(copyBase, RETAIL_QUEUE_SLOTS_ASM_NAME "-24", &sdata->queueSlots[0]);
				copyDst = (struct LoadQueueSlot *)(copyBase + sizeof(*copyDst));
				copySrc = (struct LoadQueueSlot *)(copyBase + 2 * sizeof(*copySrc));
#endif
				do
				{
#ifdef CTR_NATIVE
					*copyDst = *copySrc;
#else
					// NOTE(aalhendi): Retail copies each pending slot as four words,
					// then two words; the compiler's struct copy changes this order.
					__asm__ volatile("lw $8,0(%0)\n\t"
					                 "lw $9,4(%0)\n\t"
					                 "lw $10,8(%0)\n\t"
					                 "lw $11,12(%0)\n\t"
					                 "sw $8,0(%1)\n\t"
					                 "sw $9,4(%1)\n\t"
					                 "sw $10,8(%1)\n\t"
					                 "sw $11,12(%1)\n\t"
					                 "lw $8,16(%0)\n\t"
					                 "lw $9,20(%0)\n\t"
					                 "sw $8,16(%1)\n\t"
					                 "sw $9,20(%1)"
					                 :
					                 : "r"(copySrc), "r"(copyDst)
					                 : "$8", "$9", "$10", "$11", "memory");
#endif
					copyDst++;
					copySrc++;
					i++;
				} while (i < sdata->queueLength);
				destPage = OFFSETOF_DATA(currSlot) & 0xffff0000u;
			}
		}
		else
		{
			sdata->queueRetry = 0;
			destPage = OFFSETOF_DATA(currSlot) & 0xffff0000u;
		}

#ifdef CTR_NATIVE
		curr = &data.currSlot;
#else
		CTR_PSX_ADD_SYMBOL_LOW(curr, destPage, RETAIL_CURR_SLOT_ASM_NAME, &data.currSlot);
#endif
		switch (curr->type_UNUSED)
		{
		case LT_RAW:
		{
			register u32 requestType CTR_PSX_REGISTER("$5");
			register u32 subfileIndex CTR_PSX_REGISTER("$6");
			register void *destination CTR_PSX_REGISTER("$7");
			register u32 *sizePtr CTR_PSX_REGISTER("$2");
			register struct BigHeader *bigfile CTR_PSX_REGISTER("$4");
			register void *readResult CTR_PSX_REGISTER("$2");
			requestType = LT_SETADDR;
			CTR_PSX_OBSERVE_VALUE(requestType);
			subfileIndex = curr->subfileIndex;
			destination = curr->ptrDestination;
			sizePtr = &curr->size_UNUSED;
#ifdef CTR_NATIVE
			bigfile = CTR_PSX_PAGE_LVALUE(struct BigHeader *, destPage, OFFSETOF_DATA(currSlot) & 0xffffu, curr->ptrBigfileCdPos_UNUSED);
			curr->ptrDestination = LOAD_ReadFile_ex(bigfile, requestType, subfileIndex, destination, sizePtr, LOAD_CDRequestCallback);
#else
			// NOTE(aalhendi): GCC hoists the bigfile load ahead of the outgoing
			// stack arguments. This call setup keeps the retail delay slot/order.
			__asm__ volatile(".set\tnoreorder\n\t"
			                 "addiu $2,%4,16\n\t"
			                 "sw $2,16($29)\n\t"
			                 "lui $2,%%hi(LOAD_CDRequestCallback)\n\t"
			                 "lw $4,%%lo(" RETAIL_CURR_SLOT_ASM_NAME ")(%5)\n\t"
			                 "addiu $2,$2,%%lo(LOAD_CDRequestCallback)\n\t"
			                 "jal LOAD_ReadFile_ex\n\t"
			                 "sw $2,20($29)\n\t"
			                 ".set\treorder"
			                 : "=r"(readResult)
			                 : "r"(requestType), "r"(subfileIndex), "r"(destination), "r"(curr), "r"(destPage)
			                 : "$31", "memory");
			curr->ptrDestination = readResult;
#endif
			break;
		}

		case LT_DRAM:
		{
			register struct BigHeader *bigfile CTR_PSX_REGISTER("$4") =
			    CTR_PSX_PAGE_LVALUE(struct BigHeader *, destPage, OFFSETOF_DATA(currSlot) & 0xffffu, curr->ptrBigfileCdPos_UNUSED);
			register u32 subfileIndex CTR_PSX_REGISTER("$5") = curr->subfileIndex;
			register void *destination CTR_PSX_REGISTER("$6") = curr->ptrDestination;
			register u32 *sizePtr CTR_PSX_REGISTER("$7") = &curr->size_UNUSED;
			register u32 callback CTR_PSX_REGISTER("$2") = (u32)(s32)curr->callbackFuncPtr;
			curr->ptrDestination = LOAD_DramFile(bigfile, subfileIndex, destination, sizePtr, (int)callback);
			break;
		}

		case LT_VRAM:
		{
			register u32 *sizePtr CTR_PSX_REGISTER("$7") = &curr->size_UNUSED;
			register struct BigHeader *bigfile CTR_PSX_REGISTER("$4") =
			    CTR_PSX_PAGE_LVALUE(struct BigHeader *, destPage, OFFSETOF_DATA(currSlot) & 0xffffu, curr->ptrBigfileCdPos_UNUSED);
			register u32 subfileIndex CTR_PSX_REGISTER("$5") = curr->subfileIndex;
			register void *destination CTR_PSX_REGISTER("$6") = curr->ptrDestination;
			register u32 callback CTR_PSX_REGISTER("$2");
			CTR_PSX_OBSERVE_VALUE(sizePtr);
			callback = (u32)(s32)curr->callbackFuncPtr;
			curr->ptrDestination = LOAD_VramFile(bigfile, subfileIndex, destination, sizePtr, (int)callback);
			break;
		}
		}

		sdata->queueLength--;
	}

	{
		register u32 frameFinished CTR_PSX_REGISTER("$3") = sdata->frameFinishedVRAM;
		if (frameFinished != 0)
		{
			if ((u32)(CTR_PSX_PAGE_LVALUE(struct GameTracker *, OFFSETOF_SDATA(gGT), 0, sdata->gGT)->frameTimer_VsyncCallback - frameFinished) >=
			    LOAD_QUEUE_VRAM_CALLBACK_DELAY_FRAMES)
			{
				u16 flags;
				curr = &data.currSlot;
				if (curr->callbackFuncPtr != NULL)
				{
					curr->callbackFuncPtr(curr);
				}

				flags = curr->flags;
				sdata->frameFinishedVRAM = 0;
#ifndef CTR_NATIVE
				__asm__ volatile("" : "+r"(flags) : : "memory");
#endif

#if defined(CTR_NATIVE)
				// NOTE(aalhendi): CTR_NATIVE marks Mempack allocations with a host-only
				// flag while the retail path uses LT_SETADDR for the same ownership.
				if ((flags & (LT_SETADDR | LT_MEMPACK)) != 0)
#else
				if ((flags & LT_SETADDR) != 0)
#endif
				{
					MEMPACK_PopState();
				}

				sdata->queueReady = 1;
			}
		}
	}
}

#ifndef CTR_NATIVE
__asm__(".section .LOAD_NextQueuedFile,\"ax\",@progbits\n"
        ".size LOAD_NextQueuedFile,.-LOAD_NextQueuedFile\n"
        ".text\n");
#endif
