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

#ifndef _CCM_PREFERENCES_H_
#define _CCM_PREFERENCES_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define CCM_TYPE_PREFERENCES             (ccm_preferences_get_type ())
#define CCM_PREFERENCES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCM_TYPE_PREFERENCES, CCMPreferences))
#define CCM_PREFERENCES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CCM_TYPE_PREFERENCES, CCMPreferencesClass))
#define CCM_IS_PREFERENCES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PREFERENCES))
#define CCM_IS_PREFERENCES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CCM_TYPE_PREFERENCES))
#define CCM_PREFERENCES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CCM_TYPE_PREFERENCES, CCMPreferencesClass))

typedef struct _CCMPreferencesClass CCMPreferencesClass;
typedef struct _CCMPreferencesPrivate CCMPreferencesPrivate;
typedef struct _CCMPreferences CCMPreferences;

struct _CCMPreferencesClass
{
	GObjectClass parent_class;
};

struct _CCMPreferences
{
	GObject parent_instance;
	
	CCMPreferencesPrivate* priv;
};

GType           ccm_preferences_get_type    (void) G_GNUC_CONST;
CCMPreferences* ccm_preferences_new         (void);
void            ccm_preferences_show        (CCMPreferences* self);
void            ccm_preferences_hide        (CCMPreferences* self);

G_END_DECLS

#endif /* _CCM_PREFERENCES_H_ */
