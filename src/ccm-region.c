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
 
#include <pixman.h>

#include "ccm-debug.h"
#include "ccm-region.h"

#define CAIRO_RECTANGLE_TO_PIXMAN_BOX(r, b)  \
	{(b).x1 = pixman_double_to_fixed((r).x); \
	 (b).y1 = pixman_double_to_fixed((r).y); \
	 (b).x2 = pixman_double_to_fixed((r).x + (r).width); \
	 (b).y2 = pixman_double_to_fixed((r).y + (r).height);}
	
#define PIXMAN_BOX_TO_CAIRO_RECTANGLE(b, r)  \
	{(r).x = pixman_fixed_to_double((b).x1); \
	 (r).y = pixman_fixed_to_double((b).y1); \
	 (r).width = pixman_fixed_to_double((b).x2 - (b).x1); \
	 (r).height = pixman_fixed_to_double((b).y2 - (b).y1);}

#define X_RECTANGLE_TO_PIXMAN_BOX(r, b)  \
	{(b).x1 = pixman_int_to_fixed((r).x); \
	 (b).y1 = pixman_int_to_fixed((r).y); \
	 (b).x2 = pixman_int_to_fixed((r).x + (r).width); \
	 (b).y2 = pixman_int_to_fixed((r).y + (r).height);}

#define PIXMAN_BOX_TO_X_RECTANGLE(b, r)  \
	{(r).x = pixman_fixed_to_int((b).x1); \
	 (r).y = pixman_fixed_to_int((b).y1); \
	 (r).width = pixman_fixed_to_int((b).x2 - (b).x1); \
	 (r).height = pixman_fixed_to_int((b).y2 - (b).y1);}

#define PIXMAN_BOX_TO_REGION_BOX(b, r)  \
	{(r).x1 = (short)pixman_fixed_to_int((b).x1); \
	 (r).y1 = (short)pixman_fixed_to_int((b).y1); \
	 (r).x2 = (short)pixman_fixed_to_int((b).x2); \
	 (r).y2 = (short)pixman_fixed_to_int((b).y2);}

struct _CCMRegion
{
	pixman_region32_t   reg;
};

void
_ccm_region_print(CCMRegion* self)
{
	cairo_rectangle_t clipbox, *rects;
	int cpt, nb_rects;
	
	ccm_region_get_clipbox(self, &clipbox);
	ccm_log("REGION: %f,%f %f,%f", clipbox.x, clipbox.y,
			clipbox.width, clipbox.height);
	ccm_region_get_rectangles(self, &rects, &nb_rects);
	for (cpt = 0; cpt < nb_rects; cpt++)
	{
		ccm_log("      : %f,%f %f,%f", rects[cpt].x, rects[cpt].y,
				rects[cpt].width, rects[cpt].height);
	}
	g_free(rects);
}

CCMRegion*
ccm_region_new(void)
{
	CCMRegion* self = g_new0(CCMRegion, 1);
	
	pixman_region32_init(&self->reg);
	
	return self;
}

void
ccm_region_destroy(CCMRegion* self)
{
	g_return_if_fail(self != NULL);
	
	pixman_region32_fini(&self->reg);
	
	g_free(self);
}

CCMRegion*
ccm_region_copy(CCMRegion* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMRegion* copy = ccm_region_new();
	
	if (!pixman_region32_copy(&copy->reg, &self->reg))
	{
		ccm_region_destroy(copy);
		copy = NULL;
	}
	
	return copy;
}

CCMRegion*
ccm_region_rectangle(cairo_rectangle_t* rect)
{
	g_return_val_if_fail(rect != NULL, NULL);
	
	CCMRegion* self = ccm_region_new();
	pixman_box32_t box;
	
	CAIRO_RECTANGLE_TO_PIXMAN_BOX(*rect, box);
	
	if (!pixman_region32_init_rects(&self->reg, &box,1))
	{
		ccm_region_destroy(self);
		self = NULL;
	}
	
	return self;
}

