/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 				 Carlos Diógenes  2007 <cerdiogenes@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <X11/extensions/Xfixes.h>

#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-extension-loader.h"
#include "ccm-magnifier.h"
#include "ccm-keybind.h"
#include "ccm-cairo-utils.h"
#include "ccm-timeline.h"
#include "ccm.h"

#define CCM_MAGNIFIER_WINDOW_INFO_WIDTH 30
#define CCM_MAGNIFIER_WINDOW_INFO_HEIGHT 7

enum
{
	CCM_MAGNIFIER_ENABLE,
	CCM_MAGNIFIER_ZOOM_LEVEL,
	CCM_MAGNIFIER_ZOOM_QUALITY,
	CCM_MAGNIFIER_X,
	CCM_MAGNIFIER_Y,
	CCM_MAGNIFIER_HEIGHT,
	CCM_MAGNIFIER_WIDTH,
	CCM_MAGNIFIER_RESTRICT_AREA_X,
	CCM_MAGNIFIER_RESTRICT_AREA_Y,
	CCM_MAGNIFIER_RESTRICT_AREA_WIDTH,
	CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT,
	CCM_MAGNIFIER_BORDER,
	CCM_MAGNIFIER_SHORTCUT,
	CCM_MAGNIFIER_SHADE_DESKTOP,
	CCM_MAGNIFIER_OPTION_N
};

static gchar* CCMMagnifierOptions[CCM_MAGNIFIER_OPTION_N] = {
	"enable",
	"zoom-level",
	"zoom-quality",
	"x",
	"y",
	"height",
	"width",
	"restrict_area_x",
	"restrict_area_y",
	"restrict_area_width",
	"restrict_area_height",
	"border",
	"shortcut",
	"shade_desktop"
};

static void ccm_magnifier_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_magnifier_window_iface_init(CCMWindowPluginClass* iface);
static void ccm_magnifier_on_option_changed(CCMMagnifier* self, 
											CCMConfig* config);
static void ccm_magnifier_on_new_frame     (CCMMagnifier* self, int num_frame, 
											CCMTimeline* timeline);

CCM_DEFINE_PLUGIN (CCMMagnifier, ccm_magnifier, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_magnifier,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_magnifier_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_magnifier,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_magnifier_window_iface_init))

struct _CCMMagnifierPrivate
{	
	CCMScreen*			 screen;
	gboolean 			 enabled;
	gfloat				 scale;
	gfloat				 new_scale;
	gboolean 			 shade;
	
	int					 x_mouse;
	int 				 y_mouse;
	int					 x_hot;
	int 				 y_hot;
	int					 mouse_width;
	int 				 mouse_height;
	guchar*				 image_mouse;
	cairo_surface_t*     surface_mouse;
	int					 x_offset;
	int 				 y_offset;
	int 				 border;
	cairo_surface_t*     surface;
	cairo_rectangle_t    area;
	cairo_rectangle_t    restrict_area;
	cairo_filter_t		 quality;
	CCMRegion*			 damaged;
	CCMKeybind*			 keybind;
	
	cairo_surface_t*     surface_window_info;
	CCMTimeline*		 timeline;
	
	CCMConfig*           options[CCM_MAGNIFIER_OPTION_N];
};

#define CCM_MAGNIFIER_GET_PRIVATE(o)  \
   ((CCMMagnifierPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MAGNIFIER, CCMMagnifierClass))

static void
ccm_magnifier_init (CCMMagnifier *self)
{
	gint cpt;
	
	self->priv = CCM_MAGNIFIER_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->enabled = FALSE;
	self->priv->shade = TRUE;
	self->priv->scale = 1.0f;
	self->priv->new_scale = 1.0f;
	self->priv->x_mouse = 0;
	self->priv->y_mouse = 0;
	self->priv->x_hot = 0;
	self->priv->y_hot = 0;
	self->priv->mouse_width = 0;
	self->priv->mouse_height = 0;	
	self->priv->image_mouse = NULL;
	self->priv->surface_mouse = NULL;
	self->priv->x_offset = 0;
	self->priv->y_offset = 0;
	self->priv->border = 0;
	self->priv->surface = NULL;
	self->priv->damaged = NULL;
	self->priv->keybind = NULL;
	self->priv->surface_window_info = NULL;
	self->priv->timeline = ccm_timeline_new_for_duration(5000);
	g_object_set(G_OBJECT(self->priv->timeline), "fps", 30, NULL);
	g_signal_connect_swapped(self->priv->timeline, "new-frame", 
							 G_CALLBACK(ccm_magnifier_on_new_frame), 
							 self);
	bzero(&self->priv->area, sizeof(cairo_rectangle_t));
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++) 
		self->priv->options[cpt] = NULL;
}

