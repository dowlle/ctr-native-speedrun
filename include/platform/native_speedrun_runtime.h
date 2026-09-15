#ifndef NATIVE_SPEEDRUN_RUNTIME_H
#define NATIVE_SPEEDRUN_RUNTIME_H

#include <platform/native_speedrun.h>

#if defined(CTR_SPEEDRUN)

struct GameTracker;

// Prepares the speedrun state and, if present, loads the route config.
void NativeSpeedrunRuntime_Init(void);

// Decodes one engine frame into the speedrun state, refreshes the named-section
// surface and appends any event to the run log.
void NativeSpeedrunRuntime_Update(struct GameTracker *gGT);

#endif

#endif
