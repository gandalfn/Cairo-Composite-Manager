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
#include <math.h>
#include <string.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xregion.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "ccm-debug.h"
#include "ccm-window.h"
#include "ccm-window-backend.h"
#include "ccm-window-plugin.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-pixmap.h"
#include "ccm-pixmap-backend.h"
#include "ccm-property-async.h"
#include "ccm-object.h"

#define MWM_HINTS_DECORATIONS (1L << 1)

typedef struct
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MotifWmHints;

static GQuark CCMWindowPixmapQuark;

static void ccm_window_iface_init (CCMWindowPluginClass * iface);
static void ccm_window_query_geometry (CCMDrawable * drawable);
static void ccm_window_move (CCMDrawable * drawable, int x, int y);
static void ccm_window_resize (CCMDrawable * drawable, int width, int height);

static CCMRegion *impl_ccm_window_query_geometry (CCMWindowPlugin * plugin,
                                                  CCMWindow * self);
static gboolean impl_ccm_window_paint (CCMWindowPlugin * plugin,
                                       CCMWindow * self, cairo_t * context,
                                       cairo_surface_t * surface,
                                       gboolean y_invert);
static void impl_ccm_window_map (CCMWindowPlugin * plugin, CCMWindow * self);
static void impl_ccm_window_unmap (CCMWindowPlugin * plugin, CCMWindow * self);
static void impl_ccm_window_query_opacity (CCMWindowPlugin * plugin,
                                           CCMWindow * self);
static void impl_ccm_window_move (CCMWindowPlugin * plugin, CCMWindow * self,
                                  int x, int y);
static void impl_ccm_window_resize (CCMWindowPlugin * plugin, CCMWindow * self,
                                    int width, int height);
static void impl_ccm_window_set_opaque_region (CCMWindowPlugin * plugin,
                                               CCMWindow * self,
                                               const CCMRegion * area);
static void impl_ccm_window_get_origin (CCMWindowPlugin * plugin,
                                        CCMWindow * self, int *x, int *y);
static CCMPixmap *impl_ccm_window_get_pixmap (CCMWindowPlugin * plugin,
                                              CCMWindow * self);
static void ccm_window_on_property_async_error (CCMWindow * self,
                                                CCMPropertyASync * prop);
static void ccm_window_get_property_async (CCMWindow * self, Atom property_atom,
                                           Atom req_type, long length);
static void ccm_window_on_get_property_async (CCMWindow * self, guint n_items,
                                              gchar * result,
                                              CCMPropertyASync * property);
static void ccm_window_on_plugins_changed (CCMWindow * self,
                                           CCMScreen * screen);
static void ccm_window_on_transient_transform_changed (CCMWindow * self,
                                                       GParamSpec * pspec,
                                                       CCMWindow * transient);

enum
{
    PROP_0,
    PROP_CHILD,
    PROP_NO_UNDAMAGE_SIBLING,
    PROP_INPUT,
    PROP_REDIRECT,
    PROP_BLOCK_MOUSE_REDIRECT_EVENT,
    PROP_IMAGE,
    PROP_MASK,
    PROP_MASK_WIDTH,
    PROP_MASK_HEIGHT,
    PROP_MASK_Y_INVERT
};

enum
{
    PROPERTY_CHANGED,
    OPACITY_CHANGED,
    REDIRECT_INPUT,
    ERROR,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

CCM_DEFINE_TYPE_EXTENDED (CCMWindow, ccm_window, CCM_TYPE_DRAWABLE,
                          G_IMPLEMENT_INTERFACE (CCM_TYPE_WINDOW_PLUGIN,
                                                 ccm_window_iface_init))
struct _CCMWindowPrivate
{
    CCMWindowType hint_type;
    gchar *name;
    gchar *class_name;
	
	gboolean unmanaged;

    Window root;
    Window child;
    Window transient_for;
    Window group_leader;
    Window input;

    GSList *transients;

    cairo_rectangle_t area;
    gboolean is_input_only;
    gboolean is_viewable;
    gboolean visible;
    gboolean unmap_pending;
    gboolean is_shaped;
    gboolean is_shaded;
    gboolean is_fullscreen;
    gboolean is_decorated;
    gboolean is_modal;
    gboolean skip_taskbar;
    gboolean skip_pager;
    gboolean keep_above;
    gboolean keep_below;
    gboolean override_redirect;
    int frame_left;
    int frame_right;
    int frame_top;
    int frame_bottom;
    gulong user_time;

    GSList *properties_pending;
    CCMPixmap *pixmap;
    gboolean use_pixmap_image;
    gboolean no_undamage_sibling;
    gboolean redirect;
    gboolean block_mouse_redirect_event;

    CCMWindowPlugin *plugin;

    CCMRegion *opaque;
    CCMRegion *orig_opaque;
    double opacity;
    cairo_surface_t *mask;
    gint mask_width;
    gint mask_height;
    gboolean mask_y_invert;

    gulong id_plugins_changed;
    gulong id_transient_transform_changed;
};

#define CCM_WINDOW_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW, CCMWindowPrivate))

static void
ccm_window_set_gobject_property (GObject * object, guint prop_id,
                                 const GValue * value, GParamSpec * pspec)
{
    CCMWindowPrivate *priv = CCM_WINDOW_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_CHILD:
            priv->child = (Window) g_value_get_pointer (value);
            break;
        case PROP_IMAGE:
            {
                gboolean use_pixmap_image = g_value_get_boolean (value);
                if (use_pixmap_image != priv->use_pixmap_image)
                {
                    priv->use_pixmap_image = use_pixmap_image;
                    if (priv->pixmap)
                    {
                        g_object_unref (priv->pixmap);
                        priv->pixmap = NULL;
                    }
                }
                break;
            }
        case PROP_NO_UNDAMAGE_SIBLING:
            {
                priv->no_undamage_sibling = g_value_get_boolean (value);
                break;
            }
        case PROP_REDIRECT:
            {
                priv->redirect = g_value_get_boolean (value);
                if (!priv->redirect)
                    ccm_window_unredirect_input (CCM_WINDOW (object));
                break;
            }
        case PROP_BLOCK_MOUSE_REDIRECT_EVENT:
            {
                priv->block_mouse_redirect_event = g_value_get_boolean (value);
                break;
            }
        case PROP_MASK:
            if (priv->mask)
                cairo_surface_destroy (priv->mask);
            priv->mask = (cairo_surface_t *) g_value_get_pointer (value);
            break;
        case PROP_MASK_WIDTH:
            priv->mask_width = g_value_get_int (value);
            break;
        case PROP_MASK_HEIGHT:
            priv->mask_height = g_value_get_int (value);
            break;
        case PROP_MASK_Y_INVERT:
            priv->mask_y_invert = g_value_get_boolean (value);
            break;
        default:
            break;
    }
}

static void
ccm_window_get_gobject_property (GObject * object, guint prop_id,
                                 GValue * value, GParamSpec * pspec)
{
    CCMWindowPrivate *priv = CCM_WINDOW_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_CHILD:
            g_value_set_pointer (value, (gpointer) priv->child);
            break;
        case PROP_NO_UNDAMAGE_SIBLING:
            g_value_set_boolean (value, priv->no_undamage_sibling);
            break;
        case PROP_INPUT:
            g_value_set_pointer (value, (gpointer) priv->input);
            break;
        case PROP_MASK:
            g_value_set_pointer (value, (gpointer) priv->mask);
            break;
        case PROP_MASK_WIDTH:
            g_value_set_int (value, priv->mask_width);
            break;
        case PROP_MASK_HEIGHT:
            g_value_set_int (value, priv->mask_height);
            break;
        case PROP_MASK_Y_INVERT:
            g_value_set_boolean (value, priv->mask_y_invert);
            break;
        default:
            break;
    }
}

static void
ccm_window_init (CCMWindow * self)
{
    self->priv = CCM_WINDOW_GET_PRIVATE (self);
    self->priv->hint_type = CCM_WINDOW_TYPE_NORMAL;
    self->priv->name = NULL;
    self->priv->class_name = NULL;
	self->priv->unmanaged = FALSE;
    self->priv->child = None;
    self->priv->transient_for = None;
    self->priv->group_leader = None;
    self->priv->input = None;
    self->priv->transients = NULL;
    self->priv->area.x = 0;
    self->priv->area.y = 0;
    self->priv->area.width = 0;
    self->priv->area.height = 0;
    self->priv->is_input_only = FALSE;
    self->priv->visible = FALSE;
    self->priv->is_viewable = FALSE;
    self->priv->unmap_pending = FALSE;
    self->priv->is_shaped = FALSE;
    self->priv->is_shaded = FALSE;
    self->priv->is_fullscreen = FALSE;
    self->priv->is_decorated = TRUE;
    self->priv->is_modal = FALSE;
    self->priv->skip_taskbar = FALSE;
    self->priv->skip_pager = FALSE;
    self->priv->keep_above = FALSE;
    self->priv->keep_below = FALSE;
    self->priv->override_redirect = FALSE;
    self->priv->opaque = NULL;
    self->priv->orig_opaque = NULL;
    self->priv->opacity = 1.0f;
    self->priv->frame_left = 0;
    self->priv->frame_right = 0;
    self->priv->frame_top = 0;
    self->priv->frame_bottom = 0;
    self->priv->properties_pending = NULL;
    self->priv->pixmap = NULL;
    self->priv->use_pixmap_image = FALSE;
    self->priv->no_undamage_sibling = FALSE;
    self->priv->redirect = TRUE;
    self->priv->block_mouse_redirect_event = FALSE;
    self->priv->plugin = NULL;
    self->priv->mask = NULL;
    self->priv->mask_width = 0;
    self->priv->mask_height = 0;
    self->priv->mask_y_invert = FALSE;
    self->priv->id_plugins_changed = 0;
    self->priv->id_transient_transform_changed = 0;
}

static void
ccm_window_finalize (GObject * object)
{
    CCMWindow *self = CCM_WINDOW (object);
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
	GSList* item;
	
    ccm_debug_window (self, "FINALIZE");
    if (CCM_IS_SCREEN (screen) && G_OBJECT (screen)->ref_count
        && self->priv->id_plugins_changed)
        g_signal_handler_disconnect (screen, self->priv->id_plugins_changed);
    ccm_window_unredirect_input (self);

    for (item = self->priv->transients; item; item = item->next)
    {
		CCMWindow* window = item->data;
		ccm_drawable_pop_matrix (CCM_DRAWABLE (window), "CCMWindowTranslate");
        ccm_drawable_pop_matrix (CCM_DRAWABLE (window), "CCMWindowTransient");
		window->priv->id_transient_transform_changed = 0;
    }
	g_slist_free(self->priv->transients);
	
    if (self->priv->transient_for)
    {
        CCMWindow *transient = ccm_window_transient_for (self);

        if (transient && self->priv->id_transient_transform_changed)
        {
            g_signal_handler_disconnect (transient,
                                         self->priv->id_transient_transform_changed);
			self->priv->id_transient_transform_changed = 0;
			transient->priv->transients =
                g_slist_remove (transient->priv->transients, self);
        }
    }
    if (self->priv->mask)
    {
        cairo_surface_destroy (self->priv->mask);
        self->priv->mask = NULL;
    }
    if (self->priv->opaque)
    {
        ccm_region_destroy (self->priv->opaque);
        self->priv->opaque = NULL;
    }
    if (self->priv->orig_opaque)
    {
        ccm_region_destroy (self->priv->orig_opaque);
        self->priv->orig_opaque = NULL;
    }
    if (self->priv->pixmap)
    {
        g_object_unref (self->priv->pixmap);
        self->priv->pixmap = NULL;
    }
    if (self->priv->name)
    {
        g_free (self->priv->name);
        self->priv->name = NULL;
    }
    if (self->priv->plugin && CCM_IS_PLUGIN (self->priv->plugin))
    {
        g_object_unref (self->priv->plugin);
        self->priv->plugin = NULL;
    }
    if (self->priv->properties_pending)
    {
        g_slist_foreach (self->priv->properties_pending, (GFunc) g_object_unref,
                         NULL);
        g_slist_free (self->priv->properties_pending);
        self->priv->properties_pending = NULL;
    }

    G_OBJECT_CLASS (ccm_window_parent_class)->finalize (object);
}

