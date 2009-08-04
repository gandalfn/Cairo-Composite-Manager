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

#include <math.h>
#include <string.h>

#include "ccm-property-async.h"
#include "ccm-drawable.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-pixmap.h"
#include "ccm-shadow.h"
#include "ccm-config.h"
#include "ccm-cairo-utils.h"
#include "ccm-debug.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-adjustment.h"
#include "ccm-config-color-button.h"
#include "ccm.h"

enum
{
    CCM_SHADOW_SIDE_TOP,
    CCM_SHADOW_SIDE_RIGHT,
    CCM_SHADOW_SIDE_BOTTOM,
    CCM_SHADOW_SIDE_LEFT,
    CCM_SHADOW_SIDE_N
};

enum
{
    CCM_SHADOW_REAL_BLUR,
    CCM_SHADOW_OFFSET,
    CCM_SHADOW_RADIUS,
    CCM_SHADOW_SIGMA,
    CCM_SHADOW_COLOR,
    CCM_SHADOW_ALPHA,
    CCM_SHADOW_OPTION_N
};

static const gchar *CCMShadowOptionKeys[CCM_SHADOW_OPTION_N] = {
    "real_blur",
    "offset",
    "radius",
    "sigma",
    "color",
    "alpha"
};

typedef struct
{
    CCMPluginOptions parent;

    gboolean real_blur;
    int offset;
    int radius;
    double sigma;
    GdkColor *color;
    double alpha;
} CCMShadowOptions;

static GQuark CCMShadowPixmapQuark;
static GQuark CCMShadowQuark;

static void ccm_shadow_on_property_changed (CCMShadow * self,
                                            CCMPropertyType changed,
                                            CCMWindow * window);
static void ccm_shadow_on_event (CCMShadow * self, XEvent * event);
static void ccm_shadow_on_option_changed (CCMPlugin * plugin,
                                          CCMConfig * config);

static void ccm_shadow_window_iface_init (CCMWindowPluginClass * iface);
static void ccm_shadow_screen_iface_init (CCMScreenPluginClass * iface);
static void
ccm_shadow_preferences_page_iface_init (CCMPreferencesPagePluginClass * iface);

CCM_DEFINE_PLUGIN_WITH_OPTIONS (CCMShadow, ccm_shadow, CCM_TYPE_PLUGIN,
                                CCM_IMPLEMENT_INTERFACE (ccm_shadow, CCM_TYPE_SCREEN_PLUGIN,
                                                         ccm_shadow_screen_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_shadow, CCM_TYPE_WINDOW_PLUGIN,
                                                         ccm_shadow_window_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_shadow,
                                                         CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                                         ccm_shadow_preferences_page_iface_init))
struct _CCMShadowPrivate
{
    CCMScreen *screen;

    gboolean force_disable;
    gboolean force_enable;

    guint id_check;

    CCMWindow *window;

	CCMPixmap *pixmap;
    CCMPixmap *shadow;
    cairo_surface_t *shadow_image;

    CCMRegion *geometry;

    GtkBuilder *builder;

    gulong id_event;
    gulong id_property_changed;
};

#define CCM_SHADOW_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SHADOW, CCMShadowPrivate))


static void
ccm_shadow_options_init (CCMShadowOptions * self)
{
    self->real_blur = FALSE;
    self->offset = 0;
    self->radius = 14;
    self->sigma = 7;
    self->color = NULL;
    self->alpha = 0.6;
}

static void
ccm_shadow_options_finalize (CCMShadowOptions* self)
{
    if (self->color) g_free (self->color);
    self->color = NULL;
}

static void
ccm_shadow_init (CCMShadow * self)
{
    self->priv = CCM_SHADOW_GET_PRIVATE (self);

    self->priv->screen = NULL;
    self->priv->force_disable = FALSE;
    self->priv->force_enable = FALSE;
    self->priv->id_check = 0;
    self->priv->window = NULL;
    self->priv->shadow = NULL;
    self->priv->shadow_image = NULL;
    self->priv->geometry = NULL;
    self->priv->builder = NULL;
    self->priv->id_event = 0;
    self->priv->id_property_changed = 0;
}

static void
ccm_shadow_finalize (GObject * object)
{
    CCMShadow *self = CCM_SHADOW (object);

    ccm_plugin_options_unload (CCM_PLUGIN (self));

    if (CCM_IS_SCREEN (self->priv->screen)
        && G_OBJECT (self->priv->screen)->ref_count)
    {
        CCMDisplay *display = ccm_screen_get_display (self->priv->screen);

        g_signal_handler_disconnect (display, self->priv->id_event);
    }
    self->priv->screen = NULL;

    if (CCM_IS_WINDOW (self->priv->window)
        && G_OBJECT (self->priv->window)->ref_count)
        g_signal_handler_disconnect (self->priv->window,
                                     self->priv->id_property_changed);
    self->priv->window = NULL;

    if (self->priv->id_check)
        g_source_remove (self->priv->id_check);

    if (self->priv->geometry)
    {
        ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = NULL;
    }

    if (self->priv->shadow)
        g_object_unref (self->priv->shadow);
	self->priv->shadow = NULL;
	
	if (self->priv->pixmap)
		g_object_unref (self->priv->pixmap);	
	self->priv->pixmap = NULL;
	
    if (self->priv->shadow_image)
        cairo_surface_destroy (self->priv->shadow_image);

    if (self->priv->builder)
        g_object_unref (self->priv->builder);

    G_OBJECT_CLASS (ccm_shadow_parent_class)->finalize (object);
}

