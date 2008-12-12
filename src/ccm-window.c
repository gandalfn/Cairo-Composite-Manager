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

#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "ccm-debug.h"
#include "ccm-window.h"
#include "ccm-window-backend.h"
#include "ccm-window-plugin.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-pixmap.h"
#include "ccm-pixmap-backend.h"
#include "ccm-property-async.h"

#define MWM_HINTS_DECORATIONS (1L << 1)

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MotifWmHints;

static void 		ccm_window_iface_init		(CCMWindowPluginClass* iface);
static void			ccm_window_query_geometry	(CCMDrawable* drawable);
static void 		ccm_window_move				(CCMDrawable* drawable, 
												 int x, int y);
static void 		ccm_window_resize			(CCMDrawable* drawable, 
												 int width, int height);

static CCMRegion*	impl_ccm_window_query_geometry(CCMWindowPlugin* plugin,
												   CCMWindow* self);
static gboolean 	impl_ccm_window_paint		  (CCMWindowPlugin* plugin, 
												   CCMWindow* self, 
												   cairo_t* context,        
												   cairo_surface_t* surface,
												   gboolean y_invert);
static void 		impl_ccm_window_map			  (CCMWindowPlugin* plugin, 
												   CCMWindow* self);
static void 		impl_ccm_window_unmap		  (CCMWindowPlugin* plugin, 
												   CCMWindow* self);
static void 		impl_ccm_window_query_opacity (CCMWindowPlugin* plugin, 
												   CCMWindow* self);
static void			impl_ccm_window_move		  (CCMWindowPlugin* plugin, 
												   CCMWindow* self, 
												   int x, int y);
static void			impl_ccm_window_resize		  (CCMWindowPlugin* plugin, 
												   CCMWindow* self, 
												   int width, int height);
static void			impl_ccm_window_set_opaque_region(CCMWindowPlugin* plugin, 
													  CCMWindow* self,
													  const CCMRegion* area);
static void			impl_ccm_window_get_origin	  (CCMWindowPlugin* plugin, 
												   CCMWindow* self,
												   int* x, int* y);
static void			ccm_window_on_property_async_error(CCMWindow* self, 
													   CCMPropertyASync* prop);
static void			ccm_window_get_property_async (CCMWindow* self, 
												   Atom property_atom, 
												   Atom req_type, long length);
static void			ccm_window_on_get_property_async(CCMWindow* self, 
													 guint n_items, gchar* result, 
													 CCMPropertyASync* property);
static void			ccm_window_on_plugins_changed(CCMWindow* self, 
												  CCMScreen* screen);

enum
{
    PROP_0,
	PROP_CHILD,
	PROP_IMAGE,
	PROP_MASK
};

enum
{
	PROPERTY_CHANGED,
	OPACITY_CHANGED,
	ERROR,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_EXTENDED (CCMWindow, ccm_window, CCM_TYPE_DRAWABLE, 0,
						G_IMPLEMENT_INTERFACE(CCM_TYPE_WINDOW_PLUGIN,
											  ccm_window_iface_init))

struct _CCMWindowPrivate
{
	CCMWindowType		hint_type;
	gchar*				name;
	gchar*				class_name;
	
	Window				root;
	Window				child;
	Window				transient_for;
	Window				group_leader;
	
	cairo_rectangle_t   area;
	gboolean 			is_input_only;
	gboolean 			is_viewable;
	gboolean			visible;
	gboolean			unmap_pending;
	gboolean			is_shaped;
	gboolean			is_shaded;
	gboolean			is_fullscreen;
	gboolean			is_decorated;
	gboolean			is_modal;
	gboolean			skip_taskbar;
	gboolean			skip_pager;
	gboolean			keep_above;
	gboolean			keep_below;
	gboolean			override_redirect;
	int					frame_left;
	int					frame_right;
	int					frame_top;
	int					frame_bottom;
	
	GSList*				properties_pending;
	CCMPixmap*			pixmap;
	gboolean			use_pixmap_image;
	
	CCMWindowPlugin*	plugin;
	
	CCMRegion*			opaque;
	CCMRegion*			orig_opaque;
	double				opacity;
	cairo_matrix_t		transform;
	cairo_surface_t		*mask;
};

#define CCM_WINDOW_GET_PRIVATE(o)  \
   ((CCMWindowPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_WINDOW, CCMWindowClass))

static void
ccm_window_set_gobject_property(GObject *object,
								guint prop_id,
								const GValue *value,
								GParamSpec *pspec)
{
	CCMWindowPrivate* priv = CCM_WINDOW_GET_PRIVATE(object);
    
	switch (prop_id)
    {
    	case PROP_CHILD:
			priv->child = (Window)g_value_get_pointer (value);
			break;
		case PROP_IMAGE:
		{
			gboolean use_pixmap_image = g_value_get_boolean (value);
			if (use_pixmap_image != priv->use_pixmap_image)
			{
				priv->use_pixmap_image = use_pixmap_image;
				if (priv->pixmap)
				{
					g_object_unref(priv->pixmap);
					priv->pixmap = NULL;
				}
			}
			break;
		}
    	case PROP_MASK:
			if (priv->mask)
				cairo_surface_destroy(priv->mask);
			priv->mask = (cairo_surface_t*)g_value_get_pointer (value);
			break;
		default:
			break;
    }
}

static void
ccm_window_get_gobject_property (GObject* object,
								 guint prop_id,
								 GValue* value,
								 GParamSpec* pspec)
{
    CCMWindowPrivate* priv = CCM_WINDOW_GET_PRIVATE(object);
    
    switch (prop_id)
    {
    	case PROP_CHILD:
			g_value_set_pointer (value, (gpointer)priv->child);
			break;
		case PROP_MASK:
			g_value_set_pointer (value, (gpointer)priv->mask);
			break;
		default:
			break;
    }
}

static void
ccm_window_init (CCMWindow *self)
{
	self->priv = CCM_WINDOW_GET_PRIVATE(self);
	self->priv->hint_type = CCM_WINDOW_TYPE_NORMAL;
	self->priv->name = NULL;
	self->priv->class_name = NULL;
	self->priv->child = None;
	self->priv->transient_for = None;
	self->priv->group_leader = None;
	self->priv->area.x = 0;
	self->priv->area.y = 0;
	self->priv->area.width = 0;
	self->priv->area.height = 0;
	self->priv->is_input_only = FALSE;
	self->priv->visible = FALSE;
	self->priv->is_viewable = FALSE;
	self->priv->unmap_pending = FALSE;
	self->priv->is_shaped = FALSE;
	self->priv->is_shaded = FALSE;
	self->priv->is_fullscreen = FALSE;
	self->priv->is_decorated = TRUE;
	self->priv->is_modal = FALSE;
	self->priv->skip_taskbar = FALSE;
	self->priv->skip_pager = FALSE;
	self->priv->keep_above = FALSE;
	self->priv->keep_below = FALSE;
	self->priv->override_redirect = FALSE;
	self->priv->opaque = NULL;
	self->priv->orig_opaque = NULL;
	self->priv->opacity = 1.0f;
	self->priv->frame_left = 0;
	self->priv->frame_right = 0;
	self->priv->frame_top = 0;
	self->priv->frame_bottom = 0;
 	self->priv->properties_pending = NULL;
	self->priv->pixmap = NULL;
	self->priv->use_pixmap_image = FALSE;
	self->priv->plugin = NULL;
	self->priv->mask = NULL;
	cairo_matrix_init_identity (&self->priv->transform);
}

static void
ccm_window_finalize (GObject *object)
{
	CCMWindow* self = CCM_WINDOW(object);
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
	ccm_debug_window(self, "FINALIZE");
	g_signal_handlers_disconnect_by_func (screen, 
										  ccm_window_on_plugins_changed, 
										  self);
	if (self->priv->mask)
	{
		cairo_surface_destroy(self->priv->mask);
		self->priv->mask = NULL;
	}
	if (self->priv->opaque) 
	{
		ccm_region_destroy(self->priv->opaque);
		self->priv->opaque = NULL;
	}
	if (self->priv->orig_opaque) 
	{
		ccm_region_destroy(self->priv->orig_opaque);
		self->priv->orig_opaque = NULL;
	}
	if (self->priv->pixmap) 
	{
		g_object_unref(self->priv->pixmap);
		self->priv->pixmap = NULL;
	}
	if (self->priv->name) 
	{
		g_free(self->priv->name);
		self->priv->name = NULL;
	}
	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin)) 
	{
		g_object_unref(self->priv->plugin);
		self->priv->plugin = NULL;
	}
	if (self->priv->properties_pending)
	{
		g_slist_foreach (self->priv->properties_pending, (GFunc)g_object_unref, NULL);
		g_slist_free (self->priv->properties_pending);
		self->priv->properties_pending = NULL;
	}
	G_OBJECT_CLASS (ccm_window_parent_class)->finalize (object);
}

