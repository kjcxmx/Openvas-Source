/* OpenVAS
* $Id: ntp.c 20735 2014-10-30 13:18:36Z mwiegand $
* Description: OpenVAS Transfer Protocol handling.
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

#include <string.h> /* for strlen() */
#include <stdio.h>  /* for fprintf() */
#include <stdlib.h> /* for atoi() */

#include <glib.h>

#include <openvas/base/nvti.h>  /* for nvti_name */

#include <openvas/misc/network.h>    /* for recv_line */
#include <openvas/misc/system.h>     /* for emalloc */
#include <openvas/misc/hash_table_file.h>
#include <openvas/misc/openvas_ssh_login.h>
#include <openvas/misc/internal_com.h> /* for INTERNAL_COMM_MSG_TYPE_DATA */

#include <openvas/base/nvticache.h>     /* for nvticache_t */

#include "ntp.h"
#include "otp.h"
#include "comm.h"
#include "log.h"
#include "utils.h"
#include "preferences.h"
#include "hosts.h"

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x):(y))
#endif


static int ntp_read_prefs (struct arglist *);
static int ntp_long_attack (struct arglist *);
static int ntp_recv_file (struct arglist *);

/**
 * @brief Parses the input sent by the client before the NEW_ATTACK message.
 */
int
ntp_parse_input (struct arglist *globals, char *input)
{
  char *str;
  int result = 1;               /* default return value is 1 */

  str = strstr (input, " <|> ");
  if (str == NULL)
    return 1;

  str[0] = '\0';

  if (strcmp (input, "CLIENT") == 0)
    {
      input = str + 5;
      str = strchr (input, ' ');
      if (str != NULL)
        str[0] = '\0';

      if (input[strlen (input) - 1] == '\n')
        input[strlen (input) - 1] = '\0';

      switch (otp_get_client_request (input))
        {
        case CREQ_ATTACHED_FILE:
          ntp_recv_file (globals);
          break;

        case CREQ_LONG_ATTACK:
          result = ntp_long_attack (globals);
          break;

        case CREQ_OPENVAS_VERSION:
          otp_server_openvas_version (globals);
          break;

        case CREQ_PAUSE_WHOLE_TEST:
          log_write ("Pausing the whole test (requested by client)");
          hosts_pause_all ();
          result = NTP_PAUSE_WHOLE_TEST;
          break;

        case CREQ_PLUGIN_INFO:
          {
            char *t, *s;
            t = strstr (&(str[1]), " <|> ");
            if (t == NULL)
              {
                result = -1;
                break;
              }
            s = t + 5;
            plugin_send_infos (globals, s);
            break;
          }

        case CREQ_PREFERENCES:
          ntp_read_prefs (globals);
          break;

        case CREQ_RESUME_WHOLE_TEST:
          log_write ("Resuming the whole test (requested by client)");
          hosts_resume_all ();
          arg_del_value (globals, "stop_required");
          result = NTP_RESUME_WHOLE_TEST;
          break;

        case CREQ_STOP_WHOLE_TEST:
          log_write ("Stopping the whole test (requested by client)");
          arg_add_value (globals, "stop_required", ARG_INT, sizeof (int),
                         GSIZE_TO_POINTER (1));
          hosts_stop_all ();
          result = NTP_STOP_WHOLE_TEST;
          break;

        case CREQ_STOP_ATTACK:
          {
            char *t, *s;
            s = str + 5;
            t = strstr (s, " <|> ");
            if (t == NULL)
              {
                result = -1;
                break;
              }
            t[0] = '\0';
            log_write ("Stopping attack against %s\n", s);
            hosts_stop_host (s);
            arg_add_value (globals, "stop_required", ARG_INT, sizeof (int),
                           GSIZE_TO_POINTER (1));
            ntp_timestamp_host_scan_interrupted (globals, s);
            break;
          }

        case CREQ_NVT_INFO:
          {
            comm_send_nvt_info (globals);
            comm_send_preferences (globals);
            break;
          }

        case CREQ_UNKNOWN:
          break;
        }
    }

  return (result);
}

