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

#include "ccm-debug.h"
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
static void ccm_mosaic_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_mosaic_on_event(CCMMosaic* self, XEvent* event, CCMDisplay* display);

CCM_DEFINE_PLUGIN (CCMMosaic, ccm_mosaic, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_mosaic_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_mosaic_window_iface_init))

typedef struct {
	cairo_rectangle_t   geometry;
	CCMWindow*			window;
} CCMMosaicArea;

struct _CCMMosaicPrivate
{	
	CCMScreen*			 screen;
	gboolean 			 enabled;
	Window				 window;
	
	int					 x_mouse;
	int 				 y_mouse;
	CCMMosaicArea*     	 areas;
	gint				 nb_areas;
	cairo_surface_t* 	 surface;
	CCMKeybind*			 keybind;
	
	CCMConfig*           options[CCM_MOSAIC_OPTION_N];
};

#define CCM_MOSAIC_GET_PRIVATE(o)  \
   ((CCMMosaicPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MOSAIC, CCMMosaicClass))

static void
ccm_mosaic_init (CCMMosaic *self)
{
	self->priv = CCM_MOSAIC_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->window = 0;
	self->priv->enabled = FALSE;
	self->priv->areas = NULL;
	self->priv->surface = NULL;
	self->priv->keybind = NULL;
}

static void
ccm_mosaic_finalize (GObject *object)
{
	CCMMosaic* self = CCM_MOSAIC(object);
	
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	if (self->priv->areas) g_free(self->priv->areas);
	if (self->priv->window)
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
		XDestroyWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
	}
	if (self->priv->screen)
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
		g_signal_handlers_disconnect_by_func(display, ccm_mosaic_on_event, self);
	}
	
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
ccm_mosaic_create_window(CCMMosaic* self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	XSetWindowAttributes attr;
	
	attr.override_redirect = True;
	self->priv->window = XCreateWindow (CCM_DISPLAY_XDISPLAY(display), 
										CCM_WINDOW_XWINDOW(root), 0, 0,
										self->priv->screen->xscreen->width,
										self->priv->screen->xscreen->height,
										0, CopyFromParent, InputOnly, 
										CopyFromParent, CWOverrideRedirect, 
										&attr);
	XMapWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
	XRaiseWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
	XSelectInput (CCM_DISPLAY_XDISPLAY(display), self->priv->window,
				  ButtonPressMask | ButtonReleaseMask);
}

static gboolean
ccm_mosaic_recalc_coords(CCMMosaic* self, int num, int* x, int* y,
						 int* x_root, int* y_root)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->priv->areas[num].window != NULL, FALSE);
	g_return_val_if_fail(x != NULL && y != NULL, FALSE);
	g_return_val_if_fail(x_root != NULL && y_root != NULL, FALSE);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self->priv->areas[num].window));
	XWindowAttributes attribs;
	gfloat scale;
	
	if (!XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
							   CCM_WINDOW_XWINDOW(self->priv->areas[num].window),
							   &attribs))
		return FALSE;
	
	scale = MIN((gfloat)self->priv->areas[num].geometry.height / (gfloat)attribs.height,
				(gfloat)self->priv->areas[num].geometry.width / (gfloat)attribs.width);
				
	*x -= (gint)((gfloat)self->priv->areas[num].geometry.x + 
		  ((gfloat)self->priv->areas[num].geometry.width / (gfloat)2) - 
		  (((gfloat)attribs.width / 2) * scale));
	*x = (gint)((gfloat)*x / scale);
					
	*y -= self->priv->areas[num].geometry.y + 
		  (self->priv->areas[num].geometry.height / 2) - 
		  (attribs.height / 2) * scale;
	*y /= scale;
	
	*x_root = *x + attribs.x;
	*y_root = *y + attribs.y;
	
	return TRUE;
}
static void
ccm_mosaic_broadcast_event(Window window, int x, int y, 
						   int x_parent, int y_parent, XEvent* evt)
{
	g_return_if_fail(window != None);
	g_return_if_fail(evt != NULL);
	
	Window* windows = NULL;
	Window w, p;
	guint n_windows = 0;
	
	XSendEvent(evt->xany.display, window, True, NoEventMask, evt);
	
	if (XQueryTree(evt->xany.display, window, &w, &p, 
				   &windows, &n_windows) && windows && n_windows)
	{
		while (n_windows--)
        {
            XWindowAttributes attribs;
            gboolean send = FALSE;
			
            XGetWindowAttributes(evt->xany.display, windows[n_windows], &attribs);
            
			send = (x >= x_parent + attribs.x && x <= x_parent + attribs.x + attribs.width &&
					y >= y_parent + attribs.y && y <= y_parent + attribs.y + attribs.height);
			
			ccm_mosaic_broadcast_event (windows[n_windows], x, y, 
										x_parent + attribs.x,
										y_parent + attribs.y, evt);
			ccm_debug("BROADCAST: 0x%lx %i, %i %i, %i %i, %i %i\n", 
					  windows[n_windows], x, y, x_parent, y_parent, 
					  attribs.x, attribs.y, send);
			
			if (send)
			{
                evt->xany.window = windows[n_windows];
				if (evt->type == ButtonPress || evt->type == ButtonRelease)
                {
                    XButtonEvent* button_evt = (XButtonEvent*)evt;
                    
                    button_evt->x = x - (x_parent + attribs.x);
                    button_evt->y = y - (y_parent + attribs.y);
                    button_evt->x_root = x;
                    button_evt->y_root = y;
                }
				if (evt->type == MotionNotify)
                {
                    XMotionEvent* motion_evt = (XMotionEvent*)evt;
                    
                    motion_evt->x = x - (x_parent + attribs.x);
                    motion_evt->y = y - (y_parent + attribs.y);
                    motion_evt->x_root = x;
                    motion_evt->y_root = y;
                }
				XSendEvent(evt->xany.display, windows[n_windows], False, 
						   NoEventMask, evt);
				break;
			}
        }
        XFree(windows);
	}
}

