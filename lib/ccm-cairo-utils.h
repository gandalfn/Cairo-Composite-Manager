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
 
#ifndef _CCM_CAIRO_UTILS_H_
#define _CCM_CAIRO_UTILS_H_

#include <cairo.h>

typedef enum
{
	CAIRO_CORNER_NONE        = 0,
	CAIRO_CORNER_TOPLEFT     = 1,
	CAIRO_CORNER_TOPRIGHT    = 2,
	CAIRO_CORNER_BOTTOMLEFT  = 4,
	CAIRO_CORNER_BOTTOMRIGHT = 8,
	CAIRO_CORNER_ALL         = 15
} CairoCorners;

void
cairo_rectangle_round (cairo_t *cr, double x, double y, double w, double h,
                       int radius, CairoCorners corners);
void
cairo_notebook_page_round (cairo_t *cr, double x, double y, double w, double h,
						   double tx, double tw, double th, int radius);

cairo_surface_t* cairo_image_surface_blur (cairo_surface_t *surface,
					   int              radius,
					   double           sigma,
					   int              x,
					   int              y,
					   int              width,
					   int              height);
cairo_surface_t* cairo_image_surface_blur2 (cairo_surface_t *surface,
					   double           radius,
					   int              x,
					   int              y,
					   int              width,
					   int              height);

cairo_surface_t* cairo_blur_path(cairo_surface_t* surface, cairo_path_t* path, 
                                 cairo_path_t* clip, int border, double step, 
                                 double width, double height);
				  
#endif /* _CCM_CAIRO_UTILS_H_ */
