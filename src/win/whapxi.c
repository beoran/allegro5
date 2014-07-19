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
/* Don't compile this lot if xinput isn't supported. */

#ifndef ALLEGRO_WINDOWS
#error something is wrong with the makefile
#endif

#ifndef ALLEGRO_XINPUT_POLL_DELAY
#define ALLEGRO_XINPUT_POLL_DELAY 0.01
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

/* This is needed since the WINE header seem to lack this definition. */
#ifndef XINPUT_CAPS_FFB_SUPPORTED
#define XINPUT_CAPS_FFB_SUPPORTED 0x0001
#endif

ALLEGRO_DEBUG_CHANNEL("haptic")

/* Support at most 4 haptic devices. */
#define HAPTICS_MAX             4

/* Support at most 1 rumble effect per device, because
 * XInput doesn't really support uploading the effects. */
#define HAPTIC_EFFECTS_MAX     1

typedef struct ALLEGRO_HAPTIC_EFFECT_XINPUT {
   ALLEGRO_HAPTIC_EFFECT effect;
   XINPUT_VIBRATION      vibration;
   int id;
   double start_time;
   bool active;
   bool playing;
   bool stop_now;
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

static void * hapxi_poll_thread(ALLEGRO_THREAD *thread, void *arg);

ALLEGRO_HAPTIC_DRIVER _al_hapdrv_xinput =
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
/* For the background thread */
static ALLEGRO_THREAD * hapxi_thread = NULL;
static ALLEGRO_MUTEX  * hapxi_mutex = NULL;
/* Use acondition variable to put the thread to sleep and prevent too
 frequent polling*/
static ALLEGRO_COND   * hapxi_cond = NULL;

/* Forces vibration to stop immediately. */
static void hapxi_force_stop(ALLEGRO_HAPTIC_XINPUT * hapxi,
                           ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi)
{
   XINPUT_VIBRATION no_vibration = { 0, 0 };
   XInputSetState(hapxi->xjoy->index, &no_vibration);
   effxi->playing = false;
   effxi->stop_now = false;
}

/* Polls he xinput API for a single haptic device and effect. */
static void hapxi_poll_haptic_effect(ALLEGRO_HAPTIC_XINPUT * hapxi,
                              ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi)
{
   double delay  = effxi->effect.replay.delay;
   double length = effxi->effect.replay.length;
   double now    = al_get_time();
   
   /* If the effect is playing...*/ 
   if(effxi->playing) {
      double delta  = now - effxi->start_time;
      /* Playing longer than the total play time, so stop.
       * Aslo stop if the stop flag was set.
       * */
      if (effxi->stop_now || (delta > (delay + length)) ) {
         hapxi_force_stop(hapxi, effxi);
      } /* Playing longer than delay, start playing. */
      else if (delta > delay)  {
         XInputSetState(hapxi->xjoy->index, &effxi->vibration);
      }
   }
}

/* Polls the xinput API for a single haptic device. */
static void hapxi_poll_haptic(ALLEGRO_HAPTIC_XINPUT * hapxi)
{
   int i;

   for(i=0; i<HAPTIC_EFFECTS_MAX; i++) {
      if(!hapxi->effects[i].active) continue;
      hapxi_poll_haptic_effect(hapxi, hapxi->effects + i);
   }
}

/* Polls the xinput API for hapti effects and starts
 * or stops playback when needed.
 */
static void hapxi_poll_haptics(void)
{
   int i;
   
   for(i = 0; i < HAPTICS_MAX; i++) {
      if(!haptics[i].active) continue;
      hapxi_poll_haptic(haptics + i);
   }
}


 /* Function for the haptics polling thread. */
static void * hapxi_poll_thread(ALLEGRO_THREAD *thread, void *arg)
{
  ALLEGRO_TIMEOUT timeout;
  /* Poll once every 10 milliseconds. XXX: Should this be configurable? */
  al_init_timeout(&timeout, 0.01);
  while(!al_get_thread_should_stop(thread)) {
    al_lock_mutex(hapxi_mutex);
    /* Wait for the condition for the polling time in stead of using
     al_rest in the hope that this uses less CPU, and also allows the
     polling thread to be awoken when needed. */
    al_wait_cond_until(hapxi_cond, hapxi_mutex, &timeout);
    /* If we get here poll joystick for new input or connection
     * and dispatch events. */
    hapxi_poll_haptics();
    al_unlock_mutex(hapxi_mutex);
  }
  return arg;
}



/* Initializes the XInput haptic system. */
static bool hapxi_init_haptic(void)
{
   int i;

   ASSERT(hapxi_mutex == NULL);
   ASSERT(hapxi_thread == NULL);
   ASSERT(hapxi_cond == NULL);


   /* Create the mutex and a condition vaiable. */
   hapxi_mutex = al_create_mutex_recursive();
   if(!hapxi_mutex)
      return false;
   hapxi_cond = al_create_cond();
   if(!hapxi_cond)
      return false;

   al_lock_mutex(hapxi_mutex);

   for(i = 0; i < HAPTICS_MAX; i++) {
      haptics[i].active = false;
   }

    /* Now start a polling background thread, since XInput is a polled API,
     and also to make it possible for effects to stop running when their
     duration has passed. */
    hapxi_thread = al_create_thread(hapxi_poll_thread, NULL);
    al_unlock_mutex(hapxi_mutex);
    if (hapxi_thread) al_start_thread(hapxi_thread);
    return (hapxi_thread != NULL);
}


/* Converts a generic haptic device to a Windows-specific one. */
static ALLEGRO_HAPTIC_XINPUT * hapxi_from_al(ALLEGRO_HAPTIC *hap)
{
  return (ALLEGRO_HAPTIC_XINPUT *) hap;
}

static void hapxi_exit_haptic(void)
{
   ASSERT(hapxi_mutex);
   al_destroy_mutex(hapxi_mutex);
   hapxi_mutex = NULL;
}

/* Converts a float to a unsigned WORD range */
static bool hapxi_magnitude2win(WORD * word, double value) {
   if(!word) return false;
   (*word) = (WORD)(65535 * value);  
   return true;
} 

/* Converts Allegro haptic effect to xinput API. */
static bool hapxi_effect2win(
                            ALLEGRO_HAPTIC_EFFECT_XINPUT * effxi,
                            ALLEGRO_HAPTIC_EFFECT * effect,
                            ALLEGRO_HAPTIC_XINPUT * hapxi)
{
   (void) hapxi; 
   /* Generic setup */
   if (effect->type != ALLEGRO_HAPTIC_RUMBLE)
     return false;

   return
   hapxi_magnitude2win(&effxi->vibration.wLeftMotorSpeed ,
                        effect->data.rumble.weak_magnitude) &&
   hapxi_magnitude2win(&effxi->vibration.wRightMotorSpeed ,
                        effect->data.rumble.strong_magnitude);                        
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

   al_lock_mutex(hapxi_mutex);

   hapxi = haptics + joyxi->index;

   hapxi->parent.device = joyxi;
   hapxi->parent.from   = _AL_HAPTIC_FROM_JOYSTICK;
   hapxi->active        =  true;
   for (i = 0; i < HAPTIC_EFFECTS_MAX; i++) {
      hapxi->effects[i].active = false; /* not in use */
   }
   hapxi->parent.gain       = 1.0;
   hapxi->parent.autocenter = 0.0;
   hapxi->flags             = ALLEGRO_HAPTIC_RUMBLE;
   al_unlock_mutex(hapxi_mutex);

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
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(dev);
   return hapxi->flags;
}


static double hapxi_get_gain(ALLEGRO_HAPTIC *dev)
{
   (void) dev;
   /* Just return the 1.0, gain isn't supported  */
   return 1.0;
}


static bool hapxi_set_gain(ALLEGRO_HAPTIC *dev, double gain)
{
   (void) dev; (void) gain;
   /* Gain not supported*/
   return false;
}


double hapxi_get_autocenter(ALLEGRO_HAPTIC * dev)
{
   (void) dev;
   /* Autocenter not supported so return 0.0. */
   return 0.0;
}


static bool hapxi_set_autocenter(ALLEGRO_HAPTIC *dev, double intensity)
{
   (void) dev; (void) intensity;
   /* Autocenter not supported*/
   return false;
}

static int hapxi_get_num_effects(ALLEGRO_HAPTIC *dev)
{
   (void) dev;
   /* Support only a constant amount of effects */
   return HAPTIC_EFFECTS_MAX;
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
    int index;
    for (index = 0; index < HAPTIC_EFFECTS_MAX; index ++) {
      if (!hapxi->effects[index].active) {
        /* Set up ID here. */
        hapxi->effects[index].id  = index; 
        return  hapxi->effects + index;
      }
    }
    return NULL;
}

static bool hapxi_release_effect_windows(ALLEGRO_HAPTIC_EFFECT_XINPUT * effxi)
{
   effxi->active = false;
   return true;
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


   al_lock_mutex(hapxi_mutex);

   /* Is a haptic effect slot available? */
   effxi = hapxi_get_available_effect(hapxi);
   /* No more space for an effect. */
   if (!effxi) {
      ALLEGRO_WARN("No free effect slot.");
      al_unlock_mutex(hapxi_mutex);
      return false;
   }

   if (!hapxi_effect2win(effxi, effect, hapxi)) {
      ALLEGRO_WARN("Cannot convert haptic effect to XINPUT effect.");
      al_unlock_mutex(hapxi_mutex);
      return false;
   }
   
   effxi->effect= (*effect);
   /* set ID handle to signify success */
   id->_haptic  = dev;
   id->_pointer = effxi;
   id->_id      = effxi->id;
   id->_effect_duration = al_get_haptic_effect_duration(effect);

   al_unlock_mutex(hapxi_mutex);
   return ok;
}


static bool hapxi_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loops)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi;
   
   if ((!hapxi) || (id->_id < 0))
      return false;
   al_lock_mutex(hapxi_mutex);
   /* Simply set some flags. The polling thread will see this and start playing.
      after the effect's delay has passed. */
   effxi             = hapxi->effects + id->_id;
   effxi->playing    = true;
   effxi->start_time = al_get_time();
   id->_playing      = true;
   id->_start_time   = al_get_time();
   id->_end_time     = id->_start_time;
   id->_end_time    += id->_effect_duration * (double)loops;
   al_unlock_mutex(hapxi_mutex);
   return true;
}


