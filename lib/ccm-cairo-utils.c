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
 *
 * Cairo Immage Surface Blur based on similar code from 
 * http://taschenorakel.de/mathias/2008/11/24/blur-effect-cairo/
 *
 * Authors:
 *     Copyright (C) 2008  Mathias Hasselmann <mathias.hasselmann@gmx.de>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include <pixman.h>

#include "ccm-cairo-utils.h"

void
cairo_rectangle_round (cairo_t * cr, double x, double y, double w, double h,
                       int radius, CairoCorners corners)
{
    //       x x+r                    x+w-r
    // y       +-------------------------+
    //        +                           +
    // y+r   +                             +
    //       |                             |
    //       |                             |
    // y+h-r +                             +
    //        +                           +
    // y+h     +-------------------------+
    if (corners & CAIRO_CORNER_TOPLEFT)
        cairo_move_to (cr, x + radius, y);
    else
        cairo_move_to (cr, x, y);

    if (corners & CAIRO_CORNER_TOPRIGHT)
        cairo_arc (cr, x + w - radius, y + radius, radius, M_PI * 1.5,
                   M_PI * 2);
    else
        cairo_line_to (cr, x + w, y);

    if (corners & CAIRO_CORNER_BOTTOMRIGHT)
        cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, M_PI * 0.5);
    else
        cairo_line_to (cr, x + w, y + h);

    if (corners & CAIRO_CORNER_BOTTOMLEFT)
        cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.5, M_PI);
    else
        cairo_line_to (cr, x, y + h);

    if (corners & CAIRO_CORNER_TOPLEFT)
        cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
    else
        cairo_line_to (cr, x, y);
}

void
cairo_notebook_page_round (cairo_t * cr, double x, double y, double w, double h,
                           double tx, double tw, double th, int radius)
{
    // x    x+tx+r  x+tx+tw-r
    //          +-----+   y
    //         +       +  y+r
    //        |         |
    //  x+r  +           + y+th-r          x+w-r
    //   +--+             +-----------------+      y+th
    //  +   tx            x+tx+tw+r          +
    // +                                      +    y+th+r
    // |                                      |
    cairo_move_to (cr, x + tx + radius, y);
    cairo_arc (cr, x + tx + tw - radius, y + radius, radius, M_PI * 1.5,
               M_PI * 2);
    cairo_arc_negative (cr, x + tx + tw + radius, y + th - radius, radius, M_PI,
                        M_PI * 0.5);
    cairo_arc (cr, x + w - radius, y + th + radius, radius, M_PI * 1.5,
               M_PI * 2);
    cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, M_PI * 0.5);
    cairo_arc (cr, x + radius, y + h - radius, radius, M_PI * 0.5, M_PI);
    if (tx >= radius)
    {
        cairo_arc (cr, x + radius, y + th + radius, radius, M_PI, M_PI * 1.5);
        cairo_arc_negative (cr, x + tx - radius, y + th - radius, radius,
                            M_PI * 0.5, 0);
        cairo_arc (cr, x + tx + radius, y + radius, radius, M_PI, M_PI * 1.5);
    }
    else
        cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
}

/* G(x,y) = 1/(2 * PI * sigma^2) * exp(-(x^2 + y^2)/(2 * sigma^2))
 */
static pixman_fixed_t *
create_gaussian_blur_kernel (int radius, double sigma, int *length)
{
    const double scale2 = 2.0 * sigma * sigma;
    const double scale1 = 1.0 / (M_PI * scale2);

    const int size = 2 * radius + 1;
    const int n_params = size * size;

    pixman_fixed_t *params;
    double *tmp, sum;
    int x, y, i;

    tmp = g_newa (double, n_params);

    /* caluclate gaussian kernel in floating point format */
    for (i = 0, sum = 0, x = -radius; x <= radius; ++x)
    {
        for (y = -radius; y <= radius; ++y, ++i)
        {
            const double u = x * x;
            const double v = y * y;

            tmp[i] = scale1 * exp (-(u + v) / scale2);

            sum += tmp[i];
        }
    }

    /* normalize gaussian kernel and convert to fixed point format */
    params = g_new (pixman_fixed_t, n_params + 2);

    params[0] = pixman_int_to_fixed (size);
    params[1] = pixman_int_to_fixed (size);

    for (i = 0; i < n_params; ++i)
        params[2 + i] = pixman_double_to_fixed (tmp[i] / sum);

    if (length)
        *length = n_params + 2;

    return params;
}

