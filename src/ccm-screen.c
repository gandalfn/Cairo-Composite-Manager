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
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/XInput.h>
#include <gdk/gdk.h>
#include <strings.h>
#include <math.h>

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-screen.h"
#include "ccm-screen-plugin.h"
#include "ccm-display.h"
#include "ccm-window.h"
#include "ccm-pixmap.h"
#include "ccm-drawable.h"
#include "ccm-extension-loader.h"
#include "ccm-keybind.h"
#include "ccm-timeline.h"

#define DEFAULT_PLUGINS "opacity,fade,shadow,menu-animation,magnifier"

enum
{
    PROP_0,
	PROP_DISPLAY,
	PROP_NUMBER,
	PROP_REFRESH_RATE,
	PROP_WINDOW_PLUGINS,
	PROP_BACKEND,
	PROP_NATIVE_PIXMAP_BIND,
	PROP_BUFFERED_PIXMAP,
    PROP_INDIRECT_RENDERING,
	PROP_FILTERED_DAMAGE
};

enum
{
	PLUGINS_CHANGED,
	REFRESH_RATE_CHANGED,
	WINDOW_DESTROYED,
	ENTER_WINDOW_NOTIFY,
	LEAVE_WINDOW_NOTIFY,
	ACTIVATE_WINDOW_NOTIFY,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static GSList* 	ccm_screen_get_window_plugins	(CCMScreen* self);
static void 	ccm_screen_paint				(CCMScreen* self, int num_frame, 
							 					 CCMTimeline* timeline);
static void 	ccm_screen_iface_init			(CCMScreenPluginClass* iface);
static void 	ccm_screen_unset_selection_owner(CCMScreen* self);
static void 	ccm_screen_on_window_error		(CCMScreen* self, 
												 CCMWindow* window);
static void 	ccm_screen_on_window_property_changed(CCMScreen* self, 
												  	  CCMPropertyType changed,
												      CCMWindow* window);
static void 	ccm_screen_on_window_redirect_input(CCMScreen* self, 
	                                                gboolean redirected,
	                                                CCMWindow* window);
CCMWindow*		ccm_screen_find_window_from_input(CCMScreen* self, 
												  Window xwindow);

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
	CCM_SCREEN_INDIRECT,
	CCM_SCREEN_BACKGROUND,
	CCM_SCREEN_COLOR_BACKGROUND,
	CCM_SCREEN_BACKGROUND_X,
	CCM_SCREEN_BACKGROUND_Y,
	CCM_SCREEN_OPTION_N
};

static gchar* CCMScreenOptions[CCM_SCREEN_OPTION_N] = {
	"backend",
	"native_pixmap_bind",
	"use_buffered_pixmap",
	"plugins",
	"refresh_rate",
	"indirect",
	"background",
	"color_background",
	"background_x",
	"background_y"
};

struct _CCMScreenPrivate
{
	CCMDisplay* 		display;
	Screen* 			xscreen;
	guint 				number;
	
	cairo_t*			ctx;
	
	CCMWindow* 			root;
	CCMWindow* 			cow;
	Window				selection_owner;
	CCMWindow*          fullscreen;
	CCMWindow*			active;
	CCMWindow*			sibling_mouse;
	Window				over_mouse;
	
	CCMRegion*			damaged;
	CCMRegion*			root_damage;
	
	Window*				stack;
	guint				n_windows;
	GList*				windows;
	GList*				removed;
	gboolean			buffered;
	gint				nb_redirect_input;
	
	guint 				refresh_rate;
	CCMTimeline*		paint;
	guint				id_pendings;
	
	CCMExtensionLoader* plugin_loader;
	CCMScreenPlugin*	plugin;

