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
#include <stdlib.h>

#include "ccm-config-schema.h"
#include "ccm-config-key.h"

static gboolean ccm_config_key_initialize (CCMConfig* config, int screen, 
										   gchar* extension, gchar* key);

static CCMConfigValueType ccm_config_key_get_value_type(CCMConfig* config, 
                                                        GError** error);

static gboolean ccm_config_key_get_boolean(CCMConfig* config, GError** error);
static void ccm_config_key_set_boolean(CCMConfig* config, gboolean value,
                                       GError** error);

static gint ccm_config_key_get_integer(CCMConfig* config, GError** error);
static void ccm_config_key_set_integer(CCMConfig* config, gint value, 
                                       GError** error);

static gfloat ccm_config_key_get_float(CCMConfig* config, GError** error);
static void ccm_config_key_set_float(CCMConfig* config, gfloat value, 
                                     GError** error);

static gchar* ccm_config_key_get_string(CCMConfig* config, GError** error);
static void ccm_config_key_set_string(CCMConfig* config, gchar * value, 
                                      GError** error);

static GSList* ccm_config_key_get_string_list(CCMConfig* config, 
												GError** error);
static void ccm_config_key_set_string_list(CCMConfig* config, GSList * value,
                                           GError** error);

static GSList* ccm_config_key_get_integer_list(CCMConfig* config, 
												 GError** error);
static void ccm_config_key_set_integer_list(CCMConfig* config, GSList * value,  
                                            GError** error);

#define CCM_CONFIG_KEY_GET_PRIVATE(o)  \
   ((CCMConfigKeyPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_KEY, CCMConfigKeyClass))

G_DEFINE_TYPE (CCMConfigKey, ccm_config_key, CCM_TYPE_CONFIG);

typedef struct _CCMConfigKeyMonitor CCMConfigKeyMonitor;

struct _CCMConfigKeyPrivate
{
	int					 screen;
	gchar*				 name;
	gchar*				 schema_name;
	gchar*				 key;
	gchar*				 value;
	CCMConfigSchema*	 schema;
	CCMConfigKeyMonitor* monitor;
};

struct _CCMConfigKeyMonitor
{
	gchar*		  filename;
	GKeyFile*	  keyfile;
	GSList*		  configs;
	GFileMonitor* monitor;
};

static void
ccm_config_key_monitor_changed (CCMConfigKeyMonitor* self,
                                GFile *file,
                                GFile *other_file,
                                GFileMonitorEvent event_type,
                                GFileMonitor* monitor)
{
	g_return_if_fail(self != NULL);

	switch (event_type) 
	{
		case G_FILE_MONITOR_EVENT_CREATED:
		case G_FILE_MONITOR_EVENT_CHANGED:
		{
			GSList* item;

			g_key_file_free(self->keyfile);
			self->keyfile = g_key_file_new();
			if (g_key_file_load_from_file(self->keyfile, self->filename, 
			                              G_KEY_FILE_NONE, NULL))
			{
				for (item = self->configs; item; item = item->next)
				{
					CCMConfigKey* config = CCM_CONFIG_KEY(item->data);
				
					gchar* value = g_key_file_get_string(self->keyfile,
					                                     config->priv->name,
					                                     config->priv->key,
					                                     NULL);

					if (value && (!config->priv->value ||
					    g_ascii_strcasecmp(config->priv->value, value)))
					{
						if (config->priv->value) g_free(config->priv->value);
						config->priv->value = g_strdup(value);
						ccm_config_changed (CCM_CONFIG(config));
					}
					if (value) g_free(value);
				}
			}
		}
		break;
		default:
		break;
	}
}

static void
ccm_config_key_monitor_sync(CCMConfigKeyMonitor* self)
{
	g_return_if_fail(self != NULL);

	gsize length;
	gchar* data = g_key_file_to_data(self->keyfile, &length, NULL);
	if (data)
	{
		GError* error = NULL;
		if (!g_file_set_contents(self->filename, data, length, &error))
		{
			g_warning("Error on write %s content: %s", self->filename,
			          error->message);
			g_error_free(error);
		}
		g_free(data);
	}
}

