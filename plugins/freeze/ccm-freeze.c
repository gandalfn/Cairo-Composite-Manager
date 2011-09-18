/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-freeze.c
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

#include <X11/Xlib.h>
#include <math.h>

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-timeline.h"
#include "ccm-freeze.h"
#include "ccm-config.h"
#include "ccm-debug.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-adjustment.h"
#include "ccm-config-color-button.h"
#include "ccm.h"

enum
{
    CCM_FREEZE_DELAY,
    CCM_FREEZE_DURATION,
    CCM_FREEZE_COLOR,
    CCM_FREEZE_OPTION_N
};

static const gchar *CCMFreezeOptionKeys[CCM_FREEZE_OPTION_N] = {
    "delay",
    "duration",
    "color"
};

typedef struct
{
    CCMPluginOptions parent;
    gint delay;
    gfloat duration;
    GdkColor *color;
} CCMFreezeOptions;

static void ccm_freeze_screen_iface_init (CCMScreenPluginClass * iface);
static void ccm_freeze_window_iface_init (CCMWindowPluginClass * iface);
static void
ccm_freeze_preferences_page_iface_init (CCMPreferencesPagePluginClass * iface);
static void ccm_freeze_on_event (CCMFreeze * self, XEvent * event,
                                 CCMDisplay * display);
static void ccm_freeze_on_option_changed (CCMPlugin * plugin, int index);

CCM_DEFINE_PLUGIN_WITH_OPTIONS (CCMFreeze, ccm_freeze, CCM_TYPE_PLUGIN,
                                CCM_IMPLEMENT_INTERFACE (ccm_freeze, CCM_TYPE_SCREEN_PLUGIN,
                                                         ccm_freeze_screen_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_freeze, CCM_TYPE_WINDOW_PLUGIN,
                                                         ccm_freeze_window_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_freeze,
                                                         CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                                         ccm_freeze_preferences_page_iface_init))
struct _CCMFreezePrivate
{
    gboolean     alive;
    gfloat	     opacity;

    CCMScreen*	 screen;
    CCMWindow*   window;

    guint	     id_ping;
    glong        last_ping;
    glong        pid;

    CCMTimeline* timeline;
    GtkBuilder*  builder;

    gulong		 id_event;
};

#define CCM_FREEZE_GET_PRIVATE(o)  \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FREEZE, CCMFreezePrivate))

static void
ccm_freeze_options_init (CCMFreezeOptions* self)
{
    self->delay = 3;
    self->duration = 0.3f;
    self->color = NULL;
}

static void
ccm_freeze_options_finalize (CCMFreezeOptions* self)
{
    if (self->color) g_free (self->color);
    self->color = NULL;
}

static void
ccm_freeze_options_changed (CCMFreezeOptions* self, CCMConfig* config)
{
    if (config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                 CCM_FREEZE_DELAY))
    {
        self->delay = ccm_config_get_integer (config, NULL);
        if (!self->delay) self->delay = 3;
    }

    if (config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                 CCM_FREEZE_DURATION))
    {
        self->duration = ccm_config_get_float (config, NULL);
        if (!self->duration) self->duration = 0.3f;
    }

    if (config == ccm_plugin_options_get_config (CCM_PLUGIN_OPTIONS(self), 
                                                 CCM_FREEZE_COLOR))
    {
        if (self->color) g_free (self->color);

        self->color = ccm_config_get_color (config, NULL);
    }
}

static void
ccm_freeze_init (CCMFreeze * self)
{
    self->priv = CCM_FREEZE_GET_PRIVATE (self);
    self->priv->alive = TRUE;
    self->priv->opacity = 0.0f;
    self->priv->screen = NULL;
    self->priv->window = NULL;
    self->priv->id_ping = 0;
    self->priv->last_ping = 0;
    self->priv->pid = 0;
    self->priv->timeline = NULL;
    self->priv->builder = NULL;
    self->priv->id_event = 0;
}

