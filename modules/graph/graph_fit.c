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
#include <string.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/file.h>
#include <app/app.h>


/* Data for this function.*/

typedef struct {
    GtkWidget *graph;
    GtkObject *from;
    GtkObject *to;
    GtkObject *data;
    GtkWidget *chisq;
    GtkWidget *selector;
    GtkWidget *equation;
    GtkWidget *covar_row1;
    GtkWidget *covar_row2;
    GtkWidget *covar_row3;
    GtkWidget *covar_row4;
    GtkWidget *param1_des;
    GtkWidget *param2_des;
    GtkWidget *param3_des;
    GtkWidget *param4_des;
    GtkWidget *param1_fit;
    GtkWidget *param2_fit;
    GtkWidget *param3_fit;
    GtkWidget *param4_fit;
    GtkWidget *param1_init;
    GtkWidget *param2_init;
    GtkWidget *param3_init;
    GtkWidget *param4_init;
    GtkWidget *param1_res;
    GtkWidget *param2_res;
    GtkWidget *param3_res;
    GtkWidget *param4_res;
    GtkWidget *criterium;
} FitControls;

typedef struct {
    gint function_type;
    gint curve;
    gdouble from;
    gdouble to;
    gint data;
    gboolean par1_fit;
    gboolean par2_fit;
    gboolean par3_fit;
    gboolean par4_fit;
    gdouble par1_init;
    gdouble par2_init;
    gdouble par3_init;
    gdouble par4_init;
    gdouble par1_res;
    gdouble par2_res;
    gdouble par3_res;
    gdouble par4_res;
    gdouble crit; 
    GwyNLFitPresetFunction *fitfunc;
    GwyGraph *parent_graph;
    gdouble **parent_xs;
    gdouble **parent_ys;
    gint *parent_ns;
    gint parent_nofcurves;
} FitArgs;


static gboolean    module_register           (const gchar *name);
static gboolean    fit                       (GwyGraph *graph);
static gboolean    fit_dialog                (FitArgs *args);
static void        recompute                 (FitArgs *args, 
                                              FitControls *controls);
static void        reset                     (FitArgs *args, 
                                              FitControls *controls);
static void        type_changed_cb           (GObject *item, 
                                              FitArgs *args);
static void        from_changed_cb           (GtkWidget *entry, 
                                              gpointer data);
static void        to_changed_cb             (GtkWidget *entry, 
                                              gpointer data);
static void        par1_changed_cb           (GtkWidget *entry, 
                                              gpointer data);
static void        par2_changed_cb           (GtkWidget *entry, 
                                              gpointer data);
static void        par3_changed_cb           (GtkWidget *entry, 
                                              gpointer data);
static void        par4_changed_cb           (GtkWidget *entry, 
                                              gpointer data);
static void        ch1_changed_cb            (GtkToggleButton *button,
                                              FitArgs *args);
static void        ch2_changed_cb            (GtkToggleButton *button,
                                              FitArgs *args);
static void        ch3_changed_cb            (GtkToggleButton *button,
                                              FitArgs *args);
static void        ch4_changed_cb            (GtkToggleButton *button,
                                              FitArgs *args);
static void        dialog_update             (FitControls *controls, 
                                              FitArgs *args);
static void        graph_update              (FitControls *controls,
                                              FitArgs *args);
static void        get_data                  (FitArgs *args);
static void        graph_selected            (GwyGraphArea *area, 
                                              FitArgs *args);

FitControls *pcontrols;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "graph_fit",
    "Fit graph with function",
    "Petr Klapetek <klapetek@gwyddion.net>",
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
    static GwyGraphFuncInfo fit_func_info = {
        "graph_fit",
        "/_Fit graph",
        (GwyGraphFunc)&fit,
    };

    gwy_graph_func_register(name, &fit_func_info);

    return TRUE;
}

static gboolean
fit(GwyGraph *graph)
{
    gboolean ok;
    FitArgs args;
    args.fitfunc = NULL;
    args.function_type = 0;
    args.data=1;
    args.parent_graph = graph;

    get_data(&args);

    ok = fit_dialog(&args);

    return ok;
}

static void
get_data(FitArgs *args)
{
    gint i;
    
    args->parent_nofcurves = gwy_graph_get_number_of_curves(args->parent_graph);
    args->parent_xs = (gdouble **) g_malloc(args->parent_nofcurves*sizeof(gdouble*));
    args->parent_ys = (gdouble **) g_malloc(args->parent_nofcurves*sizeof(gdouble*));
    args->parent_ns = (gint *) g_malloc(args->parent_nofcurves*sizeof(gint));

    for (i=0; i<args->parent_nofcurves; i++)
    {
        args->parent_ns[i] = gwy_graph_get_data_size(args->parent_graph, i);
        args->parent_xs[i] = (gdouble *) g_malloc(args->parent_ns[i]*sizeof(gdouble));
        args->parent_ys[i] = (gdouble *) g_malloc(args->parent_ns[i]*sizeof(gdouble));
    
        gwy_graph_get_data(args->parent_graph, 
                           args->parent_xs[i], args->parent_ys[i], i);
    }
     
}