static void
ccm_shadow_class_init (CCMShadowClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMShadowPrivate));

	CCMShadowPixmapQuark = g_quark_from_static_string("CCMShadowPixmap");
	CCMShadowQuark = g_quark_from_static_string("CCMShadow");
	
    klass->shadow_disable_atom = None;
    klass->shadow_enable_atom = None;

    object_class->finalize = ccm_shadow_finalize;
}

static gboolean
ccm_shadow_need_shadow (CCMShadow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    CCMWindow *window = self->priv->window;
    CCMWindowType type = ccm_window_get_hint_type (window);
    const CCMRegion *opaque = ccm_window_get_opaque_region (window);

    return self->priv->force_enable || (!self->priv->force_disable
                                        && !ccm_window_is_fullscreen (window)
                                        && !ccm_window_is_input_only (window)
                                        && (ccm_window_is_decorated (window)
                                            || (type != CCM_WINDOW_TYPE_NORMAL
                                                && type !=
                                                CCM_WINDOW_TYPE_DIALOG
                                                && opaque))
                                        &&
                                        ((type == CCM_WINDOW_TYPE_DOCK
                                          && opaque)
                                         || type != CCM_WINDOW_TYPE_DOCK)
                                        && (ccm_window_is_managed (window)
                                            || type == CCM_WINDOW_TYPE_DOCK
                                            || type ==
                                            CCM_WINDOW_TYPE_DROPDOWN_MENU
                                            || type ==
                                            CCM_WINDOW_TYPE_POPUP_MENU
                                            || type == CCM_WINDOW_TYPE_TOOLTIP
                                            || type == CCM_WINDOW_TYPE_MENU
                                            || type == CCM_WINDOW_TYPE_SPLASH));
}

static gboolean
ccm_shadow_check_needed (CCMShadow * self)
{
    g_return_val_if_fail (CCM_IS_SHADOW (self), FALSE);

    if (self->priv->window && !ccm_shadow_need_shadow (self)
        && self->priv->geometry)
    {
        if (self->priv->shadow_image)
            cairo_surface_destroy (self->priv->shadow_image);
        self->priv->shadow_image = NULL;

        if (self->priv->pixmap)
            g_object_unref (self->priv->pixmap);
        self->priv->pixmap = NULL;

        if (self->priv->geometry)
            ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = NULL;

        ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));
        ccm_drawable_query_geometry (CCM_DRAWABLE (self->priv->window));
        ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));
    }
    else if (ccm_shadow_need_shadow (self) && !self->priv->geometry)
    {
        ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));
        ccm_drawable_query_geometry (CCM_DRAWABLE (self->priv->window));
        ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));
    }

    self->priv->id_check = 0;

    return FALSE;
}

static void
ccm_shadow_on_get_shadow_property (CCMShadow * self, guint n_items,
                                   gchar * result, CCMPropertyASync * prop)
{
    g_return_if_fail (CCM_IS_PROPERTY_ASYNC (prop));

    if (!CCM_IS_SHADOW (self))
    {
        g_object_unref (prop);
        return;
    }

    Atom property = ccm_property_async_get_property (prop);

    if (result)
    {
        if (property == CCM_SHADOW_GET_CLASS (self)->shadow_enable_atom)
        {
            gulong enable;
            gboolean force_enable;
            memcpy (&enable, result, sizeof (gulong));

            ccm_debug_window (self->priv->window, "ENABLE SHADOW = %i", enable);
            force_enable = enable == 0 ? FALSE : TRUE;
            if (force_enable != self->priv->force_enable)
            {
                if (!self->priv->id_check)
                    self->priv->id_check =
                        g_idle_add ((GSourceFunc) ccm_shadow_check_needed,
                                    self);
                self->priv->force_enable = force_enable;
            }
        }
        else if (property == CCM_SHADOW_GET_CLASS (self)->shadow_disable_atom)
        {
            gulong disable;
            gboolean force_disable;
            memcpy (&disable, result, sizeof (gulong));

            ccm_debug_window (self->priv->window, "DISABLE SHADOW = %i",
                              disable);
            force_disable = disable == 0 ? FALSE : TRUE;
            if (force_disable != self->priv->force_disable)
            {
                if (!self->priv->id_check)
                    self->priv->id_check =
                        g_idle_add ((GSourceFunc) ccm_shadow_check_needed,
                                    self);
                self->priv->force_disable = force_disable;
            }
        }
    }

    g_object_unref (prop);
}

