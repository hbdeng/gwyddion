/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * Keyence microscope VK
 * *.vk4
 * Read
 **/
#define DEBUG 1
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/dataline.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "VK4_"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define MAGIC0 "\x00\x00\x00\x00"
#define MAGIC0_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".vk4"

enum {
    KEYENCE_HEADER_SIZE = 12,
    KEYENCE_OFFSET_TABLE_SIZE = 72,
    KEYENCE_MEASUREMENT_CONDITIONS_MIN_SIZE = 304,
    KEYENCE_ASSEMBLY_INFO_SIZE = 16,
    KEYENCE_ASSEMBLY_CONDITIONS_SIZE = 8,
    KEYENCE_ASSEMBLY_HEADERS_SIZE = (KEYENCE_ASSEMBLY_INFO_SIZE
                                     + KEYENCE_ASSEMBLY_CONDITIONS_SIZE),
    KEYENCE_ASSEMBLY_FILE_SIZE = 532,
    KEYENCE_TRUE_COLOR_IMAGE_MIN_SIZE = 20,
    KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE = 796,
};

typedef enum {
    KEYENCE_NORMAL_FILE = 0,
    KEYENCE_ASSEMBLY_FILE = 1,
    KEYENCE_ASSEMBLY_FILE_UNICODE = 2,
} KeyenceFileType;

typedef struct {
    guchar magic[4];
    guchar dll_version[4];
    guchar file_type[4];
} KeyenceHeader;

typedef struct {
    guint setting;
    guint color_peak;
    guint color_light;
    guint light0;
    guint light1;
    guint light2;
    guint height0;
    guint height1;
    guint height2;
    guint color_peak_thumbnail;
    guint color_thumbnail;
    guint light_thumbnail;
    guint height_thumbnail;
    guint assemble;
    guint line_measure;
    guint line_thickness;
    guint string_data;
    guint reserved;
} KeyenceOffsetTable;

typedef struct {
    guint size;
    guint year;
    guint month;
    guint day;
    guint hour;
    guint minute;
    guint second;
    guint diff_utc_by_minutes;
    guint image_attributes;
    guint user_interface_mode;
    guint color_composite_mode;
    guint num_layer;
    guint run_mode;
    guint peak_mode;
    guint sharpening_level;
    guint speed;
    guint distance;
    guint pitch;
    guint optical_zoom;
    guint num_line;
    guint line0_pos;
    guint reserved1[3];
    guint lens_mag;
    guint pmt_gain_mode;
    guint pmt_gain;
    guint pmt_offset;
    guint nd_filter;
    guint reserved2;
    guint persist_count;
    guint shutter_speed_mode;
    guint shutter_speed;
    guint white_balance_mode;
    guint white_balance_red;
    guint white_balance_blue;
    guint camera_gain;
    guint plane_compensation;
    guint xy_length_unit;
    guint z_length_unit;
    guint xy_decimal_place;
    guint z_decimal_place;
    guint x_length_per_pixel;
    guint y_length_per_pixel;
    guint z_length_per_digit;
    guint reserved3[5];
    guint light_filter_type;
    guint reserved4;
    guint gamma_reverse;
    guint gamma;
    guint offset;
    guint ccd_bw_offset;
    guint numerical_aperture;
    guint head_type;
    guint pmt_gain2;
    guint omit_color_image;
    guint lens_id;
    guint light_lut_mode;
    guint light_lut_in0;
    guint light_lut_out0;
    guint light_lut_in1;
    guint light_lut_out1;
    guint light_lut_in2;
    guint light_lut_out2;
    guint light_lut_in3;
    guint light_lut_out3;
    guint light_lut_in4;
    guint light_lut_out4;
    guint upper_position;
    guint lower_position;
    guint light_effective_bit_depth;
    guint height_effective_bit_depth;
    /* XXX: There is much more... */
} KeyenceMeasurementConditions;

