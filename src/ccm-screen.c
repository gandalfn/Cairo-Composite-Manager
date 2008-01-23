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
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>

#include "ccm-screen.h"
#include "ccm-screen-plugin.h"
#include "ccm-display.h"
#include "ccm-window.h"
#include "ccm-drawable.h"
#include "ccm-extension-loader.h"
#include "ccm-keybind.h"

enum
{
	TIMER,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static void ccm_screen_iface_init(CCMScreenPluginClass* iface);

G_DEFINE_TYPE_EXTENDED (CCMScreen, ccm_screen, G_TYPE_OBJECT, 0,
						G_IMPLEMENT_INTERFACE(CCM_TYPE_SCREEN_PLUGIN,
											  ccm_screen_iface_init))

enum
{
	CCM_SCREEN_BACKEND,
	CCM_SCREEN_PIXMAP,
	CCM_SCREEN_USE_BUFFERED,
	CCM_SCREEN_PLUGINS,
	CCM_SCREEN_REFRESH_RATE,
	CCM_SCREEN_SYNC_WITH_VBLANK,
	CCM_SCREEN_OPTION_N
};

static gchar* CCMScreenOptions[CCM_SCREEN_OPTION_N] = {
	"backend",
	"native_pixmap_bind",
	"use_buffered_pixmap",
	"plugins",
	"refresh_rate",
	"sync_with_vblank"
};

struct _CCMScreenPrivate
{
	CCMDisplay* 		display;
	
	cairo_t*			ctx;
	
	CCMWindow* 			root;
	CCMWindow* 			cow;
	CCMWindow*          fullscreen;
	
	CCMRegion*			damaged;
	
	GList*				windows;
	
	GTimer*				vblank;
	guint				id_paint;
	
	CCMExtensionLoader* plugin_loader;
	CCMScreenPlugin*	plugin;

	GSList*				animations;
	CCMConfig*			options[CCM_SCREEN_OPTION_N];
};

#define CCM_SCREEN_GET_PRIVATE(o)  \
   ((CCMScreenPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SCREEN, CCMScreenClass))

static gboolean impl_ccm_screen_paint(CCMScreenPlugin* plugin, CCMScreen* self,
									  cairo_t* ctx);
static gboolean impl_ccm_screen_add_window(CCMScreenPlugin* plugin, 
										   CCMScreen* self, CCMWindow* window);
static void impl_ccm_screen_remove_window(CCMScreenPlugin* plugin, 
										  CCMScreen* self, 
										  CCMWindow* window);
static void on_window_damaged(CCMScreen* self, CCMRegion* area, 
							  CCMWindow* window);

static void
ccm_screen_init (CCMScreen *self)
{
	self->priv = CCM_SCREEN_GET_PRIVATE(self);
	
	self->priv->display = NULL;
	self->priv->ctx = NULL;
	self->priv->root = NULL;
	self->priv->cow = NULL;
	self->priv->fullscreen = NULL;
	self->priv->damaged = NULL;
	self->priv->windows = NULL;
	self->priv->vblank = g_timer_new();
	self->priv->id_paint = 0;
	self->priv->plugin_loader = NULL;
	self->priv->animations = NULL;
}

static void
ccm_screen_finalize (GObject *object)
{
	CCMScreen *self = CCM_SCREEN(object);
	gint cpt;
	
	if (self->priv->animations)
		g_slist_free(self->priv->animations);
	
	if (self->priv->plugin_loader)
		g_object_unref(self->priv->plugin_loader);
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; cpt++)
		g_object_unref(self->priv->options[cpt]);
	
	if (self->priv->vblank)
		g_timer_destroy(self->priv->vblank);
	
	if (self->priv->id_paint)
		g_source_remove(self->priv->id_paint);
	
	if (self->priv->ctx)
		cairo_destroy (self->priv->ctx);
	
