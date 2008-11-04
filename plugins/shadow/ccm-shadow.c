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
#include "ccm-shadow.h"
#include "ccm-config.h"
#include "ccm-cairo-utils.h"
#include "ccm.h"

enum
{
	CCM_SHADOW_BORDER,
	CCM_SHADOW_RADIUS,
	CCM_SHADOW_COLOR,
	CCM_SHADOW_OPTION_N
};

enum
{
	CCM_SHADOW_SIDE_TOP,
	CCM_SHADOW_SIDE_RIGHT,
	CCM_SHADOW_SIDE_BOTTOM,
	CCM_SHADOW_SIDE_LEFT,
	CCM_SHADOW_SIDE_N
};

static gchar* CCMShadowOptions[CCM_SHADOW_OPTION_N] = {
	"border",
	"radius",
	"color"
};

static void ccm_shadow_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMShadow, ccm_shadow, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_shadow,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_shadow_iface_init))

struct _CCMShadowPrivate
{
	int 				border;
	int 				radius;
	GdkColor*			color;
	
	guint 				id_check;
	
	CCMWindow* 			window;
	
	cairo_surface_t* 	shadow[CCM_SHADOW_SIDE_N];
	
	CCMRegion* 			geometry;
	
	CCMConfig* 			options[CCM_SHADOW_OPTION_N];
};

#define CCM_SHADOW_GET_PRIVATE(o)  \
   ((CCMShadowPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SHADOW, CCMShadowClass))

static void
ccm_shadow_init (CCMShadow *self)
{
	gint cpt;
	
	self->priv = CCM_SHADOW_GET_PRIVATE(self);
	
	self->priv->id_check = 0;
	self->priv->border = 12;
	self->priv->radius = 8;
	self->priv->color = NULL;
	self->priv->window = NULL;
	self->priv->geometry = NULL;
	for (cpt = 0; cpt < CCM_SHADOW_SIDE_N; cpt++)
		self->priv->shadow[cpt] = NULL;
	for (cpt = 0; cpt < CCM_SHADOW_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_shadow_finalize (GObject *object)
{
	CCMShadow* self = CCM_SHADOW(object);
	gint cpt;
	
	if (self->priv->id_check) g_source_remove (self->priv->id_check);
	
	for (cpt = 0; cpt < CCM_SHADOW_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) 
		{
			g_object_unref(self->priv->options[cpt]);
			self->priv->options[cpt] = NULL;
		}
	}
	
	for (cpt = 0; cpt < CCM_SHADOW_SIDE_N; cpt++)
	{
		if (self->priv->shadow[cpt])
			cairo_surface_destroy(self->priv->shadow[cpt]);
		self->priv->shadow[cpt] = NULL;
	}
	
	if (self->priv->geometry) 
	{
		ccm_region_destroy (self->priv->geometry);
		self->priv->geometry = NULL;
	}
	
	if (self->priv->color) g_free(self->priv->color);
	
	G_OBJECT_CLASS (ccm_shadow_parent_class)->finalize (object);
}

static void
ccm_shadow_class_init (CCMShadowClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMShadowPrivate));
	
	object_class->finalize = ccm_shadow_finalize;
}

