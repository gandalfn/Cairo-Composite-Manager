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
#include <string.h>

#include "ccm-property-async.h"
#include "ccm-config.h"
#include "ccm-debug.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-menu-animation.h"
#include "ccm-timeline.h"
#include "ccm.h"

#define CCM_MENU_ANIMATION_DIV 7
enum
{
	CCM_MENU_ANIMATION_DISABLE = 0,
	CCM_MENU_ANIMATION_LEFT = 1 << 1,
	CCM_MENU_ANIMATION_RIGHT = 1 << 2,
	CCM_MENU_ANIMATION_MIDDLE = 1 << 3,
	CCM_MENU_ANIMATION_TOP = 1 << 4,
	CCM_MENU_ANIMATION_BOTTOM = 1 << 5
};

enum
{
	CCM_MENU_ANIMATION_DURATION,
	CCM_MENU_ANIMATION_OPTION_N
};

static gchar* CCMMenuAnimationOptions[CCM_MENU_ANIMATION_OPTION_N] = {
	"duration"
};

static void ccm_menu_animation_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_menu_animation_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_menu_animation_on_error (CCMMenuAnimation* self, 
										 CCMWindow* window);
static void ccm_menu_animation_on_property_changed (CCMMenuAnimation* self, 
													CCMPropertyType changed,
													CCMWindow* window);

CCM_DEFINE_PLUGIN (CCMMenuAnimation, ccm_menu_animation, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_menu_animation,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_menu_animation_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_menu_animation,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_menu_animation_window_iface_init))

struct _CCMMenuAnimationPrivate
{	
	CCMScreen*	   screen;
	
	CCMWindow*     window;
	CCMWindowType  type;
		
	CCMTimeline*   timeline;
	gfloat 		   duration;
	guint		   x_pos;
	guint		   y_pos;
	gboolean	   forced_animation;
	
	CCMConfig*     options[CCM_MENU_ANIMATION_OPTION_N];
};

#define CCM_MENU_ANIMATION_GET_PRIVATE(o)  \
   ((CCMMenuAnimationPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MENU_ANIMATION, CCMMenuAnimationClass))

static void
ccm_menu_animation_init (CCMMenuAnimation *self)
{
	gint cpt;
	
	self->priv = CCM_MENU_ANIMATION_GET_PRIVATE(self);
	self->priv->window = NULL;
	self->priv->type = CCM_WINDOW_TYPE_UNKNOWN;
	self->priv->timeline = NULL;
	self->priv->duration = 0.1f;
	self->priv->x_pos = CCM_MENU_ANIMATION_LEFT;
	self->priv->y_pos = CCM_MENU_ANIMATION_TOP;
	self->priv->forced_animation = FALSE;
	
	for (cpt = 0; cpt < CCM_MENU_ANIMATION_OPTION_N; ++cpt) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_menu_animation_finalize (GObject *object)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(object);
	gint cpt;
	
	if (self->priv->window)
	{
		g_signal_handlers_disconnect_by_func(self->priv->window, 
											 ccm_menu_animation_on_property_changed, 
											 self);	
	
		g_signal_handlers_disconnect_by_func(self->priv->window, 
											 ccm_menu_animation_on_error, 
											 self);	
	}
	
	for (cpt = 0; cpt < CCM_MENU_ANIMATION_OPTION_N; ++cpt)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = NULL;
	}
	
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
ccm_menu_animation_on_get_animation_property(CCMMenuAnimation* self,  
											 guint n_items, gchar* result, 
											 CCMPropertyASync* prop)
{
	g_return_if_fail(CCM_IS_PROPERTY_ASYNC(prop));
	
	if (!CCM_IS_MENU_ANIMATION(self)) 
	{
		g_object_unref(prop);
		return;
	}
	
	Atom property = ccm_property_async_get_property (prop);
	
	if (result)
	{
		if (property == CCM_MENU_ANIMATION_GET_CLASS(self)->animation_atom)
		{
			gulong animation;
			memcpy(&animation, result, sizeof(gulong));
		
			ccm_debug_window(self->priv->window, "_CCM_ANIMATION_ENABLE %i",
			                 animation);

			self->priv->forced_animation = TRUE;
	
			if (animation & CCM_MENU_ANIMATION_LEFT)
				self->priv->x_pos = CCM_MENU_ANIMATION_LEFT;
			else if (animation & CCM_MENU_ANIMATION_RIGHT)
				self->priv->x_pos = CCM_MENU_ANIMATION_RIGHT;
			else 
				self->priv->x_pos = CCM_MENU_ANIMATION_MIDDLE;
	
			if (animation & CCM_MENU_ANIMATION_TOP)
				self->priv->y_pos = CCM_MENU_ANIMATION_TOP;
			else if (animation & CCM_MENU_ANIMATION_BOTTOM)
				self->priv->y_pos = CCM_MENU_ANIMATION_BOTTOM;
			else 
				self->priv->y_pos = CCM_MENU_ANIMATION_MIDDLE;
		}
	}
	
	g_object_unref(prop);
}

