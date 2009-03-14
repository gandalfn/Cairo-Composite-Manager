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
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

//#define CCM_DEBUG_ENABLE
#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-mosaic.h"
#include "ccm-keybind.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-entry-shortcut.h"
#include "ccm-config-adjustment.h"
#include "ccm.h"

enum
{
	CCM_MOSAIC_SPACING,
	CCM_MOSAIC_SHORTCUT,
	CCM_MOSAIC_OPTION_N
};

static gchar* CCMMosaicOptions[CCM_MOSAIC_OPTION_N] = {
	"spacing",
	"shortcut"
};

static void ccm_mosaic_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_mosaic_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_mosaic_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface);
static void ccm_mosaic_check_windows(CCMMosaic* self);

CCM_DEFINE_PLUGIN (CCMMosaic, ccm_mosaic, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_mosaic_screen_iface_init);
                   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_mosaic_window_iface_init);
                   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
                                           CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                           ccm_mosaic_preferences_page_iface_init))

typedef struct {
	cairo_rectangle_t   geometry;
	CCMWindow*			window;
} CCMMosaicArea;

struct _CCMMosaicPrivate
{	
	CCMScreen*			 screen;
	gboolean 			 enabled;
	
	CCMMosaicArea*     	 areas;
	gint				 nb_areas;
	
	CCMKeybind*			 keybind;

	gboolean			 mouse_over;

	GtkBuilder*			 builder;
	
	CCMConfig*           options[CCM_MOSAIC_OPTION_N];
};

#define CCM_MOSAIC_GET_PRIVATE(o)  \
   ((CCMMosaicPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MOSAIC, CCMMosaicClass))

static void
ccm_mosaic_init (CCMMosaic *self)
{
	gint cpt;
	
	self->priv = CCM_MOSAIC_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->enabled = FALSE;
	self->priv->areas = NULL;
	self->priv->nb_areas = 0;
	self->priv->keybind = NULL;
	self->priv->mouse_over = FALSE;
	self->priv->builder = NULL;
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; ++cpt) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_mosaic_finalize (GObject *object)
{
	CCMMosaic* self = CCM_MOSAIC(object);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; ++cpt)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = NULL;
	}
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	self->priv->keybind = NULL;
	if (self->priv->areas) g_free(self->priv->areas);
	self->priv->areas = NULL;
	if (self->priv->builder) g_object_unref(self->priv->builder);
	self->priv->builder = NULL;
	
	G_OBJECT_CLASS (ccm_mosaic_parent_class)->finalize (object);
}

static void
ccm_mosaic_class_init (CCMMosaicClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMMosaicPrivate));

	object_class->finalize = ccm_mosaic_finalize;
}

static void
ccm_mosaic_switch_keep_above(CCMMosaic* self, CCMWindow* window,
							 gboolean keep_above)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	XEvent event;
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(window));
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMWindow* root = ccm_screen_get_root_window(screen);
	Window child = None;
	
	g_object_get(G_OBJECT(window), "child", &child, NULL);
	event.xclient.type = ClientMessage;
	
	event.xclient.message_type = CCM_WINDOW_GET_CLASS(window)->state_atom;
	event.xclient.display = CCM_DISPLAY_XDISPLAY(display);
	event.xclient.window = child ? child : CCM_WINDOW_XWINDOW(window);
	event.xclient.send_event = True;
	event.xclient.format = 32;
	event.xclient.data.l[0] = keep_above ? 1 : 0;
	event.xclient.data.l[1] = CCM_WINDOW_GET_CLASS(window)->state_above_atom;
	event.xclient.data.l[2] = 0l;
	event.xclient.data.l[3] = 0l;
	event.xclient.data.l[4] = 0l;

	XSendEvent (CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root), True,
				SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

static void
ccm_mosaic_find_area(CCMMosaic* self, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	gint id_area = -1, cpt;
	gfloat search = G_MAXFLOAT;
	
	for (cpt = 0; cpt < self->priv->nb_areas; ++cpt)
	{
		gfloat scale;
		const cairo_rectangle_t* win_area = ccm_window_get_area(window);
		
		scale = MIN(self->priv->areas[cpt].geometry.height / win_area->height,
					self->priv->areas[cpt].geometry.width / win_area->width);
		
		if (sqrt(pow(1.0f - scale, 2)) < search && !self->priv->areas[cpt].window)
		{
			search = sqrt(pow(1.0f - scale, 2));
			id_area = cpt;
		}
	}
	
	if (id_area < 0)
	{
		for (cpt = 0; cpt < self->priv->nb_areas; ++cpt)
		{   
			if (!self->priv->areas[cpt].window)
				self->priv->areas[cpt].window = window;
		}
	}
	else
	{
		self->priv->areas[id_area].window = window;
	}
}