static void
ccm_freeze_finalize (GObject * object)
{
    CCMFreeze *self = CCM_FREEZE (object);

    if (self->priv->window)
        ccm_plugin_options_unload (CCM_PLUGIN (self));

    if (self->priv->screen && self->priv->id_event)
    {
        CCMDisplay *display = ccm_screen_get_display(self->priv->screen);

        if (g_signal_handler_is_connected(display, self->priv->id_event))
            g_signal_handler_disconnect (display, self->priv->id_event);
    }

    if (self->priv->id_ping)
        g_source_remove (self->priv->id_ping);
    self->priv->opacity = 0.0f;
    self->priv->id_ping = 0;
    self->priv->last_ping = 0;
    if (self->priv->timeline)
        g_object_unref (self->priv->timeline);
    if (self->priv->builder)
        g_object_unref (self->priv->builder);

    G_OBJECT_CLASS (ccm_freeze_parent_class)->finalize (object);
}

static void
ccm_freeze_class_init (CCMFreezeClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMFreezePrivate));

    object_class->finalize = ccm_freeze_finalize;
}

static void
ccm_freeze_on_new_frame (CCMFreeze * self, guint num_frame,
                         CCMTimeline * timeline)
{
    if (!self->priv->alive && self->priv->opacity < 0.5)
    {
        gfloat progress = ccm_timeline_get_progress (timeline);

        self->priv->opacity = progress / 2.0f;
        ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));
    }
}

static void
ccm_freeze_on_event (CCMFreeze * self, XEvent * event, CCMDisplay * display)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (event != NULL);

    if (event->type == ClientMessage)
    {
        CCMWindow* root = ccm_screen_get_root_window(self->priv->screen);
        XClientMessageEvent *client_message_event = (XClientMessageEvent *) event;

        if (client_message_event->message_type == CCM_WINDOW_GET_CLASS (root)->protocol_atom &&
            client_message_event->data.l[0] == CCM_WINDOW_GET_CLASS (root)->ping_atom)
        {
            CCMWindow* window = ccm_screen_find_window_or_child(self->priv->screen,
                                                                client_message_event->data.l[2]);

            if (window)
            {
                CCMFreeze* freeze = CCM_FREEZE(_ccm_window_get_plugin(window,
                                                                      CCM_TYPE_FREEZE));

                if (freeze->priv->last_ping)
                {
                    ccm_debug_window (window, "PONG 0x%x",
                                      client_message_event->window);
                    if (!freeze->priv->alive)
                        ccm_drawable_damage (CCM_DRAWABLE (window));
                    freeze->priv->alive = TRUE;
                    freeze->priv->last_ping = 0;
                    freeze->priv->opacity = 0.0f;
                    if (freeze->priv->timeline && 
                        ccm_timeline_get_is_playing (freeze->priv->timeline))
                        ccm_timeline_stop (freeze->priv->timeline);
                }
            }
        }
    }
}

static void
ccm_freeze_get_pid (CCMFreeze * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (self->priv->window != NULL);

    guint32 *data = NULL;
    guint n_items;
    Window child = _ccm_window_get_child(self->priv->window);

    if (!child)
    {
        data =
            ccm_window_get_property (self->priv->window,
                                     CCM_WINDOW_GET_CLASS (self->priv->window)->
                                     pid_atom, XA_CARDINAL, &n_items);
    }
    else
    {
        data =
            ccm_window_get_child_property (self->priv->window,
                                           CCM_WINDOW_GET_CLASS (self->priv->
                                                                 window)->
                                           pid_atom, XA_CARDINAL, &n_items);
    }

    if (data)
    {
        self->priv->pid = (gulong) * data;
        g_free (data);
    }
    else
    {
        self->priv->pid = 0;
    }
}

