#include "allegro5/allegro.h"
#ifdef ALLEGRO_CFG_OPENGL
#include "allegro5/allegro_opengl.h"
#endif

#include "allegro5/allegro_primitives.h"
#include "allegro5/internal/aintern.h"
#include "allegro5/internal/aintern_vector.h"

#include "allegro5/allegro_olf.h"
#include "allegro5/internal/aintern_olf_cfg.h"
#include "allegro5/internal/aintern_dtor.h"
#include "allegro5/internal/aintern_system.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdlib.h>

ALLEGRO_DEBUG_CHANNEL("font")

#define RANGE_SIZE 512


typedef struct REGION
{
   short x;
   short y;
   short w;
   short h;
} REGION;


typedef struct ALLEGRO_OLF_GLYPH_DATA
{
   FT_Face face;
   REGION region;
   short offset_x;
   short offset_y;
   short advance;
} ALLEGRO_OLF_GLYPH_DATA;


typedef struct ALLEGRO_OLF_GLYPH_RANGE
{
   int32_t range_start;
   ALLEGRO_OLF_GLYPH_DATA *glyphs;  /* [RANGE_SIZE] */
} ALLEGRO_OLF_GLYPH_RANGE;


typedef struct ALLEGRO_OLF_FONT_DATA
{
   FT_Face face;
   int flags;
   _AL_VECTOR glyph_ranges;  /* sorted array of of ALLEGRO_OLF_GLYPH_RANGE */

   int page_pos_x;
   int page_pos_y;
   int page_line_height;
   REGION lock_rect;
   ALLEGRO_LOCKED_REGION *page_lr;

   FT_StreamRec stream;
   ALLEGRO_FILE *file;
   unsigned long base_offset;
   unsigned long offset;

   int bitmap_format;
   int bitmap_flags;
} ALLEGRO_OLF_FONT_DATA;


/* globals */
static bool olf_inited;
static FT_Library ft;
static ALLEGRO_FONT_VTABLE vt;


static INLINE int align4(int x)
{
#ifdef ALIGN_TO_4_PIXEL
   return (x + 3) & ~3;
#else
   return x;
#endif
}


static ALLEGRO_OLF_GLYPH_DATA *get_glyph(ALLEGRO_OLF_FONT_DATA *data,
   int ft_index)
{
   ALLEGRO_OLF_GLYPH_RANGE *range;
   int32_t range_start;
   int lo, hi, mid;

   range_start = ft_index - (ft_index % RANGE_SIZE);

   /* Binary search for the range. */
   lo = 0;
   hi = _al_vector_size(&data->glyph_ranges);
   mid = (hi + lo)/2;
   range = NULL;

   while (lo < hi) {
      ALLEGRO_OLF_GLYPH_RANGE *r = _al_vector_ref(&data->glyph_ranges, mid);
      if (r->range_start == range_start) {
         range = r;
         break;
      }
      else if (r->range_start < range_start) {
         lo = mid + 1;
      }
      else {
         hi = mid;
      }
      mid = (hi + lo)/2;
   }

   if (!range) {
      range = _al_vector_alloc_mid(&data->glyph_ranges, mid);
      range->range_start = range_start;
      range->glyphs = al_calloc(RANGE_SIZE, sizeof(ALLEGRO_OLF_GLYPH_DATA));
   }

   return &range->glyphs[ft_index - range_start];
}


static void unlock_current_page(ALLEGRO_OLF_FONT_DATA *data)
{
   if (data->page_lr) {
      data->page_lr = NULL;
   }
}


// FIXME: Add a special case for when a single glyph rendering won't fit
// into 256x256 pixels.
static ALLEGRO_BITMAP *push_new_page(ALLEGRO_OLF_FONT_DATA *data)
{
    ALLEGRO_BITMAP *page;
    ALLEGRO_STATE state;

    unlock_current_page(data);

    /* The bitmap will be destroyed when the parent font is destroyed so
     * it is not safe to register a destructor for it.
     */
    _al_push_destructor_owner();
    al_store_state(&state, ALLEGRO_STATE_NEW_BITMAP_PARAMETERS);
    al_set_new_bitmap_format(data->bitmap_format);
    al_set_new_bitmap_flags(data->bitmap_flags);
    page = al_create_bitmap(256, 256);
    al_restore_state(&state);
    _al_pop_destructor_owner();

    data->page_pos_x = 0;
    data->page_pos_y = 0;
    data->page_line_height = 0;

    return page;
}


