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

#include "ccm-pixmap-shm.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmapShm, ccm_pixmap_shm, CCM_TYPE_PIXMAP);

struct _CCMPixmapShmPrivate
{
	XImage* 			image;
	Pixmap 				pixmap;
	GC 					gc;
	XShmSegmentInfo		shminfo;
	gboolean 			synced;
};

#define CCM_PIXMAP_SHM_GET_PRIVATE(o) \
	((CCMPixmapShmPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_SHM, CCMPixmapShmClass))

static cairo_surface_t* ccm_pixmap_shm_get_surface	  (CCMDrawable* drawable);
static gboolean	  		ccm_pixmap_shm_repair 		  (CCMDrawable* drawable, 
													   CCMRegion* area);
static void		  		ccm_pixmap_shm_bind 		  (CCMPixmap* self);
static void		  		ccm_pixmap_shm_release 		  (CCMPixmap* self);

static void
ccm_pixmap_shm_init (CCMPixmapShm *self)
{
	self->priv = CCM_PIXMAP_SHM_GET_PRIVATE(self);
	
	self->priv->image = 0;
	self->priv->pixmap = None;
	self->priv->gc = 0;
	self->priv->synced = FALSE;
}

static void
ccm_pixmap_shm_finalize (GObject *object)
{
	G_OBJECT_CLASS (ccm_pixmap_shm_parent_class)->finalize (object);
}

static void
ccm_pixmap_shm_class_init (CCMPixmapShmClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapShmPrivate));

	CCM_DRAWABLE_CLASS(klass)->repair = ccm_pixmap_shm_repair;
	CCM_DRAWABLE_CLASS(klass)->get_surface = ccm_pixmap_shm_get_surface;
	CCM_PIXMAP_CLASS(klass)->bind = ccm_pixmap_shm_bind;
	CCM_PIXMAP_CLASS(klass)->release = ccm_pixmap_shm_release;
	
	object_class->finalize = ccm_pixmap_shm_finalize;
}

static void
ccm_pixmap_shm_bind (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapShm* self = CCM_PIXMAP_SHM(pixmap);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
	XGCValues gcv;
	XWindowAttributes attribs;
		
	XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display),
						  CCM_WINDOW_XWINDOW(CCM_PIXMAP(self)->window),
						  &attribs);
	
	self->priv->image = XShmCreateImage(CCM_DISPLAY_XDISPLAY(display),
										attribs.visual,
										ccm_window_get_depth(CCM_PIXMAP(self)->window),
										ZPixmap, NULL, &self->priv->shminfo,
										attribs.width, 
										attribs.height);
	if (!self->priv->image) return;
	
	self->priv->shminfo.shmid = shmget (IPC_PRIVATE,
										self->priv->image->bytes_per_line * 
										self->priv->image->height, 
										IPC_CREAT | 0600);
	if (self->priv->shminfo.shmid == -1)
	{
		XDestroyImage(self->priv->image);
		self->priv->image = NULL;
		return;
	}
	
	self->priv->shminfo.readOnly = False;
	self->priv->shminfo.shmaddr = shmat (self->priv->shminfo.shmid, 0, 0);
	if (self->priv->shminfo.shmaddr == (gpointer)-1)
	{
		XDestroyImage(self->priv->image);
		self->priv->image = NULL;
		return;
	}
	
	self->priv->image->data = self->priv->shminfo.shmaddr;
	
	XShmAttach(CCM_DISPLAY_XDISPLAY(display), &self->priv->shminfo);
	ccm_display_sync(display);
	
	if (_ccm_display_xshm_shared_pixmap(display))
	{
		gcv.graphics_exposures = FALSE;
		gcv.subwindow_mode = IncludeInferiors;

		self->priv->gc = XCreateGC(CCM_DISPLAY_XDISPLAY(display),
								   CCM_PIXMAP_XPIXMAP(self),
								   GCGraphicsExposures | GCSubwindowMode,
							   	&gcv);
		if (!self->priv->gc)
		{
			XShmDetach (CCM_DISPLAY_XDISPLAY(display), &self->priv->shminfo);
			XDestroyImage(self->priv->image);
			self->priv->image = NULL;
			shmdt (self->priv->shminfo.shmaddr);
			shmctl (self->priv->shminfo.shmid, IPC_RMID, 0);
			return;
		}
	
		self->priv->pixmap = XShmCreatePixmap(CCM_DISPLAY_XDISPLAY(display),
											  CCM_PIXMAP_XPIXMAP(self),
											  self->priv->image->data,
											  &self->priv->shminfo,
											  attribs.width, 
											  attribs.height,
											  ccm_window_get_depth(CCM_PIXMAP(self)->window));
		if (!self->priv->pixmap)
		{
			XFreeGC(CCM_DISPLAY_XDISPLAY(display), self->priv->gc);
			self->priv->gc = NULL;
			XShmDetach (CCM_DISPLAY_XDISPLAY(display), &self->priv->shminfo);
			XDestroyImage(self->priv->image);
			self->priv->image = NULL;
			shmdt (self->priv->shminfo.shmaddr);
			shmctl (self->priv->shminfo.shmid, IPC_RMID, 0);
			return;
		}
	}
}

