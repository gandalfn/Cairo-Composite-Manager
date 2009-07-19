/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2009 <gandalfn@club-internet.fr>
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

#ifndef _CCM_PREFERENCES_PAGE_PLUGIN_H_
#define _CCM_PREFERENCES_PAGE_PLUGIN_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "ccm-plugin.h"
#include "ccm-preferences-page.h"

G_BEGIN_DECLS
#define CCM_TYPE_PREFERENCES_PAGE_PLUGIN             		(ccm_preferences_page_plugin_get_type ())
#define CCM_IS_PREFERENCES_PAGE_PLUGIN(obj)          		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCM_TYPE_PREFERENCES_PAGE_PLUGIN))
#define CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(obj)   	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), CCM_TYPE_PREFERENCES_PAGE_PLUGIN, CCMPreferencesPagePluginClass))
#define CCM_PREFERENCES_PAGE_PLUGIN_PARENT(obj)	   		    ((CCMPreferencesPagePlugin*)ccm_plugin_get_parent((CCMPlugin*)obj))
#define CCM_PREFERENCES_PAGE_PLUGIN_ROOT(obj)	   	    	((CCMPreferencesPagePlugin*)_ccm_preferences_page_plugin_get_root((CCMPreferencesPagePlugin*)obj))
#define CCM_PREFERENCES_PAGE_PLUGIN_LOCK_ROOT_METHOD(plugin, func, callback, data) \
{ \
	CCMPreferencesPagePlugin* r = (CCMPreferencesPagePlugin*)_ccm_preferences_page_plugin_get_root((CCMPreferencesPagePlugin*)plugin); \
\
	if (r && CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(r) && \
		CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(r)->func) \
		_ccm_plugin_lock_method ((GObject*)r, CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(r)->func, \
								 callback, data); \
}
#define CCM_PREFERENCES_PAGE_PLUGIN_UNLOCK_ROOT_METHOD(plugin, func) \
{ \
	CCMPreferencesPagePlugin* r = (CCMPreferencesPagePlugin*)_ccm_preferences_page_plugin_get_root((CCMPreferencesPagePlugin*)plugin); \
\
	if (r && CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(r) && \
		CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(r)->func) \
		_ccm_plugin_unlock_method ((GObject*)r, CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE(r)->func); \
}
typedef struct _CCMPreferencesPagePluginClass CCMPreferencesPagePluginClass;
typedef struct _CCMPreferencesPagePlugin CCMPreferencesPagePlugin;

struct _CCMPreferencesPagePluginClass
{
    GTypeInterface base_iface;

    void (*init_general_section) (CCMPreferencesPagePlugin * self,
                                  CCMPreferencesPage * preferences,
                                  GtkWidget * general_section);
    void (*init_desktop_section) (CCMPreferencesPagePlugin * self,
                                  CCMPreferencesPage * preferences,
                                  GtkWidget * desktop_section);
    void (*init_windows_section) (CCMPreferencesPagePlugin * self,
                                  CCMPreferencesPage * preferences,
                                  GtkWidget * windows_section);
    void (*init_effects_section) (CCMPreferencesPagePlugin * self,
                                  CCMPreferencesPage * preferences,
                                  GtkWidget * effects_section);
    void (*init_accessibility_section) (CCMPreferencesPagePlugin * self,
                                        CCMPreferencesPage * preferences,
                                        GtkWidget * accessibility_section);
    void (*init_utilities_section) (CCMPreferencesPagePlugin * self,
                                    CCMPreferencesPage * preferences,
                                    GtkWidget * utilities_section);
};

GType
ccm_preferences_page_plugin_get_type (void)
    G_GNUC_CONST;

CCMPreferencesPagePlugin *
_ccm_preferences_page_plugin_get_root (CCMPreferencesPagePlugin * self);

void
ccm_preferences_page_plugin_init_general_section (CCMPreferencesPagePlugin *
                                                  self,
                                                  CCMPreferencesPage *
                                                  preferences,
                                                  GtkWidget * general_section);
void
ccm_preferences_page_plugin_init_desktop_section (CCMPreferencesPagePlugin *
                                                  self,
                                                  CCMPreferencesPage *
                                                  preferences,
                                                  GtkWidget * desktop_section);
void
ccm_preferences_page_plugin_init_windows_section (CCMPreferencesPagePlugin *
                                                  self,
                                                  CCMPreferencesPage *
                                                  preferences,
                                                  GtkWidget * windows_section);
void
ccm_preferences_page_plugin_init_effects_section (CCMPreferencesPagePlugin *
                                                  self,
                                                  CCMPreferencesPage *
                                                  preferences,
                                                  GtkWidget * effects_section);
void
ccm_preferences_page_plugin_init_accessibility_section (CCMPreferencesPagePlugin
                                                        * self,
                                                        CCMPreferencesPage *
                                                        preferences,
                                                        GtkWidget *
                                                        accessibility_section);
void
ccm_preferences_page_plugin_init_utilities_section (CCMPreferencesPagePlugin *
                                                    self,
                                                    CCMPreferencesPage *
                                                    preferences,
                                                    GtkWidget *
                                                    utilities_section);

G_END_DECLS
#endif                          /* _CCM_PREFERENCES_PAGE_PLUGIN_H_ */
