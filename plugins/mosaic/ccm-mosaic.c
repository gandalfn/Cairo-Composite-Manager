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

//#define CCM_DEBUG_ENABLE
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
	int					 mouse_over;
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
	gint cpt;
	
	self->priv = CCM_MOSAIC_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->window = 0;
	self->priv->enabled = FALSE;
	self->priv->x_mouse = 0;
	self->priv->y_mouse = 0;
	self->priv->mouse_over = -1;
	self->priv->areas = NULL;
	self->priv->surface = NULL;
	self->priv->keybind = NULL;
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_mosaic_finalize (GObject *object)
{
	CCMMosaic* self = CCM_MOSAIC(object);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++)
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
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
				  ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
}

static gboolean
ccm_mosaic_recalc_coords(CCMMosaic* self, int num, int* x, int* y,
						 int* x_root, int* y_root)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->priv->areas[num].window != NULL, FALSE);
	g_return_val_if_fail(x != NULL && y != NULL, FALSE);
	g_return_val_if_fail(x_root != NULL && y_root != NULL, FALSE);
	
	const cairo_rectangle_t* area = ccm_window_get_area(self->priv->areas[num].window);
	gfloat scale;
	
	if (!area) return FALSE;
	
	scale = MIN((gfloat)self->priv->areas[num].geometry.height / (gfloat)area->height,
				(gfloat)self->priv->areas[num].geometry.width / (gfloat)area->width);
				
	*x -= (gint)((gfloat)self->priv->areas[num].geometry.x + 
		  ((gfloat)self->priv->areas[num].geometry.width / (gfloat)2) - 
		  (((gfloat)area->width / 2) * scale));
	*x = (gint)((gfloat)*x / scale);
					
	*y -= self->priv->areas[num].geometry.y + 
		  (self->priv->areas[num].geometry.height / 2) - 
		  (area->height / 2) * scale;
	*y /= scale;
	
	*x_root = *x + area->x;
	*y_root = *y + area->y;
	
	return TRUE;
}

static void
ccm_mosaic_send_leave_event(CCMWindow* window, int x, int y)
{
	g_return_if_fail(window != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(window));
	CCMWindow* root = ccm_screen_get_root_window (screen);
	XCrossingEvent evt;
	const cairo_rectangle_t* area = ccm_window_get_area(window);
	
	if (!area) return;
	
	ccm_debug_window(window, "LEAVE: %i,%i %i,%i", x, y, area->x, area->y);
	evt.type = LeaveNotify;
	evt.serial = 0;
	evt.send_event = True;
	evt.display = CCM_DISPLAY_XDISPLAY(display);
	evt.root = CCM_WINDOW_XWINDOW(root);
	evt.window = CCM_WINDOW_XWINDOW(window);
	evt.subwindow = 0;
	evt.time = CurrentTime;
	evt.x = x - area->x;
	evt.y = y - area->y;
	evt.x_root = x;
	evt.y_root = y;
	evt.mode = NotifyNormal;
	evt.detail = NotifyAncestor | NotifyInferior;
	evt.same_screen = True;
	evt.focus = True;
	evt.state = 0;
	
	XSendEvent(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(window), True, 
			   NoEventMask, (XEvent*)&evt);
}

static void
ccm_mosaic_send_enter_event(CCMWindow* window, int x, int y)
{
	g_return_if_fail(window != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(window));
	CCMWindow* root = ccm_screen_get_root_window (screen);
	XCrossingEvent evt;
	const cairo_rectangle_t* area = ccm_window_get_area(window);
	
	if (!area) return;
	
	ccm_debug_window(window, "ENTER: %i,%i %i,%i", x, y, area->x, area->y);
	
	evt.type = EnterNotify;
	evt.serial = 0;
	evt.send_event = True;
	evt.display = CCM_DISPLAY_XDISPLAY(display);
	evt.root = CCM_WINDOW_XWINDOW(root);
	evt.window = CCM_WINDOW_XWINDOW(window);
	evt.subwindow = 0;
	evt.time = CurrentTime;
	evt.x = x - area->x;
	evt.y = y - area->y;
	evt.x_root = x;
	evt.y_root = y;
	evt.mode = NotifyNormal;
	evt.detail = NotifyAncestor | NotifyInferior;
	evt.same_screen = True;
	evt.focus = True;
	evt.state = 0;
	
	XSendEvent(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(window), True, 
			   NoEventMask, (XEvent*)&evt);
}