static void
ccm_window_class_init (CCMWindowClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMWindowPrivate));

	CCMWindowPixmapQuark = g_quark_from_static_string("CCMWindowPixmap");
	
    object_class->get_property = ccm_window_get_gobject_property;
    object_class->set_property = ccm_window_set_gobject_property;
    object_class->finalize = ccm_window_finalize;

    CCM_DRAWABLE_CLASS (klass)->query_geometry = ccm_window_query_geometry;
    CCM_DRAWABLE_CLASS (klass)->move = ccm_window_move;
    CCM_DRAWABLE_CLASS (klass)->resize = ccm_window_resize;

    klass->plugins = NULL;

    /**
	 * CCMWindow:child:
	 *
	 * The main child of window, it is only set with decorated window and it 
	 * point on real window.
	 */
    g_object_class_install_property (object_class, PROP_CHILD,
                                     g_param_spec_pointer ("child", "Child",
                                                           "Child of window",
                                                           G_PARAM_READWRITE));

	/**
	 * CCMWindow:no_undamage_sibling:
	 *
	 * This property indicate if the window don't clip damage area of 
	 * sibling window.
	 */
    g_object_class_install_property (object_class, PROP_NO_UNDAMAGE_SIBLING,
                                     g_param_spec_boolean
                                     ("no_undamage_sibling",
                                      "No undamage sibling",
                                      "Get if window undamage sibling", FALSE,
                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

	/**
	 * CCMWindow:input:
	 *
	 * The input redirection window it is only set when window is transformed
	 * and need to be have input event redirected.
	 */
    g_object_class_install_property (object_class, PROP_INPUT,
                                     g_param_spec_pointer ("input", "Input",
                                                           "Window input",
                                                           G_PARAM_READABLE));

	/**
	 * CCMWindow:redirect:
	 *
	 * If is unset the window isn't redirected when it have transformation.
	 */
    g_object_class_install_property (object_class, PROP_REDIRECT,
                                     g_param_spec_boolean ("redirect",
                                                           "Redirect",
                                                           "Unlock/Lock redirect input",
                                                           TRUE,
                                                           G_PARAM_WRITABLE));

	/**
	 * CCMWindow:block_mouse_redirect_event:
	 *
	 * If is set block pointer redirect event when window is transformed.
	 */
    g_object_class_install_property (object_class,
                                     PROP_BLOCK_MOUSE_REDIRECT_EVENT,
                                     g_param_spec_boolean
                                     ("block_mouse_redirect_event",
                                      "Block Mouse Redirect Event",
                                      "Unblock/Block redirected pointer event when window is transformed",
                                      FALSE, G_PARAM_WRITABLE));

	/**
	 * CCMWindow:use_image:
	 *
	 * If is set the window use software rendering instead configured backend.
	 */
    g_object_class_install_property (object_class, PROP_IMAGE,
                                     g_param_spec_boolean ("use_image",
                                                           "UseImage",
                                                           "Use image backend for pixmap",
                                                           FALSE,
                                                           G_PARAM_WRITABLE));

	/**
	 * CCMWindow:mask_width:
	 *
	 * Width of window mask surface.
	 */
    g_object_class_install_property (object_class, PROP_MASK_WIDTH,
                                     g_param_spec_int ("mask_width",
                                                       "MaskWidth",
                                                       "Window mask width",
                                                       G_MININT, G_MAXINT, 0,
                                                       G_PARAM_READWRITE));

	/**
	 * CCMWindow:mask_y_invert:
	 *
	 * This property indicate if the mask paint is y inverted.
	 */
    g_object_class_install_property (object_class, PROP_MASK_Y_INVERT,
                                     g_param_spec_boolean ("mask_y_invert",
                                                           "Mask Y Invert",
                                                           "Get if mask is y inverted",
                                                           FALSE,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_WRITABLE));

	/**
	 * CCMWindow:mask_height:
	 *
	 * Height of window mask surface.
	 */
    g_object_class_install_property (object_class, PROP_MASK_HEIGHT,
                                     g_param_spec_int ("mask_height",
                                                       "MaskHeight",
                                                       "Window mask height",
                                                       G_MININT, G_MAXINT, 0,
                                                       G_PARAM_READWRITE));

	/**
	 * CCMWindow:mask:
	 *
	 * Mask surface.
	 */
    g_object_class_install_property (object_class, PROP_MASK,
                                     g_param_spec_pointer ("mask", "Mask",
                                                           "Window paint mask",
                                                           G_PARAM_READWRITE));

	/**
	 * CCMWindow::property-changed:
	 * @property: #CCMPropertyType
	 *
	 * Emitted when a window property changed.
	 */
    signals[PROPERTY_CHANGED] =
        g_signal_new ("property-changed", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	/**
	 * CCMWindow::opacity-changed:
	 *
	 * Emitted when the window opacity changed.
	 */
    signals[OPACITY_CHANGED] =
        g_signal_new ("opacity-changed", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	/**
	 * CCMWindow::redirect-input:
	 *
	 * Emitted when the window redirect input changed.
	 */
    signals[REDIRECT_INPUT] =
        g_signal_new ("redirect-input", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1,
                      G_TYPE_BOOLEAN);

	/**
	 * CCMWindow::error:
	 *
	 * Emitted when an error occur on a window request.
	 */
    signals[ERROR] =
        g_signal_new ("error", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
ccm_window_iface_init (CCMWindowPluginClass * iface)
{
    iface->load_options = NULL;
    iface->query_geometry = impl_ccm_window_query_geometry;
    iface->paint = impl_ccm_window_paint;
    iface->map = impl_ccm_window_map;
    iface->unmap = impl_ccm_window_unmap;
    iface->query_opacity = impl_ccm_window_query_opacity;
    iface->move = impl_ccm_window_move;
    iface->resize = impl_ccm_window_resize;
    iface->set_opaque_region = impl_ccm_window_set_opaque_region;
    iface->get_origin = impl_ccm_window_get_origin;
    iface->get_pixmap = impl_ccm_window_get_pixmap;
}

static void
create_atoms (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    CCMWindowClass *klass = CCM_WINDOW_GET_CLASS (self);
    if (!klass->opacity_atom)
    {
        CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));

        klass->ccm_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_CCM_CLIENT_MESSAGE",
                         False);

        klass->atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "ATOM", False);

        klass->none_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "NONE", False);

        klass->utf8_string_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "UTF8_STRING", False);
        klass->active_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_ACTIVE_WINDOW",
                         False);
        klass->root_pixmap_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_XROOTPMAP_ID",
                         False);
        klass->user_time_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_USER_TIME",
                         False);
        klass->name_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_NAME", False);
        klass->visible_name_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_VISIBLE_NAME",
                         False);
        klass->opacity_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_OPACITY", False);
        klass->client_list_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_CLIENT_LIST",
                         False);
        klass->client_stacking_list_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_CLIENT_LIST_STACKING", False);
        klass->type_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_WINDOW_TYPE",
                         False);
        klass->type_normal_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_NORMAL", False);
        klass->type_desktop_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        klass->type_dock_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_DOCK", False);
        klass->type_toolbar_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
        klass->type_menu_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_MENU", False);
        klass->type_util_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_UTILITY", False);
        klass->type_splash_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_SPLASH", False);
        klass->type_dialog_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_DIALOG", False);
        klass->type_dropdown_menu_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
        klass->type_popup_menu_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
        klass->type_tooltip_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
        klass->type_notification_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
        klass->type_combo_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_COMBO", False);
        klass->type_dnd_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_WINDOW_TYPE_DND", False);
        klass->state_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_STATE",
                         False);
        klass->state_shade_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_STATE_SHADED",
                         False);
        klass->state_fullscreen_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_STATE_FULLSCREEN", False);
        klass->state_above_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_STATE_ABOVE",
                         False);
        klass->state_below_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_STATE_BELOW",
                         False);
        klass->state_skip_taskbar =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_STATE_SKIP_TASKBAR", False);
        klass->state_skip_pager =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_STATE_SKIP_PAGER", False);
        klass->state_is_modal =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_STATE_MODAL",
                         False);
        klass->mwm_hints_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_MOTIF_WM_HINTS",
                         False);
        klass->frame_extends_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_FRAME_EXTENTS",
                         False);
        klass->transient_for_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "WM_TRANSIENT_FOR",
                         False);
        klass->current_desktop_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_CURRENT_DESKTOP",
                         False);
        klass->protocol_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "WM_PROTOCOLS", False);
        klass->delete_window_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display),
                         "_NET_WM_DELETE_WINDOW", False);
        klass->ping_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_PING", False);
        klass->pid_atom =
            XInternAtom (CCM_DISPLAY_XDISPLAY (display), "_NET_WM_PID", False);
    }
}

static GSList *
ccm_window_get_state_atom_list (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    GSList *list = NULL;

    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_shade_atom);
    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_fullscreen_atom);
    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_above_atom);
    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_below_atom);
    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_skip_taskbar);
    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_skip_pager);
    list =
        g_slist_prepend (list,
                         (gpointer) CCM_WINDOW_GET_CLASS (self)->
                         state_is_modal);

    return list;
}

static void
ccm_window_get_property_async (CCMWindow * self, Atom property_atom,
                               Atom req_type, long length)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (property_atom != None);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    CCMPropertyASync *property;

    ccm_debug_atom (display, property_atom, "GET PROPERTY");

    if (self->priv->child != None)
    {
        property =
            ccm_property_async_new (display, self->priv->child, property_atom,
                                    req_type, length);
        g_signal_connect_swapped (property, "reply",
                                  G_CALLBACK (ccm_window_on_get_property_async),
                                  self);
        g_signal_connect_swapped (property, "error",
                                  G_CALLBACK
                                  (ccm_window_on_property_async_error), self);
        self->priv->properties_pending =
            g_slist_prepend (self->priv->properties_pending, property);
    }

    property =
        ccm_property_async_new (display, CCM_WINDOW_XWINDOW (self),
                                property_atom, req_type, length);
    g_signal_connect_swapped (property, "reply",
                              G_CALLBACK (ccm_window_on_get_property_async),
                              self);
    g_signal_connect_swapped (property, "error",
                              G_CALLBACK (ccm_window_on_property_async_error),
                              self);
    self->priv->properties_pending =
        g_slist_prepend (self->priv->properties_pending, property);
}

static gchar *
ccm_window_get_utf8_property (CCMWindow * self, Atom atom)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (atom != None, NULL);

    gchar *val;
    guint32 *data = NULL;
    guint n_items;

    data =
        ccm_window_get_property (self, atom,
                                 CCM_WINDOW_GET_CLASS (self)->utf8_string_atom,
                                 &n_items);

    if (!data)
        return NULL;

    if (!g_utf8_validate ((gchar *) data, n_items, NULL))
    {
        g_free (data);
        return NULL;
    }

    val = g_strndup ((gchar *) data, n_items);

    g_free (data);

    return val;
}

static gchar *
ccm_window_get_child_utf8_property (CCMWindow * self, Atom atom)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (atom != None, NULL);

    gchar *val;
    guint32 *data = NULL;
    guint n_items;

    data =
        ccm_window_get_child_property (self, atom,
                                       CCM_WINDOW_GET_CLASS (self)->
                                       utf8_string_atom, &n_items);

    if (!data)
        return NULL;

    if (!g_utf8_validate ((gchar *) data, n_items, NULL))
    {
        XFree (data);
        return NULL;
    }

    val = g_strndup ((gchar *) data, n_items);

    g_free (data);

    return val;
}

static gchar *
text_property_to_utf8 (const XTextProperty * prop)
{
    gchar **list;
    gint count;
    gchar *retval;

    list = NULL;

    count =
        gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
                                        prop->format, prop->value, prop->nitems,
                                        &list);

    if (count == 0)
        retval = NULL;
    else
    {
        retval = list[0];
        list[0] = g_strdup ("");        /* something to free */
    }

    g_strfreev (list);

    return retval;
}

static gchar *
ccm_window_get_text_property (CCMWindow * self, Atom atom)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (atom != None, NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    XTextProperty text;
    gchar *retval = NULL;

    text.nitems = 0;
    if (XGetTextProperty
        (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (self), &text,
         atom))
    {
        retval = text_property_to_utf8 (&text);

        if (text.value)
            XFree (text.value);
    }

    return retval;
}

static gchar *
ccm_window_get_child_text_property (CCMWindow * self, Atom atom)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (atom != None, NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    XTextProperty text;
    gchar *retval = NULL;

    text.nitems = 0;
    if (XGetTextProperty
        (CCM_DISPLAY_XDISPLAY (display), self->priv->child, &text, atom))
    {
        retval = text_property_to_utf8 (&text);

        if (text.value)
            XFree (text.value);
    }

    return retval;
}

static void
ccm_window_get_plugins (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->unmanaged) return;
	
	CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    GSList *item;

    if (!CCM_WINDOW_GET_CLASS (self)->plugins)
        g_object_get (G_OBJECT (screen), "window_plugins",
                      &CCM_WINDOW_GET_CLASS (self)->plugins, NULL);

    if (self->priv->plugin && CCM_IS_PLUGIN (self->priv->plugin))
        g_object_unref (self->priv->plugin);

    self->priv->plugin = (CCMWindowPlugin *) self;

	for (item = CCM_WINDOW_GET_CLASS (self)->plugins; item; item = item->next)
    {
        GType type = GPOINTER_TO_INT (item->data);
        GObject *prev = G_OBJECT (self->priv->plugin);
        CCMWindowPlugin *plugin = g_object_new (type, "parent", prev, "screen",
                                                ccm_screen_get_number (screen),
                                                NULL);

        if (plugin)
            self->priv->plugin = plugin;
    }

    ccm_window_plugin_load_options (self->priv->plugin, self);
}

