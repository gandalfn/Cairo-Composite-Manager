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

#ifndef _CCM_DISPLAY_H_
#define _CCM_DISPLAY_H_

#include <glib-object.h>
#include <X11/X.h>
#include <X11/extensions/Xdamage.h>

#include "async-getprop.h"
#include "ccm.h"

G_BEGIN_DECLS

#define CCM_TYPE_DISPLAY             (ccm_display_get_type ())
#define CCM_DISPLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_DISPLAY, CCMDisplay))
#define CCM_DISPLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_DISPLAY, CCMDisplayClass))
#define CCM_IS_DISPLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_DISPLAY))
#define CCM_IS_DISPLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_DISPLAY))
#define CCM_DISPLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_DISPLAY, CCMDisplayClass))

#define CCM_DISPLAY_XDISPLAY(obj)    (CCM_DISPLAY(obj)->xdisplay)

struct _CCMDisplayClass
{
	GObjectClass parent_class;
};

typedef struct _CCMDisplayPrivate CCMDisplayPrivate;

struct _CCMDisplay
{
	GObject 	parent_instance;
	
	Display*	xdisplay;
	
	CCMDisplayPrivate* priv;
};

GType 		ccm_display_get_type 			(void) G_GNUC_CONST;
void		_ccm_display_trap_error			(CCMDisplay* self);
gint		_ccm_display_pop_error			(CCMDisplay* self);
gboolean	_ccm_display_use_xshm			(CCMDisplay* self);
gboolean	_ccm_display_xshm_shared_pixmap (CCMDisplay* self);

									 
G_END_DECLS

#endif /* _CCM_DISPLAY_H_ */
