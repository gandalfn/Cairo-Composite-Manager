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

#ifndef _CCM_CLONE_H_
#define _CCM_CLONE_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_CLONE             (ccm_clone_get_type ())
#define CCM_CLONE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CLONE, CCMClone))
#define CCM_CLONE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CLONE, CCMCloneClass))
#define CCM_IS_CLONE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CLONE))
#define CCM_IS_CLONE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CLONE))
#define CCM_CLONE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CLONE, CCMCloneClass))

typedef struct _CCMCloneClass CCMCloneClass;
typedef struct _CCMClone CCMClone;

struct _CCMCloneClass
{
	CCMPluginClass parent_class;

	Atom		   clone_enable_atom;
	Atom		   clone_disable_atom;
};

typedef struct _CCMClonePrivate CCMClonePrivate;

struct _CCMClone
{
	CCMPlugin parent_instance;
	
	CCMClonePrivate* priv;
};

GType ccm_clone_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CCM_CLONE_H_ */
