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

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-menu-animation.h"
#include "ccm-timeline.h"
#include "ccm.h"

enum
{
	CCM_MENU_ANIMATION_NONE = 0,
	CCM_MENU_ANIMATION_ON_MAP = 1 << 1,
	CCM_MENU_ANIMATION_ON_UNMAP = 1 << 2
};

enum
{
	CCM_MENU_ANIMATION_DURATION,
	CCM_MENU_ANIMATION_OPTION_N
};

static gchar* CCMMenuAnimationOptions[CCM_MENU_ANIMATION_OPTION_N] = {
	"duration"
};

static void ccm_menu_animation_window_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMMenuAnimation, ccm_menu_animation, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_menu_animation,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_menu_animation_window_iface_init))

struct _CCMMenuAnimationPrivate
{	
	CCMWindow*     window;
	CCMWindowType  type;
		
	CCMTimeline*   timeline;
	
	CCMConfig*     options[CCM_MENU_ANIMATION_OPTION_N];
};

#define CCM_MENU_ANIMATION_GET_PRIVATE(o)  \
   ((CCMMenuAnimationPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MENU_ANIMATION, CCMMenuAnimationClass))

static void
ccm_menu_animation_init (CCMMenuAnimation *self)
{
	self->priv = CCM_MENU_ANIMATION_GET_PRIVATE(self);
	self->priv->window = NULL;
	self->priv->type = CCM_WINDOW_TYPE_UNKNOWN;
	self->priv->timeline = NULL;
}

static void
ccm_menu_animation_finalize (GObject *object)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(object);
	
	if (self->priv->timeline) 
	{
		g_object_unref (self->priv->timeline);
		self->priv->timeline = NULL;
	}
	
	G_OBJECT_CLASS (ccm_menu_animation_parent_class)->finalize (object);
}

static void
ccm_menu_animation_class_init (CCMMenuAnimationClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMMenuAnimationPrivate));
	
	object_class->finalize = ccm_menu_animation_finalize;
}

static void
ccm_menu_animation_on_new_frame (CCMMenuAnimation* self, int num_frame, 
								 CCMTimeline* timeline)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(timeline != NULL);
	
	gdouble progress = ccm_timeline_get_progress (timeline);
	cairo_matrix_t matrix;
	
	ccm_debug_window(self->priv->window, "MENU ANIMATION %i %f", num_frame, progress);
	
	progress = MAX(progress, 0.01f);
	cairo_matrix_init_identity (&matrix);
	cairo_matrix_scale (&matrix, 
						progress > 0.5f ? 1.0f : progress  * 2, progress);
	ccm_window_set_transform (self->priv->window, &matrix);
}

static void
ccm_menu_animation_on_map_unmap_unlocked(CCMMenuAnimation* self)
{
	ccm_window_init_transfrom (self->priv->window);
}

static void
ccm_menu_animation_finish (CCMMenuAnimation* self)
{
	g_return_if_fail(self != NULL);
	
	ccm_debug_window(self->priv->window, "MENU ANIMATION COMPLETED");
	
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
ccm_menu_animation_on_completed (CCMMenuAnimation* self, CCMTimeline* timeline)
{
	ccm_menu_animation_finish(self);
}

static void
ccm_menu_animation_on_property_changed (CCMMenuAnimation* self, 
										CCMPropertyType changed,
										CCMWindow* window)
{
	if (changed == CCM_PROPERTY_HINT_TYPE)
		self->priv->type = ccm_window_get_hint_type (window);
}

static void
ccm_menu_animation_on_error (CCMMenuAnimation* self, CCMWindow* window)
{
	if (ccm_timeline_is_playing(self->priv->timeline))
	{
		ccm_timeline_stop(self->priv->timeline);
		ccm_menu_animation_finish(self);
	}
}

static void
ccm_menu_animation_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gfloat duration;
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MENU_ANIMATION_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "menu-animation", 
												  CCMMenuAnimationOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	self->priv->type = ccm_window_get_hint_type (window);
	g_signal_connect_swapped(window, "property-changed",
							 G_CALLBACK(ccm_menu_animation_on_property_changed), 
							 self);
	g_signal_connect_swapped(window, "error",
							 G_CALLBACK(ccm_menu_animation_on_error), 
							 self);
	
	duration = ccm_config_get_float(self->priv->options[CCM_MENU_ANIMATION_DURATION]);
	self->priv->timeline = ccm_timeline_new((int)(60.0 * duration), 60);
		
	g_signal_connect_swapped(self->priv->timeline, "new-frame", 
							 G_CALLBACK(ccm_menu_animation_on_new_frame), self);
	g_signal_connect_swapped(self->priv->timeline, "completed", 
							 G_CALLBACK(ccm_menu_animation_on_completed), self);
}

static void
ccm_menu_animation_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
		
	ccm_debug("MENU ANIMATION MAP: %i", self->priv->type);
	if (self->priv->type == CCM_WINDOW_TYPE_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_POPUP_MENU)
	{
		guint current_frame = 0;
		
		if (ccm_timeline_is_playing (self->priv->timeline))
		{
			current_frame = ccm_timeline_get_current_frame (self->priv->timeline);
			ccm_timeline_stop(self->priv->timeline);
			ccm_menu_animation_finish(self);
		}
		else
		{
			cairo_matrix_t matrix;
			cairo_matrix_scale(&matrix, 0.01, 0.01);
			ccm_window_set_transform (window, &matrix);
		}
		CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD(plugin, map, 
										   (CCMPluginUnlockFunc)ccm_menu_animation_on_map_unmap_unlocked,
										   self);
		
		ccm_debug_window(window, "MENU ANIMATION MAP");
		
		ccm_timeline_set_direction (self->priv->timeline, 
									CCM_TIMELINE_FORWARD);
		ccm_timeline_rewind(self->priv->timeline);
		ccm_timeline_start (self->priv->timeline);
		if (current_frame > 0)
		{
			ccm_timeline_advance (self->priv->timeline, current_frame);
		}
	}
	ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_menu_animation_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	
	if (self->priv->type == CCM_WINDOW_TYPE_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_POPUP_MENU)
	{
		guint current_frame = 0;
		
		if (ccm_timeline_is_playing (self->priv->timeline))
		{
			current_frame = ccm_timeline_get_current_frame (self->priv->timeline);
			ccm_timeline_stop(self->priv->timeline);
			ccm_menu_animation_finish(self);
		}
		else
			ccm_window_init_transfrom (window);
			
		
		CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD(plugin, unmap, 
										   (CCMPluginUnlockFunc)ccm_menu_animation_on_map_unmap_unlocked,
										   self);
		
		ccm_debug_window(window, "MENU ANIMATION UNMAP");
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
	}
	ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_menu_animation_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_menu_animation_window_load_options;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = NULL;
	iface->map				 = ccm_menu_animation_map;
	iface->unmap			 = ccm_menu_animation_unmap;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
}

