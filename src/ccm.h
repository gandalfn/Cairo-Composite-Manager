/***************************************************************************
 *            ccm.h
 *
 *  Sun Jul 29 22:20:07 2007
 *  Copyright  2007  Nicolas Bruguier
 *  <gandalfn@club-internet.fr>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with main.c; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef _CCM_H
#define _CCM_H

#include <cairo.h>
#include <glib.h>
#include <glib-object.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

/********************************** Display ***********************************/
typedef struct _CCMDisplayClass  CCMDisplayClass;
typedef struct _CCMDisplay 		 CCMDisplay;
/******************************************************************************/

/********************************** Screen ************************************/
typedef struct _CCMScreenClass 	 CCMScreenClass;
typedef struct _CCMScreen 		 CCMScreen;
/******************************************************************************/

/****************************** Drawable **************************************/
typedef struct _CCMDrawableClass CCMDrawableClass;
typedef struct _CCMDrawable 	 CCMDrawable;
/******************************************************************************/

/******************************** Window **************************************/
typedef enum 
{
	CCM_WINDOW_TYPE_UNKNOWN,
	CCM_WINDOW_TYPE_DESKTOP,
	CCM_WINDOW_TYPE_NORMAL,
    CCM_WINDOW_TYPE_DIALOG,
	CCM_WINDOW_TYPE_SPLASH,
	CCM_WINDOW_TYPE_UTILITY,
	CCM_WINDOW_TYPE_DND,
    CCM_WINDOW_TYPE_TOOLTIP,
	CCM_WINDOW_TYPE_NOTIFICATION,
	CCM_WINDOW_TYPE_TOOLBAR,
	CCM_WINDOW_TYPE_COMBO,
    CCM_WINDOW_TYPE_DROPDOWN_MENU,
	CCM_WINDOW_TYPE_POPUP_MENU,
	CCM_WINDOW_TYPE_MENU,
    CCM_WINDOW_TYPE_DOCK
} CCMWindowType;

typedef enum 
{
	CCM_PROPERTY_HINT_TYPE,
	CCM_PROPERTY_TRANSIENT,
	CCM_PROPERTY_MWM_HINTS,
    CCM_PROPERTY_WM_HINTS,
    CCM_PROPERTY_OPACITY,
	CCM_PROPERTY_STATE
} CCMPropertyType;

typedef struct _CCMWindowClass 	 CCMWindowClass;
typedef struct _CCMWindow 		 CCMWindow;
/******************************************************************************/

/******************************** Pixmap **************************************/
typedef struct _CCMPixmapClass   CCMPixmapClass;
typedef struct _CCMPixmap        CCMPixmap;
/******************************************************************************/

/******************************** Region **************************************/
typedef struct _CCMRegion 		 CCMRegion;

/* Types of overlapping between a rectangle and a region
 * CCM_OVERLAP_RECTANGLE_IN: rectangle is in region
 * CCM_OVERLAP_RECTANGLE_OUT: rectangle in not in region
 * CCM_OVERLAP_RECTANGLE_PART: rectangle in partially in region
 */
typedef enum
{
    CCM_OVERLAP_RECTANGLE_IN,
    CCM_OVERLAP_RECTANGLE_OUT,
    CCM_OVERLAP_RECTANGLE_PART
} CCMOverlapType;

CCMRegion*	 	ccm_region_new             (void);
CCMRegion*   	ccm_region_copy            (CCMRegion     *region);
CCMRegion*   	ccm_region_rectangle       (cairo_rectangle_t  *rectangle);
CCMRegion*		ccm_region_xrectangle 	   (XRectangle *rectangle);
void          	ccm_region_destroy         (CCMRegion     *region);
void          	ccm_region_get_clipbox     (CCMRegion     *region,
											cairo_rectangle_t  *rectangle);
void         	ccm_region_get_rectangles  (CCMRegion     *region,
											cairo_rectangle_t **rectangles,
											gint         *n_rectangles);
void            ccm_region_get_xrectangles (CCMRegion     *region,
                                            XRectangle **rectangles,
                                            gint          *n_rectangles);
gboolean      	ccm_region_empty           (CCMRegion     *region);
gboolean        ccm_region_shaped          (CCMRegion     *region);
gboolean      	ccm_region_equal           (CCMRegion     *region1,
											CCMRegion     *region2);
gboolean      	ccm_region_point_in        (CCMRegion     *region,
											int           x,
											int           y);
CCMOverlapType 	ccm_region_rect_in         (CCMRegion     *region,
											cairo_rectangle_t  *rect);
void          	ccm_region_offset          (CCMRegion     *region,
											gint          dx,
											gint          dy);
void			ccm_region_resize 			(CCMRegion*   region,
											 gint         width,
											 gint         height);
void            ccm_region_scale            (CCMRegion*   region,
                                             gdouble      scale_width,
                                             gdouble      scale_height);
void          	ccm_region_shrink           (CCMRegion     *region,
											 gint          dx,
											 gint          dy);
