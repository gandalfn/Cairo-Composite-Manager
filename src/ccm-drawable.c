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
#include <string.h>

#include "ccm-debug.h"
#include "ccm-drawable.h"
#include "ccm-display.h"
#include "ccm-object.h"

#define CCM_TYPE_DRAWABLE_MATRIX             (ccm_drawable_matrix_get_type ())

static cairo_matrix_t *
ccm_drawable_matrix_copy (cairo_matrix_t * matrix)
{
    return g_memdup (matrix, sizeof (cairo_matrix_t));
}

static void
ccm_drawable_matrix_free (cairo_matrix_t * matrix)
{
    g_free (matrix);
}

GType
ccm_drawable_matrix_get_type (void)
{
    static GType type = 0;
    if (type == 0)
    {
        type =
            g_boxed_type_register_static ("CCMDrawableMatrix",
                                          (GBoxedCopyFunc)
                                          ccm_drawable_matrix_copy,
                                          (GBoxedFreeFunc)
                                          ccm_drawable_matrix_free);
    }
    return type;
}

enum
{
    PROP_0,
    PROP_SCREEN,
    PROP_XDRAWABLE,
    PROP_GEOMETRY,
    PROP_DEPTH,
    PROP_VISUAL,
    PROP_DAMAGED,
    PROP_TRANSFORM
};

enum
{
    DAMAGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

CCM_DEFINE_TYPE (CCMDrawable, ccm_drawable, G_TYPE_OBJECT);

struct _CCMDrawablePrivate
{
    Drawable drawable;
    CCMScreen *screen;
    CCMRegion *device;
    CCMRegion *geometry;
    guint depth;
    Visual *visual;

    cairo_surface_t *surface;
    cairo_rectangle_t last_pos_size;

    CCMRegion *damaged;
    GData *transform;
};

#define CCM_DRAWABLE_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DRAWABLE, CCMDrawablePrivate))

static void __ccm_drawable_query_geometry (CCMDrawable * self);
static void __ccm_drawable_move (CCMDrawable * self, int x, int y);
static void __ccm_drawable_resize (CCMDrawable * self, int width, int height);

static void
ccm_drawable_set_property (GObject * object, guint prop_id,
                           const GValue * value, GParamSpec * pspec)
{
    CCMDrawablePrivate *priv = CCM_DRAWABLE_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_SCREEN:
            priv->screen = g_value_get_pointer (value);
            break;
        case PROP_XDRAWABLE:
            priv->drawable = g_value_get_ulong (value);
            break;
        case PROP_GEOMETRY:
            if (priv->device)
                ccm_region_destroy (priv->device);
            if (priv->geometry)
                ccm_region_destroy (priv->geometry);
            priv->device = NULL;
            priv->geometry = NULL;

            if (g_value_get_pointer (value)
                && !ccm_region_empty (g_value_get_pointer (value)))
            {
                cairo_matrix_t transform;

                transform = ccm_drawable_get_transform (CCM_DRAWABLE (object));
                priv->device = ccm_region_copy (g_value_get_pointer (value));
                priv->geometry = ccm_region_copy (g_value_get_pointer (value));
                ccm_region_device_transform (priv->geometry, &transform);
            }
            break;
        case PROP_DEPTH:
            priv->depth = g_value_get_uint (value);
            break;
        case PROP_VISUAL:
            priv->visual = g_value_get_pointer (value);
            break;
        default:
            break;
    }
}

static void
ccm_drawable_get_property (GObject * object, guint prop_id, GValue * value,
                           GParamSpec * pspec)
{
    CCMDrawablePrivate *priv = CCM_DRAWABLE_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_SCREEN:
            g_value_set_pointer (value, priv->screen);
            break;
        case PROP_XDRAWABLE:
            g_value_set_ulong (value, priv->drawable);
            break;
        case PROP_DEPTH:
            g_value_set_uint (value, priv->depth);
            break;
        case PROP_VISUAL:
            g_value_set_pointer (value, priv->visual);
            break;
        case PROP_GEOMETRY:
            g_value_set_pointer (value, priv->geometry);
            break;
        case PROP_DAMAGED:
            g_value_set_pointer (value, priv->damaged);
            break;
        case PROP_TRANSFORM:
            {
                cairo_matrix_t transform;

                transform = ccm_drawable_get_transform (CCM_DRAWABLE (object));
                g_value_set_boxed (value, &transform);
            }
            break;
        default:
            break;
    }
}

