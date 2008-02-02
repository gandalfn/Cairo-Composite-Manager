/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 				 Carlos Diógenes  2007 <cerdiogenes@gmail.com>
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

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-extension-loader.h"
#include "ccm-magnifier.h"
#include "ccm-keybind.h"
#include "ccm.h"

enum
{
	CCM_MAGNIFIER_ZOOM_LEVEL,
	CCM_MAGNIFIER_HEIGHT,
	CCM_MAGNIFIER_WIDTH,
	CCM_MAGNIFIER_SHORTCUT,
	CCM_MAGNIFIER_SHADE_DESKTOP,
	CCM_MAGNIFIER_OPTION_N
};

static gchar* CCMMagnifierOptions[CCM_MAGNIFIER_OPTION_N] = {
	"zoom-level",
	"height",
	"width",
	"shortcut",
	"shade_desktop"
};

static void ccm_magnifier_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_magnifier_window_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMMagnifier, ccm_magnifier, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_magnifier,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_magnifier_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_magnifier,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_magnifier_window_iface_init))

struct _CCMMagnifierPrivate
{	
	CCMScreen*			 screen;
	gboolean 			 enabled;
	
	int					 x_mouse;
	int 				 y_mouse;
	int					 x_offset;
	int 				 y_offset;
	cairo_surface_t*     surface;
	cairo_rectangle_t    area;
	CCMKeybind*			 keybind;
	
	CCMConfig*           options[CCM_MAGNIFIER_OPTION_N];
};

#define CCM_MAGNIFIER_GET_PRIVATE(o)  \
   ((CCMMagnifierPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MAGNIFIER, CCMMagnifierClass))

static void
cairo_rectangle_round (cairo_t *cr,
                       double x0,    double y0,
                       double width, double height,
                       double radius)
{
  double x1,y1;

  x1=x0+width;
  y1=y0+height;

  if (!width || !height)
    return;
  if (width/2<radius)
    {
      if (height/2<radius)
        {
          cairo_move_to  (cr, x0, (y0 + y1)/2);
          cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
          cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
        }
      else
        {
          cairo_move_to  (cr, x0, y0 + radius);
          cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
          cairo_line_to (cr, x1 , y1 - radius);
          cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
        }
    }
  else
    {
      if (height/2<radius)
        {
          cairo_move_to  (cr, x0, (y0 + y1)/2);
          cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
          cairo_line_to (cr, x1 - radius, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
          cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
          cairo_line_to (cr, x0 + radius, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
        }
      else
        {
          cairo_move_to  (cr, x0, y0 + radius);
          cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
          cairo_line_to (cr, x1 - radius, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
          cairo_line_to (cr, x1 , y1 - radius);
          cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
          cairo_line_to (cr, x0 + radius, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
        }
    }

  cairo_close_path (cr);
}

static void
ccm_magnifier_init (CCMMagnifier *self)
{
	self->priv = CCM_MAGNIFIER_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->enabled = FALSE;
	self->priv->x_mouse = 0;
	self->priv->y_mouse = 0;
	self->priv->x_offset = 0;
	self->priv->y_offset = 0;
	self->priv->surface = NULL;
	self->priv->keybind = NULL;
}

static void
ccm_magnifier_finalize (GObject *object)
{
	CCMMagnifier* self = CCM_MAGNIFIER(object);
	
	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	
	G_OBJECT_CLASS (ccm_magnifier_parent_class)->finalize (object);
}

static void
ccm_magnifier_class_init (CCMMagnifierClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMMagnifierPrivate));

	object_class->finalize = ccm_magnifier_finalize;
}

static void
ccm_magnifier_on_key_press(CCMMagnifier* self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	
	self->priv->enabled = ~self->priv->enabled;
	ccm_screen_damage(self->priv->screen);
	
	_ccm_screen_set_buffered(self->priv->screen, !self->priv->enabled);
	if (self->priv->enabled)
		XFixesHideCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
	else
		XFixesShowCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
}

static void
ccm_magnifier_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMagnifier* self = CCM_MAGNIFIER(plugin);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "magnifier", 
												  CCMMagnifierOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	self->priv->keybind = ccm_keybind_new(self->priv->screen, 
		ccm_config_get_string(self->priv->options [CCM_MAGNIFIER_SHORTCUT]));
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_magnifier_on_key_press), self);
}