void          	ccm_region_union_with_rect  (CCMRegion     *region,
											 cairo_rectangle_t  *rect);
void			ccm_region_union_with_xrect (CCMRegion    *region,
											 XRectangle   *rect);
void          	ccm_region_intersect        (CCMRegion     *source1,
											 CCMRegion     *source2);
void          	ccm_region_union            (CCMRegion     *source1,
											 CCMRegion     *source2);	
void          	ccm_region_subtract         (CCMRegion     *source1,
											 CCMRegion     *source2);
void          	ccm_region_xor              (CCMRegion     *source1,
											 CCMRegion     *source2);
void            ccm_region_transform        (CCMRegion *region, 
                                             cairo_matrix_t* matrix);
void            ccm_region_device_transform (CCMRegion *region, 
                                             cairo_matrix_t* matrix);
/******************************************************************************/

/********************************** Display ***********************************/
CCMDisplay* 	ccm_display_new        		(gchar* display);
Display*        ccm_display_get_xdisplay    (CCMDisplay* self);
CCMScreen*	 	ccm_display_get_screen 		(CCMDisplay* self, 
											 guint number);
int				ccm_display_get_shape_notify_event_type(CCMDisplay* self);
void			ccm_display_sync			(CCMDisplay* self);
void			ccm_display_grab			(CCMDisplay* self);
void			ccm_display_ungrab			(CCMDisplay* self);
void		    ccm_display_trap_error		(CCMDisplay* self);
gint		    ccm_display_pop_error		(CCMDisplay* self);

/******************************************************************************/

/********************************** Screen ************************************/
CCMScreen* 		ccm_screen_new					(CCMDisplay* display, 
										 	 	 guint number);
CCMDisplay*		ccm_screen_get_display			(CCMScreen* self);
Screen*         ccm_screen_get_xscreen          (CCMScreen* self);
guint           ccm_screen_get_number           (CCMScreen* self);
CCMWindow* 		ccm_screen_get_root_window		(CCMScreen* self);
CCMWindow* 		ccm_screen_get_overlay_window 	(CCMScreen* self);
gboolean		ccm_screen_add_window			(CCMScreen* self, 
											 	 CCMWindow* window);
void			ccm_screen_remove_window		(CCMScreen* self, 
												 CCMWindow* window);
void            ccm_screen_damage               (CCMScreen* self);
void            ccm_screen_damage_region        (CCMScreen* self, 
                                                 const CCMRegion* region);
void            ccm_screen_undamage_region      (CCMScreen* self, 
                                                 const CCMRegion* area);
GList*          ccm_screen_get_windows          (CCMScreen *self);
CCMRegion*      ccm_screen_get_damaged          (CCMScreen *self);
void            ccm_screen_add_damaged_region   (CCMScreen *self, 
                                                 CCMRegion* region);
void            ccm_screen_remove_damaged_region(CCMScreen *self, 
                                                 CCMRegion* region);
CCMWindow*      ccm_screen_find_window          (CCMScreen* self, 
                                                 Window xwindow);
CCMWindow*      ccm_screen_find_window_or_child (CCMScreen* self, 
                                                 Window xwindow);
void            ccm_screen_activate_window      (CCMScreen* self, 
                                                 CCMWindow* window, 
                                                 Time timestamp);
gboolean        ccm_screen_query_pointer        (CCMScreen* self, 
                                                 CCMWindow** below, 
                                                 gint *x, gint *y);
void			ccm_screen_motion_input_redirect(CCMScreen* self, 
												 cairo_matrix_t* transform,
												 int x, int y);
void			ccm_screen_button_input_redirect(CCMScreen* self, 
												 cairo_matrix_t* transform,
												 int x, int y, int button, 
												 gboolean pressed);
/******************************************************************************/

/****************************** Drawable **************************************/
CCMScreen*		 ccm_drawable_get_screen			(CCMDrawable* self);
CCMDisplay*		 ccm_drawable_get_display			(CCMDrawable* self);
XID				 ccm_drawable_get_xid				(CCMDrawable* self);
Visual*		     ccm_drawable_get_visual			(CCMDrawable* self);
cairo_format_t   ccm_drawable_get_format            (CCMDrawable* self);
guint            ccm_drawable_get_depth             (CCMDrawable* self);
void     		 ccm_drawable_query_geometry 		(CCMDrawable* self);
const CCMRegion* ccm_drawable_get_geometry			(CCMDrawable* self);
gboolean 		 ccm_drawable_get_geometry_clipbox	(CCMDrawable* self, 
													 cairo_rectangle_t* area);
gboolean		 ccm_drawable_is_damaged			(CCMDrawable* self);
void 			 ccm_drawable_damage_region 		(CCMDrawable* self, 
													 const CCMRegion* area);
void 			 ccm_drawable_damage_region_silently(CCMDrawable* self, 
													 const CCMRegion* area);
void 			 ccm_drawable_damage				(CCMDrawable* self);
void			 ccm_drawable_undamage_region		(CCMDrawable* self, 
													 CCMRegion* region);
