#ifndef __al_included_allegro_aintern_ljoynu_h
#define __al_included_allegro_aintern_ljoynu_h


#include "allegro5/joystick.h"
#include "allegro5/internal/aintern_joystick.h"




/* State transitions:
 *    unused -> born
 *    born -> alive
 *    born -> dying
 *    active -> dying
 *    dying -> unused
 */
typedef enum {
   LJOY_STATE_UNUSED,
   LJOY_STATE_BORN,
   LJOY_STATE_ALIVE,
   LJOY_STATE_DYING
} CONFIG_STATE;

#define ACTIVE_STATE(st) \
   ((st) == LJOY_STATE_ALIVE || (st) == LJOY_STATE_DYING)


/* Map a Linux joystick axis number to an Allegro (stick,axis) pair 
 * Uses the input event interface's numbering. ABS_MISC = 0x28,
 * So that is the maximum of allowed axes on Linux.
 */
#define TOTAL_JOYSTICK_AXES  0x28

typedef struct {
   int stick;
   int axis;
   int value;
   int min;
   int max;    
   int fuzz;
   int flat;
} AXIS_MAPPING;


typedef struct ALLEGRO_JOYSTICK_LINUX
{
   ALLEGRO_JOYSTICK parent;
   int config_state;
   bool marked;
   int fd;
   ALLEGRO_USTR *device_name;

   AXIS_MAPPING axis_mapping[TOTAL_JOYSTICK_AXES];
   ALLEGRO_JOYSTICK_STATE joystate;
   char name[100];
} ALLEGRO_JOYSTICK_LINUX;


#endif