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

#ifndef ALLEGRO_WINDOWS
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
#include <dinput.h>
/* #include <sys/time.h> */

#include "allegro5/internal/aintern_wjoydxnu.h"

ALLEGRO_DEBUG_CHANNEL("whaptic")

/* Support at most 32 haptic devices. */
#define HAPTICS_MAX             32

/* Support at most 16 effects per device. */
#define HAPTICS_EFFECTS_MAX     16

/* Support at most 3 axes per device. */
#define HAPTICS_AXES_MAX        3

/*
 * Haptic system effect data.
 */
typedef struct 
{
   bool                active;
   DIEFFECT            effect;
   LPDIRECTINPUTEFFECT ref;
   /* XINPUT_VIBRATION vibration; */
} ALLEGRO_HAPTIC_EFFECT_WINDOWS ;



typedef struct
{
   struct ALLEGRO_HAPTIC parent; /* must be first */
   bool active;
   LPDIRECTINPUTDEVICE2 device;
   GUID                 guid;
   DIDEVICEINSTANCE     instance;
   DIDEVCAPS            capabilities;
   LPDIRECTINPUTDEVICE8 device8;

   
   int flags;
   ALLEGRO_HAPTIC_EFFECT_WINDOWS effects[HAPTICS_EFFECTS_MAX];
   DWORD axes[HAPTICS_AXES_MAX];
   int naxes;
  
} ALLEGRO_HAPTIC_WINDOWS;


#define LONG_BITS    (sizeof(long) * 8)
#define NLONGS(x)    (((x) + LONG_BITS - 1) / LONG_BITS)
/* Tests if a bit in an array of longs is set. */
#define TEST_BIT(nr, addr) \
   ((1UL << ((nr) % LONG_BITS)) & (addr)[(nr) / LONG_BITS])


/* forward declarations */
static bool whap_init_haptic(void);
static void whap_exit_haptic(void);

static bool whap_is_mouse_haptic(ALLEGRO_MOUSE *dev);
static bool whap_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool whap_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev);
static bool whap_is_display_haptic(ALLEGRO_DISPLAY *dev);
static bool whap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev);

static ALLEGRO_HAPTIC *whap_get_from_mouse(ALLEGRO_MOUSE *dev);
static ALLEGRO_HAPTIC *whap_get_from_joystick(ALLEGRO_JOYSTICK *dev);
static ALLEGRO_HAPTIC *whap_get_from_keyboard(ALLEGRO_KEYBOARD *dev);
static ALLEGRO_HAPTIC *whap_get_from_display(ALLEGRO_DISPLAY *dev);
static ALLEGRO_HAPTIC *whap_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev);

static bool whap_release(ALLEGRO_HAPTIC *haptic);

static bool whap_get_active(ALLEGRO_HAPTIC *hap);
static int whap_get_capabilities(ALLEGRO_HAPTIC *dev);
static double whap_get_gain(ALLEGRO_HAPTIC *dev);
static bool whap_set_gain(ALLEGRO_HAPTIC *dev, double);
static int whap_get_num_effects(ALLEGRO_HAPTIC *dev);

static bool whap_is_effect_ok(ALLEGRO_HAPTIC *dev, ALLEGRO_HAPTIC_EFFECT *eff);
static bool whap_upload_effect(ALLEGRO_HAPTIC *dev,
                               ALLEGRO_HAPTIC_EFFECT *eff,
                               ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool whap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loop);
static bool whap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool whap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id);
static bool whap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id);

static double whap_get_autocenter(ALLEGRO_HAPTIC *dev);
static bool whap_set_autocenter(ALLEGRO_HAPTIC *dev, double);

ALLEGRO_HAPTIC_DRIVER _al_hapdrv_windows =
{
   _ALLEGRO_HAPDRV_WINDOWS,
   "",
   "",
   "Windows haptic(s)",
   whap_init_haptic,
   whap_exit_haptic,

   whap_is_mouse_haptic,
   whap_is_joystick_haptic,
   whap_is_keyboard_haptic,
   whap_is_display_haptic,
   whap_is_touch_input_haptic,

   whap_get_from_mouse,
   whap_get_from_joystick,
   whap_get_from_keyboard,
   whap_get_from_display,
   whap_get_from_touch_input,

   whap_get_active,
   whap_get_capabilities,
   whap_get_gain,
   whap_set_gain,
   whap_get_num_effects,

   whap_is_effect_ok,
   whap_upload_effect,
   whap_play_effect,
   whap_stop_effect,
   whap_is_effect_playing,
   whap_release_effect,

   whap_release,
   
   whap_get_autocenter,
   whap_set_autocenter
};


