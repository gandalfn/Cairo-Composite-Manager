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

#include <math.h>
#include <string.h>
#include <cairo.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xregion.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>

#include "ccm-window.h"
#include "ccm-window-backend.h"
#include "ccm-window-plugin.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-pixmap.h"

static void 		ccm_window_iface_init		(CCMWindowPluginClass* iface);
static CCMRegion* 	ccm_window_query_geometry	(CCMDrawable* drawable);
static void 		ccm_window_move				(CCMDrawable* drawable, 
												 int x, int y);
static void 		ccm_window_resize			(CCMDrawable* drawable, 
												 int width, int height);

static CCMRegion* 	impl_ccm_window_query_geometry(CCMWindowPlugin* plugin,
												   CCMWindow* self);
static gboolean 	impl_ccm_window_paint		  (CCMWindowPlugin* plugin, 
												   CCMWindow* self, 
												   cairo_t* context,        
												   cairo_surface_t* surface);
static void 		impl_ccm_window_map			  (CCMWindowPlugin* plugin, 
												   CCMWindow* self);
static void 		impl_ccm_window_unmap		  (CCMWindowPlugin* plugin, 
												   CCMWindow* self);
static void 		impl_ccm_window_query_opacity (CCMWindowPlugin* plugin, 
												   CCMWindow* self);

G_DEFINE_TYPE_EXTENDED (CCMWindow, ccm_window, CCM_TYPE_DRAWABLE, 0,
						G_IMPLEMENT_INTERFACE(CCM_TYPE_WINDOW_PLUGIN,
											  ccm_window_iface_init))

struct _CCMWindowPrivate
{
	CCMWindowType		hint_type;
	gchar*				name;
	gchar*				class_name;
	
	gboolean			is_input_only;
	gboolean			is_viewable;
	gboolean			is_shaped;
	gboolean			is_shaded;
	gboolean			is_fullscreen;
	gboolean			is_decorated;
	gboolean			has_format;
	cairo_format_t		format;
	gboolean			override_redirect;
	
	gboolean			unmap_pending;

	CCMWindow*			parent;
	CCMPixmap*			pixmap;
	CCMRegion*			opaque;
	
	CCMWindowPlugin*	plugin;
	double				opacity;
};

#define CCM_WINDOW_GET_PRIVATE(o)  \
   ((CCMWindowPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW, CCMWindowClass))

static void
ccm_window_init (CCMWindow *self)
{
	self->priv = CCM_WINDOW_GET_PRIVATE(self);
	self->priv->hint_type = CCM_WINDOW_TYPE_UNKNOWN;
	self->priv->name = NULL;
	self->priv->class_name = NULL;
	self->priv->is_input_only = FALSE;
	self->priv->is_viewable = FALSE;
	self->priv->is_shaped = FALSE;
	self->priv->is_shaded = FALSE;
	self->priv->is_fullscreen = FALSE;
	self->priv->has_format = FALSE;
	self->priv->format = CAIRO_FORMAT_ARGB32;
	self->priv->override_redirect = FALSE;
	self->priv->unmap_pending = FALSE;
	self->priv->opacity = 1.0f;
	self->priv->pixmap = NULL;
	self->priv->opaque = NULL;
	self->priv->plugin = NULL;
}

static void
ccm_window_finalize (GObject *object)
{
	CCMWindow* self = CCM_WINDOW(object);
	
	if (self->priv->pixmap) g_object_unref(self->priv->pixmap);
	if (self->priv->name) g_free(self->priv->name);
	if (self->priv->plugin) g_object_unref(self->priv->plugin);
	if (self->priv->opaque) ccm_region_destroy(self->priv->opaque);
	
	G_OBJECT_CLASS (ccm_window_parent_class)->finalize (object);
}

static void
ccm_window_class_init (CCMWindowClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMWindowPrivate));
	
	klass->opacity_atom = None;
	
	CCM_DRAWABLE_CLASS(klass)->query_geometry = ccm_window_query_geometry;
	CCM_DRAWABLE_CLASS(klass)->move = ccm_window_move;
	CCM_DRAWABLE_CLASS(klass)->resize = ccm_window_resize;
	
	object_class->finalize = ccm_window_finalize;
}