static void
ccm_window_class_init (CCMWindowClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMWindowPrivate));
	
	object_class->get_property = ccm_window_get_gobject_property;
    object_class->set_property = ccm_window_set_gobject_property;
	object_class->finalize = ccm_window_finalize;

	CCM_DRAWABLE_CLASS(klass)->query_geometry = ccm_window_query_geometry;
	CCM_DRAWABLE_CLASS(klass)->move = ccm_window_move;
	CCM_DRAWABLE_CLASS(klass)->resize = ccm_window_resize;
	
	g_object_class_install_property(object_class, PROP_CHILD,
		g_param_spec_pointer ("child",
		 					  "Child",
			     			  "Child of window",
			     			  G_PARAM_READWRITE));
	
	g_object_class_install_property(object_class, PROP_IMAGE,
		g_param_spec_boolean ("use_image",
		 					  "UseImage",
			     			  "Use image backend for pixmap",
							  FALSE,
		     				  G_PARAM_WRITABLE));
	
	g_object_class_install_property(object_class, PROP_MASK,
		g_param_spec_pointer ("mask",
		 					  "Mask",
			     			  "Window paint mask",
			     			  G_PARAM_READWRITE));
	
	signals[PROPERTY_CHANGED] = g_signal_new ("property-changed",
									 G_OBJECT_CLASS_TYPE (object_class),
									 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
									 g_cclosure_marshal_VOID__INT,
									 G_TYPE_NONE, 1, G_TYPE_INT);
	
	signals[OPACITY_CHANGED] = g_signal_new ("opacity-changed",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__VOID,
								   G_TYPE_NONE, 0);
	
	signals[ERROR] = g_signal_new ("error",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__VOID,
								   G_TYPE_NONE, 0);
}

static void
ccm_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = NULL;
	iface->query_geometry 	 = impl_ccm_window_query_geometry;
	iface->paint 			 = impl_ccm_window_paint;
	iface->map  			 = impl_ccm_window_map;
	iface->unmap  			 = impl_ccm_window_unmap;
	iface->query_opacity  	 = impl_ccm_window_query_opacity;
	iface->move				 = impl_ccm_window_move;
	iface->resize			 = impl_ccm_window_resize;
	iface->set_opaque_region = impl_ccm_window_set_opaque_region;
	iface->get_origin	     = impl_ccm_window_get_origin;
}

static void
create_atoms(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	CCMWindowClass* klass = CCM_WINDOW_GET_CLASS(self);
	if (!klass->opacity_atom)
	{
		CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		
		klass->atom  			 = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
												"ATOM", False);
		
		klass->none_atom  		 = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										 		"NONE", False);
		
		klass->utf8_string_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
										        "UTF8_STRING", 
											    False);
		klass->active_atom 	 	 = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
												"_NET_ACTIVE_WINDOW", 
												False);
		klass->user_time_atom 	 = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
												"_NET_WM_USER_TIME", 
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
		klass->client_list_atom  = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
									            "_NET_CLIENT_LIST", 
												False);
		klass->client_stacking_list_atom = XInternAtom (CCM_DISPLAY_XDISPLAY(display),
									            "_NET_CLIENT_LIST_STACKING", 
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
		klass->state_skip_taskbar      = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_SKIP_TASKBAR", 
											False);
		klass->state_skip_pager        = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_SKIP_PAGER", 
											False);
		klass->state_is_modal          = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_WM_STATE_MODAL", 
											False);
		klass->mwm_hints_atom          = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_MOTIF_WM_HINTS", 
											False);
		klass->frame_extends_atom      = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"_NET_FRAME_EXTENTS", 
											False);
		klass->transient_for_atom      = XInternAtom (
											CCM_DISPLAY_XDISPLAY(display),
											"WM_TRANSIENT_FOR", 
											False);
	}
}

static void
ccm_window_get_property_async(CCMWindow* self, Atom property_atom, 
							  Atom req_type, long length)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(property_atom != None);
	
    CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	CCMPropertyASync* property;
		
	property = ccm_property_async_new (display, CCM_WINDOW_XWINDOW(self), 
									   property_atom, req_type, length);
	g_signal_connect_swapped(property, "reply", 
							 G_CALLBACK(ccm_window_on_get_property_async), 
							 self);
	g_signal_connect_swapped(property, "error", 
							 G_CALLBACK(ccm_window_on_property_async_error), 
							 self);
	self->priv->properties_pending = 
			g_slist_append (self->priv->properties_pending, property);
		
	if (self->priv->child != None)
	{
		property = ccm_property_async_new (display, self->priv->child, 
									       property_atom, req_type, length);
		g_signal_connect_swapped(property, "reply", 
								 G_CALLBACK(ccm_window_on_get_property_async), 
								 self);
		g_signal_connect_swapped(property, "error", 
							 	 G_CALLBACK(ccm_window_on_property_async_error), 
								 self);
		self->priv->properties_pending = 
			g_slist_append (self->priv->properties_pending, property);
	}
}

static gchar*
ccm_window_get_utf8_property (CCMWindow* self, Atom atom)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(atom != None, NULL);
  
	gchar *val;
	guint32* data = NULL;
	guint n_items;
	
	data = ccm_window_get_property(self, atom, 
								   CCM_WINDOW_GET_CLASS(self)->utf8_string_atom, 
								   &n_items);
  
	if (!data) return NULL;
	
	if (!g_utf8_validate ((gchar*)data, n_items, NULL))
    {
		g_free (data);
		return NULL;
    }
  
  	val = g_strndup ((gchar*)data, n_items);
  
  	g_free (data);
  
  	return val;
}

static gchar*
ccm_window_get_child_utf8_property (CCMWindow* self, Atom atom)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(atom != None, NULL);
  
	gchar *val;
	guint32* data = NULL;
	guint n_items;
		
	data = ccm_window_get_child_property(self, atom,
								   CCM_WINDOW_GET_CLASS(self)->utf8_string_atom, 
								   &n_items);
  
	if (!data) return NULL;
	
	if (!g_utf8_validate ((gchar*)data, n_items, NULL))
    {
		XFree (data);
		return NULL;
    }
  
  	val = g_strndup ((gchar*)data, n_items);
  
  	g_free (data);
  
  	return val;
}

static gchar*
text_property_to_utf8 (const XTextProperty* prop)
{
	gchar **list;
	gint count;
	gchar *retval;
  
  	list = NULL;

  	count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
											prop->format,
											prop->value,
											prop->nitems,
											&list);

  	if (count == 0)
		retval = NULL;
	else
    {
      	retval = list[0];
      	list[0] = g_strdup (""); /* something to free */
    }

  	g_strfreev (list);

  	return retval;
}

