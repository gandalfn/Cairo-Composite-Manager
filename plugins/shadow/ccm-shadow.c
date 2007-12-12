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
	cairo_surface_t* shadow;
	
	CCMRegion* geometry;
	CCMRegion* shadow_region;
	
	CCMConfig* options[CCM_SHADOW_OPTION_N];
};

#define CCM_SHADOW_GET_PRIVATE(o)  \
   ((CCMShadowPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SHADOW, CCMShadowClass))

void ccm_shadow_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
CCMRegion* ccm_shadow_query_geometry(CCMWindowPlugin* plugin, 
									 CCMWindow* window);
gboolean ccm_shadow_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
						  cairo_t* context, cairo_surface_t* suface);
void ccm_shadow_unmap(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_shadow_set_opaque(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_shadow_move(CCMWindowPlugin* plugin, CCMWindow* window, 
					 int x, int y);
void ccm_shadow_resize(CCMWindowPlugin* plugin, CCMWindow* window, 
					   int width, int height);

static void
ccm_shadow_init (CCMShadow *self)
{
	self->priv = CCM_SHADOW_GET_PRIVATE(self);
	
	self->priv->geometry = NULL;
	self->priv->shadow = NULL;
	self->priv->shadow_region = NULL;
}

static void
ccm_shadow_finalize (GObject *object)
{
	CCMShadow* self = CCM_SHADOW(object);
	
	if (self->priv->shadow) cairo_surface_destroy(self->priv->shadow);
	if (self->priv->geometry) ccm_region_destroy (self->priv->geometry);
	if (self->priv->shadow_region) ccm_region_destroy (self->priv->shadow_region);
	
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
ccm_shadow_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_shadow_load_options;
	iface->query_geometry 	= ccm_shadow_query_geometry;
	iface->paint 			= ccm_shadow_paint;
	iface->map				= NULL;
	iface->unmap			= ccm_shadow_unmap;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= ccm_shadow_set_opaque;
	iface->move				= ccm_shadow_move;
	iface->resize			= ccm_shadow_resize;
}

static void
create_shadow(CCMShadow* self,int width, int height)
{
	cairo_t* cr;
	cairo_pattern_t *shadow;
	int border, offset;
	
	if (self->priv->shadow)
		cairo_surface_destroy(self->priv->shadow);
	
	border = ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	offset = ccm_config_get_integer(self->priv->options[CCM_SHADOW_OFFSET]);
	
	self->priv->shadow = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
													width, height);
	
    cr = cairo_create(self->priv->shadow);
	
	/* Clear background */
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0f, 0.0f, 0.0f, 0.0f);
	cairo_paint(cr);
	
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	
    /* Right side */
    shadow = cairo_pattern_create_linear (width, 0, width - border, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.0375);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.125);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0.37);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, width - border, offset, border, height - border - offset);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);

	/* Corner bottom right */
    shadow = cairo_pattern_create_radial(width - border, height - border, 0, 
										 width - border, height - border, border);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0 / 2.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75 / 2.0, 0, 0, 0, 0.0375);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5 / 2.0, 0, 0, 0, 0.125);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0.37);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, width - border, height - border, width, height);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);
    
	/* Bottom */
    shadow = cairo_pattern_create_linear (0, height, 0, height - border);
    cairo_pattern_add_color_stop_rgba (shadow, 0.0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba (shadow, 0.5, 0, 0, 0, 0.0375);
    cairo_pattern_add_color_stop_rgba (shadow, 0.75, 0, 0, 0, 0.125);
    cairo_pattern_add_color_stop_rgba (shadow, 1.0, 0, 0, 0, 0.37);
    cairo_set_source (cr, shadow);
    cairo_rectangle (cr, offset, height - border, width - border - offset, border);
    cairo_fill (cr);
    cairo_pattern_destroy (shadow);
	cairo_destroy(cr);
}

void
ccm_shadow_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_SHADOW_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "shadow", 
												  CCMShadowOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

