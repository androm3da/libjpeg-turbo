/*
 * YCbCr to RGB colorspace conversion (Qualcomm Hexagon HVX)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: IJG
 */

#include "../jsimdint.h"
#include "hvx-compat.h"

#define ALIGN(alignment)  __attribute__((aligned(alignment)))


/* YCbCr -> RGB conversion constants */

#define F_0_344  11277  /* 0.3441467 = 11277 * 2^-15 */
#define F_0_714  23401  /* 0.7141418 = 23401 * 2^-15 */
#define F_1_402  22971  /* 1.4020386 = 22971 * 2^-14 */
#define F_1_772  29033  /* 1.7720337 = 29033 * 2^-14 */


/* Include inline routines for colorspace extensions. */

#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED  EXT_RGB_RED
#define RGB_GREEN  EXT_RGB_GREEN
#define RGB_BLUE  EXT_RGB_BLUE
#define RGB_PIXELSIZE  EXT_RGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extrgb_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_RGBX_RED
#define RGB_GREEN  EXT_RGBX_GREEN
#define RGB_BLUE  EXT_RGBX_BLUE
#define RGB_ALPHA  3
#define RGB_PIXELSIZE  EXT_RGBX_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extrgbx_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_BGR_RED
#define RGB_GREEN  EXT_BGR_GREEN
#define RGB_BLUE  EXT_BGR_BLUE
#define RGB_PIXELSIZE  EXT_BGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extbgr_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_BGRX_RED
#define RGB_GREEN  EXT_BGRX_GREEN
#define RGB_BLUE  EXT_BGRX_BLUE
#define RGB_ALPHA  3
#define RGB_PIXELSIZE  EXT_BGRX_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extbgrx_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_XBGR_RED
#define RGB_GREEN  EXT_XBGR_GREEN
#define RGB_BLUE  EXT_XBGR_BLUE
#define RGB_ALPHA  0
#define RGB_PIXELSIZE  EXT_XBGR_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extxbgr_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx

#define RGB_RED  EXT_XRGB_RED
#define RGB_GREEN  EXT_XRGB_GREEN
#define RGB_BLUE  EXT_XRGB_BLUE
#define RGB_ALPHA  0
#define RGB_PIXELSIZE  EXT_XRGB_PIXELSIZE
#define jsimd_ycc_rgb_convert_hvx  jsimd_ycc_extxrgb_convert_hvx
#include "jdcolext-hvx.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef jsimd_ycc_rgb_convert_hvx
