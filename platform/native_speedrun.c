#include <platform/native_speedrun.h>

#include <stdarg.h>
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

internal void NativeSpeedrun_Emit(struct NativeSpeedrunState *state, u32 type, const struct NativeSpeedrunFrame *frame, s32 levelID, s32 segmentIndex)
{
	if (state->eventCount >= NATIVE_SPEEDRUN_MAX_FRAME_EVENTS)
	{
		return;
	}

	struct NativeSpeedrunEvent *event = &state->events[state->eventCount];
	state->sequence++;

	s32 finishPosition = 0;
	if (frame->playerFinished != 0)
	{
		finishPosition = (frame->finishPosition > 0) ? frame->finishPosition : 1;
	}

	event->sequence = state->sequence;
	event->type = type;
	event->levelID = levelID;
	event->gameMode1 = frame->gameMode1;
	event->segmentIndex = segmentIndex;
	event->finishPosition = finishPosition;
	event->segmentTimeMS = state->loadlessMS - state->lastSplitMS;
	event->totalTimeMS = state->loadlessMS;

	state->eventCount++;
	state->lastEvent = *event;
}

void NativeSpeedrun_Reset(struct NativeSpeedrunState *state, const struct NativeSpeedrunRoute *route)
{
	memset(state, 0, sizeof(*state));

	state->abiVersion = NATIVE_SPEEDRUN_ABI_VERSION;
	state->segmentIndex = 0;
	state->currentLevelID = -1;

	if (route != NULL)
	{
		state->route = *route;
	}
	else
	{
		state->route.count = 0;
	}
}

internal void NativeSpeedrun_RestartRun(struct NativeSpeedrunState *state)
{
	state->active = 0;
	state->finished = 0;
	state->paused = 0;
	state->segmentIndex = 0;
	state->loadlessMS = 0;
	state->rtaMS = 0;
	state->lastSplitMS = 0;
	state->currentLevelID = -1;
}

void NativeSpeedrun_Update(struct NativeSpeedrunState *state, const struct NativeSpeedrunFrame *frame)
{
	// lastEvent persists across frames on purpose: a consumer that polls the
	// surface may read after the frame that emitted it, and must still see the
	// event. Only the per-frame batch is reset here.
	state->eventCount = 0;

	const b32 gameplay = (frame->mainGameState == NATIVE_SPEEDRUN_MAIN_GAME_GAMEPLAY);
	const b32 loadFrame = (frame->loadStage != NATIVE_SPEEDRUN_LOAD_IDLE);
	const b32 paused = (frame->gameMode1 & NATIVE_SPEEDRUN_GM_PAUSE_ALL) != 0;
	const u32 wallDelta = state->havePrevWall ? NativeSpeedrun_WallDelta(frame->wallTimeMS, state->prevWallMS) : 0u;

	state->paused = paused ? 1u : 0u;

	// Run start: control gained in the starting hub.
	if ((state->active == 0) && (state->finished == 0))
	{
		const b32 canStart = gameplay && (frame->levelID == NATIVE_SPEEDRUN_HUB_N_SANITY_BEACH) && !loadFrame &&
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
			state->currentLevelID = -1;
			NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RUN_START, frame, frame->levelID, 0);
		}
	}

	if (state->active != 0)
	{
		// Clock accumulation. Loadless counts the game's frame time on active
		// non-load frames; since the engine freezes its own clock while paused,
		// pause time is recovered from the wall clock.
		state->rtaMS += wallDelta;

		if (paused)
		{
			state->loadlessMS += wallDelta;
		}
		else if (gameplay && !loadFrame)
		{
			state->loadlessMS += frame->elapsedTimeMS;
		}

		// Level enter and exit, so the log records the movement between races.
		if (gameplay && !loadFrame)
		{
			if (frame->levelID != state->currentLevelID)
			{
				if (state->currentLevelID >= 0)
				{
					NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_LEVEL_EXIT, frame, state->currentLevelID, state->segmentIndex);
				}
				state->currentLevelID = frame->levelID;
				NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_LEVEL_ENTER, frame, frame->levelID, state->segmentIndex);
			}
		}
		else if (state->currentLevelID >= 0)
		{
			NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_LEVEL_EXIT, frame, state->currentLevelID, state->segmentIndex);
			state->currentLevelID = -1;
		}

		// Race completion: always a race finish, and a split when it advances
		// the route.
		if ((frame->playerFinished != 0) && (state->prevPlayerFinished == 0))
		{
			const s32 index = state->segmentIndex;

			NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RACE_FINISH, frame, frame->levelID, index);

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
						NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RUN_END, frame, frame->levelID, index);
					}
					else
					{
						NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_SPLIT, frame, frame->levelID, index);
					}

					state->lastSplitMS = state->loadlessMS;
				}
			}
		}

		// Run reset: an active run returns to the main menu without finishing.
		if (gameplay && !loadFrame && (frame->levelID == NATIVE_SPEEDRUN_MAIN_MENU_LEVEL))
		{
			NativeSpeedrun_Emit(state, NATIVE_SPEEDRUN_EVENT_RESET, frame, frame->levelID, state->segmentIndex);
			NativeSpeedrun_RestartRun(state);
		}
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
	out->lastEventFinishPosition = state->lastEvent.finishPosition;
}