static void
ccm_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= NULL;
	iface->query_geometry 	= impl_ccm_window_query_geometry;
	iface->paint 			= impl_ccm_window_paint;
	iface->map  			= impl_ccm_window_map;
	iface->unmap  			= impl_ccm_window_unmap;
	iface->query_opacity  	= impl_ccm_window_query_opacity;
}

static void
create_atoms(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	CCMWindowClass* klass = CCM_WINDOW_GET_CLASS(self);
	if (!klass->opacity_atom)
	{
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		
		klass->none_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										 "NONE", False);
		
		klass->utf8_string_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										        "UTF8_STRING", 
											    False);
		klass->name_atom         = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										        "_NET_WM_NAME", 
											    False);
		klass->visible_name_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										        "_NET_WM_VISIBLE_NAME", 
											    False);
		klass->opacity_atom      = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										        "_NET_WM_WINDOW_OPACITY", 
											    False);
		klass->type_atom         = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
									            "_NET_WM_WINDOW_TYPE", 
												False);
		klass->type_normal_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_NORMAL", 
												False);
		klass->type_desktop_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
							 				    "_NET_WM_WINDOW_TYPE_DESKTOP", 
											    False);
    	klass->type_dock_atom    = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_DOCK", 
											    False);
    	klass->type_toolbar_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_TOOLBAR",
												False);
    	klass->type_menu_atom    = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_MENU", 
												False);
        klass->type_util_atom    = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_UTILITY",
												False);
    	klass->type_splash_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_SPLASH", 
												False);
    	klass->type_dialog_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
											    "_NET_WM_WINDOW_TYPE_DIALOG", 
												False);
        klass->type_dropdown_menu_atom = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", 
											False);
		klass->type_popup_menu_atom    = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_WINDOW_TYPE_POPUP_MENU", 
											False);
    	klass->type_tooltip_atom       = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_WINDOW_TYPE_TOOLTIP", 
											False);
    	klass->type_notification_atom  = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_WINDOW_TYPE_NOTIFICATION", 
											False);
    	klass->type_combo_atom         = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_WINDOW_TYPE_COMBO", 
											False);
    	klass->type_dnd_atom           = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_WINDOW_TYPE_DND", 
											False);
    	klass->state_atom              = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE", 
											False);
		klass->state_shade_atom        = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_SHADED", 
											False);
		klass->state_fullscreen_atom   = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_FULLSCREEN", 
											False);
		klass->state_above_atom        = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_ABOVE", 
											False);
		klass->state_below_atom        = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_BELOW", 
											False);
		klass->mwm_hints_atom          = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_XA_MOTIF_WM_HINTS", 
											False);
		klass->frame_extends_atom      = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_FRAME_EXTENTS", 
											False);
	}
}

static guint32 *
ccm_window_get_property(CCMWindow* self, Atom property_atom, int req_format, 
						Atom req_type, guint *n_items)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(property_atom != None, NULL);
	
    CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
    int ret;
    Atom type;
    int format;
    gulong n_items_internal;
    guchar *property = NULL;
    gulong bytes_after;
    guint32 *result;
    
    ret = XGetWindowProperty (CCM_DISPLAY_XDISPLAY(display), 
							  CCM_WINDOW_XWINDOW(self), property_atom, 
							  0, G_MAXLONG, False,
							  req_type, &type, &format,
							  &n_items_internal, &bytes_after,
							  &property);
    
    if (ret != Success)
    {
		if (property) XFree(property);
		return NULL;
    }
        
    result = g_memdup (property, n_items_internal * (format / 8));
    XFree(property);
	
    if (n_items) *n_items = n_items_internal;
    
    return result;
}

static gboolean
ccm_window_get_frame_extends(CCMWindow* self, int* left_frame, int* right_frame, 
							 int* top_frame, int* bottom_frame)
{
	guint32* data = NULL;
	guint n_items;
	gboolean ret = FALSE;
	
	data = ccm_window_get_property(self, 
								   CCM_WINDOW_GET_CLASS(self)->frame_extends_atom,
								   32, XA_CARDINAL, &n_items);
	if (data)
	{
		gulong* extends = (gulong*)data;
		
		if (n_items == 4)
		{
			*left_frame   = extends[0];
      		*right_frame  = extends[1];
      		*top_frame    = extends[2];
      		*bottom_frame = extends[3];
			ret = TRUE;
		}
		g_free(data);
	}
	
	return ret;
}

