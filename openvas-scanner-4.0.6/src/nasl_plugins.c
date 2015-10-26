/* OpenVAS
* $Id: nasl_plugins.c 20329 2014-09-09 10:38:48Z kroosec $
* Description: Launches NASL plugins.
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

/**
 * @brief The nasl - plugin class. Loads or launches nasl- plugins.
 */

#include <errno.h>
#include <unistd.h>   /* for close() */
#include <signal.h>   /* for SIGTERM */
#include <string.h>   /* for strlen() */
#include <sys/stat.h>

#include <glib.h>

#include <sys/types.h>
#include <utime.h>

#include <openvas/base/drop_privileges.h> /* for drop_privileges */
#include <openvas/base/nvticache.h>       /* for nvticache_add */
#include <openvas/nasl/nasl.h>
#include <openvas/misc/network.h>    /* for internal_send */
#include <openvas/misc/nvt_categories.h>  /* for ACT_SCANNER */
#include <openvas/misc/plugutils.h>     /* for plug_set_launch */
#include <openvas/misc/internal_com.h>  /* for INTERNAL_COMM_CTRL_FINISHED */
#include <openvas/misc/system.h>     /* for emalloc */
#include <openvas/misc/openvas_proctitle.h>

#include "pluginload.h"
#include "pluginscheduler.h"    /* for LAUNCH_DISABLED */
#include "preferences.h"
#include "processes.h"
#include "log.h"

static void nasl_thread (struct arglist *);


/**
 * @brief Add *one* .nasl plugin to the plugin list and return the pointer to it.
 *
 * The plugin is first attempted to be loaded from the cache.
 * If that fails, it is parsed (via exec_nasl_script) and
 * added to the cache.
 * If a plugin with the same (file)name is already present in the plugins
 * arglist, it will be replaced.
 *
 * @param folder  Path to the plugin folder.
 * @param name    File-name of the plugin (will be used as key in plugins).
 * @param plugins The arglist that the plugin shall be added to (with parameter
 *                name as the key).
 * @param preferences The plugins preferences.
 *
 * @return Pointer to the plugin (as arglist). NULL in case of errors.
 */
struct arglist *
nasl_plugin_add (char *folder, char *name, struct arglist *plugins,
                 struct arglist *preferences)
{
  char fullname[PATH_MAX + 1];
  struct arglist *plugin_args;
  struct arglist *prev_plugin = NULL;
  int nasl_mode;
  nasl_mode = NASL_EXEC_DESCR;
  nvti_t *nvti;

  snprintf (fullname, sizeof (fullname), "%s/%s", folder, name);

  if (preferences_nasl_no_signature_check (preferences) > 0)
    {
      nasl_mode |= NASL_ALWAYS_SIGNED;
    }

  nvti = nvticache_get (arg_get_value(preferences, "nvticache"), name);
  plugin_args = plug_create_from_nvti_and_prefs (nvti, preferences);
  if (plugin_args == NULL)
    {
      nvti_free (nvti);

      plugin_args = emalloc (sizeof (struct arglist));
      arg_add_value (plugin_args, "preferences", ARG_ARGLIST, -1,
                     (void *) preferences);
      nvti = nvti_new ();
      arg_add_value (plugin_args, "NVTI", ARG_PTR, -1, nvti);

      if (exec_nasl_script (plugin_args, fullname, nasl_mode) < 0)
        {
          log_write ("%s: Could not be loaded", fullname);
          arg_set_value (plugin_args, "preferences", -1, NULL);
          arg_free_all (plugin_args);
          return NULL;
        }

      // this extra pointer was only necessary during parsing the
      // description part of a NASL. Now we can remove it from the args.
      if (arg_get_value (plugin_args, "NVTI"))
        arg_del_value (plugin_args, "NVTI");

      nvti_set_src (nvti, fullname);

      // Check mtime of plugin before caching it
      // Set to now if mtime is in the future
      struct stat plug_stat;
      time_t now = time (NULL) - 1;
      stat (fullname, &plug_stat);
      if (plug_stat.st_mtime > now)
        {
          struct utimbuf fixed_timestamp;
          fixed_timestamp.actime = now;
          fixed_timestamp.modtime = now;
          if (utime (fullname, &fixed_timestamp) == 0)
            log_write ("The timestamp for %s was from the future. This has been fixed.", fullname);
          else
            log_write ("The timestamp for %s is from the future and could not be fixed.", fullname);
        }

      if (nvti_oid (nvti) != NULL)
        {
          nvticache_add (arg_get_value(preferences, "nvticache"), nvti, name);
          arg_set_value (plugin_args, "preferences", -1, NULL);
          arg_free_all (plugin_args);
          nvti_free (nvti);
          nvti = nvticache_get (arg_get_value(preferences, "nvticache"), name);
          plugin_args = plug_create_from_nvti_and_prefs (nvti, preferences);
        }
      else
        // Most likely an exit was hit before the description could be parsed.
        log_write ("\r%s could not be added to the cache and is likely to stay"
                   " invisible to the client.", name);
    }

