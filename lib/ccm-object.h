/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-object.h
 * Copyright (C) Nicolas Bruguier 2010 <gandalfn@club-internet.fr>
 * 
 * libcairo-compmgr is free software: you can redistribute it and/or modify it
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

#ifndef __CCM_OBJECT_H__
#define __CCM_OBJECT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_OBJECT            (ccm_object_get_type ())
#define CCM_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_OBJECT, CCMObject))
#define CCM_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  CCM_TYPE_OBJECT, CCMObjectClass))
#define CCM_IS_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_OBJECT))
#define CCM_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  CCM_TYPE_OBJECT))
#define CCM_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  CCM_TYPE_OBJECT, CCMObjectClass))

typedef struct _CCMObjectClass    CCMObjectClass;
typedef struct _CCMObject         CCMObject;

struct _CCMObjectClass
{
    GObjectClass parent_class;
};

struct _CCMObject
{
    GObject parent_instance;
};

CCMObject* ccm_object_construct (GType inObjectType);

gboolean ccm_object_register (GType inObjectType, GType inType);
gboolean ccm_object_unregister (GType inObjectType);

G_END_DECLS

#endif /* __CCM_OBJECT_H__ */
