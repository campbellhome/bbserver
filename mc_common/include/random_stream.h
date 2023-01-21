// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_pcg32_random
{
	u64 state;
	u64 inc;
} pcg32_random;

typedef enum tag_RandomStreamType
{
	kRandomStream_PCG32,
	kRandomStream_LCG,
} RandomStreamType;

typedef struct tag_RandomStream
{
	RandomStreamType type;
	u8 pad[4];
	union
	{
		pcg32_random pcg32;
		u32 lcg;
	} state;
} RandomStream;

pcg32_random random_make_pcg32_seed(void);
u32 random_make_lcg_seed(void);

pcg32_random random_make_pgc32_deterministic_seed(void);

RandomStream random_make_pcg32(pcg32_random seed);
RandomStream random_make_lcg(u32 seed);

u32 random_get_u32(RandomStream* s);
u32 random_get_u32_range(RandomStream* s, u32 minVal, u32 maxVal);
s32 random_get_s32_range(RandomStream* s, s32 minVal, s32 maxVal);
float random_get_float_01(RandomStream* s);
float random_get_float_range(RandomStream* s, float minVal, float maxVal);

#if defined(__cplusplus)
}
#endif