static void copy_glyph_mono(ALLEGRO_OLF_FONT_DATA *font_data, FT_Face face,
   unsigned char *glyph_data)
{
   int pitch = font_data->page_lr->pitch;
   int x, y;

   for (y = 0; y < face->glyph->bitmap.rows; y++) {
      unsigned char const *ptr = face->glyph->bitmap.buffer + face->glyph->bitmap.pitch * y;
      unsigned char *dptr = glyph_data + pitch * y;
      int bit = 0;

      if (font_data->flags & ALLEGRO_NO_PREMULTIPLIED_ALPHA) {
         for (x = 0; x < face->glyph->bitmap.width; x++) {
            unsigned char set = ((*ptr >> (7-bit)) & 1) ? 255 : 0;
            *dptr++ = 255;
            *dptr++ = 255;
            *dptr++ = 255;
            *dptr++ = set;
            bit = (bit + 1) & 7;
            if (bit == 0) {
               ptr++;
            }
         }
      }
      else {
         for (x = 0; x < face->glyph->bitmap.width; x++) {
            unsigned char set = ((*ptr >> (7-bit)) & 1) ? 255 : 0;
            *dptr++ = set;
            *dptr++ = set;
            *dptr++ = set;
            *dptr++ = set;
            bit = (bit + 1) & 7;
            if (bit == 0) {
               ptr++;
            }
         }
      }
   }
}


static void copy_glyph_color(ALLEGRO_OLF_FONT_DATA *font_data, FT_Face face,
   unsigned char *glyph_data)
{
   int pitch = font_data->page_lr->pitch;
   int x, y;

   for (y = 0; y < face->glyph->bitmap.rows; y++) {
      unsigned char const *ptr = face->glyph->bitmap.buffer + face->glyph->bitmap.pitch * y;
      unsigned char *dptr = glyph_data + pitch * y;

      if (font_data->flags & ALLEGRO_NO_PREMULTIPLIED_ALPHA) {
         for (x = 0; x < face->glyph->bitmap.width; x++) {
            unsigned char c = *ptr;
            *dptr++ = 255;
            *dptr++ = 255;
            *dptr++ = 255;
            *dptr++ = c;
            ptr++;
         }
      }
      else {
         for (x = 0; x < face->glyph->bitmap.width; x++) {
            unsigned char c = *ptr;
            *dptr++ = c;
            *dptr++ = c;
            *dptr++ = c;
            *dptr++ = c;
            ptr++;
         }
      }
   }
}


/* NOTE: this function may disable the bitmap hold drawing state
 * and leave the current page bitmap locked.
 */
static void cache_glyph(ALLEGRO_OLF_FONT_DATA *font_data, FT_Face face,
   int ft_index, ALLEGRO_OLF_GLYPH_DATA *glyph, bool lock_more)
{
    FT_Int32 ft_load_flags;
    FT_Error e;
    int w, h;
    unsigned char *glyph_data;

    if (glyph->region.x < 0)
        return;

    // FIXME: make this a config setting? FT_LOAD_FORCE_AUTOHINT

    // FIXME: Investigate why some fonts don't work without the
    // NO_BITMAP flags. Supposedly using that flag makes small sizes
    // look bad so ideally we would not used it.
    ft_load_flags = FT_LOAD_RENDER | FT_LOAD_NO_BITMAP;
    if (font_data->flags & ALLEGRO_OLF_MONOCHROME)
       ft_load_flags |= FT_LOAD_TARGET_MONO;
    if (font_data->flags & ALLEGRO_OLF_NO_AUTOHINT)
       ft_load_flags |= FT_LOAD_NO_AUTOHINT;

    e = FT_Load_Glyph(face, ft_index, ft_load_flags);
    if (e) {
       ALLEGRO_WARN("Failed loading glyph %d from.\n", ft_index);
    }

    glyph->offset_x = face->glyph->bitmap_left;
    glyph->offset_y = (face->size->metrics.ascender >> 6) - face->glyph->bitmap_top;
    glyph->advance = face->glyph->advance.x >> 6;

    w = face->glyph->bitmap.width;
    h = face->glyph->bitmap.rows;

    if (w == 0 || h == 0) {
       /* Mark this glyph so we won't try to cache it next time. */
       glyph->region.x = -1;
       glyph->region.y = -1;
       ALLEGRO_DEBUG("Glyph %d has zero size.\n", ft_index);
       return;
    }

    glyph->face = face;

    if (!lock_more) {
       unlock_current_page(font_data);
    }
}