static void
ccm_mosaic_on_window_enter_notify(CCMMosaic* self, CCMWindow* window, 
                                  CCMScreen* screen)
{
	CCMMosaic* plugin = CCM_MOSAIC(_ccm_window_get_plugin(window,
	                                                      CCM_TYPE_MOSAIC));
	if (plugin)
	{
		plugin->priv->mouse_over = TRUE;
		ccm_drawable_damage (CCM_DRAWABLE(window));
	}
}

static void
ccm_mosaic_on_window_leave_notify(CCMMosaic* self, CCMWindow* window, 
                                  CCMScreen* screen)
{
	CCMMosaic* plugin = CCM_MOSAIC(_ccm_window_get_plugin(window,
	                                                      CCM_TYPE_MOSAIC));
	if (plugin && plugin->priv->enabled)
	{
		plugin->priv->mouse_over = FALSE;
		ccm_drawable_damage (CCM_DRAWABLE(window));
	}
}

static void
ccm_mosaic_on_window_activate_notify(CCMMosaic* self, CCMWindow* window, 
                                     CCMScreen* screen)
{
	CCMMosaic* plugin = CCM_MOSAIC(_ccm_window_get_plugin(window,
	                                                      CCM_TYPE_MOSAIC));
	if (plugin && plugin->priv->enabled)
	{
		plugin->priv->mouse_over = FALSE;
		self->priv->enabled = FALSE;
		ccm_mosaic_check_windows(self);
	}
}

static void
ccm_mosaic_set_window_transform(CCMMosaic* self)
{
	g_return_if_fail(self != NULL);

	gint cpt, x, y;
	GList* item = ccm_screen_get_windows(self->priv->screen);
	CCMWindow* mouse = NULL;

	ccm_screen_query_pointer (self->priv->screen, &mouse, &x, &y);

	for (;item; item = item->next)
	{
		CCMWindowType type = ccm_window_get_hint_type(item->data);
		
		if (ccm_window_is_viewable(item->data) &&
			ccm_window_is_decorated(item->data)  &&
			type == CCM_WINDOW_TYPE_NORMAL) 
		{
			ccm_mosaic_find_area(self, item->data);
		}
	}
	
	for (cpt = 0; cpt < self->priv->nb_areas; ++cpt)
	{
		cairo_matrix_t transform;
		gfloat scale;
		cairo_rectangle_t win_area;
		CCMMosaic* plugin = CCM_MOSAIC(_ccm_window_get_plugin(self->priv->areas[cpt].window,
                                                  CCM_TYPE_MOSAIC));
		if (plugin) 
		{
			plugin->priv->enabled = TRUE;
			plugin->priv->mouse_over = FALSE;
		}
		
		ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self->priv->areas[cpt].window),
										  &win_area);
		
		cairo_matrix_init_identity(&transform);
		
		scale = MIN(self->priv->areas[cpt].geometry.height / win_area.height,
					self->priv->areas[cpt].geometry.width / win_area.width);
		
		cairo_matrix_translate (&transform, 
								(self->priv->areas[cpt].geometry.x - win_area.x) -
								(((win_area.width * scale) - self->priv->areas[cpt].geometry.width) / 2),
								(self->priv->areas[cpt].geometry.y - win_area.y) - 
								(((win_area.height * scale) - self->priv->areas[cpt].geometry.height) / 2));
		cairo_matrix_scale(&transform, scale, scale);
		ccm_drawable_push_matrix(CCM_DRAWABLE(self->priv->areas[cpt].window),
								 "CCMMosaic", &transform);
		ccm_mosaic_switch_keep_above(self, self->priv->areas[cpt].window, TRUE);
		g_object_set(G_OBJECT(self->priv->areas[cpt].window), 
		             "block_mouse_redirect_event", TRUE, NULL);
	}

	for (cpt = 0; cpt < self->priv->nb_areas; ++cpt)
	{
		CCMMosaic* plugin = CCM_MOSAIC(_ccm_window_get_plugin(self->priv->areas[cpt].window,
                                                  CCM_TYPE_MOSAIC));
		if (plugin) 
			plugin->priv->mouse_over = mouse == self->priv->areas[cpt].window;
	}
}

