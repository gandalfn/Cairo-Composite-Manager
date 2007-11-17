/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free softwstribute it and/or
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

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <stdlib.h>

#include "ccm-pixmap.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmap, ccm_pixmap, CCM_TYPE_DRAWABLE);

struct _CCMPixmapPrivate
{
	CCMWindow*			window;

	XImage*				image;
	Pixmap				pixmap;
	GC 					gc;
	XShmSegmentInfo		shminfo;
	Damage				damage;
};

#define CCM_PIXMAP_GET_PRIVATE(o)  \
   ((CCMPixmapPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP, CCMPixmapClass))

static CCMRegion* ccm_pixmap_query_geometry(CCMDrawable* drawable);
static void ccm_pixmap_repair(CCMDrawable* drawable, CCMRegion* area);
static cairo_surface_t* ccm_pixmap_get_surface(CCMDrawable* drawable);

static void
ccm_pixmap_init (CCMPixmap *self)
{
	self->priv = CCM_PIXMAP_GET_PRIVATE(self);
	
	self->priv->window = NULL;
	self->priv->image = 0;
	self->priv->damage = 0;
}

static void
ccm_pixmap_finalize (GObject *object)
{
	CCMPixmap* self = CCM_PIXMAP(object);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(object));
	
	if (self->priv->damage)
	{
		_ccm_display_unregister_damage(display, self->priv->damage);
		XDamageDestroy(CCM_DISPLAY_XDISPLAY(display), self->priv->damage);
	}
	
	if (self->priv->pixmap) 
		XFreePixmap(CCM_DISPLAY_XDISPLAY(display), self->priv->pixmap);

	if (self->priv->gc)
		XFreeGC(CCM_DISPLAY_XDISPLAY(display), self->priv->gc);
	
	if (self->priv->image)
	{
		XShmDetach (CCM_DISPLAY_XDISPLAY(display), &self->priv->shminfo);
		XDestroyImage(self->priv->image);
		shmdt (self->priv->shminfo.shmaddr);
		shmctl (self->priv->shminfo.shmid, IPC_RMID, 0);
	}
	
	XFreePixmap(CCM_DISPLAY_XDISPLAY(display), CCM_PIXMAP_XPIXMAP(self));
	G_OBJECT_CLASS (ccm_pixmap_parent_class)->finalize (object);
}

static void
ccm_pixmap_class_init (CCMPixmapClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapPrivate));
	
	CCM_DRAWABLE_CLASS(klass)->query_geometry = ccm_pixmap_query_geometry;
	CCM_DRAWABLE_CLASS(klass)->repair = ccm_pixmap_repair;
	CCM_DRAWABLE_CLASS(klass)->get_surface = ccm_pixmap_get_surface;
	
	object_class->finalize = ccm_pixmap_finalize;
}

static CCMRegion*
ccm_pixmap_query_geometry(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmap* self = CCM_PIXMAP(drawable);
	CCMRegion* win_geo = ccm_drawable_get_geometry(CCM_DRAWABLE(self->priv->window));
	CCMRegion* geometry = ccm_region_copy(win_geo);
	cairo_rectangle_t clipbox;
	
	ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self->priv->window), &clipbox);
	ccm_region_offset(geometry, -(int)clipbox.x, -(int)clipbox.y);
	
	return geometry;
}

static void
ccm_pixmap_bind (CCMPixmap* self)
{
	g_return_if_fail(self != NULL);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	Visual* visual = ccm_drawable_get_visual(CCM_DRAWABLE(self->priv->window));
	cairo_rectangle_t geometry;
	XGCValues gcv;
	
	ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self->priv->window), &geometry);
	if (_ccm_display_use_xshm(display))
	{
		self->priv->image = XShmCreateImage(CCM_DISPLAY_XDISPLAY(display),
											visual,
											ccm_window_get_depth(self->priv->window),
											ZPixmap, NULL, &self->priv->shminfo,
											(int)geometry.width, 
											(int)geometry.height);
		self->priv->shminfo.shmid = shmget (IPC_PRIVATE,
											self->priv->image->bytes_per_line * 
											self->priv->image->height, 
											IPC_CREAT | 0600);
		self->priv->shminfo.readOnly = False;
		self->priv->shminfo.shmaddr = self->priv->image->data = shmat (self->priv->shminfo.shmid, 0, 0);
		XShmAttach(CCM_DISPLAY_XDISPLAY(display), &self->priv->shminfo);
		
		gcv.graphics_exposures = FALSE;
		gcv.subwindow_mode = IncludeInferiors;

		self->priv->gc = XCreateGC(CCM_DISPLAY_XDISPLAY(display),
								   CCM_PIXMAP_XPIXMAP(self),
								   GCGraphicsExposures | GCSubwindowMode,
								   &gcv);

		self->priv->pixmap = XShmCreatePixmap(CCM_DISPLAY_XDISPLAY(display),
											  CCM_PIXMAP_XPIXMAP(self),
											  self->priv->image->data,
											  &self->priv->shminfo,
											  (int)geometry.width, 
											  (int)geometry.height,
											  ccm_window_get_depth(self->priv->window));
	}
	else
	{
		self->priv->image = XCreateImage (CCM_DISPLAY_XDISPLAY(display),
										  visual,
										  ccm_window_get_depth(self->priv->window),
										  ZPixmap, 0, NULL,
										  (int)geometry.width, 
										  (int)geometry.height, 32, 0);
		if (self->priv->image)
			self->priv->image->data = malloc(self->priv->image->bytes_per_line * geometry.height);
	}
}