static void
ccm_magnifier_cursor_get_position(CCMMagnifier*self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	int x, y, cursor_size;
	unsigned int m;
	Window r, w;
	gfloat scale;
	
	scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
			
	XQueryPointer (CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root),
				   &r, &w, &self->priv->x_mouse, &self->priv->y_mouse,
				   &x, &y, &m);
	
	if (self->priv->x_offset > self->priv->x_mouse)
	{
		self->priv->x_offset = self->priv->x_mouse;
		ccm_screen_damage (self->priv->screen);
	}
	if (self->priv->y_offset > self->priv->y_mouse)
	{
		self->priv->y_offset = self->priv->y_mouse;
		ccm_screen_damage (self->priv->screen);
	}
	if (self->priv->x_offset + (self->priv->area.width / scale) < self->priv->x_mouse)
	{
		self->priv->x_offset = self->priv->x_mouse + 20 - (self->priv->area.width / scale);
		if (self->priv->x_offset + (self->priv->area.width / scale) > self->priv->screen->xscreen->width)
			self->priv->x_offset = self->priv->screen->xscreen->width - (self->priv->area.width / scale);
		ccm_screen_damage (self->priv->screen);
	}
	if (self->priv->y_offset + (self->priv->area.height / scale) < self->priv->y_mouse)
	{
		self->priv->y_offset = self->priv->y_mouse + 20 - (self->priv->area.height / scale);
		if (self->priv->y_offset + (self->priv->area.height / scale) > self->priv->screen->xscreen->height)
			self->priv->y_offset = self->priv->screen->xscreen->height - (self->priv->area.height / scale);
		ccm_screen_damage (self->priv->screen);
	}
}

static void
ccm_magnifier_cursor_convert_to_rgba (CCMMagnifier *self, 
									  XFixesCursorImage *cursor_image)
{
	int i, count = cursor_image->width * cursor_image->height;
	
	for (i = 0; i < count; ++i) 
	{
		guint32 pixval = GUINT_TO_LE (cursor_image->pixels[i]);
		cursor_image->pixels[i] = pixval;
	}
}

static void
ccm_magnifier_paint_mouse(CCMMagnifier* self, cairo_t* context)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	XFixesCursorImage *cursor_image;
	cairo_surface_t* surface;
	int x, y;
	gfloat scale;
			
	scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
	cursor_image = XFixesGetCursorImage (CCM_DISPLAY_XDISPLAY(display));
	ccm_magnifier_cursor_convert_to_rgba (self, cursor_image);
	
	surface = cairo_image_surface_create_for_data ((guchar *)cursor_image->pixels, 
												   CAIRO_FORMAT_ARGB32,
												   cursor_image->width, cursor_image->height,
												   cursor_image->width * 4);
	cairo_set_source_surface (context, surface, 
							  self->priv->x_mouse - self->priv->x_offset, 
							  self->priv->y_mouse - self->priv->y_offset);
	cairo_paint(context);
	cairo_surface_destroy (surface);
	XFree(cursor_image);
}

