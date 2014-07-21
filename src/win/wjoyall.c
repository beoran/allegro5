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
 *      Windows wrap-all joystick driver.
 *      By Beoran.
 *      See readme.txt for copyright information.
 *    
 * This driver exists because on Windows, DirectInput and XInput are two 
 * different joystick input APIs which bith need to be supported simultaneously.
 *
 * Although DirectInput is deprecated, it is a far more capable API in terms
 * of joystick layout and force feedback effects. XInput is a much simpler
 * API but it only supports joysticks that have a layout similar to that of an
 * XBOX controller. XInput also has very limited force feedback effects,
 * it only supportd rubmble style vibrations.     
 *
 * Older joystics or input devices such as steering wheels that do not map
 * cleanly to an XBOX controller tend to have DirectInput drivers available,
 * while more recent joypads tend to have a XInput driver available. In theory
 * XInput devices also support DirectInput, and some devices even have a switch
 * that lets the user select the API. But XInput devices only partially support
 * DirectInput. In particular, XInput devices do not support force feedback when
 * using DirectInput to access them.
 *
 * For all these reasons, both directinput and xinput drivers should be
 * supported on Windows to allow greates comfort to end users.
 * The wjoyall.c and whapall.c drivers are wrapper drivers that combine
 * the Allegro XInput and DirectInput drivers into a single driver.
 * For this reason, the same joystick ormay show up twice
 * in the joystick list. However, XInput devices will appear before DirectInput
 * devices in the list, so when in doubt, use the joystick with the lowest ID. 
 * 
 */


#define ALLEGRO_NO_COMPATIBILITY

/* For waitable timers */
#define _WIN32_WINNT 0x400

#include "allegro5/allegro.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/platform/aintwin.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_joystick.h"
#include "allegro5/internal/aintern_bitmap.h"


#ifdef ALLEGRO_CFG_XINPUT

/* Don't compile this lot if xinput isn't supported. */

#ifndef ALLEGRO_WINDOWS
#error something is wrong with the makefile
#endif

#ifdef ALLEGRO_MINGW32
   #undef MAKEFOURCC
#endif

#include <stdio.h>

ALLEGRO_DEBUG_CHANNEL("wjoyall")

#include "allegro5/joystick.h"
#include "allegro5/internal/aintern_joystick.h"
#include "allegro5/internal/aintern_wjoyall.h"

/* 32 + 4 = 36 joysticks */
#define MAX_JOYSTICKS 36

/* forward declarations */
static bool joyall_init_joystick(void);
static void joyall_exit_joystick(void);
static bool joyall_reconfigure_joysticks(void);
static int joyall_get_num_joysticks(void);
static ALLEGRO_JOYSTICK *joyall_get_joystick(int num);
static void joyall_release_joystick(ALLEGRO_JOYSTICK *joy);
static void joyall_get_joystick_state(ALLEGRO_JOYSTICK *joy, ALLEGRO_JOYSTICK_STATE *ret_state);
static const char *joyall_get_name(ALLEGRO_JOYSTICK *joy);
static bool joyall_get_active(ALLEGRO_JOYSTICK *joy);


/* the driver vtable */
ALLEGRO_JOYSTICK_DRIVER _al_joydrv_windows_all =
{
   AL_JOY_TYPE_WINDOWS_ALL,
   "",
   "",
   "Windows Joystick",
   joyall_init_joystick,
   joyall_exit_joystick,
   joyall_reconfigure_joysticks,
   joyall_get_num_joysticks,
   joyall_get_joystick,
   joyall_release_joystick,
   joyall_get_joystick_state,
   joyall_get_name,
   joyall_get_active
};

/* the joystick structures */
static ALLEGRO_JOYSTICK_WINDOWS_ALL joyall_joysticks[MAX_JOYSTICKS];

/* Mutex to protect state access. XXX is this needed? */
static ALLEGRO_MUTEX  * joyall_mutex = NULL;

/* Amount of directinput and xinput joystics known. */
static int joyall_num_xinput, joyall_num_dinput;

