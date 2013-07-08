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

struct Haptic {
  ALLEGRO_HAPTIC         * haptic;
  ALLEGRO_HAPTIC_EFFECT    effect;
  ALLEGRO_HAPTIC_EFFECT_ID id;
  const char             * name;
  bool                     playing;
};



Haptic                   haptics[EX_HAPTIC2_MAX_HAPTICS];
int                      num_haptics =  0;
/* Haptic                 * last_haptic = NULL; */

struct CapacityName { 
  int   value;
  const char * name; 
};


CapacityName capname[] = {
  { ALLEGRO_HAPTIC_RUMBLE   , "ALLEGRO_HAPTIC_RUMBLE"   },  
  { ALLEGRO_HAPTIC_PERIODIC , "ALLEGRO_HAPTIC_PERIODIC" },
  { ALLEGRO_HAPTIC_CONSTANT , "ALLEGRO_HAPTIC_CONSTANT" }, 
  { ALLEGRO_HAPTIC_SPRING   , "ALLEGRO_HAPTIC_SPRING"   },
  { ALLEGRO_HAPTIC_FRICTION , "ALLEGRO_HAPTIC_FRICTION" },
  { ALLEGRO_HAPTIC_DAMPER   , "ALLEGRO_HAPTIC_DAMPER"   },
  { ALLEGRO_HAPTIC_INERTIA  , "ALLEGRO_HAPTIC_INERTIA"  },
  { ALLEGRO_HAPTIC_RAMP     , "ALLEGRO_HAPTIC_RAMP"     }, 
  { ALLEGRO_HAPTIC_SQUARE   , "ALLEGRO_HAPTIC_SQUARE"   },
  { ALLEGRO_HAPTIC_TRIANGLE , "ALLEGRO_HAPTIC_TRIANGLE" },
  { ALLEGRO_HAPTIC_SINE     , "ALLEGRO_HAPTIC_SINE"     },
  { ALLEGRO_HAPTIC_SAW_UP   , "ALLEGRO_HAPTIC_SAW_UP"   },
  { ALLEGRO_HAPTIC_SAW_DOWN , "ALLEGRO_HAPTIC_SAW_DOWN" },
  { ALLEGRO_HAPTIC_CUSTOM   , "ALLEGRO_HAPTIC_CUSTOM"   },
  { ALLEGRO_HAPTIC_GAIN     , "ALLEGRO_HAPTIC_GAIN"     },
  { ALLEGRO_HAPTIC_ANGLE    , "ALLEGRO_HAPTIC_ANGLE"    },
  { ALLEGRO_HAPTIC_RADIUS   , "ALLEGRO_HAPTIC_RADIUS"   },
  { ALLEGRO_HAPTIC_AZIMUTH  , "ALLEGRO_HAPTIC_AZIMUTH"  }
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


class CanStopAndPlay {
   public: 
   virtual void on_play() = 0;
   virtual void on_stop() = 0;
};


class PlayButton : public Button {
protected:
   CanStopAndPlay * stop_and_play;  
public:
   PlayButton(CanStopAndPlay * snp) : Button("Play") , stop_and_play(snp) {}
   void on_click(int mx, int my);
};


void PlayButton::on_click(int, int)
{
   if (is_disabled()) return; 
   log_printf("Start playing...\n");
   if(stop_and_play) {
      stop_and_play->on_play();
   }
}

class StopButton : public Button {
protected:
   CanStopAndPlay * stop_and_play;  
public:
   StopButton(CanStopAndPlay * snp) : Button("Stop") , stop_and_play(snp) {}
   void on_click(int mx, int my);
};


void StopButton::on_click(int, int)
{
   if (is_disabled()) return; 
   log_printf("Stop playing...\n");
   if(stop_and_play) {
      stop_and_play->on_stop();
   }
}



class Prog : public CanStopAndPlay {
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
   
   Label message_label, message_label_label;
   
   PlayButton play_button;
   StopButton stop_button;
   
   Haptic                 * last_haptic;
   Haptic                 * show_haptic;
   
   
public:
   Prog(const Theme & theme, ALLEGRO_DISPLAY *display);
   void run();
   void update();
   virtual void on_play();
   virtual void on_stop();
   void get_envelope(ALLEGRO_HAPTIC_ENVELOPE * envelope);
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
   replay_label("Replay"),
   length_label("Length", false),
   delay_label("Delay", false),
   loops_label("Loops"),   
   attack_length_slider(2, 10),
   attack_level_slider(4, 10),
   fade_length_slider(2, 10),
   fade_level_slider(4, 10),
   envelope_label("Envelope"),
   attack_length_label("Attack Length", false),
   attack_level_label("Attack Level", false),
   fade_length_label("Fade Length", false),
   fade_level_label("Fade Level", false),
   angle_slider(0, 360), radius_slider(0, 10), azimuth_slider(180, 360),
   coordinates_label("Coordinates"),
   angle_label("Angle", false), 
   radius_label("Radius", false), 
   azimuth_label("Azimuth", false),
   level_slider(5, 10), 
   constant_effect_label("Constant Effect"),
   level_label("Level", false),
   start_level_slider(3, 10), end_level_slider(7, 10), 
   ramp_effect_label("Ramp Effect"),
   start_level_label("Start Lvl.", false),
   end_level_label("End Lvl.", false),
   right_saturation_slider(5, 10) , right_coeff_slider(5, 10),
   left_saturation_slider(5, 10)  , left_coeff_slider(5, 10),
   deadband_slider(1, 10)         , center_slider(5,10),
   right_saturation_label("Right Saturation", false), 
   right_coeff_label("Right Coefficient", false),
   left_saturation_label("Left Saturation", false), 
   left_coeff_label("Left Coefficient", false),
   condition_effect_label("Condition Effect"),
   deadband_label("Deadband", false), 
   center_label("Center", false),
   period_slider(1, 10), magnitude_slider(5, 10), 
   offset_slider(0, 10), phase_slider(0, 10),
   periodic_effect_label("Periodic Effect"), 
   period_label("Period", false), 
   magnitude_label("Magnitude", false), 
   offset_label("Offset", false), 
   phase_label("Phase", false),
   strong_magnitude_slider(5, 10), 
   weak_magnitude_slider(5, 10),
   rumble_effect_label("Rumble effect"),
   strong_magnitude_label("Strong Magnitude", false), 
   weak_magnitude_label("Weak Magnitude", false),
   gain_slider(10, 10),
   gain_label("Gain"),
   message_label("Ready.", false),
   message_label_label("Status", false),
   play_button(this),
   stop_button(this),
   last_haptic(NULL),
   show_haptic(NULL)
{
  for (int i = 0; i < num_haptics; i++) {
    device_list.append_item(haptics[i].name);
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
   
   d.add(replay_label , 0, 11 , 7, 1);
   d.add(length_label , 0, 12 , 2, 1);
   d.add(length_slider, 2, 12 , 5, 1);
   d.add(delay_label  , 0, 13 , 2, 1);
   d.add(delay_slider , 2, 13 , 5, 1);
   
   d.add(loops_label            ,  7, 11, 7, 1);
   d.add(loops_slider           ,  7, 12, 6, 1);
   d.add(gain_label             , 13, 11, 7, 1);
   d.add(gain_slider            , 13, 12, 7, 1);
 
   
   d.add(envelope_label         , 0, 15, 9, 1);
   d.add(attack_length_label    , 0, 16, 3, 1);
   d.add(attack_length_slider   , 4, 16, 6, 1);
   d.add(attack_level_label     , 0, 17, 3, 1);
   d.add(attack_level_slider    , 4, 17, 6, 1);
   d.add(fade_length_label      , 0, 18, 3, 1);
   d.add(fade_length_slider     , 4, 18, 6, 1);
   d.add(fade_level_label       , 0, 19, 3, 1);
   d.add(fade_level_slider      , 4, 19, 6, 1);
   
   d.add(coordinates_label      , 11, 15, 9, 1);
   d.add(angle_label            , 11, 16, 2, 1);
   d.add(angle_slider           , 13, 16, 7, 1);
   d.add(radius_label           , 11, 17, 2, 1);
   d.add(radius_slider          , 13, 17, 7, 1);
   d.add(azimuth_label          , 11, 18, 2, 1);
   d.add(azimuth_slider         , 13, 18, 7, 1);
  
   
   d.add(condition_effect_label ,  0, 21, 9, 1); 
   d.add(right_coeff_label      ,  0, 22, 4, 1);
   d.add(right_coeff_slider     ,  4, 22, 6, 1);
   d.add(right_saturation_label ,  0, 23, 4, 1);
   d.add(right_saturation_slider,  4, 23, 6, 1);  
   d.add(left_coeff_label       ,  0, 24, 4, 1);
   d.add(left_coeff_slider      ,  4, 24, 6, 1);
   d.add(left_saturation_label  ,  0, 25, 4, 1);
   d.add(left_saturation_slider ,  4, 25, 6, 1);
   d.add(deadband_label         ,  0, 26, 4, 1);
   d.add(deadband_slider        ,  4, 26, 6, 1);
   d.add(center_label           ,  0, 27, 4, 1);
   d.add(center_slider          ,  4, 27, 6, 1);
   

   
   d.add(periodic_effect_label  , 11, 21, 9, 1);
   d.add(period_label           , 11, 22, 2, 1);
   d.add(period_slider          , 13, 22, 7, 1);
   d.add(magnitude_label        , 11, 23, 2, 1);
   d.add(magnitude_slider       , 13, 23, 7, 1);
   d.add(offset_label           , 11, 24, 2, 1);
   d.add(offset_slider          , 13, 24, 7, 1);
   d.add(phase_label            , 11, 25, 2, 1);
   d.add(phase_slider           , 13, 25, 7, 1);
   
   d.add(ramp_effect_label      , 11, 29, 9, 1); 
   d.add(start_level_label      , 11, 30, 2, 1);
   d.add(start_level_slider     , 13, 30, 7, 1);
   d.add(end_level_label        , 11, 31, 2, 1);
   d.add(end_level_slider       , 13, 31, 7, 1);  
   
   d.add(rumble_effect_label    ,  0, 29, 9, 1);
   d.add(strong_magnitude_label ,  0, 30, 4, 1);
   d.add(strong_magnitude_slider,  4, 30, 6, 1);
   d.add(weak_magnitude_label   ,  0, 31, 4, 1);
   d.add(weak_magnitude_slider  ,  4, 31, 6, 1);
   
   d.add(constant_effect_label  ,  0, 33, 9, 1);
   d.add(level_label            ,  0, 34, 3, 1);
   d.add(level_slider           ,  4, 34, 6, 1);
   
   d.add(message_label_label    ,  0, 36,  2, 1);
   d.add(message_label          ,  2, 36, 12, 1);
   
  
   d.add(play_button  , 6, 38,  3, 2); 
   d.add(stop_button  , 12, 38, 3, 2); 

   
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

#define TEST_CAP(CAP, FLAG) (((CAP) & (FLAG)) == (FLAG))

void Prog::update() {
  /* Update playing state and display. */  
  if(last_haptic && last_haptic->playing) {
    if(!al_is_haptic_effect_playing(&last_haptic->id)) {
      last_haptic->playing = false;
      al_release_haptic_effect(&last_haptic->id);
      message_label.set_text("Done.");
      play_button.set_disabled(false);
      d.request_draw();
      log_printf("Play done on %s\n", last_haptic->name);
    }
  }
  /* Update availability of controls based on capabilities. */
  int devno = device_list.get_cur_value();
  Haptic * dev = haptics + devno;
  if (dev && dev->haptic) {
    if ( dev != show_haptic) {
      /* Take a deep breath , here we go...*/
      bool condition, envelope, periodic;
      show_haptic = dev;
      int cap = al_get_haptic_capabilities(show_haptic->haptic);
      
      /* Gain capability */
      gain_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_GAIN));
      gain_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_GAIN));
      
      
      /* Envelope related capabilities and sliders. */
      envelope = TEST_CAP(cap, ALLEGRO_HAPTIC_PERIODIC) ||
                 TEST_CAP(cap, ALLEGRO_HAPTIC_CONSTANT) ||
                 TEST_CAP(cap, ALLEGRO_HAPTIC_RAMP);
      
      envelope_label.set_disabled(!envelope); 
      attack_level_slider.set_disabled(!envelope);
      attack_length_slider.set_disabled(!envelope);
      fade_level_slider.set_disabled(!envelope);
      fade_length_slider.set_disabled(!envelope);
      attack_level_label.set_disabled(!envelope);
      attack_length_label.set_disabled(!envelope);
      fade_level_label.set_disabled(!envelope);
      fade_length_label.set_disabled(!envelope);
      
      /* Coordinate related capabilities. */ 
      angle_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_ANGLE));
      angle_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_ANGLE));
      radius_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RADIUS));
      radius_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RADIUS));      
      azimuth_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_AZIMUTH));      
      azimuth_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_AZIMUTH)); 
      
      /* Condition effect related capabilities. */
      condition  = TEST_CAP(cap, ALLEGRO_HAPTIC_DAMPER) ||
                 TEST_CAP(cap, ALLEGRO_HAPTIC_FRICTION) ||
                 TEST_CAP(cap, ALLEGRO_HAPTIC_INERTIA)  ||
                 TEST_CAP(cap, ALLEGRO_HAPTIC_SPRING);
      
      condition_effect_label.set_disabled(!condition);
      right_coeff_slider.set_disabled(!condition);
      left_coeff_slider.set_disabled(!condition);
      right_saturation_slider.set_disabled(!condition);
      left_saturation_slider.set_disabled(!condition);
      center_slider.set_disabled(!condition);
      deadband_slider.set_disabled(!condition);
      right_coeff_label.set_disabled(!condition);
      left_coeff_label.set_disabled(!condition);
      right_saturation_label.set_disabled(!condition);
      left_saturation_label.set_disabled(!condition);
      center_label.set_disabled(!condition);
      deadband_label.set_disabled(!condition);
      
      /* Constant effect related capabilities. */
      constant_effect_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_CONSTANT)); 
      level_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_CONSTANT));
      level_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_CONSTANT)); 
      
      /* Ramp effect capabilities. */
      ramp_effect_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RAMP)); 
      start_level_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RAMP));
      start_level_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RAMP)); 
      end_level_slider.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RAMP));
      end_level_label.set_disabled(!TEST_CAP(cap, ALLEGRO_HAPTIC_RAMP)); 
      
      /* Period effect capabilities. */
      periodic = TEST_CAP(cap, ALLEGRO_HAPTIC_PERIODIC);
      waveform_label.set_disabled(!periodic);
      waveform_list.set_disabled(!periodic);
      periodic_effect_label.set_disabled(!periodic);
      period_slider.set_disabled(!periodic);
      magnitude_slider.set_disabled(!periodic);
      offset_slider.set_disabled(!periodic);
      phase_slider.set_disabled(!periodic);
      period_label.set_disabled(!periodic);
      magnitude_label.set_disabled(!periodic);
      offset_label.set_disabled(!periodic);
      phase_label.set_disabled(!periodic);
      
      /*Change list of supported effect types*/
      type_list.clear_items();
      for (int i = EX_HAPTIC2_START_TYPES; i < EX_HAPTIC2_END_TYPES; i ++) {
        CapacityName * cn = capname + i;
        if (TEST_CAP(cap, cn->value)) { 
          type_list.append_item(cn->name);
        }
      }
      
      /* Change list of supported wave form types. */
      waveform_list.clear_items();
      for (int i = EX_HAPTIC2_START_WAVES; i < EX_HAPTIC2_END_WAVES; i ++) {
        CapacityName * cn = capname + i;
        if (TEST_CAP(cap, cn->value)) { 
          waveform_list.append_item(cn->name);
        }
      }
      
    }
  } else {
    play_button.set_disabled(true);
    message_label.set_text("No Haptic Device.");
  }
  
}


