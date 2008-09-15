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
#include <X11/extensions/Xrender.h>
#include <stdlib.h>
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

#include "ccm-pixmap-xrender.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmapXRender, ccm_pixmap_xrender, CCM_TYPE_PIXMAP);

struct _CCMPixmapXRenderPrivate
{
	cairo_surface_t*    surface;
};

#define CCM_PIXMAP_XRENDER_GET_PRIVATE(o) \
	((CCMPixmapXRenderPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_XRENDER, CCMPixmapXRenderClass))

static cairo_surface_t* ccm_pixmap_xrender_get_surface	(CCMDrawable* drawable);
static void		  		ccm_pixmap_xrender_bind 		(CCMPixmap* self);

static void
ccm_pixmap_xrender_init (CCMPixmapXRender *self)
{
	self->priv = CCM_PIXMAP_XRENDER_GET_PRIVATE(self);
	
	self->priv->surface = NULL;
}

static void
ccm_pixmap_xrender_finalize (GObject *object)
{
	CCMPixmapXRender* self = CCM_PIXMAP_XRENDER(object);

	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	
	G_OBJECT_CLASS (ccm_pixmap_xrender_parent_class)->finalize (object);
}

static void
ccm_pixmap_xrender_class_init (CCMPixmapXRenderClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapXRenderPrivate));

	CCM_DRAWABLE_CLASS(klass)->get_surface = ccm_pixmap_xrender_get_surface;
	CCM_PIXMAP_CLASS(klass)->bind = ccm_pixmap_xrender_bind;
	
	object_class->finalize = ccm_pixmap_xrender_finalize;
}

static void
ccm_pixmap_xrender_bind (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapXRender* self = CCM_PIXMAP_XRENDER(pixmap);
	
	Visual* visual = ccm_drawable_get_visual (CCM_DRAWABLE(self));
	cairo_rectangle_t geometry;
	
	if (!self->priv->surface && visual &&
		ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE (self), &geometry))
	{
		CCMPixmapXRender* self = CCM_PIXMAP_XRENDER(pixmap);
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	
		self->priv->surface = cairo_xlib_surface_create (
									CCM_DISPLAY_XDISPLAY(display),
									CCM_PIXMAP_XPIXMAP(self), 
									visual,
									geometry.width, 
									geometry.height);
	}
}

static cairo_surface_t*
ccm_pixmap_xrender_get_surface (CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmapXRender *self = CCM_PIXMAP_XRENDER(drawable);
	cairo_surface_t* surface = NULL;
	
	if (self->priv->surface)
	{
		cairo_surface_reference (self->priv->surface);
		surface = self->priv->surface;
	}
	
	return surface;
}
