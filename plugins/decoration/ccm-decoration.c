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

#include "ccm-debug.h"
#include "ccm-cairo-utils.h"
#include "ccm-drawable.h"
#include "ccm-config.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-decoration.h"
#include "ccm-keybind.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-check-button.h"
#include "ccm-config-adjustment.h"
#include "ccm.h"

enum
{
	CCM_DECORATION_OPACITY,
	CCM_DECORATION_GRADIANT,
	CCM_DECORATION_OPTION_N
};

static gchar* CCMDecorationOptions[CCM_DECORATION_OPTION_N] = {
	"opacity",
	"gradiant"
};

static void ccm_decoration_window_iface_init  (CCMWindowPluginClass* iface);
static void ccm_decoration_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface);

static void ccm_decoration_create_mask		  (CCMDecoration* self);
static void ccm_decoration_on_property_changed(CCMDecoration* self, 
											   CCMPropertyType changed,
											   CCMWindow* window);
static void ccm_decoration_on_opacity_changed (CCMDecoration* self, 
											   CCMWindow* window);

CCM_DEFINE_PLUGIN (CCMDecoration, ccm_decoration, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_decoration,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_decoration_window_iface_init);
                   CCM_IMPLEMENT_INTERFACE(ccm_decoration,
                                           CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                           ccm_decoration_preferences_page_iface_init))

struct _CCMDecorationPrivate
{	
	CCMWindow	    *window;
	
	int				top;
	int				bottom;
	int				left;
	int				right;
	
	float			opacity;
	gboolean		gradiant;
	
	CCMRegion*		geometry;
	CCMRegion*		opaque;
	
	gboolean		locked;

	GtkBuilder*		builder;
	
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
	self->priv->gradiant = TRUE;
	self->priv->opacity = 1.0;
	self->priv->geometry = NULL;
	self->priv->opaque = NULL;
	self->priv->locked = FALSE;
	self->priv->builder = NULL;
	for (cpt = 0; cpt < CCM_DECORATION_OPTION_N; ++cpt) 
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
	self->priv->window = NULL;
	
	if (self->priv->opaque)
		ccm_region_destroy(self->priv->opaque);
	self->priv->opaque = NULL;

	if (self->priv->geometry)
		ccm_region_destroy(self->priv->geometry);
	self->priv->geometry = NULL;

	if (self->priv->builder) g_object_unref(self->priv->builder);

	for (cpt = 0; cpt < CCM_DECORATION_OPTION_N; ++cpt)
	{
		if (self->priv->options[cpt]) 
			g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = NULL;
	}
	
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
	gboolean ret = FALSE;
	gboolean gradiant = 
		ccm_config_get_boolean (self->priv->options[CCM_DECORATION_GRADIANT], 
								&error);

	ret = self->priv->gradiant != gradiant;
	self->priv->gradiant = gradiant;
	
	real_opacity = 
		ccm_config_get_float (self->priv->options[CCM_DECORATION_OPACITY], 
							  &error);
	if (error)
	{
		g_warning("Error on get opacity configuration value");
		g_error_free(error);
		real_opacity = 0.8f;
	}
	opacity = MAX(0.0f, real_opacity);
	opacity = MIN(1.0f, opacity);
	if (self->priv->opacity != opacity)
	{
		self->priv->opacity = opacity;
		if (opacity != real_opacity)
			ccm_config_set_float (self->priv->options[CCM_DECORATION_OPACITY], 
								  opacity, NULL);
		ret = TRUE;
	}
	
	return ret;
}

static void
ccm_decoration_on_option_changed(CCMDecoration* self, CCMConfig* config)
{
	if (ccm_decoration_get_opacity (self) && 
		self->priv->window && self->priv->geometry)
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
		 changed == CCM_PROPERTY_FRAME_EXTENDS) &&
	    !self->priv->locked)
	{
		ccm_decoration_create_mask(self);
	}
}