void Prog::run()
{
   d.prepare();

   while (!d.is_quit_requested()) {
      update(); 
      if (d.is_draw_requested()) {        
         al_clear_to_color(al_map_rgb(128, 148, 168));
         d.draw();
         al_flip_display();
      }

      d.run_step(true);
   }
   on_stop(); /* Stop playing anything we were still playing. */
}

int cap_for_name(const std::string& name) {
  for (int i = 0; i < EX_HAPTIC2_END_WAVES; i++) {
    if (name == capname[i].name) {
      return capname[i].value;
    } 
  }
  return -1;
}

const char * cap_to_name(int cap) {
  for (int i = 0; i < EX_HAPTIC2_END_WAVES; i++) {
    if (cap == capname[i].value) {
      return capname[i].name;
    } 
  }
  return "unknown";
}


double slider_to_magnitude(const HSlider & slider) {
  double value = (double) slider.get_cur_value();
  double max   = (double) slider.get_max_value();
  return value / max;
}

double slider_to_duration(const HSlider & slider) {
  double value = (double) slider.get_cur_value();
  double max   = 1.0;
  return value / max;
}

double slider_to_angle(const HSlider & slider) {
  double value = (double) slider.get_cur_value();
  double max   = (double) slider.get_max_value();
  return value / max;
}

