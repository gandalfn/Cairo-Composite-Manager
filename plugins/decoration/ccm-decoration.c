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

#include "ccm-cairo-utils.h"
#include "ccm-drawable.h"
#include "ccm-config.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-decoration.h"
#include "ccm-keybind.h"
#include "ccm.h"

enum
{
	CCM_DECORATION_OPACITY,
	CCM_DECORATION_OPTION_N
};

static gchar* CCMDecorationOptions[CCM_DECORATION_OPTION_N] = {
	"opacity"
};

static void ccm_decoration_window_iface_init  (CCMWindowPluginClass* iface);
static void ccm_decoration_create_mask		  (CCMDecoration* self);
static void ccm_decoration_on_property_changed(CCMDecoration* self, 
											   CCMPropertyType changed,
											   CCMWindow* window);
static void ccm_decoration_on_opacity_changed (CCMDecoration* self, 
											   CCMWindow* window);

CCM_DEFINE_PLUGIN (CCMDecoration, ccm_decoration, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_decoration,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_decoration_window_iface_init))

struct _CCMDecorationPrivate
{	
	CCMWindow	    *window;
	
	int				top;
	int				bottom;
	int				left;
	int				right;
	
	float			opacity;
	
	CCMRegion*		geometry;
	CCMRegion*		opaque;
	
	int				id_check;
	
	CCMConfig       *options[CCM_DECORATION_OPTION_N];
};

#define CCM_DECORATION_GET_PRIVATE(o)  \
   ((CCMDecorationPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DECORATION, CCMDecorationClass))

static void
ccm_decoration_init (CCMDecoration *self)
{
	gint cpt;
	
	self->priv = CCM_DECORATION_GET_PRIVATE(self);
	self->priv->window = NULL;
	self->priv->top = 0;
	self->priv->bottom = 0;
	self->priv->left = 0;
	self->priv->right = 0;
	self->priv->opacity = 1.0;
	self->priv->geometry = NULL;
	self->priv->opaque = NULL;
	self->priv->id_check = 0;
	
	for (cpt = 0; cpt < CCM_DECORATION_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_decoration_finalize (GObject *object)
{
	CCMDecoration* self = CCM_DECORATION(object);
	gint cpt;
	
	if (CCM_IS_WINDOW(self->priv->window) && 
		G_OBJECT(self->priv->window)->ref_count)
	{
		g_object_set(self->priv->window, "mask", NULL, NULL);
		g_signal_handlers_disconnect_by_func(self->priv->window, 
											 ccm_decoration_on_property_changed, 
											 self);	
		g_signal_handlers_disconnect_by_func(self->priv->window, 
											 ccm_decoration_on_opacity_changed, 
											 self);	
	}
	
	if (self->priv->opaque)
	{
		cairo_rectangle_t clipbox;
		
		ccm_region_get_clipbox(self->priv->opaque, &clipbox);
		ccm_region_offset(self->priv->opaque, 
						  -self->priv->left, -self->priv->top);
		ccm_region_resize(self->priv->opaque, 
						  clipbox.width + self->priv->left + self->priv->right,
						  clipbox.height + self->priv->top + self->priv->bottom);
		if (CCM_IS_WINDOW(self->priv->window) && 
			G_OBJECT(self->priv->window)->ref_count)
			ccm_window_set_opaque_region(self->priv->window, 
										 self->priv->opaque);

		ccm_region_destroy(self->priv->opaque);
	}
	self->priv->opaque = NULL;

	if (self->priv->geometry)
		ccm_region_destroy(self->priv->geometry);
	self->priv->geometry = NULL;

	for (cpt = 0; cpt < CCM_DECORATION_OPTION_N; cpt++)
		if (self->priv->options[cpt]) 
			g_object_unref(self->priv->options[cpt]);
	
	G_OBJECT_CLASS (ccm_decoration_parent_class)->finalize (object);
}

static void
ccm_decoration_class_init (CCMDecorationClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMDecorationPrivate));
	
	object_class->finalize = ccm_decoration_finalize;
}