	if (self->priv->windows) 
	{
		g_list_foreach(self->priv->windows, (GFunc)g_object_unref, NULL);
		g_list_free(self->priv->windows);
	}
	if (self->priv->root) 
	{
		ccm_window_unredirect_subwindows(self->priv->root);
		ccm_display_sync(self->priv->display);
		g_object_unref(self->priv->root);
	}
	if (self->priv->damaged)
		ccm_region_destroy(self->priv->damaged);
	if (self->priv->cow) 
	{
		XCompositeReleaseOverlayWindow(CCM_DISPLAY_XDISPLAY(self->priv->display),
									   CCM_WINDOW_XWINDOW(self->priv->cow));
		ccm_display_sync(self->priv->display);
		g_object_unref(self->priv->cow);
	}
	
	G_OBJECT_CLASS (ccm_screen_parent_class)->finalize (object);
}

static void
ccm_screen_class_init (CCMScreenClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMScreenPrivate));
	
	klass->selection_owner = None;
	
	signals[TIMER] = g_signal_new ("timer",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__VOID,
								   G_TYPE_NONE, 0, G_TYPE_NONE);
	
	object_class->finalize = ccm_screen_finalize;
}

static void
ccm_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= NULL;
	iface->paint 			= impl_ccm_screen_paint;
	iface->add_window 		= impl_ccm_screen_add_window;
	iface->remove_window 	= impl_ccm_screen_remove_window;
}

static void
ccm_screen_load_config(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	gint cpt;
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(self->number, NULL, 
												  CCMScreenOptions[cpt]);
	}
}

static gboolean
ccm_screen_create_overlay_window(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	Window window;
	CCMWindow* root = ccm_screen_get_root_window(self);
	
	window = XCompositeGetOverlayWindow (CCM_DISPLAY_XDISPLAY(self->priv->display), 
										 CCM_WINDOW_XWINDOW(root));
	if (!window) return FALSE;
	
	self->priv->cow = ccm_window_new(self, window);
	ccm_window_make_output_only(self->priv->cow);
	
	return self->priv->cow != NULL;
}

static CCMWindow*
ccm_screen_find_window(CCMScreen* self, Window xwindow)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	GList* item;
	
	for (item = self->priv->windows; item; item = item->next)
	{
		if (CCM_WINDOW_XWINDOW(item->data) == xwindow)
			return CCM_WINDOW(item->data);
	}
	
	return NULL;
}

static CCMWindow*
ccm_screen_find_window_or_child(CCMScreen* self, Window xwindow)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	GList* item;
	
	for (item = self->priv->windows; item; item = item->next)
	{
		if (CCM_WINDOW_XWINDOW(item->data) == xwindow ||
			_ccm_window_get_child (item->data) == xwindow)
			return CCM_WINDOW(item->data);
	}
	
	return NULL;
}

static gboolean
ccm_screen_set_selection_owner(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (CCM_SCREEN_GET_CLASS(self)->selection_owner == None)
	{
		gchar* cm_atom_name = g_strdup_printf("_NET_WM_CM_S%i", self->number);
		Atom cm_atom = XInternAtom(CCM_DISPLAY_XDISPLAY(self->priv->display), 
								   cm_atom_name, 0);
		CCMWindow* root = ccm_screen_get_root_window(self);
		
		g_free(cm_atom_name);
		 
		if (XGetSelectionOwner (CCM_DISPLAY_XDISPLAY(self->priv->display), 
								cm_atom) != None)
		{
			g_critical("\nScreen %d already has a composite manager running, \n"
					   "try to stop it before run cairo-compmgr", 
					   self->number);
			return FALSE;
		}
		
		CCM_SCREEN_GET_CLASS(self)->selection_owner = 
			XCreateSimpleWindow (CCM_DISPLAY_XDISPLAY(self->priv->display), 
								 CCM_WINDOW_XWINDOW(root), 
								 -100, -100, 10, 10, 0, None, None);

		Xutf8SetWMProperties (CCM_DISPLAY_XDISPLAY(self->priv->display), 
							  CCM_SCREEN_GET_CLASS(self)->selection_owner, "cairo-compmgr", 
							  "cairo-compmgr", NULL, 0, 
							  NULL, NULL, NULL);

		XSetSelectionOwner (CCM_DISPLAY_XDISPLAY(self->priv->display), 
							cm_atom, CCM_SCREEN_GET_CLASS(self)->selection_owner, 0);
		
	}
	
	return TRUE;
}

