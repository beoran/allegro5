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
 *      OSX haptic (force-feedback) device driver.
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

#ifndef ALLEGRO_OSX
#error something is wrong with the makefile
#endif


#include <stdio.h>
#include <math.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <ForceFeedback/ForceFeedback.h>
#include <ForceFeedback/ForceFeedbackConstants.h>


#include "allegro5/internal/aintern_osxjoy.h"

/* Nominal maximum strength for force feedback effects. */
#define FF_NOMINALMAX 10000

/* Conversion factor from seconds to the internal numerical 
 * time representation. */
#define FF_SECONDS 1000

ALLEGRO_DEBUG_CHANNEL("ohaptic")

/* Support at most 32 haptic devices. */
#define HAPTICS_MAX             32
/* Support at most 16 effects per device. */
#define HAPTICS_EFFECTS_MAX     16
/* Support at most 3 axes per device. */
#define HAPTICS_AXES_MAX        3

/** This union is needed to avoid
 dynamical memory allocation. */
typedef union
{
   FFCONSTANTFORCE constant;
   FFRAMPFORCE     ramp;
   FFPERIODIC      periodic;
   FFCONDITION     condition;
   FFCUSTOMFORCE   custom;
} ALLEGRO_HAPTIC_PARAMETER_OSX;


/*
 * Haptic effect system data.
 */
typedef struct
{
   int                          id;
   bool                         active;
   FFEFFECT                     effect;
   FFENVELOPE                   envelope;
   FFEffectObjectReference      ref;
   DWORD                        axes[HAPTICS_AXES_MAX];
   LONG                         directions[HAPTICS_AXES_MAX];
   ALLEGRO_HAPTIC_PARAMETER_OSX parameter;
   const CFUUIDRef              guid;
} ALLEGRO_HAPTIC_EFFECT_OSX;


typedef struct
{
   struct ALLEGRO_HAPTIC        parent; /* must be first */
   bool                         active;
   io_service_t                 service;
   FFEffectDeviceReference      device; 
   CFUUIDRef                    guid;
   FFCAPABILITIES               capabilities;
   int                          flags;
   ALLEGRO_HAPTIC_EFFECT_OSX    effects[HAPTICS_EFFECTS_MAX];
   DWORD                        axes[HAPTICS_AXES_MAX];
   int                          naxes;
   int                          neffects;
} ALLEGRO_HAPTIC_OSX;


/* forward declarations */
static bool ohap_init_haptic(void);
static void ohap_exit_haptic(void);

static bool ohap_is_mouse_haptic(ALLEGRO_MOUSE * dev);
static bool ohap_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool ohap_is_keyboard_haptic(ALLEGRO_KEYBOARD * dev);
static bool ohap_is_display_haptic(ALLEGRO_DISPLAY * dev);
static bool ohap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT * dev);

static ALLEGRO_HAPTIC *ohap_get_from_mouse(ALLEGRO_MOUSE * dev);
static ALLEGRO_HAPTIC *ohap_get_from_joystick(ALLEGRO_JOYSTICK * dev);
static ALLEGRO_HAPTIC *ohap_get_from_keyboard(ALLEGRO_KEYBOARD * dev);
static ALLEGRO_HAPTIC *ohap_get_from_display(ALLEGRO_DISPLAY * dev);
static ALLEGRO_HAPTIC *ohap_get_from_touch_input(ALLEGRO_TOUCH_INPUT * dev);

static bool ohap_release(ALLEGRO_HAPTIC * haptic);

static bool ohap_get_active(ALLEGRO_HAPTIC * hap);
static int ohap_get_capabilities(ALLEGRO_HAPTIC * dev);
static double ohap_get_gain(ALLEGRO_HAPTIC * dev);
static bool ohap_set_gain(ALLEGRO_HAPTIC * dev, double);
static int ohap_get_num_effects(ALLEGRO_HAPTIC * dev);

static bool ohap_is_effect_ok(ALLEGRO_HAPTIC * dev,
                              ALLEGRO_HAPTIC_EFFECT * eff);
static bool ohap_upload_effect(ALLEGRO_HAPTIC * dev,
                               ALLEGRO_HAPTIC_EFFECT * eff,
                               ALLEGRO_HAPTIC_EFFECT_ID * id);
static bool ohap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID * id, int loop);
static bool ohap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID * id);
static bool ohap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID * id);
static bool ohap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID * id);

static double ohap_get_autocenter(ALLEGRO_HAPTIC * dev);
static bool ohap_set_autocenter(ALLEGRO_HAPTIC * dev, double);

ALLEGRO_HAPTIC_DRIVER _al_hapdrv_directx = {
   AL_HAPTIC_TYPE_OSX,
   "",
   "",
   "OSX haptic(s)",
   ohap_init_haptic,
   ohap_exit_haptic,

   ohap_is_mouse_haptic,
   ohap_is_joystick_haptic,
   ohap_is_keyboard_haptic,
   ohap_is_display_haptic,
   ohap_is_touch_input_haptic,

   ohap_get_from_mouse,
   ohap_get_from_joystick,
   ohap_get_from_keyboard,
   ohap_get_from_display,
   ohap_get_from_touch_input,

   ohap_get_active,
   ohap_get_capabilities,
   ohap_get_gain,
   ohap_set_gain,
   ohap_get_num_effects,

   ohap_is_effect_ok,
   ohap_upload_effect,
   ohap_play_effect,
   ohap_stop_effect,
   ohap_is_effect_playing,
   ohap_release_effect,

   ohap_release,

   ohap_get_autocenter,
   ohap_set_autocenter
};


