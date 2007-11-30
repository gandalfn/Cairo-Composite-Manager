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

#include "ccm-drawable.h"
#include "ccm-display.h"

enum
{
    PROP_0,
	PROP_SCREEN,
    PROP_XDRAWABLE
};

enum
{
	DAMAGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (CCMDrawable, ccm_drawable, G_TYPE_OBJECT);

struct _CCMDrawablePrivate
{
	Drawable			drawable;
	CCMScreen* 			screen;
	CCMRegion*			geometry;
	
	CCMRegion*			damaged;
	cairo_surface_t*	surface;
};

#define CCM_DRAWABLE_GET_PRIVATE(o)  \
   ((CCMDrawablePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DRAWABLE, CCMDrawableClass))

static CCMRegion* __ccm_drawable_query_geometry(CCMDrawable* self);
static void 	  __ccm_drawable_move		   (CCMDrawable* self, 
												int x, int y);
static void 	  __ccm_drawable_resize		   (CCMDrawable* self, 
											    int width, int height);

static void
ccm_drawable_set_property(GObject *object,
						  guint prop_id,
						  const GValue *value,
						  GParamSpec *pspec)
{
	CCMDrawablePrivate* priv = CCM_DRAWABLE_GET_PRIVATE(object);
    
	switch (prop_id)
    {
    	case PROP_SCREEN:
			priv->screen= g_object_ref(g_value_get_pointer (value));
			break;
		case PROP_XDRAWABLE:
			priv->drawable = g_value_get_ulong (value);
			break;
    	default:
			break;
    }
}

static void
ccm_drawable_get_property (GObject* object,
						   guint prop_id,
						   GValue* value,
						   GParamSpec* pspec)
{
    CCMDrawablePrivate* priv = CCM_DRAWABLE_GET_PRIVATE(object);
    
    switch (prop_id)
    {
    	case PROP_SCREEN:
			g_value_set_pointer (value, priv->screen);
			break;
		case PROP_XDRAWABLE:
			g_value_set_ulong (value, priv->drawable);
			break;
		
    	default:
			break;
    }
}

static void
ccm_drawable_init (CCMDrawable *self)
{
	self->priv = CCM_DRAWABLE_GET_PRIVATE(self);
	self->priv->drawable = 0;
	self->priv->geometry = NULL;
	self->priv->damaged = NULL;
}

static void
ccm_drawable_finalize (GObject *object)
{
	CCMDrawable* self = CCM_DRAWABLE(object);
	
	self->priv->drawable = 0;
	if (self->priv->geometry) ccm_region_destroy(self->priv->geometry);
	if (self->priv->screen) g_object_unref(self->priv->screen);
	
	G_OBJECT_CLASS (ccm_drawable_parent_class)->finalize (object);
}

static void
ccm_drawable_class_init (CCMDrawableClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMDrawablePrivate));
	
	object_class->get_property = ccm_drawable_get_property;
    object_class->set_property = ccm_drawable_set_property;
	object_class->finalize = ccm_drawable_finalize;
	
	klass->query_geometry = __ccm_drawable_query_geometry;
	klass->move = __ccm_drawable_move;
	klass->resize = __ccm_drawable_resize;
	
	g_object_class_install_property(object_class, PROP_SCREEN,
		g_param_spec_pointer ("screen",
		 					 "Screen",
			     			 "Screen of the drawable",
			     			 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
    g_object_class_install_property(object_class, PROP_XDRAWABLE,
		g_param_spec_ulong ("drawable",
		 					"Drawable",
			     			"Xid of the drawable",
			     			0, G_MAXLONG, None,
			     			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
	signals[DAMAGED] = g_signal_new ("damaged",
									 G_OBJECT_CLASS_TYPE (object_class),
									 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
									 g_cclosure_marshal_VOID__POINTER,
									 G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static CCMRegion*
__ccm_drawable_query_geometry(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMDisplay* display = ccm_screen_get_display(self->priv->screen);
	Window root;		/* dummy */
    guint bw, depth;		/* dummies */
	gint x, y;
	guint width, height;
	cairo_rectangle_t rectangle;
	
	if (self->priv->geometry) ccm_region_destroy(self->priv->geometry);
	self->priv->geometry = NULL;
		
	if (!XGetGeometry (CCM_DISPLAY_XDISPLAY(display),
					   self->priv->drawable, &root, 
					   &x, &y, &width, &height,
					   &bw, &depth))
    {
		g_warning("Error on get drawable geometry");
		return NULL;
    }
	rectangle.x = (double)x;
	rectangle.y = (double)y;
	rectangle.width = (double)width;
	rectangle.height = (double)height;
	return ccm_region_rectangle(&rectangle);
}

static void
__ccm_drawable_move(CCMDrawable* self, int x, int y)
{
	g_return_if_fail(self != NULL);
	
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox(self, &geometry))
	{
		ccm_region_offset(self->priv->geometry, 
						  x - (int)geometry.x, 
						  y - (int)geometry.y);
	}
}

static void
__ccm_drawable_resize(CCMDrawable* self, int width, int height)
{
	g_return_if_fail(self != NULL);
	
	cairo_rectangle_t geometry;
	ccm_drawable_get_geometry_clipbox(self, &geometry);
	if (width != (int)geometry.width || height != (int)geometry.height)
	{
		ccm_drawable_query_geometry(self);
	}
}

/**
 * ccm_drawable_get_screen:
 * @self: #CCMDrawable
 *
 * Returns the #CCMScreen associated with drawable.
 *
 * Returns: #CCMScreen
 **/
CCMScreen*
ccm_drawable_get_screen(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->screen;
}

/**
 * ccm_drawable_get_display:
 * @self: #CCMDrawable
 *
 * Returns the #CCMDisplay associated with drawable.
 *
 * Returns: #CCMDisplay
 **/
CCMDisplay*
ccm_drawable_get_display(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->screen ? ccm_screen_get_display(self->priv->screen) : NULL;
}

/**
 * ccm_drawable_get_visual:
 * @self: #CCMDrawable
 *
 * Return visual of drawable
 *
 * Returns: Visual
 **/
Visual*
ccm_drawable_get_visual(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (CCM_DRAWABLE_GET_CLASS(self)->get_visual)
	{
		return CCM_DRAWABLE_GET_CLASS(self)->get_visual(self);
	}
	return NULL;
}

/**
 * ccm_drawable_get_xid:
 * @self: #CCMDrawable
 *
 * Returns the X resource (window or pixmap) belonging to a Drawable
 *
 * Returns: XID
 **/
XID
ccm_drawable_get_xid(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, 0);
	
	return self->priv->drawable;
}

/**
 * ccm_drawable_query_geometry:
 * @self: #CCMDrawable
 *
 * Gets the region covered by the Drawable. The coordinates are relative 
 * to the parent Drawable. 
 *
 * Returns: #CCMRegion
 **/
CCMRegion*
ccm_drawable_query_geometry(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (CCM_DRAWABLE_GET_CLASS(self)->query_geometry)
	{
		if (self->priv->geometry) ccm_region_destroy(self->priv->geometry);
		self->priv->geometry = CCM_DRAWABLE_GET_CLASS(self)->query_geometry(self);
	}
	
	return self->priv->geometry;
}

/**
 * ccm_drawable_unset_geometry:
 * @self: #CCMDrawable
 *
 * Unset geometry covered by the drawable
 **/
void
ccm_drawable_unset_geometry(CCMDrawable* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->geometry)
	{
		ccm_region_destroy (self->priv->geometry);
		self->priv->geometry = NULL;
	}
}

/**
 * ccm_drawable_get_geometry:
 * @self: #CCMDrawable
 *
 * Gets the region covered by the Drawable. The coordinates are 
 * relative to the parent Drawable. 
 *
 * Returns: #CCMRegion
 **/
CCMRegion*
ccm_drawable_get_geometry(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (self->priv->geometry)
	{
		return self->priv->geometry;
	}
	
	return NULL;
}

/**
 * ccm_drawable_get_geometry_clipbox:
 * @self: #CCMDrawable
 * @area: #cairo_rectangle_t
 *
 * Gets the rectangle region covered by the Drawable. The coordinates are 
 * relative to the parent Drawable. 
 *
 * Returns: FALSE if fail
 **/
gboolean
ccm_drawable_get_geometry_clipbox(CCMDrawable* self, cairo_rectangle_t* area)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(area != NULL, FALSE);
	
	if (self->priv->geometry)
	{
		ccm_region_get_clipbox(self->priv->geometry, area);
		return TRUE;
	}
	
	return FALSE;
}

/**
 * ccm_drawable_move:
 * @self: #CCMDrawable
 * @x: X coordinate relative to drawable's parent
 * @y: Y coordinate relative to drawable's parent
 *
 * Repositions a drawable relative to its parent drawable. 
 **/
void
ccm_drawable_move(CCMDrawable* self, int x, int y)
{
	g_return_if_fail(self != NULL);

	if (CCM_DRAWABLE_GET_CLASS(self)->move)
	{
		CCM_DRAWABLE_GET_CLASS(self)->move(self, x, y);
	}
}

/**
 * ccm_drawable_resize:
 * @self: #CCMDrawable
 * @width: new width of the drawable
 * @height: new height of the drawable
 *
 * Resize drawable.
 **/
void
ccm_drawable_resize(CCMDrawable* self, int width, int height)
{
	g_return_if_fail(self != NULL);

	if (CCM_DRAWABLE_GET_CLASS(self)->resize)
	{
		CCM_DRAWABLE_GET_CLASS(self)->resize(self, width, height);
	}
}

/**
 * ccm_drawable_flush:
 * @self: #CCMDrawable
 *
 * Flush all pending draw operation on drawable.
 **/
void
ccm_drawable_flush(CCMDrawable* self)
{
	g_return_if_fail(self != NULL);
	
	if (CCM_DRAWABLE_GET_CLASS(self)->flush)
	{
		CCM_DRAWABLE_GET_CLASS(self)->flush(self);
	}
}

/**
 * ccm_drawable_flush_region:
 * @self: #CCMDrawable
 * @region: #CCMRegion
 *
 * Flush all pending draw operation on region of drawable.
 **/
void
ccm_drawable_flush_region(CCMDrawable* self, CCMRegion* region)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(region != NULL);
	
	if (CCM_DRAWABLE_GET_CLASS(self)->flush_region)
	{
		CCM_DRAWABLE_GET_CLASS(self)->flush_region(self, region);
	}
}

