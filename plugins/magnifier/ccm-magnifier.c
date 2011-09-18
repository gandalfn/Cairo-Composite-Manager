/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-magnifier.c
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <X11/extensions/Xfixes.h>

#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-magnifier.h"
#include "ccm-keybind.h"
#include "ccm-cairo-utils.h"
#include "ccm-timeline.h"
#include "ccm.h"

#define CCM_MAGNIFIER_WINDOW_INFO_WIDTH 30
#define CCM_MAGNIFIER_WINDOW_INFO_HEIGHT 7

enum
{
    CCM_MAGNIFIER_ENABLE,
    CCM_MAGNIFIER_ZOOM_LEVEL,
    CCM_MAGNIFIER_ZOOM_QUALITY,
    CCM_MAGNIFIER_X,
    CCM_MAGNIFIER_Y,
    CCM_MAGNIFIER_HEIGHT,
    CCM_MAGNIFIER_WIDTH,
    CCM_MAGNIFIER_RESTRICT_AREA_X,
    CCM_MAGNIFIER_RESTRICT_AREA_Y,
    CCM_MAGNIFIER_RESTRICT_AREA_WIDTH,
    CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT,
    CCM_MAGNIFIER_BORDER,
    CCM_MAGNIFIER_SHORTCUT,
    CCM_MAGNIFIER_SHADE_DESKTOP,
    CCM_MAGNIFIER_OPTION_N
};

static const gchar *CCMMagnifierOptionKeys[CCM_MAGNIFIER_OPTION_N] = {
    "enable",
    "zoom-level",
    "zoom-quality",
    "x",
    "y",
    "height",
    "width",
    "restrict_area_x",
    "restrict_area_y",
    "restrict_area_width",
    "restrict_area_height",
    "border",
    "shortcut",
    "shade_desktop"
};

typedef struct
{
    CCMPluginOptions parent;

    gboolean enabled;
    gchar* shortcut;

    gfloat scale;
    gfloat new_scale;

    gboolean shade;

    cairo_rectangle_t restrict_area;
    cairo_rectangle_t area;
    cairo_filter_t quality;

    int border;
} CCMMagnifierOptions;

static void ccm_magnifier_screen_iface_init (CCMScreenPluginClass * iface);
static void ccm_magnifier_window_iface_init (CCMWindowPluginClass * iface);
static void ccm_magnifier_on_option_changed (CCMPlugin * plugin, int index);
static void ccm_magnifier_on_new_frame (CCMMagnifier * self, int num_frame,
                                        CCMTimeline * timeline);

CCM_DEFINE_PLUGIN_WITH_OPTIONS (CCMMagnifier, ccm_magnifier, CCM_TYPE_PLUGIN,
                                CCM_IMPLEMENT_INTERFACE (ccm_magnifier,
                                                         CCM_TYPE_SCREEN_PLUGIN,
                                                         ccm_magnifier_screen_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_magnifier,
                                                         CCM_TYPE_WINDOW_PLUGIN,
                                                         ccm_magnifier_window_iface_init))
struct _CCMMagnifierPrivate
{
    CCMScreen * screen;

    int x_offset;
    int y_offset;
    cairo_surface_t * surface;
    CCMRegion * damaged;
    CCMKeybind * keybind;

    cairo_surface_t * surface_window_info;
    CCMTimeline * timeline;
};

#define CCM_MAGNIFIER_GET_PRIVATE(o)  \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MAGNIFIER, CCMMagnifierPrivate))

static void
ccm_magnifier_options_init (CCMMagnifierOptions* self)
{
    self->enabled = FALSE;
    self->shade = TRUE;
    self->scale = 1.0f;
    self->new_scale = 1.0f;
    self->quality = CAIRO_FILTER_FAST;
    self->shortcut = NULL;
    self->border = 0;
    bzero (&self->area, sizeof (cairo_rectangle_t));
    bzero (&self->restrict_area, sizeof (cairo_rectangle_t));
}

static void
ccm_magnifier_options_finalize (CCMMagnifierOptions* self)
{
    if (self->shortcut) g_free(self->shortcut);
}

