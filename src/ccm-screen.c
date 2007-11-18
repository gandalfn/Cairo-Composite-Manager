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

static void ccm_screen_iface_init(CCMScreenPluginClass* iface);

G_DEFINE_TYPE_EXTENDED (CCMScreen, ccm_screen, G_TYPE_OBJECT, 0,
						G_IMPLEMENT_INTERFACE(CCM_TYPE_SCREEN_PLUGIN,
											  ccm_screen_iface_init))

enum
{
	CCM_SCREEN_PLUGINS,
	CCM_SCREEN_REFRESH_RATE,
	CCM_SCREEN_SYNC_WITH_VBLANK,
	CCM_SCREEN_OPTION_N
};

static gchar* CCMScreenOptions[CCM_SCREEN_OPTION_N] = {
	"plugins",
	"refresh_rate",
	"sync_with_vblank"
};

struct _CCMScreenPrivate
{
	CCMDisplay* 		display;
	
	CCMWindow* 			root;
	CCMWindow* 			cow;
	Window				selection_owner;
	
	GList*				windows;
	
	GTimer*				vblank;
	guint				id_paint;
	
	CCMExtensionLoader* plugin_loader;
	CCMScreenPlugin*	plugin;

	CCMConfig*			options[CCM_SCREEN_OPTION_N];
};

#define CCM_SCREEN_GET_PRIVATE(o)  \
   ((CCMScreenPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_SCREEN, CCMScreenClass))

static void impl_ccm_screen_paint(CCMScreenPlugin* plugin, CCMScreen* self);
static gboolean impl_ccm_screen_add_window(CCMScreenPlugin* plugin, 
										   CCMScreen* self, CCMWindow* window);
static void impl_ccm_screen_remove_window(CCMScreenPlugin* plugin, 
										  CCMScreen* self, 
										  CCMWindow* window);
static void on_window_damaged(CCMScreen* self, cairo_rectangle_t* area, 
							  CCMWindow* window);

static void
ccm_screen_init (CCMScreen *self)
{
	self->priv = CCM_SCREEN_GET_PRIVATE(self);
	
	self->priv->display = NULL;
	self->priv->root = NULL;
	self->priv->cow = NULL;
	self->priv->windows = NULL;
	self->priv->vblank = g_timer_new();
	self->priv->id_paint = 0;
	self->priv->selection_owner = None;
	self->priv->plugin_loader = NULL;
}

static void
ccm_screen_finalize (GObject *object)
{
	CCMScreen *self = CCM_SCREEN(object);
	gint cpt;
	
	if (self->priv->plugin_loader)
		g_object_unref(self->priv->plugin_loader);
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; cpt++)
		g_object_unref(self->priv->options[cpt]);
	
	if (self->priv->vblank)
		g_timer_destroy(self->priv->vblank);
	
	if (self->priv->id_paint)
		g_source_remove(self->priv->id_paint);
	
	if (self->priv->windows) 
	{
		g_list_foreach(self->priv->windows, (GFunc)g_object_unref, NULL);
		g_list_free(self->priv->windows);
	}
	if (self->priv->root) 
	{
		ccm_window_unredirect_subwindows(self->priv->root);
		g_object_unref(self->priv->root);
	}
	if (self->priv->cow) 
	{
		XCompositeReleaseOverlayWindow(CCM_DISPLAY_XDISPLAY(self->priv->display),
									   CCM_WINDOW_XWINDOW(self->priv->cow));
		g_object_unref(self->priv->cow);
	}
	if (self->priv->selection_owner) 
		XDestroyWindow(CCM_DISPLAY_XDISPLAY(self->priv->display),
					   self->priv->selection_owner);
	
	if (self->priv->display) g_object_unref(self->priv->display);
	
	G_OBJECT_CLASS (ccm_screen_parent_class)->finalize (object);
}

static void
ccm_screen_class_init (CCMScreenClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMScreenPrivate));
	
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

static gboolean
ccm_screen_set_selection_owner(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	gchar* cm_atom_name = g_strdup_printf("_NET_WM_CM_S%i", self->number);
	Atom cm_atom = XInternAtom(CCM_DISPLAY_XDISPLAY(self->priv->display), 
							   cm_atom_name, 0);
	CCMWindow* root = ccm_screen_get_root_window(self);
	
	g_free(cm_atom_name);
	 
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
}

