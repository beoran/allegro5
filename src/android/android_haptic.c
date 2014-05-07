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
#include "allegro5/allegro_android.h"
#include "allegro5/platform/aintandroid.h"
#include "allegro5/internal/aintern_android.h"
#include "allegro5/internal/aintern_haptic.h"

#include <jni.h>

ALLEGRO_DEBUG_CHANNEL("android_haptic")

#define HAPTIC_EFFECTS_MAX 32

typedef struct {
  int    id;
  bool   active;
  long   length;
  long   delay;
  long   pulse_length;
  long * pulse_data;
} ALLEGRO_HAPTIC_EFFECT_ANDROID; 

ALLEGRO_HAPTIC_EFFECT_ANDROID ahap_effects[HAPTIC_EFFECTS_MAX];

/* The android haptic driver is minimal, since it only 
 * wraps around the system Vibrator.
 */
typedef struct {
  ALLEGRO_HAPTIC parent;
  /* The vibrator system service object. */
  jobject        vibrator;
  bool           active;  
  int flags;
} ALLEGRO_HAPTIC_ANDROID; 

static ALLEGRO_HAPTIC_ANDROID ahap_display_haptic;



/* forward declarations */
static bool ahap_init_haptic(void);
static void ahap_exit_haptic(void);

static bool ahap_is_mouse_haptic(ALLEGRO_MOUSE *dev);
static bool ahap_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool ahap_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev);
static bool ahap_is_display_haptic(ALLEGRO_DISPLAY *dev);
static bool ahap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev);

static ALLEGRO_HAPTIC *ahap_get_from_mouse(ALLEGRO_MOUSE *dev);
static ALLEGRO_HAPTIC *ahap_get_from_joystick(ALLEGRO_JOYSTICK *dev);
static ALLEGRO_HAPTIC *ahap_get_from_keyboard(ALLEGRO_KEYBOARD *dev);
static ALLEGRO_HAPTIC *ahap_get_from_display(ALLEGRO_DISPLAY *dev);
static ALLEGRO_HAPTIC *ahap_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev);

static bool ahap_release(ALLEGRO_HAPTIC *haptic);

static bool ahap_get_active(ALLEGRO_HAPTIC *hap);
static int ahap_get_capabilities(ALLEGRO_HAPTIC *dev);
static double ahap_get_gain(ALLEGRO_HAPTIC *dev);
static bool ahap_set_gain(ALLEGRO_HAPTIC *dev, double);
static int ahap_get_num_effects(ALLEGRO_HAPTIC *dev);

static bool ahap_is_effect_ok(ALLEGRO_HAPTIC *dev, ALLEGRO_HAPTIC_EFFECT *eff);
static bool ahap_upload_effect(ALLEGRO_HAPTIC *dev,
                               ALLEGRO_HAPTIC_EFFECT *eff,
                               ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool ahap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loop);
static bool ahap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool ahap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool ahap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);

static double ahap_get_autocenter(ALLEGRO_HAPTIC *dev);
static bool ahap_set_autocenter(ALLEGRO_HAPTIC *dev, double);

ALLEGRO_HAPTIC_DRIVER _al_hapdrv_android =
{
   _ALLEGRO_HAPDRV_ANDROID,
   "",
   "",
   "Android haptic(s)",
   ahap_init_haptic,
   ahap_exit_haptic,

   ahap_is_mouse_haptic,
   ahap_is_joystick_haptic,
   ahap_is_keyboard_haptic,
   ahap_is_display_haptic,
   ahap_is_touch_input_haptic,

   ahap_get_from_mouse,
   ahap_get_from_joystick,
   ahap_get_from_keyboard,
   ahap_get_from_display,
   ahap_get_from_touch_input,

   ahap_get_active,
   ahap_get_capabilities,
   ahap_get_gain,
   ahap_set_gain,
   ahap_get_num_effects,

   ahap_is_effect_ok,
   ahap_upload_effect,
   ahap_play_effect,
   ahap_stop_effect,
   ahap_is_effect_playing,
   ahap_release_effect,

   ahap_release,
   
   ahap_get_autocenter,
   ahap_set_autocenter
};

static ALLEGRO_MUTEX *haptic_mutex = NULL;

static bool ahap_init_haptic(void)
{
   int i;

   ASSERT(haptic_mutex == NULL);
   haptic_mutex = al_create_mutex();
   if (!haptic_mutex)
      return false;

   for (i = 0; i < HAPTIC_EFFECTS_MAX; i++) {
      ahap_effects[i].id           = i;
      ahap_effects[i].active       = false;
      ahap_effects[i].pulse_data   = NULL;
      ahap_effects[i].pulse_length = 0;
      ahap_effects[i].length       = 0;
   }
   
   ahap_display_haptic.active = false;

   return true;
}


