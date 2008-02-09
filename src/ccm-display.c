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

#include <X11/Xresource.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdbe.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>

#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-watch.h"
#include "ccm-config.h"

G_DEFINE_TYPE (CCMDisplay, ccm_display, G_TYPE_OBJECT);

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
    XID			  damage;
    CCMDamageFunc callback;
    gpointer 	  data;
} CCMDamage;

typedef struct
{
    AgGetPropertyTask  *task;
    CCMAsyncGetpropFunc callback;
    gpointer 	  		data;
} CCMAsyncGetprop;

struct _CCMDisplayPrivate
{
	gint 		nb_screens;
	CCMScreen 	**screens;
	
	CCMExtension shape;
	CCMExtension composite;
	CCMExtension damage;
	CCMExtension shm;
	gboolean	 shm_shared_pixmap;
	CCMExtension dbe;
	
	GHashTable*	 damages;
	GHashTable*	 asyncprops;
	
	gint		 fd;
	CCMConfig*   options[CCM_DISPLAY_OPTION_N];
};

#define CCM_DISPLAY_GET_PRIVATE(o)  \
   ((CCMDisplayPrivate *)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_DISPLAY, CCMDisplayClass))

static void
ccm_display_init (CCMDisplay *self)
{
	self->priv = CCM_DISPLAY_GET_PRIVATE(self);
	self->priv->nb_screens = 0;
	self->priv->fd = 0;
	self->priv->screens = NULL;
	self->priv->shm_shared_pixmap = FALSE;
	self->priv->damages = g_hash_table_new_full(g_direct_hash, g_direct_equal,
												NULL, g_free);
	self->priv->asyncprops = g_hash_table_new_full(g_direct_hash, g_direct_equal,
												   NULL, g_free);
}

static void
ccm_display_finalize (GObject *object)
{
	CCMDisplay* self = CCM_DISPLAY(object);
	gint cpt;
	
	if (self->priv->nb_screens) 
	{
		for (cpt = 0; cpt < self->priv->nb_screens; cpt++)
		{
			if (self->priv->screens[cpt])
			{
				if (CCM_SCREEN_GET_CLASS(self->priv->screens[cpt])->selection_owner != None)
				{
					XDestroyWindow (CCM_DISPLAY_XDISPLAY(self),
									CCM_SCREEN_GET_CLASS(self->priv->screens[cpt])->selection_owner);
					CCM_SCREEN_GET_CLASS(self->priv->screens[cpt])->selection_owner = None;
				}
				g_object_unref(self->priv->screens[cpt]);
			}
		}
		
		g_slice_free1(sizeof(CCMScreen*) * (self->priv->nb_screens + 1), 
					  self->priv->screens);
	}
	g_hash_table_destroy(self->priv->damages);
	g_hash_table_destroy(self->priv->asyncprops);
	
	for (cpt = 0; cpt < CCM_DISPLAY_OPTION_N; cpt++)
		g_object_unref(self->priv->options[cpt]);
	
	if (self->priv->fd) fd_remove_watch (self->priv->fd);
	
	G_OBJECT_CLASS (ccm_display_parent_class)->finalize (object);
}

static void
ccm_display_class_init (CCMDisplayClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMDisplayPrivate));
	
	signals[EVENT] = g_signal_new ("event",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__POINTER,
								   G_TYPE_NONE, 1, G_TYPE_POINTER);
	
	object_class->finalize = ccm_display_finalize;
}

static void
ccm_display_load_config(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	gint cpt;
	
	for (cpt = 0; cpt < CCM_DISPLAY_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(-1, NULL, 
												  CCMDisplayOptions[cpt]);
	}
}

