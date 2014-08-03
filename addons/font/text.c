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
 *      Text drawing routines.
 *
 *      By Shawn Hargreaves.
 *
 *      textout_justify() by Seymour Shlien.
 *
 *      Laurence Withers added the textout_ex() function family.
 *
 *      See readme.txt for copyright information.
 */


#include <math.h>
#include <ctype.h>
#include "allegro5/allegro.h"

#include "allegro5/allegro_font.h"
#include "allegro5/internal/aintern_dtor.h"
#include "allegro5/internal/aintern_system.h"

/* If you call this, you're probably making a mistake. */
/*
#define strlen(s)   __are_you_sure__
*/
/* Removed the above define since some compilers seem to use some
 * preprocessor magic when calling strcmp() that inserts a call to strlen.
 * There might be a better way to do this.
 */



/* Text usually looks best when aligned to pixels -
 * but if x is 0.5 it may very well end up at an integer
 * position if the current transformation scales by 2 or
 * translated x by 0.5. So we simply apply the transformation,
 * round to nearest integer, and backtransform that.
 */
static void align_to_integer_pixel_inner(
   ALLEGRO_TRANSFORM const *fwd,
   ALLEGRO_TRANSFORM const *inv,
   float *x, float *y)
{
   al_transform_coordinates(fwd, x, y);
   *x = floorf(*x + 0.5f);
   *y = floorf(*y + 0.5f);
   al_transform_coordinates(inv, x, y);
}

static void align_to_integer_pixel(float *x, float *y)
{
   ALLEGRO_TRANSFORM const *fwd;
   ALLEGRO_TRANSFORM inv;

   fwd = al_get_current_transform();
   al_copy_transform(&inv, fwd);
   al_invert_transform(&inv);
   align_to_integer_pixel_inner(fwd, &inv, x, y);
}



/* Function: al_draw_ustr
 */
void al_draw_ustr(const ALLEGRO_FONT *font,
   ALLEGRO_COLOR color, float x, float y, int flags,
   const ALLEGRO_USTR *ustr) 
{
   ASSERT(font);
   ASSERT(ustr);

   if (flags & ALLEGRO_ALIGN_CENTRE) {
      /* Use integer division to avoid introducing a fractional
       * component to an integer x value.
       */
      x -= font->vtable->text_length(font, ustr) / 2;
   }
   else if (flags & ALLEGRO_ALIGN_RIGHT) {
      x -= font->vtable->text_length(font, ustr);
   }

   if (flags & ALLEGRO_ALIGN_INTEGER)
      align_to_integer_pixel(&x, &y);

   font->vtable->render(font, color, ustr, x, y);
}



/* Function: al_draw_text
 */
void al_draw_text(const ALLEGRO_FONT *font,
   ALLEGRO_COLOR color, float x, float y, int flags,
   char const *text) 
{
   ALLEGRO_USTR_INFO info;
   ASSERT(text);
   al_draw_ustr(font, color, x, y, flags, al_ref_cstr(&info, text));
}



/* Function: al_draw_justified_ustr
 */
void al_draw_justified_ustr(const ALLEGRO_FONT *font,
   ALLEGRO_COLOR color, float x1, float x2,
   float y, float diff, int flags, const ALLEGRO_USTR *ustr)
{
   const char *whitespace = " \t\n\r";
   ALLEGRO_USTR_INFO word_info;
   const ALLEGRO_USTR *word;
   int pos1, pos2;
   int minlen;
   int num_words;
   int space;
   float fleft, finc;
   int advance;
   ALLEGRO_TRANSFORM const *fwd = NULL;
   ALLEGRO_TRANSFORM inv;

   ASSERT(font);

   /* count words and measure min length (without spaces) */ 
   num_words = 0;
   minlen = 0;
   pos1 = 0;
   for (;;) {
      pos1 = al_ustr_find_cset_cstr(ustr, pos1, whitespace);
      if (pos1 == -1)
         break;
      pos2 = al_ustr_find_set_cstr(ustr, pos1, whitespace);
      if (pos2 == -1)
         pos2 = al_ustr_size(ustr);

      word = al_ref_ustr(&word_info, ustr, pos1, pos2);
      minlen += font->vtable->text_length(font, word);
      num_words++;

      pos1 = pos2;
   }

   /* amount of room for space between words */
   space = x2 - x1 - minlen;

   if ((space <= 0) || (space > diff) || (num_words < 2)) {
      /* can't justify */
      if (flags & ALLEGRO_ALIGN_INTEGER)
         align_to_integer_pixel(&x1, &y);
      font->vtable->render(font, color, ustr, x1, y);
      return; 
   }

   /* distribute space left evenly between words */
   fleft = (float)x1;
   finc = (float)space / (float)(num_words-1);
   pos1 = 0;

   if (flags & ALLEGRO_ALIGN_INTEGER) {
      fwd = al_get_current_transform();
      al_copy_transform(&inv, fwd);
      al_invert_transform(&inv);
   }

   for (;;) {
      pos1 = al_ustr_find_cset_cstr(ustr, pos1, whitespace);
      if (pos1 == -1)
         break;
      pos2 = al_ustr_find_set_cstr(ustr, pos1, whitespace);
      if (pos2 == -1)
         pos2 = al_ustr_size(ustr);

      word = al_ref_ustr(&word_info, ustr, pos1, pos2);
      if (flags & ALLEGRO_ALIGN_INTEGER) {
         float drawx = fleft;
         float drawy = y;
         align_to_integer_pixel_inner(fwd, &inv, &drawx, &drawy);
         advance = font->vtable->render(font, color, word, drawx, drawy);
      }
      else {
         advance = font->vtable->render(font, color, word, fleft, y);
      }
      fleft += advance + finc;
      pos1 = pos2;
   }
}


