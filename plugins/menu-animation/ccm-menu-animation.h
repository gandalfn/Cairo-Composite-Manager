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

#ifndef _CCM_MENU_ANIMATION_H_
#define _CCM_MENU_ANIMATION_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"
#include "ccm-screen-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_MENU_ANIMATION              (ccm_menu_animation_get_type ())
#define CCM_MENU_ANIMATION(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_MENU_ANIMATION, CCMMenuAnimation))
#define CCM_MENU_ANIMATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_MENU_ANIMATION, CCMMenuAnimationClass))
#define CCM_IS_MENU_ANIMATION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_MENU_ANIMATION))
#define CCM_IS_MENU_ANIMATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_MENU_ANIMATION))
#define CCM_MENU_ANIMATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_MENU_ANIMATION, CCMMenuAnimationClass))

typedef struct _CCMMenuAnimationClass CCMMenuAnimationClass;
typedef struct _CCMMenuAnimation CCMMenuAnimation;

struct _CCMMenuAnimationClass
{
    CCMPluginClass parent_class;

    Atom animation_atom;
};

typedef struct _CCMMenuAnimationPrivate CCMMenuAnimationPrivate;

struct _CCMMenuAnimation
{
    CCMPlugin parent_instance;

    CCMMenuAnimationPrivate *priv;
};

GType ccm_menu_animation_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif                          /* _CCM_MENU_ANIMATION_H_ */