CCMRegion*
ccm_shadow_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMRegion* geometry = NULL;
	cairo_rectangle_t area;
	CCMWindowType type = ccm_window_get_hint_type(window);
	CCMShadow* self = CCM_SHADOW(plugin);
		
	if (self->priv->geometry) ccm_region_destroy (self->priv->geometry);
	self->priv->geometry = NULL;
	if (self->priv->shadow_region) ccm_region_destroy (self->priv->shadow_region);
	self->priv->shadow_region = NULL;
	if (self->priv->shadow)	cairo_surface_destroy(self->priv->shadow);
	self->priv->shadow = NULL;
	
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	if (geometry && 
		(ccm_window_is_decorated (window) ||type != CCM_WINDOW_TYPE_NORMAL) &&
		type != CCM_WINDOW_TYPE_DESKTOP && 
		(type != CCM_WINDOW_TYPE_DOCK || window->opaque) &&
		type != CCM_WINDOW_TYPE_DND &&
		!ccm_window_is_shaded (window) &&
		(ccm_window_is_managed(window) || 
		 type == CCM_WINDOW_TYPE_DROPDOWN_MENU || 
		 type == CCM_WINDOW_TYPE_POPUP_MENU || 
		 type == CCM_WINDOW_TYPE_TOOLTIP || 
		 type == CCM_WINDOW_TYPE_MENU))
	{
		int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	
		self->priv->geometry = ccm_region_copy (geometry);
		ccm_region_get_clipbox(geometry, &area);
		area.width += border;
		area.height += border;
		create_shadow(self, area.width, area.height);
		ccm_region_resize(geometry, area.width, area.height);
		self->priv->shadow_region = ccm_region_copy(geometry);
		ccm_region_subtract (self->priv->shadow_region, self->priv->geometry);
	}	
	return geometry;
}

gboolean
ccm_shadow_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
				 cairo_t* context, cairo_surface_t* surface)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	CCMWindowType type = ccm_window_get_hint_type(window);
	
	if (self->priv->shadow && 
		(ccm_window_is_decorated (window) ||
		 type != CCM_WINDOW_TYPE_NORMAL) &&
		type != CCM_WINDOW_TYPE_DESKTOP && 
		(type != CCM_WINDOW_TYPE_DOCK || window->opaque) &&
		type != CCM_WINDOW_TYPE_DND &&
		!ccm_window_is_shaded (window) &&
		(ccm_window_is_managed(window) || 
		 type == CCM_WINDOW_TYPE_DROPDOWN_MENU || 
		 type == CCM_WINDOW_TYPE_POPUP_MENU || 
		 type == CCM_WINDOW_TYPE_TOOLTIP || 
		 type == CCM_WINDOW_TYPE_MENU))
	{
		cairo_rectangle_t area;
		
		if (self->priv->geometry && self->priv->shadow_region &&
			ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(window), &area))
		{
			cairo_rectangle_t* rects;
			gint nb_rects, cpt;
			
			cairo_save(context);
			ccm_region_get_rectangles (self->priv->shadow_region, 
									   &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
			{
				cairo_rectangle(context, rects[cpt].x, rects[cpt].y, 
								rects[cpt].width, rects[cpt].height);
			}
			g_free(rects);
			cairo_clip(context);
			cairo_set_source_surface(context, self->priv->shadow, 
							     area.x, area.y);
			cairo_paint_with_alpha(context,
								   ccm_window_get_opacity(window));
			cairo_restore(context);
			
			ccm_region_get_rectangles (self->priv->geometry, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
			{
				cairo_rectangle(context, rects[cpt].x, rects[cpt].y, 
								rects[cpt].width, rects[cpt].height);
			}
			g_free(rects);
			cairo_clip(context);
		}
	} 
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin),
								   window, context, surface);
}

void
ccm_shadow_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	
	if (self->priv->shadow)
	{
		cairo_surface_destroy (self->priv->shadow);
		self->priv->shadow = NULL;
	}
	
	ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

void 
ccm_shadow_set_opaque(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	
	ccm_window_plugin_set_opaque (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	if (self->priv->shadow_region)
		ccm_window_add_alpha_region (window, self->priv->shadow_region);
}

void ccm_shadow_move(CCMWindowPlugin* plugin, CCMWindow* window, 
					 int x, int y)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	cairo_rectangle_t area;
	
	if (self->priv->geometry)
	{
		ccm_region_get_clipbox(self->priv->geometry, &area);
		ccm_region_offset(self->priv->geometry, x - area.x, y - area.y);
	}
	if (self->priv->shadow_region)
	{
		ccm_region_get_clipbox(self->priv->shadow_region, &area);
		ccm_region_offset(self->priv->shadow_region, x - area.x, y - area.y);
	}
	
	ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT(plugin), window, x, y);
}

void ccm_shadow_resize(CCMWindowPlugin* plugin, CCMWindow* window, 
					   int width, int height)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	int border = 0;
	
	if (self->priv->shadow)
		border = ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	
	ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT(plugin), window,
							  width + border, height + border);
}
