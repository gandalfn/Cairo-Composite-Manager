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
#include <gdk/gdk.h>

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-screen.h"
#include "ccm-screen-plugin.h"
#include "ccm-display.h"
#include "ccm-window.h"
#include "ccm-drawable.h"
#include "ccm-extension-loader.h"
#include "ccm-keybind.h"
#include "ccm-timeline.h"

enum
{
	PLUGINS_CHANGED,
	REFRESH_RATE_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static void ccm_screen_paint(CCMScreen* self, int num_frame, 
							 CCMTimeline* timeline);
static void ccm_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_screen_unset_selection_owner(CCMScreen* self);
static void ccm_screen_on_window_error(CCMScreen* self, CCMWindow* window);
static void ccm_screen_on_window_property_changed(CCMScreen* self, 
												  CCMPropertyType changed,
												  CCMWindow* window);

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
	CCM_SCREEN_INDIRECT,
	CCM_SCREEN_OPTION_N
};

static gchar* CCMScreenOptions[CCM_SCREEN_OPTION_N] = {
	"backend",
	"native_pixmap_bind",
	"use_buffered_pixmap",
	"plugins",
	"refresh_rate",
	"sync_with_vblank",
	"indirect"
};

struct _CCMScreenPrivate
{
	CCMDisplay* 		display;
	
	cairo_t*			ctx;
	
	CCMWindow* 			root;
	CCMWindow* 			cow;
	Window				selection_owner;
	CCMWindow*          fullscreen;
	
	CCMRegion*			damaged;
	
	Window*				stack;
	guint				n_windows;
	GList*				windows;
	GList*				removed;
	gboolean			buffered;
	gboolean			filtered_damage;
	
	guint 				refresh_rate;
	CCMTimeline*		paint;
	guint				id_pendings;
	
	CCMExtensionLoader* plugin_loader;
	CCMScreenPlugin*	plugin;

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
static void ccm_screen_on_window_damaged(CCMScreen* self, CCMRegion* area, 
										 CCMWindow* window);
static void ccm_screen_on_option_changed(CCMScreen* self, CCMConfig* config);


static void
ccm_screen_init (CCMScreen *self)
{
	self->priv = CCM_SCREEN_GET_PRIVATE(self);
	
	self->priv->display = NULL;
	self->priv->ctx = NULL;
	self->priv->root = NULL;
	self->priv->cow = NULL;
	self->priv->selection_owner = None;
	self->priv->fullscreen = NULL;
	self->priv->damaged = NULL;
	self->priv->stack = NULL;
	self->priv->n_windows = 0;
	self->priv->windows = NULL;
	self->priv->removed = NULL;
	self->priv->buffered = FALSE;
	self->priv->filtered_damage = TRUE;
	self->priv->refresh_rate = 0;
	self->priv->paint = NULL;
	self->priv->id_pendings = 0;
	self->priv->plugin_loader = NULL;
	self->priv->plugin = NULL;
}

static void
ccm_screen_finalize (GObject *object)
{
	CCMScreen *self = CCM_SCREEN(object);
	gint cpt;
	
	ccm_debug("FINALIZE SCREEN");
	
	ccm_screen_unset_selection_owner(self);
	
	if (self->priv->plugin)
		g_object_unref(self->priv->plugin);
	
	if (self->priv->plugin_loader)
		g_object_unref(self->priv->plugin_loader);
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; cpt++)
		g_object_unref(self->priv->options[cpt]);
	
	if (self->priv->id_pendings)
		g_source_remove(self->priv->id_pendings);
	
	if (self->priv->paint)
	{
		ccm_timeline_stop(self->priv->paint);
		g_object_unref(self->priv->paint);
	}
	
	if (self->priv->ctx)
		cairo_destroy (self->priv->ctx);
	
	if (self->priv->stack)
	{
		g_free(self->priv->stack);
		self->priv->stack = NULL;
		self->priv->n_windows = 0;
	}
	if (self->priv->windows) 
	{
		g_list_foreach(self->priv->windows, (GFunc)g_object_unref, NULL);
		g_list_free(self->priv->windows);
	}
	if (self->priv->removed) 
	{
		g_list_foreach(self->priv->removed, (GFunc)g_object_unref, NULL);
		g_list_free(self->priv->removed);
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
	
	signals[PLUGINS_CHANGED] = g_signal_new ("plugins-changed",
											 G_OBJECT_CLASS_TYPE (object_class),
											 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
											 g_cclosure_marshal_VOID__VOID,
											 G_TYPE_NONE, 0, G_TYPE_NONE);
	
	signals[REFRESH_RATE_CHANGED] = g_signal_new ("refresh-rate-changed",
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

static gboolean
ccm_screen_update_refresh_rate(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	guint refresh_rate =
		ccm_config_get_integer (self->priv->options[CCM_SCREEN_REFRESH_RATE]);
	
	refresh_rate = MAX(20, refresh_rate);
	refresh_rate = MIN(100, refresh_rate);
	
	if (self->priv->refresh_rate != refresh_rate)
	{
		self->priv->refresh_rate = refresh_rate;
		ccm_config_set_integer (self->priv->options[CCM_SCREEN_REFRESH_RATE], 
								refresh_rate);
		if (self->priv->paint) 
		{
			ccm_timeline_stop(self->priv->paint);
			g_object_unref(self->priv->paint);
		}
		
		self->priv->paint = ccm_timeline_new(refresh_rate, refresh_rate);
		
		g_signal_connect_swapped(self->priv->paint, "new-frame",
								 G_CALLBACK(ccm_screen_paint), self);
		ccm_timeline_set_loop (self->priv->paint, TRUE);
		ccm_timeline_start (self->priv->paint);
		g_signal_emit(self, signals[REFRESH_RATE_CHANGED], 0);
		
		return TRUE;
	}
	
	return FALSE;
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
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_screen_on_option_changed), 
								 self);
	}
	self->priv->buffered = _ccm_screen_use_buffered(self);
	ccm_screen_update_refresh_rate (self);
}