static int
ntp_long_attack (struct arglist *globals)
{
  struct arglist *preferences = arg_get_value (globals, "preferences");
  int soc = GPOINTER_TO_SIZE (arg_get_value (globals, "global_socket"));
  char input[16384];
  int size;
  char *target;
  char *plugin_set;
  int n;

  n = recv_line (soc, input, sizeof (input) - 1);
  if (n <= 0)
    return -1;

#if DEBUGMORE
  printf ("long_attack :%s\n", input);
#endif
  if (!strncmp (input, "<|> CLIENT", sizeof ("<|> CLIENT")))
    return 1;
  size = atoi (input);
  target = emalloc (size + 1);

  n = 0;
  while (n < size)
    {
      int e;
      e = nrecv (soc, target + n, size - n, 0);
      if (e > 0)
        n += e;
      else
        return -1;
    }
  plugin_set = arg_get_value (preferences, "plugin_set");
  if (!plugin_set || plugin_set[0] == '\0')
    {
      plugin_set = emalloc (3);
      sprintf (plugin_set, "-1");
      if (!arg_get_value (preferences, "plugin_set"))
        arg_add_value (preferences, "plugin_set", ARG_STRING,
                       strlen (plugin_set), plugin_set);
      else
        arg_set_value (preferences, "plugin_set", strlen (plugin_set),
                       plugin_set);
    }

  comm_setup_plugins (globals, plugin_set);
  if (arg_get_value (preferences, "TARGET"))
    {
      char *old = arg_get_value (preferences, "TARGET");
      efree (&old);
      arg_set_value (preferences, "TARGET", strlen (target), target);
    }
  else
    arg_add_value (preferences, "TARGET", ARG_STRING, strlen (target), target);
  return 0;
}

/**
 * @brief Reads in "server" prefs sent by client.
 *
 * @param globals The global arglist (containing server preferences).
 * @return Always 0.
 */
static int
ntp_read_prefs (struct arglist *globals)
{
  struct arglist *preferences = arg_get_value (globals, "preferences");
  int soc = GPOINTER_TO_SIZE (arg_get_value (globals, "global_socket"));
  char *input;
  int input_sz = 1024 * 1024 * 2; /* this is sufficient for a plugin_set
                                     for upto 69K OIDs */ 

  input = emalloc (input_sz);
  for (;;)
    {
      int n;
      input[0] = '\0';
#if DEBUG_SSL > 2
      log_write ("ntp_read_prefs > soc=%d\n", soc);
#endif
      n = recv_line (soc, input, input_sz - 1);

      if (n < 0 || input[0] == '\0')
        {
          log_write ("Empty data string -- closing comm. channel\n");
          exit (0);
        }

      if (strstr (input, "<|> CLIENT") != NULL) /* finished = 1; */
        break;
      /* else */

      {
        char *pref;
        char *v;
        char *old;
        pref = input;
        v = strchr (input, '<');
        if (v)
          {
            char *value;
            v -= 1;
            v[0] = 0;

            value = v + 5;
            /*
             * "system" prefs can't be changed
             */
            if (is_scanner_only_pref (pref))
              continue;

            old = arg_get_value (preferences, pref);
#ifdef DEBUGMORE
            printf ("%s - %s (old : %s)\n", pref, value, old);
#endif
            if (value[0] != '\0')
              value[strlen (value) - 1] = '\0';

            if (old != NULL)
              {
                if (strcmp (old, value) != 0)
                  {
                    efree (&old);
                    v = estrdup (value);
                    arg_set_value (preferences, pref, strlen (v), v);
                  }
              }
            else
              {
                v = estrdup (value);
                arg_add_value (preferences, pref, ARG_STRING, strlen (v), v);
              }
          }
      }
    }

  efree (&input);
  return (0);
}

/**
 * @brief Adds a 'translation' entry for a file sent by the client.
 *
 * Files sent by the client are stored in memory on the server side.
 * In order to access these files, their original name ('local' to the client)
 * can be 'translated' into the file contents of the in-memory copy of the
 * file on the server side.
 *
 * @param globals    Global arglist.
 * @param remotename Name of the file as referenced by the client.
 * @param contents   Contents of the file.
 */
static void
files_add_translation (struct arglist *globals, const char *remotename,
                       char *contents)
{
  GHashTable *trans = arg_get_value (globals, "files_translation");
  // Register the mapping table if none there yet
  if (trans == NULL)
    {
      trans = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      arg_add_value (globals, "files_translation", ARG_PTR, -1, trans);
    }

  g_hash_table_insert (trans, g_strdup (remotename), contents);
}