static bool hapxi_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi;
   
   if ((!hapxi) || (id->_id < 0))
      return false;
   al_lock_mutex(hapxi_mutex);
   /* Simply set some flags. The polling thread will see this and stop playing.*/
   effxi = hapxi->effects + id->_id;
   effxi->stop_now = false;
   id->_playing = false;   
   al_unlock_mutex(hapxi_mutex);   
   return true;
}


static bool hapxi_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi;
   ALLEGRO_HAPTIC_EFFECT_XINPUT *effxi;
    
   ASSERT(id);
   hapxi = (ALLEGRO_HAPTIC_XINPUT *) id->_haptic;

   if ((!hapxi) || (id->_id < 0) || (!id->_playing))
      return false;

   effxi = hapxi->effects + id->_id;
   /* Simply return the effect's flag, it's up to the polling thread to
    * set this correctly.
    */
   return effxi->playing;
}


static bool hapxi_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_XINPUT *hapxi = (ALLEGRO_HAPTIC_XINPUT *)id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_XINPUT * effxi;
   if ((!hapxi) || (id->_id < 0))
      return false;
   
   effxi = hapxi->effects + id->_id;
   /* Forcefully stop since a normal stop may not be instant. */  
   hapxi_force_stop(hapxi, effxi);
   return hapxi_release_effect_windows(effxi);
}


static bool hapxi_release(ALLEGRO_HAPTIC *haptic)
{
   int index;
   ALLEGRO_HAPTIC_XINPUT *hapxi = hapxi_from_al(haptic);
   ASSERT(haptic);


   if (!hapxi->active)
      return false;

   /* Release all effects for this device. */
   for (index = 0; index < HAPTIC_EFFECTS_MAX ; index ++) {
     /* Forcefully stop since a normal stop may not be instant. */  
     hapxi_force_stop(hapxi, hapxi->effects + index);
     hapxi_release_effect_windows(hapxi->effects + index);
   }

   hapxi->active         = false;
   hapxi->parent.device  = NULL;
   return true;
}

#endif

/* vim: set sts=3 sw=3 et: */
