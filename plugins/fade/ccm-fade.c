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

#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-timeline.h"
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

static void ccm_fade_window_iface_init(CCMWindowPluginClass* iface);
static void	ccm_fade_on_error(CCMFade* self, CCMWindow* window);
static void ccm_fade_on_property_changed(CCMFade* self, 
										 CCMPropertyType changed,
										 CCMWindow* window);

CCM_DEFINE_PLUGIN (CCMFade, ccm_fade, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_fade,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_fade_window_iface_init))

struct _CCMFadePrivate
{	
	CCMWindow* window;
	
	gfloat origin;
	
	gfloat duration;
	CCMTimeline* timeline;
	
	CCMConfig* options[CCM_FADE_OPTION_N];
};

#define CCM_FADE_GET_PRIVATE(o)  \
   ((CCMFadePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FADE, CCMFadeClass))

static void
ccm_fade_init (CCMFade *self)
{
	gint cpt;
	
	self->priv = CCM_FADE_GET_PRIVATE(self);
	self->priv->origin = 1.0f;
	self->priv->duration = 1.0f;
	self->priv->window = NULL;
	self->priv->timeline = NULL;
	for (cpt = 0; cpt < CCM_FADE_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_fade_finalize (GObject *object)
{
	CCMFade* self = CCM_FADE(object);
	gint cpt;
	
	g_signal_handlers_disconnect_by_func(self->priv->window, 
										 ccm_fade_on_property_changed, 
										 self);	
	
	g_signal_handlers_disconnect_by_func(self->priv->window, 
										 ccm_fade_on_error, 
										 self);	
	
	for (cpt = 0; cpt < CCM_FADE_OPTION_N; cpt++)
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
	
	if (self->priv->timeline) 
		g_object_unref (self->priv->timeline);
		
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
ccm_fade_on_new_frame(CCMFade* self, gint num_frame, CCMTimeline* timeline)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(timeline != NULL);
	
	gdouble progress = ccm_timeline_get_progress (timeline);
	gfloat opacity = self->priv->origin * progress * progress;
	
	ccm_debug_window(self->priv->window, "FADE %i %f %f", num_frame, progress, opacity);
	
	ccm_window_set_opacity (self->priv->window, opacity);
	ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
}

static void
ccm_fade_on_map_unmap_unlocked(CCMFade* self)
{
	ccm_debug_window(self->priv->window, "FADE UNLOCKED %f", self->priv->origin);
	ccm_window_set_opacity (self->priv->window, self->priv->origin);
}

static void
ccm_fade_finish(CCMFade* self)
{
	g_return_if_fail(self != NULL);
	
	ccm_debug_window(self->priv->window, "FADE FINISH");
	if (ccm_timeline_get_direction (self->priv->timeline) == CCM_TIMELINE_FORWARD)
	{
		CCM_WINDOW_PLUGIN_UNLOCK_ROOT_METHOD(self, map);
		ccm_window_plugin_map((CCMWindowPlugin*)self->priv->window,
						  self->priv->window);
	}
	else
	{
		CCM_WINDOW_PLUGIN_UNLOCK_ROOT_METHOD(self, unmap);
		ccm_window_plugin_unmap((CCMWindowPlugin*)self->priv->window,
								self->priv->window);
	}
}

static void
ccm_fade_on_completed(CCMFade* self, CCMTimeline* timeline)
{
	g_return_if_fail(self != NULL);

	ccm_fade_finish(self);
}

static void
ccm_fade_on_error(CCMFade* self, CCMWindow* window)
{
	if (ccm_timeline_is_playing(self->priv->timeline))
	{
		ccm_debug_window(window, "FADE ON ERROR");
		
		ccm_timeline_stop(self->priv->timeline);
		
		ccm_fade_finish(self);
	}
}

static void
ccm_fade_on_property_changed(CCMFade* self, CCMPropertyType changed,
							 CCMWindow* window)
{
	if (changed == CCM_PROPERTY_OPACITY)
	{
		self->priv->origin = ccm_window_get_opacity (window);
		ccm_debug_window(window, "FADE OPACITY %f", self->priv->origin);
		if (ccm_timeline_is_playing (self->priv->timeline))
		{
			gdouble progress = ccm_timeline_get_progress (self->priv->timeline);
			gfloat opacity = self->priv->origin * progress;
	
			ccm_window_set_opacity (self->priv->window, opacity);
			ccm_debug_window(window, "FADE OPACITY PROGRESS %f %f", progress, opacity);
		}
	}
}

static gboolean
ccm_fade_get_duration(CCMFade* self)
{
	gfloat duration = 
		ccm_config_get_float(self->priv->options[CCM_FADE_DURATION]);
	
	duration = MAX(0.1f, duration);
	duration = MIN(2.0f, duration);
	if (duration != self->priv->duration)
	{
		CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self->priv->window));
		guint refresh_rate;
		g_object_get (G_OBJECT(screen), "refresh_rate", &refresh_rate, NULL);
		
		if (self->priv->timeline) g_object_unref (self->priv->timeline);

		self->priv->duration = duration;
		ccm_config_set_float(self->priv->options[CCM_FADE_DURATION],
							 self->priv->duration);
		
		self->priv->timeline = ccm_timeline_new((int)(refresh_rate * duration), refresh_rate);
	
		g_signal_connect_swapped(self->priv->timeline, "new-frame", 
								 G_CALLBACK(ccm_fade_on_new_frame), self);
		g_signal_connect_swapped(self->priv->timeline, "completed", 
								 G_CALLBACK(ccm_fade_on_completed), self);
		
		return TRUE;
	}
	
	return FALSE;
}