/**
 * @brief Adds a 'content size' entry for a file sent by the client.
 *
 * Files sent by the client are stored in memory on the server side.
 * Because they may be binary we need to store the size of the uploaded file as
 * well. This function sets up a mapping from the original name sent by the
 * client to the file size.
 *
 * @param globals    Global arglist.
 * @param remotename Name of the file as referenced by the client.
 * @param filesize   Size of the file in bytes.
 */
static void
files_add_size_translation (struct arglist *globals, const char *remotename,
                            const long filesize)
{
  GHashTable *trans = arg_get_value (globals, "files_size_translation");
  gchar *filesize_str = g_strdup_printf ("%ld", filesize);

  // Register the mapping table if none there yet
  if (trans == NULL)
    {
      trans = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      arg_add_value (globals, "files_size_translation", ARG_PTR, -1, trans);
    }

  g_hash_table_insert (trans, g_strdup (remotename), g_strdup (filesize_str));
}

/**
 * @brief Reads in a hashtable from file that maps login-account names to host
 * @brief names.
 *
 * On client side the file is known as .host_logins (if defined, found in each
 * task directory).
 * The GHashTable will be available through the (global) arglist under the key
 * "MAP_HOST_SSHLOGIN_NAME".
 *
 * @param globals     Arglist to add the GHashTable to.
 * @param keyfiledata String containing the serialized GHashTable in keyfile
 * format.
 */
static void
build_global_host_sshlogins_map (struct arglist *globals, char *keyfiledata)
{
  // Deserialize the hashtable that mapped login-account names to host names.
  GHashTable *map_host_sshlogin_name =
    hash_table_file_read_text (keyfiledata, strlen (keyfiledata));
  // Add or replace it in the arglist
  if (map_host_sshlogin_name != NULL
      && arg_get_value (globals, "MAP_HOST_SSHLOGIN_NAME") == NULL)
    arg_add_value (globals, "MAP_HOST_SSHLOGIN_NAME", ARG_PTR, -1,
                   map_host_sshlogin_name);
  else if (map_host_sshlogin_name != NULL)
    arg_set_value (globals, "MAP_HOST_SSHLOGIN_NAME", -1,
                   map_host_sshlogin_name);
}

/**
 * @brief Reads ssh login information with mapping that was sent by the client.
 *
 * The information (mapped to the client file name '.logins') is used to create a
 * GHashTable that maps openvas_ssh_login structs to the user-defined names for
 * login-accounts.
 *
 * If successful, the map is available under the key "MAP_NAME_SSHLOGIN".
 *
 * @param globals  Global arglist to add the map to.
 * @param filepath Content of the '.login' file sent by the client.
 */
static void
build_global_sshlogin_info_map (struct arglist *globals, char *keyfiledata)
{
  // Read the buffer, build map of names->structs
  GHashTable *ssh_logins =
    openvas_ssh_login_file_read_buffer (keyfiledata, strlen (keyfiledata), FALSE);
  // Add/ Replace, if not-empty
  if (ssh_logins != NULL
      && arg_get_value (globals, "MAP_NAME_SSHLOGIN") == NULL)
    arg_add_value (globals, "MAP_NAME_SSHLOGIN", ARG_PTR, -1, ssh_logins);
  else if (ssh_logins != NULL)
    arg_set_value (globals, "MAP_NAME_SSHLOGIN", -1, ssh_logins);
}


/**
 * @brief Receive a file sent by the client.
 *
 * Two files receive an extra treatment: <br>
 *  - The .logins file sent by a client is used to build a store for login
 * information.
 *  - The .host_sshlogins file sent by the client is used to map keys to
 * hostnames.
 *
 * @return 0 if successful, -1 in case of errors.
 */
