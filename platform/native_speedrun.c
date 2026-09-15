#include <platform/native_speedrun.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIVE_SPEEDRUN_WALL_DELTA_MAX_MS 60000u

// Level identity of the main menu, used to detect a run reset.
#define NATIVE_SPEEDRUN_MAIN_MENU_LEVEL 0x27

internal u32 NativeSpeedrun_WallDelta(u64 now, u64 prev)
{
	if (now <= prev)
	{
		return 0;
	}

	u64 delta = now - prev;
	if (delta > NATIVE_SPEEDRUN_WALL_DELTA_MAX_MS)
	{
		delta = NATIVE_SPEEDRUN_WALL_DELTA_MAX_MS;
	}

	return (u32)delta;
}

internal void NativeSpeedrun_Emit(struct NativeSpeedrunState *state, u32 type, const struct NativeSpeedrunFrame *frame, s32 segmentIndex)
{
	struct NativeSpeedrunEvent *event = &state->lastEvent;

	state->sequence++;
	event->sequence = state->sequence;
	event->type = type;
	event->levelID = frame->levelID;
	event->gameMode1 = frame->gameMode1;
	event->segmentIndex = segmentIndex;
	event->finishPosition = (frame->playerFinished != 0) ? 1 : 0;
	event->segmentTimeMS = state->loadlessMS - state->lastSplitMS;
	event->totalTimeMS = state->loadlessMS;
}

void NativeSpeedrun_Reset(struct NativeSpeedrunState *state, const struct NativeSpeedrunRoute *route)
{
	memset(state, 0, sizeof(*state));

	state->abiVersion = NATIVE_SPEEDRUN_ABI_VERSION;
	state->segmentIndex = 0;

	if (route != NULL)
	{
		state->route = *route;
	}
	else
	{
		state->route.count = 0;
	}
}

void NativeSpeedrun_Update(struct NativeSpeedrunState *state, const struct NativeSpeedrunFrame *frame)
{
	state->lastEvent.type = NATIVE_SPEEDRUN_EVENT_NONE;
	state->lastEvent.sequence = state->sequence;

	const b32 loadFrame = (frame->loadStage != NATIVE_SPEEDRUN_LOAD_IDLE);
	const b32 paused = (frame->gameMode1 & NATIVE_SPEEDRUN_GM_PAUSE_ALL) != 0;
	const u32 wallDelta = state->havePrevWall ? NativeSpeedrun_WallDelta(frame->wallTimeMS, state->prevWallMS) : 0u;

	state->paused = paused ? 1u : 0u;

	// Run start: control gained in the starting hub.
	if ((state->active == 0) && (state->finished == 0))
	{
		const b32 canStart = (frame->levelID == NATIVE_SPEEDRUN_HUB_N_SANITY_BEACH) && !loadFrame &&
		                     ((frame->gameMode1 & NATIVE_SPEEDRUN_GM_LOADING) == 0) &&
		                     ((frame->gameMode1 & NATIVE_SPEEDRUN_GM_START_OF_RACE) == 0) &&
		                     ((frame->gameMode1 & NATIVE_SPEEDRUN_GM_ADVENTURE) != 0) && (frame->demoMode == 0) &&
		                     (frame->numPlayers == 1);

		if (canStart)
		{
			state->active = 1;
			state->runStartWallMS = frame->wallTimeMS;
			state->loadlessMS = 0;
			state->rtaMS = 0;
			state->lastSplitMS = 0;
			NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RUN_START, frame, 0);
		}
	}

	// Clock accumulation. Loadless counts the game's frame time on non-load
	// frames; because the engine freezes its own clock while paused, pause time
	// is recovered from the wall clock.
	if (state->active != 0)
	{
		state->rtaMS += wallDelta;

		if (paused)
		{
			state->loadlessMS += wallDelta;
		}
		else if (!loadFrame)
		{
			state->loadlessMS += frame->elapsedTimeMS;
		}
	}

	// Race completion: split or run end.
	if ((state->active != 0) && (frame->playerFinished != 0) && (state->prevPlayerFinished == 0))
	{
		const s32 index = state->segmentIndex;

		if ((index >= 0) && (index < state->route.count))
		{
			const struct NativeSpeedrunSplit *split = &state->route.splits[index];
			const b32 boss = (frame->gameMode1 & NATIVE_SPEEDRUN_GM_ADVENTURE_BOSS) != 0;
			const b32 wantBoss = (split->kind == NATIVE_SPEEDRUN_SPLIT_BOSS);

			if ((split->levelID == frame->levelID) && (boss == wantBoss))
			{
				state->segmentIndex = index + 1;

				if (state->segmentIndex >= state->route.count)
				{
					state->active = 0;
					state->finished = 1;
					NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RUN_END, frame, index);
				}
				else
				{
					NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_SPLIT, frame, index);
				}

				state->lastSplitMS = state->loadlessMS;
			}
		}
	}

	// Run reset: an active run returns to the main menu without finishing.
	if ((state->active != 0) && !loadFrame && (frame->levelID == NATIVE_SPEEDRUN_MAIN_MENU_LEVEL))
	{
		state->active = 0;
		state->finished = 0;
		NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RESET, frame, state->segmentIndex);
	}

	state->prevPlayerFinished = (frame->playerFinished != 0) ? 1 : 0;
	state->prevWallMS = frame->wallTimeMS;
	state->havePrevWall = 1;
}

