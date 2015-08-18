
/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrFontAtlasSizes_DEFINED
#define GrFontAtlasSizes_DEFINED

// For debugging atlas which evict all of the time
//#define DEBUG_CONSTANT_EVICT
#ifdef DEBUG_CONSTANT_EVICT
#define GR_FONT_ATLAS_TEXTURE_WIDTH    256//1024
#define GR_FONT_ATLAS_A8_TEXTURE_WIDTH 256//2048
#define GR_FONT_ATLAS_TEXTURE_HEIGHT   256//2048

#define GR_FONT_ATLAS_PLOT_WIDTH       256
#define GR_FONT_ATLAS_A8_PLOT_WIDTH    256//512
#define GR_FONT_ATLAS_PLOT_HEIGHT      256

#define GR_FONT_ATLAS_NUM_PLOTS_X     (GR_FONT_ATLAS_TEXTURE_WIDTH / GR_FONT_ATLAS_PLOT_WIDTH)
#define GR_FONT_ATLAS_A8_NUM_PLOTS_X  (GR_FONT_ATLAS_A8_TEXTURE_WIDTH / GR_FONT_ATLAS_A8_PLOT_WIDTH)
#define GR_FONT_ATLAS_NUM_PLOTS_Y     (GR_FONT_ATLAS_TEXTURE_HEIGHT / GR_FONT_ATLAS_PLOT_HEIGHT)
#else
#define GR_FONT_ATLAS_TEXTURE_WIDTH    1024
#define GR_FONT_ATLAS_A8_TEXTURE_WIDTH 2048
#define GR_FONT_ATLAS_TEXTURE_HEIGHT   2048

#define GR_FONT_ATLAS_PLOT_WIDTH       256
#define GR_FONT_ATLAS_A8_PLOT_WIDTH    512
#define GR_FONT_ATLAS_PLOT_HEIGHT      256

#define GR_FONT_ATLAS_NUM_PLOTS_X     (GR_FONT_ATLAS_TEXTURE_WIDTH / GR_FONT_ATLAS_PLOT_WIDTH)
#define GR_FONT_ATLAS_A8_NUM_PLOTS_X  (GR_FONT_ATLAS_A8_TEXTURE_WIDTH / GR_FONT_ATLAS_A8_PLOT_WIDTH)
#define GR_FONT_ATLAS_NUM_PLOTS_Y     (GR_FONT_ATLAS_TEXTURE_HEIGHT / GR_FONT_ATLAS_PLOT_HEIGHT)
#endif
#endif