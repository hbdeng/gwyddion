/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_APP_FILE_H__
#define __GWY_APP_FILE_H__

#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* this should go to some preferences... */
extern int gwy_app_n_recent_files;

void gwy_app_file_open_cb        (void);
void gwy_app_file_open_recent_cb (GObject *item);
void gwy_app_file_save_as_cb     (void);
void gwy_app_file_save_cb        (void);
void gwy_app_file_duplicate_cb   (void);
void gwy_app_file_close_cb       (void);
void gwy_app_file_export_cb      (const gchar *name);
void gwy_app_file_import_cb      (const gchar *name);
void gwy_app_file_open_initial   (gchar **args);

/* FIXME: to be moved somewhere? refactored? */
void       gwy_app_clean_up_data            (GwyContainer *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