	CCMPixmap*			background;
	
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
static void impl_ccm_screen_damage(CCMScreenPlugin* plugin, CCMScreen* self, 
								   CCMRegion* area, CCMWindow* window);

static void ccm_screen_on_window_damaged(CCMScreen* self, CCMRegion* area, 
										 CCMWindow* window);
static void ccm_screen_on_option_changed(CCMScreen* self, CCMConfig* config);

static void
ccm_screen_set_property(GObject *object,
						guint prop_id,
						const GValue *value,
						GParamSpec *pspec)
{
	CCMScreenPrivate* priv = CCM_SCREEN_GET_PRIVATE(object);
    
	switch (prop_id)
    {
    	case PROP_DISPLAY:
		{
			priv->display = g_value_get_pointer (value);
		}
		break;
		case PROP_NUMBER:
		{
			priv->number = g_value_get_uint (value);
			priv->xscreen = ScreenOfDisplay(CCM_DISPLAY_XDISPLAY(priv->display),
											priv->number);
		}
		break;
		case PROP_BUFFERED_PIXMAP:
		{
			GError* error = NULL;
			gboolean use_buffered = 
				ccm_config_get_boolean(priv->options[CCM_SCREEN_USE_BUFFERED],
									   &error);
			if (!error)
			{
				priv->buffered = g_value_get_boolean (value) && use_buffered;
			}
			else
			{
				g_warning("Error on get use buffered key : %s", error->message);
				g_error_free(error);
				priv->buffered = g_value_get_boolean (value);
			}
		}
		break;
		default:
			break;
    }
}

static void
ccm_screen_get_property (GObject* object,
						 guint prop_id,
						 GValue* value,
						 GParamSpec* pspec)
{
    CCMScreenPrivate* priv = CCM_SCREEN_GET_PRIVATE(object);
    
    switch (prop_id)
    {
    	case PROP_DISPLAY:
		{
			g_value_set_pointer (value, priv->display);
		}
		break;
		case PROP_NUMBER:
		{
			g_value_set_uint (value, priv->number);
		}
		break;
		case PROP_REFRESH_RATE:
		{
			g_value_set_uint (value, priv->refresh_rate);
		}
		break;
		case PROP_WINDOW_PLUGINS:
		{
			g_value_set_pointer (value, 
						ccm_screen_get_window_plugins (CCM_SCREEN(object)));
		}
		break;
		case PROP_BACKEND:
		{
			GError* error = NULL;
			gchar* backend = 
				ccm_config_get_string(priv->options[CCM_SCREEN_BACKEND],
									  &error);
			if (!error && backend)
			{
				g_value_set_string (value, backend);
			}
			else
			{
				g_warning("Error on get backend value %s", 
						  error ? error->message : "");
				if (error) g_error_free(error);
				g_value_set_string (value, "xrender");
			}
			if (backend) g_free(backend);
			break;
		}
		case PROP_NATIVE_PIXMAP_BIND:
		{
			GError* error = NULL;
			gboolean native = 
			   ccm_config_get_boolean(priv->options[CCM_SCREEN_PIXMAP], &error);
			if (!error)
			{
				g_value_set_boolean (value, native);
			}
			else
			{
				g_warning("Error on get native backend conf : %s", 
						  error->message);
				g_error_free(error);
				g_value_set_boolean (value, TRUE);
			}
		}
		break;
		case PROP_BUFFERED_PIXMAP:
		{
			g_value_set_boolean (value, priv->buffered);
		}
		break;
		case PROP_INDIRECT_RENDERING:
		{
			GError* error = NULL;
			gboolean indirect = 
				ccm_config_get_boolean(priv->options[CCM_SCREEN_INDIRECT],
									   &error);
			if (!error)
			{
				g_value_set_boolean (value, indirect);
			}
			else
			{
				g_warning("Error on get indirect rendering conf : %s", 
						  error->message);
				g_error_free(error);
				g_value_set_boolean (value, TRUE);
			}
		}
		break;
		default:
		break;
    }
}

static void
ccm_screen_init (CCMScreen *self)
{
	self->priv = CCM_SCREEN_GET_PRIVATE(self);
	
	self->priv->display = NULL;
	self->priv->xscreen = NULL;
	self->priv->number = 0;
	self->priv->ctx = NULL;
	self->priv->root = NULL;
	self->priv->cow = NULL;
	self->priv->selection_owner = None;
	self->priv->fullscreen = NULL;
	self->priv->active = NULL;
	self->priv->sibling_mouse = NULL;
	self->priv->over_mouse = None;
	self->priv->damaged = NULL;
	self->priv->root_damage = NULL;
	self->priv->stack = NULL;
	self->priv->n_windows = 0;
	self->priv->windows = NULL;
	self->priv->removed = NULL;
	self->priv->buffered = FALSE;
	self->priv->nb_redirect_input = 0;
	self->priv->refresh_rate = 0;
	self->priv->paint = NULL;
	self->priv->id_pendings = 0;
	self->priv->plugin_loader = NULL;
	self->priv->plugin = NULL;
	self->priv->background = NULL;
}

static void
ccm_screen_finalize (GObject *object)
{
	CCMScreen *self = CCM_SCREEN(object);
	gint cpt;
	
	ccm_debug("FINALIZE SCREEN");
	
	ccm_screen_unset_selection_owner(self);
	
	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin))
		g_object_unref(self->priv->plugin);
	
	if (self->priv->plugin_loader)
		g_object_unref(self->priv->plugin_loader);
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
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
		GList* tmp = self->priv->windows;
		self->priv->windows = NULL;
		g_list_foreach(tmp, (GFunc)g_object_unref, NULL);
		g_list_free(tmp);
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
	if (self->priv->root_damage)
		ccm_region_destroy(self->priv->root_damage);
	
	if (self->priv->cow) 
	{
		XCompositeReleaseOverlayWindow(CCM_DISPLAY_XDISPLAY(self->priv->display),
									   CCM_WINDOW_XWINDOW(self->priv->cow));
		ccm_display_sync(self->priv->display);
		g_object_unref(self->priv->cow);
	}
	
	if (self->priv->background)
		g_object_unref (self->priv->background);

	G_OBJECT_CLASS (ccm_screen_parent_class)->finalize (object);
}