static void
ccm_screen_restack(CCMScreen* self, CCMWindow* above, CCMWindow* below)
{
	g_return_if_fail(self != NULL);
	
	if (!above && !below)
		return;
	
	if (!above)
	{
		GList* below_link = g_list_find(self->priv->windows, below);
		if (below_link) 
			self->priv->windows = g_list_remove_link (self->priv->windows,
													  below_link);
		self->priv->windows = g_list_prepend(self->priv->windows, below);
		ccm_drawable_damage(CCM_DRAWABLE(below));
		return;
	}
		
	if (!below)
	{
		GList* above_link = g_list_find(self->priv->windows, above);
		if (above_link) 
			self->priv->windows = g_list_remove_link (self->priv->windows,
													  above_link);
		self->priv->windows = g_list_append(self->priv->windows, above);
		ccm_drawable_damage(CCM_DRAWABLE(above));
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
	
	ccm_drawable_damage(CCM_DRAWABLE(below));
	ccm_drawable_damage(CCM_DRAWABLE(above));
}

static void 
impl_ccm_screen_paint(CCMScreenPlugin* plugin, CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	gboolean ret = FALSE;
		
	if (self->priv->cow)
	{
		cairo_t* ctx = ccm_drawable_create_context(CCM_DRAWABLE(self->priv->cow));
		GList* item;
		
		for (item = self->priv->windows; item; item = item->next)
		{
			CCMWindow* window = item->data;
				
			if (!ccm_window_is_input_only(window))
			{
				if (ccm_window_paint(window, ctx))
					ret = TRUE;
			}
		}
		cairo_destroy(ctx);
		if (ret) ccm_drawable_flush(CCM_DRAWABLE(self->priv->cow));
	}
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
	
	if (self->priv->selection_owner && 
		CCM_WINDOW_XWINDOW(window) == self->priv->selection_owner)
		return FALSE;
	
	if (!ccm_window_is_input_only(window) &&
		!ccm_screen_find_window(self, CCM_WINDOW_XWINDOW(window)))
	{
		CCMWindowType type = ccm_window_get_hint_type(window);

		if (type != CCM_WINDOW_TYPE_DESKTOP)
		{
			GList* item;
			
			for (item = g_list_last(self->priv->windows); 
				 (type == CCM_WINDOW_TYPE_NORMAL || 
				  type == CCM_WINDOW_TYPE_UNKNOWN) && item; item = item->prev)
			{
				if (ccm_window_get_hint_type(item->data) != CCM_WINDOW_TYPE_DOCK)
					break;
			}
			if (item && item->next)
				self->priv->windows = g_list_insert_before(self->priv->windows, 
														   item->next, window);
			else
				self->priv->windows = g_list_append(self->priv->windows, window);
		}
		else
			self->priv->windows = g_list_prepend(self->priv->windows, window);
		
		g_signal_connect_swapped(window, "damaged", G_CALLBACK(on_window_damaged), self);
		
		if (ccm_window_is_viewable(window))
			ccm_drawable_damage(CCM_DRAWABLE(window));
		
		return TRUE;
	}
	
	return FALSE;
}

static void
impl_ccm_screen_remove_window(CCMScreenPlugin* plugin, CCMScreen* self, 
							  CCMWindow* window)
{
	cairo_rectangle_t geometry;
		
	if (ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(window), &geometry))
		ccm_drawable_damage_rectangle(CCM_DRAWABLE(self->priv->cow), &geometry);
	
	self->priv->windows = g_list_remove(self->priv->windows, window);
}

static gboolean
ccm_screen_paint(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	ccm_screen_plugin_paint(self->priv->plugin, self);
	
	self->priv->id_paint = 0;
	g_timer_start(self->priv->vblank);
	
	return TRUE;
}

