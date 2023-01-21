// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "theme_config.h"

struct ImColor;
struct ImVec4;

ImVec4 MakeColor(styleColor_e idx);
ImColor MakeBackgroundTintColor(styleColor_e idx, const ImColor& defaultColor);
