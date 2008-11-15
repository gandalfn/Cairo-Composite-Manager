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
 * Blur based on similar code from gl-cairo-simple
 * Authors:
 *      Mirco "MacSlow" Mueller <macslow@bangang.de>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>

#include "ccm-cairo-utils.h"

void
cairo_rectangle_round (cairo_t *cr, double x, double y, double w, double h,
                       int radius, CairoCorners corners)
{
	if (corners & CAIRO_CORNER_TOPLEFT)
		cairo_move_to (cr, x+radius, y);
	else
		cairo_move_to (cr, x, y);
	
	if (corners & CAIRO_CORNER_TOPRIGHT)
		cairo_arc (cr, x+w-radius, y+radius, radius, M_PI * 1.5, M_PI * 2);
	else
		cairo_line_to (cr, x+w, y);
	
	if (corners & CAIRO_CORNER_BOTTOMRIGHT)
		cairo_arc (cr, x+w-radius, y+h-radius, radius, 0, M_PI * 0.5);
	else
		cairo_line_to (cr, x+w, y+h);
	
	if (corners & CAIRO_CORNER_BOTTOMLEFT)
		cairo_arc (cr, x+radius,   y+h-radius, radius, M_PI * 0.5, M_PI);
	else
		cairo_line_to (cr, x, y+h);
	
	if (corners & CAIRO_CORNER_TOPLEFT)
		cairo_arc (cr, x+radius,   y+radius,   radius, M_PI, M_PI * 1.5);
	else
		cairo_line_to (cr, x, y);
}

double*
kernel_1d_new (int    radius,
			   double deviation /* deviation, pass 0.0 for auto-generation */)
{
	double* kernel = NULL;
	double  sum    = 0.0f;
	double  value  = 0.0f;
	int     i;
	int     size = 2 * radius + 1;
	double  radiusf;

	if (radius <= 0)
		return NULL;

	kernel = (double*) g_slice_alloc ((size + 1) * sizeof (double));
	if (!kernel)
		return NULL;

	radiusf = fabs (radius) + 1.0f;
	if (deviation == 0.0f)
		deviation = sqrt (-(radiusf * radiusf) / (2.0f * log (1.0f / 255.0f)));

	kernel[0] = size;
	value = (double) -radius;
	for (i = 0; i < size; i++)
	{
		kernel[1 + i] = 1.0f / (2.506628275f * deviation) *
		expf (-((value * value) /
		(2.0f * deviation * deviation)));
		sum += kernel[1 + i];
		value += 1.0f;
	}

	for (i = 0; i < size; i++)
		kernel[1 + i] /= sum;

	return kernel;
}

void
kernel_1d_delete (int    radius,
				  double* kernel)
{
	if (!kernel)
		return;

	int     size = 2 * radius + 1;
	
	g_slice_free1 ((size + 1) * sizeof (double), (void*) kernel);
}

