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

#include "ccm-debug.h"
#include "ccm-region.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-pixmap.h"
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
static void ccm_window_glitz_flush(CCMDrawable* drawable);
static void ccm_window_glitz_flush_region(CCMDrawable* drawable, 
										  CCMRegion* region);

static CCMPixmap* ccm_window_glitz_create_pixmap(CCMWindow* self, int width, 
												 int height, int depth);

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
	CCM_DRAWABLE_CLASS(klass)->flush = ccm_window_glitz_flush;
	CCM_DRAWABLE_CLASS(klass)->flush_region = ccm_window_glitz_flush_region;

	CCM_WINDOW_CLASS(klass)->create_pixmap = ccm_window_glitz_create_pixmap;
	
	object_class->finalize = ccm_window_glitz_finalize;
}

static gboolean
ccm_window_glitz_create_gl_drawable(CCMWindowGlitz* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	glitz_drawable_format_t* format = NULL, tmp;
	
	if (!self->priv->gl_drawable)
	{
		glitz_format_t templ;
		Visual* visual = ccm_drawable_get_visual (CCM_DRAWABLE(self));
		cairo_rectangle_t geometry;
		gboolean indirect;
		unsigned long mask = GLITZ_FORMAT_DOUBLEBUFFER_MASK |
								 GLITZ_FORMAT_RED_SIZE_MASK |
								 GLITZ_FORMAT_GREEN_SIZE_MASK |
								 GLITZ_FORMAT_BLUE_SIZE_MASK |
								 GLITZ_FORMAT_ALPHA_SIZE_MASK;
			
		tmp.doublebuffer = 1;
		tmp.color.red_size = 8;
		tmp.color.green_size = 8;
		tmp.color.blue_size = 8;
		tmp.color.alpha_size = 8;
		
		if (!visual ||
			!ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self), &geometry)) 
			return FALSE;
		
		format = glitz_glx_find_window_format (
						CCM_DISPLAY_XDISPLAY(display),
						CCM_SCREEN_NUMBER(screen),
						mask, &tmp, 1);
		if (!format)
		{
			g_warning("Error on get glitz format drawable");
			return FALSE;
		}

		g_object_get(G_OBJECT(screen), "indirect_rendering", &indirect, NULL);
#ifdef ENABLE_GLITZ_TFP_BACKEND
		glitz_glx_set_render_type(CCM_DISPLAY_XDISPLAY(display),
								  CCM_SCREEN_NUMBER(screen), 
								  !indirect);
#endif
		
		self->priv->gl_drawable = glitz_glx_create_drawable_for_window(
											CCM_DISPLAY_XDISPLAY(display),
											CCM_SCREEN_NUMBER(screen),
											format,
											CCM_WINDOW_XWINDOW(self),
											geometry.width, geometry.height);
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
	
	if (!self->priv->gl_surface && 
		ccm_window_glitz_create_gl_drawable(self))
	{
		cairo_rectangle_t geometry;
		
		if (!ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self), &geometry)) 
			return FALSE;
		
		self->priv->gl_surface = glitz_surface_create(
											self->priv->gl_drawable,
											self->priv->gl_format,
											geometry.width, geometry.height,
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

static void
ccm_window_glitz_flush(CCMDrawable* drawable)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(drawable);
	
	if (ccm_window_glitz_create_gl_drawable(self))
		glitz_drawable_swap_buffers(self->priv->gl_drawable);
}

static void
ccm_window_glitz_flush_region(CCMDrawable* drawable, CCMRegion* region)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindowGlitz* self = CCM_WINDOW_GLITZ(drawable);

	
	if (ccm_window_glitz_create_gl_drawable(self))
	{
		cairo_rectangle_t geometry;
			
		if (ccm_drawable_get_geometry_clipbox (drawable, &geometry))
		{
			gint cpt, nbox = 0;
			glitz_box_t* box = NULL;
			
			box = (glitz_box_t*)ccm_region_get_boxes(region, &nbox);
			if (box)
			{
				for (cpt = 0; cpt < nbox; ++cpt)
				{
					box[cpt].x1 = box[cpt].x1 > 0 ? box[cpt].x1 : 0;
					box[cpt].y1 = box[cpt].y1 > 0 ? box[cpt].y1 : 0;
				}		
				glitz_drawable_swap_buffer_region (self->priv->gl_drawable,
												   geometry.x, geometry.y, 
												   box, nbox);
				g_free(box);
			}
		}
		else
			glitz_drawable_swap_buffers(self->priv->gl_drawable);
	}
}

static CCMPixmap*
ccm_window_glitz_create_pixmap(CCMWindow* self, int width, int height, int depth)
{
	g_return_val_if_fail(self != NULL, NULL);

	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
	CCMDisplay* display = ccm_screen_get_display (screen);
	XVisualInfo* vinfo;
	Pixmap xpixmap;
	gboolean indirect;
	glitz_drawable_format_t* format, templ;
	unsigned long mask = GLITZ_FORMAT_DOUBLEBUFFER_MASK |
						 GLITZ_FORMAT_RED_SIZE_MASK |
						 GLITZ_FORMAT_GREEN_SIZE_MASK |
						 GLITZ_FORMAT_BLUE_SIZE_MASK |
						 GLITZ_FORMAT_DEPTH_MASK;
	
	templ.doublebuffer = 0;
	templ.color.red_size = 8;
	templ.color.green_size = 8;
	templ.color.blue_size = 8;
	templ.depth = depth;
	
	g_object_get(G_OBJECT(screen), "indirect_rendering", &indirect, NULL);
#ifdef ENABLE_GLITZ_TFP_BACKEND
	glitz_glx_set_render_type(CCM_DISPLAY_XDISPLAY(display),
							  CCM_SCREEN_NUMBER(screen), 
							  !indirect);
#endif	
	
	if (depth == 32)
	{
		templ.color.alpha_size = 8;
		mask |= GLITZ_FORMAT_ALPHA_SIZE_MASK;
	}
	format = glitz_glx_find_window_format (CCM_DISPLAY_XDISPLAY(display),
										   CCM_SCREEN_NUMBER(screen), mask,
										   &templ, 0);
	
	if (!format) return NULL;
	
	vinfo = glitz_glx_get_visual_info_from_format (CCM_DISPLAY_XDISPLAY(display),
												   CCM_SCREEN_NUMBER(screen), 
												   format);
	if (!vinfo)	return NULL;
	
	xpixmap = XCreatePixmap(CCM_DISPLAY_XDISPLAY(display), 
							CCM_WINDOW_XWINDOW(self), 
							width, height, depth);

	if (xpixmap == None)
		return NULL;
		
	return ccm_pixmap_new_from_visual(screen, vinfo->visual, xpixmap);
}