static void
ccm_shadow_create_shadow(CCMShadow* self, CCMWindow* window)
{
	cairo_surface_t* tmp = NULL;
	cairo_rectangle_t clipbox, *rects;
	cairo_t* ctx;
	gint cpt;
	
	ccm_region_get_clipbox(self->priv->geometry, &clipbox);
	if (clipbox.width == 0 || clipbox.height == 0 || self->priv->border == 0)
		return;
	
	for (cpt = 0; cpt < CCM_SHADOW_SIDE_N; cpt++)
	{
		double width = 0, height = 0;
		gint i, nb_rects;
		CCMRegion* area = ccm_region_copy(self->priv->geometry);
		ccm_region_offset(area, -clipbox.x, -clipbox.y);
		
		switch (cpt)
		{
			case CCM_SHADOW_SIDE_TOP:
				ccm_region_offset(area, self->priv->border / 2, 
								  self->priv->border / 2);
				width = clipbox.width + self->priv->border;
				height = self->priv->border;
			break;
			case CCM_SHADOW_SIDE_BOTTOM:
				ccm_region_offset(area, self->priv->border / 2, 
								  - clipbox.height + self->priv->border / 2);
				width = clipbox.width + self->priv->border;
				height = self->priv->border;
			break;
			case CCM_SHADOW_SIDE_RIGHT:
				ccm_region_offset(area, - clipbox.width + self->priv->border / 2, 
								  self->priv->border / 2);
				width = self->priv->border;
				height = clipbox.height + self->priv->border;
			break;
			case CCM_SHADOW_SIDE_LEFT:
				ccm_region_offset(area, self->priv->border / 2, 
								  self->priv->border / 2);
				width = self->priv->border;
				height = clipbox.height + self->priv->border;
			break;
		}
		if (self->priv->shadow[cpt])
			cairo_surface_destroy(self->priv->shadow[cpt]);
		if (cpt == CCM_SHADOW_SIDE_RIGHT || cpt == CCM_SHADOW_SIDE_LEFT)
		{
			tmp = 
				cairo_image_surface_create (CAIRO_FORMAT_ARGB32, height, width);
			ctx = cairo_create(tmp);
			cairo_translate(ctx, height / 2, width / 2);
			cairo_rotate(ctx, - M_PI / 2);
			cairo_translate(ctx, - width / 2, - height / 2);
		}
		else
		{
			self->priv->shadow[cpt] = 
				cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
			ctx = cairo_create(self->priv->shadow[cpt]);
		}
		
		cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
		cairo_paint(ctx);
		cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
		if (self->priv->color)
			gdk_cairo_set_source_color(ctx, self->priv->color);
		else
			cairo_set_source_rgb (ctx, 0.f, 0.f, 0.f);
		ccm_region_get_rectangles(area, &rects, &nb_rects);
		for (i = 0; i < nb_rects; i++)
			cairo_rectangle(ctx, rects[i].x, rects[i].y, 
							rects[i].width, rects[i].height);
		g_free(rects);		
		ccm_region_destroy(area);
		cairo_fill(ctx);
		cairo_destroy(ctx);
		if (cpt == CCM_SHADOW_SIDE_RIGHT || cpt == CCM_SHADOW_SIDE_LEFT)
		{
			cairo_image_surface_blur(tmp, self->priv->radius, 
									 self->priv->radius);
			self->priv->shadow[cpt] = 
				cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
			ctx = cairo_create(self->priv->shadow[cpt]);
			cairo_translate(ctx, width / 2, height / 2);
			cairo_rotate(ctx, M_PI / 2);
			cairo_translate(ctx, - height / 2, - width / 2); 
			cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_surface(ctx, tmp, 0, 0);
			cairo_paint(ctx);
			cairo_destroy(ctx);
			cairo_surface_destroy(tmp);
		}
		else
		{
			cairo_image_surface_blur(self->priv->shadow[cpt], 
									 self->priv->radius, self->priv->radius);
		}
	}
}