static void
ccm_window_query_child (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    Window *windows = NULL, w, p;
    guint n_windows;

    if (CCM_WINDOW_XWINDOW (self) ==
        RootWindowOfScreen (CCM_SCREEN_XSCREEN (screen)))
        return;

    if (!self->priv->override_redirect
        && XQueryTree (CCM_DISPLAY_XDISPLAY (display),
                       CCM_WINDOW_XWINDOW (self), &w, &p, &windows, &n_windows)
        && n_windows > 0)
    {
        CCMWindow *root = ccm_screen_get_root_window (screen);
        guint n_managed = 0;
        Window *managed = (Window *) ccm_window_get_property (root,
                                                              CCM_WINDOW_GET_CLASS
                                                              (root)->
                                                              client_stacking_list_atom,
                                                              XA_WINDOW,
                                                              &n_managed);

        if (managed && n_managed)
        {
            guint i, j;
            gboolean found = FALSE;

            for (i = 0; !found && i < n_managed; ++i)
            {
                for (j = 0; !found && j < n_windows; ++j)
                {
                    if (windows[j] && managed[i] == windows[j])
                    {
                        self->priv->child = windows[j];
                        ccm_debug_window (self, "FOUND CHILD 0x%lx",
                                          self->priv->child);
                        XSelectInput (CCM_DISPLAY_XDISPLAY (display),
                                      self->priv->child,
                                      PropertyChangeMask | StructureNotifyMask);
                        found = TRUE;
                    }
                }
            }
        }
        if (managed)
            g_free (managed);
    }
    if (windows)
        XFree (windows);
}

static gboolean
ccm_window_get_attribs (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    XWindowAttributes attribs;

    if (!XGetWindowAttributes
        (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (self), &attribs))
    {
        g_signal_emit (self, signals[ERROR], 0);
        return FALSE;
    }

    self->priv->root = attribs.root;

    self->priv->area.x = attribs.x - attribs.border_width;
    self->priv->area.y = attribs.y - attribs.border_width;
    self->priv->area.width = attribs.width + (attribs.border_width * 2);
    self->priv->area.height = attribs.height + (attribs.border_width * 2);

    ccm_drawable_set_visual (CCM_DRAWABLE(self), attribs.visual);
	ccm_drawable_set_depth (CCM_DRAWABLE(self), attribs.depth);

    self->priv->is_viewable = attribs.map_state == IsViewable;

    self->priv->is_input_only = attribs.class == InputOnly;
    self->priv->override_redirect = attribs.override_redirect;
    if (self->priv->override_redirect && self->priv->child)
        self->priv->child = None;

    return TRUE;
}

static void
ccm_window_query_geometry (CCMDrawable * drawable)
{
    g_return_if_fail (drawable != NULL);

    CCMWindow *self = CCM_WINDOW (drawable);
    CCMRegion *geometry = NULL;

    if (self->priv->pixmap)
    {
        g_object_unref (self->priv->pixmap);
        self->priv->pixmap = NULL;
    }
    geometry = ccm_window_plugin_query_geometry (self->priv->plugin, self);

    ccm_drawable_set_geometry(CCM_DRAWABLE(self), geometry);

    if (geometry) ccm_region_destroy (geometry);
}

static void
ccm_window_move (CCMDrawable * drawable, int x, int y)
{
    g_return_if_fail (drawable != NULL);

    CCMWindow *self = CCM_WINDOW (drawable);

    ccm_window_plugin_move (self->priv->plugin, self, x, y);
}

static void
ccm_window_resize (CCMDrawable * drawable, int width, int height)
{
    g_return_if_fail (drawable != NULL);

    CCMWindow *self = CCM_WINDOW (drawable);

    ccm_window_plugin_resize (self->priv->plugin, self, width, height);
}

static CCMRegion *
impl_ccm_window_query_geometry (CCMWindowPlugin * plugin, CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMRegion *geometry = NULL;
    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    int bx, by, cs, cx, cy, o;  /* dummies */
    unsigned int bw, bh, cw, ch;        /* dummies */

    if (!ccm_window_get_attribs (self))
        return NULL;

    if (XShapeQueryExtents
        (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (self),
         &self->priv->is_shaped, &bx, &by, &bw, &bh, &cs, &cx, &cy, &cw, &ch)
        && self->priv->is_shaped)
    {
        gint cpt, nb;
        XRectangle *shapes;

        if ((shapes =
             XShapeGetRectangles (CCM_DISPLAY_XDISPLAY (display),
                                  CCM_WINDOW_XWINDOW (self), ShapeBounding, &nb,
                                  &o)))
        {
            geometry = ccm_region_new ();
            for (cpt = 0; cpt < nb; ++cpt)
                ccm_region_union_with_xrect (geometry, &shapes[cpt]);
            ccm_region_offset (geometry, self->priv->area.x,
                               self->priv->area.y);
        }
        else
            self->priv->is_shaped = FALSE;
        XFree (shapes);
    }

    if (!self->priv->is_shaped)
    {
        geometry = ccm_region_rectangle (&self->priv->area);
    }

    ccm_window_set_alpha (self);
    if (geometry
        && ccm_drawable_get_format (CCM_DRAWABLE (self)) != CAIRO_FORMAT_ARGB32
        && self->priv->opacity == 1.0f)
    {
        CCMRegion *area = ccm_region_copy (geometry);
        ccm_window_set_opaque_region (self, area);
        ccm_region_destroy (area);
    }

    if (geometry)
    {
        cairo_rectangle_t clipbox;
        CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));

        ccm_region_get_clipbox (geometry, &clipbox);
        if (clipbox.x <= 0 && clipbox.y <= 0
            && clipbox.width >= CCM_SCREEN_XSCREEN (screen)->width
            && clipbox.height >= CCM_SCREEN_XSCREEN (screen)->height
            && !self->priv->is_fullscreen)
        {
            self->priv->is_fullscreen = TRUE;
            g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                           CCM_PROPERTY_STATE);
        }
        else if (self->priv->is_fullscreen)
        {
            self->priv->is_fullscreen = FALSE;
            g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                           CCM_PROPERTY_STATE);
        }
    }

    if (self->priv->pixmap && CCM_IS_PIXMAP (self->priv->pixmap))
    {
        g_object_unref (self->priv->pixmap);
        self->priv->pixmap = NULL;
    }

    return geometry;
}

static void
impl_ccm_window_move (CCMWindowPlugin * plugin, CCMWindow * self, int x, int y)
{
    cairo_rectangle_t geometry;

    if (ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (self), &geometry)
        && (x != (int) geometry.x || y != (int) geometry.y))
    {
        CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
        CCMRegion *old_geometry = ccm_region_rectangle (&geometry);

        if (screen && self->priv->is_viewable && !self->priv->is_input_only
            && self->priv->is_decorated && !self->priv->override_redirect)
            g_object_set (G_OBJECT (screen), "buffered_pixmap", TRUE, NULL);

        CCM_DRAWABLE_CLASS (ccm_window_parent_class)->move (CCM_DRAWABLE (self),
                                                            x, y);

        self->priv->area.x += x - geometry.x;
        self->priv->area.y += y - geometry.y;

        if (self->priv->opaque)
            ccm_region_offset (self->priv->opaque, x - geometry.x,
                               y - geometry.y);
        if (self->priv->orig_opaque)
            ccm_region_offset (self->priv->orig_opaque, x - geometry.x,
                               y - geometry.y);
        if ((self->priv->is_viewable || self->priv->unmap_pending)
            && ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE (self),
                                                  &geometry))
        {
            ccm_region_union_with_rect (old_geometry, &geometry);
            ccm_drawable_damage_region (CCM_DRAWABLE (self), old_geometry);
        }
        ccm_region_destroy (old_geometry);
    }
	else if (!ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (self), &geometry))
	{
		ccm_drawable_query_geometry(CCM_DRAWABLE(self));
		ccm_drawable_damage(CCM_DRAWABLE(self));
	}
}

static void
impl_ccm_window_resize (CCMWindowPlugin * plugin, CCMWindow * self, int width,
                        int height)
{
    cairo_rectangle_t geometry;

    if (!ccm_drawable_get_device_geometry_clipbox
        (CCM_DRAWABLE (self), &geometry) || width != (int) geometry.width
        || height != (int) geometry.height)
    {
        CCMRegion *old_geometry = ccm_region_rectangle (&geometry);
        CCMRegion *new_geometry;

        ccm_debug_window (self, "RESIZE %i,%i", width, height);

        CCM_DRAWABLE_CLASS (ccm_window_parent_class)->
            resize (CCM_DRAWABLE (self), width, height);

        self->priv->area.width += width - geometry.width;
        self->priv->area.height += height - geometry.height;

        new_geometry =
            (CCMRegion *)
            ccm_drawable_get_device_geometry (CCM_DRAWABLE (self));

        if (ccm_drawable_get_format (CCM_DRAWABLE (self)) !=
            CAIRO_FORMAT_ARGB32)
            ccm_window_set_opaque_region (self, new_geometry);

        ccm_drawable_damage (CCM_DRAWABLE (self));

        if (new_geometry && old_geometry)
        {
            ccm_region_subtract (old_geometry, new_geometry);
            if (!ccm_region_empty (old_geometry))
            {
                cairo_matrix_t matrix;

                matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));
                ccm_region_offset (old_geometry, -geometry.x, -geometry.y);
                ccm_region_transform (old_geometry, &matrix);
                ccm_region_offset (old_geometry, geometry.x, geometry.y);
                ccm_drawable_damage_region (CCM_DRAWABLE (self), old_geometry);
            }
            ccm_region_destroy (old_geometry);
        }

        if (CCM_IS_PIXMAP (self->priv->pixmap))
        {
            g_object_unref (self->priv->pixmap);
            self->priv->pixmap = NULL;
        }

        if (new_geometry)
        {
            cairo_rectangle_t clipbox;
            CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));

            ccm_region_get_clipbox (new_geometry, &clipbox);
            if (clipbox.x <= 0 && clipbox.y <= 0
                && clipbox.width >= CCM_SCREEN_XSCREEN (screen)->width
                && clipbox.height >= CCM_SCREEN_XSCREEN (screen)->height
                && !self->priv->is_fullscreen)
            {
                self->priv->is_fullscreen = TRUE;
                g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                               CCM_PROPERTY_STATE);
            }
            else if (self->priv->is_fullscreen)
            {
                self->priv->is_fullscreen = FALSE;
                g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                               CCM_PROPERTY_STATE);
            }
        }
    }
	else if (!ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (self), &geometry))
	{
		ccm_drawable_query_geometry(CCM_DRAWABLE(self));
		ccm_drawable_damage(CCM_DRAWABLE(self));
	}
}

static void
ccm_window_check_mask (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->mask && self->priv->mask_width > 0
        && self->priv->mask_height > 0)
    {
        cairo_rectangle_t clipbox;
        int width, height;

        // Check size of mask must be match geometry
        ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (self),
                                                  &clipbox);
        width = self->priv->mask_width;
        height = self->priv->mask_height;

        // Size doesn't match a plugin add an offset to pixmap
        if (clipbox.width != width || clipbox.height != height)
        {
            CCMRegion *geometry =
                (CCMRegion *)
                ccm_drawable_get_device_geometry (CCM_DRAWABLE (self));
            CCMRegion *tmp1 = ccm_region_copy (geometry);
            CCMRegion *tmp2 = ccm_region_copy (geometry);
            int x, y, cpt, nb_rects;
            cairo_surface_t *old = self->priv->mask;
            cairo_rectangle_t *rects = NULL;
            cairo_t *ctx;
            cairo_surface_t *surface =
                ccm_drawable_get_surface (CCM_DRAWABLE (self));

            // Resize mask
            self->priv->mask =
                cairo_surface_create_similar (surface, CAIRO_CONTENT_ALPHA,
                                              clipbox.width, clipbox.height);
            cairo_surface_destroy (surface);
            self->priv->mask_width = (int) clipbox.width;
            self->priv->mask_height = (int) clipbox.height;

            // Get window origin
            ccm_window_plugin_get_origin (self->priv->plugin, self, &x, &y);

            // Add opaque mask around the old mask
            ctx = cairo_create (self->priv->mask);
            cairo_set_operator (ctx, CAIRO_OPERATOR_SOURCE);

            cairo_translate (ctx, -clipbox.x, -clipbox.y);
            cairo_set_source_surface (ctx, old, x, y);
            cairo_paint (ctx);

            // get out size region
            ccm_region_resize (tmp2, width, height);
            ccm_region_resize (tmp1, clipbox.width, clipbox.height);
            ccm_region_offset (tmp1, -(x - clipbox.x), -(y - clipbox.y));
            ccm_region_subtract (tmp1, tmp2);
            ccm_region_offset (tmp1, (x - clipbox.x), (y - clipbox.y));

            // Paint out size region
            cairo_set_source_rgba (ctx, 1, 1, 1, self->priv->opacity);
            ccm_region_get_rectangles (tmp1, &rects, &nb_rects);
            for (cpt = 0; cpt < nb_rects; ++cpt)
                cairo_rectangle (ctx, rects[cpt].x, rects[cpt].y,
                                 rects[cpt].width, rects[cpt].height);
            if (rects)
                cairo_rectangles_free (rects, nb_rects);
            cairo_fill (ctx);
            ccm_region_destroy (tmp1);
            ccm_region_destroy (tmp2);

            // Paint old mask
            cairo_destroy (ctx);
            cairo_surface_destroy (old);
        }
    }
}

