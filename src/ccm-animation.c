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

#include "ccm-animation.h"

G_DEFINE_TYPE (CCMAnimation, ccm_animation, G_TYPE_OBJECT);

struct _CCMAnimationPrivate
{	
	CCMAnimationFunc callback;
	gpointer data;
	guint id;
	GTimer* timer;
};

#define CCM_ANIMATION_GET_PRIVATE(o)  \
   ((CCMAnimationPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_ANIMATION, CCMAnimationClass))

static void
ccm_animation_init (CCMAnimation *self)
{
	self->priv = CCM_ANIMATION_GET_PRIVATE(self);
	self->priv->callback = NULL;
	self->priv->data = NULL;
	self->priv->id = 0;
	self->priv->timer = g_timer_new();
}

static void
ccm_animation_finalize (GObject *object)
{
	CCMAnimation* self = CCM_ANIMATION(object);
	
	if (self->priv->id) g_source_remove(self->priv->id);
	if (self->priv->timer) g_timer_destroy (self->priv->timer);

	G_OBJECT_CLASS (ccm_animation_parent_class)->finalize (object);
}

static void
ccm_animation_class_init (CCMAnimationClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMAnimationPrivate));
	
	object_class->finalize = ccm_animation_finalize;
}

static gboolean
ccm_animation_main(CCMAnimation* self)
{
	g_return_val_if_fail(self != NULL, FALSE);

	gboolean ret = FALSE;
	
	if (self->priv->callback)
	{
		gfloat elapsed = g_timer_elapsed (self->priv->timer, NULL);
		
		ret = self->priv->callback(self, elapsed, self->priv->data);
	}
	
	if (!ret) 
	{
		self->priv->id = 0;
		g_timer_stop(self->priv->timer);
	}
	
	return ret;
}

CCMAnimation*
ccm_animation_new(CCMAnimationFunc callback, gpointer data)
{
	g_return_val_if_fail(callback != NULL, NULL);
	
	CCMAnimation* self = g_object_new (CCM_TYPE_ANIMATION, NULL);
	
	self->priv->callback = callback;
	self->priv->data = data;
	
	return self;
}

void
ccm_animation_start(CCMAnimation* self)
{
	g_return_if_fail(self != NULL);
	
	if (!self->priv->id)
	{
		self->priv->id = g_idle_add_full(G_PRIORITY_HIGH, (GSourceFunc)ccm_animation_main, self, NULL);
		g_timer_start(self->priv->timer);
	}
}

void
ccm_animation_stop(CCMAnimation* self)
{
	g_return_if_fail(self != NULL);
	
	if (self->priv->id)
	{
		g_source_remove (self->priv->id);
		self->priv->id = 0;
		g_timer_stop(self->priv->timer);
	}
}