static gboolean
ccm_freeze_ping (CCMFreeze * self)
{
    g_return_val_if_fail (CCM_IS_FREEZE (self), FALSE);

    if (self->priv->window && ccm_window_is_viewable (self->priv->window))
    {
        CCMWindowType type = ccm_window_get_hint_type (self->priv->window);
        const gchar* name = ccm_window_get_name(self->priv->window);

        if (!self->priv->pid)
            ccm_freeze_get_pid (self);

        if ((name && !g_strcasecmp(name, "mplayer")) || 
            !self->priv->pid || ccm_window_is_input_only (self->priv->window)
            || !ccm_window_is_viewable (self->priv->window)
            || !ccm_window_is_decorated (self->priv->window)
            || (type != CCM_WINDOW_TYPE_NORMAL
                && type != CCM_WINDOW_TYPE_DIALOG))
        {
            if (!self->priv->alive)
                ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));

            self->priv->alive = TRUE;
            if (self->priv->timeline
                && ccm_timeline_get_is_playing (self->priv->timeline))
                ccm_timeline_stop (self->priv->timeline);
            return FALSE;
        }

        ccm_debug_window (self->priv->window, "PING");
        XEvent event;
        CCMDisplay *display =
            ccm_drawable_get_display (CCM_DRAWABLE (self->priv->window));
        Window window = None;

        g_return_val_if_fail (CCM_IS_DISPLAY (display), FALSE);

        window = _ccm_window_get_child(self->priv->window);

        if (!window)
            window = CCM_WINDOW_XWINDOW (self->priv->window);

        if (self->priv->last_ping)
        {
            self->priv->alive = FALSE;
            if (!self->priv->timeline)
            {
                self->priv->timeline =
                    ccm_timeline_new_for_duration ((guint)
                                                   (ccm_freeze_get_option
                                                    (self)->duration * 1000.0));

                g_signal_connect_swapped (self->priv->timeline, "new-frame",
                                          G_CALLBACK (ccm_freeze_on_new_frame),
                                          self);
            }
            if (!ccm_timeline_get_is_playing (self->priv->timeline))
                ccm_timeline_start (self->priv->timeline);
        }
        else
        {
            if (!self->priv->alive)
                ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));

            self->priv->alive = TRUE;
            self->priv->opacity = 0.0f;

            if (self->priv->timeline
                && ccm_timeline_get_is_playing (self->priv->timeline))
                ccm_timeline_stop (self->priv->timeline);
        }

        self->priv->last_ping = 1;

        event.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type =
            CCM_WINDOW_GET_CLASS (self->priv->window)->protocol_atom;
        event.xclient.format = 32;
        event.xclient.data.l[0] =
            CCM_WINDOW_GET_CLASS (self->priv->window)->ping_atom;
        event.xclient.data.l[1] = self->priv->last_ping;
        event.xclient.data.l[2] = window;
        event.xclient.data.l[3] = 0;
        event.xclient.data.l[4] = 0;
        XSendEvent (CCM_DISPLAY_XDISPLAY (display), window, FALSE, NoEventMask,
                    &event);
    }


    return TRUE;
}

static void
ccm_freeze_on_option_changed (CCMPlugin * plugin, int index)
{
    g_return_if_fail (plugin != NULL);

    CCMFreeze *self = CCM_FREEZE (plugin);

    if (index == CCM_FREEZE_DURATION && self->priv->timeline)
    {
        g_object_unref (self->priv->timeline);
        self->priv->timeline = NULL;
    }
}

static void
ccm_freeze_screen_load_options (CCMScreenPlugin * plugin, CCMScreen * screen)
{
    CCMFreeze *self = CCM_FREEZE (plugin);
    CCMDisplay *display = ccm_screen_get_display (screen);

    self->priv->screen = screen;

    ccm_screen_plugin_load_options (CCM_SCREEN_PLUGIN_PARENT (plugin), screen);

    self->priv->id_event = g_signal_connect_swapped (G_OBJECT (display), 
                                                     "event",
                                                     G_CALLBACK (ccm_freeze_on_event), 
                                                     self);
}

static void
ccm_freeze_window_load_options (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMFreeze *self = CCM_FREEZE (plugin);

    self->priv->window = window;

    ccm_plugin_options_load (CCM_PLUGIN (self), "freeze", CCMFreezeOptionKeys,
                             CCM_FREEZE_OPTION_N, ccm_freeze_on_option_changed);

    ccm_window_plugin_load_options (CCM_WINDOW_PLUGIN_PARENT (plugin), window);
}