static ALLEGRO_HAPTIC_OSX haptics[HAPTICS_MAX];
static ALLEGRO_MUTEX *haptic_mutex = NULL;

/* Map between OSX effect GUIDs and Allegro effect types. */
struct UUID_MAP
{
   CFUUIDRef uid;
   int allegro_bit;
};


static const struct UUID_MAP uuid_map[] = {
   { kFFEffectType_ConstantForce_ID , ALLEGRO_HAPTIC_CONSTANT},
   { kFFEffectType_Spring_ID        , ALLEGRO_HAPTIC_SPRING},
   { kFFEffectType_Friction_ID      , ALLEGRO_HAPTIC_FRICTION},
   { kFFEffectType_Damper_ID        , ALLEGRO_HAPTIC_DAMPER},
   { kFFEffectType_Inertia_ID       , ALLEGRO_HAPTIC_INERTIA},
   { kFFEffectType_RampForce_ID     , ALLEGRO_HAPTIC_RAMP},
   { kFFEffectType_Sine_ID          , ALLEGRO_HAPTIC_SINE},
   { kFFEffectType_Square_ID        , ALLEGRO_HAPTIC_SQUARE},
   { kFFEffectType_Triangle_ID      , ALLEGRO_HAPTIC_TRIANGLE},
   { kFFEffectType_SawtoothUp_ID    , ALLEGRO_HAPTIC_SAW_UP},
   { kFFEffectType_SawtoothDown_ID  , ALLEGRO_HAPTIC_SAW_DOWN},
   { kFFEffectType_CustomForce_ID   , ALLEGRO_HAPTIC_CUSTOM},
   { NULL                           , -1                   }
};

/* Map between OSX effect capability bits and Allegro effect types. */
struct CAP_MAP
{
   int cap;
   int allegro_bit;
};


static const struct CAP_MAP cap_map[] = {
   { FFCAP_ET_CONSTANTFORCE     , ALLEGRO_HAPTIC_CONSTANT       } ,
   { FFCAP_ET_SPRING            , ALLEGRO_HAPTIC_SPRING         } ,
   { FFCAP_ET_FRICTION          , ALLEGRO_HAPTIC_FRICTION       } ,
   { FFCAP_ET_DAMPER            , ALLEGRO_HAPTIC_DAMPER         } ,
   { FFCAP_ET_INERTIA           , ALLEGRO_HAPTIC_INERTIA        } ,
   { FFCAP_ET_RAMPFORCE         , ALLEGRO_HAPTIC_RAMP           } ,
   { FFCAP_ET_SINE              , ALLEGRO_HAPTIC_SINE           } ,
   { FFCAP_ET_SQUARE            , ALLEGRO_HAPTIC_SQUARE         } ,
   { FFCAP_ET_TRIANGLE          , ALLEGRO_HAPTIC_TRIANGLE       } ,
   { FFCAP_ET_SAWTOOTHUP        , ALLEGRO_HAPTIC_SAW_UP         } ,
   { FFCAP_ET_SAWTOOTHDOWN      , ALLEGRO_HAPTIC_SAW_DOWN       } ,
   { FFCAP_ET_CUSTOMFORCE       , ALLEGRO_HAPTIC_CUSTOM         } ,
   { -1                         , -1                            }
};


/** Converts a OSX force feedback error number to a 
 descriptive string */
static const char * ohap_error2string(HRESULT err)
{
    switch (err) {
    case FFERR_DEVICEFULL:
        return "Device full";
    case FFERR_DEVICEPAUSED:
        return "Device is paused";
    case FFERR_DEVICERELEASED:
        return "Device was released";
    case FFERR_EFFECTPLAYING:
        return "Effect is playing playing";
    case FFERR_EFFECTTYPEMISMATCH:
        return "Effect type mismatch";
    case FFERR_EFFECTTYPENOTSUPPORTED:
        return "Effect type is not supported";
    case FFERR_GENERIC:
        return "Generic error";
    case FFERR_HASEFFECTS:
        return "Device already has effects";
    case FFERR_INCOMPLETEEFFECT:
        return "Incomplete effect";
    case FFERR_INTERNAL:
        return "Internal error";
    case FFERR_INVALIDDOWNLOADID:
        return "Download ID is not valid";
    case FFERR_INVALIDPARAM:
        return "Parameter is not valid";
    case FFERR_MOREDATA:
        return "Need more data";
    case FFERR_NOINTERFACE:
        return "The interface is not supported";
    case FFERR_NOTDOWNLOADED:
        return "Effect could not be downloaded";
    case FFERR_NOTINITIALIZED:
        return "Object has not been initialized";
    case FFERR_OUTOFMEMORY:
        return "Out of memory";
    case FFERR_UNPLUGGED:
        return "Device is unplugged";
    case FFERR_UNSUPPORTED:
        return "Function call unsupported";
    case FFERR_UNSUPPORTEDAXIS:
        return "Axis unsupported";
    default:
        return "Unknown error";
    }
}



/* Initializes the haptics subystem on OSX. */
static bool ohap_init_haptic(void)
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

/* Returns a pointer the next available OSX haptic device slot or NULL 
 * if exhausted.
 */
static ALLEGRO_HAPTIC_OSX *ohap_get_available_haptic(void)
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

 /* Look for a free haptic effect slot for a device and return it,
  * or NULL if exhausted. Also initializes the effect
  * reference to NULL. */
