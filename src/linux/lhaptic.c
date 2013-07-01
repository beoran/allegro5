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
#include <stddef.h>
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
#include "allegro5/joystick.h"
#include "allegro5/haptic.h"
#include "allegro5/path.h"
#include "allegro5/platform/alplatf.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_haptic.h"
#include "allegro5/internal/aintern_ljoynu.h"
#include "allegro5/platform/aintunix.h"




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

/* Support at most 16 effects per device. */
#define HAPTICS_EFFECTS_MAX     16

/* Tests if a bit in a byte array is set. */
#define test_bit(bit, array)  (array [bit / (sizeof(long) * 8)] & (1 << (bit % (8 * sizeof(long)) )))


typedef struct ALLEGRO_HAPTIC_LINUX
{
   struct ALLEGRO_HAPTIC parent;
   int in_use;
   int config_state;
   bool marked;
   int fd;
   char device_name[HAPTICS_BUF_MAX];
   int state;
   char name[HAPTICS_BUF_MAX];
   int flags;
   int effects[HAPTICS_EFFECTS_MAX];
} ALLEGRO_HAPTIC_LINUX;



static bool lhap_init_haptic(void);
static void lhap_exit_haptic(void);




static bool lhap_is_mouse_haptic(ALLEGRO_MOUSE * dev);
static bool lhap_is_joystick_haptic(ALLEGRO_JOYSTICK *);
static bool lhap_is_keyboard_haptic(ALLEGRO_KEYBOARD * dev);
static bool lhap_is_display_haptic(ALLEGRO_DISPLAY * dev);
static bool lhap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT * dev);

static ALLEGRO_HAPTIC *lhap_get_from_mouse(ALLEGRO_MOUSE * dev);
static ALLEGRO_HAPTIC *lhap_get_from_joystick(ALLEGRO_JOYSTICK * dev);
static ALLEGRO_HAPTIC *lhap_get_from_keyboard(ALLEGRO_KEYBOARD * dev);
static ALLEGRO_HAPTIC *lhap_get_from_display(ALLEGRO_DISPLAY * dev);
static ALLEGRO_HAPTIC *lhap_get_from_touch_input(ALLEGRO_TOUCH_INPUT * dev);

static bool lhap_release(ALLEGRO_HAPTIC * haptic);

static bool lhap_get_active(ALLEGRO_HAPTIC * hap);
static int lhap_get_capabilities(ALLEGRO_HAPTIC * dev);
static double lhap_get_gain(ALLEGRO_HAPTIC * dev);
static bool lhap_set_gain(ALLEGRO_HAPTIC * dev, double);
static int lhap_get_num_effects(ALLEGRO_HAPTIC * dev);

static bool lhap_is_effect_ok(ALLEGRO_HAPTIC * dev,
                              ALLEGRO_HAPTIC_EFFECT * eff);
static bool lhap_upload_effect(ALLEGRO_HAPTIC * dev,
                               ALLEGRO_HAPTIC_EFFECT * eff,
                               ALLEGRO_HAPTIC_EFFECT_ID * id);
static bool lhap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID * id, int loop);
static bool lhap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID * id);
static bool lhap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID * id);
static bool lhap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID * id);

/* The haptics driver vtable. */
ALLEGRO_HAPTIC_DRIVER hapdrv_linux = {
   _ALLEGRO_HAPDRV_LINUX,
   "",
   "",
   "Linux haptic(s)",
   lhap_init_haptic,
   lhap_exit_haptic,

   lhap_is_mouse_haptic,
   lhap_is_joystick_haptic,
   lhap_is_keyboard_haptic,
   lhap_is_display_haptic,
   lhap_is_touch_input_haptic,

   lhap_get_from_mouse,
   lhap_get_from_joystick,
   lhap_get_from_keyboard,
   lhap_get_from_display,
   lhap_get_from_touch_input,

   lhap_get_active,
   lhap_get_capabilities,
   lhap_get_gain,
   lhap_set_gain,
   lhap_get_num_effects,

   lhap_is_effect_ok,
   lhap_upload_effect,
   lhap_play_effect,
   lhap_stop_effect,
   lhap_is_effect_playing,
   lhap_release_effect,

   lhap_release
};

