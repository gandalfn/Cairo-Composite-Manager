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
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-pixmap.h"
#include "ccm-shadow.h"
#include "ccm-config.h"
#include "ccm-cairo-utils.h"
#include "ccm-debug.h"
#include "ccm.h"

enum
{
	CCM_SHADOW_REAL_BLUR,
	CCM_SHADOW_OFFSET,
	CCM_SHADOW_RADIUS,
	CCM_SHADOW_SIGMA,
	CCM_SHADOW_COLOR,
	CCM_SHADOW_ALPHA,
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
	"real_blur",
	"offset",
	"radius",
	"sigma",
	"color",
	"alpha"
};

static void ccm_shadow_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_shadow_screen_iface_init(CCMScreenPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMShadow, ccm_shadow, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_shadow,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_shadow_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_shadow,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_shadow_window_iface_init))

struct _CCMShadowPrivate
{
	CCMScreen* 			screen;
	
	gboolean			enable;
	gboolean			real_blur;
	int 				offset;
	int 				radius;
	double 				sigma;
	GdkColor*			color;
	double				alpha;
	
	guint 				id_check;
	
	CCMWindow* 			window;
	
	CCMPixmap*			shadow;
	cairo_surface_t*	shadow_image;
	
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
	
	self->priv->screen = NULL;
	self->priv->enable = TRUE;
	self->priv->real_blur = FALSE;
	self->priv->offset = 0;
	self->priv->radius = 14;
	self->priv->sigma = 7;
	self->priv->color = NULL;
	self->priv->alpha = 0.6;
	self->priv->id_check = 0;
	self->priv->window = NULL;
	self->priv->shadow = NULL;
	self->priv->shadow_image = NULL;
	self->priv->geometry = NULL;
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
	
	if (self->priv->geometry) 
	{
		ccm_region_destroy (self->priv->geometry);
		self->priv->geometry = NULL;
	}
	
	if (CCM_IS_PIXMAP(self->priv->shadow))
		g_object_unref(self->priv->shadow);
	
	if (self->priv->shadow_image) 
		cairo_surface_destroy(self->priv->shadow_image);
	
	if (self->priv->color) g_free(self->priv->color);
	
	G_OBJECT_CLASS (ccm_shadow_parent_class)->finalize (object);
}

static void
ccm_shadow_class_init (CCMShadowClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMShadowPrivate));
	
	klass->shadow_atom = None;
	object_class->finalize = ccm_shadow_finalize;
}