/**
 * ccm_drawable_get_surface:
 * @self: #CCMDrawable
 *
 * Gets cairo surface of drawable
 *
 * Returns: #cairo_surface_t
 **/
cairo_surface_t*
ccm_drawable_get_surface(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (CCM_DRAWABLE_GET_CLASS(self)->get_surface)
	{
		return CCM_DRAWABLE_GET_CLASS(self)->get_surface(self);
	}
	return NULL;
}

/**
 * ccm_drawable_create_context:
 * @self: #CCMDrawable
 *
 * Create cairo context for a drawable
 *
 * Returns: #cairo_t
 **/
cairo_t*
ccm_drawable_create_context(CCMDrawable* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	cairo_surface_t* surface = ccm_drawable_get_surface(self);
	
	if (surface)
	{
		cairo_t* ctx = cairo_create(surface);
		cairo_surface_destroy(surface);
		return ctx;
	}
	
	return NULL;
}

/**
 * ccm_drawable_is_damaged:
 * @self: #CCMDrawable
 *
 * Return is drawable is damaged
 *
 * Returns: #gboolean
 **/
gboolean
ccm_drawable_is_damaged(CCMDrawable* self)
{
	g_return_val_if_fail (self != NULL, FALSE);
    
	return self->priv->damaged && !ccm_region_empty(self->priv->damaged);
}

