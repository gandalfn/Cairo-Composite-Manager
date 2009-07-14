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

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "ccm-config.h"
#include "ccm-config-check-button.h"
#include "ccm-config-color-button.h"
#include "ccm-config-adjustment.h"
#include "ccm-extension.h"
#include "ccm-extension-loader.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-preferences-page.h"
#include "ccm-cell-renderer-extension.h"
#include "ccm-marshallers.h"

enum
{
	CCM_GENERAL_ENABLE,
	CCM_GENERAL_USE_XSHM,
	CCM_GENERAL_UNMANAGED_SCREEN,
	CCM_GENERAL_OPTION_N
};

enum
{
	CCM_SCREEN_BACKEND,
	CCM_SCREEN_PIXMAP,
	CCM_SCREEN_USE_BUFFERED,
	CCM_SCREEN_PLUGINS,
	CCM_SCREEN_REFRESH_RATE,
	CCM_SCREEN_SYNC_WITH_VBLANK,
	CCM_SCREEN_INDIRECT,
	CCM_SCREEN_USE_ROOT_BACKGROUND,
	CCM_SCREEN_BACKGROUND,
	CCM_SCREEN_COLOR_BACKGROUND,
	CCM_SCREEN_BACKGROUND_X,
	CCM_SCREEN_BACKGROUND_Y,
	CCM_SCREEN_OPTION_N
};

static gchar* CCMGeneralOptions[CCM_GENERAL_OPTION_N] = {
	"enable",
	"use_xshm",
	"unmanaged_screen"
};

static gchar* CCMScreenOptions[CCM_SCREEN_OPTION_N] = {	
	"backend",
	"native_pixmap_bind",
	"use_buffered_pixmap",
	"plugins",
	"refresh_rate",
	"sync_with_vblank",
	"indirect",
	"use_root_background",
	"background",
	"color_background",
	"background_x",
	"background_y"
};

#define CCM_WIDGET_PLUGIN_NAME g_quark_from_static_string("CCMWidgetPluginName")
#define CCM_WIDGET_PLUGIN_ACTIVE g_quark_from_static_string("CCMWidgetPluginActive")
#define CCM_WIDGET_SECTION g_quark_from_static_string("CCMWidgetSection")

enum
{
	NEED_RESTART,
	PLUGIN_STATE_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _CCMPreferencesPagePrivate
{
	CCMPreferences*				preferences;
	
	gint						screen_num;
	GtkBuilder*					builder;
	GtkTreePath*				last_section;

	gchar*						backend;
	
	CCMExtensionLoader*			plugin_loader;
	CCMPreferencesPagePlugin*   plugin;

	CCMConfig*					general_options[CCM_GENERAL_OPTION_N];
	CCMConfig*					screen_options[CCM_SCREEN_OPTION_N];
};

static void
impl_ccm_preferences_page_init_general_section(CCMPreferencesPagePlugin* plugin,
											   CCMPreferencesPage* self,
											   GtkWidget* widget);
static void
impl_ccm_preferences_page_init_desktop_section(CCMPreferencesPagePlugin* plugin,
											   CCMPreferencesPage* self,
											   GtkWidget* desktop_section);
static void ccm_preferences_page_iface_init (CCMPreferencesPagePluginClass* iface);
static void ccm_preferences_page_on_backend_changed(CCMPreferencesPage* self,
                                                    GtkComboBox* combo);

G_DEFINE_TYPE_EXTENDED (CCMPreferencesPage, ccm_preferences_page, G_TYPE_OBJECT, 
						0, G_IMPLEMENT_INTERFACE(CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
												 ccm_preferences_page_iface_init));

#define CCM_PREFERENCES_PAGE_GET_PRIVATE(o)  \
	((CCMPreferencesPagePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PREFERENCES_PAGE, CCMPreferencesPageClass))

static void
ccm_preferences_page_init (CCMPreferencesPage *self)
{
	gint cpt;
	
	self->priv = CCM_PREFERENCES_PAGE_GET_PRIVATE(self);
	self->priv->screen_num = -1;
	self->priv->builder = NULL;
	self->priv->last_section = NULL;
	self->priv->backend = NULL;
	self->priv->plugin_loader = NULL;
	self->priv->plugin = NULL;
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
		self->priv->screen_options[cpt] = NULL;
	for (cpt = 0; cpt < CCM_GENERAL_OPTION_N; ++cpt)
		self->priv->general_options[cpt] = NULL;
}

static void
ccm_preferences_page_finalize (GObject *object)
{
	CCMPreferencesPage *self = CCM_PREFERENCES_PAGE(object);
	gint cpt;

	if (self->priv->backend) g_free(self->priv->backend);
	
	if (self->priv->last_section) gtk_tree_path_free(self->priv->last_section);
	
	if (self->priv->builder) g_object_unref(self->priv->builder);

	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin))
		g_object_unref(self->priv->plugin);
	
	if (self->priv->plugin_loader)
		g_object_unref(self->priv->plugin_loader);
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
		g_object_unref(self->priv->screen_options[cpt]);
	
	for (cpt = 0; cpt < CCM_GENERAL_OPTION_N; ++cpt)
		g_object_unref(self->priv->general_options[cpt]);
	
	G_OBJECT_CLASS (ccm_preferences_page_parent_class)->finalize (object);
}