void NativeSpeedrun_WriteSurface(const struct NativeSpeedrunState *state, struct NativeSpeedrunSurface *out)
{
	u32 flags = 0;

	if (state->active != 0)
	{
		flags |= NATIVE_SPEEDRUN_SURFACE_ACTIVE;
	}
	if (state->finished != 0)
	{
		flags |= NATIVE_SPEEDRUN_SURFACE_FINISHED;
	}
	if (state->paused != 0)
	{
		flags |= NATIVE_SPEEDRUN_SURFACE_PAUSED;
	}

	out->magic = NATIVE_SPEEDRUN_SURFACE_MAGIC;
	out->abiVersion = state->abiVersion;
	out->sequence = state->sequence;
	out->flags = flags;
	out->segmentIndex = state->segmentIndex;
	out->loadlessMS = state->loadlessMS;
	out->rtaMS = state->rtaMS;
	out->lastEventType = (s32)state->lastEvent.type;
	out->lastEventLevelID = state->lastEvent.levelID;
	out->lastEventGameMode1 = state->lastEvent.gameMode1;
	out->lastEventSegmentIndex = state->lastEvent.segmentIndex;
	out->lastEventSequence = state->lastEvent.sequence;
	out->lastEventSegmentTimeMS = state->lastEvent.segmentTimeMS;
	out->lastEventTotalTimeMS = state->lastEvent.totalTimeMS;
}

internal b32 NativeSpeedrun_KindFromToken(const char *token, s32 tokenLength, s32 *kindOut)
{
	if ((tokenLength == 6) && (strncmp(token, "normal", 6) == 0))
	{
		*kindOut = NATIVE_SPEEDRUN_SPLIT_NORMAL;
		return true;
	}

	if ((tokenLength == 4) && (strncmp(token, "boss", 4) == 0))
	{
		*kindOut = NATIVE_SPEEDRUN_SPLIT_BOSS;
		return true;
	}

	return false;
}

int NativeSpeedrun_ParseRoute(const char *text, struct NativeSpeedrunRoute *out, char *err, u32 errSize)
{
	const char *cursor = text;
	s32 lineNumber = 0;

	out->count = 0;

	while ((cursor != NULL) && (*cursor != '\0'))
	{
		const char *lineStart = cursor;
		const char *lineEnd;
		const char *scan;

		lineNumber++;

		while ((*cursor != '\0') && (*cursor != '\n'))
		{
			cursor++;
		}

		lineEnd = cursor;
		if (*cursor == '\n')
		{
			cursor++;
		}

		// Trim leading whitespace.
		while ((lineStart < lineEnd) && ((*lineStart == ' ') || (*lineStart == '\t') || (*lineStart == '\r')))
		{
			lineStart++;
		}

		// Trim trailing whitespace.
		while ((lineEnd > lineStart) && ((lineEnd[-1] == ' ') || (lineEnd[-1] == '\t') || (lineEnd[-1] == '\r')))
		{
			lineEnd--;
		}

		if ((lineStart == lineEnd) || (*lineStart == '#'))
		{
			continue;
		}

		if (out->count >= NATIVE_SPEEDRUN_MAX_SPLITS)
		{
			if ((err != NULL) && (errSize != 0))
			{
				snprintf(err, errSize, "line %d: too many splits (max %d)", lineNumber, NATIVE_SPEEDRUN_MAX_SPLITS);
			}
			return -1;
		}

		// Field 1: level id.
		char *numberEnd = NULL;
		const long levelID = strtol(lineStart, &numberEnd, 10);
		if (numberEnd == lineStart)
		{
			if ((err != NULL) && (errSize != 0))
			{
				snprintf(err, errSize, "line %d: expected a numeric level id", lineNumber);
			}
			return -1;
		}

		// Field 2: kind.
		scan = numberEnd;
		while ((scan < lineEnd) && ((*scan == ' ') || (*scan == '\t')))
		{
			scan++;
		}

		const char *kindStart = scan;
		while ((scan < lineEnd) && (*scan != ' ') && (*scan != '\t'))
		{
			scan++;
		}

		s32 kind = NATIVE_SPEEDRUN_SPLIT_NORMAL;
		if (!NativeSpeedrun_KindFromToken(kindStart, (s32)(scan - kindStart), &kind))
		{
			if ((err != NULL) && (errSize != 0))
			{
				snprintf(err, errSize, "line %d: expected kind 'normal' or 'boss'", lineNumber);
			}
			return -1;
		}

		// Field 3: name (rest of the line).
		while ((scan < lineEnd) && ((*scan == ' ') || (*scan == '\t')))
		{
			scan++;
		}

		const s32 nameLength = (s32)(lineEnd - scan);
		if (nameLength <= 0)
		{
			if ((err != NULL) && (errSize != 0))
			{
				snprintf(err, errSize, "line %d: expected a split name", lineNumber);
			}
			return -1;
		}

		if (nameLength >= NATIVE_SPEEDRUN_NAME_MAX)
		{
			if ((err != NULL) && (errSize != 0))
			{
				snprintf(err, errSize, "line %d: split name too long (max %d)", lineNumber, NATIVE_SPEEDRUN_NAME_MAX - 1);
			}
			return -1;
		}

		struct NativeSpeedrunSplit *split = &out->splits[out->count];
		split->levelID = (s32)levelID;
		split->kind = kind;
		memset(split->name, 0, sizeof(split->name));
		memcpy(split->name, scan, (size_t)nameLength);

		out->count++;
	}

	return out->count;
}