static gboolean 
ccm_shadow_need_shadow(CCMShadow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	CCMWindow* window = self->priv->window;
	CCMWindowType type = ccm_window_get_hint_type(window);
	const CCMRegion* opaque = ccm_window_get_opaque_region (window);
	
	return self->priv->enable &&
		   !ccm_window_is_input_only (window) &&
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

static gboolean
ccm_shadow_check_needed(CCMShadow* self)
{
	g_return_val_if_fail(CCM_IS_SHADOW(self), FALSE);
	
	if (!ccm_shadow_need_shadow(self) && self->priv->geometry)
	{
		if (self->priv->shadow_image) 
			cairo_surface_destroy(self->priv->shadow_image);
		self->priv->shadow_image = NULL;
	
		if (self->priv->shadow) 
			g_object_unref(self->priv->shadow);
		self->priv->shadow = NULL;
		
		if (self->priv->geometry) 
			ccm_region_destroy (self->priv->geometry);
		self->priv->geometry = NULL;
		
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
		ccm_drawable_query_geometry(CCM_DRAWABLE(self->priv->window));
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
	else if (!self->priv->geometry)
	{
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
		ccm_drawable_query_geometry(CCM_DRAWABLE(self->priv->window));
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
	
	self->priv->id_check = 0;
	
	return FALSE;
}

static void
ccm_shadow_query_avoid_shadow(CCMShadow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(self->priv->window != NULL);
	
	guint32* data = NULL;
	guint n_items;
	Window child = None;
	
	g_object_get(G_OBJECT(self->priv->window), "child", &child, NULL);
	
	if (!child)
	{
		ccm_debug_window(self->priv->window, "QUERY SHADOW");
		data = ccm_window_get_property (self->priv->window, 
										CCM_SHADOW_GET_CLASS(self)->shadow_atom,
										XA_CARDINAL, 
										&n_items);
	}
	else
	{
		ccm_debug_window(self->priv->window, "QUERY CHILD SHADOW 0x%x", child);
		data = ccm_window_get_child_property (self->priv->window, 
											  CCM_SHADOW_GET_CLASS(self)->shadow_atom,
											  XA_CARDINAL, 
											  &n_items);
	}
	
	ccm_debug_window(self->priv->window, "QUERY SHADOW = 0x%x", data);
	if (data)
	{
		ccm_debug_window(self->priv->window, "_CCM_SHADOW_DISABLED %i", 
						 (gboolean)*data);
		self->priv->enable = *data == 0 ? TRUE : FALSE;
		if (!self->priv->id_check) 
			self->priv->id_check = 
				g_idle_add ((GSourceFunc)ccm_shadow_check_needed, self);
		g_free(data);
	}
}

static void
ccm_shadow_create_atoms(CCMShadow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_SHADOW_GET_CLASS(self) != NULL);
	
	CCMShadowClass* klass = CCM_SHADOW_GET_CLASS(self);
	
	if (!klass->shadow_atom)
	{
		CCMDisplay* display = 
			ccm_drawable_get_display(CCM_DRAWABLE(self->priv->window));
		
		klass->shadow_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										  "_CCM_SHADOW_DISABLED", 
										  False);
	}
}

static void
ccm_shadow_create_fake_shadow(CCMShadow* self)
{
	g_return_if_fail(self != NULL);

	cairo_surface_t* tmp;
	cairo_t* cr;
	CCMRegion* opaque = ccm_region_copy(self->priv->geometry);
	cairo_rectangle_t* rects;
	gint cpt, nb_rects;
	cairo_rectangle_t clipbox;
	cairo_path_t* path;
	gint border = self->priv->radius * 2;

	ccm_region_get_clipbox(self->priv->geometry, &clipbox);
	
	ccm_region_offset(opaque, 
					  -clipbox.x + self->priv->radius, 
					  -clipbox.y + self->priv->radius );

	// Create tmp surface for shadow
	tmp = cairo_image_surface_create(CAIRO_FORMAT_A8, 
									 clipbox.width + border, 
									 clipbox.height + border);
	cr = cairo_create(tmp);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	ccm_region_get_rectangles(opaque, &rects, &nb_rects);
	for (cpt = 0; cpt < nb_rects; cpt++)
		cairo_rectangle(cr, rects[cpt].x, rects[cpt].y,
						rects[cpt].width, rects[cpt].height);
	
	path = cairo_copy_path(cr);
	g_free(rects);
	ccm_region_destroy(opaque);
	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
	
	// Create shadow surface
	self->priv->shadow_image = cairo_blur_path(path, self->priv->radius, 1, 
											   clipbox.width + border, 
											   clipbox.height + border);
	
	cairo_path_destroy(path);
}

static void
ccm_shadow_create_blur_shadow(CCMShadow* self)
{
	g_return_if_fail(self != NULL);

	cairo_surface_t* tmp, *side;
	cairo_t* cr;
	CCMRegion* opaque = ccm_region_copy(self->priv->geometry);
	cairo_rectangle_t* rects;
	gint cpt, nb_rects;
	cairo_rectangle_t clipbox;
	gint border = self->priv->radius * 2;

	ccm_region_get_clipbox(self->priv->geometry, &clipbox);
	
	ccm_region_offset(opaque, 
					  -clipbox.x + self->priv->radius, 
					  -clipbox.y + self->priv->radius );

	// Create tmp surface for shadow
	tmp = cairo_image_surface_create(CAIRO_FORMAT_A8, 
									 clipbox.width + border, 
									 clipbox.height + border);
	cr = cairo_create(tmp);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	ccm_region_get_rectangles(opaque, &rects, &nb_rects);
	for (cpt = 0; cpt < nb_rects; cpt++)
		cairo_rectangle(cr, rects[cpt].x, rects[cpt].y,
						rects[cpt].width, rects[cpt].height);
	cairo_fill(cr);
	g_free(rects);
	ccm_region_destroy(opaque);
	cairo_destroy(cr);
	
	/* Create shadow surface */
	self->priv->shadow_image = 
			cairo_image_surface_create(CAIRO_FORMAT_A8, 
									   clipbox.width + border, 
									   clipbox.height + border);
	cr = cairo_create(self->priv->shadow_image);
	
	/* top side */
	//    0   b                                           w  w+b
	// 0 -+---+-------------------------------------------+---+---
	//      +                                               +
	// b      +<----------------------------------------->+
	//                            w - b
	side = cairo_image_surface_blur(tmp, self->priv->radius, 
									self->priv->sigma, 0, 0, 
									clipbox.width + border, border);
	cairo_save(cr);
	cairo_rectangle (cr, border, 0, clipbox.width - border, border);
	cairo_move_to(cr, border, 0);
	cairo_line_to(cr, 0, 0);
	cairo_line_to(cr, border, border);
	cairo_move_to(cr, clipbox.width, 0);
	cairo_line_to(cr, clipbox.width + border, 0);
	cairo_line_to(cr, clipbox.width, border);
	cairo_clip(cr);
	cairo_set_source_surface(cr, side, 0, 0);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_surface_destroy(side);
	
	/* right side */
	//       w  w+b
	// 0 ----+   +  
	//         + |
	// b     +   + 
	//       |   |
	//       |   | h - b
	//       |   |
	// h     +   +
	//         + | 
	// h+b       +
	side = cairo_image_surface_blur(tmp, self->priv->radius, 
									self->priv->sigma, 
									clipbox.width, 0, border, 
									clipbox.height + border);
	cairo_save(cr);
	cairo_rectangle (cr, clipbox.width, border, border, 
					 clipbox.height - border);
	cairo_move_to(cr, clipbox.width + border, border);
	cairo_line_to(cr, clipbox.width + border, 0);
	cairo_line_to(cr, clipbox.width, border);
	cairo_move_to(cr, clipbox.width + border, clipbox.height);
	cairo_line_to(cr, clipbox.width + border, 
				  clipbox.height + border);
	cairo_line_to(cr, clipbox.width, clipbox.height);
	cairo_clip(cr);
	cairo_translate (cr, clipbox.width, 0);
	cairo_set_source_surface(cr, side, 0, 0);
	cairo_paint(cr);
	cairo_surface_destroy(side);
	cairo_restore(cr);

	/* bottom side */
	//                              w - b
	// h      +   +<------------------------------------>+
	//          +                                          +
	// h+b ---+---+--------------------------------------+---+---
	//        0   b                                      w  w+b
	side = cairo_image_surface_blur(tmp, self->priv->radius, 
									self->priv->sigma, 0, clipbox.height, 
									clipbox.width + border, border);
	cairo_save(cr);
	cairo_rectangle (cr, border, clipbox.height, 
					 clipbox.width - border, border);
	cairo_move_to(cr, border, clipbox.height + border);
	cairo_line_to(cr, 0, clipbox.height + border);
	cairo_line_to(cr, border, clipbox.height);
	cairo_move_to(cr, clipbox.width, clipbox.height + border);
	cairo_line_to(cr, clipbox.width + border, 
				  clipbox.height + border);
	cairo_line_to(cr, clipbox.width, clipbox.height);
	cairo_clip(cr);
	cairo_translate (cr, 0, clipbox.height);
	cairo_set_source_surface(cr, side, 0, 0);
	cairo_paint(cr);
	cairo_surface_destroy(side);
	cairo_restore(cr);
	
	// left side
	//       0   b
	// 0 ----+     
	//       | + 
	// b     +   + 
	//       |   |
	//       |   | h-b
	//       |   |
	// h     +   +
	//       | +
	// h+b   +
	side = cairo_image_surface_blur(tmp, self->priv->radius, 
									self->priv->sigma, 0, 0, border, 
									clipbox.height + border);
	cairo_save(cr);
	cairo_rectangle (cr, 0, border, border, 
					 clipbox.height - border);
	cairo_move_to(cr, 0, border);
	cairo_line_to(cr, 0, 0);
	cairo_line_to(cr, border, border);
	cairo_move_to(cr, 0, clipbox.height);
	cairo_line_to(cr, 0, clipbox.height + border);
	cairo_line_to(cr, border, clipbox.height);
	cairo_clip(cr);
	cairo_set_source_surface(cr, side, 0, 0);
	cairo_paint(cr);
	cairo_surface_destroy(side);
	cairo_restore(cr);
	cairo_destroy(cr);
	
	cairo_surface_destroy(tmp);
}

static void
ccm_shadow_on_event(CCMShadow* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	switch (event->type)
	{
		case PropertyNotify:
		{
			XPropertyEvent* property_event = (XPropertyEvent*)event;
			CCMWindow* window;
			
			if (property_event->atom == CCM_SHADOW_GET_CLASS(self)->shadow_atom)
			{
				window = ccm_screen_find_window_or_child (self->priv->screen,
														  property_event->window);
				if (window) 
				{
					CCMShadow* plugin = 
						CCM_SHADOW(_ccm_window_get_plugin (window, 
														   CCM_TYPE_SHADOW));
					ccm_shadow_query_avoid_shadow(plugin);
				}
			}
		}
		break;
		default:
		break;
	}
}

static void
ccm_shadow_on_property_changed(CCMShadow* self, CCMPropertyType changed,
							   CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	if (!self->priv->id_check) 
		self->priv->id_check = g_idle_add ((GSourceFunc)ccm_shadow_check_needed, 
										   self);
}

static void
ccm_shadow_on_pixmap_destroyed(CCMShadow* self)
{
	g_return_if_fail (self != NULL);
	
	self->priv->shadow = NULL;
}

static void
ccm_shadow_on_pixmap_damage(CCMShadow* self, CCMRegion* area)
{
	g_return_if_fail (self != NULL);
   
	if (self->priv->shadow)
	{
		CCMPixmap* pixmap = 
			(CCMPixmap*)g_object_get_data(G_OBJECT(self->priv->shadow), 
										  "CCMShadowPixmap");
		cairo_surface_t* surface = 
			ccm_drawable_get_surface(CCM_DRAWABLE(pixmap));
		cairo_t* ctx = 
			ccm_drawable_create_context(CCM_DRAWABLE(self->priv->shadow));
		cairo_rectangle_t* rects;
		gint cpt, nb_rects;
		cairo_rectangle_t clipbox;
				
		ccm_region_get_clipbox(self->priv->geometry, &clipbox);
			
		if (!self->priv->shadow_image)
		{
			if (self->priv->real_blur)
				ccm_shadow_create_blur_shadow(self);
			else
				ccm_shadow_create_fake_shadow(self);
		}
		
		if (area)
		{
			cairo_translate(ctx, self->priv->radius, self->priv->radius);
			ccm_region_get_rectangles(area, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
				cairo_rectangle(ctx, rects[cpt].x, rects[cpt].y,
								rects[cpt].width, rects[cpt].height);
			cairo_clip(ctx);
			g_free(rects);
		}
		else
		{
			cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
			cairo_paint(ctx);
			cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_rgba(ctx, 
								  (double)self->priv->color->red / 65535.0f,
								  (double)self->priv->color->green / 65535.0f,
								  (double)self->priv->color->blue / 65535.0f,
								  self->priv->alpha);
			cairo_mask_surface(ctx, self->priv->shadow_image,
							   self->priv->offset, self->priv->offset);
			
			cairo_translate(ctx, self->priv->radius, self->priv->radius);
			cairo_translate(ctx, -clipbox.x, -clipbox.y);
			ccm_region_get_rectangles(self->priv->geometry, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
				cairo_rectangle(ctx, rects[cpt].x, rects[cpt].y,
								rects[cpt].width, rects[cpt].height);
			cairo_clip(ctx);
			g_free(rects);
			cairo_translate(ctx, clipbox.x, clipbox.y);			
		}
		gboolean freeze ;
		g_object_get(self->priv->shadow, "freeze", &freeze, NULL);
		if (!freeze)
		{
			cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_surface(ctx, surface, 0, 0);
			cairo_paint(ctx);
		}
		cairo_destroy(ctx);
		cairo_surface_destroy(surface);
		if (area)
		{
			CCMRegion* tmp = ccm_region_copy(area);
			ccm_region_offset(tmp, self->priv->radius, self->priv->radius);
			ccm_drawable_damage_region(CCM_DRAWABLE(self->priv->shadow), tmp);
			ccm_region_destroy(tmp);
		}
	}
}

static void
ccm_shadow_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	CCMDisplay* display = ccm_screen_get_display(screen);
	
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	self->priv->screen = screen;
	g_signal_connect_swapped(display, "event", 
							 G_CALLBACK(ccm_shadow_on_event), self);
}

static void
ccm_shadow_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	GError* error = NULL;
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
	ccm_shadow_create_atoms(self);
	
	self->priv->real_blur = 
		ccm_config_get_boolean(self->priv->options[CCM_SHADOW_REAL_BLUR], &error);
	if (error)
	{
		g_warning("Error on get shadow realblur configuration value");
		g_error_free(error);
		error = NULL;
		self->priv->real_blur = FALSE;
	}
	self->priv->offset = 
		ccm_config_get_integer(self->priv->options[CCM_SHADOW_OFFSET], &error);
	if (error)
	{
		g_warning("Error on get shadow offset configuration value");
		g_error_free(error);
		error = NULL;
		self->priv->offset = 0;
	}
	self->priv->radius = 
		ccm_config_get_integer(self->priv->options[CCM_SHADOW_RADIUS], &error);
	if (error)
	{
		g_warning("Error on get shadow radius configuration value");
		g_error_free(error);
		error = NULL;
		self->priv->radius = 14;
	}
	self->priv->sigma = 
		ccm_config_get_float(self->priv->options[CCM_SHADOW_SIGMA], &error);
	if (error)
	{
		g_warning("Error on get shadow radius configuration value");
		g_error_free(error);
		error = NULL;
		self->priv->sigma = 7;
	}
	self->priv->color = 
		ccm_config_get_color(self->priv->options[CCM_SHADOW_COLOR], &error);
	if (error)
	{
		g_warning("Error on get shadow color configuration value");
		g_error_free(error);
		error = NULL;
		self->priv->color = g_new0(GdkColor, 1);
	}
	self->priv->alpha = 
		ccm_config_get_float(self->priv->options[CCM_SHADOW_ALPHA], &error);
	if (error)
	{
		g_warning("Error on get shadow alpha configuration value");
		g_error_free(error);
		error = NULL;
		self->priv->alpha = 0.6;
	}
	
	g_signal_connect_swapped(window, "property-changed",
							 G_CALLBACK(ccm_shadow_on_property_changed), self);
}

static CCMRegion*
ccm_shadow_window_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMRegion* geometry = NULL;
	cairo_rectangle_t area;
	CCMShadow* self = CCM_SHADOW(plugin);
	
	if (self->priv->geometry) 
		ccm_region_destroy (self->priv->geometry);
	self->priv->geometry = NULL;
		
	geometry = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin), 
												window);
	if (geometry && ccm_shadow_need_shadow(self))
	{
		self->priv->geometry = ccm_region_copy (geometry);
		ccm_region_get_clipbox(geometry, &area);
		ccm_region_offset(geometry, -self->priv->radius, -self->priv->radius);
		ccm_region_resize(geometry, area.width + self->priv->radius * 2, 
						  area.height + self->priv->radius * 2);
	}
	return geometry;
}

