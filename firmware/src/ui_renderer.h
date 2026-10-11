#pragma once
#include "ui_model.h"
#include "display_bsp.h"
void renderScreen(DisplayPort& display, const ScreenModel& model, const ScreenState& state, uint32_t now);