static int get_kerning(ALLEGRO_OLF_FONT_DATA const *data, FT_Face face,
   int prev_ft_index, int ft_index)
{
   /* Do kerning? */
   if (!(data->flags & ALLEGRO_OLF_NO_KERNING) && prev_ft_index != -1) {
      FT_Vector delta;
      FT_Get_Kerning(face, prev_ft_index, ft_index,
         FT_KERNING_DEFAULT, &delta);
      return delta.x >> 6;
   }

   return 0;
}

typedef struct OLF_DRAW {
   ALLEGRO_COLOR color;
   float dx;
   float dy;
   float x;
   float y;
   float thickness;
   float scale_x;
   float scale_y;
} OLF_DRAW;


#define olf_scale_x(value, draw) (draw->dx + value * draw->scale_x)
#define olf_scale_y(value, draw) (draw->dy + value * draw->scale_y)

/*
static float olf_scale_y(float value, OLF_DRAW * draw) {
   return draw->dy + value * draw->scale_y;
}
*/

static int olf_move_to(const FT_Vector *to, void *user)
{
   OLF_DRAW * draw = user;
   draw->x = olf_scale_x(to->x, draw);
   draw->y = olf_scale_y(to->y, draw);
   return 0;
}


static int olf_line_to(const FT_Vector *to, void *user)
{
   OLF_DRAW * draw = user;

   al_draw_line(draw->x, draw->y,
                olf_scale_x(to->x, draw), olf_scale_y(to->y, draw),
                draw->color, draw->thickness);
   draw->x = olf_scale_x(to->x, draw);
   draw->y = olf_scale_y(to->y, draw);
   return 0;
}


static int olf_conic_to(const FT_Vector *control, const FT_Vector *to, void *user)
{
   OLF_DRAW * draw = user;
   float points[8] =
   {
      draw->x, draw->y,
      olf_scale_x(control->x, draw), olf_scale_y(control->y, draw),
      olf_scale_x(control->x, draw), olf_scale_y(control->y, draw),
      olf_scale_x(to->x, draw), olf_scale_y(to->y, draw),
   };
   
   al_draw_spline(points, draw->color, draw->thickness);
   draw->x = olf_scale_x(to->x, draw);
   draw->y = olf_scale_y(to->y, draw);
   return 0;
}


static int olf_cubic_to(const FT_Vector *control1, const FT_Vector *control2,
                 const FT_Vector *to, void *user)
{
   OLF_DRAW * draw = user;
   float points[8] =
   {
      draw->x, draw->y,
      olf_scale_x(control1->x, draw), olf_scale_y(control1->y, draw),
      olf_scale_x(control2->x, draw), olf_scale_y(control2->y, draw),
      olf_scale_x(to->x, draw), olf_scale_y(to->y, draw),
   };
   
   al_draw_spline(points, draw->color, draw->thickness);
   draw->x = olf_scale_x(to->x, draw);
   draw->y = olf_scale_y(to->y, draw);
   return 0;
}