static void
ccm_drawable_init (CCMDrawable * self)
{
    cairo_matrix_t init;
    self->priv = CCM_DRAWABLE_GET_PRIVATE (self);
    self->priv->drawable = 0;
    self->priv->device = NULL;
    self->priv->geometry = NULL;
    self->priv->depth = 0;
    self->priv->visual = NULL;
    self->priv->surface = NULL;
    self->priv->damaged = NULL;
    self->priv->last_pos_size.x = 0;
    self->priv->last_pos_size.y = 0;
    self->priv->last_pos_size.width = 0;
    self->priv->last_pos_size.height = 0;
    self->priv->transform = NULL;
    g_datalist_init (&self->priv->transform);
    cairo_matrix_init_identity (&init);
    ccm_drawable_push_matrix (self, "CCMDrawable", &init);
}

static void
ccm_drawable_finalize (GObject * object)
{
    CCMDrawable *self = CCM_DRAWABLE (object);

    if (self->priv->surface)
    {
        cairo_surface_destroy (self->priv->surface);
        self->priv->surface = NULL;
    }
    self->priv->drawable = 0;
    self->priv->depth = 0;
    self->priv->visual = NULL;
    if (self->priv->device)
    {
        ccm_region_destroy (self->priv->device);
        self->priv->device = NULL;
    }
    if (self->priv->geometry)
    {
        ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = NULL;
    }
    if (self->priv->damaged)
    {
        ccm_region_destroy (self->priv->damaged);
        self->priv->damaged = NULL;
    }
    if (self->priv->transform)
    {
        g_datalist_clear (&self->priv->transform);
        g_datalist_init (&self->priv->transform);
    }

    G_OBJECT_CLASS (ccm_drawable_parent_class)->finalize (object);
}

