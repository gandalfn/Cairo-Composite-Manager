/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2008 <gandalfn@club-internet.fr>
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

#include <gdk/gdk.h>

#include "ccm-keybind.h"
#include "ccm-display.h"
#include "ccm-window.h"
#include "eggaccelerators.h"

enum
{
	KEY_PRESS,
	KEY_RELEASE,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (CCMKeybind, ccm_keybind, G_TYPE_OBJECT);

struct _CCMKeybindPrivate
{
	CCMScreen* screen;
	
	gchar* keystring;
	guint keycode;
	guint modifiers;
	gboolean pressed;
};

#define CCM_KEYBIND_GET_PRIVATE(o) \
	((CCMKeybindPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_KEYBIND, CCMKeybindClass))

static void ccm_keybind_get_mod_masks(CCMKeybindClass *klass);
static void ccm_keybind_ungrab (CCMKeybind* self);

static void
ccm_keybind_init (CCMKeybind *self)
{
	self->priv = CCM_KEYBIND_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->keystring = NULL;
	self->priv->keycode = 0;
	self->priv->modifiers = 0;
	self->priv->pressed = FALSE;
}

static void
ccm_keybind_finalize (GObject *object)
{
	CCMKeybind* self = CCM_KEYBIND(object);
	
	if (self->priv->keystring) 
	{
		ccm_keybind_ungrab(self);
		g_free(self->priv->keystring);
	}
	
	G_OBJECT_CLASS (ccm_keybind_parent_class)->finalize (object);
}

static void
ccm_keybind_class_init (CCMKeybindClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMKeybindPrivate));
	
	ccm_keybind_get_mod_masks(klass);
	
	signals[KEY_PRESS] = g_signal_new ("key_press",
									   G_OBJECT_CLASS_TYPE (object_class),
									   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
									   g_cclosure_marshal_VOID__VOID,
									   G_TYPE_NONE, 0, G_TYPE_NONE);
	
	signals[KEY_RELEASE] = g_signal_new ("key_release",
										 G_OBJECT_CLASS_TYPE (object_class),
										 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
										 g_cclosure_marshal_VOID__VOID,
										 G_TYPE_NONE, 0, G_TYPE_NONE);
	
	object_class->finalize = ccm_keybind_finalize;
}

static void
ccm_keybind_get_mod_masks(CCMKeybindClass *klass)
{
	g_return_if_fail(klass != NULL);
	
	GdkKeymap *keymap = gdk_keymap_get_default ();
	guint caps_lock_mask, num_lock_mask, scroll_lock_mask;
	
	egg_keymap_resolve_virtual_modifiers (keymap, EGG_VIRTUAL_LOCK_MASK,
										  &caps_lock_mask);

	egg_keymap_resolve_virtual_modifiers (keymap, EGG_VIRTUAL_NUM_LOCK_MASK,
										  &num_lock_mask);

	egg_keymap_resolve_virtual_modifiers (keymap, EGG_VIRTUAL_SCROLL_LOCK_MASK,
										  &scroll_lock_mask);
	
	klass->mod_masks[0] = 0;
	klass->mod_masks[1] = num_lock_mask;
	klass->mod_masks[2] = caps_lock_mask;
	klass->mod_masks[3] = scroll_lock_mask;
	klass->mod_masks[4] = num_lock_mask  | caps_lock_mask;
	klass->mod_masks[5] = num_lock_mask  | scroll_lock_mask;
	klass->mod_masks[6] = caps_lock_mask | scroll_lock_mask;
	klass->mod_masks[7] = num_lock_mask  | caps_lock_mask | scroll_lock_mask;
}

static void
ccm_keybind_grab (CCMKeybind* self)
{
	g_return_if_fail(self != NULL);
	
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	
	guint cpt;
	
	for (cpt = 0; cpt < 8; cpt++) 
	{
		XGrabKey (CCM_DISPLAY_XDISPLAY (display), self->priv->keycode, 
				  self->priv->modifiers | CCM_KEYBIND_GET_CLASS(self)->mod_masks [cpt], 
				  CCM_WINDOW_XWINDOW (root), True, 
				  GrabModeAsync, GrabModeAsync);
	}
}