static gchar*
ccm_window_get_text_property (CCMWindow* self, Atom atom)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(atom != None, NULL);
  
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
	XTextProperty text;
	gchar* retval = NULL;
  
  	text.nitems = 0;
  	if (XGetTextProperty(CCM_DISPLAY_XDISPLAY(display), 
						 CCM_WINDOW_XWINDOW(self), &text, atom))
    {
      	retval = text_property_to_utf8 (&text);

      	if (text.value) XFree (text.value);
    }
	
	return retval;
}

static gchar*
ccm_window_get_child_text_property (CCMWindow* self, Atom atom)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(atom != None, NULL);
  
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
	XTextProperty text;
	gchar* retval = NULL;
  
  	text.nitems = 0;
  	if (XGetTextProperty(CCM_DISPLAY_XDISPLAY(display), 
						 self->priv->child, &text, atom))
    {
      	retval = text_property_to_utf8 (&text);

      	if (text.value) XFree (text.value);
    }
	
	return retval;
}

static void
ccm_window_get_plugins(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	GSList* item, *plugins = NULL;
	
	g_object_get(G_OBJECT(screen), "window_plugins", &plugins, NULL);
	
	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin)) 
		g_object_unref(self->priv->plugin);
	
	self->priv->plugin = (CCMWindowPlugin*)self;
	
	for (item = plugins; item; item = item->next)
	{
		GType type = GPOINTER_TO_INT(item->data);
		GObject* prev = G_OBJECT(self->priv->plugin);
		CCMWindowPlugin* plugin = g_object_new(type, "parent", prev, NULL);
		
		if (plugin) self->priv->plugin = plugin;
	}
	g_slist_free(plugins);
	
	ccm_window_plugin_load_options(self->priv->plugin, self);
}

static void
ccm_window_query_child(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	Window* windows = NULL, w, p;
	guint n_windows;
	
	if (CCM_WINDOW_XWINDOW(self) == RootWindowOfScreen(CCM_SCREEN_XSCREEN(screen)))
		return;
	
	if (!self->priv->override_redirect &&
		XQueryTree(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(self), 
				   &w, &p, &windows, &n_windows) && n_windows > 0)
	{
		CCMWindow* root = ccm_screen_get_root_window (screen);
		guint n_managed = 0;
		Window* managed = (Window *)ccm_window_get_property (root, 
						CCM_WINDOW_GET_CLASS(root)->client_stacking_list_atom,
						XA_WINDOW, &n_managed);
		
		if (managed && n_managed)
		{
			guint i, j;
			gboolean found = FALSE;
			
			for (i = 0; !found && i < n_managed; i++)
			{
				for (j = 0; !found && j < n_windows; j++)
				{
					if (windows[j] && managed[i] == windows[j])
					{
						self->priv->child = windows[j];
						ccm_debug_window(self, "FOUND CHILD 0x%lx", self->priv->child);
						XSelectInput (CCM_DISPLAY_XDISPLAY(display), 
									  self->priv->child, 
									  PropertyChangeMask | 
									  StructureNotifyMask);
						found = TRUE;
					}
				}
			}
		}
		if (managed) g_free(managed);
		if (!self->priv->child) 
		{
			self->priv->child = windows[n_windows - 1];
			XSelectInput (CCM_DISPLAY_XDISPLAY(display), 
									  self->priv->child, 
									  PropertyChangeMask | 
									  StructureNotifyMask);
			ccm_debug_window(self, "FOUND DEFAULT CHILD 0x%lx", self->priv->child);
		}
	}
	if (windows) XFree(windows);
}

static gboolean
ccm_window_get_attribs (CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	XWindowAttributes attribs;
	
	if (!XGetWindowAttributes (CCM_DISPLAY_XDISPLAY(display), 
							   CCM_WINDOW_XWINDOW(self), &attribs))
		return FALSE;
	
	self->priv->root = attribs.root;
	
	self->priv->area.x = attribs.x - attribs.border_width;
	self->priv->area.y = attribs.y - attribs.border_width;
	self->priv->area.width = attribs.width + (attribs.border_width * 2);
	self->priv->area.height = attribs.height + (attribs.border_width * 2);
	
	g_object_set(self, "visual", attribs.visual, NULL);
	g_object_set(self, "depth", attribs.depth, NULL);
	
	self->priv->is_viewable = attribs.map_state == IsViewable;
	self->priv->is_input_only = attribs.class == InputOnly;
	self->priv->override_redirect = attribs.override_redirect;
	if (self->priv->override_redirect && self->priv->child)
		self->priv->child = None;
	
	return TRUE;
}

static void
ccm_window_query_geometry(CCMDrawable* drawable)
{
	g_return_if_fail(drawable != NULL);

	CCMWindow* self = CCM_WINDOW(drawable);
	CCMRegion* geometry = NULL;
	
	geometry = ccm_window_plugin_query_geometry(self->priv->plugin, self);
	
	g_object_set(self, "geometry", geometry, NULL);
	
	if (geometry) ccm_region_destroy (geometry);
}

static void
ccm_window_move(CCMDrawable* drawable, int x, int y)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindow* self = CCM_WINDOW(drawable);

	ccm_window_plugin_move(self->priv->plugin, self, x , y);
}

static void
ccm_window_resize(CCMDrawable* drawable, int width, int height)
{
	g_return_if_fail(drawable != NULL);
	
	CCMWindow* self = CCM_WINDOW(drawable);

	ccm_window_plugin_resize(self->priv->plugin, self, width, height);
}

static CCMRegion*
impl_ccm_window_query_geometry(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMRegion* geometry = NULL;
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	int bx, by, cs, cx, cy, o; /* dummies */
	unsigned int bw, bh, cw, ch; /* dummies */
	
	if (!ccm_window_get_attribs(self))
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
			ccm_region_offset(geometry, 
							  self->priv->area.x, self->priv->area.y);
		}
		else
			self->priv->is_shaped = FALSE;
		XFree(shapes);
	}
	
	if (!self->priv->is_shaped)
	{
		geometry = ccm_region_rectangle(&self->priv->area);
	}
	
	ccm_window_set_alpha(self);
	if (geometry &&
		ccm_drawable_get_format(CCM_DRAWABLE(self)) != CAIRO_FORMAT_ARGB32 && 
		self->priv->opacity == 1.0f)
	{
		CCMRegion* area = ccm_region_copy(geometry);
		ccm_window_set_opaque_region (self, area);
		ccm_region_destroy (area);
	}
	
	if (geometry)
	{
		cairo_rectangle_t clipbox;
		CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(self));
		
		ccm_region_get_clipbox (geometry, &clipbox);
		if (clipbox.x <= 0 && clipbox.y <= 0 && 
			clipbox.width >= CCM_SCREEN_XSCREEN(screen)->width &&
			clipbox.height >= CCM_SCREEN_XSCREEN(screen)->height &&
			!self->priv->is_fullscreen)
		{
			self->priv->is_fullscreen = TRUE;
			g_signal_emit(self, signals[PROPERTY_CHANGED], 0, 
						  CCM_PROPERTY_STATE);
		}
		else if (self->priv->is_fullscreen)
		{
			self->priv->is_fullscreen = FALSE;
			g_signal_emit(self, signals[PROPERTY_CHANGED], 0, 
						  CCM_PROPERTY_STATE);
		}
	}

	if (self->priv->pixmap)
	{
		g_object_unref(self->priv->pixmap);
		self->priv->pixmap = NULL;
	}
	
	return geometry;
}

