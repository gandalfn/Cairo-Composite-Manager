/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-display.c
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

#include <string.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>
#include <GL/glx.h>
#include <gtk/gtk.h>

#include "ccm-debug.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-config.h"
#include "ccm-window.h"
#include "ccm-timeline.h"

G_DEFINE_TYPE (CCMDisplay, ccm_display, CCM_TYPE_WATCH);

enum
{
    PROP_0,
    PROP_XDISPLAY,
    PROP_USE_XSHM,
    PROP_USE_RANDR,
    PROP_USE_GLX
};

enum
{
    CCM_DISPLAY_OPTION_USE_XSHM,
    CCM_DISPLAY_UNMANAGED_SCREEN,
    CCM_DISPLAY_OPTION_N
};

static gchar *CCMDisplayOptions[CCM_DISPLAY_OPTION_N] = {
    "use_xshm",
    "unmanaged_screen"
};

enum
{
    EVENT,
    DAMAGE_EVENT,
    DAMAGE_DESTROY,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

typedef struct
{
    gboolean available;
    int event_base;
    int error_base;
} CCMExtension;

typedef struct
{
    gulong press;
    gulong release;
    gulong motion;
} CCMPointerEvents;

struct _CCMDisplayPrivate
{
    Display*         xdisplay;

    uint             grab_count;

    gint             nb_screens;
    CCMScreen**      screens;

    CCMExtension     shape;
    CCMExtension     composite;
    CCMExtension     damage;
    CCMExtension     shm;
    gboolean         shm_shared_pixmap;
    CCMExtension     fixes;
    CCMExtension     input;
    CCMExtension     randr;
    CCMExtension     glx;

    CCMSet*          registered_damage;

    GSList*          pointers;
    int              type_button_press;
    int              type_button_release;
    int              type_motion_notify;
    CCMPointerEvents last_events;

    gboolean         use_shm;
    CCMConfig*       options[CCM_DISPLAY_OPTION_N];
};

static gint CCMLastXError = 0;
static CCMDisplay* CCMDefaultDisplay = NULL;

static void ccm_display_process_events (CCMWatch* watch);

#define CCM_DISPLAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DISPLAY, CCMDisplayPrivate))

typedef struct
{
    volatile int          ref_count;
    Damage                damage;
    CCMDamageCallbackFunc func;
    CCMDrawable*          drawable;
} CCMDamageCallback;

static int
ccm_damage_callback_compare (CCMDamageCallback* a, CCMDamageCallback* b)
{
    return a->damage - b->damage;
}

static int
ccm_damage_callback_compare_with_damage (CCMDamageCallback* self, guint damage)
{
    return self->damage - damage;
}

static CCMDamageCallback*
ccm_damage_callback_new ()
{
    CCMDamageCallback* self = g_slice_new0 (CCMDamageCallback);
    self->ref_count = 1;

    return self;
}

static CCMDamageCallback*
ccm_damage_callback_ref (CCMDamageCallback* self)
{
    g_atomic_int_inc (&self->ref_count);
    return self;
}

static void
ccm_damage_callback_unref (CCMDamageCallback* self)
{
    if (g_atomic_int_dec_and_test (&self->ref_count))
    {
        XDamageDestroy (CCM_DISPLAY_XDISPLAY (CCMDefaultDisplay), self->damage);
        g_signal_emit (CCMDefaultDisplay, signals[DAMAGE_DESTROY], 0, self->damage, self->drawable);
        g_slice_free (CCMDamageCallback, self);
    }
}

static void
ccm_display_set_property (GObject * object, guint prop_id, const GValue * value,
                          GParamSpec * pspec)
{
    CCMDisplayPrivate *priv = CCM_DISPLAY_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_XDISPLAY:
        {
            priv->xdisplay = g_value_get_pointer (value);
        }
            break;
        default:
            break;
    }
}

