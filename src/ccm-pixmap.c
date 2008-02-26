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
#include <X11/extensions/Xfixes.h>
#include <stdlib.h>
#include <cairo.h>

#include "ccm-pixmap.h"
#include "ccm-pixmap-backend.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"

G_DEFINE_TYPE (CCMPixmap, ccm_pixmap, CCM_TYPE_DRAWABLE);

struct _CCMPixmapPrivate
{
	Damage				damage;
};

#define CCM_PIXMAP_GET_PRIVATE(o)  \
   ((CCMPixmapPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP, CCMPixmapClass))

static CCMRegion* ccm_pixmap_query_geometry(CCMDrawable* drawable);
static void ccm_pixmap_bind (CCMPixmap* self);
static void ccm_pixmap_release (CCMPixmap* self);
static void ccm_pixmap_on_damage(CCMPixmap* self, Damage damage, CCMDisplay* display);

static void
ccm_pixmap_init (CCMPixmap *self)
{
	self->priv = CCM_PIXMAP_GET_PRIVATE(self);
	
	self->window = NULL;
	self->priv->damage = 0;
}

static void
ccm_pixmap_finalize (GObject *object)
{
	CCMPixmap* self = CCM_PIXMAP(object);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(object));
	
	if (self->priv->damage)
	{
		XDamageDestroy(CCM_DISPLAY_XDISPLAY(display), self->priv->damage);
		g_signal_handlers_disconnect_by_func(display, ccm_pixmap_on_damage, self);
	}
	
	ccm_pixmap_release(self);
	
	XFreePixmap(CCM_DISPLAY_XDISPLAY(display), CCM_PIXMAP_XPIXMAP(self));
	
	G_OBJECT_CLASS (ccm_pixmap_parent_class)->finalize (object);
}

static void
ccm_pixmap_class_init (CCMPixmapClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapPrivate));
	
	CCM_DRAWABLE_CLASS(klass)->query_geometry = ccm_pixmap_query_geometry;
	
	object_class->finalize = ccm_pixmap_finalize;
}

static CCMRegion*
ccm_pixmap_query_geometry(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	
	CCMPixmap* self = CCM_PIXMAP(drawable);
	CCMRegion* win_geo = ccm_drawable_get_geometry(CCM_DRAWABLE(self->window));
	CCMRegion* geometry = NULL;
	
	if (win_geo)
	{
		cairo_rectangle_t clipbox;
	
		geometry = ccm_region_copy(win_geo);
		
		ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self->window), &clipbox);
		ccm_region_offset(geometry, -(int)clipbox.x, -(int)clipbox.y);
	}
	
	return geometry;
}

static void
ccm_pixmap_bind (CCMPixmap* self)
{
	g_return_if_fail(self != NULL);
	
	if (CCM_PIXMAP_GET_CLASS(self)->bind) 
		CCM_PIXMAP_GET_CLASS(self)->bind (self);
}

static void
ccm_pixmap_release (CCMPixmap* self)
{
	g_return_if_fail(self != NULL);
	
	if (CCM_PIXMAP_GET_CLASS(self)->release) 
		CCM_PIXMAP_GET_CLASS(self)->release (self);
}

static void
ccm_pixmap_on_damage(CCMPixmap* self, Damage damage, CCMDisplay* display)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->damage == damage)
	{
		CCMRegion* damaged = ccm_region_new();
		XserverRegion region = XFixesCreateRegion(CCM_DISPLAY_XDISPLAY (display), NULL, 0);
		XRectangle* rects;
		gint nb_rects, cpt;
		
		XDamageSubtract (CCM_DISPLAY_XDISPLAY (display), self->priv->damage,
						 None, region);
		
		rects = XFixesFetchRegion(CCM_DISPLAY_XDISPLAY (display), region, &nb_rects);
		for (cpt = 0; cpt < nb_rects; cpt++)
			ccm_region_union_with_xrect(damaged, &rects[cpt]);
		
		XFree(rects);
		XFixesDestroyRegion(CCM_DISPLAY_XDISPLAY (display), region);
		ccm_drawable_damage_region (CCM_DRAWABLE(self), damaged);
		ccm_region_destroy (damaged);
	}
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
	
	g_signal_connect_swapped(display, "damage-event", 
							 G_CALLBACK(ccm_pixmap_on_damage), self);
}

CCMPixmap*
ccm_pixmap_new (CCMWindow* window, Pixmap xpixmap)
{
	g_return_val_if_fail(window != NULL, NULL);
	g_return_val_if_fail(xpixmap != None, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMPixmap* self = g_object_new(ccm_pixmap_backend_get_type(screen), 
								   "screen", screen,
								   "drawable", xpixmap,
								   NULL);
	self->window = window;
	ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	ccm_pixmap_register_damage(self, ccm_drawable_get_display(CCM_DRAWABLE(window)));
	ccm_pixmap_bind(self);
	ccm_drawable_damage(CCM_DRAWABLE(self));
	
	return self;
}
