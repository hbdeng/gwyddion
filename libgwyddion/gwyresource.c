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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyenum.h>
#include <libgwyddion/gwyinventory.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyresource.h>

#define MAGIC_HEADER "Gwyddion resource "

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

/* Data used in load and save functions.  Some fields are used only one of
 * them */
typedef struct {
    const gchar *path;
    GError *err;
} GwyResourceIOData;

static void         gwy_resource_finalize         (GObject *object);
static gboolean     gwy_resource_get_is_const     (gconstpointer item);
static const gchar* gwy_resource_get_item_name    (gpointer item);
static gboolean     gwy_resource_compare          (gconstpointer item1,
                                                   gconstpointer item2);
static void         gwy_resource_rename           (gpointer item,
                                                   const gchar *new_name);
static void         gwy_resource_rename           (gpointer item,
                                                   const gchar *new_name);
static void         gwy_resource_modified         (GwyResource *resource);
static gboolean     gwy_resource_save             (gpointer key,
                                                   gpointer item,
                                                   gpointer user_data);
static void         gwy_resource_class_load_dir   (const gchar *path,
                                                   GwyResourceClass *klass,
                                                   gboolean system);

static guint resource_signals[LAST_SIGNAL] = { 0 };

static const GwyInventoryItemType gwy_resource_item_type = {
    0,
    "data-changed",
    &gwy_resource_get_is_const,
    &gwy_resource_get_item_name,
    &gwy_resource_compare,
    &gwy_resource_rename,
    NULL,  /* needs particular class */
    NULL,  /* needs particular class */
    NULL,  /* needs particular class */
    NULL,  /* needs particular class */
};

G_DEFINE_ABSTRACT_TYPE(GwyResource, gwy_resource, G_TYPE_OBJECT)

static void
gwy_resource_class_init(GwyResourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_resource_finalize;

    klass->item_type = gwy_resource_item_type;
    klass->item_type.type = G_TYPE_FROM_CLASS(klass);
    klass->data_changed = gwy_resource_modified;

    /**
    * GwyResource::data-changed:
    * @gwyresource: The #GwyResource which received the signal.
    *
    * The ::data-changed signal is emitted when resource data changes.
    */
    resource_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyResourceClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_resource_init(G_GNUC_UNUSED GwyResource *resource)
{
}

static void
gwy_resource_finalize(GObject *object)
{
    GwyResource *resource = (GwyResource*)object;

    gwy_debug("%s", resource->name->str);
    if (resource->use_count)
        g_critical("Resource %p with nonzero use_count is finalized.", object);
    g_string_free(resource->name, TRUE);

    G_OBJECT_CLASS(gwy_resource_parent_class)->finalize(object);
}

static const gchar*
gwy_resource_get_item_name(gpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->name->str;
}

static gboolean
gwy_resource_get_is_const(gconstpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->is_const;
}

static gboolean
gwy_resource_compare(gconstpointer item1,
                     gconstpointer item2)
{
    GwyResource *resource1 = (GwyResource*)item1;
    GwyResource *resource2 = (GwyResource*)item2;

    return strcmp(resource1->name->str, resource2->name->str);
}

static void
gwy_resource_rename(gpointer item,
                    const gchar *new_name)
{
    GwyResource *resource = (GwyResource*)item;

    g_return_if_fail(!resource->is_const);
    g_string_assign(resource->name, new_name);
    resource->is_modified = TRUE;
}

/**
 * gwy_resource_get_name:
 * @resource: A resource.
 *
 * Returns resource name.
 *
 * Returns: Name of @resource.  The string is owned by @resource and must not
 *          be modfied or freed.
 **/
const gchar*
gwy_resource_get_name(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    return resource->name->str;
}

/**
 * gwy_resource_get_is_modifiable:
 * @resource: A resource.
 *
 * Returns whether a resource is modifiable.
 *
 * Returns: %TRUE if resource is modifiable, %FALSE if it's fixed (system)
 *          resource.
 **/
gboolean
gwy_resource_get_is_modifiable(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return !resource->is_const;
}

/**
 * gwy_resource_get_is_preferred:
 * @resource: A resource.
 *
 * Returns whether a resource is preferred.
 *
 * Returns: %TRUE if resource is preferred, %FALSE otherwise.
 **/
gboolean
gwy_resource_get_is_preferred(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->is_preferred;
}

/**
 * gwy_resource_set_is_preferred:
 * @resource: A resource.
 * @is_preferred: %TRUE to make @resource preferred, %FALSE to make it not
 *                preferred.
 *
 * Sets preferability of a resource.
 **/
void
gwy_resource_set_is_preferred(GwyResource *resource,
                              gboolean is_preferred)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    resource->is_preferred = !!is_preferred;
}

