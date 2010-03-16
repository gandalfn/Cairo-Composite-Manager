/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
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

#ifndef _CCM_CONFIG_COLOR_BUTTON_H_
#define _CCM_CONFIG_COLOR_BUTTON_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CCM_TYPE_CONFIG_COLOR_BUTTON             (ccm_config_color_button_get_type ())
#define CCM_CONFIG_COLOR_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG_COLOR_BUTTON, CCMConfigColorButton))
#define CCM_CONFIG_COLOR_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG_COLOR_BUTTON, CCMConfigColorButtonClass))
#define CCM_IS_CONFIG_COLOR_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG_COLOR_BUTTON))
#define CCM_IS_CONFIG_COLOR_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG_COLOR_BUTTON))
#define CCM_CONFIG_COLOR_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG_COLOR_BUTTON, CCMConfigColorButtonClass))

typedef struct _CCMConfigColorButtonClass CCMConfigColorButtonClass;
typedef struct _CCMConfigColorButtonPrivate CCMConfigColorButtonPrivate;
typedef struct _CCMConfigColorButton CCMConfigColorButton;

struct _CCMConfigColorButtonClass
{
    GtkColorButtonClass parent_class;
};

struct _CCMConfigColorButton
{
    GtkColorButton parent_instance;

    CCMConfigColorButtonPrivate *priv;
};

GType ccm_config_color_button_get_type (void) G_GNUC_CONST;

GtkWidget* ccm_config_color_button_new (gint screen, gchar* plugin, gchar* key);

G_END_DECLS

#endif                          /* _CCM_CONFIG_COLOR_BUTTON_H_ */
