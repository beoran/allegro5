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
 *      File System Hooks.
 *
 *      By Thomas Fjellstrom.
 *
 *      See readme.txt for copyright information.
 */

/* Title: Filesystem routines
*/

#include "allegro5/allegro.h"
#include "allegro5/internal/aintern_fshook.h"



/* Function: al_create_fs_entry
 */
ALLEGRO_FS_ENTRY *al_create_fs_entry(const char *path)
{
   const ALLEGRO_FS_INTERFACE *vt = al_get_fs_interface();
   ASSERT(vt->fs_create_entry);
   return vt->fs_create_entry(path);
}


/* Function: al_destroy_fs_entry
 */
void al_destroy_fs_entry(ALLEGRO_FS_ENTRY *fh)
{
   if (fh) {
      fh->vtable->fs_destroy_entry(fh);
   }
}


/* Function: al_get_fs_entry_name
 */
const char *al_get_fs_entry_name(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_name(e);
}


/* Function: al_update_fs_entry
 */
bool al_update_fs_entry(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_update_entry(e);
}


/* Function: al_get_fs_entry_mode
 */
uint32_t al_get_fs_entry_mode(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_mode(e);
}


/* Function: al_get_fs_entry_atime
 */
time_t al_get_fs_entry_atime(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_atime(e);
}


/* Function: al_get_fs_entry_mtime
 */
time_t al_get_fs_entry_mtime(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_mtime(e);
}


/* Function: al_get_fs_entry_ctime
 */
time_t al_get_fs_entry_ctime(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_ctime(e);
}


/* Function: al_get_fs_entry_size
 */
off_t al_get_fs_entry_size(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_size(e);
}


/* Function: al_remove_fs_entry
 */
bool al_remove_fs_entry(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_remove_entry(e);
}


/* Function: al_fs_entry_exists
 */
bool al_fs_entry_exists(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_entry_exists(e);
}


/* Function: al_open_directory
 */
bool al_open_directory(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_open_directory(e);
}


/* Function: al_close_directory
 */
bool al_close_directory(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_close_directory(e);
}


/* Function: al_read_directory
 */
ALLEGRO_FS_ENTRY *al_read_directory(ALLEGRO_FS_ENTRY *e)
{
   ASSERT(e != NULL);

   return e->vtable->fs_read_directory(e);
}


/* Function: al_get_current_directory
 */
char *al_get_current_directory(void)
{
   const ALLEGRO_FS_INTERFACE *vt = al_get_fs_interface();
   ASSERT(vt->fs_get_current_directory);
   return vt->fs_get_current_directory();
}


/* Function: al_change_directory
 */
bool al_change_directory(const char *path)
{
   const ALLEGRO_FS_INTERFACE *vt = al_get_fs_interface();
   ASSERT(vt->fs_change_directory);
   ASSERT(path);

   return vt->fs_change_directory(path);
}


/* Function: al_make_directory
 */
bool al_make_directory(const char *path)
{
   const ALLEGRO_FS_INTERFACE *vt = al_get_fs_interface();
   ASSERT(path);
   ASSERT(vt->fs_make_directory);

   return vt->fs_make_directory(path);
}


/* Function: al_filename_exists
 */
bool al_filename_exists(const char *path)
{
   const ALLEGRO_FS_INTERFACE *vt = al_get_fs_interface();
   ASSERT(path != NULL);
   ASSERT(vt->fs_filename_exists);

   return vt->fs_filename_exists(path);
}


/* Function: al_remove_filename
 */
bool al_remove_filename(const char *path)
{
   const ALLEGRO_FS_INTERFACE *vt = al_get_fs_interface();
   ASSERT(vt->fs_remove_filename);
   ASSERT(path != NULL);

   return vt->fs_remove_filename(path);
}


/* Function: al_open_fs_entry
 */
ALLEGRO_FILE *al_open_fs_entry(ALLEGRO_FS_ENTRY *e, const char *mode)
{
   ASSERT(e != NULL);

   if (e->vtable->fs_open_file)
      return e->vtable->fs_open_file(e, mode);

   al_set_errno(EINVAL);
   return NULL;
}


