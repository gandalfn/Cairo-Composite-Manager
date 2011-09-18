/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-window-xrender.c
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

#include "ccm-window-xrender.h"

#include <X11/extensions/Xdbe.h>
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-pixmap.h"
#include "ccm-window-xrender.h"

G_DEFINE_TYPE (CCMWindowXRender, ccm_window_xrender, CCM_TYPE_WINDOW);

struct _CCMWindowXRenderPrivate
{
    cairo_surface_t* front;
    cairo_surface_t* back;
    Drawable         back_buffer;
};

#define CCM_WINDOW_XRENDER_GET_PRIVATE(o)  \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW_X_RENDER, CCMWindowXRenderPrivate))

static cairo_surface_t *ccm_window_xrender_get_surface (CCMDrawable * drawable);
static void ccm_window_xrender_flush (CCMDrawable * drawable);
static void ccm_window_xrender_flush_region (CCMDrawable * drawable,
                                             CCMRegion * region);
static CCMPixmap *ccm_window_xrender_create_pixmap (CCMWindow * self, int width,
                                                    int height, int depth);

static void
ccm_window_xrender_init (CCMWindowXRender * self)
{
    self->priv = CCM_WINDOW_XRENDER_GET_PRIVATE (self);
    self->priv->front = NULL;
    self->priv->back = NULL;
    self->priv->back_buffer = None;
}

static void
ccm_window_xrender_finalize (GObject * object)
{
    CCMWindowXRender *self = CCM_WINDOW_X_RENDER (object);
    CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));

    if (self->priv->back_buffer) 
    {
        XdbeDeallocateBackBufferName(CCM_DISPLAY_XDISPLAY(display), 
                                     self->priv->back_buffer);
        self->priv->back_buffer = None;
    }

    if (self->priv->back)
    {
        cairo_surface_destroy(self->priv->back);
        self->priv->back = NULL;
    }

    if (self->priv->front)
    {
        cairo_surface_destroy(self->priv->front);
        self->priv->front = NULL;
    }

    G_OBJECT_CLASS (ccm_window_xrender_parent_class)->finalize (object);
}

static void
ccm_window_xrender_class_init (CCMWindowXRenderClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMWindowXRenderPrivate));

    CCM_DRAWABLE_CLASS (klass)->get_surface = ccm_window_xrender_get_surface;
    CCM_DRAWABLE_CLASS (klass)->flush = ccm_window_xrender_flush;
    CCM_DRAWABLE_CLASS (klass)->flush_region = ccm_window_xrender_flush_region;

    CCM_WINDOW_CLASS (klass)->create_pixmap = ccm_window_xrender_create_pixmap;

    object_class->finalize = ccm_window_xrender_finalize;
}

static gboolean
ccm_window_xrender_create_frontbuffer (CCMWindowXRender * self)
{
    g_return_val_if_fail (self != NULL, None);

    if (!self->priv->front)
    {
        CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
        Visual *visual = ccm_drawable_get_visual (CCM_DRAWABLE (self));
        cairo_rectangle_t geometry;

        if (visual && ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE (self),
                                                         &geometry))
            self->priv->front =  cairo_xlib_surface_create (CCM_DISPLAY_XDISPLAY (display),
                                                            CCM_WINDOW_XWINDOW(self),
                                                            visual,
                                                            geometry.width,
                                                            geometry.height);

    }

    return self->priv->front != NULL;
}

static gboolean
ccm_window_xrender_create_backbuffer (CCMWindowXRender * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (!self->priv->back)
    {
        cairo_rectangle_t geometry;

        if (ccm_window_xrender_create_frontbuffer (self) &&
            ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE (self),
                                               &geometry))
        {
            CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE (self));
            gboolean have_dbe;

            g_object_get(G_OBJECT(display), "use_xdbe", &have_dbe, NULL);
            if (have_dbe)
            {
                CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE (self));

                self->priv->back_buffer = XdbeAllocateBackBufferName(CCM_DISPLAY_XDISPLAY(display), 
                                                                     CCM_WINDOW_XWINDOW(self), 
                                                                     XdbeUndefined);
                Visual* visual = DefaultVisual(CCM_DISPLAY_XDISPLAY(display), 
                                               ccm_screen_get_number(screen));

                self->priv->back = cairo_xlib_surface_create(CCM_DISPLAY_XDISPLAY(display),
                                                             self->priv->back_buffer, 
                                                             visual,
                                                             (int)geometry.width, 
                                                             (int)geometry.height);
            }
            else
                self->priv->back = cairo_surface_create_similar(self->priv->front,
                                                                CAIRO_CONTENT_COLOR,
                                                                geometry.width,
                                                                geometry.height);
        }
    }

    return self->priv->back != NULL;
}

