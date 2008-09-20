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
#include <string.h>
#include <math.h>
#include <X11/extensions/Xfixes.h>

#include "ccm-debug.h"
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
	CCM_MAGNIFIER_ZOOM_QUALITY,
	CCM_MAGNIFIER_HEIGHT,
	CCM_MAGNIFIER_WIDTH,
	CCM_MAGNIFIER_SHORTCUT,
	CCM_MAGNIFIER_SHADE_DESKTOP,
	CCM_MAGNIFIER_OPTION_N
};

static gchar* CCMMagnifierOptions[CCM_MAGNIFIER_OPTION_N] = {
	"zoom-level",
	"zoom-quality",
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
	gfloat				 scale;
	gboolean 			 shade;
	
	int					 x_mouse;
	int 				 y_mouse;
	int					 x_hot;
	int 				 y_hot;
	int					 mouse_width;
	int 				 mouse_height;
	guchar*				 image_mouse;
	cairo_surface_t*     surface_mouse;
	int					 x_offset;
	int 				 y_offset;
	cairo_surface_t*     surface;
	cairo_rectangle_t    area;
	cairo_filter_t		 quality;
	CCMRegion*			 damaged;
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
	gint cpt;
	
	self->priv = CCM_MAGNIFIER_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->enabled = FALSE;
	self->priv->shade = TRUE;
	self->priv->scale = 1.0f;
	self->priv->x_mouse = 0;
	self->priv->y_mouse = 0;
	self->priv->x_hot = 0;
	self->priv->y_hot = 0;
	self->priv->mouse_width = 0;
	self->priv->mouse_height = 0;	
	self->priv->image_mouse = NULL;
	self->priv->surface_mouse = NULL;
	self->priv->x_offset = 0;
	self->priv->y_offset = 0;
	self->priv->surface = NULL;
	self->priv->damaged = NULL;
	self->priv->keybind = NULL;
	bzero(&self->priv->area, sizeof(cairo_rectangle_t));
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_magnifier_finalize (GObject *object)
{
	CCMMagnifier* self = CCM_MAGNIFIER(object);
	gint cpt;
	
	if (self->priv->screen && self->priv->enabled)
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
		CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
		
		XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(display), 
								CCM_WINDOW_XWINDOW(root), 
								0);
		XFixesShowCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
	}
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++)
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
	if (self->priv->surface) 
		cairo_surface_destroy (self->priv->surface);
	if (self->priv->image_mouse) 
		g_free (self->priv->image_mouse);
	if (self->priv->surface_mouse) 
		cairo_surface_destroy (self->priv->surface_mouse);
	if (self->priv->damaged) 
		ccm_region_destroy (self->priv->damaged);
	if (self->priv->keybind) 
		g_object_unref(self->priv->keybind);
	
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
	
	if (self->priv->enabled)
	{
		XFixesHideCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
		XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(display), 
								CCM_WINDOW_XWINDOW(root), 
								XFixesDisplayCursorNotifyMask);
	}
	else
	{
		XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(display), 
								CCM_WINDOW_XWINDOW(root), 
								0);
		XFixesShowCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
		if (self->priv->surface) 
			cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		if (self->priv->image_mouse) 
			g_free (self->priv->image_mouse);
		self->priv->image_mouse = NULL;
		if (self->priv->surface_mouse) 
			cairo_surface_destroy (self->priv->surface_mouse);
		self->priv->surface_mouse = NULL;
	}
}

static void
ccm_magnifier_get_keybind(CCMMagnifier *self)
{
	gchar* shortcut = 
		ccm_config_get_string(self->priv->options [CCM_MAGNIFIER_SHORTCUT]);
	
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	
	self->priv->keybind = ccm_keybind_new(self->priv->screen, shortcut, TRUE);
	g_free(shortcut);
	
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_magnifier_on_key_press), self);
}

