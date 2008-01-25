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
#include <stdio.h>
#include <math.h>

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-extension-loader.h"
#include "ccm-magnifier.h"
#include "ccm.h"

enum
{
	CCM_MAGNIFIER_ZOOM_LEVEL,
	CCM_MAGNIFIER_X,
	CCM_MAGNIFIER_Y,
	CCM_MAGNIFIER_HEIGHT,
	CCM_MAGNIFIER_WIDTH,
	CCM_MAGNIFIER_OPTION_N
};

static gchar* CCMMagnifierOptions[CCM_MAGNIFIER_OPTION_N] = {
	"zoom-level",
	"x",
	"y",
	"height",
	"width"
};

static void ccm_magnifier_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_magnifier_window_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMMagnifier, ccm_magnifier, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_magnifier,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_magnifier_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_magnifier,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_magnifier_window_iface_init))

struct _CCMMagnifierPrivate
{	
	cairo_surface_t*     surface;
	cairo_rectangle_t    area;
	CCMConfig*           options[CCM_MAGNIFIER_OPTION_N];
};

#define CCM_MAGNIFIER_GET_PRIVATE(o)  \
   ((CCMMagnifierPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MAGNIFIER, CCMMagnifierClass))

static void
ccm_magnifier_init (CCMMagnifier *self)
{
	self->priv = CCM_MAGNIFIER_GET_PRIVATE(self);
	self->priv->surface = NULL;
}

static void
ccm_magnifier_finalize (GObject *object)
{
	CCMMagnifier* self = CCM_MAGNIFIER(object);
	
	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	
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
ccm_magnifier_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMagnifier* self = CCM_MAGNIFIER(plugin);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MAGNIFIER_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "magnifier", 
												  CCMMagnifierOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
}

static gboolean
ccm_magnifier_screen_paint(CCMScreenPlugin* plugin, CCMScreen* screen,
						   cairo_t* context)
{
	CCMMagnifier* self = CCM_MAGNIFIER (plugin);
	
	gboolean ret = FALSE;
	
	if (!self->priv->surface) 
	{
		cairo_t* ctx;
		gfloat scale;
			
		scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
		
		self->priv->area.x = 0;
		self->priv->area.y = 0;
		self->priv->area.width = (double)
			ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_WIDTH]);
		self->priv->area.height = (double)
			ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_HEIGHT]);
		
		self->priv->surface = cairo_image_surface_create (
												CAIRO_FORMAT_RGB24, 
												self->priv->area.width / scale, 
												self->priv->area.height / scale);
		ctx = cairo_create (self->priv->surface);
		cairo_set_operator (ctx, CAIRO_OPERATOR_CLEAR);
		cairo_paint(ctx);
		cairo_destroy (ctx);
		ccm_screen_damage (screen);
	}

	ret = ccm_screen_plugin_paint(CCM_SCREEN_PLUGIN_PARENT (plugin), screen, 
								  context);

	if (ret) 
	{
		gfloat scale;
			
		cairo_save(context);
		scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
		
		cairo_rectangle (context, 
						 ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_X]), 
						 ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_Y]),
						 self->priv->area.width, self->priv->area.height);
		cairo_clip(context);
		
		cairo_translate (context, ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_X]), 
								  ccm_config_get_integer (self->priv->options [CCM_MAGNIFIER_Y]));
		cairo_scale(context, scale, scale);
			
		cairo_set_source_surface (context, self->priv->surface, 0, 0);
		cairo_paint (context);
		cairo_restore (context);
	}

	return ret;
}

static gboolean
ccm_magnifier_window_paint(CCMWindowPlugin* plugin, CCMWindow* window,
						   cairo_t* context, cairo_surface_t* surface)
{
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMMagnifier* self = CCM_MAGNIFIER(_ccm_screen_get_plugin (screen, 
														CCM_TYPE_MAGNIFIER));
	CCMRegion *area = ccm_region_rectangle (&self->priv->area);
	CCMRegion *geometry = ccm_drawable_get_geometry (CCM_DRAWABLE (window));
	
	if (geometry) 
	{
		ccm_region_intersect (area, geometry);
		if (!ccm_region_empty (area)) 
		{
			cairo_t* ctx = cairo_create (self->priv->surface);
			cairo_path_t* damaged;
			gfloat scale;
			
			scale = ccm_config_get_float (self->priv->options [CCM_MAGNIFIER_ZOOM_LEVEL]);
		
			cairo_rectangle (ctx, 0, 0,
							 self->priv->area.width / scale, self->priv->area.height / scale);
			cairo_clip(ctx);
			
			damaged = ccm_drawable_get_damage_path(CCM_DRAWABLE(window), ctx);
			cairo_clip (ctx);
			cairo_path_destroy (damaged);
			
			ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								    ctx, surface);
			cairo_destroy (ctx);
		}
		
		ccm_region_destroy(area);
	}
	
	return ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								  context, surface);
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
	iface->load_options 	= NULL;
	iface->query_geometry 	= NULL;
	iface->paint 			= ccm_magnifier_window_paint;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
	iface->move				= NULL;
	iface->resize			= NULL;
}