static ALLEGRO_HAPTIC_WINDOWS haptics[HAPTICS_MAX];
static ALLEGRO_MUTEX *haptic_mutex = NULL;

/* Capability map between directinput effects and allegro effect types. */
struct CAP_MAP {
   GUID guid;
   int allegro_bit;
};

/* GUID values are borrowed from Wine */
#define DEFINE_PRIVATE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

DEFINE_PRIVATE_GUID(_al_GUID_None, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); 
   

static const struct CAP_MAP cap_map[] = {
   { { GUID_ConstantForce },    ALLEGRO_HAPTIC_CONSTANT },
   { { GUID_Spring },           ALLEGRO_HAPTIC_SPRING },
   { { GUID_Spring },           ALLEGRO_HAPTIC_FRICTION },
   { { GUID_Damper },           ALLEGRO_HAPTIC_DAMPER },
   { { GUID_Inertia },          ALLEGRO_HAPTIC_INERTIA },
   { { GUID_RampForce },        ALLEGRO_HAPTIC_RAMP },
   { { GUID_Square },           ALLEGRO_HAPTIC_SQUARE },
   { { GUID_Triangle },         ALLEGRO_HAPTIC_TRIANGLE },
   { { GUID_Sine },             ALLEGRO_HAPTIC_SINE },
   { { GUID_SawtoothUp },       ALLEGRO_HAPTIC_SAW_UP },
   { { GUID_SawtoothDown },     ALLEGRO_HAPTIC_SAW_DOWN },
   { { GUID_CustomForce },      ALLEGRO_HAPTIC_CUSTOM },
   { { _al_GUID_None    },      -1 }
};


static bool whap_init_haptic(void)
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


static ALLEGRO_HAPTIC_WINDOWS *whap_get_available_haptic(void)
{
   int i;

   for (i = 0; i < HAPTICS_MAX; i++) {
      if (!haptics[i].active) {
         haptics[i].active = true;
         return &haptics[i];
      }
   }

   return NULL;
}


/* Converts a generic haptic device to a Windows-specific one. */
static ALLEGRO_HAPTIC_WINDOWS *whap_from_al(ALLEGRO_HAPTIC *hap)
{
   return (ALLEGRO_HAPTIC_WINDOWS *) hap;
}


static void whap_exit_haptic(void)
{
   ASSERT(haptic_mutex);
   al_destroy_mutex(haptic_mutex);
   haptic_mutex = NULL;
}

#ifdef _COMMENT_
static bool whap_type2win(__u16 *res, int type)
{
   ASSERT(res);

   switch (type) {
      case ALLEGRO_HAPTIC_RUMBLE:
         (*res) = FF_RUMBLE;
         break;
      case ALLEGRO_HAPTIC_PERIODIC:
         (*res) = FF_PERIODIC;
         break;
      case ALLEGRO_HAPTIC_CONSTANT:
         (*res) = FF_CONSTANT;
         break;
      case ALLEGRO_HAPTIC_SPRING:
         (*res) = FF_SPRING;
         break;
      case ALLEGRO_HAPTIC_FRICTION:
         (*res) = FF_FRICTION;
         break;
      case ALLEGRO_HAPTIC_DAMPER:
         (*res) = FF_DAMPER;
         break;
      case ALLEGRO_HAPTIC_INERTIA:
         (*res) = FF_INERTIA;
         break;
      case ALLEGRO_HAPTIC_RAMP:
         (*res) = FF_RAMP;
         break;
      default:
         return false;
   }
   return true;
}