static void
ccm_shadow_window_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	
	ccm_shadow_query_avoid_shadow(self);
	
	ccm_window_plugin_map(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void 
ccm_shadow_window_move(CCMWindowPlugin* plugin, CCMWindow* window, 
					   int x, int y)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	
	if (self->priv->geometry)
	{
		cairo_rectangle_t area;
		
		ccm_region_get_clipbox(self->priv->geometry, &area);
		if (x != area.x || y != area.y)
		{
			ccm_region_offset(self->priv->geometry, x - area.x, y - area.y);
			x -= self->priv->radius;
			y -= self->priv->radius;
		}
		else
			return;
	}	
	ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT(plugin), window, x, y);
}

static void 
ccm_shadow_window_resize(CCMWindowPlugin* plugin, CCMWindow* window, 
						 int width, int height)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	int border = 0;
	
	if (self->priv->geometry) 
	{
		cairo_rectangle_t area;
		
		ccm_region_get_clipbox(self->priv->geometry, &area);
		if (width != area.width || height != area.height)
		{
			ccm_region_resize(self->priv->geometry, width, height);
			border = self->priv->radius * 2;
			
			if (self->priv->shadow_image)
				cairo_surface_destroy(self->priv->shadow_image);
			self->priv->shadow_image = NULL;
			
			if (self->priv->shadow) 
				g_object_unref(self->priv->shadow);
			self->priv->shadow = NULL;
		}
		else
			return;
	}
	
	ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT(plugin), window,
							  width + border, height + border);
}