static gboolean
ccm_magnifier_get_scale(CCMMagnifier *self)
{
	gfloat scale = 
		ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
	
	scale = MAX(1.0f, scale);
	scale = MIN(5.0f, scale);
	if (self->priv->scale != scale)
	{
		self->priv->scale = scale;
		ccm_config_set_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL],
							  self->priv->scale);
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_magnifier_get_zoom_quality(CCMMagnifier *self)
{
	gboolean ret = FALSE;
	gchar* quality = 
	   ccm_config_get_string (self->priv->options [CCM_MAGNIFIER_ZOOM_QUALITY]);
	
	if (!quality)
	{
		if (self->priv->quality != CAIRO_FILTER_FAST)
		{
			self->priv->quality = CAIRO_FILTER_FAST;
			ccm_config_set_string (self->priv->options [CCM_MAGNIFIER_ZOOM_QUALITY], 
								   "fast");
			ret = TRUE;
		}
	}
	else
	{
		if (!g_ascii_strcasecmp (quality, "fast") && 
			self->priv->quality != CAIRO_FILTER_FAST)
		{
			self->priv->quality = CAIRO_FILTER_FAST;
			ret = TRUE;
		}
		else if (!g_ascii_strcasecmp (quality, "good") &&
				 self->priv->quality != CAIRO_FILTER_GOOD)
		{
			self->priv->quality = CAIRO_FILTER_GOOD;
			ret = TRUE;
		}
		else if (!g_ascii_strcasecmp (quality, "best") &&
				 self->priv->quality != CAIRO_FILTER_BEST)
		{
			self->priv->quality = CAIRO_FILTER_BEST;
			ret = TRUE;
		}
		g_free(quality);
	}
	
	return ret;
}

static gboolean
ccm_magnifier_get_shade_desktop(CCMMagnifier *self)
{
	gboolean shade =
	 ccm_config_get_boolean (self->priv->options [CCM_MAGNIFIER_SHADE_DESKTOP]);
	
	if (shade != self->priv->shade)
	{
		self->priv->shade = shade;
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_magnifier_get_size(CCMMagnifier* self)
{
	gint val;
	gdouble width, height;
	gboolean ret = FALSE;

	self->priv->area.x = 0;
	self->priv->area.y = 0;
	val = MAX(10, ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_WIDTH]));
	val = MIN(80, val);
	
	width = (gdouble)CCM_SCREEN_XSCREEN(self->priv->screen)->width * (gdouble)val / 100.0;
	if (self->priv->area.width != width)
	{
		self->priv->area.width = width;
		ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_WIDTH], val);
		ret = TRUE;
	}
	
	val = MAX(10, ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_HEIGHT]));
	val = MIN(80, val);
	height = (gdouble)CCM_SCREEN_XSCREEN(self->priv->screen)->height * (gdouble)val / 100.0;
	
	if (self->priv->area.height != height)
	{
		self->priv->area.height = height;
		ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_HEIGHT], val);
		ret = TRUE;
	}
	
	return ret;
}

static void
ccm_magnifier_cursor_convert_to_rgba (CCMMagnifier *self, 
									  XFixesCursorImage *cursor_image)
{
	int i, count = cursor_image->width * cursor_image->height;
	
	for (i = 0; i < count; ++i) 
	{
		guint32 pixval = GUINT32_TO_LE (cursor_image->pixels[i]);
		cursor_image->pixels[i] = pixval;
	}
}

static void
ccm_magnifier_cursor_get_surface(CCMMagnifier* self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	XFixesCursorImage *cursor_image;
	
	if (self->priv->image_mouse)
		g_free (self->priv->image_mouse);
	if (self->priv->surface_mouse)
		cairo_surface_destroy (self->priv->surface_mouse);
	
	cursor_image = XFixesGetCursorImage (CCM_DISPLAY_XDISPLAY(display));
	ccm_magnifier_cursor_convert_to_rgba (self, cursor_image);
	
	self->priv->image_mouse = g_memdup(cursor_image->pixels, 
									   cursor_image->width * cursor_image->height * 4);
	self->priv->surface_mouse = cairo_image_surface_create_for_data (
												self->priv->image_mouse, 
												CAIRO_FORMAT_ARGB32,
												cursor_image->width, 
												cursor_image->height,
												cursor_image->width * 4);
	self->priv->x_mouse = cursor_image->x;
	self->priv->y_mouse = cursor_image->y;
	self->priv->x_hot = cursor_image->xhot;
	self->priv->y_hot = cursor_image->yhot;
	self->priv->mouse_width = cursor_image->width;
	self->priv->mouse_height = cursor_image->height;
	
	XFree(cursor_image);
}

