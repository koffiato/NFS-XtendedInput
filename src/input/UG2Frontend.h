// UG2 frontend callbacks used by the input hooks. MIT; see LICENSE.
#pragma once
#include "../_vs/stdafx.h"

extern void(*FESendKeystroke)(unsigned int key);
void UpdateFECursorPos();