static void
ccm_shadow_query_force_shadow (CCMShadow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (self->priv->window != NULL);

    CCMDisplay *display =
        ccm_drawable_get_display (CCM_DRAWABLE (self->priv->window));

    Window child = _ccm_window_get_child(self->priv->window);

    if (!child)
    {
        ccm_debug_window (self->priv->window, "QUERY SHADOW 0x%x", child);
        CCMPropertyASync *prop = ccm_property_async_new (display,
                                                         CCM_WINDOW_XWINDOW
                                                         (self->priv->window),
                                                         CCM_SHADOW_GET_CLASS
                                                         (self)->
                                                         shadow_enable_atom,
                                                         XA_CARDINAL, 32);

        g_signal_connect (prop, "error", G_CALLBACK (g_object_unref), NULL);
        g_signal_connect_swapped (prop, "reply",
                                  G_CALLBACK
                                  (ccm_shadow_on_get_shadow_property), self);
    }
    else
    {
        ccm_debug_window (self->priv->window, "QUERY CHILD SHADOW 0x%x", child);
        CCMPropertyASync *prop = ccm_property_async_new (display, child,
                                                         CCM_SHADOW_GET_CLASS
                                                         (self)->
                                                         shadow_enable_atom,
                                                         XA_CARDINAL, 32);

        g_signal_connect (prop, "error", G_CALLBACK (g_object_unref), NULL);
        g_signal_connect_swapped (prop, "reply",
                                  G_CALLBACK
                                  (ccm_shadow_on_get_shadow_property), self);
    }
}

static void
ccm_shadow_query_avoid_shadow (CCMShadow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (self->priv->window != NULL);

    CCMDisplay *display =
        ccm_drawable_get_display (CCM_DRAWABLE (self->priv->window));

    Window child = _ccm_window_get_child(self->priv->window);

    if (!child)
    {
        ccm_debug_window (self->priv->window, "QUERY SHADOW 0x%x", child);
        CCMPropertyASync *prop = ccm_property_async_new (display,
                                                         CCM_WINDOW_XWINDOW
                                                         (self->priv->window),
                                                         CCM_SHADOW_GET_CLASS
                                                         (self)->
                                                         shadow_disable_atom,
                                                         XA_CARDINAL, 32);

        g_signal_connect (prop, "error", G_CALLBACK (g_object_unref), NULL);
        g_signal_connect_swapped (prop, "reply",
                                  G_CALLBACK
                                  (ccm_shadow_on_get_shadow_property), self);
    }
    else
    {
        ccm_debug_window (self->priv->window, "QUERY CHILD SHADOW 0x%x", child);
        CCMPropertyASync *prop = ccm_property_async_new (display, child,
                                                         CCM_SHADOW_GET_CLASS
                                                         (self)->
                                                         shadow_disable_atom,
                                                         XA_CARDINAL, 32);

        g_signal_connect (prop, "error", G_CALLBACK (g_object_unref), NULL);
        g_signal_connect_swapped (prop, "reply",
                                  G_CALLBACK
                                  (ccm_shadow_on_get_shadow_property), self);
    }
}

static void
ccm_shadow_create_atoms (CCMShadow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_SHADOW_GET_CLASS (self) != NULL);

    CCMShadowClass *klass = CCM_SHADOW_GET_CLASS (self);

    if (!klass->shadow_enable_atom || !klass->shadow_disable_atom)
    {
        CCMDisplay *display =
            ccm_drawable_get_display (CCM_DRAWABLE (self->priv->window));

        klass->shadow_disable_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_CCM_SHADOW_DISABLED",
                         False);
        klass->shadow_enable_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_CCM_SHADOW_ENABLED",
                         False);
    }
}

static void
ccm_shadow_create_fake_shadow (CCMShadow * self)
{
    g_return_if_fail (self != NULL);

    cairo_surface_t *tmp;
    cairo_t *cr;
    CCMRegion *opaque = ccm_region_copy (self->priv->geometry);
    CCMRegion *clip;
    cairo_rectangle_t *rects;
    gint cpt, nb_rects;
    cairo_rectangle_t clipbox;
    cairo_path_t *path, *clip_path;
    gint border = ccm_shadow_get_option (self)->radius * 2;
    cairo_surface_t *surface =
        ccm_drawable_get_surface (CCM_DRAWABLE (self->priv->shadow));

    ccm_region_get_clipbox (self->priv->geometry, &clipbox);

    ccm_region_offset (opaque,
                       -clipbox.x + ccm_shadow_get_option (self)->radius,
                       -clipbox.y + ccm_shadow_get_option (self)->radius);

    clip = ccm_region_rectangle (&clipbox);
    ccm_region_resize (clip,
                       clipbox.width + ccm_shadow_get_option (self)->radius * 2,
                       clipbox.height +
                       ccm_shadow_get_option (self)->radius * 2);
    ccm_region_offset (clip, -clipbox.x, -clipbox.y);
    ccm_region_subtract (clip, opaque);

    // Create tmp surface for shadow
    tmp =
        cairo_surface_create_similar (surface, CAIRO_CONTENT_ALPHA,
                                      clipbox.width + border,
                                      clipbox.height + border);
    cr = cairo_create (tmp);
    ccm_region_get_rectangles (clip, &rects, &nb_rects);
    for (cpt = 0; cpt < nb_rects; ++cpt)
        cairo_rectangle (cr, rects[cpt].x, rects[cpt].y, rects[cpt].width,
                         rects[cpt].height);

    clip_path = cairo_copy_path (cr);
    cairo_rectangles_free (rects, nb_rects);
    ccm_region_destroy (clip);
    cairo_new_path (cr);

    ccm_region_get_rectangles (opaque, &rects, &nb_rects);
    for (cpt = 0; cpt < nb_rects; ++cpt)
        cairo_rectangle (cr, rects[cpt].x, rects[cpt].y, rects[cpt].width,
                         rects[cpt].height);

    path = cairo_copy_path (cr);
    if (rects)
        cairo_rectangles_free (rects, nb_rects);
    ccm_region_destroy (opaque);
    cairo_destroy (cr);
    cairo_surface_destroy (tmp);

    // Create shadow surface
    self->priv->shadow_image =
        cairo_blur_path (surface, path, clip_path,
                         ccm_shadow_get_option (self)->radius, 1,
                         clipbox.width + border, clipbox.height + border);
    cairo_surface_destroy (surface);

    cairo_path_destroy (path);
    cairo_path_destroy (clip_path);
}