static void 
ccm_shadow_window_set_opaque_region(CCMWindowPlugin* plugin, CCMWindow* window,
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
ccm_shadow_window_get_origin(CCMWindowPlugin* plugin, CCMWindow* window, 
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

static CCMPixmap*
ccm_shadow_window_get_pixmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMShadow* self = CCM_SHADOW(plugin);
	CCMPixmap* pixmap = NULL;
	
	pixmap = ccm_window_plugin_get_pixmap (CCM_WINDOW_PLUGIN_PARENT(plugin),
										   window);

	if (pixmap && self->priv->geometry)
	{
		gint swidth, sheight;
		cairo_rectangle_t clipbox;
		
		ccm_region_get_clipbox(self->priv->geometry, &clipbox);
		swidth = clipbox.width + self->priv->radius * 2;
		sheight = clipbox.height + self->priv->radius * 2;    
	
		if (self->priv->shadow) g_object_unref(self->priv->shadow);
		self->priv->shadow = ccm_window_create_pixmap(window, swidth, 
													  sheight, 32);
		
		g_object_set_data_full(G_OBJECT(self->priv->shadow), "CCMShadowPixmap", 
							   pixmap, (GDestroyNotify)g_object_unref);
		
		g_object_set_data_full(G_OBJECT(pixmap), "CCMShadow", 
							   self, (GDestroyNotify)ccm_shadow_on_pixmap_destroyed);
		
		g_signal_connect_swapped(pixmap, "damaged",
								 G_CALLBACK(ccm_shadow_on_pixmap_damage), self);

		ccm_shadow_on_pixmap_damage(self, NULL);
		
		pixmap = self->priv->shadow;
	}
	
	return pixmap;
}

static void
ccm_shadow_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_shadow_window_load_options;
	iface->query_geometry 	 = ccm_shadow_window_query_geometry;
	iface->paint 			 = NULL;
	iface->map				 = ccm_shadow_window_map;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = ccm_shadow_window_move;
	iface->resize			 = ccm_shadow_window_resize;
	iface->set_opaque_region = ccm_shadow_window_set_opaque_region;
	iface->get_origin		 = ccm_shadow_window_get_origin;
	iface->get_pixmap		 = ccm_shadow_window_get_pixmap;
}

static void
ccm_shadow_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_shadow_screen_load_options;
	iface->paint			= NULL;
	iface->add_window		= NULL;
	iface->remove_window	= NULL;
	iface->damage			= NULL;
}