static ALLEGRO_HAPTIC_EFFECT_OSX
    *ohap_get_available_effect(ALLEGRO_HAPTIC_OSX * ohap)
{
   ALLEGRO_HAPTIC_EFFECT_OSX *oeff;
   int i;
   for (i = 0; i < al_get_num_haptic_effects(&ohap->parent); i++) {
      if (!ohap->effects[i].active) {
         oeff           = ohap->effects + i;
         oeff->id       = i;
         oeff->active   = true;
         oeff->ref      = NULL;
         return oeff;
      }
   }
   return NULL;
}


/* Releases a osx haptics effect and unloads it from the device. */
static bool 
   ohap_release_effect_osx(
      ALLEGRO_HAPTIC_OSX        *ohap,
      ALLEGRO_HAPTIC_EFFECT_OSX *oeff)
{
   bool result = true;
   if (!oeff)
      return false;             /* make it easy to handle all cases later on. */
   if (!oeff->active)
      return false;             /* already not in use, bail out. */

   /* Unload the effect from the device. */
   if (oeff->ref) {
      HRESULT ret;
      ret = FFDeviceReleaseEffect(ohap->device, oeff->ref);
      if (ret != FF_OK) {
        ALLEGRO_ERROR("Could not release effect : %s.", ohap_error2string(ret));
        return false;
      }
   }
   /* Custom force needs to clean up it's data. */
   if (oeff->guid  == kFFEffectType_CustomForce_ID) {
      al_free(oeff->parameter.custom.rglForceData);
      oeff->parameter.custom.rglForceData = NULL;
   }
   oeff->active = false;        /* Not in use. */
   oeff->ref    = NULL;         /* No reference to effect anymore. */
   return result;
}


/* Converts a generic haptic device to a OSX-specific one. */
static ALLEGRO_HAPTIC_OSX *ohap_from_al(ALLEGRO_HAPTIC * hap)
{
   return (ALLEGRO_HAPTIC_OSX *) hap;
}

/* Shuts down the haptic subsystem. */
static void ohap_exit_haptic(void)
{
   ASSERT(haptic_mutex);
   al_destroy_mutex(haptic_mutex);
   haptic_mutex = NULL;
}

/* Convert the type of the periodic allegro effect to the osx effect*/
static bool ohap_periodictype2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                                  ALLEGRO_HAPTIC_EFFECT * effect)
{
   switch (effect->data.periodic.waveform) {
      case ALLEGRO_HAPTIC_SINE:
         oeff->guid = &kFFEffectType_Sine_ID;
         return true;

      case ALLEGRO_HAPTIC_SQUARE:
         oeff->guid = &kFFEffectType_Square_ID;
         return true;

      case ALLEGRO_HAPTIC_TRIANGLE:
         oeff->guid = &kFFEffectType_Triangle_ID;
         return true;

      case ALLEGRO_HAPTIC_SAW_UP:
         oeff->guid = &kFFEffectType_SawtoothUp_ID;
         return true;

      case ALLEGRO_HAPTIC_SAW_DOWN:
         oeff->guid = &kFFEffectType_SawtoothDown_ID;
         return true;

      case ALLEGRO_HAPTIC_CUSTOM:
         oeff->guid = &kFFEffectType_CustomForce_ID;
         return true;
      default:
         return false;
   }
}



/* Convert the type of the allegro effect to the osx effect*/
static bool ohap_type2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                          ALLEGRO_HAPTIC_EFFECT * effect)
{
   switch (effect->type) {
      case ALLEGRO_HAPTIC_RUMBLE:
         oeff->guid = &kFFEffectType_Sine_ID;
         return true;
      case ALLEGRO_HAPTIC_PERIODIC:
         return ohap_periodictype2osx(oeff, effect);
      case ALLEGRO_HAPTIC_CONSTANT:
         oeff->guid = &kFFEffectType_ConstantForce_ID;
         return true;
      case ALLEGRO_HAPTIC_SPRING:
         oeff->guid = &kFFEffectType_Spring_ID;
         return true;
      case ALLEGRO_HAPTIC_FRICTION:
         oeff->guid = &kFFEffectType_Friction_ID;
         return true;
      case ALLEGRO_HAPTIC_DAMPER:
         oeff->guid = &kFFEffectType_Damper_ID;
         return true;
      case ALLEGRO_HAPTIC_INERTIA:
         oeff->guid = &kFFEffectType_Inertia_ID;
         return true;
      case ALLEGRO_HAPTIC_RAMP:
         oeff->guid = &kFFEffectType_RampForce_ID;
         return true;
      default:
         return NULL;
   }
   return true;
}

