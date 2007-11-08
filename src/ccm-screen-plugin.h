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

#ifndef _CCM_SCREEN_PLUGIN_H_
#define _CCM_SCREEN_PLUGIN_H_

#include <glib-object.h>

#include "ccm-plugin.h"
#include "ccm.h"

G_BEGIN_DECLS

#define CCM_TYPE_SCREEN_PLUGIN             		(ccm_screen_plugin_get_type ())
#define CCM_IS_SCREEN_PLUGIN(obj)          		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_SCREEN_PLUGIN))
#define CCM_SCREEN_PLUGIN_GET_INTERFACE(obj)   	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), CCM_TYPE_SCREEN_PLUGIN, CCMScreenPluginClass))

#define CCM_SCREEN_PLUGIN_PARENT(obj)	   		((CCMScreenPlugin*)ccm_plugin_get_parent((CCMPlugin*)obj))

typedef struct _CCMScreenPluginClass CCMScreenPluginClass;
typedef struct _CCMScreenPlugin CCMScreenPlugin;

struct _CCMScreenPluginClass
{
	GTypeInterface    base_iface;
	
	void 	 (*load_options)	(CCMScreenPlugin* self, CCMScreen* screen);
	void 	 (*paint) 			(CCMScreenPlugin* self, CCMScreen* screen);
	gboolean (*add_window) 		(CCMScreenPlugin* self, CCMScreen* screen,
								 CCMWindow* window);
	void	 (*remove_window) 	(CCMScreenPlugin* self, CCMScreen* screen,
								 CCMWindow* window);
};

GType 		ccm_screen_plugin_get_type 		(void) G_GNUC_CONST;

void  		ccm_screen_plugin_load_options	(CCMScreenPlugin* self, 
										   	 CCMScreen* screen);
void  		ccm_screen_plugin_paint	 	  	(CCMScreenPlugin* self, 
										     CCMScreen* screen);
gboolean 	ccm_screen_plugin_add_window  	(CCMScreenPlugin* self, 
											 CCMScreen* screen,
										     CCMWindow* window);
void	 	ccm_screen_plugin_remove_window (CCMScreenPlugin* self, 
											 CCMScreen* screen,
											 CCMWindow* window);
G_END_DECLS

#endif /* _CCM_SCREEN_PLUGIN_H_ */
