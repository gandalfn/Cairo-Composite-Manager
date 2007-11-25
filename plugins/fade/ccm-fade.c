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
#include "ccm-animation.h"
#include "ccm-fade.h"
#include "ccm.h"

enum
{
	CCM_FADE_DURATION,
	CCM_FADE_OPTION_N
};

static gchar* CCMFadeOptions[CCM_FADE_OPTION_N] = {
	"duration"
};

static void ccm_fade_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMFade, ccm_fade, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_fade,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_fade_iface_init))

struct _CCMFadePrivate
{	
	CCMWindow* window;
	
	gint way;
	gfloat origin;
	
	CCMAnimation* animation;
	
	CCMConfig* options[CCM_FADE_OPTION_N];
};

#define CCM_FADE_GET_PRIVATE(o)  \
   ((CCMFadePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FADE, CCMFadeClass))

gboolean ccm_fade_animation(CCMAnimation* animation, gfloat elapsed, 
							CCMFade* self);
void ccm_fade_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_fade_map(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_fade_unmap(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_fade_query_opacity(CCMWindowPlugin* plugin, CCMWindow* window);

static void
ccm_fade_init (CCMFade *self)
{
	self->priv = CCM_FADE_GET_PRIVATE(self);
	self->priv->window = NULL;
	self->priv->way = 0;
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
ccm_fade_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_fade_load_options;
	iface->query_geometry 	= NULL;
	iface->paint 			= NULL;
	iface->map				= ccm_fade_map;
	iface->unmap			= ccm_fade_unmap;
	iface->query_opacity  	= ccm_fade_query_opacity;
}

void
ccm_fade_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
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

gboolean
ccm_fade_animation(CCMAnimation* animation, gfloat elapsed, CCMFade* self)
{
	gboolean ret = FALSE;
	
	if (self->priv->way != 0)
	{
		gfloat duration = ccm_config_get_float(self->priv->options[CCM_FADE_DURATION]);
		gfloat opacity;
		gfloat step = self->priv->origin * (elapsed / duration);
		
		opacity = self->priv->way == 1 ? step : self->priv->origin - step;
		
		if ((self->priv->way == 1 && opacity > self->priv->origin) ||
			(self->priv->way == -1 && opacity < 0.0f))
		{
			opacity = self->priv->origin;
			if (self->priv->way == 1)
			{
				ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(self), 
									   self->priv->window);
				self->priv->way = 0;
				ret = FALSE;
			}
			else if (self->priv->way == -1)
			{
				ccm_window_set_opacity (self->priv->window, opacity);
				ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(self), 
									     self->priv->window);
				self->priv->way = 0;
				return FALSE;
			}
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
	if (self->priv->way == 0)
	{
		self->priv->way = 1;
		self->priv->origin = ccm_window_get_opacity (window);
		ccm_window_set_opacity (window, 0.0f);
	}
	else
		self->priv->way = 1;
	
	ccm_animation_start(self->priv->animation);
}

void
ccm_fade_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	
	self->priv->window = window;
	if (self->priv->way == 0)
	{
		self->priv->way = -1;
		self->priv->origin = ccm_window_get_opacity (window);
	}
	else
		self->priv->way = -1;
	
	ccm_animation_start(self->priv->animation);
}

void
ccm_fade_query_opacity(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	
	ccm_animation_stop(self->priv->animation);
	self->priv->way = 0;
	
	ccm_window_plugin_query_opacity (CCM_WINDOW_PLUGIN_PARENT(self), window);
}