static CCMConfigKeyMonitor*
ccm_config_key_monitor_new(gchar* filename)
{
	g_return_val_if_fail(filename != NULL, NULL);
	
	GFile* file = NULL;
	GFileOutputStream* output = NULL;
	CCMConfigKeyMonitor* self = NULL;
	
	self = g_new0(CCMConfigKeyMonitor, 1);
	
	self->filename = g_strdup(filename);
	self->keyfile = g_key_file_new();
	g_key_file_set_list_separator(self->keyfile, ',');
	file = g_file_new_for_path(filename);
	output = g_file_create(file, G_FILE_CREATE_NONE, NULL, NULL);
	if (output) g_object_unref(output);
	g_key_file_load_from_file(self->keyfile, filename, G_KEY_FILE_NONE, NULL);
	self->configs = NULL;
	self->monitor = g_file_monitor(file, G_FILE_MONITOR_NONE, NULL, NULL);
	g_file_monitor_set_rate_limit(self->monitor, 250);
	if (!self->monitor) 
	{
		g_key_file_free(self->keyfile);
		g_free(self->filename);
		g_free(self);
		return NULL;
	}
	g_signal_connect_swapped(self->monitor, "changed", 
	                         G_CALLBACK(ccm_config_key_monitor_changed), self);
	
	return self;
}

static void
ccm_config_key_monitor_free(CCMConfigKeyMonitor* self)
{
	g_key_file_free(self->keyfile);
	self->keyfile = NULL;
	g_free(self->filename);
	self->filename = NULL;
	g_object_unref(self->monitor);
	self->monitor = NULL;
	g_slist_free(self->configs);
	g_free(self);
}

static void
ccm_config_key_init (CCMConfigKey *self)
{
	self->priv = CCM_CONFIG_KEY_GET_PRIVATE(self);
	self->priv->screen = -2;
	self->priv->name = NULL;
	self->priv->schema_name = NULL;
	self->priv->key = NULL;
	self->priv->value = NULL;
	self->priv->schema = NULL;
	self->priv->monitor = NULL;
}

static void
ccm_config_key_finalize (GObject *object)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(object);

	if (self->priv->name) g_free(self->priv->name);
	if (self->priv->schema_name) g_free(self->priv->schema_name);
	if (self->priv->key) g_free(self->priv->key);
	if (self->priv->value) g_free(self->priv->value);
	if (self->priv->monitor) 
		self->priv->monitor->configs = 
			g_slist_remove(self->priv->monitor->configs, self);
	
	G_OBJECT_CLASS (ccm_config_key_parent_class)->finalize (object);
}

static void
ccm_config_key_class_init (CCMConfigKeyClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMConfigKeyPrivate));

	gchar* config_dir = g_strdup_printf("%s/cairo-compmgr",
	                                    g_get_user_config_dir());
	if (!g_file_test(config_dir, G_FILE_TEST_EXISTS))
	{
		g_mkdir_with_parents(config_dir, 0755);
	}
	g_free(config_dir);
	
	klass->schemas = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_object_unref);
	klass->configs = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, 
	                                        (GDestroyNotify)ccm_config_key_monitor_free);
	
	CCM_CONFIG_CLASS(klass)->initialize = ccm_config_key_initialize;

	CCM_CONFIG_CLASS(klass)->get_value_type = ccm_config_key_get_value_type;
	CCM_CONFIG_CLASS(klass)->get_boolean = ccm_config_key_get_boolean;
	CCM_CONFIG_CLASS(klass)->set_boolean = ccm_config_key_set_boolean;
	CCM_CONFIG_CLASS(klass)->get_integer = ccm_config_key_get_integer;
	CCM_CONFIG_CLASS(klass)->set_integer = ccm_config_key_set_integer;
	CCM_CONFIG_CLASS(klass)->get_float = ccm_config_key_get_float;
	CCM_CONFIG_CLASS(klass)->set_float = ccm_config_key_set_float;
	CCM_CONFIG_CLASS(klass)->get_string = ccm_config_key_get_string;
	CCM_CONFIG_CLASS(klass)->set_string = ccm_config_key_set_string;
	CCM_CONFIG_CLASS(klass)->get_string_list = ccm_config_key_get_string_list;
	CCM_CONFIG_CLASS(klass)->set_string_list = ccm_config_key_set_string_list;
	CCM_CONFIG_CLASS(klass)->get_integer_list = ccm_config_key_get_integer_list;
	CCM_CONFIG_CLASS(klass)->set_integer_list = ccm_config_key_set_integer_list;
	
	object_class->finalize = ccm_config_key_finalize;
}