static bool whap_wave2lin(__u16 *res, int type)
{
   ASSERT(res);

   switch (type) {
      case ALLEGRO_HAPTIC_SQUARE:
         (*res) = FF_SQUARE;
         break;
      case ALLEGRO_HAPTIC_TRIANGLE:
         (*res) = FF_TRIANGLE;
         break;
      case ALLEGRO_HAPTIC_SINE:
         (*res) = FF_SINE;
         break;
      case ALLEGRO_HAPTIC_SAW_UP:
         (*res) = FF_SAW_UP;
         break;
      case ALLEGRO_HAPTIC_SAW_DOWN:
         (*res) = FF_SAW_DOWN;
         break;
      case ALLEGRO_HAPTIC_CUSTOM:
         (*res) = FF_CUSTOM;
         break;
      default:
         return false;
   }
   return true;
}


/* Converts the time in seconds to a Linux-compatible time.
 * Return false if out of bounds.
 */
static bool whap_time2lin(__u16 *res, double sec)
{
   ASSERT(res);

   if (sec < 0.0 || sec > 32.767)
      return false;
   (*res) = (__u16) round(sec * 1000.0);
   return true;
}


/* Converts the time in seconds to a Linux-compatible time.
 * Return false if out of bounds. This one allows negative times.
 */
static bool whap_stime2lin(__s16 *res, double sec)
{
   ASSERT(res);

   if (sec < -32.767 || sec > 32.767)
      return false;
   (*res) = (__s16) round(sec * 1000.0);
   return true;
}


/* Converts replay data to Linux-compatible data. */
static bool whap_replay2lin(struct ff_replay *lin,
   struct ALLEGRO_HAPTIC_REPLAY *al)
{
   return whap_time2lin(&lin->delay, al->delay)
      && whap_time2lin(&lin->length, al->length);
}


/* Converts the level in range 0.0 to 1.0 to a Linux-compatible level.
 * Returns false if out of bounds.
 */
static bool whap_level2lin(__u16 *res, double level)
{
   ASSERT(res);

   if (level < 0.0 || level > 1.0)
      return false;
   *res = (__u16) round(level * (double)0x7fff);
   return true;
}


/* Converts the level in range -1.0 to 1.0 to a Linux-compatible level.
 * Returns false if out of bounds.
 */
static bool whap_slevel2lin(__s16 *res, double level)
{
   ASSERT(res);

   if (level < -1.0 || level > 1.0)
      return false;
   *res = (__s16) round(level * (double)0x7ffe);
   return true;
}


/* Converts an Allegro haptic effect envelope to Linux input API. */
static bool whap_envelope2lin(struct ff_envelope *lin,
   struct ALLEGRO_HAPTIC_ENVELOPE *al)
{
   return whap_time2lin(&lin->attack_length, al->attack_length)
      && whap_time2lin(&lin->fade_length, al->fade_length)
      && whap_level2lin(&lin->attack_level, al->attack_level)
      && whap_level2lin(&lin->fade_level, al->fade_level);
}


/* Converts a rumble effect to Linux input API. */
static bool whap_rumble2lin(struct ff_rumble_effect *lin,
   struct ALLEGRO_HAPTIC_RUMBLE_EFFECT *al)
{
   return whap_level2lin(&lin->strong_magnitude, al->strong_magnitude)
      && whap_level2lin(&lin->weak_magnitude, al->weak_magnitude);
}


/* Converts a constant effect to Linux input API. */
static bool whap_constant2lin(struct ff_constant_effect *lin,
   struct ALLEGRO_HAPTIC_CONSTANT_EFFECT *al)
{
   return whap_envelope2lin(&lin->envelope, &al->envelope)
      && whap_slevel2lin(&lin->level, al->level);
}


/* Converts a ramp effect to Linux input API. */
static bool whap_ramp2lin(struct ff_ramp_effect *lin,
   struct ALLEGRO_HAPTIC_RAMP_EFFECT *al)
{
   return whap_envelope2lin(&lin->envelope, &al->envelope)
      && whap_slevel2lin(&lin->start_level, al->start_level)
      && whap_slevel2lin(&lin->end_level, al->end_level);
}


/* Converts a ramp effect to Linux input API. */
static bool whap_condition2lin(struct ff_condition_effect *lin,
   struct ALLEGRO_HAPTIC_CONDITION_EFFECT *al)
{
   return whap_slevel2lin(&lin->center, al->center)
      && whap_level2lin(&lin->deadband, al->deadband)
      && whap_slevel2lin(&lin->right_coeff, al->right_coeff)
      && whap_level2lin(&lin->right_saturation, al->right_saturation)
      && whap_slevel2lin(&lin->left_coeff, al->left_coeff)
      && whap_level2lin(&lin->left_saturation, al->left_saturation);
}


