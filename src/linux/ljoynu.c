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
 *      Linux joystick driver.
 *
 *      By George Foot and Peter Wang.
 *
 *      Updated for new joystick API by Peter Wang.
 *
 *      See readme.txt for copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#define ALLEGRO_NO_KEY_DEFINES
#define ALLEGRO_NO_COMPATIBILITY

#include "allegro5/allegro.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_events.h"
#include "allegro5/internal/aintern_joystick.h"
#include "allegro5/platform/aintunix.h"

#ifdef ALLEGRO_HAVE_LINUX_JOYSTICK_H

/* To be safe, include sys/types.h before linux/joystick.h to avoid conflicting
 * definitions of fd_set.
 */
#include <sys/types.h>
#include <linux/joystick.h>
#include <linux/input.h>

#if defined(ALLEGRO_HAVE_SYS_INOTIFY_H) && defined(ALLEGRO_HAVE_SYS_TIMERFD_H)
   #define SUPPORT_HOTPLUG
   #include <sys/inotify.h>
   #include <sys/timerfd.h>
#endif

ALLEGRO_DEBUG_CHANNEL("ljoy");

#include "allegro5/internal/aintern_ljoynu.h"


/* forward declarations */
static bool ljoy_init_joystick(void);
static void ljoy_exit_joystick(void);
static bool ljoy_reconfigure_joysticks(void);
static int ljoy_num_joysticks(void);
static ALLEGRO_JOYSTICK *ljoy_get_joystick(int num);
static void ljoy_release_joystick(ALLEGRO_JOYSTICK *joy_);
static void ljoy_get_joystick_state(ALLEGRO_JOYSTICK *joy_, ALLEGRO_JOYSTICK_STATE *ret_state);
static const char *ljoy_get_name(ALLEGRO_JOYSTICK *joy_);
static bool ljoy_get_active(ALLEGRO_JOYSTICK *joy_);

static void ljoy_process_new_data(void *data);
static void ljoy_generate_axis_event(ALLEGRO_JOYSTICK_LINUX *joy, int stick, int axis, float pos);
static void ljoy_generate_button_event(ALLEGRO_JOYSTICK_LINUX *joy, int button, ALLEGRO_EVENT_TYPE event_type);



/* the driver vtable */
ALLEGRO_JOYSTICK_DRIVER _al_joydrv_linux =
{
   _ALLEGRO_JOYDRV_LINUX,
   "",
   "",
   "Linux joystick(s)",
   ljoy_init_joystick,
   ljoy_exit_joystick,
   ljoy_reconfigure_joysticks,
   ljoy_num_joysticks,
   ljoy_get_joystick,
   ljoy_release_joystick,
   ljoy_get_joystick_state,
   ljoy_get_name,
   ljoy_get_active
};


static unsigned num_joysticks;   /* number of joysticks known to the user */
static _AL_VECTOR joysticks;     /* of ALLEGRO_JOYSTICK_LINUX pointers */
static volatile bool config_needs_merging;
static ALLEGRO_MUTEX *config_mutex;
#ifdef SUPPORT_HOTPLUG
static int inotify_fd = -1;
static int timer_fd = -1;
#endif