/**
 * ccm_drawable_damage_rectangle:
 * @self: #CCMDrawable
 * @area: #cairo_rectangle_t damaged
 *
 * Add a damaged rectangle for a drawable
 **/
void 
ccm_drawable_damage_rectangle(CCMDrawable* self, cairo_rectangle_t* area)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);
	
	if (self->priv->geometry)
	{
		CCMRegion* new_damage = ccm_region_rectangle(area);
		CCMRegion* damaged = ccm_region_copy(self->priv->geometry);
		
		ccm_region_intersect(damaged, new_damage);
		ccm_region_destroy(new_damage);
		
		if (self->priv->damaged)
		{
			ccm_region_union(self->priv->damaged, damaged);
			ccm_region_destroy(damaged);
		}
		else
			self->priv->damaged = damaged;
		
		g_signal_emit(self, signals[DAMAGED], 0, area);
	}
}

/**
 * ccm_drawable_damage_region:
 * @self: #CCMDrawable
 * @region: #CCMRegion damaged
 *
 * Add a damaged region for a drawable
 **/
void 
ccm_drawable_damage_region(CCMDrawable* self, CCMRegion* area)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);

	cairo_rectangle_t* rectangles;
	gint cpt, nb_rects;
	
	ccm_region_get_rectangles(area, &rectangles, &nb_rects);
	for (cpt = 0; cpt < nb_rects; cpt++)
	{
		ccm_drawable_damage_rectangle(self, &rectangles[cpt]);
	}
	g_free(rectangles);
}

