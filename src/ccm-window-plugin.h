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

#ifndef _CCM_WINDOW_PLUGIN_H_
#define _CCM_WINDOW_PLUGIN_H_

#include <glib-object.h>
#include <cairo.h>

#include "ccm-plugin.h"
#include "ccm.h"

G_BEGIN_DECLS

#define CCM_TYPE_WINDOW_PLUGIN                 (ccm_window_plugin_get_type ())
#define CCM_IS_WINDOW_PLUGIN(obj)          	   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_WINDOW_PLUGIN))
#define CCM_WINDOW_PLUGIN_GET_INTERFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CCM_TYPE_WINDOW_PLUGIN, CCMWindowPluginClass))

#define CCM_WINDOW_PLUGIN_PARENT(obj)	   ((CCMWindowPlugin*)ccm_plugin_get_parent((CCMPlugin*)obj))

typedef struct _CCMWindowPluginClass CCMWindowPluginClass;
typedef struct _CCMWindowPlugin CCMWindowPlugin;

struct _CCMWindowPluginClass
{
	GTypeInterface    base_iface;
	
	void 			  (*load_options)		(CCMWindowPlugin* self, 
											 CCMWindow* window);
	CCMRegion*		  (*query_geometry)		(CCMWindowPlugin* self, 
											 CCMWindow* window);
	gboolean (*paint) (CCMWindowPlugin* self, CCMWindow* window,
					   cairo_t* ctx, cairo_surface_t* surface);
	void 			  (*map)				(CCMWindowPlugin* self, 
											 CCMWindow* window);
	void 			  (*unmap)				(CCMWindowPlugin* self, 
											 CCMWindow* window);
	void 			  (*query_opacity)		(CCMWindowPlugin* self, 
											 CCMWindow* window);
	void 			  (*set_opaque)			(CCMWindowPlugin* self, 
											 CCMWindow* window);
};

GType ccm_window_plugin_get_type (void) G_GNUC_CONST;

void 		ccm_window_plugin_load_options	(CCMWindowPlugin* self, 
											 CCMWindow* window);
CCMRegion* 	ccm_window_plugin_query_geometry(CCMWindowPlugin* self,
											 CCMWindow* window);
gboolean 	ccm_window_plugin_paint 		(CCMWindowPlugin* self, 
											 CCMWindow* window,
								  			 cairo_t* ctx, 
											 cairo_surface_t* surface);
void 		ccm_window_plugin_map			(CCMWindowPlugin* self, 
											 CCMWindow* window);
void 		ccm_window_plugin_unmap			(CCMWindowPlugin* self, 
										     CCMWindow* window);
void 		ccm_window_plugin_query_opacity (CCMWindowPlugin* self, 
										     CCMWindow* window);
void		ccm_window_plugin_set_opaque	(CCMWindowPlugin* self, 
											 CCMWindow* window);

G_END_DECLS

#endif /* _CCM_WINDOW_PLUGIN_H_ */
