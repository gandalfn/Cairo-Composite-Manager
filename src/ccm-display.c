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
#include <X11/Xresource.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>

#include "ccm-debug.h"
#include "ccm-display.h"
#include "ccm-cursor.h"
#include "ccm-screen.h"
#include "ccm-watch.h"
#include "ccm-config.h"
#include "ccm-window.h"

G_DEFINE_TYPE (CCMDisplay, ccm_display, G_TYPE_OBJECT);

enum
{
    PROP_0,
	PROP_XDISPLAY,
	PROP_USE_XSHM,
	PROP_SHM_SHARED_PIXMAP
};

enum
{
	CCM_DISPLAY_OPTION_USE_XSHM,
	CCM_DISPLAY_UNMANAGED_SCREEN,
	CCM_DISPLAY_OPTION_N
};

static gchar* CCMDisplayOptions[CCM_DISPLAY_OPTION_N] = {
	"use_xshm",
	"unmanaged_screen"
};

enum
{
	EVENT,
	DAMAGE_EVENT,
	CURSOR_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

typedef struct
{
    gboolean	available;
    int			event_base;
    int			error_base;
} CCMExtension;

typedef struct
{
	gulong press;
	gulong release;
	gulong motion;
} CCMPointerEvents;

struct _CCMDisplayPrivate
{
	Display*			xdisplay;
	
	gint 				nb_screens;
	CCMScreen**			screens;
	
	CCMExtension		shape;
	CCMExtension		composite;
	CCMExtension		damage;
	CCMExtension		shm;
	gboolean			shm_shared_pixmap;
	CCMExtension		fixes;
	CCMExtension		input;

	GSList*  			pointers;
	int					type_button_press;
	int					type_button_release;
	int					type_motion_notify;
	CCMPointerEvents	last_events;

	gchar*				cursors_theme;
	gint				cursors_size;
	CCMCursor*			cursor_current;
	GHashTable*			cursors;
	
	gboolean 			use_shm;
	
