/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
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

#ifndef _CCM_TRAY_ICON_H_
#define _CCM_TRAY_ICON_H_

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_TRAY_ICON             (ccm_tray_icon_get_type ())
#define CCM_TRAY_ICON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_TRAY_ICON, CCMTrayIcon))
#define CCM_TRAY_ICON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_TRAY_ICON, CCMTrayIconClass))
#define CCM_IS_TRAY_ICON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_TRAY_ICON))
#define CCM_IS_TRAY_ICON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_TRAY_ICON))
#define CCM_TRAY_ICON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_TRAY_ICON, CCMTrayIconClass))

typedef struct _CCMTrayIconClass CCMTrayIconClass;
typedef struct _CCMTrayIconPrivate CCMTrayIconPrivate;
typedef struct _CCMTrayIcon CCMTrayIcon;

struct _CCMTrayIconClass
{
    GObjectClass parent_class;
};

struct _CCMTrayIcon
{
    GObject parent_instance;

    CCMTrayIconPrivate *priv;
};

GType        ccm_tray_icon_get_type (void) G_GNUC_CONST;

CCMTrayIcon* ccm_tray_icon_new      (void);

G_END_DECLS

#endif                          /* _CCM_TRAY_ICON_H_ */