static void
ccm_shadow_paint_shadow(CCMShadow* self, CCMWindow* window, cairo_t* context)
{
	cairo_rectangle_t area;
		
	if (self->priv->geometry && 
		ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(window), &area))
	{
		gint i, nb_rects;
		cairo_rectangle_t* rects;
		CCMRegion* tmp = ccm_region_rectangle(&area);
		
		ccm_region_subtract(tmp, self->priv->geometry);
		cairo_save(context);
		ccm_region_get_rectangles(tmp, &rects, &nb_rects);
		for (i = 0; i < nb_rects; i++)
			cairo_rectangle(context, rects[i].x, rects[i].y,
							rects[i].width, rects[i].height);
		cairo_clip(context);
		g_free(rects);
		ccm_region_destroy(tmp);
		
		for (i = 0; i < CCM_SHADOW_SIDE_N; i++)
		{
			cairo_save(context);	
			if (ccm_window_transform (window, context, FALSE))
			{
				switch (i)
				{
					case CCM_SHADOW_SIDE_TOP:
						cairo_rectangle (context, self->priv->border, 0, 
										 area.width - 2 * self->priv->border, 
										 self->priv->border);
						cairo_move_to(context, self->priv->border, 0);
						cairo_line_to(context, 0, 0);
						cairo_line_to(context, self->priv->border, 
									  self->priv->border);
						cairo_move_to(context, area.width - self->priv->border, 0);
						cairo_line_to(context, area.width, 0);
						cairo_line_to(context, area.width - self->priv->border, 
									  self->priv->border);
						cairo_clip(context);
					break;
					case CCM_SHADOW_SIDE_RIGHT:
						cairo_rectangle (context, 
										 area.width - self->priv->border, 
										 self->priv->border, 
										 self->priv->border, 
										 area.height - 2 * self->priv->border);
						cairo_move_to(context, area.width, self->priv->border);
						cairo_line_to(context, area.width, 0);
						cairo_line_to(context, area.width - self->priv->border, 
									  self->priv->border);
						cairo_move_to(context, area.width, 
									  area.height - self->priv->border);
						cairo_line_to(context, area.width, area.height);
						cairo_line_to(context, area.width - self->priv->border, 
									  area.height - self->priv->border);
						cairo_clip(context);
						cairo_translate (context, 
										 area.width - self->priv->border, 
										 0);
					break;
					case CCM_SHADOW_SIDE_BOTTOM:
						cairo_rectangle (context, 
										 self->priv->border, 
										 area.height - self->priv->border, 
										 area.width - 2 * self->priv->border, 
										 self->priv->border);
						cairo_move_to(context, area.width - self->priv->border, 
									  area.height);
						cairo_line_to(context, area.width, area.height);
						cairo_line_to(context, area.width - self->priv->border, 
									  area.height - self->priv->border);
						cairo_move_to(context, self->priv->border, area.height);
						cairo_line_to(context, 0, area.height);
						cairo_line_to(context, self->priv->border, 
									  area.height - self->priv->border);
						cairo_clip(context);
						cairo_translate (context, 
										 0, 
										 area.height - self->priv->border);
					break;
					case CCM_SHADOW_SIDE_LEFT:
						cairo_rectangle (context, 0, 
										 self->priv->border, 
										 self->priv->border, 
										 area.height - 2 * self->priv->border);
						cairo_move_to(context, 0, 
									  area.height - self->priv->border);
						cairo_line_to(context, 0, area.height);
						cairo_line_to(context, self->priv->border, 
									  area.height - self->priv->border);
						cairo_move_to(context, 0, self->priv->border);
						cairo_line_to(context, 0, 0);
						cairo_line_to(context, self->priv->border, 
									  self->priv->border);
						cairo_clip(context);
					break;
				}
				cairo_set_source_surface(context, self->priv->shadow[i], 
										0,0);
				cairo_paint_with_alpha(context, ccm_window_get_opacity(window));
			}
			cairo_restore(context);
		}
		cairo_restore(context);
	}	
}

static gboolean
ccm_shadow_check_needed(CCMShadow* self)
{
	g_return_val_if_fail(CCM_IS_SHADOW(self), FALSE);
	
	ccm_drawable_query_geometry(CCM_DRAWABLE(self->priv->window));
	ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));

	self->priv->id_check = 0;
	
	return FALSE;
}

static gboolean 
ccm_shadow_need_shadow(CCMWindow* window)
{
	g_return_val_if_fail(window != NULL, FALSE);
	
	CCMWindowType type = ccm_window_get_hint_type(window);
	const CCMRegion* opaque = ccm_window_get_opaque_region (window);
	
	return !ccm_window_is_input_only (window) &&
		   (ccm_window_is_decorated (window) || 
		    (type != CCM_WINDOW_TYPE_NORMAL && 
			 type != CCM_WINDOW_TYPE_DIALOG && opaque)) && 
			((type == CCM_WINDOW_TYPE_DOCK && opaque) ||
			 type != CCM_WINDOW_TYPE_DOCK) &&
		   (ccm_window_is_managed(window) ||   
			type == CCM_WINDOW_TYPE_DOCK ||
			type == CCM_WINDOW_TYPE_DROPDOWN_MENU || 
			type == CCM_WINDOW_TYPE_POPUP_MENU || 
			type == CCM_WINDOW_TYPE_TOOLTIP || 
			type == CCM_WINDOW_TYPE_MENU);
}

static void
ccm_shadow_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_SHADOW_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "shadow", 
												  CCMShadowOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	self->priv->border = ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	self->priv->radius = ccm_config_get_integer(self->priv->options[CCM_SHADOW_RADIUS]);
	self->priv->color = ccm_config_get_color(self->priv->options[CCM_SHADOW_COLOR]);
}