internal const char *NativeSpeedrun_EventTypeName(u32 type)
{
	switch (type)
	{
	case NATIVE_SPEEDRUN_EVENT_RUN_START:
	{
		return "run_start";
	}
	case NATIVE_SPEEDRUN_EVENT_SPLIT:
	{
		return "split";
	}
	case NATIVE_SPEEDRUN_EVENT_RUN_END:
	{
		return "run_end";
	}
	case NATIVE_SPEEDRUN_EVENT_RESET:
	{
		return "reset";
	}
	case NATIVE_SPEEDRUN_EVENT_LEVEL_ENTER:
	{
		return "level_enter";
	}
	case NATIVE_SPEEDRUN_EVENT_LEVEL_EXIT:
	{
		return "level_exit";
	}
	case NATIVE_SPEEDRUN_EVENT_RACE_FINISH:
	{
		return "race_finish";
	}
	default:
	{
		return "none";
	}
	}
}

int NativeSpeedrun_FormatEvent(const struct NativeSpeedrunState *state, const struct NativeSpeedrunEvent *event, char *buf, u32 size)
{
	if ((buf == NULL) || (size == 0) || (event == NULL))
	{
		return -1;
	}

	const int written =
	    snprintf(buf, size, "seq=%u type=%s level=%d mode=%u segment=%d pos=%d seg_ms=%u total_ms=%u loadless_ms=%u rta_ms=%u\n", event->sequence,
	             NativeSpeedrun_EventTypeName(event->type), event->levelID, event->gameMode1, event->segmentIndex, event->finishPosition,
	             event->segmentTimeMS, event->totalTimeMS, state->loadlessMS, state->rtaMS);

	if ((written < 0) || ((u32)written >= size))
	{
		return -1;
	}

	return written;
}

internal int NativeSpeedrun_ParseFail(char *err, u32 errSize, s32 lineNumber, const char *format, ...)
{
	if ((err != NULL) && (errSize != 0))
	{
		char message[128];
		va_list args;

		va_start(args, format);
		vsnprintf(message, sizeof(message), format, args);
		va_end(args);

		snprintf(err, errSize, "line %d: %s", lineNumber, message);
	}

	return -1;
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

		while ((lineStart < lineEnd) && ((*lineStart == ' ') || (*lineStart == '\t') || (*lineStart == '\r')))
		{
			lineStart++;
		}

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
			return NativeSpeedrun_ParseFail(err, errSize, lineNumber, "too many splits (max %d)", NATIVE_SPEEDRUN_MAX_SPLITS);
		}

		char *numberEnd = NULL;
		const long levelID = strtol(lineStart, &numberEnd, 10);
		if (numberEnd == lineStart)
		{
			return NativeSpeedrun_ParseFail(err, errSize, lineNumber, "expected a numeric level id");
		}

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
			return NativeSpeedrun_ParseFail(err, errSize, lineNumber, "expected kind 'normal' or 'boss'");
		}

		while ((scan < lineEnd) && ((*scan == ' ') || (*scan == '\t')))
		{
			scan++;
		}

		const s32 nameLength = (s32)(lineEnd - scan);
		if (nameLength <= 0)
		{
			return NativeSpeedrun_ParseFail(err, errSize, lineNumber, "expected a split name");
		}

		if (nameLength >= NATIVE_SPEEDRUN_NAME_MAX)
		{
			return NativeSpeedrun_ParseFail(err, errSize, lineNumber, "split name too long (max %d)", NATIVE_SPEEDRUN_NAME_MAX - 1);
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
