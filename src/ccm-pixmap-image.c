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

#include "ccm-debug.h"
#include "ccm-image.h"
#include "ccm-pixmap-image.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-object.h"

CCM_DEFINE_TYPE (CCMPixmapImage, ccm_pixmap_image, CCM_TYPE_PIXMAP);

struct _CCMPixmapImagePrivate
{
	CCMImage* 			image;
	cairo_surface_t*    surface;
	gboolean			synced;
};

#define CCM_PIXMAP_IMAGE_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_IMAGE, CCMPixmapImagePrivate))

static cairo_surface_t* ccm_pixmap_image_get_surface	(CCMDrawable* drawable);
static gboolean	  		ccm_pixmap_image_repair 		(CCMDrawable* drawable,
														 CCMRegion* area);
static void				ccm_pixmap_image_flush       	(CCMDrawable* drawable);
static void				ccm_pixmap_image_flush_region	(CCMDrawable* drawable, 
													     CCMRegion* area);
static void		  		ccm_pixmap_image_bind 		  	(CCMPixmap* self);
static void		  		ccm_pixmap_image_release 		(CCMPixmap* self);

static void
ccm_pixmap_image_init (CCMPixmapImage *self)
{
	self->priv = CCM_PIXMAP_IMAGE_GET_PRIVATE(self);
	
	self->priv->image = NULL;
	self->priv->surface = NULL;
	self->priv->synced = FALSE;
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
	CCM_DRAWABLE_CLASS(klass)->flush = ccm_pixmap_image_flush;
	CCM_DRAWABLE_CLASS(klass)->flush_region = ccm_pixmap_image_flush_region;
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
	cairo_format_t format = ccm_drawable_get_format (CCM_DRAWABLE(pixmap));
	gint depth = ccm_drawable_get_depth (CCM_DRAWABLE(pixmap));
	Visual* visual = ccm_drawable_get_visual(CCM_DRAWABLE(pixmap));
	cairo_rectangle_t geometry;

	if (visual &&
		ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(pixmap), &geometry))
		self->priv->image = ccm_image_new(display, visual, format, 
										  geometry.width, geometry.height, depth);
	else
		ccm_debug("PIXMAP BIND ERROR");
}

static void
ccm_pixmap_image_release (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapImage* self = CCM_PIXMAP_IMAGE(pixmap);
	
	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	if (self->priv->image) ccm_image_destroy (self->priv->image);
}

static gboolean
ccm_pixmap_image_repair (CCMDrawable* drawable, CCMRegion* area)
{
	g_return_val_if_fail(drawable != NULL, FALSE);
	g_return_val_if_fail(area != NULL, FALSE);
	
	CCMPixmapImage* self = CCM_PIXMAP_IMAGE(drawable);
	gboolean ret = TRUE, frozen = FALSE;
	
	g_object_get (self, "freeze", &frozen, NULL);
	if (!frozen && self->priv->image)
	{
		if (!self->priv->synced) 
		{
			if (!ccm_image_get_image (self->priv->image, 
									  CCM_PIXMAP(self), 0, 0))
			{
				ccm_debug("IMAGE_REPAIR ERROR");
				ret = FALSE;
				ccm_image_destroy (self->priv->image);
				self->priv->image = NULL;
			}
			else
				self->priv->synced = TRUE;
		}
		else
		{
			XRectangle* rects;
			gint nb_rects, cpt;
			
			ccm_region_get_xrectangles (area, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; ++cpt)
			{
				if (!ccm_image_get_sub_image (self->priv->image,
											  CCM_PIXMAP(self), 
											  rects[cpt].x, rects[cpt].y, 
											  rects[cpt].width, 
											  rects[cpt].height))
				{
					ccm_debug("SUB IMAGE_REPAIR ERROR");
					ccm_image_destroy (self->priv->image);
					self->priv->image = NULL;
					ret = FALSE;
					break;
				}
			}
			x_rectangles_free(rects, nb_rects);
		}
	}
	
	return ret;
}

static cairo_surface_t*
ccm_pixmap_image_get_surface (CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmapImage *self = CCM_PIXMAP_IMAGE(drawable);
	
	ccm_drawable_repair(CCM_DRAWABLE(self));
		
	if (!self->priv->image && self->priv->surface)
	{
		cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
	}
	if (self->priv->image && ccm_image_get_data (self->priv->image) && 
		!self->priv->surface)
		self->priv->surface = cairo_image_surface_create_for_data(
								ccm_image_get_data (self->priv->image), 
								ccm_drawable_get_format(CCM_DRAWABLE(self)),
								ccm_image_get_width (self->priv->image), 
								ccm_image_get_height (self->priv->image), 
								ccm_image_get_stride (self->priv->image));
	
	if (self->priv->surface) cairo_surface_reference (self->priv->surface);
	
	return self->priv->surface;
}

static void
ccm_pixmap_image_flush (CCMDrawable* drawable)
{
	g_return_if_fail(drawable != NULL);
	
	CCMPixmapImage *self = CCM_PIXMAP_IMAGE(drawable);
	
	if (self->priv->image)
	{
		cairo_rectangle_t clipbox;
		
		ccm_drawable_get_device_geometry_clipbox (drawable, &clipbox);
		
		ccm_image_put_image (self->priv->image, CCM_PIXMAP(self), 0, 0, 0, 0,
							 clipbox.width, clipbox.height);
	}
}

static void
ccm_pixmap_image_flush_region (CCMDrawable* drawable, CCMRegion* area)
{
	g_return_if_fail(drawable != NULL);
	g_return_if_fail(area != NULL);
	
	CCMPixmapImage *self = CCM_PIXMAP_IMAGE(drawable);
	
	if (self->priv->image)
	{
		XRectangle* rects;
		gint cpt, nb_rects;
		
		ccm_region_get_xrectangles (area, &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; ++cpt)
			ccm_image_put_image (self->priv->image, CCM_PIXMAP(self), 
								 rects[cpt].x, rects[cpt].y,
								 rects[cpt].x, rects[cpt].y,
								 rects[cpt].width, rects[cpt].height);
		x_rectangles_free(rects, nb_rects);
	}
}
