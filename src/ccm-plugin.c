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
#include "ccm-object.h"
#include "ccm-plugin.h"

CCM_DEFINE_TYPE(CCMPluginOptions, ccm_plugin_options, G_TYPE_OBJECT);

typedef struct
{
	CCMPluginOptions*			options;
    CCMPluginOptionsChangedFunc callback;
    CCMPlugin*					plugin;
} CCMPluginOptionsChangedCallback;

struct _CCMPluginOptionsPrivate
{
	gint		screen;
    CCMConfig **configs;
    int			configs_size;
	GSList*		callbacks;
};

static GQuark CCMPLuginOptionsCallbackQuark;

#define CCM_PLUGIN_OPTIONS_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PLUGIN_OPTIONS, CCMPluginOptionsPrivate))

static void
_ccm_plugin_options_disconnect_callback(CCMPluginOptionsChangedCallback* callback)
{
	g_return_if_fail(callback != NULL);

	CCMPluginOptions* self = callback->options;
	g_object_steal_qdata(G_OBJECT(callback->plugin), CCMPLuginOptionsCallbackQuark);
	if (self->priv)
	{
		self->priv->callbacks = g_slist_remove(self->priv->callbacks, callback);
	}
	g_free(callback);
}

static void
ccm_plugin_options_init (CCMPluginOptions *self)
{
	self->priv = CCM_PLUGIN_OPTIONS_GET_PRIVATE(self);
	self->priv->screen = -1;
	self->priv->configs = NULL;
    self->priv->configs_size = 0;
	self->priv->callbacks = NULL;
}

static void
ccm_plugin_options_finalize (GObject * object)
{
    CCMPluginOptions *self = CCM_PLUGIN_OPTIONS (object);

	if (self->priv->configs)
	{
		gint cpt; 
		for (cpt = 0; cpt < self->priv->configs_size; cpt++)
        {
            ccm_debug ("%s FINALIZE CONFIG[%i]",
                       G_OBJECT_TYPE_NAME (self), cpt);
            g_object_unref (self->priv->configs[cpt]);
        }
        ccm_debug ("%s FINALIZE CONFIG TAB",
                   G_OBJECT_TYPE_NAME (self));
        g_slice_free1 (sizeof(CCMConfig*) * self->priv->configs_size, self->priv->configs);
        self->priv->configs = NULL;
    }
	self->priv->configs_size = 0;
	if (self->priv->callbacks)
	{
		GSList* copy = g_slist_copy(self->priv->callbacks);
		g_slist_foreach(copy, 
		                (GFunc)_ccm_plugin_options_disconnect_callback, 
		                NULL);
		g_slist_free(self->priv->callbacks);
		g_slist_free(copy);
	}
	self->priv->callbacks = NULL;
	
	G_OBJECT_CLASS (ccm_plugin_options_parent_class)->finalize (object);
}

static void
ccm_plugin_options_class_init (CCMPluginOptionsClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMPluginOptionsPrivate));

	CCMPLuginOptionsCallbackQuark = 
		g_quark_from_static_string ("CCMPLuginOptionsCallback");

	object_class->finalize = ccm_plugin_options_finalize;
}

static void 
ccm_plugins_options_on_changed(CCMPluginOptions* self, CCMConfig* config)
{
	GSList* item;
	int found = -1;
	gint index;
	
	if (CCM_PLUGIN_OPTIONS_GET_CLASS(self)->changed)
		CCM_PLUGIN_OPTIONS_GET_CLASS(self)->changed(self, config);

	for (index = 0; index < self->priv->configs_size && found == -1; index++)
	{
		if (self->priv->configs[index] == config)
			found = index;
	}

	if (found >= 0)
	{
		for (item = self->priv->callbacks; item; item = item->next)
		{
			CCMPluginOptionsChangedCallback* callback = item->data;
			callback->callback(callback->plugin, found);
		}
	}
}