typedef struct {
    guint size;   /* The size of *all* assembly-related blocks. */
    KeyenceFileType file_type;
    guint stage_type;
    guint x_position;
    guint y_position;
} KeyenceAssemblyInformation;

typedef struct {
    guint auto_adjustment;
    guint source;
    guint thin_out;
    guint count_x;
    guint count_y;
} KeyenceAssemblyConditions;

typedef struct {
    guint16 source_file[260];   /* This is Microsoft's wchar_t. */
    guint pos_x;
    guint pos_y;
    guint datums_pos;
    guint fix_distance;
    guint distance_x;
    guint distance_y;
} KeyenceAssemblyFile;

typedef struct {
    guint width;
    guint height;
    guint bit_depth;
    guint compression;
    guint byte_size;
    const guchar *data;
} KeyenceTrueColorImage;

typedef struct {
    guint width;
    guint height;
    guint bit_depth;
    guint compression;
    guint byte_size;
    guint palette_range_min;
    guint palette_range_max;
    guchar palette[0x300];
    const guchar *data;
} KeyenceFalseColorImage;

typedef struct {
    KeyenceHeader header;
    KeyenceOffsetTable offset_table;
    KeyenceMeasurementConditions meas_conds;
    /* The rest is optional. */
    KeyenceAssemblyInformation assembly_info;
    KeyenceAssemblyConditions assembly_conds;
    guint assembly_nfiles;
    guint nimages;
    KeyenceAssemblyFile *assembly_files;
    KeyenceFalseColorImage light0;
    KeyenceFalseColorImage light1;
    KeyenceFalseColorImage light2;
    KeyenceFalseColorImage height0;
    KeyenceFalseColorImage height1;
    KeyenceFalseColorImage height2;
    /* Raw file contents. */
    guchar *buffer;
    gsize size;
} KeyenceFile;