static pixman_format_code_t
pixman_format_from_cairo_format (cairo_format_t cairo_format)
{
    switch (cairo_format)
    {
        case CAIRO_FORMAT_ARGB32:
            return PIXMAN_a8r8g8b8;
        case CAIRO_FORMAT_RGB24:
            return PIXMAN_x8r8g8b8;
        case CAIRO_FORMAT_A8:
            return PIXMAN_a8;
        case CAIRO_FORMAT_A1:
            return PIXMAN_a1;
        default:
            break;
    }

    return PIXMAN_a8r8g8b8;
}

cairo_surface_t *
cairo_image_surface_blur (cairo_surface_t * surface, int radius, double sigma,
                          int x, int y, int width, int height)
{
    static cairo_user_data_key_t data_key;
    pixman_fixed_t *params = NULL;
    int n_params;

    pixman_image_t *src, *dst;
    int w, h, s;
    gpointer p;
    cairo_format_t cairo_format;
    pixman_format_code_t pixman_format;

    g_return_val_if_fail (cairo_surface_get_type (surface) ==
                          CAIRO_SURFACE_TYPE_IMAGE, NULL);

    w = width <= 0 ? cairo_image_surface_get_width (surface) : width;
    h = height <= 0 ? cairo_image_surface_get_height (surface) : height;
    s = cairo_image_surface_get_stride (surface);
    cairo_format = cairo_image_surface_get_format (surface);
    pixman_format = pixman_format_from_cairo_format (cairo_format);

    /* create pixman image for cairo image surface */
    p = cairo_image_surface_get_data (surface);
    p += y * s + x;
    src = pixman_image_create_bits (pixman_format, w, h, p, s);

    /* attach gaussian kernel to pixman image */
    params = create_gaussian_blur_kernel (radius, sigma, &n_params);
    pixman_image_set_filter (src, PIXMAN_FILTER_CONVOLUTION, params, n_params);
    g_free (params);

    /* render blured image to new pixman image */
    p = g_malloc0 (s * h);
    dst = pixman_image_create_bits (pixman_format, w, h, p, s);
    pixman_image_composite (PIXMAN_OP_SRC, src, NULL, dst, 0, 0, 0, 0, 0, 0, w,
                            h);
    pixman_image_unref (src);

    /* create new cairo image for blured pixman image */
    surface = cairo_image_surface_create_for_data (p, cairo_format, w, h, s);
    cairo_surface_set_user_data (surface, &data_key, p, g_free);
    pixman_image_unref (dst);

    return surface;
}

