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

#include "ccm-config.h"
#include "ccm-config-gconf.h"
#include "ccm-config-key.h"

#define CCM_CONFIG_ERROR_QUARK g_quark_from_string("CCMConfigError")

enum
{
	CHANGED,
    N_SIGNALS
};

enum
{
    PROP_0,
	PROP_SCREEN,
    PROP_EXTENSION,
	PROP_KEY
};

static guint signals[N_SIGNALS] = { 0 };
static GType backend_type = 0;

G_DEFINE_TYPE (CCMConfig, ccm_config, G_TYPE_OBJECT);

struct _CCMConfigPrivate
{
	int		screen;
	gchar*  extension;
	gchar*  key;
};

#define CCM_CONFIG_GET_PRIVATE(o)  \
   ((CCMConfigPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_CONFIG, CCMConfigClass))

static void
ccm_config_set_property(GObject *object,
						guint prop_id,
						const GValue *value,
						GParamSpec *pspec)
{
	CCMConfigPrivate* priv = CCM_CONFIG_GET_PRIVATE(object);
    
	switch (prop_id)
    {
    	case PROP_SCREEN:
			priv->screen = g_value_get_int (value);
			break;
		case PROP_EXTENSION:
			priv->extension = g_value_dup_string (value);
			break;
		case PROP_KEY:
			priv->key = g_value_dup_string (value);
			break;
		default:
			break;
    }
}

static void
ccm_config_get_property (GObject* object,
						 guint prop_id,
						 GValue* value,
						 GParamSpec* pspec)
{
    CCMConfigPrivate* priv = CCM_CONFIG_GET_PRIVATE(object);
    
    switch (prop_id)
    {
    	case PROP_SCREEN:
			g_value_set_int (value, priv->screen);
			break;
		case PROP_EXTENSION:
			g_value_set_string (value, priv->extension);
			break;
		case PROP_KEY:
			g_value_set_string (value, priv->key);
			break;
    	default:
			break;
    }
}

static void
ccm_config_init (CCMConfig *self)
{
	self->priv = CCM_CONFIG_GET_PRIVATE(self);
	self->priv->screen = -1;
	self->priv->extension = NULL;
	self->priv->key = NULL;
}

static void
ccm_config_finalize (GObject *object)
{
	CCMConfig* self = CCM_CONFIG(object);
	
	if (self->priv->extension) g_free(self->priv->extension);
	if (self->priv->key) g_free(self->priv->key);
	
	G_OBJECT_CLASS (ccm_config_parent_class)->finalize (object);
}

static void
ccm_config_class_init (CCMConfigClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CCMConfigPrivate));

	object_class->get_property = ccm_config_get_property;
    object_class->set_property = ccm_config_set_property;
	object_class->finalize = ccm_config_finalize;

	g_object_class_install_property(object_class, PROP_SCREEN,
		g_param_spec_int ("screen",
		                  "Screen",
		                  "Screen of config value",
		                  -2, G_MAXINT, -1, 
		                  G_PARAM_READWRITE));

	g_object_class_install_property(object_class, PROP_EXTENSION,
		g_param_spec_string ("extension",
		                     "Extension",
		              		 "Config extension name",
		                     NULL,
		              	     G_PARAM_READWRITE));

	g_object_class_install_property(object_class, PROP_KEY,
		g_param_spec_string ("key",
		                     "Key",
		              		 "Config key name",
		                     NULL,
		              	     G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new ("changed",
									 G_OBJECT_CLASS_TYPE (object_class),
									 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
									 g_cclosure_marshal_VOID__VOID,
									 G_TYPE_NONE, 0, G_TYPE_NONE);
}

GQuark
ccm_config_error_quark()
{
	return CCM_CONFIG_ERROR_QUARK;
}

void
ccm_config_set_backend(const gchar* backend)
{
	backend_type = CCM_TYPE_CONFIG_GCONF;
	
	if (!g_ascii_strcasecmp(backend, "key"))
		backend_type = CCM_TYPE_CONFIG_KEY;
}

void
ccm_config_changed(CCMConfig* self)
{
	g_return_if_fail(self != NULL);
	
	g_signal_emit(self, signals[CHANGED], 0, NULL);
}

CCMConfig*
ccm_config_new (int screen, gchar* extension, gchar* key)
{
	g_return_val_if_fail(key != NULL, NULL);
	
	CCMConfig* self = g_object_new(backend_type, "screen", screen,
	                               "extension", extension, "key", key, NULL);
	
	if (!CCM_CONFIG_GET_CLASS(self)->initialize ||
		!CCM_CONFIG_GET_CLASS(self)->initialize(self, screen, extension, key))
	{
		g_object_unref(self);
		self = NULL;
	}
	
	return self;
}

gboolean
ccm_config_get_boolean(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		
		return FALSE;
	}
	
	if (!CCM_CONFIG_GET_CLASS(self)->get_boolean)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
		
		return FALSE;
	}
	
	return CCM_CONFIG_GET_CLASS(self)->get_boolean(self, error);
}
	