static gboolean
impl_ccm_window_paint (CCMWindowPlugin * plugin, CCMWindow * self,
                       cairo_t * context, cairo_surface_t * surface,
                       gboolean y_invert)
{
    g_return_val_if_fail (self != NULL, FALSE);

    gboolean ret = FALSE;

    if (self->priv->opacity == 0.0f)
        return TRUE;

    cairo_save (context);
    ccm_debug_window (self, "PAINT WINDOW %f", self->priv->opacity);
    if (ccm_window_transform (self, context, y_invert))
    {
        cairo_set_source_surface (context, surface, 0.0f, 0.0f);
        if (self->priv->mask && self->priv->mask_width > 0
            && self->priv->mask_height > 0)
        {
            ccm_window_check_mask (self);
            cairo_mask_surface (context, self->priv->mask, 0, 0);
        }
        else
            cairo_paint_with_alpha (context, self->priv->opacity);

        if (cairo_status (context) != CAIRO_STATUS_SUCCESS)
        {
            ccm_debug_window (self, "PAINT ERROR %i", cairo_status (context));
            g_signal_emit (self, signals[ERROR], 0);
            ret = FALSE;
        }
        else
            ret = TRUE;
    }
    else
        ret = FALSE;
    ccm_debug_window (self, "PAINT WINDOW %i", ret);
    cairo_restore (context);

    return ret;
}

static void
impl_ccm_window_map (CCMWindowPlugin * plugin, CCMWindow * self)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (self != NULL);

    cairo_matrix_t matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));

    ccm_debug_window (self, "IMPL WINDOW MAP");
    ccm_drawable_damage (CCM_DRAWABLE (self));

    if (!(matrix.x0 == 0 && matrix.xy == 0 && matrix.xx == 1 && matrix.y0 == 0
         && matrix.yx == 0 && matrix.yy == 1))
        ccm_window_redirect_input (self);
}

static void
impl_ccm_window_unmap (CCMWindowPlugin * plugin, CCMWindow * self)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (self != NULL);

    const CCMRegion *geometry = ccm_drawable_get_geometry (CCM_DRAWABLE (self));
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));

    self->priv->is_viewable = FALSE;
    self->priv->visible = FALSE;
    self->priv->unmap_pending = FALSE;
    ccm_debug_window (self, "IMPL WINDOW UNMAP");

    if (geometry)
        ccm_screen_damage_region (screen, geometry);
    else
        ccm_screen_damage (screen);

    ccm_drawable_pop_matrix (CCM_DRAWABLE (self), "CCMWindowTranslate");
    ccm_drawable_pop_matrix (CCM_DRAWABLE (self), "CCMWindowTransient");

    ccm_window_unredirect_input (self);

    if (self->priv->mask)
    {
        cairo_surface_destroy (self->priv->mask);
        self->priv->mask = NULL;
    }

    if (CCM_IS_PIXMAP (self->priv->pixmap))
    {
        g_object_unref (self->priv->pixmap);
        self->priv->pixmap = NULL;
    }
}

static void
impl_ccm_window_query_opacity (CCMWindowPlugin * plugin, CCMWindow * self)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    ccm_debug_window (self, "QUERY OPACITY");
    ccm_window_get_property_async (self,
                                   CCM_WINDOW_GET_CLASS (self)->opacity_atom,
                                   XA_CARDINAL, 32);
}

static void
impl_ccm_window_set_opaque_region (CCMWindowPlugin * plugin, CCMWindow * self,
                                   const CCMRegion * area)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (self != NULL);

    ccm_window_set_alpha (self);

    if (area && !ccm_region_empty ((CCMRegion *) area))
    {
        cairo_matrix_t transform =
            ccm_drawable_get_transform (CCM_DRAWABLE (self));
        cairo_rectangle_t clipbox;

        self->priv->orig_opaque = ccm_region_copy ((CCMRegion *) area);
        self->priv->opaque = ccm_region_copy ((CCMRegion *) area);
        if (ccm_drawable_get_device_geometry_clipbox
            (CCM_DRAWABLE (self), &clipbox))
        {
            ccm_region_offset (self->priv->opaque, -clipbox.x, -clipbox.y);
            ccm_region_transform (self->priv->opaque, &transform);
            ccm_region_offset (self->priv->opaque, clipbox.x, clipbox.y);
        }
    }
}

static void
impl_ccm_window_get_origin (CCMWindowPlugin * plugin, CCMWindow * self, int *x,
                            int *y)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (self != NULL);
    g_return_if_fail (x != NULL && y != NULL);

    cairo_rectangle_t geometry;

    if (ccm_drawable_get_device_geometry_clipbox
        (CCM_DRAWABLE (self), &geometry))
    {
        *x = geometry.x;
        *y = geometry.y;
    }
    else
    {
        *x = 0;
        *y = 0;
    }
}

static CCMPixmap *
impl_ccm_window_get_pixmap (CCMWindowPlugin * plugin, CCMWindow * self)
{
    g_return_val_if_fail (plugin != NULL, NULL);
    g_return_val_if_fail (self != NULL, NULL);

    CCMPixmap *pixmap = NULL;
    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    Pixmap xpixmap;

    ccm_display_grab (display);
    xpixmap =
        XCompositeNameWindowPixmap (CCM_DISPLAY_XDISPLAY (display),
                                    CCM_WINDOW_XWINDOW (self));
    ccm_display_ungrab (display);
    if (xpixmap)
    {
        if (self->priv->use_pixmap_image)
        {
            pixmap = ccm_pixmap_image_new (CCM_DRAWABLE (self), xpixmap);
        }
        else
        {
            pixmap = ccm_pixmap_new (CCM_DRAWABLE (self), xpixmap);
        }
    }

    return pixmap;
}

static void
ccm_window_on_pixmap_damaged (CCMPixmap * pixmap, CCMRegion * area,
                              CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);

    cairo_rectangle_t geometry;
    cairo_matrix_t transform = ccm_drawable_get_transform (CCM_DRAWABLE (self));

    ccm_region_transform (area, &transform);
    if (ccm_drawable_get_device_geometry_clipbox
        (CCM_DRAWABLE (self), &geometry))
        ccm_region_offset (area, geometry.x, geometry.y);

    ccm_drawable_damage_region (CCM_DRAWABLE (self), area);
}

static void
ccm_window_on_property_async_error (CCMWindow * self, CCMPropertyASync * prop)
{
    ccm_debug ("PROPERTY ERROR");
    self->priv->properties_pending =
        g_slist_remove (self->priv->properties_pending, prop);
    g_object_unref (prop);
}

static void
ccm_window_on_get_property_async (CCMWindow * self, guint n_items,
                                  gchar * result, CCMPropertyASync * prop)
{
    Atom property = ccm_property_async_get_property (prop);

    if (!CCM_IS_WINDOW (self))
    {
        self->priv->properties_pending =
            g_slist_remove (self->priv->properties_pending, prop);
        g_object_unref (prop);
        return;
    }

    g_return_if_fail (CCM_IS_PROPERTY_ASYNC (prop));
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);


    if (property == CCM_WINDOW_GET_CLASS (self)->type_atom)
    {
        if (result)
        {
            CCMWindowType old = self->priv->hint_type;
            Atom atom;
            memcpy (&atom, result, sizeof (Atom));

            if (atom == CCM_WINDOW_GET_CLASS (self)->type_normal_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_NORMAL;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_menu_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_MENU;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_desktop_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_DESKTOP;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_dock_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_DOCK;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_toolbar_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_TOOLBAR;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_util_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_UTILITY;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_splash_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_SPLASH;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_dialog_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_DIALOG;
            else if (atom ==
                     CCM_WINDOW_GET_CLASS (self)->type_dropdown_menu_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_DROPDOWN_MENU;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_popup_menu_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_POPUP_MENU;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_tooltip_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_TOOLTIP;
            else if (atom ==
                     CCM_WINDOW_GET_CLASS (self)->type_notification_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_NOTIFICATION;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_combo_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_COMBO;
            else if (atom == CCM_WINDOW_GET_CLASS (self)->type_dnd_atom)
                self->priv->hint_type = CCM_WINDOW_TYPE_DND;

            if (old != self->priv->hint_type)
            {
                g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                               CCM_PROPERTY_HINT_TYPE);
            }
        }
    }
    else if (property == CCM_WINDOW_GET_CLASS (self)->transient_for_atom)
    {
        gboolean updated = FALSE;

        if (result)
        {
            Window old = self->priv->transient_for;

            memcpy (&self->priv->transient_for, result, sizeof (Window));
            updated = old != self->priv->transient_for;

            if (old != None)
            {
                CCMWindow *transient = ccm_window_transient_for (self);

                if (transient && self->priv->id_transient_transform_changed)
                {
                    ccm_drawable_pop_matrix (CCM_DRAWABLE (self),
                                             "CCMWindowTranslate");
                    ccm_drawable_pop_matrix (CCM_DRAWABLE (self),
                                             "CCMWindowTransient");
                    g_signal_handler_disconnect (transient,
                                                 self->priv->id_transient_transform_changed);
					transient->priv->transients =
                        g_slist_remove (transient->priv->transients, self);
					self->priv->id_transient_transform_changed = 0;
                }
            }

            if (self->priv->transient_for != None)
            {
                CCMWindow *transient = ccm_window_transient_for (self);

                if (transient)
                {
                    ccm_window_on_transient_transform_changed (self, NULL,
                                                               transient);
                    self->priv->id_transient_transform_changed =
                        g_signal_connect_swapped (transient,
                                                  "notify::transform",
                                                  G_CALLBACK(ccm_window_on_transient_transform_changed),
                                                  self);
					transient->priv->transients =
                        g_slist_prepend (transient->priv->transients, self);
                }
                if (self->priv->hint_type == CCM_WINDOW_TYPE_NORMAL)
                {
                    self->priv->hint_type = CCM_WINDOW_TYPE_DIALOG;
                    updated = TRUE;
                }
            }
        }
        if (updated)
        {
            ccm_debug_window (self, "TRANSIENT FOR 0x%lx",
                              self->priv->transient_for);
            g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                           CCM_PROPERTY_TRANSIENT);
        }
    }
    else if (property == CCM_WINDOW_GET_CLASS (self)->mwm_hints_atom)
    {
        if (result)
        {
            MotifWmHints *hints;
            gboolean old = self->priv->is_decorated;

            hints = (MotifWmHints *) result;

            if (hints->flags & MWM_HINTS_DECORATIONS)
                self->priv->is_decorated = hints->decorations != 0;
            if (old != self->priv->is_decorated)
            {
                ccm_debug_window (self, "IS_DECORATED %i",
                                  self->priv->is_decorated);
                g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                               CCM_PROPERTY_MWM_HINTS);
            }
        }
    }
    else if (property == CCM_WINDOW_GET_CLASS (self)->state_atom)
    {
        if (result)
        {
            Atom *atom = (Atom *) result;
            gulong cpt;
            GSList *item, *list = ccm_window_get_state_atom_list (self);

            for (cpt = 0; cpt < n_items; ++cpt)
            {
                list = g_slist_remove (list, (gpointer) atom[cpt]);
                ccm_window_set_state (self, atom[cpt]);
            }
            for (item = list; item; item = item->next)
            {
                ccm_window_unset_state (self, (Atom) item->data);
            }
            g_slist_free (list);
        }
    }
    else if (property == CCM_WINDOW_GET_CLASS (self)->opacity_atom)
    {
        if (result)
        {
            gdouble old = self->priv->opacity;
            guint32 value;
            memcpy (&value, result, sizeof (guint32));
            self->priv->opacity = (double) value / (double) 0xffffffff;

            if (self->priv->opacity == 1.0f
                && ccm_drawable_get_format (CCM_DRAWABLE (self)) !=
                CAIRO_FORMAT_ARGB32 && !self->priv->opaque)
                ccm_window_set_opaque (self);
            else
                ccm_window_set_alpha (self);
            if (old != self->priv->opacity)
            {
                ccm_drawable_damage (CCM_DRAWABLE (self));
                g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                               CCM_PROPERTY_OPACITY);
            }
        }
    }

    self->priv->properties_pending =
        g_slist_remove (self->priv->properties_pending, prop);
    g_object_unref (prop);
}

static void
ccm_window_on_plugins_changed (CCMWindow * self, CCMScreen * screen)
{
    if (CCM_WINDOW_GET_CLASS (self)->plugins)
        g_slist_free (CCM_WINDOW_GET_CLASS (self)->plugins);
    CCM_WINDOW_GET_CLASS (self)->plugins = NULL;
    ccm_window_get_plugins (self);
    ccm_drawable_query_geometry (CCM_DRAWABLE (self));
}

