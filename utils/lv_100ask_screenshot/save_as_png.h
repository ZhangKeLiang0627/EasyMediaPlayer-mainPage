/**
 * @file save_as_png.h
 *
 */

#ifndef SAVE_AS_PNG_H
#define SAVE_AS_PNG_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../libs/lvgl/lvgl.h"

#if LV_USE_PNG == 0
  #error "LV_USE_PNG must be defined in lv_conf.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 * GLOBAL VARIABLES
 ***********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool save_as_png_file(const uint8_t * image, uint32_t w, uint32_t h, uint32_t bpp, const char *filename);

/*=====================
 * Setter functions
 *====================*/

/*=====================
 * Getter functions
 *====================*/

/*=====================
 * Other functions
 *====================*/

/**********************
 *      MACROS
 **********************/

/*LV_USE_SCREENSHOT*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SAVE_AS_PNG_H*/
