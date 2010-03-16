/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>
#include <glib.h>

#include "ccm-config-schema.h"

#define CCM_CONFIG_GCONF_PREFIX "/apps/cairo-compmgr"

#define CCM_CONFIG_SCHEMA_PATH				"cairo-compmgr/schemas"
#define CCM_CONFIG_SCHEMA_SCREEN_PATH	    "default"
#define CCM_CONFIG_SCHEMA_GENERAL_PATH	    "general"

#define CCM_CONFIG_SCHEMA_ENTRY_TYPE		"Type"
#define CCM_CONFIG_SCHEMA_ENTRY_LIST_TYPE   "ListType"
#define CCM_CONFIG_SCHEMA_ENTRY_DEFAULT		"Default"
#define CCM_CONFIG_SCHEMA_ENTRY_DESCRIPTION "Description"

const gchar *CCM_CONFIG_VALUE_TYPE_NAME[] = {
    "invalid",
    "bool",
    "int",
    "string",
    "float",
    "list"
};

#define CCM_CONFIG_SCHEMA_GET_PRIVATE(o)  \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_SCHEMA, CCMConfigSchemaPrivate))

G_DEFINE_TYPE (CCMConfigSchema, ccm_config_schema, G_TYPE_OBJECT);

struct _CCMConfigSchemaPrivate
{
    int screen;
    gchar *name;
    gchar *filename;
    GKeyFile *file;
};

static void
ccm_config_schema_init (CCMConfigSchema * self)
{
    self->priv = CCM_CONFIG_SCHEMA_GET_PRIVATE (self);
    self->priv->screen = 0;
    self->priv->name = NULL;
    self->priv->filename = NULL;
    self->priv->file = NULL;
}

static void
ccm_config_schema_finalize (GObject * object)
{
    CCMConfigSchema *self = CCM_CONFIG_SCHEMA (object);

    if (self->priv->file)
        g_key_file_free (self->priv->file);
    if (self->priv->name)
        g_free (self->priv->name);
    if (self->priv->filename)
        g_free (self->priv->filename);

    G_OBJECT_CLASS (ccm_config_schema_parent_class)->finalize (object);
}

static void
ccm_config_schema_class_init (CCMConfigSchemaClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMConfigSchemaPrivate));

    object_class->finalize = ccm_config_schema_finalize;
}

static gchar *
ccm_config_schema_get_path (gchar * path, int screen, gchar * name)
{
    g_return_val_if_fail (path != NULL, NULL);

    return g_strdup_printf ("%s/%s/ccm-%s.schema-key", path,
                            CCM_CONFIG_SCHEMA_PATH,
                            name ? name : screen < 0 ? "display" : "screen");
}

static gboolean
ccm_config_schema_get_filename (CCMConfigSchema * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    const gchar *const *paths = g_get_system_data_dirs ();
    gboolean found = FALSE;
    gint cpt;

    for (cpt = 0; paths[cpt] && !found; cpt++)
    {
        gchar *filename = ccm_config_schema_get_path ((gchar *) paths[cpt],
                                                      self->priv->screen,
                                                      self->priv->name);

        if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
        {
            self->priv->filename = filename;
            found = TRUE;
        }
        else
            g_free (filename);
    }

    return found;
}