#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)
/* Tests if a bit in an array of longs is set. */
#define TEST_BIT(nr, addr) \
    (((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)

    
    
/* Gets the amount of joystick related keys/buttons. Only accept joystick-related 
 * buttons, from BTN_MISC to BTN_9 for miscellaneous input, BTN_JOYSTICK to 
 * BTN_DEAD, for joysticks, from BTN_GAMEPAD to BTN_THUMBR for game pads, 
 * BTN_WHEEL, BTN_GEAR_DOWN, and BTN_GEAR_UP for steering wheels, and 
 * BTN_TRIGGER_HAPPY_XXX buttons just in case some joysticks use these as well. 
 */
static bool get_num_buttons(int fd, int * num_buttons)
{
    unsigned long key_bits[NLONGS(KEY_CNT)]  = {0};
    int nbut = 0;
    
    int res, i;
 
    res = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
    if (res < 0) return false;
        
    for (i = BTN_MISC; i <= BTN_GEAR_UP; i++) {
      bool is_wheel, is_joystick, is_gamepad, is_misc, is_triggerhappy;
      
      if(TEST_BIT(i, key_bits)) {
        /*The device has this button. Determine kind of button by checking the range. */
        is_misc           = ((i >= BTN_MISC)     && (i <=  BTN_9));
        is_joystick       = ((i >= BTN_JOYSTICK) && (i <= BTN_DEAD));
        is_gamepad        = ((i >= BTN_GAMEPAD)  && (i <= BTN_THUMBR));
        is_wheel          = ((i >= BTN_WHEEL)    && (i <= BTN_GEAR_UP));
        is_triggerhappy   = ((i >= BTN_TRIGGER_HAPPY)  && (i <= BTN_TRIGGER_HAPPY40));
        if (!(is_misc || is_joystick || is_gamepad || is_wheel || is_triggerhappy) ) {
          /* Ignore any buttons that don't seem joystick related. */
          continue;
        }        
        nbut++;
      }
    }
    (*num_buttons) = nbut;
    return true;
}

/* Check the amount of joystick-related absolute axes. 
 * Note that some devices, like (wireless) keyboards, may actually have absolute
 * axes, such as a volume axis for the volume up / volume down buttons. 
 * Also, some devices like a PS3 controller report many unusual axis, probably 
 * used in nonstandard ways by the PS3 console. All these type of axes are  
 * ignored by this function and by this driver. 
 */
static bool get_num_axes(int fd, int * num_axes) 
{
    int res, i;
    unsigned long abs_bits[NLONGS(ABS_CNT)]  = {0};
    int axes = 0;
  
    /* Only accept the axes from ABS_X up unto ABS_HAT3Y as real joystick
    * axes. The following axes, ABS_PRESSURE, ABS_DISTANCE, ABS_TILT_X, 
    * ABS_TILT_Y, ABS_TOOL_WIDTH seem to be for use by a drawing tablet like a 
    * Wacom. ABS_VOLUME is for volume sliders or key pairs on some keyboards. 
    * Other axes up to ABS_MISC may exist, such as on the PS3, 
    * but they are most likely not useful.
    */ 
    
    /* Scan the axes to get their properties. */
    res = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);    
    if (res < 0) return false;
    for (i = ABS_X; i <= ABS_HAT3Y; i++) 
    {
        if(TEST_BIT(i, abs_bits)) 
        {
           axes++;
        }
    }
  /* Finally store the aount of axes. */
  (*num_axes) = axes;
  return true;
}



    
/* check_is_event_joystick: 
 *
 *  Return true if this fd supports the /dev/inputt/eventxx API and 
 * is a joystick indeed.
 */
static bool check_is_event_joystick(int fd) {
  unsigned long bitmask[NLONGS(EV_CNT)]   = {0};
  
  if (ioctl (fd, EVIOCGBIT(0, sizeof(bitmask)), bitmask) < 0) {
    return false;
  }
  
  /* If there are buttons and axes, it may be a joystick. Some of 
   these axes may be not joystick-related though, so investigate further. */
   if (TEST_BIT(EV_ABS, bitmask) && TEST_BIT(EV_KEY, bitmask)) {   
     int axes = 0, buttons = 0; 
     /* Check the axes and buttons to see it it really is a joystick. */
     if(!get_num_buttons(fd, &buttons)) return false;
     if(!get_num_axes(fd, &axes)) return false;
     /* The device must have at least 1 joystick related axis, and 1 joystick 
      * related button. This is needed because some devices, such as mouse pads, 
      * have ABS_X and ABS_Y axes just like a joystick would, but they don't have 
      * joystick related buttons. By checking if the device has both joystick
      *related axes and butons, such mouse pads can be excluded. 
      */
     return ((axes > 0) && (buttons > 0));
   }
   
   /* No axes, no buttons, no joystick, sorry. */
   return false;
}


static bool ljoy_detect_device_name(int num, ALLEGRO_USTR *device_name)
{
   ALLEGRO_CONFIG *cfg;
   char key[80];
   const char *value;
   struct stat stbuf;

   al_ustr_truncate(device_name, 0);

   cfg = al_get_system_config();
   if (cfg) {
      snprintf(key, sizeof(key), "device%d", num);
      value = al_get_config_value(cfg, "joystick", key);
      if (value)
         al_ustr_assign_cstr(device_name, value);
   }

   if (al_ustr_size(device_name) == 0)
      al_ustr_appendf(device_name, "/dev/input/event%d", num);

   return (stat(al_cstr(device_name), &stbuf) == 0);
}



static ALLEGRO_JOYSTICK_LINUX *ljoy_by_device_name(
   const ALLEGRO_USTR *device_name)
{
   unsigned i;

   for (i = 0; i < _al_vector_size(&joysticks); i++) {
      ALLEGRO_JOYSTICK_LINUX **slot = _al_vector_ref(&joysticks, i);
      ALLEGRO_JOYSTICK_LINUX *joy = *slot;

      if (joy && al_ustr_equal(device_name, joy->device_name))
         return joy;
   }

   return NULL;
}



static void ljoy_generate_configure_event(void)
{
   ALLEGRO_EVENT event;
   event.joystick.type = ALLEGRO_EVENT_JOYSTICK_CONFIGURATION;
   event.joystick.timestamp = al_get_time();

   _al_generate_joystick_event(&event);
}



static ALLEGRO_JOYSTICK_LINUX *ljoy_allocate_structure(void)
{
   ALLEGRO_JOYSTICK_LINUX **slot;
   ALLEGRO_JOYSTICK_LINUX *joy;
   unsigned i;

   for (i = 0; i < _al_vector_size(&joysticks); i++) {
      slot = _al_vector_ref(&joysticks, i);
      joy = *slot;

      if (joy->config_state == LJOY_STATE_UNUSED)
         return joy;
   }

   joy = al_calloc(1, sizeof *joy);
   slot = _al_vector_alloc_back(&joysticks);
   *slot = joy;
   return joy;
}



static void inactivate_joy(ALLEGRO_JOYSTICK_LINUX *joy)
{
   int i;

   if (joy->config_state == LJOY_STATE_UNUSED)
      return;
   joy->config_state = LJOY_STATE_UNUSED;

   _al_unix_stop_watching_fd(joy->fd);
   close(joy->fd);
   joy->fd = -1;

   for (i = 0; i < joy->parent.info.num_sticks; i++)
      al_free((void *)joy->parent.info.stick[i].name);
   for (i = 0; i < joy->parent.info.num_buttons; i++)
      al_free((void *)joy->parent.info.button[i].name);
   memset(&joy->parent.info, 0, sizeof(joy->parent.info));
   memset(&joy->joystate, 0, sizeof(joy->joystate));

   al_ustr_free(joy->device_name);
   joy->device_name = NULL;
}


static void ljoy_scan(bool configure)
{
   int fd;
   ALLEGRO_JOYSTICK_LINUX *joy, **joypp;
   int num;
   ALLEGRO_USTR *device_name;
   unsigned i;

   /* Clear mark bits. */
   for (i = 0; i < _al_vector_size(&joysticks); i++) {
      joypp = _al_vector_ref(&joysticks, i);
      joy = *joypp;
      joy->marked = false;
   }

   device_name = al_ustr_new("");

   /* This is a big number, but there can be gaps, and other unrelated event queues. 
    * Perhaps it would be better to use glob() here in stead o gessing the numbers 
    * like this?
    */
   for (num = 0; num < 32; num++) {
      if (!ljoy_detect_device_name(num, device_name))
         continue;

      joy = ljoy_by_device_name(device_name);
      if (joy) {
         ALLEGRO_DEBUG("Device %s still exists\n", al_cstr(device_name));
         joy->marked = true;
         continue;
      }

      /* Try to open the device. The device must be pened in O_RDWR mode to allow 
       * writing of haptic effects! The haptic driver for linux 
       * reuses the joystick driver's FD. */
      fd = open(al_cstr(device_name), O_RDWR|O_NONBLOCK);
      if (fd == -1) {
         ALLEGRO_WARN("Failed to open device %s\n", al_cstr(device_name));
         continue;
      }
 
      if (!check_is_event_joystick(fd)) {
         close(fd);
         continue;
      }
 
      ALLEGRO_DEBUG("Device %s is new\n", al_cstr(device_name));

      joy = ljoy_allocate_structure();
      joy->fd = fd;
      joy->device_name = al_ustr_dup(device_name);
      joy->config_state = LJOY_STATE_BORN;
      joy->marked = true;
      config_needs_merging = true;

      if (ioctl(fd, EVIOCGNAME(sizeof(joy->name)), joy->name) < 0)
         strcpy(joy->name, "Unknown");
      
      /* Fill in the joystick information fields. */
      {
         int num_axes = 0;
         int num_buttons;
         int b;
         unsigned long abs_bits[NLONGS(ABS_CNT)]  = {0};
         int res, i;  
         int stick = 0;
         int axis  = 0;
         int num_sticks    = 0;
         int num_throttles = 0;
   
         // get_num_axes(fd, &num_axes);
         get_num_buttons(fd, &num_buttons);
   
         if (num_buttons > _AL_MAX_JOYSTICK_BUTTONS)
            num_buttons = _AL_MAX_JOYSTICK_BUTTONS;
         
         /* Scan the axes to get their properties. */
        res = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);    
        if (res < 0) continue;
        for (i = ABS_X; i <= ABS_HAT3Y; i++) {
          if(TEST_BIT(i, abs_bits)) {
          struct input_absinfo absinfo;
          if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0)
                    continue;
            if (
                 (i == ABS_THROTTLE) || (i == ABS_RUDDER) || (i == ABS_WHEEL) 
              || (i == ABS_GAS)      || (i == ABS_BRAKE) || (i== ABS_BRAKE) 
              || (i == ABS_PRESSURE) || (i == ABS_DISTANCE) || (i== ABS_TOOL_WIDTH) 
            ) { 
              /* One axis throttle. */
               num_throttles++;
               joy->parent.info.stick[stick].flags = ALLEGRO_JOYFLAG_ANALOGUE;
               joy->parent.info.stick[stick].num_axes = 1;
               joy->parent.info.stick[stick].axis[0].name = "X";
               joy->parent.info.stick[stick].name = al_malloc(32);
               snprintf((char *)joy->parent.info.stick[stick].name, 32, "Throttle %d", num_throttles);
               joy->axis_mapping[i].stick = stick;
               joy->axis_mapping[i].axis  = 0;
               joy->axis_mapping[i].min   = absinfo.minimum;
               joy->axis_mapping[i].max   = absinfo.maximum;
               joy->axis_mapping[i].value = absinfo.value;
               joy->axis_mapping[i].fuzz  = absinfo.fuzz;
               joy->axis_mapping[i].flat  = absinfo.flat;
              /* Consider all these types of axis as throttle axis. */
               stick++;
            } else { /* regular axis, two axis stick. */
               int digital = ((i >= ABS_HAT0X) && (i <=  ABS_HAT3Y));
               if (axis == 0) { /* first axis of new joystick */
                num_sticks++;
                if (digital) {
                  joy->parent.info.stick[stick].flags = ALLEGRO_JOYFLAG_DIGITAL;
                } else { 
                  joy->parent.info.stick[stick].flags = ALLEGRO_JOYFLAG_ANALOGUE;
                }
                joy->parent.info.stick[stick].num_axes = 2;
                joy->parent.info.stick[stick].axis[0].name = "X";
                joy->parent.info.stick[stick].axis[1].name = "Y";
                joy->parent.info.stick[stick].name = al_malloc (32);
                snprintf((char *)joy->parent.info.stick[stick].name, 32, "Stick %d", num_sticks);
                joy->axis_mapping[i].stick = stick;
                joy->axis_mapping[i].axis  = axis;
                joy->axis_mapping[i].min   = absinfo.minimum;
                joy->axis_mapping[i].max   = absinfo.maximum;
                joy->axis_mapping[i].value = absinfo.value;
                joy->axis_mapping[i].fuzz  = absinfo.fuzz;
                joy->axis_mapping[i].flat  = absinfo.flat;                
                axis++;
               } else { /* Second axis. */
                joy->axis_mapping[i].stick = stick;
                joy->axis_mapping[i].axis  = axis;
                joy->axis_mapping[i].min   = absinfo.minimum;
                joy->axis_mapping[i].max   = absinfo.maximum;
                joy->axis_mapping[i].value = absinfo.value;
                joy->axis_mapping[i].fuzz  = absinfo.fuzz;
                joy->axis_mapping[i].flat  = absinfo.flat;
                stick++;
                axis = 0;
               }
            }
            num_axes++;
          }
        }
    
   
        joy->parent.info.num_sticks = stick;
   
         /* Do the buttons. */
   
         for (b = 0; b < num_buttons; b++) {
            joy->parent.info.button[b].name = al_malloc(32);
            snprintf((char *)joy->parent.info.button[b].name, 32, "B%d", b+1);
         }
   
         joy->parent.info.num_buttons = num_buttons;
      }

      /* Register the joystick with the fdwatch subsystem.  */
      _al_unix_start_watching_fd(joy->fd, ljoy_process_new_data, joy);
   }

   al_ustr_free(device_name);

   /* Schedule unmarked structures to be inactivated. */
   for (i = 0; i < _al_vector_size(&joysticks); i++) {
      joypp = _al_vector_ref(&joysticks, i);
      joy = *joypp;

      if (joy->config_state == LJOY_STATE_ALIVE && !joy->marked) {
         ALLEGRO_DEBUG("Device %s to be inactivated\n",
            al_cstr(joy->device_name));
         joy->config_state = LJOY_STATE_DYING;
         config_needs_merging = true;
      }
   }

   /* Generate a configure event if necessary.
    * Even if we generated one before that the user hasn't responded to,
    * we don't know if the user received it so always generate it.
    */
   if (config_needs_merging && configure) {
      ljoy_generate_configure_event();
   }
}



