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
#include <X11/Xlib.h>
#include <math.h>

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-timeline.h"
#include "ccm-freeze.h"
#include "ccm-config.h"
#include "ccm-debug.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-adjustment.h"
#include "ccm-config-color-button.h"
#include "ccm.h"

enum
{
	CCM_FREEZE_DELAY,
	CCM_FREEZE_DURATION,
	CCM_FREEZE_COLOR,
	CCM_FREEZE_OPTION_N
};

static const gchar* CCMFreezeOptionKeys[CCM_FREEZE_OPTION_N] = {
	"delay",
	"duration",
	"color"
};

typedef struct 
{
	CCMPluginOptions parent;
	gint			 delay;
	gfloat			 duration;
	GdkColor*	     color;
} CCMFreezeOptions;

static void ccm_freeze_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_freeze_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface);
static void ccm_freeze_on_event(CCMFreeze* self, XEvent* event, 
                                CCMDisplay* display);
static void ccm_freeze_on_option_changed(CCMPlugin* plugin, CCMConfig* config);

CCM_DEFINE_PLUGIN (CCMFreeze, ccm_freeze, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_freeze,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_freeze_window_iface_init);
                   CCM_IMPLEMENT_INTERFACE(ccm_freeze,
                                           CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                           ccm_freeze_preferences_page_iface_init))

struct _CCMFreezePrivate
{	
	gboolean	alive;
	gfloat		opacity;
	
	CCMWindow*  window;
	
	guint		id_ping;
	glong		last_ping;
	glong	    pid;
	
	CCMTimeline* timeline;
	GtkBuilder*  builder;
};

#define CCM_FREEZE_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FREEZE, CCMFreezePrivate))

static CCMPluginOptions*
ccm_freeze_options_init(CCMPlugin* plugin)
{
	CCMFreezeOptions* options = g_slice_new0(CCMFreezeOptions);
	
	options->delay = 3;
	options->duration = 0.3f;
	options->color = NULL;

	return (CCMPluginOptions*)options;
}

static void
ccm_freeze_options_finalize(CCMPlugin* plugin, CCMPluginOptions* opts)
{
	CCMFreezeOptions* options = (CCMFreezeOptions*)opts;
	
	if (options->color) g_free(options->color);
	options->color = NULL;
	g_slice_free(CCMFreezeOptions, options);
}

static void
ccm_freeze_init (CCMFreeze *self)
{
	self->priv = CCM_FREEZE_GET_PRIVATE(self);
	self->priv->alive = TRUE;
	self->priv->opacity = 0.0f;
	self->priv->window = NULL;
	self->priv->id_ping = 0;
	self->priv->last_ping = 0;
	self->priv->pid = 0;
	self->priv->timeline = NULL;
	self->priv->builder = NULL;
}

static void
ccm_freeze_finalize (GObject *object)
{
	CCMFreeze* self = CCM_FREEZE(object);

	ccm_plugin_options_unload(CCM_PLUGIN(self));
	
	if (CCM_IS_WINDOW(self->priv->window))
	{
		CCMDisplay* display = 
				ccm_drawable_get_display(CCM_DRAWABLE(self->priv->window));
		if (CCM_IS_DISPLAY(display))
			g_signal_handlers_disconnect_by_func(display, 
												 ccm_freeze_on_event, 
												 self);	
	}

	self->priv->window = NULL;
	if (self->priv->id_ping) g_source_remove (self->priv->id_ping);
	self->priv->opacity = 0.0f;
	self->priv->id_ping = 0;
	self->priv->last_ping = 0;
	if (self->priv->timeline) 
		g_object_unref (self->priv->timeline);
	if (self->priv->builder)
		g_object_unref (self->priv->builder);
	
	G_OBJECT_CLASS (ccm_freeze_parent_class)->finalize (object);
}

