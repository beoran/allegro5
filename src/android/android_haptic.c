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
 *      Android haptic (force-feedback) device driver.
 *
 *      By Beoran.
 *
 *      See LICENSE.txt for copyright information.
 */


#include "allegro5/allegro.h"
#include "allegro5/haptic.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/platform/aintandroid.h"
#include "allegro5/internal/aintern_android.h"
#include "allegro5/internal/aintern_haptic.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_joystick.h"
#include "allegro5/internal/aintern_bitmap.h"

#ifndef ALLEGRO_ANDROID
#error something is wrong with the makefile
#endif

#ifndef ALLEGRO_ANDROID_POLL_DELAY
#define ALLEGRO_ANDROID_POLL_DELAY 0.1
#endif

#include <stdio.h>
#include <math.h>


ALLEGRO_DEBUG_CHANNEL("haptic")


/* Support at most 1 haptic device (the display). */
#define HAPTICS_MAX             1

/* Index of the display's haptic device in the haptics[] array. */
#define HAPTICS_DISPLAY_INDEX   0

/* Support at most 1 rumble effect per device, because
 * the android Vibrator doesn't really support uploading the effects. */
#define HAPTIC_EFFECTS_MAX     1

/* Enum for the state of a haptic effect. The playback is managed by a small
 *  finite state machine.
 */

typedef enum ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE
{
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_INACTIVE = 0,
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY = 1,
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STARTING = 2,
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_PLAYING = 3,
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_DELAYED = 4,
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STOPPING = 5,
} ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE;

/* Allowed state transitions:
 * from inactive to ready,
 * from ready to starting
 * from starting to delayed or playing
 * from delayed to playing
 * from playing to delayed (for loops) or stopping
 * from stopping to ready
 * from ready to inactive
 */

typedef struct ALLEGRO_HAPTIC_EFFECT_ANDROID
{
   ALLEGRO_HAPTIC_EFFECT effect;
   /* os.android.VibrationEffect where supported. */ 
   jobject vibration;
   int id;
   double start_time;
   double loop_start;
   double stop_time;
   int repeats;
   int delay_repeated;
   int play_repeated;
   ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE state;
} ALLEGRO_HAPTIC_EFFECT_ANDROID;


typedef struct ALLEGRO_HAPTIC_ANDROID
{
   /* Important, parent must be first. */
   ALLEGRO_HAPTIC parent;
   /* os.android.Vibrator class */
   jclass vibrator;
   bool active;
   ALLEGRO_HAPTIC_EFFECT_ANDROID effect;
   int flags;
   /* Only one effect is supported since the Android vibrator allows only one 
    * vibration speed to be set to the device.  */
} ALLEGRO_HAPTIC_ANDROID;


/* forward declarations */
static bool hapan_init_haptic(void);
static void hapan_exit_haptic(void);

static bool hapan_is_mouse_haptic(ALLEGRO_MOUSE *dev);
static bool hapan_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool hapan_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev);
static bool hapan_is_display_haptic(ALLEGRO_DISPLAY *dev);
static bool hapan_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev);

static ALLEGRO_HAPTIC *hapan_get_from_mouse(ALLEGRO_MOUSE *dev);
static ALLEGRO_HAPTIC *hapan_get_from_joystick(ALLEGRO_JOYSTICK *dev);
static ALLEGRO_HAPTIC *hapan_get_from_keyboard(ALLEGRO_KEYBOARD *dev);
static ALLEGRO_HAPTIC *hapan_get_from_display(ALLEGRO_DISPLAY *dev);
static ALLEGRO_HAPTIC *hapan_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev);

static bool hapan_release(ALLEGRO_HAPTIC *haptic);

static bool hapan_get_active(ALLEGRO_HAPTIC *hap);
static int hapan_get_capabilities(ALLEGRO_HAPTIC *dev);
static double hapan_get_gain(ALLEGRO_HAPTIC *dev);
static bool hapan_set_gain(ALLEGRO_HAPTIC *dev, double);
static int hapan_get_max_effects(ALLEGRO_HAPTIC *dev);

