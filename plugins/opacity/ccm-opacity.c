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

#include "ccm-drawable.h"
#include "ccm-screen.h"
#include "ccm-opacity.h"
#include "ccm.h"

enum
{
	CCM_OPACITY_OPACITY,
	CCM_OPACITY_OPTION_N
};

static gchar* CCMOpacityOptions[CCM_OPACITY_OPTION_N] = {
	"opacity"
};

static void ccm_opacity_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMOpacity, ccm_opacity, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_opacity,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_opacity_iface_init))

struct _CCMOpacityPrivate
{	
	CCMConfig* options[CCM_OPACITY_OPTION_N];
};

#define CCM_OPACITY_GET_PRIVATE(o)  \
   ((CCMOpacityPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_OPACITY, CCMOpacityClass))

void ccm_opacity_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
CCMRegion* ccm_opacity_query_geometry(CCMWindowPlugin* plugin, 
									  CCMWindow* window);

static void
ccm_opacity_init (CCMOpacity *self)
{
	self->priv = CCM_OPACITY_GET_PRIVATE(self);
}

static void
ccm_opacity_finalize (GObject *object)
{
	G_OBJECT_CLASS (ccm_opacity_parent_class)->finalize (object);
}

static void
ccm_opacity_class_init (CCMOpacityClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMOpacityPrivate));
	
	object_class->finalize = ccm_opacity_finalize;
}

static void
ccm_opacity_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_opacity_load_options;
	iface->query_geometry 	= ccm_opacity_query_geometry;
	iface->paint 			= NULL;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
}

void
ccm_opacity_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_OPACITY_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "opacity", 
												  CCMOpacityOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

CCMRegion*
ccm_opacity_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	CCMWindowType type = ccm_window_get_hint_type(window);
	
	if (type == CCM_WINDOW_TYPE_MENU ||
		type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
		type == CCM_WINDOW_TYPE_POPUP_MENU)
	{
		gfloat opacity = 
				ccm_config_get_float(self->priv->options[CCM_OPACITY_OPACITY]);
		ccm_window_set_opacity(window, opacity);
	} 
	
	return ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin),
											window);
}
