/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-window.h
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

#ifndef _CCM_WINDOW_H_
#define _CCM_WINDOW_H_

#include <glib-object.h>

#include "ccm.h"
#include "ccm-drawable.h"
#include "ccm-window-plugin.h"

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

    Atom ccm_atom;

    Atom atom;
    Atom none_atom;
    Atom utf8_string_atom;
    Atom name_atom;
    Atom visible_name_atom;

    Atom active_atom;
    Atom user_time_atom;

    Atom root_pixmap_atom;

    Atom client_list_atom;
    Atom client_stacking_list_atom;

    Atom opacity_atom;

    Atom type_atom;
    Atom type_normal_atom;
    Atom type_desktop_atom;
    Atom type_dock_atom;
    Atom type_toolbar_atom;
    Atom type_menu_atom;
    Atom type_util_atom;
    Atom type_splash_atom;
    Atom type_dialog_atom;
    Atom type_dropdown_menu_atom;
    Atom type_popup_menu_atom;
    Atom type_tooltip_atom;
    Atom type_notification_atom;
    Atom type_combo_atom;
    Atom type_dnd_atom;

    Atom state_atom;
    Atom state_shade_atom;
    Atom state_fullscreen_atom;
    Atom state_above_atom;
    Atom state_below_atom;
    Atom state_is_modal;
    Atom state_skip_taskbar;
    Atom state_skip_pager;

    Atom mwm_hints_atom;

    Atom frame_extends_atom;

    Atom transient_for_atom;

    Atom current_desktop_atom;

    Atom protocol_atom;
    Atom delete_window_atom;
    Atom ping_atom;
    Atom pid_atom;

    GSList *plugins;

    CCMPixmap *(*create_pixmap) (CCMWindow * self, int width, int height,
                                 int depth);
};

typedef struct _CCMWindowPrivate CCMWindowPrivate;

struct _CCMWindow
{
    CCMDrawable parent_instance;

    CCMWindowPrivate *priv;
};

GType            ccm_window_get_type (void) G_GNUC_CONST;

CCMWindowPlugin* _ccm_window_get_plugin (CCMWindow* self, GType type);
Window           _ccm_window_get_child  (CCMWindow* self);
void             _ccm_window_reparent   (CCMWindow* self, CCMWindow* parent);

G_END_DECLS

#endif                          /* _CCM_WINDOW_H_ */
