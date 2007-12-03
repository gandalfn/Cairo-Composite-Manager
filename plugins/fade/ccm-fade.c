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
#include <math.h>

#include "ccm-drawable.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-animation.h"
#include "ccm-fade.h"
#include "ccm.h"

enum
{
	CCM_FADE_NONE = 1 << 0,
	CCM_FADE_ON_MAP = 1 << 1,
	CCM_FADE_ON_UNMAP = 1 << 2,
	CCM_FADE_ON_CREATE = 1 << 3,
	CCM_FADE_ON_DESTROY = 1 << 4
};

enum
{
	CCM_FADE_DURATION,
	CCM_FADE_OPTION_N
};

static gchar* CCMFadeOptions[CCM_FADE_OPTION_N] = {
	"duration"
};

static void ccm_fade_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_fade_screen_iface_init(CCMScreenPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMFade, ccm_fade, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_fade,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_fade_window_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_fade,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_fade_screen_iface_init))

struct _CCMFadePrivate
{	
	CCMWindow* window;
	CCMScreenPlugin* screen;
	
	gint way;
	gfloat origin;
	gboolean is_blocked;
	
	CCMAnimation* animation;
	
	CCMConfig* options[CCM_FADE_OPTION_N];
};

#define CCM_FADE_GET_PRIVATE(o)  \
   ((CCMFadePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FADE, CCMFadeClass))

gboolean ccm_fade_animation(CCMAnimation* animation, gfloat elapsed, 
							CCMFade* self);
void ccm_fade_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_fade_map(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_fade_unmap(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_fade_query_opacity(CCMWindowPlugin* plugin, CCMWindow* window);

void ccm_fade_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen);
gboolean ccm_fade_add_window(CCMScreenPlugin* plugin, CCMScreen* screen, CCMWindow* window);
void ccm_fade_remove_window(CCMScreenPlugin* plugin, CCMScreen* screen, CCMWindow* window);

static void
ccm_fade_init (CCMFade *self)
{
	self->priv = CCM_FADE_GET_PRIVATE(self);
	self->priv->window = NULL;
	self->priv->way = CCM_FADE_NONE;
	self->priv->is_blocked = FALSE;
	self->priv->animation = 
		ccm_animation_new((CCMAnimationFunc)ccm_fade_animation, self);
}

static void
ccm_fade_finalize (GObject *object)
{
	CCMFade* self = CCM_FADE(object);
	
	if (self->priv->animation) g_object_unref (self->priv->animation);
	
	G_OBJECT_CLASS (ccm_fade_parent_class)->finalize (object);
}

static void
ccm_fade_class_init (CCMFadeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMFadePrivate));
	
	object_class->finalize = ccm_fade_finalize;
}

static void
ccm_fade_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_fade_window_load_options;
	iface->query_geometry 	= NULL;
	iface->paint 			= NULL;
	iface->map				= ccm_fade_map;
	iface->unmap			= ccm_fade_unmap;
	iface->query_opacity  	= ccm_fade_query_opacity;
}

static void
ccm_fade_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_fade_screen_load_options;
	iface->paint 			= NULL;
	iface->add_window 		= ccm_fade_add_window;
	iface->remove_window 	= ccm_fade_remove_window;
}

void
ccm_fade_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_FADE_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "fade", 
												  CCMFadeOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

void
ccm_fade_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMFade* self = CCM_FADE(plugin);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_FADE_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "fade", 
												  CCMFadeOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
}

gboolean
ccm_fade_finish(CCMFade* self)
{
	gboolean ret = FALSE;
	
	if (self->priv->way & CCM_FADE_ON_DESTROY)
	{
		CCMScreen* screen = 
			ccm_drawable_get_screen (CCM_DRAWABLE(self->priv->window));
		
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
		ccm_screen_plugin_remove_window (self->priv->screen, screen, self->priv->window);
		self->priv->way = CCM_FADE_NONE;
		return TRUE;
	}
	if (self->priv->way & CCM_FADE_ON_CREATE)
	{
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
	if (self->priv->way & CCM_FADE_ON_MAP)
	{
		ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(self), 
							   self->priv->window);
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
	if (self->priv->way & CCM_FADE_ON_UNMAP)
	{
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
		ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(self), 
								 self->priv->window);
		ret = TRUE;
	}
	self->priv->way = CCM_FADE_NONE;
	
	return TRUE;
}

static gfloat
interpolate (gfloat t, gfloat begin, gfloat end, gfloat power)
{
    return (begin + (end - begin) * pow (t, power));
}

gboolean
ccm_fade_animation(CCMAnimation* animation, gfloat elapsed, CCMFade* self)
{
	gboolean ret = FALSE;
	
	if (self->priv->way != CCM_FADE_NONE)
	{
		gfloat duration = ccm_config_get_float(self->priv->options[CCM_FADE_DURATION]);
		gfloat opacity;
		gfloat step = self->priv->origin * (elapsed / duration);
		
		opacity = self->priv->way & CCM_FADE_ON_MAP || 
				  self->priv->way & CCM_FADE_ON_CREATE ? 
					interpolate(step, 0.0, self->priv->origin, 1) : interpolate(step, self->priv->origin, 0.0, 1);
		
		if (((self->priv->way & CCM_FADE_ON_MAP || 
			  self->priv->way & CCM_FADE_ON_CREATE) && 
			 opacity > self->priv->origin) ||
			((self->priv->way & CCM_FADE_ON_UNMAP || 
			  self->priv->way & CCM_FADE_ON_DESTROY) && opacity < 0.0f))
		{
			if (ccm_fade_finish(self)) 
			{
				ccm_window_set_opacity (self->priv->window, self->priv->origin);
				return FALSE;
			}
			opacity = self->priv->origin;
			ret = FALSE;
		}
		else
			ret = TRUE;
		
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
		
		ccm_window_set_opacity (self->priv->window, opacity);
	}
	
	return ret;
}

