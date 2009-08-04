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
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <glibtop.h>
#include <glibtop/cpu.h>
#include <glibtop/proctime.h>
#include <glibtop/procmap.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>
#include <sys/types.h>
#include <unistd.h>

#include "ccm-drawable.h"
#include "ccm-config.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-perf.h"
#include "ccm-keybind.h"
#include "ccm-cairo-utils.h"
#include "ccm.h"

#define CCM_LOGO_PIXMAP 	PACKAGE_PIXMAP_DIR "/cairo-compmgr.png"

enum
{
    CCM_PERF_X,
    CCM_PERF_Y,
    CCM_PERF_SHORTCUT,
    CCM_PERF_OPTION_N
};

static const gchar *CCMPerfOptionKeys[CCM_PERF_OPTION_N] = {
    "x",
    "y",
    "shortcut"
};

typedef struct
{
    CCMPluginOptions parent;

    cairo_rectangle_t area;
    CCMKeybind *keybind;
} CCMPerfOptions;

static void ccm_perf_screen_iface_init (CCMScreenPluginClass * iface);
static void ccm_perf_window_iface_init (CCMWindowPluginClass * iface);
static void ccm_perf_on_option_changed (CCMPlugin * plugin, CCMConfig * config);

CCM_DEFINE_PLUGIN_WITH_OPTIONS (CCMPerf, ccm_perf, CCM_TYPE_PLUGIN,
                                CCM_IMPLEMENT_INTERFACE (ccm_perf, CCM_TYPE_SCREEN_PLUGIN,
                                                         ccm_perf_screen_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_perf, CCM_TYPE_WINDOW_PLUGIN,
                                                         ccm_perf_window_iface_init))
struct _CCMPerfPrivate
{
    gint frames;
    gfloat elapsed;
    GTimer* timer;
    gfloat fps;

    guint64 cpu_total;
    guint64 last_cpu_total;
    guint64 cpu_time;
    guint pcpu;

    guint64 mem_size;
    guint64 mem_xorg;

    gboolean enabled;
    gboolean need_refresh;

    CCMScreen* screen;
};

#define CCM_PERF_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PERF, CCMPerfPrivate))

static void
ccm_perf_options_init (CCMPerfOptions* self)
{
    self->keybind = NULL;
}

static void
ccm_perf_options_finalize (CCMPerfOptions* self)
{
    if (self->keybind) g_object_unref (self->keybind);
    self->keybind = NULL;
}

static void
ccm_perf_init (CCMPerf * self)
{
    self->priv = CCM_PERF_GET_PRIVATE (self);
    self->priv->frames = 0;
    self->priv->elapsed = 0.0f;
    self->priv->fps = 0.0f;
    self->priv->cpu_total = 1;
    self->priv->last_cpu_total = 1;
    self->priv->cpu_time = 0;
    self->priv->pcpu = 0;
    self->priv->mem_size = 0;
    self->priv->mem_xorg = 0;
    self->priv->enabled = FALSE;
    self->priv->need_refresh = TRUE;
    self->priv->timer = g_timer_new ();
    self->priv->screen = NULL;
}

static void
ccm_perf_finalize (GObject * object)
{
    CCMPerf *self = CCM_PERF (object);

    if (self->priv->screen)
        ccm_plugin_options_unload (CCM_PLUGIN (self));

    if (self->priv->timer)
        g_timer_destroy (self->priv->timer);

    G_OBJECT_CLASS (ccm_perf_parent_class)->finalize (object);
}

static void
ccm_perf_class_init (CCMPerfClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMPerfPrivate));

    object_class->finalize = ccm_perf_finalize;
}

static void
ccm_perf_show_text (CCMPerf * self, cairo_t * context, gchar * text, int line)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (context != NULL);

    PangoLayout *layout;
    PangoFontDescription *desc;

    layout = pango_cairo_create_layout (context);
    pango_layout_set_text (layout, text, -1);
    desc = pango_font_description_from_string ("Sans Bold 12");
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);

    cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 1.0f);
    pango_cairo_update_layout (context, layout);
    cairo_move_to (context, 80, 16 * line);
    pango_cairo_show_layout (context, layout);
    g_object_unref (layout);
}

static void
ccm_perf_get_pcpu (CCMPerf * self)
{
    g_return_if_fail (self != NULL);

    glibtop_cpu cpu;

    glibtop_get_cpu (&cpu);
    self->priv->cpu_total =
        (int) ((float) (cpu.total - self->priv->last_cpu_total) /
               (float) cpu.frequency);
    if (self->priv->cpu_total >= 1)
    {
        self->priv->last_cpu_total = cpu.total;
        pid_t pid = getpid ();
        glibtop_proc_time proctime;

        glibtop_get_proc_time (&proctime, pid);
        self->priv->pcpu =
            (proctime.rtime - self->priv->cpu_time) / self->priv->cpu_total;
        self->priv->pcpu = MIN (self->priv->pcpu, 100);
        self->priv->cpu_time = proctime.rtime;
    }
}

