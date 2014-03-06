/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-screen.c
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/Xrandr.h>
#include <GL/glx.h>
#include <gdk/gdk.h>
#include <strings.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-screen.h"
#include "ccm-screen-plugin.h"
#include "ccm-display.h"
#include "ccm-window.h"
#include "ccm-pixmap.h"
#include "ccm-drawable.h"
#include "ccm-extension-loader.h"
#include "ccm-keybind.h"
#include "ccm-timeline.h"
#include "ccm-marshallers.h"

#include "ccm-window-xrender.h"
#include "ccm-pixmap-xrender.h"
#include "ccm-pixmap-buffered-image.h"
#include "ccm-pixmap-image.h"

#define DEFAULT_PLUGINS "perf,stats,snapshot,mosaic,freeze,window-animation,menu-animation,shadow,fade,opacity,clone"

typedef gint (*WaitVideoSyncFunc) (gint, gint, guint*);
typedef gint (*GetVideoSyncFunc) (guint*);

enum
{
    PROP_0,
    PROP_DISPLAY,
    PROP_NUMBER,
    PROP_REFRESH_RATE,
    PROP_CURRENT_FRAME,
    PROP_WINDOW_PLUGINS
};

enum
{
    PLUGINS_CHANGED,
    REFRESH_RATE_CHANGED,
    WINDOW_DESTROYED,
    ENTER_WINDOW_NOTIFY,
    LEAVE_WINDOW_NOTIFY,
    ACTIVATE_WINDOW_NOTIFY,
    COMPOSITE_MESSAGE,
    DESKTOP_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static void     ccm_screen_iface_init                 (CCMScreenPluginClass* iface);

static void     ccm_screen_update_backend             (CCMScreen* self);
static GSList*  ccm_screen_get_window_plugins         (CCMScreen* self);
static void     ccm_screen_paint                      (CCMScreen* self, int num_frame, CCMTimeline* timeline);
static void     ccm_screen_unset_selection_owner      (CCMScreen* self);
static void     ccm_screen_on_window_error            (CCMScreen* self, CCMWindow* window);
static void     ccm_screen_on_window_property_changed (CCMScreen* self, CCMPropertyType changed, CCMWindow* window);
static void     ccm_screen_on_window_redirect_input   (CCMScreen* self, gboolean redirected, CCMWindow* window);

CCMWindow*      ccm_screen_find_window_from_input     (CCMScreen* self, Window xwindow);

G_DEFINE_TYPE_EXTENDED (CCMScreen, ccm_screen, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (CCM_TYPE_SCREEN_PLUGIN, ccm_screen_iface_init))
enum
{
    CCM_SCREEN_BACKEND,
    CCM_SCREEN_PIXMAP,
    CCM_SCREEN_USE_BUFFERED,
    CCM_SCREEN_PLUGINS,
    CCM_SCREEN_AUTO_REFRESH_RATE,
    CCM_SCREEN_REFRESH_RATE,
    CCM_SCREEN_SYNC_WITH_VBLANK,
    CCM_SCREEN_USE_ROOT_BACKGROUND,
    CCM_SCREEN_BACKGROUND,
    CCM_SCREEN_COLOR_BACKGROUND,
    CCM_SCREEN_BACKGROUND_X,
    CCM_SCREEN_BACKGROUND_Y,
    CCM_SCREEN_OPTION_N
};

static gchar *CCMScreenOptions[CCM_SCREEN_OPTION_N] = {
    "backend",
    "native_pixmap_bind",
    "use_buffered_pixmap",
    "plugins",
    "auto_refresh_rate",
    "refresh_rate",
    "sync_with_vblank",
    "use_root_background",
    "background",
    "color_background",
    "background_x",
    "background_y"
};

struct _CCMScreenPrivate
{
    CCMDisplay*         display;
    Screen*             xscreen;
    guint               number;

    CCMSet*             damages;

    cairo_t*            ctx;

    CCMWindow*          root;
    CCMWindow*          cow;
    Window              selection_owner;
    CCMWindow*          fullscreen;
    CCMWindow*          active;
    CCMWindow*          sibling_mouse;
    Window              over_mouse;

    CCMRegion*          geometry;
    CCMRegion*          primary_geometry;
    CCMRegion*          damaged;
    CCMRegion*          root_damage;

    Window*             stack;
    guint               n_windows;
    GList*              windows;
    GList*              last_windows;
    GList*              removed;
    gboolean            redirect_input;
    gint                nb_redirect_input;

    gboolean            sync_with_vblank;
    Window              vblank_window;
    GLXContext          vblank_context;
    WaitVideoSyncFunc   wait_video_sync;
    GetVideoSyncFunc    get_video_sync;
    guint               refresh_rate;
    CCMTimeline*        paint;
    guint               id_pendings;

    CCMExtensionLoader* plugin_loader;
    CCMScreenPlugin*    plugin;

    CCMPixmap*          background;

    CCMConfig*          options[CCM_SCREEN_OPTION_N];
};

#define CCM_SCREEN_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SCREEN, CCMScreenPrivate))

static gboolean impl_ccm_screen_paint           (CCMScreenPlugin* plugin, CCMScreen* self, cairo_t* ctx);
static gboolean impl_ccm_screen_add_window      (CCMScreenPlugin* plugin, CCMScreen* self, CCMWindow* window);
static void     impl_ccm_screen_remove_window   (CCMScreenPlugin* plugin, CCMScreen* self, CCMWindow* window);
static void     impl_ccm_screen_damage          (CCMScreenPlugin* plugin, CCMScreen* self, CCMRegion* area, CCMWindow* window);

static void     ccm_screen_on_window_damaged    (CCMScreen* self, CCMRegion* area, CCMWindow* window);
static void     ccm_screen_on_option_changed    (CCMScreen* self, CCMConfig* config);

static int
_direct_compare (int inA, int inB)
{
    return inA - inB;
}

static void
ccm_screen_set_property (GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    CCMScreenPrivate *priv = CCM_SCREEN_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_DISPLAY:
            {
                priv->display = g_value_get_pointer (value);
            }
            break;
        case PROP_NUMBER:
            {
                priv->number = g_value_get_uint (value);
                priv->xscreen = ScreenOfDisplay (CCM_DISPLAY_XDISPLAY (priv->display), priv->number);
            }
            break;
        default:
            break;
    }
}

static void
ccm_screen_get_property (GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    CCMScreenPrivate *priv = CCM_SCREEN_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_DISPLAY:
            {
                g_value_set_pointer (value, priv->display);
            }
            break;
        case PROP_NUMBER:
            {
                g_value_set_uint (value, priv->number);
            }
            break;
        case PROP_REFRESH_RATE:
            {
                g_value_set_uint (value, priv->refresh_rate);
            }
            break;
        case PROP_CURRENT_FRAME:
            {
                g_value_set_uint (value, ccm_timeline_get_current_frame (priv->paint));
            }
            break;
        case PROP_WINDOW_PLUGINS:
            {
                g_value_set_pointer (value, ccm_screen_get_window_plugins (CCM_SCREEN (object)));
            }
            break;
        default:
            break;
    }
}

static void
ccm_screen_init (CCMScreen* self)
{
    self->priv = CCM_SCREEN_GET_PRIVATE (self);

    self->priv->display = NULL;
    self->priv->xscreen = NULL;
    self->priv->number = 0;
    self->priv->ctx = NULL;
    self->priv->root = NULL;
    self->priv->cow = NULL;
    self->priv->damages = ccm_set_new (G_TYPE_INT, NULL, NULL, (CCMSetCompareFunc)_direct_compare);
    self->priv->selection_owner = None;
    self->priv->fullscreen = NULL;
    self->priv->active = NULL;
    self->priv->sibling_mouse = NULL;
    self->priv->over_mouse = None;
    self->priv->geometry = NULL;
    self->priv->damaged = NULL;
    self->priv->root_damage = NULL;
    self->priv->stack = NULL;
    self->priv->n_windows = 0;
    self->priv->windows = NULL;
    self->priv->last_windows = NULL;
    self->priv->removed = NULL;
    self->priv->redirect_input = FALSE;
    self->priv->nb_redirect_input = 0;
    self->priv->refresh_rate = 0;
    self->priv->sync_with_vblank = FALSE;
    self->priv->vblank_context = NULL;
    self->priv->wait_video_sync = NULL;
    self->priv->get_video_sync = NULL;
    self->priv->vblank_window = None;
    self->priv->paint = NULL;
    self->priv->id_pendings = 0;
    self->priv->plugin_loader = NULL;
    self->priv->plugin = NULL;
    self->priv->background = NULL;
}

static void
ccm_screen_finalize (GObject * object)
{
    CCMScreen *self = CCM_SCREEN (object);
    gint cpt;

    ccm_debug ("FINALIZE SCREEN");

    ccm_screen_unset_selection_owner (self);

    if (self->priv->plugin && CCM_IS_PLUGIN (self->priv->plugin))
        g_object_unref (self->priv->plugin);

    if (self->priv->plugin_loader)
        g_object_unref (self->priv->plugin_loader);

    for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
        g_object_unref (self->priv->options[cpt]);

    if (self->priv->id_pendings)
        g_source_remove (self->priv->id_pendings);

    if (self->priv->paint)
    {
        ccm_timeline_stop (self->priv->paint);
        g_object_unref (self->priv->paint);
    }

    if (self->priv->ctx)
        cairo_destroy (self->priv->ctx);

    if (self->priv->stack)
    {
        g_slice_free1 (sizeof (Window) * self->priv->n_windows, self->priv->stack);
        self->priv->stack = NULL;
        self->priv->n_windows = 0;
    }
    if (self->priv->windows)
    {
        GList *tmp = self->priv->windows;
        self->priv->windows = NULL;
        g_list_foreach (tmp, (GFunc) g_object_unref, NULL);
        g_list_free (tmp);
    }
    if (self->priv->removed)
    {
        g_list_foreach (self->priv->removed, (GFunc) g_object_unref, NULL);
        g_list_free (self->priv->removed);
    }

    if (self->priv->geometry)
        ccm_region_destroy (self->priv->geometry);
    if (self->priv->damaged)
        ccm_region_destroy (self->priv->damaged);
    if (self->priv->root_damage)
        ccm_region_destroy (self->priv->root_damage);

    if (self->priv->vblank_window != None)
    {
        XDestroyWindow(CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->vblank_window);
        self->priv->vblank_window = None;
    }

#if HAVE_VSYNC
    if (self->priv->vblank_context != NULL)
    {
        glXDestroyContext(CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->vblank_context);
        self->priv->vblank_context = NULL;
    }
#endif

    if (self->priv->cow)
    {
        XCompositeReleaseOverlayWindow (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                        CCM_WINDOW_XWINDOW (self->priv->cow));
        ccm_display_sync (self->priv->display);
        g_object_unref (self->priv->cow);
    }

    if (self->priv->root)
    {
        ccm_window_unredirect_subwindows (self->priv->root);
        ccm_display_sync (self->priv->display);
        g_object_unref (self->priv->root);
    }

    if (self->priv->background)
        g_object_unref (self->priv->background);

    if (self->priv->damages)
        g_object_unref (self->priv->damages);

    G_OBJECT_CLASS (ccm_screen_parent_class)->finalize (object);
}