static void
ccm_drawable_class_init (CCMDrawableClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMDrawablePrivate));

    object_class->get_property = ccm_drawable_get_property;
    object_class->set_property = ccm_drawable_set_property;
    object_class->finalize = ccm_drawable_finalize;

    klass->query_geometry = __ccm_drawable_query_geometry;
    klass->move = __ccm_drawable_move;
    klass->resize = __ccm_drawable_resize;

        /**
	 * CCMDrawable:screen:
	 *
	 * Screen of the drawable.
	 */
    g_object_class_install_property (object_class, PROP_SCREEN,
                                     g_param_spec_pointer ("screen", "Screen",
                                                           "Screen of the drawable",
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT_ONLY));

    /**
	 * CCMDrawable:drawable:
	 *
	 * The X resource (window or pixmap) belonging to a Drawable.
	 */
    g_object_class_install_property (object_class, PROP_XDRAWABLE,
                                     g_param_spec_ulong ("drawable", "Drawable",
                                                         "Xid of the drawable",
                                                         0, G_MAXLONG, None,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

        /**
	 * CCMDrawable:geometry:
	 *
	 * Geometry of the drawable, if the drawable have transformation returns
	 * the transformed geometry.
	 */
    g_object_class_install_property (object_class, PROP_GEOMETRY,
                                     g_param_spec_pointer ("geometry",
                                                           "Geometry",
                                                           "Geometry of the drawable",
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT));

        /**
	 * CCMDrawable:visual:
	 *
	 * Visual of the drawable.
	 */
    g_object_class_install_property (object_class, PROP_VISUAL,
                                     g_param_spec_pointer ("visual", "Visual",
                                                           "Visual of the drawable",
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT));

        /**
	 * CCMDrawable:depth:
	 *
	 * Depth of the drawable.
	 */
    g_object_class_install_property (object_class, PROP_DEPTH,
                                     g_param_spec_uint ("depth", "Depth",
                                                        "Depth of the drawable",
                                                        0, G_MAXUINT, None,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));

        /**
	 * CCMDrawable:geometry:
	 *
	 * Damaged region of the drawable
	 */
    g_object_class_install_property (object_class, PROP_DAMAGED,
                                     g_param_spec_pointer ("damaged", "Damaged",
                                                           "Damaged region of the drawable",
                                                           G_PARAM_READABLE));

        /**
	 * CCMDrawable:transform:
	 *
	 * The cumulated #cairo_matrix_t transforms of the drawable
	 */
    g_object_class_install_property (object_class, PROP_TRANSFORM,
                                     g_param_spec_boxed ("transform",
                                                         "Transform",
                                                         "Tranform matrix of the drawable",
                                                         CCM_TYPE_DRAWABLE_MATRIX,
                                                         G_PARAM_READABLE));

        /**
	 * CCMDrawable::damaged:
	 * @self: #CCMDrawable
	 * @damaged: damaged #CCMRegion
	 *
	 * Emitted when a #CCMRegion of the drawable is damaged.
	 */
    signals[DAMAGED] =
        g_signal_new ("damaged", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,
                      G_TYPE_POINTER);
}

static void
__ccm_drawable_query_geometry (CCMDrawable * self)
{
    g_return_if_fail (self != NULL);

    CCMDisplay *display = ccm_screen_get_display (self->priv->screen);
    Window root;                /* dummy */
    guint bw;                   /* dummies */
    gint x, y;
    guint width, height;
    cairo_rectangle_t rectangle;
    cairo_matrix_t matrix = ccm_drawable_get_transform (self);

    if (self->priv->device)
        ccm_region_destroy (self->priv->device);
    self->priv->device = NULL;
    if (self->priv->geometry)
        ccm_region_destroy (self->priv->geometry);
    self->priv->geometry = NULL;

    if (!XGetGeometry
        (CCM_DISPLAY_XDISPLAY (display), self->priv->drawable, &root, &x, &y,
         &width, &height, &bw, &self->priv->depth))
        return;

    rectangle.x = (double) x;
    rectangle.y = (double) y;
    rectangle.width = (double) width;
    rectangle.height = (double) height;

    self->priv->device = ccm_region_rectangle (&rectangle);
    self->priv->geometry = ccm_region_rectangle (&rectangle);
    ccm_region_device_transform (self->priv->geometry, &matrix);
}

static void
__ccm_drawable_move (CCMDrawable * self, int x, int y)
{
    g_return_if_fail (self != NULL);

    cairo_rectangle_t geometry;

    if (self->priv->geometry)
    {
        cairo_matrix_t matrix = ccm_drawable_get_transform (self);

        ccm_region_get_clipbox (self->priv->device, &geometry);
        ccm_region_offset (self->priv->device, x - (int) geometry.x,
                           y - (int) geometry.y);
        ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = ccm_region_copy (self->priv->device);
        ccm_region_transform (self->priv->geometry, &matrix);
    }
}

static void
__ccm_drawable_resize (CCMDrawable * self, int width, int height)
{
    g_return_if_fail (self != NULL);

    if (self->priv->device)
    {
        cairo_matrix_t matrix = ccm_drawable_get_transform (self);

        ccm_region_resize (self->priv->device, width, height);
        ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = ccm_region_copy (self->priv->device);
        ccm_region_transform (self->priv->geometry, &matrix);
    }
}

static void
ccm_drawable_foreach_transform (GQuark key, cairo_matrix_t * matrix,
                                cairo_matrix_t * transform)
{
    cairo_matrix_multiply (transform, transform, matrix);
}

/**
 * ccm_drawable_get_screen:
 * @self: #CCMDrawable
 *
 * Returns the #CCMScreen associated with drawable.
 *
 * Returns: #CCMScreen
 **/
CCMScreen *
ccm_drawable_get_screen (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

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
CCMDisplay *
ccm_drawable_get_display (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->screen ? ccm_screen_get_display (self->priv->
                                                        screen) : NULL;
}

/**
 * ccm_drawable_get_visual:
 * @self: #CCMDrawable
 *
 * Return visual of drawable
 *
 * Returns: Visual
 **/
Visual *
ccm_drawable_get_visual (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->visual;
}

/**
 * ccm_drawable_get_depth:
 * @self: #CCMDrawable
 *
 * Get depth of drawable.
 *
 * Returns: drawable depth
 **/
guint
ccm_drawable_get_depth (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, 32);

    return self->priv->depth;
}

/**
 * ccm_drawable_get_format:
 * @self: #CCMDrawable
 *
 * Get cairo format of drawable.
 *
 * Returns: #cairo_format_t
 **/
cairo_format_t
ccm_drawable_get_format (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, CAIRO_FORMAT_ARGB32);
    g_return_val_if_fail (self->priv->visual != NULL, CAIRO_FORMAT_ARGB32);

    if (self->priv->depth == 16 && self->priv->visual->red_mask == 0xf800
        && self->priv->visual->green_mask == 0x7e0
        && self->priv->visual->blue_mask == 0x1f)
    {
        return CAIRO_FORMAT_A8;
    }
    else if ((self->priv->depth == 24 || self->priv->depth == 0)
             && self->priv->visual->red_mask == 0xff0000
             && self->priv->visual->green_mask == 0xff00
             && self->priv->visual->blue_mask == 0xff)
    {
        return CAIRO_FORMAT_RGB24;
    }
    else if (self->priv->depth == 32 && self->priv->visual->red_mask == 0xff0000
             && self->priv->visual->green_mask == 0xff00
             && self->priv->visual->blue_mask == 0xff)
    {
        return CAIRO_FORMAT_ARGB32;
    }
    else
    {
        g_warning ("Unknown visual format depth=%d, r=%#lx/g=%#lx/b=%#lx",
                   self->priv->depth, self->priv->visual->red_mask,
                   self->priv->visual->green_mask,
                   self->priv->visual->blue_mask);
    }

    return CAIRO_FORMAT_ARGB32;
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
ccm_drawable_get_xid (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, 0);

    return self->priv->drawable;
}

