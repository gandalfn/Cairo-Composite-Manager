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

#include "ccm-debug.h"
#include "ccm-cairo-utils.h"
#include "ccm-drawable.h"
#include "ccm-config.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-window.h"
#include "ccm-decoration.h"
#include "ccm-keybind.h"
#include "ccm-preferences-page-plugin.h"
#include "ccm-config-check-button.h"
#include "ccm-config-adjustment.h"
#include "ccm.h"

enum
{
    CCM_DECORATION_OPACITY,
    CCM_DECORATION_GRADIANT,
    CCM_DECORATION_OPTION_N
};

static const gchar *CCMDecorationOptionKeys[CCM_DECORATION_OPTION_N] = {
    "opacity",
    "gradiant"
};

typedef struct
{
    CCMPluginOptions parent_instance;

    float opacity;
    gboolean gradiant;
} CCMDecorationOptions;

static void ccm_decoration_window_iface_init (CCMWindowPluginClass * iface);
static void
ccm_decoration_preferences_page_iface_init (CCMPreferencesPagePluginClass *
                                            iface);

static void ccm_decoration_create_mask (CCMDecoration * self);
static void ccm_decoration_on_property_changed (CCMDecoration * self,
                                                CCMPropertyType changed,
                                                CCMWindow * window);
static void ccm_decoration_on_opacity_changed (CCMDecoration * self,
                                               CCMWindow * window);
static void ccm_decoration_on_option_changed (CCMPlugin * plugin, int index);

CCM_DEFINE_PLUGIN_WITH_OPTIONS (CCMDecoration, ccm_decoration, CCM_TYPE_PLUGIN,
                                CCM_IMPLEMENT_INTERFACE (ccm_decoration,
                                                         CCM_TYPE_WINDOW_PLUGIN,
                                                         ccm_decoration_window_iface_init);
                                CCM_IMPLEMENT_INTERFACE (ccm_decoration,
                                                         CCM_TYPE_PREFERENCES_PAGE_PLUGIN,
                                                         ccm_decoration_preferences_page_iface_init))
struct _CCMDecorationPrivate
{
    CCMWindow* window;

	gboolean enabled;
	
    int top;
    int bottom;
    int left;
    int right;

    CCMRegion* geometry;
    CCMRegion* opaque;

    gboolean locked;

    GtkBuilder* builder;

    gulong id_property_changed;
    gulong id_opacity_changed;
};

#define CCM_DECORATION_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DECORATION, CCMDecorationPrivate))

static void
ccm_decoration_options_init (CCMDecorationOptions* self)
{
    self->gradiant = TRUE;
    self->opacity = 1.0;
}

static void
ccm_decoration_options_finalize (CCMDecorationOptions* self)
{
}

static void
ccm_decoration_options_changed (CCMDecorationOptions* self, CCMConfig* config)
{
	GError *error = NULL;

	if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
	                                            CCM_DECORATION_GRADIANT))
	{
	    self->gradiant = ccm_config_get_boolean (config, NULL);
	}

	if (config == ccm_plugin_options_get_config(CCM_PLUGIN_OPTIONS(self),
	                                            CCM_DECORATION_OPACITY))
	{
		gfloat real_opacity;
		gfloat opacity;

		real_opacity = ccm_config_get_float (config, &error);
		if (error)
		{
		    g_warning ("Error on get opacity configuration value %s",
		               error->message);
		    g_error_free (error);
		    real_opacity = 0.8f;
		}
		opacity = MAX (0.0f, real_opacity);
		opacity = MIN (1.0f, opacity);
		if (self->opacity != opacity)
		{
		    self->opacity = opacity;
		    if (opacity != real_opacity)
		        ccm_config_set_float (config, opacity, NULL);
		}
	}
}


static void
ccm_decoration_init (CCMDecoration * self)
{
    self->priv = CCM_DECORATION_GET_PRIVATE (self);
	self->priv->enabled = FALSE;
    self->priv->window = NULL;
    self->priv->top = 0;
    self->priv->bottom = 0;
    self->priv->left = 0;
    self->priv->right = 0;
    self->priv->geometry = NULL;
    self->priv->opaque = NULL;
    self->priv->locked = FALSE;
    self->priv->builder = NULL;
}