static gboolean
ccm_screen_valid_window(CCMScreen* self, CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);
	
	cairo_rectangle_t geometry;
	
	if (self->priv->root && 
		CCM_WINDOW_XWINDOW(window) == CCM_WINDOW_XWINDOW(self->priv->root))
		return FALSE;
	
	if (self->priv->cow && 
		CCM_WINDOW_XWINDOW(window) == CCM_WINDOW_XWINDOW(self->priv->cow))
		return FALSE;
	
	if (self->priv->selection_owner && 
		CCM_WINDOW_XWINDOW(window) == self->priv->selection_owner)
		return FALSE;
	
	if (ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(window), &geometry) &&
		geometry.x == -100 && geometry.y == -100 && 
		geometry.width == 1 && geometry.height == 1)
		return FALSE;
	
	return TRUE;
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

static void
ccm_screen_destroy_window (CCMScreen* self, CCMWindow* window)
{
	ccm_debug_window(window, "DESTROY WINDOW");
	
	self->priv->windows = g_list_remove(self->priv->windows, window);
	
	g_signal_handlers_disconnect_by_func(window, 
										 ccm_screen_on_window_damaged, 
										 self);
	g_signal_handlers_disconnect_by_func(window, 
										 ccm_screen_on_window_error, 
										 self);
	g_signal_handlers_disconnect_by_func(window, 
										 ccm_screen_on_window_property_changed, 
										 self);
	if (self->priv->fullscreen == window)
	{
		self->priv->fullscreen = NULL;
		ccm_screen_damage (self);
	}		
	else if (!window->is_input_only)
	{
		CCMRegion* geometry = ccm_drawable_get_geometry (CCM_DRAWABLE(window));
		if (geometry && !ccm_region_empty (geometry))
			ccm_screen_damage_region(self, geometry);
		else
			ccm_screen_damage (self);
	}
	g_object_unref(window);
}

#if 0
static void
ccm_screen_print_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	GList* item;
	
	ccm_debug("STACK: XID\tVisible\tType\tManaged\tDecored\tFullscreen\tKA\tKB\tTransient\tGroup\tName\n");
	for (item = self->priv->windows; item; item = item->next)
	{
		CCMWindow* transient = ccm_window_transient_for (item->data);
		CCMWindow* leader = ccm_window_get_group_leader (item->data);
		ccm_debug("STACK: 0x%lx\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t0x%lx\t0x%lx\t%s\n", 
				CCM_WINDOW_XWINDOW(item->data), 
				CCM_WINDOW(item->data)->is_viewable,
				ccm_window_get_hint_type (item->data),
				ccm_window_is_managed (item->data),
				ccm_window_is_decorated (item->data),
				ccm_window_is_fullscreen (item->data),
				ccm_window_keep_above (item->data),
				ccm_window_keep_below (item->data),
				transient ? CCM_WINDOW_XWINDOW(transient) : 0,
				leader ? CCM_WINDOW_XWINDOW(leader) : 0,
				ccm_window_get_name (item->data));
	}
}

static void
ccm_screen_print_real_stack(CCMScreen* self)
{
	CCMWindow* root;
	Window* windows, w, p;
	guint n_windows, cpt;

	root = ccm_screen_get_root_window(self);
	XQueryTree(CCM_DISPLAY_XDISPLAY(self->priv->display), 
			   CCM_WINDOW_XWINDOW(root), &w, &p, 
			   &windows, &n_windows);
	g_print("XID\tVisible\tType\tManaged\tDecorated\tFullscreen\tKA\tKB\tTransient\tGroup\tName\n");
	for (cpt = 0; cpt < n_windows; cpt++)
	{
		CCMWindow *window = ccm_screen_find_window_or_child (self, windows[cpt]);
		if (window)
		{
			CCMWindow* transient = ccm_window_transient_for (window);
			CCMWindow* leader = ccm_window_get_group_leader (window);
			g_print("0x%lx\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t0x%lx\t0x%lx\t%s\n", 
				CCM_WINDOW_XWINDOW(window), 
				CCM_WINDOW(window)->is_viewable,
				ccm_window_get_hint_type (window),
				ccm_window_is_managed (window),
				ccm_window_is_decorated (window),
				ccm_window_is_fullscreen (window),
				ccm_window_keep_above (window),
				ccm_window_keep_below (window),
				transient ? CCM_WINDOW_XWINDOW(transient) : 0,
				leader ? CCM_WINDOW_XWINDOW(leader) : 0,
				ccm_window_get_name (window));
		}
	}
	XFree(windows);
}

#endif 

CCMWindow*
ccm_screen_find_window(CCMScreen* self, Window xwindow)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	GList* item;
	
	for (item = g_list_first(self->priv->windows); item; item = item->next)
	{
		if (CCM_WINDOW_XWINDOW(item->data) == xwindow)
			return CCM_WINDOW(item->data);
	}
	
	return NULL;
}