static void
ccm_keybind_ungrab (CCMKeybind* self)
{
	g_return_if_fail(self != NULL);
	
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	
	guint cpt;
	
	for (cpt = 0; cpt < 8; cpt++) 
	{
		XUngrabKey (CCM_DISPLAY_XDISPLAY (display), self->priv->keycode, 
					self->priv->modifiers | CCM_KEYBIND_GET_CLASS(self)->mod_masks [cpt], 
					CCM_WINDOW_XWINDOW (root));
	}
}

static void
ccm_keybind_on_event(CCMKeybind* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	switch (event->type) 
	{
		case KeyPress:
		{
			if (!self->priv->pressed)
			{
				guint event_mods;
	
				event_mods = event->xkey.state & ~(CCM_KEYBIND_GET_CLASS(self)->mod_masks[7]);

				if (self->priv->keycode == event->xkey.keycode && 
					self->priv->modifiers == event_mods) 
				{
					self->priv->pressed = TRUE;
					g_signal_emit (self, signals[KEY_PRESS], 0);
				}
			}
		}
		break;
		case KeyRelease:
		{
			if (XPending (event->xkey.display))
			{
	  			XEvent next_event;

	  			XPeekEvent (event->xkey.display, &next_event);

				if (next_event.type == KeyPress &&
					next_event.xkey.keycode == event->xkey.keycode &&
	      			next_event.xkey.time == event->xkey.time)
				{
	      			break;
	    		}
			}
			if (self->priv->pressed)
			{
				guint event_mods;
	
				event_mods = event->xkey.state & ~(CCM_KEYBIND_GET_CLASS(self)->mod_masks[7]);
				
				if (self->priv->keycode == event->xkey.keycode)
				{
					self->priv->pressed = FALSE;
					g_signal_emit (self, signals[KEY_RELEASE], 0);
				}
			}
		}
		break;
	}
}

static void
ccm_keybind_on_keymap_changed(CCMKeybind* self, GdkKeymap* keymap)
{
	ccm_keybind_ungrab(self);
	ccm_keybind_get_mod_masks(CCM_KEYBIND_GET_CLASS(self));
	ccm_keybind_grab(self);
}

CCMKeybind*
ccm_keybind_new (CCMScreen* screen, gchar* keystring)
{
	g_return_val_if_fail(screen != NULL, NULL);
	g_return_val_if_fail(keystring != NULL, NULL);
	
	CCMKeybind* self = g_object_new(CCM_TYPE_KEYBIND, NULL);
	GdkKeymap* keymap = gdk_keymap_get_default ();
	guint keysym = 0;
	EggVirtualModifierType virtual_mods = 0;
	CCMDisplay* display = ccm_screen_get_display (screen);
	
	self->priv->screen = screen;
	self->priv->keystring = g_strdup(keystring);
	if (!egg_accelerator_parse_virtual (self->priv->keystring, &keysym, 
										NULL, &virtual_mods))
	{
		g_warning("Error on parse %s", self->priv->keystring);
		g_object_unref(self);
		return NULL;
	}
	
	self->priv->keycode = XKeysymToKeycode (CCM_DISPLAY_XDISPLAY(display), 
											keysym);
	if (!self->priv->keycode)
	{
		g_warning("Error on get keycode %s", self->priv->keystring);
		g_object_unref(self);
		return NULL;
	}
	egg_keymap_resolve_virtual_modifiers (keymap, virtual_mods, 
										  &self->priv->modifiers);
	
	g_signal_connect_swapped(display, "event", 
							 G_CALLBACK(ccm_keybind_on_event), self);
	
	g_signal_connect_swapped (keymap, "keys_changed", 
							  G_CALLBACK (ccm_keybind_on_keymap_changed), self);
	
	ccm_keybind_grab(self);
	
	return self;
}
