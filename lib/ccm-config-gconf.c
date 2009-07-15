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

#include "ccm-config-gconf.h"

#define CCM_CONFIG_GCONF_PREFIX "/apps/cairo-compmgr"

static gboolean ccm_config_gconf_initialize (CCMConfig* config, int screen, 
											 gchar* extension, gchar* key);

static CCMConfigValueType ccm_config_gconf_get_value_type(CCMConfig* config, 
                                                          GError** error);

static gboolean ccm_config_gconf_get_boolean(CCMConfig* config, GError** error);
static void ccm_config_gconf_set_boolean(CCMConfig* config, gboolean value, 
										 GError** error);

static gint ccm_config_gconf_get_integer(CCMConfig* config, GError** error);
static void ccm_config_gconf_set_integer(CCMConfig* config, gint value, 
										 GError** error);

static gfloat ccm_config_gconf_get_float(CCMConfig* config, GError** error);
static void ccm_config_gconf_set_float(CCMConfig* config, gfloat value, 
									   GError** error);

static gchar* ccm_config_gconf_get_string(CCMConfig* config, GError** error);
static void ccm_config_gconf_set_string(CCMConfig* config, gchar * value, 
										GError** error);
	
static GSList* ccm_config_gconf_get_string_list(CCMConfig* config, 
												GError** error);
static void ccm_config_gconf_set_string_list(CCMConfig* config, GSList * value,
											 GError** error);

static GSList* ccm_config_gconf_get_integer_list(CCMConfig* config, 
												 GError** error);
static void ccm_config_gconf_set_integer_list(CCMConfig* config, GSList * value,  
											  GError** error);

#define CCM_CONFIG_GCONF_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_GCONF, CCMConfigGConfPrivate))

G_DEFINE_TYPE (CCMConfigGConf, ccm_config_gconf, CCM_TYPE_CONFIG);

struct _CCMConfigGConfPrivate
{
	gchar* key;
	gint id_notify;
};

static void
ccm_config_gconf_init (CCMConfigGConf *self)
{
	self->priv = CCM_CONFIG_GCONF_GET_PRIVATE(self);
	self->priv->key = NULL;
	self->priv->id_notify = 0;
}

static void
ccm_config_gconf_finalize (GObject *object)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(object);
	
	if (self->priv->key) g_free(self->priv->key);
	self->priv->key = NULL;
	if (self->priv->id_notify) 
		gconf_client_notify_remove(CCM_CONFIG_GCONF_GET_CLASS(self)->client,
								   self->priv->id_notify);
	self->priv->id_notify = 0;
	
	G_OBJECT_CLASS (ccm_config_gconf_parent_class)->finalize (object);
}

static void
ccm_config_gconf_class_init (CCMConfigGConfClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMConfigGConfPrivate));

	klass->client = gconf_client_get_default ();
	gconf_client_add_dir (klass->client, CCM_CONFIG_GCONF_PREFIX,
						  GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
	
	CCM_CONFIG_CLASS(klass)->initialize = ccm_config_gconf_initialize;
	CCM_CONFIG_CLASS(klass)->get_value_type = ccm_config_gconf_get_value_type;
	CCM_CONFIG_CLASS(klass)->get_boolean = ccm_config_gconf_get_boolean;
	CCM_CONFIG_CLASS(klass)->set_boolean = ccm_config_gconf_set_boolean;
	CCM_CONFIG_CLASS(klass)->get_integer = ccm_config_gconf_get_integer;
	CCM_CONFIG_CLASS(klass)->set_integer = ccm_config_gconf_set_integer;
	CCM_CONFIG_CLASS(klass)->get_float = ccm_config_gconf_get_float;
	CCM_CONFIG_CLASS(klass)->set_float = ccm_config_gconf_set_float;
	CCM_CONFIG_CLASS(klass)->get_string = ccm_config_gconf_get_string;
	CCM_CONFIG_CLASS(klass)->set_string = ccm_config_gconf_set_string;
	CCM_CONFIG_CLASS(klass)->get_string_list = ccm_config_gconf_get_string_list;
	CCM_CONFIG_CLASS(klass)->set_string_list = ccm_config_gconf_set_string_list;
	CCM_CONFIG_CLASS(klass)->get_integer_list = ccm_config_gconf_get_integer_list;
	CCM_CONFIG_CLASS(klass)->set_integer_list = ccm_config_gconf_set_integer_list;
	
	object_class->finalize = ccm_config_gconf_finalize;
}