static void
ccm_magnifier_options_get_restrict_area (CCMMagnifierOptions* self)
{
    GError *error = NULL;
    gint val, real;
    gdouble x, y, width, height;
    CCMDisplay* display = ccm_display_get_default();
    Screen* screen = ScreenOfDisplay(CCM_DISPLAY_XDISPLAY(display),
                                     ccm_plugin_options_get_screen_num(CCM_PLUGIN_OPTIONS(self)));

    real = ccm_config_get_integer (ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self), 
                                                                 CCM_MAGNIFIER_RESTRICT_AREA_WIDTH),
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = -1;
    }
    if (real < 0)
    {
        self->restrict_area.width = screen->width;
    }
    else
    {
        val = MAX (0, real);
        val = MIN (screen->width, val);
        if (real != val)
            ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                   CCM_MAGNIFIER_RESTRICT_AREA_WIDTH),
                                    val, NULL);

        width = val;
        if (self->restrict_area.width != width)
            self->restrict_area.width = width;
    }

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT),
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = -1;
    }
    if (real < 0)
    {
        self->restrict_area.height = screen->height;
    }
    else
    {
        val = MAX (0, real);
        val = MIN (screen->height, val);
        if (real != val)
            ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                   CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT),
                                    val, NULL);

        height = val;
        if (self->restrict_area.height != height)
            self->restrict_area.height = height;
    }

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_RESTRICT_AREA_X), 
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = -1;
    }
    if (real < 0)
    {
        self->restrict_area.x = 0;
    }
    else
    {
        val = MAX (0, real);
        val = MIN (self->restrict_area.width, val);
        if (real != val)
            ccm_config_set_integer (ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_RESTRICT_AREA_X), 
                                    val, NULL);

        x = val;
        if (self->restrict_area.x != x)
            self->restrict_area.x = x;
    }

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_RESTRICT_AREA_Y), 
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = -1;
    }
    if (real < 0)
    {
        self->restrict_area.y = 0;
    }
    else
    {
        val = MAX (0, real);
        val = MIN (self->restrict_area.height, val);
        if (real != val)
            ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                   CCM_MAGNIFIER_RESTRICT_AREA_Y), 
                                    val, NULL);

        y = val;
        if (self->restrict_area.y != y)
            self->restrict_area.y = y;
    }
}

static void
ccm_magnifier_options_get_size (CCMMagnifierOptions* self)
{
    GError *error = NULL;
    gint val, real;
    gdouble x, y, width, height;
    CCMDisplay* display = ccm_display_get_default();
    Screen* screen = ScreenOfDisplay(CCM_DISPLAY_XDISPLAY(display),
                                     ccm_plugin_options_get_screen_num(CCM_PLUGIN_OPTIONS(self)));

    self->area.x = 0;
    self->area.y = 0;

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_WIDTH), 
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = 80;
    }
    val = MAX (10, real);
    val = MIN (80, val);
    if (real != val)
        ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                               CCM_MAGNIFIER_WIDTH), 
                                val, NULL);

    width = (gdouble) self->restrict_area.width * (gdouble) val / 100.0;
    if (self->area.width != width)
        self->area.width = width;

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_HEIGHT), 
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = 80;
    }
    val = MAX (10, real);
    val = MIN (80, val);
    if (real != val) ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                            CCM_MAGNIFIER_HEIGHT), 
                                             val, NULL);

    height = (gdouble) self->restrict_area.height * (gdouble) val / 100.0;

    if (self->area.height != height)
        self->area.height = height;

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_X), 
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = -1;
    }
    if (real < 0)
    {
        self->area.x = (self->restrict_area.width - self->area.width) / 2;
    }
    else
    {
        val = MAX (0, real);
        val = MIN (screen->width, val);
        if (real != val)
            ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                   CCM_MAGNIFIER_X), 
                                    val, NULL);

        x = val;
        if (self->area.x != x)
            self->area.x = x;
    }

    real = ccm_config_get_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                  CCM_MAGNIFIER_Y), 
                                   &error);
    if (error)
    {
        g_error_free (error);
        error = NULL;
        real = -1;
    }
    if (real < 0)
    {
        self->area.y = (self->restrict_area.height - self->area.height) / 2;
    }
    else
    {
        val = MAX (0, real);
        val = MIN (screen->height, val);
        if (real != val)
            ccm_config_set_integer (ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                                   CCM_MAGNIFIER_Y), 
                                    val, NULL);

        y = val;
        if (self->area.y != y)
            self->area.y = y;
    }
}

static void
ccm_magnifier_options_changed (CCMMagnifierOptions* self, CCMConfig* config)
{
    GError *error = NULL;

    if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                CCM_MAGNIFIER_ENABLE))
    {
        self->enabled = ccm_config_get_boolean(config, NULL);
    }
    else if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                     CCM_MAGNIFIER_ZOOM_LEVEL))
    {
        gfloat scale, real = ccm_config_get_float (config, &error);

        if (error)
        {
            g_error_free (error);
            g_warning ("Error on get magnifier zoom level configuration value");
            real = 1.5f;
        }
        scale = MAX (1.0f, real);
        scale = MIN (5.0f, scale);
        if (real != scale) ccm_config_set_float (config, scale, NULL);

        if (self->new_scale != scale) self->new_scale = scale;
    }
    else if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                     CCM_MAGNIFIER_ZOOM_QUALITY))
    {
        gchar *quality = ccm_config_get_string (config, NULL);

        if (!quality)
        {
            if (self->quality != CAIRO_FILTER_FAST)
            {
                self->quality = CAIRO_FILTER_FAST;
                ccm_config_set_string (config, "fast", NULL);
            }
        }
        else
        {
            if (!g_ascii_strcasecmp (quality, "fast") && 
                self->quality != CAIRO_FILTER_FAST)
            {
                self->quality = CAIRO_FILTER_FAST;
            }
            else if (!g_ascii_strcasecmp (quality, "good") && 
                     self->quality != CAIRO_FILTER_GOOD)
            {
                self->quality = CAIRO_FILTER_GOOD;
            }
            else if (!g_ascii_strcasecmp (quality, "best") && 
                     self->quality != CAIRO_FILTER_BEST)
            {
                self->quality = CAIRO_FILTER_BEST;
            }
            g_free (quality);
        }
    }
    else if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                     CCM_MAGNIFIER_SHORTCUT))
    {
        if (self->shortcut) g_free(self->shortcut);
        self->shortcut = ccm_config_get_string (config, &error);
        if (error)
        {
            g_error_free (error);
            g_warning ("Error on get magnifier shortcut configuration value");
            self->shortcut = g_strdup ("<Super>F12");
        }
    }
    else if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                     CCM_MAGNIFIER_SHADE_DESKTOP))
    {
        self->shade = ccm_config_get_boolean (config, NULL);
    }
    else if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
                                                     CCM_MAGNIFIER_BORDER))
    {
        gint val, real = ccm_config_get_integer (config, NULL);

        val = MAX (0, real);
        val = MIN (self->area.width, val);
        val = MIN (self->area.height, val);
        if (val != real) ccm_config_set_integer (config, val, NULL);

        if (self->border != val) self->border = val;
    }
    else if (config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_WIDTH) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_HEIGHT) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_X) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_Y) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_RESTRICT_AREA_WIDTH) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_RESTRICT_AREA_X) ||
             config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self),
                                                      CCM_MAGNIFIER_RESTRICT_AREA_Y))
    {
        ccm_magnifier_options_get_restrict_area (self);
        ccm_magnifier_options_get_size (self);
    }
}

