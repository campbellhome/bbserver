// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

double Time_GetCurrentTime(void);
void Time_StartNewFrame(void);
float Time_GetDT(void);
double Time_GetDTDouble(void);
float Time_GetCurrentFrameElapsed(void);
u64 Time_GetFrameNumber(void);
const char *Time_StringFromEpochTime(u32 epochTime);

#if defined(__cplusplus)
}
#endif
