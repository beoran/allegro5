/*
 *    Example program for the Allegro library, by Beoran.
 *
 *    This program tests haptic effects.examples/ex_hapti
 */

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

#include "common.c"


#define MAX_HAPTICS  32

/* globals */
ALLEGRO_EVENT_QUEUE  *event_queue;



int main(void)
{
   int index;
   ALLEGRO_DISPLAY * display;
   ALLEGRO_HAPTIC  * haptic = NULL;
   ALLEGRO_HAPTIC_EFFECT effect = {0};
   double intensity = 1.0;
   double duration  = 1.0;
   int num_joysticks;

   effect.type                           = ALLEGRO_HAPTIC_RUMBLE;
   effect.data.rumble.strong_magnitude   = intensity;
   effect.data.rumble.weak_magnitude     = intensity;  
   effect.replay.delay                   = 0.1;
   effect.replay.length                  = duration;   
   
   if (!al_init()) {
      abort_example("Could not init Allegro.\n");
   }

   display = al_create_display(640, 480);
   if (!display) {
      abort_example("al_create_display failed\n");
   }
   al_install_joystick();
   al_install_haptic();

   event_queue = al_create_event_queue();
   if (!event_queue) {
      abort_example("al_create_event_queue failed\n");
   }
   open_log();
     
   num_joysticks = al_get_num_joysticks();
   for(index = 0; index < num_joysticks; index++) {
     ALLEGRO_JOYSTICK * joy = al_get_joystick(index);
     if(!joy) continue;
     if (!al_is_joystick_haptic(joy)) {  
       log_printf("Joystick %s does not support force feedback.\n", al_get_joystick_name(joy));
       al_release_joystick(joy);
       continue;
     } else {
       ALLEGRO_HAPTIC_EFFECT_ID id;
       log_printf("Joystick %s supports force feedback.\n", al_get_joystick_name(joy));
       haptic = al_get_haptic_from_joystick(joy);
       log_printf("Can play back %d haptic effects.\n", al_get_num_haptic_effects(haptic));
       log_printf("Set gain to 0.8: %d.\n", al_set_haptic_gain(haptic, 0.8));
       log_printf("Get gain: %lf.\n", al_get_haptic_gain(haptic));
       log_printf("Capabilities: %d.\n", al_get_haptic_capabilities(haptic));
       log_printf("Upload effect: %d.\n ", al_upload_haptic_effect(haptic, &effect, &id));
       log_printf("Playing effect: %d.\n ", al_play_haptic_effect(&id, 5));
       while (al_is_haptic_effect_playing(&id)) {
       }
       log_printf("Set gain to 0.4: %d.\n", al_set_haptic_gain(haptic, 0.4));
       log_printf("Get gain: %lf.\n", al_get_haptic_gain(haptic));
       log_printf("Playing effect again: %d.\n ", al_play_haptic_effect(&id, 5));
       al_rest(2.0);
       log_printf("Stopping effect: %d.\n ", al_stop_haptic_effect(&id));
       
       while (al_is_haptic_effect_playing(&id)) {
         //log_printf(".");
       }
       log_printf("Release effect: %d.\n ", al_release_haptic_effect(&id));
       log_printf("Release haptic: %d.\n ", al_release_haptic(haptic));
     }
   }
          
   log_printf("\nAll done!\n");       
   close_log(true);
   // al_register_event_source(event_queue, al_get_display_event_source(display));
   // al_register_event_source(event_queue, al_get_haptic_event_source());

   
   return 0;
}

/* vim: set ts=8 sts=3 sw=3 et: */