static void
ccm_magnifier_init (CCMMagnifier * self)
{
    self->priv = CCM_MAGNIFIER_GET_PRIVATE (self);
    self->priv->screen = NULL;
    self->priv->x_offset = 0;
    self->priv->y_offset = 0;
    self->priv->surface = NULL;
    self->priv->damaged = NULL;
    self->priv->keybind = NULL;

    self->priv->surface_window_info = NULL;
    self->priv->timeline = NULL;
}

static void
ccm_magnifier_finalize (GObject * object)
{
    CCMMagnifier *self = CCM_MAGNIFIER (object);

    if (self->priv->screen)
        ccm_plugin_options_unload (CCM_PLUGIN (self));

    if (self->priv->surface)
        cairo_surface_destroy (self->priv->surface);
    self->priv->surface = NULL;
    if (self->priv->damaged)
        ccm_region_destroy (self->priv->damaged);
    self->priv->damaged = NULL;
    if (self->priv->keybind)
        g_object_unref (self->priv->keybind);
    self->priv->keybind = NULL;
    if (self->priv->surface_window_info)
        cairo_surface_destroy (self->priv->surface_window_info);
    self->priv->surface_window_info = NULL;
    if (self->priv->timeline)
        g_object_unref (self->priv->timeline);
    self->priv->timeline = NULL;

    G_OBJECT_CLASS (ccm_magnifier_parent_class)->finalize (object);
}

static void
ccm_magnifier_class_init (CCMMagnifierClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMMagnifierPrivate));

    object_class->finalize = ccm_magnifier_finalize;
}

static void
ccm_magnifier_set_enable (CCMMagnifier * self, gboolean enabled)
{
    if (ccm_magnifier_get_option (self)->enabled != enabled)
    {
        ccm_magnifier_get_option (self)->enabled = enabled;
        if (ccm_magnifier_get_option (self)->enabled)
        {
            ccm_screen_manage_cursors (self->priv->screen);
            if (!self->priv->timeline)
            {
                self->priv->timeline = ccm_timeline_new_for_duration (3500);
                g_object_set (G_OBJECT (self->priv->timeline), "fps", 30, NULL);
                g_signal_connect_swapped (self->priv->timeline, "new-frame",
                                          G_CALLBACK
                                          (ccm_magnifier_on_new_frame), self);
            }
            ccm_timeline_rewind (self->priv->timeline);
            ccm_timeline_start (self->priv->timeline);
        }
        else
        {
            ccm_screen_unmanage_cursors (self->priv->screen);
            if (self->priv->surface)
                cairo_surface_destroy (self->priv->surface);
            self->priv->surface = NULL;
            if (self->priv->timeline)
                ccm_timeline_stop (self->priv->timeline);
        }
    }
}

static void
ccm_magnifier_on_key_press (CCMMagnifier * self)
{
    g_return_if_fail (self != NULL);

    int x, y;

    if (ccm_screen_query_pointer (self->priv->screen, NULL, &x, &y))
    {
        gboolean enabled =
            ccm_config_get_boolean (ccm_magnifier_get_config
                                    (self, CCM_MAGNIFIER_ENABLE),
                                    NULL);
        ccm_config_set_boolean (ccm_magnifier_get_config
                                (self, CCM_MAGNIFIER_ENABLE), !enabled, NULL);
        ccm_magnifier_set_enable (self, !enabled);
        ccm_screen_damage (self->priv->screen);
    }
}

