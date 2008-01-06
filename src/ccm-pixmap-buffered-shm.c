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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <stdlib.h>
#include <cairo.h>

#include "ccm-pixmap-buffered-shm.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmapBufferedShm, ccm_pixmap_buffered_shm, CCM_TYPE_PIXMAP_SHM);

struct _CCMPixmapBufferedShmPrivate
{
	cairo_surface_t*	 surface;
	CCMRegion*			 need_to_sync;
};

#define CCM_PIXMAP_BUFFERED_SHM_GET_PRIVATE(o) \
	((CCMPixmapBufferedShmPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_BUFFERED_SHM, CCMPixmapBufferedShmClass))

static void ccm_pixmap_buffered_shm_repair (CCMDrawable* drawable, CCMRegion* area);
static cairo_surface_t* ccm_pixmap_buffered_shm_get_surface (CCMDrawable* drawable);

static void
ccm_pixmap_buffered_shm_init (CCMPixmapBufferedShm *self)
{
	self->priv = CCM_PIXMAP_BUFFERED_SHM_GET_PRIVATE(self);
	
	self->priv->surface = NULL;
	self->priv->need_to_sync = NULL;
}

static void
ccm_pixmap_buffered_shm_finalize (GObject *object)
{
	CCMPixmapBufferedShm* self = CCM_PIXMAP_BUFFERED_SHM(object);
	
	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	if (self->priv->need_to_sync) ccm_region_destroy (self->priv->need_to_sync);
	
	G_OBJECT_CLASS (ccm_pixmap_buffered_shm_parent_class)->finalize (object);
}

static void
ccm_pixmap_buffered_shm_class_init (CCMPixmapBufferedShmClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapBufferedShmPrivate));

	CCM_DRAWABLE_CLASS(klass)->get_surface = ccm_pixmap_buffered_shm_get_surface;
	CCM_DRAWABLE_CLASS(klass)->repair = ccm_pixmap_buffered_shm_repair;
	
	object_class->finalize = ccm_pixmap_buffered_shm_finalize;
}

static void
ccm_pixmap_buffered_shm_repair (CCMDrawable* drawable, CCMRegion* area)
{
	g_return_if_fail(drawable != NULL);
	g_return_if_fail(area != NULL);
	
	CCMPixmapBufferedShm* self = CCM_PIXMAP_BUFFERED_SHM(drawable);
	
	CCM_DRAWABLE_CLASS(ccm_pixmap_buffered_shm_parent_class)->repair(drawable, 
																  area);
	
	if (self->priv->need_to_sync)
		ccm_region_union (self->priv->need_to_sync, area);
	else
		self->priv->need_to_sync = ccm_region_copy (area);
}

static void
ccm_pixmap_buffered_shm_sync(CCMPixmapBufferedShm* self, cairo_surface_t* surface)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(surface != NULL);
	
	if (self->priv->need_to_sync)
	{
		gdouble width, height;
		
		width = cairo_image_surface_get_width (surface);
		height = cairo_image_surface_get_height (surface);

		if (!self->priv->surface)
		{
			CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
			CCMWindow* overlay = ccm_screen_get_overlay_window (screen);
				
			cairo_surface_t* target = ccm_drawable_get_surface (CCM_DRAWABLE(overlay));
				
			if (target)
			{
				self->priv->surface = cairo_surface_create_similar (target, 
													CAIRO_CONTENT_COLOR_ALPHA,
													width, height);
			}
		}
		if (self->priv->surface)
		{
			cairo_t* cr;
			cairo_rectangle_t clipbox;
			
			cr = cairo_create(self->priv->surface);
				
			ccm_region_get_clipbox (self->priv->need_to_sync, &clipbox);
			cairo_rectangle (cr, clipbox.x, clipbox.y, 
							 clipbox.width, clipbox.height);
			ccm_region_destroy (self->priv->need_to_sync);
			self->priv->need_to_sync = NULL;
			cairo_clip(cr);
			
			if (ccm_window_get_format (CCM_PIXMAP(self)->window) == CAIRO_FORMAT_ARGB32)
			{
				cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
				cairo_paint (cr);
			}
			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
			cairo_set_source_surface (cr, surface, 0, 0);
			cairo_paint (cr);
			cairo_destroy (cr);
		}
	}
}

static cairo_surface_t*
ccm_pixmap_buffered_shm_get_surface (CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmapBufferedShm *self = CCM_PIXMAP_BUFFERED_SHM(drawable);
	cairo_surface_t* surface = NULL;
	
	if (!ccm_drawable_is_damaged (drawable))
	{
		surface = CCM_DRAWABLE_CLASS(ccm_pixmap_buffered_shm_parent_class)->get_surface(drawable);
		if (surface && CCM_PIXMAP(self)->window->is_viewable)
			ccm_pixmap_buffered_shm_sync(self, surface);
		if (self->priv->surface)
		{
			cairo_surface_destroy (surface);	
			surface = cairo_surface_reference (self->priv->surface);
		}
	}
	else
		surface = CCM_DRAWABLE_CLASS(ccm_pixmap_buffered_shm_parent_class)->get_surface(drawable);
		
	return surface;
}