static void
ccm_magnifier_finalize (GObject *object)
{
	CCMMagnifier* self = CCM_MAGNIFIER(object);
	gint cpt;
	
	if (self->priv->screen && self->priv->enabled)
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
		CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
		
		XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(display), 
								CCM_WINDOW_XWINDOW(root), 
								0);
		XFixesShowCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
	}
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++)
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
	if (self->priv->surface) 
		cairo_surface_destroy (self->priv->surface);
	if (self->priv->image_mouse) 
		g_free (self->priv->image_mouse);
	if (self->priv->surface_mouse) 
		cairo_surface_destroy (self->priv->surface_mouse);
	if (self->priv->damaged) 
		ccm_region_destroy (self->priv->damaged);
	if (self->priv->keybind) 
		g_object_unref(self->priv->keybind);
	if (self->priv->surface_window_info)
		cairo_surface_destroy (self->priv->surface_window_info);
	if (self->priv->timeline)
		g_object_unref(self->priv->timeline);
	
	G_OBJECT_CLASS (ccm_magnifier_parent_class)->finalize (object);
}

static void
ccm_magnifier_class_init (CCMMagnifierClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMMagnifierPrivate));

	object_class->finalize = ccm_magnifier_finalize;
}

static void
ccm_magnifier_on_key_press(CCMMagnifier* self)
{
	g_return_if_fail(self != NULL);
	
	int x, y;
	
	if (ccm_screen_query_pointer(self->priv->screen, NULL, &x, &y))
	{
		gboolean enabled = 
			ccm_config_get_boolean(self->priv->options [CCM_MAGNIFIER_ENABLE]);
		ccm_config_set_boolean(self->priv->options [CCM_MAGNIFIER_ENABLE], !enabled);
	}
}

static void
ccm_magnifier_on_new_frame (CCMMagnifier* self, int num_frame, 
							CCMTimeline* timeline)
{
	gdouble progress = ccm_timeline_get_progress(timeline);
	
	if (progress <= 0.25 || progress >= 0.75)
	{
		cairo_rectangle_t geometry;
		CCMRegion* area;
	
		geometry.width = self->priv->restrict_area.width * 
						 (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f);
		geometry.height = self->priv->restrict_area.height * 
				          (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f);
		geometry.x = (self->priv->restrict_area.width - geometry.width) / 2;
		geometry.y = 0;
	
		area = ccm_region_rectangle(&geometry);
		ccm_screen_damage_region(self->priv->screen, area);
		ccm_region_destroy(area);
	}
}

static void
ccm_magnifier_get_enable(CCMMagnifier *self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	GList* item = ccm_screen_get_windows(self->priv->screen);
	
	ccm_screen_damage(self->priv->screen);
	self->priv->enabled = 
		ccm_config_get_boolean(self->priv->options [CCM_MAGNIFIER_ENABLE]);
	
	for (;item; item = item->next)
	{
		if (ccm_window_is_viewable(item->data))
			g_object_set(G_OBJECT(item->data), "use_image", 
						 self->priv->enabled ? TRUE : FALSE, NULL);
	}
	
	if (self->priv->enabled)
	{
		XFixesHideCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
		XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(display), 
								CCM_WINDOW_XWINDOW(root), 
								XFixesDisplayCursorNotifyMask);
		ccm_timeline_rewind(self->priv->timeline);
		ccm_timeline_start(self->priv->timeline);
	}
	else
	{
		XFixesSelectCursorInput(CCM_DISPLAY_XDISPLAY(display), 
								CCM_WINDOW_XWINDOW(root), 
								0);
		XFixesShowCursor(CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root));
		if (self->priv->surface) 
			cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		if (self->priv->image_mouse) 
			g_free (self->priv->image_mouse);
		self->priv->image_mouse = NULL;
		if (self->priv->surface_mouse) 
			cairo_surface_destroy (self->priv->surface_mouse);
		self->priv->surface_mouse = NULL;
		ccm_timeline_stop(self->priv->timeline);
	}
}

static void
ccm_magnifier_get_keybind(CCMMagnifier *self)
{
	gchar* shortcut = 
		ccm_config_get_string(self->priv->options [CCM_MAGNIFIER_SHORTCUT]);
	
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	
	self->priv->keybind = ccm_keybind_new(self->priv->screen, shortcut, TRUE);
	g_free(shortcut);
	
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_magnifier_on_key_press), self);
}