static void
ccm_magnifier_on_new_frame (CCMMagnifier * self, int num_frame,
                            CCMTimeline * timeline)
{
    gdouble progress = ccm_timeline_get_progress (timeline);

    if (progress <= 0.25 || progress >= 0.75)
    {
        cairo_rectangle_t geometry;
        CCMRegion *area;

        geometry.width =
            ccm_magnifier_get_option (self)->restrict_area.width *
            (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f);
        geometry.height =
            ccm_magnifier_get_option (self)->restrict_area.height *
            (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f);
        geometry.x =
            (ccm_magnifier_get_option (self)->restrict_area.width -
             geometry.width) / 2;
        geometry.y = 0;

        area = ccm_region_rectangle (&geometry);
        ccm_screen_damage_region (self->priv->screen, area);
        ccm_region_destroy (area);
    }
}

static void
ccm_magnifier_get_keybind (CCMMagnifier * self)
{
    if (self->priv->keybind)
        g_object_unref (self->priv->keybind);

    self->priv->keybind = ccm_keybind_new (self->priv->screen, 
                                           ccm_magnifier_get_option(self)->shortcut, 
                                           TRUE);

    g_signal_connect_swapped (self->priv->keybind, "key_press",
                              G_CALLBACK (ccm_magnifier_on_key_press), self);
}

static void
ccm_magnifier_get_scale (CCMMagnifier * self)
{
    if (ccm_magnifier_get_option(self)->new_scale != ccm_magnifier_get_option(self)->scale)
    {
        if (self->priv->timeline
            && ccm_timeline_get_is_playing (self->priv->timeline))
        {
            gdouble progress = ccm_timeline_get_progress (self->priv->timeline);
            if (progress > 0.75)
                ccm_timeline_advance (self->priv->timeline,
                                      (progress - 0.75) *
                                      ccm_timeline_get_n_frames (self->priv->timeline));
        }
        else if (ccm_magnifier_get_option (self)->enabled)
        {
            if (!self->priv->timeline)
            {
                self->priv->timeline = ccm_timeline_new_for_duration (3500);
                g_object_set (G_OBJECT (self->priv->timeline), "fps", 30, NULL);
                g_signal_connect_swapped (self->priv->timeline, "new-frame",
                                          G_CALLBACK
                                          (ccm_magnifier_on_new_frame), self);
            }
            ccm_timeline_start (self->priv->timeline);
            ccm_timeline_rewind (self->priv->timeline);
        }
        cairo_surface_destroy (self->priv->surface_window_info);
        self->priv->surface_window_info = NULL;
    }
}

static void
ccm_magnifier_create_window_info (CCMMagnifier * self)
{
    PangoLayout *layout;
    PangoFontDescription *desc;
    cairo_rectangle_t geometry;
    cairo_t *context;
    int width, height;
    char *text;
    CCMWindow *cow = ccm_screen_get_overlay_window (self->priv->screen);
    cairo_surface_t *surface = ccm_drawable_get_surface (CCM_DRAWABLE (cow));

    if (self->priv->surface_window_info)
        cairo_surface_destroy (self->priv->surface_window_info);

    self->priv->surface_window_info =
        cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                      ccm_magnifier_get_option (self)->
                                      restrict_area.width *
                                      (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f),
                                      ccm_magnifier_get_option (self)->
                                      restrict_area.height *
                                      (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT /
                                       100.f));
    cairo_surface_destroy (surface);
    context = cairo_create (self->priv->surface_window_info);
    cairo_set_operator (context, CAIRO_OPERATOR_CLEAR);
    cairo_paint (context);
    cairo_set_operator (context, CAIRO_OPERATOR_SOURCE);

    geometry.width =
        ccm_magnifier_get_option (self)->restrict_area.width *
        (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f), geometry.height =
        ccm_magnifier_get_option (self)->restrict_area.height *
        (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f), geometry.x = 0, geometry.y =
        0;

    cairo_rectangle_round (context, geometry.x, geometry.y, geometry.width,
                           geometry.height, 20,
                           CAIRO_CORNER_BOTTOMLEFT | CAIRO_CORNER_BOTTOMRIGHT);
    cairo_set_source_rgba (context, 1.0f, 1.0f, 1.0f, 0.8f);
    cairo_fill_preserve (context);
    cairo_set_line_width (context, 2.0f);
    cairo_set_source_rgba (context, 0.f, 0.f, 0.f, 1.0f);
    cairo_stroke (context);

    layout = pango_cairo_create_layout (context);
    text =
        g_strdup_printf ("Zoom level = %i %%",
                         (int) (ccm_magnifier_get_option (self)->new_scale *
                                100));
    pango_layout_set_text (layout, text, -1);
    g_free (text);
    desc = pango_font_description_from_string ("Sans Bold 18");
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);
    pango_layout_get_pixel_size (layout, &width, &height);

    cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 1.0f);
    pango_cairo_update_layout (context, layout);
    cairo_move_to (context, (geometry.width - width) / 2.0f,
                   (geometry.height - height) / 2.0f);
    pango_cairo_show_layout (context, layout);
    g_object_unref (layout);
    cairo_destroy (context);
}

