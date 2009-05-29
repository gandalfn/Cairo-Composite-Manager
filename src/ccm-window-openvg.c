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
#include <cairo.h>
#include <vg/openvg.h>
#include <cairo-openvg.h>

#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-window-openvg.h"

G_DEFINE_TYPE (CCMWindowOpenVG, ccm_window_openvg, CCM_TYPE_WINDOW);

struct _CCMWindowOpenVGPrivate
{
	int none;
};

#define CCM_WINDOW_OPENVG_GET_PRIVATE(o)  \
   ((CCMWindowOpenVGPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW_OPENVG, CCMWindowOpenVGClass))

static cairo_surface_t* ccm_window_openvg_get_surface(CCMDrawable* drawable);
static Visual*	ccm_window_openvg_get_visual(CCMDrawable* drawable);
static void ccm_window_openvg_flush(CCMDrawable* drawable);
static void ccm_window_openvg_flush_region(CCMDrawable* drawable, 
										  CCMRegion* region);

static void
ccm_window_openvg_init (CCMWindowOpenVG *self)
{
	self->priv = CCM_WINDOW_OPENVG_GET_PRIVATE(self);
	
}

static void
ccm_window_openvg_finalize (GObject *object)
{
	CCMWindowOpenVG* self = CCM_WINDOW_OPENVG(object);
	
	vgDestroyContextSH ();
	
	G_OBJECT_CLASS (ccm_window_openvg_parent_class)->finalize (object);
}

static void
ccm_window_openvg_class_init (CCMWindowOpenVGClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMWindowOpenVGPrivate));

	CCM_DRAWABLE_CLASS(klass)->get_surface =  ccm_window_openvg_get_surface;
	CCM_DRAWABLE_CLASS(klass)->get_visual = ccm_window_openvg_get_visual;
	CCM_DRAWABLE_CLASS(klass)->flush = ccm_window_openvg_flush;
	CCM_DRAWABLE_CLASS(klass)->flush_region = ccm_window_openvg_flush_region;
	
	object_class->finalize = ccm_window_openvg_finalize;
}

static XVisualInfo*
ccm_window_openvg_get_visual_info(CCMWindowOpenVG* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	XVisualInfo* visinfo;	
	int attribs[] = { GLX_USE_GL,
      				  GLX_DOUBLEBUFFER, 
					  GLX_RGBA, 
					  GLX_RED_SIZE, 1, 
					  GLX_GREEN_SIZE, 1,
					  GLX_BLUE_SIZE, 1, 
					  GLX_ALPHA_SIZE, 1, 
					  GLX_DEPTH_SIZE, 1, 
					  0 };
					  
	
    visinfo = glXChooseVisual (CCM_DISPLAY_XDISPLAY(display), screen->number, 
							   attribs);
	
	return visinfo;
}

static GLXContext
ccm_window_openvg_get_gl_context(CCMWindowOpenVG* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	GLXContext context = g_object_get_data (G_OBJECT(screen), "GLXContext");
	
	if (!context)
	{
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		XVisualInfo* visinfo = ccm_window_openvg_get_visual_info(self);
		
		context = glXCreateContext(CCM_DISPLAY_XDISPLAY(display), visinfo, 
								   NULL, GL_TRUE);
		g_object_set_data(G_OBJECT(screen), "GLXContext", context);
	}
	
	return context;
}

static cairo_surface_t*
ccm_window_openvg_get_surface(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMWindowOpenVG* self = CCM_WINDOW_OPENVG(drawable);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	GLXContext context = ccm_window_openvg_get_gl_context(self);
	cairo_surface_t* surface = NULL;
	cairo_rectangle_t geometry;
	
	ccm_drawable_get_geometry_clipbox (drawable, &geometry);
	
	glXMakeCurrent (CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(self), 
					context);
	
	vgCreateContextSH ((int)geometry.width, (int)geometry.height);
	
	surface = cairo_openvg_surface_create((int)geometry.width, (int)geometry.height);
	
	return surface;
}

static Visual*
ccm_window_openvg_get_visual(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMWindowOpenVG* self = CCM_WINDOW_OPENVG(drawable);
	Visual* visual = NULL;
	XVisualInfo *visinfo = NULL;
			
	visinfo = ccm_window_openvg_get_visual_info(self);
	visual = g_memdup(visinfo->visual, sizeof(Visual));
	XFree(visinfo);
	
	return visual;
}

typedef void (*glXSwapIntervalSGIProc) (int);
typedef void (*glXSwapIntervalMESAProc) (int);
typedef void (*glXCopySubBufferMESAProc) (Display*, Drawable, 
										  int, int, int, int);

static void
ccm_window_openvg_vsync(CCMWindowOpenVG* self, gint interval)
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
ccm_window_openvg_flush(CCMDrawable* drawable)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowOpenVG* self = CCM_WINDOW_OPENVG(drawable);
	CCMScreen* screen = ccm_drawable_get_screen(drawable);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	
	if (_ccm_screen_sync_with_blank(screen)) 
		ccm_window_openvg_vsync(self, 1);
	else
		ccm_window_openvg_vsync(self, 0);
	
	glXSwapBuffers(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(self));
}

static void
ccm_window_openvg_flush_region(CCMDrawable* drawable, CCMRegion* region)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowOpenVG* self = CCM_WINDOW_OPENVG(drawable);
	CCMScreen* screen = ccm_drawable_get_screen(drawable);
	CCMDisplay* display = ccm_drawable_get_display (drawable);
	static glXCopySubBufferMESAProc csb = NULL;
	static gboolean supported = TRUE;
	
	if (_ccm_screen_sync_with_blank(screen)) 
		ccm_window_openvg_vsync(self, 1);
	else
		ccm_window_openvg_vsync(self, 0);
	
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
		
		ccm_drawable_get_geometry_clipbox (drawable, &geometry);
		ccm_region_get_rectangles(region, &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; ++cpt)
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
		cairo_rectangles_free(rects, nb_rects);
	}
	else
		glXSwapBuffers(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(self));
}
