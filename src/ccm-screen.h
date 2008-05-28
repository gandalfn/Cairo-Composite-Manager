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

#define CCM_SCREEN_XSCREEN(obj)     (CCM_SCREEN(obj)->xscreen)

struct _CCMScreenClass
{
	GObjectClass parent_class;
};

typedef struct _CCMScreenPrivate CCMScreenPrivate;

struct _CCMScreen
{
	GObject 	parent_instance;
	
	Screen* 	xscreen;
	guint 		number;
	
	CCMScreenPrivate* priv;
};

GType 			 ccm_screen_get_type			(void) G_GNUC_CONST;
gboolean	     _ccm_screen_use_buffered		(CCMScreen* self);
gboolean		 _ccm_screen_sync_with_blank 	(CCMScreen* self);
GSList*			 _ccm_screen_get_window_plugins	(CCMScreen* self);
gchar*			 _ccm_screen_get_window_backend	(CCMScreen* self);
gboolean		 _ccm_screen_native_pixmap_bind (CCMScreen* self);
gboolean		 _ccm_screen_indirect_rendering (CCMScreen* self);
CCMScreenPlugin* _ccm_screen_get_plugin			(CCMScreen *self, GType type);
void 			 _ccm_screen_set_buffered		(CCMScreen* self, 
												 gboolean buffered);
G_END_DECLS

#endif /* _CCM_SCREEN_H_ */