static void
impl_ccm_window_move(CCMWindowPlugin* plugin, CCMWindow* self, int x, int y)
{
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry) &&
		(x != (int)geometry.x || y != (int)geometry.y))
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
		CCMRegion* old_geometry = ccm_region_rectangle (&geometry);
		
		if (screen &&
			self->priv->is_viewable && !self->priv->is_input_only && 
			self->priv->is_decorated && !self->priv->override_redirect) 
			g_object_set(G_OBJECT(screen), "buffered_pixmap", TRUE, NULL);
		
		CCM_DRAWABLE_CLASS(ccm_window_parent_class)->move(CCM_DRAWABLE(self), 
														  x, y);
		if (self->priv->opaque)
			ccm_region_offset(self->priv->opaque, 
							  x - geometry.x, y - geometry.y);
		if (self->priv->orig_opaque)
			ccm_region_offset(self->priv->orig_opaque, 
							  x - geometry.x, y - geometry.y);
		if ((self->priv->is_viewable || self->priv->unmap_pending) &&
			ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry))
		{
			ccm_region_union_with_rect (old_geometry, &geometry);
			ccm_drawable_damage_region (CCM_DRAWABLE(self), old_geometry);
		}
		ccm_region_destroy (old_geometry);
		self->priv->area.x = x;
		self->priv->area.y = y;
	}
}

static void
impl_ccm_window_resize(CCMWindowPlugin* plugin, CCMWindow* self, 
					   int width, int height)
{
	cairo_rectangle_t geometry;
	
	if (!ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry) ||
		width != (int)geometry.width || height != (int)geometry.height)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
		CCMRegion* old_geometry = ccm_region_rectangle (&geometry);
		
		ccm_debug_window(self, "RESIZE %i,%i", width, height);
		
		CCM_DRAWABLE_CLASS(ccm_window_parent_class)->resize(CCM_DRAWABLE(self), 
															width, height);
		
		if (self->priv->opaque)
			ccm_region_offset(self->priv->opaque, width, height);
		if (self->priv->orig_opaque)
			ccm_region_offset(self->priv->orig_opaque, width, height);
		
		if (self->priv->hint_type != CCM_WINDOW_TYPE_DESKTOP &&
			(ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry) &&
			 ((geometry.x <= 0 && geometry.y <= 0 && 
			 geometry.width >= CCM_SCREEN_XSCREEN(screen)->width &&
			 geometry.height >= CCM_SCREEN_XSCREEN(screen)->height) || 
			 !self->priv->is_fullscreen)))
		{
			ccm_window_switch_state (self, CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom);
			ccm_screen_damage (screen);
			g_signal_emit(self, signals[PROPERTY_CHANGED], 0, 
						  CCM_PROPERTY_STATE);
		}
		else
		{
			ccm_drawable_damage_region (CCM_DRAWABLE(self), old_geometry);
			ccm_drawable_damage (CCM_DRAWABLE(self));
		}
		if (old_geometry) ccm_region_destroy (old_geometry);
		
		if (self->priv->pixmap)
		{
			g_object_unref(self->priv->pixmap);
			self->priv->pixmap = NULL;
		}
	}
}

static gboolean
impl_ccm_window_paint(CCMWindowPlugin* plugin, CCMWindow* self, 
					  cairo_t* context, cairo_surface_t* surface, 
					  gboolean y_invert)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	gboolean ret = FALSE;
	
	if (self->priv->opacity == 0.0f) return TRUE;
	
	cairo_save(context);
	ccm_debug_window(self, "PAINT WINDOW %f", self->priv->opacity);
	if (ccm_window_transform (self, context, y_invert))
	{
		cairo_set_source_surface(context, surface, 0.0f, 0.0f);
		if (self->priv->mask)
			cairo_mask_surface(context, self->priv->mask, 0, 0);
		else
			cairo_paint_with_alpha(context, self->priv->opacity);
		
		if (cairo_status (context) != CAIRO_STATUS_SUCCESS)
		{
			ccm_debug_window(self, "PAINT ERROR %i", cairo_status (context));
			g_object_unref(self->priv->pixmap);
			self->priv->pixmap = NULL;
			g_signal_emit (self, signals[ERROR], 0);
			ret = FALSE;
		}
		else
			ret = TRUE;
	}
	else
		ret = FALSE;
	ccm_debug_window(self, "PAINT WINDOW %i", ret);
	cairo_restore(context);
	
	return ret;
}

static void
impl_ccm_window_map(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);

	ccm_debug_window(self, "IMPL WINDOW MAP");
	ccm_drawable_damage(CCM_DRAWABLE(self));
}

static void
impl_ccm_window_unmap(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	
	const CCMRegion* geometry = ccm_drawable_get_geometry (CCM_DRAWABLE(self));
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	
	self->priv->is_viewable = FALSE;
	self->priv->visible = FALSE;
	self->priv->unmap_pending = FALSE;
	ccm_debug_window(self, "IMPL WINDOW UNMAP");
	if (self->priv->pixmap)
	{
		g_object_unref(self->priv->pixmap);
		self->priv->pixmap = NULL;
	}
	if (geometry)
		ccm_screen_damage_region (screen, geometry);
	else
		ccm_screen_damage (screen);
}
	
static void
impl_ccm_window_query_opacity(CCMWindowPlugin* plugin, CCMWindow* self)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);

	ccm_debug_window(self, "QUERY OPACITY");
	ccm_window_get_property_async(self, CCM_WINDOW_GET_CLASS(self)->opacity_atom,
								  XA_CARDINAL, 32);
}

static void
impl_ccm_window_set_opaque_region(CCMWindowPlugin* plugin, CCMWindow* self,
								  const CCMRegion* area)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);

	if (!ccm_region_empty((CCMRegion*)area))
	{
		ccm_window_set_alpha(self);
		self->priv->orig_opaque = ccm_region_copy((CCMRegion*)area);
		self->priv->opaque = ccm_region_copy((CCMRegion*)area);
		ccm_region_device_transform (self->priv->opaque, &self->priv->transform);
	}
}

static void
impl_ccm_window_get_origin(CCMWindowPlugin* plugin, CCMWindow* self,
						   int* x, int* y)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	g_return_if_fail(x != NULL && y != NULL);
	
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox(CCM_DRAWABLE(self), &geometry))
	{
		*x = geometry.x;
		*y = geometry.y;
	}
	else
	{
		*x = 0;
		*y = 0;
	}
}
				
static void
ccm_window_on_pixmap_damaged(CCMWindow* self, CCMRegion* area)
{
	g_return_if_fail (self != NULL);
    g_return_if_fail (area != NULL);
	
	int x, y;
	
	ccm_window_plugin_get_origin(self->priv->plugin, self, &x, &y);
	ccm_region_offset(area, x, y);
	ccm_drawable_damage_region(CCM_DRAWABLE(self), area);
}

static void
ccm_window_on_property_async_error(CCMWindow* self, CCMPropertyASync* prop)
{
	self->priv->properties_pending = 
			g_slist_remove (self->priv->properties_pending, prop);
	g_object_unref(prop);
}

