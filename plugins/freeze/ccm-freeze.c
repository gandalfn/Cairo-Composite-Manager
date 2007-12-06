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
#include <X11/Xlib.h>

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm-freeze.h"
#include "ccm.h"

enum
{
	CCM_FREEZE_DELAY,
	CCM_FREEZE_OPTION_N
};

static gchar* CCMFreezeOptions[CCM_FREEZE_OPTION_N] = {
	"delay"
};

static void ccm_freeze_iface_init(CCMWindowPluginClass* iface);
static Atom protocol_atom = 0;
	
CCM_DEFINE_PLUGIN (CCMFreeze, ccm_freeze, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_freeze,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_freeze_iface_init))

struct _CCMFreezePrivate
{	
	gboolean alive;
	
	CCMWindow* window;
	
	guint id_ping;
	guint32 last_ping;
	
	CCMConfig* options[CCM_FREEZE_OPTION_N];
};

#define CCM_FREEZE_GET_PRIVATE(o)  \
   ((CCMFreezePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FREEZE, CCMFreezeClass))

void ccm_freeze_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
gboolean ccm_freeze_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
						  cairo_t* context, cairo_surface_t* suface);
void ccm_freeze_map(CCMWindowPlugin* plugin, CCMWindow* window);
void ccm_freeze_unmap(CCMWindowPlugin* plugin, CCMWindow* window);

static void
ccm_freeze_init (CCMFreeze *self)
{
	self->priv = CCM_FREEZE_GET_PRIVATE(self);
	self->priv->alive = TRUE;
	self->priv->window = NULL;
	self->priv->id_ping = 0;
	self->priv->last_ping = 0;
}

static void
ccm_freeze_finalize (GObject *object)
{
	CCMFreeze* self = CCM_FREEZE(object);
	
	self->priv->window = NULL;
	if (self->priv->id_ping) g_source_remove (self->priv->id_ping);
	self->priv->id_ping = 0;
	self->priv->last_ping = 0;
	
	G_OBJECT_CLASS (ccm_freeze_parent_class)->finalize (object);
}

static void
ccm_freeze_class_init (CCMFreezeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	klass->protocol_atom = None;
	klass->ping_atom = None;
	
	g_type_class_add_private (klass, sizeof (CCMFreezePrivate));
	
	object_class->finalize = ccm_freeze_finalize;
}

static void
ccm_freeze_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_freeze_load_options;
	iface->query_geometry 	= NULL;
	iface->paint 			= ccm_freeze_paint;
	iface->map				= ccm_freeze_map;
	iface->unmap			= ccm_freeze_unmap;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
}

void
ccm_freeze_on_event(CCMFreeze* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	if (!CCM_IS_FREEZE(self)) return;
	
	if (self->priv->window && event->type == ClientMessage && 
		event->xclient.window == CCM_WINDOW_XWINDOW(self->priv->window))
	{
		if (event->xclient.message_type == protocol_atom)
		{
			g_print("Pong 0x%x, %li\n", event->xclient.window,
					event->xclient.data.l[1]);
			self->priv->alive = TRUE;
			self->priv->last_ping = 0;
		}
	}
}

gboolean
ccm_freeze_ping(CCMFreeze* self)
{
	if (self->priv->window)
	{
		XEvent event;
    	CCMDisplay* display = 
			ccm_drawable_get_display (CCM_DRAWABLE(self->priv->window));
		
		if (!ccm_window_is_viewable (self->priv->window))
			return FALSE;
		
		if (self->priv->last_ping)
		{
			g_print("Not alive 0x%x\n", CCM_WINDOW_XWINDOW(self->priv->window));
			self->priv->alive = FALSE;
			ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
		}
		else
			self->priv->alive = TRUE;
		
		self->priv->last_ping = time(NULL);
		g_print("Ping 0x%x, %li\n", CCM_WINDOW_XWINDOW(self->priv->window),
				self->priv->last_ping);
		ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
    	event.type = ClientMessage;
		event.xclient.window = CCM_WINDOW_XWINDOW(self->priv->window);
    	event.xclient.message_type = CCM_FREEZE_GET_CLASS(self)->protocol_atom;
    	event.xclient.format = 32;
    	event.xclient.data.l[0] = CCM_FREEZE_GET_CLASS(self)->ping_atom;
    	event.xclient.data.l[1] = self->priv->last_ping;
    	event.xclient.data.l[2] = CCM_WINDOW_XWINDOW(self->priv->window);
    	event.xclient.data.l[3] = 0;
    	event.xclient.data.l[4] = 0;
		XSendEvent (CCM_DISPLAY_XDISPLAY(display), 
					CCM_WINDOW_XWINDOW(self->priv->window), False, 
					SubstructureRedirectMask | 
					SubstructureNotifyMask, &event);
	}
	
	
	return TRUE;
}


void
ccm_freeze_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_FREEZE_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "freeze", 
												  CCMFreezeOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->window = window;
	g_signal_connect_swapped(G_OBJECT(display), "event", 
							 G_CALLBACK(ccm_freeze_on_event), self);
	
	if (protocol_atom == None)
		protocol_atom = 
			XInternAtom (CCM_DISPLAY_XDISPLAY(display), "WM_PROTOCOLS", 0);
	if (CCM_FREEZE_GET_CLASS(self)->protocol_atom == None)
		CCM_FREEZE_GET_CLASS(self)->protocol_atom = 
			XInternAtom (CCM_DISPLAY_XDISPLAY(display), "WM_PROTOCOLS", 0);
	if (CCM_FREEZE_GET_CLASS(self)->ping_atom == None)
		CCM_FREEZE_GET_CLASS(self)->ping_atom = 
			XInternAtom (CCM_DISPLAY_XDISPLAY(display), "_NET_WM_PING", 0);
}

gboolean 
ccm_freeze_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
				 cairo_t* context, cairo_surface_t* surface)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	gboolean ret;
	CCMWindowType type;
	type = ccm_window_get_hint_type(window);
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window, 
								   context, surface);
	
	if (type == CCM_WINDOW_TYPE_NORMAL || type == CCM_WINDOW_TYPE_UNKNOWN)
	{	
		if (!self->priv->alive)
		{
			cairo_set_source_rgba(context, 0, 0, 0, 0.5);
			cairo_paint(context);
		}
	}
	
	return ret;
}

void
ccm_freeze_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	
	CCMWindowType type = ccm_window_get_hint_type (window);
	
	if (self->priv->id_ping && (type == CCM_WINDOW_TYPE_NORMAL ||
								type == CCM_WINDOW_TYPE_UNKNOWN))	
	{
		gint delay = 
			ccm_config_get_integer (self->priv->options[CCM_FREEZE_DELAY]);
		
		self->priv->alive = TRUE;
		self->priv->id_ping = g_timeout_add (delay, 
											 (GSourceFunc)ccm_freeze_ping, 
											 self);
	}
	ccm_window_plugin_map(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

void
ccm_freeze_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	
	if (self->priv->id_ping)	
	{
		g_source_remove (self->priv->id_ping);
		self->priv->id_ping = 0;
		self->priv->alive = TRUE;
	}
	ccm_window_plugin_unmap(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}
