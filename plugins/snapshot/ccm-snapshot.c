/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
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
#include "ccm-snapshot.h"
#include "ccm-snapshot-dialog.h"
#include "ccm-keybind.h"
#include "ccm-debug.h"
#include "ccm-marshallers.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-color-button.h"
#include "ccm-config-entry-shortcut.h"
#include "ccm.h"

enum
{
    SAVE,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

enum
{
    CCM_SNAPSHOT_AREA,
    CCM_SNAPSHOT_WINDOW,
    CCM_SNAPSHOT_COLOR,
    CCM_SNAPSHOT_OPTION_N
};

static const gchar *CCMSnapshotOptionKeys[CCM_SNAPSHOT_OPTION_N] = {
    "area",
    "window",
    "color"
};

typedef struct
{
    CCMPluginOptions parent;

    gchar* area_shortcut;
    gchar* window_shortcut;

    GdkColor *color;
} CCMSnapshotOptions;

static void ccm_snapshot_screen_iface_init (CCMScreenPluginClass * iface);
static void
ccm_snapshot_preferences_page_iface_init (CCMPreferencesPagePluginClass *
                                          iface);
static void ccm_snapshot_on_option_changed (CCMPlugin * plugin, int index);

CCM_DEFINE_PLUGIN_WITH_OPTIONS (CCMSnapshot, ccm_snapshot, CCM_TYPE_PLUGIN,
                                CCM_IMPLEMENT_INTERFACE (ccm_snapshot,
                                                         CCM_TYPE_SCREEN_PLUGIN,
                                                         ccm_snapshot_screen_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_snapshot,
                                                         CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                                         ccm_snapshot_preferences_page_iface_init))
struct _CCMSnapshotPrivate
{
    CCMScreen* screen;

    CCMKeybind *area_keybind;
    CCMKeybind *window_keybind;

    cairo_rectangle_t area;
    CCMWindow* selected;

    GtkBuilder* builder;
};

#define CCM_SNAPSHOT_GET_PRIVATE(o)  \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SNAPSHOT, CCMSnapshotPrivate))

static void
ccm_snapshot_options_init (CCMSnapshotOptions* self)
{
    self->area_shortcut = NULL;
    self->window_shortcut = NULL;
    self->color = NULL;
}

static void
ccm_snapshot_options_finalize (CCMSnapshotOptions* self)
{
    if (self->color) g_free (self->color);
    self->color = NULL;
    if (self->area_shortcut) g_free (self->area_shortcut);
    self->area_shortcut = NULL;
    if (self->window_shortcut) g_free (self->window_shortcut);
    self->window_shortcut = NULL;
}

static void
ccm_snapshot_options_changed (CCMSnapshotOptions* self, CCMConfig* config)
{
    GError *error = NULL;

    if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                CCM_SNAPSHOT_AREA))
    {
        if (self->area_shortcut) g_free (self->area_shortcut);

        self->area_shortcut = ccm_config_get_string (config, &error);
        if (error)
        {
            g_warning ("Error on get snapshot area shortcut configuration value");
            g_error_free (error);
            self->area_shortcut = g_strdup ("<Super>Button1");
        }
    }

    if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                CCM_SNAPSHOT_WINDOW))
    {
        if (self->window_shortcut) g_free (self->window_shortcut);

        self->window_shortcut = ccm_config_get_string (config, &error);
        if (error)
        {
            g_warning ("Error on get snapshot area shortcut configuration value");
            g_error_free (error);
            self->window_shortcut = g_strdup ("<Super><Alt>Button1");
        }
    }

    if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                CCM_SNAPSHOT_COLOR))
    {
        if (self->color) g_free (self->color);

        self->color = ccm_config_get_color (config, &error);
        if (error)
        {
            g_warning ("Error on get snapshot select color configuration value");
            g_error_free (error);
        }
    }
}

