#ifndef __al_included_allegro5_allegro_ttf_h
#define __al_included_allegro5_allegro_ttf_h

#include "allegro5/allegro.h"
#include "allegro5/allegro_font.h"

#ifdef __cplusplus
   extern "C" {
#endif

#define ALLEGRO_OLF_NO_KERNING  1
#define ALLEGRO_OLF_MONOCHROME  2
#define ALLEGRO_OLF_NO_AUTOHINT 4

#if (defined ALLEGRO_MINGW32) || (defined ALLEGRO_MSVC) || (defined ALLEGRO_BCC32)
   #ifndef ALLEGRO_STATICLINK
      #ifdef ALLEGRO_OLF_SRC
         #define _ALLEGRO_OLF_DLL __declspec(dllexport)
      #else
         #define _ALLEGRO_OLF_DLL __declspec(dllimport)
      #endif
   #else
      #define _ALLEGRO_OLF_DLL
   #endif
#endif

#if defined ALLEGRO_MSVC
   #define ALLEGRO_OLF_FUNC(type, name, args)      _ALLEGRO_OLF_DLL type __cdecl name args
#elif defined ALLEGRO_MINGW32
   #define ALLEGRO_OLF_FUNC(type, name, args)      extern type name args
#elif defined ALLEGRO_BCC32
   #define ALLEGRO_OLF_FUNC(type, name, args)      extern _ALLEGRO_OLF_DLL type name args
#else
   #define ALLEGRO_OLF_FUNC      AL_FUNC
#endif

ALLEGRO_OLF_FUNC(ALLEGRO_FONT *, al_load_olf_font, (char const *filename, int size, int flags));
ALLEGRO_OLF_FUNC(ALLEGRO_FONT *, al_load_olf_font_f, (ALLEGRO_FILE *file, char const *filename, int size, int flags));
ALLEGRO_OLF_FUNC(ALLEGRO_FONT *, al_load_olf_font_stretch, (char const *filename, int w, int h, int flags));
ALLEGRO_OLF_FUNC(ALLEGRO_FONT *, al_load_olf_font_stretch_f, (ALLEGRO_FILE *file, char const *filename, int w, int h, int flags));
ALLEGRO_OLF_FUNC(bool, al_init_olf_addon, (void));
ALLEGRO_OLF_FUNC(void, al_shutdown_olf_addon, (void));
ALLEGRO_OLF_FUNC(uint32_t, al_get_allegro_olf_version, (void));

#ifdef __cplusplus
   }
#endif

#endif