static bool hapan_is_effect_ok(ALLEGRO_HAPTIC *dev, ALLEGRO_HAPTIC_EFFECT *eff);
static bool hapan_upload_effect(ALLEGRO_HAPTIC *dev,
                                ALLEGRO_HAPTIC_EFFECT *eff,
                                ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapan_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loop);
static bool hapan_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapan_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool hapan_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);

static double hapan_get_autocenter(ALLEGRO_HAPTIC *dev);
static bool hapan_set_autocenter(ALLEGRO_HAPTIC *dev, double);

static void *hapan_poll_thread(ALLEGRO_THREAD *thread, void *arg);

ALLEGRO_HAPTIC_DRIVER _al_hapdrv_android =
{
   ALLEGRO_HAPDRV_ANDROID,
   "",
   "",
   "Android haptic(s)",
   hapan_init_haptic,
   hapan_exit_haptic,

   hapan_is_mouse_haptic,
   hapan_is_joystick_haptic,
   hapan_is_keyboard_haptic,
   hapan_is_display_haptic,
   hapan_is_touch_input_haptic,

   hapan_get_from_mouse,
   hapan_get_from_joystick,
   hapan_get_from_keyboard,
   hapan_get_from_display,
   hapan_get_from_touch_input,

   hapan_get_active,
   hapan_get_capabilities,
   hapan_get_gain,
   hapan_set_gain,
   hapan_get_max_effects,

   hapan_is_effect_ok,
   hapan_upload_effect,
   hapan_play_effect,
   hapan_stop_effect,
   hapan_is_effect_playing,
   hapan_release_effect,

   hapan_release,

   hapan_get_autocenter,
   hapan_set_autocenter
};


static ALLEGRO_HAPTIC_ANDROID haptics[HAPTICS_MAX];
/* For the background thread */
static ALLEGRO_THREAD *hapan_thread = NULL;
static ALLEGRO_MUTEX  *hapan_mutex = NULL;
/* Use a condition variable to put the thread to sleep and prevent too
   frequent polling. */
static ALLEGRO_COND   *hapan_cond = NULL;


static bool hapan_activity_has_vibrator()
{
   JNIEnv * env = (JNIEnv *)_al_android_get_jnienv();
   return _jni_callBooleanMethodV(env, _al_android_activity_object(),
                                 "hasVibrator", "()Z");
}

static bool hapan_activity_cancel_vibrate()
{
   JNIEnv * env = (JNIEnv *)_al_android_get_jnienv();
   _jni_callVoidMethodV(env, _al_android_activity_object(),
                                 "cancelVibrate", "()V");
   return true;
}

static bool hapan_activity_vibrate(int milliseconds)
{
   JNIEnv * env = (JNIEnv *)_al_android_get_jnienv();
   jint jmilliseconds = milliseconds;
   _jni_callVoidMethodV(env, _al_android_activity_object(), "vibrate", "(I)V", 
                                 jmilliseconds);
   return true;
}