CCMWindow*
ccm_screen_find_window_or_child(CCMScreen* self, Window xwindow)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	GList* item;
	CCMWindow* child = NULL;
	
	for (item = g_list_last(self->priv->windows); item; item = item->prev)
	{
		if (CCM_WINDOW_XWINDOW(item->data) == xwindow)
			return CCM_WINDOW(item->data);
		else if (_ccm_window_get_child (item->data) == xwindow)
			child = CCM_WINDOW(item->data);
	}
	if (child) ccm_debug_window(child, "PARENT OF 0x%lx", xwindow);
	
	return child;
}

static void
ccm_screen_unset_selection_owner(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->selection_owner != None)
	{
		gchar* cm_atom_name = g_strdup_printf("_NET_WM_CM_S%i", self->number);
		Atom cm_atom = XInternAtom(CCM_DISPLAY_XDISPLAY(self->priv->display), 
								   cm_atom_name, 0);
		g_free(cm_atom_name);
		 
		XDestroyWindow (CCM_DISPLAY_XDISPLAY(self->priv->display),
						self->priv->selection_owner);
			
		XSetSelectionOwner (CCM_DISPLAY_XDISPLAY(self->priv->display), 
							cm_atom, None, 0);
	}
}

static gboolean
ccm_screen_set_selection_owner(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (self->priv->selection_owner == None)
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
		
		self->priv->selection_owner = 
			XCreateSimpleWindow (CCM_DISPLAY_XDISPLAY(self->priv->display), 
								 CCM_WINDOW_XWINDOW(root), 
								 -100, -100, 10, 10, 0, None, None);

		Xutf8SetWMProperties (CCM_DISPLAY_XDISPLAY(self->priv->display), 
							  self->priv->selection_owner, "cairo-compmgr", 
							  "cairo-compmgr", NULL, 0, 
							  NULL, NULL, NULL);

		XSetSelectionOwner (CCM_DISPLAY_XDISPLAY(self->priv->display), 
							cm_atom, self->priv->selection_owner, 0);
		
	}
	
	return TRUE;
}

static void
ccm_screen_update_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	Window* stack = NULL;
	Window w, p;
	guint n_windows = 0;
	CCMWindow* root = ccm_screen_get_root_window(self);
	
	if (XQueryTree(CCM_DISPLAY_XDISPLAY(self->priv->display), 
			   CCM_WINDOW_XWINDOW(root), &w, &p, 
			   &stack, &n_windows) && stack && n_windows)
	{
		if (self->priv->stack) g_free(self->priv->stack);
		
		self->priv->n_windows = n_windows;
		self->priv->stack = g_memdup(stack, sizeof(Window) * n_windows);
		XFree(stack);
	}
}

static void
ccm_screen_query_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	guint cpt;
	
	ccm_debug("QUERY_STACK");
	
	if (self->priv->windows)
	{
		g_list_foreach(self->priv->windows, (GFunc)g_object_unref, NULL);
		g_list_free(self->priv->windows);
		self->priv->windows = NULL;
	}
		
	ccm_screen_update_stack (self);
	for (cpt = 0; cpt < self->priv->n_windows; cpt++)
	{
		CCMWindow *window = ccm_window_new(self, self->priv->stack[cpt]);
		if (window)
		{
			if (!ccm_screen_add_window(self, window))
				g_object_unref(window);
		}
	}
}

static void
ccm_screen_check_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);

	guint cpt;
	GList* stack = NULL, *item, *last = NULL;
	GList* removed = NULL;
	
	ccm_debug("CHECK_STACK");
	
	ccm_screen_update_stack (self);
	
	for (cpt = 0; cpt < self->priv->n_windows; cpt++)
	{
		CCMWindow* window = ccm_screen_find_window (self, self->priv->stack[cpt]);
		
		if (window && !window->is_input_only &&
			!g_list_find(stack, window))
		{
			stack = g_list_append(stack, window);
			if (window->is_viewable || window->unmap_pending)
			{
				ccm_debug_window(window, "STACK IS VIEWABLE");
			}
		}
		else if (!window)
		{
			window = ccm_window_new (self, self->priv->stack[cpt]);
			if (window && !window->is_input_only &&
				ccm_screen_valid_window(self, window))
			{
				ccm_debug_window(window, "CHECK STACK NEW WINDOW");
				if (window->is_viewable)
				{
					ccm_debug_window(window, "CHECK STACK NEW WINDOW MAP");
					ccm_window_map(window);
				}
				g_signal_connect_swapped(window, "damaged", 
					 G_CALLBACK(ccm_screen_on_window_damaged), self);
				g_signal_connect_swapped(window, "error", 
					 G_CALLBACK(ccm_screen_on_window_error), self);
				g_signal_connect_swapped(window, "property-changed", 
					 G_CALLBACK(ccm_screen_on_window_property_changed), self);
				stack = g_list_append(stack, window);
			}
			else
				g_object_unref(window);
		}
	}
	
	for (item = g_list_first(self->priv->windows); item; item = item->next)
	{
		GList* link = g_list_find(stack, item->data);
		
		if (link && (CCM_WINDOW(item->data)->is_viewable || 
			CCM_WINDOW(item->data)->unmap_pending))
		{
			ccm_debug_window(item->data, "CHECK STACK LAST");
			last = link;
		}
		else if (!link) 
		{
			ccm_debug_window(item->data, "STACK NOT FOUND");
			
			if (!CCM_WINDOW(item->data)->is_viewable &&
				 CCM_WINDOW(item->data)->unmap_pending &&
				!CCM_WINDOW(item->data)->is_input_only)
			{
				ccm_debug_window(item->data, "CHECK STACK UNMAP PENDING");
				
				if (last)
				{
					ccm_debug_window(item->data, "STACK ADD UNMAP AFTER 0x%x", 
									 CCM_WINDOW_XWINDOW(last->data));
					stack = g_list_insert_before (stack,
											      last->next,
												  item->data);
				}
				else
					stack = g_list_append(stack, item->data);
			}
			else
			{
				ccm_debug_window(item->data, "CHECK STACK REMOVED");
				removed = g_list_append (removed, item->data);
			}
		}
	}
	for (item = g_list_first(removed); item; item = item->next)
		ccm_screen_remove_window (self, item->data);
	g_list_free(removed);
	
	g_list_free(self->priv->windows);
	self->priv->windows = stack;
	