static CCMRegion*
impl_ccm_window_query_geometry(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMRegion* geometry = NULL;
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	int bx, by, cs, cx, cy, o; /* dummies */
	unsigned int bw, bh, cw, ch; /* dummies */
	XWindowAttributes attrs;
	cairo_rectangle_t area;
	gint left_frame, right_frame, top_frame, bottom_frame;
	
	if (!XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display), 
							   CCM_WINDOW_XWINDOW(self), &attrs))
		return NULL;
		
	if (XShapeQueryExtents (CCM_DISPLAY_XDISPLAY(display), 
							CCM_WINDOW_XWINDOW(self), 
							&self->priv->is_shaped, &bx, &by, &bw, &bh, 
							&cs, &cx, &cy, &cw, &ch) && 
							self->priv->is_shaped)
	{
    	gint cpt, nb;
		XRectangle* shapes;
	
		if ((shapes = XShapeGetRectangles (CCM_DISPLAY_XDISPLAY(display), 
										   CCM_WINDOW_XWINDOW(self), 
										   ShapeBounding, &nb, &o)))
		{
			geometry = ccm_region_new();
			for (cpt = 0; cpt < nb; cpt++)
				ccm_region_union_with_xrect(geometry, &shapes[cpt]);
		}
		ccm_region_offset(geometry, attrs.x - attrs.border_width, 
									attrs.y - attrs.border_width);
		XFree(shapes);
	}
	else
	{
		area.x = attrs.x - attrs.border_width;
		area.y = attrs.y - attrs.border_width;
		area.width = attrs.width + attrs.border_width * 2;
		area.height = attrs.height + attrs.border_width * 2;
		geometry = ccm_region_rectangle(&area);
	}
	
	if (ccm_window_get_frame_extends(self, &left_frame, &right_frame,
									 &top_frame, &bottom_frame))
	{
		ccm_region_get_clipbox (geometry, &area);
		ccm_region_offset(geometry, area.x - left_frame, 
									area.y - top_frame);
		ccm_region_resize (geometry, area.width + left_frame + right_frame,
						   area.height + top_frame + bottom_frame);
	}
	
	if (ccm_window_get_format(self) != CAIRO_FORMAT_ARGB32 && 
		self->priv->opacity == 1.0f)
	{
		ccm_window_set_alpha(self);
		self->priv->opaque = ccm_region_copy(geometry);
	}
		
	if (geometry && self->priv->pixmap)
	{
		g_object_unref(self->priv->pixmap);
		self->priv->pixmap = NULL;
	}
	
	return geometry;
}

static CCMRegion*
ccm_window_query_geometry(CCMDrawable* drawable)
{
	g_return_val_if_fail(drawable != NULL, NULL);

	CCMWindow* self = CCM_WINDOW(drawable);
	CCMRegion* geometry = NULL;
	
	geometry = ccm_window_plugin_query_geometry(self->priv->plugin, self);
	
	return geometry;
}

static void
ccm_window_move(CCMDrawable* drawable, int x, int y)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindow* self = CCM_WINDOW(drawable);
	cairo_rectangle_t geometry;
	
	if (ccm_window_is_input_only(self) && !ccm_window_is_viewable(self))
		return;
	
	ccm_drawable_get_geometry_clipbox(drawable, &geometry);
	
	if (x != (int)geometry.x || y != (int)geometry.y)
	{
		CCM_DRAWABLE_CLASS(ccm_window_parent_class)->move(drawable, x, y);
		if (self->priv->opaque)
			ccm_region_offset(self->priv->opaque, x - geometry.x, y - geometry.y);
		ccm_drawable_damage_rectangle(drawable, &geometry);
		ccm_drawable_damage(drawable);
	}
}

static void
ccm_window_resize(CCMDrawable* drawable, int width, int height)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindow* self = CCM_WINDOW(drawable);
	cairo_rectangle_t geometry;
	
	if (ccm_window_is_input_only(self) && !ccm_window_is_viewable(self))
		return;
	
	ccm_drawable_get_geometry_clipbox(drawable, &geometry);
		
	if (width != (int)geometry.width || height != (int)geometry.height)
	{
		CCM_DRAWABLE_CLASS(ccm_window_parent_class)->resize(drawable, width, height);
		ccm_drawable_damage_rectangle(drawable, &geometry);
		ccm_drawable_damage(drawable);
		
		if (self->priv->pixmap)
		{
			g_object_unref(self->priv->pixmap);
			self->priv->pixmap = NULL;
		}
	}
}

