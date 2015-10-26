/* OpenVAS
* $Id: pluginload.c 19027 2014-03-19 10:50:05Z kroosec $
* Description: Loads plugins from disk into memory.
*
* Authors: - Renaud Deraison <deraison@nessus.org> (Original pre-fork develoment)
*          - Tim Brown <mailto:timb@openvas.org> (Initial fork)
*          - Laban Mwangi <mailto:labanm@openvas.org> (Renaming work)
*          - Tarik El-Yassem <mailto:tarik@openvas.org> (Headers section)
*
* Copyright:
* Portions Copyright (C) 2006 Software in the Public Interest, Inc.
* Based on work Copyright (C) 1998 - 2006 Tenable Network Security, Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2,
* as published by the Free Software Foundation
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h> /* for printf() */

#include <openvas/nasl/nasl.h>
#include <openvas/misc/system.h>     /* for emalloc */
#include <openvas/base/nvticache.h>  /* for nvticache_new */
#include <openvas/misc/openvas_proctitle.h>

#include <glib.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/shm.h>     /* for shmget */
#include <string.h>
#include <errno.h>

#include "utils.h"
#include "pluginload.h"
#include "log.h"
#include "preferences.h"

/**
 * @brief Collects all NVT files in a directory and recurses into subdirs.
 *
 * @param folder The main directory from where to descend and collect.
 *
 * @param subdir A subdirectory to consider for the collection: "folder/subdir"
 *               is thus the effective directory to descend from. "subdir"
 *               can be "" to make "folder" the effective start.
 *
 * @param files  A list that is extended with all found files. If it
 *               is NULL, a new list is created automatically.
 *
 * @return Parameter "files", extended with all the NVT files found in
 *         "folder" and its subdirectories. Not added are directory names.
 *         NVT files are identified by the defined filename suffixes.
 */
GSList *
collect_nvts (const char *folder, const char *subdir, GSList * files)
{
  GDir *dir;
  const gchar *fname;

  if (folder == NULL)
    return files;

  dir = g_dir_open (folder, 0, NULL);
  if (dir == NULL)
    return files;

  fname = g_dir_read_name (dir);
  while (fname)
    {
      char *path;

      path = g_build_filename (folder, fname, NULL);
      if (g_file_test (path, G_FILE_TEST_IS_DIR))
        {
          char *new_folder, *new_subdir;

          new_folder = g_build_filename (folder, fname, NULL);
          new_subdir = g_build_filename (subdir, fname, NULL);

          files = collect_nvts (new_folder, new_subdir, files);

          if (new_folder)
            g_free (new_folder);
          if (new_subdir)
            g_free (new_subdir);
        }
      else if (g_str_has_suffix (fname, ".nasl"))
        files = g_slist_prepend (files,
                                 g_build_filename (subdir, fname, NULL));
      g_free (path);
      fname = g_dir_read_name (dir);
    }

  g_dir_close (dir);
  return files;
}

int
calculate_eta (struct timeval start_time, int loaded, int total)
{
  struct timeval current_time;
  int elapsed, remaining;

  if (start_time.tv_sec == 0)
    return 0;

  gettimeofday (&current_time, NULL);
  elapsed = current_time.tv_sec - start_time.tv_sec;
  remaining = total - loaded;
  return (remaining * elapsed) / loaded;
}

static int *loading_shm = NULL;
static int loading_shmid = 0;

/*
 * @brief Initializes the shared memory data used to report plugins loading
 *        progress to other processes.
 */
void
init_loading_shm ()
{
  int shm_key;

  if (loading_shm)
    return;

  shm_key = rand () + 1;
  /*
   * Create shared memory segment if it doesn't exist.
   * This will be used to communicate current plugins loading progress to other
   * processes.
   * loading_shm[0]: Number of loaded plugins.
   * loading_shm[1]: Total number of plugins.
   */
  loading_shmid = shmget (shm_key, sizeof (int) * 2, IPC_CREAT | 0600);
  if (loading_shmid < 0)
    perror ("shmget");
  loading_shm = shmat (loading_shmid, NULL, 0);
  if (loading_shm == (void *) -1)
    {
      perror ("shmat");
      loading_shm = NULL;
    }
  else
    bzero (loading_shm, sizeof (int) * 2);
}

/*
 * @brief Destroys the shared memory data used to report plugins loading
 *        progress to other processes.
 */
void
destroy_loading_shm ()
{
  if (loading_shm)
    {
      shmdt (loading_shm);
      if (shmctl (loading_shmid, IPC_RMID, NULL))
        perror ("shmctl");
      loading_shm = NULL;
      loading_shmid = 0;
    }
}

/*
 * @brief Gives current number of loaded plugins.
 *
 * @return Number of loaded plugins,  0 if initalization wasn't successful.
 */
int
current_loading_plugins ()
{
  return loading_shm ? loading_shm[0] : 0;
}

/*
 * @brief Gives the total number of plugins to be loaded.
 *
 * @return Total of loaded plugins,  0 if initalization wasn't successful.
 */
int
total_loading_plugins ()
{
  return loading_shm ? loading_shm[1] : 0;
}

/*
 * @brief Sets number of loaded plugins.
 *
 * @param[in]   current Number of loaded plugins.
 */