static void
ccm_magnifier_paint_window_info (CCMMagnifier * self, cairo_t * context)
{
    if (self->priv->timeline && ccm_timeline_get_is_playing (self->priv->timeline))
    {
        if (!self->priv->surface_window_info)
        {
            ccm_magnifier_create_window_info (self);
        }

        if (self->priv->surface_window_info)
        {
            cairo_rectangle_t geometry;
            gdouble progress = ccm_timeline_get_progress (self->priv->timeline);

            geometry.width =
                ccm_magnifier_get_option (self)->restrict_area.width *
                (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f);
            geometry.height =
                ccm_magnifier_get_option (self)->restrict_area.height *
                (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f);
            geometry.x =
                (ccm_magnifier_get_option (self)->restrict_area.width -
                 geometry.width) / 2;
            if (progress <= 0.25)
                geometry.y = -geometry.height * (1 - (progress / 0.25));
            else if (progress > 0.25 && progress < 0.75)
                geometry.y = 0;
            else
                geometry.y = -geometry.height * ((progress - 0.75) / 0.25);

            cairo_save (context);
            cairo_translate (context, geometry.x, geometry.y);
            cairo_set_source_surface (context, self->priv->surface_window_info,
                                      0, 0);
            cairo_paint (context);
            cairo_restore (context);
        }
    }
}

static void
ccm_magnifier_on_option_changed (CCMPlugin * plugin, int index)
{
    CCMMagnifier *self = CCM_MAGNIFIER (plugin);

    switch (index)
    {
        case CCM_MAGNIFIER_ENABLE:
            ccm_screen_damage (self->priv->screen);
            ccm_magnifier_set_enable (self, 
                                      ccm_magnifier_get_option(self)->enabled);
            break;
        case CCM_MAGNIFIER_ZOOM_LEVEL:
            ccm_magnifier_get_scale (self);
            break;
        case CCM_MAGNIFIER_ZOOM_QUALITY:
            ccm_screen_damage (self->priv->screen);
            break;
        case CCM_MAGNIFIER_SHORTCUT:
            ccm_magnifier_get_keybind (self);
            break;
        case CCM_MAGNIFIER_BORDER:
            ccm_screen_damage (self->priv->screen);
            break;
        case CCM_MAGNIFIER_SHADE_DESKTOP:
            ccm_screen_damage (self->priv->screen);
            break;
        case CCM_MAGNIFIER_X:
        case CCM_MAGNIFIER_Y:
        case CCM_MAGNIFIER_WIDTH:
        case CCM_MAGNIFIER_HEIGHT:
        case CCM_MAGNIFIER_RESTRICT_AREA_X:
        case CCM_MAGNIFIER_RESTRICT_AREA_Y:
        case CCM_MAGNIFIER_RESTRICT_AREA_WIDTH:
        case CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT:
            if (self->priv->surface)
                cairo_surface_destroy (self->priv->surface);
            self->priv->surface = NULL;
            ccm_screen_damage (self->priv->screen);
            break;
        default:
            break;
    }
}

static void
ccm_magnifier_screen_load_options (CCMScreenPlugin * plugin, CCMScreen * screen)
{
    CCMMagnifier *self = CCM_MAGNIFIER (plugin);

    self->priv->screen = screen;

    ccm_plugin_options_load (CCM_PLUGIN (self), "magnifier",
                             CCMMagnifierOptionKeys, CCM_MAGNIFIER_OPTION_N,
                             ccm_magnifier_on_option_changed);

    ccm_screen_plugin_load_options (CCM_SCREEN_PLUGIN_PARENT (plugin), screen);
}