static void
_ccm_plugin_options_load (CCMPluginOptions* self, gint screen, 
                          gchar * plugin_name, const gchar ** options_key, 
                          int nb_options, CCMPluginOptionsChangedFunc func,
                          CCMPlugin* plugin)
{
	gint cpt;

	self->priv->screen = screen;
		
	if (self->priv->configs == NULL)
    {
        ccm_debug ("%s CREATE CONFIG TAB", G_OBJECT_TYPE_NAME (self));
        self->priv->configs = g_slice_alloc0 (sizeof(CCMConfig*) * nb_options);
        self->priv->configs_size = nb_options;
    }

	if (func)
	{
		CCMPluginOptionsChangedCallback* callback = 
			g_new0(CCMPluginOptionsChangedCallback, 1);

		callback->options = self;
		callback->callback = func;
		callback->plugin = plugin;

		self->priv->callbacks = g_slist_prepend(self->priv->callbacks, callback);
		g_object_set_qdata_full(G_OBJECT(plugin), CCMPLuginOptionsCallbackQuark, callback,
		                        (GDestroyNotify)_ccm_plugin_options_disconnect_callback);
	}
	
    for (cpt = 0; cpt < nb_options; cpt++)
    {
        if (self->priv->configs[cpt] == NULL)
        {
            ccm_debug ("%s CREATE CONFIG[%i]",
                       G_OBJECT_TYPE_NAME (self), cpt);

            self->priv->configs[cpt] = ccm_config_new (screen, plugin_name,
                                                 (gchar *) options_key[cpt]);

			if (CCM_PLUGIN_OPTIONS_GET_CLASS(self)->changed)
			{
				CCM_PLUGIN_OPTIONS_GET_CLASS(self)->changed(self, self->priv->configs[cpt]);
				
				g_signal_connect_swapped (G_OBJECT(self->priv->configs[cpt]), "changed",
			                              G_CALLBACK (ccm_plugins_options_on_changed),
			                              self);
			}
			if (func) func(plugin, cpt);
        }
    }
}

enum
{
    PROP_0,
    PROP_PARENT,
    PROP_SCREEN
};

typedef struct
{
    CCMPluginUnlockFunc callback;
    gpointer data;
} CCMPluginLockCallback;

typedef struct
{
    gpointer func;
    gint count;
    GSList *callbacks;
} CCMPluginLock;

CCM_DEFINE_TYPE(CCMPlugin, ccm_plugin, G_TYPE_OBJECT);

struct _CCMPluginPrivate
{
    GObject *parent;
    guint screen;
    gulong *id_options_changed;
};

static GQuark CCMPLuginLockTable;

#define CCM_PLUGIN_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PLUGIN, CCMPluginPrivate))

static void
ccm_plugin_set_property (GObject * object, guint prop_id, const GValue * value,
                         GParamSpec * pspec)
{
    CCMPlugin *self = CCM_PLUGIN (object);

    switch (prop_id)
    {
        case PROP_PARENT:
            self->priv->parent = g_value_get_pointer (value);
            break;
        case PROP_SCREEN:
            self->priv->screen = g_value_get_uint (value);
            break;
        default:
            break;
    }
}

static void
ccm_plugin_get_property (GObject * object, guint prop_id, GValue * value,
                         GParamSpec * pspec)
{
    CCMPlugin *self = CCM_PLUGIN (object);

    switch (prop_id)
    {
        case PROP_PARENT:
            g_value_set_pointer (value, self->priv->parent);
            break;
        case PROP_SCREEN:
            g_value_set_uint (value, self->priv->screen);
            break;
        default:
            break;
    }
}

static void
ccm_plugin_init (CCMPlugin * self)
{
    self->priv = CCM_PLUGIN_GET_PRIVATE (self);
    self->priv->parent = NULL;
    self->priv->screen = 0;
}

static void
ccm_plugin_finalize (GObject * object)
{
    CCMPlugin *self = CCM_PLUGIN (object);

    if (self->priv->id_options_changed != NULL)
        g_free (self->priv->id_options_changed);
    self->priv->id_options_changed = NULL;

    if (self->priv->parent && CCM_IS_PLUGIN (self->priv->parent))
    {
        g_object_unref (self->priv->parent);
        self->priv->parent = NULL;
    }

    G_OBJECT_CLASS (ccm_plugin_parent_class)->finalize (object);
}