  if (plugin_args == NULL)
    {
      /* Discard invalid plugins */
      log_write ("%s: Failed to load", name);
      nvti_free (nvti);
      return NULL;
    }

  if (nvti_oid (nvti) == NULL)
    {
      /* Discard invalid plugins */
      log_write ("%s: Failed to load, no OID", name);
      nvti_free (nvti);
      plugin_free (plugin_args);
      return NULL;
    }

  arg_add_value (plugin_args, "OID", ARG_STRING, strlen (nvti_oid (nvti)), g_strdup (nvti_oid (nvti)));
  nvti_free (nvti);

  plug_set_launch (plugin_args, LAUNCH_DISABLED);
  prev_plugin = arg_get_value (plugins, name);

  // Was a plugin with the same filename already loaded? If so, remove it.
  if (prev_plugin == NULL)
    arg_add_value (plugins, name, ARG_ARGLIST, -1, plugin_args);
  else
    {
      plugin_free (prev_plugin);
      arg_set_value (plugins, name, -1, plugin_args);
    }

  return plugin_args;
}

/**
 * @brief Launch a NASL plugin.
 */
int
nasl_plugin_launch (struct arglist *globals, struct arglist *plugin,
                    struct arglist *hostinfos, struct arglist *preferences,
                    kb_t kb, char *name)
{
  int module;
  struct arglist *d = emalloc (sizeof (struct arglist));

  arg_add_value (plugin, "HOSTNAME", ARG_ARGLIST, -1, hostinfos);
  if (arg_get_value (plugin, "globals"))
    arg_set_value (plugin, "globals", -1, globals);
  else
    arg_add_value (plugin, "globals", ARG_ARGLIST, -1, globals);


  arg_set_value (plugin, "preferences", -1, preferences);
  arg_add_value (plugin, "key", ARG_PTR, -1, kb);

  arg_add_value (d, "args", ARG_ARGLIST, -1, plugin);
  arg_add_value (d, "name", ARG_STRING, strlen (name), name);
  arg_add_value (d, "preferences", ARG_ARGLIST, -1, preferences);

  module = create_process ((process_func_t) nasl_thread, d);
  arg_free (d);
  return module;
}


static void
nasl_thread (struct arglist *g_args)
{
  struct arglist *args = arg_get_value (g_args, "args");
  struct arglist *globals = arg_get_value (args, "globals");
  struct arglist *preferences = arg_get_value (g_args, "preferences");
  char *name = arg_get_value (g_args, "name");
  int soc = GPOINTER_TO_SIZE (arg_get_value (args, "SOCKET"));
  int i;
  int nasl_mode;
  GError *error = NULL;

  if (preferences_benice (NULL))
    {
      int nice_retval;
      errno = 0;
      nice_retval = nice (-5);
      if (nice_retval == -1 && errno != 0)
        {
          log_write ("Unable to renice process: %d", errno);
        }
    }

  /* XXX ugly hack */
  soc = dup2 (soc, 4);
  if (soc < 0)
    {
      log_write ("dup2() failed ! - can not launch the plugin\n");
      return;
    }
  arg_set_value (args, "SOCKET", sizeof (gpointer), GSIZE_TO_POINTER (soc));
  arg_set_value (globals, "global_socket", sizeof (gpointer),
                 GSIZE_TO_POINTER (soc));
  for (i = 6; i < getdtablesize (); i++)
    close (i);
  proctitle_set ("openvassd: testing %s (%s)",
                 arg_get_value (arg_get_value (args, "HOSTNAME"), "NAME"),
                 name);
  signal (SIGTERM, _exit);

  nasl_mode = NASL_EXEC_DONT_CLEANUP;
  if (preferences_nasl_no_signature_check (preferences) > 0)
    nasl_mode |= NASL_ALWAYS_SIGNED;

  if (preferences_drop_privileges (preferences))
    {
      int drop_priv_res = drop_privileges (NULL, &error);
      if (drop_priv_res != OPENVAS_DROP_PRIVILEGES_OK)
        {
          if (drop_priv_res != OPENVAS_DROP_PRIVILEGES_FAIL_NOT_ROOT)
            log_write ("Failed to drop privileges for %s\n", name);
          g_error_free (error);
        }
    }

  exec_nasl_script (args, name, nasl_mode);
  internal_send (soc, NULL,
                 INTERNAL_COMM_MSG_TYPE_CTRL | INTERNAL_COMM_CTRL_FINISHED);
}
