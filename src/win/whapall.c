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
 *      Windows haptic (force-feedback) device driver.
 *
 *      By Beoran.
 *
 *      See LICENSE.txt for copyright information.
 */


#define ALLEGRO_NO_COMPATIBILITY

#define DIRECTINPUT_VERSION 0x0800

/* For waitable timers */
#define _WIN32_WINNT 0x400

#include "allegro5/allegro.h"
#include "allegro5/haptic.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/platform/aintwin.h"
#include "allegro5/internal/aintern_haptic.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_joystick.h"
#include "allegro5/internal/aintern_bitmap.h"

#ifdef ALLEGRO_CFG_XINPUT
/* Don't compile this lot if xinput and directinput isn't supported. */

#include "allegro5/internal/aintern_wjoyall.h"

ALLEGRO_DEBUG_CHANNEL("haptic")

/* Support at most 4 + 32 = 36 haptic devices. */
#define HAPTICS_MAX            36

typedef struct ALLEGRO_HAPTIC_WINDOWS_ALL {
  ALLEGRO_HAPTIC                parent;
  bool                          active;
  int                           index;
  ALLEGRO_HAPTIC_DRIVER       * driver;
  ALLEGRO_HAPTIC              * handle;  
} ALLEGRO_HAPTIC_WINDOWS_ALL;


/* forward declarations */
static bool hapall_init_haptic(void);
static void hapall_exit_haptic(void);

static bool hapall_is_mouse_haptic(ALLEGRO_MOUSE *dev);
static bool hapall_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool hapall_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev);
static bool hapall_is_display_haptic(ALLEGRO_DISPLAY *dev);
static bool hapall_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev);

static ALLEGRO_HAPTIC *hapall_get_from_mouse(ALLEGRO_MOUSE *dev);
static ALLEGRO_HAPTIC *hapall_get_from_joystick(ALLEGRO_JOYSTICK *dev);
static ALLEGRO_HAPTIC *hapall_get_from_keyboard(ALLEGRO_KEYBOARD *dev);
static ALLEGRO_HAPTIC *hapall_get_from_display(ALLEGRO_DISPLAY *dev);
static ALLEGRO_HAPTIC *hapall_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev);

static bool hapall_release(ALLEGRO_HAPTIC *haptic);

static bool hapall_get_active(ALLEGRO_HAPTIC *hap);
static int hapall_get_capabilities(ALLEGRO_HAPTIC *dev);
static double hapall_get_gain(ALLEGRO_HAPTIC *dev);
static bool hapall_set_gain(ALLEGRO_HAPTIC *dev, double);
static int hapall_get_num_effects(ALLEGRO_HAPTIC *dev);

static bool hapall_is_effect_ok(ALLEGRO_HAPTIC *dev, ALLEGRO_HAPTIC_EFFECT *eff);
static bool hapall_upload_effect(ALLEGRO_HAPTIC *dev,
                               ALLEGRO_HAPTIC_EFFECT *eff,
                               ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapall_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loop);
static bool hapall_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapall_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapall_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);

static double hapall_get_autocenter(ALLEGRO_HAPTIC *dev);
static bool hapall_set_autocenter(ALLEGRO_HAPTIC *dev, double);


ALLEGRO_HAPTIC_DRIVER _al_hapdrv_windows_all =
{
   AL_HAPTIC_TYPE_WINDOWS_ALL,
   "",
   "",
   "Windows haptic(s)",
   hapall_init_haptic,
   hapall_exit_haptic,

   hapall_is_mouse_haptic,
   hapall_is_joystick_haptic,
   hapall_is_keyboard_haptic,
   hapall_is_display_haptic,
   hapall_is_touch_input_haptic,

   hapall_get_from_mouse,
   hapall_get_from_joystick,
   hapall_get_from_keyboard,
   hapall_get_from_display,
   hapall_get_from_touch_input,

   hapall_get_active,
   hapall_get_capabilities,
   hapall_get_gain,
   hapall_set_gain,
   hapall_get_num_effects,

   hapall_is_effect_ok,
   hapall_upload_effect,
   hapall_play_effect,
   hapall_stop_effect,
   hapall_is_effect_playing,
   hapall_release_effect,

   hapall_release,

   hapall_get_autocenter,
   hapall_set_autocenter
};


static ALLEGRO_HAPTIC_WINDOWS_ALL haptics[HAPTICS_MAX];
/* Mutex for thread protection. */
static ALLEGRO_MUTEX  * hapall_mutex = NULL;


/* Initializes the combined haptic system. */
static bool hapall_init_haptic(void)
{
   int i;
   bool xi_ok, di_ok;
   ASSERT(hapall_mutex == NULL);


   /* Create the mutex. */
   hapall_mutex = al_create_mutex_recursive();
   if(!hapall_mutex)
      return false;

   al_lock_mutex(hapall_mutex);

   for(i = 0; i < HAPTICS_MAX; i++) {
      haptics[i].active = false;
      haptics[i].driver = NULL;
      haptics[i].handle = NULL;
   }
   
   xi_ok = _al_hapdrv_xinput.init_haptic();
   di_ok = _al_hapdrv_directx.init_haptic();
   al_unlock_mutex(hapall_mutex);
   return xi_ok || di_ok; 
}