static gboolean
fit_dialog(FitArgs *args)
{
    gint i;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *dialog;
    GtkWidget *hbox;
    GtkWidget *hbox2;
    GtkWidget *table2;
    GtkWidget *vbox;
    FitControls controls;
    GwyGraphAutoProperties prop;
    gint response;

    pcontrols = &controls;
    enum { RESPONSE_RESET = 1,
        RESPONSE_FIT = 2
    };
    
    dialog = gtk_dialog_new_with_buttons(_("Fit graph"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Recompute"), RESPONSE_FIT,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox,
                       FALSE, FALSE, 4);

    /*fit equation*/
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Function definition:</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    controls.selector = gwy_option_menu_nlfitpreset(G_CALLBACK(type_changed_cb),
                                                    args, args->function_type);
    gtk_container_add(GTK_CONTAINER(vbox), controls.selector);
       
    controls.equation = gtk_label_new("f(x) =");
    gtk_misc_set_alignment(GTK_MISC(controls.equation), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), controls.equation);
 
    /*fit parameters*/
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Fitting parameters:</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    table = gtk_table_new(4, 5, FALSE);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>parameter  </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>initial  </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>result  </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
 
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>fix  </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    controls.param1_des = gtk_label_new("a");
    gtk_misc_set_alignment(GTK_MISC(controls.param1_des), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.param1_des, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    controls.param2_des = gtk_label_new("c");
    gtk_misc_set_alignment(GTK_MISC(controls.param2_des), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.param2_des, 0, 1, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param3_des = gtk_label_new("b");
    gtk_misc_set_alignment(GTK_MISC(controls.param3_des), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.param3_des, 0, 1, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param4_des = gtk_label_new("d");
    gtk_misc_set_alignment(GTK_MISC(controls.param4_des), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.param4_des, 0, 1, 4, 5,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.param1_init = gtk_entry_new_with_max_length(8);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.param1_init), 8);
    g_signal_connect(controls.param1_init, "changed",
                     G_CALLBACK(par1_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param1_init, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    
    controls.param2_init = gtk_entry_new_with_max_length(8);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.param2_init), 8);
    g_signal_connect(controls.param2_init, "changed",
                     G_CALLBACK(par2_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param2_init, 1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param3_init = gtk_entry_new_with_max_length(8);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.param3_init), 8);
    g_signal_connect(controls.param3_init, "changed",
                     G_CALLBACK(par3_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param3_init, 1, 2, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param4_init = gtk_entry_new_with_max_length(8);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.param4_init), 8);
    g_signal_connect(controls.param4_init, "changed",
                     G_CALLBACK(par4_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param4_init, 1, 2, 4, 5,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.param1_res = gtk_label_new("0.0");
    gtk_table_attach(GTK_TABLE(table), controls.param1_res, 2, 3, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    controls.param2_res = gtk_label_new("0.0");
    gtk_table_attach(GTK_TABLE(table), controls.param2_res, 2, 3, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param3_res = gtk_label_new("0.0");
    gtk_table_attach(GTK_TABLE(table), controls.param3_res, 2, 3, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param4_res = gtk_label_new("0.0");
    gtk_table_attach(GTK_TABLE(table), controls.param4_res, 2, 3, 4, 5,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.param1_fit = gtk_check_button_new();
    g_signal_connect(controls.param1_fit, "toggled",
                     G_CALLBACK(ch1_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param1_fit, 3, 4, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    controls.param2_fit = gtk_check_button_new();
    g_signal_connect(controls.param2_fit, "toggled",
                     G_CALLBACK(ch2_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param2_fit, 3, 4, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param3_fit = gtk_check_button_new();
    g_signal_connect(controls.param3_fit, "toggled",
                     G_CALLBACK(ch3_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param3_fit, 3, 4, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    controls.param4_fit = gtk_check_button_new();
    g_signal_connect(controls.param4_fit, "toggled",
                     G_CALLBACK(ch4_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.param4_fit, 3, 4, 4, 5,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    
    
    gtk_container_add(GTK_CONTAINER(vbox), table);

    
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Correlation matrix:</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    controls.covar_row1 = gtk_label_new("a: ");
    gtk_misc_set_alignment(GTK_MISC(controls.covar_row1), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), controls.covar_row1);
    
    controls.covar_row2 = gtk_label_new("b: ");
    gtk_misc_set_alignment(GTK_MISC(controls.covar_row2), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), controls.covar_row2);
     
    controls.covar_row3 = gtk_label_new("c: ");
    gtk_misc_set_alignment(GTK_MISC(controls.covar_row3), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), controls.covar_row3);
     
    controls.covar_row4 = gtk_label_new("d: ");
    gtk_misc_set_alignment(GTK_MISC(controls.covar_row4), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), controls.covar_row4);
      
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Chi-square result:</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);
 
    /*FIt area*/
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Fit area</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);
  
    table2 = gtk_table_new(2, 2, FALSE);
    controls.data = gtk_adjustment_new(args->data, 0.0, 10.0, 1, 5, 0); 
    gwy_table_attach_spinbutton(table2, 1, _("graph data curve"), _(""),
                                controls.data);
    gtk_container_add(GTK_CONTAINER(vbox), table2);
    
    
    hbox2 = gtk_hbox_new(FALSE, 0);
    table2 = gtk_table_new(2, 2, FALSE);  
    
    controls.from = gtk_adjustment_new(args->from, 0.0, 100.0, 1, 5, 0); 
    gwy_table_attach_spinbutton(table2, 1, _("from"), _(""),
                                controls.from);
    gtk_container_add(GTK_CONTAINER(hbox2), table2);
    
    table2 = gtk_table_new(2, 2, FALSE);
    controls.to = gtk_adjustment_new(args->from, 0.0, 100.0, 1, 5, 0); 
    gwy_table_attach_spinbutton(table2, 1, _("to"), _(""),
                                controls.to);
    gtk_container_add(GTK_CONTAINER(hbox2), table2);
 
    gtk_container_add(GTK_CONTAINER(vbox), hbox2);

 
     /*graph*/
    controls.graph = gwy_graph_new();
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph,
                       FALSE, FALSE, 4);
    gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);
    gwy_graph_get_autoproperties(controls.graph, &prop);
    prop.is_line = 0;
    prop.point_size = 3;
    gwy_graph_set_autoproperties(controls.graph, &prop);
    g_signal_connect(GWY_GRAPH(controls.graph)->area, "selected", 
                     G_CALLBACK(graph_selected), args);
    

    if (args->fitfunc != NULL) g_free(args->fitfunc);
    args->fitfunc = gwy_math_nlfit_get_preset(args->function_type);

    reset(args, &controls);
    dialog_update(&controls, args);
    graph_update(&controls, args);
        
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            gtk_widget_destroy(dialog);
            break;

            case RESPONSE_RESET:
            reset(args, &controls);
            break;

            case RESPONSE_FIT:
            recompute(args, &controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);
    
    
    return TRUE;
}


static void        
recompute(FitArgs *args, FitControls *controls)
{

    /*select curve data and put int in x[], y[]
     (allready in SI base units)*/
    
    /*recompute fields to be reasonably scaled,
     save scaling coefficients*/

    /*fit function though fields*/

    /*rescale results and output them*/
}

static void        
reset(FitArgs *args, FitControls *controls)
{
    char buffer[20];

    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>0)
        args->par1_init = gwy_math_nlfit_get_function_param_default(args->fitfunc, 0);
    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>1)
        args->par2_init = gwy_math_nlfit_get_function_param_default(args->fitfunc, 1);
    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>2)
        args->par3_init = gwy_math_nlfit_get_function_param_default(args->fitfunc, 2);
    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>3)
        args->par4_init = gwy_math_nlfit_get_function_param_default(args->fitfunc, 3);
     
    
    dialog_update(controls, args);
}


static void
type_changed_cb(GObject *item, FitArgs *args)
{
            
    args->function_type =
        GPOINTER_TO_INT(g_object_get_data(item,
                                            "fit-type"));

    args->fitfunc = gwy_math_nlfit_get_preset(args->function_type);

    dialog_update(pcontrols, args);
}

static void
dialog_update(FitControls *controls, FitArgs *args)
{
    char buffer[20];
    gtk_label_set_markup(GTK_LABEL(controls->equation), 
                         gwy_math_nlfit_get_function_equation(args->fitfunc));
    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>0)
    {
        gtk_widget_set_sensitive(controls->param1_des, TRUE);
        gtk_widget_set_sensitive(controls->param1_init, TRUE);
        gtk_widget_set_sensitive(controls->param1_fit, TRUE);
        gtk_label_set_markup(GTK_LABEL(controls->param1_des), 
                         gwy_math_nlfit_get_function_param_name(args->fitfunc, 0));
        g_snprintf(buffer, sizeof(buffer), "%2.3g", args->par1_init);
        gtk_entry_set_text(GTK_ENTRY(controls->param1_init), buffer);
    }
    else
    {
        gtk_widget_set_sensitive(controls->param1_des, FALSE);
        gtk_widget_set_sensitive(controls->param1_init, FALSE);
        gtk_widget_set_sensitive(controls->param1_fit, FALSE);
    }
    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>1)
    {
        gtk_widget_set_sensitive(controls->param2_des, TRUE);
        gtk_widget_set_sensitive(controls->param2_init, TRUE);
        gtk_widget_set_sensitive(controls->param2_fit, TRUE);
        gtk_label_set_markup(GTK_LABEL(controls->param2_des), 
                         gwy_math_nlfit_get_function_param_name(args->fitfunc, 1));
        g_snprintf(buffer, sizeof(buffer), "%2.3g", args->par2_init);
        gtk_entry_set_text(GTK_ENTRY(controls->param2_init), buffer);
     }
    else
    {
        gtk_widget_set_sensitive(controls->param2_des, FALSE);
        gtk_widget_set_sensitive(controls->param2_init, FALSE);
        gtk_widget_set_sensitive(controls->param2_fit, FALSE);
    }

    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>2)
    {
        gtk_widget_set_sensitive(controls->param3_des, TRUE);
        gtk_widget_set_sensitive(controls->param3_init, TRUE);
        gtk_widget_set_sensitive(controls->param3_fit, TRUE);
        gtk_label_set_markup(GTK_LABEL(controls->param3_des), 
                         gwy_math_nlfit_get_function_param_name(args->fitfunc, 2));
        g_snprintf(buffer, sizeof(buffer), "%2.3g", args->par3_init);
        gtk_entry_set_text(GTK_ENTRY(controls->param3_init), buffer);
     }
    else
    {
        gtk_widget_set_sensitive(controls->param3_des, FALSE);
        gtk_widget_set_sensitive(controls->param3_init, FALSE);
        gtk_widget_set_sensitive(controls->param3_fit, FALSE);
    }

    if (gwy_math_nlfit_get_function_nparams(args->fitfunc)>3)
    {
        gtk_widget_set_sensitive(controls->param4_des, TRUE);
        gtk_widget_set_sensitive(controls->param4_init, TRUE);
        gtk_widget_set_sensitive(controls->param4_fit, TRUE);
        gtk_label_set_markup(GTK_LABEL(controls->param4_des), 
                         gwy_math_nlfit_get_function_param_name(args->fitfunc, 3));
        g_snprintf(buffer, sizeof(buffer), "%2.3g", args->par4_init);
        gtk_entry_set_text(GTK_ENTRY(controls->param4_init), buffer);
     }
    else
    {
        gtk_widget_set_sensitive(controls->param4_des, FALSE);
        gtk_widget_set_sensitive(controls->param4_init, FALSE);
        gtk_widget_set_sensitive(controls->param4_fit, FALSE);
    }
 }


static void
graph_update(FitControls *controls, FitArgs *args)
{
    gint i;
    GString *label;
    
    /*clear graph*/
    gwy_graph_clear(GWY_GRAPH(controls->graph));


    label = g_string_new("data");
    /*add curves from parent graph*/
    for (i=0; i<args->parent_nofcurves; i++)
    {
        gwy_graph_add_datavalues(GWY_GRAPH(controls->graph), 
                                 args->parent_xs[i], 
                                 args->parent_ys[i],
                                 args->parent_ns[i], 
                                 label, NULL);    
    }

}

static void
graph_selected(GwyGraphArea *area, FitArgs *args)
{
 
    if (area->seldata->data_start == area->seldata->data_end)
    {
        args->from = 0;
        args->to = 100;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->from), args->from);   
        gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->to), args->to);

    }
    else
    {
        args->from = area->seldata->data_start;
        args->to = area->seldata->data_end;
        if (args->from > args->to) GWY_SWAP(gdouble, args->from, args->to);

        gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->from), args->from);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(pcontrols->to), args->to);
    }
        
}

static void
par1_changed_cb(GtkWidget *entry, gpointer data)
{
}

static void
par2_changed_cb(GtkWidget *entry, gpointer data)
{
}

static void
par3_changed_cb(GtkWidget *entry, gpointer data)
{
}

static void
par4_changed_cb(GtkWidget *entry, gpointer data)
{
}

static void
from_changed_cb(GtkWidget *entry, gpointer data)
{
}

static void
to_changed_cb(GtkWidget *entry, gpointer data)
{
}

static void
ch1_changed_cb(GtkToggleButton *button, FitArgs *args)
{
}

static void
ch2_changed_cb(GtkToggleButton *button, FitArgs *args)
{
}

static void
ch3_changed_cb(GtkToggleButton *button, FitArgs *args)
{
}

static void
ch4_changed_cb(GtkToggleButton *button, FitArgs *args)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