static gboolean
ccm_decoration_get_opacity(CCMDecoration* self)
{
	GError* error = NULL;
	gfloat real_opacity;
	gfloat opacity;
	
	real_opacity = 
		ccm_config_get_float (self->priv->options[CCM_DECORATION_OPACITY], &error);
	if (error)
	{
		g_warning("Error on get opacity configuration value");
		g_error_free(error);
		real_opacity = 0.8f;
	}
	opacity = MAX(0.1f, real_opacity);
	opacity = MIN(1.0f, opacity);
	if (self->priv->opacity != opacity)
	{
		self->priv->opacity = opacity;
		if (opacity != real_opacity)
			ccm_config_set_float (self->priv->options[CCM_DECORATION_OPACITY], 
								  opacity, NULL);
		return TRUE;
	}
	
	return FALSE;
}

static void
ccm_decoration_on_option_changed(CCMDecoration* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_DECORATION_OPACITY] &&
		ccm_decoration_get_opacity (self) && self->priv->window &&
		self->priv->geometry)
	{
		ccm_decoration_create_mask(self);
		ccm_drawable_damage(CCM_DRAWABLE(self->priv->window));
	}
}

static void
ccm_decoration_on_property_changed(CCMDecoration* self, CCMPropertyType changed,
								   CCMWindow* window)
{
	if (self->priv->geometry && 
		(changed == CCM_PROPERTY_OPACITY || 
		 changed == CCM_PROPERTY_FRAME_EXTENDS))
	{
		ccm_decoration_create_mask(self);
	}
}

static void
ccm_decoration_on_opacity_changed(CCMDecoration* self, CCMWindow* window)
{
	if (self->priv->geometry)
	{
		ccm_decoration_create_mask(self);
	}
}

static void
ccm_decoration_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_DECORATION_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "decoration", 
												  CCMDecorationOptions[cpt]);
		if (self->priv->options[cpt])
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_decoration_on_option_changed), 
								 self);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	
	ccm_decoration_get_opacity (self);
	
	g_signal_connect_swapped(window, "property-changed",
							 G_CALLBACK(ccm_decoration_on_property_changed), 
							 self);
	g_signal_connect_swapped(window, "opacity-changed",
							 G_CALLBACK(ccm_decoration_on_opacity_changed), 
							 self);
}

static void
ccm_decoration_create_mask(CCMDecoration* self)
{
	g_return_if_fail(self != NULL);
	
	cairo_surface_t* mask = NULL;
	
	if (self->priv->opaque)
		ccm_region_destroy(self->priv->opaque);
	self->priv->opaque = NULL;
	
	g_object_set(self->priv->window, "mask", mask, NULL);
		
	ccm_window_get_frame_extends(self->priv->window, &self->priv->left,
								 &self->priv->right, &self->priv->top,
								 &self->priv->bottom);
	
	if (self->priv->left || self->priv->right || 
		self->priv->top || self->priv->bottom)
	{
		cairo_t* ctx;
		cairo_rectangle_t clipbox, *rects;
		gint cpt, nb_rects;
		gfloat opacity = ccm_window_get_opacity(self->priv->window);
		CCMRegion* decoration, *tmp;
		cairo_surface_t* surface = 
			ccm_drawable_get_surface(CCM_DRAWABLE(self->priv->window));
		
		ccm_window_get_frame_extends(self->priv->window, &self->priv->left,
									 &self->priv->right, &self->priv->top,
									 &self->priv->bottom);
		
		ccm_region_get_clipbox(self->priv->geometry, &clipbox);
		mask = cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR_ALPHA,
										    clipbox.width, clipbox.height);
		cairo_surface_destroy(surface);
		g_object_set(self->priv->window, "mask", mask, NULL);
		
		ctx = cairo_create(mask);
		cairo_set_source_rgba(ctx, 0, 0, 0, 0);
		cairo_paint(ctx);
		
		cairo_translate(ctx, -clipbox.x, -clipbox.y);
		
		clipbox.x += self->priv->left;
		clipbox.y += self->priv->top;
		clipbox.width -= self->priv->left + self->priv->right;
		clipbox.height -= self->priv->top + self->priv->bottom;
		self->priv->opaque = ccm_region_rectangle(&clipbox);
				
		decoration = ccm_region_copy(self->priv->geometry);
		tmp = ccm_region_rectangle(&clipbox);
		ccm_region_subtract(decoration, tmp);
		ccm_region_destroy(tmp);
		
		ccm_region_get_rectangles(decoration, &rects, &nb_rects);
		cairo_set_source_rgba(ctx, 1, 1, 1, self->priv->opacity * opacity);
		for (cpt = 0; cpt < nb_rects; cpt++)
			cairo_rectangle(ctx, rects[cpt].x, rects[cpt].y, 
							rects[cpt].width, rects[cpt].height);
		g_free(rects);
		cairo_fill(ctx);
		ccm_region_destroy(decoration);
		
		cairo_set_source_rgba(ctx, 1, 1, 1, opacity);
		cairo_rectangle(ctx, clipbox.x, clipbox.y, 
						clipbox.width, clipbox.height);
		cairo_fill(ctx);
		
		cairo_destroy(ctx);
	}
}

