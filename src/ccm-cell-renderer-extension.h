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

#ifndef _CCM_CELL_RENDERER_EXTENSION_H_
#define _CCM_CELL_RENDERER_EXTENSION_H_

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_CELL_RENDERER_EXTENSION             (ccm_cell_renderer_extension_get_type ())
#define CCM_CELL_RENDERER_EXTENSION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CELL_RENDERER_EXTENSION, CCMCellRendererExtension))
#define CCM_CELL_RENDERER_EXTENSION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CELL_RENDERER_EXTENSION, CCMCellRendererExtensionClass))
#define CCM_IS_CELL_RENDERER_EXTENSION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CELL_RENDERER_EXTENSION))
#define CCM_IS_CELL_RENDERER_EXTENSION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CELL_RENDERER_EXTENSION))
#define CCM_CELL_RENDERER_EXTENSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CELL_RENDERER_EXTENSION, CCMCellRendererExtensionClass))

typedef struct _CCMCellRendererExtensionClass CCMCellRendererExtensionClass;
typedef struct _CCMCellRendererExtensionPrivate CCMCellRendererExtensionPrivate;
typedef struct _CCMCellRendererExtension CCMCellRendererExtension;

struct _CCMCellRendererExtensionClass
{
    GtkCellRendererTextClass parent_class;
};

struct _CCMCellRendererExtension
{
    GtkCellRendererText parent_instance;

    CCMCellRendererExtensionPrivate *priv;
};

GType                     ccm_cell_renderer_extension_get_type (void) G_GNUC_CONST;

CCMCellRendererExtension* ccm_cell_renderer_extension_new      (void);

G_END_DECLS
#endif                          /* _CCM_CELL_RENDERER_EXTENSION_H_ */
