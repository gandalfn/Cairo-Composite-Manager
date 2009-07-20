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

#ifndef _CCM_PLUGIN_H_
#define _CCM_PLUGIN_H_

#include <glib-object.h>

#include "ccm-object.h"
#include "ccm-config.h"

G_BEGIN_DECLS

#define CCM_TYPE_PLUGIN             (ccm_plugin_get_type ())
#define CCM_PLUGIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_PLUGIN, CCMPlugin))
#define CCM_PLUGIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_PLUGIN, CCMPluginClass))
#define CCM_IS_PLUGIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PLUGIN))
#define CCM_IS_PLUGIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_PLUGIN))
#define CCM_PLUGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_PLUGIN, CCMPluginClass))
#define CCM_PLUGIN_PARENT(obj)      (ccm_plugin_get_parent(CCM_PLUGIN(obj)))

typedef struct _CCMPluginPrivate CCMPluginPrivate;
typedef struct _CCMPluginClass CCMPluginClass;
typedef struct _CCMPlugin CCMPlugin;
typedef struct _CCMPluginOptions CCMPluginOptions;

typedef void (*CCMPluginUnlockFunc) (gpointer data);

struct _CCMPluginClass
{
    GObjectClass parent_class;

    int count;
    CCMPluginOptions **options;
    int options_size;

    CCMPluginOptions* (*options_init)     (CCMPlugin* self);
    void              (*options_finalize) (CCMPlugin* self, CCMPluginOptions* options);
    void              (*option_changed)   (CCMPlugin* self, CCMConfig* config);
};

struct _CCMPlugin
{
    GObject parent_instance;

    CCMPluginPrivate *priv;
};

struct _CCMPluginOptions
{
    gboolean initialized;
    CCMConfig **configs;
    int configs_size;
};

GType ccm_plugin_get_type (void) G_GNUC_CONST;

GObject*          ccm_plugin_get_parent     (CCMPlugin* self);
void              ccm_plugin_set_parent     (CCMPlugin* self, GObject* parent);

void              ccm_plugin_options_load   (CCMPlugin* self, 
                                             gchar* plugin_name,
                                             const gchar** options_key, 
                                             int nb_options);
void              ccm_plugin_options_unload (CCMPlugin * self);
CCMPluginOptions* ccm_plugin_get_option     (CCMPlugin * self);
CCMConfig*        ccm_plugin_get_config     (CCMPlugin * self, int index);

#define CCM_DEFINE_PLUGIN(class_name, prefix, parent_class_type, CODE) \
\
static GType prefix##_type = 0; \
\
GType \
prefix##_get_type (void) \
{ \
	return prefix##_type; \
} \
\
G_MODULE_EXPORT GType \
prefix##_get_plugin_type (GTypeModule * plugin); \
\
static void 	prefix##_init              (class_name           *self); \
static void 	prefix##_class_init        (class_name##Class    *klass); \
\
static gpointer				prefix##_parent_class = NULL; \
\
static void \
prefix##_class_intern_init (gpointer klass) \
{ \
  prefix##_parent_class = g_type_class_peek_parent (klass); \
  prefix##_class_init ((class_name##Class*) klass); \
} \
\
static inline class_name##Options* \
prefix##_get_option(class_name *self) \
{ \
	return (class_name##Options*)ccm_plugin_get_option((CCMPlugin*)self); \
} \
\
static inline CCMConfig* \
prefix##_get_config(class_name *self, int option) \
{ \
	return ccm_plugin_get_config((CCMPlugin*)self, option); \
} \
\
GType \
prefix##_get_plugin_type (GTypeModule * plugin) \
{ \
	if (!prefix##_type) { \
		static const GTypeInfo type_info = { \
			sizeof (class_name##Class), \
			NULL, \
			NULL, \
			(GClassInitFunc)prefix##_class_intern_init, \
			NULL, \
			NULL, \
			sizeof (class_name), \
			CCM_OBJECT_PREFETCH, \
			(GInstanceInitFunc)prefix##_init \
		}; \
		prefix##_type = g_type_module_register_type (plugin, \
						    						 parent_class_type, \
													 #class_name, \
						    						 &type_info, 0); \
	} \
	{ CODE ; } \
	return prefix##_type; \
}

#define CCM_IMPLEMENT_INTERFACE(prefix, TYPE_IFACE, iface_init)       { \
  const GInterfaceInfo g_implement_interface_info = { \
    (GInterfaceInitFunc) iface_init, NULL, NULL \
  }; \
  g_type_module_add_interface (plugin, prefix##_type, TYPE_IFACE, &g_implement_interface_info); \
}

gboolean _ccm_plugin_method_locked (GObject* obj, gpointer func);
void     _ccm_plugin_lock_method   (GObject* obj, gpointer func,
                                    CCMPluginUnlockFunc callback, 
                                    gpointer data);
void     _ccm_plugin_unlock_method (GObject* obj, gpointer func);

G_END_DECLS

#endif                          /* _CCM_PLUGIN_H_ */