static void
ccm_window_on_transient_transform_changed (CCMWindow * self, GParamSpec * pspec,
                                           CCMWindow * transient)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (transient != NULL);

    if (CCM_IS_DRAWABLE(transient) && CCM_IS_DRAWABLE(self) &&
        ccm_window_is_viewable (transient))
    {
        cairo_matrix_t matrix, translate;
        cairo_rectangle_t clipbox, geometry;
        double dx, dy;

        matrix = ccm_drawable_get_transform (CCM_DRAWABLE (transient));
        ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (transient),
                                                  &clipbox);
        ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (self),
                                                  &geometry);

        dx = geometry.x - clipbox.x;
        dy = geometry.y - clipbox.y;
        cairo_matrix_transform_distance (&matrix, &dx, &dy);
        cairo_matrix_init_translate (&translate, dx - (geometry.x - clipbox.x),
                                     dy - (geometry.y - clipbox.y));
        ccm_drawable_push_matrix (CCM_DRAWABLE (self), "CCMWindowTranslate",
                                  &translate);
        ccm_drawable_push_matrix (CCM_DRAWABLE (self), "CCMWindowTransient",
                                  &matrix);
    }
}

static void
ccm_window_on_transform_changed (CCMWindow * self, GParamSpec * pspec)
{
    g_return_if_fail (self != NULL);

    cairo_matrix_t matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));

    if (self->priv->orig_opaque)
    {
        CCMRegion *region = ccm_region_copy (self->priv->orig_opaque);

        ccm_window_set_opaque_region (self, region);
        ccm_region_destroy (region);
    }
    if (ccm_window_is_viewable (self) && !ccm_window_is_input_only (self))
    {
        ccm_window_unredirect_input (self);
        if (!(matrix.x0 == 0 && matrix.xy == 0 && matrix.xx == 1
             && matrix.y0 == 0 && matrix.yx == 0 && matrix.yy == 1))
            ccm_window_redirect_input (self);
    }
}

static gboolean
ccm_window_translate_coordinates (CCMWindow * self, XEvent * event,
                                  int x_offset, int y_offset)
{
    Window *windows = NULL;
    Window w, p;
    guint n_windows = 0;
    gboolean found = FALSE;
    int x, y;

    ccm_window_plugin_get_origin (self->priv->plugin, self, &x, &y);

    if (XQueryTree
        (event->xany.display, event->xbutton.window, &w, &p, &windows,
         &n_windows) && windows && n_windows)
    {
        while (n_windows-- && !found)
        {
            CCMRegion *area;
            cairo_matrix_t matrix;
            XWindowAttributes attribs;

            // Get child origin
            matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));
            XGetWindowAttributes (event->xany.display, windows[n_windows],
                                  &attribs);
            // Transform child geometry and check if root point in area
            area =
                ccm_region_create (x_offset + attribs.x - attribs.border_width,
                                   y_offset + attribs.y - attribs.border_width,
                                   attribs.width + (attribs.border_width * 2),
                                   attribs.height + (attribs.border_width * 2));
            if (ccm_region_point_in (area, event->xbutton.x, event->xbutton.y))
            {
                event->xbutton.window = windows[n_windows];
                x_offset += attribs.x - attribs.border_width;
                y_offset += attribs.y - attribs.border_width;
                // we don't found another child is the final destination
                if (!ccm_window_translate_coordinates
                    (self, event, x_offset, y_offset))
                {
                    // Transform child offset
                    event->xbutton.x -= x_offset;
                    event->xbutton.y -= y_offset;
                }
                found = TRUE;
            }
            ccm_region_destroy (area);
        }
        XFree (windows);
    }

    return found;
}

static CCMWindow *
ccm_window_get_child_offset (CCMWindow * self, Window child, int *x_child,
                             int *y_child)
{
    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    Window *windows = NULL;
    Window root, parent;
    guint n_windows = 0;

    if (CCM_WINDOW_XWINDOW (self) == child)
        return self;

    if (XQueryTree
        (CCM_DISPLAY_XDISPLAY (display), child, &root, &parent, &windows,
         &n_windows) && parent)
    {
        if (windows)
            XFree (windows);

        if (parent != CCM_WINDOW_XWINDOW (self))
        {
            CCMWindow *other = ccm_screen_find_window (screen, parent);

            if (other)
            {
                *x_child = 0;
                *y_child = 0;
                return ccm_window_get_child_offset (other, parent, x_child,
                                                    y_child);
            }
            else
            {
                XWindowAttributes attribs;

                XGetWindowAttributes (CCM_DISPLAY_XDISPLAY (display), child,
                                      &attribs);
                *x_child += attribs.x - attribs.border_width;
                *y_child += attribs.y - attribs.border_width;

                return ccm_window_get_child_offset (self, parent, x_child,
                                                    y_child);
            }
        }
        else
        {
            XWindowAttributes attribs;

            XGetWindowAttributes (CCM_DISPLAY_XDISPLAY (display), child,
                                  &attribs);
            *x_child += attribs.x - attribs.border_width;
            *y_child += attribs.y - attribs.border_width;
            return self;
        }
    }

    return NULL;
}

static void
ccm_window_send_enter_leave_event (CCMWindow * self, Window window,
                                   XEvent * event, gboolean enter)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (window != None);
    g_return_if_fail (event != NULL);

    XCrossingEvent evt;


    evt.type = enter ? EnterNotify : LeaveNotify;
    evt.serial = event->xbutton.serial;
    evt.send_event = False;
    evt.display = event->xany.display;
    evt.root = event->xbutton.root;
    evt.window = window;
    evt.subwindow = 0;
    evt.time = ++event->xbutton.time;
    evt.x = event->xbutton.x;
    evt.y = event->xbutton.y;
    evt.x_root = event->xbutton.x_root;
    evt.y_root = event->xbutton.y_root;
    evt.mode = NotifyNormal;
    evt.detail = enter ? NotifyAncestor : NotifyInferior;
    evt.same_screen = True;
    evt.focus = False;
    evt.state = event->xbutton.state;

    XSendEvent (event->xany.display, window, False, NoEventMask,
                (XEvent *) & evt);
}

static void
ccm_window_on_pixmap_destroyed (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    self->priv->pixmap = NULL;
}

CCMWindowPlugin *
_ccm_window_get_plugin (CCMWindow * self, GType type)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMWindowPlugin *plugin;

    for (plugin = self->priv->plugin; plugin != (CCMWindowPlugin *) self;
         plugin = CCM_WINDOW_PLUGIN_PARENT (plugin))
    {
        if (g_type_is_a (G_OBJECT_TYPE (plugin), type))
            return plugin;
    }

    return NULL;
}

Window
_ccm_window_get_child (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, None);

    return self->priv->child;
}

void
_ccm_window_reparent(CCMWindow* self, CCMWindow* parent)
{
	g_return_if_fail(self != NULL);

	GSList* item;
	
	for (item = self->priv->transients; item; item = item->next)
	{
		CCMWindow* window = item->data;
		
		ccm_drawable_pop_matrix (CCM_DRAWABLE (window), "CCMWindowTranslate");
        ccm_drawable_pop_matrix (CCM_DRAWABLE (window), "CCMWindowTransient");
		g_signal_handler_disconnect (self, window->priv->id_transient_transform_changed);
		window->priv->id_transient_transform_changed = 0;
		
		ccm_window_on_transient_transform_changed (window, NULL, parent);
        window->priv->id_transient_transform_changed =
			g_signal_connect_swapped (parent,
			                          "notify::transform",
			                          G_CALLBACK(ccm_window_on_transient_transform_changed),
			                          window);
		parent->priv->transients = g_slist_prepend (parent->priv->transients, 
		                                            window);
	}
	g_slist_free(self->priv->transients);
	self->priv->transients = NULL;
}

/**
 * ccm_window_new:
 * @screen: #CCMScreen of window
 * @xwindow: window xid
 *
 * Create a new #CCMWindow reference which point on @xwindow
 *
 * Returns: #CCMWindow
 **/
CCMWindow *
ccm_window_new (CCMScreen * screen, Window xwindow)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (xwindow != None, NULL);

    CCMDisplay *display = ccm_screen_get_display (screen);
    CCMWindow *self = g_object_new (CCM_TYPE_WINDOW_BACKEND (screen),
                                    "screen", screen,
                                    "drawable", xwindow,
                                    NULL);

    create_atoms (self);

    ccm_window_get_plugins (self);
    if (!ccm_window_get_attribs (self))
    {
        g_object_unref (self);
        return NULL;
    }

    if (!self->priv->is_input_only)
        ccm_drawable_query_geometry (CCM_DRAWABLE (self));

    if (self->priv->root != None
        && xwindow != RootWindowOfScreen (CCM_SCREEN_XSCREEN (screen))
        && self->priv->root != RootWindowOfScreen (CCM_SCREEN_XSCREEN (screen)))
    {
        g_object_unref (self);
        return NULL;
    }

    g_signal_connect (self, "notify::transform",
                      G_CALLBACK (ccm_window_on_transform_changed), NULL);
    self->priv->id_plugins_changed =
        g_signal_connect_swapped (screen, "plugins-changed",
                                  G_CALLBACK (ccm_window_on_plugins_changed),
                                  self);

    ccm_window_set_opacity (self, 1.0f);

    if (!self->priv->is_input_only)
    {
        ccm_window_query_child (self);
        ccm_window_query_hint_type (self);
        ccm_window_query_opacity (self, FALSE);
        ccm_window_query_transient_for (self);
        ccm_window_query_wm_hints (self);
        ccm_window_query_mwm_hints (self);
        ccm_window_query_state (self);
        ccm_window_query_frame_extends (self);

        XSelectInput (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (self),
                      PropertyChangeMask | StructureNotifyMask |
                      SubstructureNotifyMask);

        XShapeSelectInput (CCM_DISPLAY_XDISPLAY (display),
                           CCM_WINDOW_XWINDOW (self), ShapeNotifyMask);

        ccm_display_sync (display);
    }

    return self;
}

/**
 * ccm_window_new_unmanaged:
 * @screen: #CCMScreen of window
 * @xwindow: window xid
 *
 * Create a new unmanaged #CCMWindow reference which point on @xwindow
 *
 * Returns: #CCMWindow
 **/
CCMWindow*
ccm_window_new_unmanaged (CCMScreen * screen, Window xwindow)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (xwindow != None, NULL);

    CCMDisplay *display = ccm_screen_get_display (screen);
    CCMWindow *self = g_object_new (CCM_TYPE_WINDOW_BACKEND (screen),
                                    "screen", screen,
                                    "drawable", xwindow,
                                    NULL);

    create_atoms (self);

	self->priv->unmanaged = TRUE;
	
	self->priv->plugin = (CCMWindowPlugin*)self;
	
    if (!ccm_window_get_attribs (self))
    {
        g_object_unref (self);
        return NULL;
    }

    if (!self->priv->is_input_only)
        ccm_drawable_query_geometry (CCM_DRAWABLE (self));

    if (!self->priv->is_input_only)
    {
        ccm_window_query_child (self);
        ccm_window_query_hint_type (self);
        ccm_window_query_opacity (self, FALSE);
        ccm_window_query_transient_for (self);
        ccm_window_query_wm_hints (self);
        ccm_window_query_mwm_hints (self);
        ccm_window_query_state (self);
        ccm_window_query_frame_extends (self);

        XSelectInput (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (self),
                      PropertyChangeMask | StructureNotifyMask |
                      SubstructureNotifyMask);

        XShapeSelectInput (CCM_DISPLAY_XDISPLAY (display),
                           CCM_WINDOW_XWINDOW (self), ShapeNotifyMask);

        ccm_display_sync (display);
    }

    return self;
}

/**
 * ccm_window_is_viewable:
 * @self: #CCMWindow
 *
 * Indicate if window is visible
 *
 * Returns: window is visible
 **/
gboolean
ccm_window_is_viewable (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->is_viewable || self->priv->unmap_pending;
}

/**
 * ccm_window_is_input_only:
 * @self: #CCMWindow
 *
 * Indicate if window is input only
 *
 * Returns: window is input only
 **/
gboolean
ccm_window_is_input_only (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->is_input_only;
}

/**
 * ccm_window_is_managed:
 * @self: #CCMWindow
 *
 * Indicate if window is managed by WM
 *
 * Returns: window is managed
 **/
gboolean
ccm_window_is_managed (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return !self->priv->override_redirect;
}

/**
 * ccm_window_get_opaque_region:
 * @self: #CCMWindow
 *
 * Get opaque region of window
 *
 * Returns: const #CCMRegion
 **/
const CCMRegion *
ccm_window_get_opaque_region (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->opaque ? 
		(self->priv->no_undamage_sibling ? NULL : self->priv->opaque): NULL;
}

/**
 * ccm_window_get_opaque_clipbox:
 * @self: #CCMWindow
 * @clipbox: #cairo_rectangle_t
 *
 * Get opaque clipbox of window
 *
 * Returns: TRUE if window have opaque region
 **/
gboolean
ccm_window_get_opaque_clipbox (CCMWindow * self, cairo_rectangle_t * clipbox)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (self->priv->opaque && !ccm_region_empty (self->priv->opaque))
    {
        ccm_region_get_clipbox (self->priv->opaque, clipbox);
        return TRUE;
    }
    return FALSE;
}