CCMConfigSchema *
ccm_config_schema_new (int screen, gchar * name)
{
    CCMConfigSchema *self = g_object_new (CCM_TYPE_CONFIG_SCHEMA, NULL);
    GError *error = NULL;

    self->priv->screen = screen;
    self->priv->name = name ? g_strdup (name) : NULL;
    self->priv->file = g_key_file_new ();

    if (!ccm_config_schema_get_filename (self))
    {
        g_warning ("No schema file found");
        g_object_unref (self);
        return NULL;
    }

    if (!g_key_file_load_from_file
        (self->priv->file, self->priv->filename,
         G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    {
        g_warning ("Error on load schemas %s: %s", self->priv->filename,
                   error->message);
        g_error_free (error);
        g_object_unref (self);
        return NULL;
    }

    return self;
}

CCMConfigSchema *
ccm_config_schema_new_from_file (int screen, gchar * filename)
{
    g_return_val_if_fail (filename != NULL, NULL);

    CCMConfigSchema *self = g_object_new (CCM_TYPE_CONFIG_SCHEMA, NULL);
    GError *error = NULL;
    gchar *basename = g_path_get_basename (filename);
    gchar **split = g_strsplit (basename, ".", -1);

    self->priv->screen = screen;
    if (split && split[0])
    {
        if (!g_ascii_strncasecmp (split[0], "ccm-", strlen ("ccm-")))
            self->priv->name = g_strdup (split[0] + strlen ("ccm-"));
        else
            self->priv->name = g_strdup (split[0]);
        g_free (basename);
    }
    else
    {
        self->priv->name = basename;
    }
    if (split)
        g_strfreev (split);

    self->priv->filename = g_strdup (filename);

    self->priv->file = g_key_file_new ();

    if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        g_warning ("No schema file found");
        g_object_unref (self);
        return NULL;
    }

    if (!g_key_file_load_from_file
        (self->priv->file, filename,
         G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    {
        g_warning ("Error on load schemas %s: %s", filename, error->message);
        g_error_free (error);
        g_object_unref (self);
        return NULL;
    }

    return self;
}

CCMConfigValueType
ccm_config_schema_get_value_type (CCMConfigSchema * self, gchar * key)
{
    g_return_val_if_fail (self != NULL, CCM_CONFIG_VALUE_INVALID);
    g_return_val_if_fail (key != NULL, CCM_CONFIG_VALUE_INVALID);

    CCMConfigValueType ret = CCM_CONFIG_VALUE_INVALID;
    gchar *type = g_key_file_get_string (self->priv->file, key,
                                         CCM_CONFIG_SCHEMA_ENTRY_TYPE, NULL);

    if (!type)
        return ret;

    if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_STRING], type))
        ret = CCM_CONFIG_VALUE_STRING;
    else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_BOOLEAN], type))
        ret = CCM_CONFIG_VALUE_BOOLEAN;
    else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_INTEGER], type))
        ret = CCM_CONFIG_VALUE_INTEGER;
    else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_FLOAT], type))
        ret = CCM_CONFIG_VALUE_FLOAT;
    else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_LIST], type))
    {
        gchar *list_type = g_key_file_get_string (self->priv->file, key,
                                                  CCM_CONFIG_SCHEMA_ENTRY_LIST_TYPE,
                                                  NULL);

        if (!list_type)
        {
            g_free (type);
            return ret;
        }

        if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_STRING], list_type))
            ret = CCM_CONFIG_VALUE_LIST_STRING;
        else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_INTEGER],
                                      list_type))
            ret = CCM_CONFIG_VALUE_LIST_INTEGER;
        else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_BOOLEAN],
                                      list_type))
            ret = CCM_CONFIG_VALUE_LIST_BOOLEAN;
        else if (!g_ascii_strcasecmp (CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_FLOAT],
                                      list_type))
            ret = CCM_CONFIG_VALUE_LIST_FLOAT;

        g_free (list_type);
    }
    g_free (type);
    
    return ret;
}

gchar *
ccm_config_schema_get_description (CCMConfigSchema * self, gchar * locale,
                                   gchar * key)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (key != NULL, NULL);

    return g_key_file_get_locale_string (self->priv->file, key,
                                         CCM_CONFIG_SCHEMA_ENTRY_DESCRIPTION,
                                         locale, NULL);
}

gchar *
ccm_config_schema_get_default (CCMConfigSchema * self, gchar * key)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (key != NULL, NULL);

    return g_key_file_get_string (self->priv->file, key,
                                  CCM_CONFIG_SCHEMA_ENTRY_DEFAULT, NULL);
}