/* Convert the direction of the allegro effect to the osx effect*/
static bool ohap_direction2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                               ALLEGRO_HAPTIC_EFFECT * effect,
                               ALLEGRO_HAPTIC_OSX * ohap)
{
   unsigned int index;
   double calc_x, calc_y;

   /* Use CARTESIAN coordinates since those seem to be the only well suppported
      ones. */
   oeff->effect.dwFlags = FFEFF_CARTESIAN;
   oeff->effect.cAxes = ohap->naxes;
   memset((void *)oeff->axes, 0, sizeof(oeff->axes));
   for (index = 0; index < oeff->effect.cAxes; index++) {
      oeff->axes[index] = ohap->axes[index];
   }
   oeff->effect.rgdwAxes = oeff->axes;
   /* Set up directions as well.. */
   memset((void *)oeff->directions, 0, sizeof(oeff->directions));
   /* Calculate the X and Y coordinates of the effect based on the angle.
      That is map angular coordinates to cartesian ones. */
   calc_x =
       sin(effect->direction.angle) * effect->direction.radius *
       FF_NOMINALMAX;
   calc_y =
       cos(effect->direction.angle) * effect->direction.radius *
       FF_NOMINALMAX;

   /* Set X if there is 1 axis and also y if there are more .
    */
   if (oeff->effect.cAxes > 1) {
      oeff->directions[0] = (long)calc_x;
   }
   if (ohap->naxes > 2) {
      oeff->directions[1] = (long)calc_y;
   }
   /* XXX: need to set the Z axis as well, perhaps??? */
   oeff->effect.rglDirection = oeff->directions;
   return true;
}

/* Converts the time in seconds to a OSX-compatible time.
 * Return false if out of bounds.
 */
static bool ohap_time2osx(DWORD * res, double sec)
{
   ASSERT(res);

   if (sec < 0.0 || sec >= 4294.967296)
      return false;
   (*res) = (DWORD) floor(sec * FF_SECONDS);
   return true;
}

/* Converts the level in range 0.0 to 1.0 to a OSX-compatible level.
 * Returns false if out of bounds.
 */
static bool ohap_level2osx(DWORD * res, double level)
{
   ASSERT(res);

   if (level < 0.0 || level > 1.0)
      return false;
   *res = (DWORD) floor(level * FF_NOMINALMAX);
   return true;
}


/* Converts the level in range -1.0 to 1.0 to a OSX-compatible level.
 * Returns false if out of bounds.
 */
static bool ohap_slevel2osx(LONG * res, double level)
{
   ASSERT(res);

   if (level < -1.0 || level > 1.0)
      return false;
   *res = (LONG) (level * FF_NOMINALMAX);
   return true;
}

/* Converts a phase in range 0.0 to 1.0 to a OSX-compatible level.
 * Returns false if out of bounds.
 */
static bool ohap_phase2osx(DWORD * res, double phase)
{
   ASSERT(res);

   if (phase < 0.0 || phase > 1.0)
      return false;
   *res = (DWORD) (phase * 35999);
   return true;
}


/* Converts replay data to Widows-compatible data. */
static bool ohap_replay2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                            ALLEGRO_HAPTIC_EFFECT * effect)
{
   return ohap_time2osx(&oeff->effect.dwStartDelay, effect->replay.delay)
       && ohap_time2osx(&oeff->effect.dwDuration, effect->replay.length);
}


/* Converts an Allegro haptic effect envelope to DirectInput API. */
static bool ohap_envelope2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                              ALLEGRO_HAPTIC_ENVELOPE * aenv)
{
   /* Prepare envelope. */
   FFENVELOPE *wenv = &oeff->envelope;

   /* Do not set any envelope if all values are 0.0  */
   if ((aenv->attack_length == 0.0) &&
       (aenv->fade_length == 0.0) &&
       (aenv->attack_level == 0.0) && (aenv->fade_level == 0.0)) {
      return true;
   }

   /* Prepare the envelope. */
   memset((void *)wenv, 0, sizeof(FFENVELOPE));
   oeff->envelope.dwSize = sizeof(FFENVELOPE);
   oeff->effect.lpEnvelope = wenv;

   /* Set the values. */
   return ohap_time2osx(&wenv->dwAttackTime, aenv->attack_length)
       && ohap_time2osx(&wenv->dwFadeTime, aenv->fade_length)
       && ohap_level2osx(&wenv->dwAttackLevel, aenv->attack_level)
       && ohap_level2osx(&wenv->dwFadeLevel, aenv->fade_level);
}

/* Converts a constant effect to Force Feedback Framework. */
static bool ohap_constant2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                              ALLEGRO_HAPTIC_EFFECT * effect)
{
   oeff->effect.cbTypeSpecificParams = sizeof(oeff->parameter.constant);
   oeff->effect.lpvTypeSpecificParams = &oeff->parameter.constant;
   return ohap_envelope2osx(oeff, &effect->data.constant.envelope)
       && ohap_slevel2osx(&oeff->parameter.constant.lMagnitude,
                          effect->data.constant.level);
}


/* Converts a ramp effect to Force Feedback Framework. */
static bool ohap_ramp2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                          ALLEGRO_HAPTIC_EFFECT * effect)
{
   oeff->effect.cbTypeSpecificParams = sizeof(oeff->parameter.ramp);
   oeff->effect.lpvTypeSpecificParams = &oeff->parameter.ramp;

   return ohap_envelope2osx(oeff, &effect->data.ramp.envelope)
       && ohap_slevel2osx(&oeff->parameter.ramp.lStart,
                          effect->data.ramp.start_level)
       && ohap_slevel2osx(&oeff->parameter.ramp.lEnd,
                          effect->data.ramp.end_level);
}