static gboolean
ccm_magnifier_get_scale(CCMMagnifier *self)
{
	gfloat scale, real = 
		ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
	
	scale = MAX(1.0f, real);
	scale = MIN(5.0f, scale);
	if (real != scale)
		ccm_config_set_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL],
							  scale);
		
	if (self->priv->new_scale != scale)
	{
		gdouble progress = ccm_timeline_get_progress(self->priv->timeline);
		
		self->priv->new_scale = scale;
		if (ccm_timeline_is_playing(self->priv->timeline))
		{
			if (progress > 0.75)
				ccm_timeline_advance(self->priv->timeline, 
									 (progress - 0.75) * 
									 ccm_timeline_get_n_frames(self->priv->timeline));
		}
		else if (self->priv->enabled) 
		{
			ccm_timeline_start(self->priv->timeline);
			ccm_timeline_rewind(self->priv->timeline);
		}
		cairo_surface_destroy(self->priv->surface_window_info);
		self->priv->surface_window_info = NULL;
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_magnifier_get_zoom_quality(CCMMagnifier *self)
{
	gboolean ret = FALSE;
	gchar* quality = 
	   ccm_config_get_string (self->priv->options [CCM_MAGNIFIER_ZOOM_QUALITY]);
	
	if (!quality)
	{
		if (self->priv->quality != CAIRO_FILTER_FAST)
		{
			self->priv->quality = CAIRO_FILTER_FAST;
			ccm_config_set_string (self->priv->options [CCM_MAGNIFIER_ZOOM_QUALITY], 
								   "fast");
			ret = TRUE;
		}
	}
	else
	{
		if (!g_ascii_strcasecmp (quality, "fast") && 
			self->priv->quality != CAIRO_FILTER_FAST)
		{
			self->priv->quality = CAIRO_FILTER_FAST;
			ret = TRUE;
		}
		else if (!g_ascii_strcasecmp (quality, "good") &&
				 self->priv->quality != CAIRO_FILTER_GOOD)
		{
			self->priv->quality = CAIRO_FILTER_GOOD;
			ret = TRUE;
		}
		else if (!g_ascii_strcasecmp (quality, "best") &&
				 self->priv->quality != CAIRO_FILTER_BEST)
		{
			self->priv->quality = CAIRO_FILTER_BEST;
			ret = TRUE;
		}
		g_free(quality);
	}
	
	return ret;
}

static gboolean
ccm_magnifier_get_shade_desktop(CCMMagnifier *self)
{
	gboolean shade =
	 ccm_config_get_boolean (self->priv->options [CCM_MAGNIFIER_SHADE_DESKTOP]);
	
	if (shade != self->priv->shade)
	{
		self->priv->shade = shade;
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_magnifier_get_border(CCMMagnifier *self)
{
	gint val, real =
	 ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_BORDER]);
	
	val = MAX(0, real);
	val = MIN(self->priv->area.width, val);
	val = MIN(self->priv->area.height, val);
	if (val != real)
		ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_BORDER], val);
	
	if (self->priv->border != val)
	{
		self->priv->border = val;
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
ccm_magnifier_get_restrict_area(CCMMagnifier* self)
{
	gint val, real;
	gdouble x, y, width, height;
	gboolean ret = FALSE;

	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_WIDTH]);
	if (real < 0)
	{
		self->priv->restrict_area.width = CCM_SCREEN_XSCREEN(self->priv->screen)->width;
	}
	else
	{
		val = MAX(0, real);
		val = MIN(CCM_SCREEN_XSCREEN(self->priv->screen)->width, val);
		if (real != val)
			ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_WIDTH], val);
			
		width = val;
		if (self->priv->restrict_area.width != width)
		{
			self->priv->restrict_area.width = width;
			ret = TRUE;
		}
	}
	
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT]);
	if (real < 0)
	{
		self->priv->restrict_area.height = CCM_SCREEN_XSCREEN(self->priv->screen)->height;
	}
	else
	{
		val = MAX(0, real);
		val = MIN(CCM_SCREEN_XSCREEN(self->priv->screen)->height, val);
		if (real != val)
			ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT], val);
			
		height = val;
		if (self->priv->restrict_area.height != height)
		{
			self->priv->restrict_area.height = height;
			ret = TRUE;
		}
	}
	
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_X]);
	if (real < 0)
	{
		self->priv->restrict_area.x = 0;
	}
	else
	{
		val = MAX(0, real);
		val = MIN(self->priv->restrict_area.width, val);
		if (real != val)
			ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_X], val);
			
		x = val;
		if (self->priv->restrict_area.x != x)
		{
			self->priv->restrict_area.x = x;
			ret = TRUE;
		}
	}
		
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_Y]);
	if (real < 0)
	{
		self->priv->restrict_area.y = 0;
	}
	else
	{
		val = MAX(0, real);
		val = MIN(self->priv->restrict_area.height, val);
		if (real != val)
			ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_RESTRICT_AREA_Y], val);
			
		y = val;
		if (self->priv->restrict_area.y != y)
		{
			self->priv->restrict_area.y = y;
			ret = TRUE;
		}
	}
	
	return ret;
}

