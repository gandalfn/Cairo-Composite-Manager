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

#include "ccm-debug.h"
#include "ccm-pixmap.h"
#include "ccm-pixmap-backend.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-object.h"

CCM_DEFINE_TYPE (CCMPixmap, ccm_pixmap, CCM_TYPE_DRAWABLE);

enum
{
    PROP_0,
	PROP_Y_INVERT,
	PROP_FREEZE
};

struct _CCMPixmapPrivate
{
	Damage				damage;
	
	gboolean			y_invert;
	gboolean			freeze;
};

#define CCM_PIXMAP_GET_PRIVATE(o)  \
   ((CCMPixmapPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PIXMAP, CCMPixmapClass))

static void ccm_pixmap_bind (CCMPixmap* self);
static void ccm_pixmap_release (CCMPixmap* self);
static void ccm_pixmap_on_damage(CCMPixmap* self, Damage damage, 
								 CCMDisplay* display);

static void
ccm_pixmap_set_property(GObject *object, guint prop_id, 
						const GValue *value, GParamSpec *pspec)
{
	CCMPixmap* self = CCM_PIXMAP(object);
    
	switch (prop_id)
    {
    	case PROP_Y_INVERT:
		{
			self->priv->y_invert = g_value_get_boolean (value);
		}
		break;
		case PROP_FREEZE:
		{
			self->priv->freeze = g_value_get_boolean (value);
		}
		break;
		default:
		break;
    }
}

static void
ccm_pixmap_get_property(GObject *object, guint prop_id, 
						GValue *value, GParamSpec *pspec)
{
	CCMPixmap* self = CCM_PIXMAP(object);
    
	switch (prop_id)
    {
    	case PROP_Y_INVERT:
		{
			g_value_set_boolean (value, self->priv->y_invert);
		}
		break;
		case PROP_FREEZE:
		{
			g_value_set_boolean (value, self->priv->freeze);
		}
		break;
		default:
		break;
    }
}

static void
ccm_pixmap_init (CCMPixmap *self)
{
	self->priv = CCM_PIXMAP_GET_PRIVATE(self);
	
	self->priv->damage = 0;
	self->priv->y_invert = FALSE;
	self->priv->freeze = FALSE;
}

static void
ccm_pixmap_finalize (GObject *object)
{
	CCMPixmap* self = CCM_PIXMAP(object);
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(object));
	
	ccm_pixmap_release(self);
	
	if (self->priv->damage)
	{
		XDamageDestroy(CCM_DISPLAY_XDISPLAY(display), self->priv->damage);
		g_signal_handlers_disconnect_by_func(display, ccm_pixmap_on_damage, self);
		self->priv->damage = None;
	}
	self->priv->y_invert = FALSE;
	self->priv->freeze = FALSE;
	
	XFreePixmap(CCM_DISPLAY_XDISPLAY(display), CCM_PIXMAP_XPIXMAP(self));
	
	G_OBJECT_CLASS (ccm_pixmap_parent_class)->finalize (object);
}