static void
ccm_shadow_create_blur_shadow (CCMShadow * self)
{
    g_return_if_fail (self != NULL);

    cairo_surface_t *tmp, *side;
    cairo_t *cr;
    CCMRegion *opaque = ccm_region_copy (self->priv->geometry);
    cairo_rectangle_t *rects;
    gint cpt, nb_rects;
    cairo_rectangle_t clipbox;
    gint border = ccm_shadow_get_option (self)->radius * 2;

    ccm_region_get_clipbox (self->priv->geometry, &clipbox);

    ccm_region_offset (opaque,
                       -clipbox.x + ccm_shadow_get_option (self)->radius,
                       -clipbox.y + ccm_shadow_get_option (self)->radius);

    // Create tmp surface for shadow
    tmp =
        cairo_image_surface_create (CAIRO_FORMAT_A8, clipbox.width + border,
                                    clipbox.height + border);
    cr = cairo_create (tmp);
    cairo_set_source_rgba (cr, 0, 0, 0, 1);
    ccm_region_get_rectangles (opaque, &rects, &nb_rects);
    for (cpt = 0; cpt < nb_rects; ++cpt)
        cairo_rectangle (cr, rects[cpt].x, rects[cpt].y, rects[cpt].width,
                         rects[cpt].height);
    cairo_fill (cr);
    cairo_rectangles_free (rects, nb_rects);
    ccm_region_destroy (opaque);
    cairo_destroy (cr);

    /* Create shadow surface */
    self->priv->shadow_image =
        cairo_image_surface_create (CAIRO_FORMAT_A8, clipbox.width + border,
                                    clipbox.height + border);
    cr = cairo_create (self->priv->shadow_image);

    /* top side */
    //    0   b                                           w  w+b
    // 0 -+---+-------------------------------------------+---+---
    //      +                                               +
    // b      +<----------------------------------------->+
    //                            w - b
    side =
        cairo_image_surface_blur (tmp, ccm_shadow_get_option (self)->radius,
                                  ccm_shadow_get_option (self)->sigma, 0, 0,
                                  clipbox.width + border, border);
    cairo_save (cr);
    cairo_rectangle (cr, border, 0, clipbox.width - border, border);
    cairo_move_to (cr, border, 0);
    cairo_line_to (cr, 0, 0);
    cairo_line_to (cr, border, border);
    cairo_move_to (cr, clipbox.width, 0);
    cairo_line_to (cr, clipbox.width + border, 0);
    cairo_line_to (cr, clipbox.width, border);
    cairo_clip (cr);
    cairo_set_source_surface (cr, side, 0, 0);
    cairo_paint (cr);
    cairo_restore (cr);
    cairo_surface_destroy (side);

    /* right side */
    //       w  w+b
    // 0 ----+   +  
    //         + |
    // b     +   + 
    //       |   |
    //       |   | h - b
    //       |   |
    // h     +   +
    //         + | 
    // h+b       +
    side =
        cairo_image_surface_blur (tmp, ccm_shadow_get_option (self)->radius,
                                  ccm_shadow_get_option (self)->sigma,
                                  clipbox.width, 0, border,
                                  clipbox.height + border);
    cairo_save (cr);
    cairo_rectangle (cr, clipbox.width, border, border,
                     clipbox.height - border);
    cairo_move_to (cr, clipbox.width + border, border);
    cairo_line_to (cr, clipbox.width + border, 0);
    cairo_line_to (cr, clipbox.width, border);
    cairo_move_to (cr, clipbox.width + border, clipbox.height);
    cairo_line_to (cr, clipbox.width + border, clipbox.height + border);
    cairo_line_to (cr, clipbox.width, clipbox.height);
    cairo_clip (cr);
    cairo_translate (cr, clipbox.width, 0);
    cairo_set_source_surface (cr, side, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (side);
    cairo_restore (cr);

    /* bottom side */
    //                              w - b
    // h      +   +<------------------------------------>+
    //          +                                          +
    // h+b ---+---+--------------------------------------+---+---
    //        0   b                                      w  w+b
    side =
        cairo_image_surface_blur (tmp, ccm_shadow_get_option (self)->radius,
                                  ccm_shadow_get_option (self)->sigma, 0,
                                  clipbox.height, clipbox.width + border,
                                  border);
    cairo_save (cr);
    cairo_rectangle (cr, border, clipbox.height, clipbox.width - border,
                     border);
    cairo_move_to (cr, border, clipbox.height + border);
    cairo_line_to (cr, 0, clipbox.height + border);
    cairo_line_to (cr, border, clipbox.height);
    cairo_move_to (cr, clipbox.width, clipbox.height + border);
    cairo_line_to (cr, clipbox.width + border, clipbox.height + border);
    cairo_line_to (cr, clipbox.width, clipbox.height);
    cairo_clip (cr);
    cairo_translate (cr, 0, clipbox.height);
    cairo_set_source_surface (cr, side, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (side);
    cairo_restore (cr);

    // left side
    //       0   b
    // 0 ----+     
    //       | + 
    // b     +   + 
    //       |   |
    //       |   | h-b
    //       |   |
    // h     +   +
    //       | +
    // h+b   +
    side =
        cairo_image_surface_blur (tmp, ccm_shadow_get_option (self)->radius,
                                  ccm_shadow_get_option (self)->sigma, 0, 0,
                                  border, clipbox.height + border);
    cairo_save (cr);
    cairo_rectangle (cr, 0, border, border, clipbox.height - border);
    cairo_move_to (cr, 0, border);
    cairo_line_to (cr, 0, 0);
    cairo_line_to (cr, border, border);
    cairo_move_to (cr, 0, clipbox.height);
    cairo_line_to (cr, 0, clipbox.height + border);
    cairo_line_to (cr, border, clipbox.height);
    cairo_clip (cr);
    cairo_set_source_surface (cr, side, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (side);
    cairo_restore (cr);
    cairo_destroy (cr);

    cairo_surface_destroy (tmp);
}

static void
ccm_shadow_on_event (CCMShadow * self, XEvent * event)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (event != NULL);

    switch (event->type)
    {
        case PropertyNotify:
            {
                XPropertyEvent *property_event = (XPropertyEvent *) event;
                CCMWindow *window;

                if (property_event->atom ==
                    CCM_SHADOW_GET_CLASS (self)->shadow_disable_atom)
                {
                    window =
                        ccm_screen_find_window_or_child (self->priv->screen,
                                                         property_event->
                                                         window);
                    if (window)
                    {
                        CCMShadow *plugin =
                            CCM_SHADOW (_ccm_window_get_plugin (window,
                                                                CCM_TYPE_SHADOW));
                        ccm_shadow_query_avoid_shadow (plugin);
                    }
                }

                if (property_event->atom ==
                    CCM_SHADOW_GET_CLASS (self)->shadow_enable_atom)
                {
                    window =
                        ccm_screen_find_window_or_child (self->priv->screen,
                                                         property_event->
                                                         window);
                    if (window)
                    {
                        CCMShadow *plugin =
                            CCM_SHADOW (_ccm_window_get_plugin (window,
                                                                CCM_TYPE_SHADOW));
                        ccm_shadow_query_force_shadow (plugin);
                    }
                }
            }
            break;
        default:
            break;
    }
}

static void
ccm_shadow_on_property_changed (CCMShadow * self, CCMPropertyType changed,
                                CCMWindow * window)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (window != NULL);

    if (!self->priv->id_check)
        self->priv->id_check =
            g_idle_add ((GSourceFunc) ccm_shadow_check_needed, self);
}

static void
ccm_shadow_on_shadow_pixmap_destroyed (CCMShadow * self)
{
    g_return_if_fail (self != NULL);

	self->priv->shadow = NULL;
    if (self->priv->pixmap) g_object_unref (self->priv->pixmap);
	self->priv->pixmap = NULL;
}

static void
ccm_shadow_on_pixmap_destroyed (CCMShadow * self)
{
    g_return_if_fail (self != NULL);

	self->priv->pixmap = NULL;
    if (self->priv->shadow) g_object_unref (self->priv->shadow);
	self->priv->shadow = NULL;
}

static void
ccm_shadow_on_pixmap_damage (CCMShadow * self, CCMRegion * area)
{
    g_return_if_fail (self != NULL);

    if (self->priv->shadow && self->priv->pixmap)
    {
        CCMDisplay *display = 
			ccm_drawable_get_display (CCM_DRAWABLE (self->priv->pixmap));
        cairo_surface_t *surface;
        cairo_t *ctx;
        cairo_rectangle_t *rects;
        gint cpt, nb_rects;
        cairo_rectangle_t clipbox;

        surface = ccm_drawable_get_surface (CCM_DRAWABLE (self->priv->pixmap));
        if (!surface)
            return;

        ctx = ccm_drawable_create_context (CCM_DRAWABLE (self->priv->shadow));
        if (!ctx)
            return;

        ccm_region_get_clipbox (self->priv->geometry, &clipbox);

        if (!self->priv->shadow_image)
        {
            if (ccm_shadow_get_option (self)->real_blur)
                ccm_shadow_create_blur_shadow (self);
            else
                ccm_shadow_create_fake_shadow (self);
        }

        if (area)
        {
            cairo_translate (ctx, ccm_shadow_get_option (self)->radius,
                             ccm_shadow_get_option (self)->radius);
            ccm_region_get_rectangles (area, &rects, &nb_rects);
            for (cpt = 0; cpt < nb_rects; ++cpt)
                cairo_rectangle (ctx, rects[cpt].x, rects[cpt].y,
                                 rects[cpt].width, rects[cpt].height);
            cairo_clip (ctx);
            cairo_rectangles_free (rects, nb_rects);
        }
        else
        {
            cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
            cairo_paint (ctx);
            cairo_set_operator (ctx, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_rgba (ctx,
                                   (double) ccm_shadow_get_option (self)->
                                   color->red / 65535.0f,
                                   (double) ccm_shadow_get_option (self)->
                                   color->green / 65535.0f,
                                   (double) ccm_shadow_get_option (self)->
                                   color->blue / 65535.0f,
                                   ccm_shadow_get_option (self)->alpha);
            cairo_mask_surface (ctx, self->priv->shadow_image,
                                ccm_shadow_get_option (self)->offset,
                                ccm_shadow_get_option (self)->offset);

            cairo_translate (ctx, ccm_shadow_get_option (self)->radius,
                             ccm_shadow_get_option (self)->radius);
            cairo_translate (ctx, -clipbox.x, -clipbox.y);
            ccm_region_get_rectangles (self->priv->geometry, &rects, &nb_rects);
            for (cpt = 0; cpt < nb_rects; ++cpt)
                cairo_rectangle (ctx, rects[cpt].x, rects[cpt].y,
                                 rects[cpt].width, rects[cpt].height);
            cairo_clip (ctx);
            cairo_rectangles_free (rects, nb_rects);
            cairo_translate (ctx, clipbox.x, clipbox.y);
        }
        if (!ccm_pixmap_get_freeze(self->priv->shadow))
        {
            cairo_set_operator (ctx, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface (ctx, surface, 0, 0);
            cairo_paint (ctx);
        }
        cairo_destroy (ctx);
        cairo_surface_destroy (surface);
        ccm_display_flush (display);
        ccm_display_sync (display);
    }
}

static void
ccm_shadow_screen_load_options (CCMScreenPlugin * plugin, CCMScreen * screen)
{
    CCMShadow *self = CCM_SHADOW (plugin);
    CCMDisplay *display = ccm_screen_get_display (screen);

    self->priv->screen = screen;
    ccm_screen_plugin_load_options (CCM_SCREEN_PLUGIN_PARENT (plugin), screen);
    self->priv->id_event =
        g_signal_connect_swapped (display, "event",
                                  G_CALLBACK (ccm_shadow_on_event), self);
}

static void
ccm_shadow_on_option_changed (CCMPlugin * plugin, CCMConfig * config)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (config != NULL);

    CCMShadow *self = CCM_SHADOW (plugin);
    GError *error = NULL;

    if (config == ccm_shadow_get_config (self, CCM_SHADOW_REAL_BLUR))
    {
        ccm_shadow_get_option (self)->real_blur =
            ccm_config_get_boolean (ccm_shadow_get_config
                                    (self, CCM_SHADOW_REAL_BLUR), &error);
        if (error)
        {
            g_warning ("Error on get shadow realblur configuration value");
            g_error_free (error);
            error = NULL;
            ccm_shadow_get_option (self)->real_blur = FALSE;
        }
    }
    if (config == ccm_shadow_get_config (self, CCM_SHADOW_OFFSET))
    {
        ccm_shadow_get_option (self)->offset =
            ccm_config_get_integer (ccm_shadow_get_config
                                    (self, CCM_SHADOW_OFFSET), &error);
        if (error)
        {
            g_warning ("Error on get shadow offset configuration value");
            g_error_free (error);
            error = NULL;
            ccm_shadow_get_option (self)->offset = 0;
        }
    }
    if (config == ccm_shadow_get_config (self, CCM_SHADOW_RADIUS))
    {
        ccm_shadow_get_option (self)->radius =
            ccm_config_get_integer (ccm_shadow_get_config
                                    (self, CCM_SHADOW_RADIUS), &error);
        if (error)
        {
            g_warning ("Error on get shadow radius configuration value");
            g_error_free (error);
            error = NULL;
            ccm_shadow_get_option (self)->radius = 14;
        }
    }
    if (config == ccm_shadow_get_config (self, CCM_SHADOW_SIGMA))
    {
        ccm_shadow_get_option (self)->sigma =
            ccm_config_get_float (ccm_shadow_get_config
                                  (self, CCM_SHADOW_SIGMA), &error);
        if (error)
        {
            g_warning ("Error on get shadow radius configuration value");
            g_error_free (error);
            error = NULL;
            ccm_shadow_get_option (self)->sigma = 7;
        }
    }
    if (config == ccm_shadow_get_config (self, CCM_SHADOW_COLOR))
    {
        if (ccm_shadow_get_option (self)->color)
            g_free (ccm_shadow_get_option (self)->color);
        ccm_shadow_get_option (self)->color =
            ccm_config_get_color (ccm_shadow_get_config
                                  (self, CCM_SHADOW_COLOR), &error);
        if (error)
        {
            g_warning ("Error on get shadow color configuration value");
            g_error_free (error);
            error = NULL;
            ccm_shadow_get_option (self)->color = g_new0 (GdkColor, 1);
        }
    }
    if (config == ccm_shadow_get_config (self, CCM_SHADOW_ALPHA))
    {
        ccm_shadow_get_option (self)->alpha =
            ccm_config_get_float (ccm_shadow_get_config
                                  (self, CCM_SHADOW_ALPHA), &error);
        if (error)
        {
            g_warning ("Error on get shadow alpha configuration value");
            g_error_free (error);
            error = NULL;
            ccm_shadow_get_option (self)->alpha = 0.6;
        }
    }

    if (!self->priv->id_check)
    {
        if (self->priv->shadow_image)
            cairo_surface_destroy (self->priv->shadow_image);
        self->priv->shadow_image = NULL;

        if (self->priv->pixmap)
            g_object_unref (self->priv->pixmap);

        if (self->priv->geometry)
            ccm_region_destroy (self->priv->geometry);
        self->priv->geometry = NULL;

        self->priv->id_check =
            g_idle_add ((GSourceFunc) ccm_shadow_check_needed, self);
    }
}

static void
ccm_shadow_window_load_options (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMShadow *self = CCM_SHADOW (plugin);

    self->priv->window = window;

    ccm_plugin_options_load (CCM_PLUGIN (self), "shadow", CCMShadowOptionKeys,
                             CCM_SHADOW_OPTION_N, ccm_shadow_on_option_changed);
    ccm_window_plugin_load_options (CCM_WINDOW_PLUGIN_PARENT (plugin), window);

    ccm_shadow_create_atoms (self);

    self->priv->id_property_changed =
        g_signal_connect_swapped (window, "property-changed",
                                  G_CALLBACK (ccm_shadow_on_property_changed),
                                  self);
}

static CCMRegion *
ccm_shadow_window_query_geometry (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMRegion *geometry = NULL;
    cairo_rectangle_t area;
    CCMShadow *self = CCM_SHADOW (plugin);

    if (self->priv->shadow_image)
        cairo_surface_destroy (self->priv->shadow_image);
    self->priv->shadow_image = NULL;

    if (self->priv->pixmap)
        g_object_unref (self->priv->pixmap);
    self->priv->pixmap = NULL;

    if (self->priv->geometry)
        ccm_region_destroy (self->priv->geometry);
    self->priv->geometry = NULL;

    geometry =
        ccm_window_plugin_query_geometry (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                          window);
    if (geometry && ccm_shadow_need_shadow (self))
    {
        self->priv->geometry = ccm_region_copy (geometry);
        ccm_region_get_clipbox (geometry, &area);
        ccm_region_offset (geometry, -ccm_shadow_get_option (self)->radius,
                           -ccm_shadow_get_option (self)->radius);
        ccm_region_resize (geometry,
                           area.width +
                           ccm_shadow_get_option (self)->radius * 2,
                           area.height +
                           ccm_shadow_get_option (self)->radius * 2);
    }

    ccm_shadow_query_avoid_shadow (self);
    ccm_shadow_query_force_shadow (self);

    return geometry;
}

static void
ccm_shadow_window_map (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMShadow *self = CCM_SHADOW (plugin);

    ccm_shadow_query_avoid_shadow (self);

    ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT (plugin), window);
}

static void
ccm_shadow_window_move (CCMWindowPlugin * plugin, CCMWindow * window, int x,
                        int y)
{
    CCMShadow *self = CCM_SHADOW (plugin);

    if (self->priv->geometry)
    {
        cairo_rectangle_t area;

        ccm_region_get_clipbox (self->priv->geometry, &area);
        if (x != area.x || y != area.y)
        {
            ccm_region_offset (self->priv->geometry, x - area.x, y - area.y);
            x -= ccm_shadow_get_option (self)->radius;
            y -= ccm_shadow_get_option (self)->radius;
        }
        else
            return;
    }
    ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT (plugin), window, x, y);
}