ALLEGRO_HAPTIC_DRIVER *_al_haptic_driver = &hapdrv_linux;

ALLEGRO_HAPTIC_DRIVER *_al_linux_haptic_driver(void)
{
   return &hapdrv_linux;
}



// static unsigned                 num_haptics = 0;  
/* number of haptics known to the user */
static ALLEGRO_HAPTIC_LINUX haptics[HAPTICS_MAX];
static ALLEGRO_MUTEX *haptic_mutex = NULL;


static bool lhap_init_haptic(void)
{
   int index;
   haptic_mutex = al_create_mutex();
   if (!haptic_mutex)
      return false;
   for (index = 0; index < HAPTICS_MAX; index++) {
      haptics[index].in_use = false;
   }

   return true;
}

static ALLEGRO_HAPTIC_LINUX *lhap_get_available_haptic()
{
   int index;
   haptic_mutex = al_create_mutex();
   if (!haptic_mutex)
      return false;
   for (index = 0; index < HAPTICS_MAX; index++) {
      if (!haptics[index].in_use) {
         haptics[index].in_use = true;
         return haptics + index;
      }
   }
   return NULL;
}

/* Converts a generic haptic device to a linux specific one. */
static ALLEGRO_HAPTIC_LINUX *lhap_from_al(ALLEGRO_HAPTIC * hap)
{
   void *ptr = hap;
   if (!ptr)
      return NULL;
   return (ALLEGRO_HAPTIC_LINUX *) (ptr -
                                    offsetof(ALLEGRO_HAPTIC_LINUX, parent));
}

static void lhap_exit_haptic()
{
   al_destroy_mutex(haptic_mutex);
   return;
}


static bool lhap_type2lin(__u16 * res, int type)
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

static bool lhap_wave2lin(__u16 * res, int type)
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

/* converts the time in seconds to a linux compatible time. Return false if
 out of bounds. */
static bool lhap_time2lin(__u16 * res, double sec)
{
   ASSERT(res);

   if (sec < 0.0)
      return false;
   if (sec > 32.767)
      return false;
   (*res) = (__u16) round(sec * 1000.0);
   return true;
}

/* converts the time in seconds to a linux compatible time. 
 * Return false if out of bounds. This one allows negative times. */
static bool lhap_stime2lin(__s16 * res, double sec)
{
   ASSERT(res);

   if (sec < -32.767)
      return false;
   if (sec > 32.767)
      return false;
   (*res) = (__s16) round(sec * 1000.0);
   return true;
}


/* Converts replay data to linux. */
static bool lhap_replay2lin(struct ff_replay *lin,
                            struct ALLEGRO_HAPTIC_REPLAY *al)
{
   if (!lhap_time2lin(&lin->delay, al->delay))
      return false;
   if (!lhap_time2lin(&lin->length, al->length))
      return false;
   return true;
}

/* Converts the level in range 0.0 to 1.0 to a linux compatible level. 
 * Returns false if out of bounds. */
static bool lhap_level2lin(__u16 * res, double level)
{
   ASSERT(res);
   if (level < 0.0)
      return false;
   if (level > 1.0)
      return false;
   (*res) = (__u16) round(level * ((double)0x7fff));
   return true;
}


/* Converts the level in range -1.0 to 1.0 to a linux compatible level. 
 * Returns false if out of bounds. */
static bool lhap_slevel2lin(__s16 * res, double level)
{
   ASSERT(res);
   if (level < -1.0)
      return false;
   if (level > 1.0)
      return false;
   (*res) = (__s16) round(level * ((double)0x7ffe));
   return true;
}


/* Converts an allegro haptic effect envelope to the linux structure. */
static bool lhap_envelope2lin(struct ff_envelope *lin,
                              struct ALLEGRO_HAPTIC_ENVELOPE *al)
{
   if (!lhap_time2lin(&lin->attack_length, al->attack_length))
      return false;
   if (!lhap_time2lin(&lin->fade_length, al->fade_length))
      return false;
   if (!lhap_level2lin(&lin->attack_level, al->attack_level))
      return false;
   if (!lhap_level2lin(&lin->fade_level, al->fade_level))
      return false;
   return true;
}