/**
 * ccm_drawable_query_geometry:
 * @self: #CCMDrawable
 *
 * Request the region covered by the Drawable. 
 **/
void
ccm_drawable_query_geometry (CCMDrawable * self)
{
    g_return_if_fail (self != NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->query_geometry)
    {
        CCM_DRAWABLE_GET_CLASS (self)->query_geometry (self);
    }
}

/**
 * ccm_drawable_get_geometry:
 * @self: #CCMDrawable
 *
 * Gets the region covered by the Drawable. The coordinates are 
 * relative to the parent Drawable. If the drawable have some transformations
 * this function return the transformed region
 *
 * Returns: const #CCMRegion
 **/
const CCMRegion *
ccm_drawable_get_geometry (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->geometry;
}

/**
 * ccm_drawable_get_device_geometry:
 * @self: #CCMDrawable
 *
 * Gets the region covered by the Drawable. The coordinates are 
 * relative to the parent Drawable. This function always return the non
 * transformed geometry.
 *
 * Returns: const #CCMRegion
 **/
const CCMRegion *
ccm_drawable_get_device_geometry (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->device;
}

/**
 * ccm_drawable_get_geometry_clipbox:
 * @self: #CCMDrawable
 * @area: #cairo_rectangle_t
 *
 * Gets the rectangle region covered by the Drawable. The coordinates are 
 * relative to the parent Drawable. If the drawable have some transformations
 * this function return the transformed clipbox.
 *
 * Returns: FALSE if fail
 **/
gboolean
ccm_drawable_get_geometry_clipbox (CCMDrawable * self, cairo_rectangle_t * area)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (area != NULL, FALSE);

    if (self->priv->geometry && !ccm_region_empty(self->priv->geometry))
    {
        ccm_region_get_clipbox (self->priv->geometry, area);
        return TRUE;
    }

    return FALSE;
}

