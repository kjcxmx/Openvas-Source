/* OpenVAS
* $Id: save_kb.h 18562 2014-01-10 22:19:46Z jan $
* Description: save_kb.c header.
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
*
*
*/


#ifndef SAVE_KB_H__
#define SAVE_KB_H__

#include <openvas/misc/arglists.h>   /* for struct arglist */

int save_kb_new (struct arglist *, char *);
void save_kb_close (struct arglist *, char *);

int save_kb_backup (char *);
int save_kb_restore_backup (char *);

int save_kb_write_int (struct arglist *, char *, char *, int);
int save_kb_write_str (struct arglist *, char *, char *, char *);

int save_kb_exists (char *);
kb_t save_kb_load_kb (struct arglist *, char *);

int save_kb (struct arglist *);

#endif
