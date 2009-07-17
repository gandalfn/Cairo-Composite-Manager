/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
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

#include <X11/Xatom.h>

#include "ccm-drawable.h"
#include "ccm-config.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-opacity.h"
#include "ccm-keybind.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-adjustment.h"
#include "ccm-config-entry-shortcut.h"
#include "ccm.h"

enum
{
	CCM_OPACITY_OPACITY,
	CCM_OPACITY_INCREASE,
	CCM_OPACITY_DECREASE,
	CCM_OPACITY_STEP,
	CCM_OPACITY_OPTION_N
};

static const gchar* CCMOpacityOptionKeys[CCM_OPACITY_OPTION_N] = {
	"opacity",
	"increase",
	"decrease",
	"step"
};

typedef struct 
{
	CCMPluginOptions parent;

	CCMKeybind*	increase;
	CCMKeybind*	decrease;
	gfloat		step;

	gfloat		opacity;
} CCMOpacityOptions;

static void ccm_opacity_screen_iface_init  (CCMScreenPluginClass* iface);
static void ccm_opacity_window_iface_init  (CCMWindowPluginClass* iface);
static void ccm_opacity_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface);
static void ccm_opacity_on_property_changed(CCMOpacity* self, 
											CCMPropertyType changed,
											CCMWindow* window);
static void ccm_opacity_on_option_changed  (CCMPlugin* plugin, 
                                            CCMConfig* config);

CCM_DEFINE_PLUGIN (CCMOpacity, ccm_opacity, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_opacity,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_opacity_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_opacity,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_opacity_window_iface_init);
                   CCM_IMPLEMENT_INTERFACE(ccm_opacity,
                                           CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                           ccm_opacity_preferences_page_iface_init))

struct _CCMOpacityPrivate
{	
	CCMScreen*	screen;
	
	CCMWindow*	window;

	GtkBuilder* builder;

	gulong		id_property_changed;
};

#define CCM_OPACITY_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_OPACITY, CCMOpacityPrivate))

static CCMPluginOptions*
ccm_opacity_options_init(CCMPlugin* plugin)
{
	CCMOpacityOptions* options = g_slice_new0(CCMOpacityOptions);
	
	options->increase = NULL;
	options->decrease = NULL;
	options->step = 0.1f;
	options->opacity = 1.0;

	return (CCMPluginOptions*)options;
}

static void
ccm_opacity_options_finalize(CCMPlugin* plugin, CCMPluginOptions* opts)
{
	CCMOpacityOptions* options = (CCMOpacityOptions*)opts;
	
	if (options->increase) g_object_unref(options->increase);
	options->increase = NULL;
	if (options->decrease) g_object_unref(options->decrease);
	options->decrease = NULL;
	g_slice_free(CCMOpacityOptions, options);
}

static void
ccm_opacity_init (CCMOpacity *self)
{
	self->priv = CCM_OPACITY_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->window = NULL;
	self->priv->builder = NULL;
	self->priv->id_property_changed = 0;
}

static void
ccm_opacity_finalize (GObject *object)
{
	CCMOpacity* self = CCM_OPACITY(object);
	
	if (CCM_IS_WINDOW(self->priv->window) && 
		G_OBJECT(self->priv->window)->ref_count &&
	    self->priv->id_property_changed)
			g_signal_handler_disconnect(self->priv->window, 
										self->priv->id_property_changed);	
	self->priv->window = NULL;
	self->priv->id_property_changed = 0;
	
	ccm_plugin_options_unload(CCM_PLUGIN(self));
	
	if (self->priv->builder) g_object_unref(self->priv->builder);
	self->priv->builder = NULL;
	
	G_OBJECT_CLASS (ccm_opacity_parent_class)->finalize (object);
}

static void
ccm_opacity_class_init (CCMOpacityClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMOpacityPrivate));

	CCM_PLUGIN_CLASS(klass)->options_init = ccm_opacity_options_init;
	CCM_PLUGIN_CLASS(klass)->options_finalize = ccm_opacity_options_finalize;
	CCM_PLUGIN_CLASS(klass)->option_changed = ccm_opacity_on_option_changed;
	
	object_class->finalize = ccm_opacity_finalize;
}

