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

#include "ccm-cell-extension.h"

struct _CCMCellExtensionPrivate
{
	gchar*	     path;
	
	GtkWidget*   name;
	GtkWidget*   author;
	GtkWidget*   enable;
};

static void ccm_cell_extension_start_editing (GtkCellEditable* cell,
											  GdkEvent *event);
static void	ccm_cell_extension_iface_init(GtkCellEditableIface* iface);

G_DEFINE_TYPE_EXTENDED (CCMCellExtension, ccm_cell_extension, 
						GTK_TYPE_EVENT_BOX, 0,
						G_IMPLEMENT_INTERFACE(GTK_TYPE_CELL_EDITABLE,
											  ccm_cell_extension_iface_init));

#define CCM_CELL_EXTENSION_GET_PRIVATE(o) \
	((CCMCellExtensionPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CELL_EXTENSION, CCMCellExtensionClass))

static void
ccm_cell_extension_init (CCMCellExtension *self)
{
	self->priv = CCM_CELL_EXTENSION_GET_PRIVATE(self);
	self->priv->path = NULL;
	self->priv->name = NULL;
	self->priv->author = NULL;
	self->priv->enable = NULL;
}

static void
ccm_cell_extension_finalize (GObject *object)
{
	CCMCellExtension* self = CCM_CELL_EXTENSION(object);
	
	if (self->priv->path) g_free(self->priv->path);
	self->priv->path = NULL;
	
	G_OBJECT_CLASS (ccm_cell_extension_parent_class)->finalize (object);
}

static void
ccm_cell_extension_class_init (CCMCellExtensionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMCellExtensionPrivate));

	object_class->finalize = ccm_cell_extension_finalize;
}

static void
ccm_cell_extension_iface_init(GtkCellEditableIface* iface)
{
	iface->start_editing = ccm_cell_extension_start_editing;
}

static void
ccm_cell_extension_on_enable_clicked (CCMCellExtension* self)
{
	gboolean active = ccm_cell_extension_get_active(self);
	
	ccm_cell_extension_set_active(self, !active);
	
	gtk_cell_editable_editing_done(GTK_CELL_EDITABLE(self));
}

static void 
ccm_cell_extension_start_editing (GtkCellEditable* cell, GdkEvent *event)
{
	CCMCellExtension* self = CCM_CELL_EXTENSION(cell);
	
	g_signal_connect_swapped(gtk_widget_get_parent(self->priv->enable), 
							 "clicked", 
							 G_CALLBACK(ccm_cell_extension_on_enable_clicked), 
							 self);
}

CCMCellExtension*
ccm_cell_extension_new (const gchar* path, int width)
{
	g_return_val_if_fail(path != NULL, NULL);
	
	CCMCellExtension* self = g_object_new(CCM_TYPE_CELL_EXTENSION, NULL);
	GtkWidget *vbox, *hbox, *button;
	
	self->priv->path = g_strdup(path);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(self), vbox);
	
	// Name/description label
	self->priv->name = gtk_label_new("\n");
	gtk_widget_set_size_request(self->priv->name, width - 4, -1);
	gtk_label_set_use_markup(GTK_LABEL(self->priv->name), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(self->priv->name), TRUE);
	gtk_widget_show(self->priv->name);
	gtk_box_pack_start(GTK_BOX(vbox), self->priv->name, TRUE, TRUE, 0);
	
	// Add activation line
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	
	self->priv->author = gtk_label_new("");
	gtk_label_set_use_markup(GTK_LABEL(self->priv->author), TRUE);
	gtk_widget_show(self->priv->author);
	gtk_box_pack_start(GTK_BOX(hbox), self->priv->author, TRUE, TRUE, 0);
	
	self->priv->enable = gtk_label_new("");
	gtk_label_set_use_markup(GTK_LABEL(self->priv->enable), TRUE);
	gtk_label_set_markup(GTK_LABEL(self->priv->enable), 
						 "<span size='small'>Enable</span>");
	gtk_widget_show(self->priv->enable);
	
	button = gtk_button_new();
	gtk_widget_show(button);
	gtk_container_add(GTK_CONTAINER(button), self->priv->enable);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_event_box_set_visible_window(GTK_EVENT_BOX(self), FALSE);
	
	return self;
}

void
ccm_cell_extension_set_active(CCMCellExtension* self, gboolean enable)
{
	g_return_if_fail(self != NULL);
	
	gtk_label_set_markup(GTK_LABEL(self->priv->enable), 
					     !enable ? "<span size='small'>Enable</span>" :
							 "<span size='small'>Disable</span>");
}

const gchar*
ccm_cell_extension_get_path(CCMCellExtension* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->path;
}

gboolean
ccm_cell_extension_get_active(CCMCellExtension* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return 
		!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(self->priv->enable)), 
							"Disable");
}