/**
 * ccm_drawable_get_device_geometry_clipbox:
 * @self: #CCMDrawable
 * @area: #cairo_rectangle_t
 *
 * Gets the rectangle region covered by the Drawable. The coordinates are 
 * relative to the parent Drawable. This function always return the non
 * transformed clipbox.
 *
 * Returns: FALSE if fail
 **/
gboolean
ccm_drawable_get_device_geometry_clipbox (CCMDrawable * self,
                                          cairo_rectangle_t * area)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (area != NULL, FALSE);

    if (self->priv->device)
    {
        ccm_region_get_clipbox (self->priv->device, area);
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
ccm_drawable_move (CCMDrawable * self, int x, int y)
{
    g_return_if_fail (self != NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->move
        && (self->priv->last_pos_size.x != x
            || self->priv->last_pos_size.y != y))
    {
        CCM_DRAWABLE_GET_CLASS (self)->move (self, x, y);
        self->priv->last_pos_size.x = x;
        self->priv->last_pos_size.y = y;
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
ccm_drawable_resize (CCMDrawable * self, int width, int height)
{
    g_return_if_fail (self != NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->resize
        && (self->priv->last_pos_size.width != width
            || self->priv->last_pos_size.height != height))
    {
        CCM_DRAWABLE_GET_CLASS (self)->resize (self, width, height);
        self->priv->last_pos_size.width = width;
        self->priv->last_pos_size.height = height;
    }
}

/**
 * ccm_drawable_flush:
 * @self: #CCMDrawable
 *
 * Flush all pending draw operation on drawable.
 **/
void
ccm_drawable_flush (CCMDrawable * self)
{
    g_return_if_fail (self != NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->flush)
    {
        CCM_DRAWABLE_GET_CLASS (self)->flush (self);
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
ccm_drawable_flush_region (CCMDrawable * self, CCMRegion * region)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (region != NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->flush_region)
    {
        CCM_DRAWABLE_GET_CLASS (self)->flush_region (self, region);
    }
}

/**
 * ccm_drawable_is_damaged:
 * @self: #CCMDrawable
 *
 * Return if drawable is damaged
 *
 * Returns: #gboolean
 **/
gboolean
ccm_drawable_is_damaged (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->damaged && !ccm_region_empty (self->priv->damaged);
}

/**
 * ccm_drawable_damage_region:
 * @self: #CCMDrawable
 * @region: #CCMRegion damaged
 *
 * Add a damaged region for a drawable
 **/
void
ccm_drawable_damage_region (CCMDrawable * self, const CCMRegion * area)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);

    if (!ccm_region_empty ((CCMRegion *) area))
    {
        ccm_drawable_damage_region_silently (self, area);
        g_signal_emit (self, signals[DAMAGED], 0, area);
    }
}

/**
 * ccm_drawable_damage_region_silently:
 * @self: #CCMDrawable
 * @region: #CCMRegion damaged
 *
 * Add a damaged region for a drawable without generate an event
 **/
void
ccm_drawable_damage_region_silently (CCMDrawable * self, const CCMRegion * area)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);

    if (!ccm_region_empty ((CCMRegion *) area))
    {
        if (self->priv->damaged)
            ccm_region_union (self->priv->damaged, (CCMRegion *) area);
        else
            self->priv->damaged = ccm_region_copy ((CCMRegion *) area);

        if (self->priv->geometry)
            ccm_region_intersect (self->priv->damaged, self->priv->geometry);
        if (!self->priv->geometry || ccm_region_empty (self->priv->damaged))
        {
            ccm_region_destroy (self->priv->damaged);
            self->priv->damaged = NULL;
        }
        ccm_debug_region (self, "DAMAGE_REGION:");
    }
}

/**
 * ccm_drawable_damage:
 * @self: #CCMDrawable
 *
 * Damage a drawable
 **/
void
ccm_drawable_damage (CCMDrawable * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->geometry)
        ccm_drawable_damage_region (self, self->priv->geometry);
}