static void
ccm_shadow_window_resize (CCMWindowPlugin * plugin, CCMWindow * window,
                          int width, int height)
{
    CCMShadow *self = CCM_SHADOW (plugin);
    int border = 0;

    if (self->priv->geometry)
    {
        cairo_rectangle_t area;

        ccm_region_get_clipbox (self->priv->geometry, &area);
        if (width != area.width || height != area.height)
        {
            ccm_region_resize (self->priv->geometry, width, height);
            border = ccm_shadow_get_option (self)->radius * 2;

            if (self->priv->shadow_image)
                cairo_surface_destroy (self->priv->shadow_image);
            self->priv->shadow_image = NULL;

            if (self->priv->pixmap) g_object_unref (self->priv->pixmap);
            self->priv->pixmap = NULL;
        }
        else
            return;
    }

    ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT (plugin), window,
                              width + border, height + border);
}


static void
ccm_shadow_window_set_opaque_region (CCMWindowPlugin * plugin,
                                     CCMWindow * window, const CCMRegion * area)
{
    CCMShadow *self = CCM_SHADOW (plugin);

    if (self->priv->geometry && area)
    {
        CCMRegion *opaque = ccm_region_copy (self->priv->geometry);

        ccm_region_intersect (opaque, (CCMRegion *) area);

        ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                             window, opaque);
        ccm_region_destroy (opaque);
    }
    else
        ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                             window, area);
}