static gboolean
ccm_magnifier_get_size(CCMMagnifier* self)
{
	gint val, real;
	gdouble x, y, width, height;
	gboolean ret = FALSE;

	self->priv->area.x = 0;
	self->priv->area.y = 0;
	
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_WIDTH]);
	val = MAX(10, real);
	val = MIN(80, val);
	if (real != val)
		ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_WIDTH], val);
		
	width = (gdouble)self->priv->restrict_area.width * (gdouble)val / 100.0;
	if (self->priv->area.width != width)
	{
		self->priv->area.width = width;
		ret = TRUE;
	}
	
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_HEIGHT]);
	val = MAX(10, real);
	val = MIN(80, val);
	if (real != val)
		ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_HEIGHT], val);
	
	height = (gdouble)self->priv->restrict_area.height * (gdouble)val / 100.0;
	
	if (self->priv->area.height != height)
	{
		self->priv->area.height = height;
		ret = TRUE;
	}
	
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_X]);
	if (real < 0)
	{
		self->priv->area.x = (self->priv->restrict_area.width - self->priv->area.width) / 2;
	}
	else
	{
		val = MAX(0, real);
		val = MIN(CCM_SCREEN_XSCREEN(self->priv->screen)->width, val);
		if (real != val)
			ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_X], val);
		
		x = val;
		if (self->priv->area.x != x)
		{
			self->priv->area.x = x;
			ret = TRUE;
		}
	}
		
	real = ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_Y]);
	if (real < 0)
	{
		self->priv->area.y = (self->priv->restrict_area.height - self->priv->area.height) / 2;
	}
	else
	{
		val = MAX(0, real);
		val = MIN(CCM_SCREEN_XSCREEN(self->priv->screen)->height, val);
		if (real != val)
			ccm_config_set_integer (self->priv->options [CCM_MAGNIFIER_Y], val);
		
		y = val;
		if (self->priv->area.y != y)
		{
			self->priv->area.y = y;
			ret = TRUE;
		}
	}
	
	return ret;
}

static void
ccm_magnifier_create_window_info(CCMMagnifier *self)
{
	PangoLayout *layout;
	PangoFontDescription *desc;
	cairo_rectangle_t geometry;
	cairo_t* context;
	int width, height;
	char* text;
	
	if (self->priv->surface_window_info) 
		cairo_surface_destroy(self->priv->surface_window_info);
	
	self->priv->surface_window_info = 
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
								   self->priv->restrict_area.width * 
								   (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f),
								   self->priv->restrict_area.height * 
								   (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f));
	context = cairo_create(self->priv->surface_window_info);
	cairo_set_operator(context, CAIRO_OPERATOR_CLEAR);
	cairo_paint(context);
	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
	
	geometry.width = self->priv->restrict_area.width * 
					 (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f),
	geometry.height = self->priv->restrict_area.height * 
					  (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f),
	geometry.x = 0,
	geometry.y = 0;
	
	cairo_rectangle_round (context, geometry.x, geometry.y, geometry.width, 
						   geometry.height, 20, 
						   CAIRO_CORNER_BOTTOMLEFT | CAIRO_CORNER_BOTTOMRIGHT);
	cairo_set_source_rgba (context, 1.0f, 1.0f, 1.0f, 0.8f);
	cairo_fill_preserve(context);
	cairo_set_line_width(context, 2.0f);
	cairo_set_source_rgba (context, 0.f, 0.f, 0.f, 1.0f);
	cairo_stroke(context);
			
	layout = pango_cairo_create_layout(context);
	text = g_strdup_printf("Zoom level = %i %%", (int)(self->priv->new_scale * 100));
	pango_layout_set_text (layout, text, -1);
	g_free(text);
	desc = pango_font_description_from_string("Sans Bold 18");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_get_pixel_size(layout, &width, &height);
	
	cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 1.0f);
	pango_cairo_update_layout (context, layout);
	cairo_move_to(context, (geometry.width - width) / 2.0f, 
				  (geometry.height - height) / 2.0f);
	pango_cairo_show_layout (context, layout);
	g_object_unref(layout);
	cairo_destroy(context);
}

static void
ccm_magnifier_paint_window_info(CCMMagnifier *self, cairo_t* context)
{
	if (!self->priv->surface_window_info)
	{
		ccm_magnifier_create_window_info(self);
	}
	
	if (self->priv->surface_window_info && 
		ccm_timeline_is_playing(self->priv->timeline))
	{
		cairo_rectangle_t geometry;
		gdouble progress = ccm_timeline_get_progress(self->priv->timeline);
		
		geometry.width = self->priv->restrict_area.width * 
						 (CCM_MAGNIFIER_WINDOW_INFO_WIDTH / 100.f);
		geometry.height = self->priv->restrict_area.height * 
			              (CCM_MAGNIFIER_WINDOW_INFO_HEIGHT / 100.f);
		geometry.x = (self->priv->restrict_area.width - geometry.width) / 2;
		if (progress <= 0.25)
			geometry.y = -geometry.height * (1 - (progress / 0.25));
		else if (progress > 0.25 && progress < 0.75)
			geometry.y = 0;
		else
			geometry.y = -geometry.height * ((progress - 0.75) / 0.25);	
		
		cairo_save(context);
		cairo_translate(context, geometry.x, geometry.y);
		cairo_set_source_surface(context, self->priv->surface_window_info, 0, 0);
		cairo_paint(context);
		cairo_restore(context);
	}
}

