/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-window-plugin.c
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
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

#include "ccm-debug.h"
#include "ccm-window-plugin.h"

static void
ccm_window_plugin_base_init (gpointer g_class)
{
    static gboolean initialized = FALSE;

    if (!initialized)
    {
        initialized = TRUE;
    }
}

GType
ccm_window_plugin_get_type (void)
{
    static GType ccm_window_plugin_type = 0;

    if (!ccm_window_plugin_type)
    {
        const GTypeInfo ccm_window_plugin_info = {
            sizeof (CCMWindowPluginClass),
            ccm_window_plugin_base_init,
            NULL
        };

        ccm_window_plugin_type =
            g_type_register_static (G_TYPE_INTERFACE, "CCMWindowPlugin",
                                    &ccm_window_plugin_info, 0);
    }

    return ccm_window_plugin_type;
}

G_GNUC_PURE CCMWindowPlugin *
_ccm_window_plugin_get_root (CCMWindowPlugin * self)
{
    g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), NULL);

    CCMWindowPlugin *plugin;

    for (plugin = self; !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin));

    return plugin;
}

void
ccm_window_plugin_load_options (CCMWindowPlugin * self, CCMWindow * window)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->load_options)
            break;
    }

    if (plugin_class->load_options)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->load_options))
            plugin_class->load_options (plugin, window);
    }
}

CCMRegion *
ccm_window_plugin_query_geometry (CCMWindowPlugin * self, CCMWindow * window)
{
    g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), NULL);
    g_return_val_if_fail (window != NULL, NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->query_geometry)
            break;
    }

    if (plugin_class->query_geometry)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->query_geometry))
            return plugin_class->query_geometry (plugin, window);
    }
    return NULL;
}

gboolean
ccm_window_plugin_paint (CCMWindowPlugin * self, CCMWindow * window,
                         cairo_t * ctx, cairo_surface_t * surface)
{
    g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), FALSE);
    g_return_val_if_fail (window != NULL, FALSE);
    g_return_val_if_fail (ctx != NULL, FALSE);
    g_return_val_if_fail (surface != NULL, FALSE);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->paint)
            break;
    }

    if (plugin_class->paint)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->paint))
            return plugin_class->paint (plugin, window, ctx, surface);
    }

    return FALSE;
}

void
ccm_window_plugin_map (CCMWindowPlugin * self, CCMWindow * window)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->map)
            break;
    }

    if (plugin_class->map)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin,  plugin_class->map))
            plugin_class->map (plugin, window);
    }
}

void
ccm_window_plugin_unmap (CCMWindowPlugin * self, CCMWindow * window)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->unmap)
            break;
    }

    if (plugin_class->unmap)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->unmap))
            plugin_class->unmap (plugin, window);
    }
}

void
ccm_window_plugin_query_opacity (CCMWindowPlugin * self, CCMWindow * window)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->query_opacity)
            break;
    }

    if (plugin_class->query_opacity)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->query_opacity))
            plugin_class->query_opacity (plugin, window);
    }
}

void
ccm_window_plugin_move (CCMWindowPlugin * self, CCMWindow * window, int x,
                        int y)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->move)
            break;
    }

    if (plugin_class->move)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->move))
            plugin_class->move (plugin, window, x, y);
    }
}

void
ccm_window_plugin_resize (CCMWindowPlugin * self, CCMWindow * window, int width,
                          int height)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->resize)
            break;
    }

    if (plugin_class->resize)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->resize))
            plugin_class->resize (plugin, window, width, height);
    }
}

void
ccm_window_plugin_set_opaque_region (CCMWindowPlugin * self, CCMWindow * window,
                                     const CCMRegion * area)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->set_opaque_region)
            break;
    }

    if (plugin_class->set_opaque_region)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->set_opaque_region))
            plugin_class->set_opaque_region (plugin, window, area);
    }
}

void
ccm_window_plugin_get_origin (CCMWindowPlugin * self, CCMWindow * window,
                              int *x, int *y)
{
    g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
    g_return_if_fail (window != NULL);
    g_return_if_fail (x != NULL && y != NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->get_origin)
            break;
    }

    if (plugin_class->get_origin)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->get_origin))
            plugin_class->get_origin (plugin, window, x, y);
    }
}

CCMPixmap *
ccm_window_plugin_get_pixmap (CCMWindowPlugin * self, CCMWindow * window)
{
    g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), NULL);
    g_return_val_if_fail (window != NULL, NULL);

    CCMWindowPlugin *plugin;
    CCMWindowPluginClass *plugin_class;

    for (plugin = self; plugin_class = CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin), !plugin_class->is_window;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (plugin_class->get_pixmap)
            break;
    }

    if (plugin_class->get_pixmap)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin, plugin_class->get_pixmap))
            return plugin_class->get_pixmap (plugin, window);
    }

    return NULL;
}
