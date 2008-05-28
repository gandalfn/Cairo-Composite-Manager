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

#include "ccm-debug.h"
#include "ccm-plugin.h"

enum
{
    PROP_0,
	PROP_PARENT
};

typedef struct {
	gpointer func;
	gint	 count;
} CCMPluginLock;

struct _CCMPluginPrivate
{
	GObject* 	parent;
};

#define CCMPLuginLockTable g_quark_from_static_string ("CCMPluginLockTable")

#define CCM_PLUGIN_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PLUGIN, CCMPluginPrivate))

G_DEFINE_TYPE (CCMPlugin, ccm_plugin, G_TYPE_OBJECT);

static void
ccm_plugin_set_property(GObject *object,
						guint prop_id,
						const GValue *value,
						GParamSpec *pspec)
{
	CCMPluginPrivate* priv = CCM_PLUGIN_GET_PRIVATE(object);
    
	switch (prop_id)
    {
    	case PROP_PARENT:
			priv->parent = g_value_get_pointer (value);
			break;
    	default:
			break;
    }
}

static void
ccm_plugin_get_property (GObject* object,
						 guint prop_id,
						 GValue* value,
						 GParamSpec* pspec)
{
	CCMPluginPrivate* priv = CCM_PLUGIN_GET_PRIVATE(object);
    
    switch (prop_id)
    {
    	case PROP_PARENT:
			g_value_set_pointer (value, priv->parent);
			break;
    	default:
			break;
    }
}

static void
ccm_plugin_init (CCMPlugin *self)
{
	self->priv = CCM_PLUGIN_GET_PRIVATE(self);
	self->priv->parent = NULL;
}

static void
ccm_plugin_finalize (GObject *object)
{
	CCMPlugin* self = CCM_PLUGIN(object);
	
	if (self->priv->parent && CCM_IS_PLUGIN(self->priv->parent)) 
		g_object_unref(self->priv->parent);
	
	G_OBJECT_CLASS (ccm_plugin_parent_class)->finalize (object);
}

static void
ccm_plugin_class_init (CCMPluginClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPluginPrivate));

	object_class->get_property = ccm_plugin_get_property;
    object_class->set_property = ccm_plugin_set_property;
	object_class->finalize = ccm_plugin_finalize;
	
	g_object_class_install_property(object_class, PROP_PARENT,
		g_param_spec_pointer ("parent",
							  "Parent",
							  "Parent plugin",
							  G_PARAM_READWRITE));
}

static GHashTable*
_ccm_plugin_get_lock_table(GObject* obj, gboolean create)
{
	g_return_val_if_fail(obj != NULL, NULL);
	
	GHashTable* lock_table = (GHashTable*)g_object_get_qdata (obj, CCMPLuginLockTable);
															 
	if (create && !lock_table)
	{
		lock_table = g_hash_table_new_full (g_str_hash, g_str_equal, 
											NULL, g_free);
		g_object_set_qdata_full (obj, CCMPLuginLockTable, (gpointer)lock_table,
								 (GDestroyNotify)g_hash_table_destroy);
	}
	
	return lock_table;
}

gboolean 
_ccm_plugin_method_locked(GObject* obj, gpointer func)
{
	g_return_val_if_fail(obj != NULL, FALSE);
	
	GHashTable* lock_table = _ccm_plugin_get_lock_table (obj, FALSE);
	
	if (lock_table == NULL || func == NULL) return FALSE;
	
	CCMPluginLock* lock = g_hash_table_lookup (lock_table, func);
	
	return lock != NULL;
}

void
_ccm_plugin_lock_method(GObject* obj, gpointer func)
{
	g_return_if_fail(obj != NULL);
	
	GHashTable* lock_table = _ccm_plugin_get_lock_table (obj, TRUE);
	
	if (lock_table == NULL || func == NULL) return;
	
	CCMPluginLock* lock = g_hash_table_lookup (lock_table, func);
	
	if (lock)
	{
		lock->count++;
	}
	else
	{
		lock = g_new(CCMPluginLock, 1);
		lock->func = func;
		lock->count = 1;
		g_hash_table_insert(lock_table, func, lock);
	}
	ccm_debug("LOCK COUNT %i", lock->count);
}

gboolean
_ccm_plugin_unlock_method(GObject* obj, gpointer func)
{
	g_return_val_if_fail(obj != NULL, FALSE);
	g_return_val_if_fail(func != NULL, FALSE);
	
	GHashTable* lock_table = _ccm_plugin_get_lock_table (obj, TRUE);
	
	if (lock_table == NULL || func == NULL) return FALSE;
	
	CCMPluginLock* lock = g_hash_table_lookup (lock_table, func);
	
	gboolean ret = FALSE;
	
	if (!lock)
	{
		ret = TRUE;
		ccm_debug("UNLOCK NOTLOCKED");
	}
	else if (!(--lock->count))
	{
		g_hash_table_remove (lock_table, func);
		ccm_debug("UNLOCK");
		ret = TRUE;
	}
	else
		ccm_debug("UNLOCK LOCK COUNT %i", lock->count);
	
	return ret;
}

GObject*
ccm_plugin_get_parent(CCMPlugin* self)
{
	g_return_val_if_fail(self != NULL, NULL);
	
	GObject* val; g_object_get(self, "parent", &val, NULL); return val;
}

void 
ccm_plugin_set_parent(CCMPlugin* self, GObject* parent)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(parent != NULL);
	
	g_object_set (G_OBJECT (self), "parent", parent, NULL);
}