static void
ccm_perf_get_mem_info (CCMPerf * self)
{
    g_return_if_fail (self != NULL);

    glibtop_proc_map buf;
    glibtop_map_entry *maps;
    guint cpt;
    pid_t pid = getpid ();
    WnckResourceUsage xresources;

    self->priv->mem_size = 0;
    self->priv->mem_xorg = 0;

    maps = glibtop_get_proc_map (&buf, pid);

    for (cpt = 0; cpt < buf.number; ++cpt)
    {
        self->priv->mem_size += maps[cpt].private_dirty;
    }

    wnck_xid_read_resource_usage (gdk_display_get_default (),
                                  _ccm_screen_get_selection_owner (self->priv->
                                                                   screen),
                                  &xresources);
    self->priv->mem_xorg = xresources.total_bytes_estimate;
    wnck_pid_read_resource_usage (gdk_display_get_default (), pid, &xresources);
    self->priv->mem_xorg += xresources.total_bytes_estimate;

    g_free (maps);
}

static void
ccm_perf_on_key_press (CCMPerf * self)
{
    self->priv->enabled = ~self->priv->enabled;
    if (!self->priv->enabled)
    {
        CCMRegion *area =
            ccm_region_rectangle (&ccm_perf_get_option (self)->area);

        ccm_screen_damage_region (self->priv->screen, area);
        ccm_region_destroy (area);
    }
}

static void
ccm_perf_on_option_changed (CCMPlugin * plugin, CCMConfig * config)
{
    CCMPerf *self = CCM_PERF (plugin);
    GError *error = NULL;

    if (config == ccm_perf_get_config (self, CCM_PERF_X)
        || config == ccm_perf_get_config (self, CCM_PERF_Y))
    {
        ccm_perf_get_option (self)->area.x =
            ccm_config_get_integer (ccm_perf_get_config (self, CCM_PERF_X),
                                    &error);
        if (error)
        {
            g_error_free (error);
            error = NULL;
            ccm_perf_get_option (self)->area.x = 20;
        }

        ccm_perf_get_option (self)->area.y =
            ccm_config_get_integer (ccm_perf_get_config (self, CCM_PERF_Y),
                                    &error);
        if (error)
        {
            g_error_free (error);
            error = NULL;
            ccm_perf_get_option (self)->area.y = 20;
        }
        ccm_perf_get_option (self)->area.width = 280;
        ccm_perf_get_option (self)->area.height = 100;
    }

    if (config == ccm_perf_get_config (self, CCM_PERF_SHORTCUT))
    {
        gchar *shortcut;

        if (ccm_perf_get_option (self)->keybind)
            g_object_unref (ccm_perf_get_option (self)->keybind);

        shortcut =
            ccm_config_get_string (ccm_perf_get_config
                                   (self, CCM_PERF_SHORTCUT), &error);
        if (error)
        {
            g_error_free (error);
            error = NULL;
            shortcut = g_strdup ("<Super>f");
        }

        ccm_perf_get_option (self)->keybind =
            ccm_keybind_new (self->priv->screen, shortcut, TRUE);
        g_free (shortcut);

        g_signal_connect_swapped (ccm_perf_get_option (self)->keybind,
                                  "key_press",
                                  G_CALLBACK (ccm_perf_on_key_press), self);
    }
}

static void
ccm_perf_screen_load_options (CCMScreenPlugin * plugin, CCMScreen * screen)
{
    CCMPerf *self = CCM_PERF (plugin);

    self->priv->screen = screen;

    ccm_plugin_options_load (CCM_PLUGIN (self), "perf", CCMPerfOptionKeys,
                             CCM_PERF_OPTION_N, ccm_perf_on_option_changed);

    ccm_screen_plugin_load_options (CCM_SCREEN_PLUGIN_PARENT (plugin), screen);
}

