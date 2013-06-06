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
 *      Linux haptic (force-feedback) device driver.
 *
 *      By Beoran.
 * 
 *      See readme.txt for copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/input.h>
#include <fcntl.h>      
#include <limits.h>     
#include <errno.h>      
#include <math.h>
#include <glob.h>


#define ALLEGRO_NO_KEY_DEFINES
#define ALLEGRO_NO_COMPATIBILITY

#include "allegro5/allegro.h"
#include "allegro5/haptic.h"
#include "allegro5/path.h"
#include "allegro5/platform/alplatf.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_haptic.h"
#include "allegro5/platform/aintunix.h"
#include </home/bjorn/src/allegro/lib/Headers/allegro5/haptic.h>


#if defined(ALLEGRO_HAVE_SYS_INOTIFY_H) && defined(ALLEGRO_HAVE_SYS_TIMERFD_H)
   #define SUPPORT_HOTPLUG
   #include <sys/inotify.h>
   #include <sys/timerfd.h>
#endif

ALLEGRO_DEBUG_CHANNEL("lhaptic");

/* Support at most 32 haptic devices. */
#define HAPTICS_MAX             32
/* Use dumb char buffers of 100 characters. whihc is "enough 
 *for everyone" for now. :p */
#define HAPTICS_BUF_MAX         1000


typedef struct ALLEGRO_HAPTIC_LINUX
{
   int in_use;
   ALLEGRO_HAPTIC parent;
   int config_state;
   bool marked;
   int fd;
   char device_name[HAPTICS_BUF_MAX];
   int state;
   char name[HAPTICS_BUF_MAX];
   int flags;
} ALLEGRO_HAPTIC_LINUX;



static bool lhap_init_haptic(void);
static void lhap_exit_haptic(void);

static bool lhap_reconfigure_haptics(void);
static int lhap_num_haptics(void);
static ALLEGRO_HAPTIC *lhap_get_haptic(int num);
static void lhap_release_haptic(ALLEGRO_HAPTIC *hap_);

static int lhap_get_flags(ALLEGRO_HAPTIC *hap_);
static const char *lhap_get_name(ALLEGRO_HAPTIC *hap_);


/* forward declarations 
static bool lhap_get_active(ALLEGRO_HAPTIC *hap_);
static void lhap_generate_event(ALLEGRO_HAPTIC_LINUX *joy, int button, ALLEGRO_EVENT_TYPE event_type);
*/

static bool lhap_get_active(ALLEGRO_HAPTIC *);
static bool lhap_is_mouse_haptic(ALLEGRO_MOUSE *);
static bool lhap_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static ALLEGRO_HAPTIC *  lhap_get_from_mouse(ALLEGRO_MOUSE *);
static ALLEGRO_HAPTIC *  lhap_get_from_joystick(ALLEGRO_JOYSTICK *);
static int               lhap_get_num_axes(ALLEGRO_HAPTIC *); 
static bool lhap_is_effect_ok(ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *);
static bool lhap_upload_effect(ALLEGRO_HAPTIC *, ALLEGRO_HAPTIC_EFFECT *, int *);
static bool lhap_play_effect(ALLEGRO_HAPTIC *, int, int);
static bool lhap_stop_effect(ALLEGRO_HAPTIC *, int);
static bool lhap_is_effect_stopped(ALLEGRO_HAPTIC *, int);
static bool lhap_stop_all_effects(ALLEGRO_HAPTIC *);
static bool lhap_is_effect_playing(ALLEGRO_HAPTIC *, int);