/**
 * ccm_drawable_undamage_region:
 * @self: #CCMDrawable
 * @region: #CCMRegion
 *
 * Remove a part of damaged region of a drawable
 **/
void
ccm_drawable_undamage_region (CCMDrawable * self, CCMRegion * region)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (region != NULL);

    if (self->priv->damaged)
    {
        ccm_region_subtract (self->priv->damaged, region);
        if (ccm_region_empty (self->priv->damaged))
        {
            ccm_region_destroy (self->priv->damaged);
            self->priv->damaged = NULL;
        }
        ccm_debug_region (self, "UNDAMAGE_REGION:");
    }
}

/**
 * ccm_drawable_repair:
 * @self: #CCMDrawable
 *
 * Repair all damaged regions of a drawable
 **/
void
ccm_drawable_repair (CCMDrawable * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->damaged)
    {
        gboolean ret = TRUE;

        if (CCM_DRAWABLE_GET_CLASS (self)->repair)
            ret =
                CCM_DRAWABLE_GET_CLASS (self)->repair (self,
                                                       self->priv->damaged);
        if (ret)
        {
            ccm_debug_region (self, "REPAIR");
            ccm_region_destroy (self->priv->damaged);
            self->priv->damaged = NULL;
        }
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
cairo_surface_t *
ccm_drawable_get_surface (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->get_surface)
    {
        return CCM_DRAWABLE_GET_CLASS (self)->get_surface (self);
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
cairo_t *
ccm_drawable_create_context (CCMDrawable * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    if (CCM_DRAWABLE_GET_CLASS (self)->create_context)
    {
        return CCM_DRAWABLE_GET_CLASS (self)->create_context (self);
    }
    else
    {
        cairo_surface_t *surface = ccm_drawable_get_surface (self);

        if (surface)
        {
            cairo_t *ctx = cairo_create (surface);
            cairo_surface_destroy (surface);
            return ctx;
        }
    }

    return NULL;
}

/**
 * ccm_drawable_get_damage_path:
 * @self: #CCMDrawable
 * @context: #cairo_t 
 *
 * Get damaged path.
 **/
void
ccm_drawable_get_damage_path (CCMDrawable * self, cairo_t * context)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (context != NULL);

    ccm_debug_region (self, "GET_DAMAGE_PATH");
    if (self->priv->damaged && !ccm_region_empty (self->priv->damaged))
    {
        cairo_rectangle_t *rectangles;
        gint nb_rects, cpt;

        ccm_region_get_rectangles (self->priv->damaged, &rectangles, &nb_rects);
        for (cpt = 0; cpt < nb_rects; ++cpt)
        {
            cairo_rectangle (context, rectangles[cpt].x, rectangles[cpt].y,
                             rectangles[cpt].width, rectangles[cpt].height);
        }
        cairo_rectangles_free (rectangles, nb_rects);
    }
}

/**
 * ccm_drawable_get_geometry_path:
 * @self: #CCMDrawable
 * @context: #cairo_t 
 *
 * Get geometry path.
 *
 * Returns: #cairo_path_t
 **/
cairo_path_t *
ccm_drawable_get_geometry_path (CCMDrawable * self, cairo_t * context)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (context != NULL, NULL);

    cairo_path_t *path = NULL;

    if (self->priv->geometry)
    {
        cairo_rectangle_t *rectangles;
        gint nb_rects, cpt;

        ccm_region_get_rectangles (self->priv->geometry, &rectangles,
                                   &nb_rects);
        for (cpt = 0; cpt < nb_rects; ++cpt)
        {
            cairo_rectangle (context, rectangles[cpt].x, rectangles[cpt].y,
                             rectangles[cpt].width, rectangles[cpt].height);
        }
        cairo_rectangles_free (rectangles, nb_rects);
        path = cairo_copy_path (context);
    }

    return path;
}

/**
 * ccm_drawable_push_matrix:
 * @self: #CCMDrawable
 * @key: matrix key
 * @matrix: #cairo_matrix_t
 *
 * Push a matrix in #CCMDrawable transform stack with @key if a matrix already
 * exist for this key it was replaced.
 **/