/* Converts a rumble effect to linux. */
static bool lhap_rumble2lin(struct ff_rumble_effect *lin,
                            struct ALLEGRO_HAPTIC_RUMBLE_EFFECT *al)
{
   if (!lhap_level2lin(&lin->strong_magnitude, al->strong_magnitude))
      return false;
   if (!lhap_level2lin(&lin->weak_magnitude, al->weak_magnitude))
      return false;
   return true;
}


/* Converts a constant effect to linux. */
static bool lhap_constant2lin(struct ff_constant_effect *lin,
                              struct ALLEGRO_HAPTIC_CONSTANT_EFFECT *al)
{
   if (!lhap_envelope2lin(&lin->envelope, &al->envelope))
      return false;
   if (!lhap_slevel2lin(&lin->level, al->level))
      return false;
   return true;
}

/* Converts a ramp effect to linux. */
static bool lhap_ramp2lin(struct ff_ramp_effect *lin,
                          struct ALLEGRO_HAPTIC_RAMP_EFFECT *al)
{
   if (!lhap_envelope2lin(&lin->envelope, &al->envelope))
      return false;
   if (!lhap_slevel2lin(&lin->start_level, al->start_level))
      return false;
   if (!lhap_slevel2lin(&lin->end_level, al->end_level))
      return false;
   return true;
}

/* Converts a ramp effect to linux. */
static bool lhap_condition2lin(struct ff_condition_effect *lin,
                               struct ALLEGRO_HAPTIC_CONDITION_EFFECT *al)
{
   if (!lhap_slevel2lin(&lin->center, al->center))
      return false;
   if (!lhap_level2lin(&lin->deadband, al->deadband))
      return false;
   if (!lhap_slevel2lin(&lin->right_coeff, al->right_coeff))
      return false;
   if (!lhap_level2lin(&lin->right_saturation, al->right_saturation))
      return false;
   if (!lhap_slevel2lin(&lin->left_coeff, al->left_coeff))
      return false;
   if (!lhap_level2lin(&lin->left_saturation, al->left_saturation))
      return false;
   return true;
}


/* converts a periodic effect  to linux */
static bool lhap_periodic2lin(struct ff_periodic_effect *lin,
                              struct ALLEGRO_HAPTIC_PERIODIC_EFFECT *al)
{
   if (!lhap_slevel2lin(&lin->magnitude, al->magnitude))
      return false;
   if (!lhap_stime2lin(&lin->offset, al->offset))
      return false;
   if (!lhap_time2lin(&lin->period, al->period))
      return false;
   if (!lhap_time2lin(&lin->phase, al->phase))
      return false;
   if (!lhap_wave2lin(&lin->waveform, al->waveform))
      return false;
   if (al->custom_data) {
      /* Custom data is not supported yet, because currently no Linux 
       * haptic driver supports it. 
       */
      return false;
   }
   if (!lhap_envelope2lin(&lin->envelope, &al->envelope))
      return false;
   return true;
}

/* Converts allegro haptic effect to linux haptic effect. */
static bool lhap_effect2lin(struct ff_effect *lin, ALLEGRO_HAPTIC_EFFECT * al)
{
   if (!lhap_type2lin(&lin->type, al->type))
      return false;
   /* lin_effect->replay = effect->re; */
   lin->direction =
       (__u16) round(((double)0xC000 * al->direction.angle) / (2 * M_PI));
   lin->id = -1;
   if (!lhap_replay2lin(&lin->replay, &al->replay))
      return false;
   switch (lin->type) {
      case FF_RUMBLE:
         if (!lhap_rumble2lin(&lin->u.rumble, &al->data.rumble))
            return false;
         break;
      case FF_PERIODIC:
         if (!lhap_periodic2lin(&lin->u.periodic, &al->data.periodic))
            return false;
         break;
      case FF_CONSTANT:
         if (!lhap_constant2lin(&lin->u.constant, &al->data.constant))
            return false;
         break;

      case FF_RAMP:
         if (!lhap_ramp2lin(&lin->u.ramp, &al->data.ramp))
            return false;
         break;

      case FF_SPRING:
      case FF_FRICTION:
      case FF_DAMPER:
      case FF_INERTIA:
         if (!lhap_condition2lin(&lin->u.condition[0], &al->data.condition))
            return false;
         break;
      default:
         return false;
   }

   return true;
}



