/*
 *    Example program for the Allegro library, by Peter Wang.
 *
 *    Compare software blending routines with hardware blending.
 */

#include <string>

#include "allegro5/allegro.h"

#include "allegro5/allegro_font.h"
#include "allegro5/allegro_image.h"
#include <allegro5/allegro_primitives.h>

#include "common.c"

#include "nihgui.hpp"

#define EX_HAPTIC2_MAX_HAPTICS 8

ALLEGRO_HAPTIC      *    haptics[EX_HAPTIC2_MAX_HAPTICS];
ALLEGRO_HAPTIC_EFFECT    haptic_effects[EX_HAPTIC2_MAX_HAPTICS];
ALLEGRO_HAPTIC_EFFECT_ID haptic_ids[EX_HAPTIC2_MAX_HAPTICS];
char                   * haptic_name[EX_HAPTIC2_MAX_HAPTICS];
int                      num_haptics;

struct CapacityName { 
  int   value;
  const char * name; 
};

CapacityName capname[] = {
  { ALLEGRO_HAPTIC_RUMBLE   , "rumble"   },  
  { ALLEGRO_HAPTIC_PERIODIC , "periodic" },
  { ALLEGRO_HAPTIC_CONSTANT , "constant" }, 
  { ALLEGRO_HAPTIC_SPRING   , "spring"   },
  { ALLEGRO_HAPTIC_FRICTION , "friction" },
  { ALLEGRO_HAPTIC_DAMPER   , "damper"   },
  { ALLEGRO_HAPTIC_INERTIA  , "inertia"  },
  { ALLEGRO_HAPTIC_RAMP     , "ramp"     }, 
  { ALLEGRO_HAPTIC_SQUARE   , "square"   },
  { ALLEGRO_HAPTIC_TRIANGLE , "triangle" },
  { ALLEGRO_HAPTIC_SINE     , "sine"     },
  { ALLEGRO_HAPTIC_SAW_UP   , "saw up"   },
  { ALLEGRO_HAPTIC_SAW_DOWN , "saw down" },
  { ALLEGRO_HAPTIC_CUSTOM   , "custom"   },
  { ALLEGRO_HAPTIC_GAIN     , "gain"     },
  { ALLEGRO_HAPTIC_ANGLE    , "angle"    },
  { ALLEGRO_HAPTIC_RADIUS   , "radius"   },
  { ALLEGRO_HAPTIC_AZIMUTH  , "azimuth"  }
};

#define EX_HAPTIC2_START_TYPES   0
#define EX_HAPTIC2_MAX_TYPES     8
#define EX_HAPTIC2_END_TYPES     8

#define EX_HAPTIC2_START_WAVES   8
#define EX_HAPTIC2_END_WAVES    13
#define EX_HAPTIC2_MAX_WAVES     5
/* Ignore custom waveforms for now... */

#define EX_HAPTIC2_START_COORDS 15
#define EX_HAPTIC2_MAX_COORDS    3
#define EX_HAPTIC2_END_COORDS   18




class Prog {
private:
   Dialog d;  
   
   List  device_list;
   List  type_list;
   List  waveform_list;
   
   Label device_label;  
   Label type_label;
   Label waveform_label;
   
   HSlider length_slider, delay_slider, loops_slider;
   Label replay_label, length_label, delay_label, loops_label;
   
   HSlider attack_length_slider, attack_level_slider;
   HSlider fade_length_slider, fade_level_slider;
   Label envelope_label, attack_length_label, attack_level_label;
   Label fade_length_label, fade_level_label;
   
   HSlider angle_slider, radius_slider, azimuth_slider;
   Label   coordinates_label, angle_label , radius_label , azimuth_label;
   
   HSlider level_slider;
   Label   constant_effect_label, level_label;
   
   HSlider start_level_slider, end_level_slider;
   Label ramp_effect_label, start_level_label, end_level_label;

   HSlider right_saturation_slider, right_coeff_slider;
   HSlider left_saturation_slider , left_coeff_slider;
   HSlider deadband_slider        , center_slider;
   
   Label right_saturation_label, right_coeff_label;
   Label left_saturation_label , left_coeff_label;
   Label condition_effect_label, deadband_label, center_label;
   
   HSlider period_slider, magnitude_slider, offset_slider, phase_slider;
   Label periodic_effect_label; 
   Label period_label, magnitude_label, offset_label, phase_label;
   
   HSlider strong_magnitude_slider, weak_magnitude_slider;
   Label rumble_effect_label, strong_magnitude_label, weak_magnitude_label;

   HSlider gain_slider;
   Label gain_label;
   
   Button play_button;
   Button stop_button;
   
   
public:
   Prog(const Theme & theme, ALLEGRO_DISPLAY *display);
   void run();

private:
   /*
    * void play_effect();
   void stop_effect();};
   */
};