static gboolean
impl_ccm_window_paint(CCMWindowPlugin* plugin, CCMWindow* self, 
					  cairo_t* context, cairo_surface_t* surface)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	cairo_rectangle_t geometry;
	
	ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry);
	cairo_set_source_surface(context, surface, geometry.x, geometry.y); 
	cairo_paint_with_alpha(context, self->priv->opacity);
		
	return TRUE;
}

static void
impl_ccm_window_map(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	
	ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	ccm_drawable_damage(CCM_DRAWABLE(self));
}

static void
impl_ccm_window_unmap(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	cairo_rectangle_t geometry;
	
	ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self), &geometry);
	self->priv->unmap_pending = FALSE;
	ccm_screen_damage_rectangle (screen, &geometry);
	ccm_drawable_unset_geometry (CCM_DRAWABLE(self));
	if (self->priv->pixmap)
	{
		g_object_unref(self->priv->pixmap);
		self->priv->pixmap = NULL;
	}
}
	
static void
impl_ccm_window_query_opacity(CCMWindowPlugin* plugin, CCMWindow* self)
{
	guint32* data = NULL;
	guint n_items;
	
	data = ccm_window_get_property(self, 
								   CCM_WINDOW_GET_CLASS(self)->opacity_atom,
								   32, XA_CARDINAL, &n_items);
									  
	if (data) 
	{
		self->priv->opacity = (double)*data / (double)0xffffffff;
		g_free(data);
		
		if (self->priv->opacity == 1.0f && 
			ccm_window_get_format(self) != CAIRO_FORMAT_ARGB32 &&
			!ccm_window_is_opaque(self))
			ccm_window_set_opaque(self);
		else
			ccm_window_set_alpha(self);
		
		ccm_drawable_damage(CCM_DRAWABLE(self));
	}
}

CCMWindow *
ccm_window_new (CCMScreen* screen, Window xwindow)
{
	g_return_val_if_fail(screen != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	CCMRegion* geometry;
	CCMWindow* self = g_object_new(CCM_TYPE_WINDOW_BACKEND(screen), 
								   "screen", screen,
								   "drawable", xwindow,
								   NULL);
	GSList* item, *plugins = _ccm_screen_get_window_plugins(screen);
	self->priv->plugin = (CCMWindowPlugin*)self;
	
	for (item = plugins; item; item = item->next)
	{
		GType type = GPOINTER_TO_INT(item->data);
		GObject* prev = G_OBJECT(self->priv->plugin);
		
		self->priv->plugin = g_object_new(type, "parent", prev, NULL);
	}
	g_slist_free(plugins);
	
	ccm_window_plugin_load_options(self->priv->plugin, self);
	
	geometry = ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	
	create_atoms(self);
	
	if (!ccm_window_is_input_only(self))
	{
		ccm_window_query_hint_type(self);
		ccm_window_query_mwm_hints (self);
		
		XSelectInput (CCM_DISPLAY_XDISPLAY(ccm_screen_get_display(screen)), 
					  CCM_WINDOW_XWINDOW(self),
					  PropertyChangeMask);
	}
	
	return self;
}

gboolean
ccm_window_is_viewable(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->is_viewable;
}

gboolean
ccm_window_is_input_only(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	return self->priv->is_input_only;
}

gboolean
ccm_window_is_managed(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	return !self->priv->override_redirect;
}

void
ccm_window_query_state(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	guint32* data = NULL;
	guint n_items, cpt;
	
	data = ccm_window_get_property(self, 
								   CCM_WINDOW_GET_CLASS(self)->state_atom,
								   32, XA_ATOM, &n_items);
									  
	if (data) 
	{
		Atom *atom = (Atom *) data;
	
		for (cpt = 0; cpt < n_items; cpt++)
		{
			ccm_window_set_state (self, atom[cpt]);
		}
		XFree ((void *) data);
	}
}

void
ccm_window_set_state(CCMWindow* self, Atom state_atom)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->parent) 
		ccm_window_set_state(self->priv->parent, state_atom);
	
	if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_shade_atom)
	{
		self->priv->is_shaded = TRUE;
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
		self->priv->is_fullscreen = TRUE;
		ccm_screen_damage (screen);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_above_atom)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
		ccm_screen_restack (screen, self, NULL);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_below_atom)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
		ccm_screen_restack (screen, NULL, self);
	}
}