static bool lhap_get_active(ALLEGRO_HAPTIC * haptic)
{
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(haptic);
   return lhap->in_use;
}

static bool lhap_is_mouse_haptic(ALLEGRO_MOUSE * mouse)
{
   (void)mouse;
   return false;
}

#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)
/* Tests if a bit in an array of longs is set. */
#define TEST_BIT(nr, addr) \
    (((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)

bool lhap_fd_can_ff(int fd)
{
   long bitmask[NLONGS(EV_CNT)] = { 0 };

   if (ioctl(fd, EVIOCGBIT(0, sizeof(bitmask)), bitmask) < 0) {
      return false;
   }
   if (TEST_BIT(EV_FF, bitmask)) {
      return true;
   }
   return false;
}




static bool lhap_is_joystick_haptic(ALLEGRO_JOYSTICK * joy)
{
   // int newfd = -1;
   ALLEGRO_JOYSTICK_LINUX *ljoy = (ALLEGRO_JOYSTICK_LINUX *) joy;
   if (!al_is_joystick_installed())
      return false;
   if (!al_get_joystick_active(joy))
      return false;
   if (ljoy->fd <= 0)
      return false;
   // al_cstr(ljoy->device_name)
   // newfd = open("/dev/input/event8", O_RDWR);  
   // close(newfd);
   return lhap_fd_can_ff(ljoy->fd);
}

static bool lhap_is_display_haptic(ALLEGRO_DISPLAY * dev)
{
   (void)dev;
   return false;
}

static bool lhap_is_keyboard_haptic(ALLEGRO_KEYBOARD * dev)
{
   (void)dev;
   return false;
}

static bool lhap_is_touch_input_haptic(ALLEGRO_TOUCH_INPUT * dev)
{
   (void)dev;
   return false;
}


static ALLEGRO_HAPTIC *lhap_get_from_mouse(ALLEGRO_MOUSE * mouse)
{
   (void)mouse;
   return NULL;
}


#define TEST_CAPA(BIT, MASK, CAP, ALCAPA) do { \
  if (TEST_BIT(FF_PERIODIC, bitmask)) { cap |= ALCAPA; } \
} while (0)


static bool get_haptic_capabilities(int fd, int *capabilities)
{

   int cap = 0;
   // unsigned long device_bits[(EV_MAX + 8) / sizeof(unsigned long)];  
   unsigned long bitmask[NLONGS(FF_CNT)] = { 0 };
   if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(bitmask)), bitmask) < 0) {
      perror("EVIOCGBIT ioctl failed");
      fprintf(stderr, "For fd %d\n", fd);
      return false;
   }
   TEST_CAPA(FF_PERIODIC, bitmask, cap, ALLEGRO_HAPTIC_PERIODIC);
   TEST_CAPA(FF_RUMBLE, bitmask, cap, ALLEGRO_HAPTIC_RUMBLE);
   TEST_CAPA(FF_CONSTANT, bitmask, cap, ALLEGRO_HAPTIC_CONSTANT);
   TEST_CAPA(FF_SPRING, bitmask, cap, ALLEGRO_HAPTIC_SPRING);
   TEST_CAPA(FF_FRICTION, bitmask, cap, ALLEGRO_HAPTIC_FRICTION);
   TEST_CAPA(FF_DAMPER, bitmask, cap, ALLEGRO_HAPTIC_DAMPER);
   TEST_CAPA(FF_INERTIA, bitmask, cap, ALLEGRO_HAPTIC_INERTIA);
   TEST_CAPA(FF_RAMP, bitmask, cap, ALLEGRO_HAPTIC_RAMP);
   TEST_CAPA(FF_SQUARE, bitmask, cap, ALLEGRO_HAPTIC_SQUARE);
   TEST_CAPA(FF_TRIANGLE, bitmask, cap, ALLEGRO_HAPTIC_TRIANGLE);
   TEST_CAPA(FF_SINE, bitmask, cap, ALLEGRO_HAPTIC_SINE);
   TEST_CAPA(FF_SAW_UP, bitmask, cap, ALLEGRO_HAPTIC_SAW_UP);
   TEST_CAPA(FF_SAW_DOWN, bitmask, cap, ALLEGRO_HAPTIC_SAW_DOWN);
   TEST_CAPA(FF_CUSTOM, bitmask, cap, ALLEGRO_HAPTIC_CUSTOM);
   TEST_CAPA(FF_GAIN, bitmask, cap, ALLEGRO_HAPTIC_GAIN);

   (*capabilities) = cap;

   return true;
}

