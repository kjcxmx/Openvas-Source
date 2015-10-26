/* OpenVAS
* $Id: save_kb.c 18815 2014-02-25 12:03:14Z kroosec $
* Description: Saves the currently used knowledge base.
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
 * @file
 * With the appropriate preferences, Knowledge bases will be saved and loaded.
 * This has not only importance for debugging, but could also allow information
 * gain other than vulnerabilities of targets.
 *
 * Knowledge base backups are (if the appropriate preferences are set) saved
 * under (PREFIX)var/lib/openvas/kbs/(HOSTNAME) ,
 * where strings in brackets have to be replaced by the respective value.
 */

#include <unistd.h>   /* for close() */
#include <string.h>   /* for strchr() */
#include <stdlib.h>   /* for atoi() */
#include <stdio.h>    /* for sprintf() */
#include <errno.h>    /* for errno() */
#include <sys/stat.h> /* for fstat() */
#include <fcntl.h>    /* for open() */
#include <sys/time.h> /* for gettimeofday() */

#include <glib.h>

#include <openvas/misc/kb.h>         /* for kb_new */
#include <openvas/misc/plugutils.h>  /* for addslashes.h */
#include <openvas/misc/system.h>     /* for estrdup */

#include "log.h"
#include "comm.h"
#include "locks.h"

#ifndef MAP_FAILED
#define MAP_FAILED (void*)(-1)
#endif

#include "save_kb.h"

/*=========================================================================

			Private functions

===========================================================================*/
/**
 * @brief Replaces slashes in name by underscores (in-place).
 *
 * @param name String in which slashes will be replaced by underscores.
 *
 * @return Pointer to the parameter name string.
 */
static char *
filter_odd_name (char *name)
{
  char *ret = name;
  while (name[0])
    {
      /* A host name should never contain any slash. But we never  know */
      if (name[0] == '/')
        name[0] = '_';
      name++;
    }
  return ret;
}


/**
 * Returns name of the directory which contains the sessions
 * (/path/to/var/lib/openvas/kbs/).
 *
 * @return Path to knowledge base directory, has to be freed using g_free.
 */
static gchar *
kb_dirname (void)
{
  return g_build_filename (OPENVAS_STATE_DIR, "kbs", NULL);
}

/**
 * Create a kb directory.
 * XXXXX does not check for the existence of a directory and does
 * not check any error
 */
static int
kb_mkdir (char *dir)
{
  char *t;
  int ret = 0;

  dir = estrdup (dir);
  t = strchr (dir + 1, '/');
  while (t)
    {
      t[0] = '\0';
      mkdir (dir, 0700);
      t[0] = '/';
      t = strchr (t + 1, '/');
    }

  if ((ret = mkdir (dir, 0700)) < 0)
    {
      if (errno != EEXIST)
        log_write ("mkdir(%s) failed : %s\n", dir, strerror (errno));
      efree (&dir);
      return ret;
    }
  efree (&dir);
  return ret;
}


/**
 * @brief Returns file name where the kb for scan of a host can be saved/read
 * @brief from.
 *
 * From \<hostname\>, return /path/to/var/lib/openvas/kbs/\<hostname\> .
 */
static char *
kb_fname (char *hostname)
{
  gchar *dir = kb_dirname ();
  char *ret;
  char *hn = strdup (hostname);

  hn = filter_odd_name (hn);

 /** @todo use glibs *build_path functions */
  ret = emalloc (strlen (dir) + strlen (hn) + 2);
  sprintf (ret, "%s/%s", dir, hn);
  g_free (dir);
  efree (&hn);
  return ret;
}


/*
 * mmap() tends to sometimes act weirdly
 */
static char *
map_file (int file)
{
  struct stat st;
  char *ret;
  int i = 0;
  int len;

  bzero (&st, sizeof (st));
  fstat (file, &st);
  len = (int) st.st_size;
  if (len == 0)
    return NULL;

  lseek (file, 0, SEEK_SET);
  ret = emalloc (len + 1);
  while (i < len)
    {
      int e = read (file, ret + i, len - i);
      if (e > 0)
        i += e;
      else
        {
          log_write ("read(%d, buf, %d) failed : %s\n", file, len,
                     strerror (errno));
          efree (&ret);
          lseek (file, len, SEEK_SET);
          return NULL;
        }
    }

  lseek (file, len, SEEK_SET);
  return ret;
}

