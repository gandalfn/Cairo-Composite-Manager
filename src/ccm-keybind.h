/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-keybind.h
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

#ifndef _CCM_KEYBIND_H_
#define _CCM_KEYBIND_H_

#include <glib-object.h>

#include "ccm-screen.h"

G_BEGIN_DECLS

#define CCM_TYPE_KEYBIND             (ccm_keybind_get_type ())
#define CCM_KEYBIND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_KEYBIND, CCMKeybind))
#define CCM_KEYBIND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_KEYBIND, CCMKeybindClass))
#define CCM_IS_KEYBIND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_KEYBIND))
#define CCM_IS_KEYBIND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_KEYBIND))
#define CCM_KEYBIND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_KEYBIND, CCMKeybindClass))

typedef struct _CCMKeybindClass CCMKeybindClass;
typedef struct _CCMKeybindPrivate CCMKeybindPrivate;
typedef struct _CCMKeybind CCMKeybind;

struct _CCMKeybindClass
{
    GObjectClass parent_class;
};

struct _CCMKeybind
{
    GObject parent_instance;

    CCMKeybindPrivate *priv;
};

GType       ccm_keybind_get_type (void) G_GNUC_CONST;
CCMKeybind* ccm_keybind_new      (CCMScreen* screen, gchar* keystring, 
                                  gboolean exclusive);

G_END_DECLS

#endif                          /* _CCM_KEYBIND_H_ */
