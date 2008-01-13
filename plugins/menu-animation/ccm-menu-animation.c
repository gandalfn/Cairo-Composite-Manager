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

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-screen.h"
#include "ccm-animation.h"
#include "ccm-menu-animation.h"
#include "ccm.h"

enum
{
	CCM_MENU_ANIMATION_NONE = 0,
	CCM_MENU_ANIMATION_ON_MAP = 1 << 1,
	CCM_MENU_ANIMATION_ON_UNMAP = 1 << 2
};

enum
{
	CCM_MENU_ANIMATION_DURATION,
	CCM_MENU_ANIMATION_OPTION_N
};

static gchar* CCMMenuAnimationOptions[CCM_MENU_ANIMATION_OPTION_N] = {
	"duration"
};

static void ccm_menu_animation_window_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMMenuAnimation, ccm_menu_animation, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_menu_animation,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_menu_animation_window_iface_init))

struct _CCMMenuAnimationPrivate
{	
	CCMWindow*     window;
	
	CCMWindowType  type;
	CCMRegion*	   opaque;
	gfloat		   scale;
	gint 		   way;
	
	CCMAnimation*  animation;
	
	CCMConfig*     options[CCM_MENU_ANIMATION_OPTION_N];
};

#define CCM_MENU_ANIMATION_GET_PRIVATE(o)  \
   ((CCMMenuAnimationPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MENU_ANIMATION, CCMMenuAnimationClass))

static void
ccm_menu_animation_init (CCMMenuAnimation *self)
{
	self->priv = CCM_MENU_ANIMATION_GET_PRIVATE(self);
	self->priv->window = NULL;
	self->priv->opaque = NULL;
	self->priv->way = CCM_MENU_ANIMATION_NONE;
	self->priv->scale = 1.0f;
	self->priv->type = CCM_WINDOW_TYPE_UNKNOWN;
	self->priv->animation = NULL;
}

static void
ccm_menu_animation_finalize (GObject *object)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(object);
	
	if (self->priv->opaque)
		ccm_region_destroy (self->priv->opaque);
	
	if (self->priv->animation) 
		g_object_unref (self->priv->animation);
	
	G_OBJECT_CLASS (ccm_menu_animation_parent_class)->finalize (object);
}

static void
ccm_menu_animation_class_init (CCMMenuAnimationClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMMenuAnimationPrivate));
	
	object_class->finalize = ccm_menu_animation_finalize;
}

static gfloat
interpolate (gfloat t, gfloat begin, gfloat end, gfloat power)
{
    return (begin + (end - begin) * pow (t, power));
}

static gboolean
ccm_menu_animation_animation(CCMAnimation* animation, gfloat elapsed, CCMMenuAnimation* self)
{
	gboolean ret = FALSE;
	
	if (self->priv->way != CCM_MENU_ANIMATION_NONE)
	{
		gfloat duration = ccm_config_get_float(self->priv->options[CCM_MENU_ANIMATION_DURATION]);
		gfloat step = elapsed / duration;
		CCMRegion* geometry = ccm_drawable_get_geometry (CCM_DRAWABLE(self->priv->window));
		
		if (geometry && (self->priv->way & CCM_MENU_ANIMATION_ON_UNMAP))
		{
			CCMRegion* damage = ccm_region_copy (geometry);
			ccm_region_scale(damage, self->priv->scale, self->priv->scale);
			
			ccm_drawable_damage_region (CCM_DRAWABLE(self->priv->window), 
										damage);
			ccm_region_destroy (damage);
		}
				
		self->priv->scale = self->priv->way & CCM_MENU_ANIMATION_ON_MAP ? 
					interpolate(step, 0.1, 1.0, 1) : interpolate(step, 1.0, 0.1, 1);
		
		if (geometry && (self->priv->way & CCM_MENU_ANIMATION_ON_MAP))
		{
			CCMRegion* damage = ccm_region_copy (geometry);
			ccm_region_scale(damage, self->priv->scale, self->priv->scale);
			
			ccm_drawable_damage_region (CCM_DRAWABLE(self->priv->window), 
										damage);
			ccm_region_destroy (damage);
		}
		
		if (((self->priv->way & CCM_MENU_ANIMATION_ON_MAP) && 
			 self->priv->scale >= 1.0f) ||
			((self->priv->way & CCM_MENU_ANIMATION_ON_UNMAP) && 
			 self->priv->scale <= 0.1f))
		{
			if (self->priv->way & CCM_MENU_ANIMATION_ON_MAP)
			{
				ccm_window_plugin_map ((CCMWindowPlugin*)self->priv->window, 
									   self->priv->window);
				ccm_drawable_damage (CCM_DRAWABLE(self->priv->window));
			}
			if (self->priv->way & CCM_MENU_ANIMATION_ON_UNMAP)
			{
				ccm_window_plugin_unmap ((CCMWindowPlugin*)self->priv->window, 
										 self->priv->window);
			}
			self->priv->way = CCM_MENU_ANIMATION_NONE;
			ret = FALSE;
		}
		else
		{
			ret = TRUE;
		}
	}
	
	return ret;
}