static void
ccm_shadow_window_get_origin (CCMWindowPlugin * plugin, CCMWindow * window,
                              int *x, int *y)
{
    CCMShadow *self = CCM_SHADOW (plugin);
    cairo_rectangle_t geometry;

    if (self->priv->geometry)
    {
        ccm_region_get_clipbox (self->priv->geometry, &geometry);
        *x = geometry.x;
        *y = geometry.y;
    }
    else
    {
        ccm_window_plugin_get_origin (CCM_WINDOW_PLUGIN_PARENT (plugin), window,
                                      x, y);
    }
}

static CCMPixmap *
ccm_shadow_window_get_pixmap (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMShadow *self = CCM_SHADOW (plugin);
    CCMPixmap *pixmap = NULL;

    pixmap =
        ccm_window_plugin_get_pixmap (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                      window);

	if (pixmap && ccm_shadow_need_shadow (self) && self->priv->geometry)
    {
        gint swidth, sheight;
        cairo_rectangle_t clipbox;

        ccm_region_get_clipbox (self->priv->geometry, &clipbox);
        swidth = clipbox.width + ccm_shadow_get_option (self)->radius * 2;
        sheight = clipbox.height + ccm_shadow_get_option (self)->radius * 2;

		if (self->priv->pixmap) g_object_unref(self->priv->pixmap);
		self->priv->pixmap = pixmap;

        self->priv->shadow =
            ccm_window_create_pixmap (window, swidth, sheight, 32);

		g_object_set_qdata_full (G_OBJECT (self->priv->shadow), 
		                         CCMShadowPixmapQuark, self,
                                 (GDestroyNotify)
                                 ccm_shadow_on_shadow_pixmap_destroyed);

		g_object_set_qdata_full (G_OBJECT (pixmap), CCMShadowQuark, self,
                                 (GDestroyNotify)
                                 ccm_shadow_on_pixmap_destroyed);

        g_signal_connect_swapped (pixmap, "damaged",
                                  G_CALLBACK (ccm_shadow_on_pixmap_damage),
                                  self);

        ccm_shadow_on_pixmap_damage (self, NULL);

        pixmap = self->priv->shadow;
    }

    return pixmap;
}