static CCMRegion*
ccm_decoration_window_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	CCMRegion* geometry = NULL;
	
	if (self->priv->geometry)
		ccm_region_destroy(self->priv->geometry);
	self->priv->geometry = NULL;

	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	if (geometry) 
	{
		self->priv->geometry = ccm_region_copy(geometry);
		ccm_decoration_create_mask(self);
	}
	
	return geometry;
}

static gboolean
ccm_decoration_window_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
							cairo_t* context, cairo_surface_t* surface,
							gboolean y_invert)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	gboolean ret = FALSE;
	cairo_surface_t* mask = NULL;
	
	if (self->priv->geometry && self->priv->opaque)
	{
		CCMRegion* decoration = ccm_region_copy(self->priv->geometry);
		CCMRegion* damaged = NULL;
		
		ccm_region_subtract(decoration, self->priv->opaque);
		g_object_get(G_OBJECT(window), "damaged", &damaged, NULL);
		if (damaged)
		{
			ccm_region_intersect(decoration, damaged);
			if (ccm_region_empty(decoration))
			{
				g_object_get(G_OBJECT(window), "mask", &mask, NULL);
				if (mask)
				{
					cairo_surface_reference(mask);
					g_object_set(G_OBJECT(window), "mask", NULL, NULL);
				}
			}
		}
		ccm_region_destroy(decoration);
	}
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin),
								  window, context, surface, y_invert);
	
	if (mask)
	{
		g_object_set(G_OBJECT(window), "mask", mask, NULL);
	}
	
	return ret;
}

static void 
ccm_decoration_window_set_opaque_region(CCMWindowPlugin* plugin, 
										CCMWindow* window,
										const CCMRegion* area)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
       
    if (self->priv->opaque) 
	{
		CCMRegion* opaque = ccm_region_copy(self->priv->opaque);
		
		ccm_region_intersect (opaque, (CCMRegion*)area);
	
		ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT(plugin), 
											 window, opaque);
		ccm_region_destroy (opaque);
	}
	else
		ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT(plugin), 
											 window, area);
}

static void 
ccm_decoration_window_move(CCMWindowPlugin* plugin, CCMWindow* window, 
						   int x, int y)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	cairo_rectangle_t clipbox;
	
	if (self->priv->geometry)
	{
		ccm_region_get_clipbox(self->priv->geometry, &clipbox);
		ccm_region_offset(self->priv->geometry, x - clipbox.x, 
						  y - clipbox.y);
	}
	if (self->priv->opaque)
	{
		ccm_region_get_clipbox(self->priv->opaque, &clipbox);
		ccm_region_offset(self->priv->opaque, 
						  (x - clipbox.x) + self->priv->left, 
						  (y - clipbox.y) + self->priv->top);
	}
	
	ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT(plugin), window, x, y);
}

static void 
ccm_decoration_window_resize(CCMWindowPlugin* plugin, CCMWindow* window, 
							 int width, int height)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	
	if (self->priv->geometry) 
		ccm_region_resize(self->priv->geometry, width, height);
	if (self->priv->opaque) 
		ccm_region_resize(self->priv->opaque, 
						  width - (self->priv->left + self->priv->right), 
						  height - (self->priv->top + self->priv->bottom));
	
	ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT(plugin), window,
							  width, height);
}


static void
ccm_decoration_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_decoration_window_load_options;
	iface->query_geometry 	 = ccm_decoration_window_query_geometry;
	iface->paint 			 = ccm_decoration_window_paint;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity	 = NULL;
	iface->move				 = ccm_decoration_window_move;
	iface->resize			 = ccm_decoration_window_resize;
	iface->set_opaque_region = ccm_decoration_window_set_opaque_region;
	iface->get_origin		 = NULL;
}