/* Utility functions and callbacks for them. */


/* Halper to handle a single entry of al_for_each_entry */
static int al_for_each_fs_entry_handle_entry(
   ALLEGRO_FS_ENTRY *entry,
   int (*callback)(ALLEGRO_FS_ENTRY *e, void *extra),
   int flags,
   void *extra)
{
   int result;
   int mode    = al_get_fs_entry_mode(entry);
   /* Handle recursion if requested. Do this before filtering so even
    * filetered entries are recursed into.  This also ensures depth-first
    * recursion.
    */
   if (mode & ALLEGRO_FILEMODE_ISDIR) {
      if (flags & ALLEGRO_FOR_EACH_FILE_RECURSE) {
         result = al_for_each_fs_entry(entry, callback, flags, extra);
         if (result < ALLEGRO_FOR_EACH_FILE_OK) {
            return result;
         }
      }
   }
   
   /* Filter if requested. */
   if (flags & ALLEGRO_FOR_EACH_FILE_FILTER) {
      int filter  = flags & 0xff; /* Low bits contain filter. */
      if (!(filter & mode)) return true;
   }
   
   result = callback(entry, extra);

   return result;
}


/* Function: al_for_each_fs_entry
 */
int al_for_each_fs_entry (
   ALLEGRO_FS_ENTRY *dir,
   int (*callback)(ALLEGRO_FS_ENTRY *e, void *extra),
   int flags,
   void *extra)
{
   ALLEGRO_FS_ENTRY * entry;
   al_set_errno(0);
   
   if (!al_open_directory(dir)) {
      al_set_errno(ENOENT);
      return ALLEGRO_FOR_EACH_FILE_ERROR;
   }
      
   entry = al_read_directory(dir);
   while(entry) {
      int result = al_for_each_fs_entry_handle_entry(entry, callback, flags, extra);
      al_destroy_fs_entry(entry);
      if (result < ALLEGRO_FOR_EACH_FILE_OK) {
         return result;
      }
      entry = al_read_directory(dir);
   }

   return ALLEGRO_FOR_EACH_FILE_OK;
}

typedef struct ALLEGRO_FOR_EACH_FILE_INFO {
   int (*callback)(const char * filename, int mode, void *extra);
   int flags;
   void * extra;
   const char * path;
} ALLEGRO_FOR_EACH_FILE_INFO;


/* Helper callback that will map al_for_each_fs_entry to
 * for each file. */
static int al_for_each_file_callback_wrapper(ALLEGRO_FS_ENTRY * e, void * extra) {
   ALLEGRO_FOR_EACH_FILE_INFO * info = extra;
   int result;
   const char * name = al_get_fs_entry_name(e);
   int mode          = al_get_fs_entry_mode(e);
   if (info->flags & ALLEGRO_FOR_EACH_FILE_FILENAME) {
      ALLEGRO_PATH * path = al_create_path(name);
      if (!path) {
         al_set_errno(ENOMEM);
         return ALLEGRO_FOR_EACH_FILE_ERROR;
      }
      result = info->callback(al_get_path_filename(path), mode, info->extra);
      al_destroy_path(path);
   } else  {
      result = info->callback(name, mode, info->extra);
   }
   return result;
}

/* Function: al_for_each_file
 */
int al_for_each_file(
   const char *path,
   int (*callback)(const char * filename, int mode, void *extra),
   int flags,
   void *extra)
{
   ALLEGRO_FOR_EACH_FILE_INFO info;
   ALLEGRO_FS_ENTRY * dir;
   int result;
   dir            = al_create_fs_entry(path);
   if (!dir) {
      al_set_errno(ENOENT);
   }
   info.callback  = callback;
   info.extra     = extra;
   info.flags     = flags;
   info.path      = path;
   result         = al_for_each_fs_entry(dir, al_for_each_file_callback_wrapper,
                     flags, &info);
   al_destroy_fs_entry(dir);
   return result;
}



/*
 * Local Variables:
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
/* vim: set sts=3 sw=3 et: */