void
ccm_window_query_state (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    ccm_window_get_property_async (self,
                                   CCM_WINDOW_GET_CLASS (self)->state_atom,
                                   XA_ATOM, sizeof (Atom));
}

gboolean
ccm_window_set_state (CCMWindow * self, Atom state_atom)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL, FALSE);

    gboolean updated = FALSE;

    if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_shade_atom)
    {
        gboolean old = self->priv->is_shaded;
        self->priv->is_shaded = TRUE;
        updated = old != self->priv->is_shaded;
        ccm_debug_window (self, "IS_SHADED %i", self->priv->is_shaded);
        if (updated)
            ccm_drawable_damage (CCM_DRAWABLE (self));
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_fullscreen_atom)
    {
        gboolean old = self->priv->is_fullscreen;

        self->priv->is_fullscreen = TRUE;
        updated = old != self->priv->is_fullscreen;
        ccm_debug_window (self, "IS_FULLSCREEN %i", self->priv->is_fullscreen);
        if (updated)
        {
            CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
            ccm_drawable_query_geometry (CCM_DRAWABLE (self));
            ccm_screen_damage (screen);
        }
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_is_modal)
    {
        gboolean old = self->priv->is_modal;
        self->priv->is_modal = TRUE;
        updated = old != self->priv->is_modal;
        ccm_debug_window (self, "IS_MODAL %i", self->priv->is_modal);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_skip_taskbar)
    {
        gboolean old = self->priv->skip_taskbar;
        self->priv->skip_taskbar = TRUE;
        updated = old != self->priv->skip_taskbar;
        ccm_debug_window (self, "SKIP_TASKBAR %i", self->priv->skip_taskbar);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_skip_pager)
    {
        gboolean old = self->priv->skip_pager;
        self->priv->skip_pager = TRUE;
        updated = old != self->priv->skip_pager;
        ccm_debug_window (self, "SKIP_PAGER %i", self->priv->skip_pager);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_above_atom)
    {
        gboolean old = self->priv->keep_above;
        self->priv->keep_above = TRUE;
        updated = old != self->priv->keep_above;
        ccm_debug_window (self, "KEEP_ABOVE %i", self->priv->keep_above);
        if (updated)
            ccm_drawable_damage (CCM_DRAWABLE (self));
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_below_atom)
    {
        gboolean old = self->priv->keep_below;
        self->priv->keep_below = TRUE;
        updated = old != self->priv->keep_below;
        ccm_debug_window (self, "KEEP_BELOW %i", self->priv->keep_below);
    }

    if (updated)
        g_signal_emit (self, signals[PROPERTY_CHANGED], 0, CCM_PROPERTY_STATE);

    return updated;
}

void
ccm_window_unset_state (CCMWindow * self, Atom state_atom)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    gboolean updated = FALSE;

    if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_shade_atom)
    {
        gboolean old = self->priv->is_shaded;
        self->priv->is_shaded = FALSE;
        updated = old != self->priv->is_shaded;
        ccm_debug_window (self, "IS_SHADED %i", self->priv->is_shaded);
        if (updated)
            ccm_drawable_damage (CCM_DRAWABLE (self));
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_fullscreen_atom)
    {
        gboolean old = self->priv->is_fullscreen;
        self->priv->is_fullscreen = FALSE;
        updated = old != self->priv->is_fullscreen;
        ccm_debug_window (self, "IS_FULLSCREEN %i", self->priv->is_fullscreen);
        if (updated)
        {
            CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
            ccm_drawable_query_geometry (CCM_DRAWABLE (self));
            ccm_screen_damage (screen);
        }
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_is_modal)
    {
        gboolean old = self->priv->is_modal;
        self->priv->is_modal = FALSE;
        updated = old != self->priv->is_modal;
        ccm_debug_window (self, "IS_MODAL %i", self->priv->is_modal);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_skip_taskbar)
    {
        gboolean old = self->priv->skip_taskbar;
        self->priv->skip_taskbar = FALSE;
        updated = old != self->priv->skip_taskbar;
        ccm_debug_window (self, "SKIP_TASKBAR %i", self->priv->skip_taskbar);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_skip_pager)
    {
        gboolean old = self->priv->skip_pager;
        self->priv->skip_pager = FALSE;
        updated = old != self->priv->skip_pager;
        ccm_debug_window (self, "SKIP_PAGER %i", self->priv->skip_pager);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_above_atom)
    {
        gboolean old = self->priv->keep_above;
        self->priv->keep_above = FALSE;
        updated = old != self->priv->keep_above;
        ccm_debug_window (self, "KEEP_ABOVE %i", self->priv->keep_above);
        if (updated)
            ccm_drawable_damage (CCM_DRAWABLE (self));
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_below_atom)
    {
        gboolean old = self->priv->keep_below;
        self->priv->keep_below = FALSE;
        updated = old != self->priv->keep_below;
        ccm_debug_window (self, "KEEP_BELOW %i", self->priv->keep_below);
    }
    if (updated)
        g_signal_emit (self, signals[PROPERTY_CHANGED], 0, CCM_PROPERTY_STATE);
}

void
ccm_window_switch_state (CCMWindow * self, Atom state_atom)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    gboolean updated = FALSE;

    if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_shade_atom)
    {
        updated = TRUE;
        self->priv->is_shaded = !self->priv->is_shaded;
        ccm_debug_window (self, "IS_SHADED %i", self->priv->is_shaded);
        ccm_drawable_damage (CCM_DRAWABLE (self));
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_fullscreen_atom)
    {
        CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
        updated = TRUE;
        self->priv->is_fullscreen = !self->priv->is_fullscreen;
        ccm_debug_window (self, "IS_FULLSCREEN %i", self->priv->is_fullscreen);
        ccm_drawable_query_geometry (CCM_DRAWABLE (self));
        ccm_screen_damage (screen);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_is_modal)
    {
        updated = TRUE;
        self->priv->is_modal = !self->priv->is_modal;
        ccm_debug_window (self, "IS_MODAL %i", self->priv->is_modal);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_skip_taskbar)
    {
        updated = TRUE;
        self->priv->skip_taskbar = !self->priv->skip_taskbar;
        ccm_debug_window (self, "SKIP_TASKBAR %i", self->priv->skip_taskbar);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_skip_pager)
    {
        updated = TRUE;
        self->priv->skip_pager = !self->priv->skip_pager;
        ccm_debug_window (self, "SKIP_PAGER %i", self->priv->skip_pager);
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_above_atom)
    {
        updated = TRUE;
        self->priv->keep_above = !self->priv->keep_above;
        ccm_debug_window (self, "KEEP_ABOVE %i", self->priv->keep_above);
        ccm_drawable_damage (CCM_DRAWABLE (self));
    }
    else if (state_atom == CCM_WINDOW_GET_CLASS (self)->state_below_atom)
    {
        updated = TRUE;
        self->priv->keep_below = !self->priv->keep_below;
        ccm_debug_window (self, "KEEP_BELOW %i", self->priv->keep_below);
    }

    if (updated)
        g_signal_emit (self, signals[PROPERTY_CHANGED], 0, CCM_PROPERTY_STATE);
}

gboolean
ccm_window_is_shaded (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->is_shaded;
}

gboolean
ccm_window_is_fullscreen (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->is_fullscreen;
}

gboolean
ccm_window_skip_taskbar (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->skip_taskbar;
}

gboolean
ccm_window_skip_pager (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->skip_pager;
}

gboolean
ccm_window_keep_above (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->keep_above;
}

gboolean
ccm_window_keep_below (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->keep_below;
}

static gboolean
ccm_window_traverse_child (CCMWindow * self, Window parent, Window window)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (self != None, FALSE);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    Window *windows = NULL, w, p;
    guint n_windows, cpt;
    gboolean ret = FALSE;

    if (XQueryTree
        (CCM_DISPLAY_XDISPLAY (display),
         parent ? parent : CCM_WINDOW_XWINDOW (self), &w, &p, &windows,
         &n_windows) && windows)
    {
        for (cpt = 0; cpt < n_windows && !ret; ++cpt)
        {
            if (windows[cpt] == window)
                ret = TRUE;
            else
                ret = ccm_window_traverse_child (self, windows[cpt], window);
        }
        XFree (windows);
    }

    return ret;
}

gboolean
ccm_window_is_child (CCMWindow * self, Window window)
{
    return ccm_window_traverse_child (self, None, window);
}

CCMWindow *
ccm_window_transient_for (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    CCMWindow *window = NULL;

    if (self->priv->transient_for)
        window = ccm_screen_find_window_or_child (screen, 
                                                  self->priv->transient_for);
 
    return window;
}

CCMWindow *
ccm_window_get_group_leader (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    CCMWindow *window = NULL;

    if (self->priv->group_leader)
        window =
            ccm_screen_find_window_or_child (screen, self->priv->group_leader);

    return window;
}

void
ccm_window_make_output_only (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    XserverRegion region;

    region = XFixesCreateRegion (CCM_DISPLAY_XDISPLAY (display), 0, 0);
    XFixesSetWindowShapeRegion (CCM_DISPLAY_XDISPLAY (display),
                                CCM_WINDOW_XWINDOW (self), ShapeInput, 0, 0,
                                region);
    XFixesDestroyRegion (CCM_DISPLAY_XDISPLAY (display), region);
}

void
ccm_window_make_input_output (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    XserverRegion region;
    cairo_rectangle_t geometry;

    if (ccm_drawable_get_device_geometry_clipbox
        (CCM_DRAWABLE (self), &geometry))
    {
        XRectangle rect;
        rect.x = geometry.x;
        rect.y = geometry.y;
        rect.width = geometry.width;
        rect.width = geometry.height;
        region = XFixesCreateRegion (CCM_DISPLAY_XDISPLAY (display), &rect, 1);
        XFixesSetWindowShapeRegion (CCM_DISPLAY_XDISPLAY (display),
                                    CCM_WINDOW_XWINDOW (self), ShapeInput, 0, 0,
                                    region);
        XFixesDestroyRegion (CCM_DISPLAY_XDISPLAY (display), region);
    }
}

void
ccm_window_redirect (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    XCompositeRedirectWindow (CCM_DISPLAY_XDISPLAY
                              (ccm_drawable_get_display (CCM_DRAWABLE (self))),
                              CCM_WINDOW_XWINDOW (self),
                              CompositeRedirectManual);
}

void
ccm_window_redirect_subwindows (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    XCompositeRedirectSubwindows (CCM_DISPLAY_XDISPLAY
                                  (ccm_drawable_get_display
                                   (CCM_DRAWABLE (self))),
                                  CCM_WINDOW_XWINDOW (self),
                                  CompositeRedirectManual);
}

void
ccm_window_unredirect (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    XCompositeUnredirectWindow (CCM_DISPLAY_XDISPLAY
                                (ccm_drawable_get_display
                                 (CCM_DRAWABLE (self))),
                                CCM_WINDOW_XWINDOW (self),
                                CompositeRedirectManual);
}

void
ccm_window_unredirect_subwindows (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    XCompositeUnredirectSubwindows (CCM_DISPLAY_XDISPLAY
                                    (ccm_drawable_get_display
                                     (CCM_DRAWABLE (self))),
                                    CCM_WINDOW_XWINDOW (self),
                                    CompositeRedirectManual);
}

CCMPixmap *
ccm_window_get_pixmap (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    if (!self->priv->pixmap || !CCM_IS_PIXMAP (self->priv->pixmap))
    {
        self->priv->pixmap =
            ccm_window_plugin_get_pixmap (self->priv->plugin, self);
        if (self->priv->pixmap)
        {
            g_object_set_qdata_full (G_OBJECT (self->priv->pixmap), 
                                     CCMWindowPixmapQuark,
                                     self,
                                     (GDestroyNotify)
                                     ccm_window_on_pixmap_destroyed);

            // we connect after to be sure an eventually plugin draw before we
            // call main window callback
            g_signal_connect_after (self->priv->pixmap, "damaged",
                                    G_CALLBACK (ccm_window_on_pixmap_damaged),
                                    self);
        }
    }

    return self->priv->pixmap ? g_object_ref (self->priv->pixmap) : NULL;
}

CCMPixmap *
ccm_window_create_pixmap (CCMWindow * self, int width, int height, int depth)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMPixmap *pixmap = NULL;

    if (CCM_WINDOW_GET_CLASS (self)->create_pixmap)
        pixmap =
            CCM_WINDOW_GET_CLASS (self)->create_pixmap (self, width, height,
                                                        depth);

    return pixmap;
}

gfloat
ccm_window_get_opacity (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, 1.0f);

    return self->priv->opacity;
}

void
ccm_window_set_opacity (CCMWindow * self, gfloat opacity)
{
    g_return_if_fail (self != NULL);

    self->priv->opacity = opacity;
    if (self->priv->opacity < 1.0f)
        ccm_window_set_alpha (self);
    else if (ccm_drawable_get_format (CCM_DRAWABLE (self)) !=
             CAIRO_FORMAT_ARGB32)
        ccm_window_set_opaque (self);
    g_signal_emit (self, signals[OPACITY_CHANGED], 0);
}