static void
ccm_mosaic_create_areas(CCMMosaic* self, gint nb_windows)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(nb_windows != 0);
	
	GError* error = NULL;
	int i, j, lines, n;
	int x, y, width, height;
    int spacing = 
		ccm_config_get_integer (self->priv->options [CCM_MOSAIC_SPACING],
								&error);
    
	if (error)
	{
		g_error_free(error);
		error = NULL;
		g_warning("Error on get mosaic spacing configuration value");
		spacing = 20;
	}
	if (self->priv->areas) g_free(self->priv->areas);
	self->priv->areas = g_new0(CCMMosaicArea, nb_windows);
    lines = sqrt (nb_windows + 1);
    self->priv->nb_areas = 0;
    
	y = spacing;
    height = (CCM_SCREEN_XSCREEN(self->priv->screen)->height - (lines + 1) * spacing) / lines;

    for (i = 0; i < lines; i++)
    {
	    n = MIN (nb_windows - self->priv->nb_areas, ceilf ((float)nb_windows / lines));
    	x = spacing;
	    width = (CCM_SCREEN_XSCREEN(self->priv->screen)->width - (n + 1) * spacing) / n;
        for (j = 0; j < n; j++)
	    {
	        self->priv->areas[self->priv->nb_areas].geometry.x = x;
	        self->priv->areas[self->priv->nb_areas].geometry.y = y;
            self->priv->areas[self->priv->nb_areas].geometry.width = width;
	        self->priv->areas[self->priv->nb_areas].geometry.height = height;

	        x += width + spacing;
            
	        self->priv->nb_areas++;
	    }
	    y += height + spacing;
    }
}

static void
ccm_mosaic_check_area(CCMMosaic* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->enabled) 
	{
		GList* item = ccm_screen_get_windows(self->priv->screen);
		gboolean changed = FALSE;
		gint nb_windows = 0;

		for (;item; item = item->next)
		{
			CCMWindowType type = ccm_window_get_hint_type(item->data);
			
			if (ccm_window_is_viewable(item->data) &&
				ccm_window_is_decorated(item->data) &&
				type == CCM_WINDOW_TYPE_NORMAL) 
			{
				gint cpt;
				gboolean found = FALSE;
				
				for (cpt = 0; cpt < self->priv->nb_areas && !found; ++cpt)
					 found = self->priv->areas[cpt].window == item->data;
				
				changed |= !found;
				nb_windows++;
			}
		}
		if (nb_windows != self->priv->nb_areas || changed) 
		{
			ccm_mosaic_create_areas(self, nb_windows);
			ccm_mosaic_set_window_transform(self);
		}
	}
}

static void
ccm_mosaic_check_windows(CCMMosaic* self)
{
	g_return_if_fail(self != NULL);

	if (self->priv->areas)
	{
		gint cpt;

		for (cpt = 0; cpt < self->priv->nb_areas; ++cpt)
		{
			if (CCM_IS_WINDOW(self->priv->areas[cpt].window))
			{
				CCMMosaic* plugin = CCM_MOSAIC(_ccm_window_get_plugin(self->priv->areas[cpt].window,
	                                                      CCM_TYPE_MOSAIC));
				if (plugin) 
				{
					plugin->priv->enabled = FALSE;
					plugin->priv->mouse_over = FALSE;
				}

				ccm_drawable_pop_matrix(CCM_DRAWABLE(self->priv->areas[cpt].window),
										"CCMMosaic");
				ccm_mosaic_switch_keep_above(self, self->priv->areas[cpt].window,
											 FALSE);
				g_object_set(G_OBJECT(self->priv->areas[cpt].window), 
				             "block_mouse_redirect_event", FALSE, NULL);
			}
		}
		g_free(self->priv->areas);
		self->priv->areas = NULL;
	}
	self->priv->nb_areas = 0;
	
	if (self->priv->enabled) ccm_mosaic_check_area(self);
	
	ccm_screen_damage(self->priv->screen);
}

static void
ccm_mosaic_on_key_press(CCMMosaic* self)
{
	g_return_if_fail(self != NULL);
	
	self->priv->enabled = ~self->priv->enabled;
	
	ccm_mosaic_check_windows(self);
}

