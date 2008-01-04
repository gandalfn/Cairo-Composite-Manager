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

#include <X11/Xlib.h>
#include <GL/glx.h>
#include <glitz.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>

#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-window-glitz.h"

G_DEFINE_TYPE (CCMWindowGlitz, ccm_window_glitz, CCM_TYPE_WINDOW);

struct _CCMWindowGlitzPrivate
{
	glitz_drawable_t*   gl_drawable;
	glitz_format_t*		gl_format;
	glitz_surface_t*	gl_surface;
};

#define CCM_WINDOW_GLITZ_GET_PRIVATE(o)  \
   ((CCMWindowGlitzPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW_GLITZ, CCMWindowGlitzClass))

static cairo_surface_t* ccm_window_glitz_get_surface(CCMDrawable* drawable);
static Visual*	ccm_window_glitz_get_visual(CCMDrawable* drawable);
static void ccm_window_glitz_flush(CCMDrawable* drawable);
static void ccm_window_glitz_flush_region(CCMDrawable* drawable, 
										  CCMRegion* region);

static void
ccm_window_glitz_init (CCMWindowGlitz *self)
{
	self->priv = CCM_WINDOW_GLITZ_GET_PRIVATE(self);
	self->priv->gl_drawable = NULL;
	self->priv->gl_format = NULL;
	self->priv->gl_surface = NULL;
}

static void
ccm_window_glitz_finalize (GObject *object)
{
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(object);
	
	if (self->priv->gl_drawable) glitz_drawable_destroy(self->priv->gl_drawable);
	if (self->priv->gl_surface) glitz_surface_destroy(self->priv->gl_surface);

	G_OBJECT_CLASS (ccm_window_glitz_parent_class)->finalize (object);
}

static void
ccm_window_glitz_class_init (CCMWindowGlitzClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMWindowGlitzPrivate));

	CCM_DRAWABLE_CLASS(klass)->get_surface =  ccm_window_glitz_get_surface;
	CCM_DRAWABLE_CLASS(klass)->get_visual = ccm_window_glitz_get_visual;
	CCM_DRAWABLE_CLASS(klass)->flush = ccm_window_glitz_flush;
	CCM_DRAWABLE_CLASS(klass)->flush_region = ccm_window_glitz_flush_region;
	
	object_class->finalize = ccm_window_glitz_finalize;
}

static gboolean
ccm_window_glitz_create_gl_drawable(CCMWindowGlitz* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	glitz_drawable_format_t* format = NULL;
	
	if (!self->priv->gl_drawable)
	{
		glitz_format_t templ;
		XWindowAttributes attribs;
		
		XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
							  CCM_WINDOW_XWINDOW(self),
							  &attribs);	
		
		format = glitz_glx_find_drawable_format_for_visual(
				CCM_DISPLAY_XDISPLAY(display),
				screen->number,
				XVisualIDFromVisual (attribs.visual));
		
		if (!format)
		{
			g_warning("Error on get glitz format drawable");
			return FALSE;
		}

#ifdef ENABLE_GLITZ_TFP_BACKEND
		format->indirect = _ccm_screen_native_pixmap_bind(screen);
#endif
		
		self->priv->gl_drawable = glitz_glx_create_drawable_for_window(
											CCM_DISPLAY_XDISPLAY(display),
											screen->number,
											format,
											CCM_WINDOW_XWINDOW(self),
											attribs.width, attribs.height);
		if (!self->priv->gl_drawable)
		{
			g_warning("Error on create glitz drawable");
			return FALSE;
		}
				
		templ.color = format->color;
		templ.color.fourcc = GLITZ_FOURCC_RGB;
		
		self->priv->gl_format = glitz_find_format(self->priv->gl_drawable,
												  GLITZ_FORMAT_RED_SIZE_MASK   |
										  		  GLITZ_FORMAT_GREEN_SIZE_MASK |
										  		  GLITZ_FORMAT_BLUE_SIZE_MASK  |
										  		  GLITZ_FORMAT_ALPHA_SIZE_MASK |
										  		  GLITZ_FORMAT_FOURCC_MASK,
										  		  &templ, 0);
		if (!self->priv->gl_format)
		{
			g_warning("Error on get gl drawable format");
			return FALSE;
		}
	}
	
	return self->priv->gl_drawable ? TRUE : FALSE;
}

static gboolean
ccm_window_glitz_create_gl_surface(CCMWindowGlitz* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		
	if (!self->priv->gl_surface && 
		ccm_window_glitz_create_gl_drawable(self))
	{
		XWindowAttributes attribs;
		
		XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
							  CCM_WINDOW_XWINDOW(self),
							  &attribs);
		
		self->priv->gl_surface = glitz_surface_create(
											self->priv->gl_drawable,
											self->priv->gl_format,
											attribs.width, attribs.height,
											0, NULL);
		if (self->priv->gl_surface)
		{
			
			glitz_surface_attach (self->priv->gl_surface,
								  self->priv->gl_drawable,
								  GLITZ_DRAWABLE_BUFFER_BACK_COLOR);
		}
	}
	
	return self->priv->gl_surface ? TRUE : FALSE;
}