static void
ccm_decoration_on_opacity_changed(CCMDecoration* self, CCMWindow* window)
{
	if (self->priv->geometry &&
	    !self->priv->locked)
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
	
	for (cpt = 0; cpt < CCM_DECORATION_OPTION_N; ++cpt)
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

	ccm_debug("CREATE MASK");
	
	if (self->priv->opaque)
		ccm_region_destroy(self->priv->opaque);
	self->priv->opaque = NULL;
	
	g_object_set(self->priv->window, "mask", mask, 
	             "mask_width", 0, "mask_height", 0, NULL);
		
	ccm_window_get_frame_extends(self->priv->window, &self->priv->left,
								 &self->priv->right, &self->priv->top,
								 &self->priv->bottom);
	
	if (self->priv->left || self->priv->right || 
		self->priv->top || self->priv->bottom)
	{
		cairo_t* ctx;
		cairo_pattern_t* pattern = NULL;
		cairo_rectangle_t clipbox, *rects;
		gint cpt, nb_rects;
		gfloat opacity = ccm_window_get_opacity(self->priv->window);
		CCMRegion* decoration, *tmp;
		cairo_surface_t* surface = 
			ccm_drawable_get_surface(CCM_DRAWABLE(self->priv->window));
		gboolean y_invert;

		g_object_get(self->priv->window, "mask_y_invert", &y_invert, NULL);

		ccm_region_get_clipbox(self->priv->geometry, &clipbox);
		mask = cairo_surface_create_similar(surface, CAIRO_CONTENT_ALPHA,
										    clipbox.width, clipbox.height);
		cairo_surface_destroy(surface);
		g_object_set(self->priv->window, 
					 "mask", mask,
					 "mask_width", (int)clipbox.width, 
					 "mask_height", (int)clipbox.height, NULL);
		
		ctx = cairo_create(mask);
		if (y_invert) 
		{
			cairo_scale(ctx, 1, -1);
			cairo_translate (ctx, 0.0f, -clipbox.height);
		}
		cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_rgba(ctx, 0, 0, 0, 0);
		cairo_paint(ctx);
		
		if (self->priv->gradiant)
		{
			pattern = cairo_pattern_create_linear(clipbox.x + clipbox.width / 2, 
												  clipbox.y, 
												  clipbox.x + clipbox.width / 2,
												  clipbox.height);
			cairo_pattern_add_color_stop_rgba(pattern, 0, 1, 1, 1, 
											  self->priv->opacity * opacity);
			cairo_pattern_add_color_stop_rgba(pattern, 
											  (double)self->priv->top /
											  (double)clipbox.height, 
											  1, 1, 1, 
											  opacity);
			cairo_pattern_add_color_stop_rgba(pattern, 1, 1, 1, 1, 
											  opacity);
		}
		cairo_translate(ctx, -clipbox.x, -clipbox.y);
		
		clipbox.x += self->priv->left;
		clipbox.y += self->priv->top;
		clipbox.width -= self->priv->left + self->priv->right;
		clipbox.height -= self->priv->top + self->priv->bottom;

		if (clipbox.width > 0 && clipbox.height > 0)
			self->priv->opaque = ccm_region_rectangle(&clipbox);
				
		decoration = ccm_region_copy(self->priv->geometry);

		if (pattern)
			cairo_set_source(ctx, pattern);
		else
			cairo_set_source_rgba(ctx, 1, 1, 1, self->priv->opacity * opacity);
		
		tmp = ccm_region_rectangle(&clipbox);
		ccm_region_subtract(decoration, tmp);
		ccm_region_destroy(tmp);
		
		ccm_region_get_rectangles(decoration, &rects, &nb_rects);
		
		for (cpt = 0; cpt < nb_rects; ++cpt)
			cairo_rectangle(ctx, rects[cpt].x, rects[cpt].y, 
							rects[cpt].width, rects[cpt].height);
		g_free(rects);
		cairo_fill(ctx);
		if (pattern) cairo_pattern_destroy(pattern);
		ccm_region_destroy(decoration);

		if (clipbox.width > 0 && clipbox.height > 0)
		{
			cairo_set_source_rgba(ctx, 1, 1, 1, opacity);
			cairo_rectangle(ctx, clipbox.x, clipbox.y, 
							clipbox.width, clipbox.height);
			cairo_fill(ctx);
		}
		
		cairo_destroy(ctx);
	}
}

static CCMRegion*
ccm_decoration_window_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	CCMRegion* geometry = NULL;
	
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	if (geometry && !ccm_region_empty (geometry)) 
	{
		if (self->priv->geometry)
		{
			cairo_rectangle_t old, new;

			ccm_region_get_clipbox (geometry, &new);
			ccm_region_get_clipbox (self->priv->geometry, &old);
			if (new.width != old.width || new.height != old.height)
			{
				ccm_region_destroy(self->priv->geometry);
				self->priv->geometry = NULL;
			}
		}
		
		if (!self->priv->geometry)
		{
			self->priv->geometry = ccm_region_copy(geometry);
			ccm_decoration_create_mask(self);
		}
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

	if (self->priv->geometry)
	{
		CCMRegion* decoration = ccm_region_copy(self->priv->geometry);
		CCMRegion* damaged = NULL;

		if (self->priv->opaque)
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
ccm_decoration_on_map_unmap_unlocked(CCMDecoration* self)
{
	ccm_debug("UNLOCK");

	self->priv->locked = FALSE;
	ccm_decoration_create_mask(self);	
}

static void
ccm_decoration_window_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMDecoration* self = CCM_DECORATION(plugin);

	ccm_debug("MAP");
	CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD(plugin, map, 
	                                   (CCMPluginUnlockFunc)ccm_decoration_on_map_unmap_unlocked, 
	                                   self);
	self->priv->locked = TRUE;

	ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(plugin), window);

	CCM_WINDOW_PLUGIN_UNLOCK_ROOT_METHOD(plugin, map);
	ccm_window_plugin_map((CCMWindowPlugin*)window, window);
}