static void
ccm_window_on_get_property_async(CCMWindow* self, guint n_items, gchar* result, 
								 CCMPropertyASync* prop)
{
	if (!CCM_IS_WINDOW(self)) 
	{
		self->priv->properties_pending = 
			g_slist_remove (self->priv->properties_pending, prop);
		g_object_unref(prop);
		return;
	}
	
	g_return_if_fail(CCM_IS_PROPERTY_ASYNC(prop));
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	Atom property = ccm_property_async_get_property (prop);
		
	if (property == CCM_WINDOW_GET_CLASS(self)->type_atom)
	{
		if (result)
		{
			CCMWindowType old = self->priv->hint_type;
			Atom atom;
			memcpy (&atom, result, sizeof (Atom));
			
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
			
			if (old != self->priv->hint_type)
			{
				g_signal_emit(self, signals[PROPERTY_CHANGED], 0, 
							  CCM_PROPERTY_HINT_TYPE);
			}
		}
	}
	else if (property == CCM_WINDOW_GET_CLASS(self)->transient_for_atom)
	{
		gboolean updated = FALSE;
		
		if (result)
		{
			Window old = self->priv->transient_for;
			
			memcpy (&self->priv->transient_for, result, sizeof (Window));
			updated = old != self->priv->transient_for;
			
			if (self->priv->transient_for != None && 
				self->priv->hint_type == CCM_WINDOW_TYPE_NORMAL)
			{
				self->priv->hint_type = CCM_WINDOW_TYPE_DIALOG;
				updated = TRUE;
			}
		}
		if (updated)
		{
			ccm_debug_window(self, "TRANSIENT FOR 0x%lx", self->priv->transient_for);
			g_signal_emit(self, signals[PROPERTY_CHANGED], 0, 
						  CCM_PROPERTY_TRANSIENT);
		}
	}
	else if (property == CCM_WINDOW_GET_CLASS(self)->mwm_hints_atom)
	{
		if (result)
		{
			MotifWmHints* hints;
			gboolean old = self->priv->is_decorated;
		
			hints = (MotifWmHints*)result;
		
			if (hints->flags & MWM_HINTS_DECORATIONS)
				self->priv->is_decorated = hints->decorations != 0;
			if (old != self->priv->is_decorated)
			{
				ccm_debug_window(self, "IS_DECORATED %i", self->priv->is_decorated);
				g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
							  CCM_PROPERTY_MWM_HINTS);
			}
		}
	}
	else if (property == CCM_WINDOW_GET_CLASS(self)->state_atom)
	{
		if (result) 
		{
			Atom *atom = (Atom *) result;
			gulong cpt;
			gboolean updated = FALSE;
			
			for (cpt = 0; cpt < n_items; cpt++)
			{
				updated |= ccm_window_set_state (self, atom[cpt]);
			}
			if (updated) 
			{
				g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
							  CCM_PROPERTY_STATE);
			}
		}
	}
	else if (property == CCM_WINDOW_GET_CLASS(self)->opacity_atom)
	{
		if (result) 
		{
			gdouble old = self->priv->opacity;
			guint32 value;
			memcpy (&value, result, sizeof (guint32));
			self->priv->opacity = (double)value/ (double)0xffffffff;
			
			if (self->priv->opacity == 1.0f && 
				ccm_drawable_get_format(CCM_DRAWABLE(self)) != CAIRO_FORMAT_ARGB32 &&
				!self->priv->opaque)
				ccm_window_set_opaque(self);
			else
				ccm_window_set_alpha(self);
			if (old != self->priv->opacity)
			{
				ccm_drawable_damage (CCM_DRAWABLE(self));
				g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
							  CCM_PROPERTY_OPACITY);
			}
		}
	}		

	self->priv->properties_pending = 
			g_slist_remove (self->priv->properties_pending, prop);
	g_object_unref(prop);
}
								 
static void
ccm_window_on_plugins_changed(CCMWindow* self, CCMScreen* screen)
{
	ccm_window_get_plugins (self);
	ccm_drawable_damage (CCM_DRAWABLE(self));
}

CCMWindowPlugin*
_ccm_window_get_plugin(CCMWindow *self, GType type)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMWindowPlugin* plugin;
	
	for (plugin = self->priv->plugin; plugin != (CCMWindowPlugin*)self; plugin = CCM_WINDOW_PLUGIN_PARENT(plugin))
	{
		if (g_type_is_a(G_OBJECT_TYPE(plugin), type))
			return plugin;
	}
	
	return NULL;
}

/**
 * ccm_window_new:
 * @screen: #CCMScreen of window
 * @xwindow: window xid
 *
 * Create a new #CCMWindow reference which point on @xwindow
 *
 * Returns: #CCMWindow
 **/
CCMWindow *
ccm_window_new (CCMScreen* screen, Window xwindow)
{
	g_return_val_if_fail(screen != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	CCMWindow* self = g_object_new(CCM_TYPE_WINDOW_BACKEND(screen), 
								   "screen", screen,
								   "drawable", xwindow,
								   NULL);

	create_atoms(self);
	
	ccm_window_get_plugins (self);
	if (!ccm_window_get_attribs (self))
	{
		g_object_unref(self);
		return NULL;
	}
	
	if (!self->priv->is_input_only)
	    ccm_drawable_query_geometry(CCM_DRAWABLE(self));
	
	if (self->priv->root != None && 
		xwindow != RootWindowOfScreen(CCM_SCREEN_XSCREEN(screen)) &&
		self->priv->root != RootWindowOfScreen(CCM_SCREEN_XSCREEN(screen)))
	{
		g_object_unref(self);
		return NULL;
	}
	
	g_signal_connect_swapped(screen, "plugins-changed", 
							 G_CALLBACK(ccm_window_on_plugins_changed), self);
	
	ccm_window_set_opacity(self, 1.0f);
	ccm_window_init_transfrom (self);
	
	if (!self->priv->is_input_only)
	{
		CCMDisplay* display = ccm_screen_get_display(screen);
	
		ccm_window_query_child (self);
		ccm_window_query_hint_type (self);
		ccm_window_query_opacity (self, FALSE);
		ccm_window_query_transient_for(self);
		ccm_window_query_wm_hints(self);
		ccm_window_query_mwm_hints(self);
		ccm_window_query_state (self);
		ccm_window_query_frame_extends (self);
		
		XSelectInput (CCM_DISPLAY_XDISPLAY(display), 
					  CCM_WINDOW_XWINDOW(self),
					  PropertyChangeMask  | 
					  StructureNotifyMask |
					  SubstructureNotifyMask);
		ccm_display_sync(display);
	}
	
	return self;
}

/**
 * ccm_window_is_viewable:
 * @self: #CCMWindow
 *
 * Indicate if window is visible
 *
 * Returns: window is visible
 **/
gboolean
ccm_window_is_viewable (CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->is_viewable || self->priv->unmap_pending;
}

/**
 * ccm_window_is_input_only:
 * @self: #CCMWindow
 *
 * Indicate if window is input only
 *
 * Returns: window is input only
 **/
gboolean
ccm_window_is_input_only (CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->is_input_only;
}

/**
 * ccm_window_is_managed:
 * @self: #CCMWindow
 *
 * Indicate if window is managed by WM
 *
 * Returns: window is managed
 **/
gboolean
ccm_window_is_managed(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	return !self->priv->override_redirect;
}

/**
 * ccm_window_get_opaque_region:
 * @self: #CCMWindow
 *
 * Get opaque region of window
 *
 * Returns: #CCMRegion
 **/
const CCMRegion*
ccm_window_get_opaque_region(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);

	return self->priv->opaque;
}

/**
 * ccm_window_get_opaque_clipbox:
 * @self: #CCMWindow
 * @clipbox: #cairo_rectangle_t
 *
 * Get opaque clipbox of window
 *
 * Returns: TRUE if window have opaque region
 **/
gboolean
ccm_window_get_opaque_clipbox(CCMWindow* self, cairo_rectangle_t* clipbox)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	if (self->priv->opaque && !ccm_region_empty(self->priv->opaque))
	{
		ccm_region_get_clipbox (self->priv->opaque, clipbox);
		return TRUE;
	}
	return FALSE;
}

void
ccm_window_query_state(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	ccm_window_get_property_async(self, CCM_WINDOW_GET_CLASS(self)->state_atom,
								  XA_ATOM, sizeof(Atom));
}

