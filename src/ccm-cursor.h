/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <nicolas.bruguier@supersonicimagine.fr>
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

#ifndef _CCM_CURSOR_H_
#define _CCM_CURSOR_H_

#include <glib-object.h>

#include "ccm.h"

G_BEGIN_DECLS

#define CCM_TYPE_CURSOR             (ccm_cursor_get_type ())
#define CCM_CURSOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CURSOR, CCMCursor))
#define CCM_CURSOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CURSOR, CCMCursorClass))
#define CCM_IS_CURSOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CURSOR))
#define CCM_IS_CURSOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CURSOR))
#define CCM_CURSOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CURSOR, CCMCursorClass))

struct _CCMCursorClass
{
	GObjectClass parent_class;
};

typedef struct _CCMCursorPrivate CCMCursorPrivate;

struct _CCMCursor
{
	GObject parent_instance;

	CCMCursorPrivate* priv;
};

GType ccm_cursor_get_type (void) G_GNUC_CONST;
CCMCursor* ccm_cursor_new (CCMDisplay* diplay, XFixesCursorImage* cursor);

G_END_DECLS

#endif /* _CCM_CURSOR_H_ */