static void
ccm_magnifier_cursor_convert_to_rgba (CCMMagnifier *self, 
									  XFixesCursorImage *cursor_image)
{
	int i, count = cursor_image->width * cursor_image->height;
	
	for (i = 0; i < count; ++i) 
	{
		guint32 pixval = GUINT32_TO_LE (cursor_image->pixels[i]);
		cursor_image->pixels[i] = pixval;
	}
}

static void
ccm_magnifier_cursor_get_surface(CCMMagnifier* self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	XFixesCursorImage *cursor_image;
	gint cpt;
	
	if (self->priv->image_mouse)
		g_free (self->priv->image_mouse);
	if (self->priv->surface_mouse)
		cairo_surface_destroy (self->priv->surface_mouse);
	
	cursor_image = XFixesGetCursorImage (CCM_DISPLAY_XDISPLAY(display));
	ccm_magnifier_cursor_convert_to_rgba (self, cursor_image);
	
	self->priv->image_mouse = g_new0(guchar, cursor_image->width * cursor_image->height * 4);
	for (cpt = 0; cpt < cursor_image->width * cursor_image->height; cpt++)
	{
		self->priv->image_mouse[(cpt * 4) + 3] = (guchar)(cursor_image->pixels[cpt] >> 24);
		self->priv->image_mouse[(cpt * 4) + 2] = (guchar)((cursor_image->pixels[cpt] >> 16) & 0xff);
		self->priv->image_mouse[(cpt * 4) + 1] = (guchar)((cursor_image->pixels[cpt] >> 8) & 0xff);
		self->priv->image_mouse[(cpt * 4) + 0] = (guchar)(cursor_image->pixels[cpt] & 0xff);
	}
	self->priv->surface_mouse = cairo_image_surface_create_for_data (
												self->priv->image_mouse, 
												CAIRO_FORMAT_ARGB32,
												cursor_image->width, 
												cursor_image->height,
												cursor_image->width * 4);
	self->priv->x_mouse = cursor_image->x;
	self->priv->y_mouse = cursor_image->y;
	self->priv->x_hot = cursor_image->xhot;
	self->priv->y_hot = cursor_image->yhot;
	self->priv->mouse_width = cursor_image->width;
	self->priv->mouse_height = cursor_image->height;
	
	XFree(cursor_image);
}

static void
ccm_magnifier_cursor_get_position(CCMMagnifier*self)
{
	gboolean damaged = FALSE;
	gint old_x = self->priv->x_mouse, 
	     old_y = self->priv->y_mouse,
		 old_xhot = self->priv->x_hot,
		 old_yhot = self->priv->y_hot;
	
	ccm_magnifier_cursor_get_surface (self);
	
	if (self->priv->x_offset > self->priv->x_mouse - self->priv->border)
	{
		self->priv->x_offset = MAX(self->priv->restrict_area.x, self->priv->x_mouse - self->priv->border);
		damaged = TRUE;
	}
	if (self->priv->y_offset > self->priv->y_mouse  - self->priv->border)
	{
		self->priv->y_offset = MAX(self->priv->restrict_area.y, self->priv->y_mouse - self->priv->border);
		damaged = TRUE;
	}
	if (self->priv->x_offset + (self->priv->area.width / self->priv->scale) < 
		self->priv->x_mouse + self->priv->border)
	{
		self->priv->x_offset = self->priv->x_mouse + self->priv->border + 
							   self->priv->mouse_width - 
							   (self->priv->area.width / self->priv->scale);
		if (self->priv->x_offset + 
			(self->priv->area.width / self->priv->scale) > 
			self->priv->restrict_area.width)
			self->priv->x_offset = self->priv->restrict_area.width - 
								   (self->priv->area.width / self->priv->scale);
		damaged = TRUE;
	}
	if (self->priv->y_offset + (self->priv->area.height / self->priv->scale) < 
		self->priv->y_mouse + self->priv->border)
	{
		self->priv->y_offset = self->priv->y_mouse + self->priv->border + 
							   self->priv->mouse_height - 
							   (self->priv->area.height / self->priv->scale) ;
		if (self->priv->y_offset + 
			(self->priv->area.height / self->priv->scale) > 
			self->priv->restrict_area.height)
			self->priv->y_offset = self->priv->restrict_area.height - 
								  (self->priv->area.height / self->priv->scale);
		damaged = TRUE;
	}
	
	if (old_x - old_xhot != self->priv->x_mouse - self->priv->x_hot || 
	    old_y - old_yhot != self->priv->y_mouse - self->priv->y_hot)
	{
		if (damaged)
		{
			CCMRegion* damage;
			cairo_rectangle_t area;
		
			area.x = self->priv->x_offset;
			area.y = self->priv->y_offset;
			area.width = self->priv->area.width / self->priv->scale;
			area.height = self->priv->area.height / self->priv->scale;
			damage = ccm_region_rectangle (&area);
			ccm_debug("MAGNIFIER POSITION CHANGED");
			ccm_screen_damage_region (self->priv->screen, damage);
			ccm_region_destroy (damage);
		}
		else
		{
			CCMRegion* damage;
			cairo_rectangle_t area;
		
			area.x = old_x - old_xhot;
			area.y = old_y - old_yhot;
			area.width = self->priv->mouse_width;
			area.height = self->priv->mouse_height;
			damage = ccm_region_rectangle (&area);
			area.x = self->priv->x_mouse - self->priv->x_hot;
			area.y = self->priv->y_mouse - self->priv->y_hot;
			ccm_region_union_with_rect(damage, &area);
			ccm_screen_damage_region (self->priv->screen, damage);
			ccm_region_destroy (damage);
		}
	}
}

