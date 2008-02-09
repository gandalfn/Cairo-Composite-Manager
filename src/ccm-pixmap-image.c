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
#include <stdlib.h>
#include <cairo.h>

#include "ccm-pixmap-image.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmapImage, ccm_pixmap_image, CCM_TYPE_PIXMAP);

struct _CCMPixmapImagePrivate
{
	XImage* 			image;
	int 				width;
	int 				height;
};

#define CCM_PIXMAP_IMAGE_GET_PRIVATE(o) \
	((CCMPixmapImagePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_IMAGE, CCMPixmapImageClass))

static cairo_surface_t* ccm_pixmap_image_get_surface	(CCMDrawable* drawable);
static void		  		ccm_pixmap_image_repair 		(CCMDrawable* drawable,
														 CCMRegion* area);
static void		  		ccm_pixmap_image_bind 		  	(CCMPixmap* self);
static void		  		ccm_pixmap_image_release 		(CCMPixmap* self);

static void
ccm_pixmap_image_init (CCMPixmapImage *self)
{
	self->priv = CCM_PIXMAP_IMAGE_GET_PRIVATE(self);
	
	self->priv->image = 0;
}

static void
ccm_pixmap_image_finalize (GObject *object)
{
	G_OBJECT_CLASS (ccm_pixmap_image_parent_class)->finalize (object);
}

static void
ccm_pixmap_image_class_init (CCMPixmapImageClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapImagePrivate));

	CCM_DRAWABLE_CLASS(klass)->repair = ccm_pixmap_image_repair;
	CCM_DRAWABLE_CLASS(klass)->get_surface = ccm_pixmap_image_get_surface;
	CCM_PIXMAP_CLASS(klass)->bind = ccm_pixmap_image_bind;
	CCM_PIXMAP_CLASS(klass)->release = ccm_pixmap_image_release;
	
	object_class->finalize = ccm_pixmap_image_finalize;
}

static void
ccm_pixmap_image_bind (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapImage* self = CCM_PIXMAP_IMAGE(pixmap);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(pixmap));
	XWindowAttributes attribs;
		
	XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
						  CCM_WINDOW_XWINDOW(CCM_PIXMAP(self)->window),
						  &attribs);
	
	self->priv->width = attribs.width;
	self->priv->height = attribs.height;
}

static void
ccm_pixmap_image_release (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapImage* self = CCM_PIXMAP_IMAGE(pixmap);
	
	if (self->priv->image) XDestroyImage(self->priv->image);
}

static void
ccm_pixmap_image_repair (CCMDrawable* drawable, CCMRegion* area)
{
	g_return_if_fail(drawable != NULL);
	g_return_if_fail(area != NULL);
	
	CCMPixmapImage* self = CCM_PIXMAP_IMAGE(drawable);
	CCMDisplay* display = ccm_drawable_get_display(drawable);
	
	if (!self->priv->image) 	
	{
		self->priv->image = XGetImage (CCM_DISPLAY_XDISPLAY(display),
									   CCM_PIXMAP_XPIXMAP(self), 0, 0, 
									   self->priv->width, 
									   self->priv->height, 
									   AllPlanes, ZPixmap);
	}
	else
	{
		XRectangle* rects;
		gint nb_rects, cpt;
	
		ccm_region_get_xrectangles (area, &rects, &nb_rects);
	
		for (cpt = 0; cpt < nb_rects; cpt++)
		{
			XGetSubImage (CCM_DISPLAY_XDISPLAY(display),
					   	  CCM_PIXMAP_XPIXMAP(self), rects[cpt].x, rects[cpt].y, 
						  rects[cpt].width, rects[cpt].height, 
						  AllPlanes, ZPixmap, self->priv->image, 
						  rects[cpt].x, rects[cpt].y);
		}
		g_free(rects);
	}
}

static cairo_surface_t*
ccm_pixmap_image_get_surface (CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmapImage *self = CCM_PIXMAP_IMAGE(drawable);
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(CCM_PIXMAP(self)->window));
	cairo_surface_t* surface = NULL;
	cairo_rectangle_t* rects;
	gint nb_rects, cpt;
	
	if (CCM_PIXMAP(self)->window->is_viewable)
		ccm_drawable_repair(CCM_DRAWABLE(self));
		
	if (self->priv->image)
		surface = cairo_image_surface_create_for_data(
									(unsigned char *)self->priv->image->data, 
									ccm_window_get_format(CCM_PIXMAP(self)->window),
									self->priv->width, 
								    self->priv->height, 
								    self->priv->image->bytes_per_line);
	
	return surface;
}

