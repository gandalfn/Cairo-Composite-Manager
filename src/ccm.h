/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm.h
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

#ifndef _CCM_H
#define _CCM_H

#include <cairo.h>
#include <glib.h>
#include <glib-object.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS
/********************************** Display ***********************************/
typedef struct _CCMDisplayClass CCMDisplayClass;
typedef struct _CCMDisplay CCMDisplay;
/******************************************************************************/

/********************************** Screen ************************************/
typedef struct _CCMScreenClass CCMScreenClass;
typedef struct _CCMScreen CCMScreen;
/******************************************************************************/

/****************************** Drawable **************************************/
typedef struct _CCMDrawableClass CCMDrawableClass;
typedef struct _CCMDrawable CCMDrawable;
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
    CCM_PROPERTY_STATE,
    CCM_PROPERTY_FRAME_EXTENDS
} CCMPropertyType;

typedef struct _CCMWindowClass CCMWindowClass;
typedef struct _CCMWindow CCMWindow;
/******************************************************************************/

/******************************** Pixmap **************************************/
typedef struct _CCMPixmapClass CCMPixmapClass;
typedef struct _CCMPixmap CCMPixmap;
/******************************************************************************/

/******************************** Region **************************************/
typedef struct _CCMRegion CCMRegion;
typedef struct _CCMRegionBox CCMRegionBox;

struct _CCMRegionBox
{
    short x1, y1, x2, y2;
};

CCMRegion*    ccm_region_new              (void);
CCMRegion*    ccm_region_copy             (CCMRegion* self);
CCMRegion*    ccm_region_rectangle        (cairo_rectangle_t* rectangle);
CCMRegion*    ccm_region_xrectangle       (XRectangle* rectangle);
CCMRegion*    ccm_region_create           (int x, int y, int width, int height);
void          ccm_region_destroy          (CCMRegion* self);
void          ccm_region_get_clipbox      (CCMRegion* self,
                                           cairo_rectangle_t* clipbox);
void          ccm_region_get_rectangles   (CCMRegion* self,
                                           cairo_rectangle_t** rectangles,
                                           gint* n_rectangles);
CCMRegionBox* ccm_region_get_boxes        (CCMRegion* self, gint* n_box);
void          ccm_region_get_xrectangles  (CCMRegion* self,
                                           XRectangle** rectangles,
                                           gint* n_rectangles);
gboolean      ccm_region_empty            (CCMRegion* self);
void          ccm_region_offset           (CCMRegion* self, int dx, int dy);
void          ccm_region_resize           (CCMRegion* self,
                                           int width, int height);
void          ccm_region_scale            (CCMRegion* self,
                                           gdouble scale_width,
                                           gdouble scale_height);
void          ccm_region_union_with_rect  (CCMRegion* self,
                                           cairo_rectangle_t* rect);
void          ccm_region_union_with_xrect (CCMRegion* self,
                                           XRectangle* rect);
void          ccm_region_intersect        (CCMRegion* self,
                                           CCMRegion* other);
void          ccm_region_union            (CCMRegion* self,
                                           CCMRegion* other);
void          ccm_region_subtract         (CCMRegion* self,
                                           CCMRegion* other);
void          ccm_region_transform        (CCMRegion* self,
                                           cairo_matrix_t* matrix);
gboolean      ccm_region_transform_invert (CCMRegion* self,
                                           cairo_matrix_t* matrix);
void          ccm_region_device_transform (CCMRegion* self,
                                           cairo_matrix_t* matrix);
gboolean      ccm_region_device_transform_invert (CCMRegion* self,
                                                  cairo_matrix_t* matrix);
gboolean      ccm_region_point_in         (CCMRegion* self, int x, int y);
gboolean      ccm_region_is_shaped        (CCMRegion* self);

#define cairo_rectangles_free(c, nb) ({if (c) g_slice_free1(sizeof(cairo_rectangle_t) * nb, c); })
#define x_rectangles_free(c, nb) ({if (c) g_slice_free1(sizeof(XRectangle) * nb, c);})

/******************************************************************************/

/********************************** Display***********************************/
typedef void (*CCMDamageCallbackFunc) (CCMDrawable* drawable, CCMRegion* damaged);