static void
ccm_pixmap_shm_release (CCMPixmap* pixmap)
{
	g_return_if_fail(pixmap != NULL);
	
	CCMPixmapShm* self = CCM_PIXMAP_SHM(pixmap);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	
	if (self->priv->pixmap) 
	{
		XFreePixmap(CCM_DISPLAY_XDISPLAY(display), self->priv->pixmap);
		self->priv->pixmap = None;
	}

	if (self->priv->gc)
	{
		XFreeGC(CCM_DISPLAY_XDISPLAY(display), self->priv->gc);
		self->priv->gc = NULL;
	}
	
	if (self->priv->image)
	{
		XShmDetach (CCM_DISPLAY_XDISPLAY(display), &self->priv->shminfo);
		XDestroyImage(self->priv->image);
		shmdt (self->priv->shminfo.shmaddr);
		shmctl (self->priv->shminfo.shmid, IPC_RMID, 0);
		self->priv->image = NULL;
	}
}

static gboolean
ccm_pixmap_shm_repair (CCMDrawable* drawable, CCMRegion* area)
{
	g_return_val_if_fail(drawable != NULL, FALSE);
	g_return_val_if_fail(area != NULL, FALSE);
	
	CCMPixmapShm* self = CCM_PIXMAP_SHM(drawable);
	gboolean ret = TRUE;
	
	if (self->priv->image)
	{
		CCMDisplay* display = ccm_drawable_get_display(drawable);
		XRectangle* rects;
		gint nb_rects;
			
		ccm_region_get_xrectangles (area, &rects, &nb_rects);
		if (self->priv->pixmap)
		{
			XserverRegion region;
			
			region = XFixesCreateRegion(CCM_DISPLAY_XDISPLAY(display), rects, nb_rects);
			
			XFixesSetGCClipRegion(CCM_DISPLAY_XDISPLAY (display), self->priv->gc,
								  0, 0, region);
			XFixesDestroyRegion(CCM_DISPLAY_XDISPLAY (display), region);
			
			XCopyArea(CCM_DISPLAY_XDISPLAY(display),
					  CCM_PIXMAP_XPIXMAP(self), self->priv->pixmap,
					  self->priv->gc, 
					  0, 0, self->priv->image->width, self->priv->image->height, 0, 0);
			
			ccm_display_sync(display);
		}
		else
		{
			if (!self->priv->synced) 
			{
				if (!XShmGetImage (CCM_DISPLAY_XDISPLAY(display), CCM_PIXMAP_XPIXMAP(self), 
								  self->priv->image , 0, 0, AllPlanes))
					ret = FALSE;
				else
					self->priv->synced = TRUE;
			}
			else
			{
				gint cpt;
				XImage* fake_image;
				CCMRegion* linear = ccm_region_new();
				
				/* XShm don't provide GetSubImage, to simulate we can shift in 
				   the image data, but we need to keep the original width. 
				   To minimize the number of rectangle, we must transform the 
				   damaged area to horizontal bands. To do this, we recreate the 
				   damaged region with horizontal rectangles, ccm_region performs 
				   a compression then repairs with this new list of rectangles */
				for (cpt = 0; cpt < nb_rects; cpt++)
				{
					rects[cpt].x = 0;
					rects[cpt].width = self->priv->image->width;
					ccm_region_union_with_xrect (linear, &rects[cpt]);
				}
				g_free(rects);
				
				fake_image = g_memdup(self->priv->image, sizeof(XImage));
				ccm_region_get_xrectangles (linear, &rects, &nb_rects);
				for (cpt = 0; cpt < nb_rects; cpt++)
				{
					fake_image->data = self->priv->image->data + 
									  rects[cpt].y * fake_image->bytes_per_line;
					fake_image->height = rects[cpt].height;
					if (!XShmGetImage (CCM_DISPLAY_XDISPLAY(display), 
									   CCM_PIXMAP_XPIXMAP(self), 
									   fake_image , 0, rects[cpt].y, AllPlanes))
					{
						ret = FALSE;
						break;
					}
				}
				g_free(fake_image);
			}
		}
		g_free(rects);
	}
	
	return ret;
}

static cairo_surface_t*
ccm_pixmap_shm_get_surface (CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmapShm *self = CCM_PIXMAP_SHM(drawable);
	cairo_surface_t* surface = NULL;
	
	if (CCM_PIXMAP(self)->window->is_viewable && 
		ccm_drawable_is_damaged (CCM_DRAWABLE(self)))
		ccm_drawable_repair(CCM_DRAWABLE(self));
		
	if (self->priv->image)
	{
		surface = cairo_image_surface_create_for_data(
									(unsigned char *)self->priv->image->data, 
									ccm_window_get_format(CCM_PIXMAP(self)->window),
									(int)self->priv->image->width, 
									(int)self->priv->image->height, 
									self->priv->image->bytes_per_line);
	}
	
	return surface;
}

