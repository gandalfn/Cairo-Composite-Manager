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

#ifndef _CCM_CONFIG_CHECK_BUTTON_H_
#define _CCM_CONFIG_CHECK_BUTTON_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CCM_TYPE_CONFIG_CHECK_BUTTON             (ccm_config_check_button_get_type ())
#define CCM_CONFIG_CHECK_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG_CHECK_BUTTON, CCMConfigCheckButton))
#define CCM_CONFIG_CHECK_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG_CHECK_BUTTON, CCMConfigCheckButtonClass))
#define CCM_IS_CONFIG_CHECK_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG_CHECK_BUTTON))
#define CCM_IS_CONFIG_CHECK_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG_CHECK_BUTTON))
#define CCM_CONFIG_CHECK_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG_CHECK_BUTTON, CCMConfigCheckButtonClass))

typedef struct _CCMConfigCheckButtonClass CCMConfigCheckButtonClass;
typedef struct _CCMConfigCheckButtonPrivate CCMConfigCheckButtonPrivate;
typedef struct _CCMConfigCheckButton CCMConfigCheckButton;

struct _CCMConfigCheckButtonClass
{
	GtkCheckButtonClass parent_class;
};

struct _CCMConfigCheckButton
{
	GtkCheckButton parent_instance;

	CCMConfigCheckButtonPrivate* priv;
};

GType ccm_config_check_button_get_type (void) G_GNUC_CONST;
GtkWidget* ccm_config_check_button_new (int screen, gchar* plugin, gchar* key);

G_END_DECLS

#endif /* _CCM_CONFIG_CHECK_BUTTON_H_ */