static void
ccm_mosaic_simulate_enter_leave_event(CCMMosaic* self, int area, 
									  int x, int y, int x_root, int y_root)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area < self->priv->nb_areas);
	
	if (area != self->priv->mouse_over)
	{
		int x_win = x,
			y_win = y,
			x_win_root = x_root,
			y_win_root = y_root;
		if (self->priv->mouse_over != -1 && 
			ccm_mosaic_recalc_coords (self, self->priv->mouse_over, 
									  &x_win, &y_win, &x_win_root, &y_win_root))
		{
			CCMWindow* window = self->priv->areas[self->priv->mouse_over].window;
			ccm_mosaic_send_leave_event(window, x_win_root, y_win_root);
		}
		x_win = x;
		y_win = y;
		x_win_root = x_root;
		y_win_root = y_root;
		if (area != -1 && 
			ccm_mosaic_recalc_coords (self, area, &x_win, &y_win, 
									  &x_win_root, &y_win_root))
		{
			CCMWindow* window = self->priv->areas[area].window;
			ccm_mosaic_send_enter_event(window, x_win_root, y_win_root);
		}
		
		self->priv->mouse_over = area;
	}
}

static gboolean
ccm_mosaic_send_event (Window window, int x, int y, 
					   int x_win, int y_win, int width, int height, 
					   XEvent* evt)
{
	g_return_val_if_fail(window != None, FALSE);
	g_return_val_if_fail(evt != NULL, FALSE);

	gboolean send = FALSE;
            
	send = (x >= x_win && x <= x_win + width && 
			y >= y_win && y <= y_win + height);

	if (send)
	{
    	evt->xany.window = window;
		if (evt->type == ButtonPress || evt->type == ButtonRelease)
        {
            XButtonEvent* button_evt = (XButtonEvent*)evt;
            
			button_evt->window = window;
			button_evt->subwindow = 0;
			button_evt->x = x - x_win;
            button_evt->y = y - y_win;
            button_evt->x_root = x;
            button_evt->y_root = y;
			ccm_debug("BROADCAST BUTTON PRESS/RELEASE: 0x%lx %i,%i %i,%i", 
					  window, x, y, button_evt->x, button_evt->y);
        }
		if (evt->type == EnterNotify || evt->type == LeaveNotify)
        {
            XCrossingEvent* crossing_evt = (XCrossingEvent*)evt;
            
			crossing_evt->x = x - x_win;
            crossing_evt->y = y - y_win;
            crossing_evt->x_root = x;
            crossing_evt->y_root = y;
			ccm_debug("BROADCAST ENTER/LEAVE: 0x%lx %i,%i %i,%i", 
					  window, x, y, crossing_evt->x, crossing_evt->y);
        }
		if (evt->type == MotionNotify)
        {
            XMotionEvent* motion_evt = (XMotionEvent*)evt;
            
			motion_evt->x = x - x_win;
            motion_evt->y = y - y_win;
            motion_evt->x_root = x;
            motion_evt->y_root = y;
			ccm_debug("BROADCAST MOTION: 0x%lx %i,%i %i,%i", 
					  window, x, y, x - x_win, y - y_win);
        }
		XSendEvent(evt->xany.display, window, False, 
				   NoEventMask, evt);
	}
	
	return send;
}

static gboolean
ccm_mosaic_broadcast_event(Window window, int x, int y, 
						   int x_win, int y_win, XEvent* evt)
{
	g_return_val_if_fail(window != None, FALSE);
	g_return_val_if_fail(evt != NULL, FALSE);
	
	Window* windows = NULL;
	Window w, p;
	guint n_windows = 0;
	XWindowAttributes attribs;
	gboolean send = FALSE;
		
	
	if (XQueryTree(evt->xany.display, window, &w, &p, 
				   &windows, &n_windows) && windows && n_windows)
	{
		while (n_windows-- && !send)
        {
			XGetWindowAttributes(evt->xany.display, windows[n_windows], 
								 &attribs);	
			
			send = ccm_mosaic_broadcast_event (windows[n_windows], x, y, 
											   x_win + attribs.x,
											   y_win + attribs.y, evt);
        }
        XFree(windows);
	}

	XGetWindowAttributes(evt->xany.display, window, &attribs);	
	send |= ccm_mosaic_send_event(window, x, y, x_win, y_win,
								  attribs.width, attribs.height, evt);
	
	return send;
}