/**
 * ccm_drawable_damage:
 * @self: #CCMDrawable
 *
 * Damage a drawable
 **/
void 
ccm_drawable_damage(CCMDrawable* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->geometry)
		ccm_drawable_damage_region(self, self->priv->geometry);
}

/**
 * ccm_drawable_undamage_region:
 * @self: #CCMDrawable
 @ @region: #Region
 *
 * Remove a part of damaged region of a drawable
 **/
void
ccm_drawable_undamage_region(CCMDrawable* self, CCMRegion* region)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(region != NULL);
					 
	if (self->priv->damaged)
	{
		ccm_region_subtract(self->priv->damaged, region);
		if (ccm_region_empty(self->priv->damaged))
		{
			ccm_region_destroy(self->priv->damaged);
			self->priv->damaged = NULL;
		}
	}
}

/**
 * ccm_drawable_repair:
 * @self: #CCMDrawable
 *
 * Repair all damaged regions of a drawable
 **/
void
ccm_drawable_repair(CCMDrawable* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->damaged)
	{
		if (CCM_DRAWABLE_GET_CLASS(self)->repair)
			CCM_DRAWABLE_GET_CLASS(self)->repair(self, self->priv->damaged);
		ccm_region_destroy(self->priv->damaged);
		self->priv->damaged = NULL;
	}
}

/**
 * ccm_drawable_get_damage_path:
 * @self: #CCMDrawable
 * @context: #cairo_t 
 *
 * Get damaged path
 *
 * Returns: #cairo_path_t
 **/
cairo_path_t*
ccm_drawable_get_damage_path(CCMDrawable* self, cairo_t* context)
{
	g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (context != NULL, NULL);
	
	cairo_path_t* path = NULL;
		
	if (self->priv->damaged && !ccm_region_empty(self->priv->damaged))
	{
		cairo_rectangle_t* rectangles;
		gint nb_rects, cpt;
	
		ccm_region_get_rectangles(self->priv->damaged, &rectangles, &nb_rects);
		for (cpt = 0; cpt < nb_rects; cpt++)
		{
			cairo_rectangle(context, rectangles[cpt].x, rectangles[cpt].y,
							rectangles[cpt].width, rectangles[cpt].height);
		}
		g_free(rectangles);
		path = cairo_copy_path(context);
	}
			
	return path;
}

/**
 * ccm_drawable_get_geometry_path:
 * @self: #CCMDrawable
 * @context: #cairo_t 
 *
 * Get geometry path
 *
 * Returns: #cairo_path_t
 **/
cairo_path_t*
ccm_drawable_get_geometry_path(CCMDrawable* self, cairo_t* context)
{
	g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (context != NULL, NULL);
	
	cairo_path_t* path = NULL;
		
	if (self->priv->geometry)
	{
		cairo_rectangle_t* rectangles;
		gint nb_rects, cpt;
	
		ccm_region_get_rectangles(self->priv->geometry, &rectangles, &nb_rects);
		for (cpt = 0; cpt < nb_rects; cpt++)
		{
			cairo_rectangle(context, rectangles[cpt].x, rectangles[cpt].y,
							rectangles[cpt].width, rectangles[cpt].height);
		}
		g_free(rectangles);
		path = cairo_copy_path(context);
	}
			
	return path;
}
