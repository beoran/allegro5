#include <allegro5/allegro.h>
#include <stdio.h>

#include "common.c"

int main(int argc, char **argv)
{
   int i;

   if (!al_init()) {
      abort_example("Could not init Allegro.\n");
   }
   open_log_monospace();
   log_printf("Amount of CPU's detected: %d", al_get_cpu_count());
   log_printf("System RAM size: %d MB.", al_get_ram_size());
   close_log(true);
   return 0;
}

/* vim: set sts=3 sw=3 et: */
