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

#ifndef _CCM_MOSAIC_H_
#define _CCM_MOSAIC_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"
#include "ccm-screen-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_MOSAIC              (ccm_mosaic_get_type ())
#define CCM_MOSAIC(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_MOSAIC, CCMMosaic))
#define CCM_MOSAIC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_MOSAIC, CCMMosaicClass))
#define CCM_IS_MOSAIC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_MOSAIC))
#define CCM_IS_MOSAIC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_MOSAIC))
#define CCM_MOSAIC_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_MOSAIC, CCMMosaicClass))

typedef struct _CCMMosaicClass CCMMosaicClass;
typedef struct _CCMMosaic CCMMosaic;

struct _CCMMosaicClass
{
	CCMPluginClass parent_class;
};

typedef struct _CCMMosaicPrivate CCMMosaicPrivate;

struct _CCMMosaic
{
	CCMPlugin parent_instance;
	
	CCMMosaicPrivate* priv;
};

GType ccm_mosaic_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CCM_MOSAIC_H_ */