static CCMRegion*
ccm_shadow_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMRegion* geometry = NULL;
	cairo_rectangle_t area;
	CCMShadow* self = CCM_SHADOW(plugin);
	gint cpt;
	
	if (self->priv->geometry) ccm_region_destroy (self->priv->geometry);
	self->priv->geometry = NULL;
	for (cpt = 0; cpt < 4; cpt++)
	{
		if (self->priv->shadow[cpt])
			cairo_surface_destroy(self->priv->shadow[cpt]);
		self->priv->shadow[cpt] = NULL;
	}
	
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	if (geometry && ccm_shadow_need_shadow(window))
	{
		int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	
		self->priv->geometry = ccm_region_copy (geometry);
		ccm_region_get_clipbox(geometry, &area);
		ccm_shadow_create_shadow(self, window);
		ccm_region_offset(geometry, -border / 2, -border / 2);
		ccm_region_resize(geometry, area.width + border, area.height + border);
	}
	
	return geometry;
}

static void
ccm_shadow_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	gboolean need = ccm_shadow_need_shadow(window);

	if ((need && !self->priv->shadow[0]) || 
		(!need && self->priv->shadow[0]))
	{
		if (!self->priv->id_check) 
			g_idle_add ((GSourceFunc)ccm_shadow_check_needed, self);
	}
	
	ccm_window_plugin_map(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static gboolean
ccm_shadow_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
				 cairo_t* context, cairo_surface_t* surface,
				 gboolean y_invert)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	gboolean need = ccm_shadow_need_shadow(window);
	gboolean ret = FALSE;
	
	if ((need && !self->priv->shadow[0]) || 
		(!need && self->priv->shadow[0]))
	{
		if (!self->priv->id_check) 
			g_idle_add ((GSourceFunc)ccm_shadow_check_needed, self);
	}
	
	if (need && self->priv->geometry)
	{
		cairo_rectangle_t* rects;
		gint cpt, nb_rects;
		cairo_matrix_t matrix, initial;
		int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
			
		cairo_save(context);
			
		ccm_window_get_transform(window, &matrix);
		ccm_window_get_transform(window, &initial);

		ccm_region_get_rectangles (self->priv->geometry, &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; cpt++)
			cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
							 rects[cpt].width, rects[cpt].height);
		cairo_clip(context);
		g_free(rects);
		
		cairo_matrix_translate(&matrix, border / 2, border / 2);
		ccm_window_set_transform(window, &matrix, FALSE);
		ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin),
									  window, context, surface, y_invert);
		ccm_window_set_transform(window, &initial, FALSE);
		cairo_restore(context);
		
		ccm_shadow_paint_shadow(self, window, context);
	} 
	else
		ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin),
									  window, context, surface, y_invert);
	
	return ret;
}

static void 
ccm_shadow_move(CCMWindowPlugin* plugin, CCMWindow* window, 
				int x, int y)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	cairo_rectangle_t area;
	
	if (self->priv->shadow[0])
	{
		if (self->priv->geometry)
		{
			int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
		
			ccm_region_get_clipbox(self->priv->geometry, &area);
			ccm_region_offset(self->priv->geometry, x - area.x, y - area.y);
			x -= border / 2;
			y -= border / 2;
		}
	}	
	ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT(plugin), window, x, y);
}

static void 
ccm_shadow_resize(CCMWindowPlugin* plugin, CCMWindow* window, 
				  int width, int height)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	int border = 0;
	
	if (self->priv->shadow[0])
		border = ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	
	ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT(plugin), window,
							  width + border, height + border);
}


static void 
ccm_shadow_set_opaque_region(CCMWindowPlugin* plugin, CCMWindow* window,
							 const CCMRegion* area)
{
	CCMShadow* self = CCM_SHADOW(plugin);
       
    if (self->priv->geometry) 
	{
		CCMRegion* opaque = ccm_region_copy(self->priv->geometry);
		
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
ccm_shadow_get_origin(CCMWindowPlugin* plugin, CCMWindow* window, 
					  int* x, int* y)
{
	CCMShadow* self = CCM_SHADOW(plugin);
    cairo_rectangle_t geometry;
	
    if (self->priv->geometry)
	{
		ccm_region_get_clipbox(self->priv->geometry, &geometry);
		*x = geometry.x;
		*y = geometry.y;
	}
	else
	{
		ccm_window_plugin_get_origin (CCM_WINDOW_PLUGIN_PARENT(plugin), 
									  window, x, y);
	}
}

static void
ccm_shadow_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_shadow_load_options;
	iface->query_geometry 	 = ccm_shadow_query_geometry;
	iface->paint 			 = ccm_shadow_paint;
	iface->map				 = ccm_shadow_map;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = ccm_shadow_move;
	iface->resize			 = ccm_shadow_resize;
	iface->set_opaque_region = ccm_shadow_set_opaque_region;
	iface->get_origin		 = ccm_shadow_get_origin;
}