static void ljoy_merge(void)
{
   unsigned i;

   config_needs_merging = false;
   num_joysticks = 0;

   for (i = 0; i < _al_vector_size(&joysticks); i++) {
      ALLEGRO_JOYSTICK_LINUX **slot = _al_vector_ref(&joysticks, i);
      ALLEGRO_JOYSTICK_LINUX *joy = *slot;

      switch (joy->config_state) {
         case LJOY_STATE_UNUSED:
            break;

         case LJOY_STATE_BORN:
         case LJOY_STATE_ALIVE:
            joy->config_state = LJOY_STATE_ALIVE;
            num_joysticks++;
            break;

         case LJOY_STATE_DYING:
            inactivate_joy(joy);
            break;
      }
   }

   ALLEGRO_DEBUG("Merge done, num_joysticks=%d\n", num_joysticks);
}



#ifdef SUPPORT_HOTPLUG
/* ljoy_config_dev_changed: [fdwatch thread]
 *  Called when the /dev hierarchy changes.
 */
static void ljoy_config_dev_changed(void *data)
{
   char buf[128];
   struct itimerspec spec;
   (void)data;

   /* Empty the event buffer. We only care that some inotify event was sent but it
    * doesn't matter what it is since we are going to do a full scan anyway once
    * the timer_fd fires.
    */
   while (read(inotify_fd, buf, sizeof(buf)) > 0) {
   }

   /* Set the timer to fire once in one second.
    * We cannot scan immediately because the devices may not be ready yet :-P
    */
   spec.it_value.tv_sec = 1;
   spec.it_value.tv_nsec = 0;
   spec.it_interval.tv_sec = 0;
   spec.it_interval.tv_nsec = 0;
   timerfd_settime(timer_fd, 0, &spec, NULL);
}