/* Converts a generic haptic device to a Windows-specific one. */
static ALLEGRO_HAPTIC_WINDOWS_ALL * hapall_from_al(ALLEGRO_HAPTIC *hap)
{
  return (ALLEGRO_HAPTIC_WINDOWS_ALL *) hap;
}

static void hapall_exit_haptic(void)
{
   ASSERT(hapall_mutex);
   al_destroy_mutex(hapall_mutex);
   hapall_mutex = NULL;
}

static bool hapall_get_active(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL *hapall = hapall_from_al(haptic);
   return hapall->driver->get_active(hapall->handle);
}


static bool hapall_is_mouse_haptic(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return false;
}


static bool hapall_is_joystick_haptic(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_WINDOWS_ALL *joyall = (ALLEGRO_JOYSTICK_WINDOWS_ALL *) joy;
   if (!al_is_joystick_installed())
      return false;
   if (!al_get_joystick_active(joy))
      return false;
   if (joyall->driver == &_al_joydrv_xinput) {
      _al_hapdrv_xinput.is_joystick_haptic(joyall->handle);
   } else if (joyall->driver == &_al_joydrv_directx) {
      _al_hapdrv_directx.is_joystick_haptic(joyall->handle);
   } 
   return false;
}


static bool hapall_is_display_haptic(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return false;
}


static bool hapall_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return false;
}


static bool hapall_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *hapall_get_from_mouse(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return NULL;
}


static ALLEGRO_HAPTIC *hapall_get_from_joystick(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_WINDOWS_ALL * joyall = (ALLEGRO_JOYSTICK_WINDOWS_ALL *) joy;
   ALLEGRO_HAPTIC_WINDOWS_ALL   * hapall;

   if (!al_is_joystick_haptic(joy))
      return NULL;

   al_lock_mutex(hapall_mutex);

   /* Index of haptic is same as that of joystick. */
   hapall                = haptics + joyall->index;
   hapall->index         = joyall->index;
 
   hapall->parent.device = joyall;
   hapall->parent.from   = _AL_HAPTIC_FROM_JOYSTICK;
   hapall->active        =  true;
   if (joyall->driver == &_al_joydrv_xinput) {
      hapall->driver = &_al_hapdrv_xinput;
   } else if (joyall->driver == &_al_joydrv_directx) {
      hapall->driver = &_al_hapdrv_directx;
   } else {
      al_unlock_mutex(hapall_mutex);
      return false;
   }
   hapall->handle = hapall->driver->get_from_joystick(joyall->handle);

   al_unlock_mutex(hapall_mutex);
   return &hapall->parent;
}


static ALLEGRO_HAPTIC *hapall_get_from_display(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *hapall_get_from_keyboard(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *hapall_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return NULL;
}


static int hapall_get_capabilities(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->get_capabilities(hapall->handle);
}


static double hapall_get_gain(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->get_gain(hapall->handle);
}


static bool hapall_set_gain(ALLEGRO_HAPTIC *dev, double gain)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->set_gain(hapall->handle, gain);
}


double hapall_get_autocenter(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->get_autocenter(hapall->handle);
}

static bool hapall_set_autocenter(ALLEGRO_HAPTIC *dev, double intensity)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->set_autocenter(hapall->handle, intensity);
}

static int hapall_get_num_effects(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->get_num_effects(hapall->handle);
}


static bool hapall_is_effect_ok(ALLEGRO_HAPTIC *dev,
                              ALLEGRO_HAPTIC_EFFECT *effect)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   return hapall->driver->is_effect_ok(hapall->handle, effect);
}


static bool hapall_upload_effect(ALLEGRO_HAPTIC *dev,
   ALLEGRO_HAPTIC_EFFECT *effect, ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL * hapall = hapall_from_al(dev);
   /* store the driver we'll need it later. */
   id->driver = hapall->driver;
   return hapall->driver->upload_effect(hapall->handle, effect, id);
}

static bool hapall_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loops)
{
   ALLEGRO_HAPTIC_DRIVER      *    driver = id->driver;
   /* Use the stored driver to perform the operation. */
   return driver->play_effect(id, loops);
}


static bool hapall_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_DRIVER      *    driver = id->driver;
   /* Use the stored driver to perform the operation. */
   return driver->stop_effect(id);
}


static bool hapall_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_DRIVER      *    driver = id->driver;
   /* Use the stored driver to perform the operation. */
   return driver->is_effect_playing(id);
}

static bool hapall_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_DRIVER      *    driver = id->driver;
   /* Use the stored driver to perform the operation. */
   return driver->release_effect(id);
}


static bool hapall_release(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_WINDOWS_ALL *hapall = hapall_from_al(haptic);
   return hapall->driver->release(hapall->handle);
}

#endif

/* vim: set sts=3 sw=3 et: */
