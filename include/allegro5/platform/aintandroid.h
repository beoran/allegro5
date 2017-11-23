/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      Some definitions for internal use by the Android library code.
 *
 *      By Thomas Fjellstrom.
 * 
 *      See readme.txt for copyright information.
 */

#ifndef __al_included_allegro5_aintandroid_h
#define __al_included_allegro5_aintandroid_h

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "allegro5/internal/aintern_system.h"
#include "allegro5/internal/aintern_display.h"

ALLEGRO_PATH *_al_android_get_path(int id);

#define ALLEGRO_HAPDRV_ANDROID    AL_ID('A','N','D','H')

#ifdef ALLEGRO_ANDROID
AL_VAR(struct ALLEGRO_HAPTIC_DRIVER, _al_hapdrv_android);
#endif

#ifdef __cplusplus
}
#endif


#endif
