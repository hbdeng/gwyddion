/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "preview.h"

#define SCARS_MARK_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)
#define SCARS_REMOVE_RUN_MODES GWY_RUN_IMMEDIATE

typedef enum {
    FEATURES_POSITIVE = 1 << 0,
    FEATURES_NEGATIVE = 1 << 2,
    FEATURES_BOTH     = (FEATURES_POSITIVE | FEATURES_NEGATIVE),
} GwyFeaturesType;

enum {
    MAX_LENGTH = 1024
};

typedef struct {
    GwyFeaturesType type;
    gdouble threshold_high;
    gdouble threshold_low;
    gint min_len;
    gint max_width;
    gboolean update;
} ScarsArgs;

typedef struct {
    ScarsArgs *args;
    GSList *type;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkObject *threshold_high;
    GtkObject *threshold_low;
    GtkObject *min_len;
    GtkObject *max_width;
    GtkWidget *color_button;
    GtkWidget *update;
    GwyContainer *mydata;
    gboolean computed;
    gboolean in_init;
} ScarsControls;

static gboolean module_register                    (void);
static void     scars_remove                       (GwyContainer *data,
                                                    GwyRunType run);
static void     scars_mark                         (GwyContainer *data,
                                                    GwyRunType run);
static void     run_noninteractive                 (ScarsArgs *args,
                                                    GwyContainer *data,
                                                    GwyDataField *dfield,
                                                    GQuark mquark);
static void     scars_mark_dialog                  (ScarsArgs *args,
                                                    GwyContainer *data,
                                                    GwyDataField *dfield,
                                                    gint id,
                                                    GQuark mquark);
static void     scars_mark_dialog_update_controls  (ScarsControls *controls,
                                                    ScarsArgs *args);
static void     scars_mark_dialog_update_values    (ScarsControls *controls,
                                                    ScarsArgs *args);
static void     scars_mark_dialog_update_thresholds(GtkObject *adj,
                                                    ScarsControls *controls);
static void     scars_invalidate                   (ScarsControls *controls);
static void     update_change_cb                   (ScarsControls *controls);
static void     preview                            (ScarsControls *controls,
                                                    ScarsArgs *args);
static void     scars_mark_load_args               (GwyContainer *container,
                                                    ScarsArgs *args);
static void     scars_mark_save_args               (GwyContainer *container,
                                                    ScarsArgs *args);

static const ScarsArgs scars_defaults = {
    FEATURES_BOTH,
    0.666,
    0.25,
    16,
    4,
    TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Marks and/or removes scars (horizontal linear artifacts)."),
    "Yeti <yeti@gwyddion.net>",
    "1.13",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("scars_mark",
                              (GwyProcessFunc)&scars_mark,
                              N_("/_Correct Data/M_ark Scars..."),
                              GWY_STOCK_SCARS,
                              SCARS_MARK_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark horizontal scars (strokes)"));
    gwy_process_func_register("scars_remove",
                              (GwyProcessFunc)&scars_remove,
                              N_("/_Correct Data/Remove _Scars"),
                              GWY_STOCK_SCARS,
                              SCARS_REMOVE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct horizontal scars (strokes)"));

    return TRUE;
}

static void
mark_scars(GwyDataField *dfield,
           GwyDataField *mfield,
           const ScarsArgs *args)
{
    GwyDataField *tmp;

    switch (args->type) {
        case FEATURES_POSITIVE:
        case FEATURES_NEGATIVE:
        gwy_data_field_mark_scars(dfield, mfield,
                                  args->threshold_high, args->threshold_low,
                                  args->min_len, args->max_width,
                                  args->type == FEATURES_NEGATIVE);
        break;

        case FEATURES_BOTH:
        gwy_data_field_mark_scars(dfield, mfield,
                                  args->threshold_high, args->threshold_low,
                                  args->min_len, args->max_width, FALSE);
        tmp = gwy_data_field_new_alike(dfield, FALSE);
        gwy_data_field_mark_scars(dfield, tmp,
                                  args->threshold_high, args->threshold_low,
                                  args->min_len, args->max_width, TRUE);
        gwy_data_field_max_of_fields(mfield, mfield, tmp);
        g_object_unref(tmp);
        break;
    }
}

static void
scars_remove(GwyContainer *data, GwyRunType run)
{
    ScarsArgs args;
    GwyDataField *dfield, *mfield;
    GQuark dquark;
    gint xres, yres, i, j, k;
    gdouble *d, *m;
    gint id;

    g_return_if_fail(run & SCARS_REMOVE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);
    scars_mark_load_args(gwy_app_settings_get(), &args);
    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);

    mfield = create_mask_field(dfield);
    mark_scars(dfield, mfield, &args);
    m = gwy_data_field_get_data(mfield);

    /* interpolate */
    for (i = 1; i < yres-1; i++) {
        for (j = 0; j < xres; j++) {
            if (m[i*xres + j] > 0.0) {
                gdouble first, last;
                gint width;

                first = d[(i - 1)*xres + j];
                for (k = 1; m[(i + k)*xres + j] > 0.0; k++)
                    ;
                last = d[(i + k)*xres + j];
                width = k + 1;
                while (k) {
                    gdouble x = (gdouble)k/width;

                    d[(i + k - 1)*xres + j] = x*last + (1.0 - x)*first;
                    m[(i + k - 1)*xres + j] = 0.0;
                    k--;
                }
            }
        }
    }
    g_object_unref(mfield);

    gwy_data_field_data_changed(dfield);
    gwy_app_channel_log_add(data, id, id, "proc::scars_remove",
                            "settings-name", "scars",
                            NULL);
}

static void
scars_mark(GwyContainer *data, GwyRunType run)
{
    ScarsArgs args;
    GwyDataField *dfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & SCARS_MARK_RUN_MODES);
    scars_mark_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && mquark);

    if (run == GWY_RUN_IMMEDIATE) {
        run_noninteractive(&args, data, dfield, mquark);
        gwy_app_channel_log_add(data, id, id, "proc::scars_remove",
                                "settings-name", "scars",
                                NULL);
    }
    else
        scars_mark_dialog(&args, data, dfield, id, mquark);
}