G_GNUC_PURE CCMDisplay* ccm_display_get_default     ();
CCMDisplay*             ccm_display_new             (gchar* display);
G_GNUC_PURE Display*    ccm_display_get_xdisplay    (CCMDisplay* self);
G_GNUC_PURE CCMScreen*  ccm_display_get_screen      (CCMDisplay* self,
                                                     guint number);
G_GNUC_PURE int         ccm_display_get_shape_notify_event_type (CCMDisplay* self);
void                    ccm_display_flush           (CCMDisplay* self);
void                    ccm_display_sync            (CCMDisplay* self);
void                    ccm_display_grab            (CCMDisplay* self);
void                    ccm_display_ungrab          (CCMDisplay* self);
void                    ccm_display_trap_error      (CCMDisplay* self);
gint                    ccm_display_pop_error       (CCMDisplay* self);
gboolean                ccm_display_report_device_event (CCMDisplay* self,
                                                         CCMScreen* screen,
                                                         gboolean report);
guint32                 ccm_display_register_damage   (CCMDisplay* self,
                                                       CCMDrawable* drawable,
                                                       CCMDamageCallbackFunc func);
void                    ccm_display_unregister_damage (CCMDisplay* self,
                                                       guint32 damage);
/******************************************************************************/

/********************************** Screen************************************/
CCMScreen*              ccm_screen_new                  (CCMDisplay* display,
                                                         guint number);
G_GNUC_PURE CCMDisplay* ccm_screen_get_display          (CCMScreen* self);
G_GNUC_PURE Screen*     ccm_screen_get_xscreen          (CCMScreen* self);
G_GNUC_PURE guint       ccm_screen_get_number           (CCMScreen* self);
G_GNUC_PURE guint       ccm_screen_get_refresh_rate     (CCMScreen* self);
G_GNUC_PURE CCMWindow*  ccm_screen_get_root_window      (CCMScreen* self);
G_GNUC_PURE CCMWindow*  ccm_screen_get_overlay_window   (CCMScreen* self);
gboolean                ccm_screen_add_window           (CCMScreen* self,
                                                         CCMWindow* window);
void                    ccm_screen_remove_window        (CCMScreen* self,
                                                         CCMWindow* window);
void                    ccm_screen_damage               (CCMScreen* self);
void                    ccm_screen_damage_region        (CCMScreen* self,
                                                         const CCMRegion* region);
void                    ccm_screen_undamage_region      (CCMScreen* self,
                                                         const CCMRegion* area);
G_GNUC_PURE GList*      ccm_screen_get_windows          (CCMScreen* self);
G_GNUC_PURE CCMRegion*  ccm_screen_get_damaged          (CCMScreen* self);
void                    ccm_screen_add_damaged_region   (CCMScreen* self,
                                                         CCMRegion* region);
void                    ccm_screen_remove_damaged_region(CCMScreen* self,
                                                         CCMRegion* region);
CCMWindow*              ccm_screen_find_window          (CCMScreen* self,
                                                         Window xwindow);
CCMWindow*              ccm_screen_find_window_or_child (CCMScreen* self,
                                                         Window xwindow);
CCMWindow*              ccm_screen_find_window_at_pos   (CCMScreen* self,
                                                         int x, int y);
gboolean                ccm_screen_query_pointer        (CCMScreen* self,
                                                         CCMWindow** below,
                                                         gint* x, gint* y);
Visual*                 ccm_screen_get_visual_for_depth (CCMScreen* self,
                                                         int depth);
G_GNUC_PURE CCMWindow*  ccm_screen_get_active_window    (CCMScreen* self);
G_GNUC_PURE CCMRegion*  ccm_screen_get_geometry         (CCMScreen* self);
void                    ccm_screen_wait_vblank          (CCMScreen* self);
G_GNUC_PURE gboolean    ccm_screen_get_redirect_input   (CCMScreen* self);
void                    ccm_screen_set_redirect_input   (CCMScreen* self, gboolean redirect_input);
G_GNUC_PURE CCMRegion* ccm_screen_get_primary_geometry  (CCMScreen * self);
/******************************************************************************/