static void
ccm_screen_class_init (CCMScreenClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMScreenPrivate));

    object_class->get_property = ccm_screen_get_property;
    object_class->set_property = ccm_screen_set_property;
    object_class->finalize = ccm_screen_finalize;

    g_object_class_install_property (object_class, PROP_DISPLAY,
                                     g_param_spec_pointer ("display", "Display",
                                                           "Display of screen",
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class, PROP_NUMBER,
                                     g_param_spec_uint ("number", "Number",
                                                        "Screen number", 0,
                                                        G_MAXUINT, None,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class, PROP_REFRESH_RATE,
                                     g_param_spec_uint ("refresh_rate",
                                                        "RefreshRate",
                                                        "Screen paint refresh rate",
                                                        0, G_MAXUINT, None,
                                                        G_PARAM_READABLE));

    g_object_class_install_property (object_class, PROP_CURRENT_FRAME,
                                     g_param_spec_uint ("current_frame",
                                                        "Current frame",
                                                        "Current frame number",
                                                        0, G_MAXUINT, None,
                                                        G_PARAM_READABLE));

    g_object_class_install_property (object_class, PROP_WINDOW_PLUGINS,
                                     g_param_spec_pointer ("window_plugins",
                                                           "WindowPlugins",
                                                           "Window plugins list",
                                                           G_PARAM_READABLE));

    signals[PLUGINS_CHANGED] =
        g_signal_new ("plugins-changed", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0,
                      G_TYPE_NONE);

    signals[REFRESH_RATE_CHANGED] =
        g_signal_new ("refresh-rate-changed",
                      G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_LAST, 0,
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0,
                      G_TYPE_NONE);

    signals[WINDOW_DESTROYED] =
        g_signal_new ("window-destroyed", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0,
                      G_TYPE_NONE);

    signals[ENTER_WINDOW_NOTIFY] =
        g_signal_new ("enter-window-notify", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,
                      G_TYPE_POINTER);

    signals[LEAVE_WINDOW_NOTIFY] =
        g_signal_new ("leave-window-notify", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,
                      G_TYPE_POINTER);

    signals[ACTIVATE_WINDOW_NOTIFY] =
        g_signal_new ("activate-window-notify",
                      G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_LAST, 0,
                      NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE,
                      1, G_TYPE_POINTER);

    signals[COMPOSITE_MESSAGE] =
        g_signal_new ("composite-message", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      ccm_cclosure_marshal_VOID__OBJECT_OBJECT_LONG_LONG_LONG,
                      G_TYPE_NONE, 5, CCM_TYPE_WINDOW, CCM_TYPE_WINDOW,
                      G_TYPE_LONG, G_TYPE_LONG, G_TYPE_LONG);

    signals[DESKTOP_CHANGED] =
        g_signal_new ("desktop-changed", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
ccm_screen_iface_init (CCMScreenPluginClass * iface)
{
    iface->is_screen = TRUE;
    iface->load_options = NULL;
    iface->paint = impl_ccm_screen_paint;
    iface->add_window = impl_ccm_screen_add_window;
    iface->remove_window = impl_ccm_screen_remove_window;
    iface->damage = impl_ccm_screen_damage;
}

static void
ccm_screen_update_background (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    gboolean use_root_background =
        ccm_config_get_boolean (self->priv->options[CCM_SCREEN_USE_ROOT_BACKGROUND],
                                NULL);
    gchar *filename =
        ccm_config_get_string (self->priv->options[CCM_SCREEN_BACKGROUND],
                               NULL);
    GdkColor *color =
        ccm_config_get_color (self->priv->options[CCM_SCREEN_COLOR_BACKGROUND],
                              NULL);

    if (use_root_background)
    {
        guint32 *data = NULL;
        guint n_items;

        if (self->priv->background)
            g_object_unref (self->priv->background);
        self->priv->background = NULL;

        data =
            ccm_window_get_property (self->priv->root,
                                     CCM_WINDOW_GET_CLASS (self->priv->root)->root_pixmap_atom,
                                     XA_PIXMAP, &n_items);
        if (data)
        {
            self->priv->background =
                ccm_pixmap_new (CCM_DRAWABLE (self->priv->root),
                                (Pixmap) * data);
            g_object_set (G_OBJECT (self->priv->background), "foreign", TRUE,
                          NULL);
            g_free (data);
        }
    }
    else if (filename && color)
    {
        cairo_t *ctx;
        int x, y;
        GError *error = NULL;

        x = ccm_config_get_integer (self->priv->options[CCM_SCREEN_BACKGROUND_X],
                                    &error);
        if (error)
        {
            g_warning ("Errror on get background x pos");
            g_error_free (error);
            g_free (filename);
            g_free (color);
            return;
        }
        y = ccm_config_get_integer (self->priv->options[CCM_SCREEN_BACKGROUND_Y],
                                    &error);
        if (error)
        {
            g_warning ("Errror on get background y pos");
            g_error_free (error);
            g_free (filename);
            g_free (color);
            return;
        }

        if (self->priv->background)
            g_object_unref (self->priv->background);

        self->priv->background =
            ccm_window_create_pixmap (self->priv->root,
                                      self->priv->xscreen->width,
                                      self->priv->xscreen->height, 32);
        ctx = ccm_drawable_create_context (CCM_DRAWABLE (self->priv->background));
        if (ctx)
        {
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
            cairo_rectangle_t area;

            area.x = 0;
            area.y = 0;
            area.width = self->priv->xscreen->width;
            area.height = self->priv->xscreen->height;

            cairo_set_operator (ctx, CAIRO_OPERATOR_OVER);
            gdk_cairo_set_source_color (ctx, (GdkColor *) color);
            cairo_rectangle (ctx, area.x, area.y, area.width, area.height);
            cairo_fill (ctx);

            if (pixbuf)
            {
                int width = gdk_pixbuf_get_width (pixbuf), height =
                    gdk_pixbuf_get_height (pixbuf);

                cairo_translate (ctx, x ? x : (area.width - width) / 2,
                                 y ? y : (area.height - height) / 2);
                gdk_cairo_set_source_pixbuf (ctx, pixbuf, 0, 0);
                g_object_unref (pixbuf);
                cairo_paint (ctx);
            }
            ccm_drawable_damage (CCM_DRAWABLE (self->priv->background));
            cairo_destroy (ctx);

            ccm_screen_damage (self);
            self->priv->root_damage = ccm_region_rectangle (&area);
        }
        else if (self->priv->background)
        {
            g_object_unref (self->priv->background);
            self->priv->background = NULL;
        }
    }
    if (filename)
        g_free (filename);
    if (color)
        g_free (color);
}

static void
ccm_screen_update_backend (CCMScreen * self)
{
    GError *error = NULL;

    gchar* backend = ccm_config_get_string (self->priv->options[CCM_SCREEN_BACKEND], &error);
    if (error)
    {
        g_warning ("Error on get backend value %s", error ? error->message : "");
        if (error) g_error_free (error);
        error = NULL;
        backend = g_strdup ("xrender");
    }

    gboolean native_pixmap_bind = ccm_config_get_boolean (self->priv->options[CCM_SCREEN_PIXMAP], &error);
    if (error)
    {
        g_warning ("Error on get native backend conf : %s", error->message);
        g_error_free (error);
        error = NULL;
        native_pixmap_bind = TRUE;
     }

    gboolean use_buffered = ccm_config_get_boolean (self->priv->options[CCM_SCREEN_USE_BUFFERED], &error);
    if (error)
    {
        g_warning ("Error on get use buffered conf : %s", error->message);
        g_error_free (error);
        error = NULL;
        use_buffered = TRUE;
    }

    ccm_object_unregister (CCM_TYPE_WINDOW);
    ccm_object_unregister (CCM_TYPE_PIXMAP);

    //if (!g_ascii_strcasecmp (backend, "xrender"))
    {
        ccm_object_register (CCM_TYPE_WINDOW, CCM_TYPE_WINDOW_X_RENDER);

        if (native_pixmap_bind)
            ccm_object_register (CCM_TYPE_PIXMAP, CCM_TYPE_PIXMAP_XRENDER);
        else if (use_buffered)
            ccm_object_register (CCM_TYPE_PIXMAP, CCM_TYPE_PIXMAP_BUFFERED_IMAGE);
        else
            ccm_object_register (CCM_TYPE_PIXMAP, CCM_TYPE_PIXMAP_IMAGE);
    }
}

static gboolean
ccm_screen_update_refresh_rate (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    gboolean have_randr;
    gboolean auto_refresh_rate;
    guint refresh_rate = 0;
    GError *error = NULL;

    auto_refresh_rate = ccm_config_get_boolean (self->priv->options[CCM_SCREEN_AUTO_REFRESH_RATE],
                                                &error);
    if (error)
    {
        g_warning ("Error on get auto refresh rate configuration");
        g_error_free (error);
        auto_refresh_rate = FALSE;
        error = NULL;
    }

    if (auto_refresh_rate)
    {
        g_object_get(G_OBJECT(self->priv->display), "use_randr", &have_randr, NULL);
        if (have_randr)
        {
            XRRScreenResources *res;

            res  = XRRGetScreenResources (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                          RootWindowOfScreen (self->priv->xscreen));

            if (res)
            {
                int i, j;
                for (i = 0; i < res->noutput; ++i)
                {
                    XRROutputInfo* output;
                    XRRCrtcInfo* crtc;

                    output = XRRGetOutputInfo (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                               res, res->outputs[i]);

                    if (output == NULL) continue;

                    if (output->connection != RR_Connected)
                    {
                        XRRFreeOutputInfo (output);
                        continue;
                    }

                    crtc = XRRGetCrtcInfo (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                           res, output->crtc);
                    XRRFreeOutputInfo (output);

                    if (crtc == NULL) continue;

                    for (j = 0; j < res->nmode; ++j)
                    {
                        if (res->modes[j].id == crtc->mode)
                        {
                            int rate = (int)ceil ((double)res->modes[j].dotClock / ((double)res->modes[j].hTotal * (double)res->modes[j].vTotal));
                            refresh_rate = refresh_rate < rate ? rate : refresh_rate;
                            break;
                        }
                    }

                    XRRFreeCrtcInfo (crtc);
                }
            }

            XRRFreeScreenResources (res);
        }
    }

    if (refresh_rate == 0)
    {
        refresh_rate = ccm_config_get_integer (self->priv->options[CCM_SCREEN_REFRESH_RATE],
                                               &error);

        if (error)
        {
            g_warning ("Error on get refresh rate configuration");
            g_error_free (error);
            refresh_rate = 60;
        }
        refresh_rate = MAX (20, refresh_rate);
        refresh_rate = MIN (150, refresh_rate);
    }

    if (self->priv->refresh_rate != refresh_rate)
    {
        self->priv->refresh_rate = refresh_rate;
        if (!error)
            ccm_config_set_integer (self->priv->options[CCM_SCREEN_REFRESH_RATE],
                                    refresh_rate, NULL);
        if (self->priv->paint)
        {
            ccm_timeline_stop (self->priv->paint);
            g_object_unref (self->priv->paint);
        }

        self->priv->paint = ccm_timeline_new (refresh_rate, refresh_rate);

        g_signal_connect_swapped (self->priv->paint, "new-frame",
                                  G_CALLBACK (ccm_screen_paint), self);
        ccm_timeline_set_master (self->priv->paint, TRUE);
        ccm_timeline_set_loop (self->priv->paint, TRUE);
        ccm_screen_wait_vblank (self);
        ccm_timeline_start (self->priv->paint);
        g_signal_emit (self, signals[REFRESH_RATE_CHANGED], 0);

        return TRUE;
    }

    return FALSE;
}

#if HAVE_VSYNC
static gboolean
ccm_screen_support_glx_extenstion (CCMScreen* self, const char* extension)
{
    gboolean ret = FALSE;
    gboolean have_glx;

    g_object_get(G_OBJECT(self->priv->display), "use_glx", &have_glx, NULL);
    if (have_glx)
    {
        const gchar *extensions = glXQueryExtensionsString (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                                            self->priv->number);
        const gchar* pos = g_strrstr(extensions, extension);

        ret = pos != NULL &&
              (pos == extensions || pos[-1] == ' ') &&
              (pos[strlen(extension)] == ' ' || pos[strlen(extension)] == '\0');
    }

    return ret;
}

static gboolean
ccm_screen_update_sync_with_vblank (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    GError *error = NULL;
    gboolean sync_with_vblank = ccm_config_get_boolean (self->priv->options[CCM_SCREEN_SYNC_WITH_VBLANK],
                                                        &error);

    if (error)
    {
        g_warning ("Error on get sync with vblank configuration");
        g_error_free (error);
    }
    else
    {
        if (sync_with_vblank && self->priv->sync_with_vblank != sync_with_vblank &&
            ccm_screen_support_glx_extenstion (self, "GLX_SGI_video_sync"))
        {
            self->priv->sync_with_vblank = FALSE;

            // Create fake vblank window
            XVisualInfo* visual_info;
            XSetWindowAttributes wattributes;
            int attrib[]= { GLX_RGBA,
                            GLX_RED_SIZE, 1,
                            GLX_GREEN_SIZE, 1,
                            GLX_BLUE_SIZE, 1,
                            None };
            // Choose a visual
            visual_info = glXChooseVisual (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                           self->priv->number, attrib);
            if (visual_info != NULL)
            {
                wattributes.colormap = XCreateColormap(CCM_DISPLAY_XDISPLAY (self->priv->display),
                                                       RootWindowOfScreen (self->priv->xscreen),
                                                       visual_info->visual, AllocNone);
                self->priv->vblank_window = XCreateWindow(CCM_DISPLAY_XDISPLAY (self->priv->display),
                                                          RootWindowOfScreen (self->priv->xscreen),
                                                          0, 0, 1, 1, 0, visual_info->depth,
                                                          InputOutput, visual_info->visual, CWColormap, &wattributes);

                if (self->priv->vblank_window != None)
                {
                    // Create a GLX context
                    self->priv->vblank_context = glXCreateContext(CCM_DISPLAY_XDISPLAY (self->priv->display),
                                                                  visual_info, None, GL_TRUE);
                    if (self->priv->vblank_context == NULL)
                    {
                        XDestroyWindow(CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->vblank_window);
                        self->priv->vblank_window = None;
                    }
                    else
                    {
                        // Make context current
                        glXMakeCurrent(CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->vblank_window,
                                       self->priv->vblank_context);

                        self->priv->wait_video_sync = (WaitVideoSyncFunc)glXGetProcAddress ((const GLubyte*)"glXWaitVideoSyncSGI");
                        self->priv->get_video_sync = (GetVideoSyncFunc)glXGetProcAddress ((const GLubyte*)"glXGetVideoSyncSGI");

                        self->priv->sync_with_vblank = self->priv->wait_video_sync != NULL &&
                                                       self->priv->get_video_sync != NULL;
                    }
                }
            }
        }
        else
        {
            self->priv->sync_with_vblank = FALSE;
        }

        if (!self->priv->sync_with_vblank)
        {
            if (self->priv->vblank_window != None)
            {
                XDestroyWindow(CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->vblank_window);
                self->priv->vblank_window = None;
            }

            if (self->priv->vblank_context != NULL)
            {
                glXDestroyContext(CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->vblank_context);
                self->priv->vblank_context = NULL;
            }
        }
        else if (self->priv->paint)
        {
            ccm_screen_wait_vblank (self);
            ccm_timeline_start (self->priv->paint);
        }

        if (self->priv->sync_with_vblank != sync_with_vblank)
            ccm_config_set_boolean (self->priv->options[CCM_SCREEN_SYNC_WITH_VBLANK],
                                    self->priv->sync_with_vblank, NULL);

        return TRUE;
    }

    return FALSE;
}
#endif

static void
ccm_screen_load_config (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    GError *error = NULL;
    gint cpt;

    for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
    {
        self->priv->options[cpt] = ccm_config_new (self->priv->number, NULL, CCMScreenOptions[cpt]);
        if (self->priv->options[cpt])
            g_signal_connect_swapped (self->priv->options[cpt], "changed",
                                      G_CALLBACK (ccm_screen_on_option_changed),
                                      self);
    }

    ccm_screen_update_backend (self);
    ccm_screen_update_refresh_rate (self);
#if HAVE_VSYNC
    ccm_screen_update_sync_with_vblank (self);
#endif
}

static gboolean
ccm_screen_valid_window (CCMScreen * self, CCMWindow * window)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (window != NULL, FALSE);

    cairo_rectangle_t geometry;

    if (ccm_screen_find_window_from_input (self, CCM_WINDOW_XWINDOW (window)))
        return FALSE;

    if (self->priv->root && CCM_WINDOW_XWINDOW (window) == CCM_WINDOW_XWINDOW (self->priv->root))
        return FALSE;

    if (self->priv->cow && CCM_WINDOW_XWINDOW (window) == CCM_WINDOW_XWINDOW (self->priv->cow))
        return FALSE;

    if (self->priv->selection_owner && CCM_WINDOW_XWINDOW (window) == self->priv->selection_owner)
        return FALSE;

    if (ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE (window), &geometry) &&
        geometry.x == -100 && geometry.y == -100 &&  geometry.width == 1 && geometry.height == 1)
        return FALSE;

    return TRUE;
}

static gboolean
ccm_screen_create_overlay_window (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    Window window;
    CCMWindow *root = ccm_screen_get_root_window (self);

    g_return_val_if_fail (root != NULL, FALSE);
    g_return_val_if_fail (self->priv->display != NULL, FALSE);

    window = XCompositeGetOverlayWindow (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                         CCM_WINDOW_XWINDOW (root));
    if (!window)
        return FALSE;

    self->priv->cow = ccm_window_new_unmanaged (self, window);
    ccm_window_make_output_only (self->priv->cow);

    return self->priv->cow != NULL;
}

static void
ccm_screen_resize (CCMScreen* self, int width, int height)
{
    if (CCM_SCREEN_XSCREEN (self)->width != width ||
        CCM_SCREEN_XSCREEN (self)->height != height)
    {
        // Destroy old cow
        if (self->priv->cow)
            g_object_unref (self->priv->cow);
        self->priv->cow = NULL;

        // Destroy old root
        if (self->priv->root)
            g_object_unref (self->priv->root);
        self->priv->root = NULL;

        // Destroy old background
        if (self->priv->background)
            g_object_unref (self->priv->background);
        self->priv->background = NULL;

        // Destroy old context
        if (self->priv->ctx)
            cairo_destroy (self->priv->ctx);

        self->priv->ctx = NULL;

        // Update screen geometry
        CCM_SCREEN_XSCREEN (self)->width = width;
        CCM_SCREEN_XSCREEN (self)->height = height;

        // Recreate overlay window
        ccm_screen_create_overlay_window (self);

        // Damage screen
        ccm_screen_damage (self);
    }
}

#if 0
static void
ccm_screen_print_stack (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    GList *item;

    ccm_log
        ("XID\t\tVisible\tOpaque\tType\tManaged\tDecored\tFullscreen\tKA\tKB\tTransient\tGroup\t\tName");
    for (item = self->priv->windows; item; item = item->next)
    {
        CCMWindow *transient = ccm_window_transient_for (item->data);
        CCMWindow *leader = ccm_window_get_group_leader (item->data);

        if (ccm_window_is_viewable (item->data))
            ccm_log
            ("0x%lx\t%i\t%i\t%i\t%i\t%i\t%i\t\t%i\t%i\t0x%08lx\t0x%08lx\t%s",
             CCM_WINDOW_XWINDOW (item->data),
             ccm_window_is_viewable (item->data),
             ccm_window_get_opaque_region (item->data) ? 1 : 0,
             ccm_window_get_hint_type (item->data),
             ccm_window_is_managed (item->data),
             ccm_window_is_decorated (item->data),
             ccm_window_is_fullscreen (item->data),
             ccm_window_keep_above (item->data),
             ccm_window_keep_below (item->data),
             transient ? CCM_WINDOW_XWINDOW (transient) : 0,
             leader ? CCM_WINDOW_XWINDOW (leader) : 0,
             ccm_window_get_name (item->data));
    }
}

#endif
#if 0
static void
ccm_screen_print_real_stack (CCMScreen * self)
{
    CCMWindow *root;
    Window *windows, w, p;
    guint n_windows, cpt;

    root = ccm_screen_get_root_window (self);
    XQueryTree (CCM_DISPLAY_XDISPLAY (self->priv->display),
                CCM_WINDOW_XWINDOW (root), &w, &p, &windows, &n_windows);
    g_print
        ("XID\tVisible\tType\tManaged\tDecorated\tFullscreen\tKA\tKB\tTransient\tGroup\tName\n");
    for (cpt = 0; cpt < n_windows; ++cpt)
    {
        CCMWindow *window =
            ccm_screen_find_window_or_child (self, windows[cpt]);
        if (window)
        {
            CCMWindow *transient = ccm_window_transient_for (window);
            CCMWindow *leader = ccm_window_get_group_leader (window);
            g_print ("0x%lx\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t0x%lx\t0x%lx\t%s\n",
                     CCM_WINDOW_XWINDOW (window),
                     ccm_window_is_viewable (window),
                     ccm_window_get_hint_type (window),
                     ccm_window_is_managed (window),
                     ccm_window_is_decorated (window),
                     ccm_window_is_fullscreen (window),
                     ccm_window_keep_above (window),
                     ccm_window_keep_below (window),
                     transient ? CCM_WINDOW_XWINDOW (transient) : 0,
                     leader ? CCM_WINDOW_XWINDOW (leader) : 0,
                     ccm_window_get_name (window));
        }
    }
    XFree (windows);
}

#endif

static void
ccm_screen_destroy_window (CCMScreen * self, CCMWindow * window)
{
    ccm_debug_window (window, "DESTROY WINDOW");

    self->priv->windows = g_list_remove (self->priv->windows, window);
    self->priv->last_windows = g_list_last (self->priv->windows);

    if (CCM_IS_WINDOW (window))
    {
        g_signal_handlers_disconnect_by_func (window, ccm_screen_on_window_damaged, self);
        g_signal_handlers_disconnect_by_func (window, ccm_screen_on_window_error, self);
        g_signal_handlers_disconnect_by_func (window, ccm_screen_on_window_property_changed, self);
        g_signal_handlers_disconnect_by_func (window, ccm_screen_on_window_redirect_input, self);
    }
    if (self->priv->fullscreen == window)
    {
        self->priv->fullscreen = NULL;
        ccm_screen_damage (self);
    }
    else if (CCM_IS_WINDOW (window) && ccm_window_is_viewable (window)
             && !ccm_window_is_input_only (window))
    {
        const CCMRegion *geometry = ccm_drawable_get_geometry (CCM_DRAWABLE (window));
        if (geometry && !ccm_region_empty ((CCMRegion *) geometry))
            ccm_screen_damage_region (self, geometry);
        else
            ccm_screen_damage (self);
    }
    if (self->priv->sibling_mouse == window)
        self->priv->sibling_mouse = NULL;

    if (G_IS_OBJECT (window))
        g_object_unref (window);

    g_signal_emit (self, signals[WINDOW_DESTROYED], 0);
}

CCMWindow *
ccm_screen_find_window (CCMScreen * self, Window xwindow)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (xwindow != None, NULL);

    GList *item;

    for (item = self->priv->windows; item; item = item->next)
    {
        if (CCM_WINDOW_XWINDOW (item->data) == xwindow)
            return CCM_WINDOW (item->data);
    }

    return NULL;
}