void
ccm_window_unset_state(CCMWindow* self, Atom state_atom)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->parent) 
		ccm_window_unset_state(self->priv->parent, state_atom);
	
	if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_shade_atom)
	{
		self->priv->is_shaded = FALSE;
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
		self->priv->is_fullscreen = FALSE;
		ccm_screen_damage (screen);
	}
}

void
ccm_window_switch_state(CCMWindow* self, Atom state_atom)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->parent) 
		ccm_window_switch_state (self->priv->parent, state_atom);
	
	if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_shade_atom)
	{
		self->priv->is_shaded = !self->priv->is_shaded;
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
		self->priv->is_fullscreen = !self->priv->is_fullscreen;
		ccm_screen_damage (screen);
	}
}

gboolean
ccm_window_is_shaded(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->is_shaded;
}

gboolean
ccm_window_is_fullscreen(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->is_fullscreen;
}

void
ccm_window_make_output_only(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	XserverRegion region;
	
	region = XFixesCreateRegion(CCM_DISPLAY_XDISPLAY(display), 0, 0);
	XFixesSetWindowShapeRegion (CCM_DISPLAY_XDISPLAY(display),
								CCM_WINDOW_XWINDOW(self),
								ShapeInput, 0, 0, region);
}

void
ccm_window_redirect (CCMWindow* self)
{
    g_return_if_fail (self != NULL);
    
	XCompositeRedirectWindow (
					CCM_DISPLAY_XDISPLAY(ccm_drawable_get_display(CCM_DRAWABLE(self))), 
					CCM_WINDOW_XWINDOW(self),
					CompositeRedirectManual);
}

void
ccm_window_redirect_subwindows(CCMWindow* self)
{
    g_return_if_fail (self != NULL);
    
	XCompositeRedirectSubwindows(
					CCM_DISPLAY_XDISPLAY(ccm_drawable_get_display(CCM_DRAWABLE(self))), 
					CCM_WINDOW_XWINDOW(self),
					CompositeRedirectManual);
}

void
ccm_window_unredirect (CCMWindow* self)
{
    g_return_if_fail (self != NULL);
    
	XCompositeUnredirectWindow (
					CCM_DISPLAY_XDISPLAY(ccm_drawable_get_display(CCM_DRAWABLE(self))), 
					CCM_WINDOW_XWINDOW(self),
					CompositeRedirectManual);
}

void
ccm_window_unredirect_subwindows (CCMWindow* self)
{
    g_return_if_fail (self != NULL);
    
	XCompositeUnredirectSubwindows (
					CCM_DISPLAY_XDISPLAY(ccm_drawable_get_display(CCM_DRAWABLE(self))), 
					CCM_WINDOW_XWINDOW(self),
					CompositeRedirectManual);
}

void
on_pixmap_damaged(CCMWindow* self, cairo_rectangle_t* area)
{
	g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);
	
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry))
	{
		area->x += geometry.x;
		area->y += geometry.y;
	}
	
	ccm_drawable_damage_rectangle(CCM_DRAWABLE(self), area);
}

CCMPixmap*
ccm_window_get_pixmap(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (!self->priv->pixmap)
	{
		CCMDisplay *display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		
		ccm_display_grab(display);
		Pixmap xpixmap = XCompositeNameWindowPixmap(
												CCM_DISPLAY_XDISPLAY(display),
						 						CCM_WINDOW_XWINDOW(self));
		ccm_display_ungrab(display);
		if (xpixmap)
		{
			self->priv->pixmap = ccm_pixmap_new(self, xpixmap);
			g_signal_connect_swapped(self->priv->pixmap, "damaged", 
									 G_CALLBACK(on_pixmap_damaged), self);
		}
	}
	
	return self->priv->pixmap;
}

