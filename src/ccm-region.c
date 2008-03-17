/* $TOG: Region.c /main/31 1998/02/06 17:50:22 kaleb $ */
/************************************************************************

Copyright 1987, 1988, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

************************************************************************/
/* $XFree86: xc/lib/X11/Region.c,v 1.5 1999/05/09 10:50:01 dawes Exp $ */
/*
 * The functions in this file implement the Region abstraction, similar to one
 * used in the X11 sample server. A Region is simply an area, as the name
 * implies, and is implemented as a "y-x-banded" array of rectangles. To
 * explain: Each Region is made up of a certain number of rectangles sorted
 * by y coordinate first, and then by x coordinate.
 *
 * Furthermore, the rectangles are banded such that every rectangle with a
 * given upper-left y coordinate (y1) will have the same lower-right y
 * coordinate (y2) and vice versa. If a rectangle has scanlines in a band, it
 * will span the entire vertical distance of the band. This means that some
 * areas that could be merged into a taller rectangle will be represented as
 * several shorter rectangles to account for shorter rectangles to its left
 * or right but within its "vertical scope".
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible. E.g. no two rectangles in a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course). This maintains
 * the y-x-banding that's so nice to have...
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ccm-region.h"

typedef struct RegionBox RegionBox;
typedef struct Point Point;

struct RegionBox
{
    int x1;
    int y1;
    int x2;
    int y2;
};

struct Point
{
    int x;
    int y;
};

/* 
 *   clip region
 */

struct _CCMRegion
{
    long size;
    long numRects;
    RegionBox *rects;
    RegionBox extents;
};

/*  1 if two BOXs overlap.
 *  0 if two BOXs do not overlap.
 *  Remember, x2 and y2 are not in the region 
 */
#define EXTENTCHECK(r1, r2)	\
    ((r1)->x2 > (r2)->x1 &&	\
     (r1)->x1 < (r2)->x2 &&	\
     (r1)->y2 > (r2)->y1 &&	\
     (r1)->y1 < (r2)->y2)

/*
 *  update region extents
 */
#define EXTENTS(r,idRect){						\
	if((r)->x1 < (idRect)->extents.x1)				\
	    (idRect)->extents.x1 = (r)->x1;				\
	if((r)->y1 < (idRect)->extents.y1)				\
	    (idRect)->extents.y1 = (r)->y1;				\
	if((r)->x2 > (idRect)->extents.x2)				\
	    (idRect)->extents.x2 = (r)->x2;				\
	if((r)->y2 > (idRect)->extents.y2)				\
	    (idRect)->extents.y2 = (r)->y2;				\
    }

/*
 *   Check to see if there is enough memory in the present region.
 */
#define MEMCHECK(reg, rect, firstrect){					\
        if ((reg)->numRects >= ((reg)->size - 1)) {			\
	    (firstrect) = g_renew (RegionBox, (firstrect), 2 * (reg)->size); \
	    (reg)->size *= 2;						\
	    (rect) = &(firstrect)[(reg)->numRects];			\
	}								\
    }

/*  this routine checks to see if the previous rectangle is the same
 *  or subsumes the new rectangle to add.
 */

#define CHECK_PREVIOUS(Reg, R, Rx1, Ry1, Rx2, Ry2)			\
    (!(((Reg)->numRects > 0)&&						\
       ((R-1)->y1 == (Ry1)) &&						\
       ((R-1)->y2 == (Ry2)) &&						\
       ((R-1)->x1 <= (Rx1)) &&						\
       ((R-1)->x2 >= (Rx2))))

/*  add a rectangle to the given Region */
#define ADDRECT(reg, r, rx1, ry1, rx2, ry2){				\
	if (((rx1) < (rx2)) && ((ry1) < (ry2)) &&			\
	    CHECK_PREVIOUS((reg), (r), (rx1), (ry1), (rx2), (ry2))){	\
	    (r)->x1 = (rx1);						\
	    (r)->y1 = (ry1);						\
	    (r)->x2 = (rx2);						\
	    (r)->y2 = (ry2);						\
	    EXTENTS((r), (reg));					\
	    (reg)->numRects++;						\
	    (r)++;							\
	}								\
    }



/*  add a rectangle to the given Region */
#define ADDRECTNOX(reg, r, rx1, ry1, rx2, ry2){				\
	if ((rx1 < rx2) && (ry1 < ry2) &&				\
	    CHECK_PREVIOUS((reg), (r), (rx1), (ry1), (rx2), (ry2))){	\
	    (r)->x1 = (rx1);						\
	    (r)->y1 = (ry1);						\
	    (r)->x2 = (rx2);						\
	    (r)->y2 = (ry2);						\
	    (reg)->numRects++;						\
	    (r)++;							\
	}								\
    }

#define EMPTY_REGION(pReg) pReg->numRects = 0

#define REGION_NOT_EMPTY(pReg) pReg->numRects

#define INBOX(r, x, y)	    \
    ( ( ((r).x2 >  x)) &&   \
      ( ((r).x1 <= x)) &&   \
      ( ((r).y2 >  y)) &&   \
      ( ((r).y1 <= y)) )

/*
 * number of points to buffer before sending them off
 * to scanlines() :  Must be an even number
 */
#define NUMPTSTOBUFFER 200

/*
 * used to allocate buffers for points and link
 * the buffers together
 */
typedef struct _POINTBLOCK {
    Point pts[NUMPTSTOBUFFER];
    struct _POINTBLOCK *next;
} POINTBLOCK;


#ifdef DEBUG
#include <stdio.h>
#define assert(expr) {if (!(expr)) fprintf(stderr,			\
					   "Assertion failed file %s, line %d: expr\n", __FILE__, __LINE__); }
#else
#define assert(expr)
#endif

typedef void (*overlapFunc) (CCMRegion    *pReg,
			     RegionBox *r1,
			     RegionBox *r1End,
			     RegionBox *r2,
			     RegionBox *r2End,
			     gint          y1,
			     gint          y2);
typedef void (*nonOverlapFunc) (CCMRegion    *pReg,
				RegionBox *r,
				RegionBox *rEnd,
				gint          y1,
				gint          y2);

static void miRegionCopy (CCMRegion      *dstrgn,
			  CCMRegion      *rgn);
static void miRegionOp   (CCMRegion      *newReg,
			  CCMRegion      *reg1,
			  CCMRegion      *reg2,
			  overlapFunc     overlapFn,
			  nonOverlapFunc  nonOverlap1Fn,
			  nonOverlapFunc  nonOverlap2Fn);

/* This function is identical to the C99 function lround(), except that it
 * performs arithmetic rounding (instead of away-from-zero rounding) and
 * has a valid input range of (INT_MIN, INT_MAX] instead of
 * [INT_MIN, INT_MAX]. It is much faster on both x86 and FPU-less systems
 * than other commonly used methods for rounding (lround, round, rint, lrint
 * or float (d + 0.5)).
 *
 * The reason why this function is much faster on x86 than other
 * methods is due to the fact that it avoids the fldcw instruction.
 * This instruction incurs a large performance penalty on modern Intel
 * processors due to how it prevents efficient instruction pipelining.
 *
 * The reason why this function is much faster on FPU-less systems is for
 * an entirely different reason. All common rounding methods involve multiple
 * floating-point operations. Each one of these operations has to be
 * emulated in software, which adds up to be a large performance penalty.
 * This function doesn't perform any floating-point calculations, and thus
 * avoids this penalty.
  */
