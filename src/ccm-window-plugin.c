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

#include "ccm-debug.h"
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

CCMWindowPlugin*
_ccm_window_plugin_get_root(CCMWindowPlugin* self)
{
  	g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), NULL);
	
	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin));
	
	return plugin;
}

void
ccm_window_plugin_load_options(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
						CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options))
			CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->load_options(plugin, 
																   window);
	}
}

CCMRegion*
ccm_window_plugin_query_geometry (CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), NULL);
	g_return_val_if_fail (window != NULL, NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
					CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry))
			return CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_geometry (plugin, 
																		     window);
	}
	return NULL;
}

gboolean 
ccm_window_plugin_paint (CCMWindowPlugin* self, CCMWindow* window,
						 cairo_t* ctx, cairo_surface_t* surface,
						 gboolean y_invert)
{
	g_return_val_if_fail (CCM_IS_WINDOW_PLUGIN (self), FALSE);
	g_return_val_if_fail (window != NULL, FALSE);
	g_return_val_if_fail (ctx != NULL, FALSE);
	g_return_val_if_fail (surface != NULL, FALSE);

  	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
							 CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint))
			return CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->paint (plugin, 
																	window, 
																	ctx, 
																	surface,
																	y_invert);
	}
	
	return FALSE;
}

void
ccm_window_plugin_map(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
							 CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map))
			CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->map(plugin, window);
		else
			ccm_debug("LOCKED");
	}
}

void
ccm_window_plugin_unmap(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap)
			break;
		else
			ccm_debug("PLUGIN NEXT");
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
							 CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap))
			CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->unmap(plugin, window);
		else
			ccm_debug("LOCKED");
	}
}

void
ccm_window_plugin_query_opacity(CCMWindowPlugin* self, CCMWindow* window)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
					CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity))
			CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->query_opacity(plugin, window);
	}
}

void
ccm_window_plugin_move(CCMWindowPlugin* self, CCMWindow* window,
					   int x, int y)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->move)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->move)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
							 CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->move))
		  CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->move(plugin, window, x, y);
	}
}

void
ccm_window_plugin_resize(CCMWindowPlugin* self, CCMWindow* window,
						 int width, int height)
{
  	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
	
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->resize)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->resize)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
							 CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->resize))
			CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->resize(plugin, window,
															 width, height);
	}
}

void
ccm_window_plugin_set_opaque_region(CCMWindowPlugin* self, CCMWindow* window,
									CCMRegion* area)
{
	g_return_if_fail (CCM_IS_WINDOW_PLUGIN (self));
	g_return_if_fail (window != NULL);

	CCMWindowPlugin* plugin;
       
	for (plugin = self; 
		 CCM_IS_PLUGIN(plugin); 
		 plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque_region)
			break;
	}
    
	if (CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque_region)
	{
		if (!_ccm_plugin_method_locked((GObject*)plugin, 
				CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque_region))
			CCM_WINDOW_PLUGIN_GET_INTERFACE (plugin)->set_opaque_region(plugin, 
																		window, 
																		area);
	}
}

