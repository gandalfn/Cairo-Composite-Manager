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
#include "ccm-extension-loader.h"
#include "ccm-mosaic.h"
#include "ccm-keybind.h"
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


CCM_DEFINE_PLUGIN (CCMMosaic, ccm_mosaic, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_mosaic_screen_iface_init))

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
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_mosaic_finalize (GObject *object)
{
	CCMMosaic* self = CCM_MOSAIC(object);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++)
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	if (self->priv->areas) g_free(self->priv->areas);
	
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
ccm_mosaic_find_area(CCMMosaic* self, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	gint id_area = -1, cpt;
	gfloat search = G_MAXFLOAT;
	
	for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
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
		for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
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
ccm_mosaic_set_window_transform(CCMMosaic* self)
{
	g_return_if_fail(self != NULL);

	gint cpt;
	GList* item = ccm_screen_get_windows(self->priv->screen);

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
	
	for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
	{
		cairo_matrix_t transform;
		gfloat scale;
		cairo_rectangle_t win_area;
		
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
				
				for (cpt = 0; cpt < self->priv->nb_areas && !found; cpt++)
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
ccm_mosaic_on_key_press(CCMMosaic* self)
{
	g_return_if_fail(self != NULL);
	
	self->priv->enabled = ~self->priv->enabled;
		
	if (self->priv->areas)
	{
		gint cpt;
		for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
		{
			ccm_drawable_pop_matrix(CCM_DRAWABLE(self->priv->areas[cpt].window),
									"CCMMosaic");
		}
		g_free(self->priv->areas);
		self->priv->areas = NULL;
	}
	self->priv->nb_areas = 0;
	
	if (self->priv->enabled) ccm_mosaic_check_area(self);
	
	ccm_screen_damage(self->priv->screen);
}

static void
ccm_mosaic_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMosaic* self = CCM_MOSAIC(plugin);
	gint cpt;
	GError* error = NULL;
	gchar* shortcut;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++)
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
	
	/*g_signal_connect_swapped(self->priv->screen, "window-destroyed", 
							 G_CALLBACK(ccm_mosaic_check_area), self);*/
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_mosaic_on_key_press), self);
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