/* Forces vibration to stop immediately. */
static bool hapan_force_stop(ALLEGRO_HAPTIC_ANDROID *hapan,
                             ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{

   ALLEGRO_DEBUG("Android haptic effect stopped.\n");
   (void) hapan;
   hapan_activity_cancel_vibrate();
   effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY;
   return true;
}

/* Starts vibration immediately. If successfully also sets playing   */
static bool hapan_force_play(ALLEGRO_HAPTIC_ANDROID *hapan,
                             ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   double duration = effan->effect.replay.length * 1000;
   (void) hapan;
   bool res = hapan_activity_vibrate(((int)duration));
   ALLEGRO_DEBUG("Starting to play back haptic effect: %d.\n", (int)(res));
   if (res) {
      effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_PLAYING;
      return true;
   }
   else {
      effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY;
      return false;
   }
}



static bool hapan_poll_haptic_effect_ready(ALLEGRO_HAPTIC_ANDROID *hapan,
                                           ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   (void)hapan; (void)effan;
   /* when ready do nothing */
   return true;
}

static bool hapan_poll_haptic_effect_starting(ALLEGRO_HAPTIC_ANDROID *hapan,
                                              ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   /* when starting switch to delayed mode or play mode */
   double now = al_get_time();
   if ((now - effan->start_time) < effan->effect.replay.delay) {
      effan->loop_start = al_get_time();
      effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_DELAYED;
   }
   else {
      hapan_force_play(hapan, effan);
   }
   ALLEGRO_DEBUG("Polling Android haptic effect. Really Starting: %d!\n", effan->state);
   return true;
}

static bool hapan_poll_haptic_effect_playing(ALLEGRO_HAPTIC_ANDROID *hapan,
                                             ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   double now = al_get_time();
   double stop = effan->loop_start + effan->effect.replay.delay +
                 effan->effect.replay.length;
   (void)hapan;
   if (now > stop) {
      /* may need to repeat play because of "loop" in playback. */
      effan->play_repeated++;
      if (effan->play_repeated < effan->repeats) {
         /* need to play another loop. Stop playing now. */
         hapan_force_stop(hapan, effan);
         effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_DELAYED;
         effan->loop_start = al_get_time();
      }
      else {
         effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STOPPING;
      }
      return true;
   }
   return false;
}

static bool hapan_poll_haptic_effect_delayed(ALLEGRO_HAPTIC_ANDROID *hapan,
                                             ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   double now = al_get_time();
   if (now > (effan->loop_start + effan->effect.replay.delay)) {
      return hapan_force_play(hapan, effan);
   }
   return false;
}

static bool hapan_poll_haptic_effect_stopping(ALLEGRO_HAPTIC_ANDROID *hapan,
                                              ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   /* when stopping, force stop and go to ready state (hapan_force_stop does this)*/
   return hapan_force_stop(hapan, effan);
}


/* Polls the android API for a single haptic device and effect. */
static bool hapan_poll_haptic_effect(ALLEGRO_HAPTIC_ANDROID *hapan,
                                     ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   /* Check the state of the effect. */
   switch (effan->state) {
      case ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_INACTIVE:
         return false;
      case ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY:
         return hapan_poll_haptic_effect_ready(hapan, effan);
      case ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STARTING:
         return hapan_poll_haptic_effect_starting(hapan, effan);
      case ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_PLAYING:
         return hapan_poll_haptic_effect_playing(hapan, effan);
      case ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_DELAYED:
         return hapan_poll_haptic_effect_delayed(hapan, effan);
      case ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STOPPING:
         return hapan_poll_haptic_effect_stopping(hapan, effan);
      default:
         ALLEGRO_DEBUG("Android haptic effect state not valid :%d.\n", effan->state);
         return false;
   }
}

/* Polls the android API for a single haptic device. */
static void hapan_poll_haptic(ALLEGRO_HAPTIC_ANDROID *hapan)
{
   hapan_poll_haptic_effect(hapan, &hapan->effect);
}

/* Polls the android API for hapan effect and starts
 * or stops playback when needed.
 */
static void hapan_poll_haptics(void)
{
   int i;

   for (i = 0; i < HAPTICS_MAX; i++) {
      if (haptics[i].active) {
         hapan_poll_haptic(haptics + i);
      }
   }
}


/* Function for the haptics polling thread. */
static void *hapan_poll_thread(ALLEGRO_THREAD *thread, void *arg)
{
   ALLEGRO_TIMEOUT timeout;
   al_lock_mutex(hapan_mutex);
   while (!al_get_thread_should_stop(thread)) {
      /* Poll once every 10 milliseconds. XXX: Should this be configurable? */
      al_init_timeout(&timeout, ALLEGRO_ANDROID_POLL_DELAY);
      /* Wait for the condition to allow the
         polling thread to be awoken when needed. */
      al_wait_cond_until(hapan_cond, hapan_mutex, &timeout);
      /* If we get here poll joystick for new input or connection
       * and dispatch events. */
      hapan_poll_haptics();
   }
   al_unlock_mutex(hapan_mutex);
   return arg;
}



/* Initializes the XInput haptic system. */
static bool hapan_init_haptic(void)
{
   int i;

   ASSERT(hapan_mutex == NULL);
   ASSERT(hapan_thread == NULL);
   ASSERT(hapan_cond == NULL);


   /* Create the mutex and a condition vaiable. */
   hapan_mutex = al_create_mutex_recursive();
   if (!hapan_mutex)
      return false;
   hapan_cond = al_create_cond();
   if (!hapan_cond)
      return false;

   al_lock_mutex(hapan_mutex);

   for (i = 0; i < HAPTICS_MAX; i++) {
      haptics[i].active = false;
   }

   /* Now start a polling background thread, since XInput is a polled API,
      and also to make it possible for effects to stop running when their
      duration has passed. */
   hapan_thread = al_create_thread(hapan_poll_thread, NULL);
   al_unlock_mutex(hapan_mutex);
   if (hapan_thread) al_start_thread(hapan_thread);
   return(hapan_thread != NULL);
}


/* Converts a generic haptic device to an Android-specific one. */
static ALLEGRO_HAPTIC_ANDROID *hapan_from_al(ALLEGRO_HAPTIC *hap)
{
   return (ALLEGRO_HAPTIC_ANDROID *)hap;
}

static void hapan_exit_haptic(void)
{
   void *ret_value;
   ASSERT(hapan_thread);
   ASSERT(hapan_mutex);
   ASSERT(hapan_cond);

   /* Request the event thread to shut down, signal the condition, then join the thread. */
   al_set_thread_should_stop(hapan_thread);
   al_signal_cond(hapan_cond);
   al_join_thread(hapan_thread, &ret_value);

   /* clean it all up. */
   al_destroy_thread(hapan_thread);
   al_destroy_cond(hapan_cond);

   al_destroy_mutex(hapan_mutex);
   hapan_mutex = NULL;
}


/* Converts Allegro haptic effect to android API. */
static bool hapan_effect2android(
   ALLEGRO_HAPTIC_EFFECT_ANDROID *effan,
   ALLEGRO_HAPTIC_EFFECT *effect,
   ALLEGRO_HAPTIC_ANDROID *hapan)
{
   (void)hapan;
   (void) effan;
   /* Generic setup */
   if (effect->type != ALLEGRO_HAPTIC_RUMBLE) 
      return false;
   /* TODO: implement amplitude control for android api >= 26. */ 
   return true;
}

static bool hapan_get_active(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_from_al(haptic);
   return hapan->active;
}

static bool hapan_is_mouse_haptic(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return false;
}


static bool hapan_is_joystick_haptic(ALLEGRO_JOYSTICK *joy)
{
   /* This should forward to the generic Linux haptic driver, 
    * in case someone was able to connecte a hapric capable 
    * joystick to their Android device somehow. But currently it isn't 
    * supported yet.*/
   ALLEGRO_DEBUG("Haptic effects for joystics not supported yet on Android.");
   (void) joy;
   return false;
}

static bool hapan_is_display_haptic(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   /* For handheld devices, the display is the haptic device. */
   return hapan_activity_has_vibrator();
}

static bool hapan_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return false;
}