/* Converts a condition effect to Force Feedback Framework. */
static bool ohap_condition2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                               ALLEGRO_HAPTIC_EFFECT * effect)
{
   oeff->effect.cbTypeSpecificParams = sizeof(oeff->parameter.condition);
   oeff->effect.lpvTypeSpecificParams = &oeff->parameter.condition;
   /* XXX: no envelope here ???  */

   return ohap_level2osx(&oeff->parameter.condition.dwNegativeSaturation,
                         effect->data.condition.left_saturation)
       && ohap_level2osx(&oeff->parameter.condition.dwPositiveSaturation,
                         effect->data.condition.right_saturation)
       && ohap_slevel2osx(&oeff->parameter.condition.lNegativeCoefficient,
                          effect->data.condition.left_coeff)
       && ohap_slevel2osx(&oeff->parameter.condition.lPositiveCoefficient,
                          effect->data.condition.right_coeff)
       && ohap_slevel2osx(&oeff->parameter.condition.lDeadBand,
                          effect->data.condition.deadband)
       && ohap_slevel2osx(&oeff->parameter.condition.lOffset,
                          effect->data.condition.center);
}

/* Converts a custom effect to Force Feedback Framework. */
static bool ohap_custom2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                            ALLEGRO_HAPTIC_EFFECT * effect)
{
   int index;
   oeff->effect.cbTypeSpecificParams = sizeof(oeff->parameter.custom);
   oeff->effect.lpvTypeSpecificParams = &oeff->parameter.custom;
   oeff->parameter.custom.cChannels = 1;
   oeff->parameter.custom.cSamples = effect->data.periodic.custom_len;
   /* Use al malloc only in this case since the custom_data can be arbitrarily long. */
   oeff->parameter.custom.rglForceData =
       (LONG *) al_malloc(sizeof(LONG) * effect->data.periodic.custom_len);
   if (!oeff->parameter.custom.rglForceData)
      return false;
   /* Gotta copy this to long values, and scale them too... */
   for (index = 0; index < effect->data.periodic.custom_len; index++) {
      oeff->parameter.custom.rglForceData[index] =
          (LONG) (effect->data.periodic.custom_data[index] *
                  ((double)(1 << 31)));
   }
   return true;
}


/* Converts a periodic effect to Force Feedback Framework. */
static bool ohap_periodic2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                              ALLEGRO_HAPTIC_EFFECT * effect)
{
   if (effect->data.periodic.waveform == ALLEGRO_HAPTIC_CUSTOM) {
      return ohap_custom2osx(oeff, effect);
   }

   oeff->effect.cbTypeSpecificParams = sizeof(oeff->parameter.periodic);
   oeff->effect.lpvTypeSpecificParams = &oeff->parameter.periodic;

   return ohap_envelope2osx(oeff, &effect->data.periodic.envelope)
       && ohap_level2osx(&oeff->parameter.periodic.dwMagnitude,
                         effect->data.periodic.magnitude)
       && ohap_phase2osx(&oeff->parameter.periodic.dwPhase,
                         effect->data.periodic.phase)
       && ohap_time2osx(&oeff->parameter.periodic.dwPeriod,
                        effect->data.periodic.period)
       && ohap_slevel2osx(&oeff->parameter.periodic.lOffset,
                          effect->data.periodic.offset);
}


/* Converts a periodic effect to Force Feedback Framework. */
static bool ohap_rumble2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                            ALLEGRO_HAPTIC_EFFECT * effect)
{
   oeff->effect.cbTypeSpecificParams = sizeof(oeff->parameter.periodic);
   oeff->effect.lpvTypeSpecificParams = &oeff->parameter.periodic;

   return ohap_level2osx(&oeff->parameter.periodic.dwMagnitude,
                         effect->data.rumble.strong_magnitude)
       && ohap_phase2osx(&oeff->parameter.periodic.dwPhase, 0)
       && ohap_time2osx(&oeff->parameter.periodic.dwPeriod, 0.01)
       && ohap_slevel2osx(&oeff->parameter.periodic.lOffset, 0);
}

/* Converts Allegro haptic effect to dinput API. */
static bool ohap_effect2osx(ALLEGRO_HAPTIC_EFFECT_OSX * oeff,
                            ALLEGRO_HAPTIC_EFFECT * effect,
                            ALLEGRO_HAPTIC_OSX * ohap)
{
   /* Generic setup */
   memset((void *)oeff, 0, sizeof(*oeff));
   /* Set global stuff. */
   oeff->effect.dwSize          = sizeof(FFEFFECT);
   oeff->effect.dwGain          = FF_NOMINALMAX;
   oeff->effect.dwSamplePeriod  = 0;
   oeff->effect.dwFlags         = 0;
   oeff->effect.lpEnvelope      = NULL;
   /* Gain of the effect must be set to max, otherwise it won't be felt
      (enough) as the per effect gain multiplies with the per-device gain. */
   oeff->effect.dwGain          = FF_NOMINALMAX;
   /* This effect is not mapped to a trigger, and must be played explicitly. */
   oeff->effect.dwTriggerButton = FFEB_NOTRIGGER;

   if (!ohap_type2osx(oeff, effect)) {
      return false;
   }

   if (!ohap_direction2osx(oeff, effect, ohap)) {
      return false;
   }

   if (!ohap_replay2osx(oeff, effect)) {
      return false;
   }


   switch (effect->type) {
      case ALLEGRO_HAPTIC_RUMBLE:
         return ohap_rumble2osx(oeff, effect);
      case ALLEGRO_HAPTIC_PERIODIC:
         return ohap_periodic2osx(oeff, effect);
      case ALLEGRO_HAPTIC_CONSTANT:
         return ohap_constant2osx(oeff, effect);
      case ALLEGRO_HAPTIC_RAMP:
         return ohap_ramp2osx(oeff, effect);
      case ALLEGRO_HAPTIC_SPRING:
      case ALLEGRO_HAPTIC_FRICTION:
      case ALLEGRO_HAPTIC_DAMPER:
      case ALLEGRO_HAPTIC_INERTIA:
         return ohap_condition2osx(oeff, effect);
      default:
         return false;
   }
}