static void
ccm_decoration_finalize (GObject * object)
{
    CCMDecoration *self = CCM_DECORATION (object);

    if (CCM_IS_WINDOW (self->priv->window)
        && G_OBJECT (self->priv->window)->ref_count)
    {
        g_object_set (self->priv->window, "mask", NULL, NULL);
        g_signal_handler_disconnect (self->priv->window,
                                     self->priv->id_property_changed);
        g_signal_handler_disconnect (self->priv->window,
                                     self->priv->id_opacity_changed);
    }
    self->priv->window = NULL;

    if (self->priv->opaque)
        ccm_region_destroy (self->priv->opaque);
    self->priv->opaque = NULL;

    if (self->priv->geometry)
        ccm_region_destroy (self->priv->geometry);
    self->priv->geometry = NULL;

    if (self->priv->builder)
        g_object_unref (self->priv->builder);

    ccm_plugin_options_unload (CCM_PLUGIN (self));

    G_OBJECT_CLASS (ccm_decoration_parent_class)->finalize (object);
}

static void
ccm_decoration_class_init (CCMDecorationClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMDecorationPrivate));

    object_class->finalize = ccm_decoration_finalize;
}

static void
ccm_decoration_on_option_changed (CCMPlugin * plugin, int index)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    if (self->priv->enabled)
    {
        ccm_decoration_create_mask (self);
        ccm_drawable_damage (CCM_DRAWABLE (self->priv->window));
    }
}

static void
ccm_decoration_on_property_changed (CCMDecoration * self,
                                    CCMPropertyType changed, CCMWindow * window)
{
    if (changed == CCM_PROPERTY_OPACITY && !self->priv->locked)
    {
		if (self->priv->enabled)
		{
			ccm_decoration_create_mask (self);
			ccm_drawable_damage (CCM_DRAWABLE(window));
		}
	}
    else if (changed == CCM_PROPERTY_FRAME_EXTENDS)
    {
		if (ccm_window_is_decorated (window) &&
    		!ccm_window_is_fullscreen (window))
		{
			self->priv->enabled = TRUE;
			if (!self->priv->locked)
			{
				ccm_decoration_create_mask (self);
				ccm_drawable_damage (CCM_DRAWABLE(window));
			}
		}
		else
		{
			if (self->priv->opaque)
				ccm_region_destroy(self->priv->opaque);
			self->priv->opaque = NULL;
			self->priv->enabled = FALSE;
			if (!self->priv->locked)
			{
				ccm_window_set_mask(window, NULL);
				ccm_drawable_damage (CCM_DRAWABLE(window));
			}
		}
	}
	else if (changed == CCM_PROPERTY_MWM_HINTS)
	{
		if (self->priv->enabled && !ccm_window_is_decorated(window))
		{
			if (self->priv->opaque)
				ccm_region_destroy(self->priv->opaque);
			self->priv->opaque = NULL;
			self->priv->enabled = FALSE;
			if (!self->priv->locked)
			{
				ccm_window_set_mask(window, NULL);
				ccm_drawable_damage (CCM_DRAWABLE(window));
			}
		}
		else if (!self->priv->enabled && ccm_window_is_decorated(window))
		{
			self->priv->enabled = TRUE;
			if (!self->priv->locked)
			{
				ccm_decoration_create_mask (self);
				ccm_drawable_damage (CCM_DRAWABLE(window));
			}
		}
	}
}

static void
ccm_decoration_on_opacity_changed (CCMDecoration * self, CCMWindow * window)
{
    if (self->priv->enabled && !self->priv->locked)
    {
        ccm_decoration_create_mask (self);
    }
}

static void
ccm_decoration_window_load_options (CCMWindowPlugin * plugin,
                                    CCMWindow * window)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    self->priv->window = window;

    ccm_plugin_options_load (CCM_PLUGIN (self), "decoration",
                             CCMDecorationOptionKeys, CCM_DECORATION_OPTION_N,
                             ccm_decoration_on_option_changed);

    ccm_window_plugin_load_options (CCM_WINDOW_PLUGIN_PARENT (plugin), window);

    self->priv->id_property_changed =
        g_signal_connect_swapped (window, "property-changed",
                                  G_CALLBACK
                                  (ccm_decoration_on_property_changed), self);
    self->priv->id_opacity_changed =
        g_signal_connect_swapped (window, "opacity-changed",
                                  G_CALLBACK
                                  (ccm_decoration_on_opacity_changed), self);
}