static gboolean
ccm_magnifier_screen_paint (CCMScreenPlugin * plugin, CCMScreen * screen,
                            cairo_t * context)
{
    CCMMagnifier *self = CCM_MAGNIFIER (plugin);

    gboolean ret = FALSE;

    if (ccm_magnifier_get_option (self)->enabled && !self->priv->surface)
    {
        CCMWindow *cow = ccm_screen_get_overlay_window (screen);
        cairo_surface_t *surface =
            ccm_drawable_get_surface (CCM_DRAWABLE (cow));
        cairo_t *ctx;

        self->priv->surface =
            cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR,
                                          ccm_magnifier_get_option (self)->area.width /
                                          ccm_magnifier_get_option (self)->scale,
                                          ccm_magnifier_get_option (self)->area.height /
                                          ccm_magnifier_get_option (self)->scale);
        cairo_surface_destroy (surface);
        ctx = cairo_create (self->priv->surface);
        cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
        cairo_paint (ctx);
        cairo_destroy (ctx);
    }

    if (ccm_magnifier_get_option (self)->enabled)
    {
        CCMRegion *area = ccm_region_rectangle (&ccm_magnifier_get_option (self)->area);
        cairo_rectangle_t *rects = NULL;
        CCMRegion *geometry;
        gint cpt, nb_rects;

        ccm_debug ("MAGNIFIER PAINT SCREEN CLIP");
        cairo_save (context);

        geometry =
            ccm_region_rectangle (&ccm_magnifier_get_option (self)->
                                  restrict_area);
        ccm_region_subtract (geometry, area);

        ccm_region_get_rectangles (geometry, &rects, &nb_rects);
        cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
        for (cpt = 0; cpt < nb_rects; ++cpt)
            cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
                             rects[cpt].width, rects[cpt].height);
        cairo_clip (context);
        if (rects) cairo_rectangles_free (rects, nb_rects);
        ccm_region_destroy (area);
        ccm_region_destroy (geometry);

        g_object_set (G_OBJECT (self->priv->screen), "buffered_pixmap", FALSE,
                      NULL);
        ccm_debug ("MAGNIFIER PAINT SCREEN");
    }

    ret =
        ccm_screen_plugin_paint (CCM_SCREEN_PLUGIN_PARENT (plugin), screen,
                                 context);

    if (ccm_magnifier_get_option (self)->enabled)
        cairo_restore (context);

    if (ret && ccm_magnifier_get_option (self)->enabled)
    {
        CCMRegion *area = ccm_region_rectangle (&ccm_magnifier_get_option (self)->area);

        ccm_debug ("MAGNIFIER PAINT SCREEN CONTENT");

        ccm_screen_remove_damaged_region (self->priv->screen, area);

        if (ccm_magnifier_get_option (self)->shade)
        {
            CCMRegion *damaged = ccm_screen_get_damaged (self->priv->screen);

            if (damaged)
            {
                gint cpt, nb_rects;
                cairo_rectangle_t *rects = NULL;

                ccm_debug ("MAGNIFIER PAINT SCREEN SHADE");

                cairo_save (context);
                cairo_rectangle (context,
                                 ccm_magnifier_get_option (self)->restrict_area.
                                 x,
                                 ccm_magnifier_get_option (self)->restrict_area.
                                 y,
                                 ccm_magnifier_get_option (self)->restrict_area.
                                 width,
                                 ccm_magnifier_get_option (self)->restrict_area.
                                 height);
                cairo_clip (context);
                ccm_region_get_rectangles (damaged, &rects, &nb_rects);
                cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
                for (cpt = 0; cpt < nb_rects; ++cpt)
                {
                    cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
                                     rects[cpt].width, rects[cpt].height);
                    cairo_fill (context);
                }
                if (rects) cairo_rectangles_free (rects, nb_rects);
                cairo_restore (context);

                cairo_save (context);
                cairo_rectangle (context, ccm_magnifier_get_option (self)->area.x - 8,
                                 ccm_magnifier_get_option (self)->area.y - 8,
                                 ccm_magnifier_get_option (self)->area.width + 16, 8);
                cairo_rectangle (context, ccm_magnifier_get_option (self)->area.x - 8,
                                 ccm_magnifier_get_option (self)->area.y + 
                                 ccm_magnifier_get_option (self)->area.height,
                                 ccm_magnifier_get_option (self)->area.width + 16, 8);
                cairo_rectangle (context, ccm_magnifier_get_option (self)->area.x - 8,
                                 ccm_magnifier_get_option (self)->area.y - 8, 8,
                                 ccm_magnifier_get_option (self)->area.height + 16);
                cairo_rectangle (context,
                                 ccm_magnifier_get_option (self)->area.x + 
                                 ccm_magnifier_get_option (self)->area.width,
                                 ccm_magnifier_get_option (self)->area.y - 8, 8,
                                 ccm_magnifier_get_option (self)->area.height + 16);
                cairo_clip (context);

                cairo_set_source_rgba (context, 1.0f, 1.0f, 1.0f, 0.8f);
                cairo_rectangle_round (context, ccm_magnifier_get_option (self)->area.x - 8,
                                       ccm_magnifier_get_option (self)->area.y - 8,
                                       ccm_magnifier_get_option (self)->area.width + 16,
                                       ccm_magnifier_get_option (self)->area.height + 16, 10.0f,
                                       CAIRO_CORNER_ALL);
                cairo_fill (context);
                cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.9f);
                cairo_rectangle (context, ccm_magnifier_get_option (self)->area.x - 1,
                                 ccm_magnifier_get_option (self)->area.y - 1,
                                 ccm_magnifier_get_option (self)->area.width + 2,
                                 ccm_magnifier_get_option (self)->area.height + 2);
                cairo_fill (context);
                cairo_restore (context);

                ccm_magnifier_paint_window_info (self, context);
            }
        }

        if (self->priv->damaged)
        {
            gint cpt, nb_rects;
            cairo_rectangle_t *rects = NULL;
            cairo_pattern_t *pattern;

            ccm_debug ("MAGNIFIER PAINT SCREEN FILL CONTENT");

            ccm_screen_add_damaged_region (screen, self->priv->damaged);

            cairo_save (context);

            ccm_debug ("MAGNIFIER PAINT SCREEN FILL TRANSLATE SCALE");

            ccm_region_get_rectangles (self->priv->damaged, &rects, &nb_rects);
            for (cpt = 0; cpt < nb_rects; ++cpt)
                cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
                                 rects[cpt].width, rects[cpt].height);
            if (rects) cairo_rectangles_free (rects, nb_rects);
            cairo_clip (context);

            ccm_debug ("MAGNIFIER PAINT SCREEN FILL CLIP");

            cairo_translate (context, ccm_magnifier_get_option (self)->area.x, 
                             ccm_magnifier_get_option (self)->area.y);
            cairo_scale (context, ccm_magnifier_get_option (self)->scale,
                         ccm_magnifier_get_option (self)->scale);
            cairo_set_source_surface (context, self->priv->surface, 0, 0);
            pattern = cairo_get_source (context);
            cairo_pattern_set_filter (pattern,
                                      ccm_magnifier_get_option (self)->quality);
            cairo_paint (context);

            ccm_debug ("MAGNIFIER PAINT SCREEN FILL PAINT");

            cairo_restore (context);

            ccm_region_destroy (self->priv->damaged);
            self->priv->damaged = NULL;
        }
        ccm_debug ("MAGNIFIER PAINT SCREEN END");

        ccm_region_destroy (area);
    }

    if (ccm_magnifier_get_option (self)->scale !=
        ccm_magnifier_get_option (self)->new_scale)
    {
        ccm_magnifier_get_option (self)->scale =
            ccm_magnifier_get_option (self)->new_scale;
        if (self->priv->surface)
            cairo_surface_destroy (self->priv->surface);
        self->priv->surface = NULL;
    }

    return ret;
}

