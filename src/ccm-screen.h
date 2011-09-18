/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-screen.h
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

#ifndef _CCM_SCREEN_H_
#define _CCM_SCREEN_H_

#include <glib-object.h>

#include "ccm.h"
#include "ccm-screen-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_SCREEN             (ccm_screen_get_type ())
#define CCM_SCREEN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_SCREEN, CCMScreen))
#define CCM_SCREEN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_SCREEN, CCMScreenClass))
#define CCM_IS_SCREEN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_SCREEN))
#define CCM_IS_SCREEN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_SCREEN))
#define CCM_SCREEN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_SCREEN, CCMScreenClass))
#define CCM_SCREEN_XSCREEN(obj)     (ccm_screen_get_xscreen(CCM_SCREEN(obj)))
#define CCM_SCREEN_NUMBER(obj)      (ccm_screen_get_number(CCM_SCREEN(obj)))

struct _CCMScreenClass
{
    GObjectClass parent_class;
};

typedef struct _CCMScreenPrivate CCMScreenPrivate;

struct _CCMScreen
{
    GObject parent_instance;

    CCMScreenPrivate *priv;
};

GType            ccm_screen_get_type (void) G_GNUC_CONST;

CCMScreenPlugin* _ccm_screen_get_plugin          (CCMScreen* self, GType type);
Window           _ccm_screen_get_selection_owner (CCMScreen* self);

G_END_DECLS

#endif                          /* _CCM_SCREEN_H_ */