static cairo_surface_t*
ccm_window_glitz_get_surface(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(drawable);
	cairo_surface_t* surface = NULL;
	
	if (ccm_window_glitz_create_gl_surface(self))
	{
		surface = cairo_glitz_surface_create(self->priv->gl_surface);
	}
	
	return surface;
}

static Visual*
ccm_window_glitz_get_visual(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(drawable);
	Visual* visual = NULL;
	
	if (ccm_window_glitz_create_gl_drawable(self))
	{
		XVisualInfo *visinfo = NULL;
		glitz_drawable_format_t* gformat = 
			glitz_drawable_get_format(self->priv->gl_drawable);
		CCMScreen* screen = ccm_drawable_get_screen(drawable);
		CCMDisplay* display = ccm_drawable_get_display(drawable);
		
		visinfo = glitz_glx_get_visual_info_from_format(
											  CCM_DISPLAY_XDISPLAY(display),
											  screen->number, gformat);		
		visual = g_memdup(visinfo->visual, sizeof(Visual));
		XFree(visinfo);
	}
	
	return visual;
}

typedef void (*glXSwapIntervalSGIProc) (int);
typedef void (*glXSwapIntervalMESAProc) (int);
typedef void (*glXCopySubBufferMESAProc) (Display*, Drawable, 
										  int, int, int, int);

static void
ccm_window_glitz_vsync(CCMWindowGlitz* self, gint interval)
{
	g_return_if_fail(self != NULL);
	static int initialized = 0;
	static gboolean supported = TRUE;
	
	if (supported && initialized != interval)
	{
		glXSwapIntervalSGIProc s = NULL;	
		glXSwapIntervalMESAProc m = NULL;

		s = (glXSwapIntervalSGIProc)
			glXGetProcAddress((const guchar*)"glXSwapIntervalSGI");
		if (s) 
		{
			s(interval);
			initialized = interval;
			return;
		}
			
		m = (glXSwapIntervalMESAProc)
			glXGetProcAddress((const guchar*)"glXSwapIntervalMESA");
		if (m)
		{
			m(interval);
			initialized = interval;
			return;
		}
		supported = FALSE;
	}
}
					   
static void
ccm_window_glitz_flush(CCMDrawable* drawable)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(drawable);
	CCMScreen* screen = ccm_drawable_get_screen(drawable);
	
	if (ccm_window_glitz_create_gl_drawable(self))
	{
		if (_ccm_screen_sync_with_blank(screen)) 
			ccm_window_glitz_vsync(self, 1);
		else
			ccm_window_glitz_vsync(self, 0);
		glitz_drawable_swap_buffers(self->priv->gl_drawable);
	}
}
static void
ccm_window_glitz_flush_region(CCMDrawable* drawable, CCMRegion* region)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(drawable);
	CCMScreen* screen = ccm_drawable_get_screen(drawable);
	
	if (ccm_window_glitz_create_gl_drawable(self))
	{
		static glXCopySubBufferMESAProc csb = NULL;
		static gboolean supported = TRUE;
		
		if (_ccm_screen_sync_with_blank(screen)) 
			ccm_window_glitz_vsync(self, 1);
		else
			ccm_window_glitz_vsync(self, 0);
		
		if (!csb && supported)
		{
		 	csb = (glXCopySubBufferMESAProc)
				   glXGetProcAddress((const guchar*)"glXCopySubBufferMESA");
			supported = csb != NULL;
		}
		
		if (csb)
		{
			cairo_rectangle_t* rects, geometry;
			gint cpt, nb_rects;
			CCMDisplay* display = ccm_drawable_get_display (drawable);
			
			ccm_drawable_get_geometry_clipbox (drawable, &geometry);
			ccm_region_get_rectangles(region, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
			{
				gint x = rects[cpt].x > 0 ? rects[cpt].x : 0;
				gint y = geometry.height - (rects[cpt].height + rects[cpt].y);
				gint width = rects[cpt].width + x > screen->xscreen->width ? 
							 screen->xscreen->width - x : rects[cpt].width;
				gint height = rects[cpt].height + y > screen->xscreen->height ? 
							  screen->xscreen->height - y : rects[cpt].height;
				
				if (width > 0 && height > 0)
					csb(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(self),
						x, y > 0 ? y : 0, width, height);
			}
			g_free(rects);
		}
		else
			glitz_drawable_swap_buffers(self->priv->gl_drawable);
	}
}