/* Function: al_draw_justified_text
 */
void al_draw_justified_text(const ALLEGRO_FONT *font,
   ALLEGRO_COLOR color, float x1, float x2,
   float y, float diff, int flags, const char *text)
{
   ALLEGRO_USTR_INFO info;
   ASSERT(text);
   al_draw_justified_ustr(font, color, x1, x2, y, diff, flags,
      al_ref_cstr(&info, text));
}


/* Function: al_draw_textf
 */
void al_draw_textf(const ALLEGRO_FONT *font, ALLEGRO_COLOR color,
   float x, float y, int flags,
   const char *format, ...)
{
   ALLEGRO_USTR *buf;
   va_list ap;
   const char *s;
   ASSERT(font);
   ASSERT(format);

   /* Fast path for common case. */
   if (0 == strcmp(format, "%s")) {
      va_start(ap, format);
      s = va_arg(ap, const char *);
      al_draw_text(font, color, x, y, flags, s);
      va_end(ap);
      return;
   }

   va_start(ap, format);
   buf = al_ustr_new("");
   al_ustr_vappendf(buf, format, ap);
   va_end(ap);

   al_draw_text(font, color, x, y, flags, al_cstr(buf));

   al_ustr_free(buf);
}



/* Function: al_draw_justified_textf
 */
void al_draw_justified_textf(const ALLEGRO_FONT *f,
   ALLEGRO_COLOR color, float x1, float x2, float y,
   float diff, int flags, const char *format, ...)
{
   ALLEGRO_USTR *buf;
   va_list ap;
   ASSERT(f);
   ASSERT(format);

   va_start(ap, format);
   buf = al_ustr_new("");
   al_ustr_vappendf(buf, format, ap);
   va_end(ap);

   al_draw_justified_text(f, color, x1, x2, y, diff, flags,
      al_cstr(buf));

   al_ustr_free(buf);
}



/* Function: al_get_ustr_width
 */
int al_get_ustr_width(const ALLEGRO_FONT *f, ALLEGRO_USTR const *ustr)
{
   ASSERT(f);
   ASSERT(ustr);

   return f->vtable->text_length(f, ustr);
}



/* Function: al_get_text_width
 */
int al_get_text_width(const ALLEGRO_FONT *f, const char *str)
{
   ALLEGRO_USTR_INFO str_info;
   const ALLEGRO_USTR *ustr;
   ASSERT(f);
   ASSERT(str);

   ustr = al_ref_cstr(&str_info, str);

   return f->vtable->text_length(f, ustr);
}



/* Function: al_get_font_line_height
 */
int al_get_font_line_height(const ALLEGRO_FONT *f)
{
   ASSERT(f);
   return f->vtable->font_height(f);
}



/* Function: al_get_font_ascent
 */
int al_get_font_ascent(const ALLEGRO_FONT *f)
{
   ASSERT(f);
   return f->vtable->font_ascent(f);
}



/* Function: al_get_font_descent
 */
int al_get_font_descent(const ALLEGRO_FONT *f)
{
   ASSERT(f);
   return f->vtable->font_descent(f);
}



/* Function: al_get_ustr_dimensions
 */