static gboolean
ccm_perf_screen_paint (CCMScreenPlugin * plugin, CCMScreen * screen,
                       cairo_t * context)
{
    CCMPerf *self = CCM_PERF (plugin);
    gboolean ret = FALSE;

    if (self->priv->enabled)
    {
        self->priv->frames++;
        self->priv->elapsed += g_timer_elapsed (self->priv->timer, NULL) * 1000;
        if (self->priv->elapsed > 1000)
        {
            self->priv->fps = (self->priv->frames / self->priv->elapsed) * 1000;
            self->priv->elapsed = 0.0f;
            self->priv->frames = 0;
            self->priv->need_refresh = TRUE;
            CCMRegion *area =
                ccm_region_rectangle (&ccm_perf_get_option (self)->area);

            ccm_screen_damage_region (screen, area);
            ccm_region_destroy (area);
        }
        g_timer_start (self->priv->timer);
    }

    ret =
        ccm_screen_plugin_paint (CCM_SCREEN_PLUGIN_PARENT (plugin), screen,
                                 context);
    if (self->priv->enabled)
    {
        if (ret)
        {
            CCMRegion *area =
                ccm_region_rectangle (&ccm_perf_get_option (self)->area);
            CCMRegion *damaged = ccm_screen_get_damaged (screen);

            ccm_region_intersect (area, damaged);
            self->priv->need_refresh |= !ccm_region_empty (area);
            ccm_region_destroy (area);
        }

        if (self->priv->need_refresh)
        {
            cairo_surface_t *icon;
            gchar *text;
            CCMRegion *area =
                ccm_region_rectangle (&ccm_perf_get_option (self)->area);

            ccm_screen_add_damaged_region (screen, area);
            ccm_region_destroy (area);


            cairo_save (context);
            cairo_rectangle_round (context, ccm_perf_get_option (self)->area.x,
                                   ccm_perf_get_option (self)->area.y,
                                   ccm_perf_get_option (self)->area.width,
                                   ccm_perf_get_option (self)->area.height, 20,
                                   CAIRO_CORNER_ALL);
            cairo_set_source_rgba (context, 1.0f, 1.0f, 1.0f, 0.6f);
            cairo_fill (context);
            icon = cairo_image_surface_create_from_png (CCM_LOGO_PIXMAP);
            cairo_translate (context, ccm_perf_get_option (self)->area.x + 16,
                             ccm_perf_get_option (self)->area.y + 16);
            cairo_set_source_surface (context, icon, 0, 0);
            cairo_paint (context);
            cairo_surface_destroy (icon);

            text = g_strdup_printf ("Rate\t: %i FPS", (int) self->priv->fps);
            ccm_perf_show_text (self, context, text, 0);
            g_free (text);

            ccm_perf_get_pcpu (self);
            text = g_strdup_printf ("CPU\t: %i %%", self->priv->pcpu);
            ccm_perf_show_text (self, context, text, 1);
            g_free (text);

            ccm_perf_get_mem_info (self);
            text =
                g_strdup_printf ("Mem : %li Kb",
                                 (glong) (self->priv->mem_size / 1024));
            ccm_perf_show_text (self, context, text, 2);
            g_free (text);
            text =
                g_strdup_printf ("XMem : %li Kb",
                                 (glong) (self->priv->mem_xorg / 1024));
            ccm_perf_show_text (self, context, text, 3);
            g_free (text);

            cairo_restore (context);
            self->priv->need_refresh = FALSE;
        }
    }

    return ret;
}

static gboolean
ccm_perf_window_paint (CCMWindowPlugin * plugin, CCMWindow * window,
                       cairo_t * context, cairo_surface_t * surface,
                       gboolean y_invert)
{
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (window));
    CCMPerf *self = CCM_PERF (_ccm_screen_get_plugin (screen, CCM_TYPE_PERF));
    
    if (self->priv->enabled && !self->priv->need_refresh)
    {
		CCMRegion *damaged = 
			(CCMRegion*)ccm_drawable_get_damaged(CCM_DRAWABLE(window));
		if (damaged && !ccm_region_empty(damaged))
		{
		    CCMRegion *area =
		        ccm_region_rectangle (&ccm_perf_get_option (self)->area);

		    ccm_region_intersect (area, damaged);
		    self->priv->need_refresh = !ccm_region_empty (area);
		    ccm_region_destroy (area);
		}
    }

    return ccm_window_plugin_paint (CCM_WINDOW_PLUGIN_PARENT (plugin), window,
                                    context, surface, y_invert);
}

static void
ccm_perf_screen_iface_init (CCMScreenPluginClass * iface)
{
    iface->load_options = ccm_perf_screen_load_options;
    iface->paint = ccm_perf_screen_paint;
    iface->add_window = NULL;
    iface->remove_window = NULL;
    iface->damage = NULL;
}

static void
ccm_perf_window_iface_init (CCMWindowPluginClass * iface)
{
    iface->load_options = NULL;
    iface->query_geometry = NULL;
    iface->paint = ccm_perf_window_paint;
    iface->map = NULL;
    iface->unmap = NULL;
    iface->query_opacity = NULL;
    iface->move = NULL;
    iface->resize = NULL;
    iface->set_opaque_region = NULL;
    iface->get_origin = NULL;
}