static bool hapan_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *hapan_get_from_mouse(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return NULL;
}


static ALLEGRO_HAPTIC *hapan_get_from_joystick(ALLEGRO_JOYSTICK *joy)
{
   if (!al_is_joystick_haptic(joy))
      return NULL;
      
   ALLEGRO_DEBUG("Haptic effects for joystics not supported yet on Android.");
   return NULL;
}


static ALLEGRO_HAPTIC *hapan_get_from_display(ALLEGRO_DISPLAY *dev)
{
   ALLEGRO_HAPTIC_ANDROID * hapan;

   if (!al_is_display_haptic(dev)) {
         return NULL;
   }

   al_lock_mutex(hapan_mutex);

   hapan = haptics +  HAPTICS_DISPLAY_INDEX;
   hapan->parent.driver = &_al_hapdrv_android;
   hapan->parent.device = dev;
   hapan->parent.from = _AL_HAPTIC_FROM_DISPLAY;
   hapan->active = true;
   hapan->effect.state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_INACTIVE;
   /* not in use */
   hapan->parent.gain = 1.0;
   hapan->parent.autocenter = 0.0;
   hapan->flags = ALLEGRO_HAPTIC_RUMBLE;
   al_unlock_mutex(hapan_mutex);

   return &hapan->parent;
}