static void
ccm_decoration_create_mask (CCMDecoration * self)
{
    g_return_if_fail (self != NULL);

    cairo_surface_t *mask = NULL;
    cairo_surface_t *surface =
            ccm_drawable_get_surface (CCM_DRAWABLE (self->priv->window));

    ccm_debug ("CREATE MASK");

    if (self->priv->opaque)
        ccm_region_destroy (self->priv->opaque);
    self->priv->opaque = NULL;

    g_object_set (self->priv->window, "mask", mask, "mask_width", 0,
                  "mask_height", 0, NULL);

    ccm_window_get_frame_extends (self->priv->window, &self->priv->left,
                                  &self->priv->right, &self->priv->top,
                                  &self->priv->bottom);

    if (surface && (self->priv->left || self->priv->right || 
                    self->priv->top || self->priv->bottom))
    {
        cairo_t *ctx;
        cairo_pattern_t *pattern = NULL;
        cairo_rectangle_t clipbox, *rects = NULL;
        gint cpt, nb_rects;
        gfloat opacity = ccm_window_get_opacity (self->priv->window);
        CCMRegion *decoration, *tmp;
        gboolean y_invert;

        g_object_get (self->priv->window, "mask_y_invert", &y_invert, NULL);

        ccm_region_get_clipbox (self->priv->geometry, &clipbox);
        mask = cairo_surface_create_similar (surface, CAIRO_CONTENT_ALPHA,
                                             clipbox.width, clipbox.height);
		
        g_object_set (self->priv->window, "mask", mask, "mask_width",
                      (int) clipbox.width, "mask_height", (int) clipbox.height,
                      NULL);

        ctx = cairo_create (mask);
        if (y_invert)
        {
            cairo_scale (ctx, 1, -1);
            cairo_translate (ctx, 0.0f, -clipbox.height);
        }
        cairo_set_operator (ctx, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba (ctx, 0, 0, 0, 0);
        cairo_paint (ctx);

        if (ccm_decoration_get_option (self)->gradiant)
        {
            pattern =
                cairo_pattern_create_linear (clipbox.x + clipbox.width / 2,
                                             clipbox.y,
                                             clipbox.x + clipbox.width / 2,
                                             clipbox.height);
            cairo_pattern_add_color_stop_rgba (pattern, 0, 1, 1, 1,
                                               ccm_decoration_get_option
                                               (self)->opacity * opacity);
            cairo_pattern_add_color_stop_rgba (pattern,
                                               (double) self->priv->top /
                                               (double) clipbox.height, 1, 1, 1,
                                               opacity);
            cairo_pattern_add_color_stop_rgba (pattern, 1, 1, 1, 1, opacity);
        }
        cairo_translate (ctx, -clipbox.x, -clipbox.y);

        clipbox.x += self->priv->left;
        clipbox.y += self->priv->top;
        clipbox.width -= self->priv->left + self->priv->right;
        clipbox.height -= self->priv->top + self->priv->bottom;

        if (clipbox.width > 0 && clipbox.height > 0)
            self->priv->opaque = ccm_region_rectangle (&clipbox);

        decoration = ccm_region_copy (self->priv->geometry);

        if (pattern)
            cairo_set_source (ctx, pattern);
        else
            cairo_set_source_rgba (ctx, 1, 1, 1,
                                   ccm_decoration_get_option (self)->opacity *
                                   opacity);

        tmp = ccm_region_rectangle (&clipbox);
        ccm_region_subtract (decoration, tmp);
        ccm_region_destroy (tmp);

        ccm_region_get_rectangles (decoration, &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; ++cpt)
            cairo_rectangle (ctx, rects[cpt].x, rects[cpt].y, rects[cpt].width,
                             rects[cpt].height);
        if (rects) cairo_rectangles_free (rects, nb_rects);
        cairo_fill (ctx);
        if (pattern)
            cairo_pattern_destroy (pattern);
        ccm_region_destroy (decoration);

        if (clipbox.width > 0 && clipbox.height > 0)
        {
            cairo_set_source_rgba (ctx, 1, 1, 1, opacity);
            cairo_rectangle (ctx, clipbox.x, clipbox.y, clipbox.width,
                             clipbox.height);
            cairo_fill (ctx);
        }

        cairo_destroy (ctx);
    }
	if (surface) cairo_surface_destroy (surface);
}

static CCMRegion *
ccm_decoration_window_query_geometry (CCMWindowPlugin * plugin,
                                      CCMWindow * window)
{
    CCMDecoration *self = CCM_DECORATION (plugin);
    CCMRegion *geometry = NULL;

	if (self->priv->geometry)
	{
		ccm_region_destroy (self->priv->geometry);
		self->priv->geometry = NULL;
	}
	if (self->priv->opaque)
	{
		ccm_region_destroy (self->priv->opaque);
		self->priv->opaque = NULL;
	}
	
    geometry = ccm_window_plugin_query_geometry (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                                 window);
		
    if (geometry && !ccm_region_empty (geometry))
	{
		self->priv->geometry = ccm_region_copy (geometry);

		if (ccm_window_is_decorated (window) && !ccm_window_is_fullscreen (window))
		{
			self->priv->enabled = TRUE;
			ccm_decoration_create_mask (self);
		}
		else
		{
			self->priv->enabled = FALSE;
		}
	}

    return geometry;
}

static gboolean
ccm_decoration_window_paint (CCMWindowPlugin * plugin, CCMWindow * window,
                             cairo_t * context, cairo_surface_t * surface,
                             gboolean y_invert)
{
    CCMDecoration *self = CCM_DECORATION (plugin);
    gboolean ret = FALSE;
    cairo_surface_t *mask = NULL;

    if (self->priv->enabled)
    {
        CCMRegion *decoration = ccm_region_copy (self->priv->geometry);
        CCMRegion *damaged = NULL;

        if (self->priv->opaque)
            ccm_region_subtract (decoration, self->priv->opaque);

        damaged = (CCMRegion*)ccm_drawable_get_damaged(CCM_DRAWABLE(window));
        if (damaged)
        {
            ccm_region_intersect (decoration, damaged);
            if (ccm_region_empty (decoration))
            {
                mask = ccm_window_get_mask(window);
                if (mask)
                {
                    cairo_surface_reference (mask);
                    ccm_window_set_mask(window, NULL);
                }
            }
        }
        ccm_region_destroy (decoration);
    }

    ret =
        ccm_window_plugin_paint (CCM_WINDOW_PLUGIN_PARENT (plugin), window,
                                 context, surface, y_invert);

    if (mask) ccm_window_set_mask(window, mask);

    return ret;
}

static void
ccm_decoration_on_map_unmap_unlocked (CCMDecoration * self)
{
    ccm_debug ("UNLOCK");

    self->priv->locked = FALSE;
	if (self->priv->enabled)
	{
		ccm_decoration_create_mask (self);
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
	}
}

static void
ccm_decoration_window_map (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    ccm_debug ("MAP");
	if (self->priv->enabled)
	{
		CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD (plugin, map,
		                                    (CCMPluginUnlockFunc)
		                                    ccm_decoration_on_map_unmap_unlocked,
		                                    self);
		self->priv->locked = TRUE;
		ccm_window_set_mask(window, NULL);

		ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT (plugin), window);

		CCM_WINDOW_PLUGIN_UNLOCK_ROOT_METHOD (plugin, map);
		ccm_window_plugin_map ((CCMWindowPlugin *) window, window);
	}
	else
		ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT (plugin), window);
}

