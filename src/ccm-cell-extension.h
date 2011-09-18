/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-cell-extension.h
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

#ifndef _CCM_CELL_EXTENSION_H_
#define _CCM_CELL_EXTENSION_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CCM_TYPE_CELL_EXTENSION             (ccm_cell_extension_get_type ())
#define CCM_CELL_EXTENSION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CELL_EXTENSION, CCMCellExtension))
#define CCM_CELL_EXTENSION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CELL_EXTENSION, CCMCellExtensionClass))
#define CCM_IS_CELL_EXTENSION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CELL_EXTENSION))
#define CCM_IS_CELL_EXTENSION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CELL_EXTENSION))
#define CCM_CELL_EXTENSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CELL_EXTENSION, CCMCellExtensionClass))

typedef struct _CCMCellExtensionClass CCMCellExtensionClass;
typedef struct _CCMCellExtensionPrivate CCMCellExtensionPrivate;
typedef struct _CCMCellExtension CCMCellExtension;

struct _CCMCellExtensionClass
{
    GtkEventBoxClass parent_class;
};

struct _CCMCellExtension
{
    GtkEventBox parent_instance;

    CCMCellExtensionPrivate *priv;
};

GType             ccm_cell_extension_get_type   (void) G_GNUC_CONST;

CCMCellExtension* ccm_cell_extension_new        (const gchar * path, int width);
void              ccm_cell_extension_set_active (CCMCellExtension * self, 
                                                 gboolean enable);
const gchar*      ccm_cell_extension_get_path   (CCMCellExtension * self);
gboolean          ccm_cell_extension_get_active (CCMCellExtension * self);

G_END_DECLS

#endif                          /* _CCM_CELL_EXTENSION_H_ */