static void
ccm_preferences_page_class_init (CCMPreferencesPageClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMPreferencesPagePrivate));

	object_class->finalize = ccm_preferences_page_finalize;

	signals[NEED_RESTART] = g_signal_new ("need-restart",
	                                G_OBJECT_CLASS_TYPE (object_class),
									G_SIGNAL_RUN_LAST, 0, NULL, NULL,
									ccm_cclosure_marshal_VOID__POINTER_POINTER,
									G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	
	signals[PLUGIN_STATE_CHANGED] = g_signal_new ("plugin-state-changed",
									G_OBJECT_CLASS_TYPE (object_class),
									G_SIGNAL_RUN_LAST, 0, NULL, NULL,
									ccm_cclosure_marshal_VOID__STRING_BOOLEAN,
									G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}

static void
ccm_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface)
{
	iface->init_general_section = impl_ccm_preferences_page_init_general_section;
	iface->init_desktop_section = impl_ccm_preferences_page_init_desktop_section;
}

static void
ccm_preferences_page_on_background_changed(CCMPreferencesPage* self,
                                           CCMConfig* config)
{
	gchar* filename;

	filename = ccm_config_get_string(self->priv->screen_options[CCM_SCREEN_BACKGROUND], 
	                                 NULL);
	if (filename && g_file_test(filename, G_FILE_TEST_EXISTS) &&
	    !g_file_test(filename, G_FILE_TEST_IS_DIR))
	{
		GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

		if (pixbuf)
		{
			GdkPixbuf* scaled;
			GtkImage* image;
			int width, height;
			
			image = GTK_IMAGE(gtk_builder_get_object(self->priv->builder,
			                                         "background_image"));
			if (!image)
			{
				g_object_unref(pixbuf);
				return;
			}

			gtk_widget_get_size_request(GTK_WIDGET(image), &width, &height);
			if (gdk_pixbuf_get_width(pixbuf) > gdk_pixbuf_get_height(pixbuf))
			{
				height = (int)((float)gdk_pixbuf_get_height(pixbuf) * 
				               ((float)width / (float)gdk_pixbuf_get_width(pixbuf)));
			}
			else
			{
				width = (int)((float)gdk_pixbuf_get_width(pixbuf) * 
					          ((float)height / (float)gdk_pixbuf_get_height(pixbuf)));
			}

			scaled = gdk_pixbuf_scale_simple(pixbuf, width, height, 
			                                 GDK_INTERP_BILINEAR);
			g_object_unref(pixbuf);
			
			gtk_image_set_from_pixbuf(image, scaled);

			g_object_unref(scaled);
		}
	}
}

static void
ccm_preferences_page_on_use_root_background_changed(CCMPreferencesPage* self,
                                                    CCMConfig* config)
{
	gboolean use_root_background = ccm_config_get_boolean (config, NULL);
	GtkWidget* widget;

	widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder, 
	                                           "color_background"));
	gtk_widget_set_sensitive(widget, !use_root_background);

	widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder, 
	                                           "background_button"));
	gtk_widget_set_sensitive(widget, !use_root_background);
	
	widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
	                                           "background_x"));
	gtk_widget_set_sensitive(widget, !use_root_background);

	widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder, 
	                                           "background_y"));
	gtk_widget_set_sensitive(widget, !use_root_background);

	if (use_root_background)
	{
		GdkAtom	prop_type;
		union 
		{
			Pixmap	*pixmap;
			guchar  *data;
		} prop_data;
		GdkDisplay* display = gdk_display_get_default();
		GdkScreen* screen = gdk_display_get_screen(display, 
		                                           self->priv->screen_num);
		GdkWindow* root = gdk_screen_get_root_window(screen);
		GdkAtom atom = gdk_atom_intern ("_XROOTPMAP_ID", FALSE);

		gdk_property_get(root, atom, 0, 0, 10, FALSE, &prop_type, 
			             NULL, NULL, &prop_data.data);

		if (prop_type == GDK_TARGET_PIXMAP && prop_data.pixmap != NULL && 
			prop_data.pixmap[0] != 0) 
		{
			GdkPixbuf* scaled;
			GtkImage* image;
			GdkColormap *colormap = NULL;
			int width, height, rwidth, rheight, pwidth, pheight;
			GdkPixmap* pixmap = gdk_pixmap_foreign_new(prop_data.pixmap[0]);
			GdkPixbuf* pixbuf;
		
			gdk_drawable_get_size (GDK_DRAWABLE (pixmap), &pwidth, &pheight);
			gdk_drawable_get_size (GDK_DRAWABLE (root), &rwidth, &rheight);
			width = MIN(pwidth, rwidth);
			height = MIN(pheight, rheight);

			colormap = gdk_drawable_get_colormap(root);
			pixbuf = gdk_pixbuf_get_from_drawable(NULL, pixmap, colormap, 
				                                  0, 0, 0, 0, width, height);
			g_object_unref(colormap);
			
			image = GTK_IMAGE(gtk_builder_get_object(self->priv->builder,
					                                     "background_image"));
			if (!image)
			{
				g_object_unref(pixbuf);
				return;
			}

			gtk_widget_get_size_request(GTK_WIDGET(image), &width, &height);
			if (gdk_pixbuf_get_width(pixbuf) > gdk_pixbuf_get_height(pixbuf))
			{
				height = (int)((float)gdk_pixbuf_get_height(pixbuf) * 
					           ((float)width / (float)gdk_pixbuf_get_width(pixbuf)));
			}
			else
			{
				width = (int)((float)gdk_pixbuf_get_width(pixbuf) * 
						      ((float)height / (float)gdk_pixbuf_get_height(pixbuf)));
			}

			scaled = gdk_pixbuf_scale_simple(pixbuf, width, height, 
				                             GDK_INTERP_BILINEAR);
			g_object_unref(pixbuf);
			
			gtk_image_set_from_pixbuf(image, scaled);
		}
	}
	else
		ccm_preferences_page_on_background_changed(self,
		                                           self->priv->screen_options[CCM_SCREEN_BACKGROUND]);
}

