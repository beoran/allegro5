#ifndef __al_included_allegro5_aintern_haptic_h
#define __al_included_allegro5_aintern_haptic_h

#include "allegro5/haptic.h"

#include "allegro5/internal/aintern_driver.h"
#include "allegro5/internal/aintern_events.h"

#ifdef __cplusplus
   extern "C" {
#endif

/* Haptic devices driver virtual table.  */
typedef struct ALLEGRO_HAPTIC_DRIVER
{
   int          hapdrv_id;
   const char * hapdrv_name;
   const char * hapdrv_desc;
   const char * hapdrv_ascii_name;
   AL_METHOD(bool, init_haptic, (void));
   AL_METHOD(void, exit_haptic, (void));
   AL_METHOD(bool, reconfigure_haptics, (void));
   AL_METHOD(int, num_haptics, (void));
   AL_METHOD(ALLEGRO_HAPTIC *, get_haptic, (int));
   AL_METHOD(void, release_haptic    , (ALLEGRO_HAPTIC *));
   AL_METHOD(int, get_haptic_flags   , (ALLEGRO_HAPTIC *));
   AL_METHOD(const char *, get_name  , (ALLEGRO_HAPTIC *));
   AL_METHOD(bool, get_active        , (ALLEGRO_HAPTIC *));
   AL_METHOD(bool, is_mouse_haptic   , (ALLEGRO_MOUSE *));
   AL_METHOD(bool, is_joystick_haptic, (ALLEGRO_JOYSTICK *));
   AL_METHOD(ALLEGRO_HAPTIC *, get_from_mouse   , (ALLEGRO_MOUSE *));
   AL_METHOD(ALLEGRO_HAPTIC *, get_from_joystick, (ALLEGRO_JOYSTICK *));
   AL_METHOD(int , get_num_axes      , (ALLEGRO_HAPTIC *)); 
   AL_METHOD(bool, is_effect_ok      , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *));
   AL_METHOD(bool, upload_effect     , (ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *, int *));
   AL_METHOD(bool, play_effect       , (ALLEGRO_HAPTIC *, int, int ));
   AL_METHOD(bool, stop_effect       , (ALLEGRO_HAPTIC *, int));
   AL_METHOD(bool, is_effect_stopped , (ALLEGRO_HAPTIC *, int));
   AL_METHOD(bool, stop_all_efects   , (ALLEGRO_HAPTIC *, int));
   AL_METHOD(bool, is_effect_playing , (ALLEGRO_HAPTIC *, int));
} ALLEGRO_HAPTIC_DRIVER;


extern ALLEGRO_HAPTIC_DRIVER * _al_haptic_driver;



/* macros for constructing the driver list */
#define _AL_BEGIN_HAPTIC_DRIVER_LIST                         \
   _AL_DRIVER_INFO _al_haptic_driver_list[] =                \
   {

#define _AL_END_HAPTIC_DRIVER_LIST                           \
      {  0,                NULL,                false }      \
   };

#define _AL_HAPTIC_INFO_NAME_MAX            256
   
/* Can playback at most 32 haptic effects at the same time. */   
#define _AL_HAPTIC_EFFECT_PLAYBACK_MAX      32    

   
/* information about an entire haptic */
typedef struct _AL_HAPTIC_INFO
{
  int  id;
  int  flags; 
  int  num_axes;
  char name[_AL_HAPTIC_INFO_NAME_MAX];
} _AL_HAPTIC_INFO;



struct ALLEGRO_HAPTIC
{
   _AL_HAPTIC_INFO info;
};

void _al_generate_haptic_event(ALLEGRO_EVENT *event);

#ifdef __cplusplus
   }
#endif

#endif

/* vi ts=8 sts=3 sw=3 et */
