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
#include <X11/ImUtil.h>
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

struct _CCMShmImage
{
	CCMDisplay* 		display;
	Visual*				visual;
	int 				depth;
	XImage* 			image;
	XShmSegmentInfo		shminfo;
};

typedef struct _CCMShmImage CCMShmImage;

struct _CCMPixmapShmPrivate
{
	CCMShmImage*		image;
	Pixmap 				pixmap;
	GC 					gc;
	gboolean 			synced;
};

#define CCM_PIXMAP_SHM_GET_PRIVATE(o) \
	((CCMPixmapShmPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP_SHM, CCMPixmapShmClass))

static cairo_surface_t* ccm_pixmap_shm_get_surface	  (CCMDrawable* drawable);
static gboolean	  		ccm_pixmap_shm_repair 		  (CCMDrawable* drawable, 
													   CCMRegion* area);
static void		  		ccm_pixmap_shm_bind 		  (CCMPixmap* self);
static void		  		ccm_pixmap_shm_release 		  (CCMPixmap* self);

static CCMShmImage*
ccm_shm_image_new(CCMDisplay* display, Visual* visual,
				  int width, int height, int depth)
{
	g_return_val_if_fail(display != NULL, NULL);
	
	CCMShmImage* image = g_new0(CCMShmImage, 1);
	
	image->display = display;
	image->visual = visual;
	image->depth = depth;
	
	image->image = XShmCreateImage(CCM_DISPLAY_XDISPLAY(image->display),
								   image->visual, image->depth,
								   ZPixmap, NULL, &image->shminfo,
								   width, height);
	if (!image->image) return NULL;
	
	image->shminfo.shmid = shmget (IPC_PRIVATE, image->image->bytes_per_line * 
								   image->image->height, IPC_CREAT | 0600);
	
	if (image->shminfo.shmid == -1)
	{
		XDestroyImage(image->image);
		image->image = NULL;
		g_free(image);
		return NULL;
	}
	
	image->shminfo.readOnly = False;
	image->shminfo.shmaddr = shmat (image->shminfo.shmid, 0, 0);
	if (image->shminfo.shmaddr == (gpointer)-1)
	{
		XDestroyImage(image->image);
		image->image = NULL;
		g_free(image);
		return NULL;
	}
	
	image->image->data = image->shminfo.shmaddr;
	
	XShmAttach(CCM_DISPLAY_XDISPLAY(display), &image->shminfo);
	
	return image;
}

static void
ccm_shm_image_destroy(CCMShmImage* image)
{
	g_return_if_fail(image != NULL);
	
	if (image->image)
	{
		XShmDetach (CCM_DISPLAY_XDISPLAY(image->display), &image->shminfo);
		XDestroyImage(image->image);
		shmdt (image->shminfo.shmaddr);
		shmctl (image->shminfo.shmid, IPC_RMID, 0);
		image->image = NULL;
	}
	g_free(image);
}

static gboolean
ccm_shm_image_get_image(CCMShmImage* image, CCMPixmap* pixmap, int x, int y)
{
	g_return_val_if_fail(image != NULL, FALSE);
	g_return_val_if_fail(pixmap != NULL, FALSE);
	
	return XShmGetImage (CCM_DISPLAY_XDISPLAY(image->display), 
						 CCM_PIXMAP_XPIXMAP(pixmap), image->image, 
						 x, y, AllPlanes);
}

static gboolean
ccm_shm_image_get_sub_image(CCMShmImage* image, CCMPixmap* pixmap, 
							int x, int y, int width, int height)
{
	g_return_val_if_fail(image != NULL, FALSE);
	g_return_val_if_fail(pixmap != NULL, FALSE);
	g_return_val_if_fail(width > 0 && height > 0, FALSE);
	
	gboolean ret = FALSE;
	CCMShmImage* sub_image = ccm_shm_image_new (image->display, image->visual,
												width, height, image->depth);
	if (sub_image)
	{
		if (ccm_shm_image_get_image (sub_image, pixmap, x, y))
		{
			// FIXME: need to use pixman when cairo 1.6.0 is faster then _XSetImage
			_XSetImage(sub_image->image, image->image, x, y);
			ret = TRUE;
		}
		ccm_shm_image_destroy (sub_image);
	}
	
	return ret;
}

static void
ccm_pixmap_shm_init (CCMPixmapShm *self)
{
	self->priv = CCM_PIXMAP_SHM_GET_PRIVATE(self);
	
	self->priv->image = NULL;
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
	
	self->priv->image = ccm_shm_image_new (display, attribs.visual, 
										   attribs.width, attribs.height,
										   ccm_window_get_depth(CCM_PIXMAP(self)->window));
	if (!self->priv->image) return;
	
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
			ccm_shm_image_destroy(self->priv->image);
			return;
		}
	
		self->priv->pixmap = XShmCreatePixmap(CCM_DISPLAY_XDISPLAY(display),
											  CCM_PIXMAP_XPIXMAP(self),
											  self->priv->image->image->data,
											  &self->priv->image->shminfo,
											  attribs.width, 
											  attribs.height,
											  ccm_window_get_depth(CCM_PIXMAP(self)->window));
		if (!self->priv->pixmap)
		{
			XFreeGC(CCM_DISPLAY_XDISPLAY(display), self->priv->gc);
			self->priv->gc = NULL;
			ccm_shm_image_destroy(self->priv->image);
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
		ccm_shm_image_destroy (self->priv->image);
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
			gint cpt;
			
			for (cpt = 0; cpt < nb_rects; cpt++)
			{
				XCopyArea(CCM_DISPLAY_XDISPLAY(display),
						  CCM_PIXMAP_XPIXMAP(self), self->priv->pixmap,
						  self->priv->gc, 
						  rects[cpt].x, rects[cpt].y, 
						  rects[cpt].width, rects[cpt].height, 
						  rects[cpt].x, rects[cpt].y);
			}
			ccm_display_sync(display);
		}
		else
		{
			if (!self->priv->synced) 
			{
				if (!ccm_shm_image_get_image (self->priv->image, 
											  CCM_PIXMAP(self), 0, 0))
					ret = FALSE;
				else
					self->priv->synced = TRUE;
			}
			else
			{
				gint cpt;
				
				for (cpt = 0; cpt < nb_rects; cpt++)
				{
					if (!ccm_shm_image_get_sub_image (self->priv->image,
										CCM_PIXMAP(self), 
										rects[cpt].x, rects[cpt].y, 
										rects[cpt].width, rects[cpt].height))
					{
						ret = FALSE;
						break;
					}
				}
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
									(unsigned char *)self->priv->image->image->data, 
									ccm_window_get_format(CCM_PIXMAP(self)->window),
									(int)self->priv->image->image->width, 
									(int)self->priv->image->image->height, 
									self->priv->image->image->bytes_per_line);
	}
	
	return surface;
}