static void olf_draw_outline(FT_Outline *outline, ALLEGRO_COLOR color,
   float x, float y)
{
   FT_Outline_Funcs funcs;
   OLF_DRAW draw;
   ALLEGRO_TRANSFORM newtrans, *oldtrans;
   
   funcs.move_to  = olf_move_to;
   funcs.line_to  = olf_line_to;
   funcs.conic_to = olf_conic_to;
   funcs.cubic_to = olf_cubic_to;
   funcs.shift    = 1;
   funcs.delta    = 1;
   /* FT's coordinates are wonky! */

   draw.color     = color;
   draw.x         = 0;
   draw.y         = 0;
   draw.thickness = 0;
   draw.scale_x   = 1.0 / 128.0; /* (1 / 64)  == >> 6 */
   draw.scale_y   = -1.0 / 128.0;
   draw.dx        = x;
   draw.dy        = y;
   /**
    * Can't get these trabsforms to work, so scale by hand ... :p
   oldtrans = al_get_current_transform();
   al_copy_transform(&newtrans, oldtrans);
   al_scale_transform(&newtrans, 0.0015625, -0.0015625);
   al_translate_transform(&newtrans, x, y); 
   al_use_transform(&newtrans);
   */
    
   FT_Outline_Decompose(outline, &funcs, &draw);
   
   /* al_use_transform(oldtrans); */
}

/*
A contour that contains a single point only is represented by a ‘move to’
operation followed by ‘line to’ to the same point. In most cases,
it is best to filter this out before using the outline for stroking
purposes (otherwise it would result in a visible dot when round caps
are used).
*/

static int render_glyph(ALLEGRO_FONT const *f,
   ALLEGRO_COLOR color, int prev_ft_index, int ft_index,
   float xpos, float ypos)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;
   ALLEGRO_OLF_GLYPH_DATA *glyph = get_glyph(data, ft_index);
   int advance = 0;
   ALLEGRO_DISPLAY *display;
   ALLEGRO_TRANSFORM old_projection_transform;

   /* Workabout for bug 3484535 */
   display = al_get_current_display();
   if (display) {
      al_copy_transform(&old_projection_transform,
         al_get_projection_transform(display));
   }

   /* We don't try to cache all glyphs in a pre-pass before drawing them.
    * While that would indeed save us making separate texture uploads, it
    * implies two passes over a string even in the common case when all glyphs
    * are already cached.  This turns out to have an measureable impact on
    * performance.
    */
   cache_glyph(data, face, ft_index, glyph, false);

   /* Workabout for bug 3484535 */
   if (display) {
      al_set_projection_transform(display, &old_projection_transform);
   }

   advance += get_kerning(data, face, prev_ft_index, ft_index);

   if (glyph && glyph->face && glyph->face->glyph ) {
      /* draw the glyph outline here */
      int shift = al_get_font_ascent(f);
      olf_draw_outline(&glyph->face->glyph->outline, color, xpos, ypos + shift);
   }
   else if (glyph->region.x > 0) {
      ALLEGRO_ERROR("Glyph %d not on any page.\n", ft_index);
   }

   advance += glyph->advance;

   return advance;
}


static int olf_font_height(ALLEGRO_FONT const *f)
{
   ASSERT(f);
   return f->height;
}


static int olf_font_ascent(ALLEGRO_FONT const *f)
{
    ALLEGRO_OLF_FONT_DATA *data;
    FT_Face face;

    ASSERT(f);

    data = f->data;
    face = data->face;

    return face->size->metrics.ascender >> 6;
}


static int olf_font_descent(ALLEGRO_FONT const *f)
{
    ALLEGRO_OLF_FONT_DATA *data;
    FT_Face face;

    ASSERT(f);

    data = f->data;
    face = data->face;

    return (-face->size->metrics.descender) >> 6;
}


static int olf_render_char(ALLEGRO_FONT const *f, ALLEGRO_COLOR color,
   int ch, float xpos, float ypos)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;
   int advance = 0;
   int32_t ch32 = (int32_t) ch;
   
   int ft_index = FT_Get_Char_Index(face, ch32);
   advance = render_glyph(f, color, -1, ft_index, xpos, ypos);
   
   return advance;
}


static int olf_char_length(ALLEGRO_FONT const *f, int ch)
{
   int result;
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;   
   int ft_index = FT_Get_Char_Index(face, ch);
   ALLEGRO_OLF_GLYPH_DATA *glyph = get_glyph(data, ft_index);
   if (!glyph)
      return 0;
   cache_glyph(data, face, ft_index, glyph, true);
   result = glyph->region.w;
  
   unlock_current_page(data);
   return result;
}