CCMRegion*
ccm_region_xrectangle(XRectangle* rect)
{
	g_return_val_if_fail(rect != NULL, NULL);
	
	CCMRegion* self = ccm_region_new();
	pixman_box32_t box;
	
	X_RECTANGLE_TO_PIXMAN_BOX(*rect, box);
	
	if (!pixman_region32_init_rects(&self->reg, &box,1))
	{
		ccm_region_destroy(self);
		self = NULL;
	}
	
	return self;
}

void
ccm_region_get_clipbox(CCMRegion* self, cairo_rectangle_t* clipbox)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(clipbox != NULL);
	
	pixman_box32_t* extents = pixman_region32_extents(&self->reg);
	
	PIXMAN_BOX_TO_CAIRO_RECTANGLE(*extents, *clipbox);
}

void
ccm_region_get_rectangles(CCMRegion* self, cairo_rectangle_t** rectangles,
						  gint* n_rectangles)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(rectangles != NULL);
	g_return_if_fail(n_rectangles != NULL);
	
	int cpt;
	
	pixman_box32_t* boxes = pixman_region32_rectangles(&self->reg,
													   n_rectangles);
	
	if (*n_rectangles > 0)
	{
		*rectangles = g_new(cairo_rectangle_t, *n_rectangles);
		for (cpt = 0; cpt < *n_rectangles; cpt++)
		{
			PIXMAN_BOX_TO_CAIRO_RECTANGLE(boxes[cpt], (*rectangles)[cpt]);
		}
	}		
}

void
ccm_region_get_xrectangles(CCMRegion* self, XRectangle** rectangles,
						   gint* n_rectangles)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(rectangles != NULL);
	g_return_if_fail(n_rectangles != NULL);
	
	int cpt;
	
	pixman_box32_t* boxes = pixman_region32_rectangles(&self->reg,
													   n_rectangles);
	
	if (*n_rectangles > 0)
	{
		*rectangles = g_new(XRectangle, *n_rectangles);
		for (cpt = 0; cpt < *n_rectangles; cpt++)
		{
			PIXMAN_BOX_TO_X_RECTANGLE(boxes[cpt], (*rectangles)[cpt]);
		}
	}		
}

CCMRegionBox*
ccm_region_get_boxes(CCMRegion* self, gint* n_boxes)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(n_boxes != NULL, NULL);	
	int cpt;
	CCMRegionBox* rboxes = NULL;
	
	pixman_box32_t* boxes = pixman_region32_rectangles(&self->reg,
													   n_boxes);
	
	if (*n_boxes > 0)
	{
		rboxes = g_new(CCMRegionBox, *n_boxes);
		for (cpt = 0; cpt < *n_boxes; cpt++)
		{
			PIXMAN_BOX_TO_REGION_BOX(boxes[cpt], rboxes[cpt]);
		}
	}		
	
	return rboxes;
}

gboolean
ccm_region_empty(CCMRegion* self)
{
	g_return_val_if_fail(self != NULL, TRUE);
	
	return !pixman_region32_not_empty(&self->reg);
}

void
ccm_region_subtract(CCMRegion* self, CCMRegion* other)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(other != NULL);	
	
	pixman_region32_subtract(&self->reg, &self->reg, &other->reg);
}

void
ccm_region_union(CCMRegion* self, CCMRegion* other)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(other != NULL);	
	
	pixman_region32_union(&self->reg, &self->reg, &other->reg);
}

void
ccm_region_union_with_rect(CCMRegion* self, cairo_rectangle_t* rect)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(rect != NULL);	
	
	CCMRegion* other = ccm_region_rectangle(rect);
	ccm_region_union(self, other);
	ccm_region_destroy(other);
}

void
ccm_region_union_with_xrect(CCMRegion* self, XRectangle* rect)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(rect != NULL);	
	
	CCMRegion* other = ccm_region_xrectangle(rect);
	ccm_region_union(self, other);
	ccm_region_destroy(other);
}