static ALLEGRO_HAPTIC_EFFECT_ANDROID *ahap_get_available_haptic_effect(void)
{
   int i;

   for (i = 0; i < HAPTIC_EFFECTS_MAX; i++) {
      if (!ahap_effects[i].active) {
         ahap_effects[i].active = true;         
         return &ahap_effects[i];
      }
   }

   return NULL;
}

static bool ahap_release_effect_android(ALLEGRO_HAPTIC_EFFECT_ANDROID * aeff) {
  if (aeff->active) return false;
  al_free(aeff->pulse_data);
  aeff->active       = false;
  aeff->pulse_data   = NULL;
  aeff->pulse_length = 0;
  aeff->length       = 0;
  return true;  
}

static void ahap_exit_haptic(void)
{
   int i;
   ASSERT(haptic_mutex);
   for (i = 0; i < HAPTIC_EFFECTS_MAX; i++) {
     ahap_release_effect_android(ahap_effects + i);
   }
   
   al_destroy_mutex(haptic_mutex);
   haptic_mutex = NULL;
}

static bool ahap_rumble2android(ALLEGRO_HAPTIC_EFFECT_ANDROID *aeff, 
                                ALLEGRO_HAPTIC_EFFECT *effect)
{
  aeff->length = effect->replay.length;
  aeff->delay  = effect->replay.delay;
  return true;
}

static bool ahap_periodic2android(ALLEGRO_HAPTIC_EFFECT_ANDROID *aeff, 
                                ALLEGRO_HAPTIC_EFFECT *effect)
{
  aeff->length = effect->replay.length;
  aeff->delay  = effect->replay.delay;
  return true;
}
  
/* Converts Allegro haptic effect to Linux input API. */
static bool ahap_effect2android(ALLEGRO_HAPTIC_EFFECT_ANDROID *aeff, 
                                ALLEGRO_HAPTIC_EFFECT *effect)
{
   switch (effect->type) {
      case ALLEGRO_HAPTIC_RUMBLE:
         return ahap_rumble2android(aeff, effect);
      case ALLEGRO_HAPTIC_PERIODIC:
         return ahap_periodic2android(aeff, effect);
      default:
         return false;
   }
}


static bool ahap_is_mouse_haptic(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return false;
}



static bool ahap_is_joystick_haptic(ALLEGRO_JOYSTICK *joy)
{
   (void) joy;
   return false;
}


static bool ahap_is_display_haptic(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return true; /* On Android, the display, that is, the device itself is haptic. */
}

static bool ahap_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return false;
}


static bool ahap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *ahap_get_from_mouse(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return NULL;
}


static ALLEGRO_HAPTIC *ahap_get_from_joystick(ALLEGRO_JOYSTICK *joy)
{       
  (void ) joy;
  return NULL;
}

static ALLEGRO_HAPTIC *ahap_get_from_display(ALLEGRO_DISPLAY *dev)
{
   (void) dev;
   al_lock_mutex(haptic_mutex);
   if(!ahap_display_haptic.active) {
     JNIEnv * env         = _al_android_get_jnienv();
     jstring service_str  = (*env)->NewStringUTF(env, "vibrator");
     ahap_display_haptic.vibrator = 
     _jni_callObjectMethodV(env, _al_android_activity_object(), "getSystemService",
                          "(Ljava/lang/String);Ljava/lang/Object", service_str);     
     ahap_display_haptic.active   = true;          
     ahap_display_haptic.flags = 
     ALLEGRO_HAPTIC_RUMBLE | ALLEGRO_HAPTIC_PERIODIC | ALLEGRO_HAPTIC_SINE | ALLEGRO_HAPTIC_CUSTOM;
   }   
   
   al_unlock_mutex(haptic_mutex);
   
   return &ahap_display_haptic.parent;
}


static ALLEGRO_HAPTIC *ahap_get_from_keyboard(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *ahap_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return NULL;
}



static ALLEGRO_HAPTIC_ANDROID * ahap_from_al(ALLEGRO_HAPTIC * hap) {
  return (ALLEGRO_HAPTIC_ANDROID *) hap;
}

static int ahap_get_capabilities(ALLEGRO_HAPTIC *hap)
{
  ALLEGRO_HAPTIC_ANDROID * ahap = ahap_from_al(hap);
  return ahap->flags;
}