static void
ccm_mosaic_on_event(CCMMosaic* self, XEvent* event, CCMDisplay* display)
{
	if (self->priv->enabled)
	{
		switch (event->type)
		{
			case ButtonPress:
			case ButtonRelease:
			case MotionNotify:
			{
				XButtonEvent* button_event = (XButtonEvent*)event;
				gint cpt;
				
				for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
				{
					if (self->priv->areas[cpt].geometry.x <= button_event->x &&
						self->priv->areas[cpt].geometry.y <= button_event->y &&
						self->priv->areas[cpt].geometry.x +
						self->priv->areas[cpt].geometry.width >= button_event->x &&
						self->priv->areas[cpt].geometry.y +
						self->priv->areas[cpt].geometry.height >= button_event->y)
					{
						if (ccm_mosaic_recalc_coords(self, cpt,
													 &button_event->x, &button_event->y,
													 &button_event->x_root, &button_event->y_root))
						{
							XWindowAttributes attribs;
            
							XGetWindowAttributes(button_event->display, 
							CCM_WINDOW_XWINDOW(self->priv->areas[cpt].window), 
							&attribs);
							button_event->window = CCM_WINDOW_XWINDOW(self->priv->areas[cpt].window);
							ccm_screen_activate_window (self->priv->screen, 
														self->priv->areas[cpt].window,
														button_event->time);
						
							ccm_mosaic_broadcast_event(button_event->window, 
													   button_event->x_root, 
													   button_event->y_root,
													   attribs.x, attribs.y, 
													   event);
							break;
						}
					}
				}
			}
			break;
			default:
			break;
		}
	}
}

static void
ccm_mosaic_on_key_press(CCMMosaic* self)
{
	self->priv->enabled = ~self->priv->enabled;
	ccm_screen_set_filtered_damage (self->priv->screen, !self->priv->enabled);
	_ccm_screen_set_buffered(self->priv->screen, !self->priv->enabled);
	if (self->priv->enabled)
	{
		self->priv->surface = cairo_image_surface_create (
										CAIRO_FORMAT_ARGB32, 
										self->priv->screen->xscreen->width, 
										self->priv->screen->xscreen->height);
		ccm_mosaic_create_window(self);
	}
	else
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	
		cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		XDestroyWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
		self->priv->window = None;
		if (self->priv->areas)
		{
			g_free(self->priv->areas);
			self->priv->areas = NULL;
		}
		self->priv->nb_areas = 0;
	}
	ccm_screen_damage (self->priv->screen);
}

static void
ccm_mosaic_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMosaic* self = CCM_MOSAIC(plugin);
	CCMDisplay* display = ccm_screen_get_display (screen);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "mosaic", 
												  CCMMosaicOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	self->priv->keybind = ccm_keybind_new(self->priv->screen, 
		ccm_config_get_string(self->priv->options [CCM_MOSAIC_SHORTCUT]));
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_mosaic_on_key_press), self);
	g_signal_connect_swapped(display, "event", 
							 G_CALLBACK(ccm_mosaic_on_event), self);
}