/* Converts a periodic effect to linux input API. */
static bool whap_periodic2lin(struct ff_periodic_effect *lin,
   struct ALLEGRO_HAPTIC_PERIODIC_EFFECT *al)
{
   /* Custom data is not supported yet, because currently no Linux
    * haptic driver supports it.
    */
   if (al->custom_data)
      return false;

   return whap_slevel2lin(&lin->magnitude, al->magnitude)
      && whap_stime2lin(&lin->offset, al->offset)
      && whap_time2lin(&lin->period, al->period)
      && whap_time2lin(&lin->phase, al->phase)
      && whap_wave2lin(&lin->waveform, al->waveform)
      && whap_envelope2lin(&lin->envelope, &al->envelope);
}


/* Converts Allegro haptic effect to Linux input API. */
static bool whap_effect2lin(struct ff_effect *lin, ALLEGRO_HAPTIC_EFFECT *al)
{
   memset(lin, 0, sizeof(*lin));

   if (!whap_type2lin(&lin->type, al->type))
      return false;
   /* lin_effect->replay = effect->re; */
   lin->direction = (__u16)
      round(((double)0xC000 * al->direction.angle) / (2 * M_PI));
   lin->id = -1;
   if (!whap_replay2lin(&lin->replay, &al->replay))
      return false;
   switch (lin->type) {
      case FF_RUMBLE:
         return whap_rumble2lin(&lin->u.rumble, &al->data.rumble);
      case FF_PERIODIC:
         return whap_periodic2lin(&lin->u.periodic, &al->data.periodic);
      case FF_CONSTANT:
         return whap_constant2lin(&lin->u.constant, &al->data.constant);
      case FF_RAMP:
         return whap_ramp2lin(&lin->u.ramp, &al->data.ramp);
      case FF_SPRING:   /* fall through */
      case FF_FRICTION: /* fall through */
      case FF_DAMPER:   /* fall through */
      case FF_INERTIA:
         return whap_condition2lin(&lin->u.condition[0], &al->data.condition);
      default:
         return false;
   }
}

#endif // __COMMENT__

static bool whap_get_active(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(haptic);
   return whap->active;
}


static bool whap_is_mouse_haptic(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return false;
}


static bool whap_is_joystick_haptic(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_DIRECTX *joydx = (ALLEGRO_JOYSTICK_DIRECTX *) joy;
   (void) joydx;
   if (!al_is_joystick_installed())
      return false;
   if (!al_get_joystick_active(joy))
      return false;
   return false;
   /*
   if (ljoy->fd <= 0)
      return false;
   return whap_fd_can_ff(ljoy->fd);
   */
}


static bool whap_is_display_haptic(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return false;
}


static bool whap_is_keyboard_haptic(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return false;
}


static bool whap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *whap_get_from_mouse(ALLEGRO_MOUSE *mouse)
{
   (void)mouse;
   return NULL;
}


/* Callback to check which effect types are supported. */
static BOOL CALLBACK
whap_check_effect_callback(LPCDIEFFECTINFO info, LPVOID data)
{
   ALLEGRO_HAPTIC * haptic = (ALLEGRO_HAPTIC *) data;
   ALLEGRO_HAPTIC_WINDOWS * whap = whap_from_al(haptic);
   
   const CAP_MAP * map;
   for (map = cap_map; map->allegro_bit != -1; map++) {
      if(GUID_EQUAL(info->guid, map->guid)) {
         whap->flags |= map->allegro_bit;
      }
   }
   /* Check for more supported effect types. */
   return DIENUM_CONTINUE;
}