gboolean
ccm_window_set_state(CCMWindow* self, Atom state_atom)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL, FALSE);
	
	gboolean updated = FALSE;
	
	if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_shade_atom)
	{
		gboolean old = self->priv->is_shaded;
		self->priv->is_shaded = TRUE;
		updated = old != self->priv->is_shaded;
		ccm_debug_window(self, "IS_SHADED %i", self->priv->is_shaded);
		if (updated) ccm_drawable_damage (CCM_DRAWABLE(self));
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom)
	{
		gboolean old = self->priv->is_fullscreen;
		
		self->priv->is_fullscreen = TRUE;
		updated = old != self->priv->is_fullscreen;
		ccm_debug_window(self, "IS_FULLSCREEN %i", self->priv->is_fullscreen);
		if (updated) 
		{
			CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
			ccm_drawable_query_geometry (CCM_DRAWABLE(self));
			ccm_screen_damage (screen);
		}
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_is_modal)
	{
		gboolean old = self->priv->is_modal;
		self->priv->is_modal = TRUE;
		updated = old != self->priv->is_modal;
		ccm_debug_window(self, "IS_MODAL %i", self->priv->is_modal);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_skip_taskbar)
	{
		gboolean old = self->priv->skip_taskbar;
		self->priv->skip_taskbar = TRUE;
		updated = old != self->priv->skip_taskbar;
		ccm_debug_window(self, "SKIP_TASKBAR %i", self->priv->skip_taskbar);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_skip_pager)
	{
		gboolean old = self->priv->skip_pager;
		self->priv->skip_pager = TRUE;
		updated = old != self->priv->skip_pager;
		ccm_debug_window(self, "SKIP_PAGER %i", self->priv->skip_pager);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_above_atom)
	{
		gboolean old = self->priv->keep_above;
		self->priv->keep_above = TRUE;
		updated = old != self->priv->keep_above;
		ccm_debug_window(self, "KEEP_ABOVE %i", self->priv->keep_above);
		if (updated) ccm_drawable_damage (CCM_DRAWABLE(self));
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_below_atom)
	{
		gboolean old = self->priv->keep_below;
		self->priv->keep_below = TRUE;
		updated = old != self->priv->keep_below;
		ccm_debug_window(self, "KEEP_BELOW %i", self->priv->keep_below);
	}
	
	return updated;
}

void
ccm_window_unset_state(CCMWindow* self, Atom state_atom)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	gboolean updated = FALSE;
	
	if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_shade_atom)
	{
		gboolean old = self->priv->is_shaded;
		self->priv->is_shaded = FALSE;
		updated = old != self->priv->is_shaded;
		ccm_debug_window(self, "IS_SHADED %i", self->priv->is_shaded);
		if (updated) ccm_drawable_damage (CCM_DRAWABLE(self));
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom)
	{
		gboolean old = self->priv->is_fullscreen;
		self->priv->is_fullscreen = FALSE;
		updated = old != self->priv->is_fullscreen;
		ccm_debug_window(self, "IS_FULLSCREEN %i", self->priv->is_fullscreen);
		if (updated) 
		{
			CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
			ccm_drawable_query_geometry (CCM_DRAWABLE(self));
			ccm_screen_damage (screen);
		}
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_is_modal)
	{
		gboolean old = self->priv->is_modal;
		self->priv->is_modal = FALSE;
		updated = old != self->priv->is_modal;
		ccm_debug_window(self, "IS_MODAL %i", self->priv->is_modal);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_skip_taskbar)
	{
		gboolean old = self->priv->skip_taskbar;
		self->priv->skip_taskbar = FALSE;
		updated = old != self->priv->skip_taskbar;
		ccm_debug_window(self, "SKIP_TASKBAR %i", self->priv->skip_taskbar);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_skip_pager)
	{
		gboolean old = self->priv->skip_pager;
		self->priv->skip_pager = FALSE;
		updated = old != self->priv->skip_pager;
		ccm_debug_window(self, "SKIP_PAGER %i", self->priv->skip_pager);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_above_atom)
	{
		gboolean old = self->priv->keep_above;
		self->priv->keep_above = FALSE;
		updated = old != self->priv->keep_above;
		ccm_debug_window(self, "KEEP_ABOVE %i", self->priv->keep_above);
		if (updated) ccm_drawable_damage (CCM_DRAWABLE(self));
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_below_atom)
	{
		gboolean old = self->priv->keep_below;
		self->priv->keep_below = FALSE;
		updated = old != self->priv->keep_below;
		ccm_debug_window(self, "KEEP_BELOW %i", self->priv->keep_below);
	}
	if (updated) g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
							   CCM_PROPERTY_STATE);
}

void
ccm_window_switch_state(CCMWindow* self, Atom state_atom)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	gboolean updated = FALSE;
	
	if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_shade_atom)
	{
		updated = TRUE;
		self->priv->is_shaded = !self->priv->is_shaded;
		ccm_debug_window(self, "IS_SHADED %i", self->priv->is_shaded);
		ccm_drawable_damage (CCM_DRAWABLE(self));
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom)
	{
		CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
		updated = TRUE;
		self->priv->is_fullscreen = !self->priv->is_fullscreen;
		ccm_debug_window(self, "IS_FULLSCREEN %i", self->priv->is_fullscreen);
		ccm_drawable_query_geometry (CCM_DRAWABLE(self));
		ccm_screen_damage (screen);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_is_modal)
	{
		updated = TRUE;
		self->priv->is_modal = !self->priv->is_modal;
		ccm_debug_window(self, "IS_MODAL %i", self->priv->is_modal);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_skip_taskbar)
	{
		updated = TRUE;
		self->priv->skip_taskbar = !self->priv->skip_taskbar;
		ccm_debug_window(self, "SKIP_TASKBAR %i", self->priv->skip_taskbar);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_skip_pager)
	{
		updated = TRUE;
		self->priv->skip_pager = !self->priv->skip_pager;
		ccm_debug_window(self, "SKIP_PAGER %i", self->priv->skip_pager);
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_above_atom)
	{
		updated = TRUE;
		self->priv->keep_above = !self->priv->keep_above;
		ccm_debug_window(self, "KEEP_ABOVE %i", self->priv->keep_above);
		ccm_drawable_damage (CCM_DRAWABLE(self));
	}
	else if (state_atom == CCM_WINDOW_GET_CLASS(self)->state_below_atom)
	{
		updated = TRUE;
		self->priv->keep_below = !self->priv->keep_below;
		ccm_debug_window(self, "KEEP_BELOW %i", self->priv->keep_below);
	}
	
	if (updated) g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
							   CCM_PROPERTY_STATE);
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

gboolean
ccm_window_skip_taskbar(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->skip_taskbar;
}

gboolean
ccm_window_skip_pager(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->skip_pager;
}

gboolean
ccm_window_keep_above(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->keep_above;
}

gboolean
ccm_window_keep_below(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	return self->priv->keep_below;
}

gboolean
ccm_window_is_child(CCMWindow* self, Window window)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self != None, FALSE);
	
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
	Window* windows = NULL, w, p;
	guint n_windows, cpt;
	gboolean ret  = FALSE;
	
	if (XQueryTree(CCM_DISPLAY_XDISPLAY(display), 
			   CCM_WINDOW_XWINDOW(self), &w, &p, 
			   &windows, &n_windows) && windows)
	{
		for (cpt = 0; cpt < n_windows; cpt++)
		{
			if (windows[cpt] == window) 
			{
				ret = TRUE;
				break;
			}
		}
		XFree(windows);
	}
	
	return ret;
}

CCMWindow*
ccm_window_transient_for(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	CCMWindow* root = ccm_screen_get_root_window (screen);
	CCMWindow* window = NULL;
	
	if (self->priv->transient_for)
		window = ccm_screen_find_window_or_child (screen,
												  self->priv->transient_for);
	else if (self->priv->transient_for == CCM_WINDOW_XWINDOW(root))
		window = root;
	
	return window;
}

CCMWindow*
ccm_window_get_group_leader(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMScreen* screen = ccm_drawable_get_screen (CCM_DRAWABLE(self));
	CCMWindow* window = NULL;
	
	if (self->priv->group_leader)
		window = ccm_screen_find_window_or_child (screen,
												  self->priv->group_leader);
	
	return window;
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
	XFixesDestroyRegion(CCM_DISPLAY_XDISPLAY(display), region);
}

