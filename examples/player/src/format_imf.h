/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#ifndef FORMAT_IMF_H_INCLUDED
#define FORMAT_IMF_H_INCLUDED

#include "player_format.h"

// IMF (.imf / .wlf)
extern const MusicFormat imfFormat;

// Force replay rate for later IMF loads. 0 selects by ext: .wlf=700, .imf=560. Rate is game constant, not in file.
void imfSetRate(uint32_t hz);

// Rate used by most recent IMF load, or 0.
uint32_t imfRate(void);

#endif // FORMAT_IMF_H_INCLUDED
