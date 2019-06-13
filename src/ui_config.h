// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "config.h"

void UIConfig_Open(config_t *config);
void UIConfig_Update(config_t *config);
void UIConfig_Reset();
bool UIConfig_IsOpen();
void UIConfig_ApplyColorscheme(config_t *config = nullptr);