/* Sets p all joysticks from the two wrapped apis. */
static void joyall_setup_joysticks(void) {
   int index;
   int stop;
   
   joyall_num_dinput = _al_joydrv_directx.num_joysticks();
   joyall_num_xinput = _al_joydrv_xinput.num_joysticks();

  
   for (index = 0; index < joyall_num_xinput; index ++) {
      ALLEGRO_JOYSTICK * joystick       = _al_joydrv_xinput.get_joystick(index);
      joyall_joysticks[index].active    = true;
      joyall_joysticks[index].handle    = joystick;
      joyall_joysticks[index].driver    = &_al_joydrv_xinput;
      joyall_joysticks[index].index     = index;
   }

   stop = joyall_num_dinput + joyall_num_xinput; 
   for (index = joyall_num_xinput; index < stop; index++) {
      ALLEGRO_JOYSTICK * joystick       = _al_joydrv_xinput.get_joystick(index);
      joyall_joysticks[index].active    = true;
      joyall_joysticks[index].handle    = joystick;
      joyall_joysticks[index].driver    = &_al_joydrv_xinput;
      joyall_joysticks[index].index     = index;
   }
}


/* Initialization API function. */
static bool joyall_init_joystick(void)
{
   bool ok_xi, ok_di;
   int index;
   /* Create the mutex and a condition vaiable. */
   joyall_mutex = al_create_mutex_recursive();
   if(!joyall_mutex)
      return false;

   al_lock_mutex(joyall_mutex);

   /* Fill in the joystick structs */
   for (index = 0; index < MAX_JOYSTICKS; index ++) {
     joyall_joysticks[index].active  = false;
     joyall_joysticks[index].driver  = NULL;
     joyall_joysticks[index].handle  = NULL;
   }
   ok_xi = _al_joydrv_xinput.init_joystick();
   ok_di = _al_joydrv_directx.init_joystick();
   joyall_setup_joysticks();
   al_unlock_mutex(joyall_mutex);
   return ok_xi || ok_di;
}


static void joyall_exit_joystick(void)
{
   int index;
   al_lock_mutex(joyall_mutex);
   /* Wipe the joystick structs */
   for (index = 0; index < MAX_JOYSTICKS; index ++) {
      joyall_joysticks[index].active = false;
   }
   _al_joydrv_xinput.exit_joystick();
   _al_joydrv_directx.exit_joystick();
   al_unlock_mutex(joyall_mutex);
   al_destroy_mutex(joyall_mutex);
}

static bool joyall_reconfigure_joysticks(void)
{
   al_lock_mutex(joyall_mutex);

   _al_joydrv_xinput.reconfigure_joysticks();
   _al_joydrv_directx.reconfigure_joysticks();
   joyall_setup_joysticks();
   al_unlock_mutex(joyall_mutex);
   return true;
}

static int joyall_get_num_joysticks(void)
{
   int num_xinput, num_dinput;
   num_dinput = _al_joydrv_directx.num_joysticks();
   num_xinput = _al_joydrv_xinput.num_joysticks();
   return num_xinput + num_dinput;
}

static ALLEGRO_JOYSTICK *joyall_get_joystick(int num)
{
   if (num < 0) return NULL;
   if (num >= joyall_get_num_joysticks()) return NULL;
   return joyall_joysticks[num].handle; 
}

/* Convert allegro joystick to locla driver joystick struct */
static ALLEGRO_JOYSTICK_WINDOWS_ALL * joyall_joy2winall(ALLEGRO_JOYSTICK * joy)
{
   return (ALLEGRO_JOYSTICK_WINDOWS_ALL *) joy;
}

static void joyall_release_joystick(ALLEGRO_JOYSTICK *joy)
{
   ALLEGRO_JOYSTICK_WINDOWS_ALL * wjoyall = joyall_joy2winall(joy);
   /* Forward to the driver's function. Here it's OK to use joy
    * and not wjoydev->handle since the get_joystick function returns a
    * pointer to the real underlying driver-specific joystick data.
    */
   wjoyall->driver->release_joystick(joy);
}

static void joyall_get_joystick_state(ALLEGRO_JOYSTICK *joy, ALLEGRO_JOYSTICK_STATE *ret_state)
{
   ALLEGRO_JOYSTICK_WINDOWS_ALL * wjoyall = joyall_joy2winall(joy);
   /* Forward to the driver's function */
   wjoyall->driver->get_joystick_state(joy, ret_state);
}

static const char *joyall_get_name(ALLEGRO_JOYSTICK *joy) {
   ALLEGRO_JOYSTICK_WINDOWS_ALL * wjoyall = joyall_joy2winall(joy);
   /* Forward to the driver's function */
   return wjoyall->driver->get_name(joy);
}

static bool joyall_get_active(ALLEGRO_JOYSTICK *joy) {
   ALLEGRO_JOYSTICK_WINDOWS_ALL * wjoyall = joyall_joy2winall(joy);
   if (!wjoyall->active) return false;
   /* Forward to the driver's function */
   return wjoyall->driver->get_active(joy);
}


#endif /* #ifdef ALLEGRO_CFG_XINPUT */