static void
ccm_shadow_preferences_page_init_windows_section (CCMPreferencesPagePlugin *
                                                  plugin,
                                                  CCMPreferencesPage *
                                                  preferences,
                                                  GtkWidget * windows_section)
{
    CCMShadow *self = CCM_SHADOW (plugin);

    self->priv->builder = gtk_builder_new ();

    if (gtk_builder_add_from_file
        (self->priv->builder, UI_DIR "/ccm-shadow.ui", NULL))
    {
        GtkWidget *widget =
            GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "shadow"));
        if (widget)
        {
            gint screen_num = ccm_preferences_page_get_screen_num (preferences);

            gtk_box_pack_start (GTK_BOX (windows_section), widget, FALSE, TRUE,
                                0);

            CCMConfigColorButton *color =
                CCM_CONFIG_COLOR_BUTTON (gtk_builder_get_object
                                         (self->priv->builder,
                                          "color"));
            g_object_set (color, "screen", screen_num, NULL);

            CCMConfigAdjustment *radius =
                CCM_CONFIG_ADJUSTMENT (gtk_builder_get_object
                                       (self->priv->builder,
                                        "radius-adjustment"));
            g_object_set (radius, "screen", screen_num, NULL);

            CCMConfigAdjustment *sigma =
                CCM_CONFIG_ADJUSTMENT (gtk_builder_get_object
                                       (self->priv->builder,
                                        "sigma-adjustment"));
            g_object_set (sigma, "screen", screen_num, NULL);

            ccm_preferences_page_section_register_widget (preferences,
                                                          CCM_PREFERENCES_PAGE_SECTION_WINDOW,
                                                          widget, "shadow");
        }
    }
    ccm_preferences_page_plugin_init_windows_section
        (CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin), preferences,
         windows_section);
}