CCMWindow *
ccm_screen_find_window_or_child (CCMScreen * self, Window xwindow)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (xwindow != None, NULL);

    GList *item;

    for (item = self->priv->windows; item; item = item->next)
    {
        if (CCM_WINDOW_XWINDOW (item->data) == xwindow)
            return CCM_WINDOW (item->data);
        else if (_ccm_window_get_child (item->data) == xwindow)
            return CCM_WINDOW (item->data);
    }

    return NULL;
}

CCMWindow *
ccm_screen_find_window_from_input (CCMScreen * self, Window xwindow)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (xwindow != None, NULL);

    GList *item;
    CCMWindow *child = NULL;

    for (item = self->priv->windows; !child && item; item = item->next)
    {
        if (ccm_window_has_redirect_input (item->data))
        {
            Window xinput = None;
            g_object_get (G_OBJECT (item->data), "input", &xinput, NULL);
            if (xinput == xwindow)
                child = CCM_WINDOW (item->data);
        }
    }
    if (child)
        ccm_debug_window (child, "PARENT OF 0x%lx", xwindow);

    return child;
}

CCMWindow *
ccm_screen_find_window_at_pos (CCMScreen * self, int x, int y)
{
    g_return_val_if_fail (self != NULL, NULL);

    GList *item;
    CCMWindow *found = NULL;

    for (item = self->priv->last_windows; item && !found;
         item = item->prev)
    {
        if (ccm_window_is_viewable (item->data) &&
            !ccm_window_is_input_only (item->data))
        {
            CCMRegion *geometry = (CCMRegion *)ccm_drawable_get_geometry (item->data);

            if (geometry && ccm_region_point_in (geometry, x, y))
            {
                found = CCM_WINDOW (item->data);
            }
        }
    }

    return found;
}