static void
ccm_fade_on_option_changed(CCMFade* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_FADE_DURATION])
	{
		ccm_fade_get_duration (self);
	}
}

static void
ccm_fade_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_FADE_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "fade", 
												  CCMFadeOptions[cpt]);
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_fade_on_option_changed), 
								 self);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	self->priv->origin = ccm_window_get_opacity (window);
	
	g_signal_connect_swapped(window, "property-changed",
							 G_CALLBACK(ccm_fade_on_property_changed), 
							 self);
	g_signal_connect_swapped(window, "error",
							 G_CALLBACK(ccm_fade_on_error), 
							 self);

	ccm_fade_get_duration(self);
}

static void
ccm_fade_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	guint current_frame = 0;
	
	if (ccm_timeline_is_playing (self->priv->timeline))
	{
		current_frame = ccm_timeline_get_current_frame (self->priv->timeline);
		ccm_timeline_stop(self->priv->timeline);
		ccm_fade_finish (self);
	}
	else
	{
		self->priv->origin = ccm_window_get_opacity (window);
		ccm_window_set_opacity (window, 0.0);
	}
	
	CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD(plugin, map, 
									   (CCMPluginUnlockFunc)ccm_fade_on_map_unmap_unlocked, 
									   self);
	
	ccm_debug_window(window, "FADE MAP %i", current_frame);
	ccm_timeline_set_direction (self->priv->timeline, 
								CCM_TIMELINE_FORWARD);
	ccm_timeline_rewind(self->priv->timeline);
	ccm_timeline_start (self->priv->timeline);
	
	if (current_frame > 0)
		ccm_timeline_advance (self->priv->timeline, current_frame);
	
	ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_fade_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFade* self = CCM_FADE(plugin);
	guint current_frame = 0;
		
	if (ccm_timeline_is_playing (self->priv->timeline))
	{
		current_frame = ccm_timeline_get_current_frame (self->priv->timeline);
		ccm_timeline_stop(self->priv->timeline);
		ccm_fade_finish (self);
	}
	else
		ccm_window_set_opacity (window, self->priv->origin);
	
	CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD(plugin, unmap, 
									   (CCMPluginUnlockFunc)ccm_fade_on_map_unmap_unlocked, 
									   self);
		
	ccm_debug_window(window, "FADE UNMAP");
	ccm_timeline_set_direction (self->priv->timeline, 
								CCM_TIMELINE_BACKWARD);
	ccm_timeline_rewind(self->priv->timeline);
	ccm_timeline_start (self->priv->timeline);
	if (current_frame > 0)
	{
		guint num_frame = ccm_timeline_get_n_frames (self->priv->timeline) - 
						  current_frame;
		ccm_timeline_advance (self->priv->timeline, num_frame);
	}
	ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_fade_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_fade_window_load_options;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = NULL;
	iface->map				 = ccm_fade_map;
	iface->unmap			 = ccm_fade_unmap;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
}

