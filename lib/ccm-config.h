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

#ifndef _CCM_CONFIG_H_
#define _CCM_CONFIG_H_

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS
#define CCM_TYPE_CONFIG             (ccm_config_get_type ())
#define CCM_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG, CCMConfig))
#define CCM_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG, CCMConfigClass))
#define CCM_IS_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG))
#define CCM_IS_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG))
#define CCM_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG, CCMConfigClass))
typedef struct _CCMConfigClass CCMConfigClass;
typedef struct _CCMConfigPrivate CCMConfigPrivate;
typedef struct _CCMConfig CCMConfig;
typedef enum _CCMConfigError CCMConfigError;
typedef enum _CCMConfigValueType CCMConfigValueType;

struct _CCMConfigClass
{
    GObjectClass parent_class;

     gboolean (*initialize) (CCMConfig * config, int screen, gchar * extension,
                             gchar * key);

     CCMConfigValueType (*get_value_type) (CCMConfig * config, GError ** error);

     gboolean (*get_boolean) (CCMConfig * config, GError ** error);
    void (*set_boolean) (CCMConfig * config, gboolean value, GError ** error);

     gint (*get_integer) (CCMConfig * config, GError ** error);
    void (*set_integer) (CCMConfig * config, gint value, GError ** error);

     gfloat (*get_float) (CCMConfig * config, GError ** error);
    void (*set_float) (CCMConfig * config, gfloat value, GError ** error);

    gchar *(*get_string) (CCMConfig * config, GError ** error);
    void (*set_string) (CCMConfig * config, gchar * value, GError ** error);

    GSList *(*get_string_list) (CCMConfig * config, GError ** error);
    void (*set_string_list) (CCMConfig * config, GSList * value,
                             GError ** error);

    GSList *(*get_integer_list) (CCMConfig * config, GError ** error);
    void (*set_integer_list) (CCMConfig * config, GSList * value,
                              GError ** error);
};

struct _CCMConfig
{
    GObject parent_instance;

    CCMConfigPrivate *priv;
};

enum _CCMConfigError
{
    CCM_CONFIG_ERROR_NONE,
    CCM_CONFIG_ERROR_IS_NULL,
    CCM_CONFIG_ERROR_NOT_SUPPORTED
};

enum _CCMConfigValueType
{
    CCM_CONFIG_VALUE_INVALID,
    CCM_CONFIG_VALUE_BOOLEAN,
    CCM_CONFIG_VALUE_INTEGER,
    CCM_CONFIG_VALUE_STRING,
    CCM_CONFIG_VALUE_FLOAT,
    CCM_CONFIG_VALUE_LIST,
    CCM_CONFIG_VALUE_LIST_BOOLEAN,
    CCM_CONFIG_VALUE_LIST_INTEGER,
    CCM_CONFIG_VALUE_LIST_STRING,
    CCM_CONFIG_VALUE_LIST_FLOAT
};

GQuark ccm_config_error_quark ();

GType
ccm_config_get_type (void)
    G_GNUC_CONST;
CCMConfig *
ccm_config_new (int screen, gchar * extension, gchar * key);
void
ccm_config_set_backend (const gchar * backend);
void
ccm_config_changed (CCMConfig * self);
CCMConfigValueType
ccm_config_get_value_type (CCMConfig * self, GError ** error);
gboolean
ccm_config_get_boolean (CCMConfig * self, GError ** error);
void
ccm_config_set_boolean (CCMConfig * self, gboolean value, GError ** error);
gint
ccm_config_get_integer (CCMConfig * self, GError ** error);
void
ccm_config_set_integer (CCMConfig * self, gint value, GError ** error);
gfloat
ccm_config_get_float (CCMConfig * self, GError ** error);
void
ccm_config_set_float (CCMConfig * self, gfloat value, GError ** error);
gchar *
ccm_config_get_string (CCMConfig * self, GError ** error);
void
ccm_config_set_string (CCMConfig * self, gchar * value, GError ** error);
GSList *
ccm_config_get_string_list (CCMConfig * self, GError ** error);
void
ccm_config_set_string_list (CCMConfig * self, GSList * value, GError ** error);
GSList *
ccm_config_get_integer_list (CCMConfig * self, GError ** error);
void
ccm_config_set_integer_list (CCMConfig * self, GSList * value, GError ** error);
GdkColor *
ccm_config_get_color (CCMConfig * self, GError ** error);
void
ccm_config_set_color (CCMConfig * self, GdkColor * color, GError ** error);

G_END_DECLS
#endif                          /* _CCM_CONFIG_H_ */