int
_cairo_lround (double d)
{
    guint32 top, shift_amount, output;
    union {
        double d;
        guint64 ui64;
        guint32 ui32[2];
    } u;

    u.d = d;

    /* If the integer word order doesn't match the float word order, we swap
     * the words of the input double. This is needed because we will be
     * treating the whole double as a 64-bit unsigned integer. Notice that we
     * use WORDS_BIGENDIAN to detect the integer word order, which isn't
     * exactly correct because WORDS_BIGENDIAN refers to byte order, not word
     * order. Thus, we are making the assumption that the byte order is the
     * same as the integer word order which, on the modern machines that we
     * care about, is OK.
     */
#if ( defined(FLOAT_WORDS_BIGENDIAN) && !defined(WORDS_BIGENDIAN)) || \
    (!defined(FLOAT_WORDS_BIGENDIAN) &&  defined(WORDS_BIGENDIAN))
    {
        uint32_t temp = u.ui32[0];
        u.ui32[0] = u.ui32[1];
        u.ui32[1] = temp;
    }
#endif

#ifdef WORDS_BIGENDIAN
    #define MSW (0) /* Most Significant Word */
    #define LSW (1) /* Least Significant Word */
#else
    #define MSW (1)
    #define LSW (0)
#endif

    /* By shifting the most significant word of the input double to the
     * right 20 places, we get the very "top" of the double where the exponent
     * and sign bit lie.
     */
    top = u.ui32[MSW] >> 20;

    /* Here, we calculate how much we have to shift the mantissa to normalize
     * it to an integer value. We extract the exponent "top" by masking out the
     * sign bit, then we calculate the shift amount by subtracting the exponent
     * from the bias. Notice that the correct bias for 64-bit doubles is
     * actually 1075, but we use 1053 instead for two reasons:
     *
     *  1) To perform rounding later on, we will first need the target
     *     value in a 31.1 fixed-point format. Thus, the bias needs to be one
     *     less: (1075 - 1: 1074).
     *
     *  2) To avoid shifting the mantissa as a full 64-bit integer (which is
     *     costly on certain architectures), we break the shift into two parts.
     *     First, the upper and lower parts of the mantissa are shifted
     *     individually by a constant amount that all valid inputs will require
     *     at the very least. This amount is chosen to be 21, because this will
     *     allow the two parts of the mantissa to later be combined into a
     *     single 32-bit representation, on which the remainder of the shift
     *     will be performed. Thus, we decrease the bias by an additional 21:
     *     (1074 - 21: 1053).
     */
    shift_amount = 1053 - (top & 0x7FF);

    /* We are done with the exponent portion in "top", so here we shift it off
     * the end.
     */
    top >>= 11;

    /* Before we perform any operations on the mantissa, we need to OR in
     * the implicit 1 at the top (see the IEEE-754 spec). We needn't mask
     * off the sign bit nor the exponent bits because these higher bits won't
     * make a bit of difference in the rest of our calculations.
     */
    u.ui32[MSW] |= 0x100000;

    /* If the input double is negative, we have to decrease the mantissa
     * by a hair. This is an important part of performing arithmetic rounding,
     * as negative numbers must round towards positive infinity in the
     * halfwase case of -x.5. Since "top" contains only the sign bit at this
     * point, we can just decrease the mantissa by the value of "top".
     */
    u.ui64 -= top;

    /* By decrementing "top", we create a bitmask with a value of either
     * 0x0 (if the input was negative) or 0xFFFFFFFF (if the input was positive
     * and thus the unsigned subtraction underflowed) that we'll use later.
     */
    top--;

    /* Here, we shift the mantissa by the constant value as described above.
     * We can emulate a 64-bit shift right by 21 through shifting the top 32
     * bits left 11 places and ORing in the bottom 32 bits shifted 21 places
     * to the right. Both parts of the mantissa are now packed into a single
     * 32-bit integer. Although we severely truncate the lower part in the
     * process, we still have enough significant bits to perform the conversion
     * without error (for all valid inputs).
     */
    output = (u.ui32[MSW] << 11) | (u.ui32[LSW] >> 21);

    /* Next, we perform the shift that converts the X.Y fixed-point number
     * currently found in "output" to the desired 31.1 fixed-point format
     * needed for the following rounding step. It is important to consider
     * all possible values for "shift_amount" at this point:
     *
     * - {shift_amount < 0} Since shift_amount is an unsigned integer, it
     *   really can't have a value less than zero. But, if the shift_amount
     *   calculation above caused underflow (which would happen with
     *   input > INT_MAX or input <= INT_MIN) then shift_amount will now be
     *   a very large number, and so this shift will result in complete
     *   garbage. But that's OK, as the input was out of our range, so our
     *   output is undefined.
     *
     * - {shift_amount > 31} If the magnitude of the input was very small
     *   (i.e. |input| << 1.0), shift_amount will have a value greater than
     *   31. Thus, this shift will also result in garbage. After performing
     *   the shift, we will zero-out "output" if this is the case.
     *
     * - {0 <= shift_amount < 32} In this case, the shift will properly convert
     *   the mantissa into a 31.1 fixed-point number.
     */
    output >>= shift_amount;

    /* This is where we perform rounding with the 31.1 fixed-point number.
     * Since what we're after is arithmetic rounding, we simply add the single
     * fractional bit into the integer part of "output", and just keep the
     * integer part.
     */
    output = (output >> 1) + (output & 1);

    /* Here, we zero-out the result if the magnitude if the input was very small
     * (as explained in the section above). Notice that all input out of the
     * valid range is also caught by this condition, which means we produce 0
     * for all invalid input, which is a nice side effect.
     *
     * The most straightforward way to do this would be:
     *
     *      if (shift_amount > 31)
     *          output = 0;
     *
     * But we can use a little trick to avoid the potential branch. The
     * expression (shift_amount > 31) will be either 1 or 0, which when
     * decremented will be either 0x0 or 0xFFFFFFFF (unsigned underflow),
     * which can be used to conditionally mask away all the bits in "output"
     * (in the 0x0 case), effectively zeroing it out. Certain, compilers would
     * have done this for us automatically.
     */
    output &= ((shift_amount > 31) - 1);

    /* If the input double was a negative number, then we have to negate our
     * output. The most straightforward way to do this would be:
     *
     *      if (!top)
     *          output = -output;
     *
     * as "top" at this point is either 0x0 (if the input was negative) or
     * 0xFFFFFFFF (if the input was positive). But, we can use a trick to
     * avoid the branch. Observe that the following snippet of code has the
     * same effect as the reference snippet above:
     *
     *      if (!top)
     *          output = 0 - output;
     *      else
     *          output = output - 0;
     *
     * Armed with the bitmask found in "top", we can condense the two statements
     * into the following:
     *
     *      output = (output & top) - (output & ~top);
     *
     * where, in the case that the input double was negative, "top" will be 0,
     * and the statement will be equivalent to:
     *
     *      output = (0) - (output);
     *
     * and if the input double was positive, "top" will be 0xFFFFFFFF, and the
     * statement will be equivalent to:
     *
     *      output = (output) - (0);
     *
     * Which, as pointed out earlier, is equivalent to the original reference
     * snippet.
     */
    output = (output & top) - (output & ~top);

    return output;
#undef MSW
#undef LSW
}

/*	Create a new empty region	*/

CCMRegion *
ccm_region_new ()
{
    CCMRegion *temp;
    
    temp = g_new (CCMRegion, 1);
    temp->rects = g_new (RegionBox, 1);
    
    temp->numRects = 0;
    temp->extents.x1 = 0;
    temp->extents.y1 = 0;
    temp->extents.x2 = 0;
    temp->extents.y2 = 0;
    temp->size = 1;
    
    return temp;
}