static void
ccm_shadow_window_iface_init (CCMWindowPluginClass * iface)
{
    iface->load_options = ccm_shadow_window_load_options;
    iface->query_geometry = ccm_shadow_window_query_geometry;
    iface->paint = NULL;
    iface->map = ccm_shadow_window_map;
    iface->unmap = NULL;
    iface->query_opacity = NULL;
    iface->move = ccm_shadow_window_move;
    iface->resize = ccm_shadow_window_resize;
    iface->set_opaque_region = ccm_shadow_window_set_opaque_region;
    iface->get_origin = ccm_shadow_window_get_origin;
    iface->get_pixmap = ccm_shadow_window_get_pixmap;
}

static void
ccm_shadow_screen_iface_init (CCMScreenPluginClass * iface)
{
    iface->load_options = ccm_shadow_screen_load_options;
    iface->paint = NULL;
    iface->add_window = NULL;
    iface->remove_window = NULL;
    iface->damage = NULL;
}

static void
ccm_shadow_preferences_page_iface_init (CCMPreferencesPagePluginClass * iface)
{
    iface->init_general_section = NULL;
    iface->init_desktop_section = NULL;
    iface->init_windows_section =
        ccm_shadow_preferences_page_init_windows_section;
    iface->init_effects_section = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section = NULL;
}