static gboolean
ccm_magnifier_screen_paint(CCMScreenPlugin* plugin, CCMScreen* screen,
						   cairo_t* context)
{
	CCMMagnifier* self = CCM_MAGNIFIER (plugin);
	
	gboolean ret = FALSE;
	
	if (self->priv->enabled && !self->priv->surface) 
	{
		cairo_t* ctx;
		gfloat scale;
			
		scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
		
		self->priv->area.x = 0;
		self->priv->area.y = 0;
		self->priv->area.width = (double)
			ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_WIDTH]);
		self->priv->area.height = (double)
			ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_HEIGHT]);
		
		self->priv->surface = cairo_image_surface_create (
												CAIRO_FORMAT_RGB24, 
												self->priv->area.width / scale, 
												self->priv->area.height / scale);
		ctx = cairo_create (self->priv->surface);
		cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
		cairo_paint(ctx);
		cairo_destroy (ctx);
		ccm_screen_damage (screen);
	}

	if (self->priv->enabled) ccm_magnifier_cursor_get_position (self);
	
	ret = ccm_screen_plugin_paint(CCM_SCREEN_PLUGIN_PARENT (plugin), screen, 
								  context);

	if (ret && self->priv->enabled) 
	{
		gfloat scale;
		CCMRegion* area = ccm_region_rectangle(&self->priv->area);
		int x = (self->priv->screen->xscreen->width - 
				 self->priv->area.width) / 2;
		int y = (self->priv->screen->xscreen->height - 
				 self->priv->area.height) / 2;
		
		ccm_region_offset(area, x, y);
		ccm_screen_add_damaged_region(screen, area);
		ccm_region_destroy(area);
		
		if (ccm_config_get_boolean (self->priv->options [CCM_MAGNIFIER_SHADE_DESKTOP]))
		{
			gint cpt, nb_rects;
			cairo_rectangle_t* rects;
			CCMRegion* region = ccm_screen_get_damaged (self->priv->screen);
			
			cairo_save(context);
			ccm_region_get_rectangles(region, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
				cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
								 rects[cpt].width, rects[cpt].height);
			cairo_clip(context);
			
			cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
			cairo_paint(context);
			cairo_restore(context);
		}
		cairo_save(context);
		cairo_set_source_rgba(context, 1.0f, 1.0f, 1.0f, 0.8f);
		cairo_rectangle_round(context, x - 10, y - 10,
							  self->priv->area.width + 20, 
							  self->priv->area.height + 20, 
							  10.0f);
		cairo_fill(context);
		cairo_set_source_rgba(context, 0.0f, 0.0f, 0.0f, 0.9f);
		cairo_rectangle_round(context, x - 1, y - 1,
							  self->priv->area.width + 2, 
							  self->priv->area.height + 2, 
							  10.0f);
		cairo_fill(context);
		
		scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
		
		cairo_rectangle_round(context, x, y,
							  self->priv->area.width, self->priv->area.height, 
							  10.0f);
		cairo_clip(context);
		
		cairo_translate (context, x, y);
		cairo_scale(context, scale, scale);
			
		cairo_set_source_surface (context, self->priv->surface, 0, 0);
		cairo_paint (context);
		
		ccm_magnifier_paint_mouse(self, context);
		cairo_restore (context);
	}

	return ret;
}

static gboolean
ccm_magnifier_window_paint(CCMWindowPlugin* plugin, CCMWindow* window,
						   cairo_t* context, cairo_surface_t* surface)
{
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMMagnifier* self = CCM_MAGNIFIER(_ccm_screen_get_plugin (screen, 
														CCM_TYPE_MAGNIFIER));
	CCMRegion *geometry = ccm_drawable_get_geometry (CCM_DRAWABLE (window));
	
	if (geometry && self->priv->enabled) 
	{
		CCMRegion *area = ccm_region_rectangle (&self->priv->area);
	
		ccm_region_offset (area, self->priv->x_offset, self->priv->y_offset);
		ccm_region_intersect (area, geometry);
		if (!ccm_region_empty (area)) 
		{
			cairo_t* ctx = cairo_create (self->priv->surface);
			cairo_path_t* damaged;
			gfloat scale;
			
			scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
		
			cairo_rectangle (ctx, 0, 0,
							 self->priv->area.width / scale, 
							 self->priv->area.height / scale);
			cairo_clip(ctx);
			cairo_translate (ctx, -self->priv->x_offset, -self->priv->y_offset);
			damaged = ccm_drawable_get_damage_path(CCM_DRAWABLE(window), ctx);
			cairo_clip (ctx);
			cairo_path_destroy (damaged);
			
			ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								    ctx, surface);
			cairo_destroy (ctx);
		}
		
		ccm_region_destroy(area);
	}
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								  context, surface);
}

static void
ccm_magnifier_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_magnifier_screen_load_options;
	iface->paint 			= ccm_magnifier_screen_paint;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
}

static void
ccm_magnifier_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= NULL;
	iface->query_geometry 	= NULL;
	iface->paint 			= ccm_magnifier_window_paint;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
	iface->move				= NULL;
	iface->resize			= NULL;
}