cairo_format_t
ccm_window_get_format (CCMWindow* self)
{
    g_return_val_if_fail(self != NULL, 0);
	
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
    XWindowAttributes attrs;
    
    if (self->priv->has_format) return self->priv->format;
    
    if (!XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display), 
							   CCM_WINDOW_XWINDOW(self), &attrs))
		return CAIRO_FORMAT_ARGB32;
	
    self->priv->is_viewable = attrs.map_state == IsViewable;
	self->priv->override_redirect = attrs.override_redirect;
	
    if (attrs.class == InputOnly)
    {
		self->priv->is_input_only = TRUE;
    }
    else if (attrs.depth == 16 &&
			 attrs.visual->red_mask == 0xf800 &&
			 attrs.visual->green_mask == 0x7e0 &&
			 attrs.visual->blue_mask == 0x1f)
    {
		self->priv->format = CAIRO_FORMAT_A8;
    }
    else if (attrs.depth == 24 &&
	     	 attrs.visual->red_mask == 0xff0000 &&
			 attrs.visual->green_mask == 0xff00 &&
			 attrs.visual->blue_mask == 0xff)
    {
		self->priv->format = CAIRO_FORMAT_RGB24;
    }
    else if (attrs.depth == 32 &&
			 attrs.visual->red_mask == 0xff0000 &&
			 attrs.visual->green_mask == 0xff00 &&
			 attrs.visual->blue_mask == 0xff)
    {
		self->priv->format = CAIRO_FORMAT_ARGB32;
    }
    else
    {
		g_warning ("Unknown visual format depth=%d, r=%#lx/g=%#lx/b=%#lx",
				   attrs.depth, attrs.visual->red_mask,
				   attrs.visual->green_mask, attrs.visual->blue_mask);
	
		self->priv->format = CAIRO_FORMAT_ARGB32;
    }
    
	self->priv->has_format = TRUE;
    
    return self->priv->format;
}

guint
ccm_window_get_depth (CCMWindow* self)
{
    g_return_val_if_fail(self != NULL, 0);
	
    switch (ccm_window_get_format(self))
    {
    	case CAIRO_FORMAT_A8:
		return 16;

    	case CAIRO_FORMAT_RGB24:
		return 24;
	
    	case CAIRO_FORMAT_ARGB32:
		return 32;

		default:
		break;
    };

    return 0;
}

gfloat
ccm_window_get_opacity (CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, 1.0f);
	
	return self->priv->opacity;
}

void
ccm_window_set_opacity (CCMWindow* self, gfloat opacity)
{
	g_return_if_fail(self != NULL);
	
	self->priv->opacity = opacity;
	if (self->priv->opacity < 1.0f)
		ccm_window_set_alpha(self);
	else if (self->priv->format != CAIRO_FORMAT_ARGB32)
		ccm_window_set_opaque(self);
}

gboolean
ccm_window_paint (CCMWindow* self, cairo_t* context)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(context != NULL, FALSE);
	
	gboolean ret = FALSE;
	
	cairo_path_t* damaged;
	
	if (!self->priv->is_viewable && !self->priv->unmap_pending)
		return ret;
	
	cairo_save(context);
	damaged = ccm_drawable_get_damage_path(CCM_DRAWABLE(self), context);
	if (damaged)
	{
		CCMPixmap* pixmap = ccm_window_get_pixmap(self);
		if (pixmap)
		{
			cairo_surface_t* surface = ccm_drawable_get_surface(CCM_DRAWABLE(pixmap));
			cairo_clip(context);
			
			if (surface)
			{
				ret = ccm_window_plugin_paint(self->priv->plugin, self, 
											  context, surface);
				cairo_surface_destroy(surface);
			}
		}
		cairo_path_destroy(damaged);
	}
	cairo_restore(context);
	
	if (ret) ccm_drawable_repair(CCM_DRAWABLE(self));
	
	return ret;
}

void
ccm_window_map(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	if (!self->priv->is_viewable)
	{
		self->priv->is_viewable = TRUE;
	
		ccm_window_plugin_map(self->priv->plugin, self);
	}
	else
		ccm_drawable_damage (CCM_DRAWABLE(self));
}

void
ccm_window_unmap(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->is_viewable)
	{
		self->priv->is_viewable = FALSE;
		self->priv->unmap_pending = TRUE;
		
		ccm_window_plugin_unmap(self->priv->plugin, self);
	}
	else
		ccm_drawable_damage (CCM_DRAWABLE(self));
}

void 
ccm_window_query_opacity(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	ccm_window_plugin_query_opacity (self->priv->plugin, self);
}

#define MWM_HINTS_DECORATIONS (1L << 1)

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MotifWmHints;