static int
save_kb_entry_present_already (struct arglist *globals, char *name, char *value)
{
  char *buf;
  int fd;
  char *req;

  fd = GPOINTER_TO_SIZE (arg_get_value (globals, "save_kb"));
  if (fd <= 0)
    return -1;

  buf = map_file (fd);
  if (buf)
    {
      int ret;
      req = emalloc (strlen (name) + strlen (value) + 2);
      sprintf (req, "%s=%s", name, value);
      if (strstr (buf, req))
        ret = 1;
      else
        ret = 0;
      efree (&buf);
      efree (&req);
      return ret;
    }
  return -1;
}

static int
save_kb_rm_entry_value (struct arglist *globals, char *name, char *value)
{
  char *buf;
  int fd;
  char *req;


  fd = GPOINTER_TO_SIZE (arg_get_value (globals, "save_kb"));
  if (fd <= 0)
    return -1;

  buf = map_file (fd);
  if (buf)
    {
      char *t;
      if (value)
        {
          req = emalloc (strlen (name) + strlen (value) + 2);
          sprintf (req, "%s=%s", name, value);
        }
      else
        req = estrdup (name);

      t = strstr (buf, req);
      if (t)
        {
          char *end;

          while (t[0] != '\n')
            {
              if (t == buf)
                break;
              else
                t--;
            }

          if (t[0] == '\n')
            t++;
          end = strchr (t, '\n');
          t[0] = '\0';
          if (end)
            {
              end[0] = '\0';
              end++;
            }

          if ((lseek (fd, 0, SEEK_SET)) < 0)
            {
              log_write ("lseek() failed - %s\n", strerror (errno));
            }

          if ((ftruncate (fd, 0)) < 0)
            {
              log_write ("ftruncate() failed - %s\n", strerror (errno));
            }


          if (write (fd, buf, strlen (buf)) < 0)
            {
              log_write ("write() failed - %s\n", strerror (errno));
            }

          if (end)
            {
              if ((write (fd, end, strlen (end))) < 0)
                log_write ("write() failed - %s\n", strerror (errno));
            }
        }
      efree (&buf);
      efree (&req);
      lseek (fd, 0, SEEK_END);
    }
  return 0;
}

static int
save_kb_rm_entry (struct arglist *globals, char *name)
{
  return save_kb_rm_entry_value (globals, name, NULL);
}

/**
 * @brief Writes an entry to a knowledge base file.
 *
 * The entry will look like:
 * 1254307384 1 Banner/22=SSH-2.0-OpenSSH_5.1p1 Debian-5\r\n
 * where the first value is a timestamp, the second item is the \ref type,
 * and the string before the equalsign in the third item is the key for the
 * knowledge base and the rest the value for that key.
 *
 * Duplicates for keys starting with:
 *   Successful/...
 *   SentData/...
 *   Launched/...
 * are not created (existing values are removed first).
 * Any items starting with /tmp/, NIDS/ or Settings/ are not written to the file
 * but rather ignored.
 *
 * @param name Key of the kb-item.
 * @return -1 if invalid file handle to write to or any parameter is NULL, 0
 *         otherwise.
 */
static int
save_kb_write (struct arglist *globals, char *hostname, char *name, char *value,
               int type)
{
  int fd;
  char *str;
  int e;
  struct timeval now;

  if (!globals || !hostname || !name || !value)
    return -1;

  fd = GPOINTER_TO_SIZE (arg_get_value (globals, "save_kb"));
  if (fd <= 0)
    {
      log_write ("Can not find KB fd for %s\n", hostname);
      return -1;
    }

  /* Skip temporary KB entries */
  if (!strncmp (name, "/tmp/", 4) || !strncmp (name, "NIDS/", 5)
      || !strncmp (name, "Settings/", 9))
    return 0;

  /* Don't save sensitive information */
  if (strncmp (name, "Secret/", 7) == 0)
    return 0;

  /* Avoid duplicates for these families */
  if (!strncmp (name, "Success/", strlen ("Success/"))
      || !strncmp (name, "Launched/", strlen ("Launched/"))
      || !strncmp (name, "SentData/", strlen ("SentData/")))
    {
      save_kb_rm_entry (globals, name);
    }

  if (save_kb_entry_present_already (globals, name, value))
    {
      save_kb_rm_entry_value (globals, name, value);
    }

  str = emalloc (strlen (name) + strlen (value) + 25);
  gettimeofday (&now, NULL);
  sprintf (str, "%ld %d %s=%s\n", (long) now.tv_sec, type, name, value);

  /** @todo Fix a bug (most probably race condition). Although following write
   * call does return > 0, sometimes the content never reaches the file,
   * especially for big amount of data in value (e.g. big file contents) */
  e = write (fd, str, strlen (str));
  if (e < 0)
    {
      log_write ("Write kb error - %s\n", strerror (errno));
    }
  efree (&str);
  return 0;
}


