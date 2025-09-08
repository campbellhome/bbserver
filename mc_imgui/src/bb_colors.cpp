// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "bb_colors.h"
#include "theme_config.h"
#include "wrap_imgui.h"

ImVec4 MakeColor(styleColor_e idx)
{
	const styleColor* sc = g_styleConfig.colors + idx;
	return ImColor(sc->r, sc->g, sc->b, sc->a);
}

ImColor MakeBackgroundTintColor(styleColor_e idx, const ImColor& defaultColor)
{
	const styleColor* sc = g_styleConfig.colors + idx;
	if (sc->a)
	{
		return ImColor(sc->r, sc->g, sc->b, sc->a);
	}
	else
	{
		return defaultColor;
	}
}