static void
ccm_magnifier_on_cursor_notify_event(CCMMagnifier* self, 
									 XFixesCursorNotifyEvent* event,
									 CCMDisplay* display)
{
	if (self->priv->enabled)
		ccm_magnifier_cursor_get_position (self);
}

static void
ccm_magnifier_on_option_changed(CCMMagnifier* self, CCMConfig* config)
{
	if (config == self->priv->options[CCM_MAGNIFIER_ENABLE])
	{
		ccm_magnifier_get_enable (self);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_ZOOM_LEVEL])
	{
		ccm_magnifier_get_scale (self);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_ZOOM_QUALITY] &&
			 ccm_magnifier_get_zoom_quality (self))
	{
		ccm_magnifier_cursor_get_surface(self);
		ccm_screen_damage (self->priv->screen);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_SHORTCUT])
	{
		ccm_magnifier_get_keybind (self);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_BORDER] &&
			 ccm_magnifier_get_border (self))
	{
		ccm_screen_damage (self->priv->screen);
	}
	else if (config == self->priv->options[CCM_MAGNIFIER_SHADE_DESKTOP] &&
			 ccm_magnifier_get_shade_desktop (self))
	{
		ccm_screen_damage (self->priv->screen);
	}
	else if ((config == self->priv->options[CCM_MAGNIFIER_WIDTH] ||
			  config == self->priv->options[CCM_MAGNIFIER_HEIGHT] ||
			  config == self->priv->options[CCM_MAGNIFIER_X] ||
			  config == self->priv->options[CCM_MAGNIFIER_Y] ||
			  config == self->priv->options[CCM_MAGNIFIER_RESTRICT_AREA_WIDTH] ||
			  config == self->priv->options[CCM_MAGNIFIER_RESTRICT_AREA_HEIGHT] ||
			  config == self->priv->options[CCM_MAGNIFIER_RESTRICT_AREA_X] ||
			  config == self->priv->options[CCM_MAGNIFIER_RESTRICT_AREA_Y]) &&
			 (ccm_magnifier_get_restrict_area(self) ||
			  ccm_magnifier_get_size (self)))
	{
		if (self->priv->surface) 
			cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		ccm_screen_damage (self->priv->screen);
	}
}

static void
ccm_magnifier_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMagnifier* self = CCM_MAGNIFIER(plugin);
	CCMDisplay* display = ccm_screen_get_display (screen);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++)
	{
		if (self->priv->options[cpt]) g_object_unref(self->priv->options[cpt]);
		self->priv->options[cpt] = ccm_config_new(CCM_SCREEN_NUMBER(screen), 
												  "magnifier", 
												  CCMMagnifierOptions[cpt]);
		g_signal_connect_swapped(self->priv->options[cpt], "changed",
								 G_CALLBACK(ccm_magnifier_on_option_changed), 
								 self);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	
	ccm_magnifier_get_enable(self);
	ccm_magnifier_get_scale(self);
	ccm_magnifier_get_zoom_quality (self);
	ccm_magnifier_get_shade_desktop (self);
	ccm_magnifier_get_restrict_area(self);
	ccm_magnifier_get_size(self);
	ccm_magnifier_get_border(self);
	ccm_magnifier_get_keybind(self);
	g_signal_connect_swapped(display, "xfixes-cursor-event", 
							 G_CALLBACK(ccm_magnifier_on_cursor_notify_event), 
							 self);
}

