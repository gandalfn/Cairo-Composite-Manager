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

#ifndef _CCM_CONFIG_KEY_H_
#define _CCM_CONFIG_KEY_H_

#include "ccm-config.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS
#define CCM_TYPE_CONFIG_KEY             (ccm_config_key_get_type ())
#define CCM_CONFIG_KEY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_CONFIG_KEY, CCMConfigKey))
#define CCM_CONFIG_KEY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_CONFIG_KEY, CCMConfigKeyClass))
#define CCM_IS_CONFIG_KEY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_CONFIG_KEY))
#define CCM_IS_CONFIG_KEY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_CONFIG_KEY))
#define CCM_CONFIG_KEY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_CONFIG_KEY, CCMConfigKeyClass))
typedef struct _CCMConfigKeyClass CCMConfigKeyClass;
typedef struct _CCMConfigKeyPrivate CCMConfigKeyPrivate;
typedef struct _CCMConfigKey CCMConfigKey;

struct _CCMConfigKeyClass
{
    CCMConfigClass parent_class;

    GHashTable *schemas;
    GHashTable *configs;
};

struct _CCMConfigKey
{
    CCMConfig parent_instance;

    CCMConfigKeyPrivate *priv;
};

GType
ccm_config_key_get_type (void)
    G_GNUC_CONST;

G_END_DECLS
#endif                          /* _CCM_CONFIG_KEY_H_ */