static void
ccm_opacity_change_opacity(CCMWindow* window, gfloat value)
{
	g_return_if_fail(window != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	Window child;
	guint32 opacity = value * 0xffffffff;
	
	g_object_get(G_OBJECT(window), "child", &child, NULL);
	
	if (value == 1.0f)
	{
		if (child)
			XDeleteProperty (CCM_DISPLAY_XDISPLAY(display), child, 
							 CCM_WINDOW_GET_CLASS(window)->opacity_atom);
		XDeleteProperty (CCM_DISPLAY_XDISPLAY(display), 
		                 CCM_WINDOW_XWINDOW(window), 
		                 CCM_WINDOW_GET_CLASS(window)->opacity_atom);
	}
	else
	{
		if (child)
			XChangeProperty(CCM_DISPLAY_XDISPLAY(display), child, 
							CCM_WINDOW_GET_CLASS(window)->opacity_atom, 
							XA_CARDINAL, 32, PropModeReplace, 
							(unsigned char *) &opacity, 1L);
		XChangeProperty(CCM_DISPLAY_XDISPLAY(display), 
						CCM_WINDOW_XWINDOW(window), 
						CCM_WINDOW_GET_CLASS(window)->opacity_atom, 
						XA_CARDINAL, 32, PropModeReplace, 
						(unsigned char *) &opacity, 1L);
	}

	ccm_display_flush(display);
}

static void
ccm_opacity_on_increase_key_press(CCMOpacity* self)
{
	CCMWindow* window = NULL;
	gint x, y;
	
	if (ccm_screen_query_pointer (self->priv->screen, &window, &x, &y) &&
		window)
	{
		if (ccm_window_is_viewable (window))
		{
			gfloat opacity = ccm_window_get_opacity (window);
				
			opacity += ccm_opacity_get_option(self)->step;
			if (opacity > 1) opacity = 1.0;
			ccm_opacity_change_opacity (window, opacity);
		}
	}
}

static void
ccm_opacity_on_decrease_key_press(CCMOpacity* self)
{
	CCMWindow* window = NULL;
	gint x, y;
	
	if (ccm_screen_query_pointer (self->priv->screen, &window, &x, &y) &&
		window)
	{
		if (ccm_window_is_viewable (window))
		{
			gfloat opacity = ccm_window_get_opacity (window);
				
			opacity -= ccm_opacity_get_option(self)->step;
			if (opacity < 0.1) opacity = 0.1;
			ccm_opacity_change_opacity (window, opacity);
		}
	}
}

static gboolean
ccm_opacity_get_opacity_step(CCMOpacity* self)
{
	GError* error = NULL;
	gfloat real_step;
	gfloat step;
	
	real_step = ccm_config_get_float (ccm_opacity_get_config(self, CCM_OPACITY_STEP), 
	                                  &error);
	if (error)
	{
		g_warning("Error on get opacity step configuration value");
		g_error_free(error);
		real_step = 0.1;
	}
	step = MAX(0.01f, real_step);
	step = MIN(0.5f, real_step);
	if (ccm_opacity_get_option(self)->step != step)
	{
		ccm_opacity_get_option(self)->step = step;
		if (real_step != step)
			ccm_config_set_float (ccm_opacity_get_config(self, CCM_OPACITY_STEP), 
								  step, NULL);
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_opacity_get_opacity(CCMOpacity* self)
{
	GError* error = NULL;
	gfloat real_opacity;
	gfloat opacity;
	
	real_opacity = 
		ccm_config_get_float (ccm_opacity_get_config(self, CCM_OPACITY_OPACITY), 
		                      &error);
	if (error)
	{
		g_warning("Error on get opacity configuration value");
		g_error_free(error);
		real_opacity = 0.85f;
	}
	opacity = MAX(0.1f, real_opacity);
	opacity = MIN(1.0f, real_opacity);
	if (ccm_opacity_get_option(self)->opacity != opacity)
	{
		ccm_opacity_get_option(self)->opacity = opacity;
		if (opacity != real_opacity)
			ccm_config_set_float (ccm_opacity_get_config(self, CCM_OPACITY_OPACITY), 
								  opacity, NULL);
		return TRUE;
	}
	
	return FALSE;
}

static void
ccm_opacity_get_increase_keybind(CCMOpacity* self)
{
	if (self->priv->screen)
	{
		GError* error = NULL;
		gchar* shortcut = NULL;

		if (ccm_opacity_get_option(self)->increase) 
			g_object_unref(ccm_opacity_get_option(self)->increase);
	
		shortcut = 
			ccm_config_get_string(ccm_opacity_get_config(self, CCM_OPACITY_INCREASE), 
								  &error);
		if (error)
		{
			g_warning("Error on get opacity shortcut configuration value");
			g_error_free(error);
			shortcut = g_strdup("<Super>Button4");
		}
		ccm_opacity_get_option(self)->increase = 
			ccm_keybind_new(self->priv->screen, shortcut, TRUE);
		g_free(shortcut);
		g_signal_connect_swapped(ccm_opacity_get_option(self)->increase, "key_press", 
								 G_CALLBACK(ccm_opacity_on_increase_key_press), 
								 self);
	}
}

static void
ccm_opacity_get_decrease_keybind(CCMOpacity* self)
{
	if (self->priv->screen)
	{
		GError* error = NULL;
		gchar* shortcut = NULL;

		if (ccm_opacity_get_option(self)->decrease) 
			g_object_unref(ccm_opacity_get_option(self)->decrease);

		shortcut = 
			ccm_config_get_string(ccm_opacity_get_config(self, CCM_OPACITY_DECREASE),
					              &error);
		if (error)
		{
			g_warning("Error on get opacity shortcut configuration value");
			g_error_free(error);
			shortcut = g_strdup("<Super>Button5");
		}
		ccm_opacity_get_option(self)->decrease = 
			ccm_keybind_new(self->priv->screen, shortcut, TRUE);
		g_free(shortcut);
		g_signal_connect_swapped(ccm_opacity_get_option(self)->decrease, "key_press", 
								 G_CALLBACK(ccm_opacity_on_decrease_key_press), 
								 self);
	}
}

static void
ccm_opacity_on_property_changed(CCMOpacity* self, CCMPropertyType changed,
								CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	if (changed == CCM_PROPERTY_HINT_TYPE)
	{
		CCMWindowType type = ccm_window_get_hint_type(window);
		
		if (type == CCM_WINDOW_TYPE_MENU ||
			type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
			type == CCM_WINDOW_TYPE_POPUP_MENU)
		{
			ccm_opacity_change_opacity(window, 
			                           ccm_opacity_get_option(self)->opacity);
		}
	}
}

static void
ccm_opacity_on_option_changed(CCMPlugin* plugin, CCMConfig* config)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	
	if (config == ccm_opacity_get_config(self, CCM_OPACITY_INCREASE))
		ccm_opacity_get_increase_keybind (self);
	else if (config == ccm_opacity_get_config(self, CCM_OPACITY_DECREASE))
		ccm_opacity_get_decrease_keybind (self);
	else if (config == ccm_opacity_get_config(self, CCM_OPACITY_STEP))
		ccm_opacity_get_opacity_step (self);
	else if (config == ccm_opacity_get_config(self, CCM_OPACITY_OPACITY) &&
			 ccm_opacity_get_opacity (self) && self->priv->window)
	{
		ccm_opacity_on_property_changed (self, CCM_PROPERTY_HINT_TYPE, 
										 self->priv->window);
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
}

static void
ccm_opacity_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	
	self->priv->screen = screen;
	
	ccm_plugin_options_load(CCM_PLUGIN(self), "opacity", CCMOpacityOptionKeys,
	                        CCM_OPACITY_OPTION_N);
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
}

static void
ccm_opacity_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	
	self->priv->window = window;

	ccm_plugin_options_load(CCM_PLUGIN(self), "opacity", CCMOpacityOptionKeys,
	                        CCM_OPACITY_OPTION_N);
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->id_property_changed = 
		g_signal_connect_swapped(self->priv->window, "property-changed",
								 G_CALLBACK(ccm_opacity_on_property_changed), 
								 self);
	ccm_opacity_on_property_changed (self, CCM_PROPERTY_HINT_TYPE, window);
}

static void
ccm_opacity_preferences_page_init_effects_section(CCMPreferencesPagePlugin* plugin,
                                                  CCMPreferencesPage* preferences,
                                                  GtkWidget* effects_section)
{
	CCMOpacity* self = CCM_OPACITY(plugin);
	
	if (!self->priv->builder)
	{
		self->priv->builder = gtk_builder_new();
		if (!gtk_builder_add_from_file(self->priv->builder, 
		                               UI_DIR "/ccm-opacity.ui", NULL))
		{
			g_object_unref(self->priv->builder);
			self->priv->builder = NULL;
		}
	}

	if (self->priv->builder)
	{
		GtkWidget* widget = 
			GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "menu-opacity"));
		if (widget)
		{
			gint screen_num = ccm_preferences_page_get_screen_num (preferences);
			
			gtk_box_pack_start(GTK_BOX(effects_section), widget, 
			                   FALSE, TRUE, 0);

			CCMConfigAdjustment* opacity = 
				CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
				                                             "opacity-adjustment"));
			g_object_set(opacity, "screen", screen_num, NULL);
			
			ccm_preferences_page_section_register_widget (preferences,
			                                              CCM_PREFERENCES_PAGE_SECTION_EFFECTS,
			                                              widget, "opacity");
		}
	}
	ccm_preferences_page_plugin_init_effects_section (CCM_PREFERENCES_PAGE_PLUGIN_PARENT(plugin),
													  preferences, effects_section);
}