static void
ccm_freeze_class_init (CCMFreezeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMFreezePrivate));

	CCM_PLUGIN_CLASS(klass)->options_init = ccm_freeze_options_init;
	CCM_PLUGIN_CLASS(klass)->options_finalize = ccm_freeze_options_finalize;
	CCM_PLUGIN_CLASS(klass)->option_changed = ccm_freeze_on_option_changed;
	
	object_class->finalize = ccm_freeze_finalize;
}

static void
ccm_freeze_on_new_frame (CCMFreeze* self, guint num_frame, 
						 CCMTimeline* timeline)
{
	if (!self->priv->alive && self->priv->opacity < 0.5)
	{
		gfloat progress = ccm_timeline_get_progress (timeline);
		
		self->priv->opacity = progress / 2.0f;
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
}

static void
ccm_freeze_on_event(CCMFreeze* self, XEvent* event, CCMDisplay* display)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);

	if (!CCM_IS_FREEZE(self) || !CCM_IS_DISPLAY(display)) return;
	
	if (self->priv->window && event->type == ClientMessage)
	{
		Window window = None;
		XClientMessageEvent* client_message_event = (XClientMessageEvent*)event;
		
		g_object_get (G_OBJECT(self->priv->window), "child", &window, NULL);
    	
		if (!window) window = CCM_WINDOW_XWINDOW(self->priv->window);
			
		if (self->priv->last_ping &&
			client_message_event->message_type == 
						CCM_WINDOW_GET_CLASS(self->priv->window)->protocol_atom &&
		    client_message_event->data.l[0] == 
					    CCM_WINDOW_GET_CLASS(self->priv->window)->ping_atom &&
			client_message_event->data.l[2] == window)
		{
			ccm_debug_window(self->priv->window, "PONG 0x%x", client_message_event->window);
			if (!self->priv->alive)
				ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
			self->priv->alive = TRUE;
			self->priv->last_ping = 0;
			self->priv->opacity = 0.0f;
			if (self->priv->timeline &&
			    ccm_timeline_is_playing(self->priv->timeline))
				ccm_timeline_stop(self->priv->timeline);
		}
	}
}

static void
ccm_freeze_get_pid(CCMFreeze* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(self->priv->window != NULL);
	
	guint32* data = NULL;
	guint n_items;
	Window child = None;
	
	g_object_get(G_OBJECT(self->priv->window), "child", &child, NULL);
	
	if (!child)
	{
		data = ccm_window_get_property (self->priv->window, 
										CCM_WINDOW_GET_CLASS(self->priv->window)->pid_atom,
										XA_CARDINAL, 
										&n_items);
	}
	else
	{
		data = ccm_window_get_child_property (self->priv->window, 
											  CCM_WINDOW_GET_CLASS(self->priv->window)->pid_atom,
											  XA_CARDINAL, 
											  &n_items);
	}
	
	if (data)
	{
		self->priv->pid = (gulong)*data;
		g_free(data);
	}
	else
	{
		self->priv->pid = 0;
	}
}

