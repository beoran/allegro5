#include <allegro5/allegro.h>
#include <stdio.h>

#include "common.c"

int main(void)
{
   if (!al_init()) {
      abort_example("Could not init Allegro.\n");
   }
   open_log_monospace();
   log_printf("Amount of CPU cores detected: %d\n", al_get_cpu_count());
   log_printf("System memory size: %d MB.\n", al_get_memory_size());
   close_log(true);
   return 0;
}

/* vim: set sts=3 sw=3 et: */