gboolean
ccm_window_paint (CCMWindow * self, cairo_t * context, gboolean buffered)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (context != NULL, FALSE);

    gboolean ret = FALSE;

    if (!self->priv->is_viewable && !self->priv->unmap_pending
        && !self->priv->is_shaded)
    {
        ccm_drawable_repair (CCM_DRAWABLE (self));
        return ret;
    }

    if (ccm_drawable_is_damaged (CCM_DRAWABLE (self)))
    {
        CCMPixmap *pixmap = ccm_window_get_pixmap (self);

        if (pixmap)
        {
            cairo_surface_t *surface;
            gboolean y_invert = ccm_pixmap_get_y_invert(pixmap);

            if (CCM_IS_PIXMAP_BUFFERED (pixmap))
                g_object_set (pixmap, "buffered", buffered, NULL);

            surface = ccm_drawable_get_surface (CCM_DRAWABLE (pixmap));

            if (surface)
            {
                ccm_debug_window (self, "PAINT");

                cairo_save (context);
                ccm_drawable_get_damage_path (CCM_DRAWABLE (self), context);
                cairo_clip (context);
                ret =
                    ccm_window_plugin_paint (self->priv->plugin, self, context,
                                             surface, y_invert);
                cairo_surface_destroy (surface);
                cairo_restore (context);
            }
            else
            {
                g_object_unref (self->priv->pixmap);
                self->priv->pixmap = NULL;
                g_signal_emit (self, signals[ERROR], 0);
            }
            g_object_unref (pixmap);
        }

    }

    if (ret)
        ccm_drawable_repair (CCM_DRAWABLE (self));

    return ret;
}

void
ccm_window_map (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    if (!self->priv->visible)
    {
        if (!ccm_drawable_get_device_geometry (CCM_DRAWABLE (self)))
            ccm_drawable_query_geometry (CCM_DRAWABLE (self));

        self->priv->visible = TRUE;
        self->priv->is_viewable = TRUE;
        self->priv->unmap_pending = FALSE;

        ccm_debug_window (self, "WINDOW MAP");
        if (CCM_IS_PIXMAP (self->priv->pixmap))
        {
            g_object_unref (self->priv->pixmap);
            self->priv->pixmap = NULL;
        }

        ccm_window_plugin_map (self->priv->plugin, self);
    }
}

void
ccm_window_unmap (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    if (self->priv->visible || self->priv->is_viewable)
    {
        ccm_debug ("UNMAP");
        self->priv->visible = FALSE;
        self->priv->is_viewable = FALSE;
        self->priv->unmap_pending = TRUE;

        if (self->priv->is_fullscreen)
            ccm_window_switch_state (self,
                                     CCM_WINDOW_GET_CLASS (self)->state_fullscreen_atom);
        if (CCM_IS_PIXMAP (self->priv->pixmap))
            g_object_set (self->priv->pixmap, "freeze", TRUE, NULL);
        ccm_debug_window (self, "WINDOW UNMAP");
        ccm_window_plugin_unmap (self->priv->plugin, self);
    }
}

void
ccm_window_query_opacity (CCMWindow * self, gboolean deleted)
{
    g_return_if_fail (self != NULL);

    if (deleted)
    {
        ccm_window_set_opacity (self, 1.0f);
        ccm_drawable_damage (CCM_DRAWABLE (self));
        g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                       CCM_PROPERTY_OPACITY);
    }
    else
        ccm_window_plugin_query_opacity (self->priv->plugin, self);
}

void
ccm_window_query_mwm_hints (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    ccm_debug_window (self, "QUERY MWM HINTS");
    ccm_window_get_property_async (self,
                                   CCM_WINDOW_GET_CLASS (self)->mwm_hints_atom,
                                   AnyPropertyType, sizeof (MotifWmHints));
}

void
ccm_window_query_transient_for (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    ccm_debug_window (self, "QUERY TRANSIENT");
    ccm_window_get_property_async (self,
                                   CCM_WINDOW_GET_CLASS (self)->
                                   transient_for_atom, XA_WINDOW,
                                   sizeof (Window));
}

void
ccm_window_query_hint_type (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    ccm_debug_window (self, "QUERY HINT TYPE");
    ccm_window_get_property_async (self, CCM_WINDOW_GET_CLASS (self)->type_atom,
                                   XA_ATOM, sizeof (Atom));
}

void
ccm_window_query_wm_hints (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    XWMHints *hints;
    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));

    hints =
        XGetWMHints (CCM_DISPLAY_XDISPLAY (display),
                     self->priv->child ? self->priv->
                     child : CCM_WINDOW_XWINDOW (self));
    if (hints)
    {
        Window old = self->priv->group_leader;

        if (hints->flags & WindowGroupHint)
            self->priv->group_leader = hints->window_group;
        if (old != self->priv->group_leader)
            g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                           CCM_PROPERTY_WM_HINTS);

        XFree (hints);
    }
}

CCMWindowType
ccm_window_get_hint_type (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, CCM_WINDOW_TYPE_NORMAL);

    return self->priv->hint_type;
}

const gchar *
ccm_window_get_name (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL, NULL);

    if (self->priv->name == NULL)
    {
        self->priv->name =
            ccm_window_get_utf8_property (self,
                                          CCM_WINDOW_GET_CLASS (self)->
                                          visible_name_atom);

        if (!self->priv->name)
            self->priv->name =
                ccm_window_get_utf8_property (self,
                                              CCM_WINDOW_GET_CLASS (self)->
                                              name_atom);

        if (!self->priv->name)
            self->priv->name = ccm_window_get_text_property (self, XA_WM_NAME);
    }

    if (self->priv->name == NULL)
    {
        self->priv->name =
            ccm_window_get_child_utf8_property (self,
                                                CCM_WINDOW_GET_CLASS (self)->
                                                visible_name_atom);

        if (!self->priv->name)
            self->priv->name =
                ccm_window_get_child_utf8_property (self,
                                                    CCM_WINDOW_GET_CLASS
                                                    (self)->name_atom);

        if (!self->priv->name)
            self->priv->name =
                ccm_window_get_child_text_property (self, XA_WM_NAME);
    }
    return self->priv->name;
}

void
ccm_window_set_alpha (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->opaque)
    {
        ccm_region_destroy (self->priv->opaque);
        self->priv->opaque = NULL;
    }
    if (self->priv->orig_opaque)
    {
        ccm_region_destroy (self->priv->orig_opaque);
        self->priv->orig_opaque = NULL;
    }
}

void
ccm_window_set_opaque_region (CCMWindow * self, const CCMRegion * region)
{
    g_return_if_fail (self != NULL);

    ccm_window_plugin_set_opaque_region (self->priv->plugin, self, region);
}

void
ccm_window_set_opaque (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    const CCMRegion *geometry =
        ccm_drawable_get_device_geometry (CCM_DRAWABLE (self));

    ccm_window_set_alpha (self);
    if (geometry)
        ccm_window_set_opaque_region (self, geometry);
}

gboolean
ccm_window_is_decorated (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, TRUE);

    return self->priv->is_decorated;
}

const cairo_rectangle_t *
ccm_window_get_area (CCMWindow * self)
{
    g_return_val_if_fail (self, NULL);

    return self->priv->area.width <= 0
        || self->priv->area.height <= 0 ? NULL : &self->priv->area;
}

CCMRegion *
ccm_window_get_area_geometry (CCMWindow * self)
{
    g_return_val_if_fail (self, NULL);

    CCMRegion *ret = NULL;
    const CCMRegion *geometry =
        ccm_drawable_get_device_geometry (CCM_DRAWABLE (self));

    if (geometry)
    {
        cairo_rectangle_t clipbox;
        ret = ccm_region_copy ((CCMRegion *) geometry);
        double xoffset, yoffset;
        cairo_matrix_t transform =
            ccm_drawable_get_transform (CCM_DRAWABLE (self));

        ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE (self),
                                                  &clipbox);
        ccm_region_resize (ret, self->priv->area.width,
                           self->priv->area.height);
        ccm_region_device_transform (ret, &transform);
        xoffset = self->priv->area.x - clipbox.x;
        yoffset = self->priv->area.y - clipbox.y;
        cairo_matrix_transform_distance (&transform, &xoffset, &yoffset);
        ccm_region_offset (ret, xoffset, yoffset);
    }

    return ret;
}

void
ccm_window_query_frame_extends (CCMWindow * self)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (CCM_WINDOW_GET_CLASS (self) != NULL);

    guint32 *data = NULL;
    guint n_items;
    gboolean updated = FALSE;
    int left_frame, right_frame, top_frame, bottom_frame;

    if (self->priv->child)
        data =
            ccm_window_get_child_property (self,
                                           CCM_WINDOW_GET_CLASS (self)->
                                           frame_extends_atom, XA_CARDINAL,
                                           &n_items);
    else
        data =
            ccm_window_get_property (self,
                                     CCM_WINDOW_GET_CLASS (self)->
                                     frame_extends_atom, XA_CARDINAL, &n_items);
    if (data)
    {
        gulong *extends = (gulong *) data;

        if (n_items == 4)
        {
            left_frame = (int) extends[0];
            updated |= left_frame != self->priv->frame_left;
            right_frame = (int) extends[1];
            updated |= right_frame != self->priv->frame_right;
            top_frame = (int) extends[2];
            updated |= left_frame != self->priv->frame_top;
            bottom_frame = (int) extends[3];
            updated |= bottom_frame != self->priv->frame_bottom;
        }
        g_free (data);
    }

    if (updated)
    {
        self->priv->frame_left = left_frame;
        self->priv->frame_right = right_frame;
        self->priv->frame_top = top_frame;
        self->priv->frame_bottom = bottom_frame;
        g_signal_emit (self, signals[PROPERTY_CHANGED], 0,
                       CCM_PROPERTY_FRAME_EXTENDS);
    }
}

void
ccm_window_get_frame_extends (CCMWindow * self, int *left_frame,
                              int *right_frame, int *top_frame,
                              int *bottom_frame)
{
    g_return_if_fail (self != NULL);

    *left_frame = self->priv->frame_left;
    *right_frame = self->priv->frame_right;
    *top_frame = self->priv->frame_top;
    *bottom_frame = self->priv->frame_bottom;
}

gboolean
ccm_window_transform (CCMWindow * self, cairo_t * ctx, gboolean y_invert)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (ctx != NULL, FALSE);

    cairo_rectangle_t geometry;

    if (ccm_drawable_get_device_geometry_clipbox
        (CCM_DRAWABLE (self), &geometry))
    {
        cairo_matrix_t matrix;

        matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));
        if (y_invert)
        {
            cairo_matrix_scale (&matrix, 1.0, -1.0);
            cairo_matrix_translate (&matrix, 0.0f, -self->priv->area.height);
        }

        cairo_identity_matrix (ctx);
        cairo_translate (ctx, geometry.x, geometry.y);
        cairo_transform (ctx, &matrix);
    }

    return TRUE;
}

guint32 *
ccm_window_get_property (CCMWindow * self, Atom property_atom, Atom req_type,
                         guint * n_items)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (property_atom != None, NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    int ret;
    Atom type;
    int format;
    gulong n_items_internal;
    guchar *property = NULL;
    gulong bytes_after;
    guint32 *result;

    ret =
        XGetWindowProperty (CCM_DISPLAY_XDISPLAY (display),
                            CCM_WINDOW_XWINDOW (self), property_atom, 0,
                            G_MAXLONG, False, req_type, &type, &format,
                            &n_items_internal, &bytes_after, &property);

    if (ret != Success)
    {
        ccm_debug ("ERROR GET  PROPERTY = %i", ret);
        if (property)
            XFree (property);
        g_signal_emit (self, signals[ERROR], 0);
        return NULL;
    }
    ccm_debug ("PROPERTY = 0x%x, %i", property, n_items_internal);

    result = g_memdup (property, n_items_internal * sizeof (gulong));
    XFree (property);

    if (n_items)
        *n_items = n_items_internal;

    return result;
}

guint32 *
ccm_window_get_child_property (CCMWindow * self, Atom property_atom,
                               Atom req_type, guint * n_items)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (property_atom != None, NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    int ret;
    Atom type;
    int format;
    gulong n_items_internal;
    guchar *property = NULL;
    gulong bytes_after;
    guint32 *result;

    if (!self->priv->child)
        return NULL;

    ret =
        XGetWindowProperty (CCM_DISPLAY_XDISPLAY (display), self->priv->child,
                            property_atom, 0, G_MAXLONG, False, req_type, &type,
                            &format, &n_items_internal, &bytes_after,
                            &property);

    if (ret != Success)
    {
        ccm_debug ("ERROR GET  PROPERTY = %i", ret);
        g_signal_emit (self, signals[ERROR], 0);
        if (property)
            XFree (property);
        return NULL;
    }

    ccm_debug ("PROPERTY = 0x%x, %i", property, n_items_internal);
    result = g_memdup (property, n_items_internal * sizeof (gulong));
    XFree (property);

    if (n_items)
        *n_items = n_items_internal;

    return result;
}