static int olf_render(ALLEGRO_FONT const *f, ALLEGRO_COLOR color,
   const ALLEGRO_USTR *text, float x, float y)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;
   int pos = 0;
   int advance = 0;
   int prev_ft_index = -1;
   int32_t ch;
   bool hold;

   hold = al_is_bitmap_drawing_held();
   al_hold_bitmap_drawing(true);

   while ((ch = al_ustr_get_next(text, &pos)) >= 0) {
      int ft_index = FT_Get_Char_Index(face, ch);
      advance += render_glyph(f, color, prev_ft_index, ft_index,
         x + advance, y);
      prev_ft_index = ft_index;
   }

   al_hold_bitmap_drawing(hold);

   return advance;
}


static int olf_text_length(ALLEGRO_FONT const *f, const ALLEGRO_USTR *text)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;
   int pos = 0;
   int prev_ft_index = -1;
   int x = 0;
   int32_t ch;

   while ((ch = al_ustr_get_next(text, &pos)) >= 0) {
      int ft_index = FT_Get_Char_Index(face, ch);
      ALLEGRO_OLF_GLYPH_DATA *glyph = get_glyph(data, ft_index);

      cache_glyph(data, face, ft_index, glyph, true);

      x += get_kerning(data, face, prev_ft_index, ft_index);
      x += glyph->advance;

      prev_ft_index = ft_index;
   }

   unlock_current_page(data);

   return x;
}


static void olf_get_text_dimensions(ALLEGRO_FONT const *f,
   ALLEGRO_USTR const *text,
   int *bbx, int *bby, int *bbw, int *bbh)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;
   int end;
   int pos = 0;
   int prev_ft_index = -1;
   bool first = true;
   int x = 0;
   int32_t ch;

   end = al_ustr_size(text);
   *bbx = 0;

   while ((ch = al_ustr_get_next(text, &pos)) >= 0) {
      int ft_index = FT_Get_Char_Index(face, ch);
      ALLEGRO_OLF_GLYPH_DATA *glyph = get_glyph(data, ft_index);

      cache_glyph(data, face, ft_index, glyph, true);

      if (pos == end) {
         x += glyph->offset_x + glyph->region.w;
      }
      else {
         x += get_kerning(data, face, prev_ft_index, ft_index);
         x += glyph->advance;
      }

      if (first) {
         *bbx = glyph->offset_x;
         first = false;
      }

      prev_ft_index = ft_index;
   }

   *bby = 0; // FIXME
   *bbw = x - *bbx;
   *bbh = f->height; // FIXME, we want the bounding box!

   unlock_current_page(data);
}


static void olf_destroy(ALLEGRO_FONT *f)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   int i;

   unlock_current_page(data);

   FT_Done_Face(data->face);
   for (i = _al_vector_size(&data->glyph_ranges) - 1; i >= 0; i--) {
      ALLEGRO_OLF_GLYPH_RANGE *range = _al_vector_ref(&data->glyph_ranges, i);
      al_free(range->glyphs);
   }
   _al_vector_free(&data->glyph_ranges);
   al_free(data);
   al_free(f);
}


static unsigned long ftread(FT_Stream stream, unsigned long offset,
    unsigned char *buffer, unsigned long count)
{
    ALLEGRO_OLF_FONT_DATA *data = stream->pathname.pointer;
    unsigned long bytes;

    if (count == 0)
       return 0;

    if (offset != data->offset)
       al_fseek(data->file, data->base_offset + offset, ALLEGRO_SEEK_SET);
    bytes = al_fread(data->file, buffer, count);
    data->offset = offset + bytes;
    return bytes;
}


static void ftclose(FT_Stream  stream)
{
    ALLEGRO_OLF_FONT_DATA *data = stream->pathname.pointer;
    al_fclose(data->file);
    data->file = NULL;
}


/* Function: al_load_olf_font_f
 */
ALLEGRO_FONT *al_load_olf_font_f(ALLEGRO_FILE *file,
    char const *filename, int size, int flags)
{
    return al_load_olf_font_stretch_f(file, filename, 0, size, flags);
}


/* Function: al_load_olf_font_stretch_f
 */
