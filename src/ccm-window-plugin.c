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

#include "ccm-window-plugin.h"

static void
ccm_window_plugin_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;
	
	if (! initialized)
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
		const GTypeInfo ccm_window_plugin_info =
		{
			sizeof (CCMWindowPluginClass),
			ccm_window_plugin_base_init,
			NULL
		};

		ccm_window_plugin_type = 
			g_type_register_static (G_TYPE_INTERFACE, 
									"CCMWindowPlugin",
									&ccm_window_plugin_info, 0);
	}

  	return ccm_window_plugin_type;
}

void
ccm_window_plugin_load_options(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options)
  		CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options(plugin, window);
}

CCMRegion*
ccm_window_plugin_query_geometry (CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), NULL);
	g_return_val_if_fail (window != NULL, NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry)
  		return CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry (plugin, 
																	     window);
	else
		return NULL;
}

gboolean 
ccm_window_plugin_paint (CCMWindowPlugin* self, CCMWindow* window,
						 cairo_t* ctx, cairo_surface_t* surface)
{
	g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), FALSE);
	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (ctx != NULL, FALSE);
	g_return_val_if_fail (surface != NULL, FALSE);

  	CCMWindowPlugin* plugin;
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint)
  		return CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint (plugin, window, 
																ctx, surface);
	else
		return FALSE;
}

void
ccm_window_plugin_map(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map)
  		CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map(plugin, window);
}

void
ccm_window_plugin_unmap(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap)
  		CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap(plugin, window);
}

void
ccm_window_plugin_query_opacity(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity)
  		CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity(plugin, window);
}

void
ccm_window_plugin_set_opaque(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin) && 
		 !CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque; 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque)
  		CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque(plugin, window);
}