static CCMConfigSchema*
ccm_config_key_get_schema(CCMConfigKey* self, gchar* extension)
{
	CCMConfigSchema* schema = NULL;
		
	self->priv->name = extension ? g_strdup(extension) : g_strdup("general");
	self->priv->schema_name = extension ? g_strdup(extension) : 
								self->priv->screen < 0 ? g_strdup("display") : g_strdup("screen");
	
	schema = g_hash_table_lookup(CCM_CONFIG_KEY_GET_CLASS(self)->schemas, 
	                             self->priv->schema_name);
	if (!schema)
	{
		schema = ccm_config_schema_new(self->priv->screen, extension);
		if (schema)
			g_hash_table_insert(CCM_CONFIG_KEY_GET_CLASS(self)->schemas, 
			                    self->priv->schema_name, schema);
	}

	return schema;
}

static gboolean
ccm_config_key_initialize (CCMConfig* config, int screen, 
						   gchar* extension, gchar* key)
{
	g_return_val_if_fail(config != NULL, FALSE);
	
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	gchar* filename = NULL;
	
	self->priv->screen = screen;
	self->priv->schema = ccm_config_key_get_schema (self, extension);

	if (!self->priv->schema) return FALSE;

	self->priv->key = g_strdup(key);
	
	self->priv->monitor = 
		g_hash_table_lookup(CCM_CONFIG_KEY_GET_CLASS(self)->configs, 
		                    &screen);
	
	if (!self->priv->monitor)
	{
		if (screen == -1)
			filename = g_strdup_printf("%s/cairo-compmgr/ccm-display.conf", 
		    		                   g_get_user_config_dir());
		else if (screen >= 0)
			filename = g_strdup_printf("%s/cairo-compmgr/ccm-screen-%i.conf", 
				                       g_get_user_config_dir(), screen);
		else
			return FALSE;

		self->priv->monitor = ccm_config_key_monitor_new (filename);
		if (!self->priv->monitor)
		{
			g_free(filename);
			return FALSE;
		}
		g_hash_table_insert(CCM_CONFIG_KEY_GET_CLASS(self)->configs,
		                    &screen, self->priv->monitor);
		g_free(filename);
	}
	
	self->priv->monitor->configs = g_slist_prepend(self->priv->monitor->configs, 
	                                               self);
	self->priv->value = g_key_file_get_string(self->priv->monitor->keyfile,
	                                          self->priv->name, self->priv->key,
	                                          NULL);
	if (!self->priv->value)
	{
		self->priv->value = ccm_config_schema_get_default (self->priv->schema,
		                                                   self->priv->key);
		g_key_file_set_string(self->priv->monitor->keyfile,
		                      self->priv->name, self->priv->key,
		                      self->priv->value ? self->priv->value : "");
		ccm_config_key_monitor_sync (self->priv->monitor);
	}
	
	return TRUE;
}

static CCMConfigValueType
ccm_config_key_get_value_type(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	return ccm_config_schema_get_value_type (self->priv->schema,
	                                         self->priv->key);
}

static gboolean
ccm_config_key_get_boolean(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	return g_key_file_get_boolean(self->priv->monitor->keyfile,
	                              self->priv->name, self->priv->key, error);
}
	
static void
ccm_config_key_set_boolean(CCMConfig* config, gboolean value, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	g_key_file_set_boolean(self->priv->monitor->keyfile,
	                       self->priv->name, self->priv->key, value);
	ccm_config_key_monitor_sync (self->priv->monitor);
}
	