#if 0
	g_print("Stack\n");
	ccm_screen_print_stack (self);
	g_print("Real stack\n");
	ccm_screen_print_real_stack (self);
#endif
}

static void
ccm_screen_check_stack_position(CCMScreen* self, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	CCMWindow* below = NULL;
	GList* below_link;
	guint cpt;
	
	for (cpt = 0; cpt < self->priv->n_windows; cpt++)
	{
		if (self->priv->stack[cpt] == CCM_WINDOW_XWINDOW(window)) 
		{
			if (cpt > 0)
				below = ccm_screen_find_window_or_child (self, self->priv->stack[cpt - 1]);
			break;
		}
	}
	
	below_link = g_list_find(self->priv->windows, below);
	if (below_link && below_link->next && below_link->next->data == window) 
		return;
	
	ccm_debug_window(window, "CHECK_STACK_POSITION");
	
	self->priv->windows = g_list_remove(self->priv->windows, window);
	if (below_link)
	{
		self->priv->windows = g_list_insert_before (self->priv->windows, 
													below_link->next, window);
		if (CCM_WINDOW(below)->is_viewable || CCM_WINDOW(below)->unmap_pending)
			ccm_drawable_damage (CCM_DRAWABLE(below));
	}
	else if (cpt > 0)
		self->priv->windows = g_list_append (self->priv->windows, window);
	else
		self->priv->windows = g_list_prepend (self->priv->windows, window);
	
	if (CCM_WINDOW(window)->is_viewable || CCM_WINDOW(window)->unmap_pending)
		ccm_drawable_damage (CCM_DRAWABLE(window));
	
#if 0
	g_print("Stack\n");
	ccm_screen_print_stack (self);
	g_print("Real stack\n");
	ccm_screen_print_real_stack (self);
#endif
}

static void
ccm_screen_restack(CCMScreen* self, CCMWindow* window, CCMWindow* sibling)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(sibling != NULL);
	
	GList* sibling_link = g_list_find(self->priv->windows, sibling);
	
	guint pos_window = g_list_index (self->priv->windows, window);
	guint pos_sibling = g_list_index (self->priv->windows, sibling);
	
	if (pos_window > pos_sibling) return;
	
	if (ccm_window_get_hint_type (window) == CCM_WINDOW_TYPE_DESKTOP)
		return;
	
	if (window->opaque)
	{
		cairo_rectangle_t geometry;
		
		ccm_region_get_clipbox (window->opaque, &geometry);
		if (geometry.x == 0 && geometry.y == 0 && 
			geometry.width == CCM_SCREEN_XSCREEN(self)->width &&
			geometry.height == CCM_SCREEN_XSCREEN(self)->height)
		{
			GList* item;
			gboolean found= FALSE;
			
			for (item = g_list_find(self->priv->windows, window); 
				 item; 
				 item = item->prev)
			{
				if (item->data != window && 
					CCM_WINDOW(item->data)->is_viewable)
				{
					found = TRUE;
					break;
				}
			}
			
			if (!found) return;
		}
	}
	
	ccm_debug_window(window, "RESTACK AFTER 0x%x", CCM_WINDOW_XWINDOW(sibling));
	
	self->priv->windows = g_list_remove (self->priv->windows, window);
	if (sibling_link)
		self->priv->windows = g_list_insert_before (self->priv->windows,
													sibling_link->next,
													window);
	else
		self->priv->windows = g_list_append (self->priv->windows, window);
	
	ccm_drawable_damage (CCM_DRAWABLE(window));
}