static void
ccm_preferences_page_load_config(CCMPreferencesPage* self)
{
	g_return_if_fail(self != NULL);
	
	gint cpt;
	
	for (cpt = 0; cpt < CCM_GENERAL_OPTION_N; ++cpt)
	{
		self->priv->general_options[cpt] = 
			ccm_config_new(-1, NULL, CCMGeneralOptions[cpt]);
	}
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
	{
		self->priv->screen_options[cpt] = 
			ccm_config_new(self->priv->screen_num, NULL, CCMScreenOptions[cpt]);

		if (cpt == CCM_SCREEN_USE_ROOT_BACKGROUND)
		{
			ccm_preferences_page_on_use_root_background_changed(self,
			                                                    self->priv->screen_options[cpt]);
			g_signal_connect_swapped(self->priv->screen_options[cpt],
			                         "changed",
			                         G_CALLBACK(ccm_preferences_page_on_use_root_background_changed),
			                         self);
		}
		if (cpt == CCM_SCREEN_BACKGROUND)
		{
			ccm_preferences_page_on_background_changed(self,
			                                           self->priv->screen_options[cpt]);
			g_signal_connect_swapped(self->priv->screen_options[cpt],
			                         "changed",
			                         G_CALLBACK(ccm_preferences_page_on_background_changed),
			                         self);
		}
	}
}

static void
ccm_preferences_page_on_section_changed(CCMPreferencesPage* self, 
										GtkTreeSelection* selection)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(selection != NULL);
	
	GtkNotebook* sections;
	GtkTreeModel* model;
	GtkTreeIter iter;
	GtkTreePath* path;
	gboolean active;
	gint page;
	
	sections = GTK_NOTEBOOK(gtk_builder_get_object(self->priv->builder, 
												   "sections"));
	if (!sections) return;
	
	gtk_tree_selection_get_selected(selection, &model, &iter);
	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_model_get(model, &iter, 0, &page, 3, &active, -1);
	
	if (!self->priv->last_section ||
		gtk_tree_path_compare(path, self->priv->last_section))
	{
		if (active)
		{
			gtk_notebook_set_current_page(sections, page);
			if (self->priv->last_section)
				gtk_tree_path_free(self->priv->last_section);
			self->priv->last_section = path;
		}
		else
		{
			gtk_tree_selection_select_path(selection, self->priv->last_section);
			gtk_tree_path_free(path);
		}
	}
}

static void
ccm_preferences_page_init_sections_list(CCMPreferencesPage* self)
{
	GtkTreeModel* sections_list;
	GtkTreeView* sections_view;
	GtkTreeSelection* selection;
	GtkTreeIter iter;
	
	sections_list = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														  "sections_list"));
	if (!sections_list) return;
	
	sections_view = GTK_TREE_VIEW(gtk_builder_get_object(self->priv->builder, 
														 "sections_view"));
	if (!sections_view) return;
	
	selection = gtk_tree_view_get_selection(sections_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(sections_list), 
								  &iter);
	gtk_tree_selection_select_iter(selection, &iter);
	g_signal_connect_swapped(selection, "changed", 
					G_CALLBACK(ccm_preferences_page_on_section_changed),
					self);
}

static void
ccm_preferences_page_on_plugin_edited(CCMPreferencesPage* self, 
									  gchar* path, gchar* text, 
									  CCMCellRendererExtension* cell)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(cell != NULL);
	
	GtkTreeView* plugins_view;
	GtkTreeViewColumn* plugins_column;
	GtkTreeIter iter, siter;
	GtkTreeModel* plugins_list;
	gboolean active, old_active;
	GtkTreeSelection* selection;
	
	g_object_get(G_OBJECT(cell), "enabled", &active, NULL);
	plugins_list = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														 "plugins_list"));
	if (!plugins_list) return;
	
	gtk_tree_model_get_iter_from_string(plugins_list, &iter, path);
	gtk_tree_model_get(plugins_list, &iter, 3, &old_active, -1);
	gtk_list_store_set(GTK_LIST_STORE(plugins_list), &iter, 3, active, -1);
	
	plugins_view = GTK_TREE_VIEW(gtk_builder_get_object(self->priv->builder, 
														"plugins_view"));
	if (!plugins_view) return;
	selection = gtk_tree_view_get_selection(plugins_view);
	
	plugins_column = 
		GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(self->priv->builder, 
													"plugins_column"));
	if (!plugins_column) return;
	
	if (old_active != active &&
		gtk_tree_selection_get_selected(selection, &plugins_list, &siter))
	{
		GtkTreePath* spath = gtk_tree_model_get_path(plugins_list, &siter);
		gtk_tree_view_set_cursor(plugins_view, spath, plugins_column, TRUE);
		gtk_tree_path_free(spath);
	}
}

static void
ccm_preferences_page_on_plugin_changed(CCMPreferencesPage* self, 
									   GtkTreeSelection* selection)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(selection != NULL);
	
	GtkTreeView* plugins_view;
	GtkTreeViewColumn* plugins_column;
	GtkTreeModel* model;
	GtkTreeIter iter;
	GtkTreePath* path;
	
	plugins_view = GTK_TREE_VIEW(gtk_builder_get_object(self->priv->builder, 
														"plugins_view"));
	if (!plugins_view) return;
	
	plugins_column = 
		GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(self->priv->builder, 
													"plugins_column"));
	if (!plugins_column) return;
	
	gtk_tree_selection_get_selected(selection, &model, &iter);
	path = gtk_tree_model_get_path(model, &iter);
	
	gtk_tree_view_set_cursor(plugins_view, path, plugins_column, TRUE);
	gtk_tree_path_free(path);
}