static gboolean
ccm_mosaic_mouse_over_area(CCMMosaic* self, gint area, gint x, gint y)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(area < self->priv->nb_areas, FALSE);
	
	return self->priv->areas[area].geometry.x <= x &&
		   self->priv->areas[area].geometry.y <= y &&
		   self->priv->areas[area].geometry.x +
		   self->priv->areas[area].geometry.width >= x &&
		   self->priv->areas[area].geometry.y +
		   self->priv->areas[area].geometry.height >= y;
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
			case EnterNotify:
			case LeaveNotify:
			{
				XButtonEvent* button_event = (XButtonEvent*)event;
				gint cpt;
				
				for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
				{
					gint x = button_event->x,
						 y = button_event->y,
						 x_root = button_event->x_root,
						 y_root = button_event->y_root;
					
					if (ccm_mosaic_mouse_over_area(self, cpt, x_root, y_root) &&
						ccm_mosaic_recalc_coords(self, cpt, &x, &y, 
												 &x_root, &y_root))
					{
						CCMWindow* window = self->priv->areas[cpt].window;
						const cairo_rectangle_t* area = ccm_window_get_area(window);
	
						if (!area) break;
						
						ccm_mosaic_simulate_enter_leave_event(self, cpt,
															  button_event->x, 
															  button_event->y,
															  button_event->x_root, 
															  button_event->y_root);
						ccm_mosaic_broadcast_event(CCM_WINDOW_XWINDOW(window), 
												   x_root, y_root,
												   area->x, area->y,
												   event);
						break;
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
	gchar* shortcut;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "mosaic", 
												  CCMMosaicOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	shortcut = 
		ccm_config_get_string(self->priv->options [CCM_MOSAIC_SHORTCUT]);
	self->priv->keybind = ccm_keybind_new(self->priv->screen, shortcut, TRUE);
	g_free(shortcut);
	
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
ccm_mosaic_get_damaged_area(CCMMosaic* self, CCMMosaicArea* area)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(area != NULL, NULL);
	
	CCMRegion* damaged = NULL;
				
	if (ccm_drawable_is_damaged (CCM_DRAWABLE(area->window)))
	{
		const cairo_rectangle_t* win_area = ccm_window_get_area(area->window);
		gfloat scale;
		
		if (!win_area) return NULL;
		
		damaged = ccm_region_rectangle (&area->geometry);
		
/*		scale = MIN(area->geometry.height / attribs->height,
					area->geometry.width / attribs->width);
		
		ccm_region_offset(damaged, area->geometry.x - attribs->x,
						  area->geometry.y - attribs->y);
		ccm_region_offset(damaged, area->geometry.width / 2,
						  area->geometry.height / 2);
		ccm_region_offset(damaged, -(attribs->width / 2) * scale,
						  -(attribs->height / 2) * scale);
		ccm_region_resize (damaged, attribs->width * scale, 
						   attribs->height * scale);*/
	}
	
	return damaged;
}
							
static gboolean
ccm_mosaic_paint_area(CCMMosaic* self, CCMWindow* window, cairo_surface_t* target,
					  cairo_surface_t* surface, gboolean y_invert)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(surface != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);
	
	gfloat scale;
	cairo_path_t* path;
	cairo_t* ctx;
	cairo_pattern_t* pattern;
	CCMRegion* damaged;
	CCMMosaicArea* area;
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	const cairo_rectangle_t* win_area = ccm_window_get_area(window);
		
	if (!win_area) return FALSE;
				
	area = ccm_mosaic_find_area(self, window, win_area->width, win_area->height);
	if (!area) return FALSE;
	
	damaged = ccm_mosaic_get_damaged_area (self, area);
	if (!damaged) return FALSE;
	ccm_screen_add_damaged_region (screen, damaged);
	ccm_region_destroy (damaged);

	scale = MIN(area->geometry.height / win_area->height,
				area->geometry.width / win_area->width);
	
	ctx = cairo_create (self->priv->surface);
	
	if (y_invert)
	{
		cairo_scale (ctx, 1.0, -1.0);
		cairo_translate (ctx, 0.0f, - area->geometry.y - area->geometry.height);
	}
	
	cairo_translate (ctx, area->geometry.x + area->geometry.width / 2, 
					 area->geometry.y + area->geometry.height / 2);
	cairo_scale(ctx, scale, scale);
	
	cairo_translate (ctx, -(win_area->width / 2) - win_area->x,
					 -(win_area->height / 2) - win_area->y);
	path = ccm_drawable_get_damage_path (CCM_DRAWABLE(window), ctx);
	cairo_clip (ctx);
	cairo_path_destroy (path);
	
	if (ccm_drawable_get_format (CCM_DRAWABLE(window)) == CAIRO_FORMAT_ARGB32)
	{
		cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
		cairo_paint(ctx);
	}
	cairo_set_operator (ctx, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface (ctx, surface, win_area->x, win_area->y);
	pattern = cairo_get_source (ctx);
	cairo_pattern_set_filter (pattern, CAIRO_FILTER_FAST);
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
				ccm_window_is_viewable(item->data) &&
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
			cairo_paint(ctx);
			cairo_destroy (ctx);
			ccm_screen_damage (screen);
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
		for (cpt = 0; cpt < nb_rects; cpt++)
		{
			ccm_debug("CLIP: %i,%i %i,%i", (int)rects[cpt].x, (int)rects[cpt].y,
							 (int)rects[cpt].width, (int)rects[cpt].height);
			cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
							 rects[cpt].width, rects[cpt].height);
			//cairo_fill_preserve (context);
		}
		cairo_clip(context);
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
				ccm_window_is_viewable(window) && 
				!ccm_window_is_input_only(window) && 
				type == CCM_WINDOW_TYPE_NORMAL) 
			{
				if (self->priv->surface)
				{
					cairo_surface_t* target = cairo_get_target (context);
					ret = ccm_mosaic_paint_area (self, window, target,
												 surface, y_invert);
				}
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
	iface->load_options 	 = NULL;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = ccm_mosaic_window_paint;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
}