static gboolean
impl_ccm_screen_paint(CCMScreenPlugin* plugin, CCMScreen* self, cairo_t* ctx)
{
	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(ctx != NULL, FALSE);
	
	gboolean ret = FALSE;
	GList* item, *destroy = NULL;
	
	ccm_debug("PAINT SCREEN BEGIN");
	for (item = self->priv->fullscreen && self->priv->fullscreen->opaque ? 
		 		g_list_find(self->priv->windows, self->priv->fullscreen) : 
			 	g_list_first(self->priv->windows); item; item = item->next)
	{
		CCMWindow* window = CCM_WINDOW(item->data);
		
		if (!window->is_input_only)
		{
			if (ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
			{
				CCMRegion* damaged;
				damaged = _ccm_drawable_get_damaged (CCM_DRAWABLE(window));
				ccm_debug_region(CCM_DRAWABLE(window), "SCREEN DAMAGE");
				if (!self->priv->damaged) 
					self->priv->damaged = ccm_region_copy(damaged);
				else
					ccm_region_union(self->priv->damaged, damaged);
			}
			
			ccm_debug_window(window, "PAINT SCREEN");
			ret |= ccm_window_paint(window, self->priv->ctx, self->priv->buffered);
			
			if (!window->is_viewable && !window->unmap_pending &&
				g_list_find(self->priv->removed, window))
			{
				self->priv->removed = g_list_remove (self->priv->removed, window);
				destroy = g_list_append(destroy, window);
			}
		}
	}
	if (destroy)
	{
		for (item = g_list_first(destroy); item; item = item->next)
		{
			self->priv->windows = g_list_remove (self->priv->windows, item->data);
			ccm_screen_destroy_window (self, item->data);
		}
		g_list_free(destroy);
	}
	
	ccm_debug("PAINT SCREEN END");
	self->priv->buffered = FALSE;
	
	return ret;
}

static gboolean
ccm_screen_on_check_stack_pendings(CCMScreen* self)
{
	ccm_screen_check_stack (self);
	
	self->priv->id_pendings = 0;
	
	return FALSE;
}

static void
ccm_screen_add_check_pending(CCMScreen* self)
{
	if (!self->priv->id_pendings)
		self->priv->id_pendings = g_idle_add_full (G_PRIORITY_LOW,
							(GSourceFunc)ccm_screen_on_check_stack_pendings,
							self, NULL);
}

static void
ccm_screen_on_window_error(CCMScreen* self, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	
	ccm_debug_window(window, "ON WINDOW ERROR");
	
	ccm_screen_add_check_pending (self);
}

static void
ccm_screen_on_window_property_changed(CCMScreen* self, CCMPropertyType changed,
									  CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	ccm_debug_window(window, "PROPERTY_CHANGED");
	
	if (changed == CCM_PROPERTY_STATE)
	{
		if (ccm_window_is_fullscreen (window) && self->priv->fullscreen != window)
		{
			ccm_debug_window(window, "FULLSCREEN");
			self->priv->fullscreen = window;
		}
		else if (self->priv->fullscreen == window)
		{
			ccm_debug_window(window, "UNFULLSCREEN");
			self->priv->fullscreen = NULL;
		}
	}
	else if (changed == CCM_PROPERTY_HINT_TYPE)
	{
		ccm_screen_add_check_pending (self);
	}
	else if (changed == CCM_PROPERTY_TRANSIENT)
	{
		CCMWindow* transient = ccm_window_transient_for (window);
		
		if (transient) ccm_screen_restack (self, window, transient);
	}
}

static gboolean
impl_ccm_screen_add_window(CCMScreenPlugin* plugin, CCMScreen* self, 
						   CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);
	g_return_val_if_fail(plugin != NULL, FALSE);
	
	ccm_debug_window(window, "ADD");

	self->priv->windows = g_list_append (self->priv->windows, window);
	
	g_signal_connect_swapped(window, "damaged", 
							 G_CALLBACK(ccm_screen_on_window_damaged), self);
	g_signal_connect_swapped(window, "error", 
							 G_CALLBACK(ccm_screen_on_window_error), self);
	g_signal_connect_swapped(window, "property-changed", 
							 G_CALLBACK(ccm_screen_on_window_property_changed), self);
	
	if (window->is_viewable) ccm_window_map(window);
	
	return TRUE;
}


static void
impl_ccm_screen_remove_window(CCMScreenPlugin* plugin, CCMScreen* self, 
							  CCMWindow* window)
{
	GList* link = g_list_find (self->priv->windows, window);

	if (link)
	{
		ccm_debug_window(window, "REMOVE");

		if ((!window->is_viewable && !window->unmap_pending) ||
			window->is_input_only)
			ccm_screen_destroy_window(self, window);
		else if (!g_list_find (self->priv->removed, window))
			self->priv->removed = g_list_append (self->priv->removed, window);
	}
}

static void
ccm_screen_get_plugins(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	GSList* filter = NULL, *plugins = NULL, *item;
	
	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin))
		g_object_unref(self->priv->plugin);
	
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
}

static void
ccm_screen_paint(CCMScreen* self, int num_frame, CCMTimeline* timeline)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->cow)
	{
		if (!self->priv->ctx)
		{
			self->priv->ctx = 
				ccm_drawable_create_context(CCM_DRAWABLE(self->priv->cow));
			cairo_rectangle(self->priv->ctx, 0, 0, 
							self->xscreen->width, self->xscreen->height);
			cairo_clip(self->priv->ctx);
		}
		else
			cairo_identity_matrix (self->priv->ctx);
			
		if (ccm_screen_plugin_paint(self->priv->plugin, self, 
									self->priv->ctx))
		{
			if (self->priv->damaged)
			{
				ccm_drawable_flush_region (CCM_DRAWABLE(self->priv->cow),
										   self->priv->damaged);
				ccm_region_destroy(self->priv->damaged);
				self->priv->damaged = NULL;
			}
			else
				ccm_drawable_flush(CCM_DRAWABLE(self->priv->cow));
		}
	}
}

static void
ccm_screen_on_option_changed (CCMScreen* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_SCREEN_PLUGINS])
	{
		ccm_screen_get_plugins (self);
		g_signal_emit (self, signals[PLUGINS_CHANGED], 0);
	}
	else if (config == self->priv->options[CCM_SCREEN_REFRESH_RATE])
	{
		ccm_screen_update_refresh_rate (self);
	}
}