void al_get_ustr_dimensions(const ALLEGRO_FONT *f,
   ALLEGRO_USTR const *ustr,
   int *bbx, int *bby, int *bbw, int *bbh)
{
   ASSERT(f);
   ASSERT(ustr);
   f->vtable->get_text_dimensions(f, ustr, bbx, bby,
      bbw, bbh);
}



/* Function: al_get_text_dimensions
 */
void al_get_text_dimensions(const ALLEGRO_FONT *f,
   char const *text,
   int *bbx, int *bby, int *bbw, int *bbh)
{
   ALLEGRO_USTR_INFO info;
   ASSERT(f);
   ASSERT(text);

   f->vtable->get_text_dimensions(f, al_ref_cstr(&info, text), bbx, bby,
      bbw, bbh);
}



/* Function: al_destroy_font
 */
void al_destroy_font(ALLEGRO_FONT *f)
{
   if (!f)
      return;

   _al_unregister_destructor(_al_dtor_list, f);

   f->vtable->destroy(f);
}


/* Function: al_get_font_ranges
 */
int al_get_font_ranges(ALLEGRO_FONT *f, int ranges_count, int *ranges)
{
   return f->vtable->get_font_ranges(f, ranges_count, ranges);
}


/* Helper function that gets a word, that is a non-blank sequence separated by
 * blanks from an ALLEGRO_USTR starting at start using the passed in info for
 * storage. */

static const ALLEGRO_USTR *
get_ustr_word(const ALLEGRO_USTR *ustr,
              ALLEGRO_USTR_INFO *info, int start, int * end )
{
   int pos = start;
   int ch;
   while ((ch = (int) al_ustr_get_next(ustr, &pos)) > 0) {
      if (isspace(ch)) break;      
   }
   *end = pos;
   return al_ref_ustr(info, ustr, start, pos);
}

/** Helper function that returns the last character of ustr */
static int32_t ustr_last(const ALLEGRO_USTR * ustr) {
   int length     = al_ustr_length(ustr);
   return al_ustr_prev_get(ustr, &length);
}

/** Helper function that chomps off the last character of ustr if it
 * is a blank. 
 **/
static const ALLEGRO_USTR *
ustr_chomp(const ALLEGRO_USTR * ustr, ALLEGRO_USTR_INFO * info)
{
   int length     = al_ustr_length(ustr);
   int32_t last   = al_ustr_prev_get(ustr, &length);
   if (isspace(last)) {
      return al_ref_ustr(info, ustr, 0, length);
   } else {
      return ustr;
   }
} 


/* Function: al_draw_multiline_ustr
 */
int al_draw_multiline_ustr(const ALLEGRO_FONT *font,
     ALLEGRO_COLOR color, float x, float y, float w, int flags,
     const ALLEGRO_USTR *ustr)
{
   int prev;
   int stop;
   ALLEGRO_USTR_INFO word_info, chomp_info, line_info, chomp_line_info;
   const ALLEGRO_USTR * word, * chomp_word, * line;
   bool end_eol, hard_break = false;
   int line_start = 0, line_stop = 0, line_w = 0;
   int pos = 0;
   int line_h = al_get_font_line_height(font);
   int lines = 1;
   int word_w, chomp_word_w, word_end;
   (void) flags;
   stop = al_ustr_size(ustr);
   while (pos < stop) {
      word   = get_ustr_word(ustr, &word_info, pos, &word_end);
      word_w = al_get_ustr_width(font, word);
      prev   = ustr_last(word);
      end_eol= (prev == '\n');
      chomp_word = ustr_chomp(word, &chomp_info);

      /* Check if word will overflow the line. */
      chomp_word_w = al_get_ustr_width(font, chomp_word);
      
      if ((w < (chomp_word_w + line_w)) || hard_break ) {
         /* Line is full, draw it and skip to new line. */
         line = al_ref_ustr(&line_info, ustr, line_start, line_stop);
         /* But the last character may need to be chomped off
          * and not dran (space, new line, etc) */
         line = ustr_chomp(line, &chomp_line_info);
            
         al_draw_ustr(font, color, x, y, flags, line);
         line_start = line_stop;
         
         
         y += line_h;
         lines++;
         line_w = 0;
      }
      line_stop += al_ustr_length(word);
      line_w    += word_w;
      pos        = word_end;
      /* break the line next time round if this word ended with an eol. */
      hard_break = end_eol;  
   }
   /* Draw the last, pending line. */
   line = al_ref_ustr(&line_info, ustr, line_start, line_stop);
   al_draw_ustr(font, color, x, y, flags, line);
         
   
   return lines;
}
 



/* vim: set sts=3 sw=3 et: */