static void
ccm_display_get_property (GObject * object, guint prop_id, GValue * value,
                          GParamSpec * pspec)
{
    CCMDisplayPrivate *priv = CCM_DISPLAY_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_XDISPLAY:
            {
                g_value_set_pointer (value, priv->xdisplay);
            }
            break;
        case PROP_USE_XSHM:
            {
                g_value_set_boolean (value, priv->use_shm);
            }
            break;
        case PROP_USE_RANDR:
            {
                g_value_set_boolean (value, priv->randr.available);
            }
            break;
        case PROP_USE_GLX:
            {
                g_value_set_boolean (value, priv->glx.available);
            }
            break;
        default:
            break;
    }
}

static void
ccm_display_init (CCMDisplay * self)
{
    self->priv = CCM_DISPLAY_GET_PRIVATE (self);

    self->priv->xdisplay = NULL;
    self->priv->grab_count = 0;
    self->priv->nb_screens = 0;
    self->priv->screens = NULL;
    self->priv->use_shm = FALSE;
    self->priv->pointers = NULL;
    self->priv->type_button_press = 0;
    self->priv->type_button_release = 0;
    self->priv->type_motion_notify = 0;
    self->priv->last_events.press = 0;
    self->priv->last_events.release = 0;
    self->priv->last_events.motion = 0;
    self->priv->registered_damage = ccm_set_new (G_TYPE_POINTER,
                                                 (GBoxedCopyFunc)ccm_damage_callback_ref,
                                                 (GDestroyNotify)ccm_damage_callback_unref,
                                                 (CCMSetCompareFunc)ccm_damage_callback_compare);
}

static void
ccm_display_finalize (GObject * object)
{
    CCMDisplay *self = CCM_DISPLAY (object);
    gint cpt;

    ccm_debug ("DISPLAY FINALIZE");

    if (self == CCMDefaultDisplay)
        CCMDefaultDisplay = NULL;

    if (self->priv->registered_damage)
        g_object_unref (self->priv->registered_damage);

    if (self->priv->nb_screens)
    {
        for (cpt = 0; cpt < self->priv->nb_screens; ++cpt)
        {
            if (self->priv->screens[cpt] && CCM_IS_SCREEN (self->priv->screens[cpt]))
            {
                g_object_unref (self->priv->screens[cpt]);
                self->priv->screens[cpt] = NULL;
            }
        }
        self->priv->nb_screens = 0;

        g_slice_free1 (sizeof (CCMScreen *) * (self->priv->nb_screens + 1),
                       self->priv->screens);
    }

    for (cpt = 0; cpt < CCM_DISPLAY_OPTION_N; ++cpt)
        g_object_unref (self->priv->options[cpt]);

    G_OBJECT_CLASS (ccm_display_parent_class)->finalize (object);
}

