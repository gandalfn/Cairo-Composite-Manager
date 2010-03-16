/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007-2010 <gandalfn@club-internet.fr>
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
#include <gtk/gtk.h>

#include "ccm-debug.h"
#include "ccm-marshallers.h"
#include "eggaccelerators.h"
#include "ccm-keybind.h"
#include "ccm-display.h"
#include "ccm-window.h"

enum
{
    KEY_PRESS,
    KEY_RELEASE,
    KEY_MOTION,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (CCMKeybind, ccm_keybind, G_TYPE_OBJECT);

struct _CCMKeybindPrivate
{
    CCMDisplay *display;
    Window root;

    gchar *keystring;
    guint keycode;
    guint button;
    GdkModifierType modifiers;

    guint num_lock_mask;
    guint caps_lock_mask;

    gboolean grabbed;
    gboolean exclusive;
    gboolean key_pressed;
};

#define CCM_KEYBIND_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_KEYBIND, CCMKeybindPrivate))

static void ccm_keybind_ungrab (CCMKeybind * self);
static void ccm_keybind_on_event (CCMKeybind * self, XEvent * xevent);
static void ccm_keybind_on_keymap_changed (CCMKeybind * self, GdkKeymap * keymap);

static void
ccm_keybind_init (CCMKeybind * self)
{
    self->priv = CCM_KEYBIND_GET_PRIVATE (self);
    self->priv->display = NULL;
    self->priv->root = None;
    self->priv->keystring = NULL;
    self->priv->keycode = 0;
    self->priv->modifiers = 0;
    self->priv->caps_lock_mask = GDK_LOCK_MASK;
    self->priv->num_lock_mask = GDK_MOD2_MASK;
    self->priv->grabbed = FALSE;
    self->priv->exclusive = FALSE;
    self->priv->key_pressed = FALSE;
}

static void
ccm_keybind_finalize (GObject * object)
{
    CCMKeybind *self = CCM_KEYBIND (object);
    GdkKeymap *keymap = gdk_keymap_get_default ();

    g_signal_handlers_disconnect_by_func (self->priv->display,
                                          ccm_keybind_on_event, self);

    g_signal_handlers_disconnect_by_func (keymap, ccm_keybind_on_keymap_changed,
                                          self);
    ccm_keybind_ungrab (self);
    g_free (self->priv->keystring);

    G_OBJECT_CLASS (ccm_keybind_parent_class)->finalize (object);
}

static void
ccm_keybind_class_init (CCMKeybindClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (CCMKeybindPrivate));

    signals[KEY_PRESS] =
        g_signal_new ("key_press", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0,
                      G_TYPE_NONE);

    signals[KEY_RELEASE] =
        g_signal_new ("key_release", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0,
                      G_TYPE_NONE);

    signals[KEY_MOTION] =
        g_signal_new ("key_motion", G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      ccm_cclosure_marshal_VOID__INT_INT, G_TYPE_NONE, 2,
                      G_TYPE_INT, G_TYPE_INT);

    object_class->finalize = ccm_keybind_finalize;
}

static void
ccm_keybind_ungrab (CCMKeybind * self)
{
    g_return_if_fail (self != NULL);

    guint cpt;
    guint mod_masks[] = {
        0,
        self->priv->num_lock_mask,
        self->priv->caps_lock_mask,
        self->priv->num_lock_mask | self->priv->caps_lock_mask
    };

    self->priv->grabbed = FALSE;
    for (cpt = 0; cpt < G_N_ELEMENTS (mod_masks); ++cpt)
    {
        if (self->priv->button)
            XUngrabButton (CCM_DISPLAY_XDISPLAY (self->priv->display),
                           self->priv->button,
                           self->priv->modifiers | mod_masks[cpt],
                           self->priv->root);
        else
            XUngrabKey (CCM_DISPLAY_XDISPLAY (self->priv->display),
                        self->priv->keycode,
                        self->priv->modifiers | mod_masks[cpt],
                        self->priv->root);
    }
}