static void
ccm_snapshot_init (CCMSnapshot * self)
{
    self->priv = CCM_SNAPSHOT_GET_PRIVATE (self);
    self->priv->screen = NULL;
    self->priv->area.x = 0;
    self->priv->area.y = 0;
    self->priv->area.width = 0;
    self->priv->area.height = 0;
    self->priv->selected = NULL;
    self->priv->area_keybind = NULL;
    self->priv->window_keybind = NULL;
    self->priv->builder = NULL;
}

static void
ccm_snapshot_finalize (GObject * object)
{
    CCMSnapshot *self = CCM_SNAPSHOT (object);

    ccm_plugin_options_unload (CCM_PLUGIN (self));

    if (self->priv->area_keybind)
        g_object_unref (self->priv->area_keybind);
    self->priv->area_keybind = NULL;

    if (self->priv->window_keybind)
        g_object_unref (self->priv->window_keybind);
    self->priv->window_keybind = NULL;

    if (self->priv->builder)
        g_object_unref (self->priv->builder);
    self->priv->builder = NULL;

    G_OBJECT_CLASS (ccm_snapshot_parent_class)->finalize (object);
}

static void
ccm_snapshot_class_init (CCMSnapshotClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMSnapshotPrivate));

    object_class->finalize = ccm_snapshot_finalize;

    signals[SAVE] =
        g_signal_new ("save", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      ccm_cclosure_marshal_STRING__POINTER, G_TYPE_STRING, 1,
                      G_TYPE_POINTER);
}

static void
ccm_snapshot_on_area_key_press (CCMSnapshot * self)
{
    gint x, y;

    ccm_screen_query_pointer (self->priv->screen, NULL, &x, &y);
    self->priv->area.x = x;
    self->priv->area.y = y;
    self->priv->area.width = 0;
    self->priv->area.height = 0;
    ccm_debug ("AREA PRESS %i, %i", x, y);
}

static void
ccm_snapshot_on_area_key_motion (CCMSnapshot * self, gint x, gint y)
{
    gdouble x1, y1, x2, y2;
    CCMRegion *damage;

    damage = ccm_region_rectangle (&self->priv->area);
    ccm_region_offset (damage, -4, -4);
    ccm_region_resize (damage, self->priv->area.width + 8,
                       self->priv->area.height + 8);
    ccm_screen_damage_region (self->priv->screen, damage);
    ccm_region_destroy (damage);

    //    x   x1     x2
    // y  +
    // y1     +------+
    //        |      |
    // y2     +------+
    x1 = x <= self->priv->area.x ? x : self->priv->area.x;
    y1 = y <= self->priv->area.y ? y : self->priv->area.y;
    x2 = x <=
        self->priv->area.x ? self->priv->area.x + self->priv->area.width : x;
    y2 = y <=
        self->priv->area.y ? self->priv->area.y + self->priv->area.height : y;
    self->priv->area.x = x1;
    self->priv->area.y = y1;
    self->priv->area.width = x2 - x1;
    self->priv->area.height = y2 - y1;

    damage = ccm_region_rectangle (&self->priv->area);
    ccm_region_offset (damage, -4, -4);
    ccm_region_resize (damage, self->priv->area.width + 8,
                       self->priv->area.height + 8);
    ccm_screen_damage_region (self->priv->screen, damage);
    ccm_region_destroy (damage);

    ccm_debug ("AREA MOTION %f,%f,%f,%f", self->priv->area.x,
               self->priv->area.y, self->priv->area.width,
               self->priv->area.height);
}

