// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bb_colors.h"
#include "theme_config.h"
#include "wrap_imgui.h"

ImVec4 MakeColor(styleColor_e idx)
{
	const styleColor *sc = g_styleConfig.colors + idx;
	return ImColor(sc->r, sc->g, sc->b, sc->a);
}
