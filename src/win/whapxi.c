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
#include "allegro5/internal/aintern.h"
#include "allegro5/platform/aintwin.h"
#include "allegro5/internal/aintern_haptic.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_joystick.h"
#include "allegro5/internal/aintern_bitmap.h"

#ifdef ALLEGRO_CFG_XINPUT_BOOH
/* Don't compile this lot if xinput isn't supported. */

#ifndef ALLEGRO_XINPUT
#error something is wrong with the makefile
#endif

#ifdef ALLEGRO_MINGW32
   #undef MAKEFOURCC
#endif

#include <initguid.h>
#include <stdio.h>
#include <mmsystem.h>
#include <process.h>
#include <math.h>
#include <xinput.h>

#include "allegro5/internal/aintern_wjoyxi.h"

ALLEGRO_DEBUG_CHANNEL("hapxitic")

/* Support at most 4 haptic devices. */
#define HAPTICS_MAX             4

/* Support at most 1 rumble effect per device, because 
 * XInput doesn't really support uploading the effects. */
#define HAPTICS_EFFECTS_MAX     1

typedef ALLEGRO_HAPTIC_EFFECT_XINPUT {
   ALLEGRO_HAPTIC_EFFECT effect;
   XINPUT_VIBTATION      vibration;
   bool  active;
} ALLEGRO_HAPTIC_EFFECT_XINPUT;


typedef struct ALLEGRO_HAPTIC_XINPUT {
  int                           flags;
  ALLEGRO_HAPTIC                parent;
  ALLEGRO_JOYSTICK_XINPUT *     xjoy;
  bool                          active;
  ALLEGRO_HAPTIC_EFFECT_XINPUT  effects[HAPTIC_EFFECTS_MAX];
} ALLEGRO_HAPTIC_XINPUT;


/* forward declarations */
static bool hapxi_init_haptic(void);
static void hapxi_exit_haptic(void);

static bool hapxi_is_mouse_haptic(ALLEGRO_MOUSE *dev);
static bool hapxi_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool hapxi_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev);
static bool hapxi_is_display_haptic(ALLEGRO_DISPLAY *dev);
static bool hapxi_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev);

static ALLEGRO_HAPTIC *hapxi_get_from_mouse(ALLEGRO_MOUSE *dev);
static ALLEGRO_HAPTIC *hapxi_get_from_joystick(ALLEGRO_JOYSTICK *dev);
static ALLEGRO_HAPTIC *hapxi_get_from_keyboard(ALLEGRO_KEYBOARD *dev);
static ALLEGRO_HAPTIC *hapxi_get_from_display(ALLEGRO_DISPLAY *dev);
static ALLEGRO_HAPTIC *hapxi_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev);

static bool hapxi_release(ALLEGRO_HAPTIC *haptic);

static bool hapxi_get_active(ALLEGRO_HAPTIC *hap);
static int hapxi_get_capabilities(ALLEGRO_HAPTIC *dev);
static double hapxi_get_gain(ALLEGRO_HAPTIC *dev);
static bool hapxi_set_gain(ALLEGRO_HAPTIC *dev, double);
static int hapxi_get_num_effects(ALLEGRO_HAPTIC *dev);

static bool hapxi_is_effect_ok(ALLEGRO_HAPTIC *dev, ALLEGRO_HAPTIC_EFFECT *eff);
static bool hapxi_upload_effect(ALLEGRO_HAPTIC *dev,
                               ALLEGRO_HAPTIC_EFFECT *eff,
                               ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapxi_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loop);
static bool hapxi_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapxi_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapxi_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);

static double hapxi_get_autocenter(ALLEGRO_HAPTIC *dev);
static bool hapxi_set_autocenter(ALLEGRO_HAPTIC *dev, double);

ALLEGRO_HAPTIC_DRIVER _al_hapdrv_directx =
{
   AL_HAPTIC_TYPE_XINPUT,
   "",
   "",
   "Windows XInput haptic(s)",
   hapxi_init_haptic,
   hapxi_exit_haptic,

   hapxi_is_mouse_haptic,
   hapxi_is_joystick_haptic,
   hapxi_is_keyboard_haptic,
   hapxi_is_display_haptic,
   hapxi_is_touch_input_haptic,

   hapxi_get_from_mouse,
   hapxi_get_from_joystick,
   hapxi_get_from_keyboard,
   hapxi_get_from_display,
   hapxi_get_from_touch_input,

   hapxi_get_active,
   hapxi_get_capabilities,
   hapxi_get_gain,
   hapxi_set_gain,
   hapxi_get_num_effects,

   hapxi_is_effect_ok,
   hapxi_upload_effect,
   hapxi_play_effect,
   hapxi_stop_effect,
   hapxi_is_effect_playing,
   hapxi_release_effect,

   hapxi_release,

   hapxi_get_autocenter,
   hapxi_set_autocenter
};