gboolean
ccm_window_has_redirect_input (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    return self->priv->input != None;
}

void
ccm_window_redirect_input (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    if (!self->priv->input && self->priv->redirect)
    {
        CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
        CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
        CCMWindow *root = ccm_screen_get_root_window (screen);
        const CCMRegion *geometry, *device;
        CCMRegion *area;
        cairo_rectangle_t clipbox;
        XSetWindowAttributes attr;

        device = ccm_drawable_get_device_geometry (CCM_DRAWABLE (self));
        geometry = ccm_drawable_get_geometry (CCM_DRAWABLE (self));
        if (device && geometry && !ccm_region_empty ((CCMRegion *) device))
        {
            area = ccm_region_copy ((CCMRegion *) device);
            ccm_region_union (area, (CCMRegion *) geometry);
            ccm_region_get_clipbox (area, &clipbox);

            attr.override_redirect = True;
            self->priv->input =
                XCreateWindow (CCM_DISPLAY_XDISPLAY (display),
                               CCM_WINDOW_XWINDOW (root), clipbox.x, clipbox.y,
                               clipbox.width, clipbox.height, 0, CopyFromParent,
                               InputOnly, CopyFromParent, CWOverrideRedirect,
                               &attr);

            if (self->priv->input)
            {
                XRectangle *rects;
                gint nb_rects;
                XserverRegion region;
                XWindowChanges xwc;

                ccm_region_offset (area, -clipbox.x, -clipbox.y);
                XMapWindow (CCM_DISPLAY_XDISPLAY (display), self->priv->input);
                XSelectInput (CCM_DISPLAY_XDISPLAY (display), self->priv->input,
                              ButtonPressMask | ButtonReleaseMask |
                              ButtonMotionMask | Button1MotionMask |
                              Button2MotionMask | Button3MotionMask |
                              Button4MotionMask | Button5MotionMask |
                              PointerMotionMask);

                ccm_region_get_xrectangles (area, &rects, &nb_rects);
                region =
                    XFixesCreateRegion (CCM_DISPLAY_XDISPLAY (display), rects,
                                        nb_rects);
                XFixesSetWindowShapeRegion (CCM_DISPLAY_XDISPLAY (display),
                                            self->priv->input, ShapeInput, 0, 0,
                                            region);
                XFixesDestroyRegion (CCM_DISPLAY_XDISPLAY (display), region);
                x_rectangles_free (rects, nb_rects);
                ccm_region_destroy (area);

                xwc.stack_mode = Above;
                xwc.sibling =
                    self->priv->child ? self->priv->
                    child : CCM_WINDOW_XWINDOW (self);
                XConfigureWindow (CCM_DISPLAY_XDISPLAY (display),
                                  self->priv->input, CWSibling | CWStackMode,
                                  &xwc);

                g_signal_emit (self, signals[REDIRECT_INPUT], 0, TRUE);
            }
        }
    }
}

void
ccm_window_unredirect_input (CCMWindow * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->input)
    {
        CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
        XDestroyWindow (CCM_DISPLAY_XDISPLAY (display), self->priv->input);
        self->priv->input = None;
        g_signal_emit (self, signals[REDIRECT_INPUT], 0, FALSE);
    }
}

void
ccm_window_activate (CCMWindow * self, Time timestamp)
{
    g_return_if_fail (self != NULL);

    CCMDisplay *display = ccm_drawable_get_display (CCM_DRAWABLE (self));
    CCMScreen *screen = ccm_drawable_get_screen (CCM_DRAWABLE (self));
    CCMWindow *root = ccm_screen_get_root_window (screen);

    ccm_debug_window (self, "ACTIVATE");

    XChangeProperty (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (self),
                     CCM_WINDOW_GET_CLASS (root)->user_time_atom, XA_CARDINAL,
                     32, PropModeReplace, (guchar *) & timestamp, 1);

    if (self->priv->child)
    {
        XClientMessageEvent event;

        bzero (&event, sizeof (XClientMessageEvent));
        event.type = ClientMessage;
        event.display = CCM_DISPLAY_XDISPLAY (display);
        event.window = self->priv->child;
        event.message_type = CCM_WINDOW_GET_CLASS (self)->active_atom;
        event.format = 32;
        event.data.l[0] = 2;
        event.data.l[1] = timestamp;
        event.data.l[2] = None;

        XSendEvent (CCM_DISPLAY_XDISPLAY (display), CCM_WINDOW_XWINDOW (root),
                    False, SubstructureRedirectMask | SubstructureNotifyMask,
                    (XEvent *) & event);
    }
    else
    {
        XSetInputFocus (CCM_DISPLAY_XDISPLAY (display),
                        CCM_WINDOW_XWINDOW (self), RevertToParent, timestamp);
    }
}

Window
ccm_window_redirect_event (CCMWindow * self, XEvent * event, Window over)
{
    g_return_val_if_fail (self != NULL, over);
    g_return_val_if_fail (event != NULL, over);

    switch (event->type)
    {
        case ButtonPress:
        case ButtonRelease:
            {
                int x, y;
                int x_root = event->xbutton.x_root;
                int y_root = event->xbutton.y_root;
                double x_real, y_real;
                cairo_matrix_t matrix;

                // Transfom x,y root point
                ccm_window_plugin_get_origin (self->priv->plugin, self, &x, &y);
                matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));
                x_real = event->xbutton.x_root - x;
                y_real = event->xbutton.y_root - y;
                if (cairo_matrix_invert (&matrix) != CAIRO_STATUS_SUCCESS)
                    return None;
                cairo_matrix_transform_point (&matrix, &x_real, &y_real);
                event->xbutton.x = x_real;
                event->xbutton.y = y_real;
                event->xbutton.x_root = x_real + x;
                event->xbutton.y_root = y_real + y;

                // Set main window as default destination
                event->xbutton.window = CCM_WINDOW_XWINDOW (self);
                event->xbutton.subwindow = None;

                // Search the final destination in child
                ccm_window_translate_coordinates (self, event, 0, 0);

                if (event->type == ButtonPress && !self->priv->transient_for)
                {
                    ccm_window_activate (self, ++event->xbutton.time);
                }
                if (over != event->xbutton.window)
                {
                    if (over)
                    {
                        int x_offset = 0, y_offset = 0;
                        CCMWindow *last =
                            ccm_window_get_child_offset (self, over,
                                                         &x_offset,
                                                         &y_offset);
                        if (last)
                        {
                            XEvent *leave = g_memdup (event, sizeof (XEvent));

                            ccm_window_plugin_get_origin (last->priv->plugin,
                                                          last, &x, &y);
                            matrix =
                                ccm_drawable_get_transform (CCM_DRAWABLE
                                                            (last));
                            x_real = x_root - x;
                            y_real = y_root - y;
                            if (cairo_matrix_invert (&matrix) !=
                                CAIRO_STATUS_SUCCESS)
                                return None;

                            cairo_matrix_transform_point (&matrix, &x_real,
                                                          &y_real);
                            leave->xbutton.x_root = x + x_real;
                            leave->xbutton.y_root = y + y_real;
                            leave->xbutton.x = x_real - x_offset;
                            leave->xbutton.y = y_real - y_offset;
                            if (!self->priv->block_mouse_redirect_event)
                                ccm_window_send_enter_leave_event (last, over,
                                                                   leave,
                                                                   FALSE);

                            ccm_debug ("LEAVE 0x%x", over);

                            ccm_debug ("LEAVE  %i,%i %i,%i",
                                       leave->xbutton.x_root,
                                       leave->xbutton.y_root, leave->xbutton.x,
                                       leave->xbutton.y);
                            g_free (leave);
                            ++event->xbutton.time;
                        }
                    }
                    if (!self->priv->block_mouse_redirect_event)
                        ccm_window_send_enter_leave_event (self,
                                                           event->xbutton.
                                                           window, event, TRUE);
                }
                ccm_debug ("WINDOW %s, 0x%x", ccm_window_get_name (self),
                           event->xbutton.window);
                ccm_debug ("REDIRECT %i,%i %i,%i", event->xbutton.x_root,
                           event->xbutton.y_root, event->xbutton.x,
                           event->xbutton.y);
                ++event->xbutton.time;
                if (!self->priv->block_mouse_redirect_event)
                    XSendEvent (event->xany.display, event->xbutton.window,
                                False, NoEventMask, event);
            }
            break;
        case MotionNotify:
            {
                int x, y;
                int x_root = event->xmotion.x_root;
                int y_root = event->xmotion.y_root;
                double x_real, y_real;
                cairo_matrix_t matrix;

                // Transfom x,y root point
                ccm_window_plugin_get_origin (self->priv->plugin, self, &x, &y);
                matrix = ccm_drawable_get_transform (CCM_DRAWABLE (self));
                x_real = event->xmotion.x_root - x;
                y_real = event->xmotion.y_root - y;
                if (cairo_matrix_invert (&matrix) != CAIRO_STATUS_SUCCESS)
                    return None;
                cairo_matrix_transform_point (&matrix, &x_real, &y_real);
                event->xmotion.x = x_real;
                event->xmotion.y = y_real;
                event->xmotion.x_root = x_real + x;
                event->xmotion.y_root = y_real + y;

                // Set main window as default destination
                event->xmotion.window = CCM_WINDOW_XWINDOW (self);
                event->xmotion.subwindow = None;

                // Search the final destination in child
                ccm_window_translate_coordinates (self, event, 0, 0);

                if (over != event->xmotion.window)
                {
                    if (over)
                    {
                        int x_offset = 0, y_offset = 0;
                        CCMWindow *last =
                            ccm_window_get_child_offset (self, over,
                                                         &x_offset,
                                                         &y_offset);
                        if (last)
                        {
                            XEvent *leave = g_memdup (event, sizeof (XEvent));

                            ccm_window_plugin_get_origin (last->priv->plugin,
                                                          last, &x, &y);
                            matrix =
                                ccm_drawable_get_transform (CCM_DRAWABLE
                                                            (last));
                            x_real = x_root - x;
                            y_real = y_root - y;
                            if (cairo_matrix_invert (&matrix) !=
                                CAIRO_STATUS_SUCCESS)
                                return None;

                            cairo_matrix_transform_point (&matrix, &x_real,
                                                          &y_real);
                            leave->xbutton.x_root = x + x_real;
                            leave->xbutton.y_root = y + y_real;
                            leave->xbutton.x = x_real - x_offset;
                            leave->xbutton.y = y_real - y_offset;
                            if (!self->priv->block_mouse_redirect_event)
                                ccm_window_send_enter_leave_event (last, over,
                                                                   leave,
                                                                   FALSE);
                            ccm_debug ("LEAVE 0x%x", over);

                            ccm_debug ("LEAVE  %i,%i %i,%i",
                                       leave->xmotion.x_root,
                                       leave->xmotion.y_root, leave->xmotion.x,
                                       leave->xmotion.y);
                            g_free (leave);
                            ++event->xmotion.time;
                        }
                    }
                    if (!self->priv->block_mouse_redirect_event)
                        ccm_window_send_enter_leave_event (self,
                                                           event->xbutton.
                                                           window, event, TRUE);
                }
                ccm_debug ("WINDOW %s, 0x%x", ccm_window_get_name (self),
                           event->xmotion.window);
                ccm_debug ("REDIRECT %i,%i %i,%i", event->xmotion.x_root,
                           event->xmotion.y_root, event->xmotion.x,
                           event->xmotion.y);
                ++event->xmotion.time;
                if (!self->priv->block_mouse_redirect_event)
                    XSendEvent (event->xany.display, event->xmotion.window,
                                False, NoEventMask, event);
            }
            break;
        default:
            break;
    }

    return event->xbutton.window;
}

GSList *
ccm_window_get_transients (CCMWindow * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->transients;
}

cairo_surface_t*
ccm_window_get_mask(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->priv->mask;
}

void
ccm_window_set_mask(CCMWindow* self, cairo_surface_t* mask)
{
	g_return_if_fail(self != NULL);

	if (self->priv->mask)
		cairo_surface_destroy (self->priv->mask);
    self->priv->mask = mask;

	g_object_notify(G_OBJECT(self), "mask");
}

gboolean 
ccm_window_get_redirect(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	return self->priv->redirect;
}

void
ccm_window_set_redirect(CCMWindow* self, gboolean redirect)
{
	g_return_if_fail(self != NULL);

	self->priv->redirect = redirect;
	
	if (!self->priv->redirect) ccm_window_unredirect_input (self);

	g_object_notify(G_OBJECT(self), "redirect");
}

gboolean
ccm_window_get_no_undamage_sibling(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	return self->priv->no_undamage_sibling;
}

void
ccm_window_set_no_undamage_sibling(CCMWindow* self, gboolean no_undamage)
{
	g_return_if_fail(self != NULL);

	self->priv->no_undamage_sibling = no_undamage;

	g_object_notify(G_OBJECT(self), "no-undamage-sibling");
}