static void
ccm_preferences_page_on_plugins_modified(CCMPreferencesPage* self,
										 GtkTreePath* path,
										 GtkTreeIter* iter,
										 GtkTreeModel* plugins_list)
{
	gchar* name = NULL;
	gboolean active = FALSE;
	GSList* plugins, *item, *new = NULL;
	
	plugins = ccm_config_get_string_list(self->priv->screen_options[CCM_SCREEN_PLUGINS],
										 NULL);
	if (plugins)
	{
		gboolean found = FALSE, update = FALSE;
		
		gtk_tree_model_get(plugins_list, iter, 0, &name, 3, &active, -1);
		if (name)
		{
			for (item = plugins; item; item = item->next)
			{
				if (g_ascii_strcasecmp(name, item->data) || active)
				{
					new = g_slist_prepend(new, 
										  g_ascii_strdown(item->data, 
														 strlen(item->data)));
				}
				found |= !g_ascii_strcasecmp(name, item->data);
			}
			if (active && !found)
			{
				update = TRUE;
				new = g_slist_prepend(new, g_ascii_strdown(name, strlen(name)));
			}
			else if (!active && found)
				update = TRUE;
			if (update)
				ccm_config_set_string_list(
								self->priv->screen_options[CCM_SCREEN_PLUGINS],
								new, NULL);
			if (new)
			{
				g_slist_foreach(new, (GFunc)g_free, NULL);
				g_slist_free(new);
			}
			g_signal_emit(self, signals[PLUGIN_STATE_CHANGED], 0, name, active);
			g_free(name);
		}
		g_slist_foreach(plugins, (GFunc)g_free, NULL);
		g_slist_free(plugins);
	}
}

static void
ccm_preferences_page_init_plugins_list(CCMPreferencesPage* self)
{
	g_return_if_fail(self != NULL);
	
	GtkTreeView* plugins_view;
	GtkTreeSelection* selection;
	GtkTreeViewColumn* plugins_column;
	CCMCellRendererExtension* cell;
	GtkListStore* plugins_list;
	GSList* plugins, *item, *actives;
	
	plugins_view = GTK_TREE_VIEW(gtk_builder_get_object(self->priv->builder, 
														"plugins_view"));
	if (!plugins_view) return;
	
	selection = gtk_tree_view_get_selection(plugins_view);
	g_signal_connect_swapped(selection, "changed", 
					G_CALLBACK(ccm_preferences_page_on_plugin_changed),
					self);

	plugins_column = 
		GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(self->priv->builder, 
													"plugins_column"));
	if (!plugins_column) return;
	
	cell = ccm_cell_renderer_extension_new ();
	gtk_tree_view_column_pack_start(plugins_column, 
									GTK_CELL_RENDERER(cell), TRUE);
	gtk_tree_view_column_add_attribute(plugins_column,
									   GTK_CELL_RENDERER(cell),
									   "name", 0);
	gtk_tree_view_column_add_attribute(plugins_column,
									   GTK_CELL_RENDERER(cell),
									   "description", 1);
	gtk_tree_view_column_add_attribute(plugins_column,
									   GTK_CELL_RENDERER(cell),
									   "version", 2);
	gtk_tree_view_column_add_attribute(plugins_column,
									   GTK_CELL_RENDERER(cell),
									   "enabled", 3);
	g_signal_connect_swapped(cell, "edited", 
							 G_CALLBACK(ccm_preferences_page_on_plugin_edited), 
							 self);
	
	plugins_list = GTK_LIST_STORE(gtk_builder_get_object(self->priv->builder, 
														 "plugins_list"));
	if (!plugins_list) return;
	
	plugins = 
	ccm_extension_loader_get_screen_window_plugins (self->priv->plugin_loader);
	
	actives = 
	ccm_config_get_string_list(self->priv->screen_options[CCM_SCREEN_PLUGINS],
							   NULL);
	for (item = plugins; item; item = item->next)
	{
		CCMExtension* extension = item->data;
		GtkTreeIter iter;
		const gchar* name = ccm_extension_get_label (extension);
		const gchar* description = ccm_extension_get_description (extension);
		const gchar* version = ccm_extension_get_version (extension);
		gboolean active = FALSE;
		GSList* item_active;
		
		for (item_active = actives; item_active && !active; 
			 item_active = item_active->next)
			active |= !g_ascii_strcasecmp(name, item_active->data);
		
		gtk_list_store_append(plugins_list, &iter);
		gtk_list_store_set(plugins_list, &iter, 
						   0, name ? name : "",
						   1, description ? description : "",
						   2, version ? version : "",
						   3, active, -1);
		g_signal_emit(self, signals[PLUGIN_STATE_CHANGED], 0, name, active);
	}
	if (actives)
	{
		g_slist_foreach(actives, (GFunc)g_free, NULL);
		g_slist_free(actives);
	}
	g_slist_free(plugins);
	g_signal_connect_swapped(plugins_list, "row-changed",
							 G_CALLBACK(ccm_preferences_page_on_plugins_modified),
							 self);
}

static void
ccm_preferences_page_init_backends_list(CCMPreferencesPage* self)
{
	GtkListStore* backends;
	GtkTreeIter iter;
	
	backends = GTK_LIST_STORE(gtk_builder_get_object(self->priv->builder, 
													 "backends_list"));
	if (!backends) return;
#ifndef DISABLE_OPENVG_BACKEND
	gtk_list_store_prepend(backends, &iter);
	gtk_list_store_set(backends, &iter, 0, "openvg", 1, "OpenVG rendering", -1);
#endif
#ifndef DISABLE_GLITZ_BACKEND
	gtk_list_store_prepend(backends, &iter);
	gtk_list_store_set(backends, &iter, 0, "glitz", 1, "OpenGL rendering", -1);
#endif
#ifndef DISABLE_GLITZ_TFP_BACKEND
	gtk_list_store_prepend(backends, &iter);
	gtk_list_store_set(backends, &iter, 0, "glitz-tfp", 1, "OpenGL TFP rendering", -1);
#endif
#ifndef DISABLE_XRENDER_BACKEND
	gtk_list_store_prepend(backends, &iter);
	gtk_list_store_set(backends, &iter, 0, "xrender", 1, "X rendering", -1);
#endif
}