static void
ccm_screen_query_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	CCMWindow* root;
	Window* windows, w, p;
	guint n_windows, cpt;
		
	if (self->priv->windows)
	{
		g_list_foreach(self->priv->windows, (GFunc)g_object_unref, NULL);
		g_list_free(self->priv->windows);
		self->priv->windows = NULL;
	}
		
	root = ccm_screen_get_root_window(self);
	XQueryTree(CCM_DISPLAY_XDISPLAY(self->priv->display), 
			   CCM_WINDOW_XWINDOW(root), &w, &p, 
			   &windows, &n_windows);
	for (cpt = 0; cpt < n_windows; cpt++)
	{
		CCMWindow *window = ccm_window_new(self, windows[cpt]);
		if (window)
		{
			if (!ccm_screen_add_window(self, window))
				g_object_unref(window);
		}
	}
	XFree(windows);
}

void
ccm_screen_restack(CCMScreen* self, CCMWindow* above, CCMWindow* below)
{
	g_return_if_fail(self != NULL);
	
	if (!above && !below)
		return;
	
	if (!above)
	{
		GList* below_link = g_list_find(self->priv->windows, below);
		GList* above_desktop =NULL, *item;
		
		if (below_link) 
			self->priv->windows = g_list_remove_link (self->priv->windows,
													  below_link);
		for (item = g_list_first(self->priv->windows); item; item = item->next)
		{
			if (ccm_window_get_hint_type (item->data) == CCM_WINDOW_TYPE_DESKTOP)
			{
				above_desktop = item->next;
				break;
			}
		}
		if (above_desktop)
			self->priv->windows = g_list_insert_before (self->priv->windows, 
														above_desktop, below);
		else
			self->priv->windows = g_list_prepend (self->priv->windows, below);
		return;
	}
		
	if (!below)
	{
		GList* above_link = g_list_find(self->priv->windows, above);
		if (above_link) 
			self->priv->windows = g_list_remove_link (self->priv->windows,
													  above_link);
		self->priv->windows = g_list_append(self->priv->windows, above);
		return;
	}
	
	GList* below_link;
	GList* above_link;
	
	below_link = g_list_find(self->priv->windows, below);
	above_link = g_list_find(self->priv->windows, above);
	
	if (!above_link || !below_link)
		return;
	
	if (below_link->next == above_link)
		return;

	self->priv->windows = g_list_remove(self->priv->windows, above);
	
	if (below_link->next)
		self->priv->windows = g_list_insert_before(self->priv->windows, 
												   below_link->next, above);
	else
		self->priv->windows = g_list_append(self->priv->windows, above);
	
	ccm_drawable_damage (CCM_DRAWABLE(above));
}

static gboolean
impl_ccm_screen_paint(CCMScreenPlugin* plugin, CCMScreen* self, cairo_t* ctx)
{
	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(ctx != NULL, FALSE);
	
	static gboolean have_desktop = FALSE;
	gboolean ret = FALSE;
	GList* item;
	
	if (!have_desktop)
	{
		static gboolean screen_damaged = FALSE;
	
		cairo_set_source_rgb (self->priv->ctx, 0, 0, 0);
		cairo_paint(self->priv->ctx);
		if (!screen_damaged)
		{
			ccm_screen_damage (self);
			screen_damaged = TRUE;
		}
	}
	for (item = self->priv->windows; item; item = item->next)
	{
		CCMWindow* window = item->data;
		
		if (!have_desktop)
			have_desktop = window->is_viewable &&
						   ccm_window_get_hint_type (window) == CCM_WINDOW_TYPE_DESKTOP;
		
		if (!window->is_input_only)
		{
			if (ccm_window_paint(window, self->priv->ctx))
				ret = TRUE;
		}
	}
	
	return ret;
}

