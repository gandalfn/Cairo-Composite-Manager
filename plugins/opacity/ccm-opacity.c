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
#include "ccm-window.h"
#include "ccm-opacity.h"
#include "ccm-keybind.h"
#include "ccm.h"

enum
{
	CCM_OPACITY_OPACITY,
	CCM_OPACITY_INCREASE,
	CCM_OPACITY_DECREASE,
	CCM_OPACITY_OPTION_N
};

static gchar* CCMOpacityOptions[CCM_OPACITY_OPTION_N] = {
	"opacity",
	"increase",
	"decrease"
};

static void ccm_opacity_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_opacity_window_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMOpacity, ccm_opacity, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_opacity,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_opacity_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_opacity,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_opacity_window_iface_init))

struct _CCMOpacityPrivate
{	
	CCMScreen*	screen;
	CCMKeybind*	increase;
	CCMKeybind*	decrease;
	
	CCMConfig* 	options[CCM_OPACITY_OPTION_N];
};

#define CCM_OPACITY_GET_PRIVATE(o)  \
   ((CCMOpacityPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_OPACITY, CCMOpacityClass))

static void
ccm_opacity_init (CCMOpacity *self)
{
	self->priv = CCM_OPACITY_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->increase = NULL;
	self->priv->decrease = NULL;
}

static void
ccm_opacity_finalize (GObject *object)
{
	CCMOpacity* self = CCM_OPACITY(object);
	
	if (self->priv->increase) g_object_unref(self->priv->increase);
	if (self->priv->decrease) g_object_unref(self->priv->decrease);
	
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
ccm_opacity_on_increase_key_press(CCMOpacity* self)
{
	GList* windows = ccm_screen_get_windows(self->priv->screen);
	if (windows)
	{
		GList *item;
		
		for (item = g_list_last(windows); item; item = item->prev)
		{
			CCMWindow* toplevel = item->data;
			CCMWindowType type = ccm_window_get_hint_type (toplevel);
			
			if (toplevel->is_viewable && 
				(type == CCM_WINDOW_TYPE_NORMAL || type == CCM_WINDOW_TYPE_UNKNOWN))
			{
				gfloat opacity = ccm_window_get_opacity (toplevel);
			
				opacity += 0.05;
				if (opacity > 1) opacity = 1.0;
				ccm_window_set_opacity (toplevel, opacity);
				ccm_drawable_damage (CCM_DRAWABLE(toplevel));
				break;
			}
		}
	}
}

static void
ccm_opacity_on_decrease_key_press(CCMOpacity* self)
{
	GList* windows = ccm_screen_get_windows(self->priv->screen);
	if (windows && g_list_last(windows))
	{
		GList *item;
		
		for (item = g_list_last(windows); item; item = item->prev)
		{
			CCMWindow* toplevel = item->data;
			CCMWindowType type = ccm_window_get_hint_type (toplevel);
			
			if (toplevel->is_viewable && 
				(type == CCM_WINDOW_TYPE_NORMAL || type == CCM_WINDOW_TYPE_UNKNOWN))
			{
				gfloat opacity = ccm_window_get_opacity (toplevel);
				
				opacity -= 0.05;
				if (opacity < 0.1) opacity = 0.1;
				ccm_window_set_opacity (toplevel, opacity);
				ccm_drawable_damage (CCM_DRAWABLE(toplevel));
				break;
			}
		}
	}
}

static void
ccm_opacity_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	CCMDisplay* display = ccm_screen_get_display (screen);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_OPACITY_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "opacity", 
												  CCMOpacityOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	self->priv->increase = ccm_keybind_new(self->priv->screen, 
		ccm_config_get_string(self->priv->options [CCM_OPACITY_INCREASE]));
	g_signal_connect_swapped(self->priv->increase, "key_press", 
							 G_CALLBACK(ccm_opacity_on_increase_key_press), self);
	self->priv->decrease = ccm_keybind_new(self->priv->screen, 
		ccm_config_get_string(self->priv->options [CCM_OPACITY_DECREASE]));
	g_signal_connect_swapped(self->priv->decrease, "key_press", 
							 G_CALLBACK(ccm_opacity_on_decrease_key_press), self);
}

static void
ccm_opacity_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
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

static CCMRegion*
ccm_opacity_window_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
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

static void
ccm_opacity_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_opacity_screen_load_options;
	iface->paint 			= NULL;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
}

static void
ccm_opacity_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_opacity_window_load_options;
	iface->query_geometry 	= ccm_opacity_window_query_geometry;
	iface->paint 			= NULL;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
	iface->move				= NULL;
	iface->resize			= NULL;
}