static gboolean
ccm_magnifier_screen_paint(CCMScreenPlugin* plugin, CCMScreen* screen,
						   cairo_t* context)
{
	CCMMagnifier* self = CCM_MAGNIFIER (plugin);
	
	gboolean ret = FALSE;
	
	if (self->priv->enabled && !self->priv->surface) 
	{
		cairo_t* ctx;
		
		self->priv->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 
								self->priv->area.width / self->priv->scale, 
								self->priv->area.height / self->priv->scale);
		ctx = cairo_create (self->priv->surface);
		cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
		cairo_paint(ctx);
		cairo_destroy (ctx);
		ccm_screen_damage (screen);
	}

	if (self->priv->enabled) 
	{
		CCMRegion* area = ccm_region_rectangle (&self->priv->area);
		cairo_rectangle_t *rects;
		CCMRegion* geometry;
		gint cpt, nb_rects;
		
		ccm_debug("MAGNIFIER PAINT SCREEN CLIP");
		cairo_save(context);
		
		geometry = ccm_region_rectangle (&self->priv->restrict_area);
		ccm_region_subtract (geometry, area);
		
		ccm_region_get_rectangles(geometry, &rects, &nb_rects);
		cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
		for (cpt = 0; cpt < nb_rects; cpt++)
			cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
							 rects[cpt].width, rects[cpt].height);
		cairo_clip(context);
		g_free(rects);
		ccm_region_destroy (area);
		ccm_region_destroy (geometry);
		
		ccm_magnifier_cursor_get_position (self);
		
		g_object_set(G_OBJECT(self->priv->screen), "buffered_pixmap", 
					 FALSE, NULL);
		ccm_debug("MAGNIFIER PAINT SCREEN");
	}
	
	ret = ccm_screen_plugin_paint(CCM_SCREEN_PLUGIN_PARENT (plugin), screen, 
								  context);
	
	if (self->priv->enabled) 
		cairo_restore (context);
	
	if (ret && self->priv->enabled) 
	{
		CCMRegion* area = ccm_region_rectangle (&self->priv->area);
				
		ccm_debug("MAGNIFIER PAINT SCREEN CONTENT");
	
		ccm_screen_undamage_region(self->priv->screen, area);
		
		if (self->priv->shade)
		{
			CCMRegion* damaged = ccm_screen_get_damaged (self->priv->screen);
			
			if (damaged)
			{
				gint cpt, nb_rects;
				cairo_rectangle_t* rects;
				
				ccm_debug("MAGNIFIER PAINT SCREEN SHADE");

				cairo_save(context);
				cairo_rectangle(context, self->priv->restrict_area.x, 
								self->priv->restrict_area.y,
								self->priv->restrict_area.width,
								self->priv->restrict_area.height);
				cairo_clip(context);
				ccm_region_get_rectangles(damaged, &rects, &nb_rects);
				cairo_set_source_rgba (context, 0.0f, 0.0f, 0.0f, 0.6f);
				for (cpt = 0; cpt < nb_rects; cpt++)
				{
					cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
									 rects[cpt].width, rects[cpt].height);
					cairo_fill(context);
				}
				g_free(rects);
				
				cairo_set_source_rgba(context, 1.0f, 1.0f, 1.0f, 0.8f);
				cairo_set_line_width (context, 10.0f);
				cairo_rectangle_round(context, self->priv->area.x - 5, 
									  self->priv->area.y - 5,
									  self->priv->area.width + 10, 
									  self->priv->area.height + 10, 
									  10.0f, CAIRO_CORNER_ALL);
				cairo_stroke(context);
				cairo_set_source_rgba(context, 0.0f, 0.0f, 0.0f, 0.9f);
				cairo_set_line_width (context, 2.0f);
				cairo_rectangle_round(context, self->priv->area.x - 1, 
									  self->priv->area.y - 1,
									  self->priv->area.width + 2, 
									  self->priv->area.height + 2, 
									  10.0f, CAIRO_CORNER_ALL);
				cairo_stroke(context);
				
				ccm_magnifier_paint_window_info(self, context);
				
				cairo_restore(context);
			}
		}
	
		if (self->priv->damaged)
		{
			gint cpt, nb_rects;
			cairo_rectangle_t* rects;
			cairo_pattern_t* pattern;
			cairo_t* ctx = cairo_create(self->priv->surface);
			
			ccm_debug("MAGNIFIER PAINT SCREEN FILL PAINT MOUSE");
			cairo_set_source_surface (ctx, self->priv->surface_mouse, 
									  self->priv->x_mouse - 
									  self->priv->x_offset - 
									  self->priv->x_hot, 
									  self->priv->y_mouse - 
									  self->priv->y_offset -
									  self->priv->y_hot);
			cairo_paint(ctx);
			cairo_destroy (ctx);
			
			ccm_debug("MAGNIFIER PAINT SCREEN FILL CONTENT");
	
			ccm_screen_add_damaged_region(screen, self->priv->damaged);
		
			cairo_save(context);
		
			ccm_debug("MAGNIFIER PAINT SCREEN FILL TRANSLATE SCALE");
					
			ccm_region_get_rectangles(self->priv->damaged, &rects, &nb_rects);
			for (cpt = 0; cpt < nb_rects; cpt++)
				cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
								 rects[cpt].width, rects[cpt].height);
			g_free(rects);
			cairo_clip(context);
				
			ccm_debug("MAGNIFIER PAINT SCREEN FILL CLIP");
	
			cairo_translate(context, self->priv->area.x, self->priv->area.y);
			cairo_scale(context, self->priv->scale, self->priv->scale);
			cairo_set_source_surface (context, self->priv->surface, 0, 0);
			pattern = cairo_get_source (context);
			cairo_pattern_set_filter (pattern, self->priv->quality);
			cairo_paint (context);
			
			ccm_debug("MAGNIFIER PAINT SCREEN FILL PAINT");
			
			cairo_restore (context);
			
			ccm_region_destroy (self->priv->damaged);
			self->priv->damaged = NULL;
		}
		ccm_debug("MAGNIFIER PAINT SCREEN END");
	
		ccm_region_destroy (area);
	}
	
	if (self->priv->scale != self->priv->new_scale)
	{
		self->priv->scale = self->priv->new_scale;
		if (self->priv->surface) cairo_surface_destroy(self->priv->surface);
		self->priv->surface = NULL;
	}

	return ret;
}