/* ljoy_config_rescan: [fdwatch thread]
 *  Rescans for joystick devices a little while after devices change.
 */
static void ljoy_config_rescan(void *data)
{
   uint64_t exp;
   (void)data;

   /* Empty the event buffer. */
   while (read(timer_fd, &exp, sizeof(uint64_t)) > 0) {
   }

   al_lock_mutex(config_mutex);
   ljoy_scan(true);
   al_unlock_mutex(config_mutex);
}
#endif



/* ljoy_init_joystick: [primary thread]
 *  Initialise the joystick driver.
 */
static bool ljoy_init_joystick(void)
{
   _al_vector_init(&joysticks, sizeof(ALLEGRO_JOYSTICK_LINUX *));
   num_joysticks = 0;

   // Scan for joysticks
   ljoy_scan(false);
   ljoy_merge();

   config_mutex = al_create_mutex();

#ifdef SUPPORT_HOTPLUG
   inotify_fd = inotify_init1(IN_NONBLOCK);
   timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
   if (inotify_fd != -1 && timer_fd != -1) {
      /* Modern Linux probably only needs to monitor /dev/input. */
      inotify_add_watch(inotify_fd, "/dev/input", IN_CREATE|IN_DELETE);
      _al_unix_start_watching_fd(inotify_fd, ljoy_config_dev_changed, NULL);
      _al_unix_start_watching_fd(timer_fd, ljoy_config_rescan, NULL);
      ALLEGRO_INFO("Hotplugging enabled\n");
   }
   else {
      ALLEGRO_WARN("Hotplugging not enabled\n");
      if (inotify_fd != -1) {
         close(inotify_fd);
         inotify_fd = -1;
      }
      if (timer_fd != -1) {
         close(timer_fd);
         timer_fd = -1;
      }
   }
#endif

   return true;
}