static void
ccm_menu_animation_query_forced_animation(CCMMenuAnimation* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(self->priv->window != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self->priv->window));
	
	Window child = None;
	
	g_object_get(G_OBJECT(self->priv->window), "child", &child, NULL);
	
	if (!child)
	{
		ccm_debug_window(self->priv->window, "QUERY ANIMATION 0x%x", child);
		CCMPropertyASync* prop = 
			ccm_property_async_new (display, 
			                        CCM_WINDOW_XWINDOW(self->priv->window),
			                        CCM_MENU_ANIMATION_GET_CLASS(self)->animation_atom,
			                        XA_CARDINAL, 32);
		
		g_signal_connect(prop, "error", G_CALLBACK(g_object_unref), NULL);
		g_signal_connect_swapped(prop, "reply", 
		                         G_CALLBACK(ccm_menu_animation_on_get_animation_property), 
		                         self);
	}
	else
	{
		ccm_debug_window(self->priv->window, "QUERY CHILD ANIMATION 0x%x", child);
		CCMPropertyASync* prop = 
			ccm_property_async_new (display, child,
			                        CCM_MENU_ANIMATION_GET_CLASS(self)->animation_atom,
			                        XA_CARDINAL, 32);
		
		g_signal_connect(prop, "error", G_CALLBACK(g_object_unref), NULL);
		g_signal_connect_swapped(prop, "reply", 
		                         G_CALLBACK(ccm_menu_animation_on_get_animation_property), 
		                         self);	
	}
}

static void
ccm_menu_animation_create_atoms(CCMMenuAnimation* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_MENU_ANIMATION_GET_CLASS(self) != NULL);
	
	CCMMenuAnimationClass* klass = CCM_MENU_ANIMATION_GET_CLASS(self);
	
	if (!klass->animation_atom)
	{
		CCMDisplay* display = 
			ccm_drawable_get_display(CCM_DRAWABLE(self->priv->window));
		
		klass->animation_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											 "_CCM_ANIMATION_ENABLE", 
											 False);
	}
}

static void
ccm_menu_animation_get_position (CCMMenuAnimation* self)
{
	if (self->priv->window)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self->priv->window));
		cairo_rectangle_t geometry;
		gint x, y;
		
		if (ccm_screen_query_pointer (screen, NULL, &x, &y) &&
			ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self->priv->window),
											   &geometry))
		{
			if (x < geometry.x + (geometry.width / CCM_MENU_ANIMATION_DIV))
			{
				self->priv->x_pos = CCM_MENU_ANIMATION_LEFT;
				ccm_debug("ON LEFT");
			}
			if (x >= geometry.x + (geometry.width / CCM_MENU_ANIMATION_DIV) && 
				x <= geometry.x + (((CCM_MENU_ANIMATION_DIV - 1) * 
									geometry.width) / CCM_MENU_ANIMATION_DIV))
			{
				self->priv->x_pos = CCM_MENU_ANIMATION_MIDDLE;
				ccm_debug("ON MIDDLE");
			}
			if (x > geometry.x + (((CCM_MENU_ANIMATION_DIV - 1) * 
								   geometry.width) / CCM_MENU_ANIMATION_DIV))
			{
				self->priv->x_pos = CCM_MENU_ANIMATION_RIGHT;
				ccm_debug("ON RIGHT");
			}
			if (y < geometry.y + (geometry.height / CCM_MENU_ANIMATION_DIV))
			{
				self->priv->y_pos = CCM_MENU_ANIMATION_TOP;
				ccm_debug("ON TOP");
			}
			if (y >= geometry.y + (geometry.height / CCM_MENU_ANIMATION_DIV) && 
				y <= geometry.y + (((CCM_MENU_ANIMATION_DIV - 1) * 
									geometry.height) / CCM_MENU_ANIMATION_DIV))
			{
				self->priv->y_pos = CCM_MENU_ANIMATION_MIDDLE;
				ccm_debug("ON MIDDLE");
			}
			if (y > geometry.y + (((CCM_MENU_ANIMATION_DIV - 1) * 
								   geometry.height) / CCM_MENU_ANIMATION_DIV))
			{
				self->priv->y_pos = CCM_MENU_ANIMATION_BOTTOM;
				ccm_debug("ON BOTTOM");
			}
		}
	}
}