static ALLEGRO_HAPTIC *lhap_get_from_joystick(ALLEGRO_JOYSTICK * joy)
{
   int index;
   ALLEGRO_HAPTIC_LINUX *lhap;
   ALLEGRO_JOYSTICK_LINUX *ljoy = (ALLEGRO_JOYSTICK_LINUX *) joy;

   if (!al_is_joystick_haptic(joy))
      return NULL;

   al_lock_mutex(haptic_mutex);

   lhap = lhap_get_available_haptic();
   if (!lhap)
      return NULL;

   lhap->parent.device = joy;
   lhap->parent.from = _AL_HAPTIC_FROM_JOYSTICK;


   lhap->fd = ljoy->fd;
   lhap->in_use = true;
   for (index = 0; index < HAPTICS_EFFECTS_MAX; index++) {
      lhap->effects[index] = -1;        // negative means not in use. 
   }
   lhap->parent.gain = 1.0;
   get_haptic_capabilities(lhap->fd, &lhap->flags);
   al_unlock_mutex(haptic_mutex);
   return &(lhap->parent);
}

static ALLEGRO_HAPTIC *lhap_get_from_display(ALLEGRO_DISPLAY * dev)
{
   (void)dev;
   return NULL;
}

static ALLEGRO_HAPTIC *lhap_get_from_keyboard(ALLEGRO_KEYBOARD * dev)
{
   (void)dev;
   return NULL;
}

static ALLEGRO_HAPTIC *lhap_get_from_touch_input(ALLEGRO_TOUCH_INPUT * dev)
{
   (void)dev;
   return NULL;
}


static int lhap_get_capabilities(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(dev);
   return lhap->flags;
}


static double lhap_get_gain(ALLEGRO_HAPTIC * dev)
{
   (void)dev;
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(dev);
   /* Unfortunately there seems to be no API to GET gain, only to set?! 
    * So, retururn the stored gain.
    */
   return lhap->parent.gain;
}

static bool lhap_set_gain(ALLEGRO_HAPTIC * dev, double gain)
{
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(dev);
   struct input_event ie;
   lhap->parent.gain = gain;
   timerclear(&ie.time);
   ie.type = EV_FF;
   ie.code = FF_GAIN;
   ie.value = (__s32) ((double)0xFFFF * gain);
   if (write(lhap->fd, &ie, sizeof(ie)) < 0) {
      return false;
   }
   return true;
}

int lhap_get_num_effects(ALLEGRO_HAPTIC * dev)
{
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(dev);
   int n_effects;               /* Number of effects the device can play at the same time */

   if (ioctl(lhap->fd, EVIOCGEFFECTS, &n_effects) < 0) {
      perror("Cannot check amount of effects");
      fprintf(stderr, "on FD %d\n", lhap->fd);
      return HAPTICS_EFFECTS_MAX;
   }
   if (n_effects > HAPTICS_EFFECTS_MAX)
      return HAPTICS_EFFECTS_MAX;
   return n_effects;
}


