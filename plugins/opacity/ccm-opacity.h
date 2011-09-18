/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-opacity.h
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

#ifndef _CCM_OPACITY_H_
#define _CCM_OPACITY_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_OPACITY             (ccm_opacity_get_type ())
#define CCM_OPACITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_OPACITY, CCMOpacity))
#define CCM_OPACITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_OPACITY, CCMOpacityClass))
#define CCM_IS_OPACITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_OPACITY))
#define CCM_IS_OPACITY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_OPACITY))
#define CCM_OPACITY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_OPACITY, CCMOpacityClass))

typedef struct _CCMOpacityClass CCMOpacityClass;
typedef struct _CCMOpacity CCMOpacity;

struct _CCMOpacityClass
{
    CCMPluginClass parent_class;
};

typedef struct _CCMOpacityPrivate CCMOpacityPrivate;

struct _CCMOpacity
{
    CCMPlugin parent_instance;

    CCMOpacityPrivate *priv;
};

GType ccm_opacity_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif                          /* _CCM_OPACITY_H_ */