static void
ccm_decoration_window_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMDecoration* self = CCM_DECORATION(plugin);

	ccm_debug("UNMAP");
	CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD(plugin, unmap, 
	                                   (CCMPluginUnlockFunc)ccm_decoration_on_map_unmap_unlocked, 
	                                   self);
	self->priv->locked = TRUE;

	ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(plugin), window);

	CCM_WINDOW_PLUGIN_UNLOCK_ROOT_METHOD(plugin, unmap);
	ccm_window_plugin_unmap((CCMWindowPlugin*)window, window);
}

static void 
ccm_decoration_window_set_opaque_region(CCMWindowPlugin* plugin, 
										CCMWindow* window,
										const CCMRegion* area)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
       
    if (self->priv->geometry) 
	{
		CCMRegion* opaque = NULL;
		
		if (self->priv->opaque && area)
		{ 
			opaque = ccm_region_copy(self->priv->opaque);   
			ccm_region_intersect (opaque, (CCMRegion*)area);
		}
	
		ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT(plugin), 
											 window, opaque);
		if (opaque) ccm_region_destroy (opaque);
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
		if (x != clipbox.x || y != clipbox.y)
		{
			ccm_region_offset(self->priv->geometry, x - clipbox.x, 
							  y - clipbox.y);
			if (self->priv->opaque)
			{
				ccm_region_get_clipbox(self->priv->opaque, &clipbox);
				ccm_region_offset(self->priv->opaque, 
								  (x - clipbox.x) + self->priv->left, 
								  (y - clipbox.y) + self->priv->top);
			}
		}
		else
			return;
	}
	
	ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT(plugin), window, x, y);
}

static void 
ccm_decoration_window_resize(CCMWindowPlugin* plugin, CCMWindow* window, 
							 int width, int height)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	
	if (self->priv->geometry) 
	{
		cairo_rectangle_t clipbox;
		
		ccm_region_get_clipbox(self->priv->geometry, &clipbox);
		if (width != clipbox.width || height != clipbox.height)
		{
			ccm_region_resize(self->priv->geometry, width, height);
		
			if (self->priv->opaque) 
				ccm_region_scale(self->priv->opaque, 
								 (gdouble)width / clipbox.width, 
								 (gdouble)height /clipbox.height);
		
			ccm_decoration_create_mask(self);
		}
		else
			return;
	}
	
	ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT(plugin), window,
							  width, height);
}

static void
ccm_decoration_preferences_page_init_windows_section(CCMPreferencesPagePlugin* plugin,
                                                     CCMPreferencesPage* preferences,
                                                     GtkWidget* windows_section)
{
	CCMDecoration* self = CCM_DECORATION(plugin);
	
	self->priv->builder = gtk_builder_new();

	if (gtk_builder_add_from_file(self->priv->builder, 
	                              UI_DIR "/ccm-decoration.ui", NULL))
	{
		GtkWidget* widget = 
			GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "decoration"));
		if (widget)
		{
			gint screen_num = ccm_preferences_page_get_screen_num (preferences);
			
			gtk_box_pack_start(GTK_BOX(windows_section), widget, 
			                   FALSE, TRUE, 0);

			CCMConfigCheckButton* gradiant = 
				CCM_CONFIG_CHECK_BUTTON(gtk_builder_get_object(self->priv->builder, 
				                                               "gradiant"));
			g_object_set(gradiant, "screen", screen_num, NULL);

			CCMConfigAdjustment* opacity = 
				CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
				                                             "alpha-adjustment"));
			g_object_set(opacity, "screen", screen_num, NULL);
			
			ccm_preferences_page_section_register_widget (preferences,
			                                              CCM_PREFERENCES_PAGE_SECTION_WINDOW,
			                                              widget, "decoration");
		}
	}
	ccm_preferences_page_plugin_init_windows_section (CCM_PREFERENCES_PAGE_PLUGIN_PARENT(plugin),
													  preferences, windows_section);
}

static void
ccm_decoration_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_decoration_window_load_options;
	iface->query_geometry 	 = ccm_decoration_window_query_geometry;
	iface->paint 			 = ccm_decoration_window_paint;
	iface->map				 = ccm_decoration_window_map;
	iface->unmap			 = ccm_decoration_window_unmap;
	iface->query_opacity	 = NULL;
	iface->move				 = ccm_decoration_window_move;
	iface->resize			 = ccm_decoration_window_resize;
	iface->set_opaque_region = ccm_decoration_window_set_opaque_region;
	iface->get_origin		 = NULL;
}

static void
ccm_decoration_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface)
{
	iface->init_general_section       = NULL;
    iface->init_desktop_section       = NULL;
    iface->init_windows_section       = ccm_decoration_preferences_page_init_windows_section;
    iface->init_effects_section		  = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section     = NULL;
}
