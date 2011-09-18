/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-extension.h
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

    CCMExtensionPrivate *priv;
};

GType                     ccm_extension_get_type        (void) G_GNUC_CONST;

CCMExtension*             ccm_extension_new             (gchar* filename);
GType                     ccm_extension_get_type_object (CCMExtension* self);
G_GNUC_PURE const gchar*  ccm_extension_get_label       (CCMExtension* self);
G_GNUC_PURE const gchar*  ccm_extension_get_description (CCMExtension* self);
G_GNUC_PURE const gchar*  ccm_extension_get_version     (CCMExtension* self);
G_GNUC_PURE const gchar** ccm_extension_get_backends    (CCMExtension* self);

gint                      _ccm_extension_compare        (CCMExtension* self, 
                                                         CCMExtension* other);

G_END_DECLS

#endif                          /* _CCM_EXTENSION_H_ */
