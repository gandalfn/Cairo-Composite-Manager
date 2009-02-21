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

#include "ccm-config-key.h"

static gboolean ccm_config_key_initialize (CCMConfig* config, int screen, 
										   gchar* extension, gchar* key);

#define CCM_CONFIG_KEY_GET_PRIVATE(o)  \
   ((CCMConfigKeyPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG_KEY, CCMConfigKeyClass))

G_DEFINE_TYPE (CCMConfigKey, ccm_config_key, CCM_TYPE_CONFIG);

struct _CCMConfigKeyPrivate
{
	gchar*			value;
	GFileMonitor*   monitor;
};

static void
ccm_config_key_init (CCMConfigKey *self)
{
	self->priv = CCM_CONFIG_KEY_GET_PRIVATE(self);
	self->priv->value = NULL;
	self->priv->monitor = NULL;
}

static void
ccm_config_key_finalize (GObject *object)
{
	CCMConfigKey* self = CCM_CONFIG_KEY(object);
	
	if (self->priv->value) g_free(self->priv->value);
	if (self->priv->monitor) g_object_unref(self->priv->monitor);
	
	G_OBJECT_CLASS (ccm_config_key_parent_class)->finalize (object);
}

static void
ccm_config_key_class_init (CCMConfigKeyClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMConfigKeyPrivate));

	CCM_CONFIG_CLASS(klass)->initialize = ccm_config_key_initialize;
	
	object_class->finalize = ccm_config_key_finalize;
}


static gboolean
ccm_config_key_initialize (CCMConfig* config, int screen, 
						   gchar* extension, gchar* key)
{
	g_return_val_if_fail(config != NULL, FALSE);
	
	
	return TRUE;
}
