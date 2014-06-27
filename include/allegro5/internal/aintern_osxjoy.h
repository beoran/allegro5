#ifndef __al_included_allegro_aintern_wjoydxnu_h
#define __al_included_allegro_aintern_wjoydxnu_h


/** Part of the new hidjoy joystick for OSX 
 * types are shared here for use by the haptic susbystem. */

/* State transitions:
 *    unused -> born
 *    born -> alive
 *    born -> dying
 *    active -> dying
 *    dying -> unused
 */
typedef enum {
   JOY_STATE_UNUSED,
   JOY_STATE_BORN,
   JOY_STATE_ALIVE,
   JOY_STATE_DYING
} CONFIG_STATE;

// These values can be found in the USB HID Usage Tables:
// http://www.usb.org/developers/hidpage
#define GENERIC_DESKTOP_USAGE_PAGE 0x01
#define JOYSTICK_USAGE_NUMBER      0x04
#define GAMEPAD_USAGE_NUMBER       0x05
#define ALLEGRO_JOYSTICK_OSX_NAME_MAX 128;

typedef struct {
   ALLEGRO_JOYSTICK parent;
   int num_buttons;
   int num_x_axes;
   int num_y_axes;
   int num_z_axes;
   IOHIDElementRef buttons[_AL_MAX_JOYSTICK_BUTTONS];
   IOHIDElementRef axes[_AL_MAX_JOYSTICK_STICKS][_AL_MAX_JOYSTICK_AXES];
   long min[_AL_MAX_JOYSTICK_STICKS][_AL_MAX_JOYSTICK_AXES];
   long max[_AL_MAX_JOYSTICK_STICKS][_AL_MAX_JOYSTICK_AXES];
   CONFIG_STATE cfg_state;
   ALLEGRO_JOYSTICK_STATE state;
   IOHIDDeviceRef ident;
   /* Name of the joystick */
   char name[ALLEGRO_JOYSTICK_OSX_NAME_MAX];
   /* For use by the haptic subsystem. */
   io_service_t service; 
} ALLEGRO_JOYSTICK_OSX;





#endif

/* vim: set sts=3 sw=3 et: */
