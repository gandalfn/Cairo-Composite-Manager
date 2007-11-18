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

#ifndef _CCM_WINDOW_H_
#define _CCM_WINDOW_H_

#include <glib-object.h>

#include "ccm.h"
#include "ccm-drawable.h"

G_BEGIN_DECLS

#define CCM_TYPE_WINDOW             (ccm_window_get_type ())
#define CCM_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_WINDOW, CCMWindow))
#define CCM_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_WINDOW, CCMWindowClass))
#define CCM_IS_WINDOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_WINDOW))
#define CCM_IS_WINDOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_WINDOW))
#define CCM_WINDOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_WINDOW, CCMWindowClass))

#define CCM_WINDOW_XWINDOW(obj)     (ccm_drawable_get_xid(CCM_DRAWABLE(obj)))

struct _CCMWindowClass
{
	CCMDrawableClass parent_class;
	
	Atom			 none_atom;
	Atom			 utf8_string_atom;
	Atom			 name_atom;
	Atom			 visible_name_atom;
	
	Atom	 		 opacity_atom;
	
	Atom	 		 type_atom;
	Atom	         type_normal_atom;
	Atom  	         type_desktop_atom;
	Atom			 type_dock_atom;
    Atom			 type_toolbar_atom;
    Atom			 type_menu_atom;
    Atom			 type_util_atom;
    Atom			 type_splash_atom;
    Atom 			 type_dialog_atom;
    Atom			 type_dropdown_menu_atom;
	Atom 			 type_popup_menu_atom;
	Atom			 type_tooltip_atom;
    Atom			 type_notification_atom;
	Atom			 type_combo_atom;
	Atom			 type_dnd_atom;
	
	Atom			 state_atom;
	Atom			 state_shade_atom;
	Atom			 state_fullscreen_atom;
	Atom			 state_above_atom;
};

typedef struct _CCMWindowPrivate CCMWindowPrivate;

struct _CCMWindow
{
	CCMDrawable parent_instance;
	
	CCMWindowPrivate* priv;
};

GType ccm_window_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CCM_WINDOW_H_ */