Prog::Prog(const Theme & theme, ALLEGRO_DISPLAY *display) :
   d(Dialog(theme, display, 20, 40)),
   device_list(0),
   type_list(0),
   waveform_list(0),
   device_label("Haptic Device"),
   type_label("Haptic Effect Type"), 
   waveform_label("Wave Form Periodic Effect"),
   length_slider(1, 10),
   delay_slider(1, 10),
   loops_slider(1, 10), 
   replay_label("replay"),
   length_label("length", false),
   delay_label("delay", false),
   loops_label("loops"),   
   attack_length_slider(1, 10),
   attack_level_slider(5, 10),
   fade_length_slider(5, 10),
   fade_level_slider(1, 10),
   envelope_label("envelope"),
   attack_length_label("attack length", false),
   attack_level_label("attack level", false),
   fade_length_label("fade length", false),
   fade_level_label("fade level", false),
   angle_slider(0, 360), radius_slider(0, 10), azimuth_slider(180, 360),
   coordinates_label("coordinates"),
   angle_label("angle", false), 
   radius_label("radius", false), 
   azimuth_label("azimuth", false),
   level_slider(5, 10), 
   constant_effect_label("constant effect"),
   level_label("level", false),
   start_level_slider(3, 10), end_level_slider(7, 10), 
   ramp_effect_label("ramp effect"),
   start_level_label("start level", false),
   end_level_label("end level", false),
   right_saturation_slider(5, 10) , right_coeff_slider(5, 10),
   left_saturation_slider(5, 10)  , left_coeff_slider(5, 10),
   deadband_slider(1, 10)         , center_slider(5,10),
   right_saturation_label("right saturation", false), 
   right_coeff_label("right coefficient", false),
   left_saturation_label("left saturation", false), 
   left_coeff_label("left coefficient", false),
   condition_effect_label("condition effect"),
   deadband_label("deadband", false), 
   center_label("center", false),
   period_slider(1, 10), magnitude_slider(5, 10), 
   offset_slider(0, 10), phase_slider(0, 10),
   periodic_effect_label("periodic effect"), 
   period_label("period", false), 
   magnitude_label("magnitude", false), 
   offset_label("offset", false), 
   phase_label("phase", false),
   strong_magnitude_slider(5, 10), 
   weak_magnitude_slider(5, 10),
   rumble_effect_label("rumble effect"),
   strong_magnitude_label("strong magnitude", false), 
   weak_magnitude_label("weak magnitude", false),
   gain_slider(10, 10),
   gain_label("gain"),
   play_button("Play"),
   stop_button("Stop") 
{
  for (int i = 0; i < num_haptics; i++) {
    device_list.append_item(haptic_name[i]);
  } 
  d.add(device_label, 0, 1, 7, 1); 
  d.add(device_list , 0, 2, 7, 8);

  
   for (int i = EX_HAPTIC2_START_TYPES  ; i < EX_HAPTIC2_END_TYPES  ; i++) {
     type_list.append_item(capname[i].name);
   }
   d.add(type_label, 7, 1, 6, 1);    
   d.add(type_list , 7, 2, 6, 8);
   
   for (int i = EX_HAPTIC2_START_WAVES  ; i < EX_HAPTIC2_END_WAVES  ; i++) {
     waveform_list.append_item(capname[i].name);
   }
   d.add(waveform_label, 13, 1, 7, 1); 
   d.add(waveform_list , 13, 2, 7, 8);
   
   d.add(replay_label , 0, 10 , 7, 1);
   d.add(length_label , 0, 11 , 2, 1);
   d.add(length_slider, 2, 11 , 5, 1);
   d.add(delay_label  , 0, 12 , 2, 1);
   d.add(delay_slider , 2, 12 , 5, 1);
   d.add(loops_label  , 0, 13 , 7, 1);
   d.add(loops_slider , 2, 14 , 5, 1);
   d.add(gain_label   , 0, 15 , 7, 1);
   d.add(gain_slider  , 2, 16 , 5, 1);
   
   d.add(envelope_label         , 7 , 10, 6, 1);
   d.add(attack_length_label    , 7 , 11, 3, 1);
   d.add(attack_length_slider   , 10, 11, 3, 1);
   d.add(attack_level_label     , 7 , 12, 3, 1);
   d.add(attack_level_slider    , 10, 12, 3, 1);
   d.add(fade_length_label      , 7 , 13, 3, 1);
   d.add(fade_length_slider     , 10, 13, 3, 1);
   d.add(fade_level_label       , 7 , 14, 3, 1);
   d.add(fade_level_slider      , 10, 14, 3, 1);
   
   d.add(coordinates_label      , 13, 10, 7, 1);
   d.add(angle_label            , 13, 11, 2, 1);
   d.add(angle_slider           , 15, 11, 5, 1);
   d.add(radius_label           , 13, 12, 2, 1);
   d.add(radius_slider          , 15, 12, 5, 1);
   d.add(azimuth_label          , 13, 13, 2, 1);
   d.add(azimuth_slider         , 15, 13, 5, 1);
  
   d.add(condition_effect_label ,  0, 18, 7, 1); 
   d.add(right_coeff_label      ,  0, 19, 4, 1);
   d.add(right_coeff_slider     ,  4, 19, 3, 1);
   d.add(right_saturation_label ,  0, 20, 4, 1);
   d.add(right_saturation_slider,  4, 20, 3, 1);  
   d.add(left_coeff_label       ,  0, 21, 4, 1);
   d.add(left_coeff_slider      ,  4, 21, 3, 1);
   d.add(left_saturation_label  ,  0, 22, 4, 1);
   d.add(left_saturation_slider ,  4, 22, 3, 1);
   d.add(deadband_label         ,  0, 23, 4, 1);
   d.add(deadband_slider        ,  4, 23, 3, 1);
   d.add(center_label           ,  0, 24, 4, 1);
   d.add(center_slider          ,  4, 24, 3, 1);
   
   d.add(constant_effect_label  ,  7, 18, 6, 1);
   d.add(level_label            ,  7, 19, 3, 1);
   d.add(level_slider           , 10, 19, 3, 1);
   
   d.add(periodic_effect_label  , 13, 18, 7, 1);
   d.add(period_label           , 13, 19, 7, 1);
   d.add(period_slider          , 15, 19, 5, 1);
   d.add(magnitude_label        , 13, 20, 2, 1);
   d.add(magnitude_slider       , 15, 20, 5, 1);
   d.add(offset_label           , 13, 21, 2, 1);
   d.add(offset_slider          , 15, 21, 5, 1);
   d.add(phase_label            , 13, 22, 2, 1);
   d.add(phase_slider           , 15, 22, 5, 1);
   
  
   d.add(play_button  , 6, 38, 3, 1); 
   d.add(stop_button  , 12, 38, 3, 1); 

   
   /*
   rgba_label[0] = Label("Source tint/color RGBA");
   rgba_label[1] = Label("Dest tint/color RGBA");
   d.add(rgba_label[0], 1, 34, 5, 1);
   d.add(rgba_label[1], 7, 34, 5, 1);

   for (int i = 0; i < 2; i++) {
      r[i] = HSlider(255, 255);
      g[i] = HSlider(255, 255);
      b[i] = HSlider(255, 255);
      a[i] = HSlider(255, 255);
      d.add(r[i], 1 + i * 6, 35, 5, 1);
      d.add(g[i], 1 + i * 6, 36, 5, 1);
      d.add(b[i], 1 + i * 6, 37, 5, 1);
      d.add(a[i], 1 + i * 6, 38, 5, 1);
   }
   */
}