/**
 * ccm_region_rectangle:
 * @rectangle: a #cairo_rectangle_t
 * 
 * Creates a new region containing the area @rectangle.
 * 
 * Return value: a new region
 **/
CCMRegion *
ccm_region_rectangle (cairo_rectangle_t *rectangle)
{
    CCMRegion *temp;
    
    g_return_val_if_fail (rectangle != NULL, NULL);
    
    if (rectangle->width <= 0 || rectangle->height <= 0)
	return ccm_region_new();
    
    temp = g_new (CCMRegion, 1);
    temp->rects = g_new (RegionBox, 1);
    
    temp->numRects = 1;
    temp->extents.x1 = temp->rects[0].x1 = rectangle->x;
    temp->extents.y1 = temp->rects[0].y1 = rectangle->y;
    temp->extents.x2 = temp->rects[0].x2 = rectangle->x + rectangle->width;
    temp->extents.y2 = temp->rects[0].y2 = rectangle->y + rectangle->height;
    temp->size = 1;
    
    return temp;
}


/**
 * ccm_region_xrectangle:
 * @rectangle: a #XRectangle
 * 
 * Creates a new region containing the area @rectangle.
 * 
 * Return value: a new region
 **/
CCMRegion *
ccm_region_xrectangle (XRectangle *rectangle)
{
    CCMRegion *temp;
    
    g_return_val_if_fail (rectangle != NULL, NULL);
    
    if (rectangle->width <= 0 || rectangle->height <= 0)
	return ccm_region_new();
    
    temp = g_new (CCMRegion, 1);
    temp->rects = g_new (RegionBox, 1);
    
    temp->numRects = 1;
    temp->extents.x1 = temp->rects[0].x1 = rectangle->x;
    temp->extents.y1 = temp->rects[0].y1 = rectangle->y;
    temp->extents.x2 = temp->rects[0].x2 = rectangle->x + rectangle->width;
    temp->extents.y2 = temp->rects[0].y2 = rectangle->y + rectangle->height;
    temp->size = 1;
    
    return temp;
}

/**
 * ccm_region_copy:
 * @region: a #CCMRegion
 * 
 * Copies @region, creating an identical new region.
 * 
 * Return value: a new region identical to @region
 **/
CCMRegion *
ccm_region_copy (CCMRegion *region)
{
    CCMRegion *temp;
    
    g_return_val_if_fail (region != NULL, NULL);
    
    temp = g_new (CCMRegion, 1);
    temp->rects = g_new (RegionBox, region->numRects);
    
    temp->numRects = region->numRects;
    temp->extents = region->extents;
    temp->size = region->numRects;
    
    memcpy (temp->rects, region->rects, region->numRects * sizeof (RegionBox));
    
    return temp;
}

void
ccm_region_get_clipbox (CCMRegion *r, cairo_rectangle_t *rect)
{
    g_return_if_fail (r != NULL);
    g_return_if_fail (rect != NULL);
    
    rect->x = r->extents.x1;
    rect->y = r->extents.y1;
    rect->width = r->extents.x2 - r->extents.x1;
    rect->height = r->extents.y2 - r->extents.y1;
}


/**
 * ccm_region_get_rectangles:
 * @region: a #CCMRegion
 * @rectangles: return location for an array of rectangles
 * @n_rectangles: length of returned array
 *
 * Obtains the area covered by the region as a list of rectangles.
 * The array returned in @rectangles must be freed with g_free().
 * 
 **/
void
ccm_region_get_rectangles (CCMRegion     *region,
			  cairo_rectangle_t **rectangles,
			  gint          *n_rectangles)
{
    gint i;
    
    g_return_if_fail (region != NULL);
    g_return_if_fail (rectangles != NULL);
    g_return_if_fail (n_rectangles != NULL);
    
    *n_rectangles = region->numRects;
    *rectangles = g_new (cairo_rectangle_t, region->numRects);
    
    for (i = 0; i < region->numRects; i++)
    {
	RegionBox rect;
	rect = region->rects[i];
	(*rectangles)[i].x = rect.x1;
	(*rectangles)[i].y = rect.y1;
	(*rectangles)[i].width = rect.x2 - rect.x1;
	(*rectangles)[i].height = rect.y2 - rect.y1;
    }
}

/**
 * ccm_region_get_xrectangles:
 * @region: a #CCMRegion
 * @rectangles: return location for an array of xrectangles
 * @n_rectangles: length of returned array
 *
 * Obtains the area covered by the region as a list of rectangles.
 * The array returned in @rectangles must be freed with g_free().
 * 
 **/
void
ccm_region_get_xrectangles (CCMRegion     *region,
			  XRectangle **rectangles,
			  gint          *n_rectangles)
{
    gint i;
    
    g_return_if_fail (region != NULL);
    g_return_if_fail (rectangles != NULL);
    g_return_if_fail (n_rectangles != NULL);
    
    *n_rectangles = region->numRects;
    *rectangles = g_new (XRectangle, region->numRects);
    
    for (i = 0; i < region->numRects; i++)
    {
	RegionBox rect;
	rect = region->rects[i];
	(*rectangles)[i].x = rect.x1;
	(*rectangles)[i].y = rect.y1;
	(*rectangles)[i].width = rect.x2 - rect.x1;
	(*rectangles)[i].height = rect.y2 - rect.y1;
    }
}

/**
 * ccm_region_union_with_rect:
 * @region: a #CCMRegion.
 * @rect: a #cairo_rectangle_t.
 * 
 * Sets the area of @region to the union of the areas of @region and
 * @rect. The resulting area is the set of pixels contained in
 * either @region or @rect.
 **/
void
ccm_region_union_with_rect (CCMRegion    *region,
			   cairo_rectangle_t *rect)
{
    CCMRegion tmp_region;
    
    g_return_if_fail (region != NULL);
    g_return_if_fail (rect != NULL);
    
    if (!rect->width || !rect->height)
	return;
    
    tmp_region.rects = &tmp_region.extents;
    tmp_region.numRects = 1;
    tmp_region.extents.x1 = rect->x;
    tmp_region.extents.y1 = rect->y;
    tmp_region.extents.x2 = rect->x + rect->width;
    tmp_region.extents.y2 = rect->y + rect->height;
    tmp_region.size = 1;
    
    ccm_region_union (region, &tmp_region);
}

/**
 * ccm_region_union_with_xrect:
 * @region: a #CCMRegion.
 * @rect: a #XRectangle.
 * 
 * Sets the area of @region to the union of the areas of @region and
 * @rect. The resulting area is the set of pixels contained in
 * either @region or @rect.
 **/
void
ccm_region_union_with_xrect (CCMRegion    *region,
							 XRectangle   *rect)
{
    CCMRegion tmp_region;
    
    g_return_if_fail (region != NULL);
    g_return_if_fail (rect != NULL);
    
    if (!rect->width || !rect->height)
	return;
    
    tmp_region.rects = &tmp_region.extents;
    tmp_region.numRects = 1;
    tmp_region.extents.x1 = rect->x;
    tmp_region.extents.y1 = rect->y;
    tmp_region.extents.x2 = rect->x + rect->width;
    tmp_region.extents.y2 = rect->y + rect->height;
    tmp_region.size = 1;
    
    ccm_region_union (region, &tmp_region);
}