/* Callback to check which axes are supported. */
static BOOL CALLBACK
whap_check_axes_calback(LPCDIDEVICEOBJECTINSTANCE dev, LPVOID data)
{
   ALLEGRO_HAPTIC * haptic = (ALLEGRO_HAPTIC *) data;
   ALLEGRO_HAPTIC_WINDOWS * whap = whap_from_al(haptic);
   
   whap->naxes = 0;
   if ((dev->dwType & DIDFT_AXIS) && (dev->dwFlags & DIDOI_FFACTUATOR)) {

        whap->axes[whap->naxes] = dev->dwOfs;
        whap->naxes++;
        
        /* Stop if the axes limit is reached */
        if (whap->naxes >= HAPTICS_AXES_MAX) {
            return DIENUM_STOP;
        }
    }

    return DIENUM_CONTINUE;
}


/* Initializes the haptic device for use with DirectInput */
static bool whap_initialize_dinput(ALLEGRO_HAPTIC_WINDOWS * whap) {
   HRESULT ret;
   ALLEGRO_HAPTIC * haptic = &whap->parent;
   DIPROPDWORD dipdw;
   
   DIDEVCAPS dicaps;
   /* Get capabilities. */
   memset((void *) &dicaps, 0, sizeof(dicaps));
   dicaps.dwSize = sizeof (dicaps);
   ret = IDirectInputDevice8_GetCapabilities(whap->device, &dicaps);
   if (FAILED(ret)) {
      ALLEGRO_WARN("IDirectInputDevice8_GetCapabilities failed on %p\n", whap->device);
      return false;
   }

   /** Is it a haptic device? */
   if ((dicaps.dwFlags & DIDC_FORCEFEEDBACK) != DIDC_FORCEFEEDBACK) {
      return false; 
   }

   
  
   /* Get number of axes. */
   ret = IDirectInputDevice8_EnumObjects(whap->device,
                                          whap_check_axes_calback,
                                          haptic, DIDFT_AXIS);
   if (FAILED(ret)) {
      ALLEGRO_WARN("Could not get haptic device axes ");
      return false;
   }

    /* Not needed to acquire the device since the JS driver did that. */

    /* Reset all actuators in case some where active */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(whap->device,
                                                       DISFFC_RESET);
    if (FAILED(ret)) {
        ALLEGRO_WARN("Could not reset haptic device");
    }

    /* Enable all actuators. */
    ret = IDirectInputDevice8_SendForceFeedbackCommand(whap->device,
                                                       DISFFC_SETACTUATORSON);
    if (FAILED(ret)) {
      ALLEGRO_WARN("Could not enable haptic device actuators ");
      return false;
    }

    /* Get known supported effects. */
    ret = IDirectInputDevice8_EnumEffects(whap->device,
                                          whap_check_effect_callback, haptic,
                                          DIEFT_ALL);
     if (FAILED(ret)) {
      ALLEGRO_WARN("Could not get haptic device supported effects ");
      return false;
     }  
    
    /** Check if any periodic effects are supported. */
    bool periodic_ok = al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SINE);
    periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SQUARE);
    periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_TRIANGLE);
    periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SAW_DOWN);
    periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SAW_UP);
 
    if (periodic_ok) { 
      /* If we have any of the effects above, we can use 
         periodic and rumble effects. */
      whap->flags |= (ALLEGRO_HAPTIC_PERIODIC | ALLEGRO_HAPTIC_RUMBLE); 
    }  
   
    /* Check gain capability and set it to maximum in one go. */
    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = 10000;
    ret = IDirectInputDevice8_SetProperty(whap->device,
                                          DIPROP_FFGAIN, &dipdw.diph);
    if (!FAILED(ret)) {         /* Gain is supported. */
        whap->flags |= ALLEGRO_HAPTIC_GAIN;
    }
    
    /* Check autocenter and turn it off in one go. */
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = DIPROPAUTOCENTER_OFF;
    ret = IDirectInputDevice8_SetProperty(whap->device,
                                          DIPROP_AUTOCENTER, &dipdw.diph);
    if (!FAILED(ret)) {         /* Autocenter is supported. */
        whap->flags |= ALLEGRO_HAPTIC_AUTOCENTER;
    }
    return true;
}