static void
ccm_menu_animation_foreach_transient(CCMWindow* window, CCMRegion* damage)
{
	g_return_if_fail(window != NULL);
	g_return_if_fail(damage != NULL);
	
	GSList* transients = ccm_window_get_transients(window);
	const CCMRegion* geometry = ccm_drawable_get_geometry (CCM_DRAWABLE(window));

	if (ccm_window_is_viewable (window) &&
		!ccm_window_is_input_only (window) &&
		geometry && !ccm_region_empty ((CCMRegion*)geometry))
		ccm_region_union (damage, (CCMRegion*)geometry);
	if (transients) 
		g_slist_foreach(transients, 
						(GFunc)ccm_menu_animation_foreach_transient,
						damage);
}

static void
ccm_menu_animation_on_new_frame (CCMMenuAnimation* self, int num_frame, 
								 CCMTimeline* timeline)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(timeline != NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self->priv->window));
	const CCMRegion* tmp;
	CCMRegion* damage = ccm_region_new ();
	gdouble progress = ccm_timeline_get_progress (timeline);
	cairo_rectangle_t geometry;
	cairo_matrix_t matrix;
	GSList* transients = ccm_window_get_transients(self->priv->window);
	
	ccm_debug_window(self->priv->window, "MENU ANIMATION %i %f", num_frame, progress);
	
	ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE(self->priv->window),
											  &geometry);
	cairo_matrix_init_identity (&matrix);
	switch (self->priv->y_pos)
	{
		case CCM_MENU_ANIMATION_TOP:
			matrix.yy = progress;
			break;
		case CCM_MENU_ANIMATION_BOTTOM:
			matrix.yy = progress;
			matrix.y0 = geometry.height - (geometry.height * matrix.yy);
			break;
		default:
			break;
	}
	switch (self->priv->x_pos)
	{
		case CCM_MENU_ANIMATION_LEFT:
			if (self->priv->y_pos == CCM_MENU_ANIMATION_TOP ||
				self->priv->y_pos == CCM_MENU_ANIMATION_BOTTOM)
				matrix.xx = progress > 0.5f ? 1.0f : progress  * 2;
			else
				matrix.xx = progress;
			break;
		case CCM_MENU_ANIMATION_RIGHT:
			if (self->priv->y_pos == CCM_MENU_ANIMATION_TOP ||
				self->priv->y_pos == CCM_MENU_ANIMATION_BOTTOM)
				matrix.xx = progress > 0.5f ? 1.0f : progress  * 2;
			else
				matrix.xx = progress;
			matrix.x0 = geometry.width - (geometry.width * matrix.xx);
			break;
		default:
			break;
	}
	
	
	if (transients) 
		g_slist_foreach(transients, 
						(GFunc)ccm_menu_animation_foreach_transient,
						damage);
	tmp = ccm_drawable_get_geometry (CCM_DRAWABLE(self->priv->window));
	if (tmp && !ccm_region_empty ((CCMRegion*)tmp))
		ccm_region_union (damage, (CCMRegion*)tmp);
	
	ccm_drawable_push_matrix (CCM_DRAWABLE(self->priv->window), 
							  "CCMMenuAnimation", &matrix);
	if (transients) 
		g_slist_foreach(transients, 
						(GFunc)ccm_menu_animation_foreach_transient,
						damage);
	tmp = ccm_drawable_get_geometry (CCM_DRAWABLE(self->priv->window));
	if (tmp && !ccm_region_empty ((CCMRegion*)tmp))
		ccm_region_union (damage, (CCMRegion*)tmp);
	
	if (!ccm_region_empty (damage))
		ccm_screen_damage_region (screen, damage);
	
	ccm_region_destroy(damage);
}

static void
ccm_menu_animation_on_map_unmap_unlocked(CCMMenuAnimation* self)
{
	ccm_drawable_pop_matrix (CCM_DRAWABLE(self->priv->window),
							 "CCMMenuAnimation");
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
	if (self->priv->timeline && 
		ccm_timeline_is_playing(self->priv->timeline))
	{
		ccm_timeline_stop(self->priv->timeline);
		ccm_menu_animation_finish(self);
	}
}