static void
ccm_config_gconf_on_change(GConfClient *client, guint cnxn_id, 
						   GConfEntry * entry, CCMConfigGConf* self)
{
	g_return_if_fail(self != NULL);
	
	ccm_config_changed (CCM_CONFIG(self));
}

static gboolean
ccm_config_gconf_copy_entry(CCMConfigGConf* self, gchar* src)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(src != NULL, FALSE);
	
	GConfEntry* entry;
	gboolean ret = FALSE;
	
	entry = gconf_client_get_entry(CCM_CONFIG_GCONF_GET_CLASS(self)->client,
								   src, NULL, TRUE, NULL);
	if (entry && entry->value &&
		gconf_engine_associate_schema (CCM_CONFIG_GCONF_GET_CLASS(self)->client->engine,
									   self->priv->key,
									   gconf_entry_get_schema_name (entry),
									   NULL))
	{
		gconf_client_set (CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
						  self->priv->key, entry->value, NULL);
		ret = TRUE;
	}
	if (entry) gconf_entry_unref(entry);
	
	return ret;
}

static gboolean
ccm_config_gconf_initialize (CCMConfig* config, int screen, 
							 gchar* extension, gchar* key)
{
	g_return_val_if_fail(key != NULL, FALSE);
	
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	if (screen >= 0)
	{
		if (extension)
		{
			GConfEntry* entry;
		
			self->priv->key = g_strdup_printf("%s/screen_%i/%s/%s", 
											  CCM_CONFIG_GCONF_PREFIX, 
											  screen, extension, key);
			
			entry = gconf_client_get_entry(CCM_CONFIG_GCONF_GET_CLASS(self)->client,
								           self->priv->key, NULL, TRUE, NULL);
			if (!entry || !entry->value)
			{
				gchar * default_config = g_strdup_printf("%s/default/%s/%s", 
														 CCM_CONFIG_GCONF_PREFIX, 
														 extension, key);
				if (!ccm_config_gconf_copy_entry(self, default_config))
				{
					g_free(default_config);
					return FALSE;
				}
				g_free(default_config);
			}
			if (entry) gconf_entry_unref(entry);
		}
		else
		{
			GConfEntry* entry;
			self->priv->key = g_strdup_printf("%s/screen_%i/general/%s", 
											  CCM_CONFIG_GCONF_PREFIX, screen, key);
			entry = gconf_client_get_entry(CCM_CONFIG_GCONF_GET_CLASS(self)->client,
								           self->priv->key, NULL, TRUE, NULL);
			if (!entry || !entry->value)
			{
				gchar * default_config = g_strdup_printf("%s/default/screen/%s", 
														 CCM_CONFIG_GCONF_PREFIX, key);
				if (ccm_config_gconf_copy_entry(self, default_config))
				{
					g_free(default_config);
					return FALSE;
				}
				g_free(default_config);
			}
			if (entry) gconf_entry_unref(entry);
		}
	}
	else if (screen == -1)
		self->priv->key = g_strdup_printf("%s/general/%s", 
										  CCM_CONFIG_GCONF_PREFIX, key);
	else
		self->priv->key = g_strdup(key);
	
	self->priv->id_notify = gconf_client_notify_add(
									CCM_CONFIG_GCONF_GET_CLASS(self)->client,
									self->priv->key, 
									(GConfClientNotifyFunc)ccm_config_gconf_on_change, 
									self, NULL, NULL);

	return TRUE;
}