static gboolean      module_register   (void);
static gint          keyence_detect    (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* keyence_load      (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static void          free_file         (KeyenceFile *kfile);
static gboolean      read_header       (const guchar **p,
                                        gsize *size,
                                        KeyenceHeader *header,
                                        GError **error);
static gboolean      read_offset_table (const guchar **p,
                                        gsize *size,
                                        KeyenceOffsetTable *offsettable,
                                        GError **error);
static gboolean      read_meas_conds   (const guchar **p,
                                        gsize *size,
                                        KeyenceMeasurementConditions *measconds,
                                        GError **error);
static gboolean      read_assembly_info(KeyenceFile *kfile,
                                        GError **error);
static gboolean      read_data_images  (KeyenceFile *kfile,
                                        GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Keyence VK4 files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("keyence",
                           N_("Omicron flat files "),
                           (GwyFileDetectFunc)&keyence_detect,
                           (GwyFileLoadFunc)&keyence_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
keyence_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE + KEYENCE_HEADER_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
        && memcmp(fileinfo->head + 8, MAGIC0, MAGIC0_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
keyence_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    KeyenceFile kfile;
    GwyContainer *data = NULL;
    guchar* buffer = NULL;
    const guchar *p;
    gsize size = 0, remsize;
    GError *err = NULL;

    gwy_clear(&kfile, 1);
    if (!gwy_file_get_contents(filename, &kfile.buffer, &kfile.size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    remsize = size;
    p = buffer;

    if (!read_header(&p, &remsize, &kfile.header, error)
        || !read_offset_table(&p, &remsize, &kfile.offset_table, error)
        || !read_meas_conds(&p, &remsize, &kfile.meas_conds, error)
        || !read_assembly_info(&kfile, error)
        || !read_data_images(&kfile, error))
        goto fail;

    if (!kfile.nimages) {
        err_NO_DATA(error);
        goto fail;
    }

    err_NO_DATA(error);

fail:
    free_file(&kfile);
    return data;
}

static void
free_file(KeyenceFile *kfile)
{
    g_free(kfile->assembly_files);
    gwy_file_abandon_contents(kfile->buffer, kfile->size, NULL);
}

static void
err_TRUNCATED(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File is truncated."));
}

static gboolean
read_header(const guchar **p,
            gsize *size,
            KeyenceHeader *header,
            GError **error)
{
    if (*size < KEYENCE_HEADER_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    get_CHARARRAY(header->magic, p);
    get_CHARARRAY(header->dll_version, p);
    get_CHARARRAY(header->file_type, p);
    if (memcmp(header->magic, MAGIC, MAGIC_SIZE) != 0
        || memcmp(header->file_type, MAGIC0, MAGIC0_SIZE) != 0) {
        err_FILE_TYPE(error, "Keyence VK4");
        return FALSE;
    }

    *size -= KEYENCE_HEADER_SIZE;
    return TRUE;
}

static gboolean
read_offset_table(const guchar **p,
                  gsize *size,
                  KeyenceOffsetTable *offsettable,
                  GError **error)
{
    if (*size < KEYENCE_OFFSET_TABLE_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    offsettable->setting = gwy_get_guint32_le(p);
    offsettable->color_peak = gwy_get_guint32_le(p);
    offsettable->color_light = gwy_get_guint32_le(p);
    offsettable->light0 = gwy_get_guint32_le(p);
    offsettable->light1 = gwy_get_guint32_le(p);
    offsettable->light2 = gwy_get_guint32_le(p);
    offsettable->height0 = gwy_get_guint32_le(p);
    offsettable->height1 = gwy_get_guint32_le(p);
    offsettable->height2 = gwy_get_guint32_le(p);
    offsettable->color_peak_thumbnail = gwy_get_guint32_le(p);
    offsettable->color_thumbnail = gwy_get_guint32_le(p);
    offsettable->light_thumbnail = gwy_get_guint32_le(p);
    offsettable->height_thumbnail = gwy_get_guint32_le(p);
    offsettable->assemble = gwy_get_guint32_le(p);
    offsettable->line_measure = gwy_get_guint32_le(p);
    offsettable->line_thickness = gwy_get_guint32_le(p);
    offsettable->string_data = gwy_get_guint32_le(p);
    offsettable->reserved = gwy_get_guint32_le(p);

    *size -= KEYENCE_OFFSET_TABLE_SIZE;
    return TRUE;
}

static gboolean
read_meas_conds(const guchar **p,
                gsize *size,
                KeyenceMeasurementConditions *measconds,
                GError **error)
{
    guint i;

    if (*size < KEYENCE_MEASUREMENT_CONDITIONS_MIN_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    measconds->size = gwy_get_guint32_le(p);
    if (*size < measconds->size) {
        err_TRUNCATED(error);
        return FALSE;
    }
    if (measconds->size < KEYENCE_MEASUREMENT_CONDITIONS_MIN_SIZE) {
        err_INVALID(error, "MeasurementConditions::Size");
        return FALSE;
    }

    measconds->year = gwy_get_guint32_le(p);
    measconds->month = gwy_get_guint32_le(p);
    measconds->day = gwy_get_guint32_le(p);
    measconds->hour = gwy_get_guint32_le(p);
    measconds->minute = gwy_get_guint32_le(p);
    measconds->second = gwy_get_guint32_le(p);
    measconds->diff_utc_by_minutes = gwy_get_guint32_le(p);
    measconds->image_attributes = gwy_get_guint32_le(p);
    measconds->user_interface_mode = gwy_get_guint32_le(p);
    measconds->color_composite_mode = gwy_get_guint32_le(p);
    measconds->num_layer = gwy_get_guint32_le(p);
    measconds->run_mode = gwy_get_guint32_le(p);
    measconds->peak_mode = gwy_get_guint32_le(p);
    measconds->sharpening_level = gwy_get_guint32_le(p);
    measconds->speed = gwy_get_guint32_le(p);
    measconds->distance = gwy_get_guint32_le(p);
    measconds->pitch = gwy_get_guint32_le(p);
    measconds->optical_zoom = gwy_get_guint32_le(p);
    measconds->num_line = gwy_get_guint32_le(p);
    measconds->line0_pos = gwy_get_guint32_le(p);
    for (i = 0; i < G_N_ELEMENTS(measconds->reserved1); i++)
        measconds->reserved1[i] = gwy_get_guint32_le(p);
    measconds->lens_mag = gwy_get_guint32_le(p);
    measconds->pmt_gain_mode = gwy_get_guint32_le(p);
    measconds->pmt_gain = gwy_get_guint32_le(p);
    measconds->pmt_offset = gwy_get_guint32_le(p);
    measconds->nd_filter = gwy_get_guint32_le(p);
    measconds->reserved2 = gwy_get_guint32_le(p);
    measconds->persist_count = gwy_get_guint32_le(p);
    measconds->shutter_speed_mode = gwy_get_guint32_le(p);
    measconds->shutter_speed = gwy_get_guint32_le(p);
    measconds->white_balance_mode = gwy_get_guint32_le(p);
    measconds->white_balance_red = gwy_get_guint32_le(p);
    measconds->white_balance_blue = gwy_get_guint32_le(p);
    measconds->camera_gain = gwy_get_guint32_le(p);
    measconds->plane_compensation = gwy_get_guint32_le(p);
    measconds->xy_length_unit = gwy_get_guint32_le(p);
    measconds->z_length_unit = gwy_get_guint32_le(p);
    measconds->xy_decimal_place = gwy_get_guint32_le(p);
    measconds->z_decimal_place = gwy_get_guint32_le(p);
    measconds->x_length_per_pixel = gwy_get_guint32_le(p);
    measconds->y_length_per_pixel = gwy_get_guint32_le(p);
    measconds->z_length_per_digit = gwy_get_guint32_le(p);
    for (i = 0; i < G_N_ELEMENTS(measconds->reserved3); i++)
        measconds->reserved3[i] = gwy_get_guint32_le(p);
    measconds->light_filter_type = gwy_get_guint32_le(p);
    measconds->reserved4 = gwy_get_guint32_le(p);
    measconds->gamma_reverse = gwy_get_guint32_le(p);
    measconds->gamma = gwy_get_guint32_le(p);
    measconds->offset = gwy_get_guint32_le(p);
    measconds->ccd_bw_offset = gwy_get_guint32_le(p);
    measconds->numerical_aperture = gwy_get_guint32_le(p);
    measconds->head_type = gwy_get_guint32_le(p);
    measconds->pmt_gain2 = gwy_get_guint32_le(p);
    measconds->omit_color_image = gwy_get_guint32_le(p);
    measconds->lens_id = gwy_get_guint32_le(p);
    measconds->light_lut_mode = gwy_get_guint32_le(p);
    measconds->light_lut_in0 = gwy_get_guint32_le(p);
    measconds->light_lut_out0 = gwy_get_guint32_le(p);
    measconds->light_lut_in1 = gwy_get_guint32_le(p);
    measconds->light_lut_out1 = gwy_get_guint32_le(p);
    measconds->light_lut_in2 = gwy_get_guint32_le(p);
    measconds->light_lut_out2 = gwy_get_guint32_le(p);
    measconds->light_lut_in3 = gwy_get_guint32_le(p);
    measconds->light_lut_out3 = gwy_get_guint32_le(p);
    measconds->light_lut_in4 = gwy_get_guint32_le(p);
    measconds->light_lut_out4 = gwy_get_guint32_le(p);
    measconds->upper_position = gwy_get_guint32_le(p);
    measconds->lower_position = gwy_get_guint32_le(p);
    measconds->light_effective_bit_depth = gwy_get_guint32_le(p);
    measconds->height_effective_bit_depth = gwy_get_guint32_le(p);

    *size -= measconds->size;
    return TRUE;
}

static gboolean
read_assembly_info(KeyenceFile *kfile,
                   GError **error)
{
    const guchar *p = kfile->buffer;
    gsize size = kfile->size;
    guint off = kfile->offset_table.assemble;
    guint nfiles, i, j;

    if (!off)
        return TRUE;

    if (size <= KEYENCE_ASSEMBLY_HEADERS_SIZE
        || off > size - KEYENCE_ASSEMBLY_HEADERS_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += off;

    kfile->assembly_info.size = gwy_get_guint32_le(&p);
    kfile->assembly_info.file_type = gwy_get_guint16_le(&p);
    kfile->assembly_info.stage_type = gwy_get_guint16_le(&p);
    kfile->assembly_info.x_position = gwy_get_guint32_le(&p);
    kfile->assembly_info.y_position = gwy_get_guint32_le(&p);

    kfile->assembly_conds.auto_adjustment = *(p++);
    kfile->assembly_conds.source = *(p++);
    kfile->assembly_conds.thin_out = gwy_get_guint16_le(&p);
    kfile->assembly_conds.count_x = gwy_get_guint16_le(&p);
    kfile->assembly_conds.count_y = gwy_get_guint16_le(&p);

    nfiles = kfile->assembly_conds.count_x * kfile->assembly_conds.count_y;
    if ((size - KEYENCE_ASSEMBLY_HEADERS_SIZE - off)/nfiles
        < KEYENCE_ASSEMBLY_FILE_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    kfile->assembly_nfiles = nfiles;
    kfile->assembly_files = g_new(KeyenceAssemblyFile, nfiles);
    for (i = 0; i < nfiles; i++) {
        KeyenceAssemblyFile *kafile = kfile->assembly_files + i;

        for (j = 0; j < G_N_ELEMENTS(kafile->source_file); j++)
            kafile->source_file[j] = gwy_get_guint16_le(&p);
        kafile->pos_x = *(p++);
        kafile->pos_y = *(p++);
        kafile->datums_pos = *(p++);
        kafile->fix_distance = *(p++);
        kafile->distance_x = gwy_get_guint32_le(&p);
        kafile->distance_y = gwy_get_guint32_le(&p);
    }

    return TRUE;
}

static gboolean
read_data_image(KeyenceFile *kfile,
                KeyenceFalseColorImage *image,
                guint offset,
                GError **error)
{
    const guchar *p = kfile->buffer;
    gsize size = kfile->size;
    guint bps;

    if (!offset)
        return TRUE;

    if (size <= KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE
        || offset > size - KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += offset;
    image->width = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, image->width))
        return FALSE;
    image->height = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, image->height))
        return FALSE;

    image->bit_depth = gwy_get_guint32_le(&p);
    if (image->bit_depth != 8
        && image->bit_depth != 16
        && image->bit_depth != 32) {
        err_BPP(error, image->bit_depth);
        return FALSE;
    }
    bps = image->bit_depth/8;

    image->compression = gwy_get_guint32_le(&p);
    image->byte_size = gwy_get_guint32_le(&p);
    if (err_SIZE_MISMATCH(error,
                          image->width*image->height*bps,
                          image->byte_size,
                          TRUE))
        return FALSE;

    image->palette_range_min = gwy_get_guint32_le(&p);
    image->palette_range_max = gwy_get_guint32_le(&p);
    memcpy(image->palette, p, sizeof(image->palette));
    p += sizeof(image->palette);

    image->data = p;
    kfile->nimages++;

    return TRUE;
}

static gboolean
read_data_images(KeyenceFile *kfile,
                 GError **error)
{
    const KeyenceOffsetTable *offtable = &kfile->offset_table;

    return (read_data_image(kfile, &kfile->light0, offtable->light0, error)
            && read_data_image(kfile, &kfile->light1, offtable->light1, error)
            && read_data_image(kfile, &kfile->light2, offtable->light2, error)
            && read_data_image(kfile, &kfile->height0, offtable->height0, error)
            && read_data_image(kfile, &kfile->height1, offtable->height1, error)
            && read_data_image(kfile, &kfile->height2, offtable->height2, error));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */