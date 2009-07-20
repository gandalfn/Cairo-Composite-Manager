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

#ifndef _CCM_CONFIG_ENTRY_SHORTCUT_H_
#define _CCM_CONFIG_ENTRY_SHORTCUT_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CCM_TYPE_CONFIG_ENTRY_SHORTCUT             (ccm_config_entry_shortcut_get_type ())
#define CCM_CONFIG_ENTRY_SHORTCUT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG_ENTRY_SHORTCUT, CCMConfigEntryShortcut))
#define CCM_CONFIG_ENTRY_SHORTCUT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG_ENTRY_SHORTCUT, CCMConfigEntryShortcutClass))
#define CCM_IS_CONFIG_ENTRY_SHORTCUT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG_ENTRY_SHORTCUT))
#define CCM_IS_CONFIG_ENTRY_SHORTCUT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG_ENTRY_SHORTCUT))
#define CCM_CONFIG_ENTRY_SHORTCUT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG_ENTRY_SHORTCUT, CCMConfigEntryShortcutClass))

typedef struct _CCMConfigEntryShortcutClass CCMConfigEntryShortcutClass;
typedef struct _CCMConfigEntryShortcutPrivate CCMConfigEntryShortcutPrivate;
typedef struct _CCMConfigEntryShortcut CCMConfigEntryShortcut;

struct _CCMConfigEntryShortcutClass
{
    GtkEntryClass parent_class;
};

struct _CCMConfigEntryShortcut
{
    GtkEntry parent_instance;

    CCMConfigEntryShortcutPrivate *priv;
};

GType ccm_config_entry_shortcut_get_type (void) G_GNUC_CONST;

GtkWidget* ccm_config_entry_shortcut_new (gboolean mouse, gint screen, 
                                          gchar* plugin, gchar* key);

G_END_DECLS

#endif                          /* _CCM_CONFIG_ENTRY_SHORTCUT_H_ */
