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
 *      Haptic (that is, force feedback) routines for Allegro. 
 *      By Beoran (beoran@gmail.com), 2013.
 *
 *      See readme.txt for copyright information.
 */

#ifndef __al_included_allegro5_haptic_h
#define __al_included_allegro5_haptic_h

#include "allegro5/base.h"
#include "allegro5/events.h"
#include "allegro5/mouse.h"
#include "allegro5/joystick.h"

#ifdef __cplusplus
   extern "C" {
#endif

/* Enum: ALLEGRO_HAPTIC_CONSTANTS
 */
enum ALLEGRO_HAPTIC_CONSTANTS { 
  ALLEGRO_HAPTIC_RUMBLE       = 1 << 0,
  ALLEGRO_HAPTIC_PERIODIC     = 1 << 1,
  ALLEGRO_HAPTIC_CONSTANT     = 1 << 2,
  ALLEGRO_HAPTIC_SPRING       = 1 << 3,
  ALLEGRO_HAPTIC_FRICTION     = 1 << 4,
  ALLEGRO_HAPTIC_DAMPER       = 1 << 5,
  ALLEGRO_HAPTIC_INERTIA      = 1 << 6,
  ALLEGRO_HAPTIC_RAMP         = 1 << 7,
  ALLEGRO_HAPTIC_SQUARE       = 1 << 8,
  ALLEGRO_HAPTIC_TRIANGLE     = 1 << 9,
  ALLEGRO_HAPTIC_SINE         = 1 << 10,
  ALLEGRO_HAPTIC_SAW_UP       = 1 << 11,
  ALLEGRO_HAPTIC_SAW_DOWN     = 1 << 12,
  ALLEGRO_HAPTIC_CUSTOM       = 1 << 13,  
  ALLEGRO_HAPTIC_GAIN         = 1 << 14,   
  ALLEGRO_HAPTIC_ANGLE        = 1 << 15,
  ALLEGRO_HAPTIC_RADIUS       = 1 << 16,
  ALLEGRO_HAPRIC_AZIMUTH      = 1 << 17,
};



/* Type: ALLEGRO_HAPTIC
 */
typedef struct ALLEGRO_HAPTIC ALLEGRO_HAPTIC;

/* Direction of a haptic effect. Angle is a value between 0 and 2*M_PI.
 * An angle 0 means oriented towards the user, M_PI is away from the user 
 * (towards the screen). Angle is only supported if the device capabilities include
 * ALLEGRO_HAPTIC_ANGLE.  
 * Radius (if supported ) is the distance of the effect from the user 
 * as a value between 0 and 1. Normally it is 0. Radius is only supported if the 
 * device capabilities include ALLEGRO_HAPTIC_RADIUS .  
 * Azimuth is the angle of elevation, between -M_PI and M_PI. 0 points to the 
 * horizontal plane, -M_PI points down, and M_PI points up.
 * Azimuth is only supported if the device capabilities include 
 * ALLEGRO_HAPTIC_AZIMUTH.
 * 
 */
struct ALLEGRO_HAPTIC_DIRECTION {
  double angle; 
  double radius;
  double azimuth;
};

/* In all of the following structs, the doubles that express duration represent 
 * time in seconds. The double that represent levels of intensity are between 0.0 
 * and 1.0 that mean no effect and full 100% effect. */

/* Delay to start the replay and duration of the replay, expressed  in seconds. */
struct ALLEGRO_HAPTIC_REPLAY {
    double length;
    double delay;
};

/* Envelope of the effect. */
struct ALLEGRO_HAPTIC_ENVELOPE {
    double attack_length;
    double attack_level;
    double fade_length;
    double fade_level;
};

/* Constant effect.  Level is between 0.0 and 1.0. */
struct ALLEGRO_HAPTIC_CONSTANT_EFFECT {
    double level;
    struct ALLEGRO_HAPTIC_ENVELOPE envelope;
};

/* Ramp effect. Both start_level and end level are between 0.0 and 1.0.  */
struct ALLEGRO_HAPTIC_RAMP_EFFECT {
    double start_level;
    double end_level;
    struct ALLEGRO_HAPTIC_ENVELOPE envelope;
};

/* Condition effect. */
struct ALLEGRO_HAPTIC_CONDITION_EFFECT {
    double right_saturation;
    double left_saturation;
    double right_coeff;
    double left_coeff;
    double deadband;
    double center;
};

/* Periodic (wave) effect. */
struct ALLEGRO_HAPTIC_PERIODIC_EFFECT {
    int waveform;
    double period;
    double magnitude;
    double offset;
    double phase;
    
    struct ALLEGRO_HAPTIC_ENVELOPE envelope;
    int    custom_len;
    double *custom_data;
};

/* Simple rumble effect with a magnitude between 0.0 and 1.0 for both 
 the strong and the weak rumble motors in the haptic device.  */
struct ALLEGRO_HAPTIC_RUMBLE_EFFECT {
    double strong_magnitude;
    double weak_magnitude;
};

union ALLEGRO_HAPTIC_EFFECT_UNION {
    struct ALLEGRO_HAPTIC_CONSTANT_EFFECT   constant;
    struct ALLEGRO_HAPTIC_RAMP_EFFECT       ramp;
    struct ALLEGRO_HAPTIC_PERIODIC_EFFECT   periodic;
    struct ALLEGRO_HAPTIC_CONDITION_EFFECT  condition; 
    struct ALLEGRO_HAPTIC_RUMBLE_EFFECT     rumble;
};

/* Type: ALLEGRO_HAPTIC_EFFECT. This neeeds to be filled in and uploaded to
 * the haptic device before it can be played back. 
 */
struct ALLEGRO_HAPTIC_EFFECT {
        int                                type;
        int                                id;
        struct ALLEGRO_HAPTIC_DIRECTION    direction;
        struct ALLEGRO_HAPTIC_REPLAY       replay;
        union ALLEGRO_HAPTIC_EFFECT_UNION  data; 
};


/* Type: ALLEGRO_HAPTIC_EFFECT
 */
typedef struct ALLEGRO_HAPTIC_EFFECT ALLEGRO_HAPTIC_EFFECT;



/* Installs the haptic (force feedback) device subsystem. */
AL_FUNC(bool,             al_install_haptic          , (void));
/* Uninstalls the haptic device subsystem. */
AL_FUNC(void,             al_uninstall_haptic        , (void));
/* Returns true if the haptic device subsystem is installed, false if not. */
AL_FUNC(bool,             al_is_haptic_installed     , (void));
/* Checks if no new haptic devices became availabe and reconfigues the haptic 
 *subsystem. */
AL_FUNC(bool,             al_reconfigure_haptic      , (void));

/* Gets the amount of available haptic devices.*/
AL_FUNC(int,              al_get_num_haptics         , (void));

/* Opens and initializes the haptic device and returns a pointer 
 * to a device handle, or NULL on error.  */
AL_FUNC(ALLEGRO_HAPTIC *, al_get_haptic             , (int hapticn));
/* Closes the haptic device. */
AL_FUNC(void,             al_release_haptic          , (ALLEGRO_HAPTIC *));
/* Returns true if the haptic device can currently be used, false if not.*/
AL_FUNC(bool,             al_get_haptic_active       , (ALLEGRO_HAPTIC *));
/* Gets a string that describes the name of the haptic device.*/
AL_FUNC(const char*,      al_get_haptic_name         , (ALLEGRO_HAPTIC *));

/* Returns an integer with or'ed values from ALLEGRO_HAPTIC_CONSTANTS, that if
 set indicate that the haptic device supports the given feature. */
AL_FUNC(int,              al_get_haptic_capabilities , (ALLEGRO_HAPTIC *));

/* Sets the gain of the haptic device if supported. Gain is much like volume for sound, 
 it is as if every effect's intensity is multiplied by it. Gain is a value between 
 0.0 and 1.0. Returns true if set sucessfully, false if not.*/
AL_FUNC(bool,             al_set_haptic_gain         , (ALLEGRO_HAPTIC *, double gain));
/* Returns the current gain of the device. */
AL_FUNC(double,           al_get_haptic_gain         , (ALLEGRO_HAPTIC *));

/* Returns true if the mouse has haptic capabilities, false if not.*/
AL_FUNC(bool,             al_is_mouse_haptic         , (ALLEGRO_MOUSE *));

/* Returns true if the joystick has haptic capabilities, false if not.*/
AL_FUNC(bool,             al_is_joystick_haptic      , (ALLEGRO_JOYSTICK *));
/* If the mouse has haptic capabilities, returns the associated haptic device handle. 
 * Otherwise returns NULL;*/
AL_FUNC(ALLEGRO_HAPTIC *, al_get_haptic_from_mouse   , (ALLEGRO_MOUSE *));
/* If the mouse has haptic capabilities, returns the associated haptic device handle. 
 * Otherwise returns NULL;*/
AL_FUNC(ALLEGRO_HAPTIC *, al_get_haptic_from_joystick, (ALLEGRO_JOYSTICK *));

/* Returns the maximum amount of haptic effects that can be uploaded to the device. */
AL_FUNC(int,              al_get_num_haptic_effects  , (ALLEGRO_HAPTIC *));

/* Returns true if the haptic device can play the haptic effect as given, false if not. */
AL_FUNC(bool,             al_is_haptic_effect_ok     , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *));

/* Uploads the haptic effect to the device. In play_id, an integer is stored that is 
 a reference to be used to control playback of the effect.*/
AL_FUNC(bool,             al_upload_haptic_effect    , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *));

/* Plays back a previously uploaded haptic effect on this device. */
AL_FUNC(bool,             al_play_haptic_effect      , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT , * int loop));

/* Stops playing a haptic effect on this device. */
AL_FUNC(bool,             al_stop_haptic_effect      , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *));
/* Stops playing all haptic effects on this device. */
AL_FUNC(bool,             al_stop_all_haptic_effects , (ALLEGRO_HAPTIC *));

/* Returns true if the haptic effect is playing on the device false if not or if stopped. */
AL_FUNC(bool,             al_is_haptic_effect_playing, (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *));

/* Uploads a simple rumble effect to the haptic device and starts playback immediately.
 */
AL_FUNC(bool,             al_rumble_haptic           , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *, double intensity));

/* Event source for haptic device events. 
 *(XXX: currently no events are planned to be emitted, but that may change. )*/
AL_FUNC(ALLEGRO_EVENT_SOURCE *, al_get_haptic_event_source, (void));




#ifdef __cplusplus
}
#endif

#endif

