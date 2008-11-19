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

#ifndef _CCM_SHADOW_H_
#define _CCM_SHADOW_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_SHADOW             (ccm_shadow_get_type ())
#define CCM_SHADOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_SHADOW, CCMShadow))
#define CCM_SHADOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_SHADOW, CCMShadowClass))
#define CCM_IS_SHADOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_SHADOW))
#define CCM_IS_SHADOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_SHADOW))
#define CCM_SHADOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_SHADOW, CCMShadowClass))

typedef struct _CCMShadowClass CCMShadowClass;
typedef struct _CCMShadow CCMShadow;

struct _CCMShadowClass
{
	CCMPluginClass parent_class;
	
	Atom		   shadow_atom;
};

typedef struct _CCMShadowPrivate CCMShadowPrivate;

struct _CCMShadow
{
	CCMPlugin parent_instance;
	
	CCMShadowPrivate* priv;
};

GType ccm_shadow_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CCM_SHADOW_H_ */