void			 ccm_drawable_repair				(CCMDrawable* self);
void			 ccm_drawable_move					(CCMDrawable* self, 
													 int x, int y);
void			 ccm_drawable_resize				(CCMDrawable* self, 
													 int width, int height);
void			 ccm_drawable_flush					(CCMDrawable* self);
void             ccm_drawable_flush_region          (CCMDrawable* self, 
                                                     CCMRegion* region);
cairo_surface_t* ccm_drawable_get_surface			(CCMDrawable* self);
cairo_t*		 ccm_drawable_create_context		(CCMDrawable* self);
cairo_path_t*	 ccm_drawable_get_geometry_path		(CCMDrawable* self, 
													 cairo_t* context);
cairo_path_t*	 ccm_drawable_get_damage_path		(CCMDrawable* self, 
													 cairo_t* context);
/******************************************************************************/

/******************************** Window **************************************/
CCMWindow*  	 ccm_window_new          			(CCMScreen* screen, 
											 		 Window xwindow);
gboolean         ccm_window_is_viewable             (CCMWindow* self);
gboolean         ccm_window_is_input_only           (CCMWindow* self);
gboolean		 ccm_window_is_managed				(CCMWindow* self);
void			 ccm_window_make_output_only		(CCMWindow* self);
void             ccm_window_make_input_output       (CCMWindow* self);
void			 ccm_window_redirect 				(CCMWindow* self);
void			 ccm_window_redirect_subwindows		(CCMWindow* self);
void			 ccm_window_unredirect 				(CCMWindow* self);
void			 ccm_window_unredirect_subwindows 	(CCMWindow* self);
CCMPixmap*		 ccm_window_get_pixmap				(CCMWindow* self);
CCMPixmap*       ccm_window_create_pixmap           (CCMWindow* self, 
                                                     int width, int height,
                                                     int depth);
gboolean		 ccm_window_paint 					(CCMWindow* self,
                                                     cairo_t* ctx,
                                                     gboolean buffered);
void			 ccm_window_map						(CCMWindow* self);
void			 ccm_window_unmap					(CCMWindow* self);
void 			 ccm_window_query_opacity			(CCMWindow* self,
                                                     gboolean deleted);
void             ccm_window_query_transient_for     (CCMWindow* self);
void             ccm_window_query_wm_hints          (CCMWindow* self);
void             ccm_window_query_hint_type         (CCMWindow* self);
CCMWindowType	 ccm_window_get_hint_type			(CCMWindow* self);
const gchar*	 ccm_window_get_name				(CCMWindow* self);
void             ccm_window_set_alpha               (CCMWindow* self);
void             ccm_window_set_opaque              (CCMWindow* self);
const CCMRegion* ccm_window_get_opaque_region       (CCMWindow* self);
gboolean         ccm_window_get_opaque_clipbox      (CCMWindow* self, 
                                                     cairo_rectangle_t* clipbox);
void             ccm_window_set_opaque_region       (CCMWindow* self, 
                                                     const CCMRegion* region);
gfloat			ccm_window_get_opacity 				(CCMWindow* self);
void			ccm_window_set_opacity 				(CCMWindow* self, 
													 gfloat opacity);
void            ccm_window_query_state              (CCMWindow* self);
gboolean        ccm_window_set_state                (CCMWindow* self, 
                                                     Atom state_atom);
void            ccm_window_unset_state              (CCMWindow* self, 
                                                     Atom state_atom);
void            ccm_window_switch_state             (CCMWindow* self, 
                                                     Atom state_atom);
gboolean        ccm_window_is_shaded                (CCMWindow* self);
gboolean        ccm_window_is_fullscreen            (CCMWindow* self);
void            ccm_window_query_mwm_hints          (CCMWindow* self);
gboolean        ccm_window_is_decorated             (CCMWindow* self);
gboolean        ccm_window_skip_taskbar             (CCMWindow* self);
gboolean        ccm_window_skip_pager               (CCMWindow* self);
gboolean        ccm_window_keep_above               (CCMWindow* self);
gboolean        ccm_window_keep_below               (CCMWindow* self);
CCMWindow*      ccm_window_transient_for            (CCMWindow* self);
CCMWindow*      ccm_window_get_group_leader         (CCMWindow* self);
const cairo_rectangle_t* ccm_window_get_area        (CCMWindow* self);
gboolean        ccm_window_get_frame_extends        (CCMWindow* self, 
                                                     int* left_frame, 
                                                     int* right_frame, 
                                                     int* top_frame, 
                                                     int* bottom_frame);
void            ccm_window_init_transfrom           (CCMWindow* self);
void            ccm_window_get_transform            (CCMWindow* self, 
                                                     cairo_matrix_t* matrix);
void            ccm_window_set_transform            (CCMWindow* self, 
                                                     cairo_matrix_t* matrix);
gboolean        ccm_window_transform                (CCMWindow* self, 
                                                     cairo_t* ctx, 
                                                     gboolean y_invert);
/******************************************************************************/

G_END_DECLS

#endif /* _CCM_H */

 
