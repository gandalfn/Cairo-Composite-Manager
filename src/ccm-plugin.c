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

struct _CCMPluginPrivate
{
    GObject *parent;
    guint screen;
    gulong *id_options_changed;
};

#define CCMPLuginLockTable g_quark_from_static_string ("CCMPluginLockTable")

#define CCM_PLUGIN_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PLUGIN, CCMPluginPrivate))

static void ccm_plugin_init (CCMPlugin * self);
static void ccm_plugin_class_init (CCMPluginClass * klass);

static gpointer ccm_plugin_parent_class = NULL;

GType
ccm_plugin_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile))
    {
        static const GTypeInfo info = {
            sizeof (CCMPluginClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) ccm_plugin_class_init,
            (GClassFinalizeFunc) NULL,
            NULL,
            sizeof (CCMPlugin),
            10,
            (GInstanceInitFunc) ccm_plugin_init,
            NULL
        };

        GType g_define_type_id =
            g_type_register_static (G_TYPE_OBJECT, "CCMPlugin",
                                    &info, (GTypeFlags) 0);

        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

static void
ccm_plugin_set_property (GObject * object, guint prop_id, const GValue * value,
                         GParamSpec * pspec)
{
    CCMPluginPrivate *priv = CCM_PLUGIN_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_PARENT:
            priv->parent = g_value_get_pointer (value);
            break;
        case PROP_SCREEN:
            priv->screen = g_value_get_uint (value);
            break;
        default:
            break;
    }
}

static void
ccm_plugin_get_property (GObject * object, guint prop_id, GValue * value,
                         GParamSpec * pspec)
{
    CCMPluginPrivate *priv = CCM_PLUGIN_GET_PRIVATE (object);

    switch (prop_id)
    {
        case PROP_PARENT:
            g_value_set_pointer (value, priv->parent);
            break;
        case PROP_SCREEN:
            g_value_set_uint (value, priv->screen);
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

    ccm_plugin_parent_class = g_type_class_peek_parent (klass);

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
ccm_plugin_options_init (CCMPlugin * self)
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
        if (klass->options_init)
        {
            ccm_debug ("%s INIT OPTIONS[%i]", G_OBJECT_TYPE_NAME (self),
                       self->priv->screen);
            klass->options[self->priv->screen] = klass->options_init (self);
        }
        else
        {
            ccm_debug ("%s NEW OPTIONS[%i]", G_OBJECT_TYPE_NAME (self),
                       self->priv->screen);
            klass->options[self->priv->screen] = g_new0 (CCMPluginOptions, 1);
        }

        klass->options[self->priv->screen]->initialized = FALSE;
        klass->options[self->priv->screen]->configs = NULL;
        klass->options[self->priv->screen]->configs_size = 0;
    }
}

static void
ccm_plugin_options_finalize (CCMPlugin * self)
{
    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    if (klass->options != NULL)
    {
        gint i, j;

        for (i = 0; i < klass->options_size; i++)
        {
            if (klass->options[i])
            {
                if (klass->options[i]->configs)
                {
                    for (j = 0; j < klass->options[i]->configs_size; j++)
                    {
                        ccm_debug ("%s FINALIZE OPTIONS[%i].CONFIG[%i]",
                                   G_OBJECT_TYPE_NAME (self), i, j);
                        g_object_unref (klass->options[i]->configs[j]);
                    }
                    ccm_debug ("%s FINALIZE OPTIONS[%i].CONFIG TAB",
                               G_OBJECT_TYPE_NAME (self), i);
                    g_free (klass->options[i]->configs);
                    klass->options[i]->configs = NULL;
                }

                if (klass->options_finalize)
                {
                    ccm_debug ("%s FINALIZE OPTIONS[%i]",
                               G_OBJECT_TYPE_NAME (self), i);
                    klass->options_finalize (self, klass->options[i]);
                }
                else
                {
                    ccm_debug ("%s FREE OPTIONS[%i]", G_OBJECT_TYPE_NAME (self),
                               i);
                    g_free (klass->options[i]);
                }
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
                         const gchar ** options_key, int nb_options)
{
    g_return_if_fail (self != NULL);
    g_return_if_fail (plugin_name != NULL);
    g_return_if_fail (options_key != NULL);
    g_return_if_fail (nb_options > 0);

    gint cpt;
    CCMPluginClass *klass = CCM_PLUGIN_GET_CLASS (self);

    ccm_debug ("%s LOAD OPTIONS[%i] : %i", G_OBJECT_TYPE_NAME (self),
               self->priv->screen, klass->count);

    klass->count++;

    ccm_plugin_options_init (self);
    if (klass->options[self->priv->screen]->configs == NULL)
    {
        ccm_debug ("%s CREATE OPTIONS[%i].CONFIG TAB",
                   G_OBJECT_TYPE_NAME (self), self->priv->screen);
        klass->options[self->priv->screen]->configs =
            g_new0 (CCMConfig *, nb_options);
        klass->options[self->priv->screen]->configs_size = nb_options;
    }

    if (self->priv->id_options_changed == NULL)
        self->priv->id_options_changed = g_new0 (gulong, nb_options);

    for (cpt = 0; cpt < nb_options; cpt++)
    {
        if (klass->options[self->priv->screen]->configs[cpt] == NULL)
        {
            ccm_debug ("%s CREATE OPTIONS[%i].CONFIG[%i]",
                       G_OBJECT_TYPE_NAME (self), self->priv->screen, cpt);

            klass->options[self->priv->screen]->configs[cpt] =
                ccm_config_new (self->priv->screen, plugin_name,
                                (gchar *) options_key[cpt]);
        }

        if (klass->options[self->priv->screen]->configs[cpt] != NULL)
        {
            if (klass->option_changed)
            {
                if (klass->options[self->priv->screen]->initialized == FALSE)
                    klass->option_changed (self,
                                           klass->options[self->priv->screen]->
                                           configs[cpt]);

                self->priv->id_options_changed[cpt] =
                    g_signal_connect_swapped (G_OBJECT
                                              (klass->
                                               options[self->priv->screen]->
                                               configs[cpt]), "changed",
                                              G_CALLBACK (klass->
                                                          option_changed),
                                              self);
            }
        }
    }
    klass->options[self->priv->screen]->initialized = TRUE;
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

        if (klass->options && klass->option_changed
            && self->priv->screen < klass->options_size
            && klass->options[self->priv->screen] != NULL
            && self->priv->id_options_changed != NULL)
        {
            gint cpt;

            for (cpt = 0;
                 cpt < klass->options[self->priv->screen]->configs_size; cpt++)
                g_signal_handler_disconnect (klass->
                                             options[self->priv->screen]->
                                             configs[cpt],
                                             self->priv->
                                             id_options_changed[cpt]);
        }

        klass->count--;
        if (klass->count == 0)
            ccm_plugin_options_finalize (self);
    }
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

    return klass->options[self->priv->screen]->configs[index];
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