	gint				fd;
	CCMConfig*			options[CCM_DISPLAY_OPTION_N];
};

static gint   CCMLastXError = 0;

#define CCM_DISPLAY_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DISPLAY, CCMDisplayPrivate))

static void
ccm_display_set_property(GObject *object,
						 guint prop_id,
						 const GValue *value,
						 GParamSpec *pspec)
{
	CCMDisplayPrivate* priv = CCM_DISPLAY_GET_PRIVATE(object);
    
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
ccm_display_get_property (GObject* object,
						  guint prop_id,
						  GValue* value,
						  GParamSpec* pspec)
{
	CCMDisplayPrivate* priv = CCM_DISPLAY_GET_PRIVATE(object);
    
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
		case PROP_SHM_SHARED_PIXMAP:
		{
			GError* error = NULL;
			gboolean xshm = 
			  ccm_config_get_boolean(priv->options[CCM_DISPLAY_OPTION_USE_XSHM],
									 &error);
				
			if (error)
			{
				g_warning("Error on get xshm configuration value");
				g_error_free(error);
				xshm = FALSE;
			}
			g_value_set_boolean (value, xshm && priv->shm.available && 
								 priv->shm_shared_pixmap);
		}
		break;
		default:
		break;
    }
}

static void
ccm_display_init (CCMDisplay *self)
{
	self->priv = CCM_DISPLAY_GET_PRIVATE(self);
	
	self->priv->xdisplay = NULL;
	self->priv->nb_screens = 0;
	self->priv->fd = 0;
	self->priv->screens = NULL;
	self->priv->shm_shared_pixmap = FALSE;
	self->priv->use_shm = FALSE;
	self->priv->pointers = NULL;
	self->priv->type_button_press = 0;
	self->priv->type_button_release = 0;
	self->priv->type_motion_notify = 0;
	self->priv->last_events.press = 0;
	self->priv->last_events.release = 0;
	self->priv->last_events.motion = 0;
	self->priv->cursors = g_hash_table_new_full (g_int_hash, g_int_equal,
	                                             g_free, g_object_unref);
	self->priv->cursor_current = NULL;
}

static void
ccm_display_finalize (GObject *object)
{
	CCMDisplay* self = CCM_DISPLAY(object);
	gint cpt;
	
	ccm_debug("DISPLAY FINALIZE");
	
	if (self->priv->cursors)
		g_hash_table_destroy (self->priv->cursors);
	
	if (self->priv->pointers)
	{
		GSList* item;
		
		for (item = self->priv->pointers; item; item = item->next)
			XCloseDevice(self->priv->xdisplay, item->data);
		g_slist_free(self->priv->pointers);
	}
			
	if (self->priv->nb_screens) 
	{
		for (cpt = 0; cpt < self->priv->nb_screens; ++cpt)
		{
			if (self->priv->screens[cpt] && 
			    CCM_IS_SCREEN(self->priv->screens[cpt]))
			{
				g_object_unref(self->priv->screens[cpt]);
				self->priv->screens[cpt] = NULL;
			}
		}
		self->priv->nb_screens = 0;
		
		g_slice_free1(sizeof(CCMScreen*) * (self->priv->nb_screens + 1), 
					  self->priv->screens);
	}
	
	for (cpt = 0; cpt < CCM_DISPLAY_OPTION_N; ++cpt)
		g_object_unref(self->priv->options[cpt]);
	
	if (self->priv->fd) fd_remove_watch (self->priv->fd);
	
	G_OBJECT_CLASS (ccm_display_parent_class)->finalize (object);
}

static void
ccm_display_class_init (CCMDisplayClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMDisplayPrivate));

	object_class->get_property = ccm_display_get_property;
    object_class->set_property = ccm_display_set_property;
	object_class->finalize = ccm_display_finalize;

	g_object_class_install_property(object_class, PROP_XDISPLAY,
		g_param_spec_pointer ("xdisplay",
		 					  "XDisplay",
			     			  "Display xid",
			     			  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
	g_object_class_install_property(object_class, PROP_USE_XSHM,
		g_param_spec_boolean ("use_xshm",
		 					  "UseXShm",
			     			  "Use XSHM",
							  TRUE,
			     			  G_PARAM_READWRITE));
	
	g_object_class_install_property(object_class, PROP_SHM_SHARED_PIXMAP,
		g_param_spec_boolean ("shm_shared_pixmap",
		 					  "ShmSharedPixmap",
			     			  "SHM Shared Pixmap",
							  TRUE,
			     			  G_PARAM_READWRITE));
	
	signals[EVENT] = g_signal_new ("event",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__POINTER,
								   G_TYPE_NONE, 1, G_TYPE_POINTER);
	
	signals[DAMAGE_EVENT] = g_signal_new ("damage-event",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__POINTER,
								   G_TYPE_NONE, 1, G_TYPE_POINTER);
	
	signals[CURSOR_CHANGED] = g_signal_new ("cursor-changed",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__POINTER,
								   G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
ccm_display_load_config(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	gint cpt;
	
	for (cpt = 0; cpt < CCM_DISPLAY_OPTION_N; ++cpt)
	{
		self->priv->options[cpt] = ccm_config_new(-1, NULL, 
												  CCMDisplayOptions[cpt]);
	}
	self->priv->use_shm = 
		ccm_config_get_boolean(self->priv->options[CCM_DISPLAY_OPTION_USE_XSHM],
							   NULL) &&	self->priv->shm.available;
}

static void
ccm_display_check_cursor (CCMDisplay* self, Atom cursor_name, 
                          gboolean emit_event)
{
	g_return_if_fail(self != NULL);

	CCMCursor* current = NULL;
	
	if (cursor_name)
	{
		current = g_hash_table_lookup(self->priv->cursors, &cursor_name);

		if (!current)
		{
			XFixesCursorImage *cursor;

			cursor = XFixesGetCursorImage (CCM_DISPLAY_XDISPLAY(self));
			ccm_debug("CHECK CURSOR %li", cursor_name);

			current = ccm_cursor_new(self, cursor);
			XFree (cursor);
			if (current)
				g_hash_table_insert(self->priv->cursors, 
						            g_memdup(&cursor_name, sizeof(gulong)), 
						            current);
		}
	}
	else
	{
		XFixesCursorImage *cursor;

		cursor = XFixesGetCursorImage (CCM_DISPLAY_XDISPLAY(self));
		
		current = ccm_cursor_new(self, cursor);

		XFree (cursor);
	}

	if (self->priv->cursor_current != current)
	{
		gboolean animated = FALSE;

		if (self->priv->cursor_current)
		{
			g_object_get(self->priv->cursor_current, "animated", 
			             &animated, NULL);
			if (animated) g_object_unref(self->priv->cursor_current);
		}
		self->priv->cursor_current = current;
		if (emit_event)
			g_signal_emit(self, signals[CURSOR_CHANGED], 0, 
					      self->priv->cursor_current);
	}

}

static void
ccm_display_get_pointers(CCMDisplay *self)
{
	g_return_if_fail(self != NULL);
	
	XDeviceInfo *info;
    gint ndevices, cpt;
	
	info = XListInputDevices(self->priv->xdisplay, &ndevices);
	for (cpt = 0; cpt < ndevices; ++cpt)
	{
		XDeviceInfo* current = &info[cpt];
        if (current->use == IsXExtensionPointer)
		{
			XDevice* device = XOpenDevice(self->priv->xdisplay, current->id);
			ccm_debug("Found device: %s (%d)", current->name, current->id);
            self->priv->pointers = g_slist_prepend(self->priv->pointers, device);
		}
	}
	XFree(info);
}

static gboolean
ccm_display_init_shape(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XShapeQueryExtension (self->priv->xdisplay,
							  &self->priv->shape.event_base,
							  &self->priv->shape.error_base))
    {
		self->priv->shape.available = TRUE;
		ccm_debug("SHAPE ERROR BASE: %i", self->priv->shape.error_base);
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_composite(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XCompositeQueryExtension (self->priv->xdisplay,
								  &self->priv->composite.event_base,
	    						  &self->priv->composite.error_base))
    {
		self->priv->composite.available = TRUE;
		ccm_debug("COMPOSITE ERROR BASE: %i", self->priv->composite.error_base);
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_damage(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XDamageQueryExtension (self->priv->xdisplay,
							   &self->priv->damage.event_base,
							   &self->priv->damage.error_base))
    {
		self->priv->damage.available = TRUE;
		ccm_debug("DAMAGE ERROR BASE: %i", self->priv->damage.error_base);
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_shm(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	int major,  minor;
	
	if (XShmQueryExtension (self->priv->xdisplay) && 
		XShmQueryVersion(self->priv->xdisplay, &major, &minor, 
						 &self->priv->shm_shared_pixmap))
    {
		self->priv->shm.available = TRUE;
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_xfixes(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XFixesQueryExtension (self->priv->xdisplay,
							  &self->priv->fixes.event_base,
							  &self->priv->fixes.error_base))
    {
		self->priv->fixes.available = TRUE;
		ccm_debug("FIXES ERROR BASE: %i", self->priv->fixes.error_base);
		return TRUE;
    }
	
    return FALSE;
}

static gboolean
ccm_display_init_input(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	XExtensionVersion	*version;
	
#ifdef HAVE_XI2
	version = XQueryInputVersion(self->priv->xdisplay, XI_2_Major, XI_2_Minor);
#else
    version = XGetExtensionVersion(self->priv->xdisplay, INAME);
#endif

    if (version && (version != (XExtensionVersion*) NoSuchExtension)) 
	{
		self->priv->input.available = TRUE;
		XFree(version);
		return TRUE;
    } 
	
    return FALSE;
}

static int
ccm_display_error_handler(Display* dpy, XErrorEvent* evt)
{
	gchar str[128];
	
	XGetErrorText (dpy, evt->error_code, str, 128);
	ccm_debug("ERROR: Xerror: %s", str);
	
	sprintf (str, "%d", evt->request_code);
    XGetErrorDatabaseText (dpy, "XRequest", str, "", str, 128);
    if (strcmp (str, ""))
		ccm_debug("ERROR: XRequest: (%s)", str);
    
	CCMLastXError = evt->error_code;
	
	ccm_debug_backtrace();
	
	return 0;
}

static void
ccm_display_process_events(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XEvent xevent;
	
	while (XEventsQueued(CCM_DISPLAY_XDISPLAY(self), QueuedAfterReading))
	{
		XNextEvent(CCM_DISPLAY_XDISPLAY(self), &xevent);
		
		if (xevent.type == self->priv->damage.event_base + XDamageNotify)
		{
			XDamageNotifyEvent* event_damage = (XDamageNotifyEvent*)&xevent;
			
			g_signal_emit (self, signals[DAMAGE_EVENT], 0, event_damage->damage);
		}
		else if (xevent.type == self->priv->fixes.event_base + XFixesCursorNotify)
		{
			XFixesCursorNotifyEvent* event_cursor = (XFixesCursorNotifyEvent*)&xevent;

			ccm_debug("CURSOR NOTIFY %li", event_cursor->cursor_name);
			ccm_display_check_cursor (self, event_cursor->cursor_name, TRUE);
		}
		else
		{
			gboolean proceed = FALSE;
			
			// Check if event is not already proceed by device events
			if (xevent.type == self->priv->type_button_press)
			{
				XDeviceButtonEvent* button_event = 
					(XDeviceButtonEvent*)g_memdup(&xevent, sizeof(XEvent));
				
				proceed = self->priv->last_events.press == xevent.xany.serial;
				self->priv->last_events.press = xevent.xany.serial;
				
				xevent.xany.type = ButtonPress;
				xevent.xany.serial = button_event->serial;
				xevent.xany.send_event = button_event->send_event;
				xevent.xany.display = button_event->display;
				xevent.xany.window = button_event->window;
				xevent.xbutton.root = button_event->root;
				xevent.xbutton.subwindow = button_event->subwindow;
				xevent.xbutton.time = button_event->time;
				xevent.xbutton.x = button_event->x;
				xevent.xbutton.y = button_event->y;
				xevent.xbutton.x_root = button_event->x_root;
				xevent.xbutton.y_root = button_event->y_root;
				xevent.xbutton.state = button_event->state;
				xevent.xbutton.button = button_event->button;
				xevent.xbutton.same_screen = button_event->same_screen;
				
				g_free(button_event);
			}
			else if (xevent.type == self->priv->type_button_release)
			{
				XDeviceButtonEvent* button_event = 
					(XDeviceButtonEvent*)g_memdup(&xevent, sizeof(XEvent));
				
				proceed = self->priv->last_events.release == xevent.xany.serial;
				self->priv->last_events.release = xevent.xany.serial;
				
				xevent.xany.type = ButtonRelease;
				xevent.xany.serial = button_event->serial;
				xevent.xany.send_event = button_event->send_event;
				xevent.xany.display = button_event->display;
				xevent.xany.window = button_event->window;
				xevent.xbutton.root = button_event->root;
				xevent.xbutton.subwindow = button_event->subwindow;
				xevent.xbutton.time = button_event->time;
				xevent.xbutton.x = button_event->x;
				xevent.xbutton.y = button_event->y;
				xevent.xbutton.x_root = button_event->x_root;
				xevent.xbutton.y_root = button_event->y_root;
				xevent.xbutton.state = button_event->state;
				xevent.xbutton.button = button_event->button;
				xevent.xbutton.same_screen = button_event->same_screen;
				
				g_free(button_event);
			}
			else if (xevent.type == self->priv->type_motion_notify)
			{
				XDeviceMotionEvent* motion_event = 
					(XDeviceMotionEvent*)g_memdup(&xevent, sizeof(XEvent));
				
				proceed = self->priv->last_events.motion == xevent.xany.serial;
				self->priv->last_events.motion = xevent.xany.serial;
				
				xevent.xany.type = MotionNotify;
				xevent.xany.serial = motion_event->serial;
				xevent.xany.send_event = motion_event->send_event;
				xevent.xany.display = motion_event->display;
				xevent.xany.window = motion_event->window;
				xevent.xmotion.root = motion_event->root;
				xevent.xmotion.subwindow = motion_event->subwindow;
				xevent.xmotion.time = motion_event->time;
				xevent.xmotion.x = motion_event->x;
				xevent.xmotion.y = motion_event->y;
				xevent.xmotion.x_root = motion_event->x_root;
				xevent.xmotion.y_root = motion_event->y_root;
				xevent.xmotion.state = motion_event->state;
				xevent.xmotion.is_hint = motion_event->is_hint;
				xevent.xmotion.same_screen = motion_event->same_screen;
				
				g_free(motion_event);
			}
			else if (xevent.type == ButtonPress)
			{
				proceed = self->priv->last_events.press == xevent.xany.serial;
				self->priv->last_events.press = xevent.xany.serial;
			}
			else if (xevent.type == ButtonRelease)
			{
				proceed = self->priv->last_events.release == xevent.xany.serial;
				self->priv->last_events.release = xevent.xany.serial;
			}
			else if (xevent.type == MotionNotify)
			{
				proceed = self->priv->last_events.motion == xevent.xany.serial;
				self->priv->last_events.motion = xevent.xany.serial;
			}
			
			if (!proceed)
				g_signal_emit (self, signals[EVENT], 0, &xevent);
		}
	}
}

CCMDisplay*
ccm_display_new(gchar* display)
{
	CCMDisplay *self;
	gint cpt;
	GSList* unmanaged = NULL;
	Display* xdisplay;
	
	xdisplay = XOpenDisplay(display);
	if (!xdisplay)
	{
		g_warning("Unable to open display %s", display);
		return NULL;
	}
	
	self = g_object_new(CCM_TYPE_DISPLAY, 
						"xdisplay", xdisplay,
						NULL);
	
	if (!ccm_display_init_shape(self))
	{
		g_object_unref(self);
		g_warning("Shape init failed for %s", display);
		return NULL;
	}
	
	if (!ccm_display_init_composite(self))
	{
		g_object_unref(self);
		g_warning("Composite init failed for %s", display);
		return NULL;
	}
	
	if (!ccm_display_init_damage(self))
	{
		g_object_unref(self);
		g_warning("Damage init failed for %s", display);
		return NULL;
	}
	
	if (!ccm_display_init_shm(self))
	{
		g_object_unref(self);
		g_warning("SHM init failed for %s", display);
		return NULL;
	}
	
	if (!ccm_display_init_xfixes(self))
	{
		g_object_unref(self);
		g_warning("FIXES init failed for %s", display);
		return NULL;
	}
	
	if (!ccm_display_init_input(self))
	{
		g_object_unref(self);
		g_warning("TEST init failed for %s", display);
		return NULL;
	}
	
	ccm_display_get_pointers(self);
	
	ccm_display_load_config(self);
	
	XSetErrorHandler(ccm_display_error_handler);
	
	self->priv->nb_screens = ScreenCount(self->priv->xdisplay);
	self->priv->screens = g_slice_alloc0(sizeof(CCMScreen*) * (self->priv->nb_screens + 1));
	
	unmanaged = ccm_config_get_integer_list(self->priv->options[CCM_DISPLAY_UNMANAGED_SCREEN], NULL);
	
	for (cpt = 0; cpt < self->priv->nb_screens; ++cpt)
	{
		gboolean found = FALSE;
		
		if (unmanaged)
		{
			GSList* item;
			
			for (item = unmanaged; item; item = item->next)
			{
				if (GPOINTER_TO_INT(item->data) == cpt)
				{
					found = TRUE;
					break;
				}
			}
		}
		if (!found) self->priv->screens[cpt] = ccm_screen_new(self, cpt);
	}
	g_slist_free(unmanaged);
	
	self->priv->fd = ConnectionNumber (CCM_DISPLAY_XDISPLAY(self));
	fd_add_watch (self->priv->fd, self);
    fd_set_read_callback (self->priv->fd, 
						  (CCMWatchCallback)ccm_display_process_events);
    fd_set_poll_callback (self->priv->fd, 
						  (CCMWatchCallback)ccm_display_process_events);
	
	return self;
}

Display*
ccm_display_get_xdisplay(CCMDisplay* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->xdisplay;
}

CCMScreen *
ccm_display_get_screen(CCMDisplay* self, guint number)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(number < self->priv->nb_screens, NULL);
	
	return self->priv->screens[number];
}

int
ccm_display_get_shape_notify_event_type(CCMDisplay* self)
{
	g_return_val_if_fail(self != NULL, 0);
	
	return self->priv->shape.event_base + ShapeNotify;
}

gboolean
ccm_display_report_device_event(CCMDisplay* self, CCMScreen* screen,
								gboolean report)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(screen != NULL, FALSE);
	
	CCMWindow* root = ccm_screen_get_root_window(screen);
	GSList* item;
	
	for (item = self->priv->pointers; item; item = item->next)
	{
		XDevice* pointer = (XDevice*)item->data;
		gint cpt, nb = 0;
		
		if (report)
		{
			XInputClassInfo* class;
			XEventClass* event = g_new0(XEventClass, 9 * pointer->num_classes);
			
			for (class = pointer->classes, cpt = 0; 
				 cpt < pointer->num_classes; class++, ++cpt) 
			{
				switch (class->input_class) 
				{
				    case ButtonClass:
						DeviceButtonPress(pointer, 
										  self->priv->type_button_press, 
										  event[nb++]);
						DeviceButtonRelease(pointer, 
											self->priv->type_button_release, 
											event[nb++]);
						break;
					case ValuatorClass:
						DeviceButton1Motion(pointer, 0, event[nb++]);
						DeviceButton2Motion(pointer, 0, event[nb++]);
						DeviceButton3Motion(pointer, 0, event[nb++]);
						DeviceButton4Motion(pointer, 0, event[nb++]);
						DeviceButton5Motion(pointer, 0, event[nb++]);
						DeviceButtonMotion(pointer, 0, event[nb++]);
						DeviceMotionNotify(pointer, 
										   self->priv->type_motion_notify, 
										   event[nb++]);
						break;
					default:
						break;
				}
			}
			if (XSelectExtensionEvent(self->priv->xdisplay, 
									  CCM_WINDOW_XWINDOW(root), event, nb)) 
			{
				g_free(event);
				return FALSE;
			}
			g_free(event);
		}
		else
		{
			XEventClass* event = g_new0(XEventClass, 1);;
			NoExtensionEvent(pointer, 0, event[nb++]);
			if (XSelectExtensionEvent(self->priv->xdisplay, 
									  CCM_WINDOW_XWINDOW(root), event, nb)) 
			{
				g_free(event);
				return FALSE;
			}
			g_free(event);
		}
	}
	
	return TRUE;
}

void
ccm_display_flush(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);

	XFlush(self->priv->xdisplay);
}
	
void
ccm_display_sync(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);

	XSync(self->priv->xdisplay, FALSE);
}

void
ccm_display_grab(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XGrabServer(self->priv->xdisplay);
}

void
ccm_display_ungrab(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XUngrabServer(self->priv->xdisplay);
}

void
ccm_display_trap_error(CCMDisplay* self)
{
	CCMLastXError = 0;
}

gint
ccm_display_pop_error(CCMDisplay* self)
{
	g_return_val_if_fail(self != NULL, 0);

	ccm_display_sync(self);
	
	return CCMLastXError;
}

const CCMCursor*
ccm_display_get_current_cursor (CCMDisplay* self, gboolean initiate)
{
	g_return_val_if_fail(self != NULL, NULL);

	if (initiate || !self->priv->cursor_current)
	{
		XFixesCursorImage *cursor;

		cursor = XFixesGetCursorImage (CCM_DISPLAY_XDISPLAY(self));
		ccm_display_check_cursor (self, cursor->atom, FALSE);
		XFree(cursor);
	}
	
	return (const CCMCursor*)self->priv->cursor_current;
}