static void
ccm_screen_unset_selection_owner (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->selection_owner != None)
    {
        gchar *cm_atom_name = g_strdup_printf ("_NET_WM_CM_S%i", self->priv->number);
        Atom cm_atom = XInternAtom (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                    cm_atom_name, 0);
        g_free (cm_atom_name);

        XDestroyWindow (CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->selection_owner);

        XSetSelectionOwner (CCM_DISPLAY_XDISPLAY (self->priv->display), cm_atom, None, 0);

        XClientMessageEvent event;
        event.type = ClientMessage;
        event.window = RootWindow (CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->number);
        event.message_type = cm_atom;
        event.format = 32;
        event.data.l[0] = CurrentTime;
        event.data.l[1] = cm_atom;
        event.data.l[2] = None;
        event.data.l[3] = 0;
        event.data.l[4] = 0;
        XSendEvent(CCM_DISPLAY_XDISPLAY (self->priv->display),
                   RootWindow (CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->number),
                   False, StructureNotifyMask, (XEvent*)&event);
    }
}

static gboolean
ccm_screen_set_selection_owner (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (self->priv->selection_owner == None)
    {
        gchar *cm_atom_name = g_strdup_printf ("_NET_WM_CM_S%i", self->priv->number);
        Atom cm_atom = XInternAtom (CCM_DISPLAY_XDISPLAY (self->priv->display), cm_atom_name, 0);
        CCMWindow *root = ccm_screen_get_root_window (self);

        g_free (cm_atom_name);

        if (XGetSelectionOwner (CCM_DISPLAY_XDISPLAY (self->priv->display), cm_atom) != None)
        {
            g_critical ("\nScreen %d already has a composite manager running, \n"
                        "try to stop it before run cairo-compmgr", self->priv->number);
            exit(1);
            return FALSE;
        }

        self->priv->selection_owner = XCreateSimpleWindow (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                                           CCM_WINDOW_XWINDOW (root), 0, 0, 1, 1,
                                                           0, None, None);

        Xutf8SetWMProperties (CCM_DISPLAY_XDISPLAY (self->priv->display),
                              self->priv->selection_owner, "cairo-compmgr",
                              "cairo-compmgr", NULL, 0, NULL, NULL, NULL);

        XSetSelectionOwner (CCM_DISPLAY_XDISPLAY (self->priv->display), cm_atom,
                            self->priv->selection_owner, 0);

        XClientMessageEvent event;
        event.type = ClientMessage;
        event.window = RootWindow (CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->number);
        event.message_type = cm_atom;
        event.format = 32;
        event.data.l[0] = CurrentTime;
        event.data.l[1] = cm_atom;
        event.data.l[2] = self->priv->selection_owner;
        event.data.l[3] = 0;
        event.data.l[4] = 0;
        XSendEvent(CCM_DISPLAY_XDISPLAY (self->priv->display),
                   RootWindow (CCM_DISPLAY_XDISPLAY (self->priv->display), self->priv->number),
                   False, StructureNotifyMask, (XEvent*)&event);
    }

    return TRUE;
}

static void
ccm_screen_update_stack (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    Window *stack = NULL;
    Window w, p;
    guint n_windows = 0;
    CCMWindow *root = ccm_screen_get_root_window (self);

    if (XQueryTree (CCM_DISPLAY_XDISPLAY (self->priv->display),
                    CCM_WINDOW_XWINDOW (root),
                    &w, &p, &stack, &n_windows) && stack && n_windows)
    {
        if (self->priv->stack)
            g_slice_free1 (sizeof (Window) * self->priv->n_windows, self->priv->stack);

        self->priv->n_windows = n_windows;
        self->priv->stack = g_slice_copy (sizeof (Window) * n_windows, stack);
        XFree (stack);
    }
}

static void
ccm_screen_query_stack (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    guint cpt;

    ccm_debug ("QUERY_STACK");

    if (self->priv->windows)
    {
        g_list_foreach (self->priv->windows, (GFunc) g_object_unref, NULL);
        g_list_free (self->priv->windows);
        self->priv->windows = NULL;
    }

    ccm_screen_update_stack (self);

    for (cpt = 0; cpt < self->priv->n_windows; ++cpt)
    {
        CCMWindow *window = ccm_window_new (self, self->priv->stack[cpt]);
        if (window)
        {
            if (!ccm_screen_add_window (self, window))
                g_object_unref (window);
            else if (ccm_window_is_viewable (window) &&
                     !ccm_window_is_input_only (window))
                ccm_window_map (window);
        }
    }
}

static int
ccm_screen_compare_window(CCMWindow* a, CCMWindow* b)
{
    CCMWindowType ta = ccm_window_get_hint_type(a);
    CCMWindowType tb = ccm_window_get_hint_type(b);

    if (!ccm_window_is_managed(a) && ccm_window_is_managed(b))
        return 1;

    if (ccm_window_is_managed(a) && !ccm_window_is_managed(b))
        return -1;

    if (ta == CCM_WINDOW_TYPE_DESKTOP && tb != CCM_WINDOW_TYPE_DESKTOP)
        return -1;

    if (ta != CCM_WINDOW_TYPE_DESKTOP && tb == CCM_WINDOW_TYPE_DESKTOP)
        return 1;

    if (ccm_window_is_fullscreen(a) && !ccm_window_is_fullscreen(b))
        return 1;

    if (!ccm_window_is_fullscreen(a) && ccm_window_is_fullscreen(b))
        return -1;

    if (ccm_window_keep_below(a) && !ccm_window_keep_below(b))
        return -1;

    if (!ccm_window_keep_below(a) && ccm_window_keep_below(b))
        return 1;

    if (ccm_window_keep_above(a) && !ccm_window_keep_above(b))
        return 1;

    if (!ccm_window_keep_above(a) && ccm_window_keep_above(b))
        return -1;

    if (ta == CCM_WINDOW_TYPE_DOCK && tb != CCM_WINDOW_TYPE_DOCK)
        return 1;

    if (ta != CCM_WINDOW_TYPE_DOCK && tb == CCM_WINDOW_TYPE_DOCK)
        return -1;

    return 0;
}

static void
ccm_screen_check_stack (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    guint cpt;
    GList *stack = NULL, *item, *last = NULL;
    GList *viewable = NULL, *old_viewable = NULL;

    ccm_debug ("CHECK_STACK");

    ccm_screen_update_stack (self);

    for (cpt = 0; cpt < self->priv->n_windows; ++cpt)
    {
        CCMWindow *window = ccm_screen_find_window (self, self->priv->stack[cpt]);

        if (window && !ccm_window_is_input_only (window) &&
            !g_list_find (stack, window))
        {
            stack = g_list_prepend (stack, window);
            if (ccm_window_is_viewable (window))
            {
                ccm_debug_window (window, "STACK IS VIEWABLE");
                viewable = g_list_prepend (viewable, window);
            }
        }
        else if (!window)
        {
            window = ccm_window_new (self, self->priv->stack[cpt]);
            if (window && !ccm_window_is_input_only (window) &&
                ccm_screen_valid_window (self, window))
            {
                ccm_debug_window (window, "CHECK STACK NEW WINDOW");
                if (ccm_window_is_viewable (window) &&
                    !ccm_window_is_input_only (window))
                {
                    ccm_debug_window (window, "CHECK STACK NEW WINDOW MAP");
                    viewable = g_list_prepend (viewable, window);
                }
                g_signal_connect_swapped (window, "damaged",
                                          G_CALLBACK
                                          (ccm_screen_on_window_damaged), self);
                g_signal_connect_swapped (window, "error",
                                          G_CALLBACK
                                          (ccm_screen_on_window_error), self);
                g_signal_connect_swapped (window, "property-changed",
                                          G_CALLBACK
                                          (ccm_screen_on_window_property_changed),
                                          self);
                g_signal_connect_swapped (window, "redirect-input",
                                          G_CALLBACK
                                          (ccm_screen_on_window_redirect_input),
                                          self);
                stack = g_list_prepend (stack, window);
            }
            else if (window)
                g_object_unref (window);
        }
    }
    stack = g_list_reverse (stack);

    for (item = self->priv->windows; item && stack; item = item->next)
    {
        GList *link = g_list_find (stack, item->data);

        if (link && ccm_window_is_viewable (item->data) &&
            !ccm_window_is_input_only (item->data))
        {
            ccm_debug_window (item->data, "OLD VIEWABLE");
            last = link;
            old_viewable = g_list_prepend (old_viewable, item->data);
        }
        else if (!link)
        {
            gboolean found = FALSE;

            ccm_debug_window (item->data, "CHECK STACK REMOVED");
            if (ccm_window_is_viewable (item->data) && !ccm_window_is_input_only (item->data))
            {
                GList *last_viewable = NULL;

                if (last)
                {
                    last_viewable = g_list_find (stack, last->data);
                    if (last_viewable)
                        stack = g_list_insert_before (stack, last_viewable->next,
                                                      item->data);
                }
                old_viewable = g_list_prepend (old_viewable, item->data);
                if (!last_viewable)
                    stack = g_list_prepend (stack, item->data);
                found = TRUE;
            }

            if (found)
                ccm_screen_remove_window (self, item->data);
            else
                ccm_screen_destroy_window (self, item->data);
        }
    }

    viewable = g_list_reverse (viewable);

    stack = g_list_sort(stack, (GCompareFunc)ccm_screen_compare_window);

    for (item = viewable; item; item = item->next)
    {
        const CCMWindow *transient = ccm_window_transient_for (item->data);
        const CCMWindow *leader = ccm_window_get_group_leader (item->data);

        if (transient &&
            ccm_window_is_viewable((CCMWindow*)transient) &&
            !ccm_window_is_input_only((CCMWindow*)transient) &&
            g_list_index (stack, item->data) < g_list_index (stack, transient))
        {
            ccm_debug ("RESTACK TRANSIENT");
            stack = g_list_remove (stack, item->data);
            stack = g_list_insert_before (stack,
                                          g_list_find (stack, transient)->next,
                                          item->data);
        }
        if (leader && leader != self->priv->root &&
            ccm_window_get_hint_type (item->data) == CCM_WINDOW_TYPE_DIALOG)
        {
            GList *iter;

            for (iter = item; iter; iter = iter->next)
            {
                if (leader == ccm_window_get_group_leader (iter->data) &&
                    ccm_window_is_viewable(iter->data) &&
                    !ccm_window_is_input_only(iter->data)
                    && ccm_window_get_hint_type (iter->data) == CCM_WINDOW_TYPE_NORMAL)
                {
                    ccm_debug ("RESTACK LEADER");
                    stack = g_list_remove (stack, item->data);
                    stack = g_list_insert_before (stack,
                                                  g_list_find (stack, iter->data)->next,
                                                  item->data);
                    break;
                }
            }
        }
    }

    if (self->priv->windows) g_list_free (self->priv->windows);
    self->priv->windows = stack;
    if (self->priv->windows)
        self->priv->last_windows = g_list_last(self->priv->windows);
    else
        self->priv->last_windows = NULL;

    viewable = g_list_sort (viewable, (GCompareFunc)ccm_screen_compare_window);
    viewable = g_list_reverse (viewable);

    last = old_viewable;
    ccm_debug ("LIST VIEWABLE");
    for (item = viewable; item; item = item->next)
    {
        ccm_debug_window (item->data, "VIEWABLE 0x%x",
                          last ? CCM_WINDOW_XWINDOW (last->data) : 0);
        if (!last || item->data != last->data)
        {
            ccm_debug_window (item->data, "DAMAGE");
            ccm_drawable_damage (item->data);
        }
        if (last)
            last = last->next;
    }


    g_list_free (viewable);
    g_list_free (old_viewable);

#if 0
    g_print ("Stack\n");
    ccm_screen_print_stack (self);
#endif
#if 0
    g_print ("Real stack\n");
    ccm_screen_print_real_stack (self);
#endif
}