void Prog::run()
{
   d.prepare();

   while (!d.is_quit_requested()) {
      if (d.is_draw_requested()) {
         al_clear_to_color(al_map_rgb(128, 128, 128));
         d.draw();
         al_flip_display();
      }

      d.run_step(true);
   }
}




int main(int argc, char *argv[])
{
   ALLEGRO_DISPLAY *display;
   ALLEGRO_FONT *font;

   (void)argc;
   (void)argv;

   if (!al_init()) {
      abort_example("Could not init Allegro\n");
   }
   al_init_primitives_addon();
   al_install_keyboard();
   al_install_mouse();
   al_install_joystick();
   al_install_haptic();

   al_init_font_addon();
 
   al_set_new_display_flags(ALLEGRO_GENERATE_EXPOSE_EVENTS);
   display = al_create_display(800, 600);
   if (!display) {
      abort_example("Unable to create display\n");
   }
   
    
   font = al_create_builtin_font();
   if (!font) {
      abort_example("Failed to create builtin font.\n");
   }
   
   /*
   font = al_load_font("data/fixed_font.tga", 0, 0);
   if (!font) {
      abort_example("Failed to load data/fixed_font.tga\n");
   }
   */
   
   num_haptics = 0;
   
   if(al_is_display_haptic(al_get_current_display())) {
     haptics[num_haptics]    = al_get_haptic_from_display(al_get_current_display());
     haptic_name[num_haptics]= (char *)"display";
     num_haptics++;
   }
   
   for (int i = 0; i < al_get_num_joysticks(); i++) { 
    ALLEGRO_JOYSTICK * joy = al_get_joystick(i);
    if(al_is_joystick_haptic(joy)) {
      haptics[num_haptics]    = al_get_haptic_from_joystick(joy);
      haptic_name[num_haptics]= (char *)al_get_joystick_name(joy);
      num_haptics++;
    }
   }

   /* Don't remove these braces. */
   {
      Theme theme(font);
      Prog prog(theme, display);
      prog.run();
   }
   
   for (int i = 0; i < num_haptics; i++) { 
      al_release_haptic(haptics[i]); 
   }

   al_destroy_font(font);

   return 0;
}

/* vim: set sts=3 sw=3 et: */