/* ljoy_exit_joystick: [primary thread]
 *  Shut down the joystick driver.
 */
static void ljoy_exit_joystick(void)
{
   int i;

#ifdef SUPPORT_HOTPLUG
   if (inotify_fd != -1) {
      _al_unix_stop_watching_fd(inotify_fd);
      close(inotify_fd);
      inotify_fd = -1;
   }
   if (timer_fd != -1) {
      _al_unix_stop_watching_fd(timer_fd);
      close(timer_fd);
      timer_fd = -1;
   }
#endif

   al_destroy_mutex(config_mutex);
   config_mutex = NULL;

   for (i = 0; i < (int)_al_vector_size(&joysticks); i++) {
      ALLEGRO_JOYSTICK_LINUX **slot = _al_vector_ref(&joysticks, i);
      inactivate_joy(*slot);
      al_free(*slot);
   }
   _al_vector_free(&joysticks);
   num_joysticks = 0;
}



/* ljoy_reconfigure_joysticks: [primary thread]
 */
static bool ljoy_reconfigure_joysticks(void)
{
   bool ret = false;

   al_lock_mutex(config_mutex);

   if (config_needs_merging) {
      ljoy_merge();
      ret = true;
   }

   al_unlock_mutex(config_mutex);

   return ret;
}