static void
ccm_magnifier_cursor_get_position(CCMMagnifier*self)
{
	gboolean damaged = FALSE;
	gint old_x = self->priv->x_mouse, 
	     old_y = self->priv->y_mouse,
		 old_xhot = self->priv->x_hot,
		 old_yhot = self->priv->y_hot;
	
	ccm_magnifier_cursor_get_surface (self);
	
	if (self->priv->x_offset > self->priv->x_mouse)
	{
		self->priv->x_offset = self->priv->x_mouse;
		damaged = TRUE;
	}
	if (self->priv->y_offset > self->priv->y_mouse)
	{
		self->priv->y_offset = self->priv->y_mouse;
		damaged = TRUE;
	}
	if (self->priv->x_offset + 
		(self->priv->area.width / self->priv->scale) < self->priv->x_mouse)
	{
		self->priv->x_offset = self->priv->x_mouse + 20 - 
							   (self->priv->area.width / self->priv->scale);
		if (self->priv->x_offset + 
			(self->priv->area.width / self->priv->scale) > 
			CCM_SCREEN_XSCREEN(self->priv->screen)->width)
			self->priv->x_offset = CCM_SCREEN_XSCREEN(self->priv->screen)->width - 
								   (self->priv->area.width / self->priv->scale);
		damaged = TRUE;
	}
	if (self->priv->y_offset + (self->priv->area.height / self->priv->scale) < 
		self->priv->y_mouse)
	{
		self->priv->y_offset = self->priv->y_mouse + 20 - 
							   (self->priv->area.height / self->priv->scale);
		if (self->priv->y_offset + 
			(self->priv->area.height / self->priv->scale) > 
			CCM_SCREEN_XSCREEN(self->priv->screen)->height)
			self->priv->y_offset = CCM_SCREEN_XSCREEN(self->priv->screen)->height - 
								  (self->priv->area.height / self->priv->scale);
		damaged = TRUE;
	}
	
	if (damaged)
	{
		CCMRegion* damage;
		cairo_rectangle_t area;
		
		area.x = self->priv->x_offset;
		area.y = self->priv->y_offset;
		area.width = self->priv->area.width / self->priv->scale;
		area.height = self->priv->area.height / self->priv->scale;
		damage = ccm_region_rectangle (&area);
		ccm_debug("MAGNIFIER POSITION CHANGED");
		ccm_screen_damage_region (self->priv->screen, damage);
		ccm_region_destroy (damage);
	}
	else if (old_x - old_xhot != self->priv->x_mouse - self->priv->x_hot || 
			 old_y - old_yhot != self->priv->y_mouse - self->priv->y_hot)
	{
		CCMRegion* damage;
		cairo_rectangle_t area;
		
		area.x = old_x - old_xhot;
		area.y = old_y - old_yhot;
		area.width = self->priv->mouse_width;
		area.height = self->priv->mouse_height;
		damage = ccm_region_rectangle (&area);
		ccm_screen_damage_region (self->priv->screen, damage);
		ccm_region_destroy (damage);
		area.x = self->priv->x_mouse - self->priv->x_hot;
		area.y = self->priv->y_mouse - self->priv->y_hot;
		damage = ccm_region_rectangle (&area);
		ccm_screen_damage_region (self->priv->screen, damage);
		ccm_region_destroy (damage);
	}
}

static void
ccm_magnifier_on_cursor_notify_event(CCMMagnifier* self, 
									 XFixesCursorNotifyEvent* event,
									 CCMDisplay* display)
{
	if (self->priv->enabled)
		ccm_magnifier_cursor_get_position (self);
}

static void
ccm_magnifier_on_option_changed(CCMMagnifier* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_MAGNIFIER_ZOOM_LEVEL] &&
		ccm_magnifier_get_scale (self))
	{
		if (self->priv->surface) 
			cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		ccm_magnifier_cursor_get_surface(self);
		ccm_screen_damage (self->priv->screen);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_ZOOM_QUALITY] &&
			 ccm_magnifier_get_zoom_quality (self))
	{
		ccm_magnifier_cursor_get_surface(self);
		ccm_screen_damage (self->priv->screen);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_SHORTCUT])
	{
		ccm_magnifier_get_keybind (self);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_SHADE_DESKTOP] &&
			 ccm_magnifier_get_shade_desktop (self))
	{
		ccm_screen_damage (self->priv->screen);
	}
	else if ((config == self->priv->options[CCM_MAGNIFIER_WIDTH] ||
			  config == self->priv->options[CCM_MAGNIFIER_HEIGHT]) &&
			 ccm_magnifier_get_size (self))
	{
		if (self->priv->surface) 
			cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		ccm_screen_damage (self->priv->screen);
	}
}