static void
run_noninteractive(ScarsArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GQuark mquark)
{
    GwyDataField *mfield;

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    mfield = create_mask_field(dfield);
    mark_scars(dfield, mfield, args);
    gwy_container_set_object(data, mquark, mfield);
    g_object_unref(mfield);
}

static void
scars_mark_dialog(ScarsArgs *args,
                  GwyContainer *data,
                  GwyDataField *dfield,
                  gint id,
                  GQuark mquark)
{
    static const GwyEnum types[] = {
        { N_("Positive"), FEATURES_POSITIVE, },
        { N_("Negative"), FEATURES_NEGATIVE, },
        { N_("Both"),     FEATURES_BOTH,     },
    };
    GtkWidget *dialog, *table, *hbox, *label;
    GwyDataField *mfield;
    ScarsControls controls;
    gint response;
    GSList *group;
    gboolean temp;
    gint row;

    controls.in_init = TRUE;
    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Mark Scars"), NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = create_preview(controls.mydata, 0, PREVIEW_SIZE, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(11, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.max_width = gtk_adjustment_new(args->max_width,
                                            1.0, 16.0, 1, 3, 0);
    gwy_table_attach_hscale(table, row++, _("Maximum _width:"), "px",
                            controls.max_width, 0);
    g_signal_connect_swapped(controls.max_width, "value-changed",
                             G_CALLBACK(scars_invalidate), &controls);

    controls.min_len = gtk_adjustment_new(args->min_len,
                                          1.0, MAX_LENGTH, 1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("Minimum _length:"), "px",
                            controls.min_len, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.min_len, "value-changed",
                             G_CALLBACK(scars_invalidate), &controls);

    controls.threshold_high = gtk_adjustment_new(args->threshold_high,
                                                 0.0, 2.0, 0.01, 0.1, 0);
    gwy_table_attach_hscale(table, row++, _("_Hard threshold:"),
                            _("RMS"), controls.threshold_high, 0);
    g_signal_connect(controls.threshold_high, "value-changed",
                     G_CALLBACK(scars_mark_dialog_update_thresholds),
                     &controls);

    controls.threshold_low = gtk_adjustment_new(args->threshold_low,
                                                0.0, 2.0, 0.01, 0.1, 0);
    gwy_table_attach_hscale(table, row, _("_Soft threshold:"), _("RMS"),
                            controls.threshold_low, 0);
    g_signal_connect(controls.threshold_low, "value-changed",
                     G_CALLBACK(scars_mark_dialog_update_thresholds),
                     &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Scars type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    group = gwy_radio_buttons_create(types, G_N_ELEMENTS(types),
                                     NULL, NULL, args->type);
    controls.type = group;
    while (group) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(group->data),
                         0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        g_signal_connect_swapped(group->data, "toggled",
                                 G_CALLBACK(scars_invalidate), &controls);
        group = g_slist_next(group);
        row++;
    }

    controls.color_button = create_mask_color_button(controls.mydata, dialog,
                                                     0);
    gwy_table_attach_hscale(table, row, _("_Mask color:"), NULL,
                            GTK_OBJECT(controls.color_button),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_change_cb), &controls);
    row++;

    scars_invalidate(&controls);
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    if (args->update) {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);
        preview(&controls, args);
    }

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            scars_mark_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            scars_mark_save_args(gwy_app_settings_get(), args);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = scars_defaults;
            args->update = temp;
            controls.in_init = TRUE;
            scars_mark_dialog_update_controls(&controls, args);
            controls.in_init = FALSE;
            preview(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            scars_mark_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    scars_mark_dialog_update_values(&controls, args);
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);
    scars_mark_save_args(gwy_app_settings_get(), args);

    if (controls.computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, dfield, mquark);
    }

    gwy_app_channel_log_add(data, id, id, "proc::scars_remove",
                            "settings-name", "scars",
                            NULL);
}

