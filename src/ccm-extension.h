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

#ifndef _CCM_EXTENSION_H_
#define _CCM_EXTENSION_H_

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define CCM_TYPE_EXTENSION             (ccm_extension_get_type ())
#define CCM_EXTENSION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_EXTENSION, CCMExtension))
#define CCM_EXTENSION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_EXTENSION, CCMExtensionClass))
#define CCM_IS_EXTENSION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_EXTENSION))
#define CCM_IS_EXTENSION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_EXTENSION))
#define CCM_EXTENSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_EXTENSION, CCMExtensionClass))

typedef struct _CCMExtensionPrivate CCMExtensionPrivate;
typedef struct _CCMExtensionClass CCMExtensionClass;
typedef struct _CCMExtension CCMExtension;

struct _CCMExtensionClass
{
	GTypeModuleClass parent_class;
};

struct _CCMExtension
{
	GTypeModule parent_instance;
	
	CCMExtensionPrivate* priv;
};

GType 		ccm_extension_get_type 			(void) G_GNUC_CONST;

CCMExtension* 	ccm_extension_new			(gchar* filename);
GType 		ccm_extension_get_type_object 		(CCMExtension* self);
const gchar*	ccm_extension_get_label			(CCMExtension* self);
gint		_ccm_extension_compare			(CCMExtension* self,
							 CCMExtension* other);

G_END_DECLS

#endif /* _CCM_EXTENSION_H_ */