static void
ccm_mosaic_create_areas(CCMMosaic* self, gint nb_windows)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(nb_windows != 0);
	
	int i, j, lines, n;
	int x, y, width, height;
    int spacing = ccm_config_get_integer (self->priv->options [CCM_MOSAIC_SPACING]);
    
	if (self->priv->areas) g_free(self->priv->areas);
	self->priv->areas = g_new0(CCMMosaicArea, nb_windows);
    lines = sqrt (nb_windows + 1);
    self->priv->nb_areas = 0;
    
	y = spacing;
    height = (self->priv->screen->xscreen->height - (lines + 1) * spacing) / lines;

    for (i = 0; i < lines; i++)
    {
	    n = MIN (nb_windows - self->priv->nb_areas, ceilf ((float)nb_windows / lines));
    	x = spacing;
	    width = (self->priv->screen->xscreen->width - (n + 1) * spacing) / n;
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

static CCMMosaicArea*
ccm_mosaic_find_area(CCMMosaic* self, CCMWindow* window, int width, int height)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(window != NULL, NULL);
	
	CCMMosaicArea* area = NULL;
	gfloat scale;
	gint width_scale = G_MAXINT;
	gint cpt;
	
	for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
	{
		if (self->priv->areas[cpt].window == window)
		{
			area = &self->priv->areas[cpt];
			break;
		}
	}
	
	if (!area)
	{
		for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
		{
			if (!self->priv->areas[cpt].window)
			{
				scale = MIN(self->priv->areas[cpt].geometry.height / height,
							self->priv->areas[cpt].geometry.width / width);
				if (width * scale < width_scale)
				{
					area = &self->priv->areas[cpt];
					self->priv->areas[cpt].window = window;
					width_scale = width * scale;
				}
			}
		}
	}
	
	return area;
}

static CCMRegion*
ccm_mosaic_get_damaged_area(CCMMosaic* self, CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(window != NULL, NULL);
	
	CCMRegion* damaged = NULL;
	CCMMosaicArea* area;
	XWindowAttributes* attribs = _ccm_window_get_attribs (window);
				
	area = ccm_mosaic_find_area(self, window, attribs->width, attribs->height);
	
	if (ccm_drawable_is_damaged (CCM_DRAWABLE(window)) && area)
	{
		gfloat scale;
		
		damaged = ccm_region_rectangle (&area->geometry);
		
		scale = MIN(area->geometry.height / attribs->height,
					area->geometry.width / attribs->width);
		
		ccm_region_offset(damaged, area->geometry.width / 2,
						  area->geometry.height / 2);
		ccm_region_resize (damaged, attribs->width * scale, 
						   attribs->height * scale);
		ccm_region_offset(damaged, -(attribs->width / 2) * scale,
						  -(attribs->height / 2) * scale);
	}
	
	return damaged;
}
							
static gboolean
ccm_mosaic_paint_area(CCMMosaic* self, CCMWindow* window, 
					  cairo_surface_t* surface, gboolean y_invert)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(surface != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);
	
	gfloat scale;
	cairo_path_t* path;
	cairo_t* ctx;
	CCMRegion* damaged;
	CCMMosaicArea* area;
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	XWindowAttributes* attribs = _ccm_window_get_attribs (window);
				
	area = ccm_mosaic_find_area(self, window, attribs->width, attribs->height);
	if (!area) return FALSE;
	
	damaged = ccm_mosaic_get_damaged_area (self, window);
	if (!damaged) return FALSE;
	ccm_screen_add_damaged_region (screen, damaged);
	ccm_region_destroy (damaged);
	
	scale = MIN(area->geometry.height / attribs->height,
					area->geometry.width / attribs->width);
	
	ctx = cairo_create (self->priv->surface);
	
	if (y_invert)
	{
		cairo_scale (ctx, 1.0, -1.0);
		cairo_translate (ctx, 0.0f, - area->geometry.y - area->geometry.height);
	}
	cairo_translate (ctx, area->geometry.x + area->geometry.width / 2, 
					 area->geometry.y + area->geometry.height / 2);
	cairo_scale(ctx, scale, scale);
	
	cairo_translate (ctx, -(attribs->width / 2) - attribs->x,
					 -(attribs->height / 2) - attribs->y);
	path = ccm_drawable_get_damage_path (CCM_DRAWABLE(window), ctx);
	cairo_clip(ctx);
	cairo_path_destroy (path);
	
	cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
	cairo_paint (ctx);
	cairo_set_operator (ctx, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface (ctx, surface, attribs->x, attribs->y);
	cairo_paint (ctx);
	
	cairo_destroy (ctx);
	
	return TRUE;
}

