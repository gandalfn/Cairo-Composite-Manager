/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/extensions/Xfixes.h>

#include "ccm-debug.h"
#include "ccm-display.h"
#include "ccm-cursor.h"

enum
{
    PROP_0,
    PROP_X_HOT,
    PROP_Y_HOT,
    PROP_ANIMATED
};

G_DEFINE_TYPE (CCMCursor, ccm_cursor, G_TYPE_OBJECT);

struct _CCMCursorPrivate
{
    cairo_surface_t *surface;
    int x_hot;
    int y_hot;
    gboolean animated;
};

#define CCM_CURSOR_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CURSOR, CCMCursorPrivate))


static void
ccm_cursor_init (CCMCursor * self)
{
    self->priv = CCM_CURSOR_GET_PRIVATE (self);
    self->priv->surface = NULL;
    self->priv->x_hot = 0;
    self->priv->y_hot = 0;
    self->priv->animated = FALSE;
}

static void
ccm_cursor_finalize (GObject * object)
{
    CCMCursor *self = CCM_CURSOR (object);

    if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
    self->priv->surface = NULL;
    self->priv->x_hot = 0;
    self->priv->y_hot = 0;

    G_OBJECT_CLASS (ccm_cursor_parent_class)->finalize (object);
}

static void
ccm_cursor_set_property (GObject * object, guint prop_id, const GValue * value,
                         GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CURSOR (object));

    CCMCursor *self = CCM_CURSOR (object);

    switch (prop_id)
    {
        case PROP_X_HOT:
            self->priv->x_hot = g_value_get_int (value);
            break;
        case PROP_Y_HOT:
            self->priv->y_hot = g_value_get_int (value);
            break;
        case PROP_ANIMATED:
            self->priv->animated = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
ccm_cursor_get_property (GObject * object, guint prop_id, GValue * value,
                         GParamSpec * pspec)
{
    g_return_if_fail (CCM_IS_CURSOR (object));

    CCMCursor *self = CCM_CURSOR (object);

    switch (prop_id)
    {
        case PROP_X_HOT:
            g_value_set_int (value, self->priv->x_hot);
            break;
        case PROP_Y_HOT:
            g_value_set_int (value, self->priv->y_hot);
            break;
        case PROP_ANIMATED:
            g_value_set_boolean (value, self->priv->animated);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
ccm_cursor_class_init (CCMCursorClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMCursorPrivate));

    object_class->finalize = ccm_cursor_finalize;
    object_class->set_property = ccm_cursor_set_property;
    object_class->get_property = ccm_cursor_get_property;

    g_object_class_install_property (object_class, PROP_X_HOT,
                                     g_param_spec_int ("x-hot", "XHot", "XHot",
                                                       0, G_MAXINT, 0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_Y_HOT,
                                     g_param_spec_int ("y-hot", "XHot", "YHot",
                                                       0, G_MAXINT, 0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_ANIMATED,
                                     g_param_spec_boolean ("animated",
                                                           "Animated",
                                                           "Animated", FALSE,
                                                           G_PARAM_READWRITE));
}


CCMCursor *
ccm_cursor_new (CCMDisplay * display, XFixesCursorImage * cursor)
{
    g_return_val_if_fail (display != NULL, NULL);
    g_return_val_if_fail (cursor != NULL, NULL);

    CCMCursor *self = NULL;
    static cairo_user_data_key_t data_key;
    guchar *image = NULL;
    gint cpt;

    self = g_object_new (CCM_TYPE_CURSOR, "x-hot", cursor->xhot, "y-hot",
                         cursor->yhot, "animated", !cursor->atom, NULL);

    image = g_new0 (guchar, cursor->width * cursor->height * 4);
    for (cpt = 0; cpt < cursor->width * cursor->height; ++cpt)
    {
        guint32 pixval = GUINT32_TO_LE (cursor->pixels[cpt]);
        image[(cpt * 4) + 3] = (guchar) (pixval >> 24);
        image[(cpt * 4) + 2] = (guchar) ((pixval >> 16) & 0xff);
        image[(cpt * 4) + 1] = (guchar) ((pixval >> 8) & 0xff);
        image[(cpt * 4) + 0] = (guchar) (pixval & 0xff);
    }
    self->priv->surface = cairo_image_surface_create_for_data (image, CAIRO_FORMAT_ARGB32,
                                                               cursor->width, cursor->height,
                                                               cursor->width * 4);
    cairo_surface_set_user_data (self->priv->surface, &data_key, image, g_free);

    return self;
}

double
ccm_cursor_get_width (CCMCursor * self)
{
    g_return_val_if_fail (self != NULL, 0);

    return self->priv->surface ? cairo_image_surface_get_width (self->priv->surface) : 0;
}

double
ccm_cursor_get_height (CCMCursor * self)
{
    g_return_val_if_fail (self != NULL, 0);

    return self->priv->surface ? cairo_image_surface_get_height (self->priv->surface) : 0;
}

void
ccm_cursor_paint (CCMCursor * self, cairo_t * ctx, double x, double y)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (ctx != NULL);

    cairo_set_source_surface (ctx, 
                              self->priv->surface, x - self->priv->x_hot,
                              y - self->priv->y_hot);
    cairo_paint (ctx);
}