void
cairo_image_surface_blur (cairo_surface_t* surface,
						  int              horzRadius,
						  int              vertRadius)
{
	int            iX;
	int            iY;
	int            i;
	int            x;
	int            y;
	int            stride;
	int            offset;
	int            baseOffset;
	double*        horzBlur;
	double*        vertBlur;
	double*        horzKernel;
	double*        vertKernel;
	unsigned char* src;
	int            width;
	int            height;
	int            channels;

	/* sanity checks */
	if (!surface || horzRadius == 0 || vertRadius == 0)
		return;

	if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
		return;

	/* flush any pending cairo-drawing operations */
	cairo_surface_flush (surface);

	src  = cairo_image_surface_get_data (surface);
	width  = cairo_image_surface_get_width (surface);
	height = cairo_image_surface_get_height (surface);

	/* only handle RGB- or RGBA-surfaces */
	if (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32)
		channels = 4;
	else if (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_RGB24)
		channels = 3;
	else
		return;

	stride = width * channels;

	/* create buffers to hold the blur-passes */
	horzBlur = (double*) g_slice_alloc ((height * stride + vertRadius * stride) * sizeof (double));
	vertBlur = (double*) g_slice_alloc ((height * stride + vertRadius * stride) * sizeof (double));
	if (!horzBlur || !vertBlur)
	{
		if (horzBlur)
			g_slice_free1 ((height * stride + vertRadius * stride) * sizeof (double), (void*) horzBlur);

		if (vertBlur)
			g_slice_free1 ((height * stride + vertRadius * stride) * sizeof (double), (void*) vertBlur);

		return;
	}

	/* create blur-kernels for horz. and vert. */
	horzKernel = kernel_1d_new (horzRadius, 0.0f);
	vertKernel = kernel_1d_new (vertRadius, 0.0f);

	/* check creation success */
	if (!horzKernel || !vertKernel)
	{
		free ((void*) horzBlur);
		free ((void*) vertBlur);

		if (horzKernel)
			kernel_1d_delete (horzRadius, horzKernel);

		if (vertKernel)
			kernel_1d_delete (vertRadius, vertKernel);

		return;
	}

	/* horizontal pass */
	for (iY = 0; iY < height; iY++)
	{
		for (iX = 0; iX < width; iX++)
		{
			double red   = 0.0f;
			double green = 0.0f;
			double blue  = 0.0f;
			double alpha = 0.0f;

			offset = ((int) horzKernel[0]) / -2;
			for (i = 0; i < (int) horzKernel[0]; i++)
			{
				x = iX + offset;
				if (x >= 0 && x < width)
				{
					baseOffset = iY * stride + x * channels;

					if (channels == 4)
						alpha += (horzKernel[1+i] * (double) src[baseOffset + 3]);

					red   += (horzKernel[1+i] * (double) src[baseOffset + 2]);
					green += (horzKernel[1+i] * (double) src[baseOffset + 1]);
					blue  += (horzKernel[1+i] * (double) src[baseOffset + 0]);
				}

				offset++;
			}

			baseOffset = iY * stride + iX * channels;

			if (channels == 4)
			horzBlur[baseOffset + 3] = alpha;

			horzBlur[baseOffset + 2] = red;
			horzBlur[baseOffset + 1] = green;
			horzBlur[baseOffset + 0] = blue;
		}
	}

	/* vertical pass */
	for (iY = 0; iY < height; iY++)
	{
		for (iX = 0; iX < width; iX++)
		{
			double red   = 0.0f;
			double green = 0.0f;
			double blue  = 0.0f;
			double alpha = 0.0f;

			offset = ((int) vertKernel[0]) / -2;
			for (i = 0; i < (int) vertKernel[0]; i++)
			{
				y = iY + offset;
				if (y >= 0 && y < height)
				{
					baseOffset = y * stride + iX * channels;

					if (channels == 4)
						alpha += (vertKernel[1+i] * horzBlur[baseOffset + 3]);

					red   += (vertKernel[1+i] * horzBlur[baseOffset + 2]);
					green += (vertKernel[1+i] * horzBlur[baseOffset + 1]);
					blue  += (vertKernel[1+i] * horzBlur[baseOffset + 0]);
				}

				offset++;
			}

			baseOffset = iY * stride + iX * channels;

			if (channels == 4)
				vertBlur[baseOffset + 3] = alpha;

			vertBlur[baseOffset + 2] = red;
			vertBlur[baseOffset + 1] = green;
			vertBlur[baseOffset + 0] = blue;
		}
	}

	kernel_1d_delete (horzRadius, horzKernel);
	kernel_1d_delete (vertRadius, vertKernel);

	for (iY = 0; iY < height; iY++)
	{
		for (iX = 0; iX < width; iX++)
		{
			offset = iY * stride + iX * channels;

			if (channels == 4)
				src[offset + 3] = (unsigned char) vertBlur[offset + 3];

			src[offset + 2] = (unsigned char) vertBlur[offset + 2];
			src[offset + 1] = (unsigned char) vertBlur[offset + 1];
			src[offset + 0] = (unsigned char) vertBlur[offset + 0];
		}
	}
	g_slice_free1 ((height * stride + vertRadius * stride) * sizeof (double), (void*) vertBlur);
	g_slice_free1 ((height * stride + vertRadius * stride) * sizeof (double), (void*) horzBlur);

	/* tell cairo we did some drawing to the surface */
	cairo_surface_mark_dirty (surface);
}