static ALLEGRO_HAPTIC *hapan_get_from_keyboard(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *hapan_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return NULL;
}


static int hapan_get_capabilities(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_from_al(dev);
   return hapan->flags;
}


static double hapan_get_gain(ALLEGRO_HAPTIC *dev)
{
   (void)dev;
   /* Just return the 1.0, gain isn't supported  */
   return 1.0;
}


static bool hapan_set_gain(ALLEGRO_HAPTIC *dev, double gain)
{
   (void)dev; (void)gain;
   /* Gain not supported*/
   return false;
}


double hapan_get_autocenter(ALLEGRO_HAPTIC *dev)
{
   (void)dev;
   /* Autocenter not supported so return 0.0. */
   return 0.0;
}


static bool hapan_set_autocenter(ALLEGRO_HAPTIC *dev, double intensity)
{
   (void)dev; (void)intensity;
   /* Autocenter not supported*/
   return false;
}

static int hapan_get_max_effects(ALLEGRO_HAPTIC *dev)
{
   (void)dev;
   /* Support only one effect */
   return 1;
}


static bool hapan_is_effect_ok(ALLEGRO_HAPTIC *haptic,
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
static ALLEGRO_HAPTIC_EFFECT_ANDROID *
hapan_get_available_effect(ALLEGRO_HAPTIC_ANDROID *hapan)
{
   if (hapan->effect.state == ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_INACTIVE) {
      /* Set up ID here. */
      hapan->effect.id = 0;
      return &hapan->effect;
   }
   return NULL;
}

static bool hapan_release_effect_android(ALLEGRO_HAPTIC_EFFECT_ANDROID *effan)
{
   effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_INACTIVE;
   return true;
}

static bool hapan_upload_effect(ALLEGRO_HAPTIC *dev,
                                ALLEGRO_HAPTIC_EFFECT *effect, ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_from_al(dev);
   ALLEGRO_HAPTIC_EFFECT_ANDROID *effan = NULL;

   ASSERT(dev);
   ASSERT(id);
   ASSERT(effect);

   /* Set id's values to indicate failure beforehand. */
   id->_haptic = NULL;
   id->_id = -1;
   id->_pointer = NULL;
   id->_playing = false;
   id->_effect_duration = 0.0;
   id->_start_time = 0.0;
   id->_end_time = 0.0;

   if (!al_is_haptic_effect_ok(dev, effect))
      return false;

   al_lock_mutex(hapan_mutex);

   /* Is a haptic effect slot available? */
   effan = hapan_get_available_effect(hapan);
   /* No more space for an effect. */
   if (!effan) {
      ALLEGRO_WARN("No free effect slot.");
      al_unlock_mutex(hapan_mutex);
      return false;
   }

   if (!hapan_effect2android(effan, effect, hapan)) {
      ALLEGRO_WARN("Cannot convert haptic effect to ANDROID effect.\n");
      al_unlock_mutex(hapan_mutex);
      return false;
   }

   effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY;
   effan->effect = (*effect);
   /* set ID handle to signify success */
   id->_haptic = dev;
   id->_pointer = effan;
   id->_id = effan->id;
   id->_effect_duration = al_get_haptic_effect_duration(effect);

   al_unlock_mutex(hapan_mutex);
   return true;
}


static ALLEGRO_HAPTIC_ANDROID *
hapan_device_for_id(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   return (ALLEGRO_HAPTIC_ANDROID *)id->_haptic;
}


static ALLEGRO_HAPTIC_EFFECT_ANDROID *
hapan_effect_for_id(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   return (ALLEGRO_HAPTIC_EFFECT_ANDROID *)id->_pointer;
}

static bool hapan_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loops)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_device_for_id(id);
   ALLEGRO_HAPTIC_EFFECT_ANDROID *effan = hapan_effect_for_id(id);

   if ((!hapan) || (id->_id < 0) || (!effan) || (loops < 1))
      return false;
   al_lock_mutex(hapan_mutex);
   /* Simply set some flags. The polling thread will see this and start playing.
      after the effect's delay has passed. */
   effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STARTING;
   effan->start_time = al_get_time();
   effan->stop_time = effan->start_time + al_get_haptic_effect_duration(&effan->effect) * loops;
   effan->repeats = loops;
   effan->play_repeated = 0;
   effan->loop_start = effan->start_time;

   id->_playing = true;
   id->_start_time = al_get_time();
   id->_start_time = effan->start_time;
   id->_end_time = effan->stop_time;
   al_unlock_mutex(hapan_mutex);
   al_signal_cond(hapan_cond);
   return true;
}