static gboolean
ccm_mosaic_screen_paint(CCMScreenPlugin* plugin, CCMScreen* screen,
                        cairo_t* context)
{
	CCMMosaic* self = CCM_MOSAIC (plugin);
	CCMRegion* damaged = NULL;
	gboolean ret = FALSE;
	gint nb_windows = 0;
		
	if (self->priv->enabled) 
	{
		GList* item = ccm_screen_get_windows(screen);
		
		for (;item; item = item->next)
		{
			CCMWindowType type = ccm_window_get_hint_type (item->data);
			if (CCM_WINDOW_XWINDOW(item->data) != self->priv->window &&
				((CCMWindow*)item->data)->is_viewable && 
				!((CCMWindow*)item->data)->is_input_only &&
				type == CCM_WINDOW_TYPE_NORMAL) 
			{
				nb_windows++;
			}
		}
		if (!nb_windows)
		{
			CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	
			self->priv->enabled = FALSE;
			cairo_surface_destroy (self->priv->surface);
			self->priv->surface = NULL;
			XDestroyWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
			self->priv->window = None;
			if (self->priv->areas)
			{
				g_free(self->priv->areas);
				self->priv->areas = NULL;
			}
			self->priv->nb_areas = 0;
			ccm_screen_damage (screen);
		}
		else if (nb_windows != self->priv->nb_areas) 
		{
			cairo_t* ctx = cairo_create (self->priv->surface);
			
			ccm_mosaic_create_areas(self, nb_windows);
							
			cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
			cairo_paint(context);
			cairo_destroy (ctx);
			ccm_screen_damage (screen);
		}
		else
		{
			gint cpt;
			
			for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
			{
				CCMRegion* d = ccm_mosaic_get_damaged_area (self, self->priv->areas[cpt].window);
				if (d)
				{
					ccm_drawable_damage_region (CCM_DRAWABLE(self->priv->areas[cpt].window), d);
					ccm_region_destroy (d);
				}
			}
		}
	}
	
	ret = ccm_screen_plugin_paint(CCM_SCREEN_PLUGIN_PARENT (plugin), screen, 
								  context);
	
	damaged = ccm_screen_get_damaged (screen);
	if (ret && damaged && !ccm_region_empty(damaged) && self->priv->enabled)
	{
		cairo_rectangle_t* rects;
		gint nb_rects, cpt;
		
		cairo_save(context);
		cairo_reset_clip (context);
		
		ccm_region_get_rectangles (damaged, &rects, &nb_rects);
		cairo_set_source_rgba (context, 0, 0, 0, 0.6);
		for (cpt = 0; cpt < nb_rects; cpt++)
		{
			ccm_debug("CLIP: %i,%i %i,%i", (int)rects[cpt].x, (int)rects[cpt].y,
							 (int)rects[cpt].width, (int)rects[cpt].height);
			cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
							 rects[cpt].width, rects[cpt].height);
			cairo_fill(context);
		}
		
		cairo_set_source_surface (context, self->priv->surface, 0, 0);
		cairo_paint(context);
		cairo_restore (context);
	}
	
	return ret;
}

static gboolean
ccm_mosaic_window_paint(CCMWindowPlugin* plugin, CCMWindow* window,
						cairo_t* context, cairo_surface_t* surface,
						gboolean y_invert)
{
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMMosaic* self = CCM_MOSAIC(_ccm_screen_get_plugin (screen, CCM_TYPE_MOSAIC));
	CCMWindowType type = ccm_window_get_hint_type (window);
	gboolean ret = FALSE;
	
	if (self->priv->enabled)
	{
		if (ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
		{	
			if (CCM_WINDOW_XWINDOW(window) != self->priv->window &&
				window->is_viewable && !window->is_input_only && 
				type == CCM_WINDOW_TYPE_NORMAL) 
			{
				if (self->priv->surface)
					ret = ccm_mosaic_paint_area (self, window,  
												 surface, y_invert);
			}
			else
				ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
											  context, surface, y_invert);
		}
	}
	else
		ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
									  context, surface, y_invert);
	
	return ret;
}

static void
ccm_mosaic_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_mosaic_screen_load_options;
	iface->paint 			= ccm_mosaic_screen_paint;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
}

static void
ccm_mosaic_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= NULL;
	iface->query_geometry 	= NULL;
	iface->paint 			= ccm_mosaic_window_paint;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
	iface->move				= NULL;
	iface->resize			= NULL;
}
