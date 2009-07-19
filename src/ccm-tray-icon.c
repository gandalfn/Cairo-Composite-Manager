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
#include "ccm-config.h"
#include "ccm-display.h"
#include "ccm-tray-menu.h"
#include "ccm-tray-icon.h"

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

G_DEFINE_TYPE (CCMTrayIcon, ccm_tray_icon, G_TYPE_OBJECT);

struct _CCMTrayIconPrivate
{
    GtkStatusIcon *trayicon;
    CCMTrayMenu *traymenu;
    CCMConfig *config;
};

#define CCM_TRAY_ICON_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_TRAY_ICON, CCMTrayIconPrivate))

static void
ccm_tray_icon_init (CCMTrayIcon * self)
{
    self->priv = CCM_TRAY_ICON_GET_PRIVATE (self);
    self->priv->trayicon = NULL;
    self->priv->traymenu = NULL;
    self->priv->config = NULL;
}

static void
ccm_tray_icon_finalize (GObject * object)
{
    CCMTrayIcon *self = CCM_TRAY_ICON (object);

    if (self->priv->trayicon)
        g_object_unref (self->priv->trayicon);
    if (self->priv->traymenu)
        g_object_ref_sink (self->priv->traymenu);
    if (self->priv->config)
        g_object_unref (self->priv->config);

    G_OBJECT_CLASS (ccm_tray_icon_parent_class)->finalize (object);
}

static void
ccm_tray_icon_class_init (CCMTrayIconClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMTrayIconPrivate));

    object_class->finalize = ccm_tray_icon_finalize;
}

static void
ccm_tray_icon_on_popup_menu (CCMTrayIcon * self, guint button,
                             guint activate_timeout, GtkStatusIcon * tray_icon)
{
    gtk_menu_popup (GTK_MENU (self->priv->traymenu), NULL, NULL,
                    (GtkMenuPositionFunc) gtk_status_icon_position_menu,
                    tray_icon, button, activate_timeout);
}

static void
ccm_tray_icon_enable_ccm_changed (CCMTrayIcon * self, CCMConfig * config)
{
    gboolean enable = ccm_config_get_boolean (self->priv->config, NULL);
    GdkPixbuf *image =
        gdk_pixbuf_new_from_file (enable ? CCM_LOGO_ON : CCM_LOGO_OFF,
                                  NULL);

    gtk_status_icon_set_from_pixbuf (self->priv->trayicon, image);
}

CCMTrayIcon *
ccm_tray_icon_new (void)
{
    CCMTrayIcon *self = g_object_new (CCM_TYPE_TRAY_ICON, NULL);
    GdkPixbuf *image;
    gboolean val;

    /* Get config */
    self->priv->config = ccm_config_new (-1, NULL, "enable");
    g_signal_connect_swapped (self->priv->config, "changed",
                              (GCallback) ccm_tray_icon_enable_ccm_changed,
                              self);
    val = ccm_config_get_boolean (self->priv->config, NULL);

    /* Create tray menu */
    self->priv->traymenu = ccm_tray_menu_new ();
    image = gdk_pixbuf_new_from_file (val ? CCM_LOGO_ON : CCM_LOGO_OFF, NULL);

    /* Create tray icon */
    self->priv->trayicon = gtk_status_icon_new_from_pixbuf (image);
    g_signal_connect_swapped (self->priv->trayicon, "popup-menu",
                              (GCallback) ccm_tray_icon_on_popup_menu, self);

    return self;
}