void Prog::get_envelope(ALLEGRO_HAPTIC_ENVELOPE * envelope) {
  if(!envelope) return;
  envelope->attack_length = slider_to_duration(attack_length_slider);
  envelope->fade_length   = slider_to_duration(fade_length_slider);
  envelope->attack_level  = slider_to_magnitude(attack_level_slider);
  envelope->fade_level    = slider_to_magnitude(fade_level_slider);
} 

void Prog::on_play() {
  int devno  = device_list.get_cur_value();
  if ((devno < 0) || (devno >= num_haptics)) {
    message_label.set_text("No Haptic Device!");
    log_printf("No such device: %d\n", devno); 
    return;
  }  
  Haptic * haptic     = haptics + devno;  
  
  if (!haptic || !haptic->haptic) {
    log_printf("Device is NULL: %d\n", devno);
    message_label.set_text("Device Is NULL!");
    return;
  }  
  
  if (!al_get_haptic_active(haptic->haptic)) {
    message_label.set_text("Device Not Active!");
    log_printf("Device is not active: %d\n", devno); 
    return;
  }
  
  /* Stop playing previous effect. */
  if (haptic->playing) { 
    al_stop_haptic_effect(&haptic->id);
    haptic->playing = false;
    al_release_haptic_effect(&haptic->id);
  }
  
  /* First set gain. */
  double gain         = slider_to_magnitude(gain_slider);
  al_set_haptic_gain(haptic->haptic, gain);
  
  /* Now fill in the effect struct. */
  int type            = cap_for_name(type_list.get_selected_item_text());
  int wavetype        = cap_for_name (waveform_list.get_selected_item_text());
  
  if (type < 0)  {
    message_label.set_text("Unknown Effect Type!");
    log_printf("Unknown effect type: %d on %s\n", type, haptic->name); 
    return;
  }
  
  if (wavetype < 0)  {
    message_label.set_text("Unknown Wave Form!");
    log_printf("Unknown wave type: %d on %s\n", wavetype, haptic->name); 
    return;
  }
  
  haptic->effect.type               = type;
  haptic->effect.replay.delay       = slider_to_duration(delay_slider);
  haptic->effect.replay.length      = slider_to_duration(length_slider);
  int loops                         = loops_slider.get_cur_value();
  haptic->effect.direction.angle    = slider_to_angle(angle_slider);
  haptic->effect.direction.radius   = slider_to_magnitude(angle_slider);
  haptic->effect.direction.azimuth  = slider_to_angle(angle_slider);  
  
  switch (type) {
  case ALLEGRO_HAPTIC_RUMBLE:
     haptic->effect.data.rumble.strong_magnitude = 
        slider_to_magnitude(strong_magnitude_slider);
     haptic->effect.data.rumble.weak_magnitude = 
        slider_to_magnitude(weak_magnitude_slider);    
  break;
  case ALLEGRO_HAPTIC_PERIODIC:
    get_envelope(&haptic->effect.data.periodic.envelope);
    haptic->effect.data.periodic.waveform   = wavetype;
    haptic->effect.data.periodic.magnitude  = slider_to_magnitude(magnitude_slider);
    haptic->effect.data.periodic.period     = slider_to_duration(period_slider);
    haptic->effect.data.periodic.offset     = slider_to_duration(offset_slider);
    haptic->effect.data.periodic.phase      = slider_to_duration(phase_slider);
    haptic->effect.data.periodic.custom_len = 0;
    haptic->effect.data.periodic.custom_data= NULL;
  break;
  case ALLEGRO_HAPTIC_CONSTANT:
    get_envelope(&haptic->effect.data.constant.envelope);
    haptic->effect.data.constant.level    = slider_to_magnitude(level_slider);
  break;
  case ALLEGRO_HAPTIC_RAMP:
    get_envelope(&haptic->effect.data.ramp.envelope);
    haptic->effect.data.ramp.start_level  = slider_to_magnitude(start_level_slider);
    haptic->effect.data.ramp.end_level    = slider_to_magnitude(end_level_slider);
  break; 
  
  case ALLEGRO_HAPTIC_SPRING:
  case ALLEGRO_HAPTIC_FRICTION:
  case ALLEGRO_HAPTIC_DAMPER:
  case ALLEGRO_HAPTIC_INERTIA: /* fall through. */
     haptic->effect.data.condition.right_saturation = 
        slider_to_magnitude(right_saturation_slider);
     haptic->effect.data.condition.left_saturation = 
        slider_to_magnitude(left_saturation_slider);   
     haptic->effect.data.condition.right_coeff = 
        slider_to_magnitude(right_coeff_slider);   
     haptic->effect.data.condition.left_coeff = 
        slider_to_magnitude(left_coeff_slider);
     haptic->effect.data.condition.deadband = 
        slider_to_magnitude(deadband_slider);
     haptic->effect.data.condition.center = 
        slider_to_magnitude(center_slider);
        /* XXX, need a different conversion function here, but  I don't have a 
         * controller that supports condition effects anyway... :p
         */
     break;
  default:
    message_label.set_text("Unknown Effect Type!");
    log_printf("Unknown effect type %d %d\n", devno, type); 
    return;
  }
  if(!al_is_haptic_effect_ok(haptic->haptic, &haptic->effect)) {
    message_label.set_text("Effect Not Supported!");
    log_printf("Playing of effect type %s on %s not supported\n", cap_to_name(type), haptic->name); 
    return;
  }
  
  haptic->playing = al_upload_and_play_haptic_effect(haptic->haptic, &haptic->effect, 
                                                     loops, &haptic->id);
  if(haptic->playing) { 
    message_label.set_text("Playing...");
    log_printf("Started playing effect type %s on %s\n", cap_to_name(type), haptic->name); 
    last_haptic = haptic;
  } else {
    message_label.set_text("Playing of effect failed!");
    log_printf("Playing of effect type %s on %s failed\n", cap_to_name(type), haptic->name); 
  }
  play_button.set_disabled(true);
  
}

