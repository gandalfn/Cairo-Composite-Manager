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

#include <config.h>

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-display.h"
#include "ccm-preferences.h"
#include "ccm-tray-menu.h"

/*
 * Standard gettext macros.
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CCM_LOGO_PIXMAP 	PACKAGE_PIXMAP_DIR "/cairo-compmgr.png"
#define CCM_LOGO_ON		 	PACKAGE_PIXMAP_DIR "/cairo-compmgr-on-24.png"
#define CCM_LOGO_OFF		PACKAGE_PIXMAP_DIR "/cairo-compmgr-off-24.png"

G_DEFINE_TYPE (CCMTrayMenu, ccm_tray_menu, GTK_TYPE_MENU);

struct _CCMTrayMenuPrivate
{
	CCMDisplay* 	display;
	GtkWidget* 		ccm_menu;
	GtkWidget* 		preferences_menu;
	GtkWidget* 		about_menu;
	GtkWidget* 		quit_menu;
	CCMConfig* 		config;
	CCMPreferences* preferences;
};

#define CCM_TRAY_MENU_GET_PRIVATE(o) \
	((CCMTrayMenuPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_TRAY_MENU, CCMTrayMenuClass))

static void
ccm_tray_menu_init (CCMTrayMenu *self)
{
	self->priv = CCM_TRAY_MENU_GET_PRIVATE(self);
	self->priv->display = NULL;
	self->priv->ccm_menu = NULL;
	self->priv->preferences_menu = NULL;
	self->priv->about_menu = NULL;
	self->priv->quit_menu = NULL;
	self->priv->config = NULL;
	self->priv->preferences = NULL;
}

static void
ccm_tray_menu_finalize (GObject *object)
{
	CCMTrayMenu* self = CCM_TRAY_MENU(object);
	
	if (self->priv->display) g_object_unref(self->priv->display);
	if (self->priv->config) g_object_unref(self->priv->config);
	if (self->priv->preferences) g_object_unref(self->priv->preferences);
	
	G_OBJECT_CLASS (ccm_tray_menu_parent_class)->finalize (object);
}

static void
ccm_tray_menu_class_init (CCMTrayMenuClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMTrayMenuPrivate));

	object_class->finalize = ccm_tray_menu_finalize;
}


static void
ccm_tray_menu_ccm_menu_activate (CCMTrayMenu* self, GtkWidget* ccm_menu)
{
	gboolean val = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(ccm_menu));
	
	ccm_debug("CCM ACTIVATE %i", val);
	
	ccm_config_set_boolean (self->priv->config, val, NULL);
}

static void
ccm_tray_menu_preferences_menu_activate (CCMTrayMenu* self, 
										 GtkWidget* preferences_menu)
{
	ccm_preferences_show (self->priv->preferences);
}

static void
ccm_tray_menu_about_menu_activate (CCMTrayMenu* self, GtkWidget* about_menu)
{
	GtkWidget * about_dialog;
	GdkPixbuf * logo = gdk_pixbuf_new_from_file(CCM_LOGO_PIXMAP, NULL);
		
	/* Create About dialog */
	about_dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_name((GtkAboutDialog *)about_dialog, 
							  "Cairo Composite Manager");
	gtk_about_dialog_set_version((GtkAboutDialog *)about_dialog, 
								  VERSION);
	gtk_about_dialog_set_comments((GtkAboutDialog *)about_dialog, 
								  _("Cairo Composite Manager"));		
	gtk_about_dialog_set_copyright((GtkAboutDialog *)about_dialog, "Copyright (c) Nicolas Bruguier");
	gtk_about_dialog_set_logo((GtkAboutDialog *)about_dialog, logo);
		
	gtk_dialog_run(GTK_DIALOG(about_dialog));
	gtk_widget_destroy(about_dialog);
}

static void
ccm_tray_menu_quit_menu_activate (CCMTrayMenu* self, GtkWidget* ccm_menu)
{
	gtk_main_quit ();
}

static void
ccm_tray_menu_enable_ccm_changed (CCMTrayMenu * self, CCMConfig* config)
{
	gboolean val = ccm_config_get_boolean (config, NULL);
	
	ccm_debug("CCM ENABLE %i", val);
		
	if (val)
	{
		if (!self->priv->display) self->priv->display = ccm_display_new(NULL);
	}
	else if (self->priv->display) 
	{
		ccm_debug("UNSET CCM");
		g_object_unref(self->priv->display);
		self->priv->display = NULL;
	}
}

static gboolean
ccm_tray_menu_on_preferences_reload (CCMTrayMenu * self, CCMPreferences* preferences)
{
	gboolean ret = FALSE;
	
	if (self->priv->display)
	{
		g_object_unref(self->priv->display);
		self->priv->display = ccm_display_new(NULL);
		ret = TRUE;
	}

	return ret;
}

CCMTrayMenu*
ccm_tray_menu_new (void)
{
	CCMTrayMenu * self = g_object_new(CCM_TYPE_TRAY_MENU, NULL);
	GtkWidget * separator;
	gboolean val;
		
	/* Get config */
	self->priv->config = ccm_config_new (-1, NULL, "enable");
	g_signal_connect_swapped(self->priv->config, "changed", 
							 (GCallback)ccm_tray_menu_enable_ccm_changed, self);
	val = ccm_config_get_boolean (self->priv->config, NULL);
	
	/* Create composite menu */
	self->priv->ccm_menu = gtk_check_menu_item_new_with_label(_("Composite desktop"));
	g_signal_connect_swapped(GTK_WIDGET(self->priv->ccm_menu), "activate", 
							 (GCallback)ccm_tray_menu_ccm_menu_activate, self);
	gtk_menu_shell_append(GTK_MENU_SHELL(self), self->priv->ccm_menu);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(self->priv->ccm_menu),
								   val);
		
	/* Get preferences dialog */
	self->priv->preferences = ccm_preferences_new ();
	g_signal_connect_swapped(self->priv->preferences,  "reload",
	                         G_CALLBACK(ccm_tray_menu_on_preferences_reload),
	                         self);
	
	/* Create preferences menu */
	self->priv->preferences_menu = 
		gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	g_signal_connect_swapped(GTK_WIDGET(self->priv->preferences_menu), 
							 "activate", 
							 (GCallback)ccm_tray_menu_preferences_menu_activate, 
							 self);
	gtk_menu_shell_append(GTK_MENU_SHELL(self), self->priv->preferences_menu);

	/* Create separator */
	separator = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(self), separator);

	/* Create about menu */
	self->priv->about_menu = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT,
														 		NULL);
	g_signal_connect_swapped(GTK_WIDGET(self->priv->about_menu), "activate", 
							 (GCallback)ccm_tray_menu_about_menu_activate, self);
	gtk_menu_shell_append(GTK_MENU_SHELL(self), self->priv->about_menu);
		
	/* Create quit menu */
	self->priv->quit_menu = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT,
															   NULL);
	g_signal_connect_swapped(GTK_WIDGET(self->priv->quit_menu), "activate", 
							 (GCallback)ccm_tray_menu_quit_menu_activate, self);
	gtk_menu_shell_append(GTK_MENU_SHELL(self), self->priv->quit_menu);
	
	gtk_widget_show_all(GTK_WIDGET(self));
		
	/* Start ccm if enabled */
	if (val) self->priv->display = ccm_display_new (NULL);
	
	return self;
}