static bool ohap_get_active(ALLEGRO_HAPTIC * haptic)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(haptic);
   return ohap->active;
}


/* Checks if an io_service_t device handle represents a haptic device. */
static bool ohap_is_ios_haptic(io_service_t ios)
{
   HRESULT ret;
   ret = FFIsForceFeedback(ios);
   return (ret == FF_OK);
}   
   
static bool ohap_is_mouse_haptic(ALLEGRO_MOUSE * mouse)
{
   (void)mouse;
   return false;
}

static bool ohap_is_joystick_haptic(ALLEGRO_JOYSTICK * joy)
{
   ALLEGRO_JOYSTICK_OSX *joyosx = (ALLEGRO_JOYSTICK_OSX *) joy;
   (void)joyosx;
   if (!al_is_joystick_installed())
      return false;
   if (!al_get_joystick_active(joy))
      return false;
   ALLEGRO_DEBUG("Checking capabilities of joystick %s\n", joyosx->name);
   return ohap_is_ios_haptic(joyosx->service);
}


static bool ohap_is_display_haptic(ALLEGRO_DISPLAY * dev)
{
   (void)dev;
   return false;
}


static bool ohap_is_keyboard_haptic(ALLEGRO_KEYBOARD * dev)
{
   (void)dev;
   return false;
}


static bool ohap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT * dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *ohap_get_from_mouse(ALLEGRO_MOUSE * mouse)
{
   (void)mouse;
   return NULL;
}


/* Sets the force feedback gain on a ForceFeedback device.
 Returns true on success and false on failure.  */
static bool ohap_set_osx_device_gain(FFDeviceObjectReference device,
                                        double gain)
{
   HRESULT ret;
   UInt32  val;
   if (gain < 0.0)
      return false;
   if (gain > 1.0)
      return false;
   
   val = gain * FF_NOMINALMAX;
   
   ret = FFDeviceSetForceFeedbackProperty(device, FFPROP_FFGAIN, &val);
   return (ret == FF_OK);
}


/* Sets the force feedback autocentering intensity on a ForceFeedback device.
 Returns true on success and false on failure. */
static bool ohap_set_osx_device_autocenter(FFDeviceObjectReference device,
                                              double intensity)
{
   HRESULT ret;
   Uint32 prop;

   if (intensity < 0.0)
      return false;
   if (intensity > 1.0)
      return false;
   
   /* OSX only supports 0 to turn off or 1 to turn on auto centering. */
   if (intensity < 0.5) { 
      prop = 0;
   } 
   else {
      prop = 1;
   }

   ret = FFDeviceSetForceFeedbackProperty(device, FFPROP_AUTOCENTER, &prop);
   
   return (ret == FF_OK);
}


static bool ohap_get_capabilities(ALLEGRO_HAPTIC_OSX * ohap)
{
   HRESULT ret;
   FFDeviceObjectReference device;
   FFCAPABILITIES features;
   int index;
   unsigned int supported;
   Uint32 prop;
   ALLEGRO_HAPTIC *haptic = &ohap->parent;

   device = ohap->device;

   ret = FFDeviceGetForceFeedbackCapabilities(device, &features);
   if (ret != FF_OK) {
     ALLEGRO_ERROR("Can't get device capabilities: %s", ohap_error2string(ret));
     return false;
   }
   
   /* Get amount of effects that can be uploaded and played at the same 
    * time, with a maximum of HAPTICS_EFFECTS_MAX. */
   ohap->neffects   = features.playbackCapacity;
   if (ohap->neffects > features.playbackCapacity) {
      ohap->neffects = features.playbackCapacity;
   }
   if (ohap->neffects > HAPTIC_EFFECTS_MAX) {
      ohap->neffects = HAPTIC_EFFECTS_MAX;
   }
      
   /* Map capabilities to flags. */
   ohap->flags = 0;
   
   for(index = 0; index < (sizeof(cap_map) / sizeof(*cap_map)); index ++) {
      if (cap_map[index].cap & features.supportedEffects) {
        ohap->flags |= cap_map[index].allegro_bit; 
      }
   } 
   
   /* Are Gain and Autocenter supported?  */
   ret = FFDeviceGetForceFeedbackProperty(device, FFPROP_FFGAIN,
                                           &prop, sizeof(prop));
   if (ret == FF_OK) {
      ohap->flags |= ALLEGRO_HAPTIC_GAIN;
   }
      
   /* Checks if supports autocenter. */
   ret = FFDeviceGetForceFeedbackProperty(device, FFPROP_AUTOCENTER,
                                           &prop, sizeof(prop));
   if (ret == FF_OK) {
      ohap->flags |= ALLEGRO_HAPTIC_AUTOCENTER;
   }
   
   /* Check for axes, with a maximum of features.numFfAxes axes */
   ohap->naxes = features.numFfAxes;
   if (ohap->naxes > HAPTICS_AXES_MAX) {
      ohap->naxes = HAPTICS_AXES_MAX;
   }
   
   /* Copy usable axis information. */
   for(index = 0; index < ohap->naxes; index++) {
      ohap->axes[index] = features.ffAxes[index];
   } 
   
   /* Check if any periodic effects are supported. */
   bool periodic_ok = al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SINE);
   periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SQUARE);
   periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_TRIANGLE);
   periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SAW_DOWN);
   periodic_ok |= al_is_haptic_capable(haptic, ALLEGRO_HAPTIC_SAW_UP);

   if (periodic_ok) {
      /* If we have any of the effects above, we can use
         periodic and rumble effects. */
      ohap->flags |= (ALLEGRO_HAPTIC_PERIODIC | ALLEGRO_HAPTIC_RUMBLE);
   }

   
   /* Support angle and radius if we have at least 2 axes.
    * Azimuth is unlikely to be supported.
    */
   if (ohap->naxes >= 1) {
      ohap->flags |= ALLEGRO_HAPTIC_ANGLE;
      ohap->flags |= ALLEGRO_HAPTIC_RADIUS;
   }
   
   return true;
}