static void
ccm_magnifier_screen_on_cursor_move (CCMScreenPlugin * plugin,
                                     CCMScreen * screen, int x, int y)
{
    CCMMagnifier *self = CCM_MAGNIFIER (plugin);

    if (ccm_magnifier_get_option (self)->enabled)
    {
        CCMDisplay *display = ccm_screen_get_display (screen);
        CCMCursor *cursor =
            (CCMCursor *) ccm_display_get_current_cursor (display, FALSE);
        gboolean damaged = FALSE;

        if (cursor)
        {
            double width = ccm_cursor_get_width (cursor);
            double height = ccm_cursor_get_height (cursor);

            if (self->priv->x_offset >
                x - ccm_magnifier_get_option (self)->border)
            {
                self->priv->x_offset =
                    MAX (ccm_magnifier_get_option (self)->restrict_area.x,
                         x - ccm_magnifier_get_option (self)->border);
                damaged = TRUE;
            }
            if (self->priv->y_offset >
                y - ccm_magnifier_get_option (self)->border)
            {
                self->priv->y_offset =
                    MAX (ccm_magnifier_get_option (self)->restrict_area.y,
                         y - ccm_magnifier_get_option (self)->border);
                damaged = TRUE;
            }
            if (self->priv->x_offset +
                (ccm_magnifier_get_option (self)->area.width /
                 ccm_magnifier_get_option (self)->scale) <
                x + width + ccm_magnifier_get_option (self)->border)
            {
                self->priv->x_offset =
                    x + ccm_magnifier_get_option (self)->border + width -
                    (ccm_magnifier_get_option (self)->area.width /
                     ccm_magnifier_get_option (self)->scale);
                if (self->priv->x_offset +
                    (ccm_magnifier_get_option (self)->area.width /
                     ccm_magnifier_get_option (self)->scale) >
                    ccm_magnifier_get_option (self)->restrict_area.width)
                    self->priv->x_offset =
                    ccm_magnifier_get_option (self)->restrict_area.width -
                    (ccm_magnifier_get_option (self)->area.width /
                     ccm_magnifier_get_option (self)->scale);
                damaged = TRUE;
            }
            if (self->priv->y_offset +
                (ccm_magnifier_get_option (self)->area.height /
                 ccm_magnifier_get_option (self)->scale) <
                y + height + ccm_magnifier_get_option (self)->border)
            {
                self->priv->y_offset =
                    y + ccm_magnifier_get_option (self)->border + height -
                    (ccm_magnifier_get_option (self)->area.height /
                     ccm_magnifier_get_option (self)->scale);
                if (self->priv->y_offset +
                    (ccm_magnifier_get_option (self)->area.height /
                     ccm_magnifier_get_option (self)->scale) >
                    ccm_magnifier_get_option (self)->restrict_area.height)
                    self->priv->y_offset =
                    ccm_magnifier_get_option (self)->restrict_area.height -
                    (ccm_magnifier_get_option (self)->area.height /
                     ccm_magnifier_get_option (self)->scale);
                damaged = TRUE;
            }
        }

        if (damaged)
        {
            CCMRegion *damage;
            cairo_rectangle_t area;

            area.x = self->priv->x_offset;
            area.y = self->priv->y_offset;
            area.width = ccm_magnifier_get_option (self)->area.width / 
                ccm_magnifier_get_option (self)->scale;
            area.height = ccm_magnifier_get_option (self)->area.height /
                ccm_magnifier_get_option (self)->scale;
            damage = ccm_region_rectangle (&area);
            ccm_debug ("MAGNIFIER POSITION CHANGED");
            ccm_screen_damage_region (self->priv->screen, damage);
            ccm_region_destroy (damage);
        }
    }

    ccm_screen_plugin_on_cursor_move (CCM_SCREEN_PLUGIN_PARENT (plugin), screen,
                                      x, y);
}