void
ccm_config_set_boolean(CCMConfig* self, gboolean value, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
	}
	
	if (!CCM_CONFIG_GET_CLASS(self)->set_boolean)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
	}
	else
		CCM_CONFIG_GET_CLASS(self)->set_boolean(self, value, error);
}
	
gint
ccm_config_get_integer(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		return 0;
	}
	
	if (!CCM_CONFIG_GET_CLASS(self)->get_integer)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
		
		return 0;
	}
	
	return CCM_CONFIG_GET_CLASS(self)->get_integer(self, error);
}
	
void
ccm_config_set_integer(CCMConfig* self, gint value, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
	}
	
	if (!CCM_CONFIG_GET_CLASS(self)->set_integer)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
	}
	else
		CCM_CONFIG_GET_CLASS(self)->set_integer(self, value, error);
}
	
gfloat
ccm_config_get_float(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		return 0.0f;
	}
	
	if (!CCM_CONFIG_GET_CLASS(self)->get_float)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
		
		return 0.0f;
	}
	
	return CCM_CONFIG_GET_CLASS(self)->get_float(self, error);
}
	
void
ccm_config_set_float(CCMConfig* self, gfloat value, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
	}
	
	if (!CCM_CONFIG_GET_CLASS(self)->set_float)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
	}
	else
		CCM_CONFIG_GET_CLASS(self)->set_float(self, value, error);
}
	
gchar *
ccm_config_get_string(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		return NULL;
	}
		
	if (!CCM_CONFIG_GET_CLASS(self)->get_string)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
		
		return NULL;
	}
	
	return CCM_CONFIG_GET_CLASS(self)->get_string(self, error);
}
	
void
ccm_config_set_string(CCMConfig* self, gchar * value, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
	}
		
	if (!CCM_CONFIG_GET_CLASS(self)->set_string)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
	}
	else
		CCM_CONFIG_GET_CLASS(self)->set_string(self, value, error);
}
	
GSList*
ccm_config_get_string_list(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		return NULL;
	}
		
	if (!CCM_CONFIG_GET_CLASS(self)->get_string_list)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
		
		return NULL;
	}
	
	return CCM_CONFIG_GET_CLASS(self)->get_string_list(self, error);
}
	
void
ccm_config_set_string_list(CCMConfig* self, GSList * value, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
	}
		
	if (!CCM_CONFIG_GET_CLASS(self)->set_string_list)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
	}
	else
		CCM_CONFIG_GET_CLASS(self)->set_string_list(self, value, error);
}

GSList*
ccm_config_get_integer_list(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		return NULL;
	}
		
	if (!CCM_CONFIG_GET_CLASS(self)->get_integer_list)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
		
		return NULL;
	}
	
	return CCM_CONFIG_GET_CLASS(self)->get_integer_list(self, error);
}
	
void
ccm_config_set_integer_list(CCMConfig* self, GSList * value, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
	}
		
	if (!CCM_CONFIG_GET_CLASS(self)->set_integer_list)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_NOT_SUPPORTED,
								 "Not supported");
	}
	else
		CCM_CONFIG_GET_CLASS(self)->set_integer_list(self, value, error);
}

GdkColor*
ccm_config_get_color(CCMConfig* self, GError** error)
{
	if (self == NULL)
	{
		if (error)
			*error = g_error_new(CCM_CONFIG_ERROR_QUARK, 
								 CCM_CONFIG_ERROR_IS_NULL,
								 "Invalid object");
		return NULL;
	}
	
	gchar* value = ccm_config_get_string (self, error);
	GdkColor* color = NULL;
	
	if (value && value[0] == '#')
	{
	    gint c[3];

		if (sscanf(value, "#%2x%2x%2x", &c[0], &c[1], &c[2]) == 3)
		{
			color = g_new0(GdkColor, 1);

			color->red = c[0] << 8 | c[0];
	        color->green = c[1] << 8 | c[1];
	        color->blue = c[2] << 8 | c[2];
		}
	}
	if (value) g_free(value);
	
	return color;
}
