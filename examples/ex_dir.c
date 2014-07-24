#include <allegro5/allegro.h>
#include <stdio.h>

#include "common.c"

static void print_file(ALLEGRO_FS_ENTRY *entry)
{
   int mode = al_get_fs_entry_mode(entry);
   time_t now = time(NULL);
   time_t atime = al_get_fs_entry_atime(entry);
   time_t ctime = al_get_fs_entry_ctime(entry);
   time_t mtime = al_get_fs_entry_mtime(entry);
   off_t size = al_get_fs_entry_size(entry);
   const char *name = al_get_fs_entry_name(entry);

   log_printf("%-36s %s%s%s%s%s%s %8u %8u %8u %8u\n",
      name,
      mode & ALLEGRO_FILEMODE_READ ? "r" : ".",
      mode & ALLEGRO_FILEMODE_WRITE ? "w" : ".",
      mode & ALLEGRO_FILEMODE_EXECUTE ? "x" : ".",
      mode & ALLEGRO_FILEMODE_HIDDEN ? "h" : ".",
      mode & ALLEGRO_FILEMODE_ISFILE ? "f" : ".",
      mode & ALLEGRO_FILEMODE_ISDIR ? "d" : ".",
      (unsigned)(now - ctime),
      (unsigned)(now - mtime),
      (unsigned)(now - atime),
      (unsigned)size);
}

static void print_entry(ALLEGRO_FS_ENTRY *entry)
{
   ALLEGRO_FS_ENTRY *next;

   print_file(entry);

   if (!(al_get_fs_entry_mode(entry) & ALLEGRO_FILEMODE_ISDIR))
      return;

   if (!al_open_directory(entry)) {
      log_printf("Error opening directory: %s\n", al_get_fs_entry_name(entry));
      return;
   }

   while (1) {
      next = al_read_directory(entry);
      if (!next)
         break;

      print_entry(next);
      al_destroy_fs_entry(next);
   }

   al_close_directory(entry);
}


static bool print_directory_callback(const char * filename, int mode, void * extra) {
   log_printf("%s: (%d) %s\n", (char *) extra,  mode, filename);
   return true;
}

static void print_directory(char * dirname)
{
   log_printf("\n------------------------------------\nExample of al_for_each_filename:\n\n");   
   al_for_each_file(dirname, print_directory_callback,  ALLEGRO_FOR_EACH_FILE_RECURSE,  (void*) dirname);
}


static bool print_fs_entry_callback(ALLEGRO_FS_ENTRY * entry, void * extra) {
   (void) extra;
   print_file(entry);
   return true;
}

static void print_fs_entry(ALLEGRO_FS_ENTRY * dir)
{
   log_printf("\n------------------------------------\nExample of al_for_each_fs_entry:\n\n");   
   al_for_each_fs_entry(dir, print_fs_entry_callback, 0, (void*) al_get_fs_entry_name(dir));
}


int main(int argc, char **argv)
{
   int i;

   if (!al_init()) {
      abort_example("Could not init Allegro.\n");
   }
   open_log_monospace();
   
   log_printf("Example of filesystem entry functions:\n\n%-36s %-6s %8s %8s %8s %8s\n",
      "name", "flags", "ctime", "mtime", "atime", "size");
   log_printf(
      "------------------------------------ "
      "------ "
      "-------- "
      "-------- "
      "-------- "
      "--------\n");

   if (argc == 1) {
      ALLEGRO_FS_ENTRY *entry = al_create_fs_entry("data");
      print_entry(entry);
      print_fs_entry(entry);
      al_destroy_fs_entry(entry);
      print_directory("data");
      print_directory("data");
      print_directory("data");
   }

   for (i = 1; i < argc; i++) {
      ALLEGRO_FS_ENTRY *entry = al_create_fs_entry(argv[i]);
      print_entry(entry);
      print_fs_entry(entry);
      al_destroy_fs_entry(entry);
      print_directory(argv[i]);
   }

   close_log(true);
   return 0;
}

/* vim: set sts=3 sw=3 et: */
