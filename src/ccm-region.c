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
 
#include <string.h>
#include <pixman.h>

#include "ccm-debug.h"
#include "ccm-region.h"

#define CAIRO_RECTANGLE_TO_PIXMAN_BOX(r, b)  \
	{(b).x1 = pixman_double_to_fixed((r).x); \
	 (b).y1 = pixman_double_to_fixed((r).y); \
	 (b).x2 = pixman_double_to_fixed((r).x + (r).width); \
	 (b).y2 = pixman_double_to_fixed((r).y + (r).height);}
	
#define PIXMAN_BOX_TO_CAIRO_RECTANGLE(b, r)  \
	{(r).x = pixman_fixed_to_double(pixman_fixed_ceil((b).x1)); \
	 (r).y = pixman_fixed_to_double(pixman_fixed_ceil((b).y1)); \
	 (r).width = pixman_fixed_to_double(pixman_fixed_ceil((b).x2 - (b).x1)); \
	 (r).height = pixman_fixed_to_double(pixman_fixed_ceil((b).y2 - (b).y1));}

#define X_RECTANGLE_TO_PIXMAN_BOX(r, b)  \
	{(b).x1 = pixman_int_to_fixed((r).x); \
	 (b).y1 = pixman_int_to_fixed((r).y); \
	 (b).x2 = pixman_int_to_fixed((r).x + (r).width); \
	 (b).y2 = pixman_int_to_fixed((r).y + (r).height);}

#define PIXMAN_BOX_TO_X_RECTANGLE(b, r)  \
	{(r).x = pixman_fixed_to_int(pixman_fixed_ceil((b).x1)); \
	 (r).y = pixman_fixed_to_int(pixman_fixed_ceil((b).y1)); \
	 (r).width = pixman_fixed_to_int(pixman_fixed_ceil((b).x2 - (b).x1)); \
	 (r).height = pixman_fixed_to_int(pixman_fixed_ceil((b).y2 - (b).y1));}

#define PIXMAN_BOX_TO_REGION_BOX(b, r)  \
	{(r).x1 = (short)pixman_fixed_to_int(pixman_fixed_ceil((b).x1)); \
	 (r).y1 = (short)pixman_fixed_to_int(pixman_fixed_ceil((b).y1)); \
	 (r).x2 = (short)pixman_fixed_to_int(pixman_fixed_ceil((b).x2)); \
	 (r).y2 = (short)pixman_fixed_to_int(pixman_fixed_ceil((b).y2));}

struct _CCMRegion
{
	pixman_region32_t   reg;
};

void
_ccm_region_print(CCMRegion* self)
{
	cairo_rectangle_t clipbox, *rects = NULL;
	int cpt, nb_rects;
	
	ccm_region_get_clipbox(self, &clipbox);
	ccm_log("REGION: %f,%f %f,%f", clipbox.x, clipbox.y,
			clipbox.width, clipbox.height);
	ccm_region_get_rectangles(self, &rects, &nb_rects);
	for (cpt = 0; cpt < nb_rects; ++cpt)
	{
		ccm_log("      : %f,%f %f,%f", rects[cpt].x, rects[cpt].y,
				rects[cpt].width, rects[cpt].height);
	}
	if (rects) g_free(rects);
}

CCMRegion*
ccm_region_new(void)
{
	CCMRegion* self = g_slice_new (CCMRegion);
	
	pixman_region32_init(&self->reg);
	
	return self;
}

void
ccm_region_destroy(CCMRegion* self)
{
	g_return_if_fail(self != NULL);
	
	pixman_region32_fini(&self->reg);
	
	g_slice_free(CCMRegion, self);
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

CCMRegion*
ccm_region_create(int x, int y, int width, int height)
{
	XRectangle rect;
	
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	
	return ccm_region_xrectangle(&rect);
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
		for (cpt = 0; cpt < *n_rectangles; ++cpt)
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
		for (cpt = 0; cpt < *n_rectangles; ++cpt)
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
		for (cpt = 0; cpt < *n_boxes; ++cpt)
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

	cairo_rectangle_t clipbox;

	if (!pixman_region32_not_empty(&self->reg))
		return TRUE;
	
	ccm_region_get_clipbox (self, &clipbox);
	
	return clipbox.width  <= 0 || clipbox.height <= 0;
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
	
	if (matrix->xx == 1.0f && matrix->yy == 1.0f &&
		matrix->x0 == 0.0f && matrix->y0 == 0.0f &&
		matrix->xy == 0.0f && matrix->yx == 0.0f)
		return;
	
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
		for (cpt = 0; cpt < n_boxes; ++cpt)
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

gboolean
ccm_region_transform_invert(CCMRegion* self, cairo_matrix_t* matrix)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(matrix != NULL, FALSE);

	cairo_matrix_t invert;
	memcpy(&invert, matrix, sizeof(cairo_matrix_t));
	
	if (cairo_matrix_invert(&invert) == CAIRO_STATUS_SUCCESS)
	{
		ccm_region_transform(self, &invert);
		return TRUE;
	}
	
	return FALSE;
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
		for (cpt = 0; cpt < n_boxes; ++cpt)
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
	
	if (matrix->xx == 1.0f && matrix->yy == 1.0f &&
		matrix->x0 == 0.0f && matrix->y0 == 0.0f &&
		matrix->xy == 0.0f && matrix->yx == 0.0f)
		return;
	
	ccm_region_get_clipbox(self, &clipbox);
	ccm_region_offset(self, -clipbox.x, -clipbox.y);
	ccm_region_transform(self, matrix);
	ccm_region_offset(self, clipbox.x, clipbox.y);
}

gboolean
ccm_region_device_transform_invert(CCMRegion* self, cairo_matrix_t* matrix)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(matrix != NULL, FALSE);
	
	cairo_rectangle_t clipbox;
	gboolean ret;
	
	ccm_region_get_clipbox(self, &clipbox);
	ccm_region_offset(self, -clipbox.x, -clipbox.y);
	ret = ccm_region_transform_invert(self, matrix);
	ccm_region_offset(self, clipbox.x, clipbox.y);
	
	return ret;
}