/*======================================================================

	                 Public functions

 =======================================================================*/

/**
 * @brief Initialize a new KB that will be saved.
 *
 * The indices of all the opened KB are in a hashlist in
 * globals, saved under the name "save_kb". This makes no sense
 * at this time, as the test of each host is done in a separate
 * process, but this allows us to regroup easily these in
 * the future.
 */
int
save_kb_new (struct arglist *globals, char *hostname)
{
  char *fname;
  char *dir;
  int f;

  if (hostname == NULL)
    return -1;

  dir = kb_dirname ();
  kb_mkdir (dir);
  efree (&dir);

  fname = kb_fname (hostname);

  if (file_locked (fname))
    {
      efree (&fname);
      return 0;
    }
  unlink (fname);               /* delete the previous kb */
  f = open (fname, O_CREAT | O_RDWR | O_EXCL, 0640);
  if (f < 0)
    {
      log_write ("Can not save KB for %s - %s", hostname,
                 strerror (errno));
      efree (&fname);
      return -1;
    }
  else
    {
      file_lock (fname);
      log_write ("New KB will be saved as %s", fname);
      if (arg_get_value (globals, "save_kb"))
        arg_set_value (globals, "save_kb", sizeof (gpointer),
                       GSIZE_TO_POINTER (f));
      else
        arg_add_value (globals, "save_kb", ARG_INT, sizeof (gpointer),
                       GSIZE_TO_POINTER (f));
    }
  return 0;
}


void
save_kb_close (struct arglist *globals, char *hostname)
{
  int fd = GPOINTER_TO_SIZE (arg_get_value (globals, "save_kb"));
  char *fname = kb_fname (hostname);
  if (fd > 0)
    close (fd);
  file_unlock (fname);
  efree (&fname);
}

/**
 * @return 1 if we already saved a KB for this host, less than \<max_age\>
 *         seconds ago. If \<max_age\> equals zero, then the age is not taken in
 *         account (returns true if a knowledge base exists).
 */
int
save_kb_exists (char *hostname)
{
  char *fname = kb_fname (hostname);
  FILE *f;

  if (file_locked (fname))
    {
      efree (&fname);
      return 0;
    }
  f = fopen (fname, "r");
  efree (&fname);
  if (!f)
    return 0;
  else
    {
      fclose (f);
      return 1;
    }
}


int
save_kb_write_str (struct arglist *globals, char *hostname, char *name,
                   char *value)
{
  char *newvalue = addslashes (value);
  int e;

  e = save_kb_write (globals, hostname, name, newvalue, ARG_STRING);
  efree (&newvalue);
  return e;
}


int
save_kb_write_int (struct arglist *globals, char *hostname, char *name,
                   int value)
{
  static char asc_value[25];
  int e;
  sprintf (asc_value, "%d", value);
  e = save_kb_write (globals, hostname, name, asc_value, ARG_INT);
  bzero (asc_value, sizeof (asc_value));
  return e;
}



/**
 * @brief Restores a copy of the knowledge base
 */
int
save_kb_restore_backup (char *hostname)
{
  char *fname = kb_fname (hostname);
  char *bakname;
  int fd;

  bakname = emalloc (strlen (fname) + 5);
  strcat (bakname, fname);
  strcat (bakname, ".bak");

  unlink (fname);
  if ((fd = open (bakname, O_RDONLY)) >= 0)
    {
      close (fd);
      rename (bakname, fname);
    }
  return 0;
}

/**
 * @brief Makes a copy of the knowledge base
 */