static void
ccm_screen_restack (CCMScreen * self, CCMWindow * window, CCMWindow * sibling)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (window != NULL);
    g_return_if_fail (sibling != NULL);

    GList *sibling_link = NULL, *item, *found = NULL;
    const CCMWindow *transient = NULL, *leader = NULL;

    if (sibling == self->priv->active)
        return;

    if (g_list_find (self->priv->removed, sibling))
        return;

    if (ccm_window_get_hint_type (window) == CCM_WINDOW_TYPE_DESKTOP)
        return;

    transient = ccm_window_transient_for (sibling);

    if (transient && transient == window)
        return;

    leader = ccm_window_get_group_leader (window);
    if (leader && leader == ccm_window_get_group_leader (sibling) &&
        ccm_window_get_hint_type (sibling) == CCM_WINDOW_TYPE_DIALOG)
        return;

    for (item = self->priv->windows; item; item = item->next)
    {
        if (item->data == window)
        {
            if (sibling_link)
                return;
            else
                found = item;
        }
        if (item->data == sibling)
        {
            sibling_link = item;
            if (found)
            {
                ccm_debug_window (window, "RESTACK AFTER 0x%x", CCM_WINDOW_XWINDOW (sibling));
                self->priv->windows = g_list_remove_link(self->priv->windows, found);
                found->next = sibling_link->next;
                found->prev = sibling_link;
                if (sibling_link->next) sibling_link->next->prev = found;
                sibling_link->next = found;
                break;
            }
        }
    }
    self->priv->last_windows = g_list_last(self->priv->windows);

    ccm_drawable_damage (CCM_DRAWABLE (window));
}

static gboolean
impl_ccm_screen_paint (CCMScreenPlugin * plugin, CCMScreen * self, cairo_t * ctx)
{
    g_return_val_if_fail (plugin != NULL, FALSE);
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (ctx != NULL, FALSE);

    gboolean ret = FALSE;
    GList *item, *destroy = NULL;

    ccm_debug ("PAINT SCREEN BEGIN");
    for (item = self->priv->windows; item; item = item->next)
    {
        CCMWindow *window = CCM_WINDOW (item->data);

        if (ccm_window_is_viewable (window) && !ccm_window_is_input_only (window))
        {
            if (ccm_drawable_is_damaged (CCM_DRAWABLE (window)))
            {
                CCMRegion *damaged = (CCMRegion*)ccm_drawable_get_damaged (CCM_DRAWABLE (window));
                ccm_debug_region (CCM_DRAWABLE (window), "SCREEN DAMAGE");
                ccm_screen_add_damaged_region (self, damaged);
            }

            ccm_debug_window (window, "PAINT SCREEN");
            ret |= ccm_window_paint (window, ctx);
        }
    }

    for (item = self->priv->removed; item; item = item->next)
    {
        if (!ccm_window_is_viewable (item->data) || ccm_window_is_input_only (item->data))
        {
            ccm_screen_destroy_window (self, item->data);
            destroy = g_list_prepend (destroy, item->data);
        }
    }

    if (destroy)
    {
        for (item = destroy; item; item = item->next)
            self->priv->removed = g_list_remove (self->priv->removed, item->data);
        g_list_free (destroy);
    }

    ccm_debug ("PAINT SCREEN END");

    return ret;
}

static gboolean
ccm_screen_on_check_stack_pendings (CCMScreen * self)
{
    ccm_screen_check_stack (self);

    self->priv->id_pendings = 0;

    return FALSE;
}

static void
ccm_screen_add_check_pending (CCMScreen * self)
{
    if (!self->priv->id_pendings)
        self->priv->id_pendings = g_idle_add_full (G_PRIORITY_LOW,
                                                   (GSourceFunc)ccm_screen_on_check_stack_pendings,
                                                   self, NULL);
}

static void
ccm_screen_on_window_error (CCMScreen * self, CCMWindow * window)
{
    g_return_if_fail (self != NULL);

    ccm_debug_window (window, "ON WINDOW ERROR");

    ccm_screen_add_check_pending (self);
}

static void
ccm_screen_on_window_redirect_input (CCMScreen * self, gboolean redirected,
                                     CCMWindow * window)
{
    g_return_if_fail (self != NULL);

    if (redirected)
        self->priv->nb_redirect_input++;
    else
        self->priv->nb_redirect_input = MAX (self->priv->nb_redirect_input - 1, 0);
}

static void
ccm_screen_on_window_property_changed (CCMScreen * self,
                                       CCMPropertyType changed,
                                       CCMWindow * window)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (window != NULL);

    ccm_debug_window (window, "PROPERTY_CHANGED");

    if (changed == CCM_PROPERTY_STATE)
    {
        if (ccm_window_is_fullscreen (window) && self->priv->fullscreen != window)
        {
            ccm_debug_window (window, "FULLSCREEN");
            self->priv->fullscreen = window;
        }
        else if (!ccm_window_is_fullscreen (window) && self->priv->fullscreen == window)
        {
            ccm_debug_window (window, "UNFULLSCREEN");
            self->priv->fullscreen = NULL;
        }
    }
    else if (changed == CCM_PROPERTY_HINT_TYPE)
    {
        ccm_screen_add_check_pending (self);
    }
    else if (changed == CCM_PROPERTY_TRANSIENT)
    {
        const CCMWindow *transient = ccm_window_transient_for (window);

        if (transient)
            ccm_screen_restack (self, window, (CCMWindow*) transient);
    }
}

static gboolean
impl_ccm_screen_add_window (CCMScreenPlugin * plugin, CCMScreen * self,
                            CCMWindow * window)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (window != NULL, FALSE);
    g_return_val_if_fail (plugin != NULL, FALSE);

    ccm_debug_window (window, "ADD");

    self->priv->windows = g_list_append (self->priv->windows, window);
    self->priv->last_windows = g_list_last(self->priv->windows);

    g_signal_connect_swapped (window, "damaged", G_CALLBACK (ccm_screen_on_window_damaged), self);
    g_signal_connect_swapped (window, "error", G_CALLBACK (ccm_screen_on_window_error), self);
    g_signal_connect_swapped (window, "property-changed", G_CALLBACK (ccm_screen_on_window_property_changed), self);
    g_signal_connect_swapped (window, "redirect-input", G_CALLBACK (ccm_screen_on_window_redirect_input), self);

    return TRUE;
}


static void
impl_ccm_screen_remove_window (CCMScreenPlugin * plugin, CCMScreen * self,
                               CCMWindow * window)
{
    GList *link = g_list_find (self->priv->windows, window);

    if (link)
    {
        ccm_debug_window (window, "REMOVE");

        if (!ccm_window_is_viewable (window) || ccm_window_is_input_only (window))
            ccm_screen_destroy_window (self, window);
        else if (!g_list_find (self->priv->removed, window))
            self->priv->removed = g_list_prepend (self->priv->removed, window);
    }
}

static GSList *
ccm_screen_get_window_plugins (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    GSList *filter, *plugins = NULL;
    GError *error = NULL;

    filter = ccm_config_get_string_list (self->priv->options[CCM_SCREEN_PLUGINS], &error);
    if (error)
    {
        gchar **default_plugins = g_strsplit (DEFAULT_PLUGINS, ",", -1);
        gint cpt;

        g_error_free (error);
        g_warning ("Error on get plugins list set default");
        for (cpt = 0; default_plugins[cpt]; ++cpt)
        {
            filter = g_slist_prepend (filter, g_strdup (default_plugins[cpt]));
        }
        g_strfreev (default_plugins);
        filter = g_slist_reverse (filter);
    }
    if (filter)
    {
        plugins = ccm_extension_loader_get_window_plugins (self->priv->plugin_loader, filter);
        g_slist_foreach (filter, (GFunc) g_free, NULL);
        g_slist_free (filter);
    }
    return plugins;
}

static void
ccm_screen_get_plugins (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    GSList *filter = NULL, *plugins = NULL, *item;
    GError *error = NULL;

    if (self->priv->plugin && CCM_IS_PLUGIN (self->priv->plugin))
        g_object_unref (self->priv->plugin);

    self->priv->plugin = (CCMScreenPlugin *) self;

    filter = ccm_config_get_string_list (self->priv->options[CCM_SCREEN_PLUGINS], &error);
    if (error)
    {
        gchar **default_plugins = g_strsplit (DEFAULT_PLUGINS, ",", -1);
        gint cpt;

        g_error_free (error);
        g_warning ("Error on get plugins list set default");
        for (cpt = 0; default_plugins[cpt]; ++cpt)
        {
            filter = g_slist_prepend (filter, g_strdup (default_plugins[cpt]));
        }
        g_strfreev (default_plugins);
        filter = g_slist_reverse (filter);
    }
    if (filter)
    {
        plugins = ccm_extension_loader_get_screen_plugins (self->priv->plugin_loader, filter);
        g_slist_foreach (filter, (GFunc) g_free, NULL);
        g_slist_free (filter);
        for (item = plugins; item; item = item->next)
        {
            GType type = GPOINTER_TO_INT (item->data);
            GObject *prev = G_OBJECT (self->priv->plugin);
            CCMScreenPlugin *plugin = g_object_new (type, "parent", prev,
                                                          "screen", self->priv->number,
                                                          NULL);

            if (plugin)
                self->priv->plugin = plugin;
        }
        if (plugins)
            g_slist_free (plugins);
        ccm_screen_plugin_load_options (self->priv->plugin, self);
    }
}