void
set_current_loading_plugins (int current)
{
  if (loading_shm)
    loading_shm[0] = current;
}

/*
 * @brief Sets total number of plugins to be loaded.
 *
 * @param[in]   total Total number of plugins
 */
void
set_total_loading_plugins (int total)
{
  if (loading_shm)
    loading_shm[1] = total;
}

static struct arglist *
plugins_reload_from_dir (struct arglist *preferences, struct arglist *plugins,
                         char *folder)
{
  GSList *files = NULL, *f;
  gchar *pref_include_folders;
  int loaded_files = 0, num_files = 0;
  struct timeval start_time;

  add_nasl_inc_dir ("");        // for absolute and relative paths

  pref_include_folders = arg_get_value (preferences, "include_folders");
  if (pref_include_folders != NULL)
    {
      gchar **include_folders = g_strsplit (pref_include_folders, ":", 0);
      int i = 0;

      for (i = 0; i < g_strv_length (include_folders); i++)
        {
          int result = add_nasl_inc_dir (include_folders[i]);
          if (result < 0)
            printf ("Could not add %s to the list of include folders.\n"
                    "Make sure %s exists and is a directory.\n",
                    include_folders[i], include_folders[i]);
        }

      g_strfreev (include_folders);
    }

  if (folder == NULL)
    {
#ifdef DEBUG
      log_write ("%s:%d : folder == NULL\n", __FILE__, __LINE__);
#endif
      printf ("Could not determine the value of <plugins_folder>. Check %s\n",
              (char *) arg_get_value (preferences, "config_file"));
      return plugins;
    }

  files = collect_nvts (folder, "", files);
  num_files = g_slist_length (files);

  /*
   * Add the plugins
   */

  if (gettimeofday (&start_time, NULL))
    {
      bzero (&start_time, sizeof (start_time));
      log_write ("gettimeofday: %s\n", strerror (errno));
    }
  f = files;
  set_total_loading_plugins (num_files);
  while (f != NULL)
    {
      char *name = f->data;
      loaded_files++;
      if (loaded_files % 50 == 0)
        {
          int percentile, eta;

          set_current_loading_plugins (loaded_files);
          percentile = (loaded_files * 100) / num_files;
          eta = calculate_eta (start_time, loaded_files, num_files);
          proctitle_set ("openvassd: Reloaded %d of %d NVTs"
                         " (%d%% / ETA: %02d:%02d)", loaded_files, num_files,
                         percentile, eta / 60, eta % 60);
        }
      if (preferences_log_plugins_at_load (preferences))
        log_write ("Loading %s\n", name);
      if (g_str_has_suffix (name, ".nasl"))
        nasl_plugin_add (folder, name, plugins, preferences);
      g_free (f->data);
      f = g_slist_next (f);
    }

  g_slist_free (files);

  proctitle_set ("openvassd: Reloaded all the NVTs.");

  return plugins;
}

/*
 * main function for loading all the
 * plugins that are in folder <folder>
 */
struct arglist *
plugins_init (struct arglist *preferences)
{
  nvticache_t * nvti_cache;
  char *plugins_folder;
  struct arglist *plugins;

  // @todo: Perhaps check wether "nvticache" is already present in arglist
  plugins_folder = arg_get_value (preferences, "plugins_folder");
  nvti_cache = nvticache_new (arg_get_value (preferences, "cache_folder"),
                              plugins_folder);
  arg_add_value (preferences, "nvticache", ARG_PTR, -1, nvti_cache);
  plugins = emalloc (sizeof (struct arglist));

  return plugins_reload_from_dir (preferences, plugins, plugins_folder);
}

void
plugin_set_socket (struct arglist *plugin, int soc)
{
  if (arg_get_value (plugin, "SOCKET") != NULL)
    arg_set_value (plugin, "SOCKET", sizeof (gpointer), GSIZE_TO_POINTER (soc));
  else
    arg_add_value (plugin, "SOCKET", ARG_INT, sizeof (gpointer),
                   GSIZE_TO_POINTER (soc));
}

int
plugin_get_socket (struct arglist *plugin)
{
  return GPOINTER_TO_SIZE (arg_get_value (plugin, "SOCKET"));
}


void
plugin_unlink (plugin)
     struct arglist *plugin;
{
  if (plugin == NULL)
    {
      fprintf (stderr, "Error in plugin_unlink - args == NULL\n");
      return;
    }
  arg_set_value (plugin, "preferences", -1, NULL);
}


void
plugin_free (plugin)
     struct arglist *plugin;
{
  plugin_unlink (plugin);
  arg_free_all (plugin);
}

void
plugins_free (plugins)
     struct arglist *plugins;
{
  struct arglist *p = plugins;
  if (p == NULL)
    return;

  while (p->next)
    {
      plugin_unlink (p->value);
      p = p->next;
    }
  arg_free_all (plugins);
}

/*
 * Put our socket somewhere in the plugins
 * arguments
 */
void
plugins_set_socket (struct arglist *plugins, int soc)
{
  struct arglist *t;

  t = plugins;
  while (t && t->next)
    {
      plugin_set_socket (t->value, soc);
      t = t->next;
    }
}