static void
ccm_magnifier_screen_paint_cursor (CCMScreenPlugin * plugin, CCMScreen * screen,
                                   cairo_t * context, int x, int y)
{
    CCMMagnifier *self = CCM_MAGNIFIER (plugin);
    CCMDisplay *display = ccm_screen_get_display (screen);
    CCMCursor *cursor =
        (CCMCursor *) ccm_display_get_current_cursor (display, FALSE);

    if (cursor)
    {
        cairo_save (context);
        cairo_translate (context, ccm_magnifier_get_option (self)->area.x, 
                         ccm_magnifier_get_option (self)->area.y);
        cairo_scale (context, ccm_magnifier_get_option (self)->scale,
                     ccm_magnifier_get_option (self)->scale);
        ccm_cursor_paint (cursor, context, x - self->priv->x_offset,
                          y - self->priv->y_offset);
        cairo_restore (context);
    }
}

static gboolean
ccm_magnifier_window_paint (CCMWindowPlugin * plugin, CCMWindow * window,
                            cairo_t * context, cairo_surface_t * surface)
{
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (window));
    CCMMagnifier *self = CCM_MAGNIFIER (_ccm_screen_get_plugin (screen,
                                                                CCM_TYPE_MAGNIFIER));
    if (ccm_magnifier_get_option (self)->enabled)
    {
        CCMRegion *damaged = (CCMRegion*)
            ccm_drawable_get_damaged(CCM_DRAWABLE(window));

        if (damaged)
        {
            CCMRegion *area = ccm_region_rectangle (&ccm_magnifier_get_option (self)->area);
            CCMRegion *tmp = ccm_region_copy (damaged);
            cairo_matrix_t matrix;

            cairo_matrix_init_scale (&matrix,
                                     ccm_magnifier_get_option (self)->scale,
                                     ccm_magnifier_get_option (self)->scale);
            cairo_matrix_translate (&matrix, -self->priv->x_offset,
                                    -self->priv->y_offset);
            ccm_region_transform (tmp, &matrix);
            cairo_matrix_init_translate (&matrix, ccm_magnifier_get_option (self)->area.x,
                                         ccm_magnifier_get_option (self)->area.y);
            ccm_region_transform (tmp, &matrix);
            ccm_region_intersect (tmp, area);

            if (!ccm_region_empty (tmp))
            {
                cairo_t *ctx = cairo_create (self->priv->surface);
                cairo_matrix_t translate;

                ccm_debug_window (window, "MAGNIFIER PAINT WINDOW");

                if (!self->priv->damaged)
                    self->priv->damaged = ccm_region_copy (tmp);
                else
                    ccm_region_union (self->priv->damaged, tmp);

                cairo_rectangle (ctx, 0, 0,
                                 ccm_magnifier_get_option (self)->area.width /
                                 ccm_magnifier_get_option (self)->scale,
                                 ccm_magnifier_get_option (self)->area.height /
                                 ccm_magnifier_get_option (self)->scale);
                cairo_clip (ctx);

                cairo_matrix_init_translate (&translate, -self->priv->x_offset,
                                             -self->priv->y_offset);

                cairo_translate (ctx, -self->priv->x_offset, -self->priv->y_offset);
                ccm_drawable_get_damage_path (CCM_DRAWABLE (window), ctx);
                cairo_clip (ctx);

                ccm_window_set_redirect(window, FALSE);
                ccm_drawable_push_matrix (CCM_DRAWABLE (window), "CCMMagnifier",
                                          &translate);

                ccm_window_plugin_paint (CCM_WINDOW_PLUGIN_PARENT (plugin), window, ctx, surface);
                cairo_destroy (ctx);
                ccm_drawable_pop_matrix (CCM_DRAWABLE (window), "CCMMagnifier");
                ccm_window_set_redirect(window, TRUE);
            }

            ccm_region_destroy (tmp);
            ccm_region_destroy (area);
        }
    }

    return ccm_window_plugin_paint (CCM_WINDOW_PLUGIN_PARENT (plugin), window, context, surface);
}

static void
ccm_magnifier_screen_iface_init (CCMScreenPluginClass * iface)
{
    iface->load_options = ccm_magnifier_screen_load_options;
    iface->paint = ccm_magnifier_screen_paint;
    iface->add_window = NULL;
    iface->remove_window = NULL;
    iface->damage = NULL;
    iface->on_cursor_move = ccm_magnifier_screen_on_cursor_move;
    iface->paint_cursor = ccm_magnifier_screen_paint_cursor;
}

static void
ccm_magnifier_window_iface_init (CCMWindowPluginClass * iface)
{
    iface->load_options = NULL;
    iface->query_geometry = NULL;
    iface->paint = ccm_magnifier_window_paint;
    iface->map = NULL;
    iface->unmap = NULL;
    iface->query_opacity = NULL;
    iface->move = NULL;
    iface->resize = NULL;
    iface->set_opaque_region = NULL;
    iface->get_origin = NULL;
}