static gboolean
ccm_display_init_shape(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XShapeQueryExtension (self->xdisplay,
							  &self->priv->shape.event_base,
							  &self->priv->shape.error_base))
    {
		self->priv->shape.available = TRUE;
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_composite(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XCompositeQueryExtension (self->xdisplay,
								  &self->priv->composite.event_base,
	    						  &self->priv->composite.error_base))
    {
		self->priv->composite.available = TRUE;
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_damage(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (XDamageQueryExtension (self->xdisplay,
							   &self->priv->damage.event_base,
							   &self->priv->damage.error_base))
    {
		self->priv->damage.available = TRUE;
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_shm(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	int major,  minor;
	
	if (XShmQueryExtension (self->xdisplay) && 
		XShmQueryVersion(self->xdisplay, &major, &minor, 
						 &self->priv->shm_shared_pixmap))
    {
    	self->priv->shm.available = TRUE;
		return TRUE;
    }
    
    return FALSE;
}

static gboolean
ccm_display_init_dbe(CCMDisplay *self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	int major, minor;

	if (XdbeQueryExtension (self->xdisplay, &major, &minor))
    {
		self->priv->dbe.available = TRUE;
		return TRUE;
    }
    
    return FALSE;
}

static int
ccm_display_error_handler(Display* dpy, XErrorEvent* evt)
{
	return 0;
}

static void
ccm_display_process_events(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XEvent xevent;
	AgGetPropertyTask *task;
	
	while (XEventsQueued(CCM_DISPLAY_XDISPLAY(self), QueuedAfterReading))
	{
		XNextEvent(CCM_DISPLAY_XDISPLAY(self), &xevent);
		if (xevent.type == self->priv->damage.event_base + XDamageNotify)
		{
			XDamageNotifyEvent* event_damage = (XDamageNotifyEvent*)&xevent;
			CCMDamage* damage = g_hash_table_lookup(self->priv->damages,
													(gpointer)event_damage->damage);
		
			if (damage) damage->callback(self, damage->data);
		}
		else
		{
			g_signal_emit (self, signals[EVENT], 0, &xevent);
		}
	}
	
	while ((task = ag_get_next_completed_task (CCM_DISPLAY_XDISPLAY(self))))
	{
		CCMAsyncGetprop* asyncprop = g_hash_table_lookup(self->priv->asyncprops,
														 task);
		if (asyncprop && ag_task_have_reply (task))
		{
			asyncprop->callback(self, task, asyncprop->data);
			g_hash_table_remove (self->priv->asyncprops, task);
		}
	}
}

gboolean
_ccm_display_use_xshm(CCMDisplay* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_DISPLAY_OPTION_USE_XSHM]) &&
		   self->priv->shm.available;
}

gboolean
_ccm_display_xshm_shared_pixmap(CCMDisplay* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_DISPLAY_OPTION_USE_XSHM]) &&
		   self->priv->shm.available && self->priv->shm_shared_pixmap;
}

void
_ccm_display_register_damage(CCMDisplay* self, XID damage, 
							 CCMDamageFunc func, gpointer data)
{
	g_return_if_fail (self != NULL);
    
	CCMDamage* info;
    
    info = g_new (CCMDamage, 1);
    
    info->damage = damage;
    info->callback = func;
    info->data = data;

	g_hash_table_insert (self->priv->damages, (gpointer)damage, info);
}

void
_ccm_display_unregister_damage(CCMDisplay* self, XID damage)
{
	g_return_if_fail (self != NULL);
    
	CCMDamage* info = g_hash_table_lookup(self->priv->damages, (gpointer)damage);
    
	if (info) g_hash_table_remove (self->priv->damages, (gpointer)damage);
}

void 		
_ccm_display_get_property_async (CCMDisplay* self, AgGetPropertyTask* task, 
								 CCMAsyncGetpropFunc func, gpointer data)
{
	g_return_if_fail (self != NULL);
    g_return_if_fail (task != NULL);
	
	CCMAsyncGetprop* info;
    
    info = g_new (CCMAsyncGetprop, 1);
    
    info->task = task;
    info->callback = func;
    info->data = data;

	g_hash_table_insert (self->priv->asyncprops, (gpointer)task, info);
}

CCMDisplay*
ccm_display_new(gchar* display)
{
	CCMDisplay *self;
	gint cpt;
	GSList* unmanaged = NULL;
	
	self = g_object_new(CCM_TYPE_DISPLAY, NULL);
	
	self->xdisplay = XOpenDisplay(display);
	if (!self->xdisplay)
	{
		g_object_unref(self);
		g_warning("Unable to open display %s", display);
		return NULL;
	}
	
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
	
	if (!ccm_display_init_dbe(self))
	{
		g_object_unref(self);
		g_warning("DBE init failed for %s", display);
		return NULL;
	}
	
	ccm_display_load_config(self);
	
	XSetErrorHandler(ccm_display_error_handler);
	
	self->priv->nb_screens = ScreenCount(self->xdisplay);
	self->priv->screens = g_slice_alloc0(sizeof(CCMScreen*) * (self->priv->nb_screens + 1));
	
	unmanaged = ccm_config_get_integer_list(self->priv->options[CCM_DISPLAY_UNMANAGED_SCREEN]);
	
	for (cpt = 0; cpt < self->priv->nb_screens; cpt++)
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

void
ccm_display_sync(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XSync(self->xdisplay, FALSE);
}

void
ccm_display_grab(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XGrabServer(self->xdisplay);
}

void
ccm_display_ungrab(CCMDisplay* self)
{
	g_return_if_fail(self != NULL);
	
	XUngrabServer(self->xdisplay);
}