static void
ccm_magnifier_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMagnifier* self = CCM_MAGNIFIER(plugin);
	CCMDisplay* display = ccm_screen_get_display (screen);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "magnifier", 
												  CCMMagnifierOptions[cpt]);
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_magnifier_on_option_changed), 
								 self);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	
	ccm_magnifier_get_scale(self);
	ccm_magnifier_get_zoom_quality (self);
	ccm_magnifier_get_shade_desktop (self);
	ccm_magnifier_get_size(self);
	ccm_magnifier_get_keybind(self);
	g_signal_connect_swapped(display, "xfixes-cursor-event", 
							 G_CALLBACK(ccm_magnifier_on_cursor_notify_event), 
							 self);
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
		
		self->priv->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 
								self->priv->area.width / self->priv->scale, 
								self->priv->area.height / self->priv->scale);
		ctx = cairo_create (self->priv->surface);
		cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
		cairo_paint(ctx);
		cairo_destroy (ctx);
		ccm_screen_damage (screen);
	}

	if (self->priv->enabled) 
	{
		CCMRegion* area = ccm_region_rectangle (&self->priv->area);
		int x = (CCM_SCREEN_XSCREEN(self->priv->screen)->width - 
				 self->priv->area.width) / 2;
		int y = (CCM_SCREEN_XSCREEN(self->priv->screen)->height - 
				 self->priv->area.height) / 2;
		cairo_rectangle_t screen_size, *rects;
		CCMRegion* geometry;
		gint cpt, nb_rects;
		
		ccm_debug("MAGNIFIER PAINT SCREEN CLIP");
		cairo_save(context);
		
		screen_size.x = 0;
		screen_size.y = 0;
		screen_size.width = CCM_SCREEN_XSCREEN(self->priv->screen)->width;
		screen_size.height = CCM_SCREEN_XSCREEN(self->priv->screen)->height;
		geometry = ccm_region_rectangle (&screen_size);
		ccm_region_offset(area, x, y);
		ccm_region_subtract (geometry, area);
		
		ccm_region_get_rectangles(geometry, &rects, &nb_rects);
		cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
		for (cpt = 0; cpt < nb_rects; cpt++)
			cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
							 rects[cpt].width, rects[cpt].height);
		cairo_clip(context);
		g_free(rects);
		ccm_region_destroy (area);
		ccm_region_destroy (geometry);
		
		ccm_magnifier_cursor_get_position (self);
		
		g_object_set(G_OBJECT(self->priv->screen), "buffered_pixmap", 
					 FALSE, NULL);
		ccm_debug("MAGNIFIER PAINT SCREEN");
	}
	
	ret = ccm_screen_plugin_paint(CCM_SCREEN_PLUGIN_PARENT (plugin), screen, 
								  context);
	
	if (self->priv->enabled) 
		cairo_restore (context);
	
	if (ret && self->priv->enabled) 
	{
		CCMRegion* area = ccm_region_rectangle (&self->priv->area);
		int x = (CCM_SCREEN_XSCREEN(self->priv->screen)->width - 
				 self->priv->area.width) / 2;
		int y = (CCM_SCREEN_XSCREEN(self->priv->screen)->height - 
				 self->priv->area.height) / 2;
		
		
		ccm_debug("MAGNIFIER PAINT SCREEN CONTENT");
	
		ccm_region_offset(area, x, y);
		
		if (self->priv->shade)
		{
			CCMRegion* region = ccm_screen_get_damaged (self->priv->screen);
			
			if (region)
			{
				CCMRegion* tmp = ccm_region_copy(region);
			
				ccm_region_subtract (tmp, area);
				if (!ccm_region_empty(tmp))
				{
					gint cpt, nb_rects;
					cairo_rectangle_t* rects;
					
					ccm_debug("MAGNIFIER PAINT SCREEN SHADE");
	
					cairo_save(context);
					ccm_region_get_rectangles(tmp, &rects, &nb_rects);
					cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
					for (cpt = 0; cpt < nb_rects; cpt++)
					{
						cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
										 rects[cpt].width, rects[cpt].height);
						cairo_fill(context);
					}
					g_free(rects);
					
					cairo_set_source_rgba(context, 1.0f, 1.0f, 1.0f, 0.8f);
					cairo_set_line_width (context, 10.0f);
					cairo_rectangle_round(context, x - 5, y - 5,
										  self->priv->area.width + 10, 
										  self->priv->area.height + 10, 
										  10.0f);
					cairo_stroke(context);
					cairo_set_source_rgba(context, 0.0f, 0.0f, 0.0f, 0.9f);
					cairo_set_line_width (context, 2.0f);
					cairo_rectangle_round(context, x - 1, y - 1,
										  self->priv->area.width + 2, 
										  self->priv->area.height + 2, 
										  10.0f);
					cairo_stroke(context);
					cairo_restore(context);
				}
				ccm_region_destroy (tmp);
			}
		}
	
		if (self->priv->damaged)
		{
			cairo_rectangle_t clipbox;
			cairo_pattern_t* pattern;
			cairo_t* ctx = cairo_create(self->priv->surface);
			
			ccm_debug("MAGNIFIER PAINT SCREEN FILL PAINT MOUSE");
			cairo_set_source_surface (ctx, self->priv->surface_mouse, 
									  self->priv->x_mouse - 
									  self->priv->x_offset - 
									  self->priv->x_hot, 
									  self->priv->y_mouse - 
									  self->priv->y_offset -
									  self->priv->y_hot);
			cairo_paint(ctx);
			cairo_destroy (ctx);
			
			ccm_debug("MAGNIFIER PAINT SCREEN FILL CONTENT");
	
			ccm_region_union(ccm_screen_get_damaged (screen), area);
		
			cairo_save(context);
		
			ccm_region_offset(self->priv->damaged, -self->priv->x_offset, 
							  -self->priv->y_offset);
			cairo_translate (context, x, y);
			cairo_scale(context, self->priv->scale, self->priv->scale);
			ccm_debug("MAGNIFIER PAINT SCREEN FILL TRANSLATE SCALE");
					
			ccm_region_get_clipbox (self->priv->damaged, &clipbox);
			cairo_rectangle (context, clipbox.x, clipbox.y,
							 clipbox.width, clipbox.height);
			cairo_clip(context);
				
			ccm_debug("MAGNIFIER PAINT SCREEN FILL CLIP");
	
			cairo_set_source_surface (context, self->priv->surface, 0, 0);
			pattern = cairo_get_source (context);
			cairo_pattern_set_filter (pattern, self->priv->quality);
			cairo_paint (context);
			
			ccm_debug("MAGNIFIER PAINT SCREEN FILL PAINT");
			
			cairo_restore (context);
			
			ccm_region_destroy (self->priv->damaged);
			self->priv->damaged = NULL;
		}
		ccm_debug("MAGNIFIER PAINT SCREEN END");
	
		ccm_region_destroy (area);
	}

	return ret;
}