static void
ccm_preferences_page_on_composite_desktop_toggled(CCMPreferencesPage* self,
												  GtkToggleButton* button)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(button != NULL);
	
	GSList* unmanaged, *item;
		
	unmanaged = ccm_config_get_integer_list 
		(self->priv->general_options[CCM_GENERAL_UNMANAGED_SCREEN], NULL);
		
	if (gtk_toggle_button_get_active(button))
	{
		GSList* list = NULL;
			
		for (item = unmanaged; item; item = item->next)
			if (self->priv->screen_num != GPOINTER_TO_INT(item->data))
				list = g_slist_prepend(list, item->data);
		g_slist_free(unmanaged);
		ccm_config_set_integer_list 
			(self->priv->general_options[CCM_GENERAL_UNMANAGED_SCREEN], list,
			 NULL);
		g_slist_free(list);
	}
	else
	{
		unmanaged = g_slist_prepend(unmanaged, GINT_TO_POINTER(self->priv->screen_num));
		ccm_config_set_integer_list 
			(self->priv->general_options[CCM_GENERAL_UNMANAGED_SCREEN], 
			 unmanaged, NULL);
		g_slist_free(unmanaged);
	}
}

static void
ccm_preferences_page_change_backend(CCMPreferencesPage* self, gchar* name)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(name != NULL);
	
	if (!g_ascii_strcasecmp(name, "image"))
	{
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_PIXMAP], 
							   FALSE, NULL);
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_USE_BUFFERED], 
							   TRUE, NULL);
		ccm_config_set_string(self->priv->screen_options[CCM_SCREEN_BACKEND], 
							  "xrender", NULL);
	}
	else if (!g_ascii_strcasecmp(name, "xrender"))
	{
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_PIXMAP], 
							   TRUE, NULL);
		ccm_config_set_string(self->priv->screen_options[CCM_SCREEN_BACKEND], 
							  "xrender", NULL);
	}
	else if (!g_ascii_strcasecmp(name, "glitz-tfp"))
	{
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_PIXMAP], 
							   TRUE, NULL);
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_INDIRECT], 
							   TRUE, NULL);
		ccm_config_set_string(self->priv->screen_options[CCM_SCREEN_BACKEND], 
							  "glitz", NULL);
	}
	else if (!g_ascii_strcasecmp(name, "glitz"))
	{
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_PIXMAP], 
							   FALSE, NULL);
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_INDIRECT], 
							   FALSE, NULL);
		ccm_config_set_string(self->priv->screen_options[CCM_SCREEN_BACKEND], 
							  "glitz", NULL);
	}
	else if (!g_ascii_strcasecmp(name, "openvg"))
	{
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_PIXMAP], 
							   FALSE, NULL);
		ccm_config_set_boolean(self->priv->screen_options[CCM_SCREEN_INDIRECT], 
							   FALSE, NULL);
		ccm_config_set_string(self->priv->screen_options[CCM_SCREEN_BACKEND], 
							  "openvg", NULL);
	}
}

static void 
ccm_preferences_page_on_backend_need_restart(CCMPreferencesPage* self,
                                             gboolean restore_old,
                                             gchar* name)
{
	g_return_if_fail(self != NULL);

	if (restore_old)
	{
		GtkComboBox* combo;
		GtkTreeModel* backends;
		GtkTreeIter iter;

		g_free(name);
		combo = GTK_COMBO_BOX(gtk_builder_get_object(self->priv->builder, 
		                                             "backend"));
		if (!combo) return;
		backends = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														 "backends_list"));
		if (!backends) return;

		ccm_preferences_page_change_backend(self, self->priv->backend);

		g_signal_handlers_block_by_func(combo, 
		                                ccm_preferences_page_on_backend_changed,
		                                self);
		if (gtk_tree_model_get_iter_first(backends, &iter))
		{
			do 
			{
				gtk_tree_model_get(backends, &iter, 0, &name, -1);
				if (name && !g_ascii_strcasecmp(name, self->priv->backend))
				{
					gtk_combo_box_set_active_iter(combo, &iter);
					g_free (name);
					break;
				}
				if (name) g_free (name);
			} while (gtk_tree_model_iter_next(backends, &iter));
		}
		g_signal_handlers_unblock_by_func(combo, 
		                                  ccm_preferences_page_on_backend_changed,
		                                  self);
	}
	else
		self->priv->backend = name;
}

static void
ccm_preferences_page_on_backend_changed(CCMPreferencesPage* self,
										GtkComboBox* combo)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(combo != NULL);
	
	GtkTreeModel* model = gtk_combo_box_get_model(combo);
	GtkTreeIter iter;
	
	if (gtk_combo_box_get_active_iter(combo, &iter))
	{
		gchar* name;

		gtk_tree_model_get(model, &iter, 0, &name, -1);
		ccm_preferences_page_change_backend(self, name);
		
		ccm_preferences_page_need_restart (self, 
		                                   (CCMNeedRestartFunc)ccm_preferences_page_on_backend_need_restart, 
		                                   name);
	}
}