static void
ccm_screen_paint (CCMScreen * self, int num_frame, CCMTimeline * timeline)
{
    g_return_if_fail (self != NULL);

    if (self->priv->cow)
    {
        CCMSetIterator* iter = ccm_set_iterator (self->priv->damages);
        while (ccm_set_iterator_next (iter))
        {
            guint damage = (guint)GPOINTER_TO_INT (ccm_set_iterator_get (iter));
            ccm_display_process_damage (self->priv->display, damage);
        }
        g_object_unref (iter);
        ccm_set_clear (self->priv->damages);

        if (!self->priv->ctx)
        {
            self->priv->ctx = ccm_drawable_create_context (CCM_DRAWABLE (self->priv->cow));
            if (self->priv->ctx)
            {
                if (self->priv->geometry == NULL)
                {
                    cairo_rectangle (self->priv->ctx, 0, 0, self->priv->xscreen->width, self->priv->xscreen->height);
                    cairo_clip (self->priv->ctx);
                }
                else
                {
                    cairo_rectangle_t *rects = NULL;
                    gint cpt, nb_rects;

                    ccm_region_get_rectangles (self->priv->geometry, &rects, &nb_rects);
                    for (cpt = 0; cpt < nb_rects; ++cpt)
                        cairo_rectangle (self->priv->ctx, rects[cpt].x, rects[cpt].y,
                                         rects[cpt].width, rects[cpt].height);
                    if (rects) cairo_rectangles_free (rects, nb_rects);
                    cairo_clip (self->priv->ctx);
                }
                cairo_set_operator (self->priv->ctx, CAIRO_OPERATOR_CLEAR);
                cairo_paint (self->priv->ctx);
                cairo_set_operator (self->priv->ctx, CAIRO_OPERATOR_OVER);
            }
        }
        else
        {
            cairo_identity_matrix (self->priv->ctx);
            if (self->priv->geometry == NULL)
            {
                cairo_rectangle (self->priv->ctx, 0, 0, self->priv->xscreen->width, self->priv->xscreen->height);
                cairo_clip (self->priv->ctx);
            }
            else
            {
                cairo_rectangle_t *rects = NULL;
                gint cpt, nb_rects;

                ccm_region_get_rectangles (self->priv->geometry, &rects, &nb_rects);
                for (cpt = 0; cpt < nb_rects; ++cpt)
                    cairo_rectangle (self->priv->ctx, rects[cpt].x, rects[cpt].y,
                                     rects[cpt].width, rects[cpt].height);
                if (rects) cairo_rectangles_free (rects, nb_rects);
                cairo_clip (self->priv->ctx);
            }
        }

        if (self->priv->root_damage)
        {
            if (!self->priv->background)
            {
                ccm_screen_update_background (self);
            }
            cairo_rectangle_t *rects = NULL;
            gint cpt, nb_rects;

            cairo_save (self->priv->ctx);
            ccm_region_get_rectangles (self->priv->root_damage, &rects, &nb_rects);
            for (cpt = 0; cpt < nb_rects; ++cpt)
                cairo_rectangle (self->priv->ctx, rects[cpt].x, rects[cpt].y,
                                 rects[cpt].width, rects[cpt].height);
            if (rects) cairo_rectangles_free (rects, nb_rects);
            cairo_clip (self->priv->ctx);

            if (self->priv->background)
            {
                cairo_surface_t *surface = ccm_drawable_get_surface (CCM_DRAWABLE (self->priv->background));

                cairo_set_source_surface (self->priv->ctx, surface, 0, 0);
                cairo_paint (self->priv->ctx);
                cairo_surface_destroy (surface);
            }
            else
            {
                cairo_set_source_rgb (self->priv->ctx, 0, 0, 0);
                cairo_paint (self->priv->ctx);
            }
            cairo_restore (self->priv->ctx);
            ccm_screen_add_damaged_region (self, self->priv->root_damage);
            ccm_region_destroy (self->priv->root_damage);
            self->priv->root_damage = NULL;
        }

        if (ccm_screen_plugin_paint (self->priv->plugin, self, self->priv->ctx))
        {
            if (self->priv->damaged)
            {
                ccm_drawable_flush_region (CCM_DRAWABLE (self->priv->cow), self->priv->damaged);
                ccm_region_destroy (self->priv->damaged);
                self->priv->damaged = NULL;
            }
            else
                ccm_drawable_flush (CCM_DRAWABLE (self->priv->cow));
        }
    }
}

static void
ccm_screen_on_option_changed (CCMScreen * self, CCMConfig * config)
{
    if (config == self->priv->options[CCM_SCREEN_PLUGINS])
    {
        ccm_screen_get_plugins (self);
        g_signal_emit (self, signals[PLUGINS_CHANGED], 0);
    }
    else if (config == self->priv->options[CCM_SCREEN_BACKEND])
    {
        ccm_screen_update_backend (self);
    }
    else if (config == self->priv->options[CCM_SCREEN_REFRESH_RATE] ||
             config == self->priv->options[CCM_SCREEN_AUTO_REFRESH_RATE])
    {
        ccm_screen_update_refresh_rate (self);
    }
#if HAVE_VSYNC
    else if (config == self->priv->options[CCM_SCREEN_SYNC_WITH_VBLANK])
    {
        ccm_screen_update_sync_with_vblank (self);
    }
#endif
    else if (config == self->priv->options[CCM_SCREEN_BACKGROUND] ||
             config == self->priv->options[CCM_SCREEN_COLOR_BACKGROUND] ||
             config == self->priv->options[CCM_SCREEN_BACKGROUND_X] ||
             config == self->priv->options[CCM_SCREEN_BACKGROUND_Y])
    {
        ccm_screen_update_background (self);
    }
}

static void
impl_ccm_screen_damage (CCMScreenPlugin * plugin, CCMScreen * self,
                        CCMRegion * area, CCMWindow * window)
{
    g_return_if_fail (plugin != NULL);
    g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);
    g_return_if_fail (window != NULL);

    GList *item = NULL;
    gboolean top = TRUE;
    CCMRegion *damage_above = NULL, *damage_below = NULL;
    const CCMRegion *opaque = NULL;

    damage_above = ccm_region_copy (area);
    damage_below = ccm_region_copy (area);

    ccm_debug_region (CCM_DRAWABLE (window), "ON_DAMAGE");

    // Substract opaque region of window to damage region below
    opaque = ccm_window_get_opaque_region (window);
    if (opaque && ccm_window_is_viewable (window))
    {
        ccm_region_subtract (damage_below, (CCMRegion *) opaque);
    }

    // Substract all obscured area to damage region
    for (item = self->priv->last_windows; item; item = item->prev)
    {
        if (!ccm_window_is_input_only (item->data) &&
            ccm_window_is_viewable (item->data) &&
            CCM_WINDOW_XWINDOW (item->data) != CCM_WINDOW_XWINDOW (window))
        {
            opaque = ccm_window_get_opaque_region (item->data);
            if (opaque)
            {
                ccm_debug_window (window, "UNDAMAGE ABOVE 0x%lx", CCM_WINDOW_XWINDOW (item->data));
                ccm_drawable_undamage_region (CCM_DRAWABLE (window), (CCMRegion *) opaque);
                // window is totaly obscured don't damage all other windows
                if (!ccm_drawable_is_damaged (CCM_DRAWABLE (window)))
                {
                    ccm_region_destroy (damage_below);
                    ccm_region_destroy (damage_above);
                    return;
                }
                ccm_region_subtract (damage_above, (CCMRegion *) opaque);
                ccm_region_subtract (damage_below, (CCMRegion *) opaque);
            }
        }
        else if (item->data == window)
            break;
    }

    // If no damage on above skip above windows
    if (ccm_region_empty (damage_above))
    {
        item = g_list_find (self->priv->windows, window);
        item = item ? item->prev : NULL;
        top = FALSE;
    }
    else
        item = self->priv->last_windows;

    // damage now all concurent window
    for (; item; item = item->prev)
    {
        if (!ccm_window_is_input_only (item->data)
            && ccm_window_is_viewable (item->data) && item->data != window)
        {
            if (top)
            {
                ccm_drawable_damage_region_silently (item->data, damage_above);
            }
            else
            {
                opaque = ccm_window_get_opaque_region (window);
                if (ccm_window_is_viewable (window) &&
                    !ccm_window_is_input_only (window) && opaque &&
                    !ccm_region_empty ((CCMRegion *) opaque))
                {
                    ccm_debug_window (item->data, "UNDAMAGE BELOW");
                    ccm_drawable_undamage_region (CCM_DRAWABLE (item->data), (CCMRegion *) opaque);
                }

                ccm_drawable_damage_region_silently (item->data, damage_below);
                opaque = ccm_window_get_opaque_region (item->data);

                if (opaque)
                {
                    ccm_region_subtract (damage_below, (CCMRegion *) opaque);
                }
            }
        }
        else if (item->data == window)
        {
            top = FALSE;
            opaque = ccm_window_get_opaque_region (window);
            if (ccm_region_empty (damage_below) && ccm_region_empty ((CCMRegion *) opaque))
                break;
        }
    }

    if (!ccm_region_empty (damage_below))
    {
        if (self->priv->geometry == NULL)
        {
            cairo_rectangle_t area;

            area.x = 0;
            area.y = 0;
            area.width = self->priv->xscreen->width;
            area.height = self->priv->xscreen->height;
            self->priv->geometry = ccm_region_rectangle (&area);
        }
        ccm_region_intersect (damage_below, self->priv->geometry);

        if (!ccm_region_empty (damage_below))
        {
            if (self->priv->root_damage)
                ccm_region_union (self->priv->root_damage, damage_below);
            else
                self->priv->root_damage = ccm_region_copy (damage_below);
        }
    }

    ccm_region_destroy (damage_above);
    ccm_region_destroy (damage_below);
}

static void
ccm_screen_on_window_damaged (CCMScreen * self, CCMRegion * area,
                              CCMWindow * window)
{
    if (!self->priv->cow)
        ccm_screen_create_overlay_window (self);

    if (self->priv->cow && CCM_WINDOW_XWINDOW (self->priv->cow) != CCM_WINDOW_XWINDOW (window))
    {
        ccm_screen_plugin_damage (self->priv->plugin, self, area, window);
    }
}

static void
ccm_screen_on_damage_event (CCMScreen * self, guint32 damage, CCMDrawable* drawable)
{
    if (ccm_drawable_get_screen (drawable) == self)
    {
        ccm_set_insert (self->priv->damages, GINT_TO_POINTER((gint)damage));
    }
}

static void
ccm_screen_on_damage_destroy (CCMScreen * self, guint32 damage, CCMDrawable* drawable)
{
    if (ccm_drawable_get_screen (drawable) == self)
    {
        ccm_set_remove (self->priv->damages, GINT_TO_POINTER((gint)damage));
    }
}