static void
ccm_decoration_window_unmap (CCMWindowPlugin * plugin, CCMWindow * window)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    ccm_debug ("UNMAP");
    if (self->priv->enabled)
	{
		CCM_WINDOW_PLUGIN_LOCK_ROOT_METHOD (plugin, unmap, NULL, NULL);
		self->priv->locked = TRUE;
		ccm_window_set_mask(window, NULL);

		ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT (plugin), window);

		CCM_WINDOW_PLUGIN_UNLOCK_ROOT_METHOD (plugin, unmap);
		ccm_window_plugin_unmap ((CCMWindowPlugin *) window, window);
	}
	else
		ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT (plugin), window);
}

static void
ccm_decoration_window_set_opaque_region (CCMWindowPlugin * plugin,
                                         CCMWindow * window,
                                         const CCMRegion * area)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    if (self->priv->enabled && self->priv->opaque)
    {
        CCMRegion *opaque = NULL;

        if (area)
        {
            opaque = ccm_region_copy (self->priv->opaque);
			ccm_region_intersect (opaque, (CCMRegion *) area);
        }

        ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                             window, opaque);
        if (opaque)
            ccm_region_destroy (opaque);
    }
    else
        ccm_window_plugin_set_opaque_region (CCM_WINDOW_PLUGIN_PARENT (plugin),
                                             window, area);
}

