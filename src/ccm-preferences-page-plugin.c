/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-preferences-page-plugin.c
 * Copyright (C) Nicolas Bruguier 2007-2011 <gandalfn@club-internet.fr>
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

#include "ccm-preferences-page-plugin.h"

static void
ccm_preferences_page_plugin_base_init (gpointer g_class)
{
    static gboolean initialized = FALSE;

    if (!initialized)
    {
        initialized = TRUE;
    }
}

GType
ccm_preferences_page_plugin_get_type (void)
{
    static GType ccm_preferences_page_plugin_type = 0;

    if (!ccm_preferences_page_plugin_type)
    {
        const GTypeInfo ccm_preferences_page_plugin_info = {
            sizeof (CCMPreferencesPagePluginClass),
            ccm_preferences_page_plugin_base_init,
            NULL
        };

        ccm_preferences_page_plugin_type =
            g_type_register_static (G_TYPE_INTERFACE,
                                    "CCMPreferencesPagePlugin",
                                    &ccm_preferences_page_plugin_info, 0);
    }

    return ccm_preferences_page_plugin_type;
}

G_GNUC_PURE CCMPreferencesPagePlugin *
_ccm_preferences_page_plugin_get_root (CCMPreferencesPagePlugin * self)
{
    g_return_val_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self), NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin));

    return plugin;
}

void
ccm_preferences_page_plugin_init_general_section (CCMPreferencesPagePlugin * self,
                                                  CCMPreferencesPage * preferences,
                                                  GtkWidget * general_section)
{
    g_return_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self));
    g_return_if_fail (preferences != NULL);
    g_return_if_fail (general_section != NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin))
    {
        if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_general_section)
            break;
    }

    if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_general_section)
    {
        if (!_ccm_plugin_method_locked((GObject *) plugin,
                                       CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_general_section))
            CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_general_section (plugin,
                                                                                      preferences,
                                                                                      general_section);
    }
}

void
ccm_preferences_page_plugin_init_desktop_section (CCMPreferencesPagePlugin * self,
                                                  CCMPreferencesPage * preferences,
                                                  GtkWidget * desktop_section)
{
    g_return_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self));
    g_return_if_fail (preferences != NULL);
    g_return_if_fail (desktop_section != NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin))
    {
        if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_desktop_section)
            break;
    }

    if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_desktop_section)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin,
                                        CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)-> init_desktop_section))
            CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_desktop_section (plugin,
                                                                                      preferences,
                                                                                      desktop_section);
    }
}

void
ccm_preferences_page_plugin_init_windows_section (CCMPreferencesPagePlugin * self,
                                                  CCMPreferencesPage * preferences,
                                                  GtkWidget * windows_section)
{
    g_return_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self));
    g_return_if_fail (preferences != NULL);
    g_return_if_fail (windows_section != NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin))
    {
        if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_windows_section)
            break;
    }

    if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->
        init_windows_section)
    {
        if (!_ccm_plugin_method_locked((GObject *) plugin,
                                       CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_windows_section))
            CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_windows_section (plugin,
                                                                                      preferences, 
                                                                                      windows_section);
    }
}

void
ccm_preferences_page_plugin_init_effects_section (CCMPreferencesPagePlugin * self,
                                                  CCMPreferencesPage * preferences,
                                                  GtkWidget * effects_section)
{
    g_return_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self));
    g_return_if_fail (preferences != NULL);
    g_return_if_fail (effects_section != NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin))
    {
        if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_effects_section)
            break;
    }

    if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_effects_section)
    {
        if (!_ccm_plugin_method_locked((GObject *) plugin,
                                       CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_effects_section))
            CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_effects_section (plugin, 
                                                                                      preferences, 
                                                                                      effects_section);
    }
}

void
ccm_preferences_page_plugin_init_accessibility_section (CCMPreferencesPagePlugin * self,
                                                        CCMPreferencesPage * preferences,
                                                        GtkWidget * accessibility_section)
{
    g_return_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self));
    g_return_if_fail (preferences != NULL);
    g_return_if_fail (accessibility_section != NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin))
    {
        if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_accessibility_section)
            break;
    }

    if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_effects_section)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin,
                                        CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_accessibility_section))
            CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_accessibility_section (plugin, 
                                                                                            preferences,
                                                                                            accessibility_section);
    }
}

void
ccm_preferences_page_plugin_init_utilities_section (CCMPreferencesPagePlugin * self,
                                                    CCMPreferencesPage * preferences,
                                                    GtkWidget * utilities_section)
{
    g_return_if_fail (CCM_IS_PREFERENCES_PAGE_PLUGIN (self));
    g_return_if_fail (preferences != NULL);
    g_return_if_fail (utilities_section != NULL);

    CCMPreferencesPagePlugin *plugin;

    for (plugin = self; CCM_IS_PLUGIN (plugin);
         plugin = CCM_PREFERENCES_PAGE_PLUGIN_PARENT (plugin))
    {
        if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_utilities_section)
            break;
    }

    if (CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_utilities_section)
    {
        if (!_ccm_plugin_method_locked ((GObject *) plugin,
                                        CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_utilities_section))
            CCM_PREFERENCES_PAGE_PLUGIN_GET_INTERFACE (plugin)->init_utilities_section (plugin,
                                                                                        preferences,
                                                                                        utilities_section);
    }
}