static void
ccm_preferences_page_on_refresh_rate_changed(CCMPreferencesPage* self,
											 GtkSpinButton* spin)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(spin != NULL);

	ccm_config_set_integer 
		(self->priv->screen_options[CCM_SCREEN_REFRESH_RATE], 
		 (gint)gtk_spin_button_get_value(spin),
		 NULL);
}

static void
ccm_preferences_page_on_file_chooser_response(CCMPreferencesPage* self,
                                              int response,
                                              GtkDialog* dialog)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gchar* filename;
		
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		ccm_config_set_string (self->priv->screen_options[CCM_SCREEN_BACKGROUND],
		                       filename, NULL);
		g_free(filename);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
ccm_preferences_page_on_button_background_clicked(CCMPreferencesPage* self,
                                                  GtkButton* button)
{
	GtkWidget* dialog;
	GtkFileFilter* filter;
	gchar* filename;

	filename = ccm_config_get_string(self->priv->screen_options[CCM_SCREEN_BACKGROUND], 
	                                 NULL);
	dialog = gtk_file_chooser_dialog_new("Open File",
				  						  NULL,
	                                     GTK_FILE_CHOOSER_ACTION_OPEN,
	                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                     NULL);
	filter = gtk_file_filter_new();
	gtk_file_filter_add_pixbuf_formats(filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), filename);
	g_free(filename);
	
	gtk_widget_show(dialog);
	g_signal_connect_swapped(G_OBJECT(dialog), "response",
	                         G_CALLBACK(ccm_preferences_page_on_file_chooser_response),
	                         self);
}

static void
impl_ccm_preferences_page_init_general_section(CCMPreferencesPagePlugin* plugin,
											   CCMPreferencesPage* self,
											   GtkWidget* widget)
{
	gboolean not_active = FALSE;
	GSList* unmanaged, *item;
	GtkToggleButton* button;
	GtkSpinButton* spin;
	gint refresh_rate;
	
	button = GTK_TOGGLE_BUTTON(gtk_builder_get_object(self->priv->builder,
													  "enable_composite_desktop"));
	if (!button) return;
	
	unmanaged = ccm_config_get_integer_list 
		(self->priv->general_options[CCM_GENERAL_UNMANAGED_SCREEN], NULL);
	
	for (item = unmanaged; item; item = item->next)
		not_active |= self->priv->screen_num == GPOINTER_TO_INT(item->data);
	g_slist_free(unmanaged);
	gtk_toggle_button_set_active(button, !not_active);
	
	g_signal_connect_swapped(button, "toggled", 
				G_CALLBACK(ccm_preferences_page_on_composite_desktop_toggled),
				self);
	
	ccm_preferences_page_init_backends_list(self);
	
	self->priv->backend = ccm_config_get_string 
		(self->priv->screen_options[CCM_SCREEN_BACKEND], NULL);
	if (self->priv->backend)
	{
		GtkComboBox* combo;
		GtkTreeModel* backends;
		GtkTreeIter iter;
		gboolean native;
		
		combo = GTK_COMBO_BOX(gtk_builder_get_object(self->priv->builder, 
													 "backend"));
		if (!combo) return;
		g_signal_connect_swapped(combo, "changed", 
					G_CALLBACK(ccm_preferences_page_on_backend_changed),
					self);
		
		backends = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														 "backends_list"));
		if (!backends) return;
		native = ccm_config_get_boolean
			(self->priv->screen_options[CCM_SCREEN_PIXMAP], NULL);
		
		if (!g_ascii_strcasecmp(self->priv->backend, "xrender") && !native)
		{
			g_free(self->priv->backend);
			self->priv->backend = g_strdup("image");
		}
		if (!g_ascii_strcasecmp(self->priv->backend, "glitz") && native)
		{
			g_free(self->priv->backend);
			self->priv->backend = g_strdup("glitz-tfp");
		}
		if (gtk_tree_model_get_iter_first(backends, &iter))
		{
			do 
			{
				gchar* name;
				gtk_tree_model_get(backends, &iter, 0, &name, -1);
				if (name && !g_ascii_strcasecmp(name, self->priv->backend))
				{
					gtk_combo_box_set_active_iter(combo, &iter);
					g_free (name);
					break;
				}
				if (name) g_free (name);
			} while (gtk_tree_model_iter_next(backends, &iter));
		}
	}
	
	refresh_rate = ccm_config_get_integer 
		(self->priv->screen_options[CCM_SCREEN_REFRESH_RATE], NULL);
	spin = GTK_SPIN_BUTTON(gtk_builder_get_object(self->priv->builder, 
												  "refresh_rate"));
	if (!spin) return;
	gtk_spin_button_set_value(spin, (gdouble)refresh_rate);
	
	g_signal_connect_swapped(spin, "value-changed", 
					G_CALLBACK(ccm_preferences_page_on_refresh_rate_changed),
					self);
	
	ccm_preferences_page_init_plugins_list(self);

	ccm_preferences_page_section_p(self,CCM_PREFERENCES_PAGE_SECTION_GENERAL);
}