static gint
ccm_config_key_get_integer(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	return g_key_file_get_integer(self->priv->monitor->keyfile,
	                              self->priv->name, self->priv->key, error);
}
	
static void
ccm_config_key_set_integer(CCMConfig* config, gint value, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	g_key_file_set_integer(self->priv->monitor->keyfile,
	                       self->priv->name, self->priv->key, value);
	ccm_config_key_monitor_sync (self->priv->monitor);
}
	
static gfloat
ccm_config_key_get_float(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	return g_key_file_get_double(self->priv->monitor->keyfile,
	                             self->priv->name, self->priv->key, error);
}
	
static void
ccm_config_key_set_float(CCMConfig* config, gfloat value, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	g_key_file_set_double(self->priv->monitor->keyfile,
	                      self->priv->name, self->priv->key, value);
	ccm_config_key_monitor_sync (self->priv->monitor);
}

static gchar *
ccm_config_key_get_string(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	return g_key_file_get_string(self->priv->monitor->keyfile,
	                             self->priv->name, self->priv->key, error);
}

static void
ccm_config_key_set_string(CCMConfig* config, gchar * value, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	
	if (value)
	{
		g_key_file_set_string(self->priv->monitor->keyfile,
		                      self->priv->name, self->priv->key, value);
		ccm_config_key_monitor_sync (self->priv->monitor);
	}
}

static GSList*
ccm_config_key_get_string_list(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	GSList* result = NULL;
	gchar* value = g_key_file_get_string(self->priv->monitor->keyfile,
	                                     self->priv->name, self->priv->key, 
	                                     error);
	if (value)
	{
		gchar** list = g_strsplit(value, ",", -1);
		if (list)
		{
			gint cpt;
		
			for (cpt = 0; list[cpt]; cpt++)
				result = g_slist_prepend(result, g_strdup(list[cpt]));
			result = g_slist_reverse(result);
			g_strfreev(list);
		}
		g_free(value);
	}

	return result;
}
	
static void
ccm_config_key_set_string_list(CCMConfig* config, GSList * value, 
                               GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	gchar** result = NULL, *val = NULL;
	gint cpt, nb = g_slist_length(value);
	GSList* item;
	
	result = g_new0(gchar*, nb + 1);
	for (item = value, cpt = 0; item; item = item->next, cpt++)
		result[cpt] = g_strdup(item->data);
	val = g_strjoinv(",", result);
	g_strfreev(result);
	g_key_file_set_string(self->priv->monitor->keyfile,
	                      self->priv->name, self->priv->key, 
	                      val ? val : "");
	ccm_config_key_monitor_sync (self->priv->monitor);
	g_free(val);
}

static GSList*
ccm_config_key_get_integer_list(CCMConfig* config, GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	GSList* result = NULL;
	gchar* value = g_key_file_get_string(self->priv->monitor->keyfile,
	                                     self->priv->name, self->priv->key, 
	                                     error);
	if (value)
	{
		gchar** list = g_strsplit(value, ",", -1);
		if (list)
		{
			gint cpt;
		
			for (cpt = 0; list[cpt]; cpt++)
				result = g_slist_prepend(result, GINT_TO_POINTER(atoi(list[cpt])));
			result = g_slist_reverse(result);
			g_strfreev(list);
		}
		g_free(value);
	}

	return result;
}
	
static void
ccm_config_key_set_integer_list(CCMConfig* config, GSList * value, 
                                GError** error)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(config);
	gchar** result = NULL, *val = NULL;
	gint cpt, nb = g_slist_length(value);
	GSList* item;
	
	result = g_new0(gchar*, nb + 1);
	for (item = value, cpt = 0; item; item = item->next, cpt++)
		result[cpt] = g_strdup_printf("%i", GPOINTER_TO_INT(item->data));
	val = g_strjoinv(",", result);
	g_strfreev(result);

	g_key_file_set_string(self->priv->monitor->keyfile,
	                      self->priv->name, self->priv->key, 
	                      val ? val : "");
	ccm_config_key_monitor_sync (self->priv->monitor);

	g_free(val);
}