void
ccm_window_make_input_output(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	CCMDisplay* display = ccm_drawable_get_display(CCM_DRAWABLE(self));
	XserverRegion region;
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self), &geometry))
	{
		XRectangle rect;
		rect.x = geometry.x;
		rect.y = geometry.y;
		rect.width = geometry.width;
		rect.width = geometry.height;
		region = XFixesCreateRegion(CCM_DISPLAY_XDISPLAY(display), 
									&rect, 1);
		XFixesSetWindowShapeRegion (CCM_DISPLAY_XDISPLAY(display),
									CCM_WINDOW_XWINDOW(self),
									ShapeInput, 0, 0, 
									region);
		XFixesDestroyRegion(CCM_DISPLAY_XDISPLAY(display), region);
	}
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

CCMPixmap*
ccm_window_get_pixmap(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	if (!self->priv->pixmap)
	{
		CCMDisplay *display = ccm_drawable_get_display(CCM_DRAWABLE(self));
		Pixmap xpixmap;
		
		ccm_display_sync(display);
		ccm_display_trap_error (display);
		ccm_display_grab(display);
		xpixmap = XCompositeNameWindowPixmap(CCM_DISPLAY_XDISPLAY(display),
											 CCM_WINDOW_XWINDOW(self));
		ccm_display_ungrab(display);
		ccm_display_sync(display);
		if (xpixmap && !ccm_display_pop_error (display))
		{
			if (self->priv->use_pixmap_image)
			{
				self->priv->pixmap = ccm_pixmap_image_new(CCM_DRAWABLE(self), 
														  xpixmap);
			}
			else 
			{
				self->priv->pixmap = ccm_pixmap_new(CCM_DRAWABLE(self), 
													xpixmap);
			}
			if (self->priv->pixmap)
				g_signal_connect_swapped(self->priv->pixmap, "damaged", 
										 G_CALLBACK(ccm_window_on_pixmap_damaged), 
										 self);
		}
	}
	
	return self->priv->pixmap ? g_object_ref(self->priv->pixmap) : NULL;
}

CCMPixmap*
ccm_window_create_pixmap(CCMWindow* self, int width, int height, int depth)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	CCMPixmap* pixmap = NULL;
	
	if (CCM_WINDOW_GET_CLASS(self)->create_pixmap)
		pixmap = CCM_WINDOW_GET_CLASS(self)->create_pixmap(self, width, 
														   height, depth);
	   
	return pixmap;
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
	else if (ccm_drawable_get_format (CCM_DRAWABLE(self)) != CAIRO_FORMAT_ARGB32)
		ccm_window_set_opaque(self);
	g_signal_emit(self, signals[OPACITY_CHANGED], 0);
}

gboolean
ccm_window_paint (CCMWindow* self, cairo_t* context, gboolean buffered)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(context != NULL, FALSE);
	
	gboolean ret = FALSE;
	
	if (!self->priv->is_viewable && !self->priv->unmap_pending &&
		!self->priv->is_shaded)
	{
		ccm_drawable_repair(CCM_DRAWABLE(self));
		return ret;
	}
	
	if (ccm_drawable_is_damaged(CCM_DRAWABLE(self)))
	{
		CCMPixmap* pixmap = ccm_window_get_pixmap(self);
		
		if (pixmap)
		{
			cairo_surface_t* surface;
			gboolean y_invert;
			
			g_object_get (pixmap, "y_invert", &y_invert, NULL);
			
			if (CCM_IS_PIXMAP_BUFFERED(pixmap))
				g_object_set(pixmap, "buffered", buffered, NULL);
			
			surface = ccm_drawable_get_surface(CCM_DRAWABLE(pixmap));
				
			if (surface)
			{
				ccm_debug_window(self, "PAINT");
				cairo_path_t* damaged;
			
				cairo_save(context);
				damaged = ccm_drawable_get_damage_path(CCM_DRAWABLE(self), 
													   context);
				cairo_clip(context);
				cairo_path_destroy(damaged);
				ret = ccm_window_plugin_paint(self->priv->plugin, self, 
											  context, surface, y_invert);
				cairo_surface_destroy(surface);
				cairo_restore(context);
			}
			else
			{
				g_object_unref(self->priv->pixmap);
				self->priv->pixmap = NULL;
				g_signal_emit (self, signals[ERROR], 0);
			}
			g_object_unref(pixmap);
		}
		
	}
	
	if (ret) ccm_drawable_repair(CCM_DRAWABLE(self));
	
	return ret;
}

void
ccm_window_map(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	if (!self->priv->visible)
	{
		self->priv->visible = TRUE;
		self->priv->is_viewable = TRUE;
		self->priv->unmap_pending = FALSE;
		
		ccm_debug_window(self, "WINDOW MAP");
		ccm_window_plugin_map(self->priv->plugin, self);
	}
}

void
ccm_window_unmap(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	if (self->priv->visible)
	{
		self->priv->visible = FALSE;
		self->priv->is_viewable = FALSE;
		self->priv->unmap_pending = TRUE;
		
		if (self->priv->is_fullscreen)
			ccm_window_switch_state (self, CCM_WINDOW_GET_CLASS(self)->state_fullscreen_atom);
		if (self->priv->pixmap)
			g_object_set(self->priv->pixmap, "freeze", TRUE, NULL);
		ccm_debug_window(self, "WINDOW UNMAP");
		ccm_window_plugin_unmap(self->priv->plugin, self);
	}
}

void 
ccm_window_query_opacity(CCMWindow* self, gboolean deleted)
{
	g_return_if_fail(self != NULL);
	
	if (deleted)
	{
		ccm_window_set_opacity (self, 1.0f);
		ccm_drawable_damage (CCM_DRAWABLE(self));
		g_signal_emit (self, signals[PROPERTY_CHANGED], 0, 
					   CCM_PROPERTY_OPACITY);
	}
	else
		ccm_window_plugin_query_opacity (self->priv->plugin, self);
}

void 
ccm_window_query_mwm_hints(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	ccm_debug_window(self, "QUERY MWM HINTS");
	ccm_window_get_property_async(self, 
								  CCM_WINDOW_GET_CLASS(self)->mwm_hints_atom,
								  AnyPropertyType, sizeof(MotifWmHints));
}

void 
ccm_window_query_transient_for (CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	ccm_debug_window(self, "QUERY TRANSIENT");
	ccm_window_get_property_async (self, 
								   CCM_WINDOW_GET_CLASS(self)->transient_for_atom,
								   XA_WINDOW, sizeof(Window));
}

void
ccm_window_query_hint_type(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	ccm_debug_window(self, "QUERY HINT TYPE");
	ccm_window_get_property_async (self, CCM_WINDOW_GET_CLASS(self)->type_atom,
								   XA_ATOM, sizeof(Atom));
}

void
ccm_window_query_wm_hints(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	XWMHints *hints;
	CCMDisplay* display = ccm_drawable_get_display (CCM_DRAWABLE(self));
	
	hints = XGetWMHints (CCM_DISPLAY_XDISPLAY(display), 
						 CCM_WINDOW_XWINDOW(self));
	if (hints)
    {
		Window old = self->priv->group_leader;
		
		if (hints->flags & WindowGroupHint)
			self->priv->group_leader = hints->window_group;
		if (old != self->priv->group_leader)
			g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
						  CCM_PROPERTY_WM_HINTS);
		
		XFree (hints);
	}
}

CCMWindowType
ccm_window_get_hint_type(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, CCM_WINDOW_TYPE_NORMAL);
	
	return self->priv->hint_type;
}