static void
ccm_plugin_class_init (CCMPluginClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

	CCMPLuginLockTable = g_quark_from_static_string ("CCMPluginLockTable");
	
    g_type_class_add_private (klass, sizeof (CCMPluginPrivate));

    klass->count = 0;
    klass->options = NULL;
    klass->options_size = 0;

    object_class->get_property = ccm_plugin_get_property;
    object_class->set_property = ccm_plugin_set_property;
    object_class->finalize = ccm_plugin_finalize;

    g_object_class_install_property (object_class, PROP_PARENT,
                                     g_param_spec_pointer ("parent", "Parent",
                                                           "Parent plugin",
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (object_class, PROP_SCREEN,
                                     g_param_spec_uint ("screen", "Screen",
                                                        "Plugin screen number",
                                                        0, G_MAXUINT, 0,
                                                        G_PARAM_READWRITE));
}

static void
ccm_plugin_init_options (CCMPlugin * self)
{
    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    if (self->priv->screen >= klass->options_size)
    {
        if (klass->options == NULL)
        {
            ccm_debug ("%s CREATE OPTIONS TAB: %i", G_OBJECT_TYPE_NAME (self),
                       self->priv->screen + 1);
            klass->options =
                g_new0 (CCMPluginOptions *, self->priv->screen + 1);
        }
        else
        {
            ccm_debug ("%s RESIZE OPTIONS TAB: %i", G_OBJECT_TYPE_NAME (self),
                       self->priv->screen + 1);
            klass->options =
                g_renew (CCMPluginOptions *, klass->options,
                         self->priv->screen + 1);
        }
        klass->options[self->priv->screen] = NULL;
        klass->options_size = self->priv->screen + 1;
    }

    if (klass->options[self->priv->screen] == NULL)
    {
        ccm_debug ("%s INIT OPTIONS[%i]", G_OBJECT_TYPE_NAME (self),
                   self->priv->screen);
		klass->options[self->priv->screen] = g_object_new (klass->type_options, 
		                                                   NULL);
    }
}

static void
ccm_plugin_finalize_options (CCMPlugin * self)
{
    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    if (klass->options != NULL)
    {
        gint i;

        for (i = 0; i < klass->options_size; i++)
        {
            if (klass->options[i])
            {
				ccm_debug ("%s FINALIZE OPTIONS[%i]",
				           G_OBJECT_TYPE_NAME (self), i);
				g_object_unref(klass->options[i]);
                klass->options[i] = NULL;
            }
        }
        ccm_debug ("%s FINALIZE OPTIONS TAB", G_OBJECT_TYPE_NAME (self));
        g_free (klass->options);
        klass->options = NULL;
        klass->options_size = 0;
    }
}

static void
_ccm_plugin_lock_free (CCMPluginLock * lock)
{
    g_return_if_fail (lock != NULL);

    if (lock->callbacks != NULL)
    {
        g_slist_foreach (lock->callbacks, (GFunc) g_free, NULL);
        g_slist_free (lock->callbacks);
    }
    g_free (lock);
}

static GHashTable *
_ccm_plugin_get_lock_table (GObject * obj, gboolean create)
{
    g_return_val_if_fail (obj != NULL, NULL);

    GHashTable *lock_table =
        (GHashTable *) g_object_get_qdata (obj, CCMPLuginLockTable);

    if (create && !lock_table)
    {
        lock_table =
            g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                   (GDestroyNotify) _ccm_plugin_lock_free);
        g_object_set_qdata_full (obj, CCMPLuginLockTable, (gpointer) lock_table,
                                 (GDestroyNotify) g_hash_table_destroy);
    }

    return lock_table;
}

gboolean
_ccm_plugin_method_locked (GObject * obj, gpointer func)
{
    g_return_val_if_fail (obj != NULL, FALSE);

    GHashTable *lock_table = _ccm_plugin_get_lock_table (obj, FALSE);

    if (lock_table == NULL || func == NULL)
        return FALSE;

    CCMPluginLock *lock = g_hash_table_lookup (lock_table, func);

    return lock != NULL;
}

void
_ccm_plugin_lock_method (GObject * obj, gpointer func,
                         CCMPluginUnlockFunc callback, gpointer data)
{
    g_return_if_fail (obj != NULL);

    GHashTable *lock_table = _ccm_plugin_get_lock_table (obj, TRUE);

    if (lock_table == NULL || func == NULL)
        return;

    CCMPluginLock *lock = g_hash_table_lookup (lock_table, func);

    if (lock)
    {
        lock->count++;
    }
    else
    {
        lock = g_new0 (CCMPluginLock, 1);
        lock->func = func;
        lock->count = 1;
        g_hash_table_insert (lock_table, func, lock);
    }

    if (callback)
    {
        CCMPluginLockCallback *cb = g_new (CCMPluginLockCallback, 1);

        cb->callback = callback;
        cb->data = data;
        lock->callbacks = g_slist_prepend (lock->callbacks, cb);
    }
}

void
_ccm_plugin_unlock_method (GObject * obj, gpointer func)
{
    g_return_if_fail (obj != NULL);
    g_return_if_fail (func != NULL);

    GHashTable *lock_table = _ccm_plugin_get_lock_table (obj, TRUE);

    if (lock_table == NULL || func == NULL)
        return;

    CCMPluginLock *lock = g_hash_table_lookup (lock_table, func);

    if (!(--lock->count))
    {
        if (lock->callbacks)
        {
            GSList *item;

            for (item = lock->callbacks; item; item = item->next)
            {
                CCMPluginLockCallback *cb =
                    (CCMPluginLockCallback *) item->data;

                if (cb->callback)
                    cb->callback (cb->data);
            }
        }
        g_hash_table_remove (lock_table, func);
    }
}

void
ccm_plugin_options_load (CCMPlugin * self, gchar * plugin_name,
                         const gchar ** options_key, int nb_options,
                         CCMPluginOptionsChangedFunc callback)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (plugin_name != NULL);
    g_return_if_fail (options_key != NULL);
    g_return_if_fail (nb_options > 0);

    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    ccm_debug ("%s LOAD OPTIONS[%i] : %i", G_OBJECT_TYPE_NAME (self),
               self->priv->screen, klass->count);

    klass->count++;

    ccm_plugin_init_options (self);
    _ccm_plugin_options_load(klass->options[self->priv->screen], 
                             self->priv->screen, plugin_name, 
                             options_key, nb_options, callback, self);
}