/* ljoy_num_joysticks: [primary thread]
 *
 *  Return the number of joysticks available on the system.
 */
static int ljoy_num_joysticks(void)
{
   return num_joysticks;
}



/* ljoy_get_joystick: [primary thread]
 *
 *  Returns the address of a ALLEGRO_JOYSTICK structure for the device
 *  number NUM.
 */
static ALLEGRO_JOYSTICK *ljoy_get_joystick(int num)
{
   ALLEGRO_JOYSTICK *ret = NULL;
   unsigned i;
   ASSERT(num >= 0);

   al_lock_mutex(config_mutex);

   for (i = 0; i < _al_vector_size(&joysticks); i++) {
      ALLEGRO_JOYSTICK_LINUX **slot = _al_vector_ref(&joysticks, i);
      ALLEGRO_JOYSTICK_LINUX *joy = *slot;

      if (ACTIVE_STATE(joy->config_state)) {
         if (num == 0) {
            ret = (ALLEGRO_JOYSTICK *)joy;
            break;
         }
         num--;
      }
   }

   al_unlock_mutex(config_mutex);

   return ret;
}



/* ljoy_release_joystick: [primary thread]
 *
 *  Close the device for a joystick then free the joystick structure.
 */
static void ljoy_release_joystick(ALLEGRO_JOYSTICK *joy_)
{
   (void)joy_;
}



/* ljoy_get_joystick_state: [primary thread]
 *
 *  Copy the internal joystick state to a user-provided structure.
 */
static void ljoy_get_joystick_state(ALLEGRO_JOYSTICK *joy_, ALLEGRO_JOYSTICK_STATE *ret_state)
{
   ALLEGRO_JOYSTICK_LINUX *joy = (ALLEGRO_JOYSTICK_LINUX *) joy_;
   ALLEGRO_EVENT_SOURCE *es = al_get_joystick_event_source();

   _al_event_source_lock(es);
   {
      *ret_state = joy->joystate;
   }
   _al_event_source_unlock(es);
}



static const char *ljoy_get_name(ALLEGRO_JOYSTICK *joy_)
{
   ALLEGRO_JOYSTICK_LINUX *joy = (ALLEGRO_JOYSTICK_LINUX *)joy_;
   return joy->name;
}



