/*
 *    Example program for the Allegro library, by Peter Wang.
 *
 *    Test multi line text routines.
 */

#include <string>
#include "allegro5/allegro.h"
#include "allegro5/allegro_font.h"
#include "allegro5/allegro_image.h"
#include "allegro5/allegro_ttf.h"
#include <allegro5/allegro_primitives.h>
#include "nihgui.hpp"

#include "common.c"

ALLEGRO_FONT *font;
ALLEGRO_FONT *font_gui;

class Prog {
private:
   Dialog d;
   Label text_label;
   Label width_label;
   Label align_label;
   TextEntry text_entry;
   HSlider width_slider;
   List text_align;

public:
   Prog(const Theme & theme, ALLEGRO_DISPLAY *display);
   void run();
   void draw_text();
};

Prog::Prog(const Theme & theme, ALLEGRO_DISPLAY *display) :
   d(Dialog(theme, display, 10, 20)),
   text_label(Label("Text")),
   width_label(Label("Width")),
   align_label(Label("Align")),
   text_entry(TextEntry("This is multi line text output with a\nhard break,\n\ntwice even!")),
   width_slider(HSlider(200, al_get_display_width(display))),
   text_align(List(0))
{
   text_align.append_item("Left");
   text_align.append_item("Center");
   text_align.append_item("Right");

   d.add(text_label, 0, 15, 1, 1);
   d.add(text_entry, 1, 15, 8, 1);

   d.add(width_label,  0, 16, 1, 1);
   d.add(width_slider, 1, 16, 8, 1);
   
   d.add(align_label,  0, 17, 1, 1);
   d.add(text_align ,  1, 17, 1, 3); 
    
}

void Prog::run()
{
   d.prepare();

   while (!d.is_quit_requested()) {
      if (d.is_draw_requested()) {
         al_clear_to_color(al_map_rgb(128, 128, 128));
         draw_text();
         d.draw();
         al_flip_display();
      }

      d.run_step(true);
   }
}

void Prog::draw_text()
{
   int lines;
   int x  = 10, y  = 10, tx = 10;
   int w = width_slider.get_cur_value();   
   int th = al_get_font_line_height(font);
   int flags = 0;
   ALLEGRO_USTR_INFO info;
   const ALLEGRO_USTR * ustr = al_ref_cstr(&info, text_entry.get_text());
   if (text_align.get_selected_item_text() == "Left") {
      flags = ALLEGRO_ALIGN_LEFT | ALLEGRO_ALIGN_INTEGER;
   } else if (text_align.get_selected_item_text() == "Center") {
      flags = ALLEGRO_ALIGN_CENTER | ALLEGRO_ALIGN_INTEGER;
      tx = 10 + w / 2;
   } else if (text_align.get_selected_item_text() == "Right") {
      flags = ALLEGRO_ALIGN_RIGHT | ALLEGRO_ALIGN_INTEGER;
      tx = 10 + w;
   }

   lines = al_draw_multiline_ustr(font, al_map_rgb_f(1, 1, 1), tx, y, w,
      flags, (ALLEGRO_USTR *) ustr);

   al_draw_rectangle(x, y,  x + w, y + lines*th, al_map_rgb(0, 0, 255), 0);
}

int main(int argc, char *argv[])
{
   ALLEGRO_DISPLAY *display;

   (void)argc;
   (void)argv;

   if (!al_init()) {
      abort_example("Could not init Allegro\n");
   }
   al_init_primitives_addon();
   al_install_keyboard();
   al_install_mouse();

   al_init_image_addon();
   al_init_font_addon();
   al_init_ttf_addon();
   init_platform_specific();

   al_set_new_display_flags(ALLEGRO_GENERATE_EXPOSE_EVENTS);
   display = al_create_display(640, 480);
   if (!display) {
      abort_example("Unable to create display\n");
   }

   /* Test TTF fonts or bitmap fonts. */
#if 1
   font = al_load_font("data/DejaVuSans.ttf", 24, 0);
   if (!font) {
      abort_example("Failed to load data/DejaVuSans.ttf\n");
   }
#else
   font = al_load_font("data/font.tga", 0, 0);
   if (!font) {
      abort_example("Failed to load data/font.tga\n");
   }
#endif

   font_gui = al_load_font("data/DejaVuSans.ttf", 14, 0);
   if (!font_gui) {
      abort_example("Failed to load data/DejaVuSans.ttf\n");
   }

   /* Don't remove these braces. */
   {
      Theme theme(font_gui);
      Prog prog(theme, display);
      prog.run();
   }

   al_destroy_font(font);
   al_destroy_font(font_gui);

   return 0;
}

/* vim: set sts=3 sw=3 et: */