static void
ccm_menu_animation_window_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MENU_ANIMATION_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "menu-animation", 
												  CCMMenuAnimationOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
	
	self->priv->animation = ccm_animation_new(screen, (CCMAnimationFunc)ccm_menu_animation_animation, self);
}

static CCMRegion*
ccm_menu_animation_query_geometry(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	CCMRegion* region;
	
	region = ccm_window_plugin_query_geometry(CCM_WINDOW_PLUGIN_PARENT(plugin),
											  window);
	self->priv->type = ccm_window_get_hint_type(window);
	
	return region;
}

static void
ccm_menu_animation_map(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	
	self->priv->type = ccm_window_get_hint_type(window);
		
	if (self->priv->type == CCM_WINDOW_TYPE_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_POPUP_MENU)
	{
		self->priv->window = window;
		if (self->priv->way == CCM_MENU_ANIMATION_NONE)
		{
			self->priv->scale = 0.1f;
			ccm_animation_start(self->priv->animation);
		}
		self->priv->way = CCM_MENU_ANIMATION_ON_MAP;
	}
	else
		ccm_window_plugin_map (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static void
ccm_menu_animation_unmap(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);

	if (self->priv->type == CCM_WINDOW_TYPE_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_DROPDOWN_MENU ||
		self->priv->type == CCM_WINDOW_TYPE_POPUP_MENU)
	{
		self->priv->window = window;
		if (self->priv->way == CCM_MENU_ANIMATION_NONE)
		{
			self->priv->scale = 1.0f;
			ccm_animation_start(self->priv->animation);
		}
		self->priv->way = CCM_MENU_ANIMATION_ON_UNMAP;
	}
	else
		ccm_window_plugin_unmap (CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

static gboolean
ccm_menu_animation_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
						 cairo_t* context, cairo_surface_t* surface)
{
	CCMMenuAnimation* self = CCM_MENU_ANIMATION(plugin);
	gboolean ret;
	
	cairo_save(context);
	if (self->priv->scale < 1.0f) 
	{
		cairo_matrix_t matrix;
		
		cairo_get_matrix (context, &matrix);
		cairo_matrix_scale (&matrix, self->priv->scale, self->priv->scale);
		cairo_set_matrix (context, &matrix);
	}
	ret = ccm_window_plugin_paint (CCM_WINDOW_PLUGIN_PARENT(plugin), window, 
								   context, surface);
	cairo_restore (context);
	
	return ret;
}

static void
ccm_menu_animation_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_menu_animation_window_load_options;
	iface->query_geometry 	= ccm_menu_animation_query_geometry;
	iface->paint 			= ccm_menu_animation_paint;
	iface->map				= ccm_menu_animation_map;
	iface->unmap			= ccm_menu_animation_unmap;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
	iface->move				= NULL;
	iface->resize			= NULL;
}