static void
ccm_pixmap_class_init (CCMPixmapClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPixmapPrivate));
	
	object_class->set_property = ccm_pixmap_set_property;
	object_class->get_property = ccm_pixmap_get_property;
	
	/**
	 * CCMPixmap:y_invert:
	 *
	 * This property indicate if the pixmap paint is y inverted.
	 */
	g_object_class_install_property(object_class, PROP_Y_INVERT,
		g_param_spec_boolean("y_invert",
		 					 "Y Invert",
			     			 "Get if pixmap is y inverted",
							 FALSE,
			     			 G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	/**
	 * CCMPixmap:freeze:
	 *
	 * This property locks paint and damage if is true.
	 */
	g_object_class_install_property(object_class, PROP_FREEZE,
		g_param_spec_boolean("freeze",
		 					 "Freeze",
			     			 "Freeze pixmap damage and repair",
							 FALSE,
			     			 G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	object_class->finalize = ccm_pixmap_finalize;
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
	
	if (!self->priv->freeze && self->priv->damage == damage)
	{
		CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
		XserverRegion region = XFixesCreateRegion(CCM_DISPLAY_XDISPLAY (display), NULL, 0);
		
		if (region)
		{
			XRectangle* rects;
			gint nb_rects, cpt;
		
			XDamageSubtract (CCM_DISPLAY_XDISPLAY (display), self->priv->damage,
							 None, region);
			rects = XFixesFetchRegion(CCM_DISPLAY_XDISPLAY (display), region, &nb_rects);
			if (rects)
			{
				CCMRegion* damaged = ccm_region_new();
				
				for (cpt = 0; cpt < nb_rects; ++cpt)
				{
					ccm_region_union_with_xrect(damaged, &rects[cpt]);
					ccm_debug ("PIXMAP DAMAGE %i,%i,%i,%i", 
							   rects[cpt].x, rects[cpt].y,
							   rects[cpt].width, rects[cpt].height);
				}
				XFree(rects);
				
				
				ccm_drawable_damage_region (CCM_DRAWABLE(self), damaged);
				ccm_region_destroy (damaged);
			}
			XFixesDestroyRegion(CCM_DISPLAY_XDISPLAY (display), region);
		}
	}
}

static void
ccm_pixmap_register_damage(CCMPixmap* self)
{
	g_return_if_fail(self != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
	
	self->priv->damage = XDamageCreate (CCM_DISPLAY_XDISPLAY (display),
								  		CCM_PIXMAP_XPIXMAP (self),
								  		XDamageReportNonEmpty);
	if (self->priv->damage)
	{
    	XDamageSubtract (CCM_DISPLAY_XDISPLAY (display), self->priv->damage,
						 None, None);
	
		g_signal_connect_swapped(display, "damage-event", 
								 G_CALLBACK(ccm_pixmap_on_damage), self);
	}
	else
		self->priv->damage = None;
}

/**
 * ccm_pixmap_new:
 * @drawable: #CCMDrawable
 * @xpixmap: pixmap
 *
 * Create a new pixmap
 *
 * Returns: #CCMPixmap
 **/
CCMPixmap*
ccm_pixmap_new (CCMDrawable* drawable, Pixmap xpixmap)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	g_return_val_if_fail(xpixmap != None, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen(drawable);
	Visual* visual = ccm_drawable_get_visual(drawable);
	CCMPixmap* self;
	
	g_return_val_if_fail(screen != NULL && visual != None, NULL);
	
	self = g_object_new(ccm_pixmap_backend_get_type(screen), 
						"screen", screen,
						"drawable", xpixmap,
						"visual", visual,
						NULL);
	
	ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	if (!ccm_drawable_get_device_geometry (CCM_DRAWABLE(self)))
	{
		g_object_unref(self);
		return NULL;
	}
	ccm_pixmap_register_damage(self);
	if (!self->priv->damage)
	{
		g_object_unref(self);
		return NULL;
	}
	ccm_pixmap_bind(self);
	ccm_drawable_damage(CCM_DRAWABLE(self));
	
	return self;
}

/**
 * ccm_pixmap_new_from_visual:
 * @screen: #CCMScreen
 * @visual: XVisual
 * @xpixmap: pixmap
 *
 * Create a new pixmap for a screen visual
 *
 * Returns: #CCMPixmap
 **/
CCMPixmap*
ccm_pixmap_new_from_visual (CCMScreen* screen, Visual* visual, Pixmap xpixmap)
{
	g_return_val_if_fail(screen != NULL, NULL);
	g_return_val_if_fail(visual != NULL, NULL);
	g_return_val_if_fail(xpixmap != None, NULL);
	
	CCMPixmap* self;
	
	self = g_object_new(ccm_pixmap_backend_get_type(screen), 
						"screen", screen,
						"drawable", xpixmap,
						"visual", visual,
						NULL);
	
	ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	if (!ccm_drawable_get_device_geometry (CCM_DRAWABLE(self)))
	{
		g_object_unref(self);
		return NULL;
	}
	ccm_pixmap_register_damage(self);
	if (!self->priv->damage)
	{
		g_object_unref(self);
		return NULL;
	}
	ccm_pixmap_bind(self);
	ccm_drawable_damage(CCM_DRAWABLE(self));
	
	return self;
}

/**
 * ccm_pixmap_image_new:
 * @drawable: #CCMDrawable
 * @xpixmap: pixmap
 *
 * Create a new pixmap which software rendering backend
 *
 * Returns: #CCMPixmap
 **/
CCMPixmap*
ccm_pixmap_image_new (CCMDrawable* drawable, Pixmap xpixmap)
{
	g_return_val_if_fail(drawable != NULL, NULL);
	g_return_val_if_fail(xpixmap != None, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen(drawable);
	Visual* visual = ccm_drawable_get_visual(drawable);
	CCMPixmap* self;
	
	g_return_val_if_fail(screen != NULL && visual != None, NULL);
	
	self = g_object_new(ccm_pixmap_image_get_type(), 
						"screen", screen,
						"drawable", xpixmap,
						"visual", visual,
						NULL);
	
	ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	ccm_pixmap_register_damage(self);
	if (!self->priv->damage)
	{
		g_object_unref(self);
		return NULL;
	}
	ccm_pixmap_bind(self);
	ccm_drawable_damage(CCM_DRAWABLE(self));
	
	return self;
}