int
ntp_recv_file (struct arglist *globals)
{
  int soc = GPOINTER_TO_SIZE (arg_get_value (globals, "global_socket"));
  char input[4096];
  char *origname, *contents;
  gchar *cont_ptr = NULL;
  int n;
  long bytes = 0;
  long tot = 0;

  n = recv_line (soc, input, sizeof (input) - 1);
  if (n <= 0)
    return -1;

  if (strncmp (input, "name: ", strlen ("name: ")) == 0)
    {
      origname = estrdup (input + sizeof ("name: ") - 1);
      if (origname[strlen (origname) - 1] == '\n')
        origname[strlen (origname) - 1] = '\0';
    }
  else
    return -1;

  n = recv_line (soc, input, sizeof (input) - 1);
  if (n <= 0)
    return -1;
  /* XXX content: message. Ignored for the moment */

  n = recv_line (soc, input, sizeof (input) - 1);
  if (n <= 0)
    return -1;

  if (strncmp (input, "bytes: ", sizeof ("bytes: ") - 1) == 0)
    {
      char *t = input + sizeof ("bytes: ") - 1;
      bytes = atol (t);
    }
  else
    return -1;

  /* We now know that we have to read <bytes> bytes from the remote socket. */

  contents = g_try_malloc0 (bytes);

  if (contents == NULL)
    {
      log_write ("ntp_recv_file: Failed to allocate memory for uploaded file.");
      return -1;
    }

  cont_ptr = contents;
  while (tot < bytes)
    {
      bzero (input, sizeof (input));
      n = nrecv (soc, input, MIN (sizeof (input) - 1, bytes - tot), 0);
      if (n < 0)
        {
          char s[80];
          sprintf (s, "11_recv_file: nrecv(%d)", soc);
          perror (s);
          break;
        }
      else
        {
          memcpy ((cont_ptr + (tot * sizeof (char))), &input, n);
          tot += n;
        }
    }
  auth_printf (globals, "SERVER <|> FILE_ACCEPTED <|> SERVER\n");
  /* Add the fact that what the remote client calls <filename> is actually
   * stored in <contents> here and has a size of <bytes> bytes. */
  files_add_translation (globals, origname, contents);
  files_add_size_translation (globals, origname, bytes);

  // Check for files that are handled in a special manner access per-host
  // login information.
  gchar *origname_file = g_path_get_basename (origname);
  if (!strcmp (origname_file, ".host_sshlogins"))
    {
      build_global_host_sshlogins_map (globals, contents);
    }
  else if (!strcmp (origname_file, ".logins"))
    {
      build_global_sshlogin_info_map (globals, contents);
    }

  g_free (origname);
  g_free (origname_file);

  return 0;
}

/*----------------------------------------------------------

   Communication protocol: timestamps

 ----------------------------------------------------------*/


static int
__ntp_timestamp_scan (struct arglist *globals, char *msg)
{
  char timestr[1024];
  char *tmp;
  time_t t;
  int len;

  t = time (NULL);
  tmp = ctime (&t);
  timestr[sizeof (timestr) - 1] = '\0';
  strncpy (timestr, tmp, sizeof (timestr) - 1);
  len = strlen (timestr);
  if (timestr[len - 1] == '\n')
    timestr[len - 1] = '\0';

  auth_printf (globals, "SERVER <|> TIME <|> %s <|> %s <|> SERVER\n", msg,
               timestr);
  return 0;
}


static int
__ntp_timestamp_scan_host (struct arglist *globals, char *msg, char *host)
{
  char timestr[1024];
  char *tmp;
  time_t t;
  int len;
  char buf[1024];
  int soc;

  t = time (NULL);
  tmp = ctime (&t);
  timestr[sizeof (timestr) - 1] = '\0';
  strncpy (timestr, tmp, sizeof (timestr) - 1);
  len = strlen (timestr);
  if (timestr[len - 1] == '\n')
    timestr[len - 1] = '\0';

  soc = GPOINTER_TO_SIZE (arg_get_value (globals, "global_socket"));

  snprintf (buf, sizeof (buf),
            "SERVER <|> TIME <|> %s <|> %s <|> %s <|> SERVER\n", msg, host,
            timestr);

  internal_send (soc, buf, INTERNAL_COMM_MSG_TYPE_DATA);

  return 0;
}


int
ntp_timestamp_scan_starts (struct arglist *globals)
{
  return __ntp_timestamp_scan (globals, "SCAN_START");
}

int
ntp_timestamp_scan_ends (struct arglist *globals)
{
  return __ntp_timestamp_scan (globals, "SCAN_END");
}

int
ntp_timestamp_host_scan_starts (struct arglist *globals, char *host)
{
  return __ntp_timestamp_scan_host (globals, "HOST_START", host);
}

int
ntp_timestamp_host_scan_ends (struct arglist *globals, char *host)
{
  return __ntp_timestamp_scan_host (globals, "HOST_END", host);
}

int
ntp_timestamp_host_scan_interrupted (struct arglist *globals, char *host)
{
  return __ntp_timestamp_scan_host (globals, "HOST_INTERRUPTED", host);
}
