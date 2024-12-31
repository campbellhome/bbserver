// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

double Time_GetCurrentFrameStartTime(void);
double Time_GetCurrentTime(void);
void Time_StartNewFrame(void);
float Time_GetLastFrameDT(void);
double Time_GetLastFrameDTDouble(void);
float Time_GetCurrentFrameElapsed(void);
u64 Time_GetFrameNumber(void);
const char* Time_StringFromEpochTime(u32 epochTime);

#if defined(__cplusplus)
}
#endif
