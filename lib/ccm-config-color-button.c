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
#include "ccm-config-color-button.h"

enum
{
    PROP_0,
    PROP_KEY,
    PROP_KEY_ALPHA,
    PROP_PLUGIN,
    PROP_SCREEN
};

G_DEFINE_TYPE (CCMConfigColorButton, ccm_config_color_button,
               GTK_TYPE_COLOR_BUTTON);

struct _CCMConfigColorButtonPrivate
{
    CCMConfig *config;
    CCMConfig *config_alpha;
    gint screen;
    gchar *plugin;
    gchar *key;
    gchar *key_alpha;
};

#define CCM_CONFIG_COLOR_BUTTON_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_COLOR_BUTTON, CCMConfigColorButtonPrivate))

static void ccm_config_color_button_on_changed (CCMConfigColorButton * self,
                                                CCMConfig * config);
static void ccm_config_color_button_on_alpha_changed (CCMConfigColorButton *
                                                      self, CCMConfig * config);
static void ccm_config_color_button_color_set (GtkColorButton * button);
static void ccm_config_color_button_realize (GtkWidget * widget);

static void
ccm_config_color_button_init (CCMConfigColorButton * self)
{
    self->priv = CCM_CONFIG_COLOR_BUTTON_GET_PRIVATE (self);
    self->priv->config = NULL;
    self->priv->config_alpha = NULL;
    self->priv->key = NULL;
    self->priv->key_alpha = NULL;
    self->priv->plugin = NULL;
    self->priv->screen = -1;
}

static void
ccm_config_color_button_finalize (GObject * object)
{
    CCMConfigColorButton *self = CCM_CONFIG_COLOR_BUTTON (object);

    if (self->priv->config)
        g_object_unref (self->priv->config);
    if (self->priv->config_alpha)
        g_object_unref (self->priv->config_alpha);
    if (self->priv->key)
        g_free (self->priv->key);
    if (self->priv->key_alpha)
        g_free (self->priv->key_alpha);
    if (self->priv->plugin)
        g_free (self->priv->plugin);

    G_OBJECT_CLASS (ccm_config_color_button_parent_class)->finalize (object);
}

static void
ccm_config_color_button_set_property (GObject * object, guint prop_id,
                                      const GValue * value, GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CONFIG_COLOR_BUTTON (object));

    CCMConfigColorButton *self = CCM_CONFIG_COLOR_BUTTON (object);

    switch (prop_id)
    {
        case PROP_KEY:
            if (self->priv->key)
                g_free (self->priv->key);
            self->priv->key = g_value_dup_string (value);
            break;
        case PROP_KEY_ALPHA:
            if (self->priv->key_alpha)
                g_free (self->priv->key_alpha);
            self->priv->key_alpha = g_value_dup_string (value);
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
                                  G_CALLBACK
                                  (ccm_config_color_button_on_changed), self);
    }
    if (self->priv->key_alpha)
    {
        if (self->priv->config_alpha)
            g_object_unref (self->priv->config_alpha);
        self->priv->config_alpha =
            ccm_config_new (self->priv->screen, self->priv->plugin,
                            self->priv->key_alpha);
        g_signal_connect_swapped (self->priv->config_alpha, "changed",
                                  G_CALLBACK
                                  (ccm_config_color_button_on_alpha_changed),
                                  self);
    }
}

static void
ccm_config_color_button_get_property (GObject * object, guint prop_id,
                                      GValue * value, GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CONFIG_COLOR_BUTTON (object));

    CCMConfigColorButton *self = CCM_CONFIG_COLOR_BUTTON (object);

    switch (prop_id)
    {
        case PROP_KEY:
            g_value_set_string (value, self->priv->key);
            break;
        case PROP_KEY_ALPHA:
            g_value_set_string (value, self->priv->key_alpha);
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
ccm_config_color_button_class_init (CCMConfigColorButtonClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMConfigColorButtonPrivate));

    object_class->finalize = ccm_config_color_button_finalize;
    object_class->set_property = ccm_config_color_button_set_property;
    object_class->get_property = ccm_config_color_button_get_property;

    GTK_COLOR_BUTTON_CLASS (klass)->color_set =
        ccm_config_color_button_color_set;
    GTK_WIDGET_CLASS (klass)->realize = ccm_config_color_button_realize;

    g_object_class_install_property (object_class, PROP_KEY,
                                     g_param_spec_string ("key", "Config key",
                                                          "Config key name", "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_KEY_ALPHA,
                                     g_param_spec_string ("key_alpha",
                                                          "Config key alpha",
                                                          "Config key alpha name",
                                                          "",
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
ccm_config_color_button_on_changed (CCMConfigColorButton * self,
                                    CCMConfig * config)
{
    GdkColor *color = ccm_config_get_color (config, NULL);

    if (color)
    {
        gtk_color_button_set_color (GTK_COLOR_BUTTON (self), color);
        g_free (color);
    }
}

static void
ccm_config_color_button_on_alpha_changed (CCMConfigColorButton * self,
                                          CCMConfig * config)
{
    if (gtk_color_button_get_use_alpha (GTK_COLOR_BUTTON (self)))
    {
        gfloat alpha = ccm_config_get_float (config, NULL);
        gtk_color_button_set_alpha (GTK_COLOR_BUTTON (self),
                                    (int) ((gfloat) 65535 * alpha));
    }
}

static void
ccm_config_color_button_color_set (GtkColorButton * button)
{
    CCMConfigColorButton *self = CCM_CONFIG_COLOR_BUTTON (button);
    GdkColor color;

    if (GTK_COLOR_BUTTON_CLASS (ccm_config_color_button_parent_class)->
        color_set)
        GTK_COLOR_BUTTON_CLASS (ccm_config_color_button_parent_class)->
            color_set (button);

    if (self->priv->config)
    {
        gtk_color_button_get_color (button, &color);
        ccm_config_set_color (self->priv->config, &color, NULL);
    }
    if (self->priv->config_alpha && gtk_color_button_get_use_alpha (button))
    {
        gint alpha = gtk_color_button_get_alpha (button);

        ccm_config_set_float (self->priv->config_alpha,
                              (gfloat) alpha / 65535.f, NULL);
    }
}

static void
ccm_config_color_button_realize (GtkWidget * widget)
{
    CCMConfigColorButton *self = CCM_CONFIG_COLOR_BUTTON (widget);

    if (GTK_WIDGET_CLASS (ccm_config_color_button_parent_class)->realize)
        GTK_WIDGET_CLASS (ccm_config_color_button_parent_class)->
            realize (widget);

    if (self->priv->config)
        ccm_config_color_button_on_changed (self, self->priv->config);
    if (self->priv->config_alpha)
        ccm_config_color_button_on_alpha_changed (self,
                                                  self->priv->config_alpha);
}

GtkWidget *
ccm_config_color_button_new (gint screen, gchar * plugin, gchar * key)
{
    g_return_val_if_fail (key != NULL, NULL);

    CCMConfigColorButton *self = g_object_new (CCM_TYPE_CONFIG_COLOR_BUTTON,
                                               "screen", screen,
                                               "plugin", plugin,
                                               "key", key, NULL);

    return GTK_WIDGET (self);
}