static gboolean
ccm_menu_animation_get_duration(CCMMenuAnimation* self)
{
	GError* error = NULL;
	gfloat real_duration = 
		ccm_config_get_float(self->priv->options[CCM_MENU_ANIMATION_DURATION],
							 &error);
	gfloat duration;
	
	if (error)
	{
		g_error_free(error);
		g_warning("Error on get menu animation duration configuration value");
		real_duration = 0.2f;
	}
	duration = MAX(0.1f, real_duration);
	duration = MIN(2.0f, real_duration);
	if (duration != self->priv->duration)
	{
		CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self->priv->window));
		guint refresh_rate;
		
		g_object_get (G_OBJECT(screen), "refresh_rate", &refresh_rate, NULL);
		
		if (self->priv->timeline) g_object_unref (self->priv->timeline);

		self->priv->duration = duration;
		if (duration != real_duration)
			ccm_config_set_float(self->priv->options[CCM_MENU_ANIMATION_DURATION],
								 self->priv->duration, NULL);
		
		self->priv->timeline = ccm_timeline_new((int)(refresh_rate * duration), 
												refresh_rate);
	
		g_signal_connect_swapped(self->priv->timeline, "new-frame", 
							 	 G_CALLBACK(ccm_menu_animation_on_new_frame), 
								 self);
		g_signal_connect_swapped(self->priv->timeline, "completed", 
								 G_CALLBACK(ccm_menu_animation_on_completed), 
								 self);
		
		return TRUE;
	}
	
	return FALSE;
}

static void
ccm_menu_animation_on_option_changed(CCMMenuAnimation* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_MENU_ANIMATION_DURATION])
	{
		ccm_menu_animation_get_duration (self);
	}
}

static void
ccm_menu_animation_on_event(CCMMenuAnimation* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	switch (event->type)
	{
		case PropertyNotify:
		{
			XPropertyEvent* property_event = (XPropertyEvent*)event;
			CCMWindow* window;
			
			if (property_event->atom == CCM_MENU_ANIMATION_GET_CLASS(self)->animation_atom)
			{
				window = ccm_screen_find_window_or_child (self->priv->screen,
														  property_event->window);
				if (window) 
				{
					CCMMenuAnimation* plugin = 
						CCM_MENU_ANIMATION(_ccm_window_get_plugin (window, 
													CCM_TYPE_MENU_ANIMATION));
					ccm_menu_animation_query_forced_animation(plugin);
				}
			}
		}
		break;
		default:
		break;
	}
}

static void
ccm_menu_animation_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	CCMDisplay* display = ccm_screen_get_display(screen);
	
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	self->priv->screen = screen;
	g_signal_connect_swapped(display, "event", 
							 G_CALLBACK(ccm_menu_animation_on_event), self);
}

static void
ccm_menu_animation_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MENU_ANIMATION_OPTION_N; ++cpt)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "menu-animation", 
												  CCMMenuAnimationOptions[cpt]);
		if (self->priv->options[cpt])
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_menu_animation_on_option_changed), 
								 self);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	ccm_menu_animation_create_atoms(self);
	
	self->priv->type = ccm_window_get_hint_type (window);
	g_signal_connect_swapped(window, "property-changed",
							 G_CALLBACK(ccm_menu_animation_on_property_changed), 
							 self);
	g_signal_connect_swapped(window, "error",
							 G_CALLBACK(ccm_menu_animation_on_error), 
							 self);	
	
	ccm_menu_animation_get_duration (self);
}

static CCMRegion*
ccm_menu_animation_window_query_geometry(CCMWindowPlugin* plugin, 
										 CCMWindow* window)
{
	CCMRegion* geometry = NULL;
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	
	ccm_menu_animation_query_forced_animation(self);

	return geometry;
}

static void
ccm_menu_animation_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
		
	ccm_debug("MENU ANIMATION MAP: %i", self->priv->type);
	if (self->priv->forced_animation ||
		self->priv->type == CCM_WINDOW_TYPE_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_POPUP_MENU)
	{
		guint current_frame = 0;
		
		if (!self->priv->forced_animation)
			ccm_menu_animation_get_position (self);
		if (ccm_timeline_is_playing (self->priv->timeline))
		{
			current_frame = ccm_timeline_get_current_frame (self->priv->timeline);
			ccm_timeline_stop(self->priv->timeline);
			ccm_menu_animation_finish(self);
		}
		else
		{
			cairo_matrix_t matrix;
			cairo_matrix_scale(&matrix, 0., 0.);
			ccm_drawable_push_matrix (CCM_DRAWABLE(self->priv->window), 
									  "CCMMenuAnimation", &matrix);

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
	
	if (self->priv->forced_animation ||
		self->priv->type == CCM_WINDOW_TYPE_MENU ||
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
			ccm_drawable_pop_matrix (CCM_DRAWABLE(self->priv->window), 
									 "CCMMenuAnimation");
		
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
	iface->query_geometry 	 = ccm_menu_animation_window_query_geometry;
	iface->paint 			 = NULL;
	iface->map				 = ccm_menu_animation_map;
	iface->unmap			 = ccm_menu_animation_unmap;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
	iface->get_origin		 = NULL;
}

static void
ccm_menu_animation_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_menu_animation_screen_load_options;
	iface->paint			= NULL;
	iface->add_window		= NULL;
	iface->remove_window	= NULL;
	iface->damage			= NULL;
}