/* Initializes the haptic device for use with OSX */
static bool ohap_initialize_osx(ALLEGRO_HAPTIC_OSX * ohap)
{
   HRESULT ret;
   ALLEGRO_HAPTIC *haptic = &ohap->parent;
   
   /* Open the device from the io service. */
   ret = FFCreateDevice(ohap->service, &ohap->device) 
   if (ret != FF_OK) { 
      ALLEGRO_ERROR("Can't create device: %s", ohap_error2string(ret));
      return false;
   }
   
   /* Get the capabilities, fail if that fails. */
   if(!ohap_get_capabilities(ohap)) {
      return false;
   }
   
   /* Reset all actuators in case some where active */
   ret = FFDeviceSendForceFeedbackCommand(ohap->device,
                                          FFSFFC_RESET);
   if (ret != FF_OK) {
      ALLEGRO_ERROR("Can't reset device: %s", ohap_error2string(ret));
      return false;
   }
    
   /* Enable the actuators again */
   ret = FFDeviceSendForceFeedbackCommand(ohap->device,
                                          FFSFFC_SETACTUATORSON);
   if (ret != FF_OK) {
      ALLEGRO_ERROR("Can't enable actuators on device: %s", ohap_error2string(ret));
      return false;
   }
   
   
   /* Check gain and set it to 1.0 in one go. */
   if (ohap_set_osx_device_gain(ohap->device, 1.0)) {
      ohap->flags |= ALLEGRO_HAPTIC_GAIN;
   }

   /* Check autocenter and turn it off in one go. */
   if (ohap_set_osx_device_autocenter(ohap->device, 0.0)) {
      ohap->flags |= ALLEGRO_HAPTIC_AUTOCENTER;
   }

   return true;
}



static ALLEGRO_HAPTIC *ohap_get_from_joystick(ALLEGRO_JOYSTICK * joy)
{
   ALLEGRO_JOYSTICK_OSX *joyosx = (ALLEGRO_JOYSTICK_OSX *) joy;
   ALLEGRO_HAPTIC_OSX *ohap;

   int i;

   if (!al_is_joystick_haptic(joy))
      return NULL;

   al_lock_mutex(haptic_mutex);

   ohap = ohap_get_available_haptic();

   if (!ohap) {
      al_unlock_mutex(haptic_mutex);
      return NULL;
   }

   ohap->parent.device  = joy;
   ohap->parent.from    = _AL_HAPTIC_FROM_JOYSTICK;
   ohap->service        = joyosx->service;
   ohap->active         = true;
   
   for (i = 0; i < HAPTICS_EFFECTS_MAX; i++) {
      ohap->effects[i].active = false;  /* not in use */
   }
   
   ohap->parent.gain = 1.0;
   ohap->parent.autocenter = 0.0;

   /* Result is ok if init functions returns true. */
   if (!ohap_initialize_osx(ohap)) {
      al_release_haptic(&ohap->parent);
      al_unlock_mutex(haptic_mutex);
      return NULL;
   }

   al_unlock_mutex(haptic_mutex);

   return &ohap->parent;
}


static ALLEGRO_HAPTIC *ohap_get_from_display(ALLEGRO_DISPLAY * dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *ohap_get_from_keyboard(ALLEGRO_KEYBOARD * dev)
{
   (void)dev;
   return NULL;
}


static ALLEGRO_HAPTIC *ohap_get_from_touch_input(ALLEGRO_TOUCH_INPUT * dev)
{
   (void)dev;
   return NULL;
}


static int ohap_get_capabilities(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   return ohap->flags;
}


static double ohap_get_gain(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   /* Just return the stored gain, it's easier than querying. */
   return ohap->parent.gain;
}


static bool ohap_set_gain(ALLEGRO_HAPTIC * dev, double gain)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   bool ok = ohap_set_osx_device_gain(ohap->device, gain);
   if (ok) {
      ohap->parent.gain = gain;
   }
   else {
      ohap->parent.gain = 1.0;
   }
   return ok;
}


double ohap_get_autocenter(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   /* Return the stored autocenter value. It's easiest like that,
    * in case getting it is unsupported. */
   return ohap->parent.autocenter;
}


static bool ohap_set_autocenter(ALLEGRO_HAPTIC * dev, double intensity)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   bool ok = ohap_set_osx_device_autocenter(ohap->device, intensity);
   if (ok) {
      ohap->parent.autocenter = intensity;
   }
   else {
      ohap->parent.autocenter = 0.0;
   }
   return ok;
}

static int ohap_get_num_effects(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   return ohap->neffects;
}