static double ahap_get_gain(ALLEGRO_HAPTIC *dev)
{
  (void)dev;
  return 1.0;   
}


static bool ahap_set_gain(ALLEGRO_HAPTIC *dev, double gain)
{
  (void)dev; (void) gain;
  return false; /* not suppported */
}


static bool ahap_set_autocenter(ALLEGRO_HAPTIC *dev, double autocenter)
{
  (void)dev; (void)autocenter;
  return false;
}

static double ahap_get_autocenter(ALLEGRO_HAPTIC *dev)
{
  (void)dev;
  return 0.0;
}

int ahap_get_num_effects(ALLEGRO_HAPTIC *dev)
{
  (void)dev;
  return HAPTIC_EFFECTS_MAX;
}


static bool ahap_is_effect_ok(ALLEGRO_HAPTIC *haptic,
                              ALLEGRO_HAPTIC_EFFECT *effect)
{
   int caps;

   caps = al_get_haptic_capabilities(haptic);
   if (caps & effect->type) {
      return true;
   }
   return false;
}


static bool ahap_upload_effect(ALLEGRO_HAPTIC *dev,
   ALLEGRO_HAPTIC_EFFECT *effect, ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *ahap = ahap_from_al(dev);
   ( void ) ahap;
   
   ASSERT(dev);
   ASSERT(id);
   ASSERT(effect);
   
   ALLEGRO_HAPTIC_EFFECT_ANDROID * aeff;
   aeff = ahap_get_available_haptic_effect();
   if(!aeff) {
      ALLEGRO_WARN("No free effect slot.");
      return false;
   }

   /* Set id's values to indicate failure. */
   id->_haptic = NULL;
   id->_id = -1;
   id->_handle = -1;

   if (!ahap_effect2android(aeff, effect)) {
      ALLEGRO_WARN("ahap_effect2android failed");
      ahap_release_effect_android(aeff);
      return false;
   }
   /* Set id for success. */
   id->_haptic = dev;
   id->_id     = aeff->id;
   id->_pointer= aeff;
   id->_handle = 0;   
   id->_effect_duration = al_get_haptic_effect_duration(effect);
   id->_playing = false;

   return true;
}


static bool ahap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loops)
{
   ALLEGRO_HAPTIC_ANDROID * ahap = (ALLEGRO_HAPTIC_ANDROID *)  id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_ANDROID * aeff = 
      (ALLEGRO_HAPTIC_EFFECT_ANDROID *)  id->_pointer;
   

   double now;
   double duration;
   JNIEnv * env         = _al_android_get_jnienv();

   if (!ahap)
      return false;
  
   
   _jni_callObjectMethodV(env, ahap->vibrator, "vibrate", "(J)V", aeff->length);

   now = al_get_time();
   duration = loops * id->_effect_duration;

   id->_playing    = true;
   id->_start_time = now;
   id->_end_time   = now + duration;

   return true;
}


static bool ahap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *ahap = (ALLEGRO_HAPTIC_ANDROID *) id->_haptic;
   JNIEnv * env         = _al_android_get_jnienv();
   
   _jni_callObjectMethod(env, ahap->vibrator, "cancel", "()V");
   id->_playing = false;
   return true;
}


static bool ahap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ASSERT(id);

   /* Since AFAICS there is no  API to test this, use a timer to check
    * if the effect has been playing long enough to be finished or not.
    */
   return (id->_playing && al_get_time() < id->_end_time);
}


static bool ahap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_ANDROID *ahap = (ALLEGRO_HAPTIC_ANDROID *)id->_haptic;
   (void) ahap;
   ALLEGRO_HAPTIC_EFFECT_ANDROID * aeff = 
    (ALLEGRO_HAPTIC_EFFECT_ANDROID *)  id->_pointer;
   ahap_stop_effect(id); 
   ahap_release_effect_android(aeff);
   return true;
}


static bool ahap_release(ALLEGRO_HAPTIC *haptic)
{
   int index;
   ALLEGRO_HAPTIC_ANDROID *ahap = ahap_from_al(haptic);
   ASSERT(haptic);

   if (!ahap->active)
      return false;
   for (index = 0; index < HAPTIC_EFFECTS_MAX ; index++) {
    ahap_release_effect_android(ahap_effects + index); 
   }
   

   ahap->active   = false;
   ahap->vibrator = NULL;
   return true;
}

static bool ahap_get_active(ALLEGRO_HAPTIC *haptic) {
   ALLEGRO_HAPTIC_ANDROID *ahap = ahap_from_al(haptic);
   return ahap->active;
}


/* vim: set sts=3 sw=3 et: */
