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

#include "ccm-window-xrender.h"

#include <X11/extensions/Xdbe.h>
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window-xrender.h"

G_DEFINE_TYPE (CCMWindowXRender, ccm_window_xrender, CCM_TYPE_WINDOW);

struct _CCMWindowXRenderPrivate
{
	Drawable back_buffer;
};

#define CCM_WINDOW_XRENDER_GET_PRIVATE(o)  \
   ((CCMWindowXRenderPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW_X_RENDER, CCMWindowXRenderClass))

static cairo_surface_t* ccm_window_xrender_get_surface(CCMDrawable* drawable);
static Visual*	ccm_window_xrender_get_visual(CCMDrawable* drawable);
static void ccm_window_xrender_flush(CCMDrawable* drawable);
static void ccm_window_xrender_flush_region(CCMDrawable* drawable, CCMRegion* region);

static void
ccm_window_xrender_init (CCMWindowXRender *self)
{
	self->priv = CCM_WINDOW_XRENDER_GET_PRIVATE(self);
	self->priv->back_buffer = None;
}

static void
ccm_window_xrender_finalize (GObject *object)
{
	CCMWindowXRender* self = CCM_WINDOW_X_RENDER(object);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	
	if (self->priv->back_buffer) 
		XdbeDeallocateBackBufferName(CCM_DISPLAY_XDISPLAY(display), 
									 self->priv->back_buffer);
	
	G_OBJECT_CLASS (ccm_window_xrender_parent_class)->finalize (object);
}

static void
ccm_window_xrender_class_init (CCMWindowXRenderClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMWindowXRenderPrivate));

	CCM_DRAWABLE_CLASS(klass)->get_surface =  ccm_window_xrender_get_surface;
	CCM_DRAWABLE_CLASS(klass)->get_visual = ccm_window_xrender_get_visual;
	CCM_DRAWABLE_CLASS(klass)->flush = ccm_window_xrender_flush;
	CCM_DRAWABLE_CLASS(klass)->flush_region = ccm_window_xrender_flush_region;
	
	object_class->finalize = ccm_window_xrender_finalize;
}

static gboolean
ccm_window_xrender_create_backbuffer(CCMWindowXRender* self)
{
	g_return_val_if_fail(self != NULL, None);
	
	if (!self->priv->back_buffer)
	{
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	
		self->priv->back_buffer = XdbeAllocateBackBufferName(CCM_DISPLAY_XDISPLAY(display), 
															 CCM_WINDOW_XWINDOW(self), 
															 XdbeUndefined);
	}
	
	return self->priv->back_buffer != None;
}

static cairo_surface_t*
ccm_window_xrender_get_surface(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMWindowXRender* self = CCM_WINDOW_X_RENDER(drawable);
	cairo_surface_t* surface = NULL;
		
	if (ccm_window_xrender_create_backbuffer(self))
	{
		CCMDisplay* display = ccm_drawable_get_display(drawable);
		CCMScreen* screen = ccm_drawable_get_screen(drawable);
		cairo_rectangle_t geometry;
		Visual* visual = DefaultVisual(CCM_DISPLAY_XDISPLAY(display), 
									   screen->number);
		
		if (ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry))
			surface = cairo_xlib_surface_create(CCM_DISPLAY_XDISPLAY(display),
											self->priv->back_buffer, 
											visual,
											(int)geometry.width, 
											(int)geometry.height);
	}
	
	return surface;
}

static Visual*
ccm_window_xrender_get_visual(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	Visual* visual = NULL;
	cairo_surface_t* surface = ccm_drawable_get_surface(drawable);
	
	if (surface)
	{
		visual = g_memdup(cairo_xlib_surface_get_visual(surface), sizeof(Visual));
		cairo_surface_destroy(surface);
	}
	
	return visual;
}

static void
ccm_window_xrender_flush(CCMDrawable* drawable)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowXRender* self = CCM_WINDOW_X_RENDER(drawable);
	
	if (ccm_window_xrender_create_backbuffer(self))
	{
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		XdbeSwapInfo swap_info;
		
		swap_info.swap_window = CCM_WINDOW_XWINDOW(self);
		swap_info.swap_action = XdbeUndefined;
		
		XdbeSwapBuffers(CCM_DISPLAY_XDISPLAY(display), &swap_info, 1);
	}
}

static void
ccm_window_xrender_flush_region(CCMDrawable* drawable, CCMRegion* region)
{
	g_return_if_fail(drawable != NULL);
	g_return_if_fail(region != NULL);
	
	CCMWindowXRender* self = CCM_WINDOW_X_RENDER(drawable);
	
	if (ccm_window_xrender_create_backbuffer(self))
	{
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		XdbeSwapInfo swap_info;
		
		swap_info.swap_window = CCM_WINDOW_XWINDOW(self);
		swap_info.swap_action = XdbeCopied;
		
		XdbeSwapBuffers(CCM_DISPLAY_XDISPLAY(display), &swap_info, 1);
	}
}