static void
ccm_keybind_grab (CCMKeybind * self)
{
    g_return_if_fail (self != NULL);

    if (!self->priv->grabbed)
    {
        guint cpt;
        guint mod_masks[] = {
            0,
            self->priv->num_lock_mask,
            self->priv->caps_lock_mask,
            self->priv->num_lock_mask | self->priv->caps_lock_mask
        };

        self->priv->grabbed = TRUE;
        for (cpt = 0; self->priv->grabbed && cpt < G_N_ELEMENTS (mod_masks);
             ++cpt)
        {
            if (self->priv->button)
            {
                if (!XGrabButton (CCM_DISPLAY_XDISPLAY (self->priv->display),
                                  self->priv->button, self->priv->modifiers | mod_masks[cpt],
                                  self->priv->root, !self->priv->exclusive,
                                  ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                                  GrabModeAsync,
                                  self->priv->exclusive ? GrabModeAsync : GrabModeSync, None,
                                  None))
                {
                    ccm_keybind_ungrab (self);
                    self->priv->grabbed = FALSE;
                }
            }
            else
            {
                if (!XGrabKey (CCM_DISPLAY_XDISPLAY (self->priv->display),
                               self->priv->keycode,
                               self->priv->modifiers | mod_masks[cpt], self->priv->root,
                               !self->priv->exclusive, GrabModeAsync,
                               self->priv->exclusive ? GrabModeAsync : GrabModeSync))
                {
                    ccm_keybind_ungrab (self);
                    self->priv->grabbed = FALSE;
                }
            }
        }
    }
}

static void
ccm_keybind_on_event (CCMKeybind * self, XEvent * xevent)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (xevent != NULL);

    guint event_mods;

    switch (xevent->type)
    {
        case KeyPress:
        {
            if (xevent->xkey.root != self->priv->root)
                return;

            event_mods =
                xevent->xkey.state & ~(self->priv->caps_lock_mask | self->
                                       priv->num_lock_mask);

            ccm_debug ("Key Press: window = 0x%lx, keycode = %i, modifiers = %i",
                       xevent->xkey.window, xevent->xkey.keycode, xevent->xkey.state);
            if (self->priv->keycode && 
                self->priv->keycode == xevent->xkey.keycode && 
                self->priv->modifiers == event_mods)
            {
                if (!self->priv->key_pressed)
                {
                    g_signal_emit (self, signals[KEY_PRESS], 0);
                    self->priv->key_pressed = TRUE;
                }
                if (!self->priv->exclusive)
                {
                    ccm_keybind_ungrab (self);
                    XAllowEvents (xevent->xkey.display, ReplayKeyboard,
                                  CurrentTime);
                    XSync (xevent->xkey.display, FALSE);
                    ccm_keybind_grab (self);
                }
            }
        }
            break;
        case ButtonPress:
        {
            gint button = xevent->xbutton.button;
            gint state = xevent->xbutton.state & 0xFF;

            if (xevent->xbutton.root != self->priv->root)
                return;

            event_mods = state & ~(self->priv->caps_lock_mask | self->priv->num_lock_mask);

            if (self->priv->button && self->priv->button == button
                && self->priv->modifiers == event_mods)
            {
                ccm_debug ("Button Press: window = 0x%lx, button = %i, modifiers = %i",
                           xevent->xbutton.window, xevent->xbutton.button, event_mods);
                g_signal_emit (self, signals[KEY_PRESS], 0);
                self->priv->key_pressed = TRUE;
                if (!self->priv->exclusive)
                {
                    ccm_keybind_ungrab (self);
                    XAllowEvents (xevent->xbutton.display, ReplayKeyboard,
                                  CurrentTime);
                    XSync (xevent->xbutton.display, FALSE);
                    ccm_keybind_grab (self);
                }
            }
        }
            break;
        case KeyRelease:
        {
            if (xevent->xkey.root != self->priv->root)
                return;

            ccm_debug ("Key Release: keycode = %i, modifiers = %i",
                       xevent->xkey.keycode, xevent->xkey.state);
            if (self->priv->keycode == xevent->xkey.keycode)
            {
                if (self->priv->key_pressed)
                {
                    g_signal_emit (self, signals[KEY_RELEASE], 0);
                    self->priv->key_pressed = FALSE;
                }
                if (!self->priv->exclusive)
                {
                    ccm_keybind_ungrab (self);
                    XAllowEvents (xevent->xkey.display, ReplayKeyboard,
                                  CurrentTime);
                    XSync (xevent->xkey.display, FALSE);
                    ccm_keybind_grab (self);
                }
            }
        }
            break;
        case ButtonRelease:
        {
            gint button = xevent->xbutton.button;
            gint state = xevent->xbutton.state & 0xFF;

            if (xevent->xbutton.root != self->priv->root)
                return;

            event_mods = state & ~(self->priv->caps_lock_mask | self->priv->num_lock_mask);

            ccm_debug ("Button Release: window = 0x%lx, button = %i, modifiers = %i",
                       xevent->xbutton.window, button, event_mods);
            ccm_debug ("Button Release: button = %i, modifiers = %i",
                       self->priv->button, self->priv->modifiers);

            if (self->priv->button && 
                self->priv->button == button && 
                self->priv->modifiers == event_mods)
            {
                g_signal_emit (self, signals[KEY_RELEASE], 0);
                self->priv->key_pressed = FALSE;
                if (!self->priv->exclusive)
                {
                    ccm_keybind_ungrab (self);
                    XAllowEvents (xevent->xbutton.display, ReplayKeyboard,
                                  CurrentTime);
                    XSync (xevent->xbutton.display, FALSE);
                    ccm_keybind_grab (self);
                }
            }
        }
            break;
        case MotionNotify:
        {
            gint button = xevent->xbutton.state >> 8;
            gint state = xevent->xbutton.state & 0xFF;

            if (xevent->xbutton.root != self->priv->root)
                return;

            event_mods = state & ~(self->priv->caps_lock_mask | self->priv->num_lock_mask);

            ccm_debug ("Motion notify: window = 0x%lx, button = %i, modifiers = %i",
                       xevent->xbutton.windowI, button, event_mods);

            if (self->priv->button && self->priv->button == button && 
                self->priv->modifiers == event_mods)
            {
                g_signal_emit (self, signals[KEY_MOTION], 0,
                               xevent->xbutton.x_root,
                               xevent->xbutton.y_root);
                if (!self->priv->exclusive)
                {
                    ccm_keybind_ungrab (self);
                    XAllowEvents (xevent->xbutton.display, ReplayKeyboard,
                                  CurrentTime);
                    XSync (xevent->xbutton.display, FALSE);
                    ccm_keybind_grab (self);
                }
            }
        }
            break;
        default:
            break;
    }
}