static gboolean
impl_ccm_screen_add_window(CCMScreenPlugin* plugin, CCMScreen* self, 
						   CCMWindow* window)
{
	if (self->priv->root && 
		CCM_WINDOW_XWINDOW(window) == CCM_WINDOW_XWINDOW(self->priv->root))
		return FALSE;
	
	if (self->priv->cow && 
		CCM_WINDOW_XWINDOW(window) == CCM_WINDOW_XWINDOW(self->priv->cow))
		return FALSE;
	
	if (CCM_SCREEN_GET_CLASS(self)->selection_owner && 
		CCM_WINDOW_XWINDOW(window) == CCM_SCREEN_GET_CLASS(self)->selection_owner)
		return FALSE;
	
	if (!window->is_input_only &&
		!ccm_screen_find_window(self, CCM_WINDOW_XWINDOW(window)))
	{
		CCMWindowType type = ccm_window_get_hint_type(window);

		if (type != CCM_WINDOW_TYPE_DESKTOP)
			self->priv->windows = g_list_append(self->priv->windows, window);
		else
			self->priv->windows = g_list_prepend(self->priv->windows, window);
		
		g_signal_connect_swapped(window, "damaged", G_CALLBACK(on_window_damaged), self);
		
		if (window->is_viewable) ccm_window_map(window);
		
		return TRUE;
	}
	
	return FALSE;
}

static void
impl_ccm_screen_remove_window(CCMScreenPlugin* plugin, CCMScreen* self, 
							  CCMWindow* window)
{
	GList* link = g_list_find (self->priv->windows, window);
	
	if (link)
	{
		self->priv->windows = g_list_remove(self->priv->windows, window);
		
		if (self->priv->fullscreen == window)
			self->priv->fullscreen = NULL;
		
		if (!window->is_input_only)
		{
			CCMRegion* geometry = ccm_drawable_get_geometry (CCM_DRAWABLE(window));
			if (geometry) 
				ccm_screen_damage_region (self, geometry);
			else
				ccm_screen_damage (self);
		}
		g_object_unref(window);
	}
}

static gboolean
ccm_screen_paint(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	GSList* item;
	
	if (g_timer_elapsed (self->priv->vblank, NULL) >= 1/60)
	{
		for (item = self->priv->animations; item; item = item->next)
		{	
			if (!_ccm_animation_main (item->data))
				_ccm_screen_remove_animation (self, item->data);
		}
				
		if (self->priv->cow)
		{
			if (!self->priv->ctx)
			{
				self->priv->ctx = 
					ccm_drawable_create_context(CCM_DRAWABLE(self->priv->cow));
				cairo_identity_matrix (self->priv->ctx);
				cairo_rectangle(self->priv->ctx, 0, 0, 
								self->xscreen->width, self->xscreen->height);
				cairo_clip(self->priv->ctx);
			}
			if (ccm_screen_plugin_paint(self->priv->plugin, self, 
										self->priv->ctx))
			{
				if (self->priv->damaged)
				{
					ccm_drawable_flush_region(CCM_DRAWABLE(self->priv->cow), 
											  self->priv->damaged);
					ccm_region_destroy(self->priv->damaged);
					self->priv->damaged = NULL;
				}
				else
					ccm_drawable_flush(CCM_DRAWABLE(self->priv->cow));
			}
		}
		g_timer_start(self->priv->vblank);
	}
	
	return TRUE;
}