static gboolean
ccm_freeze_ping(CCMFreeze* self)
{
	g_return_val_if_fail(CCM_IS_FREEZE(self), FALSE);
	
	if (self->priv->window && ccm_window_is_viewable (self->priv->window))
	{
		CCMWindowType type = ccm_window_get_hint_type (self->priv->window);
		
		if (!self->priv->pid)
			ccm_freeze_get_pid(self);
		
		if (!self->priv->pid || ccm_window_is_input_only (self->priv->window) ||
			!ccm_window_is_viewable (self->priv->window) ||
			!ccm_window_is_decorated (self->priv->window) ||
			(type != CCM_WINDOW_TYPE_NORMAL && type != CCM_WINDOW_TYPE_DIALOG))
		{
			if (!self->priv->alive)
				ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
				
			self->priv->alive = TRUE;
			if (self->priv->timeline &&
			    ccm_timeline_is_playing(self->priv->timeline))
				ccm_timeline_stop(self->priv->timeline);
			return FALSE;
		}
		
		ccm_debug_window(self->priv->window, "PING");
		XEvent event;
		CCMDisplay* display = 
			ccm_drawable_get_display (CCM_DRAWABLE(self->priv->window));
		Window window = None;

		g_return_val_if_fail(CCM_IS_DISPLAY(display), FALSE);
		
		g_object_get (G_OBJECT(self->priv->window), "child", &window, NULL);
		
		if (!window) window = CCM_WINDOW_XWINDOW(self->priv->window);
		
		if (self->priv->last_ping)
		{
			self->priv->alive = FALSE;
			if (!self->priv->timeline)
			{
				self->priv->timeline = 
					ccm_timeline_new_for_duration ((guint)(ccm_freeze_get_option(self)->duration * 1000.0));
		
				g_signal_connect_swapped(self->priv->timeline, "new-frame", 
										 G_CALLBACK(ccm_freeze_on_new_frame), self);
			}
			if (!ccm_timeline_is_playing(self->priv->timeline))
				ccm_timeline_start(self->priv->timeline);
		}
		else
		{
			if (!self->priv->alive)
				ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
			
			self->priv->alive = TRUE;
			self->priv->opacity = 0.0f;
			
			if (self->priv->timeline && 
			    ccm_timeline_is_playing(self->priv->timeline))
				ccm_timeline_stop(self->priv->timeline);
		}
		
		self->priv->last_ping = 1;
		
		event.type = ClientMessage;
		event.xclient.window = window;
		event.xclient.message_type = CCM_WINDOW_GET_CLASS(self->priv->window)->protocol_atom;
		event.xclient.format = 32;
		event.xclient.data.l[0] = CCM_WINDOW_GET_CLASS(self->priv->window)->ping_atom;
		event.xclient.data.l[1] = self->priv->last_ping;
		event.xclient.data.l[2] = window;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;
		XSendEvent (CCM_DISPLAY_XDISPLAY(display), window, FALSE, 
					NoEventMask, &event);
	}
	
	
	return TRUE;
}

static void
ccm_freeze_on_option_changed(CCMPlugin* plugin, CCMConfig* config)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(config != NULL);

	CCMFreeze* self = CCM_FREEZE(plugin);

	if (config == ccm_freeze_get_config(self, CCM_FREEZE_DELAY))
	{
		ccm_freeze_get_option(self)->delay = 
			ccm_config_get_integer(ccm_freeze_get_config(self, CCM_FREEZE_DELAY), 
			                       NULL);
		if (!ccm_freeze_get_option(self)->delay) 
			ccm_freeze_get_option(self)->delay = 3;
	}
	
	if (config == ccm_freeze_get_config(self, CCM_FREEZE_DURATION))
	{
		ccm_freeze_get_option(self)->duration = 
			ccm_config_get_float(ccm_freeze_get_config(self, CCM_FREEZE_DURATION),
								 NULL);
		if (!ccm_freeze_get_option(self)->duration) 
			ccm_freeze_get_option(self)->duration = 0.3f;

		if (self->priv->timeline) 
		{
			g_object_unref(self->priv->timeline);
			self->priv->timeline = NULL;
		}
	}

	if (config == ccm_freeze_get_config(self, CCM_FREEZE_COLOR))
	{
		if (ccm_freeze_get_option(self)->color) 
			g_free(ccm_freeze_get_option(self)->color);
		
		ccm_freeze_get_option(self)->color = 
			ccm_config_get_color(ccm_freeze_get_config(self, CCM_FREEZE_COLOR),
								 NULL);
	}	
}

static void
ccm_freeze_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	
	ccm_plugin_options_load(CCM_PLUGIN(self), "freeze", CCMFreezeOptionKeys,
	                        CCM_FREEZE_OPTION_N);
	
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	g_signal_connect_swapped(G_OBJECT(display), "event", 
							 G_CALLBACK(ccm_freeze_on_event), self);
}

