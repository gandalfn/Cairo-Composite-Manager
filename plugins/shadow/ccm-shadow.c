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
	
	CCMConfig* options[CCM_SHADOW_OPTION_N];
};

#define CCM_SHADOW_GET_PRIVATE(o)  \
   ((CCMShadowPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SHADOW, CCMShadowClass))

void ccm_shadow_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
CCMRegion* ccm_shadow_query_geometry(CCMWindowPlugin* plugin, 
									 CCMWindow* window);
gboolean ccm_shadow_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
						  cairo_t* context, cairo_surface_t* suface);

static void
ccm_shadow_init (CCMShadow *self)
{
	self->priv = CCM_SHADOW_GET_PRIVATE(self);
	
	self->priv->shadow = NULL;
}

static void
ccm_shadow_finalize (GObject *object)
{
	CCMShadow* self = CCM_SHADOW(object);
	
	if (self->priv->shadow) cairo_surface_destroy(self->priv->shadow);
	
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
	iface->unmap			= NULL;
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
	
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	if (geometry && 
		type != CCM_WINDOW_TYPE_DESKTOP && 
		type != CCM_WINDOW_TYPE_DOCK &&
		type != CCM_WINDOW_TYPE_UTILITY &&
		type != CCM_WINDOW_TYPE_DND &&
		!ccm_window_is_shaded (window) &&
		(ccm_window_is_managed(window) || 
		 type == CCM_WINDOW_TYPE_DROPDOWN_MENU || 
		 type == CCM_WINDOW_TYPE_POPUP_MENU || 
		 type == CCM_WINDOW_TYPE_TOOLTIP || 
		 type == CCM_WINDOW_TYPE_MENU))
	{
		CCMShadow* self = CCM_SHADOW(plugin);
		CCMRegion* shadow_geometry = ccm_region_copy(geometry);
		int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
	
		ccm_region_get_clipbox(geometry, &area);
		
		area.width += border;
		area.height += border;
		create_shadow(self, area.width, area.height);
		ccm_region_resize(shadow_geometry, area.width, area.height);
		ccm_region_subtract(shadow_geometry, geometry);
		ccm_region_union(geometry, shadow_geometry);
		ccm_region_destroy(shadow_geometry);
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
		type != CCM_WINDOW_TYPE_DESKTOP && 
		type != CCM_WINDOW_TYPE_DOCK &&
		type != CCM_WINDOW_TYPE_UTILITY &&
		type != CCM_WINDOW_TYPE_DND &&
		!ccm_window_is_shaded (window) &&
		(ccm_window_is_managed(window) || 
		 type == CCM_WINDOW_TYPE_DROPDOWN_MENU || 
		 type == CCM_WINDOW_TYPE_POPUP_MENU || 
		 type == CCM_WINDOW_TYPE_TOOLTIP || 
		 type == CCM_WINDOW_TYPE_MENU))
	{
		cairo_rectangle_t geometry;
		int border = 
				ccm_config_get_integer(self->priv->options[CCM_SHADOW_BORDER]);
		
		ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(window), &geometry);
		cairo_set_source_surface(context, self->priv->shadow, 
								 geometry.x, geometry.y);
		cairo_paint_with_alpha(context,
							   ccm_window_get_opacity(window));
		
		geometry.width -= border;
		geometry.height -= border;
		cairo_rectangle(context, geometry.x, geometry.y, 
						geometry.width, geometry.height);
		cairo_clip(context);
	} 
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin),
								   window, context, surface);
}
