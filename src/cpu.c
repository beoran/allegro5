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
 *      CPU and hardware information.
 *
 *      By Beoran.
 *
 *      See readme.txt for copyright information.
 */

/* Title: Event sources
 */


#ifdef HAVE_SYSCONF
#include <unistd.h>
#endif

#ifdef HAVE_SYSCTLBYNAME
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(ALLEGRO_WINDOWS)
#include <windows.h>
#include <windowsx.h>

#include "allegro5/allegro_windows.h"
#include "allegro5/platform/aintwin.h"
#endif

#include "allegro5/allegro.h"
#include "allegro5/cpu.h"
#include "allegro5/internal/aintern.h"


/** Function: al_get_cpu_count
 */
int al_get_cpu_count(void)
{
#if defined(ALLEGRO_HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
   return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(ALLEGRO_HAVE_SYSCTLBYNAME)
   int result;
   size_t size = sizeof(int);
   sysctlbyname("hw.ncpu", &result, &size, NULL, 0);
   return result;
#elif defined(ALLEGRO_WINDOWS)
   SYSTEM_INFO info;
   GetSystemInfo(&info);
   return info.dwNumberOfProcessors;
#else
   return 1;
#endif
}

/** Function al_get_memory_size
 */
int al_get_memory_size(void)
{
#if defined(ALLEGRO_HAVE_SYSCONF) && defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
   uint64_t aid = (uint64_t) sysconf(_SC_PHYS_PAGES);
   aid         *= (uint64_t) sysconf(_SC_PAGESIZE);
   aid         /= (uint64_t) (1024 * 1024);
   return floor(aid);
#elif defined(ALLEGRO_HAVE_SYSCTL)
   #ifdef HW_REALMEM
      int mib[2] = {CTL_HW, HW_REALMEM};
   #elif defined(HW_PHYSMEM)
      int mib[2] = {CTL_HW, HW_PHYSMEM};
   #else
      int mib[2] = {CTL_HW, HW_MEMSIZE};
   #endif  
   uint64_t memsize = 0;
   size_t len = sizeof(memsize);
   if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) { 
      return (int)(memsize / (1024*1024));
   } 
   return -1;
#elif defined(ALLEGRO_WINDOWS)
   MEMORYSTATUSEX status;
   status.dwLength = sizeof(status);
   if (GlobalMemoryStatusEx(&stat)) {
      return (int)(status.ullTotalPhys / (1024 * 1024));
   }
   return -1;
#else
   return -1;
#endif
    }
    return SDL_SystemRAM;
}


#ifdef TEST_MAIN

#include <stdio.h>

int
main()
{
    printf("CPU count: %d\n", SDL_GetCPUCount());
    printf("CPU type: %s\n", SDL_GetCPUType());
    printf("CPU name: %s\n", SDL_GetCPUName());
    printf("CacheLine size: %d\n", SDL_GetCPUCacheLineSize());
    printf("RDTSC: %d\n", SDL_HasRDTSC());
    printf("Altivec: %d\n", SDL_HasAltiVec());
    printf("MMX: %d\n", SDL_HasMMX());
    printf("3DNow: %d\n", SDL_Has3DNow());
    printf("SSE: %d\n", SDL_HasSSE());
    printf("SSE2: %d\n", SDL_HasSSE2());
    printf("SSE3: %d\n", SDL_HasSSE3());
    printf("SSE4.1: %d\n", SDL_HasSSE41());
    printf("SSE4.2: %d\n", SDL_HasSSE42());
    printf("AVX: %d\n", SDL_HasAVX());
    printf("RAM: %d MB\n", SDL_GetSystemRAM());
    return 0;
}

#endif /* TEST_MAIN */

/* vi: set ts=4 sw=4 expandtab: */
      