/*-
 *-----------------------------------------------------------------------
 * miSetExtents --
 *	Reset the extents of a region to what they should be. Called by
 *	miSubtract and miIntersect b/c they can't figure it out along the
 *	way or do so easily, as miUnion can.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void
miSetExtents (CCMRegion *pReg)
{
    RegionBox *pBox, *pBoxEnd, *pExtents;
    
    if (pReg->numRects == 0)
    {
	pReg->extents.x1 = 0;
	pReg->extents.y1 = 0;
	pReg->extents.x2 = 0;
	pReg->extents.y2 = 0;
	return;
    }
    
    pExtents = &pReg->extents;
    pBox = pReg->rects;
    pBoxEnd = &pBox[pReg->numRects - 1];
    
    /*
     * Since pBox is the first rectangle in the region, it must have the
     * smallest y1 and since pBoxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  pBox and pBoxEnd, resp., as good things to initialize them
     * to...
     */
    pExtents->x1 = pBox->x1;
    pExtents->y1 = pBox->y1;
    pExtents->x2 = pBoxEnd->x2;
    pExtents->y2 = pBoxEnd->y2;
    
    assert(pExtents->y1 < pExtents->y2);
    while (pBox <= pBoxEnd)
    {
	if (pBox->x1 < pExtents->x1)
	{
	    pExtents->x1 = pBox->x1;
	}
	if (pBox->x2 > pExtents->x2)
	{
	    pExtents->x2 = pBox->x2;
	}
	pBox++;
    }
    assert(pExtents->x1 < pExtents->x2);
}

void
ccm_region_destroy (CCMRegion *r)
{
    g_return_if_fail (r != NULL);
    
    g_free (r->rects);
    g_free (r);
}


/* TranslateRegion(pRegion, x, y)
   translates in place
   added by raymond
*/

void
ccm_region_offset (CCMRegion *region,
		  gint       x,
		  gint       y)
{
    int nbox;
    RegionBox *pbox;
    
    g_return_if_fail (region != NULL);
    
    pbox = region->rects;
    nbox = region->numRects;
    
    while(nbox--)
    {
	pbox->x1 += x;
	pbox->x2 += x;
	pbox->y1 += y;
	pbox->y2 += y;
	pbox++;
    }
    region->extents.x1 += x;
    region->extents.x2 += x;
    region->extents.y1 += y;
    region->extents.y2 += y;
}

/* ResizeRegion(pRegion, width, height)
   resize in place
   added by gandalfn for ccm
*/
void
ccm_region_resize (CCMRegion *region,
		  gint       width,
		  gint       height)
{
    int nbox;
	double scale_width, scale_height;
    RegionBox *pbox;
    
    g_return_if_fail (region != NULL);
	g_return_if_fail (width != 0 && height != 0);
    
	scale_width = (double)width / (double)(region->extents.x2 - region->extents.x1);
	scale_height = (double)height / (double)(region->extents.y2 - region->extents.y1);
	
    region->extents.x2 = region->extents.x1 + width;
    region->extents.y2 = region->extents.y1 + height;
	
    pbox = region->rects;
    nbox = region->numRects;
    
    while(nbox--)
    {
	pbox->x2 = _cairo_lround(((double)pbox->x2 - (double)pbox->x1) * scale_width) + 
			    pbox->x1;
	pbox->y2 = _cairo_lround(((double)pbox->y2 - (double)pbox->y1) * scale_height) + 
			    pbox->y1;
	pbox++;
    }
}

/* ScaleRegion(pRegion, scale_width, scale_height)
   scale region
   added by gandalfn for ccm
*/
void
ccm_region_scale (CCMRegion *region,
		  gdouble       scale_width,
		  gdouble       scale_height)
{
    g_return_if_fail (region != NULL);
	g_return_if_fail (scale_width != 0 && scale_height != 0);
    
    int width, height;
    
	width = _cairo_lround((double)(region->extents.x2 - region->extents.x1) * scale_width) + 1;
    height = _cairo_lround((double)(region->extents.y2 - region->extents.y1) * scale_height) + 1;
    
    ccm_region_resize(region, width, height);
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

#define ZOpRegion(a,b) if (grow) ccm_region_union (a, b);		\
    else ccm_region_intersect (a,b)
#define ZShiftRegion(a,b) if (xdir) ccm_region_offset (a,b,0);		\
    else ccm_region_offset (a,0,b)

static void
Compress(CCMRegion *r,
	 CCMRegion *s,
	 CCMRegion *t,
	 guint      dx,
	 int        xdir,
	 int        grow)
{
    guint shift = 1;
    
    miRegionCopy (s, r);
    while (dx)
    {
	if (dx & shift)
	{
	    ZShiftRegion(r, -(int)shift);
	    ZOpRegion(r, s);
	    dx -= shift;
	    if (!dx) break;
        }
	miRegionCopy (t, s);
	ZShiftRegion(s, -(int)shift);
	ZOpRegion(s, t);
	shift <<= 1;
    }
}

#undef ZOpRegion
#undef ZShiftRegion
#undef ZCopyRegion

void
ccm_region_shrink (CCMRegion *r,
		  int        dx,
		  int        dy)
{
    CCMRegion *s, *t;
    int grow;
    
    g_return_if_fail (r != NULL);
    
    if (!dx && !dy)
	return;
    
    s = ccm_region_new ();
    t = ccm_region_new ();
    
    grow = (dx < 0);
    if (grow)
	dx = -dx;
    if (dx)
	Compress(r, s, t, (unsigned) 2*dx, TRUE, grow);
    
    grow = (dy < 0);
    if (grow)
	dy = -dy;
    if (dy)
	Compress(r, s, t, (unsigned) 2*dy, FALSE, grow);
    
    ccm_region_offset (r, dx, dy);
    ccm_region_destroy (s);
    ccm_region_destroy (t);
}


/*======================================================================
 *	    Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * miIntersectO --
 *	Handle an overlapping band for miIntersect.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miIntersectO (CCMRegion    *pReg,
	      RegionBox *r1,
	      RegionBox *r1End,
	      RegionBox *r2,
	      RegionBox *r2End,
	      gint          y1,
	      gint          y2)
{
    int  	x1;
    int  	x2;
    RegionBox *pNextRect;
    
    pNextRect = &pReg->rects[pReg->numRects];
    
    while ((r1 != r1End) && (r2 != r2End))
    {
	x1 = MAX (r1->x1,r2->x1);
	x2 = MIN (r1->x2,r2->x2);
	
	/*
	 * If there's any overlap between the two rectangles, add that
	 * overlap to the new region.
	 * There's no need to check for subsumption because the only way
	 * such a need could arise is if some region has two rectangles
	 * right next to each other. Since that should never happen...
	 */
	if (x1 < x2)
	{
	    assert (y1<y2);
	    
	    MEMCHECK (pReg, pNextRect, pReg->rects);
	    pNextRect->x1 = x1;
	    pNextRect->y1 = y1;
	    pNextRect->x2 = x2;
	    pNextRect->y2 = y2;
	    pReg->numRects += 1;
	    pNextRect++;
	    assert (pReg->numRects <= pReg->size);
	}
	
	/*
	 * Need to advance the pointers. Shift the one that extends
	 * to the right the least, since the other still has a chance to
	 * overlap with that region's next rectangle, if you see what I mean.
	 */
	if (r1->x2 < r2->x2)
	{
	    r1++;
	}
	else if (r2->x2 < r1->x2)
	{
	    r2++;
	}
	else
	{
	    r1++;
	    r2++;
	}
    }
}

/**
 * ccm_region_intersect:
 * @source1: a #CCMRegion
 * @source2: another #CCMRegion
 *
 * Sets the area of @source1 to the intersection of the areas of @source1
 * and @source2. The resulting area is the set of pixels contained in
 * both @source1 and @source2.
 **/