static bool hapan_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_device_for_id(id);
   ALLEGRO_HAPTIC_EFFECT_ANDROID *effan = hapan_effect_for_id(id);

   if ((!hapan) || (id->_id < 0))
      return false;
   /* Simply set some flags. The polling thread will see this and stop playing.*/
   effan = (ALLEGRO_HAPTIC_EFFECT_ANDROID *)id->_pointer;
   if (effan->state <= ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY) return false;
   al_lock_mutex(hapan_mutex);
   effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_STOPPING;
   id->_playing = false;
   al_unlock_mutex(hapan_mutex);
   al_signal_cond(hapan_cond);
   return true;
}


static bool hapan_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *hapan;
   ALLEGRO_HAPTIC_EFFECT_ANDROID *effan;
   bool result;
   ASSERT(id);

   hapan = hapan_device_for_id(id);
   effan = hapan_effect_for_id(id);

   if ((!hapan) || (id->_id < 0) || (!id->_playing))
      return false;
   al_lock_mutex(hapan_mutex);
   ALLEGRO_DEBUG("Playing effect state: %d %p %lf %lf\n", effan->state, effan, al_get_time(), id->_end_time);

   result = (effan->state > ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_READY);
   al_unlock_mutex(hapan_mutex);
   al_signal_cond(hapan_cond);

   return result;
}


static bool hapan_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_device_for_id(id);
   ALLEGRO_HAPTIC_EFFECT_ANDROID *effan = hapan_effect_for_id(id);
   bool result;
   if ((!hapan) || (!effan))
      return false;

   al_lock_mutex(hapan_mutex);
   /* Forcefully stop since a normal stop may not be instant. */
   hapan_force_stop(hapan, effan);
   effan->state = ALLEGRO_HAPTIC_EFFECT_ANDROID_STATE_INACTIVE;
   result = hapan_release_effect_android(effan);
   al_unlock_mutex(hapan_mutex);
   return result;
}


static bool hapan_release(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_ANDROID *hapan = hapan_from_al(haptic);
   ASSERT(haptic);

   if (!hapan->active)
      return false;
   al_lock_mutex(hapan_mutex);

   /* Release the effect for this device. */
   /* Forcefully stop since a normal stop may not be instant. */
   hapan_force_stop(hapan, &hapan->effect);
   hapan_release_effect_android(&hapan->effect);

   hapan->active = false;
   hapan->parent.device = NULL;
   al_unlock_mutex(hapan_mutex);
   return true;
}

/* vim: set sts=3 sw=3 et: */