static ALLEGRO_HAPTIC *whap_get_from_joystick(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_DIRECTX * joydx = (ALLEGRO_JOYSTICK_DIRECTX *) joy;
   ALLEGRO_HAPTIC_WINDOWS * whap;
   ALLEGRO_HAPTIC * result = NULL;
   
   int i;

   if (!al_is_joystick_haptic(joy))
      return NULL;

   al_lock_mutex(haptic_mutex);

   whap = whap_get_available_haptic();

   if (!whap) {
      al_unlock_mutex(haptic_mutex);
      return NULL;
   }

   whap->parent.device = joy;
   whap->parent.from   = _AL_HAPTIC_FROM_JOYSTICK;

   whap->guid   = joydx->guid;
   whap->device = joydx->device;
   whap->active = true;
   for (i = 0; i < HAPTICS_EFFECTS_MAX; i++) {
      whap->effects[i].active = false; /* not in use */
   }
   whap->parent.gain = 1.0;
   whap->parent.autocenter = 0.0;
   
   /* result is ok if init functions returns true. */
   if (whap_initialize_dinput(whap)) {
      result = &whap->parent;  
   }

   al_unlock_mutex(haptic_mutex);

   return result;
}


static ALLEGRO_HAPTIC *whap_get_from_display(ALLEGRO_DISPLAY *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *whap_get_from_keyboard(ALLEGRO_KEYBOARD *dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *whap_get_from_touch_input(ALLEGRO_TOUCH_INPUT *dev)
{
   (void)dev;
   return NULL;
}


static int whap_get_capabilities(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);
   return whap->flags;
}


static double whap_get_gain(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);
   /* Unfortunately there seems to be no API to GET gain, only to set?!
    * So, return the stored gain.
    */
   return whap->parent.gain;
}


static bool whap_set_gain(ALLEGRO_HAPTIC *dev, double gain)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);
   (void) whap, (void) gain;
   return false;
}


double whap_get_autocenter (ALLEGRO_HAPTIC * dev) 
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);
   /* Unfortunately there seems to be no API to GET gain, only to set?!
    * So, return the stored gain.
    */
   return whap->parent.gain;
}


static bool whap_set_autocenter(ALLEGRO_HAPTIC *dev, double intensity)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);
   (void) whap, (void) intensity;
   return false;
}

static int whap_get_num_effects(ALLEGRO_HAPTIC *dev)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);
   int n_effects;
   (void) n_effects, (void)whap;
   
   return HAPTICS_EFFECTS_MAX;
}


static bool whap_is_effect_ok(ALLEGRO_HAPTIC *haptic,
                              ALLEGRO_HAPTIC_EFFECT *effect)
{
   int caps;

   caps = al_get_haptic_capabilities(haptic);
   if (caps & effect->type) {
      return true; // whap_effect2lin(&leff, effect);
   }
   return false;
}


static double whap_effect_duration(ALLEGRO_HAPTIC_EFFECT *effect)
{
   return effect->replay.delay + effect->replay.length;
}


static bool whap_upload_effect(ALLEGRO_HAPTIC *dev,
   ALLEGRO_HAPTIC_EFFECT *effect, ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(dev);

   ASSERT(dev);
   ASSERT(id);
   ASSERT(effect);

   /* Set id's values to indicate failure. */
   id->_haptic = NULL;
   id->_id     = -1;
   id->_handle = -1;
   return false;
}


static bool whap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID *id, int loops)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = (ALLEGRO_HAPTIC_WINDOWS *) id->_haptic;
   /*
   struct input_event play;
   int fd;
   double now;
   double duration;
   */

   if (!whap)
      return false;

   return false;
}


static bool whap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = (ALLEGRO_HAPTIC_WINDOWS *) id->_haptic;
 
   if (!whap)
      return false;
   return false;
}


static bool whap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ASSERT(id);

   /* Since AFAICS there is no Linux API to test this, use a timer to check
    * if the effect has been playing long enough to be finished or not.
    */
   return (id->_playing && al_get_time() < id->_end_time);
}


static bool whap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID *id)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = (ALLEGRO_HAPTIC_WINDOWS *)id->_haptic;

   whap_stop_effect(id);
   whap->effects[id->_id].active = false; /* not in use */
   return true;
}


static bool whap_release(ALLEGRO_HAPTIC *haptic)
{
   ALLEGRO_HAPTIC_WINDOWS *whap = whap_from_al(haptic);
   ASSERT(haptic);

   if (!whap->active)
      return false;

   whap->active = false;
   whap->device = NULL;
   return true;
}



/* vim: set sts=3 sw=3 et: */