/* The haptics driver vtable. */
ALLEGRO_HAPTIC_DRIVER hapdrv_linux = 
{
   _ALLEGRO_HAPDRV_LINUX,
   "",
   "",
   "Linux haptic(s)",
   lhap_init_haptic,
   lhap_exit_haptic,
   lhap_reconfigure_haptics,
   lhap_num_haptics,
   lhap_get_haptic,
   lhap_release_haptic,
   lhap_get_flags,
   lhap_get_name,
   lhap_get_active,
   lhap_is_mouse_haptic,
   lhap_is_joystick_haptic,
   lhap_get_from_mouse,
   lhap_get_from_joystick,
   lhap_get_num_axes,
   lhap_is_effect_ok,
   lhap_upload_effect,
   lhap_play_effect,
   lhap_stop_effect,
   lhap_is_effect_stopped,
   lhap_stop_all_effects,
   lhap_is_effect_playing
};


ALLEGRO_HAPTIC_DRIVER * _al_haptic_driver = &hapdrv_linux;



ALLEGRO_HAPTIC_DRIVER *_al_linux_haptic_driver(void)
{
   return &hapdrv_linux;
}



static unsigned                 num_haptics = 0;  
/* number of haptics known to the user */
static ALLEGRO_HAPTIC_LINUX     haptics[HAPTICS_MAX];
static ALLEGRO_MUTEX          * haptic_mutex;
#ifdef SUPPORT_HOTPLUG

static int                      haptic_inotify_fd = -1;
static int                      haptic_timer_fd = -1;

#endif

/* Approach: to find the available haptic devices, scan 
 * the /sys/class/input directory for any directory named eventxxx. 
 * Then read  /sys/class/input/event1/capabilities/ff to find out the 
 * capabilities of the device. If it's 0, then no it doesn't support force feedback.
 * The idea of this approach is that the /dev file doesn' t have to be opened
 * to inspect the existence of the haptic device. 
 */

/*
 * Scans the /sys/class/input/ directory to find any haptic devices and 
 * already sets them up to be used if needed. Returns the amount of haptics found
 * or negative or error.
 */
int lhap_scan_haptics(void) {
  char buf[HAPTICS_BUF_MAX];
  char line[HAPTICS_BUF_MAX];
  glob_t found;
  int res;
  unsigned int index;
  num_haptics = 0;
  for (index = 0; index < HAPTICS_MAX; index ++) {
    haptics[index].in_use = 0;
    haptics[index].fd     = -1;
  }  
  res         = glob("/sys/class/input/event*", GLOB_MARK, NULL, &found);
  if (res == GLOB_NOMATCH) { 
    globfree(&found);
    return -ENOENT;
  }
  for (index = 0; index < found.gl_pathc; index ++) {
    int scanres;
    unsigned int capa = 0, extra = 0;
    FILE * fin;
    char * path = found.gl_pathv[index]; 
    memset(buf, 0, HAPTICS_BUF_MAX);
    snprintf(buf, HAPTICS_BUF_MAX, "%sdevice/capabilities/ff", path);
    fin = fopen(buf, "r");
    if(!fin) {  continue;  }
    scanres = fscanf(fin, "%u%u", &capa, &extra);
    fclose(fin);
    
    if (capa > 0) { 
      char * devname = strchr(strchr(path + 1, '/') + 1, '/');
      /* It's a haptic device. */
      ALLEGRO_HAPTIC_LINUX * hap = haptics + num_haptics;
      hap->parent.info.id        = num_haptics;
      num_haptics++;
      hap->flags     = capa;
      snprintf(hap->device_name, HAPTICS_BUF_MAX, "/dev%s", devname);
      hap->device_name[strlen(hap->device_name) -1] = '\0';
      snprintf(buf, HAPTICS_BUF_MAX, "%sdevice/name", path);
      fin = fopen(buf, "r");
      if (!fin) {  continue;  }
      if(fgets(line, HAPTICS_BUF_MAX, fin)) {
        line[HAPTICS_BUF_MAX-1] = '\0';
        line[strlen(line)]      = '\0';
        strcpy(hap->name, line);      
      }
      fclose(fin);      
      fprintf(stderr, "Haptic device found: %s (%s) (%s), %d %d %d\n", buf, hap->device_name, hap->name, scanres, capa, extra);
      
    }
    
  }
  globfree(&found);  
  return num_haptics;
} 