static gboolean
ccm_freeze_window_paint (CCMWindowPlugin * plugin, CCMWindow * window,
                         cairo_t * context, cairo_surface_t * surface)
{
    CCMFreeze *self = CCM_FREEZE (plugin);
    gboolean ret;

    ret = ccm_window_plugin_paint (CCM_WINDOW_PLUGIN_PARENT (plugin), window,
                                   context, surface);

    if (ccm_window_is_viewable (window) && !self->priv->alive)
    {
        CCMRegion *tmp = ccm_window_get_area_geometry (window);
        int cpt, nb_rects;
        cairo_rectangle_t *rects;
        cairo_save (context);

        ccm_region_get_rectangles (tmp, &rects, &nb_rects);
        for (cpt = 0; cpt < nb_rects; ++cpt)
            cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
                             rects[cpt].width, rects[cpt].height);
        cairo_clip (context);
        cairo_rectangles_free (rects, nb_rects);
        ccm_region_destroy (tmp);
        if (!ccm_freeze_get_option (self)->color)
            cairo_set_source_rgb (context, 0, 0, 0);
        else
            gdk_cairo_set_source_color (context,
                                        ccm_freeze_get_option (self)->color);
        cairo_paint_with_alpha (context, self->priv->opacity);
        cairo_restore (context);
    }

    return ret;
}

static void
ccm_freeze_window_map (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMFreeze *self = CCM_FREEZE (plugin);

    if (!self->priv->id_ping)
        self->priv->id_ping =
        g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                    ccm_freeze_get_option (self)->delay,
                                    (GSourceFunc) ccm_freeze_ping, self,
                                    NULL);
    ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT (plugin), window);
}

static void
ccm_freeze_window_unmap (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMFreeze *self = CCM_FREEZE (plugin);

    self->priv->window = window;
    if (self->priv->id_ping)
    {
        g_source_remove (self->priv->id_ping);
        if (self->priv->timeline)
            ccm_timeline_stop (self->priv->timeline);
        self->priv->id_ping = 0;
    }
    ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT (plugin), window);
}

static void
ccm_freeze_preferences_page_init_windows_section (CCMPreferencesPagePlugin *
                                                  plugin,
                                                  CCMPreferencesPage *
                                                  preferences,
                                                  GtkWidget * windows_section)
{
    CCMFreeze *self = CCM_FREEZE (plugin);

    self->priv->builder = gtk_builder_new ();

    if (gtk_builder_add_from_file
        (self->priv->builder, UI_DIR "/ccm-freeze.ui", NULL))
    {
        GtkWidget *widget =
            GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "freeze"));
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

            CCMConfigAdjustment *delay =
                CCM_CONFIG_ADJUSTMENT (gtk_builder_get_object
                                       (self->priv->builder,
                                        "delay-adjustment"));
            g_object_set (delay, "screen", screen_num, NULL);

            CCMConfigAdjustment *duration =
                CCM_CONFIG_ADJUSTMENT (gtk_builder_get_object
                                       (self->priv->builder,
                                        "duration-adjustment"));
            g_object_set (duration, "screen", screen_num, NULL);

            ccm_preferences_page_section_register_widget (preferences,
                                                          CCM_PREFERENCES_PAGE_SECTION_WINDOW,
                                                          widget, "freeze");
        }
    }
    ccm_preferences_page_plugin_init_windows_section
        (CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin), preferences,
         windows_section);
}

static void
ccm_freeze_screen_iface_init (CCMScreenPluginClass * iface)
{
    iface->load_options = ccm_freeze_screen_load_options;
    iface->paint = NULL;
    iface->add_window = NULL;
    iface->remove_window = NULL;
    iface->damage = NULL;
}

static void
ccm_freeze_window_iface_init (CCMWindowPluginClass * iface)
{
    iface->load_options = ccm_freeze_window_load_options;
    iface->query_geometry = NULL;
    iface->paint = ccm_freeze_window_paint;
    iface->map = ccm_freeze_window_map;
    iface->unmap = ccm_freeze_window_unmap;
    iface->query_opacity = NULL;
    iface->move = NULL;
    iface->resize = NULL;
    iface->set_opaque_region = NULL;
    iface->get_origin = NULL;
}

static void
ccm_freeze_preferences_page_iface_init (CCMPreferencesPagePluginClass * iface)
{
    iface->init_general_section = NULL;
    iface->init_desktop_section = NULL;
    iface->init_windows_section = ccm_freeze_preferences_page_init_windows_section;
    iface->init_effects_section = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section = NULL;
}