static void
ccm_screen_on_event (CCMScreen * self, XEvent * event)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (event != NULL);

    switch (event->type)
    {
        case ButtonPress:
        case ButtonRelease:
            {
                XButtonEvent *button_event = ((XButtonEvent *) event);

                if (button_event->root != CCM_WINDOW_XWINDOW (self->priv->root))
                    return;

                if (self->priv->redirect_input && self->priv->nb_redirect_input)
                {
                    CCMWindow *window = ccm_screen_find_window_at_pos (self,
                                                                       button_event->x_root,
                                                                       button_event->y_root);

                    if (window && ccm_window_has_redirect_input (window))
                    {
                        Window input = None;
                        g_object_get (G_OBJECT (window), "input", &input, NULL);
                        if (input)
                            self->priv->over_mouse =
                            ccm_window_redirect_event (window, event,
                                                       self->priv->over_mouse);
                        else
                            self->priv->over_mouse = None;
                    }
                    else
                        self->priv->over_mouse = None;
                }
                else
                    self->priv->over_mouse = None;
            }
            break;
        case MotionNotify:
            {
                XMotionEvent *motion_event = ((XMotionEvent *) event);

                if (motion_event->root != CCM_WINDOW_XWINDOW (self->priv->root))
                    return;

                if (self->priv->redirect_input && self->priv->nb_redirect_input)
                {
                    CCMWindow *window = ccm_screen_find_window_at_pos (self,
                                                                       motion_event->x_root,
                                                                       motion_event->y_root);
                    if (window && ccm_window_has_redirect_input (window))
                    {
                        Window input = None;
                        g_object_get (G_OBJECT (window), "input", &input, NULL);
                        if (input)
                        {
                            self->priv->over_mouse =
                                ccm_window_redirect_event (window, event,
                                                           self->priv->over_mouse);
                        }
                        else
                            self->priv->over_mouse = None;

                        if (window != self->priv->sibling_mouse)
                        {
                            if (self->priv->sibling_mouse)
                                g_signal_emit (self,
                                               signals[LEAVE_WINDOW_NOTIFY], 0,
                                               self->priv->sibling_mouse);
                            self->priv->sibling_mouse = window;
                            g_signal_emit (self, signals[ENTER_WINDOW_NOTIFY],
                                           0, self->priv->sibling_mouse);
                        }
                    }
                }
            }
            break;
        case CreateNotify:
            {
                XCreateWindowEvent *create_event = ((XCreateWindowEvent *) event);
                CCMWindow *window = ccm_screen_find_window_or_child (self,
                                                                     create_event->window);
                ccm_debug ("CREATE 0x%lx 0x%lx", create_event->window, create_event->parent);

                if (!window)
                {
                    CCMWindow *root = ccm_screen_get_root_window (self);
                    if (create_event->parent == CCM_WINDOW_XWINDOW (root))
                    {
                        window = ccm_window_new (self, create_event->window);
                        if (window)
                        {
                            ccm_debug_window (window, "CREATE");
                            if (!ccm_screen_add_window (self, window))
                                g_object_unref (window);
                        }
                    }
                }
            }
            break;
        case DestroyNotify:
            {
                CCMWindow *window = ccm_screen_find_window_or_child (self,
                                                                     ((XDestroyWindowEvent *) event)->window);
                ccm_debug ("REMOVE 0x%x", ((XDestroyWindowEvent *) event)->window);
                if (window)
                {
                    ccm_debug_window (window, "EVENT REMOVE 0x%x",
                                      ((XDestroyWindowEvent *) event)->window);
                    ccm_screen_remove_window (self, window);
                }
            }
            break;
        case MapNotify:
            {
                CCMWindow *window, *parent;

                parent = ccm_screen_find_window (self, ((XMapEvent *)event)->event);
                window = ccm_screen_find_window (self, ((XMapEvent *)event)->window);
                if (window)
                {
                    ccm_debug_window (window, "MAP 0x%lx", ((XMapEvent *) event)->event);
                    ccm_window_map (window);
                }
                else if (((XMapEvent *) event)->event == CCM_WINDOW_XWINDOW (self->priv->root))
                {
                    window = ccm_window_new (self, ((XMapEvent *) event)->window);
                    if (window)
                    {
                        ccm_debug_window (window, "CREATE MAP");
                        if (!ccm_screen_add_window (self, window))
                        {
                            g_object_unref (window);
                            window = NULL;
                        }
                        else
                            ccm_window_map (window);
                    }
                }
                if (window)
                {
                    const CCMWindow *transient = ccm_window_transient_for (window);

                    if (transient)
                        ccm_screen_restack (self, window, (CCMWindow*)transient);
                }
            }
            break;
        case UnmapNotify:
            {
                CCMWindow *window;

                window = ccm_screen_find_window (self, ((XUnmapEvent *) event)->window);
                if (window)
                {
                    ccm_debug_window (window, "UNMAP");
                    ccm_window_unmap (window);
                }
            }
            break;
        case ReparentNotify:
            {
                CCMWindow *window = ccm_screen_find_window_or_child (self, ((XReparentEvent *)event)->window);

                ccm_debug ("REPARENT 0x%x, 0x%x",
                           ((XReparentEvent *) event)->parent,
                           ((XReparentEvent *) event)->window);

                if (((XReparentEvent *) event)->parent == CCM_WINDOW_XWINDOW (self->priv->root))
                {
                    if (!window)
                    {
                        window = ccm_window_new (self, ((XReparentEvent *) event)->window);
                        if (window)
                        {
                            ccm_debug_window (window, "REPARENT ADD");
                            if (!ccm_screen_add_window (self, window))
                                g_object_unref (window);
                        }
                    }
                }
                else if (window)
                {
                    ccm_debug_window (window, "REPARENT REMOVE");
                    ccm_screen_destroy_window (self, window);
                }
            }
            break;
        case CirculateNotify:
            {
                XCirculateEvent *circulate_event = (XCirculateEvent *) event;

                CCMWindow *window = ccm_screen_find_window (self,
                                                            circulate_event->window);

                if (window)
                {
                    ccm_debug_window (window, "CIRCULATE");
                    ccm_screen_add_check_pending (self);
                }
            }
            break;
        case ConfigureNotify:
            {
                XConfigureEvent *configure_event = (XConfigureEvent *) event;

                if (configure_event->window == CCM_WINDOW_XWINDOW (self->priv->root))
                {
                    ccm_screen_resize (self, configure_event->width, configure_event->height);
                }
                else
                {
                    CCMWindow *window;

                    window = ccm_screen_find_window (self, configure_event->window);

                    if (window)
                    {
                        XEvent ce;

                        while (XCheckTypedWindowEvent(CCM_DISPLAY_XDISPLAY(self->priv->display),
                                                      configure_event->window,
                                                      event->type, &ce))
                        {
                            if (ce.xconfigure.above != configure_event->above)
                            {
                                XPutBackEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), &ce);
                                break;
                            }
                            configure_event = &ce.xconfigure;
                        }
                        if (configure_event->above != None)
                        {
                            if (configure_event->above
                                && configure_event->above !=
                                CCM_WINDOW_XWINDOW (self->priv->root)
                                && configure_event->above !=
                                CCM_WINDOW_XWINDOW (self->priv->cow)
                                && configure_event->above !=
                                self->priv->selection_owner)
                            {
                                CCMWindow *above;
                                above =
                                    ccm_screen_find_window (self,
                                                            configure_event->above);
                                if (above)
                                    ccm_screen_restack (self, window, above);
                            }
                        }

                        ccm_drawable_move (CCM_DRAWABLE (window),
                                           configure_event->x -
                                           configure_event->border_width,
                                           configure_event->y -
                                           configure_event->border_width);

                        ccm_drawable_resize (CCM_DRAWABLE (window),
                                             configure_event->width +
                                             configure_event->border_width * 2,
                                             configure_event->height +
                                             configure_event->border_width * 2);
                    }
                }
            }
            break;
        case PropertyNotify:
            {
                XPropertyEvent *property_event = (XPropertyEvent *) event;
                CCMWindow *window;

                ccm_debug_atom (self->priv->display, property_event->atom,
                                "PROPERTY_NOTIFY");

                if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->active_atom)
                {
                    guint32 *data;
                    guint n_items;
                    Window active;

                    ccm_screen_add_check_pending (self);
                    data =
                        ccm_window_get_property (self->priv->root,
                                                 CCM_WINDOW_GET_CLASS (self->priv->root)->active_atom,
                                                 XA_WINDOW, &n_items);
                    if (data)
                    {
                        active = (Window)*data;
                        if (active)
                            self->priv->active = ccm_screen_find_window_or_child (self, (Window)*data);
                        else
                            self->priv->active = NULL;
                        g_free (data);
                        if (self->priv->active)
                            g_signal_emit (self, signals[ACTIVATE_WINDOW_NOTIFY], 0, self->priv->active);
                    }
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->root_pixmap_atom)
                {
                    if (self->priv->background)
                        g_object_unref (self->priv->background);
                    self->priv->background = NULL;
                    ccm_screen_damage (self);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->client_stacking_list_atom ||
                         property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->client_list_atom)
                {
                    ccm_screen_add_check_pending (self);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->transient_for_atom)
                {
                    window = ccm_screen_find_window_or_child (self, property_event->window);
                    if (window)
                        ccm_window_query_transient_for (window);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->opacity_atom)
                {
                    window = ccm_screen_find_window_or_child (self, property_event->window);
                    if (window)
                        ccm_window_query_opacity (window, property_event->state == PropertyDelete);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->type_atom)
                {
                    window = ccm_screen_find_window_or_child (self, property_event->window);
                    if (window)
                        ccm_window_query_hint_type (window);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->mwm_hints_atom)
                {
                    window = ccm_screen_find_window_or_child (self, property_event->window);
                    if (window)
                        ccm_window_query_mwm_hints (window);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->state_atom)
                {
                    window = ccm_screen_find_window_or_child (self, property_event->window);
                    if (window)
                        ccm_window_query_state (window);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->frame_extends_atom)
                {
                    window = ccm_screen_find_window_or_child (self, property_event->window);
                    if (window)
                        ccm_window_query_frame_extends (window);
                }
                else if (property_event->atom == CCM_WINDOW_GET_CLASS (self->priv->root)->current_desktop_atom)
                {
                    guint n_items;
                    guint32 *desktop = NULL;

                    desktop = ccm_window_get_property (self->priv->root,
                                                       CCM_WINDOW_GET_CLASS (self->priv->root)->current_desktop_atom,
                                                       XA_CARDINAL, &n_items);
                    if (desktop)
                    {
                        g_signal_emit (self, signals[DESKTOP_CHANGED], 0,
                                       *desktop + 1);
                        g_free (desktop);
                    }
                }
            }
            break;
        case Expose:
            {
                XExposeEvent *expose_event = (XExposeEvent *) event;
                cairo_rectangle_t area;
                CCMRegion *damaged;

                ccm_debug ("EXPOSE");
                area.x = expose_event->x;
                area.y = expose_event->y;
                area.width = expose_event->width;
                area.height = expose_event->height;
                damaged = ccm_region_rectangle (&area);
                ccm_screen_damage_region (self, damaged);
                ccm_region_destroy (damaged);
            }
            break;
        case ClientMessage:
            {
                XClientMessageEvent *client_event = (XClientMessageEvent *)event;

                ccm_debug_atom (self->priv->display, client_event->message_type,
                                "CLIENT MESSAGE");

                if (client_event->window == self->priv->selection_owner &&
                    client_event->message_type == CCM_WINDOW_GET_CLASS (self->priv->root)->ccm_atom)
                {
                    if (CCM_WINDOW_XWINDOW (self->priv->root) == client_event->data.l[1])
                    {
                        CCMWindow *window = ccm_screen_find_window_or_child (self, client_event->data.l[4]);
                        g_signal_emit (self, signals[COMPOSITE_MESSAGE], 0,
                                       window ? window : self->priv->root,
                                       window ? window : self->priv->root,
                                       client_event->data.l[0],
                                       client_event->data.l[2],
                                       client_event->data.l[3]);
                    }
                    else
                    {
                        CCMWindow *window = ccm_screen_find_window_or_child (self, client_event->data.l[1]);
                        CCMWindow *client = ccm_screen_find_window_or_child (self, client_event->data.l[4]);
                        if (window)
                            g_signal_emit (self, signals[COMPOSITE_MESSAGE], 0,
                                           client, window,
                                           client_event->data.l[0],
                                           client_event->data.l[2],
                                           client_event->data.l[3]);
                    }
                }
                else if (client_event->message_type == CCM_WINDOW_GET_CLASS (self->priv->root)->state_atom)
                {
                    CCMWindow *window = ccm_screen_find_window_or_child (self, client_event->window);
                    if (window)
                    {
                        gint cpt;

                        for (cpt = 1; cpt < 3; ++cpt)
                        {
                            switch (client_event->data.l[0])
                            {
                                case 0:
                                    ccm_debug_atom (self->priv->display,
                                                    client_event->data.l[cpt],
                                                    "UNSET STATE");
                                    ccm_window_unset_state (window,
                                                            client_event->data.l[cpt]);
                                    break;
                                case 1:
                                    ccm_debug_atom (self->priv->display,
                                                    client_event->data.l[cpt],
                                                    "SET STATE");
                                    ccm_window_set_state (window,
                                                          client_event->data.l[cpt]);
                                    break;
                                case 2:
                                    ccm_debug_atom (self->priv->display,
                                                    client_event->data.l[cpt],
                                                    "SWITCH STATE");
                                    ccm_window_switch_state (window,
                                                             client_event->data.l[cpt]);
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                }
                else if (client_event->message_type == CCM_WINDOW_GET_CLASS (self->priv->root)->type_atom)
                {
                    CCMWindow *window = ccm_screen_find_window_or_child (self, client_event->window);
                    if (window)
                        ccm_window_query_hint_type (window);
                }
                else if (client_event->message_type == CCM_WINDOW_GET_CLASS (self->priv->root)->opacity_atom)
                {
                    CCMWindow *window = ccm_screen_find_window_or_child (self, client_event->window);
                    if (window)
                        ccm_window_query_opacity (window, FALSE);
                }
                else if (client_event->message_type == CCM_WINDOW_GET_CLASS (self->priv->root)->root_pixmap_atom)
                {
                    if(self->priv->background)
                        g_object_unref (self->priv->background);
                    self->priv->background = NULL;
                    ccm_screen_damage (self);
                }
            }
            break;
        default:
            {
                // Check for shape notify
                if (event->type == ccm_display_get_shape_notify_event_type (self->priv->display))
                {
                    XShapeEvent *shape_event = (XShapeEvent *) event;

                    if (shape_event->kind != ShapeInput)
                    {
                        CCMWindow *window = ccm_screen_find_window_or_child (self, shape_event->window);
                        if (window)
                        {
                            const CCMRegion *geometry = ccm_drawable_get_device_geometry (CCM_DRAWABLE(window));

                            if (!geometry || !ccm_region_is_shaped ((CCMRegion *) geometry))
                            {
                                ccm_drawable_damage (CCM_DRAWABLE (window));
                                ccm_drawable_query_geometry (CCM_DRAWABLE (window));
                                ccm_drawable_damage (CCM_DRAWABLE (window));
                            }
                        }
                    }
                }
            }
            break;
    }
}

CCMScreenPlugin *
_ccm_screen_get_plugin (CCMScreen * self, GType type)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMScreenPlugin *plugin;

    for (plugin = self->priv->plugin; plugin != (CCMScreenPlugin *) self;
         plugin = CCM_SCREEN_PLUGIN_PARENT (plugin))
    {
        if (g_type_is_a (G_OBJECT_TYPE (plugin), type))
            return plugin;
    }

    return NULL;
}