static void
impl_ccm_preferences_page_init_desktop_section(CCMPreferencesPagePlugin* plugin,
											   CCMPreferencesPage* self,
											   GtkWidget* desktop_section)
{
	GtkWidget* widget = 
			GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "background"));
	if (widget)
	{
		gboolean use_root = 
			ccm_config_get_boolean (self->priv->screen_options[CCM_SCREEN_USE_ROOT_BACKGROUND], 
				                    NULL);
		gint screen_num = ccm_preferences_page_get_screen_num (self);
		
		gtk_box_pack_start(GTK_BOX(desktop_section), widget, 
		                   FALSE, TRUE, 0);

		ccm_preferences_page_section_register_widget (self,
		                                              CCM_PREFERENCES_PAGE_SECTION_DESKTOP,
		                                              widget, "general");
		
		CCMConfigCheckButton* use_root_background = 
			CCM_CONFIG_CHECK_BUTTON(gtk_builder_get_object(self->priv->builder, 
			                                               "use_root_background"));
		g_object_set(use_root_background, "screen", screen_num, NULL);

		GtkButton* button_background =
			GTK_BUTTON(gtk_builder_get_object(self->priv->builder, 
			                                  "background_button"));
		g_signal_connect_swapped(G_OBJECT(button_background),
		                         "clicked", 
		                         G_CALLBACK(ccm_preferences_page_on_button_background_clicked),
		                         self);
		ccm_preferences_page_on_background_changed (self, 
		                                            self->priv->screen_options[CCM_SCREEN_BACKGROUND]);
		
		CCMConfigColorButton* color_background = 
			CCM_CONFIG_COLOR_BUTTON(gtk_builder_get_object(self->priv->builder, 
			                                               "color_background"));
		g_object_set(color_background, "screen", screen_num, NULL);
		gtk_widget_set_sensitive(GTK_WIDGET(color_background), !use_root);

		widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
	                                           "background_button"));
		gtk_widget_set_sensitive(GTK_WIDGET(widget), !use_root);

		CCMConfigAdjustment* background_x = 
			CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
			                                             "background_x_adjustment"));
		g_object_set(background_x, "screen", screen_num, NULL);
		widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
	                                           "background_x"));
		gtk_widget_set_sensitive(GTK_WIDGET(widget), !use_root);

		CCMConfigAdjustment* background_y = 
			CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
			                                             "background_y_adjustment"));
		g_object_set(background_y, "screen", screen_num, NULL);
		widget = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
	                                           "background_y"));
		gtk_widget_set_sensitive(GTK_WIDGET(widget), !use_root);
	}
	
	ccm_preferences_page_section_p(self,CCM_PREFERENCES_PAGE_SECTION_DESKTOP);
}

static void
ccm_preferences_page_init_general_section(CCMPreferencesPage* self)
{
	GtkWidget* general_section;
	
	general_section = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
														"general_section"));
	if (!general_section) return;
	
	ccm_preferences_page_plugin_init_general_section (self->priv->plugin, self,
													  general_section);
}

static void
ccm_preferences_page_init_desktop_section(CCMPreferencesPage* self)
{
	GtkWidget* desktop_section;
	
	desktop_section = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
														"desktop_section"));
	if (!desktop_section) return;
	
	ccm_preferences_page_plugin_init_desktop_section (self->priv->plugin, self,
													  desktop_section);
}

static void
ccm_preferences_page_init_windows_section(CCMPreferencesPage* self)
{
	GtkWidget* windows_section;
	
	windows_section = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
														"windows_section"));
	if (!windows_section) return;
	
	ccm_preferences_page_plugin_init_windows_section (self->priv->plugin, self,
													  windows_section);
}

static void
ccm_preferences_page_init_effects_section(CCMPreferencesPage* self)
{
	GtkWidget* effects_section;
	
	effects_section = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
														"effects_section"));
	if (!effects_section) return;
	
	ccm_preferences_page_plugin_init_effects_section (self->priv->plugin, self,
													  effects_section);
}

static void
ccm_preferences_page_init_accessibility_section(CCMPreferencesPage* self)
{
	GtkWidget* accessibility_section;
	
	accessibility_section = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
															  "accessibility_section"));
	if (!accessibility_section) return;
	
	ccm_preferences_page_plugin_init_accessibility_section (self->priv->plugin, 
															self,
															accessibility_section);
}

static void
ccm_preferences_page_init_utilities_section(CCMPreferencesPage* self)
{
	GtkWidget* utilities_section;
	
	utilities_section = GTK_WIDGET(gtk_builder_get_object(self->priv->builder,
														  "utilities_section"));
	if (!utilities_section) return;
	
	ccm_preferences_page_plugin_init_utilities_section (self->priv->plugin, self,
														utilities_section);
}

static void
ccm_preferences_page_get_plugins(CCMPreferencesPage* self)
{
	g_return_if_fail(self != NULL);
	
	GSList* plugins = NULL, *item;
	
	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin))
		g_object_unref(self->priv->plugin);
	
	self->priv->plugin = (CCMPreferencesPagePlugin*)self;
	
	plugins = 
		ccm_extension_loader_get_preferences_plugins(self->priv->plugin_loader);
	for (item = plugins; item; item = item->next)
	{
		GType type = GPOINTER_TO_INT(item->data);
		GObject* prev = G_OBJECT(self->priv->plugin);
		CCMPreferencesPagePlugin* plugin = g_object_new(type, "parent", prev, NULL);
		
		if (plugin) self->priv->plugin = plugin;
	}
	if (plugins) g_slist_free(plugins);
}

CCMPreferencesPage*
ccm_preferences_page_new (gint screen_num)
{
	g_return_val_if_fail(screen_num >= 0, NULL);
	
	CCMPreferencesPage* self = g_object_new(CCM_TYPE_PREFERENCES_PAGE, NULL);
	
	self->priv->screen_num = screen_num;
	self->priv->builder = gtk_builder_new();
	gtk_builder_add_from_file(self->priv->builder, 
							  UI_DIR "/ccm-preferences.ui", NULL);
	
	self->priv->plugin_loader = ccm_extension_loader_new ();
	
	ccm_preferences_page_get_plugins(self);

	ccm_preferences_page_load_config(self);
	ccm_preferences_page_init_sections_list(self);
	ccm_preferences_page_init_desktop_section(self);
	ccm_preferences_page_init_windows_section(self);
	ccm_preferences_page_init_effects_section(self);
	ccm_preferences_page_init_accessibility_section(self);
	ccm_preferences_page_init_utilities_section(self);
	ccm_preferences_page_init_general_section(self);
	
	return self;
}