/****************************** Drawable**************************************/
G_GNUC_PURE CCMScreen*  ccm_drawable_get_screen         (CCMDrawable* self);
G_GNUC_PURE CCMDisplay* ccm_drawable_get_display        (CCMDrawable* self);
G_GNUC_PURE XID         ccm_drawable_get_xid            (CCMDrawable* self);
G_GNUC_PURE Visual*     ccm_drawable_get_visual         (CCMDrawable* self);
void                    ccm_drawable_set_visual         (CCMDrawable* self,
                                                         Visual* visual);
cairo_format_t          ccm_drawable_get_format         (CCMDrawable* self);
G_GNUC_PURE guint       ccm_drawable_get_depth          (CCMDrawable* self);
void                    ccm_drawable_set_depth          (CCMDrawable* self,
                                                         guint depth);
void                    ccm_drawable_query_geometry     (CCMDrawable* self);
G_GNUC_PURE const CCMRegion* ccm_drawable_get_geometry  (CCMDrawable* self);
void                    ccm_drawable_set_geometry       (CCMDrawable* self,
                                                         CCMRegion* geometry);
G_GNUC_PURE const CCMRegion* ccm_drawable_get_device_geometry (CCMDrawable* self);
gboolean                ccm_drawable_get_geometry_clipbox (CCMDrawable* self,
                                                           cairo_rectangle_t* area);
gboolean                ccm_drawable_get_device_geometry_clipbox (CCMDrawable* self,
                                                                  cairo_rectangle_t* area);
gboolean                ccm_drawable_is_damaged         (CCMDrawable* self);
void                    ccm_drawable_damage_region      (CCMDrawable* self,
                                                         const CCMRegion* area);
void                    ccm_drawable_damage_region_silently (CCMDrawable* self,
                                                             const CCMRegion* area);
void                    ccm_drawable_damage             (CCMDrawable* self);
void                    ccm_drawable_undamage_region    (CCMDrawable* self,
                                                         CCMRegion* region);
void                    ccm_drawable_repair             (CCMDrawable* self);
void                    ccm_drawable_move               (CCMDrawable* self,
                                                         int x, int y);
void                    ccm_drawable_resize             (CCMDrawable* self,
                                                         int width, int height);
void                    ccm_drawable_flush              (CCMDrawable* self);
void                    ccm_drawable_flush_region       (CCMDrawable* self,
                                                         CCMRegion* region);
cairo_surface_t*        ccm_drawable_get_surface        (CCMDrawable* self);
cairo_t*                ccm_drawable_create_context     (CCMDrawable* self);
cairo_path_t*           ccm_drawable_get_geometry_path (CCMDrawable* self,
                                                        cairo_t* context);
void                    ccm_drawable_get_damage_path    (CCMDrawable* self,
                                                         cairo_t* context);
void                    ccm_drawable_push_matrix        (CCMDrawable* self,
                                                         gchar* key,
                                                         cairo_matrix_t* matrix);
void                    ccm_drawable_pop_matrix         (CCMDrawable* self,
                                                         gchar* key);
cairo_matrix_t          ccm_drawable_get_transform      (CCMDrawable* self);
G_GNUC_PURE const CCMRegion* ccm_drawable_get_damaged   (CCMDrawable* self);
/******************************************************************************/

/******************************** Window**************************************/
CCMWindow*              ccm_window_new                  (CCMScreen* screen,
                                                         Window xwindow);
CCMWindow*              ccm_window_new_unmanaged        (CCMScreen* screen,
                                                         Window xwindow);
