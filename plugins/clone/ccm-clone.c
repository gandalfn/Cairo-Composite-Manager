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

#include <X11/Xatom.h>

#include "ccm-debug.h"
#include "ccm-drawable.h"
#include "ccm-window-plugin.h"
#include "ccm-screen-plugin.h"
#include "ccm-plugin.h"
#include "ccm-pixmap.h"
#include "ccm-window.h"
#include "ccm-clone.h"
#include "ccm-screen.h"
#include "ccm-display.h"
#include "ccm.h"

static void ccm_clone_screen_iface_init  (CCMScreenPluginClass* iface);
static void ccm_clone_window_iface_init  (CCMWindowPluginClass* iface);

static void ccm_clone_on_composite_message (CCMClone* self, CCMWindow* window,
                                            Atom atom, Pixmap xpixmap, 
                                            gint depth, CCMScreen* screen);

CCM_DEFINE_PLUGIN (CCMClone, ccm_clone, CCM_TYPE_PLUGIN, 
                   CCM_IMPLEMENT_INTERFACE(ccm_clone,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_clone_screen_iface_init)
				   CCM_IMPLEMENT_INTERFACE(ccm_clone,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_clone_window_iface_init))

struct _CCMClonePrivate
{	
	CCMScreen	*screen;
	
	GSList		*outputs;
};

#define CCM_CLONE_GET_PRIVATE(o)  \
   ((CCMClonePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CLONE, CCMCloneClass))

static void
ccm_clone_init (CCMClone* self)
{
	self->priv = CCM_CLONE_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->outputs = NULL;
}

static void
ccm_clone_finalize (GObject *object)
{
	CCMClone* self = CCM_CLONE(object);
	
	if (self->priv->screen)
		g_signal_handlers_disconnect_by_func(self->priv->screen, 
											 ccm_clone_on_composite_message, 
											 self);	

	g_slist_foreach(self->priv->outputs, (GFunc)g_object_unref, NULL);
	g_slist_free(self->priv->outputs);
	
	G_OBJECT_CLASS (ccm_clone_parent_class)->finalize (object);
}

static void
ccm_clone_class_init (CCMCloneClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMClonePrivate));
	
	object_class->finalize = ccm_clone_finalize;
}

static void
ccm_clone_add_output(CCMClone* self, CCMWindow* window, 
                     Pixmap xpixmap, gint depth)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(xpixmap != None);

	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(window));
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(window));
	XVisualInfo vinfo;
	
	if (XMatchVisualInfo (CCM_DISPLAY_XDISPLAY(display), 
	                      CCM_SCREEN_NUMBER(screen),
	                      depth, TrueColor, &vinfo))
	{
		CCMPixmap* output = ccm_pixmap_new_from_visual (screen, vinfo.visual, 
		                                                xpixmap);
		if (output)
		{
			const cairo_rectangle_t* area = ccm_window_get_area (window);
			cairo_rectangle_t clipbox, geometry;
			cairo_t* ctx;
			
			ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE(window), 
			                                          &geometry);
			self->priv->outputs = g_slist_prepend(self->priv->outputs, output);
			ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(output), &clipbox);

			ctx = ccm_drawable_create_context (CCM_DRAWABLE(output));
			if (ctx)
			{
				cairo_surface_t* surface = 
					ccm_drawable_get_surface (CCM_DRAWABLE(window));
				
				cairo_scale(ctx, clipbox.width / area->width, 
							clipbox.height / area->height);
				cairo_set_source_surface(ctx, surface, 
				                         -(geometry.width - area->width) / 2, 
				                         -(geometry.height - area->height) / 2);
				cairo_paint(ctx);
				cairo_destroy(ctx);
				cairo_surface_destroy(surface);
			}
		}
	}
}

static void
ccm_clone_remove_output(CCMClone* self, CCMWindow* window, Pixmap xpixmap)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(xpixmap != None);

	GSList* item;

	for (item = self->priv->outputs; item; item = item->next)
	{
		if (CCM_PIXMAP_XPIXMAP(item->data) == xpixmap)
		{
			self->priv->outputs = g_slist_remove_link(self->priv->outputs, item);
			break;
		}
	}
}