ALLEGRO_FONT *al_load_olf_font_stretch_f(ALLEGRO_FILE *file,
    char const *filename, int w, int h, int flags)
{
    FT_Face face;
    ALLEGRO_OLF_FONT_DATA *data;
    ALLEGRO_FONT *f;
    ALLEGRO_PATH *path;
    FT_Open_Args args;
    int result;

    if ((h > 0 && w < 0) || (h < 0 && w > 0)) {
       ALLEGRO_ERROR("Height/width have opposite signs (w = %d, h = %d).\n", w, h);
       return NULL;
    }

    data = al_calloc(1, sizeof *data);
    data->stream.read = ftread;
    data->stream.close = ftclose;
    data->stream.pathname.pointer = data;
    data->base_offset = al_ftell(file);
    data->stream.size = al_fsize(file);
    data->file = file;
    data->bitmap_format = al_get_new_bitmap_format();
    data->bitmap_flags = al_get_new_bitmap_flags();

    memset(&args, 0, sizeof args);
    args.flags = FT_OPEN_STREAM;
    args.stream = &data->stream;

    if ((result = FT_Open_Face(ft, &args, 0, &face)) != 0) {
        ALLEGRO_ERROR("Reading %s failed. Freetype error code %d\n", filename,
      result);
        // Note: Freetype already closed the file for us.
        al_free(data);
        return NULL;
    }

    // FIXME: The below doesn't use Allegro's streaming.
    /* Small hack for Type1 fonts which store kerning information in
     * a separate file - and we try to guess the name of that file.
     */
    path = al_create_path(filename);
    if (!strcmp(al_get_path_extension(path), ".pfa")) {
        const char *helper;
        ALLEGRO_DEBUG("Type1 font assumed for %s.\n", filename);

        al_set_path_extension(path, ".afm");
        helper = al_path_cstr(path, '/');
        FT_Attach_File(face, helper);
        ALLEGRO_DEBUG("Guessed afm file %s.\n", helper);

        al_set_path_extension(path, ".tfm");
        helper = al_path_cstr(path, '/');
        FT_Attach_File(face, helper);
        ALLEGRO_DEBUG("Guessed tfm file %s.\n", helper);
    }
    al_destroy_path(path);

    if (h > 0) {
       FT_Set_Pixel_Sizes(face, w, h);
    }
    else {
       /* Set the "real dimension" of the font to be the passed size,
        * in pixels.
        */
       FT_Size_RequestRec req;
       ASSERT(w <= 0);
       ASSERT(h <= 0);
       req.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
       req.width = (-w) << 6;
       req.height = (-h) << 6;
       req.horiResolution = 0;
       req.vertResolution = 0;
       FT_Request_Size(face, &req);
    }

    ALLEGRO_DEBUG("Font %s loaded with pixel size %d x %d.\n", filename,
        w, h);
    ALLEGRO_DEBUG("    ascent=%.1f, descent=%.1f, height=%.1f\n",
        face->size->metrics.ascender / 64.0,
        face->size->metrics.descender / 64.0,
        face->size->metrics.height / 64.0);

    data->face = face;
    data->flags = flags;

    _al_vector_init(&data->glyph_ranges, sizeof(ALLEGRO_OLF_GLYPH_RANGE));

    f = al_malloc(sizeof *f);
    f->height = face->size->metrics.height >> 6;
    f->vtable = &vt;
    f->data = data;

    _al_register_destructor(_al_dtor_list, f,
       (void (*)(void *))al_destroy_font);

    return f;
}


/* Function: al_load_olf_font
 */
ALLEGRO_FONT *al_load_olf_font(char const *filename, int size, int flags)
{
   return al_load_olf_font_stretch(filename, 0, size, flags);
}


/* Function: al_load_olf_font_stretch
 */
ALLEGRO_FONT *al_load_olf_font_stretch(char const *filename, int w, int h,
   int flags)
{
   ALLEGRO_FILE *f;
   ALLEGRO_FONT *font;
   ASSERT(filename);

   f = al_fopen(filename, "rb");
   if (!f)
      return NULL;

   /* The file handle is owned by the function and the file is usually only
    * closed when the font is destroyed, in case Freetype has to load data
    * at a later time.
    */
   font = al_load_olf_font_stretch_f(f, filename, w, h, flags);

   return font;
}


