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

#include <X11/Xatom.h>

#include "ccm-drawable.h"
#include "ccm-config.h"
#include "ccm-display.h"
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
	CCM_OPACITY_STEP,
	CCM_OPACITY_OPTION_N
};

static gchar* CCMOpacityOptions[CCM_OPACITY_OPTION_N] = {
	"opacity",
	"increase",
	"decrease",
	"step"
};

static void ccm_opacity_screen_iface_init  (CCMScreenPluginClass* iface);
static void ccm_opacity_window_iface_init  (CCMWindowPluginClass* iface);
static void ccm_opacity_on_property_changed(CCMOpacity* self, 
											CCMPropertyType changed,
											CCMWindow* window);

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
	gfloat step;
	
	
	CCMWindow*	window;
	gfloat opacity;
	
	CCMConfig* 	options[CCM_OPACITY_OPTION_N];
};

#define CCM_OPACITY_GET_PRIVATE(o)  \
   ((CCMOpacityPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_OPACITY, CCMOpacityClass))

static void
ccm_opacity_init (CCMOpacity *self)
{
	gint cpt;
	
	self->priv = CCM_OPACITY_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->increase = NULL;
	self->priv->decrease = NULL;
	self->priv->step = 0.1f;
	self->priv->window = NULL;
	self->priv->opacity = 1.0;
	for (cpt = 0; cpt < CCM_OPACITY_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_opacity_finalize (GObject *object)
{
	CCMOpacity* self = CCM_OPACITY(object);
	gint cpt;
	
	g_signal_handlers_disconnect_by_func(self->priv->window, 
										 ccm_opacity_on_property_changed, 
										 self);	
	
	for (cpt = 0; cpt < CCM_OPACITY_OPTION_N; cpt++)
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
	
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
ccm_opacity_change_opacity(CCMWindow* window, gfloat value)
{
	g_return_if_fail(window != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	guint32 opacity = value * 0xffffffff;
	
	if (value == 1.0f)
		XDeleteProperty (CCM_DISPLAY_XDISPLAY(display), 
						 CCM_WINDOW_XWINDOW(window), 
						 CCM_WINDOW_GET_CLASS(window)->opacity_atom);
	else
		XChangeProperty(CCM_DISPLAY_XDISPLAY(display), 
						CCM_WINDOW_XWINDOW(window), 
						CCM_WINDOW_GET_CLASS(window)->opacity_atom, 
						XA_CARDINAL, 32, PropModeReplace, 
						(unsigned char *) &opacity, 1L);
	ccm_display_sync(display);
}

static void
ccm_opacity_on_increase_key_press(CCMOpacity* self)
{
	CCMWindow* window = NULL;
	gint x, y;
	
	if (ccm_screen_query_pointer (self->priv->screen, &window, &x, &y) &&
		window)
	{
		if (ccm_window_is_viewable (window))
		{
			gfloat opacity = ccm_window_get_opacity (window);
				
			opacity += self->priv->step;
			if (opacity > 1) opacity = 1.0;
			ccm_opacity_change_opacity (window, opacity);
		}
	}
}

static void
ccm_opacity_on_decrease_key_press(CCMOpacity* self)
{
	CCMWindow* window = NULL;
	gint x, y;
	
	if (ccm_screen_query_pointer (self->priv->screen, &window, &x, &y) &&
		window)
	{
		if (ccm_window_is_viewable (window))
		{
			gfloat opacity = ccm_window_get_opacity (window);
				
			opacity -= self->priv->step;
			if (opacity < 0.1) opacity = 0.1;
			ccm_opacity_change_opacity (window, opacity);
		}
	}
}

static gboolean
ccm_opacity_get_opacity_step(CCMOpacity* self)
{
	gfloat real_step = 
		ccm_config_get_float (self->priv->options[CCM_OPACITY_STEP]);
	gfloat step;
	
	step = MAX(0.01f, real_step);
	step = MIN(0.5f, real_step);
	if (self->priv->step != step)
	{
		self->priv->step = step;
		if (real_step != step)
			ccm_config_set_float (self->priv->options[CCM_OPACITY_STEP], step);
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_opacity_get_opacity(CCMOpacity* self)
{
	gfloat real_opacity = 
		ccm_config_get_float (self->priv->options[CCM_OPACITY_OPACITY]);
	gfloat opacity;
	
	opacity = MAX(0.1f, real_opacity);
	opacity = MIN(1.0f, real_opacity);
	if (self->priv->opacity != opacity)
	{
		self->priv->opacity = opacity;
		if (opacity != real_opacity)
			ccm_config_set_float (self->priv->options[CCM_OPACITY_OPACITY], 
								  opacity);
		return TRUE;
	}
	
	return FALSE;
}

static void
ccm_opacity_get_increase_keybind(CCMOpacity* self)
{
	gchar* shortcut;

	if (self->priv->increase) g_object_unref(self->priv->increase);
	
	shortcut = 
		ccm_config_get_string(self->priv->options [CCM_OPACITY_INCREASE]);
	self->priv->increase = ccm_keybind_new(self->priv->screen, shortcut, TRUE);
	g_free(shortcut);
	g_signal_connect_swapped(self->priv->increase, "key_press", 
							 G_CALLBACK(ccm_opacity_on_increase_key_press), 
							 self);
}

static void
ccm_opacity_get_decrease_keybind(CCMOpacity* self)
{
	gchar* shortcut;

	if (self->priv->decrease) g_object_unref(self->priv->decrease);
	
	shortcut = 
		ccm_config_get_string(self->priv->options [CCM_OPACITY_DECREASE]);
	self->priv->decrease = ccm_keybind_new(self->priv->screen, shortcut, TRUE);
	g_free(shortcut);
	g_signal_connect_swapped(self->priv->decrease, "key_press", 
							 G_CALLBACK(ccm_opacity_on_decrease_key_press), 
							 self);
}

static void
ccm_opacity_on_property_changed(CCMOpacity* self, CCMPropertyType changed,
								CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	if (changed == CCM_PROPERTY_HINT_TYPE)
	{
		CCMWindowType type = ccm_window_get_hint_type(window);
		
		if (type == CCM_WINDOW_TYPE_MENU ||
			type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
			type == CCM_WINDOW_TYPE_POPUP_MENU)
		{
			ccm_opacity_change_opacity(window, self->priv->opacity);
		}
	}
}

static void
ccm_opacity_on_option_changed(CCMOpacity* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_OPACITY_INCREASE])
		ccm_opacity_get_increase_keybind (self);
	else if (config == self->priv->options[CCM_OPACITY_DECREASE])
		ccm_opacity_get_decrease_keybind (self);
	else if (config == self->priv->options[CCM_OPACITY_STEP])
		ccm_opacity_get_opacity_step (self);
	else if (config == self->priv->options[CCM_OPACITY_OPACITY] &&
			 ccm_opacity_get_opacity (self) && self->priv->window)
	{
		ccm_opacity_on_property_changed (self, CCM_PROPERTY_HINT_TYPE, 
										 self->priv->window);
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
}

static void
ccm_opacity_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_OPACITY_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "opacity", 
												  CCMOpacityOptions[cpt]);
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_opacity_on_option_changed), 
								 self);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
		
	self->priv->screen = screen;
	
	ccm_opacity_get_opacity_step(self);
	ccm_opacity_get_increase_keybind(self);
	ccm_opacity_get_decrease_keybind(self);
}

static void
ccm_opacity_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_OPACITY_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "opacity", 
												  CCMOpacityOptions[cpt]);
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_opacity_on_option_changed), 
								 self);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	
	ccm_opacity_get_opacity (self);
	
	g_signal_connect_swapped(window, "property-changed",
							 G_CALLBACK(ccm_opacity_on_property_changed), self);
	ccm_opacity_on_property_changed (self, CCM_PROPERTY_HINT_TYPE, window);
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
	iface->load_options 	 = ccm_opacity_window_load_options;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = NULL;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
}

