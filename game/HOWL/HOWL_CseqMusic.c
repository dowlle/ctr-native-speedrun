#include <common.h>

int CseqMusic_Start(u16 songID, s32 deltaBPM, struct SongSet *songSet, int songSetActiveBits, b32 boolLoopAtEnd)
{
	int i;
	int started = 0;
	struct Song *song;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return started;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return started;
	}

	if (GAME_CSEQ_HEADER->numSongs <= songID)
	{
		return started;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is free
		if ((song->flags & 1) == 0)
		{
			// start song in this pool
			SongPool_Start(song, songID, deltaBPM, boolLoopAtEnd, songSet, songSetActiveBits);
			started = 1;
			break;
		}
	}

	Smart_ExitCriticalSection();
	return started;
}

// pause all songs
void CseqMusic_Pause()
{
	int i;
	struct Song *song;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if ((song->flags & 1) != 0)
		{
			// pause song
			song->flags |= 2;
		}
	}

	Smart_ExitCriticalSection();
}

// resume all songs
void CseqMusic_Resume()
{
	int i;
	struct Song *song;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if ((song->flags & 1) != 0)
		{
			// unpause song
			song->flags &= ~(2);
		}
	}

	Smart_ExitCriticalSection();
}

void CseqMusic_ChangeVolume(u16 songID, int newVol, int newStep)
{
	struct Song *song;
	int i;
	// NOTE(aalhendi): These copies preserve the retail argument-save order and registers.
	u16 id = songID;
	register int volume CTR_PSX_REGISTER("$19") = newVol;
	register int step CTR_PSX_REGISTER("$20") = newStep;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER->numSongs <= id)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if (((song->flags & 1) != 0) && (song->id == id))
		{
			SongPool_Volume(song, volume & 0xff, step & 0xff, 0);
		}
	}

	Smart_ExitCriticalSection();
}

void CseqMusic_Restart(u16 songID, int newStep)
{
	int i;
	struct Song *song;
	// NOTE(aalhendi): These copies preserve the retail argument-save order and registers.
	u16 id = songID;
	register int step CTR_PSX_REGISTER("$19") = newStep;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER->numSongs <= id)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if (((song->flags & 1) != 0) && (song->id == id))
		{
			song->flags |= 4;
			SongPool_Volume(song, 0, step & 0xff, 0);
		}
	}

	Smart_ExitCriticalSection();
}

void CseqMusic_ChangeTempo(u16 songID, s32 deltaBPM)
{
	int i;
	struct Song *song;
	// NOTE(aalhendi): These copies preserve the retail argument-save order and registers.
	u16 id = songID;
	register s32 tempoDelta CTR_PSX_REGISTER("$19") = deltaBPM;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER->numSongs <= id)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if (((song->flags & 1) != 0) && (song->id == id))
		{
			SongPool_ChangeTempo(song, tempoDelta);
		}
	}

	Smart_ExitCriticalSection();
}

void CseqMusic_AdvHubSwap(u16 songID, struct SongSet *songSet, int songSetActiveBits)
{
	struct Song *song;
	int i;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER->numSongs <= songID)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if song is playing
		if (song->flags & 1)
		{
			if (song->id == songID)
			{
				SongPool_AdvHub2(song, songSet, songSetActiveBits);
			}
		}
	}

	Smart_ExitCriticalSection();
}

void CseqMusic_Stop(u16 songID)
{
	int i;
	struct Song *song;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER->numSongs <= songID)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if (((song->flags & 1) != 0) && (song->id == songID))
		{
			SongPool_StopAllCseq(song);
		}
	}

	Smart_ExitCriticalSection();
}

void CseqMusic_StopAll()
{
	int i;
	struct Song *song;

	if (GAME_AUDIO_ENABLED == 0)
	{
		return;
	}
	if (GAME_CSEQ_HEADER == 0)
	{
		return;
	}

	Smart_EnterCriticalSection();

	for (i = 0; i < 2; i++)
	{
		song = &GAME_SONG_POOL[i];

		// if pool is taken
		if ((song->flags & 1) != 0)
		{
			SongPool_StopAllCseq(song);
		}
	}

	Smart_ExitCriticalSection();
}
