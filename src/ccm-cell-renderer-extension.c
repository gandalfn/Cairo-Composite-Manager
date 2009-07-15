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
#include "ccm-cell-renderer-extension.h"

enum
{
    PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_VERSION,
	PROP_ENABLED
};

struct _CCMCellRendererExtensionPrivate
{
	gchar* name;
	gchar* description;
	gchar* version;
	gboolean enabled;
};

G_DEFINE_TYPE (CCMCellRendererExtension, ccm_cell_renderer_extension, 
			   GTK_TYPE_CELL_RENDERER_TEXT);

#define CCM_CELL_RENDERER_EXTENSION_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CELL_RENDERER_EXTENSION, CCMCellRendererExtensionPrivate))

static GtkCellEditable*
ccm_cell_renderer_extension_start_editing (GtkCellRenderer *cell, 
										   GdkEvent *event,
										   GtkWidget *widget,
										   const gchar *path,
										   GdkRectangle *background_area,
										   GdkRectangle *cell_area,
										   GtkCellRendererState flags);

static void
ccm_cell_renderer_extension_set_gobject_property(GObject *object,
												 guint prop_id,
												 const GValue *value,
												 GParamSpec *pspec)
{
	CCMCellRendererExtension* self = CCM_CELL_RENDERER_EXTENSION(object);
	GtkWidget* fake = gtk_label_new("");
	    
	switch (prop_id)
    {
    	case PROP_NAME:
			if (self->priv->name) g_free(self->priv->name);
			self->priv->name = g_value_dup_string(value);
			break;
    	case PROP_DESCRIPTION:
			if (self->priv->description) g_free(self->priv->description);
			self->priv->description = g_value_dup_string(value);
			break;
    	case PROP_VERSION:
			if (self->priv->version) g_free(self->priv->version);
			self->priv->version = g_value_dup_string(value);
			break;
		case PROP_ENABLED:
			self->priv->enabled = g_value_get_boolean (value);
			break;
		default:
			break;
    }
	
	if (self->priv->name)
	{
		gchar* text = g_strdup_printf("<b>%s</b>\n%s\nVersion: %s\n", self->priv->name,
									  self->priv->description ? 
									  self->priv->description : "",
									  self->priv->version ? 
									  self->priv->version : "");
		g_object_set(object, "markup", text, NULL);
		g_free(text);
	}
	
	g_object_set(object, "foreground-gdk", 
				 &fake->style->text[self->priv->enabled ?
								    GTK_STATE_NORMAL : GTK_STATE_INSENSITIVE],
				 NULL);
}

static void
ccm_cell_renderer_extension_get_gobject_property (GObject* object,
												  guint prop_id,
												  GValue* value,
												  GParamSpec* pspec)
{
	CCMCellRendererExtension* self = CCM_CELL_RENDERER_EXTENSION(object);
    
	switch (prop_id)
    {
    	case PROP_NAME:
			g_value_set_string(value, self->priv->name);
			break;
    	case PROP_DESCRIPTION:
			g_value_set_string(value, self->priv->description);
			break;
    	case PROP_VERSION:
			g_value_set_string(value, self->priv->version);
			break;
    	case PROP_ENABLED:
			g_value_set_boolean (value, self->priv->enabled);
			break;
		default:
			break;
    }
}

static void
ccm_cell_renderer_extension_init (CCMCellRendererExtension *self)
{
	self->priv = CCM_CELL_RENDERER_EXTENSION_GET_PRIVATE(self);
	self->priv->name = NULL;
	self->priv->description = NULL;
	self->priv->version = NULL;
	self->priv->enabled = FALSE;
}

static void
ccm_cell_renderer_extension_finalize (GObject *object)
{
	CCMCellRendererExtension* self = CCM_CELL_RENDERER_EXTENSION(object);
	
	if (self->priv->name) g_free(self->priv->name);
	if (self->priv->description) g_free(self->priv->description);
	if (self->priv->version) g_free(self->priv->version);
	
	G_OBJECT_CLASS (ccm_cell_renderer_extension_parent_class)->finalize (object);
}

static void
ccm_cell_renderer_extension_class_init (CCMCellRendererExtensionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMCellRendererExtensionPrivate));
	
	object_class->get_property = ccm_cell_renderer_extension_get_gobject_property;
    object_class->set_property = ccm_cell_renderer_extension_set_gobject_property;
	
	GTK_CELL_RENDERER_CLASS (klass)->start_editing = 
									ccm_cell_renderer_extension_start_editing; 
		
	object_class->finalize = ccm_cell_renderer_extension_finalize;
	
	g_object_class_install_property(object_class, PROP_NAME,
		g_param_spec_string ("name",
		 					 "Name",
			     			 "Extension name",
							 "",
		     				 G_PARAM_READWRITE));
	
	g_object_class_install_property(object_class, PROP_DESCRIPTION,
		g_param_spec_string ("description",
		 					 "Description",
			     			 "Extension description",
							 "",
		     				 G_PARAM_READWRITE));

	g_object_class_install_property(object_class, PROP_VERSION,
		g_param_spec_string ("version",
	 						 "Version",
			     			 "Extension version",
							 "",
		     				 G_PARAM_READWRITE));

	g_object_class_install_property(object_class, PROP_ENABLED,
		g_param_spec_boolean ("enabled",
		 					  "Enabled",
			     			  "Extension is active",
							  FALSE,
		     				  G_PARAM_READWRITE));
}

static void
ccm_cell_renderer_extension_on_editing_done(CCMCellRendererExtension* self,
											CCMCellExtension* cell)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(cell != NULL);
	
	const gchar* path = ccm_cell_extension_get_path(cell);
	gboolean enabled = ccm_cell_extension_get_active(cell);
	gchar* text;
		
	text = g_strdup_printf("<b>%s</b>\n%s", self->priv->name,
						   self->priv->description ? 
						   self->priv->description : "");
	g_object_set(G_OBJECT(self), "enabled", enabled, NULL);
	g_signal_emit_by_name (self, "edited", path, text); 
	g_free(text);
}

static GtkCellEditable*
ccm_cell_renderer_extension_start_editing (GtkCellRenderer *cell, 
										   GdkEvent *event,
										   GtkWidget *widget,
										   const gchar *path,
										   GdkRectangle *background_area,
										   GdkRectangle *cell_area,
										   GtkCellRendererState flags)
{ 
	g_return_val_if_fail(cell != NULL, NULL);
	g_return_val_if_fail(path != NULL, NULL);
	
	CCMCellRendererExtension* self = CCM_CELL_RENDERER_EXTENSION(cell);
	CCMCellExtension* cell_editable = NULL;
	gboolean editable = FALSE;
	
	g_object_get(G_OBJECT(self), "editable", &editable, NULL);
	if (!editable) return NULL;
	
	cell_editable = ccm_cell_extension_new(path, cell_area->width);
	if (cell_editable)
	{
		ccm_cell_extension_set_active(cell_editable, self->priv->enabled);
		
		gtk_widget_show(GTK_WIDGET(cell_editable));
		
		g_signal_connect_swapped(cell_editable, "editing-done", 
								 G_CALLBACK(ccm_cell_renderer_extension_on_editing_done),
								 self);
	}
	
	return cell_editable ? GTK_CELL_EDITABLE(cell_editable) : NULL;
}

CCMCellRendererExtension*
ccm_cell_renderer_extension_new (void)
{
	CCMCellRendererExtension* self = 
		g_object_new(CCM_TYPE_CELL_RENDERER_EXTENSION, NULL);
	
	g_object_set (self, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_object_set (self, "editable", TRUE, NULL);
	
	return self;
}
