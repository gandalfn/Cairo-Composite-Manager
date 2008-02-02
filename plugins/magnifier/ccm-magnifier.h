/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 				 Carlos Diógenes  2007 <cerdiogenes@gmail.com>
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

#ifndef _CCM_MAGNIFIER_H_
#define _CCM_MAGNIFIER_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"
#include "ccm-screen-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_MAGNIFIER              (ccm_magnifier_get_type ())
#define CCM_MAGNIFIER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_MAGNIFIER, CCMMagnifier))
#define CCM_MAGNIFIER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_MAGNIFIER, CCMMagnifierClass))
#define CCM_IS_MAGNIFIER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_MAGNIFIER))
#define CCM_IS_MAGNIFIER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_MAGNIFIER))
#define CCM_MAGNIFIER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_MAGNIFIER, CCMMagnifierClass))

typedef struct _CCMMagnifierClass CCMMagnifierClass;
typedef struct _CCMMagnifier CCMMagnifier;

struct _CCMMagnifierClass
{
	CCMPluginClass parent_class;
};

typedef struct _CCMMagnifierPrivate CCMMagnifierPrivate;

struct _CCMMagnifier
{
	CCMPlugin parent_instance;
	
	CCMMagnifierPrivate* priv;
};

GType ccm_magnifier_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CCM_MAGNIFIER_H_ */