static bool ljoy_get_active(ALLEGRO_JOYSTICK *joy_)
{
   ALLEGRO_JOYSTICK_LINUX *joy = (ALLEGRO_JOYSTICK_LINUX *)joy_;

   return ACTIVE_STATE(joy->config_state);
}



/* ljoy_process_new_data: [fdwatch thread]
 *
 *  Process new data arriving in the joystick's fd.
 */
static void ljoy_process_new_data(void *data)
{
   ALLEGRO_JOYSTICK_LINUX *joy = data;
   ALLEGRO_EVENT_SOURCE *es = al_get_joystick_event_source();

   if (!es) {
      // Joystick driver not fully initialized
      return;
   }
   
   _al_event_source_lock(es);
   {
      struct input_event input_events[32];
      int bytes, nr, i;

      while ((bytes = read(joy->fd, &input_events, sizeof input_events)) > 0) {

         nr = bytes / sizeof(struct input_event);

         for (i = 0; i < nr; i++) {
            int type   = input_events[i].type;
            int code   = input_events[i].code;
            int value  = input_events[i].value;
            if (type == EV_KEY) {
              int number = code - BTN_JOYSTICK;
              if (number < _AL_MAX_JOYSTICK_BUTTONS) {              
              if (value)
                     joy->joystate.button[number] = 32767;
                  else
                     joy->joystate.button[number] = 0;

              ljoy_generate_button_event(joy, number,
                                             (value
                                              ? ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN
                                              : ALLEGRO_EVENT_JOYSTICK_BUTTON_UP));
              } 
            } else if ((type == EV_ABS ) && (code < ABS_MISC)) {
                int       stick = -1;
                int       axis  = -1;
                float     range, pos;
                
              
                AXIS_MAPPING * map = joy->axis_mapping +code;
                axis  = map->axis;
                stick = map->stick;                
                range = (float) map->max - (float) map->min;
                /* Normalize around 0. */
                pos   = (float) value    - (float) map->min;
                /* Divide by range, to get value between 0.0 and 1.0  */
                pos   = pos              / range;
                /* Now multiply by 2.0 and substract 1.0 to get a value between 
                * -1.0 and 1.0
                */
                pos   = pos * 2.0f - 1.0f;
                joy->joystate.stick[stick].axis[axis] = pos;
                ljoy_generate_axis_event(joy, stick, axis, pos);           
            }
         }   
      }
   }
   _al_event_source_unlock(es);
}



/* ljoy_generate_axis_event: [fdwatch thread]
 *
 *  Helper to generate an event after an axis is moved.
 *  The joystick must be locked BEFORE entering this function.
 */
static void ljoy_generate_axis_event(ALLEGRO_JOYSTICK_LINUX *joy, int stick, int axis, float pos)
{
   ALLEGRO_EVENT event;
   ALLEGRO_EVENT_SOURCE *es = al_get_joystick_event_source();

   if (!_al_event_source_needs_to_generate_event(es))
      return;

   event.joystick.type = ALLEGRO_EVENT_JOYSTICK_AXIS;
   event.joystick.timestamp = al_get_time();
   event.joystick.id = (ALLEGRO_JOYSTICK *)joy;
   event.joystick.stick = stick;
   event.joystick.axis = axis;
   event.joystick.pos = pos;
   event.joystick.button = 0;

   _al_event_source_emit_event(es, &event);
}



/* ljoy_generate_button_event: [fdwatch thread]
 *
 *  Helper to generate an event after a button is pressed or released.
 *  The joystick must be locked BEFORE entering this function.
 */
static void ljoy_generate_button_event(ALLEGRO_JOYSTICK_LINUX *joy, int button, ALLEGRO_EVENT_TYPE event_type)
{
   ALLEGRO_EVENT event;
   ALLEGRO_EVENT_SOURCE *es = al_get_joystick_event_source();

   if (!_al_event_source_needs_to_generate_event(es))
      return;

   event.joystick.type = event_type;
   event.joystick.timestamp = al_get_time();
   event.joystick.id = (ALLEGRO_JOYSTICK *)joy;
   event.joystick.stick = 0;
   event.joystick.axis = 0;
   event.joystick.pos = 0.0;
   event.joystick.button = button;

   _al_event_source_emit_event(es, &event);
}

#endif /* ALLEGRO_HAVE_LINUX_JOYSTICK_H */



/* vim: set sts=3 sw=3 et: */
