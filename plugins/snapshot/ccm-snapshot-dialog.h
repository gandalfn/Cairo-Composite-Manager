/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <gandalfn@club-internet.fr>
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

#ifndef _CCM_SNAPSHOT_DIALOG_H_
#define _CCM_SNAPSHOT_DIALOG_H_

#include <cairo.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_SNAPSHOT_DIALOG             (ccm_snapshot_dialog_get_type ())
#define CCM_SNAPSHOT_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_SNAPSHOT_DIALOG, CCMSnapshotDialog))
#define CCM_SNAPSHOT_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_SNAPSHOT_DIALOG, CCMSnapshotDialogClass))
#define CCM_IS_SNAPSHOT_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_SNAPSHOT_DIALOG))
#define CCM_IS_SNAPSHOT_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_SNAPSHOT_DIALOG))
#define CCM_SNAPSHOT_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_SNAPSHOT_DIALOG, CCMSnapshotDialogClass))

typedef struct _CCMSnapshotDialogClass CCMSnapshotDialogClass;
typedef struct _CCMSnapshotDialogPrivate CCMSnapshotDialogPrivate;
typedef struct _CCMSnapshotDialog CCMSnapshotDialog;

struct _CCMSnapshotDialogClass
{
    GObjectClass parent_class;

    gint nb;
};

struct _CCMSnapshotDialog
{
    GObject parent_instance;

    CCMSnapshotDialogPrivate *priv;
};

GType ccm_snapshot_dialog_get_type (void) G_GNUC_CONST;

CCMSnapshotDialog* ccm_snapshot_dialog_new (cairo_surface_t *surface, 
                                            CCMScreen *screen);

G_END_DECLS
#endif                          /* _CCM_SNAPSHOT_DIALOG_H_ */