void 
ccm_window_query_mwm_hints(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	guint32* data = NULL;
	guint n_items;
	
	data = ccm_window_get_property(self, 
								   CCM_WINDOW_GET_CLASS(self)->mwm_hints_atom,
								   sizeof (MotifWmHints) / sizeof(long), 
								   AnyPropertyType, &n_items);
									  
	if (data) 
	{
		MotifWmHints* hints = (MotifWmHints*)data;
		
      	if (hints->flags & MWM_HINTS_DECORATIONS)
			self->priv->is_decorated = hints->decorations != 0;
	  	g_free(data);
    }
}

void
ccm_window_query_hint_type(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	guint32* data = NULL;
	guint n_items;
	
	data = ccm_window_get_property(self, 
								   CCM_WINDOW_GET_CLASS(self)->type_atom,
								   32, XA_ATOM, &n_items);
									  
	if (data) 
	{
		Atom atom;
		
		memcpy (&atom, data, sizeof (Atom));
		XFree ((void *) data);

		if (atom == CCM_WINDOW_GET_CLASS(self)->type_normal_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_NORMAL;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_menu_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_MENU;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_desktop_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_DESKTOP;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_dock_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_DOCK;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_toolbar_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_TOOLBAR;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_util_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_UTILITY;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_splash_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_SPLASH;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_dialog_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_DIALOG;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_dropdown_menu_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_DROPDOWN_MENU;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_popup_menu_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_POPUP_MENU;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_tooltip_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_TOOLTIP;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_notification_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_NOTIFICATION;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_combo_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_COMBO;
		else if (atom == CCM_WINDOW_GET_CLASS(self)->type_dnd_atom)
			self->priv->hint_type = CCM_WINDOW_TYPE_DND;
	}
}

CCMWindowType
ccm_window_get_hint_type(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, CCM_WINDOW_TYPE_NORMAL);
	
	if (self->priv->hint_type == CCM_WINDOW_TYPE_UNKNOWN)
	{
		ccm_window_query_hint_type(self);
	}
	
	return self->priv->hint_type;
}

const gchar*
ccm_window_get_name(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (self->priv->name == NULL)
	{
		guint32* data = NULL;
		guint n_items;
		
		data = ccm_window_get_property(self, 
							CCM_WINDOW_GET_CLASS(self)->visible_name_atom,
							8, CCM_WINDOW_GET_CLASS(self)->utf8_string_atom, 
						    &n_items);
		if (!data)
		{
			data = ccm_window_get_property(self, 
							CCM_WINDOW_GET_CLASS(self)->name_atom,
							8, CCM_WINDOW_GET_CLASS(self)->utf8_string_atom, 
						    &n_items);
		}
		
		if (!data)
		{
			data = ccm_window_get_property(self, 
							CCM_WINDOW_GET_CLASS(self)->name_atom,
							8, XA_WM_NAME, &n_items);
		}
			
		if (data)
		{
			self->priv->name = g_strndup((const gchar *)data, n_items);
			XFree(data);
		}
	}
	
	return self->priv->name;
}

gboolean
ccm_window_is_opaque(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->opaque != NULL;
}

void
ccm_window_add_alpha_region(CCMWindow* self, CCMRegion* region)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(region != NULL);
	
	if (self->priv->opaque)
	{
		ccm_region_subtract(self->priv->opaque, region);
		if (ccm_region_empty(self->priv->opaque))
		{
			ccm_region_destroy(self->priv->opaque);
			self->priv->opaque = NULL;
		}
	}
}

void
ccm_window_set_alpha(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->opaque)
	{
		ccm_region_destroy(self->priv->opaque);
		self->priv->opaque = NULL;
	}
}

void
ccm_window_set_opaque(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	CCMRegion* geometry = ccm_drawable_get_geometry(CCM_DRAWABLE(self));
	
	ccm_window_set_alpha(self);
	if (geometry)
	self->priv->opaque = ccm_region_copy(geometry);
}

CCMRegion*
ccm_window_get_opaque_region(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	return self->priv->opaque;
}

void
ccm_window_set_parent(CCMWindow* self, CCMWindow* parent)
{
	g_return_if_fail(self != NULL);
	
	self->priv->parent = parent;
	if (parent) 
	{
		cairo_rectangle_t geometry;
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
		self->priv->is_viewable = FALSE;
		
		ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self), &geometry);
		ccm_screen_damage_rectangle (screen, &geometry);
	}
}

gboolean
ccm_window_is_decorated(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, TRUE);
	
	return self->priv->is_decorated;
}