static void
ccm_pixmap_repair(CCMDrawable* drawable, CCMRegion* area)
{
	g_return_if_fail(drawable != NULL);
	g_return_if_fail(area != NULL);
	
	CCMPixmap* self = CCM_PIXMAP(drawable);
	CCMDisplay* display = ccm_drawable_get_display(drawable);
	cairo_rectangle_t* rects;
	gint nb_rects, cpt;
		
	ccm_region_get_rectangles(area, &rects, &nb_rects);
	
	for (cpt = 0; cpt < nb_rects; cpt++)
	{
		if (_ccm_display_use_xshm(display))
		{
			XCopyArea(CCM_DISPLAY_XDISPLAY(display),
					  CCM_PIXMAP_XPIXMAP(self), self->priv->pixmap,
					  self->priv->gc, 
					  (int)rects[cpt].x, (int)rects[cpt].y, 
					  (int)rects[cpt].width, (int)rects[cpt].height, 
					  (int)rects[cpt].x, (int)rects[cpt].y);
		}
		else
		{
			XGetSubImage (CCM_DISPLAY_XDISPLAY(display),
						  CCM_PIXMAP_XPIXMAP(self),
						  (int)rects[cpt].x, (int)rects[cpt].y, 
						  (int)rects[cpt].width, (int)rects[cpt].height,
						  AllPlanes, ZPixmap, self->priv->image, 
						  (int)rects[cpt].x, (int)rects[cpt].y);
		}
	}
	ccm_display_sync(display);
	
	g_free(rects);
}

static cairo_surface_t*
ccm_pixmap_get_surface(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmap *self = CCM_PIXMAP(drawable);
	cairo_rectangle_t geometry;
	cairo_surface_t* surface = NULL;
	
	if (ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self->priv->window), 
									  &geometry))
	{
		if (ccm_window_is_viewable(self->priv->window))
			ccm_drawable_repair(CCM_DRAWABLE(self));
		
		surface = cairo_image_surface_create_for_data(
									(unsigned char *)self->priv->image->data, 
									ccm_window_get_format(self->priv->window),
									(int)geometry.width, 
									(int)geometry.height, 
									self->priv->image->bytes_per_line);
	}
	return surface;
}

static void
ccm_pixmap_on_damage(CCMDisplay* display, cairo_rectangle_t* area, 
					 CCMPixmap* self)
{
	g_return_if_fail(display != NULL);
	g_return_if_fail(area != NULL);
	g_return_if_fail(self != NULL);
	
	XDamageSubtract (CCM_DISPLAY_XDISPLAY (display), self->priv->damage,
					 None, None);
	
	ccm_drawable_damage_rectangle(CCM_DRAWABLE(self), area);
}

static void
ccm_pixmap_register_damage(CCMPixmap* self, CCMDisplay* display)
{
	g_return_if_fail(self != NULL);
	
	self->priv->damage = XDamageCreate (CCM_DISPLAY_XDISPLAY (display),
								  		CCM_PIXMAP_XPIXMAP (self),
								  		XDamageReportDeltaRectangles);

    XDamageSubtract (CCM_DISPLAY_XDISPLAY (display), self->priv->damage,
					 None, None);
	
	_ccm_display_register_damage(display, self->priv->damage, 
								 (CCMDamageFunc)ccm_pixmap_on_damage, self);
}

CCMPixmap*
ccm_pixmap_new (CCMWindow* window, Pixmap xpixmap)
{
	g_return_val_if_fail(window != NULL, NULL);
	g_return_val_if_fail(xpixmap != None, NULL);
	
	CCMPixmap* self = g_object_new(CCM_TYPE_PIXMAP, 
								   "screen", ccm_drawable_get_screen(CCM_DRAWABLE(window)),
								   "drawable", xpixmap,
								   NULL);
	self->priv->window = window;
	ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	ccm_pixmap_register_damage(self, ccm_drawable_get_display(CCM_DRAWABLE(window)));
	ccm_pixmap_bind(self);
	ccm_drawable_damage(CCM_DRAWABLE(self));
	
	return self;
}