static gboolean
ccm_magnifier_window_paint(CCMWindowPlugin* plugin, CCMWindow* window,
						   cairo_t* context, cairo_surface_t* surface,
						   gboolean y_invert)
{
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMMagnifier* self = CCM_MAGNIFIER(_ccm_screen_get_plugin (screen, 
														CCM_TYPE_MAGNIFIER));
	CCMRegion *damaged = NULL;
	
	g_object_get(G_OBJECT(window), "damaged", &damaged, NULL);
	if (damaged && self->priv->enabled) 
	{
		CCMRegion *area = ccm_region_rectangle (&self->priv->area);
		CCMRegion* tmp = ccm_region_copy(damaged);
		cairo_matrix_t matrix;
		
		cairo_matrix_init_scale(&matrix, self->priv->scale, self->priv->scale);
		cairo_matrix_translate(&matrix, -self->priv->x_offset, -self->priv->y_offset);
		ccm_region_transform(tmp, &matrix);
		cairo_matrix_init_translate(&matrix, self->priv->area.x, self->priv->area.y);
		ccm_region_transform(tmp, &matrix);
		ccm_region_intersect (tmp, area);
		
		if (!ccm_region_empty (tmp)) 
		{
			cairo_t* ctx = cairo_create (self->priv->surface);
			cairo_path_t* damaged_path;
			cairo_matrix_t matrix, initial, translate;
			
			ccm_debug_window(window, "MAGNIFIER PAINT WINDOW");

			if (!self->priv->damaged)
				self->priv->damaged = ccm_region_copy (tmp);
			else
				ccm_region_union (self->priv->damaged, tmp);
			
			ccm_window_get_transform (window, &initial);
			ccm_window_get_transform (window, &matrix);
			
			cairo_rectangle (ctx, 0, 0,
							 self->priv->area.width / self->priv->scale, 
							 self->priv->area.height / self->priv->scale);
			cairo_clip(ctx);
			
			cairo_matrix_init_translate (&translate, 
										 -self->priv->x_offset, 
										 -self->priv->y_offset);
			cairo_matrix_multiply (&matrix, &matrix, &translate);
			ccm_window_set_transform (window, &matrix, FALSE);
			
			cairo_translate (ctx, -self->priv->x_offset, -self->priv->y_offset);
			damaged_path = ccm_drawable_get_damage_path(CCM_DRAWABLE(window), ctx);
			cairo_clip (ctx);
			cairo_path_destroy (damaged_path);
			
			g_object_set(G_OBJECT(window), "use_image", TRUE, NULL);
			ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								    ctx, surface, y_invert);
			cairo_destroy (ctx);
			ccm_window_set_transform (window, &initial, FALSE);
		}
		
		ccm_region_destroy(tmp);
		ccm_region_destroy(area);
	}
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								   context, surface, y_invert);
}

static void
ccm_magnifier_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_magnifier_screen_load_options;
	iface->paint 			= ccm_magnifier_screen_paint;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
}

static void
ccm_magnifier_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	 = NULL;
	iface->query_geometry 	 = NULL;
	iface->paint 			 = ccm_magnifier_window_paint;
	iface->map				 = NULL;
	iface->unmap			 = NULL;
	iface->query_opacity  	 = NULL;
	iface->move				 = NULL;
	iface->resize			 = NULL;
	iface->set_opaque_region = NULL;
}