void
ccm_region_intersect (CCMRegion *region,
		     CCMRegion *other)
{
    g_return_if_fail (region != NULL);
    g_return_if_fail (other != NULL);
    
    /* check for trivial reject */
    if ((!(region->numRects)) || (!(other->numRects))  ||
	(!EXTENTCHECK(&region->extents, &other->extents)))
	region->numRects = 0;
    else
	miRegionOp (region, region, other, 
		    miIntersectO, (nonOverlapFunc) NULL, (nonOverlapFunc) NULL);
    
    /*
     * Can't alter region's extents before miRegionOp depends on the
     * extents of the regions being unchanged. Besides, this way there's
     * no checking against rectangles that will be nuked due to
     * coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(region);
}

static void
miRegionCopy(CCMRegion *dstrgn, CCMRegion *rgn)
{
    if (dstrgn != rgn) /*  don't want to copy to itself */
    {  
	if (dstrgn->size < rgn->numRects)
        {
	    dstrgn->rects = g_renew (RegionBox, dstrgn->rects, rgn->numRects);
	    dstrgn->size = rgn->numRects;
	}
	dstrgn->numRects = rgn->numRects;
	dstrgn->extents.x1 = rgn->extents.x1;
	dstrgn->extents.y1 = rgn->extents.y1;
	dstrgn->extents.x2 = rgn->extents.x2;
	dstrgn->extents.y2 = rgn->extents.y2;
	
	memcpy (dstrgn->rects, rgn->rects, rgn->numRects * sizeof (RegionBox));
    }
}


/*======================================================================
 *	    Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miCoalesce --
 *	Attempt to merge the boxes in the current band with those in the
 *	previous one. Used only by miRegionOp.
 *
 * Results:
 *	The new index for the previous band.
 *
 * Side Effects:
 *	If coalescing takes place:
 *	    - rectangles in the previous band will have their y2 fields
 *	      altered.
 *	    - pReg->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
/* static int*/
static int
miCoalesce (CCMRegion *pReg,         /* Region to coalesce */
	    gint       prevStart,    /* Index of start of previous band */
	    gint       curStart)     /* Index of start of current band */
{
    RegionBox *pPrevBox;   	/* Current box in previous band */
    RegionBox *pCurBox;    	/* Current box in current band */
    RegionBox *pRegEnd;    	/* End of region */
    int	    	curNumRects;	/* Number of rectangles in current
				 * band */
    int	    	prevNumRects;	/* Number of rectangles in previous
				 * band */
    int	    	bandY1;	    	/* Y1 coordinate for current band */
    
    pRegEnd = &pReg->rects[pReg->numRects];
    
    pPrevBox = &pReg->rects[prevStart];
    prevNumRects = curStart - prevStart;
    
    /*
     * Figure out how many rectangles are in the current band. Have to do
     * this because multiple bands could have been added in miRegionOp
     * at the end when one region has been exhausted.
     */
    pCurBox = &pReg->rects[curStart];
    bandY1 = pCurBox->y1;
    for (curNumRects = 0;
	 (pCurBox != pRegEnd) && (pCurBox->y1 == bandY1);
	 curNumRects++)
    {
	pCurBox++;
    }
    
    if (pCurBox != pRegEnd)
    {
	/*
	 * If more than one band was added, we have to find the start
	 * of the last band added so the next coalescing job can start
	 * at the right place... (given when multiple bands are added,
	 * this may be pointless -- see above).
	 */
	pRegEnd--;
	while (pRegEnd[-1].y1 == pRegEnd->y1)
	{
	    pRegEnd--;
	}
	curStart = pRegEnd - pReg->rects;
	pRegEnd = pReg->rects + pReg->numRects;
    }
    
    if ((curNumRects == prevNumRects) && (curNumRects != 0)) {
	pCurBox -= curNumRects;
	/*
	 * The bands may only be coalesced if the bottom of the previous
	 * matches the top scanline of the current.
	 */
	if (pPrevBox->y2 == pCurBox->y1)
	{
	    /*
	     * Make sure the bands have boxes in the same places. This
	     * assumes that boxes have been added in such a way that they
	     * cover the most area possible. I.e. two boxes in a band must
	     * have some horizontal space between them.
	     */
	    do
	    {
		if ((pPrevBox->x1 != pCurBox->x1) ||
		    (pPrevBox->x2 != pCurBox->x2))
		{
		    /*
		     * The bands don't line up so they can't be coalesced.
		     */
		    return (curStart);
		}
		pPrevBox++;
		pCurBox++;
		prevNumRects -= 1;
	    } while (prevNumRects != 0);
	    
	    pReg->numRects -= curNumRects;
	    pCurBox -= curNumRects;
	    pPrevBox -= curNumRects;
	    
	    /*
	     * The bands may be merged, so set the bottom y of each box
	     * in the previous band to that of the corresponding box in
	     * the current band.
	     */
	    do
	    {
		pPrevBox->y2 = pCurBox->y2;
		pPrevBox++;
		pCurBox++;
		curNumRects -= 1;
	    }
	    while (curNumRects != 0);
	    
	    /*
	     * If only one band was added to the region, we have to backup
	     * curStart to the start of the previous band.
	     *
	     * If more than one band was added to the region, copy the
	     * other bands down. The assumption here is that the other bands
	     * came from the same region as the current one and no further
	     * coalescing can be done on them since it's all been done
	     * already... curStart is already in the right place.
	     */
	    if (pCurBox == pRegEnd)
	    {
		curStart = prevStart;
	    }
	    else
	    {
		do
		{
		    *pPrevBox++ = *pCurBox++;
		}
		while (pCurBox != pRegEnd);
	    }
	    
	}
    }
    return curStart;
}