static void
on_window_damaged(CCMScreen* self, CCMRegion* area, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);
	g_return_if_fail(window != NULL);
	
	if (!self->priv->cow)
		ccm_screen_create_overlay_window(self);
	
	if (CCM_WINDOW_XWINDOW(self->priv->cow) != CCM_WINDOW_XWINDOW(window))
	{
		GList* item = NULL;
		gboolean top = TRUE;
		CCMRegion* damage_above = NULL, * damage_below = NULL;
		
		damage_above = ccm_region_copy(area);
		damage_below = ccm_region_copy(area);
		
		// Substract opaque region of window to damage region below
		if (window->opaque && window->is_viewable)
		{
			ccm_region_subtract(damage_below, window->opaque);
		}
		
		if (self->priv->fullscreen && self->priv->fullscreen != window &&
			self->priv->fullscreen->is_viewable)
		{
			if (self->priv->fullscreen->opaque)
			{
				// substract opaque region of fullscreen window to damage
				ccm_region_subtract(damage_below, 
									self->priv->fullscreen->opaque);
				ccm_region_subtract(damage_above, 
									self->priv->fullscreen->opaque);
				// Undamage opaque region
				ccm_drawable_undamage_region(CCM_DRAWABLE(window), 
											 self->priv->fullscreen->opaque);
			}
			// If no below damage or window is undamaged
			if (ccm_region_empty (damage_below) ||
				!ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
			{
				ccm_region_destroy (damage_below);
				ccm_region_destroy (damage_above);
				return;
			}
		}
		
		// Substract all obscured area to damage region
		for (item = g_list_last(self->priv->windows); item; item = item->prev)
		{
			if (((CCMWindow*)item->data)->is_viewable && item->data != window)
			{
				if (((CCMWindow*)item->data)->opaque)
				{
					ccm_drawable_undamage_region(CCM_DRAWABLE(window), 
												 ((CCMWindow*)item->data)->opaque);
					// window is totaly obscured don't damage all other windows
					if (!ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
					{
						ccm_region_destroy (damage_below);
						ccm_region_destroy (damage_above);
						return;
					}
					ccm_region_subtract (damage_above, 
										 ((CCMWindow*)item->data)->opaque);
					ccm_region_subtract (damage_below, 
										 ((CCMWindow*)item->data)->opaque);
				}
			}
			else if (item->data == window)
				break;
		}
		
		if (!self->priv->damaged) self->priv->damaged = ccm_region_new();
		ccm_region_union(self->priv->damaged, damage_below);
		ccm_region_union(self->priv->damaged, damage_above);
		
		// If no damage on above skip above windows
		if (ccm_region_empty(damage_above))
		{
			item = g_list_find(self->priv->windows, window);
			item = item ? item->prev : NULL;
			top = FALSE;
		}
		else
			item = g_list_last(self->priv->windows);
		
		// damage now all concurent window
		for (; item; item = item->prev)
		{
			if (((CCMWindow*)item->data)->is_viewable && item->data != window)
			{
				if (top)
				{
					g_signal_handlers_block_by_func(item->data, 
													on_window_damaged, 
													self);
					ccm_drawable_damage_region(item->data, damage_above);
					g_signal_handlers_unblock_by_func(item->data, 
													  on_window_damaged, 
													  self);
				}
				else
				{
					g_signal_handlers_block_by_func(item->data, 
													on_window_damaged, 
													self);
					ccm_drawable_damage_region(CCM_DRAWABLE(item->data), 
											   damage_below);
					g_signal_handlers_unblock_by_func(item->data, 
													  on_window_damaged, 
													  self);
					if (((CCMWindow*)item->data)->opaque) 
						ccm_region_subtract (damage_below, 
											 ((CCMWindow*)item->data)->opaque);
					
					if (ccm_region_empty(damage_below)) break;
				}
			}
			else if (item->data == window)
			{
				top = FALSE;
				if (ccm_region_empty(damage_below)) break;
			}
		}
		
		ccm_region_destroy(damage_above);
		ccm_region_destroy(damage_below);
	}
}

static void
on_event(CCMScreen* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	gint refresh_rate = ccm_config_get_integer(self->priv->options[CCM_SCREEN_REFRESH_RATE]);
	
	switch (event->type)
	{
		case CreateNotify:
		{	
			XCreateWindowEvent* create_event = ((XCreateWindowEvent*)event);
			CCMWindow* window = ccm_screen_find_window(self,
													   create_event->window);
			if (!window) 
			{
				CCMWindow* root = ccm_screen_get_root_window(self);
				if (create_event->parent == CCM_WINDOW_XWINDOW(root))
				{
					window = ccm_window_new(self, create_event->window);
					if (!ccm_screen_add_window(self, window))
						g_object_unref(window);
				}
			}
			break;	
		}
		case DestroyNotify:
		{	
			CCMWindow* window = ccm_screen_find_window(self,
										((XCreateWindowEvent*)event)->window);
			if (window) ccm_screen_remove_window(self, window);
			break;	
		}
		case MapNotify:
		{	
			CCMWindow* window = ccm_screen_find_window(self,
											((XMapEvent*)event)->window);
			if (window)
				ccm_window_map(window);
		}
		break;
		case UnmapNotify:
		{	
			CCMWindow* window = ccm_screen_find_window(self,
											((XUnmapEvent*)event)->window);
			if (window) 
				ccm_window_unmap(window);
		}
		break;
		case ReparentNotify:
		{
			CCMWindow* window = ccm_screen_find_window(self,
											((XReparentEvent*)event)->window);
			CCMWindow* parent = ccm_screen_find_window(self,
											((XReparentEvent*)event)->parent);
			
			if (((XReparentEvent*)event)->parent == CCM_WINDOW_XWINDOW(self->priv->root))
			{
				if (!window)
				{
					window = ccm_window_new(self, ((XReparentEvent*)event)->window);
					if (!ccm_screen_add_window(self, window))
						g_object_unref(window);
				}
			}
			else if (window) ccm_screen_remove_window (self, window);
			
			if (parent) 
				_ccm_window_set_child (parent, ((XReparentEvent*)event)->window);
		}
		break;
		case CirculateNotify:
		{
			XCirculateEvent* circulate_event = (XCirculateEvent*)event;
			
			CCMWindow* window = ccm_screen_find_window(self,
													   circulate_event->window);
			if (window)
			{
				if (circulate_event->place == PlaceOnTop)
					ccm_screen_restack (self, window, NULL);
				else
					ccm_screen_restack (self, NULL, window);
			}
		}
		break;
		case ConfigureNotify:
		{
			XConfigureEvent* configure_event = (XConfigureEvent*)event;
			
			CCMWindow* window = ccm_screen_find_window(self,
													   configure_event->window);
			
			if (window)
			{
				CCMWindow* below = NULL;
				cairo_rectangle_t geometry;
				
				if (configure_event->above != None)
				{
					if (configure_event->above && 
						configure_event->above != CCM_WINDOW_XWINDOW(self->priv->root) &&
						configure_event->above != CCM_WINDOW_XWINDOW(self->priv->cow) &&
						configure_event->above != CCM_SCREEN_GET_CLASS(self)->selection_owner)
					{
						below = ccm_screen_find_window(self, configure_event->above);
						if (below)
							ccm_screen_restack(self, window, below);
					}
				}
				else
					ccm_screen_restack(self, window, NULL);
				
				ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(window), 
												  &geometry);
				
				ccm_drawable_move(CCM_DRAWABLE(window),
								  configure_event->x - configure_event->border_width, 
								  configure_event->y - configure_event->border_width);
					
				ccm_drawable_resize(CCM_DRAWABLE(window),
									configure_event->width + 
										configure_event->border_width * 2,
									configure_event->height + 
										configure_event->border_width * 2);
			}
		}
		break;
		case PropertyNotify:
		{
			XPropertyEvent* property_event = (XPropertyEvent*)event;
			CCMWindow* window;
			
			if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->opacity_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) ccm_window_query_opacity(window);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->type_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) ccm_window_query_hint_type(window);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->state_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) 
				{
					ccm_window_query_hint_type(window);
					ccm_window_query_state(window);
					if (ccm_window_is_fullscreen (window))
					{
						self->priv->fullscreen = window;
						ccm_screen_restack (self, window, NULL);
					}
					else if (self->priv->fullscreen == window)
						self->priv->fullscreen = NULL;
				}
			}
		}
		break;
		case Expose:
		{
			XExposeEvent* expose_event = (XExposeEvent*)event;
			CCMWindow* window = ccm_screen_find_window(self,
													   expose_event->window);
			if (window || 
				expose_event->window == CCM_WINDOW_XWINDOW(self->priv->root))
			{
				cairo_rectangle_t area;
				CCMRegion* damaged;
				
				area.x = expose_event->x;
 				area.y = expose_event->y;
 				area.width = expose_event->width;
 				area.height = expose_event->height;
				damaged = ccm_region_rectangle (&area);
				ccm_screen_damage_region (self, damaged);
				ccm_region_destroy (damaged);
			}
		}
		break;
		case ClientMessage:
		{
			XClientMessageEvent* client_event = (XClientMessageEvent*)event;
			
			if (client_event->message_type == 
							CCM_WINDOW_GET_CLASS(self->priv->root)->state_atom)
			{
				CCMWindow* window = ccm_screen_find_window_or_child (self,
													   client_event->window);
				if (window)
				{
					gint cpt;
					
					for (cpt = 1; cpt < 3; cpt++)
					{
						switch (client_event->data.l[0])
						{
							case 0:
								ccm_window_unset_state(window, 
													 client_event->data.l[cpt]);
								break;
							case 1:
								ccm_window_set_state(window, 
													 client_event->data.l[cpt]);
								break;
							case 2:
								ccm_window_switch_state(window, 
													 client_event->data.l[cpt]);
								break;
							default:
								break;
						}
					}
					if (ccm_window_is_fullscreen (window))
					{
						ccm_screen_restack (self, window, NULL);
						self->priv->fullscreen = window;
					}
					else if (self->priv->fullscreen == window)
						self->priv->fullscreen = NULL;
				}
			}
			else if (client_event->message_type == 
							CCM_WINDOW_GET_CLASS(self->priv->root)->type_atom)
			{
				CCMWindow* window = ccm_screen_find_window_or_child (self,
													   client_event->window);
				if (window) ccm_window_query_hint_type(window);
			}
		}
		break;
		default:
		break;
	}
	
	if (!refresh_rate) refresh_rate = 60;
	if (g_timer_elapsed(self->priv->vblank, NULL) >= 1.0f / (gfloat)refresh_rate)
		ccm_screen_paint(self);
}

