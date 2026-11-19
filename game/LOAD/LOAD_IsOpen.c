#include <common.h>

b32 LOAD_IsOpen_RacingOrBattle(void)
{
	return GAME_TRACKER->overlayIndex_Threads == OVERLAY_INDEX_RACING_OR_BATTLE;
}

b32 LOAD_IsOpen_MainMenu(void)
{
	return GAME_TRACKER->overlayIndex_Threads == OVERLAY_INDEX_MAIN_MENU;
}

b32 LOAD_IsOpen_AdvHub(void)
{
	return GAME_TRACKER->overlayIndex_Threads == OVERLAY_INDEX_ADV_HUB;
}

b32 LOAD_IsOpen_Podiums(void)
{
	return GAME_TRACKER->overlayIndex_Threads == OVERLAY_INDEX_PODIUMS;
}