static void
ccm_screen_on_window_damaged(CCMScreen* self, CCMRegion* area, CCMWindow* window)
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
		
		ccm_debug_region(CCM_DRAWABLE(window), "ON_DAMAGE");
		
		// Substract opaque region of window to damage region below
		if (self->priv->filtered_damage && window->opaque && 
			(window->is_viewable || window->unmap_pending))
		{
			ccm_region_subtract(damage_below, window->opaque);
		}
		
		if (self->priv->fullscreen && self->priv->fullscreen != window &&
			self->priv->fullscreen != ccm_window_transient_for (window) &&
			self->priv->fullscreen != ccm_window_get_group_leader (window) &&
			g_list_index(self->priv->windows, window) < 
				 g_list_index(self->priv->windows, self->priv->fullscreen) &&
			(self->priv->fullscreen->is_viewable || 
			 self->priv->fullscreen->unmap_pending))
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
		for (item = g_list_last(self->priv->windows); 
			 self->priv->filtered_damage && item; item = item->prev)
		{
			if (!((CCMWindow*)item->data)->is_input_only &&
				(((CCMWindow*)item->data)->is_viewable ||
				 ((CCMWindow*)item->data)->unmap_pending) && 
				CCM_WINDOW_XWINDOW(item->data) != CCM_WINDOW_XWINDOW(window))
			{
				if (((CCMWindow*)item->data)->opaque)
				{
					ccm_debug_window(window, "UNDAMAGE ABOVE 0x%lx", 
									 CCM_WINDOW_XWINDOW(item->data));
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
			if (!((CCMWindow*)item->data)->is_input_only &&
				(((CCMWindow*)item->data)->is_viewable ||
				 ((CCMWindow*)item->data)->unmap_pending) && 
				item->data != window)
			{
				if (top)
				{
					g_signal_handlers_block_by_func(item->data, 
													ccm_screen_on_window_damaged, 
													self);
					ccm_drawable_damage_region(item->data, damage_above);
					g_signal_handlers_unblock_by_func(item->data, 
													  ccm_screen_on_window_damaged, 
													  self);
				}
				else
				{
					if (self->priv->filtered_damage && 
						(window->is_viewable || window->unmap_pending) &&
						window->opaque && !ccm_region_empty(window->opaque))
					{
						ccm_debug_window(item->data, "UNDAMAGE BELOW");
						ccm_drawable_undamage_region(CCM_DRAWABLE(item->data), 
													 window->opaque);
					}

					g_signal_handlers_block_by_func(item->data, 
													ccm_screen_on_window_damaged, 
													self);
					ccm_drawable_damage_region(CCM_DRAWABLE(item->data), 
											   damage_below);
					g_signal_handlers_unblock_by_func(item->data, 
													  ccm_screen_on_window_damaged, 
													  self);
					if (self->priv->filtered_damage && ((CCMWindow*)item->data)->opaque) 
						ccm_region_subtract (damage_below, 
											 ((CCMWindow*)item->data)->opaque);
				}
			}
			else if (item->data == window)
			{
				top = FALSE;
				if (self->priv->filtered_damage &&
					ccm_region_empty(damage_below) && 
					ccm_region_empty(window->opaque)) break;
			}
		}
		
		ccm_region_destroy(damage_above);
		ccm_region_destroy(damage_below);
	}
}