void Prog::on_stop() {
  int devno = device_list.get_cur_value();
  if ((devno < 0) || (devno >= num_haptics)) {
    log_printf("No such device %d\n", devno); 
    return;
  }  
  Haptic * haptic = haptics + devno;
  if (haptic->playing && al_is_haptic_effect_playing(&haptic->id)) { 
    al_stop_haptic_effect(&haptic->id);
    haptic->playing = false;
    al_release_haptic_effect(&haptic->id);
    log_printf("Stopped device %d: %s\n", devno, haptic->name);
  }
  message_label.set_text("Stopped.");
  play_button.set_disabled(false);
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
     haptics[num_haptics].haptic  = al_get_haptic_from_display(al_get_current_display());
     if(haptics[num_haptics].haptic) { 
        haptics[num_haptics].name    = (const char *)"display";
        haptics[num_haptics].playing = false;
        num_haptics++;
     }
   }
   
   for (int i = 0; i < al_get_num_joysticks(); i++) { 
    ALLEGRO_JOYSTICK * joy = al_get_joystick(i);
    if(al_is_joystick_haptic(joy)) {
      haptics[num_haptics].haptic  = al_get_haptic_from_joystick(joy);
      if(haptics[num_haptics].haptic) { 
         haptics[num_haptics].name    = (const char *)al_get_joystick_name(joy);
         haptics[num_haptics].playing = false;
         num_haptics++;
      }
    }
   }

   /* Don't remove these braces. */
   {
      Theme theme(font);
      Prog prog(theme, display);
      prog.run();
   }
   
   for (int i = 0; i < num_haptics; i++) { 
      al_release_haptic(haptics[i].haptic); 
   }

   al_destroy_font(font);

   return 0;
}

/* vim: set sts=3 sw=3 et: */