void
ccm_plugin_options_unload (CCMPlugin * self)
{
    g_return_if_fail (self != NULL);

    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    if (klass->count > 0)
    {
        ccm_debug ("%s UNLOAD OPTIONS[%i] : %i", G_OBJECT_TYPE_NAME (self),
                   self->priv->screen, klass->count);

        klass->count--;
        if (klass->count == 0)
            ccm_plugin_finalize_options (self);
    }
}

int
ccm_plugin_options_get_screen_num (CCMPluginOptions* self)
{
    g_return_val_if_fail (self != NULL, -1);

    return self->priv->screen;
}

CCMConfig *
ccm_plugin_options_get_config (CCMPluginOptions * self, int index)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->configs[index];
}

CCMPluginOptions *
ccm_plugin_get_option (CCMPlugin * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    return klass->options[self->priv->screen];
}

CCMConfig *
ccm_plugin_get_config (CCMPlugin * self, int index)
{
    g_return_val_if_fail (self != NULL, NULL);

    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    return klass->options[self->priv->screen]->priv->configs[index];
}

GObject *
ccm_plugin_get_parent (CCMPlugin * self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->parent;
}

void
ccm_plugin_set_parent (CCMPlugin * self, GObject * parent)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (parent != NULL);

    g_object_set (G_OBJECT (self), "parent", parent, NULL);
}