static void
on_window_damaged(CCMScreen* self, cairo_rectangle_t* area, CCMWindow* window)
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
		CCMRegion* geometry = ccm_drawable_get_geometry(CCM_DRAWABLE(window));
		CCMRegion* obscured = NULL;
		CCMRegion* damaged = NULL;
		
		damaged = ccm_region_rectangle(area);
		if (ccm_window_is_opaque(window) && 
			ccm_window_is_viewable(window))
		{
			CCMRegion* opaque = ccm_window_get_opaque_region(window);
			ccm_region_subtract(damaged, opaque);
		}
		
		// Substract all obscured area to damaged region
		for (item = g_list_last(self->priv->windows); item; item = item->prev)
		{
			if (ccm_window_is_viewable(item->data) && item->data != window)
			{
				if (top)
				{
					if (ccm_window_is_opaque(item->data))
					{
						CCMRegion* opaque = ccm_window_get_opaque_region(item->data);
						
						if (obscured)
						{
							CCMRegion* tmp = ccm_region_copy(geometry);
						
							ccm_region_intersect(tmp, opaque);
							ccm_region_union(obscured, tmp);
							ccm_region_destroy(tmp);
						}
						else
						{
							obscured = ccm_region_copy(opaque);
							ccm_region_intersect(obscured, geometry);
						}
						
					}
				}
			}
			else if (item->data == window)
			{
				if (obscured)
					ccm_drawable_undamage_region(CCM_DRAWABLE(window), 
												 obscured);
				break;
			}
		}
		
		// window is totaly obscured don't damage all other windows
		if (!ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
			return;
		
		// damage now all concurent window
		for (item = g_list_last(self->priv->windows); item; item = item->prev)
		{
			if (ccm_window_is_viewable(item->data) && item->data != window)
			{
				if (top)
				{
					if (ccm_window_is_opaque(item->data))
					{
						CCMRegion* win_geometry;
						CCMRegion* d = ccm_region_rectangle(area);
						CCMRegion* opaque = ccm_window_get_opaque_region(item->data);
						
						win_geometry = ccm_drawable_get_geometry(item->data);
						if (win_geometry)
						{
							CCMRegion* translucent;
							
							translucent  = ccm_region_copy(win_geometry);
							ccm_region_subtract(translucent, opaque);
							
							if (!ccm_region_empty (translucent))
							{
								ccm_region_intersect(d, translucent);
								
								if (!ccm_region_empty(d))
								{
									if (obscured) ccm_region_subtract(d, obscured);
								
									g_signal_handlers_block_by_func(item->data, 
																on_window_damaged, 
																self);
									ccm_drawable_damage_region(CCM_DRAWABLE(item->data), 
															   d);
									g_signal_handlers_unblock_by_func(item->data, 
																	  on_window_damaged, 
																	  self);
								}
							}
							ccm_region_destroy(translucent);
						}
						ccm_region_destroy(d);
						
						if (!ccm_region_empty(damaged))
						{
							g_signal_handlers_block_by_func(item->data, 
														on_window_damaged, 
														self);
							ccm_drawable_damage_region(CCM_DRAWABLE(item->data), 
													   damaged);
							g_signal_handlers_unblock_by_func(item->data, 
															  on_window_damaged, 
															  self);
						}
					}
					else
					{
						CCMRegion* d = ccm_region_rectangle(area);
						if (obscured) ccm_region_subtract(d, obscured);
						g_signal_handlers_block_by_func(item->data, 
														on_window_damaged, 
														self);
						ccm_drawable_damage_region(item->data, d);
						g_signal_handlers_unblock_by_func(item->data, 
														  on_window_damaged, 
														  self);
						ccm_region_destroy(d);
					}
				}
				else
				{
					g_signal_handlers_block_by_func(item->data, 
													on_window_damaged, 
													self);
					ccm_drawable_damage_region(CCM_DRAWABLE(item->data), 
											   damaged);
					g_signal_handlers_unblock_by_func(item->data, 
													  on_window_damaged, 
													  self);
				}
			}
			else if (item->data == window)
			{
				top = FALSE;
				if (ccm_region_empty(damaged)) break;
			}
		}
		
		if (obscured) ccm_region_destroy(obscured);
		if (damaged) ccm_region_destroy(damaged);
	}
}