G_GNUC_PURE gboolean    ccm_window_is_viewable          (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_is_input_only        (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_is_managed           (CCMWindow* self);
void                    ccm_window_make_output_only     (CCMWindow* self);
void                    ccm_window_make_input_output    (CCMWindow* self);
void                    ccm_window_redirect             (CCMWindow* self);
void                    ccm_window_redirect_subwindows  (CCMWindow* self);
void                    ccm_window_unredirect           (CCMWindow* self);
void                    ccm_window_unredirect_subwindows(CCMWindow* self);
void                    ccm_window_redirect_input       (CCMWindow* self);
void                    ccm_window_unredirect_input     (CCMWindow* self);
G_GNUC_PURE CCMPixmap*  ccm_window_get_pixmap           (CCMWindow* self);
CCMPixmap*              ccm_window_create_pixmap        (CCMWindow* self,
                                                         int width, int height,
                                                         int depth);
gboolean                ccm_window_paint                (CCMWindow* self,
                                                         cairo_t* ctx);
void                    ccm_window_map                  (CCMWindow* self);
void                    ccm_window_unmap                (CCMWindow* self);
void                    ccm_window_query_opacity        (CCMWindow* self,
                                                         gboolean deleted);
void                    ccm_window_query_transient_for  (CCMWindow* self);
void                    ccm_window_query_wm_hints       (CCMWindow* self);
void                    ccm_window_query_hint_type      (CCMWindow* self);
G_GNUC_PURE CCMWindowType ccm_window_get_hint_type      (CCMWindow* self);
const gchar*            ccm_window_get_name             (CCMWindow* self);
void                    ccm_window_set_alpha            (CCMWindow* self);
void                    ccm_window_set_opaque           (CCMWindow* self);
G_GNUC_PURE const CCMRegion* ccm_window_get_opaque_region (CCMWindow* self);
gboolean                ccm_window_get_opaque_clipbox   (CCMWindow* self,
                                                         cairo_rectangle_t* clipbox);
void                    ccm_window_set_opaque_region    (CCMWindow* self,
                                                         const CCMRegion* region);
G_GNUC_PURE gfloat      ccm_window_get_opacity          (CCMWindow* self);
void                    ccm_window_set_opacity          (CCMWindow* self,
                                                         gfloat opacity);
void                    ccm_window_query_state          (CCMWindow* self);
gboolean                ccm_window_set_state            (CCMWindow* self,
                                                         Atom state_atom);
void                    ccm_window_unset_state          (CCMWindow* self,
                                                         Atom state_atom);
void                    ccm_window_switch_state         (CCMWindow* self,
                                                         Atom state_atom);
G_GNUC_PURE gboolean    ccm_window_is_shaded            (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_is_fullscreen        (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_is_maximized         (CCMWindow* self);
void                    ccm_window_query_mwm_hints      (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_is_decorated         (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_skip_taskbar         (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_skip_pager           (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_keep_above           (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_keep_below           (CCMWindow* self);
G_GNUC_PURE const CCMWindow* ccm_window_transient_for   (CCMWindow* self);
G_GNUC_PURE const CCMWindow* ccm_window_get_group_leader(CCMWindow* self);
const cairo_rectangle_t*ccm_window_get_area             (CCMWindow* self);
CCMRegion*              ccm_window_get_area_geometry    (CCMWindow* self);
void                    ccm_window_query_frame_extends  (CCMWindow* self);
void                    ccm_window_get_frame_extends    (CCMWindow* self,
                                                         int*left_frame,
                                                         int*right_frame,
                                                         int*top_frame,
                                                         int*bottom_frame);
gboolean                ccm_window_transform            (CCMWindow* self,
                                                         cairo_t* ctx);
guint32*                ccm_window_get_property         (CCMWindow* self,
                                                         Atom property_atom,
                                                         Atom req_type,
                                                         guint* n_items);
guint32*                ccm_window_get_child_property   (CCMWindow* self,
                                                         Atom property_atom,
                                                         Atom req_type,
                                                         guint* n_items);
Window                  ccm_window_redirect_event       (CCMWindow* self,
                                                         XEvent* event,
                                                         Window over);
void                    ccm_window_activate             (CCMWindow* self,
                                                         Time timestamp);
G_GNUC_PURE GSList*     ccm_window_get_transients       (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_get_no_undamage_sibling (CCMWindow* self);
void                    ccm_window_set_no_undamage_sibling (CCMWindow* self,
                                                            gboolean no_undamage);
G_GNUC_PURE gboolean    ccm_window_has_redirect_input   (CCMWindow* self);
G_GNUC_PURE gboolean    ccm_window_get_redirect         (CCMWindow* self);
void                    ccm_window_set_redirect         (CCMWindow* self,
                                                         gboolean redirect);
G_GNUC_PURE cairo_surface_t* ccm_window_get_mask        (CCMWindow* self);
void                    ccm_window_set_mask             (CCMWindow* self,
                                                         cairo_surface_t* mask);
/******************************************************************************/

G_END_DECLS
#endif                          /* _CCM_H*/
