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
 *      New haptic API.
 * 
 *      By Peter Wang.
 *
 *      See readme.txt for copyright information.
 */

/* Title: Joystick routines
 */


#define ALLEGRO_NO_COMPATIBILITY

#include "allegro5/allegro.h"
#include "allegro5/haptic.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_exitfunc.h"
#include "allegro5/internal/aintern_haptic.h"
#include "allegro5/internal/aintern_system.h"



/* the active haptic driver */
static ALLEGRO_HAPTIC_DRIVER *haptic_driver = NULL;
static ALLEGRO_EVENT_SOURCE   haptic_es;


/* Function: al_install_haptic
 */
bool al_install_haptic(void)
{
   ALLEGRO_SYSTEM *sysdrv;
   ALLEGRO_HAPTIC_DRIVER *hapdrv;

   if (haptic_driver)
      return true;

   sysdrv = al_get_system_driver();
   ASSERT(sysdrv);

   /* Currently every platform only has at most one haptic driver. */
   if (sysdrv->vt->get_haptic_driver) {
      hapdrv = sysdrv->vt->get_haptic_driver();
      /* Avoid race condition in case the haptic driver generates an
       * event right after ->init_haptic.
       */
      _al_event_source_init(&haptic_es);
      if (hapdrv && hapdrv->init_haptic()) {
         haptic_driver = hapdrv;
         _al_add_exit_func(al_uninstall_haptic, "al_uninstall_haptic");
         return true;
      }
      _al_event_source_free(&haptic_es);
   }

   return false;
}



/* Function: al_uninstall_haptic
 */
void al_uninstall_haptic(void)
{
   if (haptic_driver) {
      /* perform driver clean up */
      haptic_driver->exit_haptic();
      _al_event_source_free(&haptic_es);
      haptic_driver = NULL;
   }
}


/* Function: al_is_haptic_installed
 */
bool al_is_haptic_installed(void)
{
   return (haptic_driver) ? true : false;
}


/* Function: al_reconfigure_haptics
 */
bool al_reconfigure_haptics(void)
{
   if (!haptic_driver)
      return false;
   
   return haptic_driver->reconfigure_haptics();
}



/* Function: al_get_haptic_event_source
 */
ALLEGRO_EVENT_SOURCE *al_get_haptic_event_source(void)
{
   if (!haptic_driver)
      return NULL;
   return &haptic_es;
}



void _al_generate_haptic_event(ALLEGRO_EVENT *event)
{
   ASSERT(haptic_driver);

   _al_event_source_lock(&haptic_es);
   if (_al_event_source_needs_to_generate_event(&haptic_es)) {
      _al_event_source_emit_event(&haptic_es, event);
   }
   _al_event_source_unlock(&haptic_es);
}



/* Function: al_get_num_haptics
 */
int al_get_num_haptics(void)
{
   if (haptic_driver)
      return haptic_driver->num_haptics();

   return 0;
}



/* Function: al_get_haptic
 */
ALLEGRO_HAPTIC * al_get_haptic(int num)
{
   ASSERT(haptic_driver);
   ASSERT(num >= 0);

   return haptic_driver->get_haptic(num);
}



/* Function: al_release_haptic
 */
void al_release_haptic(ALLEGRO_HAPTIC *hap)
{
   ASSERT(haptic_driver);
   ASSERT(hap);

   haptic_driver->release_haptic(hap);
}



/* Function: al_get_haptic_active
 */
bool al_get_haptic_active(ALLEGRO_HAPTIC *hap)
{
   ASSERT(hap);

   return hap->info.flags;
}


/* Function: al_get_haptic_num_axes
 */
int al_get_haptic_num_axes(ALLEGRO_HAPTIC *hap)
{
   ASSERT(hap);

   return hap->info.num_axes;
}


/* Function: al_get_haptic_flags
 */
int al_get_haptic_flags(ALLEGRO_HAPTIC *hap)
{
   ASSERT(hap);

   return haptic_driver->get_haptic_flags(hap);
}



/* Function: al_get_haptic_name
 */
const char *al_get_haptic_name(ALLEGRO_HAPTIC *hap)
{
   ASSERT(hap);
   return haptic_driver->get_name(hap);
   return NULL;
}

/*
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
AL_FUNC(bool,             al_is_haptic_effect_playing, (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_stop_all_haptic_effects , (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_is_haptic_stopped       , (ALLEGRO_HAPTIC *, int play_id));
AL_FUNC(bool,             al_rumble_haptic           , (ALLEGRO_HAPTIC *, double intensity, double duration, int * play_id));


*/