/*-
 *-----------------------------------------------------------------------
 * miRegionOp --
 *	Apply an operation to two regions. Called by miUnion, miInverse,
 *	miSubtract, miIntersect...
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The new region is overwritten.
 *
 * Notes:
 *	The idea behind this function is to view the two regions as sets.
 *	Together they cover a rectangle of area that this function divides
 *	into horizontal bands where points are covered only by one region
 *	or by both. For the first case, the nonOverlapFunc is called with
 *	each the band and the band's upper and lower extents. For the
 *	second, the overlapFunc is called to process the entire band. It
 *	is responsible for clipping the rectangles in the band, though
 *	this function provides the boundaries.
 *	At the end of each band, the new region is coalesced, if possible,
 *	to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miRegionOp(CCMRegion *newReg,
	   CCMRegion *reg1,
	   CCMRegion *reg2,
	   overlapFunc    overlapFn,   	        /* Function to call for over-
						 * lapping bands */
	   nonOverlapFunc nonOverlap1Fn,	/* Function to call for non-
						 * overlapping bands in region
						 * 1 */
	   nonOverlapFunc nonOverlap2Fn)	/* Function to call for non-
						 * overlapping bands in region
						 * 2 */
{
    RegionBox *r1; 	    	    	/* Pointer into first region */
    RegionBox *r2; 	    	    	/* Pointer into 2d region */
    RegionBox *r1End;    	    	/* End of 1st region */
    RegionBox *r2End;    	    	/* End of 2d region */
    int    	  ybot;	    	    	/* Bottom of intersection */
    int  	  ytop;	    	    	/* Top of intersection */
    RegionBox *oldRects;   	    	/* Old rects for newReg */
    int	    	  prevBand;   	    	/* Index of start of
					 * previous band in newReg */
    int	  	  curBand;    	    	/* Index of start of current
					 * band in newReg */
    RegionBox *r1BandEnd;  	    	/* End of current band in r1 */
    RegionBox *r2BandEnd;  	    	/* End of current band in r2 */
    int     	  top;	    	    	/* Top of non-overlapping
					 * band */
    int     	  bot;	    	    	/* Bottom of non-overlapping
					 * band */
    
    /*
     * Initialization:
     *	set r1, r2, r1End and r2End appropriately, preserve the important
     * parts of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */
    r1 = reg1->rects;
    r2 = reg2->rects;
    r1End = r1 + reg1->numRects;
    r2End = r2 + reg2->numRects;
    
    oldRects = newReg->rects;
    
    EMPTY_REGION(newReg);
    
    /*
     * Allocate a reasonable number of rectangles for the new region. The idea
     * is to allocate enough so the individual functions don't need to
     * reallocate and copy the array, which is time consuming, yet we don't
     * have to worry about using too much memory. I hope to be able to
     * nuke the Xrealloc() at the end of this function eventually.
     */
    newReg->size = MAX (reg1->numRects, reg2->numRects) * 2;
    newReg->rects = g_new (RegionBox, newReg->size);
    
    /*
     * Initialize ybot and ytop.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     * 	In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *	For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */
    if (reg1->extents.y1 < reg2->extents.y1)
	ybot = reg1->extents.y1;
    else
	ybot = reg2->extents.y1;
    
    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. miCoalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;
    
    do
    {
	curBand = newReg->numRects;
	
	/*
	 * This algorithm proceeds one source-band (as opposed to a
	 * destination band, which is determined by where the two regions
	 * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
	 * rectangle after the last one in the current band for their
	 * respective regions.
	 */
	r1BandEnd = r1;
	while ((r1BandEnd != r1End) && (r1BandEnd->y1 == r1->y1))
	{
	    r1BandEnd++;
	}
	
	r2BandEnd = r2;
	while ((r2BandEnd != r2End) && (r2BandEnd->y1 == r2->y1))
	{
	    r2BandEnd++;
	}
	
	/*
	 * First handle the band that doesn't intersect, if any.
	 *
	 * Note that attention is restricted to one band in the
	 * non-intersecting region at once, so if a region has n
	 * bands between the current position and the next place it overlaps
	 * the other, this entire loop will be passed through n times.
	 */
	if (r1->y1 < r2->y1)
	{
	    top = MAX (r1->y1,ybot);
	    bot = MIN (r1->y2,r2->y1);
	    
	    if ((top != bot) && (nonOverlap1Fn != (void (*)())NULL))
	    {
		(* nonOverlap1Fn) (newReg, r1, r1BandEnd, top, bot);
	    }
	    
	    ytop = r2->y1;
	}
	else if (r2->y1 < r1->y1)
	{
	    top = MAX (r2->y1,ybot);
	    bot = MIN (r2->y2,r1->y1);
	    
	    if ((top != bot) && (nonOverlap2Fn != (void (*)())NULL))
	    {
		(* nonOverlap2Fn) (newReg, r2, r2BandEnd, top, bot);
	    }
	    
	    ytop = r1->y1;
	}
	else
	{
	    ytop = r1->y1;
	}
	
	/*
	 * If any rectangles got added to the region, try and coalesce them
	 * with rectangles from the previous band. Note we could just do
	 * this test in miCoalesce, but some machines incur a not
	 * inconsiderable cost for function calls, so...
	 */
	if (newReg->numRects != curBand)
	{
	    prevBand = miCoalesce (newReg, prevBand, curBand);
	}
	
	/*
	 * Now see if we've hit an intersecting band. The two bands only
	 * intersect if ybot > ytop
	 */
	ybot = MIN (r1->y2, r2->y2);
	curBand = newReg->numRects;
	if (ybot > ytop)
	{
	    (* overlapFn) (newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot);
	    
	}
	
	if (newReg->numRects != curBand)
	{
	    prevBand = miCoalesce (newReg, prevBand, curBand);
	}
	
	/*
	 * If we've finished with a band (y2 == ybot) we skip forward
	 * in the region to the next band.
	 */
	if (r1->y2 == ybot)
	{
	    r1 = r1BandEnd;
	}
	if (r2->y2 == ybot)
	{
	    r2 = r2BandEnd;
	}
    } while ((r1 != r1End) && (r2 != r2End));
    
    /*
     * Deal with whichever region still has rectangles left.
     */
    curBand = newReg->numRects;
    if (r1 != r1End)
    {
	if (nonOverlap1Fn != (nonOverlapFunc )NULL)
	{
	    do
	    {
		r1BandEnd = r1;
		while ((r1BandEnd < r1End) && (r1BandEnd->y1 == r1->y1))
		{
		    r1BandEnd++;
		}
		(* nonOverlap1Fn) (newReg, r1, r1BandEnd,
				   MAX (r1->y1,ybot), r1->y2);
		r1 = r1BandEnd;
	    } while (r1 != r1End);
	}
    }
    else if ((r2 != r2End) && (nonOverlap2Fn != (nonOverlapFunc) NULL))
    {
	do
	{
	    r2BandEnd = r2;
	    while ((r2BandEnd < r2End) && (r2BandEnd->y1 == r2->y1))
	    {
		r2BandEnd++;
	    }
	    (* nonOverlap2Fn) (newReg, r2, r2BandEnd,
			       MAX (r2->y1,ybot), r2->y2);
	    r2 = r2BandEnd;
	} while (r2 != r2End);
    }
    
    if (newReg->numRects != curBand)
    {
	(void) miCoalesce (newReg, prevBand, curBand);
    }
    
    /*
     * A bit of cleanup. To keep regions from growing without bound,
     * we shrink the array of rectangles to match the new number of
     * rectangles in the region. This never goes to 0, however...
     *
     * Only do this stuff if the number of rectangles allocated is more than
     * twice the number of rectangles in the region (a simple optimization...).
     */
    if (newReg->numRects < (newReg->size >> 1))
    {
	if (REGION_NOT_EMPTY (newReg))
	{
	    newReg->size = newReg->numRects;
	    newReg->rects = g_renew (RegionBox, newReg->rects, newReg->size);
	}
	else
	{
	    /*
	     * No point in doing the extra work involved in an Xrealloc if
	     * the region is empty
	     */
	    newReg->size = 1;
	    g_free (newReg->rects);
	    newReg->rects = g_new (RegionBox, 1);
	}
    }
    g_free (oldRects);
}


/*======================================================================
 *	    Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miUnionNonO --
 *	Handle a non-overlapping band for the union operation. Just
 *	Adds the rectangles into the region. Doesn't have to check for
 *	subsumption or anything.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg->numRects is incremented and the final rectangles overwritten
 *	with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */
static void
miUnionNonO (CCMRegion    *pReg,
	     RegionBox *r,
	     RegionBox *rEnd,
	     gint          y1,
	     gint          y2)
{
    RegionBox *pNextRect;
    
    pNextRect = &pReg->rects[pReg->numRects];
    
    assert(y1 < y2);
    
    while (r != rEnd)
    {
	assert(r->x1 < r->x2);
	MEMCHECK(pReg, pNextRect, pReg->rects);
	pNextRect->x1 = r->x1;
	pNextRect->y1 = y1;
	pNextRect->x2 = r->x2;
	pNextRect->y2 = y2;
	pReg->numRects += 1;
	pNextRect++;
	
	assert(pReg->numRects<=pReg->size);
	r++;
    }
}


/*-
 *-----------------------------------------------------------------------
 * miUnionO --
 *	Handle an overlapping band for the union operation. Picks the
 *	left-most rectangle each time and merges it into the region.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Rectangles are overwritten in pReg->rects and pReg->numRects will
 *	be changed.
 *
 *-----------------------------------------------------------------------
 */