static void
ccm_opacity_preferences_page_init_utilities_section(CCMPreferencesPagePlugin* plugin,
                                                    CCMPreferencesPage* preferences,
                                                    GtkWidget* utilities_section)
{
	CCMOpacity* self = CCM_OPACITY(plugin);

	if (!self->priv->builder)
	{
		self->priv->builder = gtk_builder_new();
		if (!gtk_builder_add_from_file(self->priv->builder, 
		                               UI_DIR "/ccm-opacity.ui", NULL))
		{
			g_object_unref(self->priv->builder);
			self->priv->builder = NULL;
		}
	}

	if (self->priv->builder)
	{
		GtkWidget* widget = 
			GTK_WIDGET(gtk_builder_get_object(self->priv->builder, "opacity"));
		if (widget)
		{
			gint screen_num = ccm_preferences_page_get_screen_num (preferences);
			
			gtk_box_pack_start(GTK_BOX(utilities_section), widget, 
			                   FALSE, TRUE, 0);

			CCMConfigEntryShortcut* increase = 
				CCM_CONFIG_ENTRY_SHORTCUT(gtk_builder_get_object(self->priv->builder, 
				                                                 "increase"));
			g_object_set(increase, "screen", screen_num, NULL);
			
			CCMConfigEntryShortcut* decrease = 
				CCM_CONFIG_ENTRY_SHORTCUT(gtk_builder_get_object(self->priv->builder, 
				                                                 "decrease"));
			g_object_set(decrease, "screen", screen_num, NULL);

			CCMConfigAdjustment* step = 
				CCM_CONFIG_ADJUSTMENT(gtk_builder_get_object(self->priv->builder, 
				                                             "step-adjustment"));
			g_object_set(step, "screen", screen_num, NULL);
			
			ccm_preferences_page_section_register_widget (preferences,
			                                              CCM_PREFERENCES_PAGE_SECTION_UTILITIES,
			                                              widget, "opacity");
		}
	}
	ccm_preferences_page_plugin_init_utilities_section (CCM_PREFERENCES_PAGE_PLUGIN_PARENT(plugin),
	                                                    preferences, utilities_section);
}

static void
ccm_opacity_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_opacity_screen_load_options;
	iface->paint 			= NULL;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
	iface->damage			= NULL;
}

static void
ccm_opacity_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = ccm_opacity_window_load_options;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = NULL;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
	iface->get_origin		 = NULL;
}

static void
ccm_opacity_preferences_page_iface_init(CCMPreferencesPagePluginClass* iface)
{
	iface->init_general_section       = NULL;
    iface->init_desktop_section       = NULL;
    iface->init_windows_section       = NULL;
    iface->init_effects_section		  = ccm_opacity_preferences_page_init_effects_section;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section     = ccm_opacity_preferences_page_init_utilities_section;
}