static ALLEGRO_HAPTIC_XINPUT haptics[HAPTICS_MAX];
static ALLEGRO_MUTEX *haptic_mutex = NULL;


static bool hapxi_init_haptic(void)
{
   int i;

   ASSERT(haptic_mutex == NULL);
   haptic_mutex = al_create_mutex();
   if (!haptic_mutex)
      return false;

   for (i = 0; i < HAPTICS_MAX; i++) {
      haptics[i].active = false;
   }   

   return true;
}


/* Converts a generic haptic device to a Windows-specific one. */
static ALLEGRO_HAPTICS_XINPUT *hapxi_from_al(ALLEGRO_HAPTIC *hap)
{
  return (ALLEGRO_HAPTICS_XINPUT *) hap;
}

static void hapxi_exit_haptic(void)
{
   ASSERT(haptic_mutex);
   al_destroy_mutex(haptic_mutex);
   haptic_mutex = NULL;
}

/* Converts Allegro haptic effect to xinput API. */
static bool hapxi_effect2win(XINPUT_VIBRATION * vib,
                            ALLEGRO_HAPTIC_EFFECT * effect,
                            ALLEGRO_HAPTIC_XINPUT * hapxi)
{
   /* Generic setup */
   memset((void *) vib, 0, sizeof(*vib));
   if (effect->type != ALLEGRO_HAPTIC_RUMBLE_EFFECT) 
     return false;
   
   return true;
}

static bool hapxi_get_active(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(haptic);
   return hapxi->active;
}



static bool hapxi_is_mouse_haptic(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return false;
}


static bool hapxi_is_joystick_haptic(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_XINPUT *joyxi = (ALLEGRO_JOYSTICK_XINPUT *) joy;   
   if (!al_is_joystick_installed())
      return false;
   if (!al_get_joystick_active(joy))
      return false;
   
   return (joyxi->capabilities.Flags & XINPUT_CAPS_FFB_SUPPORTED);
}


static bool hapxi_is_display_haptic(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return false;
}


static bool hapxi_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return false;
}


static bool hapxi_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *hapxi_get_from_mouse(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return NULL;
}


static ALLEGRO_HAPTIC *hapxi_get_from_joystick(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_XINPUT * joyxi = (ALLEGRO_JOYSTICK_XINPUT *) joy;
   ALLEGRO_HAPTIC_XINPUT   * hapxi;

   int i;

   if (!al_is_joystick_haptic(joy))
      return NULL;

   al_lock_mutex(haptic_mutex);
   
   hapxi = haptics[joyxi->index];

   hapxi->parent.device = xjoy;
   hapxi->parent.from   = _AL_HAPTIC_FROM_JOYSTICK;   
   hapxi->active = true;
   for (i = 0; i < HAPTICS_EFFECTS_MAX; i++) {
      hapxi->effects[i].active = false; /* not in use */
   }
   hapxi->parent.gain       = 1.0;
   hapxi->parent.autocenter = 0.0;
   hapxi->flags              = ALLEGRO_HAPTIC_RUMBLE_EFFECT;
   al_unlock_mutex(haptic_mutex);

   return &hapxi->parent;
}


static ALLEGRO_HAPTIC *hapxi_get_from_display(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *hapxi_get_from_keyboard(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *hapxi_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return NULL;
}


static int hapxi_get_capabilities(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTICXINPUT *hapxi = hapxi_from_al(dev);
   return hapxi->flags;
}


static double hapxi_get_gain(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(dev);
   /* Just return the 1.0, gain isn't supported  */
   return 1.0;
}


static bool hapxi_set_gain(ALLEGRO_HAPTIC *dev, double gain)
{
   /* Gain not supported*/
   return false;
}


double hapxi_get_autocenter(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(dev);
   /* Autocenter not supported so return 0.0. */
   return 0.0;
}


static bool hapxi_set_autocenter(ALLEGRO_HAPTIC *dev, double intensity)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(dev);
   bool ok = hapxi_set_dinput_device_autocenter(hapxi->device, intensity);
   if (ok) {
     hapxi->parent.autocenter = intensity;
   } else {
     hapxi->parent.autocenter = 0.0;
   }
   return ok;
}

static int hapxi_get_num_effects(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(dev);
   int n_effects;
   (void) n_effects, (void)hapxi;

   return HAPTICS_EFFECTS_MAX;
}


static bool hapxi_is_effect_ok(ALLEGRO_HAPTIC *haptic,
                              ALLEGRO_HAPTIC_EFFECT *effect)
{
   int caps;

   caps = al_get_haptic_capabilities(haptic);
   if (caps & effect->type) {
      return true;
   }
   return false;
}

/* Gets an available haptic effect slot from the device or NULL if not 
 * available. 
 */
