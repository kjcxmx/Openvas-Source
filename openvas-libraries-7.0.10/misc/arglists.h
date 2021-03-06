/* OpenVAS
 * $Id$
 * Description: Header file for module arglists.
 *
 * Authors:
 * Renaud Deraison <deraison@nessus.org> (Original pre-fork development)
 *
 * Copyright:
 * Based on work Copyright (C) 1998 - 2007 Tenable Network Security, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef OPENVAS_ARGLISTS_H
#define OPENVAS_ARGLISTS_H

struct arglist
{
  char *name;
  int type;
  void *value;
  long length;
  struct arglist *next;
  int hash;
};

/**
 * @brief Struct to cache names (keys) of arglist entries.
 *
 * A lot of entries in our arglists have the same name.
 * We use a caching system to avoid to allocate twice the same name
 *
 * This saves about 300Kb of memory, with minimal performance impact
 */
struct name_cache
{
  char *name;
  int occurences;
  struct name_cache *next;
  struct name_cache *prev;
};

#define ARG_STRING  1
#define ARG_PTR     2
#define ARG_INT     3
#define ARG_ARGLIST 4
#define ARG_STRUCT  5

char *cache_inc (const char *name);
void cache_dec (const char *name);

void arg_add_value (struct arglist *, const char *, int, long, void *);
int arg_set_value (struct arglist *, const char *, long, void *);
void *arg_get_value (struct arglist *, const char *);
int arg_get_type (struct arglist *, const char *);
void arg_dump (struct arglist *, int);
void arg_dup (struct arglist *, struct arglist *);
void arg_free (struct arglist *);
void arg_free_all (struct arglist *);
void arg_del_value (struct arglist *, const char *name);

struct arglist * str2arglist (char *str);

#endif /* OPENVAS_ARGLISTS_H */