static void
ccm_display_class_init (CCMDisplayClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMDisplayPrivate));

    CCM_WATCH_CLASS (klass)->process_watch = ccm_display_process_events;
    object_class->get_property = ccm_display_get_property;
    object_class->set_property = ccm_display_set_property;
    object_class->finalize = ccm_display_finalize;

    g_object_class_install_property (object_class, PROP_XDISPLAY,
                                     g_param_spec_pointer ("xdisplay",
                                                           "XDisplay",
                                                           "Display xid",
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (object_class, PROP_USE_XSHM,
                                     g_param_spec_boolean ("use_xshm",
                                                           "UseXShm",
                                                           "Use XSHM", TRUE,
                                                           G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_USE_RANDR,
                                     g_param_spec_boolean ("use_randr",
                                                           "UseRandr",
                                                           "Use Randr", TRUE,
                                                           G_PARAM_READWRITE));

    g_object_class_install_property (object_class, PROP_USE_GLX,
                                     g_param_spec_boolean ("use_glx",
                                                           "UseGLX",
                                                           "Use GLX Extension",
                                                           TRUE,
                                                           G_PARAM_READABLE));

    signals[EVENT] =
        g_signal_new ("event", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,
                      G_TYPE_POINTER);

    signals[DAMAGE_EVENT] =
        g_signal_new ("damage-event", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE, 2,
                      G_TYPE_INT, G_TYPE_POINTER);

    signals[DAMAGE_DESTROY] =
        g_signal_new ("damage-destroy", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE, 2,
                      G_TYPE_INT, G_TYPE_POINTER);
}

static void
ccm_display_load_config (CCMDisplay * self)
{
    g_return_if_fail (self != NULL);

    gint cpt;

    for (cpt = 0; cpt < CCM_DISPLAY_OPTION_N; ++cpt)
    {
        self->priv->options[cpt] = ccm_config_new (-1, NULL, CCMDisplayOptions[cpt]);
    }
    self->priv->use_shm = ccm_config_get_boolean (self->priv->options[CCM_DISPLAY_OPTION_USE_XSHM], NULL) &&
                                                  self->priv->shm.available;
}

static gboolean
ccm_display_init_shape (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (XShapeQueryExtension (self->priv->xdisplay,
                              &self->priv->shape.event_base,
                              &self->priv->shape.error_base))
    {
        self->priv->shape.available = TRUE;
        ccm_debug ("SHAPE EVENT BASE: %i", self->priv->shape.event_base);
        ccm_debug ("SHAPE ERROR BASE: %i", self->priv->shape.error_base);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_composite (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (XCompositeQueryExtension (self->priv->xdisplay,
                                  &self->priv->composite.event_base,
                                  &self->priv->composite.error_base))
    {
        self->priv->composite.available = TRUE;
        ccm_debug ("COMPOSITE EVENT BASE: %i", self->priv->composite.event_base);
        ccm_debug ("COMPOSITE ERROR BASE: %i",
                   self->priv->composite.error_base);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_damage (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (XDamageQueryExtension (self->priv->xdisplay,
                               &self->priv->damage.event_base,
                               &self->priv->damage.error_base))
    {
        self->priv->damage.available = TRUE;
        ccm_debug ("DAMAGE EVENT BASE: %i", self->priv->damage.event_base);
        ccm_debug ("DAMAGE ERROR BASE: %i", self->priv->damage.error_base);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_shm (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, FALSE);
    int major, minor;

    if (XShmQueryExtension (self->priv->xdisplay) &&
        XShmQueryVersion (self->priv->xdisplay, &major, &minor,
                          &self->priv->shm_shared_pixmap))
    {
        self->priv->shm.available = TRUE;
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_xfixes (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    if (XFixesQueryExtension (self->priv->xdisplay,
                              &self->priv->fixes.event_base,
                              &self->priv->fixes.error_base))
    {
        self->priv->fixes.available = TRUE;
        ccm_debug ("FIXES EVENT BASE: %i", self->priv->fixes.event_base);
        ccm_debug ("FIXES ERROR BASE: %i", self->priv->fixes.error_base);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_input (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, FALSE);

    XExtensionVersion *version;

#ifdef HAVE_XI2
    version = XQueryInputVersion (self->priv->xdisplay, XI_2_Major, XI_2_Minor);
#else
    version = XGetExtensionVersion (self->priv->xdisplay, INAME);
#endif

    if (version && (version != (XExtensionVersion *) NoSuchExtension))
    {
        self->priv->input.available = TRUE;
        XFree (version);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_randr(CCMDisplay *self)
{
    g_return_val_if_fail(self != NULL, FALSE);

    if (XRRQueryExtension (self->priv->xdisplay,
                           &self->priv->randr.event_base,
                           &self->priv->randr.error_base))
    {
        self->priv->randr.available = TRUE;
        ccm_debug ("RANDR EVENT BASE: %i", self->priv->randr.event_base);
        ccm_debug ("RANDR ERROR BASE: %i", self->priv->randr.error_base);
        return TRUE;
    }

    return FALSE;
}

static gboolean
ccm_display_init_glx(CCMDisplay *self)
{
    g_return_val_if_fail(self != NULL, FALSE);

    if (glXQueryExtension (self->priv->xdisplay,
                           &self->priv->glx.event_base,
                           &self->priv->glx.error_base))
    {
        self->priv->glx.available = TRUE;
        ccm_debug ("GLX EVENT BASE: %i", self->priv->glx.event_base);
        ccm_debug ("GLX ERROR BASE: %i", self->priv->glx.error_base);
        return TRUE;
    }

    return FALSE;
}

static int
ccm_display_error_handler (Display * dpy, XErrorEvent * evt)
{
    gchar str[128];

    XGetErrorText (dpy, evt->error_code, str, 128);
    ccm_debug ("ERROR: Xerror: %s", str);

    sprintf (str, "%d", evt->request_code);
    XGetErrorDatabaseText (dpy, "XRequest", str, "", str, 128);
    if (strcmp (str, ""))
        ccm_debug ("ERROR: XRequest: (%s)", str);

    CCMLastXError = evt->error_code;

    ccm_debug_backtrace ();

    return 0;
}

static void
ccm_display_process_events (CCMWatch* watch)
{
    g_return_if_fail (watch != NULL);

    CCMDisplay* self = CCM_DISPLAY (watch);
    XEvent xevent;
    gboolean have_create_notify = FALSE;

    while (!have_create_notify && XEventsQueued (CCM_DISPLAY_XDISPLAY (self), QueuedAfterReading))
    {
        XNextEvent (CCM_DISPLAY_XDISPLAY (self), &xevent);
        ccm_debug ("EVENT %i", xevent.type);

        if (xevent.type == self->priv->damage.event_base + XDamageNotify)
        {
            XDamageNotifyEvent* event_damage = (XDamageNotifyEvent *)&xevent;

            CCMDamageCallback* callback;

            callback = (CCMDamageCallback*)ccm_set_search (self->priv->registered_damage,
                                                           G_TYPE_UINT, NULL, NULL,
                                                           (gpointer)event_damage->damage,
                                                           (CCMSetValueCompareFunc)ccm_damage_callback_compare_with_damage);
            if (callback)
            {
                g_signal_emit (self, signals[DAMAGE_EVENT], 0, event_damage->damage, callback->drawable);
            }
        }
        else
        {
            g_signal_emit (self, signals[EVENT], 0, &xevent);
            if (xevent.type == CreateNotify)
                have_create_notify = TRUE;
        }
    }
}

CCMDisplay *
ccm_display_new (gchar * display)
{
    CCMDisplay *self;
    gint cpt;
    GSList *unmanaged = NULL;
    Display *xdisplay;

    xdisplay = XOpenDisplay (display);
    if (!xdisplay)
    {
        g_warning ("Unable to open display %s", display);
        return NULL;
    }

    self = g_object_new (CCM_TYPE_DISPLAY, "xdisplay", xdisplay, NULL);

    if (!ccm_display_init_shape (self))
    {
        g_object_unref (self);
        g_warning ("Shape init failed for %s", display);
        return NULL;
    }

    if (!ccm_display_init_composite (self))
    {
        g_object_unref (self);
        g_warning ("Composite init failed for %s", display);
        return NULL;
    }

    if (!ccm_display_init_damage (self))
    {
        g_object_unref (self);
        g_warning ("Damage init failed for %s", display);
        return NULL;
    }

    if (!ccm_display_init_shm (self))
    {
        g_object_unref (self);
        g_warning ("SHM init failed for %s", display);
        return NULL;
    }
    if (!ccm_display_init_xfixes (self))
    {
        g_object_unref (self);
        g_warning ("FIXES init failed for %s", display);
        return NULL;
    }

    if (!ccm_display_init_input (self))
    {
        g_object_unref (self);
        g_warning ("TEST init failed for %s", display);
        return NULL;
    }

    ccm_display_init_randr (self);
    ccm_display_init_glx (self);

    if (CCMDefaultDisplay == NULL) CCMDefaultDisplay = self;

    ccm_display_load_config (self);

    XSetErrorHandler (ccm_display_error_handler);

    self->priv->nb_screens = ScreenCount (self->priv->xdisplay);
    self->priv->screens = g_slice_alloc0 (sizeof (CCMScreen *) * (self->priv->nb_screens + 1));

    unmanaged = ccm_config_get_integer_list (self->priv->options[CCM_DISPLAY_UNMANAGED_SCREEN],
                                             NULL);

    for (cpt = 0; cpt < self->priv->nb_screens; ++cpt)
    {
        gboolean found = FALSE;

        if (unmanaged)
        {
            GSList *item;

            for (item = unmanaged; item; item = item->next)
            {
                if (GPOINTER_TO_INT (item->data) == cpt)
                {
                    found = TRUE;
                    break;
                }
            }
        }
        if (!found)
            self->priv->screens[cpt] = ccm_screen_new (self, cpt);
    }
    g_slist_free (unmanaged);

    ccm_watch_watch (CCM_WATCH (self), ConnectionNumber (CCM_DISPLAY_XDISPLAY (self)), NULL);

    return self;
}

G_GNUC_PURE Display *
ccm_display_get_xdisplay (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->xdisplay;
}

G_GNUC_PURE CCMScreen *
ccm_display_get_screen (CCMDisplay * self, guint number)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (number < self->priv->nb_screens, NULL);

    return self->priv->screens[number];
}

G_GNUC_PURE int
ccm_display_get_shape_notify_event_type (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, 0);

    return self->priv->shape.event_base + ShapeNotify;
}

void
ccm_display_flush (CCMDisplay * self)
{
    g_return_if_fail (self != NULL);

    XFlush (self->priv->xdisplay);
}

void
ccm_display_sync (CCMDisplay * self)
{
    g_return_if_fail (self != NULL);

    XSync (self->priv->xdisplay, FALSE);
}

void
ccm_display_grab (CCMDisplay * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->grab_count == 0)
    {
        XGrabServer (self->priv->xdisplay);
    }
    self->priv->grab_count++;
}

void
ccm_display_ungrab (CCMDisplay * self)
{
    g_return_if_fail (self != NULL);

    if (self->priv->grab_count > 0)
    {
        self->priv->grab_count--;
        if (self->priv->grab_count == 0)
        {
            XUngrabServer (self->priv->xdisplay);
        }
    }
}

void
ccm_display_trap_error (CCMDisplay * self)
{
    CCMLastXError = 0;
}

gint
ccm_display_pop_error (CCMDisplay * self)
{
    g_return_val_if_fail (self != NULL, 0);

    ccm_display_sync (self);

    return CCMLastXError;
}

G_GNUC_PURE CCMDisplay*
ccm_display_get_default()
{
    return CCMDefaultDisplay;
}

guint32
ccm_display_register_damage (CCMDisplay* self, CCMDrawable* drawable, CCMDamageCallbackFunc func)
{
    Damage damage = XDamageCreate (CCM_DISPLAY_XDISPLAY (self),
                                   ccm_drawable_get_xid (drawable),
                                   XDamageReportDeltaRectangles);
    if (damage)
    {
        GSList* item;
        gboolean found = FALSE;
        CCMDamageCallback* callback;

        callback = (CCMDamageCallback*)ccm_set_search (self->priv->registered_damage,
                                                       G_TYPE_UINT, NULL, NULL,
                                                       (gpointer)damage,
                                                       (CCMSetValueCompareFunc)ccm_damage_callback_compare_with_damage);
        if (callback == NULL)
        {
            callback = ccm_damage_callback_new ();
            XDamageSubtract (CCM_DISPLAY_XDISPLAY (self), damage, None, None);
            callback->damage = damage;
            callback->func = func;
            callback->drawable = drawable;

            ccm_set_insert (self->priv->registered_damage, callback);
        }
        else
        {
            callback->func = func;
            callback->drawable = drawable;
        }
    }
    else
        damage = None;

    return (guint32)damage;
}

void
ccm_display_unregister_damage (CCMDisplay* self, guint32 damage, CCMDrawable* drawable)
{
    CCMDamageCallback* callback;

    callback = (CCMDamageCallback*)ccm_set_search (self->priv->registered_damage,
                                                   G_TYPE_UINT, NULL, NULL,
                                                   GINT_TO_POINTER (damage),
                                                   (CCMSetValueCompareFunc)ccm_damage_callback_compare_with_damage);

    if (callback && callback->drawable == drawable)
    {
        ccm_set_remove (self->priv->registered_damage, callback);
    }
}

void
ccm_display_process_damage (CCMDisplay* self, guint32 damage)
{
    CCMDamageCallback* callback;

    callback = (CCMDamageCallback*)ccm_set_search (self->priv->registered_damage,
                                                   G_TYPE_UINT, NULL, NULL,
                                                   GINT_TO_POINTER (damage),
                                                   (CCMSetValueCompareFunc)ccm_damage_callback_compare_with_damage);

    if (callback)
    {
        callback->func (callback->drawable, callback->damage);
    }
}