static void
ccm_screen_class_init (CCMScreenClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMScreenPrivate));
	
	object_class->get_property = ccm_screen_get_property;
    object_class->set_property = ccm_screen_set_property;
	object_class->finalize = ccm_screen_finalize;
	
	g_object_class_install_property(object_class, PROP_DISPLAY,
		g_param_spec_pointer ("display",
		 					  "Display",
			     			  "Display of screen",
			     			  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
	g_object_class_install_property(object_class, PROP_NUMBER,
		g_param_spec_uint ("number",
		 				   "Number",
						   "Screen number",
						    0, G_MAXUINT, None,
			     			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
	g_object_class_install_property(object_class, PROP_REFRESH_RATE,
		g_param_spec_uint ("refresh_rate",
		 				   "RefreshRate",
						   "Screen paint refresh rate",
						    0, G_MAXUINT, None,
			     			G_PARAM_READABLE));
	
	g_object_class_install_property(object_class, PROP_WINDOW_PLUGINS,
		g_param_spec_pointer ("window_plugins",
		 					  "WindowPlugins",
			     			  "Window plugins list",
			     			  G_PARAM_READABLE));
	
	g_object_class_install_property(object_class, PROP_BACKEND,
		g_param_spec_string ("backend",
		 					  "Backend",
			     			  "Screen backend",
							  "xrender",
			     			  G_PARAM_READABLE));
	
	g_object_class_install_property(object_class, PROP_NATIVE_PIXMAP_BIND,
		g_param_spec_boolean ("native_pixmap_bind",
		 					  "NativePixmapBind",
			     			  "Native pixmap bind",
							  TRUE,
			     			  G_PARAM_READABLE));
	
	g_object_class_install_property(object_class, PROP_BUFFERED_PIXMAP,
		g_param_spec_boolean ("buffered_pixmap",
		 					  "BufferedPixmap",
			     			  "Buffered pixmap",
							  TRUE,
			     			  G_PARAM_READWRITE));
	
	g_object_class_install_property(object_class, PROP_INDIRECT_RENDERING,
		g_param_spec_boolean ("indirect_rendering",
		 					  "IndirectRendering",
			     			  "Indirect rendering",
							  TRUE,
			     			  G_PARAM_READABLE));
	
	g_object_class_install_property(object_class, PROP_FILTERED_DAMAGE,
		g_param_spec_boolean ("filtered_damage",
		 					  "Filtered Damage",
			     			  "Use filtered damage",
							  TRUE,
			     			  G_PARAM_READWRITE));
	
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
	
	signals[WINDOW_DESTROYED] = g_signal_new ("window-destroyed",
											 G_OBJECT_CLASS_TYPE (object_class),
											 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
											 g_cclosure_marshal_VOID__VOID,
											 G_TYPE_NONE, 0, G_TYPE_NONE);

	signals[ENTER_WINDOW_NOTIFY] = g_signal_new ("enter-window-notify",
	                                             G_OBJECT_CLASS_TYPE (object_class),
	                                             G_SIGNAL_RUN_LAST, 0, NULL, NULL,
	                                             g_cclosure_marshal_VOID__POINTER,
	                                             G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[LEAVE_WINDOW_NOTIFY] = g_signal_new ("leave-window-notify",
	                                             G_OBJECT_CLASS_TYPE (object_class),
	                                             G_SIGNAL_RUN_LAST, 0, NULL, NULL,
	                                             g_cclosure_marshal_VOID__POINTER,
	                                             G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[ACTIVATE_WINDOW_NOTIFY] = g_signal_new ("activate-window-notify",
	                                                G_OBJECT_CLASS_TYPE (object_class),
	                                                G_SIGNAL_RUN_LAST, 0, NULL, NULL,
	                                                g_cclosure_marshal_VOID__POINTER,
	                                                G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
ccm_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= NULL;
	iface->paint 			= impl_ccm_screen_paint;
	iface->add_window 		= impl_ccm_screen_add_window;
	iface->remove_window 	= impl_ccm_screen_remove_window;
	iface->damage			= impl_ccm_screen_damage;
}

static void
ccm_screen_update_background (CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	gchar* filename = 
		ccm_config_get_string (self->priv->options[CCM_SCREEN_BACKGROUND], 
							   NULL);
	GdkColor* color = 
		ccm_config_get_color (self->priv->options[CCM_SCREEN_COLOR_BACKGROUND], 
							  NULL);

	if (filename && color)
	{
		cairo_t* ctx;
		int x, y;
		GError* error = NULL;
		
		x = ccm_config_get_integer(self->priv->options[CCM_SCREEN_BACKGROUND_X],
								   &error);		
		if (error)
		{
			g_warning("Errror on get background x pos");
			g_error_free(error);
			g_free(filename);
			g_free(color);
			return;
		}
		y = ccm_config_get_integer(self->priv->options[CCM_SCREEN_BACKGROUND_Y],
								   &error);
		if (error)
		{
			g_warning("Errror on get background y pos");
			g_error_free(error);
			g_free(filename);
			g_free(color);
			return;
		}	
	
		if (self->priv->background)
			g_object_unref(self->priv->background);
		
		self->priv->background = ccm_window_create_pixmap(
												self->priv->root,
												self->priv->xscreen->width,
												self->priv->xscreen->height, 
												32);
		ctx = ccm_drawable_create_context (CCM_DRAWABLE(self->priv->background));
		if (ctx)
		{
			GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
			cairo_rectangle_t area;
			
			area.x = 0;
			area.y = 0;
			area.width = self->priv->xscreen->width;
			area.height = self->priv->xscreen->height;
			
			cairo_set_operator (ctx, CAIRO_OPERATOR_OVER);
			gdk_cairo_set_source_color (ctx, (GdkColor*)color);
			cairo_rectangle (ctx, area.x, area.y, area.width, area.height);
			cairo_fill(ctx);
				
			if (pixbuf)
			{
				int width = gdk_pixbuf_get_width (pixbuf),
					height = gdk_pixbuf_get_height (pixbuf);
				
				cairo_translate (ctx, x ? x : (area.width - width) / 2, 
								 y ? y : (area.height - height) / 2);
				gdk_cairo_set_source_pixbuf (ctx, pixbuf, 0, 0);
				g_object_unref(pixbuf);
				cairo_paint(ctx);
			}
			ccm_drawable_damage (CCM_DRAWABLE(self->priv->background));
			cairo_destroy (ctx);
			
			ccm_screen_damage (self);
			self->priv->root_damage = ccm_region_rectangle (&area);
		}
		else
		{
			g_object_unref(self->priv->background);
			self->priv->background = NULL;
		}
	}
	if (filename) g_free(filename);
	if (color) g_free(color);
}

static gboolean
ccm_screen_update_refresh_rate(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	GError* error = NULL;
	guint refresh_rate =
		ccm_config_get_integer (self->priv->options[CCM_SCREEN_REFRESH_RATE], 
								&error);
	
	if (error)
	{
		g_warning("Error on get refresh rate configuration");
		g_error_free(error);
		refresh_rate = 60;
	}
	refresh_rate = MAX(20, refresh_rate);
	refresh_rate = MIN(100, refresh_rate);
	
	if (self->priv->refresh_rate != refresh_rate)
	{
		self->priv->refresh_rate = refresh_rate;
		if (!error)
			ccm_config_set_integer (self->priv->options[CCM_SCREEN_REFRESH_RATE], 
									refresh_rate, NULL);
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
	
	GError* error = NULL;
	gint cpt;
	
	for (cpt = 0; cpt < CCM_SCREEN_OPTION_N; ++cpt)
	{
		self->priv->options[cpt] = ccm_config_new(self->priv->number, NULL, 
												  CCMScreenOptions[cpt]);
		if (self->priv->options[cpt])
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_screen_on_option_changed), 
								 self);
	}
	self->priv->buffered = 
		ccm_config_get_boolean(self->priv->options[CCM_SCREEN_USE_BUFFERED],
							   &error);
	if (error)
	{
		g_warning("Error on get use buffered configuration");
		g_error_free(error);
		self->priv->buffered = TRUE;
	}
	ccm_screen_update_refresh_rate (self);
}

static gboolean
ccm_screen_valid_window(CCMScreen* self, CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(window != NULL, FALSE);
	
	cairo_rectangle_t geometry;
	
	if (ccm_screen_find_window_from_input(self, CCM_WINDOW_XWINDOW(window)))
		return FALSE;
		
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

	g_return_val_if_fail(root != NULL, FALSE);
	g_return_val_if_fail(self->priv->display != NULL, FALSE);
	
	window = XCompositeGetOverlayWindow (CCM_DISPLAY_XDISPLAY(self->priv->display), 
										 CCM_WINDOW_XWINDOW(root));
	if (!window) return FALSE;
	
	self->priv->cow = ccm_window_new(self, window);
	ccm_window_make_output_only(self->priv->cow);
	
	return self->priv->cow != NULL;
}

#if 0
static void
ccm_screen_print_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	GList* item;
	
	ccm_debug("XID\t\tVisible\tType\tManaged\tDecored\tFullscreen\tKA\tKB\tTransient\tGroup\t\tName");
	for (item = self->priv->windows; item; item = item->next)
	{
		CCMWindow* transient = ccm_window_transient_for (item->data);
		CCMWindow* leader = ccm_window_get_group_leader (item->data);

		if (ccm_window_is_viewable (item->data))
		ccm_debug("0x%lx\t%i\t%i\t%i\t%i\t%i\t\t%i\t%i\t0x%08lx\t0x%08lx\t%s", 
				CCM_WINDOW_XWINDOW(item->data), 
				ccm_window_is_viewable (item->data),
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

#endif
#if 0
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
	for (cpt = 0; cpt < n_windows; ++cpt)
	{
		CCMWindow *window = ccm_screen_find_window_or_child (self, windows[cpt]);
		if (window)
		{
			CCMWindow* transient = ccm_window_transient_for (window);
			CCMWindow* leader = ccm_window_get_group_leader (window);
			g_print("0x%lx\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t0x%lx\t0x%lx\t%s\n", 
				CCM_WINDOW_XWINDOW(window), 
				ccm_window_is_viewable (window),
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

static void
ccm_screen_destroy_window (CCMScreen* self, CCMWindow* window)
{
	ccm_debug_window(window, "DESTROY WINDOW");
	
	self->priv->windows = g_list_remove(self->priv->windows, window);
	
	if (CCM_IS_WINDOW(window))
	{
		g_signal_handlers_disconnect_by_func(window, 
											 ccm_screen_on_window_damaged, 
											 self);
		g_signal_handlers_disconnect_by_func(window, 
											 ccm_screen_on_window_error, 
											 self);
		g_signal_handlers_disconnect_by_func(window, 
											 ccm_screen_on_window_property_changed, 
											 self);
		g_signal_handlers_disconnect_by_func(window, 
		                                     ccm_screen_on_window_redirect_input,
		                                     self);
	}
	if (self->priv->fullscreen == window)
	{
		self->priv->fullscreen = NULL;
		ccm_screen_damage (self);
	}
	else if (CCM_IS_WINDOW(window) && 
			 ccm_window_is_viewable(window) &&
			 !ccm_window_is_input_only(window))
	{
		const CCMRegion* geometry = ccm_drawable_get_geometry (CCM_DRAWABLE(window));
		if (geometry && !ccm_region_empty ((CCMRegion*)geometry))
			ccm_screen_damage_region(self, geometry);
		else
			ccm_screen_damage (self);
	}
	if (self->priv->sibling_mouse == window)
		self->priv->sibling_mouse = NULL;
	
	if (G_IS_OBJECT(window)) g_object_unref(window);
	
	g_signal_emit (self, signals[WINDOW_DESTROYED], 0);
}

CCMWindow*
ccm_screen_find_window(CCMScreen* self, Window xwindow)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	GList* item;
	
	for (item = g_list_first(self->priv->windows); item; item = item->next)
	{
		if (CCM_IS_WINDOW(item->data) && CCM_WINDOW_XWINDOW(item->data) == xwindow)
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
		if (CCM_IS_WINDOW(item->data) && CCM_WINDOW_XWINDOW(item->data) == xwindow)
			return CCM_WINDOW(item->data);
		else if (CCM_IS_WINDOW(item->data))
		{
			Window xchild = None;
			g_object_get(G_OBJECT(item->data), "child", &xchild, NULL);
			if (xchild == xwindow) child = CCM_WINDOW(item->data);
		}
	}
	
	return child;
}

CCMWindow*
ccm_screen_find_window_from_input(CCMScreen* self, Window xwindow)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(xwindow != None, NULL);
	
	GList* item;
	CCMWindow* child = NULL;
	
	for (item = g_list_last(self->priv->windows); item; item = item->prev)
	{
		if (CCM_IS_WINDOW(item->data))
		{
			Window xinput = None;
			g_object_get(G_OBJECT(item->data), "input", &xinput, NULL);
			if (xinput == xwindow) 
				child = CCM_WINDOW(item->data);
		}
	}
	if (child) ccm_debug_window(child, "PARENT OF 0x%lx", xwindow);
	
	return child;
}

CCMWindow*
ccm_screen_find_window_at_pos(CCMScreen* self, int x, int y)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	GList* item;
	CCMWindow* found = NULL;
	
	for (item = g_list_last(self->priv->windows); item && !found; 
		 item = item->prev)
	{
		if (CCM_IS_WINDOW(item->data) && 
			ccm_window_is_viewable(item->data) &&
			!ccm_window_is_input_only(item->data))
		{
			CCMRegion* geometry = 
				(CCMRegion*)ccm_drawable_get_geometry(item->data);
			
			if (geometry && ccm_region_point_in(geometry, x, y))
			{
				found = CCM_WINDOW(item->data);
			}
		}
	}
	
	return found;
}

static void
ccm_screen_unset_selection_owner(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->selection_owner != None)
	{
		gchar* cm_atom_name = g_strdup_printf("_NET_WM_CM_S%i", 
											  self->priv->number);
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
		gchar* cm_atom_name = g_strdup_printf("_NET_WM_CM_S%i", 
											  self->priv->number);
		Atom cm_atom = XInternAtom(CCM_DISPLAY_XDISPLAY(self->priv->display), 
								   cm_atom_name, 0);
		CCMWindow* root = ccm_screen_get_root_window(self);
		
		g_free(cm_atom_name);
		 
		if (XGetSelectionOwner (CCM_DISPLAY_XDISPLAY(self->priv->display), 
								cm_atom) != None)
		{
			g_critical("\nScreen %d already has a composite manager running, \n"
					   "try to stop it before run cairo-compmgr", 
					   self->priv->number);
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
	for (cpt = 0; cpt < self->priv->n_windows; ++cpt)
	{
		CCMWindow *window = ccm_window_new(self, self->priv->stack[cpt]);
		if (window)
		{
			if (!ccm_screen_add_window(self, window))
				g_object_unref(window);
			else if (ccm_window_is_viewable (window) &&
			         !ccm_window_is_input_only (window))
				ccm_window_map(window);
		}
	}
}

static void
ccm_screen_check_stack(CCMScreen* self)
{
	g_return_if_fail(self != NULL);

	guint cpt;
	GList* stack = NULL, *item, *last = NULL;
	GList* viewable = NULL, *new_viewable = NULL;
	CCMWindow* desktop = NULL;
	
	ccm_debug("CHECK_STACK");
	
	ccm_screen_update_stack (self);
	
	for (cpt = 0; cpt < self->priv->n_windows; ++cpt)
	{
		CCMWindow* window = ccm_screen_find_window (self, self->priv->stack[cpt]);
		
		if (window && !ccm_window_is_input_only(window) &&
			!g_list_find(stack, window))
		{
			stack = g_list_prepend(stack, window);
			if (ccm_window_is_viewable (window))
			{
				if (ccm_window_get_hint_type(window) == CCM_WINDOW_TYPE_DESKTOP)
					desktop = window;
				
				ccm_debug_window(window, "STACK IS VIEWABLE");
				viewable = g_list_prepend(viewable, window);
			}
		}
		else if (!window)
		{
			window = ccm_window_new (self, self->priv->stack[cpt]);
			if (window && !ccm_window_is_input_only(window) &&
				ccm_screen_valid_window(self, window))
			{
				ccm_debug_window(window, "CHECK STACK NEW WINDOW");
				if (ccm_window_is_viewable (window) &&
				    !ccm_window_is_input_only (window))
				{
					ccm_debug_window(window, "CHECK STACK NEW WINDOW MAP");
					new_viewable = g_list_prepend(new_viewable, window);
					viewable = g_list_prepend(viewable, window);
				}
				g_signal_connect_swapped(window, "damaged", 
					 G_CALLBACK(ccm_screen_on_window_damaged), self);
				g_signal_connect_swapped(window, "error", 
					 G_CALLBACK(ccm_screen_on_window_error), self);
				g_signal_connect_swapped(window, "property-changed", 
					 G_CALLBACK(ccm_screen_on_window_property_changed), self);
				g_signal_connect_swapped(window, "redirect-input", 
					 G_CALLBACK(ccm_screen_on_window_redirect_input), self);
				stack = g_list_prepend(stack, window);
			}
			else if (window)
				g_object_unref(window);
		}
	}
	stack = g_list_reverse(stack);
	
	for (item = g_list_first(self->priv->windows); item && stack; item = item->next)
	{
		GList* link = g_list_find(stack, item->data);

		if (link && ccm_window_is_viewable (item->data) &&
		    !ccm_window_is_input_only (item->data))
		{
			last = link;
		}
		else if (!link)
		{
			gboolean found = FALSE;
			
			ccm_debug_window(item->data, "CHECK STACK REMOVED");
			if (ccm_window_is_viewable (item->data) &&
				!ccm_window_is_input_only (item->data))
			{
				GList* last_viewable = NULL;
				
				if (last)
				{
					last_viewable = g_list_find(stack, last->data);
					if (last_viewable)
						stack = g_list_insert_before (stack, 
													  last_viewable->next,
													  item->data);
				}
				if (!last_viewable) stack = g_list_prepend (stack, item->data);
				found = TRUE;
			}
			
			if (found)
				ccm_screen_remove_window (self, item->data);
			else
				ccm_screen_destroy_window (self, item->data);
		}
	}

	viewable = g_list_reverse(viewable);
	if (desktop && viewable->data != desktop)
	{
		GList* desktop_link = g_list_find(stack, desktop);

		stack = g_list_remove(stack, viewable->data);
		stack = g_list_insert_before (stack, desktop_link->next, viewable->data);
	}
	
	g_list_free(self->priv->windows);
	g_list_free(viewable);
	self->priv->windows = stack;
	
	for (item = new_viewable; item; item = item->next)
		ccm_drawable_damage (item->data);

	g_list_free(new_viewable);
	
#if 0
	g_print("Stack\n");
	ccm_screen_print_stack (self);
#endif
#if 0	
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

	if (ccm_window_get_hint_type (window) == CCM_WINDOW_TYPE_DESKTOP)
		return;

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
	for (item = self->priv->fullscreen && 
		 ccm_window_get_opaque_region(self->priv->fullscreen) ? 
		 g_list_find(self->priv->windows, self->priv->fullscreen) : 
		 g_list_first(self->priv->windows); item; item = item->next)
	{
		if (CCM_IS_WINDOW(item->data))
		{
			CCMWindow* window = CCM_WINDOW(item->data);
		
			if (!ccm_window_is_input_only(window))
			{
				if (ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
				{
					CCMRegion* damaged = NULL;
					g_object_get(G_OBJECT(window), "damaged", &damaged, NULL);
					ccm_debug_region(CCM_DRAWABLE(window), "SCREEN DAMAGE");
					if (!self->priv->damaged) 
						self->priv->damaged = ccm_region_copy(damaged);
					else
						ccm_region_union(self->priv->damaged, damaged);
				}
			
				ccm_debug_window(window, "PAINT SCREEN");
				ret |= ccm_window_paint(window, self->priv->ctx, self->priv->buffered);
			}
		}
	}
	for (item = g_list_first(self->priv->removed); item; item = item->next)
	{
		if (!ccm_window_is_viewable (item->data) ||
			ccm_window_is_input_only (item->data))
		{
			self->priv->windows = g_list_remove (self->priv->windows, item->data);
			ccm_screen_destroy_window (self, item->data);
			destroy = g_list_prepend(destroy, item->data);
		}
	}

	if (destroy)
	{
		for (item = g_list_first(destroy); item; item = item->next)
			self->priv->removed = g_list_remove (self->priv->removed, item->data);
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
ccm_screen_on_window_redirect_input(CCMScreen* self, gboolean redirected,
                                    CCMWindow* window)
{
	g_return_if_fail(self != NULL);
	
	if (redirected)
		self->priv->nb_redirect_input++;
	else
		self->priv->nb_redirect_input = MAX(self->priv->nb_redirect_input - 1, 0);
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
		else if (!ccm_window_is_fullscreen (window) && 
		         self->priv->fullscreen == window)
		{
			ccm_debug_window(window, "UNFULLSCREEN");
			self->priv->fullscreen = NULL;
			ccm_screen_damage (self);
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
	g_signal_connect_swapped(window, "redirect-input", 
							 G_CALLBACK(ccm_screen_on_window_redirect_input), self);
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
		
		if (!ccm_window_is_viewable (window) ||
			ccm_window_is_input_only (window))
			ccm_screen_destroy_window(self, window);
		else if (!g_list_find (self->priv->removed, window))
			self->priv->removed = g_list_prepend (self->priv->removed, window);
	}
}

static GSList*
ccm_screen_get_window_plugins(CCMScreen* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	GSList* filter, *plugins = NULL;
	GError* error = NULL;
	
	filter = ccm_config_get_string_list(self->priv->options[CCM_SCREEN_PLUGINS],
										&error);
	if (error)
	{
		gchar** default_plugins = g_strsplit(DEFAULT_PLUGINS, ",", -1);
		gint cpt;
		
		g_error_free(error);
		g_warning("Error on get plugins list set default");
		for (cpt = 0; default_plugins[cpt]; ++cpt)
		{
			filter = g_slist_prepend(filter, g_strdup(default_plugins[cpt]));
		}
		g_strfreev(default_plugins);
		filter = g_slist_reverse(filter);
	}
	if (filter)
	{
		plugins = ccm_extension_loader_get_window_plugins(self->priv->plugin_loader,
													  filter);
		g_slist_foreach(filter, (GFunc)g_free, NULL);
		g_slist_free(filter);
	}
	return plugins;
}

static void
ccm_screen_get_plugins(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	GSList* filter = NULL, *plugins = NULL, *item;
	GError* error = NULL;
	
	if (self->priv->plugin && CCM_IS_PLUGIN(self->priv->plugin))
		g_object_unref(self->priv->plugin);
	
	self->priv->plugin = (CCMScreenPlugin*)self;
	
	filter = ccm_config_get_string_list(self->priv->options[CCM_SCREEN_PLUGINS],
										&error);
	if (error)
	{
		gchar** default_plugins = g_strsplit(DEFAULT_PLUGINS, ",", -1);
		gint cpt;
		
		g_error_free(error);
		g_warning("Error on get plugins list set default");
		for (cpt = 0; default_plugins[cpt]; ++cpt)
		{
			filter = g_slist_prepend(filter, g_strdup(default_plugins[cpt]));
		}
		g_strfreev(default_plugins);
		filter = g_slist_reverse(filter);
	}
	if (filter)
	{
		plugins = ccm_extension_loader_get_screen_plugins(self->priv->plugin_loader,
														  filter);
		g_slist_foreach(filter, (GFunc)g_free, NULL);
		g_slist_free(filter);
		for (item = plugins; item; item = item->next)
		{
			GType type = GPOINTER_TO_INT(item->data);
			GObject* prev = G_OBJECT(self->priv->plugin);
			CCMScreenPlugin* plugin = g_object_new(type, "parent", prev, NULL);
		
			if (plugin) self->priv->plugin = plugin;
		}
		if (plugins) g_slist_free(plugins);
		ccm_screen_plugin_load_options(self->priv->plugin, self);
	}
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
			if (self->priv->ctx)
			{
				cairo_rectangle(self->priv->ctx, 0, 0, 
								self->priv->xscreen->width, 
								self->priv->xscreen->height);
				cairo_clip(self->priv->ctx);
				cairo_set_operator (self->priv->ctx, CAIRO_OPERATOR_CLEAR);
				cairo_paint(self->priv->ctx);
				cairo_set_operator (self->priv->ctx, CAIRO_OPERATOR_OVER);
			}
		}
		else
		{
			cairo_identity_matrix (self->priv->ctx);
			cairo_rectangle(self->priv->ctx, 0, 0, 
								self->priv->xscreen->width, 
								self->priv->xscreen->height);
			cairo_clip(self->priv->ctx);
		}
			
		if (self->priv->root_damage)
		{	
			if (!self->priv->background)
			{
				ccm_screen_update_background(self);
			}
			if (self->priv->background)
			{
				cairo_rectangle_t* rects;
				gint cpt, nb_rects;
				cairo_surface_t* surface = ccm_drawable_get_surface(CCM_DRAWABLE(self->priv->background));
				
				cairo_save(self->priv->ctx);
				ccm_region_get_rectangles (self->priv->root_damage, &rects, &nb_rects);
				for (cpt = 0; cpt < nb_rects; ++cpt)
					cairo_rectangle (self->priv->ctx, rects[cpt].x, rects[cpt].y,
									 rects[cpt].width, rects[cpt].height);
				g_free(rects);
				cairo_clip (self->priv->ctx);
				
				cairo_set_source_surface (self->priv->ctx, surface, 0, 0);
				cairo_paint(self->priv->ctx);
				cairo_surface_destroy (surface);
				cairo_restore(self->priv->ctx);
			}
			ccm_screen_add_damaged_region (self, self->priv->root_damage);
			ccm_region_destroy (self->priv->root_damage);
			self->priv->root_damage = NULL;
		}
		
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
	else if (config == self->priv->options[CCM_SCREEN_BACKGROUND] ||
			 config == self->priv->options[CCM_SCREEN_COLOR_BACKGROUND] ||
			 config == self->priv->options[CCM_SCREEN_BACKGROUND_X] ||
			 config == self->priv->options[CCM_SCREEN_BACKGROUND_Y])
	{
		ccm_screen_update_background (self);
	}
}

static void
impl_ccm_screen_damage(CCMScreenPlugin* plugin, CCMScreen* self, 
					   CCMRegion* area, CCMWindow* window)
{
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);
	g_return_if_fail(window != NULL);
	
	GList* item = NULL;
	gboolean top = TRUE;
	CCMRegion* damage_above = NULL, * damage_below = NULL;
	const CCMRegion* opaque = NULL;
	
	damage_above = ccm_region_copy(area);
	damage_below = ccm_region_copy(area);
	
	ccm_debug_region(CCM_DRAWABLE(window), "ON_DAMAGE");
	
	// Substract opaque region of window to damage region below
	opaque = ccm_window_get_opaque_region(window);
	if (opaque && 
		ccm_window_is_viewable(window))
	{
		ccm_region_subtract(damage_below, (CCMRegion*)opaque);
	}
	
	// Substract all obscured area to damage region
	for (item = g_list_last(self->priv->windows); item; item = item->prev)
	{
		if (!ccm_window_is_input_only(item->data) &&
			ccm_window_is_viewable(item->data) && 
			CCM_WINDOW_XWINDOW(item->data) != CCM_WINDOW_XWINDOW(window))
		{
			opaque = ccm_window_get_opaque_region(item->data);
			if (opaque)
			{
				ccm_debug_window(window, "UNDAMAGE ABOVE 0x%lx", 
								 CCM_WINDOW_XWINDOW(item->data));
				ccm_drawable_undamage_region(CCM_DRAWABLE(window), 
											 (CCMRegion*)opaque);
				// window is totaly obscured don't damage all other windows
				if (!ccm_drawable_is_damaged (CCM_DRAWABLE(window)))
				{
					ccm_region_destroy (damage_below);
					ccm_region_destroy (damage_above);
					return;
				}
				ccm_region_subtract (damage_above, (CCMRegion*)opaque);
				ccm_region_subtract (damage_below, (CCMRegion*)opaque);
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
		if (!ccm_window_is_input_only(item->data) &&
			ccm_window_is_viewable(item->data) && 
			item->data != window)
		{
			if (top)
			{
				ccm_drawable_damage_region_silently(item->data, 
													damage_above);
			}
			else
			{
				opaque = ccm_window_get_opaque_region(window);
				if (ccm_window_is_viewable(window) && 
					!ccm_window_is_input_only(window) &&
					opaque && !ccm_region_empty((CCMRegion*)opaque))
				{
					ccm_debug_window(item->data, "UNDAMAGE BELOW");
					ccm_drawable_undamage_region(CCM_DRAWABLE(item->data), 
												 (CCMRegion*)opaque);						
				}

				ccm_drawable_damage_region_silently(item->data, 
													damage_below);
				opaque = ccm_window_get_opaque_region(item->data);
				
				if (opaque) 
				{
					ccm_region_subtract (damage_below, (CCMRegion*)opaque);
				}
			}
		}
		else if (item->data == window)
		{
			top = FALSE;
			opaque = ccm_window_get_opaque_region(window);
			if (ccm_region_empty(damage_below) && 
				ccm_region_empty((CCMRegion*)opaque)) break;
		}
	}
	
	if (!ccm_region_empty (damage_below))
	{
		cairo_rectangle_t area;
		CCMRegion* geometry;
		
		area.x = 0;
		area.y = 0;
		area.width = self->priv->xscreen->width;
		area.height = self->priv->xscreen->height;
		geometry = ccm_region_rectangle (&area);
		ccm_region_intersect (damage_below, geometry);
		ccm_region_destroy (geometry);
		
		if (!ccm_region_empty (damage_below))
		{
			ccm_region_get_clipbox (damage_below, &area);
			if (self->priv->root_damage)
				ccm_region_union (self->priv->root_damage, damage_below);
			else
				self->priv->root_damage = ccm_region_copy(damage_below);
		}
	}
	
	ccm_region_destroy(damage_above);
	ccm_region_destroy(damage_below);
}

static void
ccm_screen_on_window_damaged(CCMScreen* self, CCMRegion* area, CCMWindow* window)
{
	if (!self->priv->cow)
		ccm_screen_create_overlay_window(self);
	
	if (self->priv->cow &&
	    CCM_WINDOW_XWINDOW(self->priv->cow) != CCM_WINDOW_XWINDOW(window))
	{
		ccm_screen_plugin_damage(self->priv->plugin, self, area, window);
	}
}

static void
ccm_screen_on_event(CCMScreen* self, XEvent* event)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(event != NULL);
	
		
	switch (event->type)
	{
		case ButtonPress:
		case ButtonRelease:
		{
			XButtonEvent* button_event = ((XButtonEvent*)event);
			
			if (button_event->root != CCM_WINDOW_XWINDOW(self->priv->root))
				return;

			if (self->priv->nb_redirect_input)
			{
				CCMWindow* input = 
					ccm_screen_find_window_from_input(self, button_event->window);
			
				if (input)
				{
					CCMWindow* window = ccm_screen_find_window_at_pos(self,
														button_event->x_root,
														button_event->y_root);

					self->priv->over_mouse = 
							ccm_window_redirect_event(window, event, 
													  self->priv->over_mouse);
				}
				else
					self->priv->over_mouse = None;
			}
			else
				self->priv->over_mouse = None;
		}
		break;
		case MotionNotify:
		{
			XMotionEvent* motion_event = ((XMotionEvent*)event);
			
			if (motion_event->root != CCM_WINDOW_XWINDOW(self->priv->root))
				return;

			if (self->priv->nb_redirect_input)
			{
				CCMWindow* window = ccm_screen_find_window_at_pos(self,
													motion_event->x_root,
													motion_event->y_root);
				if (window)
				{
					Window input = None;
					g_object_get(G_OBJECT(window), "input", &input, NULL);
					if (input)
					{
						self->priv->over_mouse = 
							ccm_window_redirect_event(window, event, 
													  self->priv->over_mouse);
					}
					else
						self->priv->over_mouse = None;	

					if (window != self->priv->sibling_mouse)
					{
						if (self->priv->sibling_mouse)
							g_signal_emit(self, signals[LEAVE_WINDOW_NOTIFY], 0, 
									      self->priv->sibling_mouse);
						self->priv->sibling_mouse = window;
						g_signal_emit(self, signals[ENTER_WINDOW_NOTIFY], 0, 
							          self->priv->sibling_mouse);
					}
				}
			}
		}
		break;
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
										((XDestroyWindowEvent*)event)->window);
			ccm_debug("REMOVE 0x%x", ((XDestroyWindowEvent*)event)->window);
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
				
			ccm_debug("REPARENT 0x%x, 0x%x", ((XReparentEvent*)event)->parent,
					((XReparentEvent*)event)->window);
			
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
				ccm_screen_destroy_window (self, window);
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
				ccm_screen_add_check_pending (self);
			}
		}
		break;
		case ConfigureNotify:
		{
			XConfigureEvent* configure_event = (XConfigureEvent*)event;
			CCMWindow* window;
			
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
			CCMWindow* window;
			
			ccm_debug_atom(self->priv->display, property_event->atom, 
						   "PROPERTY_NOTIFY");
			
			if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->active_atom)
			{
				guint32* data;
				guint n_items;
				Window active;
				
				ccm_screen_add_check_pending (self);
				data = ccm_window_get_property(self->priv->root, 
							CCM_WINDOW_GET_CLASS(self->priv->root)->active_atom,
							XA_WINDOW, &n_items);
				if (data)
				{
					active = (Window)*data;
					if (active)
						self->priv->active = 
							ccm_screen_find_window_or_child (self, (Window)*data);
					else
						self->priv->active = NULL;
					g_free(data);
					if (self->priv->active)
						g_signal_emit(self, signals[ACTIVATE_WINDOW_NOTIFY], 0, 
							          self->priv->active);
				}
			}
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->client_stacking_list_atom ||
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
			else if (property_event->atom == CCM_WINDOW_GET_CLASS(self->priv->root)->frame_extends_atom)
			{
				window = ccm_screen_find_window_or_child (self,
														  property_event->window);
				if (window) ccm_window_query_frame_extends(window);
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
			XClientMessageEvent* client_event = (XClientMessageEvent*)event;
			
			ccm_debug_atom(self->priv->display, client_event->message_type, 
						   "CLIENT MESSAGE");
			
			if (client_event->message_type == 
							CCM_WINDOW_GET_CLASS(self->priv->root)->state_atom)
			{
				CCMWindow* window = ccm_screen_find_window_or_child (self,
													   client_event->window);
				if (window)
				{
					gint cpt;
					
					for (cpt = 1; cpt < 3; ++cpt)
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
		{
			// Check for shape notify
			if (event->type == ccm_display_get_shape_notify_event_type(self->priv->display))
			{
				XShapeEvent* shape_event = (XShapeEvent*)event;
				
				CCMWindow* window = ccm_screen_find_window_or_child (self,
													   shape_event->window);
				if (window)
				{
					const CCMRegion* geometry = 
						ccm_drawable_get_device_geometry(CCM_DRAWABLE(window));
	
					if (!geometry || !ccm_region_is_shaped((CCMRegion*)geometry))
					{
						ccm_drawable_damage (CCM_DRAWABLE(window));
						ccm_drawable_query_geometry (CCM_DRAWABLE(window));
						ccm_drawable_damage (CCM_DRAWABLE(window));
					}
				}
			}
		}
		break;
	}
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

CCMScreen*
ccm_screen_new(CCMDisplay* display, guint number)
{
	g_return_val_if_fail(display != NULL, NULL);
	
	CCMWindow* root;
	cairo_rectangle_t area;
	CCMScreen *self = g_object_new(CCM_TYPE_SCREEN, 
								   "display", display,
								   "number", number,
								   NULL);
	
	if (!self->priv->xscreen)
	{
		g_object_unref(self);
		return NULL;
	}
	
	self->priv->plugin_loader = ccm_extension_loader_new();
	
	ccm_screen_load_config(self);
			
	/* Load plugins */
	ccm_screen_get_plugins (self);
	
	g_signal_connect_swapped(self->priv->display, "event", 
							 G_CALLBACK(ccm_screen_on_event), self);
	
	if (!ccm_screen_create_overlay_window(self))
	{
		g_warning("Error on create overlay window");
		g_object_unref(self);
		return NULL;
	}
	
	if (!ccm_screen_set_selection_owner(self))
	{
		g_object_unref(self);
		return NULL;
	}
	
	root = ccm_screen_get_root_window(self);
	ccm_window_redirect_subwindows(root);
	ccm_screen_query_stack(self);
	
	area.x = 0;
	area.y = 0;
	area.width = self->priv->xscreen->width;
	area.height = self->priv->xscreen->height;
	self->priv->root_damage = ccm_region_rectangle (&area);
	
	ccm_display_report_device_event(display, self, TRUE);
	
	return self;
}

CCMDisplay*
ccm_screen_get_display(CCMScreen* self)
{
	g_return_val_if_fail(CCM_IS_SCREEN(self), NULL);
	
	return self->priv->display;
}

Screen*
ccm_screen_get_xscreen(CCMScreen* self)
{
	g_return_val_if_fail(CCM_IS_SCREEN(self), NULL);
	
	return self->priv->xscreen;
}

guint
ccm_screen_get_number(CCMScreen* self)
{
	g_return_val_if_fail(CCM_IS_SCREEN(self), 0);
	
	return self->priv->number;
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
		Window root = RootWindowOfScreen(self->priv->xscreen);
		
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
	
	if (!ccm_window_is_input_only(window) &&
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
		if (ccm_window_is_viewable(window) &&
			!ccm_window_is_input_only (window)) 
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
		
	ccm_debug("DAMAGE SCREEN");
	screen_geometry = ccm_region_rectangle (&area);	
	ccm_screen_damage_region (self, screen_geometry);
	ccm_region_destroy (screen_geometry);
}

void
ccm_screen_damage_region(CCMScreen* self, const CCMRegion* area)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);
	
	GList* item;
	
	ccm_debug("SCREEN DAMAGE REGION");
	
	for (item = g_list_last(self->priv->windows); item; item = item->prev)
	{
		if (CCM_IS_WINDOW(item->data) &&
			!ccm_window_is_input_only (item->data) &&
			ccm_window_is_viewable (item->data))
		{
			ccm_drawable_damage_region (item->data, area);
		}
	}
}

void
ccm_screen_undamage_region(CCMScreen* self, const CCMRegion* area)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(area != NULL);
	
	GList* item;
	
	for (item = g_list_last(self->priv->windows); item; item = item->prev)
	{
		if (!ccm_window_is_input_only (item->data) &&
			ccm_window_is_viewable (item->data))
		{
			ccm_drawable_undamage_region (item->data, (CCMRegion*)area);
		}
	}
	if (self->priv->root_damage)
	{
		ccm_region_subtract (self->priv->root_damage, (CCMRegion*)area);
		if (ccm_region_empty (self->priv->root_damage))
		{
			ccm_region_destroy (self->priv->root_damage);
			self->priv->root_damage = NULL;
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
ccm_screen_remove_damaged_region (CCMScreen *self, CCMRegion* region)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (region != NULL);

	if (self->priv->damaged && !ccm_region_empty (region))
	{
		ccm_region_subtract (self->priv->damaged, region);
		if (ccm_region_empty (self->priv->damaged))
		{
			ccm_region_destroy (self->priv->damaged);
			self->priv->damaged = NULL;
		}
	}
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
		if (below) *below = ccm_screen_find_window_at_pos (self, *x, *y);
		ret = CCM_WINDOW_XWINDOW(root) == r;
	}
	
	return ret;
}

void
ccm_screen_manage_cursors(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(self->priv->display), 
							CCM_WINDOW_XWINDOW(self->priv->root), 
	                        XFixesDisplayCursorNotifyMask);
}

void
ccm_screen_unmanage_cursors(CCMScreen* self)
{
	g_return_if_fail(self != NULL);
	
	XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(self->priv->display), 
							CCM_WINDOW_XWINDOW(self->priv->root), 
	                        0);
}