cairo_surface_t *
cairo_image_surface_blur2 (cairo_surface_t * surface, double radius, int x_orig,
                           int y_orig, int width, int height)
{
    // Steve Hanov, 2009
    // Released into the public domain.
    static cairo_user_data_key_t data_key;
    gint w, h, s, channel, iteration, nchan = 4;
    pixman_image_t *psrc, *pdst;
    guchar *src, *dst;
    guint *precalc;
    gdouble mul;
    // The number of times to perform the averaging. According to wikipedia,
    // three iterations is good enough to pass for a gaussian.
    const gint MAX_ITERATIONS = 3;
    cairo_format_t cairo_format;
    pixman_format_code_t pixman_format;

    g_return_val_if_fail (cairo_surface_get_type (surface) ==
                          CAIRO_SURFACE_TYPE_IMAGE, NULL);

    cairo_format = cairo_image_surface_get_format (surface);
    pixman_format = pixman_format_from_cairo_format (cairo_format);

    switch (cairo_format)
    {
        case CAIRO_FORMAT_ARGB32:
            nchan = 4;
            break;
        case CAIRO_FORMAT_RGB24:
            nchan = 3;
            break;
        case CAIRO_FORMAT_A8:
        case CAIRO_FORMAT_A1:
            nchan = 1;
            break;
    }
    w = width <= 0 ? cairo_image_surface_get_width (surface) : width;
    h = height <= 0 ? cairo_image_surface_get_height (surface) : height;
    s = cairo_image_surface_get_stride (surface);

    src = cairo_image_surface_get_data (surface);
    src += y_orig * s + x_orig;
    psrc = pixman_image_create_bits (pixman_format, w, h, (gpointer) src, s);
    dst = g_malloc0 (s * h);
    pdst = pixman_image_create_bits (pixman_format, w, h, (gpointer) dst, s);
    pixman_image_composite (PIXMAN_OP_SRC, psrc, NULL, pdst, 0, 0, 0, 0, 0, 0,
                            w, h);
    pixman_image_unref (psrc);
    pixman_image_unref (pdst);

    precalc = g_malloc0 (s * h);
    mul = 1.f / ((radius * 2) * (radius * 2));

    for (iteration = 0; iteration < MAX_ITERATIONS; iteration++)
    {
        for (channel = 0; channel < nchan; channel++)
        {
            int x, y;

            // precomputation step.
            guchar *pix = src;
            guint *pre = precalc;

            pix += channel;
            for (y = 0; y < h; y++)
            {
                for (x = 0; x < w; x++)
                {
                    int tot = pix[0];
                    if (x > 0)
                        tot += pre[-1];
                    if (y > 0)
                        tot += pre[-w];
                    if (x > 0 && y > 0)
                        tot -= pre[-w - 1];
                    *pre++ = tot;
                    pix += nchan;
                }
            }

            // blur step.
            pix = dst + (int) radius *w * nchan + (int) radius *nchan + channel;
            for (y = radius; y < h - radius; y++)
            {
                for (x = radius; x < w - radius; x++)
                {
                    int l = x < radius ? 0 : x - radius;
                    int t = y < radius ? 0 : y - radius;
                    int r = x + radius >= w ? w - 1 : x + radius;
                    int b = y + radius >= h ? h - 1 : y + radius;
                    int tot =
                        precalc[r + b * w] + precalc[l + t * w] - precalc[l +
                                                                          b *
                                                                          w] -
                        precalc[r + t * w];
                    *pix = (unsigned char) (tot * mul);
                    pix += nchan;
                }
                pix += (int) radius *2 * nchan;
            }
        }
    }

    /* create new cairo image for blured image */
    surface = cairo_image_surface_create_for_data (dst, cairo_format, w, h, s);
    cairo_surface_set_user_data (surface, &data_key, dst, g_free);

    g_free (precalc);

    return surface;
}

cairo_surface_t *
cairo_blur_path (cairo_surface_t * ref, cairo_path_t * path,
                 cairo_path_t * clip, int border, double step, double width,
                 double height)
{
    g_return_val_if_fail (ref != NULL, NULL);
    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (border > 0, NULL);
    g_return_val_if_fail (step > 0, NULL);
    g_return_val_if_fail (width > 0 && height > 0, NULL);

    double cpt;
    double x1, y1, x2, y2;
    cairo_surface_t *surface = cairo_surface_create_similar (ref,
                                                             CAIRO_CONTENT_ALPHA,
                                                             width, height);
    cairo_t *cr = cairo_create (surface);

    // Get clip
    if (clip)
    {
        cairo_append_path (cr, clip);
        cairo_clip (cr);
        cairo_new_path (cr);
    }

    // Get path extents
    cairo_append_path (cr, path);
    cairo_path_extents (cr, &x1, &y1, &x2, &y2);
    cairo_new_path (cr);

    // Draw shadow
    for (cpt = border; cpt > 0; cpt -= step)
    {
        cairo_matrix_t matrix;
        double alpha;
        double p = 1.f - (cpt / (double) border);
        double x_scale = (double) ((x2 - x1) + cpt) / (double) (x2 - x1);
        double y_scale = (double) ((y2 - y1) + cpt) / (double) (y2 - y1);

        if (p < 0.5)
            alpha = p * 0.12;
        else if (p >= 0.5 && p < 0.75)
            alpha = 0.12 + ((p - 0.5) * (0.24 - 0.12));
        else if (p >= 0.75 && p < 1)
            alpha = 0.24 + ((p - 0.75) * (1 - 0.24));
        else
            alpha = 0.72;

        cairo_save (cr);
        cairo_matrix_init (&matrix, x_scale, 0, 0, y_scale,
                           (-cpt / 2.f) + (x1 * (1 - x_scale)),
                           (-cpt / 2.f) + (y1 * (1 - y_scale)));
        cairo_set_matrix (cr, &matrix);
        cairo_set_source_rgba (cr, 0, 0, 0, alpha);
        cairo_append_path (cr, path);
        cairo_fill (cr);
        cairo_restore (cr);
    }
    cairo_destroy (cr);

    return surface;
}