static bool ohap_is_effect_ok(ALLEGRO_HAPTIC * haptic,
                              ALLEGRO_HAPTIC_EFFECT * effect)
{   
   if (al_is_haptic_capable(haptic, effect->type)) { 
      return true;
   }
   /* XXX: should do more checking here? */
   return false;
}


static bool ohap_upload_effect_helper
    (ALLEGRO_HAPTIC_OSX * ohap,
     ALLEGRO_HAPTIC_EFFECT_OSX * oeff, ALLEGRO_HAPTIC_EFFECT * effect)
{
   HRESULT ret;
   
   if(!ohap_effect2osx(oeff, effect, ohap)) {
      ALLEGRO_WARN("Could not convert haptic effect.\n");
      return false;
   }
   
   /* Create the effect. */
   ret = FFDeviceCreateEffect(ohap->device, oeff->guid,
                               &oeff->effect,
                               &oeff->ref);

   if (ret != FF_OK) {
      ALLEGRO_WARN("Could not create haptic effect %s.", ohap_error2string(ret));      
      return false;
   }
   
   /* AFAICS no upload is needed, create seems to do that. */
   return true;
}

static bool ohap_upload_effect(ALLEGRO_HAPTIC * dev,
                               ALLEGRO_HAPTIC_EFFECT * effect,
                               ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   bool ok = FALSE;
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(dev);
   ALLEGRO_HAPTIC_EFFECT_OSX *oeff = NULL;

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

   al_lock_mutex(haptic_mutex);

   /* Look for a free haptic effect slot. */
   oeff = ohap_get_available_effect(ohap);
   /* Returns NULL if there is no more space for an effect. */
   if (oeff) {
      if (ohap_upload_effect_helper(ohap, oeff, effect)) {
         /* set ID handle to signify success */
         id->_haptic = dev;
         id->_pointer = oeff;
         id->_id = oeff->id;
         id->_effect_duration = al_get_haptic_effect_duration(effect);
         ok = true;
      }
      else {
         ALLEGRO_WARN("Could not upload effect.");
      }
   }
   else {
      ALLEGRO_WARN("No free effect slot.");
   }

   al_unlock_mutex(haptic_mutex);
   return ok;
}


static bool ohap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID * id, int loops)
{
   HRESULT ret;
   ALLEGRO_HAPTIC_OSX *ohap = (ALLEGRO_HAPTIC_OSX *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_OSX *oeff;
   if ((!ohap) || (id->_id < 0))
      return false;
   oeff = ohap->effects + id->_id;

   ret = FFEffectStart(oeff->ref, loops, 0);
   if (ret != FF_OK) {
      ALLEGRO_WARN("Failed to play an effect: %s.", ohap_error2string(ret));
      return false;
   }
   id->_playing    = true;
   id->_start_time = al_get_time();
   id->_end_time   = id->_start_time;
   id->_end_time  += id->_effect_duration * (double)loops;
   return true;
}


static bool ohap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   HRESULT ret;
   ALLEGRO_HAPTIC_OSX *ohap = (ALLEGRO_HAPTIC_OSX *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_OSX *oeff;

   if ((!ohap) || (id->_id < 0))
      return false;

   oeff = ohap->effects + id->_id;
   ret  = FFEffectStop(oeff->ref);
   if (ret != FF_OK) {
      ALLEGRO_WARN("Failed to stop an effect: %s.", ohap_error2string(ret));
      return false;
   }

   id->_playing = false;   
   return true;
}


static bool ohap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   ASSERT(id);
   HRESULT res;
   FFEffectStatusFlag flags = 0;
   ALLEGRO_HAPTIC_OSX *ohap = (ALLEGRO_HAPTIC_OSX *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_OSX *oeff;

   if ((!ohap) || (id->_id < 0) || (!id->_playing))
      return false;

   oeff = ohap->effects + id->_id;

   res = FFEffectGetEffectStatus(oeff->ref, &flags);
   if (res != FF_OK) {
      ALLEGRO_WARN("Failed to get the status of effect.");
      /* If we get here, then use the play time in stead to
       * see if the effect should still be playing.
       * Do this because in case FFGetEffectStatus fails, we can't
       * assume the sample isn't playing. In fact, if the play command
       * was sucessful, it should still be playing as long as the play
       * time has not passed.
       */
      return (al_get_time() < id->_end_time);
   }
   return (flags != 0);
}



static bool ohap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   ALLEGRO_HAPTIC_OSX *ohap = (ALLEGRO_HAPTIC_OSX *) id->_haptic;
   ALLEGRO_HAPTIC_EFFECT_OSX *oeff;
   if ((!ohap) || (id->_id < 0))
      return false;

   ohap_stop_effect(id);

   oeff = ohap->effects + id->_id;
   return ohap_release_effect_osx(ohap, oeff);
}


static bool ohap_release(ALLEGRO_HAPTIC * haptic)
{
   ALLEGRO_HAPTIC_OSX *ohap = ohap_from_al(haptic);
   int index;
   HRESULT res;

   ASSERT(haptic);

   if (!ohap->active)
      return false;

   /* Release all effects for this device. */
   for (index = 0; index < HAPTICS_EFFECTS_MAX; index++) {
      ohap_release_effect_osx(ohap->effects + index);
   }

   /* Release the force feedback device. */
   res = FFReleaseDevice(ohap->device);
   
   if (res != FF_OK) {
      ALLEGRO_WARN("FFRelesseDevice failed for haptic device.\n");
   }

   ohap->active = false;
   ohap->device = NULL;
   return true;
}



/* vim: set sts=3 sw=3 et: */
