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
#include "allegro5/allegro.h"
#include "allegro5/cpu.h"
#include "allegro5/internal/aintern.h"

#ifdef ALLEGRO_HAVE_SYSCONF
#include <unistd.h>
#endif

#if defined(ALLEGRO_HAVE_SYSCTLBYNAME) || defined(ALLEGRO_HAVE_SYSCTL)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifdef ALLEGRO_WINDOWS
#define UNICODE
#include <windows.h>
#endif




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
   return (int)(aid);
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
#elif defined(ALLEGRO_WINDOWS)
   MEMORYSTATUSEX status;
   status.dwLength = sizeof(status);
   if (GlobalMemoryStatusEx(&status)) {
      return (int)(status.ullTotalPhys / (1024 * 1024));
   }
#endif
   return -1;
}


/* vi: set ts=4 sw=4 expandtab: */
      