void
ccm_drawable_push_matrix (CCMDrawable * self, gchar * key,
                          cairo_matrix_t * matrix)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (key != NULL);
    g_return_if_fail (matrix != NULL);

    cairo_rectangle_t clipbox;
    cairo_matrix_t transform = ccm_drawable_get_transform (self);

    if (self->priv->device)
    {
        ccm_region_get_clipbox (self->priv->device, &clipbox);
        ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = NULL;
        if (self->priv->damaged)
        {
            ccm_region_offset (self->priv->damaged, -clipbox.x, -clipbox.y);
            if (ccm_region_transform_invert (self->priv->damaged, &transform))
            {
                ccm_region_offset (self->priv->damaged, clipbox.x, clipbox.y);
            }
            else
            {
                ccm_region_destroy (self->priv->damaged);
                self->priv->damaged = ccm_region_copy (self->priv->device);
            }
        }
    }
    g_datalist_set_data_full (&self->priv->transform, key,
                              g_memdup (matrix, sizeof (cairo_matrix_t)),
                              g_free);

    transform = ccm_drawable_get_transform (self);
    if (self->priv->device)
    {
        ccm_region_get_clipbox (self->priv->device, &clipbox);
        self->priv->geometry = ccm_region_copy (self->priv->device);
        ccm_region_device_transform (self->priv->geometry, &transform);
        if (self->priv->damaged)
        {
            ccm_region_offset (self->priv->damaged, -clipbox.x, -clipbox.y);
            ccm_region_transform (self->priv->damaged, &transform);
            ccm_region_offset (self->priv->damaged, clipbox.x, clipbox.y);
        }
    }

    g_object_notify (G_OBJECT (self), "transform");
}

/**
 * ccm_drawable_pop_matrix:
 * @self: #CCMDrawable
 * @key: matrix key
 *
 * Pop a matrix associated with @key from #CCMDrawable transform stack.
 **/
void
ccm_drawable_pop_matrix (CCMDrawable * self, gchar * key)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (key != NULL);

    cairo_rectangle_t clipbox;
    cairo_matrix_t transform = ccm_drawable_get_transform (self);

    if (self->priv->device)
    {
        ccm_region_get_clipbox (self->priv->device, &clipbox);
        ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = NULL;
        if (self->priv->damaged)
        {
            ccm_region_offset (self->priv->damaged, -clipbox.x, -clipbox.y);
            if (ccm_region_transform_invert (self->priv->damaged, &transform))
            {
                ccm_region_offset (self->priv->damaged, clipbox.x, clipbox.y);
            }
            else
            {
                ccm_region_destroy (self->priv->damaged);
                self->priv->damaged = ccm_region_copy (self->priv->device);
            }
        }
    }

    g_datalist_remove_data (&self->priv->transform, key);

    transform = ccm_drawable_get_transform (self);
    if (self->priv->device)
    {
        ccm_region_get_clipbox (self->priv->device, &clipbox);
        self->priv->geometry = ccm_region_copy (self->priv->device);
        ccm_region_device_transform (self->priv->geometry, &transform);
        if (self->priv->damaged)
        {
            ccm_region_offset (self->priv->damaged, -clipbox.x, -clipbox.y);
            ccm_region_transform (self->priv->damaged, &transform);
            ccm_region_offset (self->priv->damaged, clipbox.x, clipbox.y);
        }
    }

    g_object_notify (G_OBJECT (self), "transform");
}

/**
 * ccm_drawable_get_transform:
 * self:  #CCMDrawable
 *
 * Get the current #cairo_matrix_t transform
 *
 * return: #cairo_matrix_t
 **/
cairo_matrix_t
ccm_drawable_get_transform (CCMDrawable * self)
{
    cairo_matrix_t matrix;

    cairo_matrix_init_identity (&matrix);

    g_return_val_if_fail (self != NULL, matrix);

    g_datalist_foreach (&self->priv->transform,
                        (GDataForeachFunc) ccm_drawable_foreach_transform,
                        &matrix);

    return matrix;
}
