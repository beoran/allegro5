/*
 *    Example program for the Allegro library, by Peter Wang.
 *
 *    This program tests joystick events.
 */

#include <allegro5/allegro.h>
#include <allegro5/haptic.h>
#include <allegro5/allegro_primitives.h>

#include "common.c"

#define MAX_HAPTICS  32

/* globals */
ALLEGRO_EVENT_QUEUE  *event_queue;

int num_haptics = 0;
ALLEGRO_HAPTIC * haptics[MAX_HAPTICS];


int main(void)
{
   int index;
   ALLEGRO_DISPLAY *display;

   if (!al_init()) {
      abort_example("Could not init Allegro.\n");
   }

   display = al_create_display(640, 480);
   if (!display) {
      abort_example("al_create_display failed\n");
   }

   al_install_haptic();

   event_queue = al_create_event_queue();
   if (!event_queue) {
      abort_example("al_create_event_queue failed\n");
   }
   open_log();
   num_haptics = al_get_num_haptics();
   log_printf("Found %d haptic devices.\n", num_haptics);
   for(index = 0; index < num_haptics; index++) {
     ALLEGRO_HAPTIC * hap = al_get_haptic(index);; 
     haptics[index] = hap;
     if (hap) {  
      log_printf("Opened device %d: %s.\n", al_get_haptic_name(hap));
     } else {
      log_printf("Could not open haptic device %d.\n", index);
     }
   }
   
   
   close_log(true);
   // al_register_event_source(event_queue, al_get_display_event_source(display));
   // al_register_event_source(event_queue, al_get_haptic_event_source());

   
   return 0;
}

/* vim: set ts=8 sts=3 sw=3 et: */