static void
ccm_snapshot_on_area_key_release (CCMSnapshot * self)
{
    gint x, y;
    gdouble x1, y1, x2, y2;
    CCMWindow *overlay = ccm_screen_get_overlay_window (self->priv->screen);
    cairo_surface_t *src = ccm_drawable_get_surface (CCM_DRAWABLE (overlay));
    cairo_surface_t *dst;
    cairo_t *ctx;
    gboolean saved;
    CCMRegion *damage;

    damage = ccm_region_rectangle (&self->priv->area);
    ccm_region_offset (damage, -4, -4);
    ccm_region_resize (damage, self->priv->area.width + 8,
                       self->priv->area.height + 8);
    ccm_screen_damage_region (self->priv->screen, damage);
    ccm_region_destroy (damage);

    ccm_screen_query_pointer (self->priv->screen, NULL, &x, &y);
    x1 = x <= self->priv->area.x ? x : self->priv->area.x;
    y1 = y <= self->priv->area.y ? y : self->priv->area.y;
    x2 = x <=
        self->priv->area.x ? self->priv->area.x + self->priv->area.width : x;
    y2 = y <=
        self->priv->area.y ? self->priv->area.y + self->priv->area.height : y;
    self->priv->area.x = x1;
    self->priv->area.y = y1;
    self->priv->area.width = x2 - x1;
    self->priv->area.height = y2 - y1;

    if (self->priv->area.width > 10 && self->priv->area.height > 10)
    {
        damage = ccm_region_rectangle (&self->priv->area);
        ccm_region_offset (damage, -4, -4);
        ccm_region_resize (damage, self->priv->area.width + 8,
                           self->priv->area.height + 8);
        ccm_screen_damage_region (self->priv->screen, damage);
        ccm_region_destroy (damage);

        ccm_debug ("AREA RELEASE %f,%f,%f,%f", self->priv->area.x,
                   self->priv->area.y, self->priv->area.width,
                   self->priv->area.height);

        dst =
            cairo_image_surface_create (ccm_drawable_get_format
                                        (CCM_DRAWABLE (overlay)),
                                        self->priv->area.width,
                                        self->priv->area.height);
        ctx = cairo_create (dst);
        cairo_set_source_surface (ctx, src, -self->priv->area.x,
                                  -self->priv->area.y);
        cairo_paint (ctx);
        cairo_destroy (ctx);

        g_signal_emit (self, signals[SAVE], 0, dst, &saved);
        if (!saved)
        {
            CCMSnapshotDialog *dialog;
            dialog = ccm_snapshot_dialog_new (dst, self->priv->screen);
        }

        cairo_surface_destroy (src);
    }

    self->priv->area.width = 0;
    self->priv->area.height = 0;
}

static void
ccm_snapshot_on_window_key_press (CCMSnapshot * self)
{
    CCMWindow *window = NULL;
    gint x, y;

    self->priv->selected = NULL;
    self->priv->area.width = 0;
    self->priv->area.height = 0;

    if (ccm_screen_query_pointer (self->priv->screen, &window, &x, &y)
        && window)
    {
        if (ccm_window_is_viewable (window))
        {
            self->priv->selected = window;
            ccm_drawable_damage (CCM_DRAWABLE (self->priv->selected));
            ccm_debug ("WINDOW PRESS %s",
                       ccm_window_get_name (self->priv->selected));
        }
    }
}

static void
ccm_snapshot_on_window_key_release (CCMSnapshot * self)
{
    if (self->priv->selected)
    {
        CCMPixmap *pixmap = ccm_window_get_pixmap (self->priv->selected);

        ccm_drawable_damage (CCM_DRAWABLE (self->priv->selected));
        if (pixmap)
        {
            cairo_surface_t *src =
                ccm_drawable_get_surface (CCM_DRAWABLE (pixmap));
            cairo_surface_t *dst;
            cairo_t *ctx;
            const cairo_rectangle_t *area;
            cairo_rectangle_t clipbox;
            gboolean saved = FALSE;

            ccm_debug ("WINDOW RELEASE %s",
                       ccm_window_get_name (self->priv->selected));
            area = ccm_window_get_area (self->priv->selected);
            ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE
                                                      (self->priv->selected),
                                                      &clipbox);

            dst =
                cairo_image_surface_create (ccm_drawable_get_format
                                            (CCM_DRAWABLE
                                             (self->priv->selected)),
                                            area->width, area->height);
            ctx = cairo_create (dst);
            cairo_set_source_surface (ctx, src, clipbox.x - area->x,
                                      clipbox.y - area->y);
            cairo_paint (ctx);
            cairo_destroy (ctx);

            g_signal_emit (self, signals[SAVE], 0, dst, &saved);
            if (!saved)
            {
                CCMSnapshotDialog *dialog;
                dialog = ccm_snapshot_dialog_new (dst, self->priv->screen);
            }
            cairo_surface_destroy (src);
            g_object_unref (pixmap);
        }

        self->priv->selected = NULL;
    }
}

