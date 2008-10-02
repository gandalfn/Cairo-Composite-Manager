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
#include "ccm-window.h"
#include "ccm-shadow.h"
#include "ccm-config.h"
#include "ccm.h"

enum
{
	CCM_SHADOW_BORDER,
	CCM_SHADOW_OFFSET,
	CCM_SHADOW_OPTION_N
};

static gchar* CCMShadowOptions[CCM_SHADOW_OPTION_N] = {
	"border",
	"offset"
};

static void ccm_shadow_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMShadow, ccm_shadow, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_shadow,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_shadow_iface_init))

struct _CCMShadowPrivate
{
	int border;
	int offset;
	
	guint id_check;
	
	CCMWindow* window;
	
	cairo_surface_t* shadow_right;
	cairo_surface_t* shadow_bottom;
	
	CCMRegion* geometry;
	
	CCMConfig* options[CCM_SHADOW_OPTION_N];
};

#define CCM_SHADOW_GET_PRIVATE(o)  \
   ((CCMShadowPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SHADOW, CCMShadowClass))

static void
ccm_shadow_init (CCMShadow *self)
{
	gint cpt;
	
	self->priv = CCM_SHADOW_GET_PRIVATE(self);
	
	self->priv->id_check = 0;
	self->priv->window = NULL;
	self->priv->geometry = NULL;
	self->priv->shadow_right = NULL;
	self->priv->shadow_bottom = NULL;
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
	if (self->priv->shadow_right) 
	{
		cairo_surface_destroy(self->priv->shadow_right);
		self->priv->shadow_right = NULL;
	}
	if (self->priv->shadow_bottom) 
	{
		cairo_surface_destroy(self->priv->shadow_bottom);
		self->priv->shadow_bottom = NULL;
	}
	if (self->priv->geometry) 
	{
		ccm_region_destroy (self->priv->geometry);
		self->priv->geometry = NULL;
	}
	
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
create_shadow(CCMShadow* self,CCMWindow* window, int width, int height, 
			  gboolean shaped)
{
	cairo_t* cr;
	cairo_pattern_t *shadow;
	int border, offset;
	
	if (self->priv->shadow_right)
		cairo_surface_destroy(self->priv->shadow_right);
	if (self->priv->shadow_bottom)
		cairo_surface_destroy(self->priv->shadow_bottom);
	
	border = ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	offset = ccm_config_get_integer(self->priv->options[CCM_SHADOW_OFFSET]);
	
	self->priv->shadow_right = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 
														   border * 2, height - offset + border);
	self->priv->shadow_bottom = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 
															width - offset, border);
	
    cr = cairo_create(self->priv->shadow_right);
	
	/* Corner top right */
    shadow = cairo_pattern_create_radial(border, border / 2.0, 0, 
										 border, border / 2.0, border / 2.0);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.05);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.17);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0.5);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, border, 0, border / 2.0, border / 2.0);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);
	
	/* Right side */
    shadow = cairo_pattern_create_linear (border * 2, 0, border, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.05);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.17);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0.5);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, border, border / 2.0, border, height - offset - 3 * border / 2);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);

	/* Corner bottom right */
	if (shaped)
	{
		cairo_set_source_rgba (cr, 0.0f, 0.0f, 0.0f, 0.5f);
		cairo_rectangle (cr, 0, height - offset - border * 2, border, border);
		cairo_fill(cr);
	}
    shadow = cairo_pattern_create_radial(border, height - offset - border, 0, 
										 border, height - offset - border, border / 2.0);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.05);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.17);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0.5);
    cairo_set_source (cr, shadow);
	cairo_rectangle (cr, border, height - offset - border, border, border);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);
    cairo_destroy (cr);
	
	cr = cairo_create(self->priv->shadow_bottom);
		
	/* Corner bottom left */
    shadow = cairo_pattern_create_radial(border / 2.0, 0, 0, 
										 border / 2.0, 0, border / 2.0);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.05);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.17);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0.5);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, 0, 0, border / 2.0, border / 2.0);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);
	
	/* Bottom */
    shadow = cairo_pattern_create_linear (0, border, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.05);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.17);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0.5);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, border / 2.0, 0, width - offset - 3 * border / 2.0, border);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);
	cairo_destroy(cr);
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
			 type != CCM_WINDOW_TYPE_DIALOG)) && 
		   (type != CCM_WINDOW_TYPE_DOCK || opaque) &&
		   ((type != CCM_WINDOW_TYPE_DOCK && opaque) || 
			(!ccm_window_skip_taskbar (window) &&   
			 !ccm_window_skip_pager (window))) &&   
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
	self->priv->offset = ccm_config_get_integer(self->priv->options[CCM_SHADOW_OFFSET]);
}

