/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define DIST_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

    
static gboolean    module_register            (const gchar *name);
static gboolean    dist                        (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "grain_dist",
    "Evaluate grain distribution",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo dist_func_info = {
        "grain_dist",
        "/_Grains/_Size distribution",
        (GwyProcessFunc)&dist,
        DIST_RUN_MODES,
    };

    gwy_process_func_register(name, &dist_func_info);

    return TRUE;
}

static gboolean
dist(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GString *lab;
    GtkWidget *window, *graph;
    GwyGraphAutoProperties prop;
    GwyDataLine *dataline;
    gint i;

    g_assert(run & DIST_RUN_MODES);
    
    /*udelat graf*/
    graph = gwy_graph_new();
    gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

    dataline = (GwyDataLine *)gwy_data_line_new(10, 10, 0);
    for (i=0; i<10; i++) dataline->data[i]=i*i;

    lab = g_string_new("Dist");
    gwy_graph_add_dataline(GWY_GRAPH(graph), dataline, 0, lab, NULL);

    window = gwy_app_graph_window_create(graph);
    g_string_free(lab, TRUE);
    g_object_unref(dataline);
    return TRUE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