static cairo_surface_t *
ccm_window_xrender_get_surface (CCMDrawable * drawable)
{
    g_return_val_if_fail (drawable != NULL, NULL);

    CCMWindowXRender *self = CCM_WINDOW_X_RENDER (drawable);
    cairo_surface_t *surface = NULL;

    if (ccm_window_xrender_create_backbuffer (self))
    {
        surface = cairo_surface_reference(self->priv->back);
    }

    return surface;
}

static void
ccm_window_xrender_flush (CCMDrawable * drawable)
{
    g_return_if_fail (drawable != NULL);

    CCMWindowXRender *self = CCM_WINDOW_X_RENDER (drawable);

    if (ccm_window_xrender_create_backbuffer (self))
    {
        CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
        gboolean have_dbe;

        g_object_get(G_OBJECT(display), "use_xdbe", &have_dbe, NULL);
        if (have_dbe)
        {
            XdbeSwapInfo swap_info;

            swap_info.swap_window = CCM_WINDOW_XWINDOW(self);
            swap_info.swap_action = XdbeUndefined;
            XdbeSwapBuffers(CCM_DISPLAY_XDISPLAY(display), &swap_info, 1);
        }
        else
        {
            cairo_t* ctx = cairo_create(self->priv->front);

            cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface(ctx, self->priv->back, 0, 0);
            cairo_paint(ctx);
            cairo_destroy(ctx);
        }

        ccm_display_flush(display);
    }
}

static void
ccm_window_xrender_flush_region (CCMDrawable * drawable, CCMRegion * region)
{
    g_return_if_fail (drawable != NULL);
    g_return_if_fail (region != NULL);

    g_return_if_fail (drawable != NULL);

    CCMWindowXRender *self = CCM_WINDOW_X_RENDER (drawable);

    if (ccm_window_xrender_create_backbuffer (self))
    {
        CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
        gboolean have_dbe;

        g_object_get(G_OBJECT(display), "use_xdbe", &have_dbe, NULL);
        if (have_dbe)
        {
            XdbeSwapInfo swap_info;

            swap_info.swap_window = CCM_WINDOW_XWINDOW(self);
            swap_info.swap_action = XdbeUndefined;

            XdbeSwapBuffers(CCM_DISPLAY_XDISPLAY(display), &swap_info, 1);
        }
        else
        {
            cairo_t* ctx = cairo_create(self->priv->front);
            cairo_rectangle_t *rects = NULL;
            gint nb_rects, cpt;

            cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
            ccm_region_get_rectangles (region, &rects, &nb_rects);
            for (cpt = 0; cpt < nb_rects; ++cpt)
                cairo_rectangle(ctx, rects[cpt].x, rects[cpt].y,
                                rects[cpt].width, rects[cpt].height);
            cairo_clip(ctx);
            if (rects) cairo_rectangles_free (rects, nb_rects);

            cairo_set_source_surface(ctx, self->priv->back, 0, 0);
            cairo_paint(ctx);
            cairo_destroy(ctx);
        }
        ccm_display_flush(display);
    }
}

static CCMPixmap *
ccm_window_xrender_create_pixmap (CCMWindow * self, int width, int height,
                                  int depth)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    CCMDisplay *display = ccm_screen_get_display (screen);
    XVisualInfo vinfo;
    Pixmap xpixmap;

    if (!XMatchVisualInfo (CCM_DISPLAY_XDISPLAY (display),
                           CCM_SCREEN_NUMBER (screen), depth,
                           TrueColor, &vinfo))
        return NULL;

    xpixmap = XCreatePixmap (CCM_DISPLAY_XDISPLAY (display),
                             CCM_WINDOW_XWINDOW (self), width, height, depth);

    if (xpixmap == None)
        return NULL;

    return ccm_pixmap_new_from_visual (screen, vinfo.visual, xpixmap);
}
