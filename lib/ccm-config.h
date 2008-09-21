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
#include <gconf/gconf-client.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define CCM_TYPE_CONFIG             (ccm_config_get_type ())
#define CCM_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG, CCMConfig))
#define CCM_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG, CCMConfigClass))
#define CCM_IS_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG))
#define CCM_IS_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG))
#define CCM_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG, CCMConfigClass))

typedef struct _CCMConfigClass   CCMConfigClass;
typedef struct _CCMConfigPrivate CCMConfigPrivate;
typedef struct _CCMConfig        CCMConfig;

struct _CCMConfigClass
{
	GObjectClass parent_class;
	
	GConfClient* client;
};

struct _CCMConfig
{
	GObject parent_instance;
	
	CCMConfigPrivate* priv;
};

GType 			ccm_config_get_type 				(void) G_GNUC_CONST;
CCMConfig* 		ccm_config_new 						(int screen,
													 gchar* extension, 
													 gchar* key);
gboolean 		ccm_config_get_boolean				(CCMConfig* self);
void 			ccm_config_set_boolean				(CCMConfig* self, 
													 gboolean value);
gint 			ccm_config_get_integer				(CCMConfig* self);
void 			ccm_config_set_integer				(CCMConfig* self, 
													 gint value);
gfloat 			ccm_config_get_float				(CCMConfig* self);
void 			ccm_config_set_float				(CCMConfig* self, 
													 gfloat value);
gchar* 			ccm_config_get_string				(CCMConfig* self);
void 			ccm_config_set_string				(CCMConfig* self, 
													 gchar * value);
GSList* 		ccm_config_get_string_list			(CCMConfig* self);
void 			ccm_config_set_string_list			(CCMConfig* self, 
													 GSList * value);
GSList*			ccm_config_get_integer_list			(CCMConfig* self);
void			ccm_config_set_integer_list			(CCMConfig* self, 
													 GSList * value);
GdkColor*       ccm_config_get_color                (CCMConfig* self);


G_END_DECLS

#endif /* _CCM_CONFIG_H_ */