/**
 * gwy_resource_class_get_name:
 * @klass: Resource class.
 *
 * Gets the name of resource class.
 *
 * This is an simple identifier usable for example as directory name.
 *
 * Returns: Resource class name, as a constant string that must not be modified
 *          nor freed.
 **/
const gchar*
gwy_resource_class_get_name(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->name;
}

GwyInventory*
gwy_resource_class_get_inventory(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->inventory;
}

const GwyInventoryItemType*
gwy_resource_class_get_item_type(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return &klass->item_type;
}

/**
 * gwy_resource_use:
 * @resource: A resource.
 *
 * Starts using a resource.
 *
 * Call to this function is necessary to use a resource properly.
 * It makes the resource to create any auxiliary structures that consume
 * considerable amount of memory and perform other initialization to
 * ready-to-use form.
 *
 * When a resource is no longer used, it should be released with
 * gwy_resource_release().
 *
 * In addition, it calls g_object_ref() on the resource.
 *
 * Resources usually exist through almost whole program lifetime from
 * #GObject perspective, but from the viewpoint of use this method is the
 * constructor and gwy_resource_release() is the destructor.
 **/
void
gwy_resource_use(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    gwy_debug("%s %p<%s> %d",
              g_type_name(G_TYPE_FROM_INSTANCE(resource)),
              resource, resource->name->str, resource->use_count);

    g_object_ref(resource);
    if (!resource->use_count++) {
        void (*method)(GwyResource*);

        method = GWY_RESOURCE_GET_CLASS(resource)->use;
        if (method)
            method(resource);
    }
}

/**
 * gwy_resource_release:
 * @resource: A resource.
 *
 * Releases a resource.
 *
 * When the number of resource uses drops to zero, it frees all auxiliary data
 * and returns back to `latent' form.  In addition, it calls g_object_unref()
 * on it.  See gwy_resource_use() for more.
 **/
void
gwy_resource_release(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    gwy_debug("%s %p<%s> %d",
              g_type_name(G_TYPE_FROM_INSTANCE(resource)),
              resource, resource->name->str, resource->use_count);
    g_return_if_fail(resource->use_count);

    if (!--resource->use_count) {
        void (*method)(GwyResource*);

        method = GWY_RESOURCE_GET_CLASS(resource)->release;
        if (method)
            method(resource);
    }
    g_object_unref(resource);
}

/**
 * gwy_resource_is_used:
 * @resource: A resource.
 *
 * Tells whether a resource is currently in use.
 *
 * See gwy_resource_use() for details.
 *
 * Returns: %TRUE if resource is in use, %FALSE otherwise.
 **/
gboolean
gwy_resource_is_used(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return resource->use_count > 0;
}

/**
 * gwy_resource_dump:
 * @resource: A resource.
 *
 * Dumps a resource to a textual (human readable) form.
 *
 * Returns: Textual resource representation.
 **/
GString*
gwy_resource_dump(GwyResource *resource)
{
    void (*method)(GwyResource*, GString*);
    GString *str;

    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    method = GWY_RESOURCE_GET_CLASS(resource)->dump;
    g_return_val_if_fail(method, NULL);

    str = g_string_new(MAGIC_HEADER);
    g_string_append(str, g_type_name(G_TYPE_FROM_INSTANCE(resource)));
    g_string_append_c(str, '\n');
    method(resource, str);

    return str;
}

/**
 * gwy_resource_parse:
 * @text: Textual resource representation.
 * @expected_type: Resource object type.  If not 0, only resources of give type
 *                 are allowed.  Zero value means any #GwyResource is allowed.
 *
 * Reconstructs a resource from human readable form.
 *
 * Returns: Newly created resource (or %NULL).
 **/
GwyResource*
gwy_resource_parse(const gchar *text,
                   GType expected_type)
{
    GwyResourceClass *klass;
    GwyResource *resource;
    GType type;
    gchar *name;
    guint len;

    if (!g_str_has_prefix(text, MAGIC_HEADER)) {
        g_warning("Wrong resource magic header");
        return NULL;
    }

    text += sizeof(MAGIC_HEADER) - 1;
    len = strspn(text, G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS);
    name = g_strndup(text, len);
    text = strchr(text + len, '\n');
    if (!text) {
        g_warning("Truncated resource header");
        return NULL;
    }
    text++;
    type = g_type_from_name(name);
    if (!type
        || (expected_type && type != expected_type)
        || !g_type_is_a(type, GWY_TYPE_RESOURCE)
        || !G_TYPE_IS_INSTANTIATABLE(type)) {
        g_warning("Wrong resource type `%s'", name);
        g_free(name);
        return NULL;
    }
    klass = GWY_RESOURCE_CLASS(g_type_class_peek_static(type));
    g_return_val_if_fail(klass && klass->parse, NULL);

    resource = klass->parse(text);
    if (resource) {
        g_string_assign(resource->name, name);
        /* TODO: change once we have GUI for that */
        resource->is_preferred = TRUE;
    }
    g_free(name);

    return resource;
}