static void
ccm_screen_on_event(CCMScreen* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	switch (event->type)
	{
		case CreateNotify:
		{	
			XCreateWindowEvent* create_event = ((XCreateWindowEvent*)event);
			CCMWindow* window = ccm_screen_find_window(self,
													   create_event->window);
			ccm_debug("CREATE 0x%lx", create_event->window);
						
			if (!window) 
			{
				CCMWindow* root = ccm_screen_get_root_window(self);
				if (create_event->parent == CCM_WINDOW_XWINDOW(root))
				{
					window = ccm_window_new(self, create_event->window);
					if (window)
					{
						ccm_debug_window(window, "CREATE");
						if (!ccm_screen_add_window(self, window))
							g_object_unref(window);
					}
				}
			}
		}
		break;	
		case DestroyNotify:
		{	
			CCMWindow* window = ccm_screen_find_window(self,
										((XCreateWindowEvent*)event)->window);
			if (window) 
			{
				ccm_debug_window(window, "EVENT REMOVE");
				ccm_screen_remove_window(self, window);
			}
		}
		break;
		case MapNotify:
		{	
			CCMWindow* window, *parent;
			
			parent = ccm_screen_find_window(self,
											((XMapEvent*)event)->event);
			window = ccm_screen_find_window (self,
											((XMapEvent*)event)->window);
			if (window) 
			{
				ccm_debug_window(window, "MAP");
				ccm_window_map(window);
			}
			else if (parent == self->priv->root)
			{
				window = ccm_window_new(self, ((XMapEvent*)event)->window);
				if (window)
				{
					ccm_debug_window(window, "CREATE MAP");
					if (!ccm_screen_add_window(self, window))
					{
						g_object_unref(window);
						window = NULL;
					}
					else
						ccm_window_map(window);
				}
			}
			if (window)
			{
				CCMWindow* transient = ccm_window_transient_for (window);
		
				if (transient) ccm_screen_restack (self, window, transient);
			}
		}
		break;
		case UnmapNotify:
		{	
			CCMWindow* window;
			
			window = ccm_screen_find_window (self,
											((XUnmapEvent*)event)->window);
			if (window) 
			{
				ccm_debug_window(window, "UNMAP");
				ccm_window_unmap(window);
			}
		}
		break;
		case ReparentNotify:
		{
			CCMWindow* window = ccm_screen_find_window(self,
											((XReparentEvent*)event)->window);
			CCMWindow* parent = ccm_screen_find_window(self,
											((XReparentEvent*)event)->parent);
				
			if (parent == self->priv->root)
			{
				if (!window)
				{
					window = ccm_window_new(self, ((XReparentEvent*)event)->window);
					if (window)
					{
						ccm_debug_window(window, "REPARENT ADD");
						if (!ccm_screen_add_window(self, window))
							g_object_unref(window);
					}
				}
			}
			else if (parent && window)
			{
				ccm_debug_window(window, "REPARENT REMOVE");
				ccm_screen_remove_window (self, window);
				ccm_drawable_damage(CCM_DRAWABLE(parent));
			}
		}
		break;
		case CirculateNotify:
		{
			XCirculateEvent* circulate_event = (XCirculateEvent*)event;
			
			CCMWindow* window = ccm_screen_find_window(self,
													   circulate_event->window);
			
			if (window) 
			{
				ccm_debug_window(window,"CIRCULATE");
				ccm_screen_check_stack_position (self, window);
			}
		}
		break;
		case ConfigureNotify:
		{
			XEvent ce;
			XConfigureEvent* configure_event = (XConfigureEvent*)event;
			CCMWindow* window;
			
			while (XCheckTypedWindowEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), 
										  configure_event->window,
                                          event->type, &ce)) 
			{
                if (ce.xconfigure.above != configure_event->above &&
					ce.xconfigure.width != configure_event->width &&
					ce.xconfigure.above != configure_event->height) 
				{
                    XPutBackEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), &ce);
                    break;
                }
                configure_event = &ce.xconfigure;
			}
			
			window = ccm_screen_find_window(self, configure_event->window);
			
			if (window)
			{
				if (configure_event->above != None)
				{
					if (configure_event->above && 
						configure_event->above != CCM_WINDOW_XWINDOW(self->priv->root) &&
						configure_event->above != CCM_WINDOW_XWINDOW(self->priv->cow) &&
						configure_event->above != self->priv->selection_owner)
					{
						CCMWindow* above;
						above = ccm_screen_find_window (self,
														configure_event->above);
						if (above) ccm_screen_restack(self, window, above);
					}
				}
								
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
			XEvent ce;
			CCMWindow* window;
			
			ccm_debug_atom(self->priv->display, property_event->atom, 
						   "PROPERTY_NOTIFY");

			while (XCheckTypedWindowEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), 
										  property_event->window,
                                          event->type, &ce)) 
			{
                if (ce.xproperty.atom != property_event->atom) 
				{
                    XPutBackEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), &ce);
                    break;
                }
                property_event = &ce.xproperty;
            }
			
			if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->client_stacking_list_atom ||
				property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->client_list_atom)
			{
				ccm_screen_add_check_pending (self);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->transient_for_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) ccm_window_query_transient_for (window);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->opacity_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) 
					ccm_window_query_opacity(window, 
									property_event->state == PropertyDelete);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->type_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) ccm_window_query_hint_type(window);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->mwm_hints_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) ccm_window_query_mwm_hints (window);
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->state_atom)
			{
				window = ccm_screen_find_window_or_child (self,
													   property_event->window);
				if (window) ccm_window_query_state(window);
			}
		}
		break;
		case Expose:
		{
			XExposeEvent* expose_event = (XExposeEvent*)event;
			cairo_rectangle_t area;
			CCMRegion* damaged;
			
			ccm_debug("EXPOSE");
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
			XEvent ce;
			XClientMessageEvent* client_event = (XClientMessageEvent*)event;
			
			ccm_debug_atom(self->priv->display, client_event->message_type, 
						   "CLIENT MESSAGE");
			while (XCheckTypedWindowEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), 
										  client_event->window,
                                          event->type, &ce)) 
			{
                if (ce.xclient.message_type != client_event->message_type) 
				{
                    XPutBackEvent(CCM_DISPLAY_XDISPLAY(self->priv->display), &ce);
                    break;
                }
                client_event = &ce.xclient;
				ccm_debug_atom(self->priv->display, client_event->message_type, 
						   "COMPRESS CLIENT MESSAGE");
            }
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
								ccm_debug_atom(self->priv->display,
											   client_event->data.l[cpt],
											   "UNSET STATE");
								ccm_window_unset_state(window, 
													 client_event->data.l[cpt]);
								break;
							case 1:
								ccm_debug_atom(self->priv->display,
											   client_event->data.l[cpt],
											   "SET STATE");
								ccm_window_set_state(window, 
													 client_event->data.l[cpt]);
								break;
							case 2:
								ccm_debug_atom(self->priv->display,
											   client_event->data.l[cpt],
											   "SWITCH STATE");
								ccm_window_switch_state(window, 
													 client_event->data.l[cpt]);
								break;
							default:
								break;
						}
					}
				}
			}
			else if (client_event->message_type == 
							CCM_WINDOW_GET_CLASS(self->priv->root)->type_atom)
			{
				CCMWindow* window = ccm_screen_find_window_or_child (self,
													   client_event->window);
				if (window) ccm_window_query_hint_type(window);
			}
			else if (client_event->message_type == 
						CCM_WINDOW_GET_CLASS(self->priv->root)->opacity_atom)
			{
				CCMWindow* window = ccm_screen_find_window_or_child (self,
													   client_event->window);
				if (window) ccm_window_query_opacity (window, FALSE);
			}
		}
		break;
		default:
		break;
	}
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
_ccm_screen_indirect_rendering(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_SCREEN_INDIRECT]);
}