static void
scars_mark_dialog_update_thresholds(GtkObject *adj,
                                    ScarsControls *controls)
{
    static gboolean in_update = FALSE;
    ScarsArgs *args;

    if (in_update)
        return;

    in_update = TRUE;
    args = controls->args;
    if (adj == controls->threshold_high) {
        args->threshold_high = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
        if (args->threshold_low > args->threshold_high)
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_low),
                                     args->threshold_high);
    }
    else if (adj == controls->threshold_low) {
        args->threshold_low = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
        if (args->threshold_low > args->threshold_high)
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_high),
                                     args->threshold_low);
    }
    else {
        g_assert_not_reached();
    }

    in_update = FALSE;
    scars_invalidate(controls);
}

static void
scars_mark_dialog_update_controls(ScarsControls *controls,
                                  ScarsArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_high),
                             args->threshold_high);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_low),
                             args->threshold_low);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->min_len),
                             args->min_len);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_width),
                             args->max_width);
    gwy_radio_buttons_set_current(controls->type, args->type);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
}

static void
scars_mark_dialog_update_values(ScarsControls *controls,
                                ScarsArgs *args)
{
    args->threshold_high
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_high));
    args->threshold_low
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_low));
    args->min_len = gwy_adjustment_get_int(controls->min_len);
    args->max_width = gwy_adjustment_get_int(controls->max_width);
    args->type = gwy_radio_buttons_get_current(controls->type);
    args->update
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));
}

static void
scars_invalidate(ScarsControls *controls)
{
    controls->computed = FALSE;

    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init) {
        scars_mark_dialog_update_values(controls, controls->args);
        preview(controls, controls->args);
    }
}

static void
update_change_cb(ScarsControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);

    if (controls->args->update)
        scars_invalidate(controls);
}

static void
preview(ScarsControls *controls,
        ScarsArgs *args)
{
    GwyDataField *mask, *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /* Set up the mask */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);
    }
    gwy_data_field_copy(dfield, mask, FALSE);
    mark_scars(dfield, mask, args);
    gwy_data_field_data_changed(mask);

    controls->computed = TRUE;
}

static const gchar type_key[]           = "/module/scars/type";
static const gchar threshold_low_key[]  = "/module/scars/threshold_low";
static const gchar threshold_high_key[] = "/module/scars/threshold_high";
static const gchar min_len_key[]        = "/module/scars/min_len";
static const gchar max_width_key[]      = "/module/scars/max_width";
static const gchar update_key[]         = "/module/scars/update";

static void
scars_mark_sanitize_args(ScarsArgs *args)
{
    args->type = CLAMP(args->type, FEATURES_POSITIVE, FEATURES_BOTH);
    args->threshold_low = MAX(args->threshold_low, 0.0);
    args->threshold_high = MAX(args->threshold_low, args->threshold_high);
    args->min_len = CLAMP(args->min_len, 1, MAX_LENGTH);
    args->max_width = CLAMP(args->max_width, 1, 16);
    args->update = !!args->update;
}

static void
scars_mark_load_args(GwyContainer *container,
                     ScarsArgs *args)
{
    *args = scars_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_double_by_name(container, threshold_high_key,
                                     &args->threshold_high);
    gwy_container_gis_double_by_name(container, threshold_low_key,
                                     &args->threshold_low);
    gwy_container_gis_int32_by_name(container, min_len_key, &args->min_len);
    gwy_container_gis_int32_by_name(container, max_width_key, &args->max_width);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    scars_mark_sanitize_args(args);
}

static void
scars_mark_save_args(GwyContainer *container,
                     ScarsArgs *args)
{
    gwy_container_set_enum_by_name(container, type_key, args->type);
    gwy_container_set_double_by_name(container, threshold_high_key,
                                     args->threshold_high);
    gwy_container_set_double_by_name(container, threshold_low_key,
                                     args->threshold_low);
    gwy_container_set_int32_by_name(container, min_len_key, args->min_len);
    gwy_container_set_int32_by_name(container, max_width_key, args->max_width);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