/**
 * gwy_resource_data_changed:
 * @resource: A resource.
 *
 * Emits signal "data-changed" on a resource.
 *
 * Mostly useful in resource implementation.
 **/
void
gwy_resource_data_changed(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_signal_emit(resource, resource_signals[DATA_CHANGED], 0);
}

static void
gwy_resource_modified(GwyResource *resource)
{
    resource->is_modified = TRUE;
}

gboolean
gwy_resource_class_save(GwyResourceClass *klass,
                        GError **err)
{
    gchar *path;
    GwyResourceIOData iodata;
    gboolean ok;

    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), FALSE);
    g_return_val_if_fail(klass->inventory, FALSE);

    path = g_build_filename(gwy_get_user_dir(), klass->name, NULL);
    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
        if (g_mkdir(path, 0700) != 0) {
            g_set_error(err,
                        G_FILE_ERROR,
                        g_file_error_from_errno(errno),
                        "Cannot create directory `%s': %s",
                        path, g_strerror(errno));
            g_free(path);
            return FALSE;
        }
    }

    iodata.path = path;
    iodata.err = NULL;
    ok = (gwy_inventory_find(klass->inventory, &gwy_resource_save,
                             &iodata) == NULL);
    g_free(path);
    if (!ok)
        g_propagate_error(err, iodata.err);

    return ok;
}

static gboolean
gwy_resource_save(G_GNUC_UNUSED gpointer key,
                  gpointer item,
                  gpointer user_data)
{
    GwyResource *resource = GWY_RESOURCE(item);
    GwyResourceIOData *iodata = (GwyResourceIOData*)user_data;
    GString *str;
    FILE *fh;
    gchar *filename;

    /* Only attempt to save modified user resourced */
    if (resource->is_const || !resource->is_modified)
        return FALSE;

    filename = g_build_filename(iodata->path, resource->name->str, NULL);
    fh = g_fopen(filename, "w");
    if (!fh) {
        g_set_error(&iodata->err,
                    G_FILE_ERROR,
                    g_file_error_from_errno(errno),
                    "Cannot save file `%s': %s",
                    filename, g_strerror(errno));
        g_free(filename);
        return TRUE;
    }
    g_free(filename);
    str = gwy_resource_dump(resource);
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    return FALSE;
}

void
gwy_resource_class_load(GwyResourceClass *klass)
{
    gchar *path;

    g_return_if_fail(GWY_IS_RESOURCE_CLASS(klass));
    g_return_if_fail(klass->inventory);

    gwy_inventory_forget_order(klass->inventory);

    path = g_build_filename(gwy_find_self_dir("data"), klass->name, NULL);
    gwy_resource_class_load_dir(path, klass, TRUE);
    g_free(path);

    path = g_build_filename(gwy_get_user_dir(), klass->name, NULL);
    gwy_resource_class_load_dir(path, klass, FALSE);
    g_free(path);

    gwy_inventory_restore_order(klass->inventory);
}

static void
gwy_resource_class_load_dir(const gchar *path,
                            GwyResourceClass *klass,
                            gboolean system)
{
    GDir *dir;
    GwyResource *resource;
    GError *err = NULL;
    const gchar *name;
    gchar *filename, *text;

    if (!(dir = g_dir_open(path, 0, NULL)))
        return;

    while ((name = g_dir_read_name(dir))) {
        if (name[0] == '.'
            || g_str_has_suffix(name, "~")
            || g_str_has_suffix(name, ".bak")
            || g_str_has_suffix(name, ".BAK"))
            continue;

        if (gwy_inventory_get_item(klass->inventory, name)) {
            g_warning("Ignoring duplicite %s `%s'", klass->name, name);
            continue;
        }
        /* FIXME */
        filename = g_build_filename(path, name, NULL);
        if (!g_file_get_contents(filename, &text, NULL, &err)) {
            g_warning("Cannot read `%s': %s", filename, err->message);
            g_clear_error(&err);
            g_free(filename);
            continue;
        }
        g_free(filename);

        resource = gwy_resource_parse(text, G_TYPE_FROM_CLASS(klass));
        if (resource) {
            resource->name = g_string_new(name);
            resource->is_const = system;
            resource->is_modified = FALSE;
            gwy_inventory_insert_item(klass->inventory, resource);
            g_object_unref(resource);
        }
        g_free(text);
    }

    g_dir_close(dir);
}

/************************** Documentation ****************************/

/**
 * GwyResource:
 *
 * The #GwyResource struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