static void
ccm_snapshot_get_area_keybind (CCMSnapshot * self)
{
    if (self->priv->area_keybind)
        g_object_unref (self->priv->area_keybind);

    self->priv->area_keybind = ccm_keybind_new (self->priv->screen, 
                                                ccm_snapshot_get_option (self)->area_shortcut, 
                                                TRUE);

    g_signal_connect_swapped (self->priv->area_keybind,
                              "key_press",
                              G_CALLBACK (ccm_snapshot_on_area_key_press),
                              self);
    g_signal_connect_swapped (self->priv->area_keybind,
                              "key_release",
                              G_CALLBACK (ccm_snapshot_on_area_key_release),
                              self);
    g_signal_connect_swapped (self->priv->area_keybind,
                              "key_motion",
                              G_CALLBACK (ccm_snapshot_on_area_key_motion),
                              self);
}

static void
ccm_snapshot_get_window_keybind (CCMSnapshot * self)
{
    if (self->priv->window_keybind)
        g_object_unref (self->priv->window_keybind);

    self->priv->window_keybind = ccm_keybind_new (self->priv->screen, 
                                                  ccm_snapshot_get_option (self)->window_shortcut, 
                                                  TRUE);
    g_signal_connect_swapped (self->priv->window_keybind,
                              "key_press",
                              G_CALLBACK (ccm_snapshot_on_window_key_press),
                              self);
    g_signal_connect_swapped (self->priv->window_keybind,
                              "key_release",
                              G_CALLBACK (ccm_snapshot_on_window_key_release),
                              self);
}

static void
ccm_snapshot_on_option_changed (CCMPlugin * plugin, int index)
{
    CCMSnapshot *self = CCM_SNAPSHOT (plugin);

    switch (index)
    {
        case CCM_SNAPSHOT_AREA:
            ccm_snapshot_get_area_keybind (self);
            break;
        case CCM_SNAPSHOT_WINDOW:
            ccm_snapshot_get_window_keybind (self);
            break;
        default:
            break;
    }
}

static void
ccm_snapshot_screen_load_options (CCMScreenPlugin * plugin, CCMScreen * screen)
{
    CCMSnapshot *self = CCM_SNAPSHOT (plugin);

    self->priv->screen = screen;

    ccm_plugin_options_load (CCM_PLUGIN (self), "snapshot",
                             CCMSnapshotOptionKeys, CCM_SNAPSHOT_OPTION_N,
                             ccm_snapshot_on_option_changed);

    ccm_screen_plugin_load_options (CCM_SCREEN_PLUGIN_PARENT (plugin), screen);

}

