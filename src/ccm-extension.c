/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with cairo-compmgr.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include "ccm-extension.h"

#define PLUGIN_SECTION "CCMPlugin"

typedef G_MODULE_EXPORT gboolean (*CCMExtensionGetType) (GTypeModule *);

struct _CCMExtensionPrivate
{
    gchar *name;
    gchar *label;
    gchar *description;
    gchar *version;
    gchar *filename;
    gchar **backends;
    gchar **depends;
    GType type;
    GModule *module;
    CCMExtensionGetType get_type;
};

#define CCM_EXTENSION_GET_PRIVATE(o) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_EXTENSION, CCMExtensionPrivate))

G_DEFINE_TYPE (CCMExtension, ccm_extension, G_TYPE_TYPE_MODULE);

static gboolean ccm_extension_load (GTypeModule * module);
static void ccm_extension_unload (GTypeModule * module);

static void
ccm_extension_init (CCMExtension * self)
{
    self->priv = CCM_EXTENSION_GET_PRIVATE (self);
    self->priv->name = NULL;
    self->priv->label = NULL;
    self->priv->description = NULL;
    self->priv->version = NULL;
    self->priv->filename = NULL;
    self->priv->module = NULL;
    self->priv->backends = NULL;
    self->priv->depends = NULL;
    self->priv->type = 0;
    self->priv->get_type = NULL;
}

static void
ccm_extension_finalize (GObject * object)
{
    CCMExtension *self = CCM_EXTENSION (object);

    if (self->priv->name)
        g_free (self->priv->name);
    if (self->priv->label)
        g_free (self->priv->label);
    if (self->priv->description)
        g_free (self->priv->description);
    if (self->priv->version)
        g_free (self->priv->version);
    if (self->priv->filename)
        g_free (self->priv->filename);
    if (self->priv->module)
        g_module_close (self->priv->module);
    if (self->priv->backends)
        g_strfreev (self->priv->backends);
    if (self->priv->depends)
        g_strfreev (self->priv->depends);

    G_OBJECT_CLASS (ccm_extension_parent_class)->finalize (object);
}

static void
ccm_extension_class_init (CCMExtensionClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMExtensionPrivate));

    G_TYPE_MODULE_CLASS (klass)->load = ccm_extension_load;
    G_TYPE_MODULE_CLASS (klass)->unload = ccm_extension_unload;

    object_class->finalize = ccm_extension_finalize;
}

static gboolean
ccm_extension_load (GTypeModule * module)
{
    CCMExtension *self = CCM_EXTENSION (module);
    gchar *get_type_func_name;

    if ((self->priv->module = g_module_open (self->priv->filename, 0)) == NULL)
    {
        g_warning ("Error on load %s : %s", self->priv->filename,
                   g_module_error ());
        return FALSE;
    }

    get_type_func_name =
        g_strdup_printf ("%s_get_plugin_type", G_TYPE_MODULE (self)->name);
    if (!g_module_symbol
        (self->priv->module, get_type_func_name,
         (gpointer) & self->priv->get_type))
    {
        g_free (get_type_func_name);
        return FALSE;
    }
    g_free (get_type_func_name);

    return TRUE;
}

static void
ccm_extension_unload (GTypeModule * module)
{
    CCMExtension *self = CCM_EXTENSION (module);

    if (self->priv->module)
        g_module_close (self->priv->module);

    self->priv->get_type = NULL;
}

CCMExtension *
ccm_extension_new (gchar * filename)
{
    g_return_val_if_fail (filename != NULL, NULL);

    CCMExtension *self = g_object_new (CCM_TYPE_EXTENSION, NULL);
    GKeyFile *plugin_file = g_key_file_new ();
    gint cpt;
    gchar *dirname = NULL;

    /* Load plugin configuration file */
    if (!g_key_file_load_from_file
        (plugin_file, filename, G_KEY_FILE_NONE, NULL))
    {
        g_warning ("Error on load %s", filename);
        g_object_unref (self);
        return NULL;
    }

    /* Get plugin name */
    if ((self->priv->name =
         g_key_file_get_string (plugin_file, PLUGIN_SECTION, "Plugin",
                                NULL)) == NULL)
    {
        g_warning ("Error on get plugin name in %s", filename);
        g_object_unref (self);
        return NULL;
    }
    for (cpt = 0; self->priv->name[cpt]; ++cpt)
    {
        if (self->priv->name[cpt] == '-')
            self->priv->name[cpt] = '_';
    }
    g_type_module_set_name (G_TYPE_MODULE (self), self->priv->name);

    /* Get Label */
    self->priv->label =
        g_key_file_get_locale_string (plugin_file, PLUGIN_SECTION, "Name", NULL,
                                      NULL);

    /* Get description */
    self->priv->description =
        g_key_file_get_locale_string (plugin_file, PLUGIN_SECTION,
                                      "Description", NULL, NULL);

    /* Get version */
    self->priv->version =
        g_key_file_get_locale_string (plugin_file, PLUGIN_SECTION, "Version",
                                      NULL, NULL);

    /* Get backends */
    self->priv->backends =
        g_key_file_get_string_list (plugin_file, PLUGIN_SECTION, "Backends",
                                    NULL, NULL);

    /* Get plugin depends */
    self->priv->depends =
        g_key_file_get_string_list (plugin_file, PLUGIN_SECTION, "Depends",
                                    NULL, NULL);

    g_key_file_free (plugin_file);

    dirname = g_path_get_dirname (filename);
    self->priv->filename =
        g_module_build_path (dirname, G_TYPE_MODULE (self)->name);
    g_free (dirname);

    g_type_module_use (G_TYPE_MODULE (self));

    return self;
}

const gchar *
ccm_extension_get_label (CCMExtension * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return (const gchar *) self->priv->label;
}

const gchar *
ccm_extension_get_description (CCMExtension * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return (const gchar *) self->priv->description;
}

const gchar *
ccm_extension_get_version (CCMExtension * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return (const gchar *) self->priv->version;
}

const gchar **
ccm_extension_get_backends (CCMExtension * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return (const gchar **) self->priv->backends;
}

GType
ccm_extension_get_type_object (CCMExtension * self)
{
    g_return_val_if_fail (self != NULL, 0);

    if (!self->priv->type && self->priv->get_type)
        self->priv->type = self->priv->get_type (G_TYPE_MODULE (self));

    return self->priv->type;
}

gint
_ccm_extension_compare (CCMExtension * self, CCMExtension * other)
{
    int cpt;

    if (!self && !other)
        return 0;
    if (!self && other)
        return -1;
    if (self && !other)
        return 1;

    if (other->priv->depends)
    {
        for (cpt = 0; other->priv->depends[cpt]; ++cpt)
        {
            if (!g_ascii_strcasecmp
                (other->priv->depends[cpt], self->priv->label))
                return 0;
        }
    }

    if (self->priv->depends)
    {
        for (cpt = 0; self->priv->depends[cpt]; ++cpt)
        {
            if (!g_ascii_strcasecmp
                (self->priv->depends[cpt], other->priv->label))
                return 1;
        }
    }

    return 1;
}
