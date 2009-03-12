/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <string.h>

#include <gladeui/glade.h>
#include <gladeui/glade-editor-property.h>

#include "ccm-config.h"
#include "ccm-config-check-button.h"
#include "ccm-config-adjustment.h"
#include "ccm-config-color-button.h"

/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void
empty (GObject *container, GladeCreateReason reason)
{
}

void
glade_ccm_init (const gchar *name)
{
    ccm_config_set_backend("key");
}

void
glade_ccm_config_check_button_set_property (GladeWidgetAdaptor *adaptor,
                                            GObject            *object, 
                                            const gchar        *id,
                                            const GValue       *value)
{
    if (!strcmp (id, "key"))
        g_object_set(object, "key", g_value_get_string(value), NULL);
    else if (!strcmp (id, "plugin"))
        g_object_set(object, "plugin", g_value_get_string(value), NULL);
    else if (!strcmp (id, "screen"))
        g_object_set(object, "screen", g_value_get_int(value), NULL);
    else
        GWA_GET_CLASS (GTK_TYPE_CHECK_BUTTON)->set_property (adaptor, object, 
                                                             id, value);
}

void
glade_ccm_config_adjustment_set_property (GladeWidgetAdaptor *adaptor,
                                          GObject            *object, 
                                          const gchar        *id,
                                          const GValue       *value)
{
    if (!strcmp (id, "key"))
        g_object_set(object, "key", g_value_get_string(value), NULL);
    else if (!strcmp (id, "plugin"))
        g_object_set(object, "plugin", g_value_get_string(value), NULL);
    else if (!strcmp (id, "screen"))
        g_object_set(object, "screen", g_value_get_int(value), NULL);
    else
        GWA_GET_CLASS (GTK_TYPE_ADJUSTMENT)->set_property (adaptor, object,
                                                           id, value);
}

void
glade_ccm_config_color_button_set_property (GladeWidgetAdaptor *adaptor,
                                            GObject            *object, 
                                            const gchar        *id,
                                            const GValue       *value)
{
    if (!strcmp (id, "key"))
        g_object_set(object, "key", g_value_get_string(value), NULL);
    else if (!strcmp (id, "key-alpha"))
        g_object_set(object, "key_alpha", g_value_get_string(value), NULL);
    else if (!strcmp (id, "plugin"))
        g_object_set(object, "plugin", g_value_get_string(value), NULL);
    else if (!strcmp (id, "screen"))
        g_object_set(object, "screen", g_value_get_int(value), NULL);
    else 
    {
        GladeWidget *gwidget = glade_widget_get_from_gobject (object);
        if (!strcmp (id, "use-alpha"))
            glade_widget_property_set_sensitive (gwidget, "key-alpha",
                                                 g_value_get_boolean(value), 
                                                 NULL);
                
        GWA_GET_CLASS (GTK_TYPE_COLOR_BUTTON)->set_property (adaptor, object,
                                                             id, value);
    }
}
