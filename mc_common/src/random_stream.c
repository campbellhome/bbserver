// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "random_stream.h"
#include "bb_assert.h"
#include "bb_common.h"

BB_WARNING_PUSH(4820)
#include <time.h>
BB_WARNING_POP

// pcg32_random_r is from http://www.pcg-random.org/download.html
// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

static u32 pcg32_random_r(pcg32_random* rng)
{
	u64 oldstate = rng->state;
	// Advance internal state
	rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
	// Calculate output function (XSH RR), uses old state for max ILP
	u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
	u32 rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << (((u32)(-(s32)rot)) & 31));
}

pcg32_random random_make_pcg32_seed(void)
{
	pcg32_random seed;
	seed.state = time(NULL) ^ (intptr_t)&time;
	seed.inc = (intptr_t)&time;
	return seed;
}

// constants in random_make_pgc32_deterministic_seed are from
// https://github.com/imneme/pcg-c-basic/blob/master/pcg_basic.h
// minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

pcg32_random random_make_pgc32_deterministic_seed(void)
{
	pcg32_random seed;
	seed.state = 0x853c49e6748fea9bULL;
	seed.inc = 0xda3e39cb94b95bdbULL;
	return seed;
}

u32 random_make_lcg_seed(void)
{
	return (u32)time(NULL);
}

RandomStream random_make_pcg32(pcg32_random seed)
{
	RandomStream s = { BB_EMPTY_INITIALIZER };
	s.type = kRandomStream_PCG32;
	s.state.pcg32 = seed;
	return s;
}

RandomStream random_make_lcg(u32 seed)
{
	RandomStream s = { BB_EMPTY_INITIALIZER };
	s.type = kRandomStream_LCG;
	s.state.lcg = seed;
	return s;
}

static u32 lcg_random_r(u32* rng)
{
	*rng = (*rng * 196314165) + 907633515;
	return *rng;
}

u32 random_get_u32(RandomStream* s)
{
	u32 result = 0;
	switch (s->type)
	{
	case kRandomStream_PCG32:
		result = pcg32_random_r(&s->state.pcg32);
		break;
	case kRandomStream_LCG:
		result = lcg_random_r(&s->state.lcg);
		break;
	default:
		BB_ASSERT(false);
		break;
	}
	return result;
}

u32 random_get_u32_range(RandomStream* s, u32 minVal, u32 maxVal)
{
	// algorithm taken from pcg32_boundedrand_r in https://github.com/imneme/pcg-c-basic/blob/master/pcg_basic.c
	u32 range = maxVal - minVal;
	u32 threshold = ((u32)(-(s32)range)) % range;
	for (;;)
	{
		u32 r = random_get_u32(s);
		if (r >= threshold)
		{
			return minVal + r % range;
		}
	}
}

s32 random_get_s32_range(RandomStream* s, s32 minVal, s32 maxVal)
{
	s32 range = maxVal - minVal;
	if (range <= 0)
		return minVal;
	u32 val = random_get_u32_range(s, 0, (u32)range);
	return minVal + val;
}

float random_get_float_01(RandomStream* s)
{
	float result;
	do
	{
		u32 val = random_get_u32(s);

		const float fOne = 1.0f;
		*(s32*)&result = (*(s32*)&fOne & 0xff800000) | (val & 0x007fffff);
	} while (result >= 2.0f);
	return result - 1.0f;
}

float random_get_float_range(RandomStream* s, float minVal, float maxVal)
{
	float result;
	do
	{
		u32 val = random_get_u32(s);

		const float fOne = 1.0f;
		*(s32*)&result = (*(s32*)&fOne & 0xff800000) | (val & 0x007fffff);
	} while (result >= 2.0f);
	return minVal + (maxVal - minVal) * (result - 1.0f);
}