static bool lhap_is_effect_ok(ALLEGRO_HAPTIC * haptic,
                              ALLEGRO_HAPTIC_EFFECT * effect)
{
   struct ff_effect leff;
   int caps = al_get_haptic_capabilities(haptic);
   if (!((caps & effect->type) == effect->type))
      return false;
   if (!lhap_effect2lin(&leff, effect))
      return false;
   return true;
}

static double lhap_effect_duration(ALLEGRO_HAPTIC_EFFECT * effect)
{
   return effect->replay.delay + effect->replay.length;
}


static bool lhap_upload_effect(ALLEGRO_HAPTIC * dev,
                               ALLEGRO_HAPTIC_EFFECT * effect,
                               ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(dev);
   struct ff_effect leff;
   int index;
   int found = -1;

   ASSERT(dev);
   ASSERT(id);
   ASSERT(effect);

   /* set id's values to indicate failure. */
   id->_haptic = NULL;
   id->_effect = NULL;
   id->_id = -1;
   id->_handle = -1;


   if (!lhap_effect2lin(&leff, effect)) {
      return false;
   }

   leff.id = -1;

   /* Find empty spot for effect . */
   for (index = 0; index < al_get_num_haptic_effects(dev); index++) {
      if (lhap->effects[index] < 0) {
         found = index;
         break;
      }
   }

   /* No more space for an effect. */
   if (found < 0) {
      return false;
   }

   /* Upload effect. */
   if (ioctl(lhap->fd, EVIOCSFF, &leff) < 0) {
      return false;
   }

   id->_haptic = dev;
   id->_effect = effect;
   id->_id = found;
   id->_handle = leff.id;
   id->_playing = false;

   return true;
}

static bool lhap_play_effect(ALLEGRO_HAPTIC_EFFECT_ID * id, int loops)
{
   struct input_event play;
   ALLEGRO_HAPTIC_LINUX *lhap = (ALLEGRO_HAPTIC_LINUX *) id->_haptic;
   int fd;

   if (!lhap)
      return false;

   fd = lhap->fd;

   timerclear(&play.time);
   play.type = EV_FF;
   play.code = id->_handle;
   loops = (loops < 0) ? 1 : loops;
   play.value = loops;          /* play: 1, stop: 0 */

   if (write(fd, (const void *)&play, sizeof(play)) < 0) {
      perror("Effect play failed.");
      return false;
   }
   id->_playing = true;
   id->_started = al_get_time();
   id->_loops = loops;

   return true;
}


static bool lhap_stop_effect(ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   struct input_event play;
   ALLEGRO_HAPTIC_LINUX *lhap = (ALLEGRO_HAPTIC_LINUX *) id->_haptic;
   if (!lhap)
      return false;

   play.type = EV_FF;
   play.code = id->_handle;
   play.value = 0;

   if (write(lhap->fd, (const void *)&play, sizeof(play)) < 0) {
      return false;
   }
   id->_playing = false;
   return true;
}


static bool lhap_is_effect_playing(ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   double duration;
   ASSERT(id);

   if (!id->_playing)
      return false;
   /* Since AFAICS there is no Linux API to test this, use a timer to check if the 
      effect has been playing long enough to be finished or not. */
   duration = lhap_effect_duration(id->_effect) * id->_loops;
   if ((id->_started + duration) >= al_get_time())
      return true;
   return false;
}

static bool lhap_release_effect(ALLEGRO_HAPTIC_EFFECT_ID * id)
{
   ALLEGRO_HAPTIC_LINUX *lhap = (ALLEGRO_HAPTIC_LINUX *) id->_haptic;
   lhap_stop_effect(id);

   if (ioctl(lhap->fd, EVIOCRMFF, id->_handle) < 0) {
      return false;
   }
   lhap->effects[id->_id] = -1; // negative means not in use.
   return true;
}


static bool lhap_release(ALLEGRO_HAPTIC * haptic)
{
   ALLEGRO_HAPTIC_LINUX *lhap = lhap_from_al(haptic);

   ASSERT(haptic);

   if (!lhap->in_use)
      return false;

   lhap->in_use = false;
   lhap->fd = -1;
   return true;
}