bool lhap_init_haptic(void) {
  return lhap_scan_haptics() >= 0;
}


void lhap_exit_haptic() {
   return;
}

static bool lhap_reconfigure_haptics(void){
  return 0;
}

int lhap_num_haptics() {
  return num_haptics;
}

ALLEGRO_HAPTIC_LINUX * lhap_al2lin(ALLEGRO_HAPTIC * haptic) {
  if(!haptic) return NULL;
  ALLEGRO_HAPTIC_LINUX  * lhap = haptics + haptic->info.id;
  /* Could also have used offsetof, but, hey... */
  return lhap;
}


void lhap_release_haptic(ALLEGRO_HAPTIC * haptic) {
  ALLEGRO_HAPTIC_LINUX  * lhap = lhap_al2lin(haptic);
  ASSERT(haptic);
  if (!lhap->in_use) return;
  if (lhap->fd < 0) return;
  close(lhap->fd);
}


ALLEGRO_HAPTIC * lhap_get_haptic(int index) {
  ALLEGRO_HAPTIC_LINUX * lhap;
  if(index >= HAPTICS_MAX) return NULL;
  lhap = haptics + index;
  if (!lhap->in_use) {
    lhap->fd     = open(lhap->device_name, O_RDWR);
    if(lhap->fd < 0 ) return NULL;
    lhap->in_use = 1;
    return &lhap->parent;
  } else {
    return &lhap->parent;
  }
  return NULL;
}


const char * lhap_get_name(ALLEGRO_HAPTIC * haptic) {
  ALLEGRO_HAPTIC_LINUX * lhap = lhap_al2lin(haptic);
  return lhap->name;
}

int lhap_get_flags(ALLEGRO_HAPTIC * haptic) {
  ALLEGRO_HAPTIC_LINUX * lhap = lhap_al2lin(haptic);
  return lhap->flags;
}


static bool lhap_type2lin(__u16 * res, int type) {
  ASSERT(res);
  
  switch (type) {
    case ALLEGRO_HAPTIC_RUMBLE          : (*res) = FF_RUMBLE       ; break;
    case ALLEGRO_HAPTIC_PERIODIC        : (*res) = FF_PERIODIC     ; break;
    case ALLEGRO_HAPTIC_CONSTANT        : (*res) = FF_CONSTANT     ; break;
    case ALLEGRO_HAPTIC_SPRING          : (*res) = FF_SPRING       ; break;
    case ALLEGRO_HAPTIC_FRICTION        : (*res) = FF_FRICTION     ; break;
    case ALLEGRO_HAPTIC_DAMPER          : (*res) = FF_DAMPER       ; break;
    case ALLEGRO_HAPTIC_INERTIA         : (*res) = FF_INERTIA      ; break;
    case ALLEGRO_HAPTIC_RAMP            : (*res) = FF_RAMP         ; break;
    default: 
      return false;
  }
  return true;
}

static bool lhap_wave2lin(__u16 * res, int type) {
  ASSERT(res);
  
  switch (type) {
    case ALLEGRO_HAPTIC_SQUARE          : (*res) = FF_SQUARE       ; break;
    case ALLEGRO_HAPTIC_TRIANGLE        : (*res) = FF_TRIANGLE     ; break;
    case ALLEGRO_HAPTIC_SINE            : (*res) = FF_SINE         ; break;
    case ALLEGRO_HAPTIC_SAW_UP          : (*res) = FF_SAW_UP       ; break;
    case ALLEGRO_HAPTIC_SAW_DOWN        : (*res) = FF_SAW_DOWN     ; break;
    case ALLEGRO_HAPTIC_CUSTOM          : (*res) = FF_CUSTOM       ; break;    
    default: 
      return false;
  }
  return true;
}

/* converts the time in seconds to a linux compatible time. Return false if
 out of bounds. */
static bool lhap_time2lin(__u16 * res, double sec) {
  ASSERT(res); 
  
  if (sec < 0.0)        return false; 
  if (sec > 32.767)     return false;
  return (__u16) round(sec * 1000.0);
}