gboolean
_ccm_screen_sync_with_blank(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_SCREEN_SYNC_WITH_VBLANK]);
}

gchar*
_ccm_screen_get_window_backend(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	static gchar* backend = NULL;
	
	if (backend) g_free(backend);
	backend = ccm_config_get_string(self->priv->options[CCM_SCREEN_BACKEND]);
	
	return backend;
}

gboolean
_ccm_screen_native_pixmap_bind(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_SCREEN_PIXMAP]);
}

gboolean
_ccm_screen_use_buffered(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_SCREEN_USE_BUFFERED]);
}

CCMScreenPlugin*
_ccm_screen_get_plugin(CCMScreen *self, GType type)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMScreenPlugin* plugin;
 	
	for (plugin = self->priv->plugin; plugin != (CCMScreenPlugin*)self; 
		 plugin = CCM_SCREEN_PLUGIN_PARENT(plugin))
	{
		if (g_type_is_a(G_OBJECT_TYPE(plugin), type))
			return plugin;
	}
	
	return NULL;
}

GSList*
_ccm_screen_get_window_plugins(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	GSList* filter, *plugins = NULL;
	
	filter = ccm_config_get_string_list(self->priv->options[CCM_SCREEN_PLUGINS]);
	plugins = ccm_extension_loader_get_window_plugins(self->priv->plugin_loader,
													  filter);
	g_slist_foreach(filter, (GFunc)g_free, NULL);
	g_slist_free(filter);
	
	return plugins;
}

