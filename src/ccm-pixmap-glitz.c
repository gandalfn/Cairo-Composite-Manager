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

#include "ccm-pixmap-glitz.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmapGlitz, ccm_pixmap_glitz, CCM_TYPE_PIXMAP);

struct _CCMPixmapGlitzPrivate
{
	glitz_drawable_t* 	gl_drawable;
	glitz_drawable_t* 	gl_pixmap;
	glitz_surface_t*  	gl_surface;
	glitz_format_t*		gl_format;
};

#define CCM_PIXMAP_GLITZ_GET_PRIVATE(o) \
	((CCMPixmapGlitzPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_GLITZ, CCMPixmapGlitzClass))

static cairo_surface_t* ccm_pixmap_glitz_get_surface	(CCMDrawable* drawable);
static void		  		ccm_pixmap_glitz_bind 		  	(CCMPixmap* self);
static gboolean			ccm_pixmap_glitz_repair 		(CCMDrawable* drawable, 
														 CCMRegion* area);

static void
ccm_pixmap_glitz_init (CCMPixmapGlitz *self)
{
	self->priv = CCM_PIXMAP_GLITZ_GET_PRIVATE(self);
	
	self->priv->gl_drawable = NULL;
	self->priv->gl_pixmap = NULL;
	self->priv->gl_surface = NULL;
	self->priv->gl_format = NULL;
}

static void
ccm_pixmap_glitz_finalize (GObject *object)
{
	CCMPixmapGlitz* self = CCM_PIXMAP_GLITZ(object);
	
	if (self->priv->gl_pixmap && self->priv->gl_surface) 
	{
		glitz_surface_release_tex_image(self->priv->gl_surface, 
										self->priv->gl_pixmap);
		glitz_drawable_destroy(self->priv->gl_pixmap);
	}
	if (self->priv->gl_drawable) glitz_drawable_destroy(self->priv->gl_drawable);
	if (self->priv->gl_surface) glitz_surface_destroy(self->priv->gl_surface);
	
	G_OBJECT_CLASS (ccm_pixmap_glitz_parent_class)->finalize (object);
}

static void
ccm_pixmap_glitz_class_init (CCMPixmapGlitzClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapGlitzPrivate));

	CCM_DRAWABLE_CLASS(klass)->get_surface = ccm_pixmap_glitz_get_surface;
	CCM_DRAWABLE_CLASS(klass)->repair = ccm_pixmap_glitz_repair;
	CCM_PIXMAP_CLASS(klass)->bind = ccm_pixmap_glitz_bind;
	
	object_class->finalize = ccm_pixmap_glitz_finalize;
}

static gboolean
ccm_pixmap_glitz_create_gl_drawable(CCMPixmapGlitz* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	glitz_drawable_format_t* format = NULL;
	
	if (!self->priv->gl_drawable)
	{
		glitz_format_t templ;
		XWindowAttributes attribs;
		
		if (!XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
								   CCM_WINDOW_XWINDOW(CCM_PIXMAP(self)->window),
								   &attribs))
			return FALSE;
		
		format = glitz_glx_find_drawable_format_for_visual(
				CCM_DISPLAY_XDISPLAY(display),
				screen->number,
				XVisualIDFromVisual (attribs.visual));
		if (!format)
		{
			g_warning("Error on get glitz format drawable");
			return FALSE;
		}
		
		g_object_set(self, "y_invert", format->y_invert ? TRUE : FALSE, NULL);
		
		format->indirect = _ccm_screen_indirect_rendering (screen);
		self->priv->gl_drawable = glitz_glx_create_pbuffer_drawable (
											CCM_DISPLAY_XDISPLAY(display),
											screen->number,
											format,
											attribs.width, attribs.height);
		if (!self->priv->gl_drawable)
		{
			self->priv->gl_drawable = glitz_glx_create_drawable_for_window (
											CCM_DISPLAY_XDISPLAY(display),
											screen->number,
											format,
											CCM_WINDOW_XWINDOW(CCM_PIXMAP(self)->window),
											attribs.width, attribs.height);
			if (!self->priv->gl_drawable)
			{	
				g_warning("Error on create glitz drawable");
				return FALSE;
			}
		}
				
		self->priv->gl_pixmap = glitz_glx_create_drawable_for_pixmap (
											CCM_DISPLAY_XDISPLAY(display),
											screen->number,
											format,
											CCM_PIXMAP_XPIXMAP(self),
											attribs.width, attribs.height);
		if (!self->priv->gl_pixmap)
		{	
			g_warning("Error on create glitz pixmap");
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

static void
ccm_pixmap_glitz_bind (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapGlitz* self = CCM_PIXMAP_GLITZ(pixmap);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		
	if (!self->priv->gl_surface && 
		ccm_pixmap_glitz_create_gl_drawable(self))
	{
		XWindowAttributes attribs;
		
		if (!XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
								  CCM_WINDOW_XWINDOW(CCM_PIXMAP(self)->window),
								  &attribs))
			return;
		
		self->priv->gl_surface = glitz_surface_create(
											self->priv->gl_drawable,
											self->priv->gl_format,
											attribs.width, attribs.height,
											0, NULL);
		if (self->priv->gl_surface)
		{
			glitz_surface_attach (self->priv->gl_surface,
								  self->priv->gl_drawable,
								  GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);
		}
	}
}

static gboolean
ccm_pixmap_glitz_repair (CCMDrawable* drawable, CCMRegion* area)
{
	g_return_val_if_fail(drawable != NULL, FALSE);
	g_return_val_if_fail(area != NULL, FALSE);
	
	CCMPixmapGlitz* self = CCM_PIXMAP_GLITZ(drawable);
	gboolean ret = FALSE;
	
	if (self->priv->gl_surface && self->priv->gl_pixmap)
		ret = glitz_surface_bind_tex_image(self->priv->gl_surface,
										   self->priv->gl_pixmap);
	
	return ret;
}

static cairo_surface_t*
ccm_pixmap_glitz_get_surface (CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmapGlitz *self = CCM_PIXMAP_GLITZ(drawable);
	cairo_surface_t* surface = NULL;
	
	if (CCM_PIXMAP(self)->window->is_viewable)
		ccm_drawable_repair (drawable);
	
	if (self->priv->gl_surface)
		surface = cairo_glitz_surface_create (self->priv->gl_surface);
	
	return surface;
}

