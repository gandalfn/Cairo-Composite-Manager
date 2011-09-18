/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-snapshot.h
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

#ifndef _CCM_SNAPSHOT_H_
#define _CCM_SNAPSHOT_H_

#include <glib-object.h>
#include <gmodule.h>

#include "ccm-plugin.h"
#include "ccm-window-plugin.h"

G_BEGIN_DECLS

#define CCM_TYPE_SNAPSHOT             (ccm_snapshot_get_type ())
#define CCM_SNAPSHOT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_SNAPSHOT, CCMSnapshot))
#define CCM_SNAPSHOT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_SNAPSHOT, CCMSnapshotClass))
#define CCM_IS_SNAPSHOT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_SNAPSHOT))
#define CCM_IS_SNAPSHOT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_SNAPSHOT))
#define CCM_SNAPSHOT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_SNAPSHOT, CCMSnapshotClass))

typedef struct _CCMSnapshotClass CCMSnapshotClass;
typedef struct _CCMSnapshot CCMSnapshot;

struct _CCMSnapshotClass
{
    CCMPluginClass parent_class;
};

typedef struct _CCMSnapshotPrivate CCMSnapshotPrivate;

struct _CCMSnapshot
{
    CCMPlugin parent_instance;

    CCMSnapshotPrivate *priv;
};

GType ccm_snapshot_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif                          /* _CCM_SNAPSHOT_H_ */