static void
ccm_mosaic_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMosaic* self = CCM_MOSAIC(plugin);
	gint cpt;
	GError* error = NULL;
	gchar* shortcut;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; ++cpt)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "mosaic", 
												  CCMMosaicOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	shortcut = 
	   ccm_config_get_string(self->priv->options [CCM_MOSAIC_SHORTCUT], &error);
	if (error)
	{
		g_error_free(error);
		error = NULL;
		g_warning("Error on get mosaic shortcut configuration value");
		shortcut = g_strdup("<Super>Tab");
	}
	self->priv->keybind = ccm_keybind_new(self->priv->screen, shortcut, TRUE);
	g_free(shortcut);
	
	g_signal_connect_swapped(self->priv->screen, "enter-window-notify",
	                         G_CALLBACK(ccm_mosaic_on_window_enter_notify),
	                         self);
	g_signal_connect_swapped(self->priv->screen, "leave-window-notify",
	                         G_CALLBACK(ccm_mosaic_on_window_leave_notify),
	                         self);
	g_signal_connect_swapped(self->priv->screen, "activate-window-notify",
	                         G_CALLBACK(ccm_mosaic_on_window_activate_notify),
	                         self);
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_mosaic_on_key_press), self);
}

static gboolean 
ccm_mosaic_window_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
                        cairo_t* context, cairo_surface_t* surface,
                        gboolean y_invert)
{
	CCMMosaic* self = CCM_MOSAIC(plugin);
	gboolean ret;
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window, 
								  context, surface, y_invert);
	
	if (self->priv->enabled && !self->priv->mouse_over)
	{
		CCMRegion* tmp = ccm_window_get_area_geometry (window);
		int cpt, nb_rects;
		cairo_rectangle_t* rects;

		cairo_save(context);
		cairo_set_source_rgba(context, 0, 0, 0, 0.5);
		ccm_region_get_rectangles(tmp, &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; ++cpt)
			cairo_rectangle(context, rects[cpt].x, rects[cpt].y, 
							rects[cpt].width, rects[cpt].height);
		cairo_fill(context);
		g_free(rects);
		ccm_region_destroy(tmp);
		cairo_restore(context);
	}
	
	return ret;
}

static void
ccm_mosaic_preferences_page_init_desktop_section(CCMPreferencesPagePlugin* plugin,
                                                 CCMPreferencesPage* preferences,
                                                 GtkWidget* desktop_section)
{
	CCMMosaic* self = CCM_MOSAIC(plugin);
	
	self->priv->builder = gtk_builder_new();

	if (gtk_builder_add_from_file(self->priv->builder, 
	                              UI_DIR "/ccm-mosaic.ui", NULL))
	{
		GtkWidget* widget = 
			GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "mosaic"));
		if (widget)
		{
			gint screen_num = ccm_preferences_page_get_screen_num (preferences);
			
			gtk_box_pack_start(GTK_BOX(desktop_section), widget, 
			                   FALSE, TRUE, 0);

			CCMConfigAdjustment* spacing = 
				CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
				                                             "spacing-adjustment"));
			g_object_set(spacing, "screen", screen_num, NULL);
			
			CCMConfigEntryShortcut* shortcut = 
				CCM_CONFIG_ENTRY_SHORTCUT(gtk_builder_get_object(self->priv->builder,
				                                                 "shortcut"));
			g_object_set(shortcut, "screen", screen_num, NULL);

			ccm_preferences_page_section_register_widget (preferences,
			                                              CCM_PREFERENCES_PAGE_SECTION_DESKTOP,
			                                              widget, "mosaic");
		}
	}
	ccm_preferences_page_plugin_init_desktop_section (CCM_PREFERENCES_PAGE_PLUGIN_PARENT(plugin),
													  preferences, desktop_section);
}

static void
ccm_mosaic_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_mosaic_screen_load_options;
	iface->paint 			= NULL;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
	iface->damage			= NULL;
}

static void
ccm_mosaic_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = NULL;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = ccm_mosaic_window_paint;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
	iface->get_origin		 = NULL;
}

static void
ccm_mosaic_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface)
{
	iface->init_general_section       = NULL;
    iface->init_desktop_section       = ccm_mosaic_preferences_page_init_desktop_section;
    iface->init_windows_section       = NULL;
    iface->init_effects_section		  = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section     = NULL;
}