void
_ccm_screen_add_animation(CCMScreen* self, CCMAnimation* animation)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(self != NULL);
	
	self->priv->animations = g_slist_append(self->priv->animations, animation);
}

void
_ccm_screen_remove_animation(CCMScreen* self, CCMAnimation* animation)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(self != NULL);
	
	self->priv->animations = g_slist_remove(self->priv->animations, animation);
}

CCMScreen*
ccm_screen_new(CCMDisplay* display, guint number)
{
	g_return_val_if_fail(display != NULL, NULL);
	
	CCMScreen *self = g_object_new(CCM_TYPE_SCREEN, NULL);
	GSList* filter = NULL, *plugins = NULL, *item;
	int refresh_rate;
	
	self->priv->display = display;
	
	self->xscreen = ScreenOfDisplay(CCM_DISPLAY_XDISPLAY(display), number);
	self->number = number;
	
	ccm_screen_load_config(self);
	
	self->priv->plugin_loader = ccm_extension_loader_new();
	
	/* Load plugins */
	self->priv->plugin = (CCMScreenPlugin*)self;
	filter = ccm_config_get_string_list(self->priv->options[CCM_SCREEN_PLUGINS]);
	plugins = ccm_extension_loader_get_screen_plugins(self->priv->plugin_loader,
													  filter);
	g_slist_foreach(filter, (GFunc)g_free, NULL);
	g_slist_free(filter);
	for (item = plugins; item; item = item->next)
	{
		GType type = GPOINTER_TO_INT(item->data);
		GObject* prev = G_OBJECT(self->priv->plugin);
		
		self->priv->plugin = g_object_new(type, "parent", prev, NULL);
	}
	g_slist_free(plugins);
	ccm_screen_plugin_load_options(self->priv->plugin, self);
	
	g_signal_connect_swapped(self->priv->display, "event", 
							 G_CALLBACK(on_event), self);
	
	if (!ccm_screen_create_overlay_window(self))
	{
		g_warning("Error on create overlay window");
		return NULL;
	}
	
	if (!ccm_screen_set_selection_owner(self))
	{
		return NULL;
	}
	
	ccm_window_redirect_subwindows(ccm_screen_get_root_window(self));
	ccm_screen_query_stack(self);
	
	refresh_rate = ccm_config_get_integer(self->priv->options[CCM_SCREEN_REFRESH_RATE]);
	if (!refresh_rate) refresh_rate = 60;
	
	self->priv->id_paint = g_timeout_add_full(G_PRIORITY_HIGH, 
											  1000/refresh_rate, 
											  (GSourceFunc)ccm_screen_paint, 
											  self, NULL);
	g_timer_start(self->priv->vblank);
	
	return self;
}