static gboolean
ccm_magnifier_window_paint(CCMWindowPlugin* plugin, CCMWindow* window,
						   cairo_t* context, cairo_surface_t* surface,
						   gboolean y_invert)
{
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMMagnifier* self = CCM_MAGNIFIER(_ccm_screen_get_plugin (screen, 
														CCM_TYPE_MAGNIFIER));
	CCMRegion *damaged = NULL;
	
	g_object_get(G_OBJECT(window), "damaged", &damaged, NULL);
	if (damaged && self->priv->enabled) 
	{
		CCMRegion *area = ccm_region_rectangle (&self->priv->area);
	
		ccm_region_scale(area, 1 / self->priv->scale, 1 / self->priv->scale);
		ccm_region_offset (area, self->priv->x_offset, self->priv->y_offset);
		ccm_region_intersect (area, damaged);
		
		if (!ccm_region_empty (area)) 
		{
			cairo_t* ctx = cairo_create (self->priv->surface);
			cairo_path_t* damaged_path;
			cairo_matrix_t matrix, initial, translate;
			
			ccm_debug_window(window, "MAGNIFIER PAINT WINDOW");

			if (!self->priv->damaged)
				self->priv->damaged = ccm_region_copy (area);
			else
				ccm_region_union (self->priv->damaged, area);
			
			ccm_window_get_transform (window, &initial);
			ccm_window_get_transform (window, &matrix);
			
			cairo_rectangle (ctx, 0, 0,
							 self->priv->area.width / self->priv->scale, 
							 self->priv->area.height / self->priv->scale);
			cairo_clip(ctx);
			
			cairo_matrix_init_translate (&translate, 
										 -self->priv->x_offset, 
										 -self->priv->y_offset);
			cairo_matrix_multiply (&matrix, &matrix, &translate);
			ccm_window_set_transform (window, &matrix, FALSE);
			
			cairo_translate (ctx, -self->priv->x_offset, -self->priv->y_offset);
			damaged_path = ccm_drawable_get_damage_path(CCM_DRAWABLE(window), ctx);
			cairo_clip (ctx);
			cairo_path_destroy (damaged_path);
			
			ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								    ctx, surface, y_invert);
			cairo_destroy (ctx);
			ccm_window_set_transform (window, &initial, FALSE);
		}
		
		ccm_region_destroy(area);
	}
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								   context, surface, y_invert);
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
	iface->load_options 	 = NULL;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = ccm_magnifier_window_paint;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
}