static CCMRegion*
ccm_shadow_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMRegion* geometry = NULL;
	cairo_rectangle_t area;
	CCMShadow* self = CCM_SHADOW(plugin);
		
	if (self->priv->geometry) ccm_region_destroy (self->priv->geometry);
	self->priv->geometry = NULL;
	if (self->priv->shadow_right) cairo_surface_destroy(self->priv->shadow_right);
	self->priv->shadow_right = NULL;
	if (self->priv->shadow_bottom) cairo_surface_destroy(self->priv->shadow_bottom);
	self->priv->shadow_bottom = NULL;
	
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	if (geometry && ccm_shadow_need_shadow(window))
	{
		int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	
		self->priv->geometry = ccm_region_copy (geometry);
		ccm_region_get_clipbox(geometry, &area);
		area.width += border;
		area.height += border;
		create_shadow(self, window, area.width, area.height, 
					  ccm_region_shaped(geometry));
		ccm_region_resize(geometry, area.width, area.height);
	}
	
	return geometry;
}

static void
ccm_shadow_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	gboolean need =ccm_shadow_need_shadow(window);
	
	if ((need && !self->priv->shadow_right && !self->priv->shadow_bottom) || 
		(!need && self->priv->shadow_right && self->priv->shadow_bottom))
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
	
	if ((need && !self->priv->shadow_right && !self->priv->shadow_bottom) || 
		(!need && self->priv->shadow_right && self->priv->shadow_bottom))
	{
		if (!self->priv->id_check) 
			g_idle_add ((GSourceFunc)ccm_shadow_check_needed, self);
	}
	
	if (need && self->priv->shadow_right && self->priv->shadow_bottom)
	{
		cairo_rectangle_t area;
		
		if (self->priv->geometry && 
			ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(window), &area))
		{
			cairo_rectangle_t* rects;
			gint cpt, nb_rects;
			
			cairo_save(context);
			
			if (ccm_window_transform (window, context, FALSE))
			{
				cairo_save(context);
				
				cairo_rectangle (context, area.width - self->priv->border * 2, 
								 self->priv->offset, self->priv->border * 2, 
								 area.height - self->priv->offset + self->priv->border);
				cairo_clip(context);
				cairo_set_source_surface(context, self->priv->shadow_right, 
										 area.width - self->priv->border * 2, 
										 self->priv->offset);
				cairo_paint_with_alpha(context,
									   ccm_window_get_opacity(window));
				cairo_restore (context);
				
				cairo_save(context);
				cairo_rectangle (context, self->priv->offset, 
								 area.height - self->priv->border,
								 area.width - self->priv->offset, 
								 self->priv->border);
				cairo_clip(context);
				cairo_set_source_surface(context, self->priv->shadow_bottom, 
										 self->priv->offset, 
										 area.height - self->priv->border);
				cairo_paint_with_alpha(context,
									   ccm_window_get_opacity(window));
				cairo_restore(context);
			}
			cairo_restore(context);
			ccm_region_get_rectangles (self->priv->geometry, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
				cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
								 rects[cpt].width, rects[cpt].height);
			cairo_clip(context);
			g_free(rects);
		}
	} 
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin),
								   window, context, surface, y_invert);
}

static void 
ccm_shadow_move(CCMWindowPlugin* plugin, CCMWindow* window, 
				int x, int y)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	cairo_rectangle_t area;
	
	if (self->priv->shadow_right && self->priv->shadow_bottom)
	{
		if (self->priv->geometry)
		{
			ccm_region_get_clipbox(self->priv->geometry, &area);
			ccm_region_offset(self->priv->geometry, x - area.x, y - area.y);
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
	
	if (self->priv->shadow_right && self->priv->shadow_bottom)
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
}