static gboolean
ccm_snapshot_screen_paint (CCMScreenPlugin * plugin, CCMScreen * screen,
                           cairo_t * context)
{
    CCMSnapshot *self = CCM_SNAPSHOT (plugin);
    gboolean ret;

    ret =
        ccm_screen_plugin_paint (CCM_SCREEN_PLUGIN_PARENT (plugin), screen,
                                 context);

    if (self->priv->area.height > 0 || self->priv->area.width > 0)
    {
        cairo_save (context);
        cairo_reset_clip (context);
        cairo_set_line_width (context, 4);
        if (ccm_snapshot_get_option (self)->color)
            gdk_cairo_set_source_color (context,
                                        ccm_snapshot_get_option (self)->color);
        else
            cairo_set_source_rgb (context, 1, 1, 1);
        cairo_rectangle (context, self->priv->area.x - 2,
                         self->priv->area.y - 2, self->priv->area.width + 4,
                         self->priv->area.height + 4);
        cairo_stroke (context);
        cairo_restore (context);
    }
    if (self->priv->selected)
    {
        cairo_path_t *path;
        const cairo_rectangle_t *area;

        area = ccm_window_get_area (self->priv->selected);
        cairo_save (context);
        cairo_rectangle (context, area->x, area->y, area->width, area->height);
        cairo_clip (context);

        path =
            ccm_drawable_get_geometry_path (CCM_DRAWABLE (self->priv->selected),
                                            context);
        if (ccm_snapshot_get_option (self)->color)
            cairo_set_source_rgba (context,
                                   (double) ccm_snapshot_get_option (self)->
                                   color->red / 65535.0f,
                                   (double) ccm_snapshot_get_option (self)->
                                   color->green / 65535.0f,
                                   (double) ccm_snapshot_get_option (self)->
                                   color->blue / 65535.0f, 0.5);
        else
            cairo_set_source_rgba (context, 1, 1, 1, 0.5);
        cairo_append_path (context, path);
        cairo_fill (context);
        cairo_restore (context);
        cairo_path_destroy (path);
    }

    return ret;
}

static void
ccm_snapshot_preferences_page_init_utilities_section (CCMPreferencesPagePlugin *
                                                      plugin,
                                                      CCMPreferencesPage *
                                                      preferences,
                                                      GtkWidget *
                                                      utilities_section)
{
    CCMSnapshot *self = CCM_SNAPSHOT (plugin);

    if (!self->priv->builder)
    {
        self->priv->builder = gtk_builder_new ();
        if (!gtk_builder_add_from_file
            (self->priv->builder, UI_DIR "/ccm-snapshot.ui", NULL))
        {
            g_object_unref (self->priv->builder);
            self->priv->builder = NULL;
        }
    }

    if (self->priv->builder)
    {
        GtkWidget *widget =
            GTK_WIDGET (gtk_builder_get_object
                        (self->priv->builder, "snapshot"));
        if (widget)
        {
            gint screen_num = ccm_preferences_page_get_screen_num (preferences);

            gtk_box_pack_start (GTK_BOX (utilities_section), widget, FALSE,
                                TRUE, 0);

            CCMConfigEntryShortcut *area =
                CCM_CONFIG_ENTRY_SHORTCUT (gtk_builder_get_object
                                           (self->priv->builder,
                                            "area"));
            g_object_set (area, "screen", screen_num, NULL);

            CCMConfigEntryShortcut *window =
                CCM_CONFIG_ENTRY_SHORTCUT (gtk_builder_get_object
                                           (self->priv->builder,
                                            "window"));
            g_object_set (window, "screen", screen_num, NULL);

            CCMConfigColorButton *color =
                CCM_CONFIG_COLOR_BUTTON (gtk_builder_get_object
                                         (self->priv->builder,
                                          "color"));
            g_object_set (color, "screen", screen_num, NULL);

            ccm_preferences_page_section_register_widget (preferences,
                                                          CCM_PREFERENCES_PAGE_SECTION_UTILITIES,
                                                          widget, "snapshot");
        }
    }
    ccm_preferences_page_plugin_init_utilities_section
        (CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin), preferences,
         utilities_section);
}

static void
ccm_snapshot_screen_iface_init (CCMScreenPluginClass * iface)
{
    iface->load_options = ccm_snapshot_screen_load_options;
    iface->paint = ccm_snapshot_screen_paint;
    iface->add_window = NULL;
    iface->remove_window = NULL;
    iface->damage = NULL;
}

static void
ccm_snapshot_preferences_page_iface_init (CCMPreferencesPagePluginClass * iface)
{
    iface->init_general_section = NULL;
    iface->init_desktop_section = NULL;
    iface->init_windows_section = NULL;
    iface->init_effects_section = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section = ccm_snapshot_preferences_page_init_utilities_section;
}