static int olf_get_font_ranges(ALLEGRO_FONT *font, int ranges_count,
   int *ranges)
{
   ALLEGRO_OLF_FONT_DATA *data = font->data;
   FT_UInt g;
   FT_ULong unicode = FT_Get_First_Char(data->face, &g);
   int i = 0;
   if (i < ranges_count) {
      ranges[i * 2 + 0] = unicode;
      ranges[i * 2 + 1] = unicode;
   }
   while (g) {
      FT_ULong unicode2 = FT_Get_Next_Char(data->face, unicode, &g);
      if (unicode + 1 != unicode2) {
         if (i < ranges_count) {
            ranges[i * 2 + 1] = unicode;
            if (i + 1 < ranges_count) {
               ranges[(i + 1) * 2 + 0] = unicode2;
            }
         }
         i++;
      }
      if (i < ranges_count) {
         ranges[i * 2 + 1] = unicode2;
      }
      unicode = unicode2;
   }
   return i;
}

static bool olf_get_glyph_dimensions(ALLEGRO_FONT const *f,
   int codepoint,
   int *bbx, int *bby, int *bbw, int *bbh)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;   
   {
      int ft_index = FT_Get_Char_Index(face, codepoint);
      ALLEGRO_OLF_GLYPH_DATA *glyph = get_glyph(data, ft_index);
      if (!glyph) return false;
      cache_glyph(data, face, ft_index, glyph, true);
      *bbx = glyph->offset_x;
      *bbw = glyph->region.w;
      *bbh = glyph->region.h;
      *bby = glyph->offset_y;
   }

   unlock_current_page(data);
   return true;
}

static int olf_get_glyph_advance(ALLEGRO_FONT const *f, int codepoint1,
   int codepoint2)
{
   ALLEGRO_OLF_FONT_DATA *data = f->data;
   FT_Face face = data->face;
   int ft_index = FT_Get_Char_Index(face, codepoint1);
   ALLEGRO_OLF_GLYPH_DATA *glyph = get_glyph(data, ft_index);   
   int kerning = 0;
   int advance = 0;
   if (!glyph)
      return 0;
      
   cache_glyph(data, face, ft_index, glyph, true);
   
   if (codepoint2 != ALLEGRO_NO_KERNING) { 
      int ft_index1 = FT_Get_Char_Index(face, codepoint1);
      int ft_index2 = FT_Get_Char_Index(face, codepoint2); 
      kerning = get_kerning(data, face, ft_index1, ft_index2);
   }
   
   advance = glyph->advance;
   unlock_current_page(data);
   return advance + kerning;
}



/* Function: al_init_olf_addon
 */
bool al_init_olf_addon(void)
{
   if (olf_inited) {
      ALLEGRO_WARN("OLF addon already initialised.\n");
      return true;
   }

   FT_Init_FreeType(&ft);
   vt.font_height = olf_font_height;
   vt.font_ascent = olf_font_ascent;
   vt.font_descent = olf_font_descent;
   vt.char_length = olf_char_length;
   vt.text_length = olf_text_length;
   vt.render_char = olf_render_char;
   vt.render = olf_render;
   vt.destroy = olf_destroy;
   vt.get_text_dimensions = olf_get_text_dimensions;
   vt.get_font_ranges = olf_get_font_ranges;
   vt.get_glyph_dimensions = olf_get_glyph_dimensions;
   vt.get_glyph_advance = olf_get_glyph_advance;

   al_register_font_loader(".olf", al_load_olf_font);

   /* Can't fail right now - in the future we might dynamically load
    * the FreeType DLL here and/or initialize FreeType (which both
    * could fail and would cause a false return).
    */
   olf_inited = true;
   return olf_inited;
}


/* Function: al_shutdown_olf_addon
 */
void al_shutdown_olf_addon(void)
{
   if (!olf_inited) {
      ALLEGRO_ERROR("OLF addon not initialised.\n");
      return;
   }

   al_register_font_loader(".olf", NULL);

   FT_Done_FreeType(ft);

   olf_inited = false;
}


/* Function: al_get_allegro_olf_version
 */
uint32_t al_get_allegro_olf_version(void)
{
   return ALLEGRO_VERSION_INT;
}

/* vim: set sts=3 sw=3 et: */