void
ccm_fade_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	
	self->priv->window = window;
	if (!self->priv->is_blocked)
	{
		if (self->priv->way == CCM_FADE_NONE)
		{
			self->priv->way = CCM_FADE_ON_MAP;
			self->priv->origin = ccm_window_get_opacity (window);
			ccm_window_set_opacity (window, 0.0f);
			ccm_animation_start(self->priv->animation);
		}
		else
			self->priv->way |= CCM_FADE_ON_MAP;
	}
	else
	{
		//ccm_drawable_damage (CCM_DRAWABLE(window));
		ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	}
	self->priv->is_blocked = FALSE;
}

void
ccm_fade_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	
	self->priv->window = window;
	if (!self->priv->is_blocked)
	{
		if (self->priv->way == CCM_FADE_NONE)
		{
			self->priv->origin = ccm_window_get_opacity (window);
			if (self->priv->origin)
			{
				self->priv->way = CCM_FADE_ON_UNMAP;
				self->priv->origin = ccm_window_get_opacity (window);
				ccm_animation_start(self->priv->animation);
			}
		}
		else
			self->priv->way |= CCM_FADE_ON_UNMAP;
	}
	else
	{
		ccm_drawable_damage (CCM_DRAWABLE(window));
		ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	}
	self->priv->is_blocked = FALSE;
}

void
ccm_fade_query_opacity(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	
	if (self->priv->way != CCM_FADE_NONE)
	{
		ccm_animation_stop(self->priv->animation);
	
		ccm_fade_finish(self);
		self->priv->is_blocked = TRUE;
	}
		
	ccm_window_plugin_query_opacity (CCM_WINDOW_PLUGIN_PARENT(self), window);
}

gboolean
ccm_fade_add_window(CCMScreenPlugin* plugin, CCMScreen* screen, CCMWindow* window)
{
	g_return_val_if_fail(window != NULL, FALSE);
	
	CCMFade* self = CCM_FADE(_ccm_window_get_plugin (window));
	gboolean ret;
	
	self->priv->screen = plugin;
	self->priv->window = window;
	
	if (window)
	{
		if (self->priv->way & CCM_FADE_ON_UNMAP)
		{
			ccm_animation_stop(self->priv->animation);
			ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(self), window);
		}
		self->priv->way &= ~CCM_FADE_ON_UNMAP;
		if (self->priv->way & CCM_FADE_ON_DESTROY)
		{
			ccm_animation_stop(self->priv->animation);
			ccm_screen_plugin_remove_window (CCM_SCREEN_PLUGIN_PARENT(plugin), 
											 screen, window);
		}
		self->priv->way &= ~CCM_FADE_ON_DESTROY;
	}
	
	ret = ccm_screen_plugin_add_window (CCM_SCREEN_PLUGIN_PARENT(plugin),
										screen, window);
	
	if (ret && window && ccm_window_is_viewable (window))
	{
		if (self->priv->way == CCM_FADE_NONE)
		{
			self->priv->origin = ccm_window_get_opacity (window);
			if (self->priv->origin)
			{
				self->priv->way = CCM_FADE_ON_CREATE;
				ccm_window_set_opacity (window, 0.0f);
				ccm_animation_start(self->priv->animation);
				self->priv->is_blocked = FALSE;
			}
		}
		else
			self->priv->way |= CCM_FADE_ON_CREATE;
	}
	
	return ret;
}

void 
ccm_fade_remove_window(CCMScreenPlugin* plugin, CCMScreen* screen, CCMWindow* window)
{
	g_return_if_fail(window != NULL);
	
	CCMFade* self = CCM_FADE(_ccm_window_get_plugin (window));
	
	self->priv->screen = plugin;
	self->priv->window = window;
	
	if (window)
	{
		self->priv->is_blocked = FALSE;
		if (self->priv->way & CCM_FADE_ON_MAP)
		{
			ccm_animation_stop(self->priv->animation);
			ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(self), window);
		}
		self->priv->way &= ~CCM_FADE_ON_MAP;
		if (self->priv->way & CCM_FADE_ON_CREATE)
		{
			ccm_animation_stop(self->priv->animation);
			ccm_screen_plugin_add_window (CCM_SCREEN_PLUGIN_PARENT(plugin), 
										  screen, window);
		}
		self->priv->way &= ~CCM_FADE_ON_CREATE;
		
		if (ccm_window_is_viewable (window) && !self->priv->is_blocked)
		{
			if (self->priv->way == CCM_FADE_NONE)
			{
				self->priv->way = CCM_FADE_ON_DESTROY;
				self->priv->origin = ccm_window_get_opacity (window);
				ccm_animation_start(self->priv->animation);
			}
			else
				self->priv->way |= CCM_FADE_ON_DESTROY;
		}
		else
		{
			ccm_drawable_damage (CCM_DRAWABLE(window));
			ccm_screen_plugin_remove_window (CCM_SCREEN_PLUGIN_PARENT(plugin), 
											 screen, window);
		}
	}
}