CCMDisplay*
ccm_screen_get_display(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->display;
}

CCMWindow*
ccm_screen_get_overlay_window(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->cow;
}

CCMWindow*
ccm_screen_get_root_window(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (!self->priv->root)
	{
		Window root = RootWindowOfScreen(self->xscreen);
		
		self->priv->root = ccm_window_new(self, root);
		XSelectInput (CCM_DISPLAY_XDISPLAY(ccm_screen_get_display(self)), 
				      root,
				      SubstructureNotifyMask   |
					  ExposureMask);
	}
	
	return self->priv->root;
}

gboolean
ccm_screen_add_window(CCMScreen* self, CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);

	return ccm_screen_plugin_add_window(self->priv->plugin, self, window);
}

void
ccm_screen_remove_window(CCMScreen* self, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	ccm_screen_plugin_remove_window(self->priv->plugin, self, window);
}

void
ccm_screen_damage(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	CCMRegion* screen_geometry;
	
	cairo_rectangle_t area;
	
	area.x = 0.0f;
	area.y = 0.0f;
	area.width = CCM_SCREEN_XSCREEN(self)->width;
	area.height = CCM_SCREEN_XSCREEN(self)->height;
		
	screen_geometry = ccm_region_rectangle (&area);	
	ccm_screen_damage_region (self, screen_geometry);
	ccm_region_destroy (screen_geometry);
}

void
ccm_screen_damage_region(CCMScreen* self, CCMRegion* area)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);
	
	GList* item;
	
	for (item = g_list_last(self->priv->windows); item; item = item->prev)
	{
		if (!((CCMWindow*)item->data)->is_input_only &&
			((CCMWindow*)item->data)->is_viewable)
		{
			ccm_drawable_damage_region (item->data, area);
			break;
		}
	}
}