static void
ccm_decoration_window_move (CCMWindowPlugin * plugin, CCMWindow * window, int x,
                            int y)
{
    CCMDecoration *self = CCM_DECORATION (plugin);
    cairo_rectangle_t clipbox;

    if (self->priv->enabled && self->priv->geometry)
    {
        ccm_region_get_clipbox (self->priv->geometry, &clipbox);
        if (x != clipbox.x || y != clipbox.y)
        {
            ccm_region_offset (self->priv->geometry, x - clipbox.x,
                               y - clipbox.y);
            if (self->priv->opaque)
            {
                ccm_region_get_clipbox (self->priv->opaque, &clipbox);
                ccm_region_offset (self->priv->opaque,
                                   (x - clipbox.x) + self->priv->left,
                                   (y - clipbox.y) + self->priv->top);
            }
        }
        else
            return;
    }

    ccm_window_plugin_move (CCM_WINDOW_PLUGIN_PARENT (plugin), window, x, y);
}

static void
ccm_decoration_window_resize (CCMWindowPlugin * plugin, CCMWindow * window,
                              int width, int height)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    if (self->priv->enabled && self->priv->geometry)
    {
        cairo_rectangle_t clipbox;

        ccm_region_get_clipbox (self->priv->geometry, &clipbox);
        if (width != clipbox.width || height != clipbox.height)
        {
            ccm_region_resize (self->priv->geometry, width, height);

            if (self->priv->opaque)
                ccm_region_scale (self->priv->opaque,
                                  (gdouble) width / clipbox.width,
                                  (gdouble) height / clipbox.height);

            ccm_decoration_create_mask (self);
        }
        else
            return;
    }

    ccm_window_plugin_resize (CCM_WINDOW_PLUGIN_PARENT (plugin), window, width,
                              height);
}

static void
ccm_decoration_preferences_page_init_windows_section (CCMPreferencesPagePlugin *
                                                      plugin,
                                                      CCMPreferencesPage *
                                                      preferences,
                                                      GtkWidget *
                                                      windows_section)
{
    CCMDecoration *self = CCM_DECORATION (plugin);

    self->priv->builder = gtk_builder_new ();

    if (gtk_builder_add_from_file
        (self->priv->builder, UI_DIR "/ccm-decoration.ui", NULL))
    {
        GtkWidget *widget =
            GTK_WIDGET (gtk_builder_get_object
                        (self->priv->builder, "decoration"));
        if (widget)
        {
            gint screen_num = ccm_preferences_page_get_screen_num (preferences);

            gtk_box_pack_start (GTK_BOX (windows_section), widget, FALSE, TRUE,
                                0);

            CCMConfigCheckButton *gradiant =
                CCM_CONFIG_CHECK_BUTTON (gtk_builder_get_object
                                         (self->priv->builder,
                                          "gradiant"));
            g_object_set (gradiant, "screen", screen_num, NULL);

            CCMConfigAdjustment *opacity =
                CCM_CONFIG_ADJUSTMENT (gtk_builder_get_object
                                       (self->priv->builder,
                                        "alpha-adjustment"));
            g_object_set (opacity, "screen", screen_num, NULL);

            ccm_preferences_page_section_register_widget (preferences,
                                                          CCM_PREFERENCES_PAGE_SECTION_WINDOW,
                                                          widget, "decoration");
        }
    }
    ccm_preferences_page_plugin_init_windows_section
        (CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin), preferences,
         windows_section);
}

static void
ccm_decoration_window_iface_init (CCMWindowPluginClass * iface)
{
    iface->load_options = ccm_decoration_window_load_options;
    iface->query_geometry = ccm_decoration_window_query_geometry;
    iface->paint = ccm_decoration_window_paint;
    iface->map = ccm_decoration_window_map;
    iface->unmap = ccm_decoration_window_unmap;
    iface->query_opacity = NULL;
    iface->move = ccm_decoration_window_move;
    iface->resize = ccm_decoration_window_resize;
    iface->set_opaque_region = ccm_decoration_window_set_opaque_region;
    iface->get_origin = NULL;
}

static void
ccm_decoration_preferences_page_iface_init (CCMPreferencesPagePluginClass *
                                            iface)
{
    iface->init_general_section = NULL;
    iface->init_desktop_section = NULL;
    iface->init_windows_section =
        ccm_decoration_preferences_page_init_windows_section;
    iface->init_effects_section = NULL;
    iface->init_accessibility_section = NULL;
    iface->init_utilities_section = NULL;
}