/* Converts replay data to linux. */
static bool lhap_replay2lin(struct ff_replay * lin, ALLEGRO_HAPTIC_REPLAY * al) {
  if(!lhap_time2lin(&lin.delay, al.delay))  return false;
  if(!lhap_time2lin(&lin.length, al.length)) return false;
  return true;
}

/* Converts the level in range 0.0 to 1.0 to a linux compatible level. 
 * Returns false if out of bounds. */
static bool lhap_level2lin(__u16 * res, double level) {
  ASSERT(res);   
  if (level < 0.0)        return false; 
  if (level > 1.0)        return false;
  return (__u16) round(level * ((double) 0x7fff));
}

/* Converts a rumble effect to linux. */
static bool lhap_rumble2lin(struct ff_rumble_effect * lin, ALLEGRO_HAPTIC_RUMBLE_EFFECT * al) {
  if(!lhap_level2lin(&lin->strong_magnitude, al->strong_magnitude)) return false;
  if(!lhap_level2lin(&lin->weak_magnitude  , al->weak_magnitude)) return false;
  return true;
}

/* converts a periodic effect  to linux */
static bool lhap_periodic2lin(struct ff_periodic_effect * lin, ALLEGRO_HAPTIC_PERIODIC_EFFECT * al) {
  
  return true;
}

/* Converts allegro haptic effect to linux haptic effect. */
static bool lhap_effect2lin(struct ff_effect * lin, ALLEGRO_HAPTIC_EFFECT * al) {
  if(!lhap_type2lin(&lin->type, al->type,) return false;
  /* lin_effect->replay = effect->re; */
  lin->direction = (__u16) round(((double)0xC000 * al->direction.angle) / (2 * M_PI));
  lin->id        = -1;
  if(!lhap_replay2lin(&lin->replay, &al->replay);
  switch(lin->type) {
    case FF_RUMBLE:
      if(!lhap_rumble2lin(&lin->rumble, &al->rumble);
      break;
    case FF_PERIODIC:      
      if(!lhap_periodic2lin(&lin->periodic, al->data.periodic)) return false;
      break;
      
  }
     
  return false;
}



static bool lhap_get_active(ALLEGRO_HAPTIC * haptic) {
  ALLEGRO_HAPTIC_LINUX * lhap = lhap_al2lin(haptic);
  return lhap->in_use;
}

static bool lhap_is_mouse_haptic(ALLEGRO_MOUSE * mouse) {
  return false;
}

static bool lhap_is_joystick_haptic(ALLEGRO_JOYSTICK * joy) {
   return false;
}

static ALLEGRO_HAPTIC *  lhap_get_from_mouse(ALLEGRO_MOUSE * mouse) {
  return NULL;
}

static ALLEGRO_HAPTIC *  lhap_get_from_joystick(ALLEGRO_JOYSTICK * joy) {
  return NULL;
}

static int lhap_get_num_axes(ALLEGRO_HAPTIC * haptic) {
  return 1;
}

static bool lhap_is_effect_ok(ALLEGRO_HAPTIC * haptic, ALLEGRO_HAPTIC_EFFECT * effect) {
   return false;
}

static bool lhap_upload_effect(ALLEGRO_HAPTIC * haptic, ALLEGRO_HAPTIC_EFFECT * effect, int * playid) {
  return false;
}

static bool lhap_play_effect(ALLEGRO_HAPTIC * haptic, int repeats, int playid) {
  return false;  
}

static bool lhap_stop_effect(ALLEGRO_HAPTIC * haptic, int playid) {
  return false;
}

static bool lhap_is_effect_stopped(ALLEGRO_HAPTIC * haptic, int playid) {
  return true;
}

static bool lhap_stop_all_effects(ALLEGRO_HAPTIC * haptic) {
  return false;
}

static bool lhap_is_effect_playing(ALLEGRO_HAPTIC * haptic, int playid) {
  return false;
}



