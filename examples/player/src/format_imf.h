/*
 * Copyright (c) 2026 Bitdancer (github.com/RealBitdancer).
 * SPDX-License-Identifier: MIT
 */

#ifndef FORMAT_IMF_H_INCLUDED
#define FORMAT_IMF_H_INCLUDED

#include "player_format.h"

extern const MusicFormat imfFormat;

// 0 picks by extension (.wlf 700, .imf 560). Rate is not in the file.
void imfSetRate(uint32_t hz);
uint32_t imfRate(void);

#endif // FORMAT_IMF_H_INCLUDED