/* 
   Utility procedure Compress:
   Replace r by the region r', where 
     p in r' iff (Quantifer m <= dx) (p + m in r), and
     Quantifier is Exists if grow is TRUE, For all if grow is FALSE, and
     (x,y) + m = (x+m,y) if xdir is TRUE; (x,y+m) if xdir is FALSE.

   Thus, if xdir is TRUE and grow is FALSE, r is replaced by the region
   of all points p such that p and the next dx points on the same
   horizontal scan line are all in r.  We do this using by noting
   that p is the head of a run of length 2^i + k iff p is the head
   of a run of length 2^i and p+2^i is the head of a run of length
   k. Thus, the loop invariant: s contains the region corresponding
   to the runs of length shift.  r contains the region corresponding
   to the runs of length 1 + dxo & (shift-1), where dxo is the original
   value of dx.  dx = dxo & ~(shift-1).  As parameters, s and t are
   scratch regions, so that we don't have to allocate them on every
   call.
*/

#define ZOpRegion(a,b) if (grow) ccm_region_union (a, b); \
			 else ccm_region_intersect (a,b)
#define ZShiftRegion(a,b) if (xdir) ccm_region_offset (a,b,0); \
			  else ccm_region_offset (a,0,b)

static void
Compress(CCMRegion *r, CCMRegion *s, CCMRegion *t, guint dx,
         int xdir, int grow)
{
	guint shift = 1;

	pixman_region32_copy(&s->reg, &r->reg);
	while (dx)
	{
		if (dx & shift)
		{
			ZShiftRegion(r, -(int)shift);
			ZOpRegion(r, s);
			dx -= shift;
			if (!dx) break;
        }
		pixman_region32_copy(&t->reg, &s->reg);
		ZShiftRegion(s, -(int)shift);
		ZOpRegion(s, t);
		shift <<= 1;
    }
}

#undef ZOpRegion
#undef ZShiftRegion

void
ccm_region_resize(CCMRegion* self, int width, int height)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(width != 0 && height != 0);
	
	CCMRegion *s, *t;
	int grow;
	int dx, dy;

	pixman_box32_t* extents = pixman_region32_extents(&self->reg);

	dx = pixman_fixed_to_int(extents->x2 - extents->x1) - width;
	dy = pixman_fixed_to_int(extents->y2 - extents->y1) - height;
	
	if (!dx && !dy)
		return;

	s = ccm_region_new ();
	t = ccm_region_new ();

	grow = (dx < 0);
	if (grow)
		dx = -dx;
	if (dx)
		Compress(self, s, t, (unsigned) dx, TRUE, grow);
	if (grow)
		dx = -dx;
	
	grow = (dy < 0);
	if (grow)
		dy = -dy;
	if (dy)
		Compress(self, s, t, (unsigned) dy, FALSE, grow);
	if (grow)
		dy = -dy;
	
	ccm_region_offset (self, dx <= 0 ? - dx : 0, dy <= 0 ? -dy : 0);
	ccm_region_destroy (s);
	ccm_region_destroy (t);
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

gboolean
ccm_region_point_in(CCMRegion* self, int x, int y)
{
	g_return_val_if_fail(self != NULL, FALSE);
	pixman_box32_t box;
	
	return pixman_region32_contains_point(&self->reg, 
										  pixman_int_to_fixed(x), 
										  pixman_int_to_fixed(y), 
										  &box);
}

gboolean 
ccm_region_is_shaped(CCMRegion* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return pixman_region32_n_rects(&self->reg) > 1;
}
