/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-drawable.h
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
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

#ifndef _CCM_DRAWABLE_H_
#define _CCM_DRAWABLE_H_

#include <cairo.h>
#include <X11/Xlib.h>
#include <glib-object.h>

#include "ccm.h"
#include "ccm-object.h"

G_BEGIN_DECLS

#define CCM_TYPE_DRAWABLE             (ccm_drawable_get_type ())
#define CCM_TYPE_DRAWABLE_MATRIX      (ccm_drawable_matrix_get_type ())
#define CCM_DRAWABLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_DRAWABLE, CCMDrawable))
#define CCM_DRAWABLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_DRAWABLE, CCMDrawableClass))
#define CCM_IS_DRAWABLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_DRAWABLE))
#define CCM_IS_DRAWABLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_DRAWABLE))
#define CCM_DRAWABLE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_DRAWABLE, CCMDrawableClass))

struct _CCMDrawableClass
{
    CCMObjectClass parent_class;

    cairo_t*         (*create_context) (CCMDrawable * self);
    cairo_surface_t* (*get_surface)    (CCMDrawable * self);
    void             (*query_geometry) (CCMDrawable * self);
    void             (*move)           (CCMDrawable * self, int x, int y);
    void             (*resize)         (CCMDrawable * self, int width, int height);
    gboolean         (*repair)         (CCMDrawable * self, CCMRegion * damaged);
    void             (*flush)          (CCMDrawable * self);
    void             (*flush_region)   (CCMDrawable * self, CCMRegion * region);
};

typedef struct _CCMDrawablePrivate CCMDrawablePrivate;

struct _CCMDrawable
{
    CCMObject parent_instance;

    CCMDrawablePrivate *priv;
};

GType ccm_drawable_get_type        (void) G_GNUC_CONST;
GType ccm_drawable_matrix_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif                          /* _CCM_DRAWABLE_H_ */