/* static void*/
static void
miUnionO (CCMRegion *pReg,
	  RegionBox *r1,
	  RegionBox *r1End,
	  RegionBox *r2,
	  RegionBox *r2End,
	  gint          y1,
	  gint          y2)
{
    RegionBox *	pNextRect;
    
    pNextRect = &pReg->rects[pReg->numRects];
    
#define MERGERECT(r) 					\
    if ((pReg->numRects != 0) &&  			\
	(pNextRect[-1].y1 == y1) &&  			\
	(pNextRect[-1].y2 == y2) &&  			\
	(pNextRect[-1].x2 >= r->x1))  			\
    {							\
	if (pNextRect[-1].x2 < r->x2)  			\
	{  						\
	    pNextRect[-1].x2 = r->x2;  			\
	    assert(pNextRect[-1].x1<pNextRect[-1].x2); 	\
	}  						\
    }							\
    else  						\
    {							\
	MEMCHECK(pReg, pNextRect, pReg->rects); 	\
	pNextRect->y1 = y1;  				\
	pNextRect->y2 = y2;  				\
	pNextRect->x1 = r->x1;  			\
	pNextRect->x2 = r->x2;  			\
	pReg->numRects += 1;  				\
        pNextRect += 1;  				\
    }							\
    assert(pReg->numRects<=pReg->size);			\
    r++;
    
    assert (y1<y2);
    while ((r1 != r1End) && (r2 != r2End))
    {
	if (r1->x1 < r2->x1)
	{
	    MERGERECT(r1);
	}
	else
	{
	    MERGERECT(r2);
	}
    }
    
    if (r1 != r1End)
    {
	do
	{
	    MERGERECT(r1);
	} while (r1 != r1End);
    }
    else while (r2 != r2End)
    {
	MERGERECT(r2);
    }
}

/**
 * ccm_region_union:
 * @source1:  a #CCMRegion
 * @source2: a #CCMRegion 
 * 
 * Sets the area of @source1 to the union of the areas of @source1 and
 * @source2. The resulting area is the set of pixels contained in
 * either @source1 or @source2.
 **/
void
ccm_region_union (CCMRegion *region,
		 CCMRegion *other)
{
    g_return_if_fail (region != NULL);
    g_return_if_fail (other != NULL);
    
    /*  checks all the simple cases */
    
    /*
     * region and other are the same or other is empty
     */
    if ((region == other) || (!(other->numRects)))
	return;
    
    /* 
     * region is empty
     */
    if (!(region->numRects))
    {
	miRegionCopy (region, other);
	return;
    }
    
    /*
     * region completely subsumes otehr
     */
    if ((region->numRects == 1) && 
	(region->extents.x1 <= other->extents.x1) &&
	(region->extents.y1 <= other->extents.y1) &&
	(region->extents.x2 >= other->extents.x2) &&
	(region->extents.y2 >= other->extents.y2))
	return;
    
    /*
     * other completely subsumes region
     */
    if ((other->numRects == 1) && 
	(other->extents.x1 <= region->extents.x1) &&
	(other->extents.y1 <= region->extents.y1) &&
	(other->extents.x2 >= region->extents.x2) &&
	(other->extents.y2 >= region->extents.y2))
    {
	miRegionCopy(region, other);
	return;
    }
    
    miRegionOp (region, region, other, miUnionO, 
		miUnionNonO, miUnionNonO);
    
    region->extents.x1 = MIN (region->extents.x1, other->extents.x1);
    region->extents.y1 = MIN (region->extents.y1, other->extents.y1);
    region->extents.x2 = MAX (region->extents.x2, other->extents.x2);
    region->extents.y2 = MAX (region->extents.y2, other->extents.y2);
}


/*======================================================================
 * 	    	  Region Subtraction
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miSubtractNonO --
 *	Deal with non-overlapping band for subtraction. Any parts from
 *	region 2 we discard. Anything from region 1 we add to the region.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg may be affected.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miSubtractNonO1 (CCMRegion    *pReg,
		 RegionBox *r,
		 RegionBox *rEnd,
		 gint          y1,
		 gint          y2)
{
    RegionBox *	pNextRect;
    
    pNextRect = &pReg->rects[pReg->numRects];
    
    assert(y1<y2);
    
    while (r != rEnd)
    {
	assert (r->x1<r->x2);
	MEMCHECK (pReg, pNextRect, pReg->rects);
	pNextRect->x1 = r->x1;
	pNextRect->y1 = y1;
	pNextRect->x2 = r->x2;
	pNextRect->y2 = y2;
	pReg->numRects += 1;
	pNextRect++;
	
	assert (pReg->numRects <= pReg->size);
	
	r++;
    }
}

/*-
 *-----------------------------------------------------------------------
 * miSubtractO --
 *	Overlapping band subtraction. x1 is the left-most point not yet
 *	checked.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/* static void*/
static void
miSubtractO (CCMRegion    *pReg,
	     RegionBox *r1,
	     RegionBox *r1End,
	     RegionBox *r2,
	     RegionBox *r2End,
	     gint          y1,
	     gint          y2)
{
    RegionBox *	pNextRect;
    int  	x1;
    
    x1 = r1->x1;
    
    assert(y1<y2);
    pNextRect = &pReg->rects[pReg->numRects];
    
    while ((r1 != r1End) && (r2 != r2End))
    {
	if (r2->x2 <= x1)
	{
	    /*
	     * Subtrahend missed the boat: go to next subtrahend.
	     */
	    r2++;
	}
	else if (r2->x1 <= x1)
	{
	    /*
	     * Subtrahend preceeds minuend: nuke left edge of minuend.
	     */
	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend completely covered: advance to next minuend and
		 * reset left fence to edge of new minuend.
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend now used up since it doesn't extend beyond
		 * minuend
		 */
		r2++;
	    }
	}
	else if (r2->x1 < r1->x2)
	{
	    /*
	     * Left part of subtrahend covers part of minuend: add uncovered
	     * part of minuend to region and skip to next subtrahend.
	     */
	    assert(x1<r2->x1);
	    MEMCHECK(pReg, pNextRect, pReg->rects);
	    pNextRect->x1 = x1;
	    pNextRect->y1 = y1;
	    pNextRect->x2 = r2->x1;
	    pNextRect->y2 = y2;
	    pReg->numRects += 1;
	    pNextRect++;
	    
	    assert(pReg->numRects<=pReg->size);
	    
	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend used up: advance to new...
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend used up
		 */
		r2++;
	    }
	}
	else
	{
	    /*
	     * Minuend used up: add any remaining piece before advancing.
	     */
	    if (r1->x2 > x1)
	    {
		MEMCHECK(pReg, pNextRect, pReg->rects);
		pNextRect->x1 = x1;
		pNextRect->y1 = y1;
		pNextRect->x2 = r1->x2;
		pNextRect->y2 = y2;
		pReg->numRects += 1;
		pNextRect++;
		assert(pReg->numRects<=pReg->size);
	    }
	    r1++;
	    if (r1 != r1End)
		x1 = r1->x1;
	}
    }
    
    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1End)
    {
	assert(x1<r1->x2);
	MEMCHECK(pReg, pNextRect, pReg->rects);
	pNextRect->x1 = x1;
	pNextRect->y1 = y1;
	pNextRect->x2 = r1->x2;
	pNextRect->y2 = y2;
	pReg->numRects += 1;
	pNextRect++;
	
	assert(pReg->numRects<=pReg->size);
	
	r1++;
	if (r1 != r1End)
	{
	    x1 = r1->x1;
	}
    }
}