static gboolean 
ccm_freeze_window_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
                        cairo_t* context, cairo_surface_t* surface,
                        gboolean y_invert)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	gboolean ret;
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window, 
								  context, surface, y_invert);
	
	if (ccm_window_is_viewable (window) && !self->priv->alive)
	{
		CCMRegion* tmp = ccm_window_get_area_geometry (window);
		int cpt, nb_rects;
		cairo_rectangle_t* rects;
		cairo_save(context);
		
		ccm_region_get_rectangles(tmp, &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; ++cpt)
			cairo_rectangle(context, rects[cpt].x, rects[cpt].y, 
							rects[cpt].width, rects[cpt].height);
		cairo_clip(context);
		cairo_rectangles_free(rects, nb_rects);
		ccm_region_destroy(tmp);
		if (!ccm_freeze_get_option(self)->color)
			cairo_set_source_rgb(context, 0, 0, 0);
		else
			gdk_cairo_set_source_color(context, 
			                           ccm_freeze_get_option(self)->color);
		cairo_paint_with_alpha(context, self->priv->opacity);
		cairo_restore(context);
	}
	
	return ret;
}

static void
ccm_freeze_window_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	
	if (!self->priv->id_ping)
		self->priv->id_ping = g_timeout_add_seconds_full (G_PRIORITY_LOW, 
												  ccm_freeze_get_option(self)->delay, 
												  (GSourceFunc)ccm_freeze_ping, 
												  self, NULL);
	ccm_window_plugin_map(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_freeze_window_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	
	self->priv->window = window;
	if (self->priv->id_ping)	
	{
		g_source_remove (self->priv->id_ping);
		if (self->priv->timeline) ccm_timeline_stop(self->priv->timeline);
		self->priv->id_ping = 0;
	}
	ccm_window_plugin_unmap(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_freeze_preferences_page_init_windows_section(CCMPreferencesPagePlugin* plugin,
                                                 CCMPreferencesPage* preferences,
                                                 GtkWidget* windows_section)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	
	self->priv->builder = gtk_builder_new();

	if (gtk_builder_add_from_file(self->priv->builder, 
	                              UI_DIR "/ccm-freeze.ui", NULL))
	{
		GtkWidget* widget = 
			GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "freeze"));
		if (widget)
		{
			gint screen_num = ccm_preferences_page_get_screen_num (preferences);
			
			gtk_box_pack_start(GTK_BOX(windows_section), widget, 
			                   FALSE, TRUE, 0);
			
			CCMConfigColorButton* color = 
				CCM_CONFIG_COLOR_BUTTON(gtk_builder_get_object(self->priv->builder, 
				                                               "color"));
			g_object_set(color, "screen", screen_num, NULL);

			CCMConfigAdjustment* delay = 
				CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
				                                             "delay-adjustment"));
			g_object_set(delay, "screen", screen_num, NULL);
			
			CCMConfigAdjustment* duration = 
				CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
				                                             "duration-adjustment"));
			g_object_set(duration, "screen", screen_num, NULL);

			ccm_preferences_page_section_register_widget (preferences,
			                                              CCM_PREFERENCES_PAGE_SECTION_WINDOW,
			                                              widget, "freeze");
		}
	}
	ccm_preferences_page_plugin_init_windows_section (CCM_PREFERENCES_PAGE_PLUGIN_PARENT(plugin),
													  preferences, windows_section);
}

static void
ccm_freeze_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_freeze_window_load_options;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = ccm_freeze_window_paint;
	iface->map				 = ccm_freeze_window_map;
	iface->unmap			 = ccm_freeze_window_unmap;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
	iface->get_origin		 = NULL;
}

static void
ccm_freeze_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface)
{
	iface->init_general_section       = NULL;
    iface->init_desktop_section       = NULL;
    iface->init_windows_section       = ccm_freeze_preferences_page_init_windows_section;
    iface->init_effects_section		  = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section     = NULL;
}
