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

#ifndef _CCM_PROPERTY_ASYNC_H_
#define _CCM_PROPERTY_ASYNC_H_

#include <glib-object.h>

#include "ccm-display.h"
#include "ccm-window.h"

G_BEGIN_DECLS

#define CCM_TYPE_PROPERTY_ASYNC             (ccm_property_async_get_type ())
#define CCM_PROPERTY_ASYNC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_PROPERTY_ASYNC, CCMPropertyASync))
#define CCM_PROPERTY_ASYNC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_PROPERTY_ASYNC, CCMPropertyASyncClass))
#define CCM_IS_PROPERTY_ASYNC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PROPERTY_ASYNC))
#define CCM_IS_PROPERTY_ASYNC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_PROPERTY_ASYNC))
#define CCM_PROPERTY_ASYNC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_PROPERTY_ASYNC, CCMPropertyASyncClass))

typedef struct _CCMPropertyASyncClass CCMPropertyASyncClass;
typedef struct _CCMPropertyASyncPrivate CCMPropertyASyncPrivate;
typedef struct _CCMPropertyASync CCMPropertyASync;

struct _CCMPropertyASyncClass
{
    GObjectClass parent_class;
};

struct _CCMPropertyASync
{
    GObject parent_instance;

    CCMPropertyASyncPrivate *priv;
};

GType ccm_property_async_get_type (void) G_GNUC_CONST;
CCMPropertyASync* ccm_property_async_new (CCMDisplay* display, Window window, 
                                          Atom property, Atom req_type, 
                                          long length);
Atom ccm_property_async_get_property (CCMPropertyASync * self);

G_END_DECLS

#endif                          /* _CCM_PROPERTY_ASYNC_H_ */