static void
on_event(CCMScreen* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
	switch (event->type)
	{
		case CreateNotify:
		{	
			CCMWindow* window = ccm_screen_find_window(self,
										((XCreateWindowEvent*)event)->window);
			if (!window) 
			{
				CCMWindow* parent = ccm_screen_find_window(self,
										((XCreateWindowEvent*)event)->parent);
				CCMWindow* root = ccm_screen_get_root_window(self);
				if (parent || ((XCreateWindowEvent*)event)->parent == CCM_WINDOW_XWINDOW(root))
				{
					window = ccm_window_new(self, ((XCreateWindowEvent*)event)->window);
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
			if (window) 
			{
				ccm_screen_remove_window(self, window);
				g_object_unref(window);
			}
			break;	
		}
		case MapNotify:
		{	
			CCMWindow* window = ccm_screen_find_window(self,
											((XMapEvent*)event)->window);
			if (!window) 
			{
				window = ccm_window_new(self, ((XMapEvent*)event)->window);
				if (!ccm_screen_add_window(self, window))
					g_object_unref(window);
			}
			else
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
			if (((XReparentEvent*)event)->parent == CCM_WINDOW_XWINDOW(self->priv->root))
			{
				if (!window)
				{
					window = ccm_window_new(self, ((XReparentEvent*)event)->window);
					if (!ccm_screen_add_window(self, window))
						g_object_unref(window);
				}
				else
					ccm_window_set_parent (window, NULL);
			}
			else if (window)
			{
				CCMWindow* parent = ccm_screen_find_window(self,
											((XReparentEvent*)event)->parent);
				if (parent)
				{
					ccm_window_set_parent (window, parent);
					ccm_window_unmap (window);
				}
				else
				{
					ccm_screen_remove_window (self, window);
					g_object_unref(window);
				}
			}
		}
		break;
		case CirculateNotify:
		{
			g_print("CirculateNotify\n");
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
						configure_event->above != self->priv->selection_owner)
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
			CCMWindow* window = ccm_screen_find_window(self,
													   property_event->window);
			
			if (window)
			{
				if (property_event->atom == CCM_WINDOW_GET_CLASS(window)->opacity_atom)
					ccm_window_query_opacity(window);
				else if (property_event->atom == CCM_WINDOW_GET_CLASS(window)->type_atom)
					ccm_window_query_hint_type(window);
				else if (property_event->atom == CCM_WINDOW_GET_CLASS(window)->state_atom)
					ccm_window_query_state(window);
			}
		}
		break;
		case Expose:
			g_print("Expose\n");
			break;
		case ClientMessage:
		{
			XClientMessageEvent* client_event = (XClientMessageEvent*)event;
			
			CCMWindow* window = ccm_screen_find_window(self,
													   client_event->window);
			
			if (window)
			{
				if (client_event->message_type == CCM_WINDOW_GET_CLASS(window)->state_atom)
				{
					gint cpt;
					
					for (cpt = 1; cpt < 3; cpt++)
					{
						ccm_window_switch_state(window, client_event->data.l[cpt]);
					}
				}
			}
		}
		break;
		default:
			if (event->type == 
				ccm_display_get_shape_notify_event_type(self->priv->display))
			{
				XShapeEvent* shape_event = (XShapeEvent*)event;
				CCMWindow* window = ccm_screen_find_window(self,
														   shape_event->window);
				if (window)
				{
					//ccm_drawable_query_geometry(CCM_DRAWABLE(window));
					//ccm_drawable_damage(CCM_DRAWABLE(window));
				}
			}
			else
				g_print("Event : %i\n", event->type);
		break;
	}
	
	if (g_timer_elapsed(self->priv->vblank, NULL) > 1.0f / 60.0f)
		ccm_screen_paint(self);
}

gboolean
_ccm_screen_sync_with_blank(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return ccm_config_get_boolean(self->priv->options[CCM_SCREEN_SYNC_WITH_VBLANK]);
}

GSList*
_ccm_screen_get_window_plugins(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return ccm_extension_loader_get_window_plugins(self->priv->plugin_loader);
}

CCMScreen*
ccm_screen_new(CCMDisplay* display, guint number)
{
	g_return_val_if_fail(display != NULL, NULL);
	
	CCMScreen *self = g_object_new(CCM_TYPE_SCREEN, NULL);
	GSList* filter = NULL, *plugins = NULL, *item;
	int refresh_rate;
	
	self->priv->display = g_object_ref(display);
	
	self->xscreen = ScreenOfDisplay(CCM_DISPLAY_XDISPLAY(display), number);
	self->number = number;
	
	ccm_screen_load_config(self);
	
	filter = ccm_config_get_string_list(self->priv->options[CCM_SCREEN_PLUGINS]);
	self->priv->plugin_loader = ccm_extension_loader_new(filter);
	g_slist_foreach(filter, (GFunc)g_free, NULL);
	g_slist_free(filter);
	
	/* Load plugins */
	self->priv->plugin = (CCMScreenPlugin*)self;
	plugins = ccm_extension_loader_get_screen_plugins(self->priv->plugin_loader);
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
	
	ccm_screen_set_selection_owner(self);
	ccm_window_redirect_subwindows(ccm_screen_get_root_window(self));
	ccm_screen_query_stack(self);
	
	refresh_rate = ccm_config_get_integer(self->priv->options[CCM_SCREEN_REFRESH_RATE]);
	
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
				      SubstructureNotifyMask |
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