static void
ccm_keybind_on_keymap_changed (CCMKeybind * self, GdkKeymap * keymap)
{
    ccm_keybind_ungrab (self);
    ccm_keybind_grab (self);
}

CCMKeybind *
ccm_keybind_new (CCMScreen * screen, gchar * keystring, gboolean exclusive)
{
    g_return_val_if_fail (screen != NULL, NULL);
    g_return_val_if_fail (keystring != NULL, NULL);

    CCMKeybind *self = g_object_new (CCM_TYPE_KEYBIND, NULL);
    EggVirtualModifierType virtual_mods;
    GdkKeymap *keymap = gdk_keymap_get_default ();
    guint keysym = 0;

    self->priv->display = ccm_screen_get_display (screen);
    self->priv->root = CCM_WINDOW_XWINDOW (ccm_screen_get_root_window (screen));
    self->priv->exclusive = exclusive;
    self->priv->keystring = g_strdup (keystring);

    if (!egg_accelerator_parse_virtual (keystring, &keysym, NULL, 
                                        &self->priv->button, &virtual_mods))
    {
        g_warning ("Error on parse keybind %s", self->priv->keystring);
        g_object_unref (self);
        return NULL;
    }
    self->priv->keycode =
        XKeysymToKeycode (CCM_DISPLAY_XDISPLAY (self->priv->display), keysym);
    if (!self->priv->keycode && !self->priv->button)
    {
        g_warning ("Error on get keycode %s", self->priv->keystring);
        g_object_unref (self);
        return NULL;
    }
    egg_keymap_resolve_virtual_modifiers (keymap, virtual_mods,
                                          &self->priv->modifiers);
    ccm_debug ("KEYBIND: button %i, keycode = %i, modifiers = %i",
               self->priv->button, self->priv->keycode, self->priv->modifiers);

    g_signal_connect_swapped (self->priv->display, "event",
                              G_CALLBACK (ccm_keybind_on_event), self);

    g_signal_connect_swapped (keymap, "keys_changed",
                              G_CALLBACK (ccm_keybind_on_keymap_changed), self);

    ccm_keybind_grab (self);

    return self;
}