GtkWidget*
ccm_preferences_page_get_widget(CCMPreferencesPage* self)
{
	g_return_val_if_fail(self != NULL, NULL);	
	
	return GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "page"));
}

int
ccm_preferences_page_get_screen_num(CCMPreferencesPage* self)
{
	g_return_val_if_fail(self != NULL, -1);

	return self->priv->screen_num;
}

void
ccm_preferences_page_set_current_section(CCMPreferencesPage* self,
										 CCMPreferencesPageSection section)
{
	GtkTreeModel* sections_list;
	GtkTreeView* sections_view;
	GtkTreeSelection* selection;
	GtkTreeIter iter;
	
	sections_list = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														  "sections_list"));
	if (!sections_list) return;
	
	sections_view = GTK_TREE_VIEW(gtk_builder_get_object(self->priv->builder, 
														 "sections_view"));
	if (!sections_view) return;
	
	selection = gtk_tree_view_get_selection(sections_view);

	if (gtk_tree_model_get_iter_first(sections_list, &iter))
	{
		do 
		{
			gint page;

			gtk_tree_model_get(sections_list, &iter, 0, &page, -1);
			if (section == page)
			{
				gtk_tree_selection_select_iter(selection, &iter);
				break;
			}
		} while (gtk_tree_model_iter_next(sections_list, &iter));
	}
}

void
ccm_preferences_page_section_p(CCMPreferencesPage* self,
                               CCMPreferencesPageSection section)
{
	g_return_if_fail(self != NULL);

	GtkTreeModel* sections_list;
	GtkTreeIter iter;
	
	sections_list = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														  "sections_list"));
	if (!sections_list) return;

	if (gtk_tree_model_get_iter_first(sections_list, &iter))
	{
		do 
		{
			gint count, page;
			gboolean active;

			gtk_tree_model_get(sections_list, &iter, 0, &page, 
			                   3, &active, 4, &count, -1);
			if (section == page)
			{
				count++;
				active = count > 0;
				gtk_list_store_set(GTK_LIST_STORE(sections_list), &iter, 
				                   3, active, 4, count, -1);
			}
		} while (gtk_tree_model_iter_next(sections_list, &iter));
	}
}

void
ccm_preferences_page_section_v(CCMPreferencesPage* self, 
                               CCMPreferencesPageSection section)
{
	g_return_if_fail(self != NULL);

	GtkTreeModel* sections_list;
	GtkTreeIter iter;
	
	sections_list = GTK_TREE_MODEL(gtk_builder_get_object(self->priv->builder, 
														  "sections_list"));
	if (!sections_list) return;

	if (gtk_tree_model_get_iter_first(sections_list, &iter))
	{
		do 
		{
			gint count, page;
			gboolean active;

			gtk_tree_model_get(sections_list, &iter, 0, &page, 
			                   3, &active, 4, &count, -1);
			if (section == page)
			{
				count = MAX(count - 1, 0);
				active = count > 0;
				gtk_list_store_set(GTK_LIST_STORE(sections_list), &iter, 
				                   3, active, 4, count, -1);
			}
		} while (gtk_tree_model_iter_next(sections_list, &iter));
	}
}

static void
ccm_preferences_page_section_on_plugin_changed(CCMPreferencesPage* self, 
                                               gchar* name, gboolean active,
                                               GtkWidget* widget)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(widget != NULL);

	gchar* plugin = (gchar*)g_object_get_qdata(G_OBJECT(widget), 
	                                           CCM_WIDGET_PLUGIN_NAME);
	gboolean wactive = (gboolean)
		GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(widget), 
		                                   CCM_WIDGET_PLUGIN_ACTIVE));
	CCMPreferencesPageSection section = (CCMPreferencesPageSection)
		GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(widget), 
		                                   CCM_WIDGET_SECTION));
	
	if (plugin && !g_ascii_strcasecmp(name, plugin))
	{
		if (wactive != active)
		{
			if (active)
			{
				gtk_widget_show(widget);
				ccm_preferences_page_section_p(self, section);
			}
			else
			{
				gtk_widget_hide(widget);
				ccm_preferences_page_section_v(self, section);
			}
			g_object_set_qdata(G_OBJECT(widget), CCM_WIDGET_PLUGIN_ACTIVE, 
			                   GINT_TO_POINTER(active));
		}
	}
}

void
ccm_preferences_page_section_register_widget(CCMPreferencesPage* self,
                                             CCMPreferencesPageSection section,
                                             GtkWidget* widget,
                                             gchar* plugin)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(widget != NULL);
	g_return_if_fail(plugin != NULL);

	g_object_set_qdata_full(G_OBJECT(widget), CCM_WIDGET_PLUGIN_NAME, 
	                        g_strdup(plugin), g_free);
	g_object_set_qdata(G_OBJECT(widget), CCM_WIDGET_PLUGIN_ACTIVE, 
	                   GINT_TO_POINTER(TRUE));
	g_object_set_qdata(G_OBJECT(widget), CCM_WIDGET_SECTION, 
	                   GINT_TO_POINTER(section));
	
	ccm_preferences_page_section_p(self, section);
	g_signal_connect(G_OBJECT(self), "plugin-state-changed",
	                 G_CALLBACK(ccm_preferences_page_section_on_plugin_changed),
	                 widget);
}

void
ccm_preferences_page_need_restart (CCMPreferencesPage* self,
                                   CCMNeedRestartFunc func,
                                   gpointer data)
{
	g_return_if_fail(self != NULL);

	g_signal_emit(self, signals[NEED_RESTART], 0, func, data);
}
