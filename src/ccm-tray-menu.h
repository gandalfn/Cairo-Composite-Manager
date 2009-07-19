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

#ifndef _CCM_TRAY_MENU_H_
#define _CCM_TRAY_MENU_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS
#define CCM_TYPE_TRAY_MENU             (ccm_tray_menu_get_type ())
#define CCM_TRAY_MENU(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_TRAY_MENU, CCMTrayMenu))
#define CCM_TRAY_MENU_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_TRAY_MENU, CCMTrayMenuClass))
#define CCM_IS_TRAY_MENU(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_TRAY_MENU))
#define CCM_IS_TRAY_MENU_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_TRAY_MENU))
#define CCM_TRAY_MENU_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_TRAY_MENU, CCMTrayMenuClass))
typedef struct _CCMTrayMenuClass CCMTrayMenuClass;
typedef struct _CCMTrayMenuPrivate CCMTrayMenuPrivate;
typedef struct _CCMTrayMenu CCMTrayMenu;

struct _CCMTrayMenuClass
{
    GtkMenuClass parent_class;
};

struct _CCMTrayMenu
{
    GtkMenu parent_instance;

    CCMTrayMenuPrivate *priv;
};

GType
ccm_tray_menu_get_type (void)
    G_GNUC_CONST;
CCMTrayMenu *
ccm_tray_menu_new (void);

G_END_DECLS
#endif                          /* _CCM_TRAY_MENU_H_ */