/**
 * ccm_region_subtract:
 * @source1: a #CCMRegion
 * @source2: another #CCMRegion
 *
 * Subtracts the area of @source2 from the area @source1. The resulting
 * area is the set of pixels contained in @source1 but not in @source2.
 **/
void
ccm_region_subtract (CCMRegion *region,
		    CCMRegion *other)
{
    g_return_if_fail (region != NULL);
    g_return_if_fail (other != NULL);
    
    /* check for trivial reject */
    if ((!(region->numRects)) || (!(other->numRects)) ||
	(!EXTENTCHECK(&region->extents, &other->extents)))
	return;
    
    miRegionOp (region, region, other, miSubtractO,
		miSubtractNonO1, (nonOverlapFunc) NULL);
    
    /*
     * Can't alter region's extents before we call miRegionOp because miRegionOp
     * depends on the extents of those regions being the unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents (region);
}

/**
 * ccm_region_xor:
 * @source1: a #CCMRegion
 * @source2: another #CCMRegion
 *
 * Sets the area of @source1 to the exclusive-OR of the areas of @source1
 * and @source2. The resulting area is the set of pixels contained in one
 * or the other of the two sources but not in both.
 **/
void
ccm_region_xor (CCMRegion *sra,
	       CCMRegion *srb)
{
    CCMRegion *trb;
    
    g_return_if_fail (sra != NULL);
    g_return_if_fail (srb != NULL);
    
    trb = ccm_region_copy (srb);
    
    ccm_region_subtract (trb, sra);
    ccm_region_subtract (sra, srb);
    
    ccm_region_union (sra,trb);
    
    ccm_region_destroy (trb);
}

/*
 * Check to see if the region is empty.  Assumes a region is passed 
 * as a parameter
 */
gboolean
ccm_region_empty (CCMRegion *r)
{
    g_return_val_if_fail (r != NULL, FALSE);
    
    if (r->numRects == 0)
	return TRUE;
    else
	return FALSE;
}

/*
 * Check to see if the region is shaped.  Assumes a region is passed 
 * as a parameter
 */
gboolean
ccm_region_shaped (CCMRegion *r)
{
    g_return_val_if_fail (r != NULL, FALSE);
    
    if (r->numRects > 1)
	return TRUE;
    else
	return FALSE;
}

/*
 *	Check to see if two regions are equal	
 */
gboolean
ccm_region_equal (CCMRegion *r1,
		 CCMRegion *r2)
{
    int i;
    
    g_return_val_if_fail (r1 != NULL, FALSE);
    g_return_val_if_fail (r2 != NULL, FALSE);
    
    if (r1->numRects != r2->numRects) return FALSE;
    else if (r1->numRects == 0) return TRUE;
    else if (r1->extents.x1 != r2->extents.x1) return FALSE;
    else if (r1->extents.x2 != r2->extents.x2) return FALSE;
    else if (r1->extents.y1 != r2->extents.y1) return FALSE;
    else if (r1->extents.y2 != r2->extents.y2) return FALSE;
    else
	for(i=0; i < r1->numRects; i++ )
	{
	    if (r1->rects[i].x1 != r2->rects[i].x1) return FALSE;
	    else if (r1->rects[i].x2 != r2->rects[i].x2) return FALSE;
	    else if (r1->rects[i].y1 != r2->rects[i].y1) return FALSE;
	    else if (r1->rects[i].y2 != r2->rects[i].y2) return FALSE;
	}
    return TRUE;
}

gboolean
ccm_region_point_in (CCMRegion *region,
		    int        x,
		    int        y)
{
    int i;
    
    g_return_val_if_fail (region != NULL, FALSE);
    
    if (region->numRects == 0)
	return FALSE;
    if (!INBOX(region->extents, x, y))
	return FALSE;
    for (i=0; i<region->numRects; i++)
    {
	if (INBOX (region->rects[i], x, y))
	    return TRUE;
    }
    return FALSE;
}

CCMOverlapType
ccm_region_rect_in (CCMRegion    *region,
		   cairo_rectangle_t *rectangle)
{
    RegionBox *pbox;
    RegionBox *pboxEnd;
    RegionBox  rect;
    RegionBox *prect = &rect;
    gboolean      partIn, partOut;
    gint rx, ry;
    
    g_return_val_if_fail (region != NULL, CCM_OVERLAP_RECTANGLE_OUT);
    g_return_val_if_fail (rectangle != NULL, CCM_OVERLAP_RECTANGLE_OUT);
    
    rx = rectangle->x;
    ry = rectangle->y;
    
    prect->x1 = rx;
    prect->y1 = ry;
    prect->x2 = rx + rectangle->width;
    prect->y2 = ry + rectangle->height;
    
    /* this is (just) a useful optimization */
    if ((region->numRects == 0) || !EXTENTCHECK (&region->extents, prect))
	return CCM_OVERLAP_RECTANGLE_OUT;
    
    partOut = FALSE;
    partIn = FALSE;
    
    /* can stop when both partOut and partIn are TRUE, or we reach prect->y2 */
    for (pbox = region->rects, pboxEnd = pbox + region->numRects;
	 pbox < pboxEnd;
	 pbox++)
    {
	
	if (pbox->y2 <= ry)
	    continue;	/* getting up to speed or skipping remainder of band */
	
	if (pbox->y1 > ry)
	{
	    partOut = TRUE;	/* missed part of rectangle above */
	    if (partIn || (pbox->y1 >= prect->y2))
		break;
	    ry = pbox->y1;	/* x guaranteed to be == prect->x1 */
	}
	
	if (pbox->x2 <= rx)
	    continue;		/* not far enough over yet */
	
	if (pbox->x1 > rx)
	{
	    partOut = TRUE;	/* missed part of rectangle to left */
	    if (partIn)
		break;
	}
	
	if (pbox->x1 < prect->x2)
	{
	    partIn = TRUE;	/* definitely overlap */
	    if (partOut)
		break;
	}
	
	if (pbox->x2 >= prect->x2)
	{
	    ry = pbox->y2;	/* finished with this band */
	    if (ry >= prect->y2)
		break;
	    rx = prect->x1;	/* reset x out to left again */
	}
	else
	{
	    /*
	     * Because boxes in a band are maximal width, if the first box
	     * to overlap the rectangle doesn't completely cover it in that
	     * band, the rectangle must be partially out, since some of it
	     * will be uncovered in that band. partIn will have been set true
	     * by now...
	     */
	    break;
	}
	
    }
    
    return (partIn ?
	    ((ry < prect->y2) ?
	     CCM_OVERLAP_RECTANGLE_PART : CCM_OVERLAP_RECTANGLE_IN) : 
	    CCM_OVERLAP_RECTANGLE_OUT);
}

void
_ccm_region_print(CCMRegion *self)
{
    int nb_rects;
    int cpt;
    cairo_rectangle_t* rects;
	cairo_rectangle_t clipbox;

    ccm_region_get_rectangles(self, &rects, &nb_rects);
    g_print("num: %d\n", nb_rects);
	ccm_region_get_clipbox(self, &clipbox);
    g_print("clipbox: %f %f %f %f\n",
	   clipbox.x, clipbox.y, clipbox.x + clipbox.width, clipbox.y + clipbox.height);
    for (cpt = 0; cpt < nb_rects; cpt++)
	g_print("\t%f %f %f %f \n",
		rects[cpt].x, rects[cpt].y, rects[cpt].x + rects[cpt].width, rects[cpt].y + rects[cpt].height);
	g_free(rects);
    g_print("\n");
 }