static CCMConfigValueType
ccm_config_gconf_get_value_type(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	CCMConfigValueType ret = CCM_CONFIG_VALUE_INVALID;
	GConfValue* value;
	
	value = gconf_client_get(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
	                         self->priv->key, error);
	if (value)
	{
		GConfSchema* schema = gconf_value_get_schema (value);
		switch (gconf_schema_get_type (schema))
		{
			case GCONF_VALUE_BOOL:
				ret = CCM_CONFIG_VALUE_BOOLEAN;
				break;
			case GCONF_VALUE_INT:
				ret = CCM_CONFIG_VALUE_INTEGER;
				break;
			case GCONF_VALUE_STRING:
				ret = CCM_CONFIG_VALUE_STRING;
				break;
			case GCONF_VALUE_FLOAT:
				ret = CCM_CONFIG_VALUE_FLOAT;
				break;
			case GCONF_VALUE_LIST:
				ret = CCM_CONFIG_VALUE_LIST;
				switch (gconf_schema_get_list_type(schema))
				{
					case GCONF_VALUE_BOOL:
						ret = CCM_CONFIG_VALUE_LIST_BOOLEAN;
						break;
					case GCONF_VALUE_INT:
						ret = CCM_CONFIG_VALUE_LIST_INTEGER;
						break;
					case GCONF_VALUE_STRING:
						ret = CCM_CONFIG_VALUE_LIST_STRING;
						break;
					case GCONF_VALUE_FLOAT:
						ret = CCM_CONFIG_VALUE_LIST_FLOAT;
						break;
					default:
						break;
				}        
				break;
			default:
				break;
		}
	}
	
	return ret;
}

static gboolean
ccm_config_gconf_get_boolean(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	return gconf_client_get_bool(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
								 self->priv->key, error);
}
	
static void
ccm_config_gconf_set_boolean(CCMConfig* config, gboolean value, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	gconf_client_set_bool(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
						  self->priv->key, value, error);
}
	
static gint
ccm_config_gconf_get_integer(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	return gconf_client_get_int(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
								self->priv->key, error);
}
	
static void
ccm_config_gconf_set_integer(CCMConfig* config, gint value, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	gconf_client_set_int(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
						 self->priv->key, value, error);
}
	
static gfloat
ccm_config_gconf_get_float(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	return gconf_client_get_float(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
								  self->priv->key, error);
}
	
static void
ccm_config_gconf_set_float(CCMConfig* config, gfloat value, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	gconf_client_set_float(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
						   self->priv->key, value, error);
}
	
static gchar *
ccm_config_gconf_get_string(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	return gconf_client_get_string(CCM_CONFIG_GCONF_GET_CLASS(self)->client,
								   self->priv->key, error);
}
	
static void
ccm_config_gconf_set_string(CCMConfig* config, gchar * value, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	if (value)
		gconf_client_set_string(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
								self->priv->key, value, error);
}
	
static GSList*
ccm_config_gconf_get_string_list(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	return gconf_client_get_list(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
								 self->priv->key, GCONF_VALUE_STRING, error);
}
	
static void
ccm_config_gconf_set_string_list(CCMConfig* config, GSList * value, 
								 GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	gconf_client_set_list(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
						  self->priv->key, GCONF_VALUE_STRING, 
						  (GSList *)value, error);
}

static GSList*
ccm_config_gconf_get_integer_list(CCMConfig* config, GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	return gconf_client_get_list(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
								 self->priv->key, GCONF_VALUE_INT, error);
}
	
static void
ccm_config_gconf_set_integer_list(CCMConfig* config, GSList * value, 
								  GError** error)
{
	CCMConfigGConf* self = CCM_CONFIG_GCONF(config);
	
	gconf_client_set_list(CCM_CONFIG_GCONF_GET_CLASS(self)->client, 
						  self->priv->key, GCONF_VALUE_INT, 
						  (GSList *)value, error);
}
