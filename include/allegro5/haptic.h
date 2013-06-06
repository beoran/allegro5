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

/* Enum: ALLEGRO_HAPTIC_FLAGS
 */
enum ALLEGRO_HAPTIC_FLAGS { 
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
  ALLEGRO_HAPTIC_AUTOCENTER   = 1 << 15,  
};


/* Type: ALLEGRO_HAPTIC
 */
typedef struct ALLEGRO_HAPTIC ALLEGRO_HAPTIC;

/* Direction of a haptic effect. Angle is a value between 0 and 2*M_PI.
 * An angle 0 means oriented towards the user, M_PI is away from the user 
 * (towards the screen). 
 * Radius (if supported ) is the diistance of the effect from the user 
 * as a value between 0 and 1. Normally it is 0. 
 * Azimuth is the angle of elevation, between -M_PI and M_PI. 0 points to the 
 * horizontal plane, -M_PI points down, and M_PI points up.
 * 
 */
struct ALLEGRO_HAPTIC_DIRECTION {
  double angle; 
  double radius;
  double azimuth;
};

struct ALLEGRO_HAPTIC_REPLAY {
    double length;
    double delay;
};

struct ALLEGRO_HAPTIC_ENVELOPE {
    double attack_length;
    double attack_level;
    double fade_length;
    double fade_level;
};

struct ALLEGRO_HAPTIC_CONSTANT_EFFECT {
    double level;
    struct ALLEGRO_HAPTIC_ENVELOPE envelope;
};

struct ALLEGRO_HAPTIC_RAMP_EFFECT {
    double start_level;
    double end_level;
    struct ALLEGRO_HAPTIC_ENVELOPE envelope;
};

struct ALLEGRO_HAPTIC_CONDITION_EFFECT {
    double right_saturation;
    double left_saturation;
    double right_coeff;
    double left_coeff;
    double deadband;
    double center;
};

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

/* Type: ALLEGRO_HAPTIC_EFFECT
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




AL_FUNC(bool,             al_install_haptic          , (void));
AL_FUNC(void,             al_uninstall_haptic        , (void));
AL_FUNC(bool,             al_is_haptic_installed     , (void));
AL_FUNC(bool,             al_reconfigure_haptic      , (void));

AL_FUNC(int,              al_get_num_haptics         , (void));
AL_FUNC(ALLEGRO_HAPTIC *, al_get_haptic              , (int hapticn));
AL_FUNC(void,             al_release_haptic          , (ALLEGRO_HAPTIC *));
AL_FUNC(bool,             al_get_haptic_active       , (ALLEGRO_HAPTIC *));
AL_FUNC(const char*,      al_get_haptic_name         , (ALLEGRO_HAPTIC *));

AL_FUNC(int,              al_get_haptic_flags        , (ALLEGRO_HAPTIC *)); 

AL_FUNC(bool,             al_is_mouse_haptic         , (ALLEGRO_MOUSE *));
AL_FUNC(bool,             al_is_joystick_haptic      , (ALLEGRO_JOYSTICK *));
AL_FUNC(ALLEGRO_HAPTIC *, al_get_haptic_from_mouse   , (ALLEGRO_MOUSE *));
AL_FUNC(ALLEGRO_HAPTIC *, al_get_haptic_from_joystick, (ALLEGRO_JOYSTICK *));
AL_FUNC(int,              al_get_haptic_num_axes     , (ALLEGRO_HAPTIC *)); 
AL_FUNC(bool,             al_is_haptic_effect_ok     , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *));

AL_FUNC(bool,             al_upload_haptic_effect    , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *, int * play_id));
AL_FUNC(bool,             al_play_haptic_effect      , (ALLEGRO_HAPTIC *, int play_id, int loop));
AL_FUNC(bool,             al_stop_haptic_effect      , (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_is_haptic_effect_stopped, (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_stop_all_haptic_effects , (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_is_haptic_effect_playing, (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_rumble_haptic           , (ALLEGRO_HAPTIC *, double intensity, double duration, int * play_id));

AL_FUNC(ALLEGRO_EVENT_SOURCE *, al_get_haptic_event_source, (void));




#ifdef __cplusplus
}
#endif

#endif