static void 
ccm_clone_on_composite_message (CCMClone* self, CCMWindow* window, Atom atom, 
                                Pixmap xpixmap, gint depth, CCMScreen* screen)

{
	g_return_if_fail(self != NULL);
	g_return_if_fail(window != NULL);

	if (atom == CCM_CLONE_GET_CLASS(self)->clone_enable_atom)
	{
		CCMClone* plugin = CCM_CLONE(_ccm_window_get_plugin (window, 
		                                                     CCM_TYPE_CLONE));
		if (plugin)
		{
			ccm_debug_window(window, "ENABLE CLONE MESSAGE 0x%x,%i", xpixmap, depth);
			ccm_clone_add_output(plugin, window, xpixmap, depth);
		}
	}

	if (atom == CCM_CLONE_GET_CLASS(self)->clone_disable_atom)
	{
		CCMClone* plugin = CCM_CLONE(_ccm_window_get_plugin (window, 
		                                                     CCM_TYPE_CLONE));
		
		if (plugin)
		{
			ccm_debug_window(window, "DISABLE CLONE MESSAGE 0x%x,%i", xpixmap, depth);
			ccm_clone_remove_output(plugin, window, xpixmap);
		}
	}
}

static void
ccm_clone_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMClone* self = CCM_CLONE(plugin);
	CCMCloneClass* klass = CCM_CLONE_GET_CLASS(self);
	
	if (!klass->clone_enable_atom || !klass->clone_disable_atom)
	{
		CCMDisplay* display = ccm_screen_get_display(screen);
		
		klass->clone_enable_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
		                                        "_CCM_CLONE_ENABLE", False);
		klass->clone_disable_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
		                                         "_CCM_CLONE_DISABLE", False);
	}
	g_signal_connect_swapped(screen, "composite-message", 
	                         G_CALLBACK(ccm_clone_on_composite_message), self);

	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
}

static gboolean 
ccm_clone_window_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
                       cairo_t* context, cairo_surface_t* surface, 
                       gboolean y_invert)
{
	CCMClone* self = CCM_CLONE(plugin);
	GSList* item;
	gboolean ret;
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window, 
	                              context, surface, y_invert);

	if (ret)
	{
		const cairo_rectangle_t* area = ccm_window_get_area (window);
		
		for (item = self->priv->outputs; area && item; item = item->next)
		{
			CCMPixmap* pixmap = CCM_PIXMAP(item->data);
			cairo_rectangle_t clipbox, geometry;

			ccm_drawable_get_device_geometry_clipbox (CCM_DRAWABLE(window), 
			                                          &geometry);
			
			if (ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(pixmap), 
			                                       &clipbox))
			{
				cairo_t* ctx;
				
				ctx = ccm_drawable_create_context (CCM_DRAWABLE(pixmap));
				if (ctx)
				{
					cairo_path_t* damaged;
					
					ccm_debug("PAINT CLONE %x", CCM_PIXMAP_XPIXMAP(pixmap));
					cairo_translate(ctx, -geometry.x, -geometry.y);
					damaged = ccm_drawable_get_damage_path(CCM_DRAWABLE(window), 
														   context);
					cairo_clip(context);
					cairo_translate(ctx, geometry.x, geometry.y);
					cairo_scale(ctx, clipbox.width / area->width, 
								clipbox.height / area->height);
					cairo_path_destroy(damaged);
					cairo_set_source_surface(ctx, surface, 
					                         -(geometry.width - area->width) / 2, 
					                         -(geometry.height - area->height) / 2);
					cairo_paint(ctx);
					cairo_destroy(ctx);
				}
			}
		}
	}
	
	return ret;
}

static void
ccm_clone_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_clone_screen_load_options;
	iface->paint			= NULL;
	iface->add_window		= NULL;
	iface->remove_window	= NULL;
	iface->damage			= NULL;
}

static void
ccm_clone_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = NULL;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = ccm_clone_window_paint;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
	iface->get_origin		 = NULL;
}

