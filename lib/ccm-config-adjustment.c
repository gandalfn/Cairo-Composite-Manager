/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <nicolas.bruguier@supersonicimagine.fr>
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

#include "ccm-config.h"
#include "ccm-config-adjustment.h"

enum
{
    PROP_0,
    PROP_KEY,
    PROP_PLUGIN,
    PROP_SCREEN
};

G_DEFINE_TYPE (CCMConfigAdjustment, ccm_config_adjustment, GTK_TYPE_ADJUSTMENT);

struct _CCMConfigAdjustmentPrivate
{
    CCMConfig *config;
    gchar *key;
    gchar *plugin;
    gint screen;
};

#define CCM_CONFIG_ADJUSTMENT_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_ADJUSTMENT, CCMConfigAdjustmentPrivate))

static void ccm_config_adjustment_on_changed (CCMConfigAdjustment * self,
                                              CCMConfig * config);
static void ccm_config_adjustment_value_changed (GtkAdjustment * adjustment);


static void
ccm_config_adjustment_init (CCMConfigAdjustment * self)
{
    self->priv = CCM_CONFIG_ADJUSTMENT_GET_PRIVATE (self);
    self->priv->config = NULL;
    self->priv->key = NULL;
    self->priv->plugin = NULL;
    self->priv->screen = -1;
}

static void
ccm_config_adjustment_finalize (GObject * object)
{
    CCMConfigAdjustment *self = CCM_CONFIG_ADJUSTMENT (object);

    if (self->priv->config)
        g_object_unref (self->priv->config);
    if (self->priv->key)
        g_free (self->priv->key);
    if (self->priv->plugin)
        g_free (self->priv->plugin);

    G_OBJECT_CLASS (ccm_config_adjustment_parent_class)->finalize (object);
}

static void
ccm_config_adjustment_set_property (GObject * object, guint prop_id,
                                    const GValue * value, GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CONFIG_ADJUSTMENT (object));

    CCMConfigAdjustment *self = CCM_CONFIG_ADJUSTMENT (object);

    switch (prop_id)
    {
        case PROP_KEY:
            if (self->priv->key)
                g_free (self->priv->key);
            self->priv->key = g_value_dup_string (value);
            break;
        case PROP_PLUGIN:
            if (self->priv->plugin)
                g_free (self->priv->plugin);
            self->priv->plugin = g_value_dup_string (value);
            break;
        case PROP_SCREEN:
            self->priv->screen = g_value_get_int (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }

    if (self->priv->key)
    {
        if (self->priv->config)
            g_object_unref (self->priv->config);
        self->priv->config =
            ccm_config_new (self->priv->screen, self->priv->plugin,
                            self->priv->key);
        g_signal_connect_swapped (self->priv->config, "changed",
                                  G_CALLBACK (ccm_config_adjustment_on_changed),
                                  self);
        ccm_config_adjustment_on_changed (self, self->priv->config);
    }
}

static void
ccm_config_adjustment_get_property (GObject * object, guint prop_id,
                                    GValue * value, GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CONFIG_ADJUSTMENT (object));

    CCMConfigAdjustment *self = CCM_CONFIG_ADJUSTMENT (object);

    switch (prop_id)
    {
        case PROP_KEY:
            g_value_set_string (value, self->priv->key);
            break;
        case PROP_PLUGIN:
            g_value_set_string (value, self->priv->plugin);
            break;
        case PROP_SCREEN:
            g_value_set_int (value, self->priv->screen);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
ccm_config_adjustment_class_init (CCMConfigAdjustmentClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMConfigAdjustmentPrivate));

    object_class->finalize = ccm_config_adjustment_finalize;
    object_class->set_property = ccm_config_adjustment_set_property;
    object_class->get_property = ccm_config_adjustment_get_property;

    GTK_ADJUSTMENT_CLASS (klass)->value_changed =
        ccm_config_adjustment_value_changed;

    g_object_class_install_property (object_class, PROP_KEY,
                                     g_param_spec_string ("key", "Config key",
                                                          "Config key name", "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_PLUGIN,
                                     g_param_spec_string ("plugin",
                                                          "Plugin name ",
                                                          "Plugin name (screen or ",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_SCREEN,
                                     g_param_spec_int ("screen",
                                                       "Screen number",
                                                       "Screen number", -1, 10,
                                                       -1, G_PARAM_READWRITE));
}

static void
ccm_config_adjustment_on_changed (CCMConfigAdjustment * self,
                                  CCMConfig * config)
{
    g_return_if_fail (CCM_IS_CONFIG_ADJUSTMENT (self));
    g_return_if_fail (CCM_IS_CONFIG (config));

    if (ccm_config_get_value_type (config, NULL) == CCM_CONFIG_VALUE_INTEGER)
    {
        gint value = ccm_config_get_integer (config, NULL);
        gtk_adjustment_set_value (GTK_ADJUSTMENT (self), (gdouble) value);
    }
    else if (ccm_config_get_value_type (config, NULL) == CCM_CONFIG_VALUE_FLOAT)
    {
        gfloat value = ccm_config_get_float (config, NULL);
        gtk_adjustment_set_value (GTK_ADJUSTMENT (self), (gdouble) value);
    }
}

static void
ccm_config_adjustment_value_changed (GtkAdjustment * adjustment)
{
    g_return_if_fail (CCM_IS_CONFIG_ADJUSTMENT (adjustment));

    CCMConfigAdjustment *self = CCM_CONFIG_ADJUSTMENT (adjustment);

    if (GTK_ADJUSTMENT_CLASS (ccm_config_adjustment_parent_class)->
        value_changed)
        GTK_ADJUSTMENT_CLASS (ccm_config_adjustment_parent_class)->
            value_changed (adjustment);

    if (self->priv->config)
    {
        if (ccm_config_get_value_type (self->priv->config, NULL) ==
            CCM_CONFIG_VALUE_INTEGER)
        {
            gdouble value = gtk_adjustment_get_value (adjustment);
            ccm_config_set_integer (self->priv->config, (gint) value, NULL);
        }
        else if (ccm_config_get_value_type (self->priv->config, NULL) ==
                 CCM_CONFIG_VALUE_FLOAT)
        {
            gdouble value = gtk_adjustment_get_value (adjustment);
            ccm_config_set_float (self->priv->config, (gfloat) value, NULL);
        }
    }
}

GtkAdjustment *
ccm_config_adjustment_new (int screen, gchar * plugin, gchar * key)
{
    g_return_val_if_fail (key != NULL, NULL);

    CCMConfigAdjustment *self = g_object_new (CCM_TYPE_CONFIG_ADJUSTMENT,
                                              "screen", screen,
                                              "plugin", plugin,
                                              "key", key, NULL);

    return GTK_ADJUSTMENT (self);
}
