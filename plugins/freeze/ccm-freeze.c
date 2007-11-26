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

#include "ccm-drawable.h"
#include "ccm-screen.h"
#include "ccm-animation.h"
#include "ccm-freeze.h"
#include "ccm.h"

enum
{
	CCM_FREEZE_DURATION,
	CCM_FREEZE_OPTION_N
};

static gchar* CCMFreezeOptions[CCM_FREEZE_OPTION_N] = {
	"duration"
};

static void ccm_freeze_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMFreeze, ccm_freeze, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_freeze,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_freeze_iface_init))

struct _CCMFreezePrivate
{	
	CCMAnimation* animation;
	
	CCMConfig* options[CCM_FREEZE_OPTION_N];
};

#define CCM_FREEZE_GET_PRIVATE(o)  \
   ((CCMFreezePrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_FREEZE, CCMFreezeClass))

gboolean ccm_freeze_animation(CCMAnimation* animation, gfloat elapsed, 
							  CCMFreeze* self);
void ccm_freeze_load_options(CCMWindowPlugin* plugin, CCMWindow* window);
gboolean ccm_freeze_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
						  cairo_t* context, cairo_surface_t* suface);

static void
ccm_freeze_init (CCMFreeze *self)
{
	self->priv = CCM_FREEZE_GET_PRIVATE(self);
	self->priv->animation = 
		ccm_animation_new((CCMAnimationFunc)ccm_freeze_animation, self);
}

static void
ccm_freeze_finalize (GObject *object)
{
	CCMFreeze* self = CCM_FREEZE(object);
	
	if (self->priv->animation) g_object_unref (self->priv->animation);
	
	G_OBJECT_CLASS (ccm_freeze_parent_class)->finalize (object);
}

static void
ccm_freeze_class_init (CCMFreezeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMFreezePrivate));
	
	object_class->finalize = ccm_freeze_finalize;
}

static void
ccm_freeze_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= ccm_freeze_load_options;
	iface->query_geometry 	= NULL;
	iface->paint 			= ccm_freeze_paint;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
}

void
ccm_freeze_load_options(CCMWindowPlugin* plugin, CCMWindow* window)
{
	CCMFreeze* self = CCM_FREEZE(plugin);
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	gint cpt;
	
	for (cpt = 0; cpt < CCM_FREEZE_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "freeze", 
												  CCMFreezeOptions[cpt]);
	}
	ccm_window_plugin_load_options(CCM_WINDOW_PLUGIN_PARENT(plugin), window);
}

gboolean
ccm_freeze_animation(CCMAnimation* animation, gfloat elapsed, CCMFreeze* self)
{
	gboolean ret = FALSE;
	
	return ret;
}

gboolean 
ccm_freeze_paint(CCMWindowPlugin* plugin, CCMWindow* window, 
				 cairo_t* context, cairo_surface_t* surface)
{
	gboolean ret;
	CCMWindowType type;
	type = ccm_window_get_hint_type(window);
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window, 
								   context, surface);
	if (type == CCM_WINDOW_TYPE_NORMAL || type == CCM_WINDOW_TYPE_UNKNOWN)
	{	
		cairo_set_source_rgba(context, 1, 0, 0, 0.5);
		cairo_paint(context);
	}
	
	return ret;
}