int
save_kb_backup (char *hostname)
{
  char *fname = kb_fname (hostname);
  char *newname = NULL;
  int fd_src = -1, fd_dst = -1;

  if (file_locked (fname))
    {
      log_write ("%s is locked\n", fname);
      goto failed1;
    }

  file_lock (fname);

  newname = emalloc (strlen (fname) + 5);
  strcat (newname, fname);
  strcat (newname, ".bak");

  if ((fd_src = open (fname, O_RDONLY)) >= 0)
    {
      char buf[4096];
      int n;
      fd_dst = open (newname, O_WRONLY | O_CREAT | O_TRUNC, 0640);
      if (fd_dst < 0)
        {
          log_write ("save_kb_backup failed : %s", strerror (errno));
          close (fd_src);
          goto failed;
        }
      bzero (buf, sizeof (buf));
      while ((n = read (fd_src, buf, sizeof (buf))) > 0)
        {
          int m = 0;
          while (m != n)
            {
              int e = write (fd_dst, &(buf[m]), n - m);
              if (e < 0)
                {
                  log_write ("save_kb_backup failed : %s", strerror (errno));
                  close (fd_src);
                  close (fd_dst);
                  goto failed;
                }
              m += e;
            }
          bzero (buf, sizeof (buf));
        }
    }
  else
    log_write ("save_kb_backup failed : %s\n", strerror (errno));

  close (fd_src);
  close (fd_dst);
  efree (&newname);
  file_unlock (fname);
  efree (&fname);
  return 0;
failed:
  file_unlock (fname);
failed1:
  efree (&fname);
  efree (&newname);
  return -1;
}


/**
 * @brief Restores a previously saved knowledge base.
 *
 * The KB entry 'Host/dead' is ignored, as well as all the
 * entries starting with '/tmp/'.
 */
kb_t
save_kb_load_kb (struct arglist *globals, char *hostname)
{
  char *fname = kb_fname (hostname);
  FILE *f;
  int fd;
  kb_t kb;
  char buf[4096];
  long max_age = 864000;

  if (file_locked (fname))
    {
      efree (&fname);
      return NULL;
    }
  f = fopen (fname, "r");
  if (!f)
    {
      log_write ("Could not open %s - kb won't be restored for %s\n",
                 fname, hostname);
      efree (&fname);
      return NULL;
    }
  bzero (buf, sizeof (buf));
  if (fgets (buf, sizeof (buf) - 1, f) == NULL)
    {
      log_write ("Could not read %s - kb won't be restored for %s\n",
                 fname, hostname);
      efree (&fname);
      fclose (f);
      return NULL;
    }

  kb = kb_new ();

  /* Ignore the date */
  bzero (buf, sizeof (buf));

  while (fgets (buf, sizeof (buf) - 1, f))
    {
      int type;
      char *name, *value, *t;
      struct timeval then, now;

      buf[strlen (buf) - 1] = '\0';     /* chomp(buf) */
      t = strchr (buf, ' ');
      if (!t)
        continue;

      t[0] = '\0';

      then.tv_sec = atol (buf);
      t[0] = ' ';
      t++;
      type = atoi (t);
      t = strchr (t, ' ');
      if (!t)
        continue;

      t[0] = ' ';
      t++;
      name = t;
      t = strchr (name, '=');
      if (!t)
        continue;

      t[0] = '\0';
      name = strdup (name);
      t[0] = ' ';
      t++;
      value = strdup (t);

      if (strcmp (name, "Host/dead") && strncmp (name, "/tmp/", 4)
          && strcmp (name, "Host/ping_failed"))
        {
          gettimeofday (&now, NULL);
          if (now.tv_sec - then.tv_sec > max_age)
            {
              /*
                 log_write("discarding %s because it's too old\n",
                 name,
                 (now.tv_sec - then.tv_sec));
               */
            }
          else
            {
              if (type == ARG_STRING)
                {
                  char *tmp = rmslashes (value);
                  kb_item_add_str (kb, name, tmp);
                  efree (&tmp);
                }
              else if (type == ARG_INT)
                kb_item_add_int (kb, name, atoi (value));
            }
        }
      efree (&value);
      efree (&name);
      bzero (buf, sizeof (buf));
    }
  fclose (f);

  /*
   * Re-open the file
   */
  fd = open (fname, O_RDWR);
  efree (&fname);
  if (fd > 0)
    {
      lseek (fd, 0, SEEK_END);
      if (arg_get_value (globals, "save_kb"))
        arg_set_value (globals, "save_kb", ARG_INT, GSIZE_TO_POINTER (fd));
      else
        arg_add_value (globals, "save_kb", ARG_INT, sizeof (gpointer),
                       GSIZE_TO_POINTER (fd));
    }
  else
    log_write ("ERROR - %s\n", strerror (errno));
  return kb;
}

/**
 * @return 1 if a network scan is in progress.
 * @todo This operation is possibly executed often (with every kb modification).
 *       Evaluate wether the preference can change during a scan, consider the
 *       use of a static variable.
 */
int
save_kb (struct arglist *globals)
{
  char *value;

  if (!globals)
    return 0;

  value = arg_get_value (globals, "network_scan_status");
  if (value && !strcmp (value, "busy"))
    return 1;

  return 0;
}