gboolean
_ccm_screen_use_buffered(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_SCREEN_USE_BUFFERED]);
}

guint
_ccm_screen_get_refresh_rate(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, 60);
	
	return self->priv->refresh_rate;
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
_ccm_screen_set_buffered(CCMScreen* self, gboolean buffered)
{
	self->priv->buffered = buffered && _ccm_screen_use_buffered(self);
}

CCMScreen*
ccm_screen_new(CCMDisplay* display, guint number)
{
	g_return_val_if_fail(display != NULL, NULL);
	
	CCMScreen *self = g_object_new(CCM_TYPE_SCREEN, NULL);
	
	self->priv->display = display;
	
	self->xscreen = ScreenOfDisplay(CCM_DISPLAY_XDISPLAY(display), number);
	self->number = number;
	
	ccm_screen_load_config(self);
	
	self->priv->plugin_loader = ccm_extension_loader_new();
	
	/* Load plugins */
	ccm_screen_get_plugins (self);
	
	g_signal_connect_swapped(self->priv->display, "event", 
							 G_CALLBACK(ccm_screen_on_event), self);
	
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
				      ExposureMask |
					  PropertyChangeMask |
					  StructureNotifyMask |
					  SubstructureNotifyMask |
					  SubstructureRedirectMask);
	}
	
	return self->priv->root;
}

gboolean
ccm_screen_add_window(CCMScreen* self, CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);

	gboolean ret = FALSE;
	
	if (!ccm_screen_valid_window(self, window))
		return ret;
	
	if (!window->is_input_only &&
		!ccm_screen_find_window(self, CCM_WINDOW_XWINDOW(window)))
	{
		ret = ccm_screen_plugin_add_window(self->priv->plugin, self, window);
	}
	
	return ret;
}

void
ccm_screen_remove_window(CCMScreen* self, CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	
	if (!g_list_find(self->priv->removed, window))
	{
		if (window->is_viewable && !window->is_input_only) 
		{
			ccm_debug_window(window, "UNMAP ON REMOVE");
			ccm_window_unmap (window);
		}
	
		ccm_screen_plugin_remove_window(self->priv->plugin, self, window);
	}
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
			(((CCMWindow*)item->data)->is_viewable || 
			 ((CCMWindow*)item->data)->unmap_pending))
		{
			ccm_drawable_damage_region (item->data, area);
		}
	}
}

GList*
ccm_screen_get_windows (CCMScreen *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->windows;
}

CCMRegion*
ccm_screen_get_damaged (CCMScreen *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->priv->damaged;
}

void
ccm_screen_add_damaged_region (CCMScreen *self, CCMRegion* region)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (region != NULL);

	if (self->priv->damaged)
		ccm_region_union(self->priv->damaged, region);
	else
		self->priv->damaged = ccm_region_copy(region);
}

void
ccm_screen_set_filtered_damage(CCMScreen* self, gboolean filtered)
{
    g_return_if_fail(self != NULL);
    self->priv->filtered_damage = filtered;
}

void
ccm_screen_activate_window(CCMScreen* self, CCMWindow* window, Time timestamp)
{
	g_return_if_fail(self != NULL);
    g_return_if_fail(window != NULL);
	
	XEvent event;
    CCMDisplay* display = ccm_screen_get_display (self);
	CCMWindow* root = ccm_screen_get_root_window (self);

	g_print("%s\n", __FUNCTION__);
	event.xclient.type = ClientMessage;
  	event.xclient.serial = 0;
  	event.xclient.send_event = True;
  	event.xclient.display = CCM_DISPLAY_XDISPLAY(display);
  	event.xclient.window = CCM_WINDOW_XWINDOW(window);
  	event.xclient.message_type = CCM_WINDOW_GET_CLASS(root)->active_atom;
	event.xclient.format = 32;
	event.xclient.data.l[0] = 2;
  	event.xclient.data.l[1] = timestamp;
  	event.xclient.data.l[2] = 0;
  	event.xclient.data.l[3] = 0;
  	event.xclient.data.l[4] = 0;

  	XSendEvent (CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root), False,
	      		SubstructureRedirectMask | SubstructureNotifyMask, &event); 
	
	XRaiseWindow (CCM_DISPLAY_XDISPLAY(display),CCM_WINDOW_XWINDOW(window));
}

gboolean
ccm_screen_query_pointer(CCMScreen* self, CCMWindow** below, 
						 gint *x, gint *y)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(x != NULL && y != NULL, FALSE);
	
	CCMWindow* root = ccm_screen_get_root_window (self);
	gboolean ret = FALSE;
	Window r, w;
	gint xw, yw;
	guint state;
	
	if (XQueryPointer(CCM_DISPLAY_XDISPLAY(self->priv->display), 
					  CCM_WINDOW_XWINDOW(root), &r, &w, x, y, &xw, &yw,
					  &state))
	{
		if (below) *below = ccm_screen_find_window_or_child (self, w);
		ret = TRUE;
	}
	
	return ret;
}
