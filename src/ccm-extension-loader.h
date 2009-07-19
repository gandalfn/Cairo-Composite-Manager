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

#ifndef _CCM_EXTENSION_LOADER_H_
#define _CCM_EXTENSION_LOADER_H_

#include <glib-object.h>

G_BEGIN_DECLS
#define CCM_TYPE_EXTENSION_LOADER             (ccm_extension_loader_get_type ())
#define CCM_EXTENSION_LOADER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_EXTENSION_LOADER, CCMExtensionLoader))
#define CCM_EXTENSION_LOADER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_EXTENSION_LOADER, CCMExtensionLoaderClass))
#define CCM_IS_EXTENSION_LOADER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_EXTENSION_LOADER))
#define CCM_IS_EXTENSION_LOADER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_EXTENSION_LOADER))
#define CCM_EXTENSION_LOADER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_EXTENSION_LOADER, CCMExtensionLoaderClass))
typedef struct _CCMExtensionLoaderPrivate CCMExtensionLoaderPrivate;
typedef struct _CCMExtensionLoaderClass CCMExtensionLoaderClass;
typedef struct _CCMExtensionLoader CCMExtensionLoader;

struct _CCMExtensionLoaderClass
{
    GObjectClass parent_class;
};

struct _CCMExtensionLoader
{
    GObject parent_instance;

    CCMExtensionLoaderPrivate *priv;
};

GType
ccm_extension_loader_get_type (void)
    G_GNUC_CONST;
CCMExtensionLoader *
ccm_extension_loader_new ();

GSList *
ccm_extension_loader_get_preferences_plugins (CCMExtensionLoader * self);
GSList *
ccm_extension_loader_get_screen_window_plugins (CCMExtensionLoader * self);
GSList *
ccm_extension_loader_get_screen_plugins (CCMExtensionLoader * self,
                                         GSList * filter);
GSList *
ccm_extension_loader_get_window_plugins (CCMExtensionLoader * self,
                                         GSList * filter);
void
ccm_extension_loader_add_plugin_path (gchar * path);

G_END_DECLS
#endif                          /* _CCM_EXTENSION_LOADER_H_ */