Window
_ccm_screen_get_selection_owner (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, None);

    return self->priv->selection_owner;
}

void
_ccm_screen_query_geometry (CCMScreen* self)
{
    gboolean have_randr;

    g_object_get(G_OBJECT(self->priv->display), "use_randr", &have_randr, NULL);
    if (have_randr)
    {
        XRRScreenResources *res;

        res  = XRRGetScreenResources (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                      RootWindowOfScreen (self->priv->xscreen));

        RROutput primary = XRRGetOutputPrimary (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                                RootWindowOfScreen (self->priv->xscreen));

        if (res)
        {
            int i, j;

            if (self->priv->geometry != NULL)
            {
                ccm_region_destroy (self->priv->geometry);
                self->priv->geometry = NULL;
            }
            if (self->priv->primary_geometry != NULL)
            {
                ccm_region_destroy (self->priv->primary_geometry);
                self->priv->primary_geometry = NULL;
            }
            self->priv->geometry = ccm_region_new ();

            for (i = 0; i < res->noutput; ++i)
            {
                XRROutputInfo* output;
                XRRCrtcInfo* crtc;

                output = XRRGetOutputInfo (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                           res, res->outputs[i]);

                if (output == NULL) continue;

                if (output->connection != RR_Connected)
                {
                    XRRFreeOutputInfo (output);
                    continue;
                }

                crtc = XRRGetCrtcInfo (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                       res, output->crtc);
                XRRFreeOutputInfo (output);

                if (crtc == NULL) continue;

                cairo_rectangle_t area = { crtc->x, crtc->y, crtc->width, crtc->height };
                ccm_region_union_with_rect (self->priv->geometry, &area);

                if (res->outputs[i] == primary)
                {
                    self->priv->primary_geometry = ccm_region_rectangle (&area);
                }

                XRRFreeCrtcInfo (crtc);
            }

            if (ccm_region_empty (self->priv->geometry))
            {
                cairo_rectangle_t area = { 0, 0, self->priv->xscreen->width, self->priv->xscreen->height };
                self->priv->geometry = ccm_region_rectangle (&area);
            }
            if (self->priv->primary_geometry == NULL)
            {
                self->priv->primary_geometry = ccm_region_copy (self->priv->geometry);
            }
        }

        XRRFreeScreenResources (res);
    }
}

CCMScreen *
ccm_screen_new (CCMDisplay * display, guint number)
{
    g_return_val_if_fail (display != NULL, NULL);

    CCMWindow *root;
    cairo_rectangle_t area;
    CCMScreen *self = g_object_new (CCM_TYPE_SCREEN, "display", display,
                                    "number", number, NULL);

    if (!self->priv->xscreen)
    {
        g_object_unref (self);
        return NULL;
    }

    self->priv->plugin_loader = ccm_extension_loader_new ();

    ccm_screen_load_config (self);

    /* Load plugins */
    ccm_screen_get_plugins (self);

    g_signal_connect_swapped (self->priv->display, "event",
                              G_CALLBACK (ccm_screen_on_event), self);

    g_signal_connect_swapped (self->priv->display, "damage-event",
                              G_CALLBACK (ccm_screen_on_damage_event), self);

    g_signal_connect_swapped (self->priv->display, "damage-destroy",
                              G_CALLBACK (ccm_screen_on_damage_destroy), self);

    if (!ccm_screen_create_overlay_window (self))
    {
        g_warning ("Error on create overlay window");
        g_object_unref (self);
        return NULL;
    }

    if (!ccm_screen_set_selection_owner (self))
    {
        g_object_unref (self);
        return NULL;
    }

    root = ccm_screen_get_root_window (self);
    ccm_window_redirect_subwindows (root);
    ccm_screen_query_stack (self);

    _ccm_screen_query_geometry (self);
    self->priv->root_damage = ccm_region_copy (self->priv->geometry);

    return self;
}

G_GNUC_PURE CCMDisplay *
ccm_screen_get_display (CCMScreen * self)
{
    g_return_val_if_fail (CCM_IS_SCREEN (self), NULL);

    return self->priv->display;
}

G_GNUC_PURE Screen *
ccm_screen_get_xscreen (CCMScreen * self)
{
    g_return_val_if_fail (CCM_IS_SCREEN (self), NULL);

    return self->priv->xscreen;
}

G_GNUC_PURE guint
ccm_screen_get_number (CCMScreen * self)
{
    g_return_val_if_fail (CCM_IS_SCREEN (self), 0);

    return self->priv->number;
}

G_GNUC_PURE guint
ccm_screen_get_refresh_rate (CCMScreen * self)
{
    g_return_val_if_fail (CCM_IS_SCREEN (self), 60);

    return self->priv->refresh_rate;
}

G_GNUC_PURE CCMWindow *
ccm_screen_get_overlay_window (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->cow;
}

G_GNUC_PURE CCMWindow *
ccm_screen_get_root_window (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    if (!self->priv->root)
    {
        Window root = RootWindowOfScreen (self->priv->xscreen);

        self->priv->root = ccm_window_new_unmanaged (self, root);
        XSelectInput (CCM_DISPLAY_XDISPLAY (ccm_screen_get_display (self)),
                      root,
                      ExposureMask | PropertyChangeMask | StructureNotifyMask |
                      SubstructureNotifyMask | SubstructureRedirectMask);
    }

    return self->priv->root;
}

gboolean
ccm_screen_add_window (CCMScreen * self, CCMWindow * window)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (window != NULL, FALSE);

    gboolean ret = FALSE;

    if (!ccm_screen_valid_window (self, window))
        return ret;

    if (!ccm_window_is_input_only (window) && !ccm_screen_find_window (self, CCM_WINDOW_XWINDOW (window)))
    {
        ret = ccm_screen_plugin_add_window (self->priv->plugin, self, window);
    }

    return ret;
}

void
ccm_screen_remove_window (CCMScreen * self, CCMWindow * window)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (window != NULL);

    if (!g_list_find (self->priv->removed, window))
    {
        if (ccm_window_is_viewable (window)
            && !ccm_window_is_input_only (window))
        {
            ccm_debug_window (window, "UNMAP ON REMOVE");
            ccm_window_unmap (window);
        }

        ccm_screen_plugin_remove_window (self->priv->plugin, self, window);
    }
}

void
ccm_screen_damage (CCMScreen * self)
{
    g_return_if_fail (self != NULL);

    cairo_rectangle_t area;

    if (self->priv->geometry == NULL)
    {
        area.x = 0.0f;
        area.y = 0.0f;
        area.width = CCM_SCREEN_XSCREEN (self)->width;
        area.height = CCM_SCREEN_XSCREEN (self)->height;
        self->priv->geometry = ccm_region_rectangle (&area);
    }

    ccm_debug ("DAMAGE SCREEN");
    ccm_screen_damage_region (self, self->priv->geometry);
}

void
ccm_screen_damage_region (CCMScreen * self, const CCMRegion * area)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);

    GList *item;

    ccm_debug ("SCREEN DAMAGE REGION");

    for (item = self->priv->last_windows; item; item = item->prev)
    {
        if (CCM_IS_WINDOW (item->data) && !ccm_window_is_input_only (item->data)
            && ccm_window_is_viewable (item->data))
        {
            ccm_drawable_damage_region (item->data, area);
            break;
        }
    }
}

void
ccm_screen_undamage_region (CCMScreen * self, const CCMRegion * area)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);

    GList *item;

    for (item = self->priv->last_windows; item; item = item->prev)
    {
        if (!ccm_window_is_input_only (item->data)
            && ccm_window_is_viewable (item->data))
        {
            ccm_drawable_undamage_region (item->data, (CCMRegion *) area);
        }
    }
    if (self->priv->root_damage)
    {
        ccm_region_subtract (self->priv->root_damage, (CCMRegion *) area);
        if (ccm_region_empty (self->priv->root_damage))
        {
            ccm_region_destroy (self->priv->root_damage);
            self->priv->root_damage = NULL;
        }
    }
}

G_GNUC_PURE GList *
ccm_screen_get_windows (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->windows;
}

G_GNUC_PURE CCMRegion *
ccm_screen_get_geometry (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->geometry;
}

G_GNUC_PURE CCMRegion *
ccm_screen_get_primary_geometry (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->primary_geometry;
}

G_GNUC_PURE CCMRegion *
ccm_screen_get_damaged (CCMScreen * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->damaged;
}

void
ccm_screen_remove_damaged_region (CCMScreen * self, CCMRegion * region)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (region != NULL);

    if (self->priv->damaged && !ccm_region_empty (region))
    {
        ccm_region_subtract (self->priv->damaged, region);
        if (ccm_region_empty (self->priv->damaged))
        {
            ccm_region_destroy (self->priv->damaged);
            self->priv->damaged = NULL;
        }
    }
}

void
ccm_screen_add_damaged_region (CCMScreen * self, CCMRegion * region)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (region != NULL);

    if (self->priv->damaged)
        ccm_region_union (self->priv->damaged, region);
    else
        self->priv->damaged = ccm_region_copy (region);
}

gboolean
ccm_screen_query_pointer (CCMScreen * self, CCMWindow ** below, gint * x,
                          gint * y)
{
    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (x != NULL && y != NULL, FALSE);

    CCMWindow *root = ccm_screen_get_root_window (self);
    gboolean ret = FALSE;
    Window r, w;
    gint xw, yw;
    guint state;

    if (XQueryPointer (CCM_DISPLAY_XDISPLAY (self->priv->display),
                       CCM_WINDOW_XWINDOW (root),
                       &r, &w, x, y, &xw, &yw, &state))
    {
        if (below)
            *below = ccm_screen_find_window_at_pos (self, *x, *y);
        ret = CCM_WINDOW_XWINDOW (root) == r;
    }

    return ret;
}

Visual *
ccm_screen_get_visual_for_depth (CCMScreen * self, int depth)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (depth > 0, NULL);

    XVisualInfo vinfo;

    if (!XMatchVisualInfo (CCM_DISPLAY_XDISPLAY (self->priv->display),
                           CCM_SCREEN_NUMBER (self),
                           depth, TrueColor, &vinfo))
        return NULL;

    return vinfo.visual;
}

G_GNUC_PURE CCMWindow*
ccm_screen_get_active_window (CCMScreen* self)
{
    g_return_val_if_fail(self != NULL, NULL);

    return self->priv->active;
}

void
ccm_screen_wait_vblank (CCMScreen* self)
{
    g_return_if_fail(self != NULL);

    if (self->priv->sync_with_vblank)
    {
        guint vblank_count;

        ccm_display_sync (self->priv->display);
        self->priv->get_video_sync (&vblank_count);
        self->priv->wait_video_sync (2, (vblank_count + 1) % 2, &vblank_count);
    }
}

G_GNUC_PURE gboolean
ccm_screen_get_redirect_input (CCMScreen* self)
{
    g_return_val_if_fail(self != NULL, FALSE);

    return self->priv->redirect_input;
}

void
ccm_screen_set_redirect_input (CCMScreen* self, gboolean redirect_input)
{
    g_return_if_fail(self != NULL);

    self->priv->redirect_input = redirect_input;
}
