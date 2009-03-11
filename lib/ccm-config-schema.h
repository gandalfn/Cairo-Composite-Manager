/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <nicolas.bruguier@supersonicimagine.fr>
 * 
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CCM_CONFIG_SCHEMA_H_
#define _CCM_CONFIG_SCHEMA_H_

#include <glib-object.h>

#include "ccm-config.h"

G_BEGIN_DECLS

#define CCM_TYPE_CONFIG_SCHEMA             (ccm_config_schema_get_type ())
#define CCM_CONFIG_SCHEMA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG_SCHEMA, CCMConfigSchema))
#define CCM_CONFIG_SCHEMA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG_SCHEMA, CCMConfigSchemaClass))
#define CCM_IS_CONFIG_SCHEMA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG_SCHEMA))
#define CCM_IS_CONFIG_SCHEMA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG_SCHEMA))
#define CCM_CONFIG_SCHEMA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG_SCHEMA, CCMConfigSchemaClass))

typedef struct _CCMConfigSchemaClass CCMConfigSchemaClass;
typedef struct _CCMConfigSchemaPrivate CCMConfigSchemaPrivate;
typedef struct _CCMConfigSchema CCMConfigSchema;

struct _CCMConfigSchemaClass
{
	GObjectClass parent_class;
};

struct _CCMConfigSchema
{
	GObject parent_instance;
	
	CCMConfigSchemaPrivate* priv;
};

GType               ccm_config_schema_get_type       (void) G_GNUC_CONST;

CCMConfigSchema*    ccm_config_schema_new            (int screen, gchar* name);
CCMConfigSchema*    ccm_config_schema_new_from_file  (int screen, 
                                                      gchar* filename);
CCMConfigValueType  ccm_config_schema_get_value_type (CCMConfigSchema* self,  
                                                      gchar* key);
gchar*              ccm_config_schema_get_description(CCMConfigSchema* self, 
                                                      gchar* locale, 
                                                      gchar* key);
gchar*              ccm_config_schema_get_default    (CCMConfigSchema* self, 
                                                      gchar* key);
gboolean            ccm_config_schema_write_gconf    (CCMConfigSchema* self, 
                                                      gchar* filename);

G_END_DECLS

#endif /* _CCM_CONFIG_SCHEMA_H_ */