const gchar*
ccm_window_get_name(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL, NULL);
	
	if (self->priv->name == NULL)
	{
		self->priv->name = ccm_window_get_utf8_property (self, 
								CCM_WINDOW_GET_CLASS(self)->visible_name_atom);
		
		if (!self->priv->name)
			self->priv->name = ccm_window_get_utf8_property (self, 
										CCM_WINDOW_GET_CLASS(self)->name_atom);
		
		if (!self->priv->name)
			self->priv->name = ccm_window_get_text_property (self, XA_WM_NAME);
	}
	
	if (self->priv->name == NULL)
	{
		self->priv->name = ccm_window_get_child_utf8_property (self, 
								CCM_WINDOW_GET_CLASS(self)->visible_name_atom);
		
		if (!self->priv->name)
			self->priv->name = ccm_window_get_child_utf8_property (self, 
										CCM_WINDOW_GET_CLASS(self)->name_atom);
		
		if (!self->priv->name)
			self->priv->name = ccm_window_get_child_text_property (self, XA_WM_NAME);
	}
	return self->priv->name;
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
	if (self->priv->orig_opaque)
	{
		ccm_region_destroy(self->priv->orig_opaque);
		self->priv->orig_opaque = NULL;
	}
}

void
ccm_window_set_opaque_region(CCMWindow* self, const CCMRegion* region)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(region != NULL);
	
	ccm_window_plugin_set_opaque_region (self->priv->plugin, self, region);
}

void
ccm_window_set_opaque(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	const CCMRegion* geometry = ccm_drawable_get_geometry(CCM_DRAWABLE(self));
	
	ccm_window_set_alpha(self);
	if (geometry) ccm_window_set_opaque_region (self, geometry);
}

gboolean
ccm_window_is_decorated(CCMWindow* self)
{
	g_return_val_if_fail(self != NULL, TRUE);
	
	return self->priv->is_decorated;
}

const cairo_rectangle_t*
ccm_window_get_area(CCMWindow* self)
{
	g_return_val_if_fail(self, NULL);
	
	return self->priv->area.width <= 0 || self->priv->area.height <= 0 ?
		   NULL : &self->priv->area;
}

void
ccm_window_query_frame_extends(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(CCM_WINDOW_GET_CLASS(self) != NULL);
	
	guint32* data = NULL;
	guint n_items;
	gboolean updated = FALSE;
	int left_frame, right_frame, top_frame, bottom_frame;
	
	if (self->priv->child)
		data = ccm_window_get_child_property(self, 
								CCM_WINDOW_GET_CLASS(self)->frame_extends_atom,
								XA_CARDINAL, &n_items);
	else
		data = ccm_window_get_property(self, 
								CCM_WINDOW_GET_CLASS(self)->frame_extends_atom,
								XA_CARDINAL, &n_items);
	if (data)
	{
		gulong* extends = (gulong*)data;
		
		if (n_items == 4)
		{
			left_frame = (int)extends[0];
			updated |= left_frame != self->priv->frame_left;
      		right_frame  = (int)extends[1];
			updated |= right_frame != self->priv->frame_right;
      		top_frame    = (int)extends[2];
			updated |= left_frame != self->priv->frame_top;
      		bottom_frame = (int)extends[3];
			updated |= bottom_frame != self->priv->frame_bottom;
		}
		g_free(data);
	}
	
	if (updated)
	{
		self->priv->frame_left = left_frame;
		self->priv->frame_right = right_frame;
		self->priv->frame_top = top_frame;
		self->priv->frame_bottom = bottom_frame;
		g_signal_emit(self, signals[PROPERTY_CHANGED], 0,
					  CCM_PROPERTY_FRAME_EXTENDS);
	}
}

void
ccm_window_get_frame_extends(CCMWindow* self, int* left_frame, int* right_frame, 
							 int* top_frame, int* bottom_frame)
{
	g_return_if_fail(self != NULL);
	
	*left_frame   = self->priv->frame_left;
    *right_frame  = self->priv->frame_right;
    *top_frame    = self->priv->frame_top;
    *bottom_frame = self->priv->frame_bottom;
}

void 
ccm_window_init_transfrom(CCMWindow* self)
{
	g_return_if_fail(self != NULL);
	
	cairo_matrix_init_identity (&self->priv->transform);
	if (self->priv->orig_opaque)
	{
		CCMRegion* region = ccm_region_copy (self->priv->orig_opaque);
		
		ccm_window_set_opaque_region(self, region);
		ccm_region_destroy (region);
	}
	ccm_drawable_damage (CCM_DRAWABLE(self));
}

void
ccm_window_set_transform(CCMWindow* self, cairo_matrix_t* matrix)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(matrix != NULL);
	
	memcpy (&self->priv->transform, matrix, sizeof(cairo_matrix_t));
	
	if (self->priv->orig_opaque)
	{
		CCMRegion* region = ccm_region_copy (self->priv->orig_opaque);
		
		ccm_window_set_opaque_region(self, region);
		ccm_region_destroy (region);
	}
}

void
ccm_window_get_transform(CCMWindow* self, cairo_matrix_t* matrix)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(matrix != NULL);
	
	memcpy (matrix, &self->priv->transform, sizeof(cairo_matrix_t));
}

gboolean
ccm_window_transform(CCMWindow* self, cairo_t* ctx, gboolean y_invert)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(ctx != NULL, FALSE);
	
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(self), &geometry))
	{
		cairo_matrix_t matrix;
		
		ccm_window_get_transform (self, &matrix);
		ccm_debug_window(self,"MATRIX: %f,%f %f,%f %f,%f\n", 
						 matrix.xx, matrix.xy, matrix.yy, matrix.yx, 
						 matrix.x0, matrix.y0);
		if (matrix.xx <= 0.01f || matrix.yy <= 0.01f ||
			cairo_matrix_invert (&matrix) != CAIRO_STATUS_SUCCESS)
		{
			ccm_debug_window(self, "INVALID MATRIX");
			return FALSE;
		}
	
		ccm_window_get_transform (self, &matrix);
		if (y_invert)
		{
			cairo_matrix_scale (&matrix, 1.0, -1.0);
			cairo_matrix_translate (&matrix, 0.0f, -self->priv->area.height);
		}
		
		cairo_identity_matrix (ctx);
		cairo_translate (ctx, geometry.x, geometry.y);
		cairo_transform (ctx, &matrix);
	}
	
	return TRUE;
}

guint32*
ccm_window_get_property(CCMWindow* self, Atom property_atom, 
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
		ccm_debug("ERROR GET  PROPERTY = %i", ret);
		if (property) XFree(property);
		g_signal_emit(self, signals[ERROR], 0);
		return NULL;
    }
	ccm_debug("PROPERTY = 0x%x, %i", property, n_items_internal);
		
	result = g_memdup (property, n_items_internal * sizeof(gulong));
    XFree(property);
	
    if (n_items) *n_items = n_items_internal;
    
    return result;
}

guint32 *
ccm_window_get_child_property(CCMWindow* self, Atom property_atom,
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
    
	if (!self->priv->child) return NULL;
	
    ret = XGetWindowProperty (CCM_DISPLAY_XDISPLAY(display), 
							  self->priv->child, property_atom, 
							  0, G_MAXLONG, False,
							  req_type, &type, &format,
							  &n_items_internal, &bytes_after,
							  &property);
    
    if (ret != Success)
    {
		ccm_debug("ERROR GET  PROPERTY = %i", ret);
		g_signal_emit(self, signals[ERROR], 0);
		if (property) XFree(property);
		return NULL;
    }
        
    ccm_debug("PROPERTY = 0x%x, %i", property, n_items_internal);
	result = g_memdup (property, n_items_internal * sizeof(gulong));
    XFree(property);
	
    if (n_items) *n_items = n_items_internal;
    
    return result;
}