void
ccm_region_intersect(CCMRegion* self, CCMRegion* other)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(other != NULL);	
	
	pixman_region32_intersect(&self->reg, &self->reg, &other->reg);
}

void
ccm_region_transform(CCMRegion* self, cairo_matrix_t* matrix)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(matrix != NULL);
	
	int n_boxes, cpt;
	pixman_box32_t* extents = pixman_region32_extents(&self->reg);
	pixman_box32_t* boxes = pixman_region32_rectangles(&self->reg, &n_boxes);
	double x, y;
	
	x = pixman_fixed_to_double(extents->x1);
	y = pixman_fixed_to_double(extents->y1);
	cairo_matrix_transform_point(matrix, &x, &y);
	extents->x1 = pixman_double_to_fixed(x);
	extents->y1 = pixman_double_to_fixed(y);
	
	x = pixman_fixed_to_double(extents->x2);
	y = pixman_fixed_to_double(extents->y2);
	cairo_matrix_transform_point(matrix, &x, &y);
	extents->x2 = pixman_double_to_fixed(x);
	extents->y2 = pixman_double_to_fixed(y);

	if (extents != boxes)
	{
		for (cpt = 0; cpt < n_boxes; cpt++)
		{
			x = pixman_fixed_to_double(boxes[cpt].x1);
			y = pixman_fixed_to_double(boxes[cpt].y1);
			cairo_matrix_transform_point(matrix, &x, &y);
			boxes[cpt].x1 = pixman_double_to_fixed(x);
			boxes[cpt].y1 = pixman_double_to_fixed(y);
	
			x = pixman_fixed_to_double(boxes[cpt].x2);
			y = pixman_fixed_to_double(boxes[cpt].y2);
			cairo_matrix_transform_point(matrix, &x, &y);
			boxes[cpt].x2 = pixman_double_to_fixed(x);
			boxes[cpt].y2 = pixman_double_to_fixed(y);
		}
	}
}

void
ccm_region_offset(CCMRegion* self, int dx, int dy)
{
	g_return_if_fail(self != NULL);
	
	int n_boxes, cpt;
	pixman_box32_t* extents = pixman_region32_extents(&self->reg);
	pixman_box32_t* boxes = pixman_region32_rectangles(&self->reg, &n_boxes);
	
	extents->x1 += pixman_int_to_fixed(dx);
	extents->x2 += pixman_int_to_fixed(dx);
	extents->y1 += pixman_int_to_fixed(dy);
	extents->y2 += pixman_int_to_fixed(dy);

	if (extents != boxes)
	{
		for (cpt = 0; cpt < n_boxes; cpt++)
		{
			boxes[cpt].x1 += pixman_int_to_fixed(dx);
			boxes[cpt].x2 += pixman_int_to_fixed(dx);
			boxes[cpt].y1 += pixman_int_to_fixed(dy);
			boxes[cpt].y2 += pixman_int_to_fixed(dy);
		}
	}
}

void
ccm_region_device_transform(CCMRegion* self, cairo_matrix_t* matrix)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(matrix != NULL);
	
	cairo_rectangle_t clipbox;
	
	ccm_region_get_clipbox(self, &clipbox);
	ccm_region_offset(self, -clipbox.x, -clipbox.y);
	ccm_region_transform(self, matrix);
	ccm_region_offset(self, clipbox.x, clipbox.y);
}

void
ccm_region_resize(CCMRegion* self, int width, int height)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(width != 0 && height != 0);
	
	cairo_rectangle_t clipbox;
	cairo_matrix_t matrix;
	
	ccm_region_get_clipbox(self, &clipbox);
	cairo_matrix_init_scale(&matrix, (double)width / clipbox.width, 
							(double)height / clipbox.height);
	ccm_region_device_transform(self, &matrix);
}

void
ccm_region_scale(CCMRegion* self, double scale_width, double scale_height)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(scale_width != 0 && scale_height != 0);
	
	cairo_matrix_t matrix;
	
	cairo_matrix_init_scale(&matrix, scale_width, scale_height);
	ccm_region_device_transform(self, &matrix);
}