gboolean
ccm_config_schema_write_gconf (CCMConfigSchema * self, gchar * filename)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    GError *error = NULL;
    gboolean ret = FALSE;
    gchar *top =
        g_markup_printf_escaped ("<?xml version=\"1.0\"?>\n"
                                 "<gconfschemafile>\n" "  <schemalist>\n");
    gchar *bottom =
        g_markup_printf_escaped ("  </schemalist>\n" "</gconfschemafile>\n");
    gchar *body = NULL, *content = NULL;
    gchar **groups = g_key_file_get_groups (self->priv->file, NULL);
    gint cpt;

    for (cpt = 0; groups && groups[cpt]; cpt++)
    {
        gchar *key = NULL;
        gchar *applyto = NULL;
        gchar *owner = g_markup_printf_escaped ("      <owner>gnome</owner>\n");
        gchar *tmp = NULL;
        gchar *type = NULL;
        gchar *defaut = NULL;
        gchar *description = NULL;
        CCMConfigValueType t = ccm_config_schema_get_value_type (self,
                                                                 groups[cpt]);

        if (self->priv->screen < 0)
        {
            key =
                g_markup_printf_escaped ("    <schema>\n"
                                         "      <key>/schemas%s/%s/%s</key>\n",
                                         CCM_CONFIG_GCONF_PREFIX,
                                         CCM_CONFIG_SCHEMA_GENERAL_PATH,
                                         groups[cpt]);
            applyto =
                g_markup_printf_escaped ("      <applyto>%s/%s/%s</applyto>\n",
                                         CCM_CONFIG_GCONF_PREFIX,
                                         CCM_CONFIG_SCHEMA_GENERAL_PATH,
                                         groups[cpt]);
        }
        else
        {
            key =
                g_markup_printf_escaped ("    <schema>\n"
                                         "      <key>/schemas%s/%s/%s/%s</key>\n",
                                         CCM_CONFIG_GCONF_PREFIX,
                                         CCM_CONFIG_SCHEMA_SCREEN_PATH,
                                         self->priv->name, groups[cpt]);
            applyto =
                g_markup_printf_escaped
                ("      <applyto>%s/%s/%s/%s</applyto>\n",
                 CCM_CONFIG_GCONF_PREFIX, CCM_CONFIG_SCHEMA_SCREEN_PATH,
                 self->priv->name, groups[cpt]);
        }

        if (t > CCM_CONFIG_VALUE_INVALID && t < CCM_CONFIG_VALUE_LIST)
        {
            type =
                g_markup_printf_escaped ("      <type>%s</type>\n",
                                         CCM_CONFIG_VALUE_TYPE_NAME[t]);
        }
        else if (t > CCM_CONFIG_VALUE_LIST)
        {
            gchar *type_list = NULL;

            tmp =
                g_markup_printf_escaped ("      <type>%s</type>\n",
                                         CCM_CONFIG_VALUE_TYPE_NAME
                                         [CCM_CONFIG_VALUE_LIST]);
            if (t == CCM_CONFIG_VALUE_LIST_BOOLEAN)
            {
                type_list =
                    g_markup_printf_escaped
                    ("      <list_type>%s</list_type>\n",
                     CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_BOOLEAN]);
            }
            else if (t == CCM_CONFIG_VALUE_LIST_INTEGER)
            {
                type_list =
                    g_markup_printf_escaped
                    ("      <list_type>%s</list_type>\n",
                     CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_INTEGER]);
            }
            else if (t == CCM_CONFIG_VALUE_LIST_FLOAT)
            {
                type_list =
                    g_markup_printf_escaped
                    ("      <list_type>%s</list_type>\n",
                     CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_FLOAT]);
            }
            else
            {
                type_list =
                    g_markup_printf_escaped
                    ("      <list_type>%s</list_type>\n",
                     CCM_CONFIG_VALUE_TYPE_NAME[CCM_CONFIG_VALUE_STRING]);
            }

            type = g_strconcat (tmp, type_list, NULL);
            g_free (tmp);
            g_free (type_list);
        }
        if (!type)
        {
            g_free (top);
            g_free (bottom);
            if (body)
                g_free (body);
            g_free (key);
            g_free (applyto);
            g_free (owner);
            g_strfreev (groups);

            return FALSE;
        }

        tmp = ccm_config_schema_get_default (self, groups[cpt]);
        if (tmp)
        {
            if (t > CCM_CONFIG_VALUE_INVALID && t < CCM_CONFIG_VALUE_LIST)
                defaut =
                g_markup_printf_escaped ("      <default>%s</default>\n",
                                         tmp);
            else if (t > CCM_CONFIG_VALUE_LIST)
                defaut =
                g_markup_printf_escaped ("      <default>[%s]</default>\n",
                                         tmp);
            g_free (tmp);
        }

        tmp = ccm_config_schema_get_description (self, "C", groups[cpt]);
        if (tmp)
        {
            description =
                g_markup_printf_escaped ("      <locale name=\"C\">\n"
                                         "        <short>%s</short>\n"
                                         "        <long>%s</long>\n"
                                         "      </locale>\n", tmp, tmp);
            g_free (tmp);
        }

        if (!body)
        {
            body =
                g_strconcat (key, applyto, owner, type, defaut, description,
                             "    </schema>\n", NULL);
        }
        else
        {
            tmp = body;
            body =
                g_strconcat (tmp, key, applyto, owner, type, defaut,
                             description, "    </schema>\n", NULL);
            g_free (tmp);
        }
        g_free (key);
        g_free (applyto);
        g_free (owner);
        g_free (type);
        g_free (defaut);
        g_free (description);
    }
    content = g_strconcat (top, body, bottom, NULL);
    g_free (top);
    g_free (bottom);
    g_free (body);
    g_strfreev (groups);

    ret = g_file_set_contents (filename, content, -1, &error);
    if (!ret)
    {
        g_warning ("Error on write %s: %s", filename, error->message);
        g_error_free (error);
    }
    g_free (content);

    return ret;
}