static ALLEGRO_HAPTIC_EFFECT_XINPUT * 
   hapxi_get_available_effect(ALLEGRO_HAPTIC_XINPUT * hapxi) 
{
    int index
    for(index = 0; index < HAPTIC_EFFECTS_MAX; index +) {
      if (!hapxi->effects[index].active) 
        return  hapxi->effects + index;
    }
    return NULL;
}

static bool hapxi_upload_effect(ALLEGRO_HAPTIC *dev,
   ALLEGRO_HAPTIC_EFFECT *effect, ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   bool ok;
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(dev);
   ALLEGRO_HAPTIC_EFFECT_XINPUT * effxi = NULL;

   ASSERT(dev);
   ASSERT(id);
   ASSERT(effect);

   /* Set id's values to indicate failure beforehand. */
   id->_haptic          = NULL;
   id->_id              = -1;
   id->_pointer         = NULL;
   id->_playing         = false;
   id->_effect_duration = 0.0;
   id->_start_time      = 0.0;
   id->_end_time        = 0.0;
   
   if(!al_is_haptic_effect_ok(dev, effect))
     return  false;
   

   al_lock_mutex(haptic_mutex);

   /* Is a haptic effect slot available? */
   effxi = hapxi_get_available_effect(hapxi);
   /* No more space for an effect. */
   if (!effxi) {
      ALLEGRO_WARN("No free effect slot.");
      al_unlock_mutex(haptic_mutex);      
      return false;
   }
   
   hapxi->effect   = (*effect);
   /* set ID handle to signify success */
   id->_haptic  = dev;
   id->_pointer = effxi;
   id->_id      = effxi->id;
   id->_effect_duration = al_get_haptic_effect_duration(effect);      
   
   al_unlock_mutex(haptic_mutex);
   return ok;
}


static bool hapxi_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loops)
{
   HRESULT res;
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi;
   if ((!hapxi) || (id->_id < 0))
      return false;
   
   effxi = hapxi->effects + id->_id;

  /* IDirectInputEffect_SetParameters(effxi->ref, effxi->effect, effxi-> flags); */
  
   res = IDirectInputEffect_Start(effxi->ref, loops, 0);
   if(FAILED(res)) {
      ALLEGRO_WARN("Failed to play an effect.");
      return false;
   }
   id->_playing = true;
   id->_start_time = al_get_time();
   id->_end_time   = id->_start_time;
   id->_end_time  += id->_effect_duration * (double)loops;
   return true;
}


static bool hapxi_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   HRESULT res;
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi;

   if ((!hapxi) || (id->_id < 0))
      return false;
   
   effxi = hapxi->effects + id->_id;

   res = IDirectInputEffect_Stop(effxi->ref);
   if(FAILED(res)) {
      ALLEGRO_WARN("Failed to play an effect.");
      return false;
   }
   id->_playing = false;


   return true;
}


static bool hapxi_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ASSERT(id);
   HRESULT res;
   DWORD flags = 0;
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi;

   if ((!hapxi) || (id->_id < 0) || (!id->_playing))
      return false;

   effxi = hapxi->effects + id->_id;
    
   res = IDirectInputEffect_GetEffectStatus(effxi->ref, &flags);
   if(FAILED(res)) {
      ALLEGRO_WARN("Failed to get the status of effect.");
      /* If we get here, then use the play time in stead to 
       * see if the effect should still be playing. 
       * Do this because in case GeteffectStatus fails, we can't
       * assume the sample isn't playing. In fact, if the play command 
       * was sucessful, it should still be playing as long as the play 
       * time has not passed. 
       */
      return (al_get_time() < id->_end_time);
   }   
   if (flags & DIEGES_PLAYING) return true;
    /* WINE is bugged here, it doesn't set flags, but it also 
    * just returns DI_OK. Thats why here, don't believe the API 
    * when it the playing flag isn't set if the effect's duration
    * has not passed. On real Windows it should probably always be the 
    * case that the effect will have played completely when 
    * the play time has ended.
    */
   return (al_get_time() < id->_end_time);
}



static bool hapxi_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *)id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT * effxi;
   if ((!hapxi) || (id->_id < 0))
      return false;
   
   hapxi_stop_effect(id);

   effxi = hapxi->effects + id->_id;
   return hapxi_release_effect_windows(effxi);
}


static bool hapxi_release(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(haptic);
   ASSERT(haptic);
   int index;

   if (!hapxi->active)
      return false;

   /* Release all effects for this device. */
   for (index = 0; index < HAPTICS_EFFECTS_MAX ; index ++) {
     hapxi_release_effect_windows(hapxi->effects + index);
   }

   hapxi->active = false;
   hapxi->device = NULL;
   return true;
}

#endif

/* vim: set sts=3 sw=3 et: */
